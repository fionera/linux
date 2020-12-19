#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <rbus/stb_reg.h>
#include <rbus/emcu_reg.h>

#include "../emcu/rtk_kdv_emcu.h"
#include <rtk_kdriver/rtk_emcu_export.h>
#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>
#include <asm/system_misc.h>
#include <asm/suspend.h>

#include <mach/platform.h>

#ifdef CONFIG_RTK_KDRV_IR
#ifdef CONFIG_MULTI_IR_KEY
extern int ir_suspend_protocol_multi_irda(void);
#else
extern int ir_suspend_protocol(void);
#endif
#endif

#ifdef CONFIG_RTK_KDRV_CEC
extern int pm_rtd299x_cec_suspend(void);
#endif

extern platform_info_t platform_info;
extern int rtk_SetIOPinDirection(unsigned long long pininfo, unsigned int value);
extern int emcu_chk_event(void);
void emcu_set_boot_cmd_irda(void);

static void pm_load_st_gpio(void)
{
#if 1   //fixme
    printk("waiting for \"platform_info.pcb_st_enum_parameter\"\n");
#else
    unsigned long long value;
    char *buf, *token;
    buf = platform_info.pcb_st_enum_parameter;
    while(1)
    {
        token = strchr(buf,',');
        if (token){
            token = token + 1;
            value = simple_strtoull(token, 0, 0);
            rtk_SetIOPinDirection(value,0);
        }
        else
            break;

        buf = token;
    }
#endif
}

static void calculate_DDR_checksum(void)
{
    unsigned int *begin_ptr = phys_to_virt(0x02000000);
    unsigned int *end_ptr = phys_to_virt(0x16000000);
    const unsigned int length = 0x01000000/4;
    unsigned int total_checksume = 0;

    while(begin_ptr < end_ptr)
    {
        unsigned int i;
        unsigned int sumValue = 0;
        unsigned int *data_ptr = begin_ptr;

        for(i=0; i<length; i++)
        {
            sumValue+= *data_ptr;
            data_ptr++;
        }

        printk("DDR_checksum(%lx/%x)\n", (unsigned long int)begin_ptr, sumValue);
        total_checksume +=sumValue;
        begin_ptr+=length;

    }
    printk("DDR_checksum total (%x)\n", total_checksume);

    //check for ACPU ddr
    {
        unsigned int *data_ptr = phys_to_virt(0x01600000 );
        unsigned int *end_ptr = phys_to_virt(0x01a00000);
        unsigned int sumValue=0;
        while(data_ptr<end_ptr)
        {
            sumValue+=*data_ptr;
            data_ptr++;
        }
        printk("DDR_checksum A(%x)\n", sumValue);
    }

    //check for VCPU1 ddr
    {
        unsigned int *data_ptr = phys_to_virt(0x01a00000 );
        unsigned int *end_ptr = phys_to_virt(0x01e00000);
        unsigned int sumValue=0;
        while(data_ptr<end_ptr)
        {
            sumValue+=*data_ptr;
            data_ptr++;
        }
        printk("DDR_checksum V1(%x)\n", sumValue);
    }

    //check for VCPU2 ddr
    {
        unsigned int *data_ptr = phys_to_virt(0x01e00000 );
        unsigned int *end_ptr = phys_to_virt(0x01f00000);
        unsigned int sumValue=0;
        while(data_ptr<end_ptr)
        {
            sumValue+=*data_ptr;
            data_ptr++;
        }
        printk("DDR_checksum V2(%x)\n", sumValue);
    }
}

/** NOTICE,
 * wakup_8051 is modified as instead of timer shudown main area,
 * but wait for STANDBYWFIL2 for sure all SCPU activities idle.
 * An additional option will be set for emcu fw, to countdown this waiting.
 * - if timeout, SOC will reboot. (AC reboot)
 * - if option=0, means no reboot. (Keep platform for debugging.)
 **/
void wakeup_8051(int flag)
{
    printk("@@!!!!!enter pm_wakeup_emcu\n");

    /* ================================================
     * refine enable EMCU core flow as DIC's recommend
     * CLKEN=1 > CLKEN=0 > RSTN=1 > CLKEN=1
     * ================================================*/

    if(rtd_inl(0xb8060110) == 0x9021aebe)
		rtd_outl(STB_ST_SRST1_reg, _BIT9);           // rst off
    rtd_outl(STB_ST_CLKEN1_reg, _BIT9 | _BIT0);  // clk enable
    rtd_outl(STB_ST_SRST1_reg, _BIT9 | _BIT0);   // rst on

    udelay(500);

    if(flag == SUSPEND_RAM){
        pr_alert("%s(%u)ecpu reset release done\n",__func__,__LINE__);
    }
    else{
        rtd_outl(STB_ST_SRST1_reg, _BIT9 | _BIT0);
        pr_alert("%s(%u)ecpu reset release done\n",__func__,__LINE__);
#ifdef CONFIG_MULTI_IR_KEY
	while(1);//wait here for 8051 cut down main top power
#endif
    }

    //go forwared to enter optee, where to do cpu_suspend.
    /* fixme, if not optee, SCPU core 0 should also do individual cpu shutdown flow, instead of while(1). */
#if !defined (CONFIG_OPTEE)
        while(1);
#endif
}


void rtkpm_get_param(void){
#ifdef CONFIG_RTK_KDRV_IR
#ifdef CONFIG_MULTI_IR_KEY
    ir_suspend_protocol_multi_irda();
#else
    ir_suspend_protocol();
#endif
    emcu_set_boot_cmd_irda();
#endif
#ifdef CONFIG_RTK_KDRV_CEC
    if(CHK_CEC(GET_EVENT_MSG())){
        printk("\n==>initial CEC suspend block.\n");
        pm_rtd299x_cec_suspend();
        rtd_outl(STB_ST_BUSCLK_reg, rtd_inl(STB_ST_BUSCLK_reg) |
                                (1 << STB_ST_BUSCLK_bus_clksel_shift)); /*let clock is 27Mhz*/
    }
#endif

}

#if defined (CONFIG_RTK_KDRV_EMCU_DCOFF_TIMEOUT)
static int emcu_dcoff_timeout = CONFIG_RTK_KDRV_EMCU_DCOFF_TIMEOUT;

void set_emcu_dcoff_timeout(void)
{
    rtd_outl(RTD_SHARE_MEM_BASE+(4*IDX_REG_DC_OFF_TIMEOUT), emcu_dcoff_timeout);
}

#else
#define set_emcu_dcoff_timeout() do {} while(0)
#endif //#if defined (RTK_KDRV_EMCU_DCOFF_TIMEOUT)


#ifdef CONFIG_RTK_KDRV_WATCHDOG
extern int watchdog_enable (unsigned char On);
#endif
int rtk_pm_load_8051(int flag)
{
    printk("\n%s(%u)2013/07/09 11:30\n",__func__,__LINE__);
    local_irq_disable();

/*MA6PBU-1437 disable watchdog due to 8051 not tick watchdog*/
#ifdef CONFIG_RTK_KDRV_WATCHDOG
	printk("Suspend action...MUST disable watchdog\n");
	watchdog_enable(0);
#endif

    if(flag == SUSPEND_BOOTCODE)
    {
        printk("bootcode call suspend.\n");
    }else if((flag == SUSPEND_RAM )||
             (flag == SUSPEND_WAKEUP))
    {
        unsigned int pm_event_msg;
        unsigned int stm_value;

        pm_event_msg = GET_EVENT_MSG();

        if(flag == SUSPEND_WAKEUP){
            stm_value = STM_WAKEUP;
        }else{
            //for after add rtc_wakeup, stm area only used BIT_28 & BIT_29
            stm_value = (pm_event_msg>>loc_stm) &0x03;
            if(!stm_value)
                stm_value = STM_NOEMAL;
        }

#ifdef CONFIG_MULTI_IR_KEY
        if(stm_value == STM_NOEMAL)
            pm_event_msg |= RENO_STM_MASK;
        else
            pm_event_msg &= ~RENO_STM_MASK;
#else
        //for after add rtc_wakeup, stm area only used BIT_28 & BIT_29
        pm_event_msg = (pm_event_msg&0xcfffffff)|(stm_value<<loc_stm);
#endif
        SET_EVENT_MSG(pm_event_msg);
        printk("Suspend to RAM action...(0x%08x)\n",pm_event_msg);

    }else{
#ifdef CONFIG_RTK_KDRV_WATCHDOG
    /*Jira MAC5P-2215, disable watchdog when enter emcu_on mode*/
    watchdog_enable(0);
#endif
        printk("system call suspend.(disable watchdog)\n");
    }

    if(flag != SUSPEND_WAKEUP)
    {
        pm_load_st_gpio();
    }
    printk("load bin file from DDR\n");
#ifndef CONFIG_TV005
    rtkpm_get_param();
#endif

/* Calculate DDR checksum for make sure STR flow DDR data is not corrupt */
#if 1
    if((rtd_inl(STB_WDOG_DATA14_reg) == 0xfafefafe) && (flag == SUSPEND_RAM))
        calculate_DDR_checksum();
#endif

    set_emcu_dcoff_timeout();

    if(emcu_chk_event())
        BUG_ON(1);
    else
        wakeup_8051(flag);

    return 0;
}

extern int rtk_emcu_smem_set_ir(char *ibuf);
static int rtk_pm_load_8051_code(char *s)
{
    printk("\n%s(%u)2018/03/29 18:00\n",__func__,__LINE__);

#ifndef CONFIG_MULTI_IR_KEY
    if((strlen(s) < 1) || rtk_emcu_smem_set_ir(s+1)){
        pr_alert("\n%s(%u)There are no IR key setting!!!\n",__func__,__LINE__);
        //BUG_ON(1);
    }

    #ifndef CONFIG_TV005
    rtk_pm_load_8051(0);
    #endif
#endif
    return 0;
}

static int rtk_pm_act_8051_code(char *s){
    printk("\n%s(%u)2013/04/08 11:50\n",__func__,__LINE__);
    #ifdef CONFIG_TV005
    // init IrDA
    rtd_outl(ISO_IRDA_IR_PSR_VADDR ,0x5a13133b);
    rtd_outl(ISO_IRDA_IR_PER_VADDR ,0x1162d);
    rtd_outl(ISO_IRDA_IR_SF_VADDR, 0xff00021b);
    rtd_outl(ISO_IRDA_IR_DPIR_VADDR ,0x1f4);
    rtd_outl(ISO_IRDA_IR_CR_VADDR ,0x5df);
    // load 8051
    rtk_pm_load_8051(0);
    #endif
    return 0;
}

__setup("POWERDOWN",rtk_pm_load_8051_code);
__setup("ac_on",    rtk_pm_act_8051_code);

/* end of file */
