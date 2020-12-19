#include <linux/uaccess.h>
#include <linux/bitops.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/io.h>
#include <mach/io.h>
#include <linux/irq.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/input.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/kfifo.h>
#include <linux/kthread.h>
#include <linux/of.h>
#include <linux/pci.h>
#include <linux/poll.h>
#include <linux/platform_device.h>
#include <linux/random.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/signal.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <mach/pcbMgr.h>

#include <linux/kernel.h>
#include <rtk_kdriver/rtk_spdif.h>

int spdif_dev_major;
int spdif_dev_minor;

#define SPDIF_GET_GPIO_ID(x)          ((unsigned int)(x & 0xFF))
#define SPDIF_GET_GPIO_GROUP(x)        ((unsigned int)((x >> 8) & 0xF))
#define SPDIF_GET_PAD_GPIO_VAL(x)     ((unsigned int)((x >> 48) & 0xF))
#define SPDIF_GET_PAD_SPDIF_VAL(x)    ((unsigned int)((x >> 52) & 0xF))
#define SPDIF_GET_PINMUX_REG(x)       ((unsigned int)((x >> 16) & 0xFFFFFFFF))
#define SPDIF_GET_PINMUX_MASK_START_BIT(x)      ((unsigned int)((x >> 56) & 0xFF))


static RTK_SPDIF_Info g_spdif_info;
static SPDIF_STATUS g_spdif_status = SPDIF_ENABLE;
extern int m_ioctl_enable;
static struct class *_gstspdif_class;
static struct device *spdif_device_st;
static int  g_spdif_parse_cfg = 0;

/*------------------------------------------------
 * Func  : rtk_spdif_parse_cfg
 *
 * Desc  : parse spdif bootcode config information
 *
 * Param : null
 *
 * Retn  : null
 *-----------------------------------------------*/
static void rtk_spdif_parse_cfg(void)
{
    int i;
    unsigned int pin_group,pin_index;
    long long pcb_enum_value;

    if(g_spdif_parse_cfg == 0){
        for(i = 0; i < PCB_ENUM_MAX; i++){
            if(memcmp(pcb_enum_all[i].name, "SPDIF_CFG", 9) == 0 && pcb_enum_all[i].value != 0)
            {
                pcb_enum_value = pcb_enum_all[i].value;
                pin_group = SPDIF_GET_GPIO_GROUP(pcb_enum_value);
                pin_index = SPDIF_GET_GPIO_ID(pcb_enum_value);

                g_spdif_info.gid = rtk_gpio_id(pin_group,pin_index);
                g_spdif_info.pad_mux_reg = SPDIF_GET_PINMUX_REG(pcb_enum_value);
                g_spdif_info.pad_mux_mask_start_bit =SPDIF_GET_PINMUX_MASK_START_BIT(pcb_enum_value);
                g_spdif_info.pad_mux_val_spdif = SPDIF_GET_PAD_SPDIF_VAL(pcb_enum_value);;
                g_spdif_info.pad_mux_val_gpio =  SPDIF_GET_PAD_GPIO_VAL(pcb_enum_value);
                break;
            }
        }
        g_spdif_parse_cfg = 1;
    }
}


/*------------------------------------------------
 * Func  : rtk_spdif_enable
 *
 * Desc  : enable / disable SPDIF output
 *
 * Param : enable : enable/disable SPDIF out
 *            0 : disable
 *            others : enable
 *
 * Retn  : 0 : successed, others : failed
 *-----------------------------------------------*/
int rtk_spdif_enable(unsigned char en)
{
    RTK_SPDIF_Info p_this = g_spdif_info;
    unsigned int val;
    unsigned int pinmux_mask;

    rtk_spdif_parse_cfg();
    pinmux_mask = 0xf << p_this.pad_mux_mask_start_bit;

    if (g_spdif_parse_cfg ==0) 
    {
        return -1;
    }
    // set pinmux 
    val = rtd_inl(p_this.pad_mux_reg) & ~(pinmux_mask);
    if (en)
    {
        val |= ((p_this.pad_mux_val_spdif) << (p_this.pad_mux_mask_start_bit));            // zap to spdif
        g_spdif_status = SPDIF_ENABLE;

    }else{                        
        rtk_gpio_output(p_this.gid,  0);    // set gpio to output low
        rtk_gpio_set_dir(p_this.gid, 1);    // set gpio to output state
        val |= ((p_this.pad_mux_val_gpio) << (p_this.pad_mux_mask_start_bit));             // zap to gpio
        g_spdif_status = SPDIF_DISABLE;

    }

    rtd_outl(p_this.pad_mux_reg, val);
    return 0;
}


ssize_t rtk_spdif_cls_show_param(struct class *class, 
                     struct class_attribute *attr,char *buf)
{
    int ret = 0;
    if (strncmp(attr->attr.name, "enable", strlen("enable")) == 0) {
        ret = sprintf(buf, "Describle: Enable or diasble SPDIF out\n");
        ret += sprintf(buf + ret, "Usage: echo 1 > enbale \n");
        ret += sprintf(buf + ret, "Current status: spdif out %s\n",\
            (g_spdif_status)?"disable":"enable");
    }else if (strncmp(attr->attr.name, "status", strlen("status")) == 0) {
        ret = sprintf(buf, "Current status: spdif out %s\n",\
            (g_spdif_status)?"disable":"enable");
    }else if (strncmp(attr->attr.name, "dump", strlen("dump")) == 0) {
        ret = sprintf(buf, "Describle: dump spdif cfg info\n");
        ret += sprintf(buf + ret, "GID      : 0x%x        //gpio num:%d  gpio type:%s\n",\
                    g_spdif_info.gid,gpio_idx(g_spdif_info.gid),\
                    (gpio_group(g_spdif_info.gid) == MIS_GPIO)?"MIS_GPIO":"ISO_GPIO");
        ret += sprintf(buf + ret, "pad_mux_reg            :0x%8x\n ",g_spdif_info.pad_mux_reg);
        ret += sprintf(buf + ret, "pad_mux_mask_start_bit :0x%x\n ",g_spdif_info.pad_mux_mask_start_bit);
        ret += sprintf(buf + ret, "pad_mux_val_spdif      :0x%x\n ",g_spdif_info.pad_mux_val_spdif);
        ret += sprintf(buf + ret, "pad_mux_val_gpio       :0x%x\n ",g_spdif_info.pad_mux_val_gpio);
        ret += sprintf(buf + ret, "Current status: spdif out %s\n",\
            (g_spdif_status)?"disable":"enable");
    }else if(strncmp(attr->attr.name, "gpio", strlen("gpio")) == 0){
        ret = sprintf(buf, "Describle: For spdif pad gpio debug,test gpio set output.\n");
        ret += sprintf(buf + ret, "Current status: spdif out %s\n",\
            (g_spdif_status)?"disable":"enable");
        if(SPDIF_DISABLE == g_spdif_status){
            ret += sprintf(buf + ret, "GPIO output:%s\n ",\
                (0 != rtk_gpio_output_get(g_spdif_info.gid))?"HIGH":"LOW");
        }else{
            ret += sprintf(buf + ret, "Not change pad pinmux to GPIO.\n ");
        }
    }
    return ret;
}

 ssize_t rtk_spdif_cls_set_param(struct class *class, struct class_attribute *attr,
                    const char *buf, size_t count)
{
     int val = 0;
     sscanf(buf, "%d\n", &val);

    if (strncmp(attr->attr.name, "enable", strlen("enable")) == 0) {
        rtk_spdif_enable(val);
    }else if(strncmp(attr->attr.name, "gpio", strlen("gpio")) == 0){
        if(SPDIF_DISABLE == g_spdif_status){
            rtk_gpio_output(g_spdif_info.gid,val);
        }else{
            SPDIF_WARN("spdif out enable cannot set gpio output\n");
        }
    }

    return count;
}

#define enable_show rtk_spdif_cls_show_param
#define enable_store rtk_spdif_cls_set_param
#define status_show rtk_spdif_cls_show_param
#define status_store rtk_spdif_cls_set_param
#define dump_show rtk_spdif_cls_show_param
#define dump_store rtk_spdif_cls_set_param
#define gpio_show rtk_spdif_cls_show_param
#define gpio_store rtk_spdif_cls_set_param

CLASS_ATTR_RW(enable);
CLASS_ATTR_RW(status);
CLASS_ATTR_RW(dump);
CLASS_ATTR_RW(gpio);

int  rtk_spdif_create_class_attr(struct class *cls)
{
    int ret;
    ret = class_create_file(cls, &class_attr_enable);
    ret = class_create_file(cls, &class_attr_status);
    ret = class_create_file(cls, &class_attr_dump);
    ret = class_create_file(cls, &class_attr_gpio);
    return ret;
}

void  rtk_spdif_remove_class_attr(struct class *cls)
{
    class_remove_file(cls, &class_attr_enable);
    class_remove_file(cls, &class_attr_status);
    class_remove_file(cls, &class_attr_dump);
    class_remove_file(cls, &class_attr_gpio);
}

int __init rtk_spdif_dev_init(void)
{
    int ret = 0;
    spdif_dev_major = 0;
    spdif_dev_minor = 0;

    _gstspdif_class = class_create(THIS_MODULE, "rtk_spdif");
    if (IS_ERR(_gstspdif_class)) {
        SPDIF_WARN("SPDIF: can not create class...\n");
        goto FAIL_TO_CREATE_SPDIF_DEVICE;
    }

    spdif_device_st = device_create(_gstspdif_class, NULL, MKDEV(spdif_dev_major, spdif_dev_minor), NULL, "rtk_spdif");

    ret = rtk_spdif_create_class_attr(_gstspdif_class);
    if(spdif_device_st == NULL) {
        SPDIF_WARN(" device create spdif dev failed\n");
        goto FAIL_TO_CREATE_SPDIF_DEVICE;
    }
    SPDIF_WARN("register chrdev(%d,%d) success.\n", spdif_dev_major, spdif_dev_minor);
    rtk_spdif_parse_cfg();
    return 0;

FAIL_TO_CREATE_SPDIF_DEVICE:
    rtk_spdif_remove_class_attr(_gstspdif_class);
    class_destroy(_gstspdif_class);
    device_destroy(_gstspdif_class, MKDEV(0, 0));
    return -1;

}

static void __exit rtk_spdif_dev_uninit(void)
{
    rtk_spdif_remove_class_attr(_gstspdif_class);
    device_destroy(_gstspdif_class, MKDEV(spdif_dev_major, spdif_dev_minor));
    class_destroy(_gstspdif_class);
}

module_init(rtk_spdif_dev_init);
module_exit(rtk_spdif_dev_uninit);
MODULE_AUTHOR("Banghui_yin, Realtek Semiconductor");
MODULE_LICENSE("GPL");


