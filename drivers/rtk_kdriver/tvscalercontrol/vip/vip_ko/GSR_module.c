#if 1
#include <generated/autoconf.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <asm/barrier.h>
#include <asm/cacheflush.h>
#include <mach/irqs.h>
#include <mach/platform.h>
#include <mach/system.h>
#include <mach/timex.h>

#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/random.h>
#ifdef CONFIG_CMA_RTK_ALLOCATOR
#include <linux/pageremap.h>
#endif
#include <linux/kthread.h>  /* for threads */
#include <linux/time.h>   /* for using jiffies */
#include <linux/hrtimer.h>

#include <linux/proc_fs.h>

#if 0
#include <rbus/timer_reg.h>
#include <rbus/dmato3dtable_reg.h>
#include <rbus/vgip_reg.h>
#include <rbus/sb2_reg.h>
#include <scaler/scalerCommon.h>
#include <tvscalercontrol/scalerdrv/scalermemory.h>
#include <rtk_kdriver/tvscalercontrol/vip/scalerColor.h>
#endif
#endif
struct VIP_GSR_interface {
	unsigned short (*get_LG_GSR_GetAdaptiveRgbGain)(unsigned int * pstParams, unsigned short pApl[34][60], unsigned short maxGain);
};

extern void fwif_color_set_VIP_GSR_adapter(struct VIP_GSR_interface *pVIP_GSR_adapter);
extern unsigned short LG_GSR_GetAdaptiveRgbGain(unsigned int * pstParams, unsigned short pApl[34][60], unsigned short maxGain);



static struct VIP_GSR_interface vip_gsr_adapter = {
	.get_LG_GSR_GetAdaptiveRgbGain = LG_GSR_GetAdaptiveRgbGain,
};

static int VIP_GSR_Module_init(void)
{
	fwif_color_set_VIP_GSR_adapter(&vip_gsr_adapter);
	return 0;
}
static void VIP_GSR_Module_cleanup_module(void)
{
	return;
}

module_init(VIP_GSR_Module_init);
module_exit(VIP_GSR_Module_cleanup_module);
//MODULE_LICENSE("Proprietary");

