/*
 * drivers/staging/ekp/ekp.c
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

#define pr_fmt(fmt) "ekp: " fmt

#include <linux/arm-smccc.h>
#include <linux/cache.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/cpu_pm.h>
#include <linux/debugfs.h>
#include <linux/ekp.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/memblock.h>
#include <linux/proc_fs.h>
#include <linux/romempool.h>
#include <linux/suspend.h>
#include <linux/slab.h>

#include <asm/cacheflush.h>
#include <asm/ekp.h>
#include <asm/fixmap.h>
#include <asm/memory.h>
#include <asm/virt.h>
#include <asm/pgalloc.h>
#include <asm-generic/sections.h>

#include "ekp-hise.h"
#include "ekp_debug.h"

#define CREATE_TRACE_POINTS
#include <trace/events/ekp.h>

#ifdef CONFIG_EKP_FORCE_ENABLE
static bool ekp_enabled __initdata = true;
#else
static bool ekp_enabled __initdata;
#endif

static int __init ekp_enable(char *p)
{
	ekp_enabled = true;
	return 0;
}
early_param("ekp_enabled", ekp_enable);

bool ekp_fw_loaded __ro_after_init;
static bool ekp_started = false;

/* EKP-specific SMCCC defines */
#define ARM_SMCCC_OWNER_STANDARD_HYP	5
#define ARM_SMCCC_OWNER_VENDOR_HYP	6
#ifdef CONFIG_ARM64
#define EKP_SMCCC_SMC			ARM_SMCCC_SMC_64
#else
#define EKP_SMCCC_SMC			ARM_SMCCC_SMC_32
#endif

/* Convert EKP command identifier to HVC function identifier */
#define EKP_CMD_TO_HVC_FUNC_ID(func_id) \
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, EKP_SMCCC_SMC, \
			   ARM_SMCCC_OWNER_VENDOR_HYP, (func_id))

#ifdef CONFIG_EKP_VIOLATION_FAULT_NOTIFIER
#include <linux/fault_notifier.h>
static uint64_t fault_count[EKP_HISE_VIOLATION_MAX];

static void ekp_fault_handler(struct arm_smccc_res res, const char *violation)
{
	int index;

	BUG_ON(!ekp_started);

	index = __ffs(res.a1);

	if (fault_count[index] == ~0UL)
		fault_count[index] = 0;

	fault_count[index]++;

	fn_kernel_notify(FN_TYPE_ALL, "%s [%lu] - %lx %lx",
			 violation, fault_count[index], res.a2, res.a3);

	WARN_ON(1);
}
#else
static inline void ekp_fault_handler(struct arm_smccc_res res,
				     const char *violation)
{
	BUG();
}
#endif

static long parse_hise_error(unsigned long ekp_cmd,
			     struct arm_smccc_res res)
{
	long ret;
	unsigned long errno = res.a0;
	char *violation;

	switch (errno) {
	case EKP_HISE_OK:
		ret = 0;
		break;
	case EKP_HISE_CMD_NOT_SUPPORT:
		ret = -ENODEV;
		break;
	case EKP_HISE_DETECT_VIOLATION:
		switch (res.a1) {
		case INIT_AFTER_START:
			violation = "INIT_AFTER_START";
			break;
		case SET_PUD_VIOLATION:
			violation = "SET_PUD_VIOLATION";
			break;
		case SET_PMD_VIOLATION:
			violation = "SET_PMD_VIOLATION";
			break;
		case SET_PTE_VIOLATION:
			violation = "SET_PTE_VIOLATION";
			break;
		case SET_RO_S2_VIOLATION:
			violation = "SET_RO_S2_VIOLATION";
			break;
		case SLAB_SET_FP_VIOLATION:
			violation = "SLAB_SET_FP_VIOLATION";
			break;
		case CRED_INIT_VIOLATION:
			violation = "CRED_INIT_VIOLATION";
			break;
		case CRED_SECURITY_VIOLATION:
			violation = "CRED_SECURITY_VIOLATION";
			break;
		case SET_MOD_SECT_S2_VIOLATION:
			violation = "SET_MODULE_SECT_S2_VIOLATION";
			break;
		case UNSET_MOD_CORE_S2_VIOLATION:
			violation = "MODULE_CORE_UNSET_S2_VIOLATION";
			break;
		case UNSET_MOD_INIT_S2_VIOLATION:
			violation = "MODULE_INIT_UNSET_S2_VIOLATION";
			break;
		case SET_ROMEMPOOL_VIOLATION:
			violation = "SET_ROMEMPOOL_VIOLATION";
			break;
		default:
			res.a1 = UNKNOWN_VIOLATION;
			violation = "UNKNOWN_VIOLATION";
			break;
		}

		pr_err("%s has been detected - %lx %lx\n", violation,
		       res.a2, res.a3);

		ekp_fault_handler(res, violation);
		ret = 0;
		break;
	default:
		ret = -ENODEV;
		break;
	}

	return ret;
}

#ifdef CONFIG_EKP_CMD_COUNT
/* We don't take care of "wrap around" issue intentionally. */
static atomic_t cmd_cnt[EKP_CMD_MAX];

/* Set true if you want to reset cmd_cnt on STR resume */
bool ekp_str_resume_perf;

static char *cmd_to_string(int cmd)
{
	switch (cmd) {
	case EKP_INIT:
		return "EKP_INIT";
	case EKP_START:
		return "EKP_START";
	case EKP_SET_PUD:
		return "EKP_SET_PUD";
	case EKP_SET_PMD:
		return "EKP_SET_PMD";
	case EKP_SET_PTE:
		return "EKP_SET_PTE";
	case EKP_SET_RO_S2:
		return "EKP_SET_RO_S2";
	case EKP_GET_RO_S2:
		return "EKP_GET_RO_S2";
	case EKP_CLEAR_ROMEM:
		return "EKP_CLEAR_ROMEM";
	case EKP_SLAB_SET_FP:
		return "EKP_SLAB_SET_FP";
	case EKP_SET_ROMEMPOOL:
		return "EKP_SET_ROMEMPOOL";
	case EKP_CRED_INIT:
		return "EKP_CRED_INIT";
	case EKP_CRED_SECURITY:
		return "EKP_CRED_SECURITY";
	case EKP_CPU_PM_ENTER:
		return "EKP_CPU_PM_ENTER";
	case EKP_CPU_PM_EXIT:
		return "EKP_CPU_PM_EXIT";
	case EKP_SET_MOD_SECT_S2:
		return "EKP_SET_MOD_SECT_S2";
	case EKP_UNSET_MOD_CORE_S2:
		return "EKP_UNSET_MOD_CORE_S2";
	case EKP_UNSET_MOD_INIT_S2:
		return "EKP_UNSET_MOD_INIT_S2";
	case EKP_CHECK_MOD_ROOT:
		return "EKP_CHECK_MOD_ROOT";
	case EKP_ARCH_CMD_1:
	case EKP_ARCH_CMD_2:
	case EKP_ARCH_CMD_3:
	case EKP_ARCH_CMD_4:
	case EKP_ARCH_CMD_5:
	case EKP_ARCH_CMD_6:
		return "EKP_ARCH_CMD";
#ifdef CONFIG_EKP_DEBUGFS_HISE_INFO
	case EKP_DEBUG_INFO:
		return "EKP_DEBUG_INFO";
	case EKP_DEBUG_MODULE_S2_INFO:
		return "EKP_DEBUG_MODULE_S2_INFO";
#endif
#ifdef CONFIG_EKP_DEBUGFS_TEST_HELPERS
	case EKP_TEST_HVC_HANDLER:
		return "EKP_TEST_HVC_HANDLER";
	case EKP_TEST_GET_S2_DESC:
		return "EKP_TEST_GET_S2_DESC";
	case EKP_TEST_SET_S2_DESC:
		return "EKP_TEST_SET_S2_DESC";
#endif
	default:
		return "Unknown Command";
	}
}

void show_cmd_cnt(void)
{
	int i, cnt;
	unsigned long total_cnt = 0;

	pr_info("****** EKP Command Count ******\n");
	for (i = 0; i < EKP_CMD_MAX; i++) {
		cnt = atomic_read(&cmd_cnt[i]);
		if (cnt) {
			total_cnt += cnt;
			pr_info("%s: %d\n", cmd_to_string(i), cnt);
		}
	}

	pr_info("\n");
	pr_info("Total: %lu\n", total_cnt);
}

static void inline reset_cmd_cnt(void)
{
	int i;

	for (i = 0; i < EKP_CMD_MAX; i++)
		atomic_set(&cmd_cnt[i], 0);
}
#endif

/*
 * This function is for calling 'hvc' instruction with keeping rules
 * defined in ARM SMCCC (SMC Calling Convention) document.
 */
long ekp_tunnel(unsigned long ekp_cmd, unsigned long arg1,
		unsigned long arg2, unsigned long arg3,
		unsigned long arg4, unsigned long arg5,
		unsigned long arg6)
{
	struct arm_smccc_res res;
	long ret;

	if (!ekp_initialized())
		return -ENODEV;

	arm_smccc_hvc(EKP_CMD_TO_HVC_FUNC_ID(ekp_cmd),
		      arg1, arg2, arg3, arg4, arg5, arg6, 0, &res);

#ifdef CONFIG_EKP_CMD_COUNT
	atomic_inc(&cmd_cnt[ekp_cmd]);
#endif

	/* Converting a returned value (from HISE) into a kernel one */
	ret = parse_hise_error(ekp_cmd, res);
	if (!ret)
		trace_ekp_cmd(ekp_cmd, arg1, arg2, arg3, arg4, arg5,
			      arg6, res.a0, res.a1, res.a2, res.a3);

	return ret;
}

long ekp_set_romem(phys_addr_t phy, unsigned int order, bool set)
{
	long ret;

	BUG_ON(phy % PAGE_SIZE);

	ret = ekp_tunnel(EKP_SET_RO_S2, phy, order, set, 0, 0, 0);
	if (!ret)
		trace_ekp_set_ro_s2(phy, order, set);

	return ret;
}
#ifdef CONFIG_EKP_DEBUGFS_TEST_HELPERS_FOR_HACKING
EXPORT_SYMBOL(ekp_set_romem);
#endif

#ifdef CONFIG_EKP_EL1_S2_PROT
bool ekp_is_romem(phys_addr_t phy)
{
	long ret;

	ret = ekp_tunnel(EKP_GET_RO_S2, phy & PAGE_MASK, 0, 0, 0, 0, 0);

	return (ret == 0) ? true : false;
}
#endif

#ifdef CONFIG_EKP_DEBUGFS_TEST_HELPERS_FOR_HACKING
EXPORT_SYMBOL(ekp_is_romem);
#endif

void ekp_alloc_pages(struct page *page, unsigned int order,
		     gfp_t *gfp_flags)
{
#ifdef CONFIG_EKP_EL1_S2_PROT
	phys_addr_t phy = __pfn_to_phys(page_to_pfn(page));
#ifdef CONFIG_EKP_ALLOC_DEBUG
	int i;

	/* Newly allocated pages should not be read-only. */
	for (i = 0; i < (1 << order); i++)
		BUG_ON(ekp_is_romem(phy + PAGE_SIZE * i));
#endif

	/* Nothing to do if __GFP_READONLY is not set. */
	if (!(*gfp_flags & __GFP_READONLY))
		return;

	/* __GFP_READONLY cannot be used with these flags */
	BUG_ON(*gfp_flags & (__GFP_RECLAIMABLE|__GFP_MOVABLE));

	if (!ekp_set_romem(phy, order, true)) {
		SetPageReadonly(page);

		/* Drop __GFP_ZERO since EKP clears the memory */
		*gfp_flags &= ~__GFP_ZERO;
	}
#endif	/* CONFIG_EKP_EL1_S2_PROT */
}

void ekp_free_pages(struct page *page, unsigned int order)
{
#ifdef CONFIG_EKP_EL1_S2_PROT
	phys_addr_t phy = __pfn_to_phys(page_to_pfn(page));
#ifdef CONFIG_EKP_ALLOC_DEBUG
	int i;
	bool romem = ekp_is_romem(phy);

	for (i = 1; i < (1 << order); i++)
		BUG_ON(ekp_is_romem(phy + PAGE_SIZE * i) != romem);
#endif

	if (PageReadonly(page))
		ekp_set_romem(phy, order, false);
#endif	/* CONFIG_EKP_EL1_S2_PROT */
}

#ifdef CONFIG_EKP_EL1_S2_PROT
static void * __ref ekp_memblock_page_alloc(void)
{
	phys_addr_t phys;
	void *p;

	phys = memblock_alloc(PAGE_SIZE, PAGE_SIZE);
	if (!phys)
		return NULL;

	p = __va(phys);

	/*
	 * ekp_set_romem() clears memory allocated, so we don't
	 * need to call memset() when ekp_set_romem() returns 0.
	 */
	if (ekp_set_romem(phys, 0, true) != 0)
		memset(p, 0, PAGE_SIZE);

	return p;
}

void * __meminit ekp_el1_s2_vmemmap_alloc_pgtable(int node)
{
	if (!ekp_initialized())
		return NULL;

	if (slab_is_available()) {
		/*
		 * It refers to vmemmap_alloc_block() in mm/sparse-vmemmap.c
		 */
		struct page *page;

		if (node_state(node, N_HIGH_MEMORY))
			page = alloc_pages_node(node, PGALLOC_GFP, 0);
		else
			page = alloc_pages(PGALLOC_GFP, 0);

		if (page)
			return page_address(page);
		return NULL;
	}

	return ekp_memblock_page_alloc();
}
#endif

#define ARM_SMCCC_HVC_ZEROS(res)	\
	arm_smccc_hvc(0, 0, 0, 0, 0, 0, 0, 0, res)

struct ekp_extra_info {
	phys_addr_t empty_zero_page;
	unsigned long vdso_data;
	struct ekp_extra_arch_info arch;
};

struct ekp_init_info {
	/* Page tables */
	phys_addr_t swapper_pg_dir;

	/* Kernel sections */
	struct ekp_init_ksection_info ksection;

	/* Virtual addresses */
	unsigned long fixaddr_start;
	struct ekp_init_arch_info arch;

	/* Address of panic() */
	unsigned long panic_func;

	/* Credentials */
	uint32_t cred_size;
	uint32_t cred_usage_offset;
	uint32_t cred_rcu_offset;
	uint32_t cred_override_offset;
	uint32_t cred_security_offset;

	/* Physical address of romempool_phys bitmap */
	unsigned long romempool_phys;
};

#define MAX_MEMORY_BLOCK	5

struct memory_layout {
	uint32_t memblock_cnt;		/* Count of memory block */
	unsigned long memblock_start[MAX_MEMORY_BLOCK];
	unsigned long memblock_end[MAX_MEMORY_BLOCK];
};

void __weak cpu_dcache_flush_area(void *addr, size_t len)
{
	return;
}

/*
 * When this function is invoked, we can assume that current EL2 vector
 * is HISE init_vector.
 */
static inline void hise_init_fw(struct ekp_init_info *info,
				struct memory_layout *memlayout)
{
	struct arm_smccc_res res;

	/* Following calls always should be done successfully. */
	if (info) {
		/*
		 * We should flush dcache area for memlayout since EL2
		 * will access to them with d-cache disable.
		 */
		cpu_dcache_flush_area(memlayout,
				      sizeof(struct memory_layout));

		arm_smccc_hvc(__pa(info), __pa(memlayout), 0, 0, 0, 0,
			      0, 0, &res);
	}
	else
		ARM_SMCCC_HVC_ZEROS(&res);

	pr_info("enabled on CPU %d\n", smp_processor_id());
}

enum ekp_pm_state {
	EKP_PM_IN_HIBERNATION = (1 << 0),
	EKP_PM_IN_SUSPEND = (1 << 1)
};
static enum ekp_pm_state pm_state;

static inline bool is_valid_pm_state(void)
{
	return (pm_state == EKP_PM_IN_HIBERNATION) ||
	       (pm_state == EKP_PM_IN_SUSPEND);
}

static inline char *pm_state_to_string(void)
{
	char *state;

	switch (pm_state) {
	case EKP_PM_IN_HIBERNATION:
		state = "PM_HIBERNATION";
		break;
	case EKP_PM_IN_SUSPEND:
		state = "PM_SUSPEND";
		break;
	default:
		state = "PM_UNKNOWN";
		break;
	}

	return state;
}

static int ekp_pm_notify(struct notifier_block *nb, unsigned long action,
			 void *data)
{
	switch (action) {
	case PM_HIBERNATION_PREPARE:
		pm_state |= EKP_PM_IN_HIBERNATION;
		return NOTIFY_OK;
	case PM_POST_HIBERNATION:
		pm_state &= ~EKP_PM_IN_HIBERNATION;
		return NOTIFY_OK;
	case PM_SUSPEND_PREPARE:
		pm_state |= EKP_PM_IN_SUSPEND;
		return NOTIFY_OK;
	case PM_POST_SUSPEND:
		pm_state &= ~EKP_PM_IN_SUSPEND;
		return NOTIFY_OK;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block ekp_pm_nb = {
	.notifier_call = ekp_pm_notify,
};

static cpumask_var_t ekp_fw_initialized;
static phys_addr_t hise_init_vector __ro_after_init;

static void hise_init_cpu(void *unused)
{
	int cpu = smp_processor_id();

	if (!is_hyp_mode_available())
		return;
	if (!cpumask_test_cpu(cpu, ekp_fw_initialized)) {
		/* Set HISE initial vectors to initialize non-boot CPUs */
		__hyp_set_vectors(hise_init_vector);

		/* Initialize EL2 environment */
		hise_init_fw(NULL, NULL);

		cpumask_set_cpu(cpu, ekp_fw_initialized);
	}
	else
		pr_warn("CPU %d was already initialized\n", cpu);
}

void hise_init_cpu_early(void)
{
	if (ekp_initialized())
		hise_init_cpu(NULL);
}

/* Non-boot CPUs are initialized by this event handler. */
static int ekp_cpu_notify(struct notifier_block *nb, unsigned long action,
			  void *data)
{
	long target_cpu = (long)((long *)data);

        switch (action) {
	case CPU_STARTING:
	case CPU_STARTING_FROZEN:
		smp_call_function_single((int)target_cpu,
					 hise_init_cpu, NULL, 1);
		return NOTIFY_OK;
	case CPU_DYING:
	case CPU_DYING_FROZEN:
		cpumask_clear_cpu((int)target_cpu, ekp_fw_initialized);
		return NOTIFY_OK;
	default:
		break;
        }

        return NOTIFY_DONE;
}

static struct notifier_block ekp_cpu_nb = {
        .notifier_call = ekp_cpu_notify,
};

/*
 * EKP assuems that only syscore_suspend/resume invokes CPU_PM_ENTER
 * and CPU_PM_EXIT events respectively. This event handler is for
 * boot CPU. Non-boot CPUs are handled by ekp_cpu_notify().
 */
static int ekp_cpu_pm_notify(struct notifier_block *nb,
			     unsigned long action, void *data)
{
	int cpu = smp_processor_id();

	switch (action) {
	case CPU_PM_ENTER:
		if (is_valid_pm_state() &&
		    cpumask_test_cpu(cpu, ekp_fw_initialized)) {
			pr_info("CPU %d enters into %s\n", cpu,
				pm_state_to_string());

			ekp_tunnel(EKP_CPU_PM_ENTER, pm_state, 0, 0, 0,
				   0, 0);
#ifdef CONFIG_ARM64	// FIXME
			if (pm_state == EKP_PM_IN_SUSPEND)
				__hyp_reset_cpu();
#endif
			cpumask_clear_cpu(cpu, ekp_fw_initialized);

#ifdef CONFIG_EKP_CMD_COUNT
			if (ekp_str_resume_perf)
				reset_cmd_cnt();
#endif

			return NOTIFY_OK;
		}
		else
			pr_info("CPU %d already enters into %s\n",
				cpu, pm_state_to_string());

		break;
	case CPU_PM_ENTER_FAILED:
	case CPU_PM_EXIT:
		if (is_valid_pm_state() &&
		    !cpumask_test_cpu(cpu, ekp_fw_initialized)) {
			pr_info("CPU %d is resumed from %s\n", cpu,
				pm_state_to_string());

			hise_init_cpu(NULL);
			ekp_tunnel(EKP_CPU_PM_EXIT, pm_state, 0, 0, 0,
				   0, 0);

			return NOTIFY_OK;
		}
		else
			pr_info("CPU %d is already resumed from %s\n",
				cpu, pm_state_to_string());

		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block ekp_cpu_pm_nb = {
	.notifier_call = ekp_cpu_pm_notify,
};

#if defined(CONFIG_ARM64) || defined(CONFIG_VDSO)
extern struct vdso_data *vdso_data;
#endif

static phys_addr_t hise_fw_start;
static phys_addr_t hise_fw_size;

void __weak cpu_extra_arch_info(struct ekp_extra_arch_info *info)
{
	return;
}

void ekp_start(void)
{
	struct ekp_extra_info info;
	char *fw_info;

	if (!ekp_initialized())
		return;

#if defined(CONFIG_EKP_ROMEMPOOL) && defined(arch_mark_romempool_ro)
	arch_mark_romempool_ro((unsigned long)__va(hise_fw_start),
			       (unsigned long)hise_fw_size);
#endif
	hise_fw_start = 0;
	hise_fw_size = 0;

	memset(&info, 0, sizeof(struct ekp_extra_info));

#if defined(CONFIG_ARM64) || defined(CONFIG_VDSO)
	info.vdso_data = (unsigned long)vdso_data;
#endif
	info.empty_zero_page = page_to_phys(empty_zero_page);
	cpu_extra_arch_info(&(info.arch));

	ekp_tunnel(EKP_START, __pa(&info), __pa(&fw_info), 0, 0, 0, 0);

	ekp_started = true;

	pr_info("%s\n", fw_info);
}

/*
 * EKP-HISE Firmware Header
 */
struct ekp_hise_header {
	uint64_t magic;
	uint64_t version;
	uint64_t mem_start;
	uint64_t mem_size;
} __packed;
extern struct ekp_hise_header __ekp_hise_header __initdata;

#define ekp_hise_magic		__ekp_hise_header.magic
#define ekp_hise_version	__ekp_hise_header.version
#define ekp_hise_mem_start	__ekp_hise_header.mem_start
#define ekp_hise_mem_size	__ekp_hise_header.mem_size

/* Defined in ekp-hise.S */
extern unsigned long ekp_hise_size __initdata;
extern void *ekp_hise_stub __initdata;

/*
 * We assume that all CPUs are booted in EL2, so kernel stub vector
 * is installed on the CPUs. That is, we can use __hyp_{get,set}_vectors().
 */
static inline phys_addr_t hise_load_fw(void)
{
	struct arm_smccc_res res;

	if (!is_hyp_mode_available())
		return 0;

	/* Set EL2 stub vectors for boot CPU */
	__hyp_set_vectors(virt_to_phys(ekp_hise_stub));

	/*
	 * EKP stub vector does the following things.
	 * - relocate HISE firmware from .init section to reserved area
	 * - set EL2 vector to HISE initial vector which is only for
	 *   initialzing HISE environment
	 */
	ARM_SMCCC_HVC_ZEROS(&res);

	return (phys_addr_t)res.a0;
}

void __init __weak
cpu_init_ksection_info(struct ekp_init_ksection_info *info)
{
	return;
}

void __init __weak
cpu_init_arch_info(struct ekp_init_arch_info *info)
{
	return;
}

static void __init init_ekp_info(struct ekp_init_info *info)
{
	memset(info, 0, sizeof(struct ekp_init_info));

	info->swapper_pg_dir = __pa(swapper_pg_dir);
	cpu_init_ksection_info(&(info->ksection));
	info->fixaddr_start = FIXADDR_START;
	cpu_init_arch_info(&(info->arch));
	info->panic_func = (unsigned long)panic;
	info->cred_size = sizeof(struct cred);
	info->cred_usage_offset = offsetof(struct cred, usage);
	info->cred_rcu_offset = offsetof(struct cred, rcu);
#ifdef CONFIG_EKP_CRED_PROTECTION
	info->cred_override_offset = offsetof(struct cred, override);
#endif
#ifdef CONFIG_SECURITY_EKP_ROOT_RESTRICTION
	info->cred_security_offset = offsetof(struct cred, security);
#endif
#ifdef CONFIG_EKP_ROMEMPOOL
	info->romempool_phys = __pa(romempool_phys);
#endif
}

static void __init init_memory_layout(struct memory_layout *memlayout)
{
	struct memblock_region *r;
	int i = 0;

	memset(memlayout, 0, sizeof(struct memory_layout));

	for_each_memblock(memory, r) {
		BUG_ON(r->base % PAGE_SIZE);
		BUG_ON(r->size % PAGE_SIZE);

		memlayout->memblock_start[i] = r->base;
		memlayout->memblock_end[i] = r->base + r->size;
		memlayout->memblock_cnt = ++i;
	}
}

/* Ascii values for string "EKP-HISE" */
static const uint64_t EKP_HISE_MAGIC __initdata = 0x455349482D504B45;

void __init ekp_reserve_memory(void)
{
	if (!ekp_enabled)
		goto done;

	if ((ekp_hise_size == 0) || (ekp_hise_magic != EKP_HISE_MAGIC)) {
		pr_err("fail to detect HISE firmware\n");
		ekp_enabled = false;
		goto done;
	}

	hise_fw_start = (phys_addr_t)ekp_hise_mem_start;
	hise_fw_size = (phys_addr_t)ekp_hise_mem_size;

	if (memblock_reserve(hise_fw_start, hise_fw_size)){
		pr_err("fail to reserve memory for HISE firmware\n");
		ekp_enabled = false;
		goto done;
	}

	pr_info("reserved %lu KB for HISE firmware\n",
		(unsigned long)hise_fw_size / SZ_1K);

done:
	romempool_reserve(ekp_enabled);
}

static bool check_fw_version(uint64_t fw_version)
{
	int major, minor, subminor;

	major = (fw_version >> 16) & 0xFF;
	minor = (fw_version >> 8) & 0xFF;
	subminor = fw_version & 0xFF;

	if ((EKP_VERSION_MAJOR != major) || (EKP_VERSION_MINOR != minor)) {
		pr_err("cannot support this HISE firmware - "
		       "kernel: v%d.%d.%d, HISE firmware: v%d.%d.%d\n",
		       EKP_VERSION_MAJOR, EKP_VERSION_MINOR,
		       EKP_VERSION_SUBMINOR, major, minor, subminor);
		return false;
	}

	return true;
}

void __init ekp_init(void)
{
	struct ekp_init_info info;
	struct memory_layout memlayout;

	if (!ekp_enabled)
		return;
	if (!check_fw_version(ekp_hise_version))
		return;

	cpumask_clear(ekp_fw_initialized);

	/* Relocate HISE F/W and get an address of HISE initial vector */
	hise_init_vector = hise_load_fw();
	if (!hise_init_vector) {
		pr_err("fail to relocate HISE firmware\n");
		return;
	}

	init_ekp_info(&info);
	init_memory_layout(&memlayout);
	hise_init_fw(&info, &memlayout);

	cpumask_set_cpu(0, ekp_fw_initialized);

	/* From now on, ekp_tunnel can be used to trap into EL2 */
	ekp_fw_loaded = true;

	register_pm_notifier(&ekp_pm_nb);
	register_cpu_notifier(&ekp_cpu_nb);
	cpu_pm_register_notifier(&ekp_cpu_pm_nb);
}

#ifdef CONFIG_PROC_FS
static int enabled_proc_show(struct seq_file *m, void *v)
{
	unsigned int enabled = 0;

	if (!ekp_initialized())
		goto out;

	if (IS_ENABLED(CONFIG_EKP_FORCE_ENABLE))
		enabled = 1;
	else
		enabled = 2;

out:
	seq_printf(m, "%u\n", enabled);
	return 0;
}

static int enabled_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, enabled_proc_show, NULL);
}

static const struct file_operations enabled_proc_fops = {
	.open		= enabled_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init ekp_proc_init(void)
{
	struct proc_dir_entry *ekp_procfs;

	ekp_procfs = proc_mkdir("ekp", NULL);
	if (!ekp_procfs)
		return -ENOMEM;

	if (!proc_create("enabled", S_IRUSR, ekp_procfs,
			 &enabled_proc_fops))
		return -ENOMEM;

	return 0;
}
fs_initcall(ekp_proc_init);
#endif

#ifdef CONFIG_EKP_DEBUGFS
struct dentry *ekp_debugfs;

#ifdef CONFIG_EKP_DEBUGFS_HISE_INFO
/*
 * /sys/kernel/debug/ekp/hise_info
 *
 * Get some useful information for debugging from HISE F/W
 *
 * You can see the following things.
 *  - 'EL1 stage 2 translation' is enabled or not
 *  - 'EL1 VM register trap' is enabled or not
 *  - Contents of core EL2 registers
 *  - Physical memory information
 *  - Current values of core data structures in HISE F/W
 *
 * Note that HISE F/W prints the above information out by itself.
 * That is, all messages are not added to the kernel log buffer.
 *
 * READ: cat info
 * WRITE: N/A
 */
static int hise_info_get(void *data, u64 *val)
{
	ekp_tunnel(EKP_DEBUG_INFO, 0, 0, 0, 0, 0, 0);

	*val = 0;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(hise_info_fops, hise_info_get, NULL, "%llu\n");

#define MODULE_S2_INFO		0
#define MODULE_S2_INFO_PER_PAGE	1

void __weak arch_module_s2_info_prepare(void)
{
	return;
}

/*
 * /sys/kernel/debug/ekp/module_s2_info
 *
 * Get contents of stage2 translation tables for module area
 *
 * Note that HISE F/W prints the contents out by itself.
 * That is, all messages are not added to the kernel log buffer.
 *
 * READ: N/A
 * WRITE echo [0 or 1] > module_s2_info
 *  - 0: show stage 2 translation tables for module area
 *  - 1: show each VA/IPA mapped on module area and
 *       corresponding stage 2 translation table
 */
static int module_s2_info_set(void *data, u64 val)
{
	if ((val != MODULE_S2_INFO) &&
	    (val != MODULE_S2_INFO_PER_PAGE))
		return -EPERM;

	arch_module_s2_info_prepare();

	ekp_tunnel(EKP_DEBUG_MODULE_S2_INFO, val, 0, 0, 0, 0, 0);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(module_s2_info_fops, NULL,
			module_s2_info_set, "%llu\n");
#endif	/* CONFIG_EKP_DEBUGFS_HISE_INFO */

#ifdef CONFIG_EKP_DEBUGFS_KERNEL_INFO
bool __weak cpu_is_ro_table(unsigned long addr)
{
	return false;
}

void __weak cpu_pt_dump(pgd_t *pgd)
{
	return;
}

static void __pt_dump(bool kernel_only)
{
	struct task_struct *p;

	pr_info("-------- Page Table Dump --------\n");

	if (kernel_only) {
		pr_info("Kernel (PGD: 0x%p(0x%p), %s)\n", (void *)swapper_pg_dir,
			(void *)__pa(swapper_pg_dir),
			cpu_is_ro_table((unsigned long)swapper_pg_dir)
				? "RO" : "Writable");

		cpu_pt_dump(swapper_pg_dir);
	}
	else {
		read_lock(&tasklist_lock);
		for_each_process(p) {
			if (p->mm) {
				pr_info("Process: %s(%d) (PGD: 0x%p(0x%p), %s)\n",
					p->comm, p->pid, p->mm->pgd,
					(void *)__pa(p->mm->pgd),
					cpu_is_ro_table((unsigned long)p->mm->pgd)
						? "RO" : "Writable");
				cpu_pt_dump(p->mm->pgd);
			}
			else
				pr_info("Kernel thread: %s(%d)\n",
					p->comm, p->pid);
		}
		read_unlock(&tasklist_lock);
	}
}

/*
 * /sys/kernel/debug/ekp/pt_dump
 *
 * Show contents of page tables for kernel and user processes
 *
 * READ: N/A
 * WRITE: echo [0 or 1] > pt_dump
 *  - 0: dump kernel page table (swapper_pg_dir)
 *  - 1: dump page tables of user processes (only PGD)
 */
static int pt_dump_set(void *data, u64 val)
{
	/* Dump swapper_pg_dir */
	if (val == 0)
		__pt_dump(true);
	/* Dump page tables from all user processes */
	else if (val == 1)
		__pt_dump(false);
	else
		return -EPERM;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(pt_dump_fops, NULL, pt_dump_set, "%llu\n");

/*
 * /sys/kernel/debug/ekp/task_info
 *
 * Show virtual addresses of task_struct and its members
 * for a process which has a given PID
 *
 * READ: cat task_info
 *  - get virtual addresses of task_struct and its members
 * WRITE: echo [pid of target process] > task_info
 */
static pid_t task_info_target_pid = 0;

static int task_info_set(void *data, u64 val)
{
	task_info_target_pid = (pid_t)val;
	return 0;
}

static int task_info_get(void *data, u64 *val)
{
	struct task_struct *target = NULL;
	struct task_struct *p, *t;

	if (task_info_target_pid == 0)
		target = &init_task;
	else {
		read_lock(&tasklist_lock);
		for_each_process_thread(p, t) {
			if (t->pid == task_info_target_pid) {
				target = t;
				break;
			}
		}
		read_unlock(&tasklist_lock);
	}

	if (!target)
		return 0;

	pr_info("task_struct: %p\n", target);
	pr_info("task_struct->group_leader: %p\n",
		target->group_leader);
	pr_info("task_struct->cred: %p @ %p\n", target->cred,
		&(target->cred));
	pr_info("task_struct->cred->user: %p @ %p\n",
		target->cred->user, &(target->cred->user));
	pr_info("tprocesses: %d\n",
		atomic_read(&target->cred->user->processes));
#ifdef CONFIG_SECURITY_EKP_ROOT_RESTRICTION
	pr_info("task_struct->cred->security: %p @ %p\n",
		target->cred->security, &(target->cred->security));
#endif

	*val = 0UL;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(task_info_fops, task_info_get, task_info_set,
			"%llu\n");
#endif	/* CONFIG_EKP_DEBUGFS_KERNEL_INFO */

char *__ok_fail_string(bool flag)
{
	return flag ? "OK" : "FAIL";
}

bool __weak cpu_pgd_walk_for_checking_pxn(pgd_t *pgd)
{
	return false;
}

void __weak cpu_report(void)
{
	return;
}

static void __report_user_processes(void)
{
	struct task_struct *p, *t;
	int total = 0, good = 0;
	bool ro_pgd = true;
#ifdef CONFIG_EKP_CRED_PROTECTION
	bool ro_cred = true;
#endif
#ifdef CONFIG_SECURITY_EKP_ROOT_RESTRICTION
	bool valid_security = true;
#endif
	bool pxn = true;

	read_lock(&tasklist_lock);

	for_each_process_thread(p, t) {
		if (!t->mm)
			continue;

		/* User process should have read-only PGD. */
		if (!ekp_is_romem(__pa(t->mm->pgd))) {
			ro_pgd = false;
			pr_warn("%s(%d) has writable PGD.\n",
				t->comm, t->pid);
		}

#ifdef CONFIG_EKP_CRED_PROTECTION
		/* User process should have read-only credentials. */
		if (!ekp_is_romem(__pa(t->real_cred)) ||
		    !ekp_is_romem(__pa(t->cred))) {
			ro_cred = false;
			pr_warn("%s(%d) has writable credentials.\n",
				t->comm, t->pid);
		}
#endif
#ifdef CONFIG_SECURITY_EKP_ROOT_RESTRICTION
		if (!check_ekp_security(t)) {
			valid_security = false;
			pr_warn("%s(%d) has invalid security pointer.\n",
				t->comm, t->pid);
		}
#endif

		/* All valid PGD entries should have PXNTable bit. */
		if (!cpu_pgd_walk_for_checking_pxn(t->mm->pgd)) {
			pxn = false;
			pr_warn("%s(%d) has non-pxn PGD entries.\n",
				t->comm, t->pid);
		}
	}

	read_unlock(&tasklist_lock);

	pr_info("****** User processes ******\n");
	pr_info("Read-only PGD [%s]\n", __ok_fail_string(ro_pgd));
	total += 1;
	good += ro_pgd ? 1 : 0;

#ifdef CONFIG_EKP_CRED_PROTECTION
	pr_info("Read-only credentials [%s]\n",
		__ok_fail_string(ro_cred));
	total += 1;
	good += ro_cred ? 1: 0;
#else
	pr_info("Read-only credentials [NOT SUPPORT]\n");
#endif

#ifdef CONFIG_SECURITY_EKP_ROOT_RESTRICTION
	pr_info("Valid security pointer [%s]\n",
		__ok_fail_string(valid_security));
	total += 1;
	good += valid_security ? 1 : 0;
#else
	pr_info("Valid security pointer [NOT SUPPORT]\n");
#endif

	pr_info("PXN bits [%s]\n", __ok_fail_string(pxn));
	total += 1;
	good += pxn ? 1 : 0;

	pr_info("Total %d Good %d Bad %d\n", total, good,
		total - good);
	pr_info("\n");
}

/*
 * /sys/kernel/debug/ekp/report
 *
 * Show current EKP kernel configurations and status of user processes
 * protection.
 *
 * READ: cat report
 * WRITE: N/A
 */
static int report_get(void *data, u64 *val)
{
	bool ro_empty_zero_page;

	pr_info("\n");
	pr_info("****** Configurations ******\n");
	pr_info("Forcedly enabled [%s]\n",
		IS_ENABLED(CONFIG_EKP_FORCE_ENABLE) ? "O" : "X");
	pr_info("Read-only Memory Pool [%s]\n",
		IS_ENABLED(CONFIG_EKP_ROMEMPOOL) ? "O" : "X");
#ifdef CONFIG_EKP_ROMEMPOOL
	pr_info(" - Default size: %d MB, CMA size: %d MB, High-order allocation: %s\n",
		CONFIG_EKP_ROMEMPOOL_SIZE / SZ_1M,
#ifdef CONFIG_EKP_ROMEMPOOL_USE_CMA_ALLOCATION
		CONFIG_EKP_ROMEMPOOL_CMA_SIZE / SZ_1M,
#else
		0,
#endif
		IS_ENABLED(CONFIG_EKP_ROMEMPOOL_USE_HIGH_ORDER_ALLOCATION) ? "O" : "X");
#endif
	pr_info("Read-only credentials [%s]\n",
		IS_ENABLED(CONFIG_EKP_CRED_PROTECTION) ? "O" : "X");
	pr_info("Kernel module protection [%s]\n",
		IS_ENABLED(CONFIG_EKP_MODULE_PROTECTION) ? "O" : "X");
	pr_info("EKP LSM (Linux Security Module) [%s]\n",
		IS_ENABLED(CONFIG_SECURITY_EKP) ? "O" : "X");
#ifdef CONFIG_SECURITY_EKP
	pr_info(" - Module validation [%s]\n",
		IS_ENABLED(CONFIG_SECURITY_EKP_MODULE_VALIDATION) ? "O" : "X");
	pr_info(" - Root privilege restriction [%s]\n",
		IS_ENABLED(CONFIG_SECURITY_EKP_ROOT_RESTRICTION) ? "O" : "X");
#endif
	pr_info("\n");

	__report_user_processes();

	pr_info("****** General ******\n");
	ro_empty_zero_page = ekp_is_romem(page_to_phys(empty_zero_page));
	pr_info("Read-only empty_zero_page [%s]\n",
		__ok_fail_string(ro_empty_zero_page));
	cpu_report();
	pr_info("\n");

	*val = 0UL;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(report_fops, report_get, NULL, "%llu\n");

#ifdef CONFIG_EKP_CMD_COUNT
static int cmd_count_get(void *data, u64 *val)
{
	show_cmd_cnt();
	*val = 0UL;
	return 0;
}

static int cmd_count_set(void *data, u64 val)
{
	reset_cmd_cnt();
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(cmd_count_fops, cmd_count_get,
			cmd_count_set, "%llu\n");
#endif

static int __init ekp_debugfs_init(void)
{
	if (!debugfs_initialized())
		return -ENODEV;

	ekp_debugfs = debugfs_create_dir("ekp", NULL);
	if (IS_ERR_OR_NULL(ekp_debugfs)) {
		pr_err("cannot create debugfs root entry\n");
		return -ENOMEM;
	}

#ifdef CONFIG_EKP_DEBUGFS_HISE_INFO
	debugfs_create_file("hise_info", S_IRUSR, ekp_debugfs, NULL,
			    &hise_info_fops);
	debugfs_create_file("module_s2_info", S_IRUSR, ekp_debugfs, NULL,
			    &module_s2_info_fops);
#endif
#ifdef CONFIG_EKP_DEBUGFS_KERNEL_INFO
	debugfs_create_file("pt_dump", S_IRUSR, ekp_debugfs, NULL,
			    &pt_dump_fops);
	debugfs_create_file("task_info", S_IRUSR, ekp_debugfs, NULL,
			    &task_info_fops);
#endif
	debugfs_create_file("report", S_IRUSR, ekp_debugfs, NULL,
			    &report_fops);
#ifdef CONFIG_EKP_CMD_COUNT
	debugfs_create_file("cmd_count", S_IRUSR, ekp_debugfs, NULL,
			    &cmd_count_fops);
#endif

	ekp_test_helpers_init();
	ekp_lsm_debugfs_init();

	return 0;
}
late_initcall(ekp_debugfs_init);
#endif	/* CONFIG_EKP_DEBUGFS */
