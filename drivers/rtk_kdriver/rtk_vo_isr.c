//Copyright (C) 2007-2013 Realtek Semiconductor Corporation.
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#include <rbus/vodma_reg.h>
#include <rbus/vgip_reg.h>
#include <rbus/timer_reg.h>

#include "rtk_vo_isr.h"
#include <mach/rtk_log.h>
#include <tvscalercontrol/vo/rtk_vo.h>

// for register dump
#include <tvscalercontrol/io/ioregdrv.h>
#undef rtd_outl
#define rtd_outl(x, y)     								IoReg_Write32(x,y)
#undef rtd_inl
#define rtd_inl(x)     									IoReg_Read32(x)
#undef rtd_maskl
#define rtd_maskl(x, y, z)     							IoReg_Mask32(x,y,z)
#undef rtd_setbits
#define rtd_setbits(offset, Mask) rtd_outl(offset, (rtd_inl(offset) | Mask))
#undef rtd_clearbits
#define rtd_clearbits(offset, Mask) rtd_outl(offset, ((rtd_inl(offset) & ~(Mask))))

/* Function Prototype */
static int vo_isr_suspend(struct platform_device *dev, pm_message_t state);
static int vo_isr_resume(struct platform_device *dev);
static long vo_isr_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
void vodma_enableint_allvodmairq(VO_VIDEO_PLANE plane, bool bEnbable);
void vodma_enableint_vpos(VO_VIDEO_PLANE plane, bool bEnbable);
void vodma_enableint_finish(VO_VIDEO_PLANE plane, bool bEnbable);
void vodma_enableint_underflow(VO_VIDEO_PLANE plane, bool bEnbable);
void vodma_enableInt(void);

#undef ABS
#define ABS(x) ((x >= 0) ? x : -(x))
#define _VO_INT_ROUTE_TO_VCPU_ENABLE
#define _DUMP_PERIOD 90000 // dump period = 1 sec
unsigned int dumpStc[2][3]={{0,0,0},{0,0,0}};

static struct platform_device *vo_isr_platform_devs = NULL;
static struct platform_driver vo_isr_platform_driver = {
#ifdef CONFIG_PM
    .suspend	= vo_isr_suspend,
    .resume	= vo_isr_resume,
#endif
    .driver = {
    	.name	= "vo_isr",
    	.bus		= &platform_bus_type,
    },
};

//static struct platform_driver online_isr_platform_driver;

struct file_operations vo_isr_fops = {
    .owner                  = THIS_MODULE,
    .unlocked_ioctl         = vo_isr_ioctl,
};

#ifdef CONFIG_PM
static int vo_isr_suspend(struct platform_device *dev, pm_message_t state)
{
    printk(KERN_NOTICE "[VO_ISR]%s %d\n",__func__,__LINE__);

        // disable VO interrupt
    vodma_enableint_allvodmairq(VO_VIDEO_PLANE_V1, false);
    vodma_enableint_vpos(VO_VIDEO_PLANE_V1, false);
    vodma_enableint_finish(VO_VIDEO_PLANE_V1, false);
    vodma_enableint_underflow(VO_VIDEO_PLANE_V1, false);

    printk(KERN_NOTICE "[VO_ISR] suspend done\n");

    return 0;
}

static int vo_isr_resume(struct platform_device *dev)
{
    sys_reg_int_ctrl_vcpu_RBUS sys_reg_int_ctrl_vcpu_reg;
    sys_reg_int_ctrl_scpu_2_RBUS sys_reg_int_ctrl_scpu_2_reg;

    printk(KERN_NOTICE "[VO_ISR]%s %d\n",__func__,__LINE__);

    // disable route to VCPU
    sys_reg_int_ctrl_vcpu_reg.regValue = 0;
#ifndef _VO_INT_ROUTE_TO_VCPU_ENABLE
    sys_reg_int_ctrl_vcpu_reg.vodma_int_vcpu_routing_en = 1;
    sys_reg_int_ctrl_vcpu_reg.write_data = 0;
    rtd_outl(SYS_REG_INT_CTRL_VCPU_reg, sys_reg_int_ctrl_vcpu_reg.regValue);
#endif

    // enable route to SCPU
    sys_reg_int_ctrl_scpu_2_reg.regValue = 0;
    sys_reg_int_ctrl_scpu_2_reg.vodma_int_scpu_routing_en = 1;
    sys_reg_int_ctrl_scpu_2_reg.write_data = 1;
    rtd_outl(SYS_REG_INT_CTRL_SCPU_2_reg, sys_reg_int_ctrl_scpu_2_reg.regValue);

    // enable VO interrupt
    vodma_enableInt();

    printk(KERN_NOTICE "[VO_ISR] resume done\n");

    return 0;
}

#endif

static long vo_isr_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	dummy_vo_isr_struct dummy;
	int ret = 0;

	switch (cmd) {
	case VO_ISR_IOCT_DUMMY:
		ret = copy_from_user(&dummy, (void *)arg, sizeof(dummy));
		break;
	};

	return ret;
}

irqreturn_t vodma_vpos1_handler(unsigned int vector, unsigned int data)
{
    unsigned int stc = rtd_inl(TIMER_SCPU_CLK90K_LO_reg);
    static int dumpCnt=0;
    if(ABS(stc - dumpStc[VO_VIDEO_PLANE_V1][VODMA_VODMA_VGIP_INTST_vpos_ints1_shift]) > _DUMP_PERIOD){
        printk(KERN_NOTICE "[WARN][VO][ISR][%d] V1.POS@LC=%d\n", dumpCnt, (rtd_inl(VODMA_VODMA_LINE_ST_reg) & 0xfff));
        dumpStc[VO_VIDEO_PLANE_V1][VODMA_VODMA_VGIP_INTST_vpos_ints1_shift] = stc;
        dumpCnt = 0;
    }
    dumpCnt++;

#ifdef _VO_INT_ROUTE_TO_VCPU_ENABLE // [TEST] don't clear interrupt status if VO interrupt route to VCPU too
    if(rtd_inl(SYS_REG_INT_CTRL_VCPU_reg) & SYS_REG_INT_CTRL_VCPU_vodma_int_vcpu_routing_en_mask)
        return IRQ_HANDLED;
#endif

    // clear v1.posision interrupt
    rtd_outl(VODMA_VODMA_VGIP_INTST_reg, VODMA_VODMA_VGIP_INTST_vpos_ints1_mask); //write "1" to clear

    //WarningSec(1, "[VO][ISR] V.POS1\n");
    return IRQ_HANDLED;
}


irqreturn_t vodma_finish_handler(unsigned int vector, unsigned int data)
{
    unsigned int stc = rtd_inl(TIMER_SCPU_CLK90K_LO_reg);
    static int dumpCnt=0;
    if(ABS(stc - dumpStc[VO_VIDEO_PLANE_V1][VODMA_VODMA_VGIP_INTST_finish_ints1_shift]) > _DUMP_PERIOD){
        printk(KERN_NOTICE "[WARN][VO][ISR][%d] V1.Finish@LC=%d\n", dumpCnt, (rtd_inl(VODMA_VODMA_LINE_ST_reg) & 0xfff));
        dumpStc[VO_VIDEO_PLANE_V1][VODMA_VODMA_VGIP_INTST_finish_ints1_shift] = stc;
        dumpCnt = 0;
    }
    dumpCnt++;

#ifdef _VO_INT_ROUTE_TO_VCPU_ENABLE // [TEST] don't clear interrupt status if VO interrupt route to VCPU too
    if(rtd_inl(SYS_REG_INT_CTRL_VCPU_reg) & SYS_REG_INT_CTRL_VCPU_vodma_int_vcpu_routing_en_mask)
        return IRQ_HANDLED;
#endif

    /* clear v1.finish interrupt */
    rtd_outl(VODMA_VODMA_VGIP_INTST_reg, VODMA_VODMA_VGIP_INTST_finish_ints1_mask); //write "1" to clear

    //WarningSec(1, "[VO][ISR] V.END1\n");
    return IRQ_HANDLED;
}


irqreturn_t vodma_buf_underflow_handler(unsigned int vector, unsigned int data)
{
    unsigned int stc = rtd_inl(TIMER_SCPU_CLK90K_LO_reg);
    static int dumpCnt=0;
    if(ABS(stc - dumpStc[VO_VIDEO_PLANE_V1][VODMA_VODMA_VGIP_INTST_buf_underflow_ints1_shift]) > _DUMP_PERIOD){
        printk(KERN_NOTICE "[WARN][VO][ISR][%d] VO1 UNDER(%d)\n", dumpCnt, (rtd_inl(VODMA_VODMA_LINE_ST_reg) & 0xfff));
        dumpStc[VO_VIDEO_PLANE_V1][VODMA_VODMA_VGIP_INTST_buf_underflow_ints1_shift] = stc;
        dumpCnt = 0;
    }
    dumpCnt++;

#ifdef _VO_INT_ROUTE_TO_VCPU_ENABLE // [TEST] don't clear interrupt status if VO interrupt route to VCPU too
    if(rtd_inl(SYS_REG_INT_CTRL_VCPU_reg) & SYS_REG_INT_CTRL_VCPU_vodma_int_vcpu_routing_en_mask)
        return IRQ_HANDLED;
#endif

    /* clear v1.buffer underflow interrupt */
    rtd_outl(VODMA_VODMA_VGIP_INTST_reg, VODMA_VODMA_VGIP_INTST_buf_underflow_ints1_mask ); //write "1" to clear

    //WarningSec(1, "[VO][ISR] VO1 UNDER(%d)\n", (rtd_inl(VODMA_VODMA_LINE_ST_reg) & 0xfff));

    return IRQ_HANDLED;
}


#ifdef VODMA2_EXIST
irqreturn_t vodma2_vpos1_handler(unsigned int vector, unsigned int data)
{
    unsigned int stc = rtd_inl(TIMER_SCPU_CLK90K_LO_reg);
    static int dumpCnt=0;
    if(ABS(stc - dumpStc[VO_VIDEO_PLANE_V2][VODMA2_VODMA2_VGIP_INTST_vpos_ints1_shift]) > _DUMP_PERIOD){
        printk(KERN_NOTICE "[WARN][VO][ISR][%d] V2.POS@%d\n", dumpCnt, (rtd_inl(VODMA_VODMA_LINE_ST_reg) & 0xfff));
        dumpStc[VO_VIDEO_PLANE_V2][VODMA2_VODMA2_VGIP_INTST_vpos_ints1_shift] = stc;
        dumpCnt = 0;
    }
    dumpCnt++;

#ifdef _VO_INT_ROUTE_TO_VCPU_ENABLE // [TEST] don't clear interrupt status if VO interrupt route to VCPU too
    if(rtd_inl(SYS_REG_INT_CTRL_VCPU_reg) & SYS_REG_INT_CTRL_VCPU_vodma_int_vcpu_routing_en_mask)
        return IRQ_HANDLED;
#endif

    /* clear v2.position interrupt */
    rtd_outl(VODMA2_VODMA2_VGIP_INTST_reg, VODMA2_VODMA2_VGIP_INTST_vpos_ints1_mask ); //write "1" to clear

    //WarningSec(1, "[VO][ISR] V.POS2\n");
    return IRQ_HANDLED;
}


irqreturn_t vodma2_finish_handler(unsigned int vector, unsigned int data)
{
    unsigned int stc = rtd_inl(TIMER_SCPU_CLK90K_LO_reg);
    static int dumpCnt=0;
    if(ABS(stc - dumpStc[VO_VIDEO_PLANE_V2][VODMA2_VODMA2_VGIP_INTST_finish_ints1_shift]) > _DUMP_PERIOD){
        printk(KERN_NOTICE "[WARN][VO][ISR][%d] V2.Finish@%d\n", dumpCnt, (rtd_inl(VODMA_VODMA_LINE_ST_reg) & 0xfff));
        dumpStc[VO_VIDEO_PLANE_V2][VODMA2_VODMA2_VGIP_INTST_finish_ints1_shift] = stc;
        dumpCnt = 0;
    }
    dumpCnt++;

#ifdef _VO_INT_ROUTE_TO_VCPU_ENABLE // [TEST] don't clear interrupt status if VO interrupt route to VCPU too
    if(rtd_inl(SYS_REG_INT_CTRL_VCPU_reg) & SYS_REG_INT_CTRL_VCPU_vodma_int_vcpu_routing_en_mask)
        return IRQ_HANDLED;
#endif

    /* clear v2.finish interrupt */
    rtd_outl(VODMA2_VODMA2_VGIP_INTST_reg, VODMA2_VODMA2_VGIP_INTST_finish_ints1_mask ); //write "1" to clear

    //WarningSec(1, "[VO][ISR] V.END2\n");
    return IRQ_HANDLED;
}

irqreturn_t vodma2_buf_underflow_handler(unsigned int vector, unsigned int data)
{
    unsigned int stc = rtd_inl(TIMER_SCPU_CLK90K_LO_reg);
    static int dumpCnt=0;
    if(ABS(stc - dumpStc[VO_VIDEO_PLANE_V2][VODMA2_VODMA2_VGIP_INTST_buf_underflow_ints1_shift]) > _DUMP_PERIOD){
        printk(KERN_NOTICE "[WARN][VO][ISR][%d] VO2 UNDER(%d)\n", dumpCnt, (rtd_inl(VODMA2_VODMA2_LINE_ST_reg) & 0xfff));
        dumpStc[VO_VIDEO_PLANE_V2][VODMA2_VODMA2_VGIP_INTST_buf_underflow_ints1_shift] = stc;
        dumpCnt = 0;
    }
    dumpCnt++;

#ifdef _VO_INT_ROUTE_TO_VCPU_ENABLE // [TEST] don't clear interrupt status if VO interrupt route to VCPU too
    if(rtd_inl(SYS_REG_INT_CTRL_VCPU_reg) & SYS_REG_INT_CTRL_VCPU_vodma_int_vcpu_routing_en_mask)
        return IRQ_HANDLED;
#endif

    // clear VO2 underflow & passive mode error status
    rtd_outl(VODMA2_VODMA2_VGIP_INTST_reg, VODMA2_VODMA2_VGIP_INTST_buf_underflow_ints1_mask|VODMA2_VODMA2_VGIP_INTST_passivemode_ints1_mask); //write "1" to clear

    //WarningSec(1, "[VO][ISR] VO2 UNDER(%d)\n", (rtd_inl(VODMA2_VODMA2_LINE_ST_reg) & 0xfff));
    return IRQ_HANDLED;
}
#endif


irqreturn_t vo_isr(int irq, void *dev_id)
{
    unsigned int ret = IRQ_NONE;

    vodma_vodma_vgip_intpos_RBUS V1_int_enable;
    vodma_vodma_vgip_intst_RBUS V1_int_status;
#ifdef VODMA2_EXIST
    vodma2_vodma2_vgip_intpos_RBUS	V2_int_enable;
    vodma2_vodma2_vgip_intst_RBUS	V2_int_status;
    V2_int_enable.regValue = rtd_inl(VODMA2_VODMA2_VGIP_INTPOS_reg);
    V2_int_status.regValue = rtd_inl(VODMA2_VODMA2_VGIP_INTST_reg);
#endif
    V1_int_enable.regValue = rtd_inl(VODMA_VODMA_VGIP_INTPOS_reg);
    V1_int_status.regValue = rtd_inl(VODMA_VODMA_VGIP_INTST_reg);

    // vo1 v-position ISR
    if (V1_int_enable.vodma_irq_ie && V1_int_enable.vpos_inte1 && V1_int_status.vpos_ints1)
    {
        //Warning("*\n");
        ret = vodma_vpos1_handler(0, 0);
    }

    // vo1 v-finish ISR
    if (V1_int_enable.vodma_irq_ie && V1_int_enable.finish_inte1 && V1_int_status.finish_ints1)
    {
        //		Warning("*");
        ret = vodma_finish_handler(0, 0);
    }

    // vo1 buf-underflow ISR
    if (V1_int_enable.vodma_irq_ie && V1_int_enable.buf_underflow_inte1 && V1_int_status.buf_underflow_ints1)
    {
        //		Warning("-");
        ret = vodma_buf_underflow_handler(0, 0);
    }

    if (V1_int_enable.vodma_irq_ie && V1_int_status.hist_irq_record_ro)
    {
        rtd_outl(VODMA_VODMA_VGIP_INTST_reg, VODMA_VODMA_VGIP_INTST_hist_irq_record_ro_mask ); //write "1" to clear
        ret = IRQ_HANDLED;
    }

    if (V1_int_enable.vodma_irq_ie && V1_int_status.i2rnd_irq_record_ro)
    {
        rtd_outl(VODMA_VODMA_VGIP_INTST_reg, VODMA_VODMA_VGIP_INTST_i2rnd_irq_record_ro_mask ); //write "1" to clear
        ret = IRQ_HANDLED;
    }

    if (V1_int_enable.vodma_irq_ie && V1_int_status.decomp_irq_record_ro)
    {
        rtd_outl(VODMA_VODMA_VGIP_INTST_reg, VODMA_VODMA_VGIP_INTST_decomp_irq_record_ro_mask ); //write "1" to clear
        ret = IRQ_HANDLED;
    }

    if (V1_int_enable.vodma_irq_ie && V1_int_status.vodma_irq_record_ro)
    {
        rtd_outl(VODMA_VODMA_VGIP_INTST_reg, VODMA_VODMA_VGIP_INTST_vodma_irq_record_ro_mask ); //write "1" to clear
        ret = IRQ_HANDLED;
    }

    if (V1_int_enable.vodma_irq_ie && V1_int_status.buf_underflow_ints1
        && (rtd_inl(VODMA_VODMA_V1_DCFG_reg)&_BIT0) && (rtd_inl(VODMA_VODMA_V1INT_reg)&_BIT18)) // vodma go and TimingGen Output Enable
    {
        //		Warning("-");
        ret = vodma_buf_underflow_handler(0, 0);
    }

#ifdef VODMA2_EXIST
    if (V2_int_enable.vodma_irq_ie && V2_int_enable.vpos_inte1 && V2_int_status.vpos_ints1)
    {
        //		Warning("*");
        ret = vodma2_vpos1_handler(0, 0);
    }

    if (V2_int_enable.vodma_irq_ie && V2_int_enable.finish_inte1 && V2_int_status.finish_ints1)
    {
        //		Warning("*");
        ret = vodma2_finish_handler(0, 0);
    }

    if (V2_int_enable.vodma_irq_ie && V2_int_enable.buf_underflow_inte1 && V2_int_status.buf_underflow_ints1)
    {
        //		Warning("-");
        ret = vodma2_buf_underflow_handler(0, 0);
    }

    if (V2_int_enable.vodma_irq_ie && V2_int_status.passivemode_ints1)
    {
        rtd_outl(VODMA2_VODMA2_VGIP_INTST_reg, VODMA2_VODMA2_VGIP_INTST_passivemode_ints1_mask ); //write "1" to clear
        ret = IRQ_HANDLED;
    }

    if (V2_int_enable.vodma_irq_ie && V2_int_status.vodma_irq_record_ro)
    {
        rtd_outl(VODMA2_VODMA2_VGIP_INTST_reg, VODMA2_VODMA2_VGIP_INTST_vodma_irq_record_ro_mask ); //write "1" to clear
        ret = IRQ_HANDLED;
    }

    if (V2_int_enable.vodma_irq_ie && V2_int_status.decomp_irq_record_ro)
    {
        rtd_outl(VODMA2_VODMA2_VGIP_INTST_reg, VODMA2_VODMA2_VGIP_INTST_decomp_irq_record_ro_mask ); //write "1" to clear
        ret = IRQ_HANDLED;
    }

    if (V2_int_enable.vodma_irq_ie && (V2_int_status.buf_underflow_ints1 || V2_int_status.passivemode_ints1)
        && (rtd_inl(VODMA2_VODMA2_V1_DCFG_reg)&_BIT0) && (rtd_inl(VODMA2_VODMA2_V1INT_reg)&_BIT18)) // vodma go and TimingGen Output Enable
    {
        //		Warning("-");
        ret = vodma2_buf_underflow_handler(0, 0);
    }
#endif

    return ret;
}


// default : Turn on all vodma irq
void vodma_enableint_allvodmairq(VO_VIDEO_PLANE plane, bool bEnbable)
{

	switch (plane) {
		case VO_VIDEO_PLANE_V1:
                {
                    vodma_vodma_vgip_intpos_RBUS V1_int_enable;
                    // clear pending
                    rtd_outl(VODMA_VODMA_VGIP_INTST_reg, 0xF000000F); //write "1" to clear

                    V1_int_enable.regValue = rtd_inl(VODMA_VODMA_VGIP_INTPOS_reg);
                    V1_int_enable.vodma_irq_ie = bEnbable;
                    rtd_outl(VODMA_VODMA_VGIP_INTPOS_reg, V1_int_enable.regValue);
                }
                break;

#ifdef VODMA2_EXIST
		case VO_VIDEO_PLANE_V2:
                {
                    vodma2_vodma2_vgip_intpos_RBUS V2_int_enable;
                    // clear pending
                    rtd_outl(VODMA2_VODMA2_VGIP_INTST_reg, 0x3000001F); //write "1" to clear

                    V2_int_enable.regValue = rtd_inl(VODMA2_VODMA2_VGIP_INTPOS_reg);
                    V2_int_enable.vodma_irq_ie = bEnbable;
                    rtd_outl(VODMA2_VODMA2_VGIP_INTPOS_reg, V2_int_enable.regValue);
                }
                break;
#endif
		default:
			break;
	}
}

void vodma_enableint_vpos(VO_VIDEO_PLANE plane, bool bEnbable)
{
	switch (plane) {
		case VO_VIDEO_PLANE_V1:
                {
                    vodma_vodma_vgip_intpos_RBUS V1_int_enable;
                    // clear pending
                    rtd_outl(VODMA_VODMA_VGIP_INTST_reg, 0x04); //write "1" to clear

                    V1_int_enable.regValue = rtd_inl(VODMA_VODMA_VGIP_INTPOS_reg);
                    V1_int_enable.vpos_inte1 = bEnbable;
                    rtd_outl(VODMA_VODMA_VGIP_INTPOS_reg, V1_int_enable.regValue);
                }
                break;

#ifdef VODMA2_EXIST
		case VO_VIDEO_PLANE_V2:
                {
                    vodma2_vodma2_vgip_intpos_RBUS V2_int_enable;
                    // clear pending
                    rtd_outl(VODMA2_VODMA2_VGIP_INTST_reg, 0x04); //write "1" to clear

                    V2_int_enable.regValue = rtd_inl(VODMA2_VODMA2_VGIP_INTPOS_reg);
                    V2_int_enable.vpos_inte1 = bEnbable;
                    rtd_outl(VODMA2_VODMA2_VGIP_INTPOS_reg, V2_int_enable.regValue);
                }
                break;
#endif

		default:
			break;
	}
}


void vodma_enableint_finish(VO_VIDEO_PLANE plane, bool bEnbable)
{
	switch (plane) {
		case VO_VIDEO_PLANE_V1:
                {
                    vodma_vodma_vgip_intpos_RBUS V1_int_enable;
                    // clear pending
                    rtd_outl(VODMA_VODMA_VGIP_INTST_reg, 0x02); //write "1" to clear

                    V1_int_enable.regValue = rtd_inl(VODMA_VODMA_VGIP_INTPOS_reg);
                    V1_int_enable.finish_inte1 = bEnbable;
                    rtd_outl(VODMA_VODMA_VGIP_INTPOS_reg, V1_int_enable.regValue);
                }
                break;

#ifdef VODMA2_EXIST
		case VO_VIDEO_PLANE_V2:
                {
                    vodma2_vodma2_vgip_intpos_RBUS V2_int_enable;
                    // clear pending
                    rtd_outl(VODMA2_VODMA2_VGIP_INTST_reg, 0x02); //write "1" to clear

                    V2_int_enable.regValue = rtd_inl(VODMA2_VODMA2_VGIP_INTPOS_reg);
                    V2_int_enable.finish_inte1 = bEnbable;
                    rtd_outl(VODMA2_VODMA2_VGIP_INTPOS_reg, V2_int_enable.regValue);
                }
                break;
#endif

		default:
			break;
	}
}


void vodma_enableint_underflow(VO_VIDEO_PLANE plane, bool bEnbable)
{
	switch (plane) {
		case VO_VIDEO_PLANE_V1:
                {
                    vodma_vodma_vgip_intpos_RBUS V1_int_enable;
                    // clear pending
                    rtd_outl(VODMA_VODMA_VGIP_INTST_reg, 0x01); //write "1" to clear

                    V1_int_enable.regValue = rtd_inl(VODMA_VODMA_VGIP_INTPOS_reg);
                    V1_int_enable.buf_underflow_inte1 = bEnbable;
                    rtd_outl(VODMA_VODMA_VGIP_INTPOS_reg, V1_int_enable.regValue);
                }
                break;

#ifdef VODMA2_EXIST
		case VO_VIDEO_PLANE_V2:
                {
                    vodma2_vodma2_vgip_intpos_RBUS V2_int_enable;
                    // clear pending
                    rtd_outl(VODMA2_VODMA2_VGIP_INTST_reg, 0x01); //write "1" to clear

                    V2_int_enable.regValue = rtd_inl(VODMA2_VODMA2_VGIP_INTPOS_reg);
                    V2_int_enable.buf_underflow_inte1 = bEnbable;
                    rtd_outl(VODMA2_VODMA2_VGIP_INTPOS_reg, V2_int_enable.regValue);
                }
                break;
#endif

		default:
			break;
	}
}



/**
 * enable interrupt
 *
 * @param 			{ void}
 * @return 			{ void}
 * @ingroup drv_vodma
 */
void vodma_enableInt(void)
{
	rtd_outl(VODMA_VODMA_VGIP_INTST_reg, 0xF000000F); // clear pending //write "1" to clear
	vodma_enableint_allvodmairq(VO_VIDEO_PLANE_V1, true);
	vodma_enableint_vpos(VO_VIDEO_PLANE_V1, true);
	vodma_enableint_finish(VO_VIDEO_PLANE_V1, true);
	vodma_enableint_underflow(VO_VIDEO_PLANE_V1, false);
}


extern u32 gic_irq_find_mapping(u32 hwirq);//cnange interrupt register way
static int __init vo_isr_init_irq(void)
{
    sys_reg_int_ctrl_vcpu_RBUS sys_reg_int_ctrl_vcpu_reg;
    sys_reg_int_ctrl_scpu_2_RBUS sys_reg_int_ctrl_scpu_2_reg;

    /* Request IRQ */
    if(request_irq(gic_irq_find_mapping(IRQ_VODMA),
                            vo_isr,
                            IRQF_SHARED,
                            "VO ISR",
                            (void *)vo_isr_platform_devs))
    {
        pr_err("vo_isr: cannot register IRQ %d\n", gic_irq_find_mapping(IRQ_VODMA));
        return -ENXIO;
    }

    // disable route to VCPU
    sys_reg_int_ctrl_vcpu_reg.regValue = 0;
#ifndef _VO_INT_ROUTE_TO_VCPU_ENABLE
    sys_reg_int_ctrl_vcpu_reg.vodma_int_vcpu_routing_en = 1;
    sys_reg_int_ctrl_vcpu_reg.write_data = 0;
    rtd_outl(SYS_REG_INT_CTRL_VCPU_reg, sys_reg_int_ctrl_vcpu_reg.regValue);
#endif

    // enable route to SCPU
    sys_reg_int_ctrl_scpu_2_reg.regValue = 0;
    sys_reg_int_ctrl_scpu_2_reg.vodma_int_scpu_routing_en = 1;
    sys_reg_int_ctrl_scpu_2_reg.write_data = 1;
    rtd_outl(SYS_REG_INT_CTRL_SCPU_2_reg, sys_reg_int_ctrl_scpu_2_reg.regValue);

    // enable VO interrupt
    vodma_enableInt();

    pr_info("vo_isr: register IRQ %d\n", gic_irq_find_mapping(IRQ_VODMA));

    return 0;
}



static int vo_isr_init_module(void)
{
	int result;
	vo_isr_platform_devs = platform_device_register_simple("vo_isr", -1, NULL, 0);
	if (platform_driver_register(&vo_isr_platform_driver) != 0) {
                printk("vo_isr: can not register platform driver...\n");
                result = -EINVAL;
                return result;
        }
	result = vo_isr_init_irq();
	if (result < 0) {
		pr_err("vo_isr: can not register irq...\n");
		return result;
	}

	return 0;
}

void vo_isr_exit_module(void)
{
	free_irq(gic_irq_find_mapping(IRQ_VODMA), (void *)vo_isr);
}

module_init(vo_isr_init_module);
module_exit(vo_isr_exit_module);

