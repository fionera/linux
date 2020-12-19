/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2010 by Chien-An Lin <colin@realtek.com.tw>
 *
 * Venus series board platform device registration
 */

#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <mach/platform.h>
#include <mach/irqs.h>
#ifdef CONFIG_RTC_DRV_RTK
#include <rbus/drtc_reg.h>
#endif
#include <rbus/sys_reg_reg.h>
#include <rbus/stb_reg.h>
#include <linux/syscalls.h>
#ifndef CONFIG_ARM64
#include <asm/mach-types.h>
#else
#include <linux/of_reserved_mem.h>
#endif
#ifndef CONFIG_ARM64
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/time.h>
#endif
#include <mach/common.h>
#include <mach/timex.h>
#ifdef CONFIG_REALTEK_LOGBUF
#include <mach/rtk_platform.h>
#endif
#ifdef CONFIG_LG_SNAPSHOT_BOOT
#include "power.h"
#endif
#ifdef CONFIG_CMA_RTK_ALLOCATOR
#ifdef CONFIG_REALTEK_MANAGE_OVERLAPPED_REGION
#include <linux/auth.h>
#endif
#endif
#ifdef CONFIG_HIBERNATION
#include <linux/suspend.h>
#endif
#include <rtk_kdriver/rtk_otp_util.h>

#ifdef CONFIG_RTK_KDRIVER_SUPPORT
#include <rtk_kdriver/rtk_qos_export.h>
#endif

#ifdef CONFIG_RTC_DRV_RTK
static struct resource rtk_rtc_resources[] = {
	[0] = {
		.start	= GET_MAPPED_RBUS_ADDR(RBUS_BASE_VIRT_OLD + DRTC_RTCSEC_reg),
		.end 	= GET_MAPPED_RBUS_ADDR(RBUS_BASE_VIRT_OLD + DRTC_RTCCR_reg + 4 - 1),
		.flags	= IORESOURCE_MEM
	},
};

static struct platform_device rtk_rtc_device = {
	.name		= "rtc-rtk",
	.num_resources	= ARRAY_SIZE(rtk_rtc_resources),
	.resource	= rtk_rtc_resources
};
#endif	//CONFIG_RTC_DRV_RTK

// add by cfyeh
struct device usb_at_platform = {
	.init_name	= "usb",
	.parent		= &platform_bus,
};
EXPORT_SYMBOL_GPL(usb_at_platform);

#if 0
static struct resource rtk_usb_dwc_otg_resources[] = {
	[0] = {
		.start		= GET_MAPPED_RBUS_ADDR(0xb8090000),
		.end		= GET_MAPPED_RBUS_ADDR(0xb8090000 + 0x8000),
		.flags		= IORESOURCE_MEM,
	},

	[1] = {
		.start		= IRQ_USB,
		.end		= IRQ_USB,
		.flags		= IORESOURCE_IRQ,
	},
};

static u64 dwc_otg_dmamask = DMA_BIT_MASK(32);

static struct platform_device rtk_usb_dwc_otg_device = {
	.name		= "dwc_otg",
	.id		= -1,
	.dev = {
		.parent			= &usb_at_platform,
		.dma_mask		= &dwc_otg_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
	.num_resources	= ARRAY_SIZE(rtk_usb_dwc_otg_resources),
	.resource	= rtk_usb_dwc_otg_resources,
};
#endif

#if 0 // get uart clock source from register instead of early_param
#ifdef CONFIG_REALTEK_FPGA
static unsigned long uart_clock = UART_CLOCK_40M;
#else
static unsigned long uart_clock = UART_CLOCK_27M;
#endif
#endif

/*****************************************************************************
 * SD/SDIO/MMC -
 ****************************************************************************/
#if 0
static struct resource rtksdio_resources[] = {
    [0] = {
        .start  = GET_MAPPED_RBUS_ADDR((u32)0xb8010C00),
        .end    = GET_MAPPED_RBUS_ADDR((u32)0xb8010C00 + 0x3ff),
        .flags  = IORESOURCE_MEM,
    },
    [1] = {
        .start  = IRQ_CR,
        .end    = IRQ_CR,
        .flags  = IORESOURCE_IRQ,
    },
};

static u64 rtksdio_dmamask = DMA_BIT_MASK(32);

static struct platform_device rtksdio_device = {
    .name           = "rtksdio",
    .id             = -1,
    .dev            = {
        .dma_mask = &rtksdio_dmamask,
        .coherent_dma_mask = DMA_BIT_MASK(32),
    },
    .num_resources  = ARRAY_SIZE(rtksdio_resources),
    .resource       = rtksdio_resources,
};
#endif

#if defined(CONFIG_MACH_RTK_298x) || defined(CONFIG_MACH_RTK_294X)
static struct resource rtkemmc_resources[] = {
    [0] = {
        .start  = GET_MAPPED_RBUS_ADDR((u32)0xb8010800),
        .end    = GET_MAPPED_RBUS_ADDR((u32)0xb8010800 + 0x3FF),
        .flags  = IORESOURCE_MEM,
    },
    [1] = {
        .start  = IRQ_CR,
        .end    = IRQ_CR,
        .flags  = IORESOURCE_IRQ,
    },
};

static u64 rtkemmc_dmamask = DMA_BIT_MASK(32);

static struct platform_device rtkemmc_device = {
    .name           = "rtkemmc",
    .id             = -1,
    .dev            = {
        .dma_mask = &rtkemmc_dmamask,
        .coherent_dma_mask = DMA_BIT_MASK(32),
    },
    .num_resources  = ARRAY_SIZE(rtkemmc_resources),
    .resource       = rtkemmc_resources,
};
#endif

//#if defined(CONFIG_MACH_RTK_294x) || defined(CONFIG_ARCH_RTK299S)
#if 0
static struct resource rtkemmc_dw_resources[] = {
    [0] = {
        .start  = GET_MAPPED_RBUS_ADDR((u32)0xb8010800),
        .end    = GET_MAPPED_RBUS_ADDR((u32)0xb8010800 + 0x3FF),
        .flags  = IORESOURCE_MEM,
    },
    [1] = {
        .start  = IRQ_CR,
        .end    = IRQ_CR,
        .flags  = IORESOURCE_IRQ,
    },
};

static u64 rtkemmc_dw_dmamask = DMA_BIT_MASK(32);

static struct platform_device rtkemmc_dw_device = {
    .name           = "rtkemmc_dw",
    .id             = -1,
    .dev            = {
        .dma_mask = &rtkemmc_dw_dmamask,
        .coherent_dma_mask = DMA_BIT_MASK(32),
    },
    .num_resources  = ARRAY_SIZE(rtkemmc_dw_resources),
    .resource       = rtkemmc_dw_resources,
};
#endif

/* PMU ( Performance Moniters ) */
#if 0
static struct resource rtk_pmu_resources[] = {
	DEFINE_RES_IRQ(SPI_GICP_PMU_IRQ)
};

static struct platform_device rtk_pmu_device = {
	.name              = "armv7-pmu",
    .id            = -1,
    .num_resources     = ARRAY_SIZE(rtk_pmu_resources),
	.resource = rtk_pmu_resources
};
#endif


#ifdef CONFIG_ARM64
static struct platform_device *rtk_platform_devices[] = {

};
#else
static struct platform_device *rtk_platform_devices[] = {
#ifdef CONFIG_RTC_DRV_RTK
	&rtk_rtc_device,
#endif

#if defined(CONFIG_MACH_RTK_298x) || defined(CONFIG_MACH_RTK_294X) || defined(CONFIG_ARCH_RTK299S)
	&rtksdio_device,        /* SDIO */
#endif

#if defined(CONFIG_MACH_RTK_298x) || defined(CONFIG_MACH_RTK_294X)
	&rtkemmc_device,        /* EMMC */
#endif

//#if defined(CONFIG_MACH_RTK_294x) || defined(CONFIG_ARCH_RTK299S)
#if 1
#ifndef CONFIG_REALTEK_OF //temp patch, it cause system crashed
	&rtkemmc_dw_device,     /* EMMC_PLUS */
#endif
#endif
#if 0
	&rtk_pmu_device		    /* PMU */
#endif
};
#endif

void plat_mem_protect(void)
{
#if 0 //will be enable later
   unsigned int scramble_key = CONTENT_PROTECTION_KEY;
	int index;

   printk("do plat_mem_protect......\n");

   // KCPU code protection
	//rtd_outl(RTD298X_DC_MEM_PROTECT_SADDR0, 0x20000000);
   //rtd_outl(RTD298X_DC_MEM_PROTECT_EADDR0, 0xfffff000);

   //rtd_outl(RTD298X_DC_MEM_PROTECT_SADDR1, (CONFIG_REALTEK_LINUX_LOAD_ADDR-0x1000) & 0x1fffffff);
   //rtd_outl(RTD298X_DC_MEM_PROTECT_EADDR1, 0x1ffff000);

   //sed_rng_init();
   //sed_rng_get(&scramble_key);

	index = 0; //use 1st one to do content protection

   // Content protection
   rtd_outl(DC_MEM_SCRAMBLE_SADDR(index), VIDEO_RINGBUFFER_START);
   rtd_outl(DC_MEM_SCRAMBLE_EADDR(index), VIDEO_RINGBUFFER_START + VIDEO_RINGBUFFER_SIZE);

   rtd_outl(DC_MEM_SCRAMBLE_CURR_KEY0(index), scramble_key);
   rtd_outl(DC_MEM_SCRAMBLE_CURR_KEY1(index), scramble_key);
   rtd_outl(DC_MEM_SCRAMBLE_CURR_KEY2(index), scramble_key);
   rtd_outl(DC_MEM_SCRAMBLE_CURR_KEY3(index), scramble_key);
   rtd_outl(DC_MEM_SCRAMBLE_CURR_KEY4(index), scramble_key);
   rtd_outl(DC_MEM_SCRAMBLE_CURR_KEY5(index), scramble_key);
   rtd_outl(DC_MEM_SCRAMBLE_CURR_KEY6(index), scramble_key);

   rtd_outl(DC_MEM_SCRAMBLE_PREV_KEY0(index), scramble_key);
   rtd_outl(DC_MEM_SCRAMBLE_PREV_KEY1(index), scramble_key);
   rtd_outl(DC_MEM_SCRAMBLE_PREV_KEY2(index), scramble_key);
   rtd_outl(DC_MEM_SCRAMBLE_PREV_KEY3(index), scramble_key);
   rtd_outl(DC_MEM_SCRAMBLE_PREV_KEY4(index), scramble_key);
   rtd_outl(DC_MEM_SCRAMBLE_PREV_KEY5(index), scramble_key);
   rtd_outl(DC_MEM_SCRAMBLE_PREV_KEY6(index), scramble_key);

   rtd_outl(DC_MEM_SCRAMBLE_UPDATE_KEY0(index), scramble_key);
   rtd_outl(DC_MEM_SCRAMBLE_UPDATE_KEY1(index), scramble_key);
   rtd_outl(DC_MEM_SCRAMBLE_UPDATE_KEY2(index), scramble_key);
   rtd_outl(DC_MEM_SCRAMBLE_UPDATE_KEY3(index), scramble_key);
   rtd_outl(DC_MEM_SCRAMBLE_UPDATE_KEY4(index), scramble_key);
   rtd_outl(DC_MEM_SCRAMBLE_UPDATE_KEY5(index), scramble_key);
   rtd_outl(DC_MEM_SCRAMBLE_UPDATE_KEY6(index), scramble_key);

	// Enable all permission for KCPU/ACPU/VCPU
	rtd_outl(DC_MEM_SCRAMBLE_ACCESS(index), 0xffffffff);

   rtd_setbits(DC_MEM_SCRAMBLE_CTRL, BIT(12+index));
   rtd_setbits(DC_MEM_PROTECT_CTRL, BIT(12+index));
#endif
}

extern void rtk_read_persistent_clock(struct timespec *ts);
extern int  rtk_rtc_ip_init(void);

void __init rtk_init_machine(void)
{
   	platform_add_devices(rtk_platform_devices, ARRAY_SIZE(rtk_platform_devices));
}

void __init rtk_init_late(void)
{
	/* Even we use arch_timer, but still need init misc timer2 timer for schedule log used.*/

   // add console driver node
   sys_mknod("/dev/console", S_IFCHR + 0600, new_encode_dev(MKDEV(5, 1)));

#ifdef CONFIG_CMA_RTK_ALLOCATOR
#ifdef CONFIG_REALTEK_MANAGE_OVERLAPPED_REGION
   init_overlapped_region(0x18000, 0x00200);
#endif
#endif

#ifdef CONFIG_HIBERNATION
    register_nosave_region_late(0x00000000, 0x00000100);
#ifdef CONFIG_REALTEK_LOGBUF
    register_nosave_region_late(
                        (CONST_LOGBUF_MEM_ADDR_START) >> PAGE_SHIFT,
                        (CONST_LOGBUF_MEM_ADDR_START + CONST_LOGBUF_MEM_SIZE) >> PAGE_SHIFT);
#endif
#endif
}

unsigned long get_uart_clock(void)
{
#ifdef CONFIG_REALTEK_FPGA
	return UART_CLOCK_40M;
#else
	// uart0 clock source select
	if ((rtd_inl(STB_ST_CLKMUX_reg) & BIT(0)) == 0x0)
	{
		return UART_CLOCK_108M;
	}
	else
	{
		return UART_CLOCK_27M;
	}
#endif
}

#if 0 // get uart clock source from register instead of early_param
static int __init early_uart_clock(char *str)
{
	const char *str_98M = "98M";

	if (strncmp(str_98M, str, strlen(str_98M)) == 0)
	{
		uart_clock = UART_CLOCK_98M;
		pr_info("UART_CLOCK_98M\n");
	}
	else
	{
		uart_clock = UART_CLOCK_27M;
		pr_info("UART_CLOCK_27M");
	}
	return 0;
}

early_param("uartclk", early_uart_clock);
#endif


void __init rtk_init_early(void)
{
   // init clcok&PLL or something in early stage
#ifndef CONFIG_REALTEK_FPGA
#if (SYSTEM_CONSOLE == 0)
#if 0 // set uart clock source at bootcode
	if (get_uart_clock() == UART_CLOCK_98M) {
		if ((rtd_inl(0xb80004e4) & (BIT(0) | BIT(1) | BIT(3))) != 0x9)
		{
			rtd_setbits(0xb80004e4, BIT(0) | BIT(3));
			rtd_clearbits(0xb80004e4, BIT(1));
		}
		rtd_clearbits(0xB8060058, BIT(0));
	}
#endif
#elif (SYSTEM_CONSOLE == 1 || CONFIG_SERIAL_8250_RUNTIME_UARTS == 2 || CONFIG_RTK_KDRV_SERIAL_8250_RUNTIME_UARTS == 2)
	int system_concole = 1;
	CRT_CLK_OnOff(UART, CLK_ON , &system_concole);
#elif (SYSTEM_CONSOLE == 2)
	int system_concole = 2;
	CRT_CLK_OnOff(UART, CLK_ON , &system_concole);
#elif (SYSTEM_CONSOLE == 3)
	int system_concole = 3;
	CRT_CLK_OnOff(UART, CLK_ON , &system_concole);
#endif
#endif

#ifdef CONFIG_REALTEK_PCBMGR // cklai@121106 setup platform related settings
   printk("pcbMgr: RTK\n");
// printk("prom_init(), _prom_envp=%x\n", (unsigned int)_prom_envp);
   // must be called after parse_early_param that will assign _prom_envp from boot command line
   prom_init();
#endif

#ifdef CONFIG_CACHE_L2X0
   rtk_l2_cache_init();
#endif

#ifdef CONFIG_RTC_DRV_RTK
   rtk_rtc_ip_init();
   register_persistent_clock(NULL, rtk_read_persistent_clock);
#endif


	plat_mem_protect();
}

static const char * const rtk_board_compat[] = {
	"rtk,rtd288o",
	NULL
};

#ifndef CONFIG_ARM64
MACHINE_START(RTK288O, "rtd288o")

// .atag_offset   = 0x100,
#ifdef CONFIG_SMP
   .smp          = smp_ops(rtk_smp_ops),
#endif
   .nr_irqs      = 0,
   .reserve      = rtk_reserve,
   .map_io       = rtk_map_io,
   .init_early   = rtk_init_early,
   .init_irq     = gic_init_irq,
#if 0 // fix me - jinncheng - kernel porting
   .handle_irq   = gic_handle_irq,
#endif
   .init_machine = rtk_init_machine,
   .init_late    = rtk_init_late,
#ifdef CONFIG_BRINGUP_RTD288O_HACK /* For KTASKWBS-8666 support VDSO, use nonstop arch_timer instread of MISC timer */
    .init_time    = rtk_timer_init,
#endif
   .restart      = rtk_machine_restart,
	.dt_compat    = rtk_board_compat,
MACHINE_END
#endif



#include <mach/rtk_platform.h>
/**
   list by address order,
   {addr_kxlp, addr_kxlp, size_kxl, size_kxlp}
 */
#define _TBD_ 0
unsigned long carvedout_buf[][4] = {
	{0x00000000, 0x00000000,  1*_MB_,  1*_MB_}, // CARVEDOUT_BOOTCODE
	{0x15400000, 0x15400000,  4*_MB_,  4*_MB_}, // CARVEDOUT_DEMOD
	{0x10000000, 0x10000000,  0*_MB_,  0*_MB_}, // CARVEDOUT_AV_DMEM
	{0x15000000, 0x15000000,  0*_MB_,  0*_MB_}, // CARVEDOUT_K_OS_1
	{0x16000000, 0x16000000, 16*_MB_, 16*_MB_}, // CARVEDOUT_K_OS
	{0x18000000, 0x18000000,  2*_MB_,  2*_MB_}, // CARVEDOUT_MAP_RBUS
	{0x1a500000, 0x1a500000,  9*_MB_,  9*_MB_}, // CARVEDOUT_AV_OS
	{0x1b100000, 0x1b100000,  1*_MB_,  1*_MB_}, // CARVEDOUT_MAP_RPC
	{VIDEO_RINGBUFFER_START, VIDEO_RINGBUFFER_START,  VIDEO_RINGBUFFER_SIZE,  VIDEO_RINGBUFFER_SIZE}, // CARVEDOUT_VDEC_RINGBUF
#ifdef CONFIG_REALTEK_LOGBUF
	{CONST_LOGBUF_MEM_ADDR_START, CONST_LOGBUF_MEM_ADDR_START, CONST_LOGBUF_MEM_SIZE, CONST_LOGBUF_MEM_SIZE}, // CARVEDOUT_LOGBUF
#else
	{_TBD_, _TBD_, _TBD_, _TBD_}, // CARVEDOUT_LOGBUF
#endif
	{0x1fc00000, 0x1fc00000, 0x00008000, 0x00008000}, // CARVEDOUT_ROMCODE
#if 0//def CONFIG_VMSPLIT_3G_OPT
	{0x1ffff000, 0x1ffff000, 0x00001000, 0x00001000}, // CARVEDOUT_IB_BOUNDARY
#else
	{_TBD_, _TBD_, _TBD_, _TBD_}, // CARVEDOUT_IB_BOUNDARY
#endif
	{KXLP_CONST_GAL_MEM_ADDR_START, KXHP_CONST_GAL_MEM_ADDR_START, KXLP_CONST_GAL_MEM_SIZE, KXHP_CONST_GAL_MEM_SIZE}, // CARVEDOUT_GAL
#ifdef CONFIG_LG_SNAPSHOT_BOOT
	{KXLP_SNAPSHOT_RESV_START, KXHP_SNAPSHOT_RESV_START, KXLP_SNAPSHOT_RESV_SIZE, KXHP_SNAPSHOT_RESV_SIZE}, // CARVEDOUT_SNAPSHOT
#else
	{_TBD_, _TBD_, _TBD_, _TBD_}, // CARVEDOUT_SNAPSHOT
#endif
	{KXLP_CONST_GAL_MEM_ADDR_START, KXHP_CONST_GAL_MEM_ADDR_START, KXLP_CONST_GAL_MEM_SIZE, KXHP_CONST_GAL_MEM_SIZE}, // CARVEDOUT_CMA_3
	{KXLP_SCLAER_MODULE_START, KXHP_SCLAER_MODULE_START, KXLP_SCALER_MODULE_SIZE, KXHP_SCALER_MODULE_SIZE}, // CARVEDOUT_SCALER
#ifdef CONFIG_ARM64
    {_TBD_, _TBD_, _TBD_, _TBD_}, // CARVEDOUT_SCALER_BAND
#endif
	{KXLP_SCALER_MEMC_START,    KXHP_SCALER_MEMC_START,     KXLP_SCALER_MEMC_SIZE,      KXHP_SCALER_MEMC_SIZE},     // CARVEDOUT_SCALER_MEMC
	{KXLP_SCALER_MDOMAIN_START, KXHP_SCALER_MDOMAIN_START,  KXLP_SCALER_MDOMAIN_SIZE,   KXHP_SCALER_MDOMAIN_SIZE},  // CARVEDOUT_SCALER_MDOMAIN
	{KXLP_SCALER_DI_NR_START,   KXHP_SCALER_DI_NR_START,    KXLP_SCALER_DI_NR_SIZE,     KXHP_SCALER_DI_NR_SIZE},    // CARVEDOUT_SCALER_DI_NR
	{KXLP_SCALER_NN_START,      KXHP_SCALER_NN_START,       KXLP_SCALER_NN_SIZE,        KXHP_SCALER_NN_SIZE},       // CARVEDOUT_SCALER_NN
	{KXLP_SCALER_VIP_START,     KXHP_SCALER_VIP_START,      KXLP_SCALER_VIP_SIZE,       KXHP_SCALER_VIP_SIZE},      // CARVEDOUT_SCALER_VIP
	{KXLP_SCALER_OD_START,      KXHP_SCALER_OD_START,       KXLP_SCALER_OD_SIZE,        KXHP_SCALER_OD_SIZE},       // CARVEDOUT_SCALER_OD
	{KXLP_VDEC_BUFFER_START,    KXHP_VDEC_BUFFER_START,     KXLP_VDEC_BUFFER_SIZE,      KXHP_VDEC_BUFFER_SIZE},     // CARVEDOUT_VDEC_VBM
	{KXLP_DEMUX_TP_BUFFER_START,KXHP_DEMUX_TP_BUFFER_START, KXLP_DEMUX_TP_BUFFER_SIZE,  KXHP_DEMUX_TP_BUFFER_SIZE}, // CARVEDOUT_TP

	{KXLP_CMA_HIGH_START, KXHP_CMA_HIGH_START, 4*_MB_, 4*_MB_}, // CARVEDOUT_VT

	{_TBD_, 0x7ffff000, _TBD_, 0x1000 }, // CARVEDOUT_DDR_BOUNDARY

#ifdef CONFIG_ZONE_ZRAM
	{0, 0, _TBD_, _TBD_}, // CARVEDOUT_ZONE_ZRAM
#endif

    // for desired cma size calculation
	{KXLP_CMA_LOW_START, KXHP_CMA_LOW_START, KXLP_CMA_LOW_RESERVED_SIZE, KXHP_CMA_LOW_RESERVED_SIZE}, // CARVEDOUT_CMA_LOW
	{KXLP_CMA_HIGH_START, KXHP_CMA_HIGH_START, (KXLP_CMA_HIGH_RESERVED + KXLP_CMA_HIGH_RESERVED_SIZE), (KXHP_CMA_HIGH_RESERVED + KXHP_CMA_HIGH_RESERVED_SIZE)}, // CARVEDOUT_CMA_HIGH

#ifdef CONFIG_OPTEE_SUPPORT_MC_ALLOCATOR
	{KXLP_CMA_MC_START, KXHP_CMA_MC_START, KXLP_CMA_MC_SIZE, KXHP_CMA_MC_SIZE}, // CARVEDOUT_CMA_MC
#endif
};

static enum PLAFTORM_TYPE platform=PLATFORM_OTHER;
enum PLAFTORM_TYPE get_platform (void)
{
    return platform;
}

EXPORT_SYMBOL(get_platform);

int is_DDR4(void)
{
	if (get_platform() == PLATFORM_K6HP)
		return 1;
	else
		return 0;
}

EXPORT_SYMBOL(is_DDR4);

#ifdef CONFIG_CUSTOMER_TV006
static int boot_freqnum=2;
#else
static int boot_freqnum=4;
#endif
static int intDemod = 1; // default use internal demod

static int __init early_parse_intDemod(char *str)
{
	ulong val;

	if (!str) {
		intDemod = 1; // default use internal demod
		return 0;
	}

	sscanf(str, "%lx", &val);
	intDemod = (unsigned int)val;

	return 0;
}
early_param("intdemod", early_parse_intDemod);

webos_model_info_t webosModel;
static int __init early_parse_webosModel(char *str)
{
    const char *jpON="18";
    if (strncmp(jpON,str,strlen(jpON))==0)
    {
	pr_info("this is JP Model\n");
        webosModel = jpModel;
    }
    else
    {
        pr_info("this is not JP Model\n");
        webosModel = NonJpModel;
    }   
    return 0;
}
early_param("countryGrp", early_parse_webosModel);


static int __init early_parse_dvfs (char *str)
{
    const char * dvfs_2="2";
    const char * dvfs_4="4";
    if (strncmp(dvfs_2,str,strlen(dvfs_2))==0)
    {
           boot_freqnum=2;
    }
    else if (strncmp(dvfs_4,str,strlen(dvfs_4))==0)
    {
           boot_freqnum=4;
    }
    else
    {
           boot_freqnum=2;
    }
    return 0 ;
}

static int __init early_parse_platform (char *str)
{
    const char *str_kxlp="K6LP";
    const char *str_kxhp="K6HP";

    if (strncmp(str_kxlp,str,strlen(str_kxlp))==0)
    {
        platform=PLATFORM_K6LP;
        pr_info("K6LP platform\n");
    }
    else if (strncmp(str_kxhp,str,strlen(str_kxhp))==0)
    {
        platform=PLATFORM_K6HP;
        pr_info("K6HP platform\n");
    }
    else
    {
        pr_err("Wrong platform  ? \e[1;31m%s\e[0m\n",str);
    }

    return 0;
}

int is_platform_dvs_enable(void)
{
	return platform_info.enable_dvs;
}

int platform_get_dvfs(void)
{
	return boot_freqnum;
}
early_param("chip", early_parse_platform);
early_param("boot_dvfs", early_parse_dvfs);


/*****************************************
(Sam temp section)
attempt to remove
******************************************/
//*
webos_info_t webos_tooloption;
static int __init early_webos_toolopt_backlight_type (char *str)
{
    const char * opt1_backlight_type_0="direct";
    const char * opt1_backlight_type_1="edgeled";
    const char * opt1_backlight_type_2="oled";

    if (strncmp(opt1_backlight_type_0,str,strlen(opt1_backlight_type_0))==0)
    {
           webos_tooloption.eBackLight = direct;
    }
    else if (strncmp(opt1_backlight_type_1,str,strlen(opt1_backlight_type_1))==0)
    {
           webos_tooloption.eBackLight = edgeled;
    }
    else if (strncmp(opt1_backlight_type_2,str,strlen(opt1_backlight_type_2))==0)
    {
           webos_tooloption.eBackLight = oled;
    }
	pr_info("[pyToolOpt]eBackLight:%d\n",webos_tooloption.eBackLight);
    return 0 ;
}
early_param("eBackLight",early_webos_toolopt_backlight_type);

static int __init early_webos_toolopt_local_dim_block (char *str)
{
    const char * opt1_local_dim_block_6="6";
    const char * opt1_local_dim_block_12="12";
    const char * opt1_local_dim_block_32="32";
    const char * opt1_local_dim_block_36="36";

    if (strncmp(opt1_local_dim_block_6,str,strlen(opt1_local_dim_block_6))==0)
    {
           webos_tooloption.eLEDBarType = local_dim_block_6;
    }
    else if (strncmp(opt1_local_dim_block_12,str,strlen(opt1_local_dim_block_12))==0)
    {
           webos_tooloption.eLEDBarType = local_dim_block_12;
    }
    else if (strncmp(opt1_local_dim_block_32,str,strlen(opt1_local_dim_block_32))==0)
    {
           webos_tooloption.eLEDBarType = local_dim_block_32;
    }
    else if (strncmp(opt1_local_dim_block_36,str,strlen(opt1_local_dim_block_36))==0)
    {
           webos_tooloption.eLEDBarType = local_dim_block_36;
    }
pr_info("[pyToolOpt]local dim block:%d\n",webos_tooloption.eLEDBarType);
    return 0 ;
}

early_param("eLEDBarType",early_webos_toolopt_local_dim_block);

static int __init early_webos_toolopt_inch_type (char *str)
{
    const char * opt1_inch_type_65="65";

    if (strncmp(opt1_inch_type_65,str,strlen(opt1_inch_type_65))==0)
    {
           webos_tooloption.eModelInchType = inch_65;
    }
    else
    {
           webos_tooloption.eModelInchType = inch_49;
    }
pr_info("[pyToolOpt]InchType:%d\n",webos_tooloption.eModelInchType);
    return 0 ;
}
early_param("eModelInchType",early_webos_toolopt_inch_type);

static int __init early_webos_toolopt_tool_type (char *str)
{
    const char * opt1_tool_sm85="NANO85";

    if (strncmp(opt1_tool_sm85,str,strlen(opt1_tool_sm85))==0)
    {
           webos_tooloption.eModelToolType = tool_NANO85;
    }
    else
    {
           webos_tooloption.eModelToolType = tool_UN77;
    }
pr_info("[pyToolOpt]ToolType:%d\n",webos_tooloption.eModelToolType);
    return 0 ;
}
early_param("eModelToolType",early_webos_toolopt_tool_type);

static int __init early_webos_toolopt_maker_type (char *str)
{
    webos_tooloption.eModelModuleType = module_LGD;
    pr_info("[pyToolOpt]ModuleType:%d\n",webos_tooloption.eModelModuleType);
    return 0 ;
}
early_param("eModelModuleType",early_webos_toolopt_maker_type);

static int __init early_webos_toolopt_local_dimming (char *str)
{
    const char * opt3_local_dimming="0"; // 0:off ,1:on

    if (strncmp(opt3_local_dimming,str,strlen(opt3_local_dimming))==0)
    {
           webos_tooloption.bLocalDimming = local_dimming_off;
    }
    else 
    {
           webos_tooloption.bLocalDimming = local_dimming_on;
    }
pr_info("[pyToolOpt]localDimming:%d\n",webos_tooloption.bLocalDimming);
    return 0 ;
}
early_param("bLocalDimming",early_webos_toolopt_local_dimming);
//*/
/*****************************************
(END Sam temp section)
******************************************/

webos_strInfo_t webos_strToolOption;

static int __init early_webos_strToolOpt_backLight_type (char *str)
{
	//webos_strToolOption.eBackLight = (char*)malloc(strlen(str)+1);
	strncpy(webos_strToolOption.eBackLight, str, sizeof(webos_strToolOption.eBackLight)-1);
	webos_strToolOption.eBackLight[sizeof(webos_strToolOption.eBackLight)-1] = '\0';

pr_info("[pyStrToolOpt]eBackLight:%s\n",webos_strToolOption.eBackLight);
    return 0 ;
}
early_param("eBackLight",early_webos_strToolOpt_backLight_type);

static int __init early_webos_strToolOpt_local_dim_block (char *str)
{
	//webos_strToolOption.eLEDBarType = (char*)malloc(strlen(str)+1);
	strncpy(webos_strToolOption.eLEDBarType, str, sizeof(webos_strToolOption.eLEDBarType)-1);
	webos_strToolOption.eLEDBarType[sizeof(webos_strToolOption.eLEDBarType)-1] = '\0';
	
pr_info("[pyStrToolOpt]local dim block:%s\n",webos_strToolOption.eLEDBarType);
    return 0 ;
}
early_param("eLEDBarType",early_webos_strToolOpt_local_dim_block);

static int __init early_webos_strToolOpt_inch_type (char *str)
{
	//webos_strToolOption.eModelInchType = (char*)malloc(strlen(str)+1);
	strncpy(webos_strToolOption.eModelInchType, str, sizeof(webos_strToolOption.eModelInchType)-1);
	webos_strToolOption.eModelInchType[sizeof(webos_strToolOption.eModelInchType)-1] = '\0';

pr_info("[pyStrToolOpt]InchType:%s\n",webos_strToolOption.eModelInchType);
    return 0 ;
}
early_param("eModelInchType",early_webos_strToolOpt_inch_type);

static int __init early_webos_strToolOpt_tool_type (char *str)
{
	//webos_strToolOption.eModelToolType = (char*)malloc(strlen(str)+1);
	strncpy(webos_strToolOption.eModelToolType, str, sizeof(webos_strToolOption.eModelToolType)-1);
	webos_strToolOption.eModelToolType[sizeof(webos_strToolOption.eModelToolType)-1] = '\0';

pr_info("[pyStrToolOpt]ToolType:%s\n",webos_strToolOption.eModelToolType);
    return 0 ;
}
early_param("eModelToolType",early_webos_strToolOpt_tool_type);

static int __init early_webos_strToolOpt_maker_type (char *str)
{
	//webos_strToolOption.eModelModuleType = (char*)malloc(strlen(str)+1);
	strncpy(webos_strToolOption.eModelModuleType, str, sizeof(webos_strToolOption.eModelModuleType)-1);
	webos_strToolOption.eModelModuleType[sizeof(webos_strToolOption.eModelModuleType)-1] = '\0';

pr_info("[pyStrToolOpt]ModuleType:%s\n",webos_strToolOption.eModelModuleType);
    return 0 ;
}
early_param("eModelModuleType",early_webos_strToolOpt_maker_type);

static int __init early_webos_strToolOpt_local_dimming (char *str)
{
    const char * opt3_local_dimming="0"; // 0:off ,1:on

    if (strncmp(opt3_local_dimming,str,strlen(opt3_local_dimming))==0)
    {
           webos_strToolOption.bLocalDimming = str_local_dimming_off;
    }
    else 
    {
           webos_strToolOption.bLocalDimming = str_local_dimming_on;
    }
pr_info("[pyStrToolOpt]localDimming:%d\n",webos_strToolOption.bLocalDimming);
    return 0 ;
}
early_param("bLocalDimming",early_webos_strToolOpt_local_dimming);

unsigned int get_ic_version(void)
{
#define GET_IC_VERSION()	rtd_inl(STB_SC_VerID_reg)
#define VERSION_A_REG_CODE	0x66170000
#define VERSION_B_REG_CODE	0x66170000
#define VERSION_C_REG_CODE	0x66170010

	if (GET_IC_VERSION() == VERSION_A_REG_CODE)
	{
		return VERSION_A;
	}
	else if (GET_IC_VERSION() == VERSION_B_REG_CODE)
	{
		return VERSION_B;
	}
	else
	{
		return VERSION_C;
	}
}

bcas_info_t bcas;
static int __init early_parse_bcas(char *str)
{
    const char *strON="on";
    const char *strOFF="off";
    if (strncmp(strON,str,strlen(strON))==0)
    {
        bcas = bcasON;
        pr_debug("bcas on\n");
    }
    else if (strncmp(strOFF,str,strlen(strOFF))==0)
    {
        bcas = bcasOFF;
        pr_debug("bcas off\n");
    }
    else
    {
        bcas = bcasUNKNOWN;
        pr_err("bcas is unknown, error!!\n");
    }
    return 0;
}
early_param("bcas", early_parse_bcas);

#ifdef CONFIG_ZONE_ZRAM
unsigned long get_zone_zram_size(void)
{
	unsigned long zone_zram_addr = 0;

	return carvedout_buf_query(CARVEDOUT_ZONE_ZRAM, (void *)&zone_zram_addr);
}
EXPORT_SYMBOL(get_zone_zram_size);

unsigned long get_zone_zram_pfn(void)
{
	unsigned long zone_zram_addr = 0;

	if (carvedout_buf_query(CARVEDOUT_ZONE_ZRAM, (void *)&zone_zram_addr) > 0) {
		return zone_zram_addr >> PAGE_SHIFT;
	}

	return 0;
}
EXPORT_SYMBOL(get_zone_zram_pfn);

unsigned int get_zone_zram_boundary(unsigned int total)
{
    unsigned int boundary = total;
    unsigned int my_ret = boundary;
    bool swap_en = 0;

#ifdef CONFIG_RTK_KDRIVER_SUPPORT
    RTK_DC_INFO_t dc_info;
    RTK_DC_RET_t ret;

    ret = rtk_dc_get_dram_info(&dc_info);
    if (ret == RTK_DC_RET_FAIL) {
        pr_err("warning, no get boundary, qos enable?\n");
        swap_en = 0;
        boundary = total;
    } else {
        swap_en = (bool)dc_info.swap_en;
        boundary = (unsigned int)dc_info.boundary;
    }
#endif
#ifdef CONFIG_ZONE_ZRAM_HAS_SIZE
    /* zone_zram only apply for non-swap case */
    if ((swap_en == 0) && (total > boundary) && (boundary > 512*_MB_) && (get_platform() == PLATFORM_K6HP)) {
        my_ret = boundary;
    } else {
        my_ret = total;
    }
#else
    my_ret = total;
#endif

    return my_ret;
}
#endif

#ifdef CONFIG_ARM64
static const char *carvedout_mem_dts_name[CARVEDOUT_NUM] = {
	"boot",    // CARVEDOUT_BOOTCODE
	"demod",   // CARVEDOUT_DEMOD
	"dmem",    // CARVEDOUT_AV_DMEM
	"kboot",   // CARVEDOUT_K_OS_1
	"kcpu",    // CARVEDOUT_K_OS
	"rbus",    // CARVEDOUT_MAP_RBUS
	"avcpu",   // CARVEDOUT_AV_OS
	"rtkrpc",  // CARVEDOUT_MAP_RPC
	"vdec_ringbuffer", // CARVEDOUT_VDEC_RINGBUF
	"logbuf",  // CARVEDOUT_LOGBUF
	"rom",     // CARVEDOUT_ROMCODE
	"ib_boundary",     // CARVEDOUT_IB_BOUNDARY
	"gal",     // CARVEDOUT_GAL
	"snapshot",// CARVEDOUT_SNAPSHOT
	"scaler",  // CARVEDOUT_SCALER
	"scaler_band",  // CARVEDOUT_SCALER_BAND
	"memc",    // CARVEDOUT_SCALER_MEMC
	"mdomain", // CARVEDOUT_SCALER_MDOMAIN
	"di_nr",   // CARVEDOUT_SCALER_DI_NR
	"nn",      // CARVEDOUT_SCALER_NN
	"vip",     // CARVEDOUT_SCALER_VIP
	"od",      // CARVEDOUT_SCALER_OD
	"vbm",     // CARVEDOUT_VDEC_VBM
	"tp",      // CARVEDOUT_TP
	"vt",      // CARVEDOUT_VT
	"ddr_boundary",      // CARVEDOUT_DDR_BOUNDARY
#ifdef CONFIG_ZONE_ZRAM
	"zone_zram", //CARVEDOUT_ZONE_ZRAM 
#endif
	"cma_low_reserved",  //CARVEDOUT_CMA_LOW
	"cma_high_reserved", //CARVEDOUT_CMA_HIGH
};
#endif

unsigned long carvedout_buf_query(carvedout_buf_t what, void **addr)
{
	unsigned long size;
	void *address = NULL;
    unsigned int addrIdx = (get_platform() - PLATFORM_KXLP);
	unsigned int platIdx = (get_platform() - PLATFORM_KXLP) + 2; // 0: KxLP, 1: KxL. (2 is array index offset)

#define SVP_PROT_ALIGN 6*1024

	/* this switch case is list by address order */
	switch (what)
	{
	case CARVEDOUT_DEMOD :
		if (intDemod == 0)
			size = 0;
		else {
#if 1//ndef CONFIG_ARM64
            address = (void *)carvedout_buf[what][addrIdx];
			size = carvedout_buf[what][platIdx];
#else
            size = fdt_get_carvedout_mem_info(carvedout_mem_dts_name[what], &address);
#endif
        }
		break;

    case CARVEDOUT_CMA_LOW :
    case CARVEDOUT_CMA_HIGH :
		address = (void *)carvedout_buf[what][addrIdx];
		size = carvedout_buf[what][platIdx];
        break;

    case CARVEDOUT_SCALER_MEMC:
    case CARVEDOUT_SCALER_MDOMAIN:
    case CARVEDOUT_SCALER_OD:
    case CARVEDOUT_SCALER_VIP:
    case CARVEDOUT_SCALER_DI_NR:
    case CARVEDOUT_TP:
		address = (void *)carvedout_buf[what][addrIdx];
		size = carvedout_buf[what][platIdx];

        /* 200K from vbm cmphdr space */
        if (what == CARVEDOUT_SCALER_OD)
            size += 200*1024;

        if (size > 0) {
            size = ALIGNED_DOWN(((unsigned long)address + size), SVP_PROT_ALIGN) - ALIGNED_UP((unsigned long)address, SVP_PROT_ALIGN);
            address = (void *)ALIGNED_UP((unsigned long)address, SVP_PROT_ALIGN);
        }
        break;

    case CARVEDOUT_VDEC_VBM :
        if (get_platform() == PLATFORM_K6HP) {
            address = (void *)(KXHP_VDEC_BUFFER_START_QUERY + 200*1024); // 200K for OD
            size = KXHP_VDEC_BUFFER_END - KXHP_VDEC_BUFFER_START_QUERY - 200*1024;
        } else if (get_platform() == PLATFORM_K6LP) {
            address = (void *)(KXLP_VDEC_BUFFER_START_QUERY + 200*1024); // 200K for OD
            size = KXLP_VDEC_BUFFER_END - KXLP_VDEC_BUFFER_START_QUERY - 200*1024;
        } else {
            address = (void *)carvedout_buf[what][addrIdx];
            size = carvedout_buf[what][platIdx];
        }

		if (size > 0) {
			address = (void *)dvr_size_alignment_ex((unsigned long)address, SVP_PROT_ALIGN);
			size = dvr_size_alignment_ex(size, SVP_PROT_ALIGN) - SVP_PROT_ALIGN;
		}

        break;

#ifdef CONFIG_ZONE_ZRAM
	case CARVEDOUT_ZONE_ZRAM:
    {
        unsigned int total = get_memory_size(GFP_DCU1) + get_memory_size(GFP_DCU2);
        unsigned int boundary = total;
        boundary = get_zone_zram_boundary(total);

        if (total == boundary) {
            address = (void *)total;
            size = 0;
            pr_debug("[MEM] No zone ZRAM\n");
        } else {
            address = (void *)boundary;
            size = total - boundary;
            pr_debug("[MEM] Set 0x%lx to 0x%lx as zone ZRAM\n", (unsigned long)address, (unsigned long)address + size);
        }
    }
        break;
#endif

	default:
#if 1//ndef CONFIG_ARM64
		address = (void *)carvedout_buf[what][addrIdx];
		size = carvedout_buf[what][platIdx];
#else
        size = fdt_get_carvedout_mem_info(carvedout_mem_dts_name[what], &address);
#endif

        if (what == CARVEDOUT_VT) {
            unsigned int total = get_memory_size(GFP_DCU1) + get_memory_size(GFP_DCU2);
            unsigned int boundary = total;
#ifdef CONFIG_ZONE_ZRAM
            boundary = get_zone_zram_boundary(total);
#endif
            address = (void *)(boundary - size);
        }

		break;

//  implement your own specific case 
//	case :
//		break;
	}

	if (addr)
		*addr = address;

	return size;
}
EXPORT_SYMBOL(carvedout_buf_query);

/* 
 * return negative value for invalid query
 */
int carvedout_buf_query_is_in_range(unsigned long in_addr, void **start, void **end)
{
    carvedout_buf_t idx;
    unsigned long size, addr;

    for (idx = 0; idx < CARVEDOUT_CMA_LOW; idx++) { // except cma area
        size = carvedout_buf_query(idx, (void *)&addr);
        if (((idx == CARVEDOUT_BOOTCODE) && size) || (addr && size)) { // bootcode addr start from 0x0
            if ((in_addr >= addr) && (in_addr < (addr + size))) {
                if (start && end) {
                    *start = (void *)addr;
                    *end   = (void *)(addr + size);
                }
                pr_debug("carvedout_buf query(%lx) in idx(%d), range(%lx-%lx)\n", in_addr, idx, addr, addr + size);
                return (int)idx;
            }
        }
    }

    return -1;
}
EXPORT_SYMBOL(carvedout_buf_query_is_in_range);
