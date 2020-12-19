#include <linux/bootmem.h>
#include <linux/kasan.h>
#include <linux/kernel.h>
#include <linux/memblock.h>
#include <linux/start_kernel.h>

#include <asm/cputype.h>
#include <asm/highmem.h>
#include <asm/mach/map.h>
#include <asm/memory.h>
#include <asm/page.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/procinfo.h>
#include <asm/proc-fns.h>
#include <asm/tlbflush.h>
#include <asm/sections.h>

#include <asm/smp_plat.h>

#include "mm.h"

#define cpu_get_ttbr(nr)					\
	({							\
		unsigned long ttbr;				\
		__asm__("mrc	p15, 0, %0, c2, c0, 0"		\
			: "=r" (ttbr));				\
		ttbr;						\
	})

#define cpu_set_ttbr0(val)					\
	do {							\
		u64 ttbr = val;					\
		__asm__("mcr	p15, 0, %0, c2, c0, 0"		\
			: : "r" (ttbr));			\
	} while (0)

static pgd_t tmp_page_table[PTRS_PER_PGD] __initdata __aligned(1ULL << 14);

static __init void *kasan_alloc_block(size_t size, int node)
{
	void *ptr;
#ifdef CONFIG_NO_BOOTMEM
	ptr = __va(memblock_alloc_nid(size, size, node));
#else
	ptr = alloc_bootmem_low_pages_node(NODE_DATA(node), size);
#endif
	memset(ptr, 0, size);
	return ptr;
}

static void __init kasan_early_pmd_populate(unsigned long start, unsigned long end, pud_t *pud)
{
	unsigned long addr;
	unsigned long next;
	pmd_t *pmd;

	pmd = pmd_offset(pud, start);
	for (addr = start; addr < end;) {
		pmd_populate_kernel(&init_mm, pmd, kasan_zero_pte);
		next = pmd_addr_end(addr, end);
		addr = next;
		flush_pmd_entry(pmd);
		pmd++;
	}
}

static void __init kasan_early_pud_populate(unsigned long start, unsigned long end, pgd_t *pgd)
{
	unsigned long addr;
	unsigned long next;
	pud_t *pud;

	pud = pud_offset(pgd, start);
	for (addr = start; addr < end;) {
		next = pud_addr_end(addr, end);
		kasan_early_pmd_populate(addr, next, pud);
		addr = next;
		pud++;
	}
}

void __init __kasan_map_early_shadow(unsigned long start, unsigned long end)
{
	unsigned long addr;
	unsigned long next;
	pgd_t *pgd;

	pgd = pgd_offset_k(start);
	for (addr = start; addr < end;) {
		next = pgd_addr_end(addr, end);
		kasan_early_pud_populate(addr, next, pgd);
		addr = next;
		pgd++;
	}
}

void __init kasan_map_early_shadow(pgd_t *pgdp)
{
	int i;
	unsigned long kernel_offset;

	for (i = 0; i < PTRS_PER_PTE; i++)
		set_pte_at(&init_mm, KASAN_SHADOW_START + i*PAGE_SIZE,
			&kasan_zero_pte[i], pfn_pte(
				virt_to_pfn(kasan_zero_page),
				__pgprot(_L_PTE_DEFAULT | L_PTE_DIRTY | L_PTE_XN)));

	/* Prepare black shadow memory */
	for (i = 0; i < PTRS_PER_PTE; i++)
		set_pte_at(&init_mm, KASAN_SHADOW_START + i*PAGE_SIZE,
			&kasan_black_pte[i], pfn_pte(
				virt_to_pfn(kasan_black_page),
				__pgprot(_L_PTE_DEFAULT | L_PTE_DIRTY | L_PTE_XN)));

	__kasan_map_early_shadow(KASAN_SHADOW_START, KASAN_SHADOW_END);

	/* VMALLOC_START isn't inited so we need to wait. Do temporal init */
	kernel_offset = (unsigned long)kasan_shadow_to_mem(
					(void *)KASAN_SHADOW_START);
	kasan_pshadow_offset = KASAN_SHADOW_START -
				(kernel_offset >> PAGE_SHIFT);
}

extern struct proc_info_list *lookup_processor_type(unsigned int);

void __init kasan_early_init(void)
{
	struct proc_info_list *list;

	/*
	 * locate processor in the list of supported processor
	 * types.  The linker builds this table for us from the
	 * entries in arch/arm/mm/proc-*.S
	 */
	list = lookup_processor_type(read_cpuid_id());
	if (!list)
		BUG();

#ifdef MULTI_CPU
	processor = *list->proc;
#endif

	kasan_map_early_shadow(swapper_pg_dir);
	start_kernel();
}

static void __init clear_pgds(unsigned long start,
			unsigned long end)
{
	for (; start && start < end; start += PMD_SIZE)
		pmd_clear(pmd_off_k(start));
}

pte_t * __meminit kasan_pte_populate(pmd_t *pmd, unsigned long addr, int node)
{
	pte_t *pte = pte_offset_kernel(pmd, addr);

	/* Allow overwrite on the mapping to the kasan_(zero|black)_page */
	if (pte_none(*pte) ||
		(pte_pfn(*pte) == virt_to_pfn(kasan_black_page) ||
		(pte_pfn(*pte) == virt_to_pfn(kasan_zero_page)))) {
		pte_t entry;
		void *p = kasan_alloc_block(PAGE_SIZE, node);
		if (!p)
			return NULL;
		entry = pfn_pte(virt_to_pfn(p), PAGE_KERNEL);
		set_pte_at(&init_mm, addr, pte, entry);
	}
	return pte;
}

pmd_t * __meminit kasan_pmd_populate(pud_t *pud, unsigned long addr, int node)
{
	pmd_t *pmd = pmd_offset(pud, addr);
	if (pmd_none(*pmd)) {
		void *p = kasan_alloc_block(PAGE_SIZE, node);
		if (!p)
			return NULL;
		pmd_populate_kernel(&init_mm, pmd, p);
	}
	return pmd;
}

pud_t * __meminit kasan_pud_populate(pgd_t *pgd, unsigned long addr, int node)
{
	pud_t *pud = pud_offset(pgd, addr);
	if (pud_none(*pud)) {
		void *p = kasan_alloc_block(PAGE_SIZE, node);
		if (!p)
			return NULL;
		pr_err("populating pud addr %lx\n", addr);
		pud_populate(&init_mm, pud, p);
	}
	return pud;
}

pgd_t * __meminit kasan_pgd_populate(unsigned long addr, int node)
{
	pgd_t *pgd = pgd_offset_k(addr);
	if (pgd_none(*pgd)) {
		void *p = kasan_alloc_block(PAGE_SIZE, node);
		if (!p)
			return NULL;
		pgd_populate(&init_mm, pgd, p);
	}
	return pgd;
}

static int __init create_mapping(unsigned long start, unsigned long end, int node)
{
	unsigned long addr = start;
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	pr_info("populating shadow for %lx, %lx\n", start, end);
	for (; addr < end; addr += PAGE_SIZE) {
		pgd = kasan_pgd_populate(addr, node);
		if (!pgd)
			return -ENOMEM;

		pud = kasan_pud_populate(pgd, addr, node);
		if (!pud)
			return -ENOMEM;

		pmd = kasan_pmd_populate(pud, addr, node);
		if (!pmd)
			return -ENOMEM;

		pte = kasan_pte_populate(pmd, addr, node);
		if (!pte)
			return -ENOMEM;
	}
	return 0;
}


void __init kasan_init(void)
{
	struct memblock_region *reg;
	u64 orig_ttbr0;
	int i;

	kasan_early_init_pshadow();

	__kasan_map_early_shadow(KASAN_PSHADOW_START, KASAN_PSHADOW_END);

	orig_ttbr0 = cpu_get_ttbr(0);

	memcpy(tmp_page_table, swapper_pg_dir, sizeof(tmp_page_table));
	cpu_set_ttbr0(__pa(tmp_page_table));

	flush_cache_all();
	local_flush_tlb_all();

	clear_pgds(KASAN_SHADOW_START, KASAN_SHADOW_END);

	kasan_populate_shadow(
		(void *)KASAN_SHADOW_START,
		kasan_mem_to_shadow((void *)KASAN_SHADOW_END),
		true, false);

	kasan_populate_shadow(
		kasan_mem_to_shadow((void *)PKMAP_BASE),
		kasan_mem_to_shadow((void *)PKMAP_BASE + PMD_SIZE),
		true, false);

	kasan_populate_shadow(kasan_mem_to_shadow((void *)VMALLOC_START),
				kasan_mem_to_shadow((void *)-1UL) + 1,
				true, false);

	for_each_memblock(memory, reg) {
		void *start = __va(reg->base);
		void *end = __va(reg->base + reg->size);

		if (reg->base + reg->size > arm_lowmem_limit)
			end = __va(arm_lowmem_limit);
		if (start >= end)
			break;

#ifdef CONFIG_KASAN_INLINE
		/*
		 * This optimization isn't stable now and benefit is not
		 * considerable in KASAN_OUTLINE build.
		 */
		kasan_populate_shadow(kasan_mem_to_shadow(start),
				kasan_mem_to_shadow(end),
				true, true);
#else
		kasan_populate_shadow(kasan_mem_to_shadow(start),
				kasan_mem_to_shadow(end),
				false, true);
#endif
	}

	create_mapping((unsigned long)kasan_mem_to_shadow((void *)MODULES_VADDR),
			(unsigned long)kasan_mem_to_shadow((void *)MODULES_END),
			NUMA_NO_NODE);

	/* global variable will be checked by original shadow memory */
	create_mapping((unsigned long)kasan_mem_to_shadow(_text),
			(unsigned long)kasan_mem_to_shadow(
				(void *)ALIGN((unsigned long)_end,
					PAGE_SIZE << KASAN_SHADOW_SCALE_SHIFT)),
			NUMA_NO_NODE);

	/* For per-page shadow */
	clear_pgds(KASAN_PSHADOW_START, KASAN_PSHADOW_END);

	kasan_populate_shadow((void *)KASAN_PSHADOW_START,
			kasan_mem_to_pshadow((void *)KASAN_SHADOW_END),
			true, false);

	kasan_populate_shadow(
		kasan_mem_to_pshadow((void *)PKMAP_BASE),
		kasan_mem_to_pshadow((void *)PKMAP_BASE + PMD_SIZE),
		true, false);

	kasan_populate_shadow(kasan_mem_to_pshadow((void *)VMALLOC_START),
				kasan_mem_to_pshadow((void *)-1UL) + 1,
				true, false);

	for_each_memblock(memory, reg) {
		void *start = __va(reg->base);
		void *end = __va(reg->base + reg->size);

		if (reg->base + reg->size > arm_lowmem_limit)
			end = __va(arm_lowmem_limit);
		if (start >= end)
			break;

		create_mapping((unsigned long)kasan_mem_to_pshadow(start),
			(unsigned long)kasan_mem_to_pshadow(end),
			NUMA_NO_NODE);
	}

	kasan_populate_shadow(
		kasan_mem_to_pshadow((void *)MODULES_VADDR),
		kasan_mem_to_pshadow((void *)MODULES_END),
		false, false);

	/* global variable will be checked by original shadow memory */
	kasan_poison_pshadow(_text, ALIGN(_end - _text, PAGE_SIZE));

	/* Resetup kasan_zero_pte with buffer/cache policy */
	for (i = 0; i < PTRS_PER_PTE; i++) {
		set_pte_at(&init_mm, KASAN_SHADOW_START + i*PAGE_SIZE,
			&kasan_zero_pte[i], pfn_pte(
				virt_to_pfn(kasan_zero_page),
				PAGE_KERNEL | L_PTE_RDONLY));
	}

	cpu_set_ttbr0(orig_ttbr0);
	flush_cache_all();
	local_flush_tlb_all();

	__memset(kasan_zero_page, 0, PAGE_SIZE);
	pr_info("Kernel address sanitizer initialized\n");
	init_task.kasan_depth = 0;
}

void arch_kasan_map_shadow(unsigned long s, unsigned long e)
{

	/*
	 * If global TLB flush needs broadcast, it will cause deadlock.
	 * We could handle that case with complex implementation however
	 * avoid it until we really need it.
	 */
	BUG_ON(tlb_ops_need_broadcast());
	flush_tlb_all();
}

/* To fix-up stale TLB problem */
bool arch_kasan_recheck_prepare(unsigned long addr, size_t size)
{
	return false;
}
