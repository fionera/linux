/*
 * drivers/staging/webos/logger/wdt_detect.c
 */
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/smp.h>
#include "wdt_log.h"

static DECLARE_WAIT_QUEUE_HEAD(wdt_wait);

int wdt_log_thread(void)
{
	int once_do = 0;

	DECLARE_WAITQUEUE(wait, current);

	while (1) {

		add_wait_queue(&wdt_wait, &wait);
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule();
		set_current_state(TASK_RUNNING);
		remove_wait_queue(&wdt_wait, &wait);

		if (!once_do) {
			once_do = 1;
			wdt_log_save();
		}

		msleep(500);
	}

	while (1)
		;
EXIT:
	return 0;
}

void wakeup_wdt_thread(void)
{
	wake_up(&wdt_wait);
}
EXPORT_SYMBOL(wakeup_wdt_thread);

static int __init init_log_thread(void)
{
	struct task_struct *log_task;

	log_task = kthread_run(wdt_log_thread, NULL, "wdt_log_thread");
	if (!IS_ERR(log_task)) {
		struct sched_param param = { .sched_priority = 99 };

		sched_setscheduler_nocheck(log_task, SCHED_FIFO,
							&param);
	}
	return 0;
}
late_initcall(init_log_thread);
