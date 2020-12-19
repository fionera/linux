/*
 * PGD allocation/freeing
 *
 * Copyright (C) 2012 ARM Ltd.
 * Author: Catalin Marinas <catalin.marinas@arm.com>
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/mm.h>
#include <linux/gfp.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/ekp.h>
#include <linux/romempool.h>

#include <asm/pgalloc.h>
#include <asm/page.h>
#include <asm/tlbflush.h>

#include "mm.h"

static struct kmem_cache *pgd_cache;

pgd_t *pgd_alloc(struct mm_struct *mm)
{
	pgd_t *pgd;

	if (PGD_SIZE == PAGE_SIZE) {
		pgd = (pgd_t *)romempool_alloc(PGALLOC_GFP, RMP_PGD, 0);
		if (!pgd)
			pgd = (pgd_t *)__get_free_page(PGALLOC_GFP);

		return pgd;
	}
	else
		return kmem_cache_alloc(pgd_cache, PGALLOC_GFP);
}

void pgd_free(struct mm_struct *mm, pgd_t *pgd)
{
	if (PGD_SIZE == PAGE_SIZE) {
		if (is_romempool_addr((unsigned long)pgd))
			romempool_free((unsigned long)pgd, RMP_PGD, 0);
		else
			free_page((unsigned long)pgd);
	}
	else
		kmem_cache_free(pgd_cache, pgd);
}

static int __init pgd_cache_init(void)
{
	/*
	 * Naturally aligned pgds required by the architecture.
	 */
	if (PGD_SIZE != PAGE_SIZE)
		pgd_cache = kmem_cache_create("pgd_cache", PGD_SIZE, PGD_SIZE,
					      SLAB_PANIC, NULL);
	return 0;
}
core_initcall(pgd_cache_init);
