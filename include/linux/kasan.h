#ifndef _LINUX_KASAN_H
#define _LINUX_KASAN_H

#include <linux/sched.h>
#include <linux/types.h>

struct kmem_cache;
struct page;
struct vm_struct;

#ifdef CONFIG_KASAN

#define KASAN_SHADOW_SCALE_SHIFT 3

#include <asm/kasan.h>
#include <asm/pgtable.h>

#ifndef KASAN_PSHADOW_SIZE
#define KASAN_PSHADOW_SIZE 0
#endif
#ifndef KASAN_PSHADOW_START
#define KASAN_PSHADOW_START 0
#endif
#ifndef KASAN_PSHADOW_END
#define KASAN_PSHADOW_END 0
#endif

extern unsigned long kasan_pshadow_offset;

extern unsigned char kasan_zero_page[PAGE_SIZE];
#ifdef CONFIG_ARM
extern pte_t kasan_zero_pte[PTRS_PER_PTE + PTE_HWTABLE_PTRS];
#else
extern pte_t kasan_zero_pte[PTRS_PER_PTE];
#endif
extern pmd_t kasan_zero_pmd[PTRS_PER_PMD];
extern pud_t kasan_zero_pud[PTRS_PER_PUD];

extern unsigned char kasan_black_page[PAGE_SIZE];
#ifdef CONFIG_ARM
extern pte_t kasan_black_pte[PTRS_PER_PTE + PTE_HWTABLE_PTRS];
#else
extern pte_t kasan_black_pte[PTRS_PER_PTE];
#endif
extern pmd_t kasan_black_pmd[PTRS_PER_PMD];
extern pud_t kasan_black_pud[PTRS_PER_PUD];

void kasan_populate_shadow(const void *shadow_start,
				const void *shadow_end,
				bool zero, bool private);
void kasan_early_init_pshadow(void);

static inline const void *kasan_shadow_to_mem(const void *shadow_addr)
{
	return (void *)(((unsigned long)shadow_addr - KASAN_SHADOW_OFFSET)
		<< KASAN_SHADOW_SCALE_SHIFT);
}

static inline void *kasan_mem_to_shadow(const void *addr)
{
	return (void *)((unsigned long)addr >> KASAN_SHADOW_SCALE_SHIFT)
		+ KASAN_SHADOW_OFFSET;
}

static inline void *kasan_mem_to_pshadow(const void *addr)
{
	return (void *)((unsigned long)addr >> PAGE_SHIFT)
		+ kasan_pshadow_offset;
}

static inline void *kasan_shadow_to_pshadow(const void *addr)
{
	/*
	 * KASAN_SHADOW_END needs special handling since
	 * it will overflow in kasan_shadow_to_mem()
	 */
	if ((unsigned long)addr == KASAN_SHADOW_END)
		return (void *)KASAN_PSHADOW_END;

	return kasan_mem_to_pshadow(kasan_shadow_to_mem(addr));
}

/* Enable reporting bugs after kasan_disable_current() */
static inline void kasan_enable_current(void)
{
	current->kasan_depth++;
}

/* Disable reporting bugs for current task */
static inline void kasan_disable_current(void)
{
	current->kasan_depth--;
}

void kasan_unpoison_shadow(const void *address, size_t size);
void kasan_poison_pshadow(const void *address, size_t size);
void kasan_unpoison_pshadow(const void *address, size_t size);
int kasan_stack_alloc(const void *address, size_t size);
void kasan_stack_free(const void *addr, size_t size);
int kasan_slab_page_alloc(const void *address, size_t size, gfp_t flags);
void kasan_slab_page_free(const void *addr, size_t size);
bool kasan_free_buddy(struct page *page, unsigned int order,
			unsigned int max_order);

void kasan_unpoison_task_stack(struct task_struct *task);

void kasan_alloc_pages(struct page *page, unsigned int order);
void kasan_free_pages(struct page *page, unsigned int order);

void kasan_poison_slab(struct page *page);
void kasan_unpoison_object_data(struct kmem_cache *cache, void *object);
void kasan_poison_object_data(struct kmem_cache *cache, void *object);

int kasan_kmalloc_large(const void *ptr, size_t size, gfp_t flags);
void kasan_kfree_large(const void *ptr);
void kasan_kfree(void *ptr);
void kasan_kmalloc(struct kmem_cache *s, const void *object, size_t size);
void kasan_krealloc(const void *object, size_t new_size, gfp_t flags);

void kasan_slab_alloc(struct kmem_cache *s, void *object);
void kasan_slab_free(struct kmem_cache *s, void *object);

int kasan_bootmem_fixup(struct page *page, unsigned int order);

int kasan_module_alloc(void *addr, size_t size);
void kasan_free_shadow(const struct vm_struct *vm);

#else /* CONFIG_KASAN */

static inline void kasan_unpoison_shadow(const void *address, size_t size) {}
static inline void kasan_poison_pshadow(const void *address, size_t size) {}
static inline void kasan_unpoison_pshadow(const void *address, size_t size) {}
static inline int kasan_stack_alloc(const void *address,
					size_t size) { return 0; }
static inline void kasan_stack_free(const void *addr, size_t size) {}
static inline int kasan_slab_page_alloc(const void *address, size_t size,
					gfp_t flags) { return 0; }
static inline void kasan_slab_page_free(const void *addr, size_t size) {}
static inline bool kasan_free_buddy(struct page *page, unsigned int order,
			unsigned int max_order) { return false; }

static inline void kasan_unpoison_task_stack(struct task_struct *task) {}

static inline void kasan_enable_current(void) {}
static inline void kasan_disable_current(void) {}

static inline void kasan_alloc_pages(struct page *page, unsigned int order) {}
static inline void kasan_free_pages(struct page *page, unsigned int order) {}

static inline void kasan_poison_slab(struct page *page) {}
static inline void kasan_unpoison_object_data(struct kmem_cache *cache,
					void *object) {}
static inline void kasan_poison_object_data(struct kmem_cache *cache,
					void *object) {}

static inline int kasan_kmalloc_large(void *ptr, size_t size,
				gfp_t flags) { return 0; }
static inline void kasan_kfree_large(const void *ptr) {}
static inline void kasan_kfree(void *ptr) {}
static inline void kasan_kmalloc(struct kmem_cache *s, const void *object,
				size_t size) {}
static inline void kasan_krealloc(const void *object, size_t new_size,
				gfp_t flags) {}

static inline void kasan_slab_alloc(struct kmem_cache *s, void *object) {}
static inline void kasan_slab_free(struct kmem_cache *s, void *object) {}

static inline int kasan_bootmem_fixup(struct page *page,
			unsigned int order) { return 0; }

static inline int kasan_module_alloc(void *addr, size_t size) { return 0; }
static inline void kasan_free_shadow(const struct vm_struct *vm) {}

#endif /* CONFIG_KASAN */

#endif /* LINUX_KASAN_H */
