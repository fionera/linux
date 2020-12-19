#ifndef _LINUX_ROMEMPOOL_H
#define _LINUX_ROMEMPOOL_H

#include <linux/types.h>

enum romempool_flags {
	RMP_PGD = 0,
	RMP_PUD,
	RMP_PMD,
	RMP_PTE_KERNEL,
	RMP_PTE_USER,
	RMP_SPECIAL,
	RMP_EMERGENCY,
	RMP_SLAB,
	NR_RMP_FLAGS
};

#ifdef CONFIG_EKP_ROMEMPOOL
#define RO_MEMPOOL_SIZE		CONFIG_EKP_ROMEMPOOL_SIZE

/*
 * Note that 2048 means "4 GB Physical Memory / PMD_SIZE"
 */
#define ROMEMPOOL_PHYS_SIZE	2048
extern DECLARE_BITMAP(romempool_phys, ROMEMPOOL_PHYS_SIZE);

extern char __romempool_start[];
extern char __static_pgtab_end[];

void romempool_reserve(bool init);
void romempool_init(void);
void mark_romempool_ro(void);
unsigned long *romempool_alloc(gfp_t gfpmask, enum romempool_flags flag,
			       unsigned long order);
struct page *romempool_alloc_pages(gfp_t gfpmask,
				   enum romempool_flags flag,
				   unsigned long order);
void romempool_free(unsigned long addr, enum romempool_flags flag,
		    unsigned long order);
unsigned long *romempool_early_alloc(unsigned long order);
void *romempool_vmemmap_alloc_pgtable(enum romempool_flags flag);
bool romempool_free_init_pgtable(phys_addr_t phys);
#ifdef CONFIG_HIBERNATION
void mark_free_romempool(void);
#endif

#define is_romempool_addr(addr)		is_romempool(__pa(addr))
bool is_romempool(phys_addr_t phy);

static inline bool ekp_is_romem(phys_addr_t phy)
{
	return is_romempool(phy);
}

#else	/* CONFIG_EKP_ROMEMPOOL */

#define RO_MEMPOOL_SIZE		0

static inline void romempool_reserve(bool init) { }
static inline void romempool_init(void) { }
static inline void mark_romempool_ro(void) { }
static inline bool is_romempool_addr(unsigned long addr)
{
	return false;
}
static inline unsigned long *romempool_alloc(gfp_t gfpmask,
					     enum romempool_flags flag,
					     unsigned long order)
{
	return NULL;
}
static inline struct page *romempool_alloc_pages(gfp_t gfpmask,
						 enum romempool_flags flag,
						 unsigned long order)
{
	return NULL;
}
static inline void romempool_free(unsigned long addr,
				  enum romempool_flags flag,
				  unsigned long order) { }
static inline unsigned long *romempool_early_alloc(unsigned long order)
{
	return NULL;
}
static inline bool romempool_free_init_pgtable(phys_addr_t phys)
{
	return false;
}
#ifdef CONFIG_HIBERNATION
static inline void mark_free_romempool(void) { }
#endif
#endif	/* CONFIG_EKP_ROMEMPOOL */

#endif	/* _LINUX_ROMEMPOOL_H */
