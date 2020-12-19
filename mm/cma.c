/*
 * Contiguous Memory Allocator
 *
 * Copyright (c) 2010-2011 by Samsung Electronics.
 * Copyright IBM Corporation, 2013
 * Copyright LG Electronics Inc., 2014
 * Written by:
 *	Marek Szyprowski <m.szyprowski@samsung.com>
 *	Michal Nazarewicz <mina86@mina86.com>
 *	Aneesh Kumar K.V <aneesh.kumar@linux.vnet.ibm.com>
 *	Joonsoo Kim <iamjoonsoo.kim@lge.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License or (at your optional) any later version of the license.
 */

#define pr_fmt(fmt) "cma: " fmt

#ifdef CONFIG_CMA_DEBUG
#ifndef DEBUG
#  define DEBUG
#endif
#endif
#define CREATE_TRACE_POINTS

#include <linux/memblock.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include <linux/log2.h>
#include <linux/cma.h>
#include <linux/highmem.h>
#include <linux/io.h>
#include <trace/events/cma.h>

#include "cma.h"
#include "internal.h"

#ifdef CONFIG_CMA_RTK_ALLOCATOR
#include <linux/delay.h>
#include <linux/rtkblueprint.h>
#endif

#include <mach/rtk_platform.h>

#ifdef CONFIG_OPTEE_SUPPORT_MC_ALLOCATOR
extern unsigned long	cma_mc_start_addr;
#endif

struct cma cma_areas[MAX_CMA_AREAS];
unsigned cma_area_count;
static DEFINE_MUTEX(cma_mutex);

phys_addr_t cma_get_base(const struct cma *cma)
{
	return PFN_PHYS(cma->base_pfn);
}

unsigned long cma_get_size(const struct cma *cma)
{
	return cma->count << PAGE_SHIFT;
}

unsigned long cma_get_free(void)
{
	struct zone *zone;
	unsigned long freecma = 0;

	for_each_populated_zone(zone) {
		if (!is_zone_cma(zone))
			continue;

		freecma += zone_page_state(zone, NR_FREE_PAGES);
	}

	return freecma;
}

static unsigned long cma_bitmap_aligned_mask(const struct cma *cma,
					     int align_order)
{
	if (align_order <= cma->order_per_bit)
		return 0;
	return (1UL << (align_order - cma->order_per_bit)) - 1;
}

/*
 * Find a PFN aligned to the specified order and return an offset represented in
 * order_per_bits.
 */
static unsigned long cma_bitmap_aligned_offset(const struct cma *cma,
					       int align_order)
{
	if (align_order <= cma->order_per_bit)
		return 0;

	return (ALIGN(cma->base_pfn, (1UL << align_order))
		- cma->base_pfn) >> cma->order_per_bit;
}

static unsigned long cma_bitmap_pages_to_bits(const struct cma *cma,
					      unsigned long pages)
{
	return ALIGN(pages, 1UL << cma->order_per_bit) >> cma->order_per_bit;
}

static void cma_clear_bitmap(struct cma *cma, unsigned long pfn,
			     unsigned int count)
{
	unsigned long bitmap_no, bitmap_count;

	bitmap_no = (pfn - cma->base_pfn) >> cma->order_per_bit;
	bitmap_count = cma_bitmap_pages_to_bits(cma, count);

	mutex_lock(&cma->lock);
	bitmap_clear(cma->bitmap, bitmap_no, bitmap_count);
	mutex_unlock(&cma->lock);
}

static int __init cma_activate_bitmap_area(struct cma *cma)
{
	int bitmap_size = BITS_TO_LONGS(cma_bitmap_maxno(cma)) * sizeof(long);
	unsigned long base_pfn = cma->base_pfn, pfn = base_pfn;
	unsigned i = cma->count >> pageblock_order;
	struct zone *zone;

	cma->bitmap = kzalloc(bitmap_size, GFP_KERNEL);
	cma->used = 0;
	cma->peak = 0;

	if (!cma->bitmap)
		return -ENOMEM;

	WARN_ON_ONCE(!pfn_valid(pfn));
	zone = page_zone(pfn_to_page(pfn));

	do {
		unsigned j;

		base_pfn = pfn;
		for (j = pageblock_nr_pages; j; --j, pfn++) {
			WARN_ON_ONCE(!pfn_valid(pfn));
			/*
			 * alloc_contig_range requires the pfn range
			 * specified to be in the same zone. Make this
			 * simple by forcing the entire CMA resv range
			 * to be in the same zone.
			 */
			if (page_zone(pfn_to_page(pfn)) != zone) {
				pr_err("cma_activate_bitmap_area ERROR, zone of page 0x%lx is %s, not %s\n", pfn * PAGE_SIZE, page_zone(pfn_to_page(pfn))->name, zone->name);
				goto err;
			}
		}
		init_cma_reserved_pageblock(pfn_to_page(base_pfn));
	} while (--i);

	mutex_init(&cma->lock);

#ifdef CONFIG_CMA_DEBUGFS
	INIT_HLIST_HEAD(&cma->mem_head);
	spin_lock_init(&cma->mem_head_lock);
#endif

	pr_info("%s: returned normal bitmap cma %p\n", __func__, (void *)cma);

	return 0;

err:
	pr_err("CMA bitmap area could not be activated\n");
	kfree(cma->bitmap);
	cma->count = 0;
	return -EINVAL;
}

static int __init cma_activate_area(struct cma *cma)
{
#ifndef CONFIG_CMA_RTK_ALLOCATOR
	int bitmap_size = BITS_TO_LONGS(cma_bitmap_maxno(cma)) * sizeof(long);
#endif
	unsigned long base_pfn = cma->base_pfn, pfn = base_pfn;
	unsigned i = cma->count >> pageblock_order;
	struct zone *zone;

#ifdef CONFIG_CMA_RTK_ALLOCATOR
	if (!cma->count) {
		pr_info("%s: returned null cma %p\n", __func__, (void *)cma);
		return -EINVAL;
	}

    cma->bitmap = kzalloc(sizeof(struct mem_bp), GFP_KERNEL);

    if (!cma->bitmap)
		return -ENOMEM;

    init_rtkbp((struct mem_bp *)cma->bitmap, cma->base_pfn, cma->count);
#else
	cma->bitmap = kzalloc(bitmap_size, GFP_KERNEL);

	if (!cma->bitmap)
		return -ENOMEM;
#endif

	WARN_ON_ONCE(!pfn_valid(pfn));
	zone = page_zone(pfn_to_page(pfn));

	do {
		unsigned j;

		base_pfn = pfn;
		for (j = pageblock_nr_pages; j; --j, pfn++) {
			WARN_ON_ONCE(!pfn_valid(pfn));
			/*
			 * alloc_contig_range requires the pfn range
			 * specified to be in the same zone. Make this
			 * simple by forcing the entire CMA resv range
			 * to be in the same zone.
			 */
			if (page_zone(pfn_to_page(pfn)) != zone) {
				pr_err("cma_activate_area ERROR, zone of page 0x%lx is %s, not %s\n", pfn * PAGE_SIZE, page_zone(pfn_to_page(pfn))->name, zone->name);
				goto err;
            }
		}
        if (unlikely(is_banned(base_pfn, BAN_NOT_USED))) {
            pr_info("CMA: exclude %ld MiB at %08lx\n",
					pageblock_nr_pages * PAGE_SIZE / SZ_1M,
					base_pfn << PAGE_SHIFT);
            continue;
        }
		init_cma_reserved_pageblock(pfn_to_page(base_pfn));
	} while (--i);

	mutex_init(&cma->lock);

#ifdef CONFIG_CMA_DEBUGFS
	INIT_HLIST_HEAD(&cma->mem_head);
	spin_lock_init(&cma->mem_head_lock);
#endif

	pr_info("%s: returned normal cma %p\n", __func__, (void *)cma);

	return 0;

err:
	kfree(cma->bitmap);
	cma->count = 0;
	return -EINVAL;
}

static int __init cma_init_reserved_areas(void)
{
	int i;
	struct zone *zone;
	unsigned long start_pfn = UINT_MAX, end_pfn = 0;

	if (!cma_area_count)
		return 0;

	for (i = 0; i < cma_area_count; i++) {
		if (start_pfn > cma_areas[i].base_pfn)
			start_pfn = cma_areas[i].base_pfn;
		if (end_pfn < cma_areas[i].base_pfn + cma_areas[i].count)
			end_pfn = cma_areas[i].base_pfn + cma_areas[i].count;
	}

	for_each_populated_zone(zone) {
		if (!is_zone_cma(zone))
			continue;

		/* ZONE_CMA doesn't need to exceed CMA region */
		zone->zone_start_pfn = max(zone->zone_start_pfn, start_pfn);
		zone->spanned_pages = min(zone_end_pfn(zone), end_pfn) -
					zone->zone_start_pfn;
	}

	for (i = 0; i < cma_area_count; i++) {
#ifdef CONFIG_OPTEE_SUPPORT_MC_ALLOCATOR
		int ret;

		if (cma_areas[i].base_pfn == PFN_DOWN(cma_mc_start_addr)) {
			ret = cma_activate_bitmap_area(&cma_areas[i]);
		} else {
			ret = cma_activate_area(&cma_areas[i]);
		}
#else
        int ret;
        unsigned long carvedout_cma_3_size, carvedout_cma_3_start;
        carvedout_cma_3_size = carvedout_buf_query(CARVEDOUT_CMA_3, &carvedout_cma_3_start);

		if (cma_areas[i].base_pfn == PFN_DOWN((unsigned int)carvedout_cma_3_start))
            ret = cma_activate_bitmap_area(&cma_areas[i]);
        else
            ret = cma_activate_area(&cma_areas[i]);
#endif

		if (ret)
			return ret;
	}

	/*
	 * Reserved pages for ZONE_CMA are now activated and this would change
	 * ZONE_CMA's managed page counter and other zone's present counter.
	 * We need to re-calculate various zone information that depends on
	 * this initialization.
	 */
	for_each_populated_zone(zone) {
		if (!is_zone_cma(zone))
			continue;

		setup_zone_pageset(zone);
	}
	build_all_zonelists(NULL, NULL);
	for_each_populated_zone(zone)
		zone_pcp_update(zone);

	/*
	 * We need to re-init per zone wmark by calling
	 * init_per_zone_wmark_min() but doesn't call here because it is
	 * registered on module_init and it will be called later than us.
	 */

	return 0;
}
core_initcall(cma_init_reserved_areas);

/**
 * cma_init_reserved_mem() - create custom contiguous area from reserved memory
 * @base: Base address of the reserved area
 * @size: Size of the reserved area (in bytes),
 * @order_per_bit: Order of pages represented by one bit on bitmap.
 * @res_cma: Pointer to store the created cma region.
 *
 * This function creates custom contiguous area from already reserved memory.
 */
int __init cma_init_reserved_mem(phys_addr_t base, phys_addr_t size,
				 unsigned int order_per_bit,
				 struct cma **res_cma)
{
	struct cma *cma;
	phys_addr_t alignment;

	/* Sanity checks */
	if (cma_area_count == ARRAY_SIZE(cma_areas)) {
		pr_err("Not enough slots for CMA reserved regions!\n");
		return -ENOSPC;
	}

	if (!size || !memblock_is_region_reserved(base, size))
		return -EINVAL;

	/* ensure minimal alignment required by mm core */
	alignment = PAGE_SIZE <<
			max_t(unsigned long, MAX_ORDER - 1, pageblock_order);

	/* alignment should be aligned with order_per_bit */
	if (!IS_ALIGNED(alignment >> PAGE_SHIFT, 1 << order_per_bit))
		return -EINVAL;

	if (ALIGN(base, alignment) != base || ALIGN(size, alignment) != size)
		return -EINVAL;

	/*
	 * Each reserved area must be initialised later, when more kernel
	 * subsystems (like slab allocator) are available.
	 */
	cma = &cma_areas[cma_area_count];
	cma->base_pfn = PFN_DOWN(base);
	cma->count = size >> PAGE_SHIFT;
	cma->order_per_bit = order_per_bit;
	*res_cma = cma;
	cma_area_count++;
	totalcma_pages += (size / PAGE_SIZE);

	return 0;
}

/**
 * cma_declare_contiguous() - reserve custom contiguous area
 * @base: Base address of the reserved area optional, use 0 for any
 * @size: Size of the reserved area (in bytes),
 * @limit: End address of the reserved memory (optional, 0 for any).
 * @alignment: Alignment for the CMA area, should be power of 2 or zero
 * @order_per_bit: Order of pages represented by one bit on bitmap.
 * @fixed: hint about where to place the reserved area
 * @res_cma: Pointer to store the created cma region.
 *
 * This function reserves memory from early allocator. It should be
 * called by arch specific code once the early allocator (memblock or bootmem)
 * has been activated and all other subsystems have already allocated/reserved
 * memory. This function allows to create custom reserved areas.
 *
 * If @fixed is true, reserve contiguous area at exactly @base.  If false,
 * reserve in range from @base to @limit.
 */
int __init cma_declare_contiguous(phys_addr_t base,
			phys_addr_t size, phys_addr_t limit,
			phys_addr_t alignment, unsigned int order_per_bit,
			bool fixed, struct cma **res_cma)
{
	phys_addr_t memblock_end = memblock_end_of_DRAM();
	phys_addr_t highmem_start;
	int ret = 0;

#ifdef CONFIG_X86
	/*
	 * high_memory isn't direct mapped memory so retrieving its physical
	 * address isn't appropriate.  But it would be useful to check the
	 * physical address of the highmem boundary so it's justifiable to get
	 * the physical address from it.  On x86 there is a validation check for
	 * this case, so the following workaround is needed to avoid it.
	 */
	highmem_start = __pa_nodebug(high_memory);
#else
	highmem_start = __pa(high_memory);
#endif
	pr_debug("%s(size %pa, base %pa, limit %pa alignment %pa)\n",
		__func__, &size, &base, &limit, &alignment);

	if (cma_area_count == ARRAY_SIZE(cma_areas)) {
		pr_err("Not enough slots for CMA reserved regions!\n");
		return -ENOSPC;
	}

	if (!size)
		return -EINVAL;

	if (alignment && !is_power_of_2(alignment))
		return -EINVAL;

	/*
	 * Sanitise input arguments.
	 * Pages both ends in CMA area could be merged into adjacent unmovable
	 * migratetype page by page allocator's buddy algorithm. In the case,
	 * you couldn't get a contiguous memory, which is not what we want.
	 */
	alignment = max(alignment,  (phys_addr_t)PAGE_SIZE <<
			  max_t(unsigned long, MAX_ORDER - 1, pageblock_order));
	base = ALIGN(base, alignment);
	size = ALIGN(size, alignment);
	limit &= ~(alignment - 1);

	if (!base)
		fixed = false;

	/* size should be aligned with order_per_bit */
	if (!IS_ALIGNED(size >> PAGE_SHIFT, 1 << order_per_bit))
		return -EINVAL;

	/*
	 * If allocating at a fixed base the request region must not cross the
	 * low/high memory boundary.
	 */
	if (fixed && base < highmem_start && base + size > highmem_start) {
		ret = -EINVAL;
		pr_err("Region at %pa defined on low/high memory boundary (%pa)\n",
			&base, &highmem_start);
		goto err;
	}

	/*
	 * If the limit is unspecified or above the memblock end, its effective
	 * value will be the memblock end. Set it explicitly to simplify further
	 * checks.
	 */
	if (limit == 0 || limit > memblock_end)
		limit = memblock_end;

	/* Reserve memory */
	if (fixed) {
		if (memblock_reserve(base, size) < 0) {
			ret = -EBUSY;
			goto err;
		}
	} else {
		phys_addr_t addr = 0;

		/*
		 * All pages in the reserved area must come from the same zone.
		 * If the requested region crosses the low/high memory boundary,
		 * try allocating from high memory first and fall back to low
		 * memory in case of failure.
		 */
		if (base < highmem_start && limit > highmem_start) {
			addr = memblock_alloc_range(size, alignment,
						    highmem_start, limit,
						    MEMBLOCK_NONE);
			limit = highmem_start;
		}

		if (!addr) {
			addr = memblock_alloc_range(size, alignment, base,
						    limit,
						    MEMBLOCK_NONE);
			if (!addr) {
				ret = -ENOMEM;
				goto err;
			} else if (addr + size > ~(unsigned long)0) {
				memblock_free(addr, size);
				base = -EINVAL;
				goto err;
			}
		}

		/*
		 * kmemleak scans/reads tracked objects for pointers to other
		 * objects but this address isn't mapped and accessible
		 */
		kmemleak_ignore(phys_to_virt(addr));
		base = addr;
	}

	ret = cma_init_reserved_mem(base, size, order_per_bit, res_cma);
	if (ret)
		goto err;

	pr_info("Reserved %ld MiB at %pa\n", (unsigned long)size / SZ_1M,
		&base);
	return 0;

err:
	pr_err("Failed to reserve %ld MiB\n", (unsigned long)size / SZ_1M);
	return ret;
}

int __init cma_declare_null(struct cma **res_cma)
{
	struct cma *cma;

	/* Sanity checks */
	if (cma_area_count == ARRAY_SIZE(cma_areas)) {
		pr_err("Not enough slots for CMA reserved regions!\n");
		return -ENOSPC;
	}

	cma = &cma_areas[cma_area_count];
	cma->base_pfn = 0;
	cma->count = 0;

	*res_cma = cma;
	cma_area_count++;

	pr_info("CMA: reserved null cma\n");

	return 0;
}

void cma_show_bitmap(struct cma *cma)
{
	unsigned long bitmap_maxno;
	unsigned long cma_used_size = cma->used << PAGE_SHIFT;
	unsigned long cma_bitmap_all_size = cma->count << PAGE_SHIFT;
	unsigned long cma_peak_size = cma->peak << PAGE_SHIFT;
	unsigned long cma_free = 0, cma_free_size = 0;
	cma_free = cma->count - cma->used;
	cma_free_size = cma_free << PAGE_SHIFT;
	pr_warn("bitmap:base_pfn=0x%lx(0x%lx) used=%d(0x%lx) peak=%d(0x%lx) all=%d(0x%lx) free=%d(0x%lx) pages\n",cma->base_pfn, cma->base_pfn << PAGE_SHIFT, cma->used, cma_used_size, cma->peak, cma_peak_size, cma->count, cma_bitmap_all_size,  cma_free, cma_free_size);
}
/**
 * cma_bitmap_alloc() - allocate pages from contiguous area by bitmap alocator
 * @cma:   Contiguous memory region for which the allocation is performed.
 * @count: Requested number of pages.
 * @align: Requested alignment of pages (in PAGE_SIZE order).
 *
 * This function allocates part of contiguous memory on specific
 * contiguous memory area.
 */
struct page *cma_bitmap_alloc(struct cma *cma, size_t count, unsigned int align)
{
	unsigned long mask, offset;
	unsigned long pfn = -1, pageno;
	unsigned long start = 0;
	unsigned long bitmap_maxno, bitmap_no, bitmap_count;
	struct page *page = NULL;
	int ret;

	if (!cma || !cma->count)
		return NULL;

	pr_debug("%s(cma %p, count %zu, align %d)\n", __func__, (void *)cma,
		 count, align);

	if (!count)
		return NULL;

	mask = cma_bitmap_aligned_mask(cma, align);
	offset = cma_bitmap_aligned_offset(cma, align);
	bitmap_maxno = cma_bitmap_maxno(cma);
	bitmap_count = cma_bitmap_pages_to_bits(cma, count);

	for (;;) {
		mutex_lock(&cma->lock);
		bitmap_no = bitmap_find_next_zero_area_off(cma->bitmap,
				bitmap_maxno, start, bitmap_count, mask,
				offset);
		if (bitmap_no >= bitmap_maxno) {
			mutex_unlock(&cma->lock);
			break;
		}
		bitmap_set(cma->bitmap, bitmap_no, bitmap_count);
		/*
		 * It's safe to drop the lock here. We've marked this region for
		 * our exclusive use. If the migration fails we will take the
		 * lock again and unmark it.
		 */
		mutex_unlock(&cma->lock);

		pfn = cma->base_pfn + (bitmap_no << cma->order_per_bit);
		mutex_lock(&cma_mutex);
		ret = alloc_contig_range(pfn, pfn + count);
		mutex_unlock(&cma_mutex);
		if (ret == 0) {
			page = pfn_to_page(pfn);
			break;
		}

		cma_clear_bitmap(cma, pfn, count);
		if (ret != -EBUSY)
			break;

		pr_debug("%s(): memory range at %p is busy, retrying\n",
			 __func__, pfn_to_page(pfn));
		/* try again with a bit different memory target */
		start = bitmap_no + mask + 1;
	}

	trace_cma_alloc(pfn, page, count, align);

	pr_debug("%s(): returned %p\n", __func__, page);
	cma->used += count;
	if(cma->used > cma->peak)
		cma->peak = cma->used;
	if(cma->peak > cma->count)
		cma->peak = cma->count;
	pr_debug("bitmap alloc %d bitmap used %d peak %d all %d order %d pfn %lx\n", count, cma->used, cma->peak, cma->count, cma->order_per_bit, cma->base_pfn);

	return page;
}

/**
 * cma_bitmap_release() - release allocated pages by bitmap alocator
 * @cma:   Contiguous memory region for which the allocation is performed.
 * @pages: Allocated pages.
 * @count: Number of allocated pages.
 *
 * This function releases memory allocated by alloc_cma().
 * It returns false when provided pages do not belong to contiguous area and
 * true otherwise.
 */
bool cma_bitmap_release(struct cma *cma, const struct page *pages, unsigned int count)
{
	unsigned long pfn;
	unsigned long bitmap_maxno;

	if (!cma || !pages)
		return false;

	pr_debug("%s(page %p)\n", __func__, (void *)pages);

	pfn = page_to_pfn(pages);

	if (pfn < cma->base_pfn || pfn >= cma->base_pfn + cma->count)
		return false;

	VM_BUG_ON(pfn + count > cma->base_pfn + cma->count);

	free_contig_range(pfn, count);
	cma_clear_bitmap(cma, pfn, count);
	trace_cma_release(pfn, pages, count);

	cma->used -= count;
	if(cma->used < 0)
		cma->used = 0;
	bitmap_maxno = cma_bitmap_maxno(cma);
	pr_debug("bitmap free %d bitmap used %d peak %d all %d\n", count, cma->used, cma->peak, cma->count);


	return true;
}

#ifdef CONFIG_OPTEE_SUPPORT_MC_ALLOCATOR
int cma_migrate_range(struct cma *cma, unsigned long pfn, unsigned long count)
{
	int ret = 0;

	if (!cma || !pfn || !count)
		return -EINVAL;

	if (pfn < cma->base_pfn || pfn >= cma->base_pfn + cma->count)
		return -EINVAL;

	for (;;) {
		mutex_lock(&cma_mutex);
		ret = alloc_contig_range(pfn, pfn + count);
		mutex_unlock(&cma_mutex);
		if (ret == 0) {
			break;
		}

		pr_info("%s(): memory range at 0x%lx ~ 0x%lx is busy, ret = %d, retrying\n",
			 __func__, pfn << PAGE_SHIFT, (pfn + count) << PAGE_SHIFT, ret);
		//msleep(10);
	}

	pr_debug("%s(): migrate 0x%lx ~ 0x%lx returned %d\n", __func__, pfn << PAGE_SHIFT, (pfn + count) << PAGE_SHIFT, ret);
	return ret;
}

bool cma_release_range(struct cma *cma, const unsigned long pfn, unsigned int count)
{
	if (!cma || !pfn || !count)
		return false;

	pr_debug("%s(0x%lx ~ 0x%lx)\n", __func__, pfn << PAGE_SHIFT, (pfn + count) << PAGE_SHIFT);

	if (pfn < cma->base_pfn || pfn >= cma->base_pfn + cma->count)
		return false;

	VM_BUG_ON(pfn + count > cma->base_pfn + cma->count);

	free_contig_range(pfn, count);

	return true;
}
#endif

/**
 * cma_alloc() - allocate pages from contiguous area
 * @cma:   Contiguous memory region for which the allocation is performed.
 * @count: Requested number of pages.
 * @align: Requested alignment of pages (in PAGE_SIZE order).
 *
 * This function allocates part of contiguous memory on specific
 * contiguous memory area.
 */
struct page *cma_alloc(struct cma *cma, size_t count, unsigned int align)
{
	unsigned long mask, offset;
	unsigned long pfn = -1, pageno;
	unsigned long start = 0;
	unsigned long bitmap_maxno, bitmap_no, bitmap_count;
	struct page *page = NULL;
	int ret;

	if (!cma || !cma->count)
		return NULL;

	pr_debug("%s(cma %p, count %zu, align %d)\n", __func__, (void *)cma,
		 count, align);

	if (!count)
		return NULL;

	mask = cma_bitmap_aligned_mask(cma, align);
	offset = cma_bitmap_aligned_offset(cma, align);
	bitmap_maxno = cma_bitmap_maxno(cma);
	bitmap_count = cma_bitmap_pages_to_bits(cma, count);

#ifdef CONFIG_CMA_RTK_ALLOCATOR
	mutex_lock(&cma->lock);
	pageno = alloc_rtkbp_memory((struct mem_bp *)cma->bitmap, get_order(count * PAGE_SIZE));
//	show_rtkbp((struct mem_bp *)cma->bitmap);
	if (pageno == INVALID_VAL) {
		pr_info("cma %p: free pages %lu\n", (void *)cma, ((struct mem_bp *)cma->bitmap)->avail_size);
		dump_stack();
		mutex_unlock(&cma->lock);
		return NULL;
	}
	mutex_unlock(&cma->lock);

	pfn = cma->base_pfn + pageno;

	mutex_lock(&cma_mutex);
	for (;;) {
		ret = alloc_contig_range(pfn, pfn + count);

		if (ret == 0) {
			page = pfn_to_page(pfn);
			break;
		} else if (ret != -EBUSY) {
			free_rtkbp_memory((struct mem_bp *)cma->bitmap, pageno, get_order(count * PAGE_SIZE));
			break;
		}

		mutex_unlock(&cma_mutex);
		pr_info("cma %p: retry remapping...\n", (void *)cma);
		msleep(10);
		mutex_lock(&cma_mutex);
	}
	mutex_unlock(&cma_mutex);
#else
	for (;;) {
		mutex_lock(&cma->lock);
		bitmap_no = bitmap_find_next_zero_area_off(cma->bitmap,
				bitmap_maxno, start, bitmap_count, mask,
				offset);
		if (bitmap_no >= bitmap_maxno) {
			mutex_unlock(&cma->lock);
			break;
		}
		bitmap_set(cma->bitmap, bitmap_no, bitmap_count);
		/*
		 * It's safe to drop the lock here. We've marked this region for
		 * our exclusive use. If the migration fails we will take the
		 * lock again and unmark it.
		 */
		mutex_unlock(&cma->lock);

		pfn = cma->base_pfn + (bitmap_no << cma->order_per_bit);
		mutex_lock(&cma_mutex);
		ret = alloc_contig_range(pfn, pfn + count);
		mutex_unlock(&cma_mutex);
		if (ret == 0) {
			page = pfn_to_page(pfn);
			break;
		}

		cma_clear_bitmap(cma, pfn, count);
		if (ret != -EBUSY)
			break;

		pr_debug("%s(): memory range at %p is busy, retrying\n",
			 __func__, pfn_to_page(pfn));
		/* try again with a bit different memory target */
		start = bitmap_no + mask + 1;
	}
#endif

	trace_cma_alloc(pfn, page, count, align);

	pr_debug("%s(): returned %p\n", __func__, page);
	return page;
}

/**
 * cma_release() - release allocated pages
 * @cma:   Contiguous memory region for which the allocation is performed.
 * @pages: Allocated pages.
 * @count: Number of allocated pages.
 *
 * This function releases memory allocated by alloc_cma().
 * It returns false when provided pages do not belong to contiguous area and
 * true otherwise.
 */
bool cma_release(struct cma *cma, const struct page *pages, unsigned int count)
{
	unsigned long pfn;

	if (!cma || !pages)
		return false;

	pr_debug("%s(page %p)\n", __func__, (void *)pages);

	pfn = page_to_pfn(pages);

	if (pfn < cma->base_pfn || pfn >= cma->base_pfn + cma->count)
		return false;

	VM_BUG_ON(pfn + count > cma->base_pfn + cma->count);

	free_contig_range(pfn, count);
#ifdef CONFIG_CMA_RTK_ALLOCATOR
	mutex_lock(&cma->lock);
	free_rtkbp_memory((struct mem_bp *)cma->bitmap, pfn - cma->base_pfn, get_order(count * PAGE_SIZE));
//	show_rtkbp((struct mem_bp *)cma->bitmap);
	mutex_unlock(&cma->lock);
#else
	cma_clear_bitmap(cma, pfn, count);
#endif
	trace_cma_release(pfn, pages, count);

	return true;
}

void lock_cma(void)
{
	mutex_lock(&cma_mutex);
}

void unlock_cma(void)
{
	mutex_unlock(&cma_mutex);
}

unsigned long cma_avail_size(const struct cma *cma)
{
#ifdef CONFIG_CMA_RTK_ALLOCATOR
	return ((struct mem_bp *)cma->bitmap)->avail_size;
#else
	return 0;
#endif
}

unsigned long cma_get_bitmap(const struct cma *cma)
{
	return cma->bitmap;
}
