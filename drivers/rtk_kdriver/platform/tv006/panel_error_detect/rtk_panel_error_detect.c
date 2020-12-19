#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <rtk_kdriver/rtk_gpio.h>
#include <rtk_kdriver/pcbMgr.h>

RTK_GPIO_ID panel_error_detect_pin;

static int count = 0;

void rtk_panel_error_detect_isr(
    RTK_GPIO_ID      gid,
    unsigned char    assert,
    void*            dev_id
){
    count++;
    rtd_outl(0xb806015c, count);
}

static int __init rtk_panel_error_detect_init(void)
{
    int ret = 0, index = 0, type = 0;
    static unsigned long long value = 0;
    ret = pcb_mgr_get_enum_info_byname("PANEL_ERROR_DETECT", &value);
    if (ret) {
        return -EINVAL;
    }

    if(GET_PIN_TYPE(value) == PCB_PIN_TYPE_GPIO)
        type = MIS_GPIO;
    else if(GET_PIN_TYPE(value) == PCB_PIN_TYPE_ISO_GPIO)
        type = ISO_GPIO;

    index = GET_PIN_INDEX(value);
    panel_error_detect_pin = rtk_gpio_id(type, index);
    ret = rtk_gpio_isr_register(panel_error_detect_pin, rtk_panel_error_detect_isr, "rtk_pwm_panel_error_detect_isr", RTK_GPIO_DEBOUNCE_1us, 1);
    if(ret < 0) {
        RTK_GPIO_WARNING("panel_error_detect_pin isr register %s GPIO %d (%x) failed\n",
            gpio_type(gpio_group(panel_error_detect_pin)),
            gpio_idx(panel_error_detect_pin),
            panel_error_detect_pin);
    }
    else {
        RTK_GPIO_WARNING("panel_error_detect_pin isr register %s GPIO %d (%x) ready\n",
            gpio_type(gpio_group(panel_error_detect_pin)),
            gpio_idx(panel_error_detect_pin),
            panel_error_detect_pin);
    }
    return 0;
}

static void __exit rtk_panel_error_detect_exit(void)
{
    rtk_gpio_set_irq_enable(panel_error_detect_pin, 0);
    rtk_gpio_free_irq(panel_error_detect_pin,0 );
}

module_init(rtk_panel_error_detect_init);
module_exit(rtk_panel_error_detect_exit);
