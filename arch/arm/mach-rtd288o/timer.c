/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2012 by Chuck Chen <ycchen@realtek.com>
 *
 * Time initialization.
 */

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <asm/io.h>
#ifndef CONFIG_ARM64
#include <asm/mach/time.h>
#endif
#include <linux/of.h>
#include <linux/sched_clock.h>
#include <mach/timex.h>
#include <mach/irqs.h>
#include <rbus/timer_reg.h>
#include <linux/cpu.h>

#ifdef CONFIG_ARM_ARCH_TIMER
#include <asm/arch_timer.h>

#define WRAP_A7_APB_TSG_EN0                                                          0xB805C800
#define WRAP_A7_APB_TSG_EN1                                                          0xB805C804
#endif


#ifdef CONFIG_REALTEK_SCHED_LOG
unsigned int sched_log_time_scale;
#endif // CONFIG_REALTEK_SCHED_LOG

extern int rtk_clocksource_init(int cpu);

static u64 clock_offset = 0;
#define SYSTEM_TIMER8  8
#define SYSTEM_TIMER9  9
#define SYSTEM_TIMER10 10
#define SYSTEM_TIMER11 11
extern u32 gic_irq_find_mapping(u32 hwirq);
#include <mach/platform.h>

struct clock_event_device rtk_clock_event[4];
static struct timer_reg sys_timer[MAX_HWTIMER_NUM] = {
{TIMER_TC0TVR_reg, TIMER_TC0CVR_reg, TIMER_TC0CR_reg},
{TIMER_TC1TVR_reg, TIMER_TC1CVR_reg, TIMER_TC1CR_reg},
{TIMER_TC2TVR_reg, TIMER_TC2CVR_reg, TIMER_TC2CR_reg},
{TIMER_TC3TVR_reg, TIMER_TC3CVR_reg, TIMER_TC3CR_reg},
{TIMER_TC4TVR_reg, TIMER_TC4CVR_reg, TIMER_TC4CR_reg},
{TIMER_TC5TVR_reg, TIMER_TC5CVR_reg, TIMER_TC5CR_reg},
{TIMER_TC6TVR_reg, TIMER_TC6CVR_reg, TIMER_TC6CR_reg},
{TIMER_TC7TVR_reg, TIMER_TC7CVR_reg, TIMER_TC7CR_reg},
{TIMER_TC8TVR_reg, TIMER_TC8CVR_reg, TIMER_TC8CR_reg},
{TIMER_TC9TVR_reg, TIMER_TC9CVR_reg, TIMER_TC9CR_reg},
{TIMER_TC10TVR_reg, TIMER_TC10CVR_reg, TIMER_TC10CR_reg},
{TIMER_TC11TVR_reg, TIMER_TC11CVR_reg, TIMER_TC11CR_reg},
};


static short timer_id_map[]={SYSTEM_TIMER8,SYSTEM_TIMER9,SYSTEM_TIMER10,SYSTEM_TIMER11};


static void clear_irq_reason(unsigned int id)
{
    rtd_outl(TIMER_ISR_reg, (1<<id));
}

static int get_cpu_by_devid(void *dev_id)
{
	if(dev_id==(void *)&sys_timer[SYSTEM_TIMER8])
		return 0;
	else if(dev_id==(void *)&sys_timer[SYSTEM_TIMER9])
		return 1;
	else if(dev_id==(void *)&sys_timer[SYSTEM_TIMER10])
		return 2;
	else if(dev_id==(void *)&sys_timer[SYSTEM_TIMER11])
		return 3;
	return 0;
}

extern void smp_send_rtk_timer(int cpu);

static irqreturn_t rtk_timer_interrupt(int irq, void *dev_id)
{
	int timer_id=timer_id_map[0];
	if(rtd_inl(TIMER_ISR_reg) & (1<<timer_id))
	{
		struct clock_event_device *evt = &rtk_clock_event[0];
		clear_irq_reason(timer_id);
		evt->event_handler(evt);
		return IRQ_HANDLED;
	}
	else
		return IRQ_NONE;
}
static irqreturn_t rtk_timer_interrupt1(int irq, void *dev_id)
{
	int timer_id=timer_id_map[1];
	if(rtd_inl(TIMER_ISR_reg) & (1<<timer_id))
	{
		struct clock_event_device *evt = &rtk_clock_event[1];
		clear_irq_reason(timer_id);
		smp_send_rtk_timer(1);
//		evt->event_handler(evt);
		return IRQ_HANDLED;
	}
	else
		return IRQ_NONE;
}
static irqreturn_t rtk_timer_interrupt2(int irq, void *dev_id)
{
	int timer_id=timer_id_map[2];
	if(rtd_inl(TIMER_ISR_reg) & (1<<timer_id))
	{
		struct clock_event_device *evt = &rtk_clock_event[2];
		clear_irq_reason(timer_id);
		smp_send_rtk_timer(2);
//		evt->event_handler(evt);
		return IRQ_HANDLED;
	}
	else
		return IRQ_NONE;
}
static irqreturn_t rtk_timer_interrupt3(int irq, void *dev_id)
{
	int timer_id=timer_id_map[3];
	if(rtd_inl(TIMER_ISR_reg) & (1<<timer_id))
	{
		struct clock_event_device *evt = &rtk_clock_event[3];
		clear_irq_reason(timer_id);
		smp_send_rtk_timer(3);
//		evt->event_handler(evt);
		return IRQ_HANDLED;
	}
	else
		return IRQ_NONE;
}

void rtk_timer_handler(int cpu)
{
//	pr_info("handle ipi:%d %x\n",cpu,rtk_clock_event[cpu].event_handler);
	if(rtk_clock_event[cpu].event_handler)
		rtk_clock_event[cpu].event_handler(&rtk_clock_event[cpu]);
}


static struct irqaction rtk_timer_irq = {
    .dev_id     = (void *)&sys_timer[SYSTEM_TIMER8],
    .name       = "rtk timer8",
    .flags      = IRQF_TIMER | IRQF_SHARED,
    .handler    = rtk_timer_interrupt,
};

static struct irqaction rtk_timer_irq1 = {
    .dev_id     = (void *)&sys_timer[SYSTEM_TIMER9],
    .name       = "rtk timer9",
    .flags      = IRQF_TIMER | IRQF_SHARED,
    .handler    = rtk_timer_interrupt1,
};

static struct irqaction rtk_timer_irq2 = {
    .dev_id     = (void *)&sys_timer[SYSTEM_TIMER10],
    .name       = "rtk timer10",
    .flags      = IRQF_TIMER | IRQF_SHARED,
    .handler    = rtk_timer_interrupt2,
};
static struct irqaction rtk_timer_irq3 = {
    .dev_id     = (void *)&sys_timer[SYSTEM_TIMER11],
    .name       = "rtk timer11",
    .flags      = IRQF_TIMER | IRQF_SHARED,
    .handler    = rtk_timer_interrupt3,
};

int create_timer(unsigned char id, unsigned int interval, unsigned char mode)
{
	//Disable Interrupt
	rtk_timer_control(id, HWT_INT_DISABLE);

	//Disable Timer
//	rtk_timer_control(id, HWT_STOP);

	//Set The Initial Value
	rtk_timer_set_value(id, 0);

	//Set The Target Value
	rtk_timer_set_target(id, interval);

	//Enable Timer Mode
	rtk_timer_set_mode(id, mode);

	//Enable Timer
//	rtk_timer_control(id, HWT_START);

	//Enable Interrupt
	rtk_timer_control(id, HWT_INT_ENABLE);

	return 0;
}


// route timer 6~11 interrupt to SCPU 
int rtk_timer6_11_route_to_SCPU(unsigned char id, unsigned char enable)
{
    //timer 6 ~ 11 need to set routing int to which CPU for RTD287x platform
    if(id >=6)
    {
        if(enable)
            rtd_outl(SYS_REG_INT_CTRL_timer6_11_reg, (0xd) << ((id-6)*4));
        else
            rtd_outl(SYS_REG_INT_CTRL_timer6_11_reg, (0x8) << ((id-6)*4));
    }

    return 0;
}


int rtk_timer_set_mode(unsigned char id, unsigned char mode)
{
	unsigned int set_value;

	if (id >= MAX_HWTIMER_NUM)
		BUG();

	if(mode == TIMER) //TIMER mode
		rtd_setbits(sys_timer[id].cr, TIMERINFO_TIMER_MODE);
	else //COUNTER mode
		rtd_clearbits( sys_timer[id].cr, TIMERINFO_TIMER_MODE);

        if(id >=6)
        {
                rtd_outl(SYS_REG_INT_CTRL_timer6_11_reg, (0xd) << ((id-6)*4));
        }
        else if(id ==3)
        {
                rtd_outl(SYS_REG_INT_CTRL_timer6_11_reg, (0xd) << SYS_REG_INT_CTRL_timer6_11_timer3_routing_mux_sel_shift);
        }
        else if(id ==5)
        {
                rtd_outl(SYS_REG_INT_CTRL_timer6_11_reg, (0xd) << SYS_REG_INT_CTRL_timer6_11_timer5_routing_mux_sel_shift);
        }

	return 0;
}

int rtk_timer_get_value(unsigned char id)
{
    if (id >= MAX_HWTIMER_NUM)
	BUG();

    // get the current timer's value
    return rtd_inl(sys_timer[id].cvr);
}
static cycle_t rtk_get_counter0(void)
{
	int cpu=0;//smp_processor_id();
	return rtk_timer_get_value(timer_id_map[cpu]);
}

static cycle_t  rtk_get_counter1(void)
{
	int cpu=1;//smp_processor_id();
	return rtk_timer_get_value(timer_id_map[cpu]);
}

static cycle_t  rtk_get_counter2(void)
{
	int cpu=2;//smp_processor_id();
	return rtk_timer_get_value(timer_id_map[cpu]);
}

static cycle_t  rtk_get_counter3(void)
{
	int cpu=3;//smp_processor_id();
	return rtk_timer_get_value(timer_id_map[cpu]);
}

int rtk_timer_set_value(unsigned char id, unsigned int value)
{
    if (id >= MAX_HWTIMER_NUM)
	BUG();

    // set the timer's initial value
    rtd_outl(sys_timer[id].cvr, value);
    return 0;
}

int rtk_timer_set_target(unsigned char id, unsigned int value)
{
    if (id >= MAX_HWTIMER_NUM)
	BUG();

    // set the timer's initial value
    rtd_outl(sys_timer[id].tvr, value);
    return 0;
}

static void rtk_timer_set_interrupt(unsigned char id, bool enable)
{
	unsigned int set_value;

	if(enable)
		rtd_outl(TIMER_TCICR_reg, (1 <<(id+1) | TIMER_TCICR_write_data_mask));
	else
		rtd_outl(TIMER_TCICR_reg, 1 <<(id+1));
}

static void rtk_timer_set_hwt_enable(unsigned id, bool enable)
{
	if(enable) //HWT_START
		rtd_setbits(sys_timer[id].cr, TIMERINFO_TIMER_ENABLE);
	else //HWT_STOP
		rtd_clearbits( sys_timer[id].cr, TIMERINFO_TIMER_ENABLE);
}

static void rtk_timer_set_hwt_pause(unsigned id, bool pause)
{

	if(pause) //HWT_PAUSE
		rtd_setbits(sys_timer[id].cr, TIMERINFO_TIMER_PAUSE);
	else //HWT_RESUME
		rtd_clearbits( sys_timer[id].cr, TIMERINFO_TIMER_ENABLE);
}

int rtk_timer_control(unsigned char id, unsigned int cmd)
{
    if (id >= MAX_HWTIMER_NUM)
	{
		BUG();
        return 1;
	}
    switch(cmd) {
        case HWT_INT_CLEAR:
            //rtd_setbits(UMSK_ISR_reg, 1 << (sys_timer[id].isr_bit));
            clear_irq_reason(id);
            break;
        case HWT_START:
            rtk_timer_set_hwt_enable(id, 1);
            // Clear Interrupt Pending (must after enable)
            //rtd_setbits(ISR_reg, 1 << (sys_timer[id].isr_bit));
            clear_irq_reason(id);
            break;
        case HWT_STOP:
            rtk_timer_set_hwt_enable(id, 0);
            break;
        case HWT_PAUSE:
            rtk_timer_set_hwt_pause(id, 1);
            break;
        case HWT_RESUME:
            rtk_timer_set_hwt_pause(id, 0);
            break;
        case HWT_INT_ENABLE:
            rtk_timer_set_interrupt(id, 1);
            break;
        case HWT_INT_DISABLE:
            rtk_timer_set_interrupt(id, 0);
            break;
        default:
            return 1;
    }
    return 0;
}

#if 0 // before 4.2
static void cevt_set_mode(enum clock_event_mode mode,
                        struct clock_event_device *clk)
{
    switch (mode) {
    case CLOCK_EVT_MODE_PERIODIC:
        create_timer(SYSTEM_TIMER, TIMERINFO_BASE_CLOCK/HZ, TIMER);
        break;

    case CLOCK_EVT_MODE_ONESHOT:
        /* period set, and timer enabled in 'next_event' hook */
        create_timer(SYSTEM_TIMER, TIMERINFO_BASE_CLOCK/HZ, COUNTER);
        break;

    case CLOCK_EVT_MODE_RESUME:
	pr_info("ready to resume clock event device...\n");
	break;

    case CLOCK_EVT_MODE_SHUTDOWN:
	rtk_timer_control(SYSTEM_TIMER, HWT_INT_DISABLE);
//	rtk_timer_control(SYSTEM_TIMER, HWT_STOP);
	break;

    case CLOCK_EVT_MODE_UNUSED:
    default: break;
    }
}
#else
static int get_cpu_by_evt_dev(struct clock_event_device *clk)
{
	if(clk==&rtk_clock_event[0])
		return 0;
	else if(clk==&rtk_clock_event[1])
		return 1;
	else if(clk==&rtk_clock_event[2])
		return 2;	
	else if(clk==&rtk_clock_event[3])
		return 3;
	return 0;	
}	
static int cevt_set_state_periodic(struct clock_event_device *clk)
{
	int cpu=get_cpu_by_evt_dev(clk);
	int timer_id=timer_id_map[cpu];
	create_timer(timer_id, TIMERINFO_BASE_CLOCK/HZ, TIMER);
	return 0;
}
static int cevt_set_state_oneshot(struct clock_event_device *clk)
{
	int cpu=get_cpu_by_evt_dev(clk);
	int timer_id=timer_id_map[cpu];
	create_timer(timer_id, TIMERINFO_BASE_CLOCK/HZ, COUNTER);
	return 0;
}
static int cevt_set_state_oneshot_stopped(struct clock_event_device *clk)
{
	int cpu=get_cpu_by_evt_dev(clk);
	int timer_id=timer_id_map[cpu];
//	rtk_timer_control(timer_id, HWT_STOP);
	rtk_timer_control(timer_id, HWT_INT_DISABLE);
//	pr_info("%s %d cpu:%d stopped\n",__FILE__,__LINE__,cpu);
	return 0;
}
static int cevt_set_state_shutdown(struct clock_event_device *clk)
{
	int cpu=get_cpu_by_evt_dev(clk);
	int timer_id=timer_id_map[cpu];
	rtk_timer_control(timer_id, HWT_INT_DISABLE);
//	pr_info("%s %d cpu:%d shutdown\n",__FILE__,__LINE__,cpu);
	return 0;
}
static int cevt_tick_resume(struct clock_event_device *clk)
{
	pr_info("ready to resume clock event device...\n");

	return 0;
}
#endif

static int cevt_set_next_event(unsigned long evt,
                        struct clock_event_device *clk)
{
	int cpu=get_cpu_by_evt_dev(clk);
	int timer_id=timer_id_map[cpu];
	unsigned long cnt=0;
	int res=0;
//	rtk_timer_set_hwt_pause(timer_id,1);
	rtk_timer_control(timer_id, HWT_INT_ENABLE);
//	rtk_timer_set_interrupt(timer_id, 1);
	cnt = rtd_inl(sys_timer[timer_id].cvr); //aligned to timer 0's cvr
//	rtd_outl(sys_timer[timer_id].cvr,cnt);
	cnt += evt;
	rtd_outl(sys_timer[timer_id].tvr, cnt);
	res = ((int)(rtd_inl(sys_timer[timer_id].cvr) - cnt) > 0) ? -ETIME : 0;

//	rtk_timer_set_hwt_pause(timer_id,0);
	pr_debug("%s %d %d %d %ld\n",__FILE__,__LINE__,res,cpu,evt);
	return res;
}

void cevt_suspend(struct clock_event_device *evt)
{
	int cpu=get_cpu_by_evt_dev(evt);
	int timer_id=timer_id_map[cpu];
	rtk_timer_control(timer_id, HWT_INT_DISABLE);
	cevt_set_state_shutdown(evt);
	pr_debug("api:%s cpu:%d\n",__func__,cpu);
}

void cevt_resume(struct clock_event_device *evt)
{
    // ensure the timer is off before we do the next step...
	int cpu=get_cpu_by_evt_dev(evt);
	int timer_id=timer_id_map[cpu];
	rtk_timer_control(timer_id, HWT_INT_DISABLE);
//	rtk_timer_control(timer_id, HWT_INT_ENABLE);
	cevt_set_state_shutdown(evt);
	pr_debug("api:%s cpu:%d\n",__func__,cpu);
//  rtk_timer_control(SYSTEM_TIMER, HWT_STOP);
}

//Chuck TODO
#if (0)
static unsigned int __init estimate_cpu_frequency(void)
{
   unsigned int value_m, value_n, value_o;
   unsigned int freq, tmp;

   // use CRT register to estimate cpu frequency
   if ((rtd_inl(SYS_PLL_SCPU2_reg) & SYS_PLL_SCPU2_pllscpu_oeb_mask) == 0) {
      tmp = rtd_inl(SYS_PLL_SCPU1_reg);
      value_m = ((tmp & SYS_PLL_SCPU1_pllscpu_m_mask) >> SYS_PLL_SCPU1_pllscpu_m_shift);
      value_n = ((tmp & SYS_PLL_SCPU1_pllscpu_n_mask) >> SYS_PLL_SCPU1_pllscpu_n_shift);
      value_o = ((tmp & SYS_PLL_SCPU1_pllscpu_o_mask) >> SYS_PLL_SCPU1_pllscpu_o_shift);
      freq = 27 * (value_m+2) / (value_n+1) / (value_o+1);
   }
   else {
      freq = 27;
   }

   return freq;
}
#endif

static u64 rtk_read_sched_clock(void)
{
    int timer_id=timer_id_map[0];
    return rtd_inl(sys_timer[timer_id].cvr) + clock_offset;
}

static cycle_t rtk_hpt_read(struct clocksource *cs);
static void rtk_hpt_suspend(struct clocksource *cs);
static void rtk_hpt_resume(struct clocksource *cs);
static struct clocksource clocksource_rtk[4] = {
       { .name           = "RTK_CSRC",
        .read           = rtk_hpt_read,
        .mask           = CLOCKSOURCE_MASK(32),
        .flags          = CLOCK_SOURCE_IS_CONTINUOUS | CLOCK_SOURCE_SUSPEND_NONSTOP ,
        .suspend        = rtk_hpt_suspend,
	.rating		= 450,
        .resume         = rtk_hpt_resume, },
       { .name           = "RTK_CSRC",
        .read           = rtk_hpt_read,
        .mask           = CLOCKSOURCE_MASK(32),
        .flags          = CLOCK_SOURCE_IS_CONTINUOUS | CLOCK_SOURCE_SUSPEND_NONSTOP,
        .suspend        = rtk_hpt_suspend,
	.rating		= 450,
        .resume         = rtk_hpt_resume, },
       { .name           = "RTK_CSRC",
        .read           = rtk_hpt_read,
        .mask           = CLOCKSOURCE_MASK(32),
        .flags          = CLOCK_SOURCE_IS_CONTINUOUS | CLOCK_SOURCE_SUSPEND_NONSTOP,
        .suspend        = rtk_hpt_suspend,
	.rating		= 450,
        .resume         = rtk_hpt_resume, },
       { .name           = "RTK_CSRC",
        .read           = rtk_hpt_read,
        .mask           = CLOCKSOURCE_MASK(32),
        .flags          = CLOCK_SOURCE_IS_CONTINUOUS | CLOCK_SOURCE_SUSPEND_NONSTOP,
        .suspend        = rtk_hpt_suspend,
	.rating		= 450,
        .resume         = rtk_hpt_resume, }
};

static int get_cpu_by_cs(struct clocksource *cs)
{
	if(cs==&clocksource_rtk[0])
		return 0;
	else if(cs==&clocksource_rtk[1])
		return 1;
	else if(cs==&clocksource_rtk[2])
		return 2;
	else if(cs==&clocksource_rtk[3])
		return 3;
	return 0;
}


static cycle_t rtk_hpt_read(struct clocksource *cs)
{
	int cpu=0;//get_cpu_by_cs(cs);
	int timer_id=timer_id_map[cpu];
//	pr_info("%s %d\n",__FILE__,__LINE__);
	return rtd_inl(sys_timer[timer_id].cvr);
}

#ifdef CONFIG_ARM_ARCH_TIMER

unsigned int u32_CNTCVLW, u32_CNTCVUP, timer_resume_done;

void suspend_cs_timestamp(void)
{
#ifndef CONFIG_OPTEE 
	rtd_outl(WRAP_A7_APB_TSG_EN0, 0x8);
	u32_CNTCVLW = rtd_inl(WRAP_A7_APB_TSG_EN1);
	rtd_outl(WRAP_A7_APB_TSG_EN0, 0xc);
	u32_CNTCVUP = rtd_inl(WRAP_A7_APB_TSG_EN1);
	timer_resume_done = 0;
#endif
}

void resume_cs_timestamp(void)
{
#ifndef CONFIG_OPTEE
	if(timer_resume_done)
		return;

	rtd_outl(WRAP_A7_APB_TSG_EN0, 0);
	rtd_outl(WRAP_A7_APB_TSG_EN1, 0);

	rtd_outl(WRAP_A7_APB_TSG_EN0, 0x8);
	rtd_outl(WRAP_A7_APB_TSG_EN1, u32_CNTCVLW);

	rtd_outl(WRAP_A7_APB_TSG_EN0, 0xc);
	rtd_outl(WRAP_A7_APB_TSG_EN1, u32_CNTCVUP);

	// configure coresight CNTCR
	rtd_outl(WRAP_A7_APB_TSG_EN0, 0);
	rtd_outl(WRAP_A7_APB_TSG_EN1, 1);

	// configure coresight CNTFID0
	rtd_outl(WRAP_A7_APB_TSG_EN0, 0x20);
	rtd_outl(WRAP_A7_APB_TSG_EN1, 0x019bfcc0);

	timer_resume_done = 1;
#endif
}
#endif

static void rtk_hpt_suspend(struct clocksource *cs)
{
	int cpu=get_cpu_by_cs(cs);
	int timer_id=timer_id_map[cpu];
	if(cpu==0)
		clock_offset = rtk_read_sched_clock();
	rtk_timer_control(timer_id, HWT_STOP);
#ifdef CONFIG_ARM_ARCH_TIMER
	suspend_cs_timestamp();
#endif
	pr_debug("api:%s cpu:%d\n",__func__,cpu);
}

static void rtk_hpt_resume(struct clocksource *cs)
{
	int cpu=get_cpu_by_cs(cs);
	int timer_id=timer_id_map[cpu];
	rtk_timer_control(timer_id, HWT_START);
#ifdef CONFIG_ARM_ARCH_TIMER
	resume_cs_timestamp();
#endif
	pr_debug("api:%s cpu:%d\n",__func__,cpu);
}

int rtk_clocksource_init(int cpu)
{
	int timer_id=timer_id_map[cpu];
	int ret;
        /* Calculate a somewhat reasonable rating value */
	clocksource_rtk[cpu].rating = 450;//400 + TIMERINFO_BASE_CLOCK / 10000000;
	if(cpu==0)
		sched_clock_register(rtk_read_sched_clock, 32, TIMERINFO_BASE_CLOCK);

	rtk_timer_control(timer_id, HWT_START);	
        return 0;
}

char clock_name[4][35]=
{
	{"system timer 0"},
	{"system timer 1"},
	{"system timer 2"},
	{"system timer 3"}
};
extern void arch_timer_delay_timer_register(void);

void rtk_timer_init(void);
static int clock_suspend_flag[4]={0,0,0,0};
static int rtk_timer_cpu_notify(struct notifier_block *self,
                                           unsigned long action, void *hcpu)
{
	int cpu=raw_smp_processor_id();
        switch (action & ~CPU_TASKS_FROZEN) {
        case CPU_STARTING:
//		if(!clock_suspend_flag[cpu])
			rtk_timer_init();
#if 0
		else {
			if(cpu!=0) {
				cevt_resume(&rtk_clock_event[cpu]);
				rtk_hpt_resume(&clocksource_rtk[cpu]);
			}
		}
#endif
		clock_suspend_flag[cpu]=1;
                break;
        case CPU_DYING:
#if 1
		if(clock_suspend_flag[cpu])
		{
			if(cpu!=0) {
				rtk_hpt_suspend(&clocksource_rtk[cpu]);
				cevt_suspend(&rtk_clock_event[cpu]);
			}
		}
#endif
                break;
        }

        return NOTIFY_OK;
}

static struct notifier_block rtk_timer_cpu_nb = {
        .notifier_call = rtk_timer_cpu_notify,
};



static struct cyclecounter cyclecounter[4] = {
	{
        	.read   = rtk_get_counter0,
        	.mask   = CLOCKSOURCE_MASK(32),
	},
	{
        	.read   = rtk_get_counter1,
        	.mask   = CLOCKSOURCE_MASK(32),
	},
	{
        	.read   = rtk_get_counter2,
        	.mask   = CLOCKSOURCE_MASK(32),
	},
	{
        	.read   = rtk_get_counter3,
        	.mask   = CLOCKSOURCE_MASK(32),
	},
};

void rtk_timer_init(void)
{
    
    unsigned int ver = get_ic_version();
    int cpu=raw_smp_processor_id();
    unsigned int est_freq_hw_timer = 0;
    est_freq_hw_timer = 27;

    if(ver >= VERSION_C)
    {
#ifdef CONFIG_REALTEK_SCHED_LOG
  	sched_log_time_scale = est_freq_hw_timer;
#endif
	if(cpu==0) {
        	//enable timer2
        	rtk_timer_control(SYSTEM_TIMER, HWT_START);
	}
	return;
    }
    struct cpumask *cpu_mask;
    struct clock_event_device *evt = &rtk_clock_event[cpu];

    //printk("Estimated  CPU  frequency: %d MHz\n", est_freq);
    //printk("Estimated Timer frequency: %d MHz\n", est_freq_hw_timer);

#ifdef CONFIG_REALTEK_SCHED_LOG
//  sched_log_time_scale = est_freq;
    sched_log_time_scale = est_freq_hw_timer;
#endif // CONFIG_REALTEK_SCHED_LOG

    if(clock_suspend_flag[cpu]==0)
        memset(evt, 0, sizeof(struct clock_event_device));
//    sprintf(evt->name,"%s %d","system timer",cpu);
    evt->features       =  CLOCK_EVT_FEAT_ONESHOT ; 
//    evt->features      |=CLOCK_EVT_FEAT_C3STOP;
    evt->rating         = 450;//HZ; // 350
#if 0 // before 4.2
    evt->set_mode       = cevt_set_mode;
#else
//	evt->set_state_periodic = cevt_set_state_periodic;
	evt->set_state_oneshot = cevt_set_state_oneshot;
	evt->set_state_oneshot_stopped = cevt_set_state_oneshot_stopped;
	evt->set_state_shutdown = cevt_set_state_shutdown;
	evt->tick_resume = cevt_tick_resume;
#endif
    evt->set_next_event = cevt_set_next_event;
    evt->shift          = 32;
    evt->mult           = div_sc(TIMERINFO_BASE_CLOCK, NSEC_PER_SEC, evt->shift);
    evt->max_delta_ns   = clockevent_delta2ns(0x7fffffff, evt);
    evt->min_delta_ns   = clockevent_delta2ns(0x300, evt);
    evt->cpumask        = cpumask_of(cpu); // cpu_all_mask
//    evt->cpumask        = cpu_all_mask;
    evt->suspend        = cevt_suspend;
    evt->resume         = cevt_resume;
    evt->name           = (char *)&clock_name[cpu][0];

//    cpu_mask = (struct cpumask *)get_cpu_mask(cpu);   // ISR move to cpu1
//    cpumask_set_cpu(cpu, cpu_mask);
//    cpumask_set_cpu(cpu, evt->cpumask);

    if(cpu==0 && clock_suspend_flag[cpu]==0) 
    {
    	evt->irq            = gic_irq_find_mapping(SPI_MISC_TIMER8);
    	irq_set_affinity(evt->irq, cpumask_of(cpu));		
        setup_irq(evt->irq, &rtk_timer_irq);
	if(cpu==0) {  //for sched log only, no irq attached for timer2
        	//enable timer2
        	rtk_timer_control(SYSTEM_TIMER, HWT_START);
	}
    }
    else if(cpu==1 && clock_suspend_flag[cpu]==0)
    {
    	evt->irq            = gic_irq_find_mapping(SPI_MISC_TIMER9);
//    	irq_set_affinity(evt->irq, cpu_mask);		
    	setup_irq(evt->irq, &rtk_timer_irq1);
    }
    else if(cpu==2 && clock_suspend_flag[cpu]==0)
    {
    	evt->irq            = gic_irq_find_mapping(SPI_MISC_TIMER10);
    	setup_irq(evt->irq, &rtk_timer_irq2);
    }
    else if(cpu==3 && clock_suspend_flag[cpu]==0)
    {
    	evt->irq            = gic_irq_find_mapping(SPI_MISC_TIMER11);
    	setup_irq(evt->irq, &rtk_timer_irq3);
    }
 	
//    clockevents_register_device(evt);
    if(cpu==0) 
    {
	arch_timer_delay_timer_register();
        register_cpu_notifier(&rtk_timer_cpu_nb);
    }

    evt->set_state_shutdown(evt);
//    clockevents_config_and_register(evt, 27000000, 0xf, 0x7fffffff);
//  rtk_timer_control(timer_id_map[0], HWT_START);
//original timer.c
    clockevents_register_device(evt);
#ifdef CONFIG_REALTEK_SCHED_LOG
    sched_log_time_scale = est_freq_hw_timer;
#endif // CONFIG_REALTEK_SCHED_LOG
    rtk_clocksource_init(cpu);
#if 1
    if(cpu==0) 
    {
	static struct timecounter timecounter[4];
        u64 start_count;
	start_count=0;//rtk_timer_get_value(timer_id_map[cpu]);
        clocksource_register_hz(&clocksource_rtk[cpu], 27000000);
        cyclecounter[cpu].mult = clocksource_rtk[cpu].mult;
        cyclecounter[cpu].shift = clocksource_rtk[cpu].shift;
        timecounter_init(&timecounter[cpu], &cyclecounter[cpu], start_count);
    }
#endif
}

void second_timer_irq_setup(void)
{
	rtk_timer_init();
}

