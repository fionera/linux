#include <linux/sysrq.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kmod.h>

extern void do_sysrq_print_klog(void);

static void sysrq_handle_print_klog(int key)
{
	do_sysrq_print_klog();
}
static struct sysrq_key_op sysrq_printklog_op = {
	.handler	= sysrq_handle_print_klog,
	.help_msg	= "print-kernel-log-buffer(y)",
	.action_msg	= "print-kernel-log-buffer",
	.enable_mask	= SYSRQ_ENABLE_LOG,
};

static int __init sysrq_klog_init(void)
{
	return register_sysrq_key('y', &sysrq_printklog_op);
}
device_initcall(sysrq_klog_init);

static int do_sysrq_emergency_shell(void)
{
	char *argv[2] = {"/usr/sbin/sysrq_shell", NULL};
	static char *envp[4] = {"HOME=/", "TERM=linux",
		"PATH=/sbin:/bin:/usr/sbin:/usr/bin", NULL};
	int ret;

	pr_info(" --- emergency shell sysrq ---\n");

	ret = call_usermodehelper(argv[0], argv, envp, UMH_NO_WAIT);
	if (ret != 0)
		pr_info("FOO: error incall to usermodehelper: %i\n", ret);

	pr_info(" --- call_usermodehelper is invoked for emergency shell ---\n");
	return ret;
}

static void sysrq_handle_emergency_shell(int key)
{
	do_sysrq_emergency_shell();
}

static struct sysrq_key_op sysrq_shell_op = {
	.handler	= sysrq_handle_emergency_shell,
	.help_msg	= "launch-shell-service(x)",
	.action_msg	= "launch-shell-service",
	.enable_mask	= SYSRQ_ENABLE_LOG,
};

static int __init sysrq_shell_init(void)
{
	return register_sysrq_key('x', &sysrq_shell_op);
}

device_initcall(sysrq_shell_init);

