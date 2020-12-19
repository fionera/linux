/*
 * arch/arm64/ekp/ekp_v8.c
 *
 * Copyright (C) 2018 LG Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/ekp.h>
#include <linux/memblock.h>
#include <linux/mm.h>
#include <linux/romempool.h>

#include <asm/cacheflush.h>
#include <asm/ekp.h>
#include <asm/pgtable.h>
#include <asm-generic/getorder.h>
#include <asm-generic/sections.h>

#include "ekp_debug.h"

void __init
cpu_init_ksection_info(struct ekp_init_ksection_info *info)
{
	info->ktext_start = __pa(_stext);
	info->ktext_end = __pa(__start_rodata);
	info->krodata_end = __pa(_etext);
}

void __init cpu_init_arch_info(struct ekp_init_arch_info *info)
{
	info->idmap_pg_dir = __pa(idmap_pg_dir);
	info->__init_end_aligned = round_up((unsigned long)__init_end,
					    SWAPPER_BLOCK_SIZE);
	info->vmemmap_start = VMEMMAP_START;
	info->vmemmap_size = VMEMMAP_SIZE;
}

#ifdef CONFIG_KUSER_HELPERS
extern struct page *kuser_helpers_page;
#endif

void cpu_extra_arch_info(struct ekp_extra_arch_info *info)
{
#ifdef CONFIG_KUSER_HELPERS
	info->kuser_helpers_page = page_to_phys(kuser_helpers_page);
#endif
}

void cpu_dcache_flush_area(void *addr, size_t len)
{
	__flush_dcache_area(addr, len);
}

/*
 * Note that fixmap_remap_fdt() @ mmu.c uses create_mapping() and this
 * function can use early_alloc() before EKP is initialized.
 * However, fixmap_remap_fdt() doesn't actually allocate some memory
 * via early_alloc() since it uses fixmap area.
 *
 * You should take care of this if mainline kernel is changed to use
 * early_alloc() before EKP is initialized.
 */
void __init *ekp_early_alloc(unsigned long sz)
{
	phys_addr_t phys;
	void *ptr;
	long ret;

	if (ekp_initialized())
		BUG_ON(sz % PAGE_SIZE != 0);

	if (ekp_initialized() && IS_ENABLED(CONFIG_EKP_ROMEMPOOL)) {
		ptr = (void *)romempool_early_alloc(get_order(sz));
		BUG_ON(!ptr);
		memset(ptr, 0, sz);
	}
	else {
		phys = memblock_alloc(sz, sz);
		BUG_ON(!phys);
		ptr = __va(phys);

		/*
		 * ekp_set_romem() clears memory allocated, so we don't
		 * need to call memset() when ekp_set_romem() returns 0.
		 */
		ret = ekp_set_romem(phys, get_order(sz), true);
		if (ret != 0)
			memset(ptr, 0, sz);
	}

	return ptr;
}

#define pud_offset_k(addr)		pud_offset(pgd_offset_k(addr), addr)
#define pmd_offset_k(addr)		pmd_offset(pud_offset_k(addr), addr)
#define pgd_offset_target(pgd, addr)	(pgd + pgd_index(addr))
#define pud_offset_target(pgd, addr)	pud_offset(pgd_offset_target(pgd, addr), addr)
#define pmd_offset_target(pgd, addr)	pmd_offset(pud_offset_target(pgd, addr), addr)

/* Hardware page table definitions */
#define PUD_TABLE_NSTABLE		(_AT(pudval_t, 1) << 63)
#define PUD_TABLE_RDONLY		(_AT(pudval_t, 1) << 62)
#define PUD_TABLE_UXNTABLE		(_AT(pudval_t, 1) << 60)
#define PUD_TABLE_PXNTABLE		(_AT(pudval_t, 1) << 59)
#define PMD_TABLE_NSTABLE		PUD_TABLE_NSTABLE
#define PMD_TABLE_RDONLY		PUD_TABLE_RDONLY
#define PMD_TABLE_UXNTABLE		PUD_TABLE_UXNTABLE
#define PMD_TABLE_PXNTABLE		PUD_TABLE_PXNTABLE

#ifdef CONFIG_EKP_DEBUGFS_KERNEL_INFO
bool cpu_is_ro_table(unsigned long addr)
{
	return ekp_is_romem(__pa(addr));
}

static void print_separator(unsigned long addr)
{
	if (addr == 0x0UL)
		printk("[User Mapping]\n");
	else if (addr == VMALLOC_START)
		printk("[vmalloc Area]\n");
	/* Areas for fixaddr, pci io, and modules are included in this area */
	else if (addr == VMEMMAP_START)
		printk("[vmemmap/fixmap/pci io/module Area]\n");
	else if (addr == PAGE_OFFSET)
		printk("[Kernel Mapping]\n");
}

static void print_descriptor(unsigned long start, unsigned long end,
			     u64 val, bool block_map, bool level3,
			     char *indicator)
{
	char *map;
	unsigned long ro_flag, uxn_flag, pxn_flag;

	if (level3) {
		map = "P";
		ro_flag = PTE_RDONLY;
		uxn_flag = PTE_UXN;
		pxn_flag = PTE_PXN;
	}
	else if (block_map) {
		map = "B";
		ro_flag = PMD_SECT_RDONLY;
		uxn_flag = PMD_SECT_UXN;
		pxn_flag = PMD_SECT_PXN;
	}
	else {
		map = "T";
		ro_flag = PUD_TABLE_RDONLY;
		uxn_flag = PUD_TABLE_UXNTABLE;
		pxn_flag = PUD_TABLE_PXNTABLE;
	}

	printk("%s[0x%lx-0x%lx] (%s/%s/%s/%s)\n",
		indicator, start, end, val ? map : "-",
		(val & ro_flag) ? "R-" : (val ? "RW" : "--"),
		(val & uxn_flag) ? "UXN" : "---",
		(val & pxn_flag) ? "PXN" : "---");
}

static inline pteval_t pte_mask_attr(pteval_t pteval)
{
	return pte_val(pteval) & (PTE_RDONLY | PTE_UXN | PTE_PXN);
}

static void walk_and_print_pte(unsigned long addr, pgd_t *target, bool dump)
{
	unsigned long i, start, end;
	char *indicator = "      ";
	pmd_t *pmdp = pmd_offset_target(target, addr);
	pte_t *ptep = pte_offset_kernel(pmdp, addr);
	pteval_t prot;
	bool prev_map = false;

	printk("%sPTE: 0x%p = %s\n", indicator, ptep,
	       cpu_is_ro_table((unsigned long)ptep) ? "RO" : "Writable");

	if (!dump)
		return;

	for (i = 0; i < PTRS_PER_PTE; i++) {
		if (!prev_map)
			start = addr + PAGE_SIZE * i;
		end = addr + PAGE_SIZE * (i + 1);

		if (!prev_map) {
			if (!pte_none(*ptep)) {
				prot = pte_mask_attr(*ptep);

				prev_map = true;
			}
		}
		else {
			if (prot != pte_mask_attr(*ptep)) {
				/* Print previous contiguous mappings */
				print_descriptor(start, end - PAGE_SIZE, prot,
						 false, true, indicator);

				if (!pte_none(*ptep)) {
					/* Start new contiguous mappings */
					start = end - PAGE_SIZE;
					prot = pte_mask_attr(*ptep);
				}
				else
					prev_map = false;
			}
		}

		ptep++;
	}

	if (prev_map)
		print_descriptor(start, end, prot, false, true, indicator);
}

static inline bool pmd_block(pmdval_t pmdval)
{
	return (pmd_val(pmdval) & PMD_TYPE_MASK) == PMD_TYPE_SECT;
}
static inline pmdval_t pmd_mask_attr(pmdval_t pmdval)
{
	return pmd_val(pmdval) & (PMD_SECT_RDONLY | PMD_SECT_UXN | PMD_SECT_PXN);
}

static void walk_and_print_pmd(unsigned long addr, pgd_t *target, bool dump)
{
	unsigned long i, start, end;
	bool prev_block_map = false;
	char *indicator = "    ";
	pmd_t *pmdp = pmd_offset_target(target, addr);
	pmdval_t prot;

	printk("%sPMD: 0x%p = %s\n", indicator, pmdp,
	       cpu_is_ro_table((unsigned long)pmdp) ? "RO" : "Writable");

	for (i = 0; i < PTRS_PER_PMD; i++) {
		if (!prev_block_map)
			start = addr + PMD_SIZE * i;
		end = addr + PMD_SIZE * (i + 1);

		if (!pmd_block(*pmdp)) {
			/* Print previous contiguous block mappings */
			if (prev_block_map) {
				if (dump)
					print_descriptor(start, end - PMD_SIZE, prot,
							 true, false, indicator);
				start = end - PMD_SIZE;
			}
			/* Print current table mapping if it exists */
			if (!pmd_none(*pmdp)) {
				if (dump)
					print_descriptor(start, end, pmd_val(*pmdp),
							 false, false, indicator);
				walk_and_print_pte(start, target, dump);
			}

			prev_block_map = false;
		}
		else {
			/* Start contiguous mappings */
			if (!prev_block_map) {
				prot = pmd_mask_attr(*pmdp);

				prev_block_map = true;
			}
			else {
				if (prot != pmd_mask_attr(*pmdp)) {
					if (dump)
						/* Print previous contiguous mappings */
						print_descriptor(start, end - PMD_SIZE,
								 prot, true, false,
								 indicator);

					/* Start new contiguous mappings */
					start = end - PMD_SIZE;
					prot = pmd_mask_attr(*pmdp);
				}
			}
		}

		pmdp++;
	}

	if (prev_block_map)
		print_descriptor(start, end, prot, true, false, indicator);
}

static inline bool pgd_block(pgdval_t pgdval)
{
	return (pgd_val(pgdval) & PUD_TYPE_MASK) == PUD_TYPE_SECT;
}

void cpu_pt_dump(pgd_t *pgd)
{
	int i = 0;
	pgd_t *pgd_i = pgd;
	unsigned long addr;
	bool kernel_only = (pgd == swapper_pg_dir);
	unsigned long base = kernel_only ? VA_START : 0;
	bool block;
	char *indicator = "  ";

	for (i = 0; i < PTRS_PER_PGD; i++) {
		addr = base + (i * PGDIR_SIZE);

		if (kernel_only)
			print_separator(addr);

		if (!pgd_val(*pgd_i))
			goto next;

		block = pgd_block(*pgd_i);
		print_descriptor(addr, addr + PGDIR_SIZE,
				 pgd_val(*pgd_i), block, false,
				 indicator);

		if (!block)
			walk_and_print_pmd(addr, pgd, kernel_only);

next:
		pgd_i++;
	}
}
#endif	/* CONFIG_EKP_DEBUGFS_KERNEL_INFO */

bool cpu_pgd_walk_for_checking_pxn(pgd_t *pgd)
{
	int i;
	pgdval_t descriptor;

	for (i = 0; i < PTRS_PER_PGD; i++) {
		if (!pgd_val(*pgd))
			goto next;

		descriptor = pgd_val(*pgd);
		if (!(descriptor & PUD_TABLE_PXNTABLE))
			return false;

next:
		pgd++;
	}

	return true;
}

void cpu_report(void)
{
#ifdef CONFIG_KUSER_HELPERS
	bool ro_kuserhelpers_page;

	ro_kuserhelpers_page = ekp_is_romem(page_to_phys(kuser_helpers_page));
	printk("ekp: Read-only kuserhelpers [%s]\n",
	       ro_kuserhelpers_page ? "OK" : "FAIL");
#endif
}

#ifdef CONFIG_EKP_DEBUGFS_TEST_HELPERS_FOR_HACKING
static void inline print_attributes(unsigned long long val, bool block)
{
	if (!val)
		printk("\t-----/--/---/---\n");
	else if (block)
		printk("\t%s/%s/%s/%s\n",
			"Block",
			(val & PMD_SECT_RDONLY) ? "R-" : "RW",
			(val & PMD_SECT_PXN) ? "PXN" : "---",
			(val & PMD_SECT_UXN) ? "UXN" : "---");
	else
		printk("\t%s/%s/%s/%s\n",
			"Table",
			(val & PUD_TABLE_RDONLY) ? "R-" : "RW",
			(val & PUD_TABLE_PXNTABLE) ? "PXN" : "---",
			(val & PUD_TABLE_UXNTABLE) ? "UXN" : "---");
}

void cpu_test_pt_modify(struct ekp_test_pt_modify *info)
{
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	pud = pud_offset_target(info->pgd, info->addr);

	printk("addr: 0x%lx, pgd: 0x%p\n", info->addr,
	       pgd_offset_target(info->pgd, info->addr));
	printk("pud/*pud: 0x%p/0x%llx\n", pud, pud_val(*pud));
	print_attributes(pud_val(*pud), pud_sect(*pud));

	if (info->level == PGD_LEVEL) {
		if (info->set) {
			printk("Setting PUD...\n");

			ekp_tunnel(EKP_SET_PUD, (u64)__pa(pud), (u64)info->val, 0,
				   0, 0, 0);
			dsb(ishst);
			isb();

			printk("pud/*pud: 0x%p/0x%llx\n", pud, pud_val(*pud));
			print_attributes(pud_val(*pud), pud_sect(*pud));
		}

		return;
	}

	/* We cannot walk into a pmd in this case */
	if (pud_none(*pud) || pud_sect(*pud))
		return;

	pmd = pmd_offset_target(info->pgd, info->addr);

	printk("pmd/*pmd: 0x%p/0x%llx\n", pmd, pmd_val(*pmd));
	print_attributes(pmd_val(*pmd), pmd_sect(*pmd));

	if (info->level == PMD_LEVEL) {
		if (info->set) {
			printk("Setting PMD...\n");

			ekp_tunnel(EKP_SET_PMD, (u64)__pa(pmd), (u64)info->val, 0,
				   0, 0, 0);
			dsb(ishst);
			isb();

			printk("pmd/*pmd: 0x%p/0x%llx\n", pmd, pmd_val(*pmd));
			print_attributes(pmd_val(*pmd), pmd_sect(*pmd));
		}

		return;
	}

	/* We cannot walk into a pte in this case */
	if (pmd_none(*pmd) || pmd_sect(*pmd))
		return;

	pte = pte_offset_kernel(pmd, info->addr);

	printk("pte/*pte: 0x%p/0x%llx\n", pte, pte_val(*pte));
	print_attributes(pte_val(*pte), true);

	if (info->level == PTE_LEVEL) {
		if (info->set) {
			printk("Setting PTE...\n");

			ekp_tunnel(EKP_SET_PTE, (u64)__pa(pte), (u64)info->val, 0,
				   0, 0, 0);
			dsb(ishst);
			isb();

			printk("pte/*pte: 0x%p/0x%llx\n", pte, pte_val(*pte));
			print_attributes(pte_val(*pte), true);
		}

		return;
	}

	/* Don't get here */
	printk("AArch64 doesn't support this level\n");
}

static inline void deadbeef(unsigned long *ptr)
{
	*ptr = 0xdeadbeef;
}

/*
 * d65f03c0	ret
 * d503201f	nop
 * d503201f	nop
 * d503201f	nop
 */
static unsigned long shellcode[] = {
	0xd503201fd65f03c0,
	0xd503201fd503201f,
};
typedef void (*shell_func)(void);

static void exec_shellcode(unsigned long *ptr)
{
	int size = sizeof(shellcode) / sizeof(unsigned long);
	int i;
	shell_func start_ptr = (shell_func)ptr;

	for (i = 0; i < size; i++) {
		*ptr = shellcode[i];
		ptr++;
	}

	start_ptr();
}

void cpu_test_attack_ops(unsigned long target)
{
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	bool write_to_ro_area = false;
	bool execute_on_xn_area = false;

	pud = pud_offset_k(target);

	printk("addr: 0x%lx, pgd: 0x%p\n", target, pgd_offset_k(target));
	printk("pud/*pud: 0x%p/0x%llx\n", pud, pud_val(*pud));
	print_attributes(pud_val(*pud), pud_sect(*pud));

	if (pud_none(*pud)) {
		printk("No mapping found...\n");
		return;
	}
	else if (pud_sect(*pud)) {
		if (pud_val(*pud) & PMD_SECT_RDONLY)
			write_to_ro_area = true;
		if ((pud_val(*pud) & PMD_SECT_PXN) ||
		    (pud_val(*pud) & PMD_SECT_UXN))
			execute_on_xn_area = true;
		goto attack;
	}
	else {
		if (pud_val(*pud) & PUD_TABLE_RDONLY)
			write_to_ro_area = true;
		if ((pud_val(*pud) & PUD_TABLE_PXNTABLE) ||
		    (pud_val(*pud) & PUD_TABLE_UXNTABLE))
			execute_on_xn_area = true;
	}

	pmd = pmd_offset_k(target);

	printk("pmd/*pmd: 0x%p/0x%llx\n", pmd, pmd_val(*pmd));
	print_attributes(pmd_val(*pmd), pmd_sect(*pmd));

	if (pmd_none(*pmd)) {
		printk("No mapping found...\n");
		return;
	}
	else if (pmd_sect(*pmd)) {
		if (pmd_val(*pmd) & PMD_SECT_RDONLY)
			write_to_ro_area = true;
		if ((pmd_val(*pmd) & PMD_SECT_PXN) ||
		    (pmd_val(*pmd) & PMD_SECT_UXN))
			execute_on_xn_area = true;
		goto attack;
        }
	else {
		if (pmd_val(*pmd) & PMD_TABLE_RDONLY)
			write_to_ro_area = true;
		if ((pmd_val(*pmd) & PMD_TABLE_PXNTABLE) ||
		    (pmd_val(*pmd) & PMD_TABLE_UXNTABLE))
			execute_on_xn_area = true;
	}

	pte = pte_offset_kernel(pmd, target);

	printk("pte/*pte: 0x%p/0x%llx\n", pte, pte_val(*pte));
	print_attributes(pte_val(*pte), true);

	if (pte_none(*pte)) {
		printk("No mapping found...\n");
		return;
	}
	else {
		if (pte_val(*pte) & PTE_RDONLY)
			write_to_ro_area = true;
		if ((pte_val(*pte) & PTE_PXN) || (pte_val(*pte) & PTE_UXN))
			execute_on_xn_area = true;
	}

attack:
	if (write_to_ro_area) {
		printk("Trying to write something to 0x%lx...\n", target);
		deadbeef((unsigned long *)target);
	}
	if (execute_on_xn_area) {
		printk("Trying to executing something on 0x%lx...\n", target);
		exec_shellcode((unsigned long *)target);
	}
}
#endif	/* CONFIG_EKP_DEBUGFS_TEST_HELPERS_FOR_HACKING */
