#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/suspend.h>
#include <linux/cpu.h>
#include <linux/cpu_pm.h>
#include <linux/psci.h>
#include <uapi/linux/psci.h>
#include <asm/suspend.h>
#if 0
#include <asm/smp_scu.h>
#include <asm/memory.h>
#include <asm/suspend.h>
#include <asm/idmap.h>
#endif

static int lg1k_cpu_suspend_finish(unsigned long arg)
{

#define LG1K_SUSPEND_PARAM \
	((0 << PSCI_0_2_POWER_STATE_ID_SHIFT) | \
	 (1 << PSCI_0_2_POWER_STATE_AFFL_SHIFT) | \
	 (PSCI_POWER_STATE_TYPE_POWER_DOWN << PSCI_0_2_POWER_STATE_TYPE_SHIFT))

	if (psci_ops.cpu_suspend)
		psci_ops.cpu_suspend(LG1K_SUSPEND_PARAM, __pa(cpu_resume));
	/* never get here */
	BUG();
	return 0;
}

/*
 *      lg1k_pm_enter - Actually enter a sleep state.
 *      @state:         State we're entering.
 *
 */
static int lg1k_pm_enter(suspend_state_t state)
{
	int ret;
	cpu_pm_enter();
	cpu_cluster_pm_enter();

	ret = cpu_suspend(0, lg1k_cpu_suspend_finish);

	cpu_cluster_pm_exit();
	cpu_pm_exit();

	return ret;
}

static const struct platform_suspend_ops lg1k_pm_ops = {
	.enter		= lg1k_pm_enter,
	.valid		= suspend_valid_only_mem,
};

static __init int lg115x_pm_syscore_init(void)
{
	suspend_set_ops(&lg1k_pm_ops);
	return 0;
}
arch_initcall(lg115x_pm_syscore_init);
