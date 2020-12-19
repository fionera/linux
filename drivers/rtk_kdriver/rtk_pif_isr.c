//Copyright (C) 2007-2013 Realtek Semiconductor Corporation.
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#ifdef CONFIG_ARM64
#include <mach/irqs.h>
#endif

#include "rtk_ddomain_isr.h"
#include <rbus/ppoverlay_reg.h>
#include <rbus/timer_reg.h>
#include <rbus/sfg_reg.h>
#include <rbus/sys_reg_reg.h>
#include <scaler/scalerCommon.h>
#include <tvscalercontrol/scaler_vscdev.h>
#include <tvscalercontrol/scaler_vbedev.h>
#include <rbus/vgip_reg.h>
#include <rbus/tcon_reg.h>

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

#ifndef UINT8
typedef unsigned char  UINT8;
#endif
#ifndef UINT16
typedef unsigned short UINT16;
#endif
#ifndef UINT32
typedef unsigned int   UINT32;
#endif


/* Function Prototype */
static int pif_isr_suspend(struct platform_device *dev, pm_message_t state);
static int pif_isr_resume(struct platform_device *dev);

extern unsigned char vbe_disp_oled_orbit_enable;
extern unsigned char vbe_disp_oled_orbit_mode;
static struct platform_device *pif_isr_platform_devs = NULL;
static ORBIT_PIXEL_SHIFT_STRUCT pre_orbit_shift = {0};

static struct platform_driver pif_isr_platform_driver = {
#ifdef CONFIG_PM
	.suspend		= pif_isr_suspend,
	.resume			= pif_isr_resume,
#endif
	.driver = {
		.name		= "pif_isr",
		.bus		= &platform_bus_type,
	},
};


#ifdef CONFIG_PM
static int pif_isr_suspend(struct platform_device *dev, pm_message_t state)
{
	sfg_sfg_irq_ctrl_0_RBUS sfg_sfg_irq_ctrl_0_reg;

	printk(KERN_NOTICE "[pif_ISR]%s %d\n",__func__,__LINE__);

	//enable sfg interrupt
	sfg_sfg_irq_ctrl_0_reg.regValue = rtd_inl(SFG_SFG_irq_ctrl_0_reg);
	sfg_sfg_irq_ctrl_0_reg.sfg_tim_irq_en = 0;	//sfg vsync output
	rtd_outl(SFG_SFG_irq_ctrl_0_reg, sfg_sfg_irq_ctrl_0_reg.regValue);

	printk(KERN_NOTICE "[pif_ISR] suspend done\n");

	return 0;
}

static int pif_isr_resume(struct platform_device *dev)
{
	sys_reg_int_ctrl_scpu_RBUS sys_reg_int_ctrl_scpu_reg;
	sfg_sfg_irq_ctrl_0_RBUS sfg_sfg_irq_ctrl_0_reg;

	printk(KERN_NOTICE "[pif_ISR]%s %d\n",__func__,__LINE__);

	// enable route to SCPU,  Dctl_int_2_scpu_routing_en
	sys_reg_int_ctrl_scpu_reg.regValue = 0;
	sys_reg_int_ctrl_scpu_reg.osd_int_scpu_routing_en = 1;  //Interrupt enable (osd_int & pif_irq)
	sys_reg_int_ctrl_scpu_reg.write_data = 1;
	rtd_outl(SYS_REG_INT_CTRL_SCPU_reg, sys_reg_int_ctrl_scpu_reg.regValue);

	//enable sfg interrupt
	sfg_sfg_irq_ctrl_0_reg.regValue = rtd_inl(SFG_SFG_irq_ctrl_0_reg);
	sfg_sfg_irq_ctrl_0_reg.sfg_tim_irq_en = 5;	//sfg vsync output
	rtd_outl(SFG_SFG_irq_ctrl_0_reg, sfg_sfg_irq_ctrl_0_reg.regValue);

	printk(KERN_NOTICE "[pif_ISR] resume done\n");

	return 0;
}

#endif


irqreturn_t pif_isr(int irq, void *dev_id)
{
	unsigned int ret=IRQ_NONE;
	sfg_sfg_irq_ctrl_1_RBUS sfg_sfg_irq_ctrl_1_reg;
    ppoverlay_double_buffer_ctrl2_RBUS ppoverlay_double_buffer_ctrl2_reg;
	ORBIT_PIXEL_SHIFT_STRUCT orbit_shift;
	//static unsigned int cost_time=0;


	sfg_sfg_irq_ctrl_1_reg.regValue = IoReg_Read32(SFG_SFG_irq_ctrl_1_reg);

	if(sfg_sfg_irq_ctrl_1_reg.sfg_tim_irq_flag & _BIT2){
        ppoverlay_double_buffer_ctrl2_reg.regValue = IoReg_Read32(PPOVERLAY_Double_Buffer_CTRL2_reg);
        //pr_info("pif_isr orbit: memc_dtgreg_dbuf_set= %d \n", ppoverlay_double_buffer_ctrl2_reg.memc_dtgreg_dbuf_set);

        if(!ppoverlay_double_buffer_ctrl2_reg.memc_dtgreg_dbuf_set && vbe_disp_oled_orbit_enable)
        {
            orbit_shift = Get_Orbit_Shift_Data();
            if((pre_orbit_shift.x != orbit_shift.x) || (pre_orbit_shift.y != orbit_shift.y))
            {
                //pr_info("pif_isr: line count den_end=  %d\n", PPOVERLAY_new_meas1_linecnt_real_get_memcdtg_line_cnt_rt(IoReg_Read32(PPOVERLAY_new_meas1_linecnt_real_reg)) );
                if(vbe_disp_oled_orbit_mode == _VBE_PANEL_ORBIT_JUSTSCAN_MODE){
                    //pr_info("pif_isr orbit: line count den_end=  %d\n", PPOVERLAY_new_meas1_linecnt_real_get_memcdtg_line_cnt_rt(IoReg_Read32(PPOVERLAY_new_meas1_linecnt_real_reg)) );

                    if(vbe_disp_orbit_set_position_justscan(orbit_shift.x, orbit_shift.y) ==0)
                    {
                        pre_orbit_shift = orbit_shift;
                    }
                }
            }
        }
        ppoverlay_double_buffer_ctrl2_reg.regValue = IoReg_Read32(PPOVERLAY_Double_Buffer_CTRL2_reg);
        ppoverlay_double_buffer_ctrl2_reg.memcdtgreg_dbuf_en = 1;
        IoReg_Write32(PPOVERLAY_Double_Buffer_CTRL2_reg, ppoverlay_double_buffer_ctrl2_reg.regValue);

		IoReg_Write32(SFG_SFG_irq_ctrl_1_reg, _BIT2);
	}
	if(sfg_sfg_irq_ctrl_1_reg.sfg_tim_irq_flag & _BIT0){
		vbe_disp_dynamic_polarity_control_analyze_pattern();
		//use tcon18 0xb802d458[7] decide enable 28s toggle or not.
		if(TCON_TCON18_Ctrl_get_tcon18_en(IoReg_Read32(TCON_TCON18_Ctrl_reg))){

			if(vbe_disp_tcon_28s_toggle_get_state()== DISP_TCON_TOGGLE_STATE_NONE){
				vbe_disp_tcon_28s_toggle_handle_state(DISP_TCON_TOGGLE_STATE_NONE);
				vbe_disp_tcon_28s_toggle_set_state(DISP_TCON_TOGGLE_STATE_INIT);
			}

			if(vbe_disp_tcon_28s_toggle_check_timeout()){
				if(vbe_disp_tcon_28s_toggle_get_state()== DISP_TCON_TOGGLE_STATE_INIT){
					vbe_disp_tcon_28s_toggle_set_state(DISP_TCON_TOGGLE_STATE_ACTIVE1);
				}else if(vbe_disp_tcon_28s_toggle_get_state()== DISP_TCON_TOGGLE_STATE_ACTIVE1){
					vbe_disp_tcon_28s_toggle_set_state(DISP_TCON_TOGGLE_STATE_ACTIVE2);
				}else if(vbe_disp_tcon_28s_toggle_get_state()== DISP_TCON_TOGGLE_STATE_ACTIVE2){
					vbe_disp_tcon_28s_toggle_set_state(DISP_TCON_TOGGLE_STATE_INIT);
				}else{
					vbe_disp_tcon_28s_toggle_set_state(DISP_TCON_TOGGLE_STATE_INIT);
				}
				vbe_disp_tcon_28s_toggle_handle_state(vbe_disp_tcon_28s_toggle_get_state());
			}
		}


/*
		if(!cost_time){
			cost_time = IoReg_Read32(TIMER_SCPU_CLK90K_LO_reg);
		}else{
			pr_err("[pif_isr] period:%d,(%x) \n", (IoReg_Read32(TIMER_SCPU_CLK90K_LO_reg)- cost_time)/90, IoReg_Read32(0xb8028248));
			cost_time = IoReg_Read32(TIMER_SCPU_CLK90K_LO_reg);
		}
*/
		IoReg_Write32(SFG_SFG_irq_ctrl_1_reg, _BIT0);
		ret = IRQ_HANDLED;
	}
	return ret;
}

extern u32 gic_irq_find_mapping(u32 hwirq);//cnange interrupt register way
static int __init pif_isr_init_irq(void)
{
	sys_reg_int_ctrl_scpu_RBUS sys_reg_int_ctrl_scpu_reg;
	sfg_sfg_irq_ctrl_0_RBUS sfg_sfg_irq_ctrl_0_reg;



	IoReg_Write32(SFG_SFG_irq_ctrl_0_reg, IoReg_Read32(SFG_SFG_irq_ctrl_0_reg));

	/* Request IRQ */
	if(request_irq(gic_irq_find_mapping(IRQ_DDOMAIN),
                   pif_isr,
                   IRQF_SHARED,
                   "PIF ISR",
                   (void *)pif_isr_platform_devs))
	{
		pr_err("pif_isr: cannot register IRQ %d\n", gic_irq_find_mapping(IRQ_DDOMAIN));
		return -ENXIO;
	}

	// enable route to SCPU,  Dctl_int_2_scpu_routing_en
	sys_reg_int_ctrl_scpu_reg.regValue = 0;
	sys_reg_int_ctrl_scpu_reg.osd_int_scpu_routing_en = 1;  //Interrupt enable (osd_int & pif_irq)
	sys_reg_int_ctrl_scpu_reg.write_data = 1;
	rtd_outl(SYS_REG_INT_CTRL_SCPU_reg, sys_reg_int_ctrl_scpu_reg.regValue);

	//enable sfg interrupt
	sfg_sfg_irq_ctrl_0_reg.regValue = rtd_inl(SFG_SFG_irq_ctrl_0_reg);
	sfg_sfg_irq_ctrl_0_reg.sfg_tim_irq_en = 5;	//sfg vsync output
	rtd_outl(SFG_SFG_irq_ctrl_0_reg, sfg_sfg_irq_ctrl_0_reg.regValue);

	pr_info("pif_isr: register IRQ %d\n", gic_irq_find_mapping(IRQ_DDOMAIN));

	return 0;
}
#if 0
static char *vgip_isr_devnode(struct device *dev, mode_t *mode)
{
	return NULL;
}
#endif
static int pif_isr_init_module(void)
{

	int result;
	pif_isr_platform_devs = platform_device_register_simple("pif_isr", -1, NULL, 0);
	if (platform_driver_register(&pif_isr_platform_driver) != 0) {
                printk("pif_isr: can not register platform driver...\n");
                result = -EINVAL;
                return result;
        }
	result = pif_isr_init_irq();
	if (result < 0) {
		pr_err("pif_isr: can not register irq...\n");
		return result;
	}

	return 0;

}

void pif_isr_exit_module(void)
{
	free_irq(gic_irq_find_mapping(IRQ_DDOMAIN), (void *)pif_isr);
}

module_init(pif_isr_init_module);
module_exit(pif_isr_exit_module);

