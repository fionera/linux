/*
 * This file contains shadow memory manipulation code.
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 * Author: Andrey Ryabinin <ryabinin.a.a@gmail.com>
 *
 * Some code borrowed from https://github.com/xairy/kasan-prototype by
 *        Andrey Konovalov <adech.fo@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#define DISABLE_BRANCH_PROFILING

#include <linux/export.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kmemleak.h>
#include <linux/linkage.h>
#include <linux/memblock.h>
#include <linux/memory.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/stacktrace.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/kasan.h>
#include <linux/page-isolation.h>
#include <asm/cacheflush.h>
#include <asm/tlbflush.h>
#include <asm/sections.h>

#include <asm/unaligned.h>

#include "kasan.h"
#include "../slab.h"
#include "../internal.h"

static DEFINE_SPINLOCK(shadow_lock);
static LIST_HEAD(unmap_list);
static void kasan_unmap_shadow_workfn(struct work_struct *work);
static DECLARE_WORK(kasan_unmap_shadow_work, kasan_unmap_shadow_workfn);
static bool kasan_unmap_pending;

/*
 * Poisons the shadow memory for 'size' bytes starting from 'addr'.
 * Memory addresses should be aligned to KASAN_SHADOW_SCALE_SIZE.
 */
static void kasan_poison_shadow(const void *address, size_t size, u8 value)
{
	void *shadow_start, *shadow_end;

	shadow_start = kasan_mem_to_shadow(address);
	shadow_end = kasan_mem_to_shadow(address + size);

	memset(shadow_start, value, shadow_end - shadow_start);
}

void kasan_unpoison_shadow(const void *address, size_t size)
{
	kasan_poison_shadow(address, size, 0);

	if (size & KASAN_SHADOW_MASK) {
		u8 *shadow = (u8 *)kasan_mem_to_shadow(address + size);
		*shadow = size & KASAN_SHADOW_MASK;
	}
}

static void __kasan_unpoison_stack(struct task_struct *task, void *sp)
{
	void *base = task_stack_page(task);
	size_t size = sp - base;

	kasan_unpoison_shadow(base, size);
}

/* Unpoison the entire stack for a task. */
void kasan_unpoison_task_stack(struct task_struct *task)
{
	__kasan_unpoison_stack(task, task_stack_page(task) + THREAD_SIZE);
}

/* Unpoison the stack for the current task beyond a watermark sp value. */
asmlinkage void kasan_unpoison_remaining_stack(void *sp)
{
	__kasan_unpoison_stack(current, sp);
}

static void kasan_mark_pshadow(const void *address, size_t size, u8 value)
{
	void *pshadow_start;
	void *pshadow_end;

	if (!kasan_pshadow_inited())
		return;

	pshadow_start = kasan_mem_to_pshadow(address);
	pshadow_end =  kasan_mem_to_pshadow(address + size);

	memset(pshadow_start, value, pshadow_end - pshadow_start);
}

void kasan_poison_pshadow(const void *address, size_t size)
{
	kasan_mark_pshadow(address, size, KASAN_PER_PAGE_BYPASS);
}

void kasan_unpoison_pshadow(const void *address, size_t size)
{
	kasan_mark_pshadow(address, size, 0);
}

static bool kasan_zero_shadow(pte_t *ptep)
{
	if (pte_pfn(*ptep) == kasan_zero_page_pfn)
		return true;

	return false;
}

static bool kasan_black_shadow(pte_t *ptep)
{
	pte_t pte = *ptep;

	if (pte_none(pte))
		return true;

	if (pte_pfn(pte) == kasan_black_page_pfn)
		return true;

	return false;
}

static bool kasan_is_binary_address(unsigned long shadow_addr)
{
	unsigned long s, e;

	s = (unsigned long)kasan_mem_to_shadow(_text);
	e = (unsigned long)kasan_mem_to_shadow(
		(void *)ALIGN((unsigned long)_end,
			PAGE_SIZE << KASAN_SHADOW_SCALE_SHIFT));

	if (shadow_addr >= s && shadow_addr < e)
		return true;

	return false;
}

static int kasan_exist_shadow_pte(pte_t *ptep, pgtable_t token,
			unsigned long addr, void *data)
{
	unsigned long *count = data;

	if (kasan_black_shadow(ptep))
		return 0;

	(*count)++;
	return 0;
}

static int kasan_map_shadow_pte(pte_t *ptep, pgtable_t token,
			unsigned long addr, void *data)
{
	pte_t pte;
	gfp_t gfp_flags = *(gfp_t *)data;
	struct page *page;
	unsigned long flags;

	if (!kasan_black_shadow(ptep))
		return 0;

	if (kasan_is_binary_address(addr))
		return 0;

	page = alloc_page(gfp_flags);
	if (!page)
		return -ENOMEM;

	__memcpy(page_address(page), kasan_black_page, PAGE_SIZE);

	spin_lock_irqsave(&shadow_lock, flags);
	if (!kasan_black_shadow(ptep))
		goto out;

	pte = mk_pte(page, PAGE_KERNEL);
	set_pte_at(&init_mm, addr, ptep, pte);
	page = NULL;

out:
	spin_unlock_irqrestore(&shadow_lock, flags);
	if (page)
		__free_page(page);

	return 0;
}

static int kasan_map_shadow(const void *addr, size_t size, gfp_t flags)
{
	int err;
	unsigned long shadow_start, shadow_end;
	unsigned long count = 0;

	if (!kasan_pshadow_inited())
		return 0;

	flags = flags & GFP_RECLAIM_MASK;
	shadow_start = (unsigned long)kasan_mem_to_shadow(addr);
	shadow_end = (unsigned long)kasan_mem_to_shadow(addr + size);
	shadow_start = round_down(shadow_start, PAGE_SIZE);
	shadow_end = ALIGN(shadow_end, PAGE_SIZE);

	err = apply_to_page_range(&init_mm, shadow_start,
				shadow_end - shadow_start,
				kasan_exist_shadow_pte, &count);
	if (err) {
		pr_err("checking shadow entry is failed\n");
		return err;
	}

	if (count == (shadow_end - shadow_start) / PAGE_SIZE)
		goto out;

	err = apply_to_page_range(&init_mm, shadow_start,
		shadow_end - shadow_start,
		kasan_map_shadow_pte, (void *)&flags);

out:
	arch_kasan_map_shadow(shadow_start, shadow_end);
	flush_cache_vmap(shadow_start, shadow_end);
	if (err)
		pr_err("mapping shadow entry is failed\n");

	return err;
}

static int kasan_unmap_shadow_pte(pte_t *ptep, pgtable_t token,
			unsigned long addr, void *data)
{
	pte_t pte;
	struct page *page;
	struct list_head *list = data;

	if (kasan_black_shadow(ptep))
		return 0;

	if (kasan_is_binary_address(addr))
		return 0;

	if (kasan_zero_shadow(ptep)) {
		WARN_ON_ONCE(1);
	} else {
		page = pfn_to_page(pte_pfn(*ptep));
		list_add(&page->lru, list);
	}

	pte = pfn_pte(PFN_DOWN(__pa(kasan_black_page)), PAGE_KERNEL);
	pte = pte_wrprotect(pte);
	set_pte_at(&init_mm, addr, ptep, pte);

	return 0;
}

static void kasan_unmap_shadow_workfn(struct work_struct *work)
{
	struct page *page, *next;
	LIST_HEAD(list);
	LIST_HEAD(shadow_list);
	unsigned long flags;
	unsigned int order;
	unsigned long shadow_addr, shadow_size;
	unsigned long tlb_start = ULONG_MAX, tlb_end = 0;
	int err;

	spin_lock_irqsave(&shadow_lock, flags);
	list_splice_init(&unmap_list, &list);
	spin_unlock_irqrestore(&shadow_lock, flags);

	if (list_empty(&list))
		return;

	list_for_each_entry_safe(page, next, &list, lru) {
		order = page_private(page);
		post_alloc_hook(page, order, GFP_NOWAIT);
		set_page_private(page, order);

		shadow_addr = (unsigned long)kasan_mem_to_shadow(
						page_address(page));
		shadow_size = PAGE_SIZE << (order - KASAN_SHADOW_SCALE_SHIFT);

		tlb_start = min(shadow_addr, tlb_start);
		tlb_end = max(shadow_addr + shadow_size, tlb_end);

		flush_cache_vunmap(shadow_addr, shadow_addr + shadow_size);
		err = apply_to_page_range(&init_mm, shadow_addr, shadow_size,
				kasan_unmap_shadow_pte, &shadow_list);
		if (err) {
			pr_err("invalid shadow entry is found\n");
			list_del(&page->lru);
		}
	}
	flush_tlb_kernel_range(tlb_start, tlb_end);

	list_for_each_entry_safe(page, next, &list, lru) {
		list_del(&page->lru);
		__free_pages(page, page_private(page));
	}
	list_for_each_entry_safe(page, next, &shadow_list, lru) {
		list_del(&page->lru);
		__free_page(page);
	}
}

static bool kasan_unmap_shadow(struct page *page, unsigned int order,
			unsigned int max_order)
{
	int err;
	unsigned long shadow_addr, shadow_size;
	unsigned long count = 0;
	LIST_HEAD(list);
	unsigned long flags;
	struct zone *zone;
	int mt;

	if (unlikely(kasan_unmap_pending && keventd_up())) {
		kasan_unmap_pending = false;
		schedule_work(&kasan_unmap_shadow_work);
	}

	if (order < KASAN_SHADOW_SCALE_SHIFT)
		return false;

	if (max_order != (KASAN_SHADOW_SCALE_SHIFT + 1))
		return false;

	if (PageHighMem(page))
		return false;

	shadow_addr = (unsigned long)kasan_mem_to_shadow(page_address(page));
	shadow_size = PAGE_SIZE << (order - KASAN_SHADOW_SCALE_SHIFT);
	err = apply_to_page_range(&init_mm, shadow_addr, shadow_size,
				kasan_exist_shadow_pte, &count);
	if (err) {
		pr_err("checking shadow entry is failed\n");
		return false;
	}

	if (!count)
		return false;

	zone = page_zone(page);
	mt = get_pageblock_migratetype(page);
	if (!is_migrate_isolate(mt))
		__mod_zone_page_state(zone, NR_FREE_PAGES, 1 << order);

	set_page_private(page, order);

	spin_lock_irqsave(&shadow_lock, flags);
	list_add(&page->lru, &unmap_list);
	spin_unlock_irqrestore(&shadow_lock, flags);

	if (!keventd_up()) {
		kasan_unmap_pending = true;
		return true;
	}

	schedule_work(&kasan_unmap_shadow_work);

	return true;
}

static int kasan_bootmem_fixup_pte(pte_t *ptep, pgtable_t token,
			unsigned long addr, void *data)
{
	struct page *page;
	pte_t pte;

	if (!kasan_zero_shadow(ptep))
		return 0;

	if (kasan_is_binary_address(addr))
		return 0;

	if (slab_is_available()) {
		page = alloc_page(GFP_NOWAIT);
		if (!page)
			return -ENOMEM;

		__memset(page_address(page), 0, PAGE_SIZE);
		pte = mk_pte(page, PAGE_KERNEL);
	} else {
		pte = pfn_pte(PFN_DOWN(__pa(kasan_black_page)), PAGE_KERNEL);
		pte = pte_wrprotect(pte);
	}

	set_pte_at(&init_mm, addr, ptep, pte);

	return 0;
}

int kasan_bootmem_fixup(struct page *page, unsigned int order)
{
	int err;
	const void *addr;
	size_t size;
	unsigned long shadow_start, shadow_end;
	unsigned long count = 0;

	if (!kasan_pshadow_inited())
		return 0;

	if (PageHighMem(page))
		return 0;

	/*
	 * We cannot use page allocator at this moment thus we cannot allocate
	 * shadow memory of partial freed page. It's possible to use black
	 * shadow, however it would cause performance problem so skipping it
	 * seems to be more safer here.
	 */
	if (!slab_is_available()) {
		if (order < KASAN_SHADOW_SCALE_SHIFT) {
			pr_info("skip to allocate shadow memory for order %d page\n", order);
			return 1;
		}
	}

	addr = page_address(page);
	size = PAGE_SIZE << order;
	shadow_start = (unsigned long)kasan_mem_to_shadow(addr);
	shadow_end = (unsigned long)kasan_mem_to_shadow(addr + size);
	shadow_start = round_down(shadow_start, PAGE_SIZE);
	shadow_end = ALIGN(shadow_end, PAGE_SIZE);

	err = apply_to_page_range(&init_mm, shadow_start,
				shadow_end - shadow_start,
				kasan_exist_shadow_pte, &count);
	if (err) {
		pr_err("checking shadow entry is failed\n");
		return err;
	}

	if (!count)
		return 0;

	err = apply_to_page_range(&init_mm, shadow_start,
		shadow_end - shadow_start,
		kasan_bootmem_fixup_pte, NULL);

	arch_kasan_map_shadow(shadow_start, shadow_end);
	flush_cache_vmap(shadow_start, shadow_end);
	if (err) {
		pr_err("mapping shadow entry is failed\n");
		return err;
	}

	if (slab_is_available())
		kasan_poison_shadow(addr, size, KASAN_FREE_PAGE);

	return 0;
}

/*
 * All functions below always inlined so compiler could
 * perform better optimizations in each of __asan_loadX/__assn_storeX
 * depending on memory access size X.
 */

static __always_inline bool memory_is_poisoned_1(unsigned long addr)
{
	s8 shadow_value = *(s8 *)kasan_mem_to_shadow((void *)addr);

	if (unlikely(shadow_value)) {
		s8 last_accessible_byte = addr & KASAN_SHADOW_MASK;
		return unlikely(last_accessible_byte >= shadow_value);
	}

	return false;
}

static __always_inline bool memory_is_poisoned_2_4_8(unsigned long addr,
						unsigned long size)
{
	u8 *shadow_addr = (u8 *)kasan_mem_to_shadow((void *)addr);

	if (unlikely(((addr + size - 1) & KASAN_SHADOW_MASK) < size - 1))
		return *shadow_addr || memory_is_poisoned_1(addr + size - 1);

	return memory_is_poisoned_1(addr + size - 1);
}

static __always_inline bool memory_is_poisoned_16(unsigned long addr)
{
	u16 *shadow_addr = (u16 *)kasan_mem_to_shadow((void *)addr);

	if (unlikely(!IS_ALIGNED(addr, KASAN_SHADOW_SCALE_SIZE))) {
		return get_unaligned(shadow_addr) ||
			memory_is_poisoned_1(addr + 15);
	}

	return get_unaligned(shadow_addr);
}

static __always_inline unsigned long bytes_is_nonzero(const u8 *start,
					size_t size)
{
	while (size) {
		if (unlikely(*start))
			return (unsigned long)start;
		start++;
		size--;
	}

	return 0;
}

static __always_inline unsigned long memory_is_nonzero(const void *start,
						const void *end)
{
	unsigned int words;
	unsigned long ret;
	unsigned int prefix = (unsigned long)start % 8;

	if (end - start <= 16)
		return bytes_is_nonzero(start, end - start);

	if (prefix) {
		prefix = 8 - prefix;
		ret = bytes_is_nonzero(start, prefix);
		if (unlikely(ret))
			return ret;
		start += prefix;
	}

	words = (end - start) / 8;
	while (words) {
		if (unlikely(*(u64 *)start))
			return bytes_is_nonzero(start, 8);
		start += 8;
		words--;
	}

	return bytes_is_nonzero(start, (end - start) % 8);
}

static __always_inline bool memory_is_poisoned_n(unsigned long addr,
						size_t size)
{
	unsigned long ret;

	ret = memory_is_nonzero(kasan_mem_to_shadow((void *)addr),
			kasan_mem_to_shadow((void *)addr + size - 1) + 1);

	if (unlikely(ret)) {
		unsigned long last_byte = addr + size - 1;
		s8 *last_shadow = (s8 *)kasan_mem_to_shadow((void *)last_byte);

		if (unlikely(ret != (unsigned long)last_shadow ||
			((long)(last_byte & KASAN_SHADOW_MASK) >= *last_shadow)))
			return true;
	}
	return false;
}

static __always_inline u8 pshadow_val_builtin(unsigned long addr, size_t size)
{
	u8 shadow_val = *(u8 *)kasan_mem_to_pshadow((void *)addr);

	if (shadow_val == KASAN_PER_PAGE_FREE)
		return shadow_val;

	if (likely(((addr + size - 1) & PAGE_MASK) >= (size - 1)))
		return shadow_val;

	if (shadow_val != *(u8 *)kasan_mem_to_pshadow((void *)addr + size - 1))
		return KASAN_PER_PAGE_FREE;

	return shadow_val;
}

static __always_inline u8 pshadow_val_n(unsigned long addr, size_t size)
{
	u8 *start, *end;
	u8 shadow_val;

	start = kasan_mem_to_pshadow((void *)addr);
	end = kasan_mem_to_pshadow((void *)addr + size - 1);
	size = end - start + 1;

	shadow_val = *start;
	if (shadow_val == KASAN_PER_PAGE_FREE)
		return shadow_val;

	while (size) {
		/*
		 * Different shadow value means that access is over
		 * the boundary. Report the error even if it is
		 * in the valid area.
		 */
		if (shadow_val != *start)
			return KASAN_PER_PAGE_FREE;

		start++;
		size--;
	}

	return shadow_val;
}

static __always_inline u8 pshadow_val(unsigned long addr, size_t size)
{
	if (!kasan_pshadow_inited())
		return KASAN_PER_PAGE_BYPASS;

	if (__builtin_constant_p(size)) {
		switch (size) {
		case 1:
		case 2:
		case 4:
		case 8:
		case 16:
			return pshadow_val_builtin(addr, size);
		default:
			BUILD_BUG();
		}
	}

	return pshadow_val_n(addr, size);
}

static __always_inline bool memory_is_poisoned(unsigned long addr, size_t size)
{
	if (__builtin_constant_p(size)) {
		switch (size) {
		case 1:
			return memory_is_poisoned_1(addr);
		case 2:
		case 4:
		case 8:
			return memory_is_poisoned_2_4_8(addr, size);
		case 16:
			return memory_is_poisoned_16(addr);
		default:
			BUILD_BUG();
		}
	}

	return memory_is_poisoned_n(addr, size);
}

static noinline void check_memory_region_slow(unsigned long addr,
				size_t size, bool write,
				unsigned long ret_ip)
{
	preempt_disable();
	if (!arch_kasan_recheck_prepare(addr, size))
		goto report;

	if (!memory_is_poisoned(addr, size)) {
		preempt_enable();
		return;
	}

report:
	preempt_enable();
	__kasan_report(addr, size, write, ret_ip);
}

static __always_inline void check_memory_region(unsigned long addr,
						size_t size, bool write)
{
	if (unlikely(size == 0))
		return;

	if (unlikely((void *)addr <
		kasan_shadow_to_mem((void *)KASAN_SHADOW_START))) {
		__kasan_report(addr, size, write, _RET_IP_);
		return;
	}

	if (likely(!memory_is_poisoned(addr, size)))
		return;

	if (!pshadow_val(addr, size))
		return;

	check_memory_region_slow(addr, size, write, _RET_IP_);
}

void __asan_loadN(unsigned long addr, size_t size);
void __asan_storeN(unsigned long addr, size_t size);

#ifndef CONFIG_ARM
#undef memset
void *memset(void *addr, int c, size_t len)
{
	__asan_storeN((unsigned long)addr, len);

	return __memset(addr, c, len);
}

#undef memmove
void *memmove(void *dest, const void *src, size_t len)
{
	__asan_loadN((unsigned long)src, len);
	__asan_storeN((unsigned long)dest, len);

	return __memmove(dest, src, len);
}

#undef memcpy
void *memcpy(void *dest, const void *src, size_t len)
{
	__asan_loadN((unsigned long)src, len);
	__asan_storeN((unsigned long)dest, len);

	return __memcpy(dest, src, len);
}
#endif

#ifdef CONFIG_ARM
#undef __memset
void *__memset(void *addr, int c, size_t len)
{
	__asan_storeN((unsigned long)addr, len);

	return ____memset(addr, c, len);
}

#undef __memmove
void *__memmove(void *dest, const void *src, size_t len)
{
	__asan_loadN((unsigned long)src, len);
	__asan_storeN((unsigned long)dest, len);

	return ____memmove(dest, src, len);
}

#undef __memcpy
void *__memcpy(void *dest, const void *src, size_t len)
{
	__asan_loadN((unsigned long)src, len);
	__asan_storeN((unsigned long)dest, len);

	return ____memcpy(dest, src, len);
}

#undef __memchr
void *__memchr(const void *p, int c, size_t len)
{
	__asan_loadN((unsigned long)p, len);

	return ____memchr(p, c, len);
}
#endif

void kasan_report(unsigned long addr, size_t size,
		bool is_write, unsigned long ip)
{
	if (!pshadow_val(addr, size))
		return;

	check_memory_region_slow(addr, size, is_write, ip);
}

void kasan_alloc_pages(struct page *page, unsigned int order)
{
	if (likely(!PageHighMem(page))) {
		if (!kasan_pshadow_inited()) {
			kasan_unpoison_shadow(page_address(page),
					PAGE_SIZE << order);
			return;
		}

		kasan_unpoison_pshadow(page_address(page), PAGE_SIZE << order);
	}
}

void kasan_free_pages(struct page *page, unsigned int order)
{
	if (likely(!PageHighMem(page))) {
		if (!kasan_pshadow_inited()) {
			kasan_poison_shadow(page_address(page),
					PAGE_SIZE << order,
					KASAN_FREE_PAGE);
			return;
		}

		kasan_mark_pshadow(page_address(page),
					PAGE_SIZE << order,
					KASAN_PER_PAGE_FREE);
	}
}

bool kasan_free_buddy(struct page *page, unsigned int order,
			unsigned int max_order)
{
	if (!kasan_pshadow_inited())
		return false;

	return kasan_unmap_shadow(page, order, max_order);
}

void kasan_poison_slab(struct page *page)
{
	kasan_poison_shadow(page_address(page),
			PAGE_SIZE << compound_order(page),
			KASAN_KMALLOC_REDZONE);
}

void kasan_unpoison_object_data(struct kmem_cache *cache, void *object)
{
	kasan_unpoison_shadow(object, cache->object_size);
}

void kasan_poison_object_data(struct kmem_cache *cache, void *object)
{
	kasan_poison_shadow(object,
			round_up(cache->object_size, KASAN_SHADOW_SCALE_SIZE),
			KASAN_KMALLOC_REDZONE);
}

void kasan_slab_alloc(struct kmem_cache *cache, void *object)
{
	kasan_kmalloc(cache, object, cache->object_size);
}

void kasan_slab_free(struct kmem_cache *cache, void *object)
{
	unsigned long size = cache->object_size;
	unsigned long rounded_up_size = round_up(size, KASAN_SHADOW_SCALE_SIZE);

	/* RCU slabs could be legally used after free within the RCU period */
	if (unlikely(cache->flags & SLAB_DESTROY_BY_RCU))
		return;

	kasan_poison_shadow(object, rounded_up_size, KASAN_KMALLOC_FREE);
}

void kasan_kmalloc(struct kmem_cache *cache, const void *object, size_t size)
{
	unsigned long redzone_start;
	unsigned long redzone_end;

	if (unlikely(object == NULL))
		return;

	redzone_start = round_up((unsigned long)(object + size),
				KASAN_SHADOW_SCALE_SIZE);
	redzone_end = round_up((unsigned long)object + cache->object_size,
				KASAN_SHADOW_SCALE_SIZE);

	kasan_unpoison_shadow(object, size);
	kasan_poison_shadow((void *)redzone_start, redzone_end - redzone_start,
		KASAN_KMALLOC_REDZONE);
}
EXPORT_SYMBOL(kasan_kmalloc);

int kasan_kmalloc_large(const void *ptr, size_t size, gfp_t flags)
{
	struct page *page;
	unsigned long redzone_start;
	unsigned long redzone_end;
	int err;

	if (unlikely(ptr == NULL))
		return 0;

	page = virt_to_page(ptr);
	err = kasan_slab_page_alloc(ptr,
		PAGE_SIZE << compound_order(page), flags);
	if (err)
		return err;

	redzone_start = round_up((unsigned long)(ptr + size),
				KASAN_SHADOW_SCALE_SIZE);
	redzone_end = (unsigned long)ptr + (PAGE_SIZE << compound_order(page));

	kasan_unpoison_shadow(ptr, size);
	kasan_poison_shadow((void *)redzone_start, redzone_end - redzone_start,
		KASAN_PAGE_REDZONE);

	return 0;
}

void kasan_krealloc(const void *object, size_t size, gfp_t flags)
{
	struct page *page;

	if (unlikely(object == ZERO_SIZE_PTR))
		return;

	page = virt_to_head_page(object);

	if (unlikely(!PageSlab(page)))
		kasan_kmalloc_large(object, size, flags);
	else
		kasan_kmalloc(page->slab_cache, object, size);
}

void kasan_kfree(void *ptr)
{
	struct page *page;

	page = virt_to_head_page(ptr);

	if (unlikely(!PageSlab(page)))
		kasan_poison_shadow(ptr, PAGE_SIZE << compound_order(page),
				KASAN_FREE_PAGE);
	else
		kasan_slab_free(page->slab_cache, ptr);
}

void kasan_kfree_large(const void *ptr)
{
	struct page *page = virt_to_page(ptr);

	kasan_poison_shadow(ptr, PAGE_SIZE << compound_order(page),
			KASAN_FREE_PAGE);
}

int kasan_slab_page_alloc(const void *addr, size_t size, gfp_t flags)
{
	int err;

	if (!kasan_pshadow_inited() || !addr)
		return 0;

	err = kasan_map_shadow(addr, size, flags);
	if (err)
		return err;

	kasan_unpoison_shadow(addr, size);
	kasan_poison_pshadow(addr, size);

	return 0;
}

void kasan_slab_page_free(const void *addr, size_t size)
{
	if (!kasan_pshadow_inited() || !addr)
		return;

	kasan_poison_shadow(addr, size, KASAN_FREE_PAGE);
}

int kasan_module_alloc(void *addr, size_t size)
{
	void *ret;
	size_t shadow_size;
	unsigned long shadow_start;

	shadow_start = (unsigned long)kasan_mem_to_shadow(addr);
	shadow_size = round_up(size >> KASAN_SHADOW_SCALE_SHIFT,
			PAGE_SIZE);

	if (WARN_ON(!PAGE_ALIGNED(shadow_start)))
		return -EINVAL;

	if (IS_ENABLED(CONFIG_ARM)) {
		kasan_unpoison_shadow(addr, size);
		find_vm_area(addr)->flags |= VM_KASAN;
		return 0;
	}

	ret = __vmalloc_node_range(shadow_size, 1, shadow_start,
			shadow_start + shadow_size,
			GFP_KERNEL | __GFP_HIGHMEM | __GFP_ZERO,
			PAGE_KERNEL, VM_NO_GUARD, NUMA_NO_NODE,
			__builtin_return_address(0));

	if (ret) {
		find_vm_area(addr)->flags |= VM_KASAN;
		kmemleak_ignore(ret);
		return 0;
	}

	return -ENOMEM;
}

void kasan_free_shadow(const struct vm_struct *vm)
{
	if (IS_ENABLED(CONFIG_ARM) && vm->flags & VM_KASAN) {
		kasan_poison_shadow(vm->addr, vm->size, KASAN_FREE_PAGE);
		return;
	}

	if (vm->flags & VM_KASAN)
		vfree(kasan_mem_to_shadow(vm->addr));
}

int kasan_stack_alloc(const void *addr, size_t size)
{
	int err;

	if (!kasan_pshadow_inited() || !addr)
		return 0;

	err = kasan_map_shadow(addr, size, THREADINFO_GFP);
	if (err)
		return err;

	kasan_unpoison_shadow(addr, size);
	kasan_poison_pshadow(addr, size);

	return 0;
}

void kasan_stack_free(const void *addr, size_t size)
{
	if (!kasan_pshadow_inited() || !addr)
		return;

	kasan_poison_shadow(addr, size, KASAN_FREE_PAGE);
}

static void register_global(struct kasan_global *global)
{
	size_t aligned_size = round_up(global->size, KASAN_SHADOW_SCALE_SIZE);

	kasan_unpoison_shadow(global->beg, global->size);

	kasan_poison_shadow(global->beg + aligned_size,
		global->size_with_redzone - aligned_size,
		KASAN_GLOBAL_REDZONE);
}

void __asan_register_globals(struct kasan_global *globals, size_t size)
{
	int i;

	for (i = 0; i < size; i++)
		register_global(&globals[i]);
}
EXPORT_SYMBOL(__asan_register_globals);

void __asan_unregister_globals(struct kasan_global *globals, size_t size)
{
}
EXPORT_SYMBOL(__asan_unregister_globals);

#define DEFINE_ASAN_LOAD_STORE(size)				\
	void __asan_load##size(unsigned long addr)		\
	{							\
		check_memory_region(addr, size, false);		\
	}							\
	EXPORT_SYMBOL(__asan_load##size);			\
	__alias(__asan_load##size)				\
	void __asan_load##size##_noabort(unsigned long);	\
	EXPORT_SYMBOL(__asan_load##size##_noabort);		\
	void __asan_store##size(unsigned long addr)		\
	{							\
		check_memory_region(addr, size, true);		\
	}							\
	EXPORT_SYMBOL(__asan_store##size);			\
	__alias(__asan_store##size)				\
	void __asan_store##size##_noabort(unsigned long);	\
	EXPORT_SYMBOL(__asan_store##size##_noabort)

DEFINE_ASAN_LOAD_STORE(1);
DEFINE_ASAN_LOAD_STORE(2);
DEFINE_ASAN_LOAD_STORE(4);
DEFINE_ASAN_LOAD_STORE(8);
DEFINE_ASAN_LOAD_STORE(16);

void __asan_loadN(unsigned long addr, size_t size)
{
	check_memory_region(addr, size, false);
}
EXPORT_SYMBOL(__asan_loadN);

__alias(__asan_loadN)
void __asan_loadN_noabort(unsigned long, size_t);
EXPORT_SYMBOL(__asan_loadN_noabort);

void __asan_storeN(unsigned long addr, size_t size)
{
	check_memory_region(addr, size, true);
}
EXPORT_SYMBOL(__asan_storeN);

__alias(__asan_storeN)
void __asan_storeN_noabort(unsigned long, size_t);
EXPORT_SYMBOL(__asan_storeN_noabort);

/* to shut up compiler complaints */
void __asan_handle_no_return(void) {}
EXPORT_SYMBOL(__asan_handle_no_return);

#ifdef CONFIG_MEMORY_HOTPLUG
static int kasan_mem_notifier(struct notifier_block *nb,
			unsigned long action, void *data)
{
	return (action == MEM_GOING_ONLINE) ? NOTIFY_BAD : NOTIFY_OK;
}

static int __init kasan_memhotplug_init(void)
{
	pr_err("WARNING: KASAN doesn't support memory hot-add\n");
	pr_err("Memory hot-add will be disabled\n");

	hotplug_memory_notifier(kasan_mem_notifier, 0);

	return 0;
}

module_init(kasan_memhotplug_init);
#endif
