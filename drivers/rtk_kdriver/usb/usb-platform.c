#include <linux/clk.h>
#include <linux/module.h>
#include <asm/delay.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <rtk_kdriver/usb/usb3_crt.h>
#include <rtk_kdriver/usb/usb2_crt.h>
#include <rbus/usb3_top_reg.h> /* for U3 wrapper register define */
#include <rbus/usb2_top_reg.h>
#include <mach/pcbMgr.h>
#include <mach/platform.h>
#include <mach/pcbMgr.h>
#include <rbus/sb2_reg.h>

#define DRIVER_DESC "RTK USB generic platform driver"

#define LED_PANEL       (0)
#define OLED_PANEL      (1)
#define KxLP_MODEL      (0)
#define KxHP_MODEL      (1)
#define UNKNOW_PARAM    (0xff)
unsigned int info_panel     = UNKNOW_PARAM;
unsigned int info_platform  = UNKNOW_PARAM;

static void power_on_all_usb_host(void)
{
    usb3_crt_on();
    usb2_crt_on();
}


static int rtk_usb_platform_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct device_node *node = pdev->dev.of_node;
    int error = 0;

    power_on_all_usb_host();

    error = of_platform_populate(node, NULL, NULL, dev); /* populate all children nodes of this device */
    if (error)
        dev_err(&pdev->dev, "failed to create rtk usb platform\n");

#if 0
    if (sysfs_create_group(&dev->kobj, &dev_attr_grp)) {
        pr_warn("Create self-defined sysfs attributes fail \n");
    }
#endif

    return error;
}


static int rtk_usb_platform_remove(struct platform_device *pdev)
{
#if 0
    sysfs_remove_group(&pdev->dev.kobj, &dev_attr_grp);
#endif
    of_platform_depopulate(&pdev->dev);
    platform_set_drvdata(pdev, NULL);

    return 0;
}


static int rtk_usb_platform_resume(struct device *dev)
{
    power_on_all_usb_host();

    return 0;
}


static int rtk_usb_platform_restore(struct device *dev)
{
    power_on_all_usb_host();

    return 0;
}


static const struct dev_pm_ops rtk_usb_platform_pm_ops = {
    .resume = rtk_usb_platform_resume,
    .restore = rtk_usb_platform_restore,
};


static const struct of_device_id rtk_usb_of_match[] = {
    { .compatible = "rtk,usb-platform", },
    {},
};
MODULE_DEVICE_TABLE(of, rtk_usb_of_match);


static struct platform_driver rtk_usb_platform_driver = {
    .probe      = rtk_usb_platform_probe,
    .remove     = rtk_usb_platform_remove,
    .driver     = {
        .name = "usb-platform",
        .pm = &rtk_usb_platform_pm_ops,
        .of_match_table = rtk_usb_of_match,
    }
};


static int __init rtk_usb_platform_init(void)
{
    return platform_driver_register(&rtk_usb_platform_driver);
}
module_init(rtk_usb_platform_init);


static void __exit rtk_usb_platform_cleanup(void)
{
    platform_driver_unregister(&rtk_usb_platform_driver);
}
module_exit(rtk_usb_platform_cleanup);


MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR("Jason Chiu");
MODULE_LICENSE("GPL");

/************************************************************************
 *  Early Parameter
 ************************************************************************/

/* ================================================================
reference bootcode "./customer/src/CusCmnio.c" 670
gModelOpt.country_type,             // 1. HW_OPT_COUNTRY
gModelOpt.pmic_type,                // 2. HW_OPT_PMIC_TYPE
gModelOpt.panel_interface,          // 3. HW_OPT_PANEL_INTERFACE
gModelOpt.panel_resolution,         // 4. HW_OPT_PANEL_RESOLUTION
gModelOpt.bSupport_frc,             // 5. HW_OPT_FRC
gModelOpt.reserved,                 // 6. reserved
gModelOpt.reserved,                 // 7. reserved
gModelOpt.reserved,                 // 8. reserved
gModelOpt.tuner_option,             // 9. HW_OPT_TUNER
gModelOpt.reserved,                 // 10. reserved
gModelOpt.isSupportEWBS,            // 11. isSupportEWBS
gModelOpt.ddr_size,                 // 12. ddr size
gModelOpt.bSupportOptic,            // 13. HW_OPT_OPTIC
gModelOpt.reserved,                 // 14. reserved
gModelOpt.graphic_resolution,       // 15. HW_OPT_GRAPHIC_RESOLUTION
gModelOpt.reserved,                 // 16. reserved
gModelOpt.reserved,                 // 17. reserved
gModelOpt.reserved                  // 18. reserved
 ================================================================ */
static int __init rtk_usb_get_panel_info(char *str)
{
    int i;
    char tmp_c[4] = {0};
    unsigned char HWsption[18] = {0xff};
    unsigned long tmp_long;

    pr_debug("%s(%d)panel info=%s\n", __func__,__LINE__,str);

    for(i=0; i<18; i++){
        strncpy(tmp_c,str+i,1);
        if(!kstrtoul(tmp_c, 16, &tmp_long)){
            HWsption[i] = (unsigned char)tmp_long;
        }
        pr_debug("%s(%d)HWsption_int[%d]=0x%x\n", __func__,__LINE__,
                (i+1),HWsption[i]);
    }

    // 13. HW_OPT_OPTIC for check OLED
    if(HWsption[12] == 1){
        info_panel = OLED_PANEL;
        pr_info("OLED panel\n");
    }else{
        info_panel = LED_PANEL;
        pr_info("LED panel\n");
    }

    return 0;
}
early_param("hwopt", rtk_usb_get_panel_info);

static int __init rtk_usb_get_platform_info (char *str)
{
    const char *str_kxlp="K6LP";
    const char *str_kxhp="K6HP";

    pr_debug("%s(%d)platform_info=%s\n", __func__,__LINE__,str);
    if (strncmp(str_kxlp,str,strlen(str_kxlp))==0)
    {
        info_platform=KxLP_MODEL;
        pr_info("K6LP platform\n");
    }
    else if (strncmp(str_kxhp,str,strlen(str_kxhp))==0)
    {
        info_platform=KxHP_MODEL;
        pr_info("K6HP platform\n");
    }
    else
    {
        pr_alert("Wrong platform  ? \e[1;31m%s\e[0m\n",str);
    }

    return 0;
}
early_param("chip", rtk_usb_get_platform_info);

