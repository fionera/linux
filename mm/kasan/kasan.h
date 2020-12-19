#ifndef __MM_KASAN_KASAN_H
#define __MM_KASAN_KASAN_H

#include <linux/kasan.h>

#define KASAN_SHADOW_SCALE_SIZE (1UL << KASAN_SHADOW_SCALE_SHIFT)
#define KASAN_SHADOW_MASK       (KASAN_SHADOW_SCALE_SIZE - 1)

#define KASAN_FREE_PAGE         0xFF  /* page was freed */
#define KASAN_PAGE_REDZONE      0xFE  /* redzone for kmalloc_large allocations */
#define KASAN_KMALLOC_REDZONE   0xFC  /* redzone inside slub object */
#define KASAN_KMALLOC_FREE      0xFB  /* object was freed (kmem_cache_free/kfree) */
#define KASAN_GLOBAL_REDZONE    0xFA  /* redzone for global variable */

#define KASAN_PER_PAGE_BYPASS	0xFF  /* page should be checked by per-byte shadow */
#define KASAN_PER_PAGE_FREE	0xFE  /* page was freed */

/*
 * Stack redzone shadow values
 * (Those are compiler's ABI, don't change them)
 */
#define KASAN_STACK_LEFT        0xF1
#define KASAN_STACK_MID         0xF2
#define KASAN_STACK_RIGHT       0xF3
#define KASAN_STACK_PARTIAL     0xF4

/* Don't break randconfig/all*config builds */
#ifndef KASAN_ABI_VERSION
#define KASAN_ABI_VERSION 1
#endif

struct kasan_access_info {
	const void *access_addr;
	const void *first_bad_addr;
	size_t access_size;
	bool is_write;
	unsigned long ip;
};

/* The layout of struct dictated by compiler */
struct kasan_source_location {
	const char *filename;
	int line_no;
	int column_no;
};

/* The layout of struct dictated by compiler */
struct kasan_global {
	const void *beg;		/* Address of the beginning of the global variable. */
	size_t size;			/* Size of the global variable. */
	size_t size_with_redzone;	/* Size of the variable + size of the red zone. 32 bytes aligned */
	const void *name;
	const void *module_name;	/* Name of the module where the global variable is declared. */
	unsigned long has_dynamic_init;	/* This needed for C++ */
#if KASAN_ABI_VERSION >= 4
	struct kasan_source_location *location;
#endif
#if KASAN_ABI_VERSION >= 5
	char *odr_indicator;
#endif
};

extern unsigned long kasan_zero_page_pfn;
extern unsigned long kasan_black_page_pfn;

#ifdef HAVE_KASAN_PER_PAGE_SHADOW
void arch_kasan_map_shadow(unsigned long s, unsigned long e);
bool arch_kasan_recheck_prepare(unsigned long addr, size_t size);

static inline bool kasan_pshadow_inited(void) { return true; }

#else
static inline void arch_kasan_map_shadow(unsigned long s, unsigned long e) { }
static inline bool arch_kasan_recheck_prepare(unsigned long addr,
		size_t size) { return false; }

static inline bool kasan_pshadow_inited(void) { return false; }
#endif

static inline bool kasan_report_enabled(void)
{
	return !current->kasan_depth;
}

void __kasan_report(unsigned long addr, size_t size,
		bool is_write, unsigned long ip);
void kasan_report(unsigned long addr, size_t size,
		bool is_write, unsigned long ip);

#endif
