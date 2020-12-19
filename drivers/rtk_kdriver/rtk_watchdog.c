#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/ioctl.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/version.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0))
#include <linux/sched/types.h>
#endif
#include <linux/sched/rt.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <rbus/iso_misc_reg.h>
#include <rbus/wdog_reg.h>
#include <linux/freezer.h>
#include <rtk_kdriver/rtd_logger.h>
#include <rbus/sb2_reg.h>
#include <mach/platform.h>
#include <rtk_kdriver/rtk_watchdog.h>
#include <rtd_types.h>
#include <generated/autoconf.h>
#include <rtk_kdriver/io.h>
#include <rbus/sys_reg_reg.h>
#include <rbus/VE1/vde_reg.h>
#include <rbus/VE1/rif_reg_reg.h>

#define WDOG_DEVICE_NAME			"watchdog"
#define WDOG_FREQUENCY              27000000     //wdog clock is 27Mhz
#define WDOG_SECS_TO_TICKS(secs)    ((secs) * WDOG_FREQUENCY)
#define WDOG_TICKS_TO_SECS(ticks)   ((ticks) / WDOG_FREQUENCY)
#define WDOG_TIMEOUT_MAX            60
#define WDIOS_ENABLE_WITH_KERNEL_AUTO_KICK     0x0008          /* Turn on the watchdog timer,and kernel will kick watchdog by itself */

typedef enum{
	RHAL_WDOG_IS_ON_NEED_AP_KICK = 0,
	RHAL_WDOG_IS_OFF,
	RHAL_WDOG_IS_ON_WITH_KERNEL_AUTO_KICK,
	RHAL_WDOG_STATUS_MAX	
}RHAL_WDOG_STATUS_T;

unsigned int wdog_major = 0;
unsigned int wdog_minor = 0;
static struct wdog_dev *wdog_drv_dev;
static struct class *wdog_class;
static unsigned int rtk_wdog_kick_by_ap = 0;
static unsigned int rtk_wdog_timeout_value = WDOG_TIMEOUT_MAX;
static __kernel_time_t rtk_wdog_expires;
static DEFINE_MUTEX(rtk_wdog_mutex);
static unsigned long    watchdog_task = 0;
extern int is_in_dcmt_isr;

/* From Dora: */
/* Command register: */
/* 0xb8008EB4[24] => 0: off; 1: on */
/* Status register: */
/* 0xb8008EB8[15:0] for error status */
#define CMD_REG (0xb8008EB4)
#define STA_REG (0xb8008EB8)


extern EnumPanicCpuType g_rtdlog_panic_cpu_type;
static __maybe_unused bool rtd_show_log[NORMAL] = {1,1,1,1,1,1,1};

#ifdef CONFIG_REALTEK_UART2RBUS_CONTROL
extern void enable_uart2rbus(unsigned int value);
#endif

void DDR_scan_set_error(int cpu)
{
	unsigned int reg_base = STA_REG;
	unsigned int reg_mask;
	unsigned int reg_shift;
	unsigned int ctrl_reg_base = CMD_REG;
	unsigned int i = 0;

	if (cpu >= DDR_SCAN_STATUS_NUM)
		return;

	if ((rtd_inl(ctrl_reg_base) & _BIT24) == 0x0)
		return;

	switch(cpu) {
		case DDR_SCAN_STATUS_SCPU: // SCPU
			reg_mask = 0x00000001;
			reg_shift = 0;
			break;
		case DDR_SCAN_STATUS_ACPU1: // ACPU1
			reg_mask = 0x00000002;
			reg_shift = 1;
			break;
		case DDR_SCAN_STATUS_ACPU2: // ACPU2
			reg_mask = 0x00000004;
			reg_shift = 2;
			break;
		case DDR_SCAN_STATUS_VCPU1: // VCPU1
			reg_mask = 0x00000008;
			reg_shift = 3;
			break;
		case DDR_SCAN_STATUS_VCPU2: // VCPU2
			reg_mask = 0x00000010;
			reg_shift = 4;
			break;
		case DDR_SCAN_STATUS_KCPU: // KCPU
			reg_mask = 0x00000020;
			reg_shift = 5;
			break;
		case DDR_SCAN_STATUS_GPU: // GPU
			reg_mask = 0x00000040;
			reg_shift = 6;
			break;
		default:
			return;
	}

	// wait and get hw semaphore 2
	while(1) {
		if ((rtd_inl(SB2_HD_SEM_NEW_2_reg) & _BIT0) == 0x1)
			break;

		for(i = 0;i < 1024;i++); //reduce rbus traffic
	}

	rtd_maskl(reg_base, ~(reg_mask), 1 << reg_shift);

	// release hw semaphore 2
	rtd_outl(SB2_HD_SEM_NEW_2_reg, 0x0);

}

extern unsigned int debugMode; // declaire in mach-rtdxxxx/init.c
static int my_panic(const char *fmt, ...)
{
#ifdef CONFIG_REALTEK_UART2RBUS_CONTROL
	enable_uart2rbus(1);
#endif
	if (fmt) {
		struct va_format vaf;
		va_list args;

		va_start(args, fmt);

		vaf.fmt = fmt;
		vaf.va = &args;

		if (debugMode == RELEASE_LEVEL)
			panic("%pV", &vaf);
		else {
#ifdef CONFIG_CUSTOMER_TV010
			panic("%pV", &vaf);
#else
                        if(is_in_dcmt_isr){
                                va_end(args);
                                return 0;
                        }

			pr_err("%pV", &vaf);
#if defined (CONFIG_REALTEK_LOGBUF)
			if(rtd_show_log[g_rtdlog_panic_cpu_type]){
			        //DumpWork.param = g_rtdlog_panic_cpu_type;
			        //queue_work(p_dumpqueue, &DumpWork.work);
			        rtd_show_log[g_rtdlog_panic_cpu_type] = 0;
			        rtdlog_watchdog_dump_work(g_rtdlog_panic_cpu_type);
			}
#endif //   #if defined (CONFIG_REALTEK_LOGBUF)
#endif
		}

		va_end(args);
	}

	return 0;
}

static int flag_watchdog=0;
static void rtk_watchdog_set_flag(int flag)
{
	flag_watchdog = flag;
}

int rtk_watchdog_get_flag(void)
{
	return flag_watchdog;
}

/* return value: 1 => success, 0 => failure */
int kill_watchdog (void)
{
    watchdog_task = 0;
    return 1;
}

int judge_watchdog (void)
{
    return watchdog_task;
}

/*If hw watchdog enable, return 1, otherwise return 0.*/
int is_watchdog_enable(void)
{
	if((rtd_inl(WDOG_TCWCR_reg) & WDOG_TCWCR_wden_mask) == 0xa5)
		return 0;
	else
		return 1;
}


//WDOG_TCWOV_reg 0xB8062210
//WDOG_TCWCR_reg 0xB8062204
int watchdog_enable (unsigned char On)
{
    if (On)
    {
        if(rtd_inl(WDOG_TCWOV_reg) == 0x10237980 || rtd_inl(WDOG_TCWOV_reg) == 0x10237880){
            rtd_maskl(WDOG_TCWCR_reg, ~WDOG_TCWCR_wden_mask, 0xa5);
            rtd_outl(WDOG_TCWOV_reg, 0x10237880);
        }else {
        /* enable watchdog */
        rtd_outl(WDOG_TCWTR_reg, 0x01);
        rtd_maskl(WDOG_TCWCR_reg, ~WDOG_TCWCR_wden_mask, 0xa5);
#ifdef CONFIG_CUSTOMER_TV010
        rtd_outl(WDOG_TCWOV_reg, 0xB43EA00);//7.0s // Max: 0x11000000 10.56s
#else
        rtd_outl(WDOG_TCWOV_reg, 0x8000000);//4.97s
#endif
        rtd_clearbits(WDOG_TCWCR_reg, WDOG_TCWCR_wden_mask);
        rtk_watchdog_set_flag(1);
        }
    }
    else
    {
        /* disable watchdog */
        rtd_maskl(WDOG_TCWCR_reg, ~WDOG_TCWCR_wden_mask, 0xa5);
        rtk_watchdog_set_flag(2);
    }
    return 1;
}

//used to adjust watchodg max timeout value from 10s to 60s
void rtk_wdog_adjust_timeout_upper_bound(void)
{
	if(!(rtd_inl(WDOG_TCWCR_reg) & WDOG_TCWCR_upper_bound_sel_mask)){
		rtd_setbits(WDOG_TCWCR_reg, WDOG_TCWCR_upper_bound_sel_mask);
	}
}

void rtk_wdog_dump_vcpu_status (void)
{
	static unsigned int cnt = 0;
	unsigned int clken0 = 0;
	if(cnt % 10 == 0) {
		pr_err("=== dump VCPU1 status (0xB8001200 ~ 0XB800122C) ===\n");
		pr_err("I0    :%08x I1    :%08x EPC    :%08x CAUSE :%08x\n", rtd_inl(VDE_VC_TRACE_0_reg), rtd_inl(VDE_VC_TRACE_1_reg), rtd_inl(VDE_VC_TRACE_2_reg), rtd_inl(VDE_VC_TRACE_3_reg));
		pr_err("STATUS:%08x ECAUSE:%08x ESTATUS:%08x INTVEC:%08x\n", rtd_inl(VDE_VC_TRACE_4_reg), rtd_inl(VDE_VC_TRACE_5_reg), rtd_inl(VDE_VC_TRACE_6_reg), rtd_inl(VDE_VC_TRACE_7_reg));
		pr_err("SP    :%08x RA    :%08x RETIRE :%08x DEBUG :%08x\n", rtd_inl(VDE_VC_TRACE_8_reg), rtd_inl(VDE_VC_TRACE_9_reg), rtd_inl(VDE_VC_TRACE_10_reg), rtd_inl(VDE_VC_DBG_reg));
		pr_err("===================================================\n");

		clken0 = rtd_inl(SYS_REG_SYS_CLKEN0_reg);
		if(SYS_REG_SYS_CLKEN0_get_clken_ve(clken0)) {
			pr_err("=== dump VE status ===\n");
			pr_err("%x : %08x\n", RIF_REG_VE_CTRL_reg, rtd_inl(RIF_REG_VE_CTRL_reg));
			pr_err("%x : %08x\n", RIF_REG_VE_INT_STATUS_reg, rtd_inl(RIF_REG_VE_INT_STATUS_reg));
			pr_err("%x : %08x\n", RIF_REG_VE_INT_EN_reg, rtd_inl(RIF_REG_VE_INT_EN_reg));
			pr_err("%x : %08x\n", RIF_REG_FPGA_DBG1_reg, rtd_inl(RIF_REG_FPGA_DBG1_reg));
			pr_err("======================\n");
		}

	}
	cnt++;
}

// SB2_DUMMY_2_reg:
//		[31:24] : audio1
//		[23:16] : audio2
//		[15: 0] : video1
// SB2_DUMMY_3_reg:
//		[31:16] : kcpu
//		[15: 0] : video2

#define ACPU1_WDOG_MAGIC_VALUE	(0x23000000)
#define ACPU1_WDOG_MAGIC_MASK	(0xFF000000)

#define ACPU2_WDOG_MAGIC_VALUE	(0x00790000)
#define ACPU2_WDOG_MAGIC_MASK	(0x00FF0000)

#define VCPU1_WDOG_MAGIC_VALUE	(0x00002379)
#define VCPU1_WDOG_MAGIC_MASK	(0x0000FFFF)

#define VCPU2_WDOG_MAGIC_VALUE	(0x00002379)
#define VCPU2_WDOG_MAGIC_MASK	(0x0000FFFF)

#define KCPU_WDOG_MAGIC_VALUE	(0x23790000)
#define KCPU_WDOG_MAGIC_MASK	(0xFFFF0000)

int hw_watchdog (void *p)
{
#define TO_MSEC 2000

	unsigned long to_a, to_v, to_k;
	unsigned int val_dummy_2, val_dummy_3;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO-1 };
	watchdog_task = (unsigned long)current;
#if 0 // fix me - jinn - kernel porting
	daemonize("hw_watchdog");
#endif
	sched_setscheduler(current, SCHED_FIFO, &param);
	current->flags &= ~PF_NOFREEZE;
//#if 0 /*no need to turn on watchdog, watchdog on/off control by bootcode.*/
#ifdef CONFIG_CUSTOMER_TV010
	watchdog_enable(1);
	printk("enter hw_watchdog!!!    %x\n",current->flags);
#endif
	rtk_wdog_adjust_timeout_upper_bound();
	to_a = to_v = to_k = jiffies + msecs_to_jiffies(TO_MSEC);

	while (1)
	{
		if(rtk_wdog_kick_by_ap == 0)
		{
			//printk("now kernel control kick!!!\n");
			rtd_outl(WDOG_TCWTR_reg, 0x01);
		}
		else
		{
			//printk("now ap control kick!!!\n");
		}

		val_dummy_2 = rtd_inl(SB2_DUMMY_2_reg);
		val_dummy_3 = rtd_inl(SB2_DUMMY_3_reg);

		if ( (val_dummy_2 & VCPU1_WDOG_MAGIC_MASK) == VCPU1_WDOG_MAGIC_VALUE ) // video1
		{
			to_v = jiffies + msecs_to_jiffies(TO_MSEC);
			rtd_maskl(SB2_DUMMY_2_reg, ~VCPU1_WDOG_MAGIC_MASK, 0x0000);
		}
		if ( (val_dummy_2 & ACPU1_WDOG_MAGIC_MASK) == ACPU1_WDOG_MAGIC_VALUE ) // audio1
		{
			to_a = jiffies + msecs_to_jiffies(TO_MSEC);
			rtd_maskl(SB2_DUMMY_2_reg, ~ACPU1_WDOG_MAGIC_MASK, 0x00);
		}
#if 0
		if ( (val_dummy_2 & ACPU2_WDOG_MAGIC_MASK) == ACPU2_WDOG_MAGIC_VALUE ) // audio2
		{
			to_a2 = jiffies + msecs_to_jiffies(TO_MSEC);
			rtd_maskl(SB2_DUMMY_2_reg, ~ACPU2_WDOG_MAGIC_MASK, 0x00);
		}
		if ( (val_dummy_3 & VCPU2_WDOG_MAGIC_MASK) == VCPU2_WDOG_MAGIC_VALUE ) // video2
		{
			to_v2 = jiffies + msecs_to_jiffies(TO_MSEC);
			rtd_maskl(SB2_DUMMY_3_reg, ~VCPU2_WDOG_MAGIC_MASK, 0x0000);
		}
#endif
#ifndef CONFIG_OPTEE
		if ( (val_dummy_3 & KCPU_WDOG_MAGIC_MASK) == KCPU_WDOG_MAGIC_VALUE ) // kcpu
		{
			to_k = jiffies + msecs_to_jiffies(TO_MSEC);
			rtd_maskl(SB2_DUMMY_3_reg, ~KCPU_WDOG_MAGIC_MASK, 0x0000);
		}
#endif

		if (time_is_before_jiffies(to_a)) {
			set_rtdlog_panic_cpu_type(ACPU1);
			DDR_scan_set_error(DDR_SCAN_STATUS_ACPU1);
			my_panic("[A1DSP] WatchDog does not receive signal for %d seconds, value: %08x \n", TO_MSEC/1000, val_dummy_2);
		}
#if 0
		if (time_is_before_jiffies(to_a2)) {
			set_rtdlog_panic_cpu_type(ACPU2);
			DDR_scan_set_error(DDR_SCAN_STATUS_ACPU2);
			my_panic("[A2DSP] WatchDog does not receive signal for %d seconds, value: %08x \n", TO_MSEC/1000, val_dummy_2);
		}
#endif
		if (time_is_before_jiffies(to_v)) {
			set_rtdlog_panic_cpu_type(VCPU1);
			DDR_scan_set_error(DDR_SCAN_STATUS_VCPU1);
			rtk_wdog_dump_vcpu_status();
			my_panic("[V1DSP] WatchDog does not receive signal for %d seconds, value: %08x \n", TO_MSEC/1000, val_dummy_2);
		}
#ifndef CONFIG_OPTEE
		if (time_is_before_jiffies(to_k)) {
			/*ravi_li dump crt info when KCPU not kick watchdog*/
			set_rtdlog_panic_cpu_type(KCPU);
			DDR_scan_set_error(DDR_SCAN_STATUS_KCPU);
			pr_err("read 0x18000100 val == %#x read 0x18000110 val == %#x \n", rtd_inl(0xb8000100), rtd_inl(0xb8000110));
			my_panic("[KDSP] WatchDog does not receive signal for %d seconds, value: %08x \n", TO_MSEC/1000, val_dummy_3);
		}
#endif
#if 0
		if (time_is_before_jiffies(to_v2)) {
			set_rtdlog_panic_cpu_type(VCPU2);
			DDR_scan_set_error(DDR_SCAN_STATUS_VCPU2);
			my_panic("[V2DSP] WatchDog does not receive signal for %d seconds, value: %08x \n", TO_MSEC/1000, val_dummy_3);
		}
#endif
		do
		{
			msleep(100);

			if (freezing(current))
			{
				printk("freeze hw_watchdog!!!\n");
				watchdog_enable(0);
				__refrigerator(false);
				printk(KERN_NOTICE "monitor watchdog thread wakeup\n");
				to_a = to_v = to_k = jiffies + msecs_to_jiffies(TO_MSEC);
			}
			//try_to_freeze();

		}
		while(0);

		if (watchdog_task == 0)
		{
			watchdog_enable(0);
			break;
		}

	}

	printk("hw_watchdog: exit...\n");
	do_exit(0);
	return 0;
}

#if 0
int hw_watchdog(void *p)
{
    struct sched_param param = { .sched_priority = MAX_RT_PRIO-1 };
    //static int tmp=0;
#if 0 // fix me - jinn - kernel porting
    daemonize("hw_watchdog");
#endif
    sched_setscheduler(current, SCHED_FIFO, &param);
    current->flags &= ~PF_NOFREEZE;

    //  printk("************************************\n");
    //  printk("*********watchdog mechanism*********\n");
    //  printk("************************************\n");

    /* enable watchdog */
    #if 0
    rtd_outl(WDOG_TCWTR_reg, 0x01);//outl(0x01, VENUS_MIS_TCWTR);//WDOG_TCWTR_reg
    rtd_outl(WDOG_TCWCR_reg, 0xa5);//outl(0xa5, VENUS_MIS_TCWCR);
    rtd_outl(WDOG_TCWOV_reg, 0xc000000);//outl(0xc000000, VENUS_MIS_TCWOV);
    #endif

    //rtd_outl(WDOG_TCWCR_reg, 0xc000000);//outl(0x00, VENUS_MIS_TCWCR);
    watchdog_task = (unsigned long)current;
    printk("enter hw_watchdog!!!\n");

    while (1)
    {
        msleep_interruptible(1000);
        rtd_outl(WDOG_TCWTR_reg, 0x01);//outl(0x01, VENUS_MIS_TCWTR);

        if (watchdog_task == 0)
        {
            printk("hw_watchdog: exit...\n");
            break;
        }

        /* check if we need to freeze */
#if 0
        if (freezing(current))
        {
            /* disable watchdog */
            outl(0xa5, VENUS_MIS_TCWCR);

            refrigerator();

            /* enable watchdog */
            outl(0xc000000, VENUS_MIS_TCWOV);
            outl(0x00, VENUS_MIS_TCWCR);
        }
#endif

    }
    do_exit(0);
    return 0;
}
#endif

static void rtk_wdog_ping (void)
{
	rtd_outl(WDOG_TCWTR_reg, 0x01);
	rtk_wdog_expires = ktime_to_timespec(ktime_get()).tv_sec + rtk_wdog_timeout_value;
}

static void rtk_wdog_set_timeout(unsigned int timeout)
{
	rtd_outl(WDOG_TCWOV_reg, WDOG_SECS_TO_TICKS(timeout));
	rtk_wdog_timeout_value = timeout;
}

static unsigned int rtk_wdog_get_timeout(void)
{
	//For The LSB 8 bit is fixed to 0x80 in hardware.(TCWOV_sel 32¡¯hxxxx_xxxx will map to 32¡¯hxxxx_xx80.)
	//so need add additional timeout Ticks offset to get the accurate timeout seconds. 
	return WDOG_TICKS_TO_SECS(rtd_inl(WDOG_TCWOV_reg) + 0xff);
}
int rtk_wdog_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int timeout = 0;
	int timeout_left = 0;
	int option =0;
	RHAL_WDOG_STATUS_T wdog_status = RHAL_WDOG_IS_OFF;
	switch(cmd) {
			case WDIOC_KEEPALIVE:
					rtk_wdog_ping();
					break;

			case WDIOC_SETTIMEOUT:
					if (get_user(timeout, (int __user *)arg))
							return -EFAULT;
					if (timeout > WDOG_TIMEOUT_MAX)
					{
							pr_err("[%d][%s]ERR:Set timeout value %d Fail,For timeout value should between: 0 ~ %d seconds!!!\n", __LINE__ ,__func__, timeout, WDOG_TIMEOUT_MAX);
							return -EFAULT;
					}

					if(timeout > 10)
					{
							rtk_wdog_adjust_timeout_upper_bound();
					}
					rtk_wdog_set_timeout(timeout);
					rtk_wdog_ping();
					break;	

			case WDIOC_SETOPTIONS: 
					if (copy_from_user(&option, (int __user *)arg, sizeof(int)))
							return -EFAULT;

					if (option & WDIOS_DISABLECARD) {
							rtk_wdog_kick_by_ap = 0;
							watchdog_enable(0);
					}

					if (option & WDIOS_ENABLECARD) {
							rtk_wdog_kick_by_ap = 1;
							watchdog_enable(1);
							 //Set previous timeout value to watchdog. if this is the first time enable watchdog, We will set max timeout value to watchdog.
							rtk_wdog_set_timeout(rtk_wdog_timeout_value);

							rtk_wdog_ping();
					}
					if (option & WDIOS_ENABLE_WITH_KERNEL_AUTO_KICK) {
							rtk_wdog_kick_by_ap = 0;
							watchdog_enable(1); //use fault watchdog value.
							rtk_wdog_ping();
					}
					break;

			case WDIOC_GETTIMEOUT:
					timeout = rtk_wdog_get_timeout();
					return put_user(timeout, (int __user *)arg);	

			case WDIOC_GETTIMELEFT:
					//get the time that's left before rebooting
					timeout_left = rtk_wdog_expires - ktime_to_timespec(ktime_get()).tv_sec;
					if(timeout_left < 0){
							timeout_left = 0;
					}
					return put_user(timeout_left, (int __user *)arg);
			case WDIOC_GETSTATUS:
					if(is_watchdog_enable()){
							if(rtk_wdog_kick_by_ap == 0){
									wdog_status = RHAL_WDOG_IS_ON_WITH_KERNEL_AUTO_KICK;
							}
							else{
									wdog_status = RHAL_WDOG_IS_ON_NEED_AP_KICK;      
							}
					}
					else{
							wdog_status = RHAL_WDOG_IS_OFF;
					}
					return put_user(wdog_status, (int __user *)arg);
				     
			default:  /* redundant, as cmd was checked against MAXNR */
					pr_err("wdog ioctl code not supported\n");
				        return -ENOTTY;
	}
	return 0;
       
}

static long rtk_wdog_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;

	mutex_lock(&rtk_wdog_mutex);
	ret = rtk_wdog_ioctl(file, cmd, arg);
	mutex_unlock(&rtk_wdog_mutex);

	return ret;
}
struct file_operations wdog_fops = {
        .owner                 =    THIS_MODULE,
        .unlocked_ioctl        =    rtk_wdog_unlocked_ioctl,
};
static __init int rtk_wdog_init_module(void)
{
	int result = -1;
	dev_t dev = 0;

	if (wdog_major) {
			dev = MKDEV(wdog_major, wdog_minor);
			result = register_chrdev_region(dev, 1, WDOG_DEVICE_NAME);
	} else {
			result = alloc_chrdev_region(&dev, wdog_minor, 1, WDOG_DEVICE_NAME);
			wdog_major = MAJOR(dev);
	}
	if (result < 0) {
			pr_err("wdog: can't get major %d\n", wdog_major);
			return result;
	}
	pr_info("wdog:get result:%d,get dev:0x%08x,major:%d\n", result, dev, wdog_major);

	wdog_class = class_create(THIS_MODULE, WDOG_DEVICE_NAME);
	if (IS_ERR(wdog_class))
			return PTR_ERR(wdog_class);

	wdog_drv_dev = kmalloc(sizeof(struct wdog_dev), GFP_KERNEL);
	if (!wdog_drv_dev) {
			device_destroy(wdog_class, dev);
			result = -ENOMEM;
			pr_err("wdog: alloc memory Failed, result = %d\n", result);
			return result;
	}
	memset(wdog_drv_dev, 0, sizeof(struct wdog_dev));

	cdev_init(&wdog_drv_dev->cdev, &wdog_fops);
	wdog_drv_dev->cdev.owner = THIS_MODULE;
	wdog_drv_dev->cdev.ops = &wdog_fops;

	result = cdev_add(&wdog_drv_dev->cdev, dev, 1);
	if (result) {
			device_destroy(wdog_class, dev);
			pr_err("wdog: add cdev Failed, result = %d\n", result);
			goto fail;
	}

	device_create(wdog_class, NULL, dev, NULL, WDOG_DEVICE_NAME); 
	pr_info("rtk_wdog_init_module successfully!! \n");

	return 0;
fail:
	kfree(wdog_drv_dev);
	wdog_drv_dev = NULL;
	return result;
}
static __exit  void  rtk_wdog_cleanup_module(void)
{
    	dev_t devno = MKDEV(wdog_major, wdog_minor);
    	if (wdog_drv_dev) {
        	cdev_del(&(wdog_drv_dev->cdev));
        	device_destroy(wdog_class, MKDEV(wdog_major, wdog_minor));
        	kfree(wdog_drv_dev);
	}
    	class_destroy(wdog_class);
    	unregister_chrdev_region(devno, 1);	
}
EXPORT_SYMBOL(rtk_wdog_init_module);
EXPORT_SYMBOL(rtk_wdog_cleanup_module);

module_init(rtk_wdog_init_module);
module_exit(rtk_wdog_cleanup_module);
MODULE_AUTHOR("Realtek.com");
MODULE_LICENSE("GPL");
