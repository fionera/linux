/*
 * include/linux/ekp.h
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

#ifndef __EKP_H
#define __EKP_H

/*
 * EKP Tunneling Commands
 */
#define EKP_INIT		(0x01)		/* deprecated */
#define EKP_START		(0x02)
/* Page table updates */
#define EKP_SET_PUD		(0x10)
#define EKP_SET_PMD		(0x11)
#define EKP_SET_PTE		(0x12)
/* Read-only memory */
#define EKP_SET_RO_S2		(0x30)
#define EKP_GET_RO_S2		(0x31)
#define EKP_CLEAR_ROMEM		(0x32)
#define EKP_SLAB_SET_FP		(0x33)
#define EKP_SET_ROMEMPOOL	(0x34)
/* Creds */
#define EKP_CRED_INIT		(0x40)
#define EKP_CRED_SECURITY	(0x41)
/* Suspend/Resume */
#define EKP_CPU_PM_ENTER	(0x50)
#define EKP_CPU_PM_EXIT		(0x51)
/* Modules */
#define EKP_SET_MOD_SECT_S2	(0x60)
#define EKP_UNSET_MOD_CORE_S2	(0x61)
#define EKP_UNSET_MOD_INIT_S2	(0x62)
#define EKP_CHECK_MOD_ROOT	(0x63)
/* Arch-specfic commands */
#define EKP_ARCH_CMD_1		(0xA0)
#define EKP_ARCH_CMD_2		(0xA1)
#define EKP_ARCH_CMD_3		(0xA2)
#define EKP_ARCH_CMD_4		(0xA3)
#define EKP_ARCH_CMD_5		(0xA4)
#define EKP_ARCH_CMD_6		(0xA5)
#ifdef CONFIG_EKP_DEBUGFS_HISE_INFO
/* 0xE0 ~ 0xEF are reserved for debugging purpose. */
#define EKP_DEBUG_INFO		(0xE0)
#define EKP_DEBUG_MODULE_S2_INFO (0xE1)
#endif
/* 0xF0 ~ 0xFE are reserved for testing/hacking purpose. */
#ifdef CONFIG_EKP_DEBUGFS_TEST_HELPERS
#define EKP_TEST_HVC_HANDLER	(0xF0)
#define EKP_TEST_GET_S2_DESC	(0xF1)
#define EKP_TEST_SET_S2_DESC	(0xF2)
#endif
#define EKP_CMD_MAX		(0xFF)

#ifndef __ASSEMBLY__

#include <linux/errno.h>
#include <linux/gfp.h>
#include <linux/mm_types.h>
#include <linux/types.h>
#include <linux/romempool.h>

#ifdef CONFIG_EKP
/*
 * EKP Version Information
 */
#define EKP_VERSION_MAJOR	1
#define EKP_VERSION_MINOR	1
#define EKP_VERSION_SUBMINOR	1

/*
 * EKP Initialization
 */
void __init ekp_reserve_memory(void);
void __init ekp_init(void);
void ekp_start(void);

extern bool ekp_fw_loaded;
static inline bool ekp_initialized(void)
{
	return ekp_fw_loaded;
}

void hise_init_cpu_early(void);

/*
 * EKP tunneling
 */
long ekp_tunnel(unsigned long ekp_cmd, unsigned long arg1, unsigned long arg2,
		unsigned long arg3, unsigned long arg4, unsigned long arg5,
		unsigned long arg6);

/*
 * EKP Information
 */
struct ekp_init_ksection_info {
	phys_addr_t ktext_start;
	phys_addr_t ktext_end;
	/* krodata_start == ktext_end */
	phys_addr_t krodata_end;
};
void __init cpu_init_ksection_info(struct ekp_init_ksection_info *info);

struct ekp_init_arch_info;
void __init cpu_init_arch_info(struct ekp_init_arch_info *info);

struct ekp_extra_arch_info;
void cpu_extra_arch_info(struct ekp_extra_arch_info *info);

/*
 * Cache Flush
 */
void cpu_dcache_flush_area(void *addr, size_t len);

/*
 * Read-only Memory
 */
long ekp_set_romem(phys_addr_t phy, unsigned int order, bool set);
#ifdef CONFIG_EKP_EL1_S2_PROT
bool ekp_is_romem(phys_addr_t phy);
#endif
static inline void ekp_clear_romem(phys_addr_t phy, unsigned long size)
{
	ekp_tunnel(EKP_CLEAR_ROMEM, phy, size, 0, 0, 0, 0);
}

void ekp_alloc_pages(struct page *page, unsigned int order, gfp_t *gfp_flags);
void ekp_free_pages(struct page *page, unsigned int order);

#ifdef CONFIG_EKP_EL1_S2_PROT
void * __meminit ekp_el1_s2_vmemmap_alloc_pgtable(int node);
static inline void *ekp_vmemmap_alloc_pt(int node, enum romempool_flags flag)
{
	return ekp_el1_s2_vmemmap_alloc_pgtable(node);
}
#else
static inline void *ekp_vmemmap_alloc_pt(int node, enum romempool_flags flag)
{
	return romempool_vmemmap_alloc_pgtable(flag);
}
#endif

/*
 * Modules
 */
#ifdef CONFIG_EKP_MODULE_PROTECTION

#include <linux/module.h>

static inline void ekp_set_module_sect_s2_pages(struct module *mod)
{
	ekp_tunnel(EKP_SET_MOD_SECT_S2, (unsigned long)mod,
		   offsetof(struct module, module_init),
		   offsetof(struct module, init_ro_size),
		   0, 0, 0);
}

static inline void ekp_unset_module_core_s2_pages(struct module *mod)
{
	ekp_tunnel(EKP_UNSET_MOD_CORE_S2, (unsigned long)mod,
		   offsetof(struct module, module_init),
		   offsetof(struct module, init_ro_size),
		   0, 0, 0);
}

static inline void ekp_unset_module_init_s2_pages(struct module *mod)
{
	ekp_tunnel(EKP_UNSET_MOD_INIT_S2, (unsigned long)mod,
		   offsetof(struct module, module_init),
		   offsetof(struct module, init_ro_size),
		   0, 0, 0);
}

#endif /* CONFIG_EKP_MODULE_PROTECTION */

#else	/* CONFIG_EKP */

static inline void __init ekp_reserve_memory(void) { }
static inline void __init ekp_init(void) { }
static inline void ekp_start(void) { }
static inline bool ekp_initialized(void) { return false; }
static inline void hise_init_cpu_early(void) { }

static inline long ekp_tunnel(unsigned long ekp_cmd, unsigned long arg1,
			      unsigned long arg2, unsigned long arg3,
			      unsigned long arg4, unsigned long arg5,
			      unsigned long arg6)
{
	return -ENODEV;
}

static inline long ekp_set_romem(phys_addr_t phy, unsigned int order,
				 bool set)
{
	return -ENODEV;
}
static inline bool ekp_is_romem(phys_addr_t phy) { return false; }
static inline void ekp_clear_romem(phys_addr_t phy, unsigned long size) { }

static inline void ekp_alloc_pages(struct page *page,
				   unsigned int order, gfp_t *gfp_flags) { }
static inline void ekp_free_pages(struct page *page, unsigned int order) { }
static inline void *ekp_vmemmap_alloc_pt(int node, enum romempool_flags flag)
{
	return NULL;
}
#endif	/* CONFIG_EKP */

/*
 * Performance test
 */
#ifdef CONFIG_EKP_DEBUGFS_TEST_HELPERS_FOR_PERFORMANCE
void ekp_perf_begin(int ekp_cmd);
void ekp_perf_end(int ekp_cmd);
#else
static inline void ekp_perf_begin(int ekp_cmd) { }
static inline void ekp_perf_end(int ekp_cmd) { }
#endif

#endif  // !__ASSEMBLY__

#endif	/* __EKP_H */
