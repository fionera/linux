/*
 * drivers/staging/webos/ekp/ekp_test_helpers.c
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

#include <linux/cpumask.h>
#include <linux/debugfs.h>
#include <linux/ekp.h>
#include <linux/ktime.h>
#include <linux/perf_event.h>
#include <linux/sched.h>

#include <asm/pgtable.h>
#include <asm/virt.h>

#include <trace/events/ekp.h>

#include "ekp_debug.h"

#define HVC_TEST_HISE_HANDLER	0
#define HVC_TEST_EL2_VECTOR	1

static void __hvc_on_target_cpu(void *data)
{
	int *cmd = data;
	long ret;
	phys_addr_t el2_vector;

	switch (*cmd) {
	case HVC_TEST_HISE_HANDLER:
		ret = ekp_tunnel(EKP_TEST_HVC_HANDLER, 0, 0, 0, 0, 0, 0);
#ifdef CONFIG_EKP_DEBUGFS_HISE_INFO
		pr_info("EKP: hise handler is %sworking for CPU %d\n",
			ret == 0 ? "" : "not ", smp_processor_id());
#endif
		break;
	case HVC_TEST_EL2_VECTOR:
		el2_vector = __hyp_get_vectors();
#ifdef CONFIG_EKP_DEBUGFS_HISE_INFO
		pr_info("EKP: el2 vector = 0x%p for CPU %d\n",
			(void *)el2_vector, smp_processor_id());
#endif
		break;
	default:
		break;
	}
}

/*
 * /sys/kernel/debug/ekp/test_helpers/hvc__hise_handler
 *
 * Check whether hvc_handler() on HISE F/W is working on given cpu
 * If this is working, you can think that hise_vectors for the cpu
 * has been installed successfully.
 *
 * READ: N/A
 * WRITE: echo [target_cpu] > hvc__hise_handler
 *  - CPU 0: bit[0], CPU 1: bit[1], ... CPU N: bit[N]
 *  - e.g.) echo 0x3 > hvc: test CPU 0 and 1
 *
 * Example on SoC with quad core:
 * /sys/kernel/debug/ekp/test_helpers # echo 0xf > hvc__hise_handler
 * [ 3859.512465] EKP: hise handler is working for CPU 0
 * [ 3859.517424] EKP: hise handler is working for CPU 1
 * [ 3859.522667] EKP: hise handler is working for CPU 2
 * [ 3859.527631] EKP: hise handler is working for CPU 3
 */
static int hvc__hise_handler_set(void *data, u64 val)
{
	int cpu;
	int cmd = HVC_TEST_HISE_HANDLER;

	for_each_online_cpu(cpu) {
		if ((1 << cpu) & val)
			smp_call_function_single(cpu, __hvc_on_target_cpu,
						 &cmd, 1);
	}

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(hvc__hise_handler_fops, NULL,
			hvc__hise_handler_set, "%llu\n");

/*
 * /sys/kernel/debug/ekp/test_helpers/hvc__el2_vector
 *
 * Get the physical address of EL2 vector on given cpu
 *
 * READ: N/A
 * WRITE: echo [target_cpu] > hvc__el2_vector
 *  - CPU 0: bit[0], CPU 1: bit[1], ... CPU N: bit[N]
 *  - e.g.) echo 0x3 > hvc: test CPU 0 and 1
 *
 * Example on webOS O18 SoC
 * /sys/kernel/debug/ekp/test_helpers # echo 0xf > hvc__el2_vector
 * [ 3975.454281] EKP: el2 vector = 0x0000000080002000 for CPU 0
 * [ 3975.459405] EKP: el2 vector = 0x0000000080002000 for CPU 1
 * [ 3975.464335] EKP: el2 vector = 0x0000000080002000 for CPU 2
 * [ 3975.469092] EKP: el2 vector = 0x0000000080002000 for CPU 3
 */
static int hvc__el2_vector_set(void *data, u64 val)
{
	int cpu;
	int cmd = HVC_TEST_EL2_VECTOR;

	for_each_online_cpu(cpu) {
		if ((1 << cpu) & val)
			smp_call_function_single(cpu, __hvc_on_target_cpu,
						 &cmd, 1);
	}

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(hvc__el2_vector_fops, NULL,
			hvc__el2_vector_set, "%llu\n");

/*
 * /sys/kernel/debug/ekp/test_helpers/s2__target_ipa
 *
 * Set/get the intermediate physical address (IPA) for s2__xxx operations
 *
 * Note that you don't need to give the address with PAGE_SIZE aligned,
 * since s2__xxx operations will make it PAGE_SIZE aligned internally.
 *
 * READ: cat s2__target_ipa
 *  - get current target IPA (0 by default)
 * WRITE: echo 0x00004000 > s2__target_ipa
 *  - set target IPA
 */
static phys_addr_t s2__target_ipa = 0;

static int s2__target_ipa_get(void *data, u64 *val)
{
	*val = (u64)s2__target_ipa;
	return 0;
}

static int s2__target_ipa_set(void *data, u64 val)
{
	s2__target_ipa = (phys_addr_t)val;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(s2__target_ipa_fops, s2__target_ipa_get,
			s2__target_ipa_set, "0x%016llx\n");

/*
 * /sys/kernel/debug/ekp/test_helpers/s2__ro
 *
 * Check whether current target IPA is set as RO on stage 2 translation
 * table.
 * If EKP_DEBUGFS_TEST_HELPERS_FOR_HACKING=y, you also set current
 * target IPA as RW or RO.
 *
 * READ: cat s2__ro
 *  - check the current RO attribute of target IPA
 * WRITE: echo [0 or 1] > s2__ro
 *  - 0: make IPA writable
 *  - 1: make IPA read-only
 *  - This only works when EKP_DEBUGFS_TEST_HELPERS_FOR_HACKING=y.
 */
static int s2__ro_get(void *data, u64 *val)
{
#ifdef CONFIG_EKP_DEBUGFS_HISE_INFO
	long ret;
	bool ro;

	ret = ekp_tunnel(EKP_GET_RO_S2, s2__target_ipa & PAGE_MASK,
			 0, 0, 0, 0, 0);

	ro = (ret == 0) ? true : false;

	pr_info("EKP: IPA 0x%p is set as %s on stage 2\n",
		(void *)s2__target_ipa, ro ? "RO" : "RW");

	*val = ro ? 1 : 0;
	return 0;
#else
	return -EPERM;
#endif
}

static int s2__ro_set(void *data, u64 val)
{
#ifdef CONFIG_EKP_DEBUGFS_TEST_HELPERS_FOR_HACKING
	u64 temp;
	bool ro;

	if (val != 0 && val != 1)
		return -EINVAL;

	ro = (val == 1) ? true : false;

	pr_info("EKP: IPA 0x%p is being set to %s on stage 2\n",
		(void *)s2__target_ipa, ro ? "RO" : "RW");

	ekp_tunnel(EKP_SET_RO_S2, s2__target_ipa & PAGE_MASK,
		   0, ro, 0, 0, 0);

	return s2__ro_get(NULL, &temp);
#else
	return -EPERM;
#endif
}

DEFINE_SIMPLE_ATTRIBUTE(s2__ro_fops, s2__ro_get, s2__ro_set, "%llu\n");

#define S2_ATTR_AP_SHIFT	(6)
#define S2_ATTR_AP_MASK		(0x3ULL << S2_ATTR_AP_SHIFT)
#define S2_ATTR_AP_NONE		(UL(0x0))
#define S2_ATTR_AP_RO		(UL(0x1))
#define S2_ATTR_AP_WO		(UL(0x2))
#define S2_ATTR_AP_RW		(UL(0x3))
#define S2_ATTR_XN_SHIFT	(54)
#define S2_ATTR_XN		(0x1ULL << S2_ATTR_XN_SHIFT)

char *__get_s2_attr_ap_string(unsigned long s2ap)
{
	char *access;

	switch (s2ap) {
	case S2_ATTR_AP_NONE:
		access = "None";
		break;
	case S2_ATTR_AP_RO:
		access = "Read-only";
		break;
	case S2_ATTR_AP_WO:
		access = "Write-only";
		break;
	case S2_ATTR_AP_RW:
	default:
		access = "Read/Write";
		break;
	}

	return access;
}

/*
 * /sys/kernel/debug/ekp/test_helpers/s2__attr
 *
 * See the contents of stage 2 translation table for given IPA
 * If EKP_DEBUGFS_TEST_HELPERS_FOR_HACKING=y, you also change the
 * contents of the table for the IPA.
 *
 * READ: cat s2__attr
 *  - see the contents of stage 2 translation table for given IPA
 * WRITE: echo [execute-never << 4 | access permisison] > s2__attr
 *  - access permission: 0 (none), 1 (read-only), 2 (write-only),
 *			 3 (read/write)
 *  - execute-never: 1 (non-executable), 0 (executable)
 *  - If you want to change the attribute with XN/WO, you need to
 *    specify a value like (1 << 4 | 2) = 0x12
 *  - This only works when EKP_DEBUGFS_TEST_HELPERS_FOR_HACKING=y.
 */
static int s2__attr_get(void *data, u64 *val)
{
#ifdef CONFIG_EKP_DEBUGFS_HISE_INFO
	unsigned long s2ap;
	unsigned long xn;

	ekp_tunnel(EKP_TEST_GET_S2_DESC, s2__target_ipa & PAGE_MASK,
		   __pa(val), 0, 0, 0, 0);

	pr_info("EKP: IPA 0x%p's attribute = 0x%016llx\n",
		(void *)s2__target_ipa, *val);

	s2ap = (*val & S2_ATTR_AP_MASK) >> S2_ATTR_AP_SHIFT;
	pr_info("EKP:\tAccess from non-secure EL1 or EL0 = %s\n",
		__get_s2_attr_ap_string(s2ap));

	xn = (*val & S2_ATTR_XN) == S2_ATTR_XN ? 1 : 0;
	pr_info("EKP:\tExecutable = %s\n", xn ? "no" : "yes");

	return 0;
#else
	return -EPERM;
#endif
}

#define S2_ATTR_SET_AP_MASK	(UL(0x3))
#define S2_ATTR_SET_XN_MASK	(UL(0x1) << 4)

static int s2__attr_set(void *data, u64 val)
{
#ifdef CONFIG_EKP_DEBUGFS_TEST_HELPERS_FOR_HACKING
	u64 s2ap, xn;
	u64 temp;

	s2__attr_get(NULL, &temp);

	s2ap = val & S2_ATTR_SET_AP_MASK;
	xn = (val & S2_ATTR_SET_XN_MASK) >> 4;

	pr_info("EKP: IPA 0x%p's attribute is being set to %s/%s\n",
		(void *)s2__target_ipa, __get_s2_attr_ap_string(s2ap),
		xn ? "non-executable" : "executable");

	temp &= ~S2_ATTR_AP_MASK;
	temp |= s2ap << S2_ATTR_AP_SHIFT;

	temp &= ~S2_ATTR_XN;
	temp |= xn << S2_ATTR_XN_SHIFT;

	ekp_tunnel(EKP_TEST_SET_S2_DESC, s2__target_ipa & PAGE_MASK,
		   __pa(&temp), 0, 0, 0, 0);

	return s2__attr_get(NULL, &temp);
#else
	return -EPERM;
#endif
}

DEFINE_SIMPLE_ATTRIBUTE(s2__attr_fops, s2__attr_get, s2__attr_set,
			"0x%016llx\n");

#ifdef CONFIG_EKP_DEBUGFS_TEST_HELPERS_FOR_HACKING
/*
 * /sys/kernel/debug/ekp/test_helpers/hack__target_virt
 *
 * Set/get the target virtual address for hack__xxx operations
 *
 * READ: cat hack__target_virt
 *  - get current target virtual address (0 by default)
 * WRITE: echo 0x00004000 > hack__target_virt
 *  - set target virtual address
 */
static unsigned long hack__target_virt = 0;

static int hack__target_virt_get(void *data, u64 *val)
{
	*val = (u64)hack__target_virt;
	return 0;
}

static int hack__target_virt_set(void *data, u64 val)
{
	hack__target_virt = (unsigned long)val;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(hack__target_virt_fops, hack__target_virt_get,
			hack__target_virt_set, "0x%016llx\n");

/*
 * /sys/kernel/debug/ekp/test_helpers/hack__read_write_virt
 *
 * Read/write value from/to the given virtual address
 *
 * READ: cat hack__read_write_virt
 *  - get a value from the given virutal address
 * WRITE: echo [value] > hack__read_write_virt
 *  - set a value to the given virtual address
 */
static int hack__read_write_virt_get(void *data, u64 *val)
{
	*val = (u64)*((unsigned long *)hack__target_virt);
	return 0;
}

static int hack__read_write_virt_set(void *data, u64 val)
{
	unsigned long *p = (unsigned long *)hack__target_virt;
	*p = (unsigned long)val;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(hack__read_write_virt_fops, hack__read_write_virt_get,
			hack__read_write_virt_set, "0x%016llx\n");

void __weak cpu_test_attack_ops(unsigned long target)
{
	return;
}

/* TODO */
static int hack__attack_set(void *data, u64 val)
{
	cpu_test_attack_ops(val);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(hack__attack_fops, NULL, hack__attack_set, "%llu\n");

/*
 * /sys/kernel/debug/ekp/test_helpers/hack__target_pid
 *
 * Get/set a PID of target process for hacking
 *
 * READ: cat hack__target_pid
 * WRITE: echo [pid of target process] > hack__target_pid
 */
static unsigned long hack__target_pid = 0;

static int hack__target_pid_get(void *data, u64 *val)
{
	*val = (u64)hack__target_pid;
	return 0;
}

static int hack__target_pid_set(void *data, u64 val)
{
	hack__target_pid = (unsigned long)val;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(hack__target_pid_fops, hack__target_pid_get,
			hack__target_pid_set, "%llu\n");

void __weak cpu_test_pt_modify(struct ekp_test_pt_modify *info)
{
	return;
}

static inline void __pt_modify(pgd_t *pgd, enum EKP_TEST_PT_LEVEL level,
			       unsigned long addr, bool set, u64 val)
{
	struct ekp_test_pt_modify info = {
		.pgd = pgd,
		.level = level,
		.addr = addr,
		.set = set,
		.val = val,
	};

	cpu_test_pt_modify(&info);
}

static pgd_t *__find_pgd_from_pid(unsigned long pid)
{
	struct task_struct *p;
	pgd_t *pgd = NULL;

	if (pid == 0)
		return swapper_pg_dir;

	read_lock(&tasklist_lock);
	for_each_process(p) {
		if (p->pid == pid) {
			pgd = p->mm->pgd;
			break;
		}
	}
	read_unlock(&tasklist_lock);

	return pgd;
}

/*
 * /sys/kernel/debug/ekp/test_helpers/hack__pgd_modify
 *
 * Get/set PGD descriptor of target virtual address/pid
 *
 * This entry finds a PGD start address of target process which is
 * specified by hack__target_pid. And then, it finds out PGD descriptor
 * of current target virtual address specified by hack__target_virt.
 *
 * READ: cat hack__pgd_modify
 * WRITE: echo [new value of the descriptor] > hack__pgd_modify
 */
static int hack__pgd_modify_get(void *data, u64 *val)
{
	__pt_modify(__find_pgd_from_pid(hack__target_pid), PGD_LEVEL,
		    hack__target_virt, false, 0);
	*val = 0UL;
	return 0;
}

static int hack__pgd_modify_set(void *data, u64 val)
{
	__pt_modify(__find_pgd_from_pid(hack__target_pid), PGD_LEVEL,
		    hack__target_virt, true, val);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(hack__pgd_modify_fops, hack__pgd_modify_get,
			hack__pgd_modify_set, "0x%016llx\n");

/*
 * /sys/kernel/debug/ekp/test_helpers/hack__pmd_modify

 * This entry finds a PMD start address of target process which is
 * specified by hack__target_pid. And then, it finds out PMD descriptor
 * of current target virtual address specified by hack__target_virt.
 *
 * READ: cat hack__pmd_modify
 * WRITE: echo [new value of the descriptor] > hack__pmd_modify
 */
static int hack__pmd_modify_get(void *data, u64 *val)
{
	__pt_modify(__find_pgd_from_pid(hack__target_pid), PMD_LEVEL,
		    hack__target_virt, false, 0);
	*val = 0UL;
	return 0;
}

static int hack__pmd_modify_set(void *data, u64 val)
{
	__pt_modify(__find_pgd_from_pid(hack__target_pid), PMD_LEVEL,
		    hack__target_virt, true, val);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(hack__pmd_modify_fops, hack__pmd_modify_get,
			hack__pmd_modify_set, "0x%016llx\n");

/*
 * /sys/kernel/debug/ekp/test_helpers/hack__pte_modify

 * This entry finds a PTE start address of target process which is
 * specified by hack__target_pid. And then, it finds out PTE descriptor
 * of current target virtual address specified by hack__target_virt.
 *
 * READ: cat hack__pte_modify
 * WRITE: echo [new value of the descriptor] > hack__pte_modify
 */
static int hack__pte_modify_get(void *data, u64 *val)
{
	__pt_modify(__find_pgd_from_pid(hack__target_pid), PTE_LEVEL,
		    hack__target_virt, false, 0);
	*val = 0UL;
	return 0;
}

static int hack__pte_modify_set(void *data, u64 val)
{
	__pt_modify(__find_pgd_from_pid(hack__target_pid), PTE_LEVEL,
		    hack__target_virt, true, val);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(hack__pte_modify_fops, hack__pte_modify_get,
			hack__pte_modify_set, "0x%016llx\n");
#endif	/* CONFIG_EKP_DEBUGFS_TEST_HELPERS_FOR_HACKING */

#ifdef CONFIG_EKP_DEBUGFS_TEST_HELPERS_FOR_PERFORMANCE
#define EKP_PERF_PMU_CPU_CYCLE	1
#define EKP_PERF_PMU_HW_INST	2
#define EKP_PERF_KTIME		3

static int perf_enabled;

static DEFINE_PER_CPU(struct perf_event *, pmu_events);
static DEFINE_PER_CPU(u64 [EKP_CMD_MAX], pmu_prev);
static DEFINE_PER_CPU(ktime_t [EKP_CMD_MAX], ktime_prev);

static inline u64 __read_pmu(struct perf_event *event)
{
	/*
	 * Here, perf_event_read_local is used instead of perf_event_read_value.
	 * Because perf_read_value sometimes calls smp_call_function_single with
	 * interrupts disabled from set_pte_at, so it causes a deadlock warning
	 * in smp_call_function_single. And perf_event_read_local also disables
	 * interrupts avoids all counter scheduling.
	 */
	return perf_event_read_local(event);
}

static inline bool __is_pmu_perf_enabled(void)
{
	return (perf_enabled == EKP_PERF_PMU_CPU_CYCLE) ||
	       (perf_enabled == EKP_PERF_PMU_HW_INST);
}

void ekp_perf_begin(int ekp_cmd)
{
	u64 *pmu_prev_ptr;
	struct perf_event *event;
	u64 count;
	ktime_t *ktime_prev_ptr;

	/* Ensure ekp_perf_begin and ekp_perf_end running on the same cpu */
	preempt_disable();

	if (!perf_enabled)
		return;

	if (__is_pmu_perf_enabled()) {
		pmu_prev_ptr = this_cpu_ptr(&pmu_prev)[ekp_cmd];

		event = *this_cpu_ptr(&pmu_events);
		count = __read_pmu(event);

		*pmu_prev_ptr = count;
	}
	else {
		ktime_prev_ptr = this_cpu_ptr(&ktime_prev)[ekp_cmd];
		*ktime_prev_ptr = ktime_get();
	}
}

void ekp_perf_end(int ekp_cmd)
{
	u64 count;
	u64 *pmu_prev_ptr;
	struct perf_event *event;
	ktime_t cur;
	ktime_t *ktime_prev_ptr;

	if (__is_pmu_perf_enabled()) {
		pmu_prev_ptr = this_cpu_ptr(&pmu_prev)[ekp_cmd];
		event = *this_cpu_ptr(&pmu_events);
	}
	else if (perf_enabled)
		ktime_prev_ptr = this_cpu_ptr(&ktime_prev)[ekp_cmd];

	/* Ensure ekp_perf_begin and ekp_perf_end running on the same cpu */
	preempt_enable();

	if (__is_pmu_perf_enabled()) {
		if (*pmu_prev_ptr) {
			count = __read_pmu(event);
			trace_ekp_perf_end(ekp_cmd, *pmu_prev_ptr, count,
					   count - *pmu_prev_ptr);
			*pmu_prev_ptr = 0;
		}
	}
	else if (perf_enabled) {
		if (ktime_to_ns(*ktime_prev_ptr)) {
			cur = ktime_get();
			trace_ekp_perf_end(ekp_cmd, ktime_to_ns(*ktime_prev_ptr),
					   ktime_to_ns(cur),
					   ktime_to_ns(ktime_sub(cur, *ktime_prev_ptr)));
			ktime_prev_ptr->tv64 = 0;
		}
	}
}

static int __perf_enable(int val)
{
	unsigned int cpu;
	struct perf_event *event;
	struct perf_event_attr attr;

	if (perf_enabled)
		return -EINVAL;
	if (val < EKP_PERF_PMU_CPU_CYCLE || val > EKP_PERF_KTIME)
		return -EINVAL;

	if (val == EKP_PERF_KTIME)
		goto out;

	memset(&attr, 0, sizeof(struct perf_event_attr));
	attr.type = PERF_TYPE_HARDWARE;
	attr.size = sizeof(struct perf_event_attr);
	attr.pinned = 1;
	attr.disabled = 1;

	switch (val) {
	case EKP_PERF_PMU_CPU_CYCLE:
		attr.config = PERF_COUNT_HW_CPU_CYCLES;
		break;
	case EKP_PERF_PMU_HW_INST:
	default:
		attr.config = PERF_COUNT_HW_INSTRUCTIONS;
		break;
	}

	for_each_possible_cpu(cpu) {
		event = perf_event_create_kernel_counter(&attr, cpu,
							 NULL, NULL, NULL);
		if (IS_ERR(event)) {
			pr_err("EKP: failed to create perf event on CPU %d\n",
			       cpu);
			return (int)PTR_ERR(event);
		}

		per_cpu(pmu_events, cpu) = event;
		perf_event_enable(event);
	}

out:
	perf_enabled = val;
	return 0;
}

static int __perf_disable(void)
{
	unsigned int cpu;
	struct perf_event *event;

	if (!perf_enabled) {
		/* Just no need to return -EINVAL for disable */
		return 0;
	}

	if (perf_enabled == EKP_PERF_KTIME)
		goto out;

	for_each_possible_cpu(cpu) {
		event = per_cpu(pmu_events, cpu);
		if (event) {
			perf_event_disable(event);
			perf_event_release_kernel(event);
			per_cpu(pmu_events, cpu) = NULL;
		}
	}

out:
	perf_enabled = 0;
	return 0;
}

/* TODO */
static int perf__enable_get(void *data, u64 *val)
{
	*val = (u64)perf_enabled;
	return 0;
}

static int perf__enable_set(void *data, u64 val)
{
	return val ? __perf_enable(val) : __perf_disable();
}

DEFINE_SIMPLE_ATTRIBUTE(perf__enable_fops, perf__enable_get,
			perf__enable_set, "%llu\n");

/* TODO */
static int perf__hvc_get(void *data, u64 *val)
{
	u64 count1, count2;
	struct perf_event *event;
	ktime_t start, stop, diff;

	if (!perf_enabled)
		return -EPERM;

	/* Ensure both two read_pmu()s running on the same cpu */
	preempt_disable();

	if (__is_pmu_perf_enabled()) {
		event = *this_cpu_ptr(&pmu_events);
		count1 = __read_pmu(event);
	}
	else
		start = ktime_get();

	ekp_tunnel(EKP_TEST_HVC_HANDLER, 0, 0, 0, 0, 0, 0);

	if (__is_pmu_perf_enabled()) {
		count2 = __read_pmu(event);
		*val = count2 - count1;
	}
	else {
		stop = ktime_get();
		diff = ktime_sub(stop, start);
		*val = ktime_to_ns(diff);
	}

	preempt_enable();

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(perf__hvc_fops, perf__hvc_get, NULL, "%llu\n");

/* TODO */
static int perf__trap_get(void *data, u64 *val)
{
	u64 count1, count2;
	struct perf_event *event;
	ktime_t start, stop, diff;
	u32 sctlr_el1;

	if (!perf_enabled)
		return -EPERM;

	/* Ensure both two read_pmu()s running on the same cpu */
	preempt_disable();

	asm volatile(
#ifdef CONFIG_ARM64
	"	mrs	%0, sctlr_el1"
#else
	"	mrc	p15, 0, %0, c1, c0, 0"
#endif
	: "=r" (sctlr_el1)
	);

	if (__is_pmu_perf_enabled()) {
		event = *this_cpu_ptr(&pmu_events);
		count1 = __read_pmu(event);
	}
	else
		start = ktime_get();

	asm volatile(
#ifdef CONFIG_ARM64
	"	msr	sctlr_el1, xzr"
#else
	"	mcr	p15, 0, %0, c1, c0, 0\n"
#endif
	: : "r" (sctlr_el1)
	);

	if (__is_pmu_perf_enabled()) {
		count2 = __read_pmu(event);
		*val = count2 - count1;
	}
	else {
		stop = ktime_get();
		diff = ktime_sub(stop, start);
		*val = ktime_to_ns(diff);
	}

	preempt_enable();

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(perf__trap_fops, perf__trap_get, NULL, "%llu\n");
#endif	/* CONFIG_EKP_DEBUGFS_TEST_HELPERS_FOR_PERFORMANCE */

void __init ekp_test_helpers_init(void)
{
	struct dentry *test_helpers_debugfs;

	test_helpers_debugfs = debugfs_create_dir("test_helpers",
						  ekp_debugfs);
	if (IS_ERR_OR_NULL(test_helpers_debugfs))
		return;

	/* HVC */
	debugfs_create_file("hvc__hise_handler", S_IRUSR,
			    test_helpers_debugfs, NULL,
			    &hvc__hise_handler_fops);
	debugfs_create_file("hvc__el2_vector", S_IRUSR,
			    test_helpers_debugfs, NULL,
			    &hvc__el2_vector_fops);

	/* Stage 2 translation table */
	debugfs_create_file("s2__target_ipa", S_IRUSR,
			    test_helpers_debugfs, NULL,
			    &s2__target_ipa_fops);
	debugfs_create_file("s2__ro", S_IRUSR, test_helpers_debugfs,
			    NULL, &s2__ro_fops);
	debugfs_create_file("s2__attr", S_IRUSR, test_helpers_debugfs,
			    NULL, &s2__attr_fops);

#ifdef CONFIG_EKP_DEBUGFS_TEST_HELPERS_FOR_HACKING
	debugfs_create_file("hack__target_virt", S_IRUSR,
			    test_helpers_debugfs, NULL,
			    &hack__target_virt_fops);
	debugfs_create_file("hack__read_write_virt", S_IRUSR,
			    test_helpers_debugfs, NULL,
			    &hack__read_write_virt_fops);
	debugfs_create_file("hack__attack", S_IRUSR, test_helpers_debugfs,
			    NULL, &hack__attack_fops);

	debugfs_create_file("hack__target_pid", S_IRUSR,
			    test_helpers_debugfs, NULL,
			    &hack__target_pid_fops);
	debugfs_create_file("hack__pgd_modify", S_IRUSR,
			    test_helpers_debugfs, NULL,
			    &hack__pgd_modify_fops);
	debugfs_create_file("hack__pmd_modify", S_IRUSR,
			    test_helpers_debugfs, NULL,
			    &hack__pmd_modify_fops);
	debugfs_create_file("hack__pte_modify", S_IRUSR,
			    test_helpers_debugfs, NULL,
			    &hack__pte_modify_fops);
#endif

#ifdef CONFIG_EKP_DEBUGFS_TEST_HELPERS_FOR_PERFORMANCE
	debugfs_create_file("perf__enable", S_IRUSR, test_helpers_debugfs,
			    NULL, &perf__enable_fops);
	debugfs_create_file("perf__hvc", S_IRUSR, test_helpers_debugfs,
			    NULL, &perf__hvc_fops);
	debugfs_create_file("perf__trap", S_IRUSR, test_helpers_debugfs,
			    NULL, &perf__trap_fops);
#endif
}
