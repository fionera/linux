#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/compat.h>
#include <linux/sysfs.h>
#include <linux/kthread.h>
#include <linux/err.h>
#include <linux/freezer.h>
#include <linux/kfifo.h>
#include <linux/atomic.h>
#include <rbus/timer_reg.h>
#include "rtk_ir_timer.h"

static atomic_t g_ir_timer_run = ATOMIC_INIT(0);
extern u32 gic_irq_find_mapping(u32 hwirq);

void irrc_mod_key_htimer_func(void);

static irqreturn_t rtk_timer_ir_interrupt(int irq, void *dev_id)
{
	unsigned int is_trigger = 0;
	TIMER_CALLBACK_FUNC callback = NULL;
	atomic_set(&g_ir_timer_run, 1);
	callback = (TIMER_CALLBACK_FUNC)dev_id;
	is_trigger = (TIMER_ISR_get_tc2_int(rtd_inl(TIMER_ISR_reg)) 
			&& TIMER_TCICR_get_tc2ie(rtd_inl(TIMER_TCICR_reg))) ? 1 : 0;
	if (is_trigger) {
		rtk_timer_control(IR_HW_TIMER_ID, HWT_STOP);
		rtk_timer_control(IR_HW_TIMER_ID, HWT_INT_DISABLE);
		rtk_timer_control(IR_HW_TIMER_ID, HWT_INT_CLEAR);
		callback();
		atomic_set(&g_ir_timer_run, 0);
		return IRQ_HANDLED;
	}
	atomic_set(&g_ir_timer_run, 0);
	return IRQ_NONE;
	
}


int rtk_ir_setup_timer(unsigned int us)
{
	rtk_timer_set_value(IR_HW_TIMER_ID, 0);
	rtk_timer_set_target(IR_HW_TIMER_ID, HW_TIMER_CNT_PER_US * us);
	rtk_timer_control(IR_HW_TIMER_ID, HWT_INT_ENABLE);
	rtk_timer_control(IR_HW_TIMER_ID, HWT_START);
	return 0;
}

int rtk_ir_cancel_timer(void)
{
	while(atomic_read(&g_ir_timer_run)) {
		msleep(1);
	}
	rtk_timer_control(IR_HW_TIMER_ID, HWT_STOP);
	rtk_timer_control(IR_HW_TIMER_ID, HWT_INT_DISABLE);
	rtk_timer_control(IR_HW_TIMER_ID, HWT_INT_CLEAR);// write 1 clear
	while(atomic_read(&g_ir_timer_run)) {
		msleep(1);
	}
	return 0;
}

int __init rtk_ir_timer_init(TIMER_CALLBACK_FUNC callback)
{
	int ret = 0;
	if(!callback)
		return -1;
	if (request_irq(gic_irq_find_mapping(IR_HW_TIMER_IRQ), rtk_timer_ir_interrupt, 
			IRQF_SHARED, "irq_ir_timer", callback)) {
		printk(KERN_EMERG"irq_ir_timer: can't get assigned irq%d\n", IR_HW_TIMER_IRQ);
		ret = -1;
		goto FAIL;
	}
	rtk_timer_control(IR_HW_TIMER_ID, HWT_STOP);
	rtk_timer_control(IR_HW_TIMER_ID, HWT_INT_DISABLE);
	rtk_timer_set_mode(IR_HW_TIMER_ID, COUNTER);
	rtk_timer_control(IR_HW_TIMER_ID, HWT_INT_CLEAR);// write 1 clear
	ret = 0;
FAIL:
	return ret;
}

void __exit rtk_ir_timer_exit(void)
{
	rtk_timer_control(IR_HW_TIMER_ID, HWT_STOP);
	rtk_timer_control(IR_HW_TIMER_ID, HWT_INT_DISABLE);
	free_irq(gic_irq_find_mapping(IR_HW_TIMER_IRQ), rtk_timer_ir_interrupt);
}