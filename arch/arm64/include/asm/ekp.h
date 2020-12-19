/*
 * arch/arm64/include/asm/ekp.h
 *
 * Copyright (C) 2017 LG Electronics
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

#ifndef __ASM_EKP_H
#define __ASM_EKP_H

#include <asm/kernel-pgtable.h>
#include <asm/pgtable-hwdef.h>

#ifndef __ASSEMBLY__

/*
 * Arch-specific EKP Commands
 */

/*
 * Arch-specific EKP Information
 */
struct ekp_init_arch_info {
	phys_addr_t idmap_pg_dir;

	unsigned long __init_end_aligned;
	unsigned long vmemmap_start;
	unsigned long vmemmap_size;
};

struct ekp_extra_arch_info {
	phys_addr_t kuser_helpers_page;
};

/*
 * Read-only Memory Pool
 */
#ifdef CONFIG_EKP_ROMEMPOOL
#define arch_mark_romempool_ro arch_mark_romempool_ro
void arch_mark_romempool_ro(unsigned long start, unsigned long size);

#define arch_mark_romempool_rw arch_mark_romempool_rw
void arch_mark_romempool_rw(unsigned long start, unsigned long size);
#endif  /* CONFIG_EKP_ROMEMPOOL */

/*
 * Page Table Allocator and Modifiers
 */
#ifdef CONFIG_EKP
void __init *ekp_early_alloc(unsigned long sz);
#endif

#endif	/* !__ASSEMBLY__ */

#endif	/* __ASM_EKP_H */
