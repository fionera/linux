#include <linux/syscalls.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <mach/io.h>
#include "scaler_vfedev_v4l2-hdmi.h"

#define HDMI_V4L2_DRV_VERSION    "2019.10.24.V1.1"

//===========================================================================
// Func : hdmi_attr_v4l2_show_support_function
//===========================================================================
static ssize_t hdmi_attr_v4l2_show_support_function(
    struct device*              dev,
    struct device_attribute*    attr,
    char*                       buf
    )
{
    int  n;
    int  count = PAGE_SIZE;
    char *ptr = buf;
    unsigned int i = 0;
    HDMI_V4L2_DEFINE_T* p_v4l2_hdmi_func_table = newbase_hdmi_v4l2_function_table();

    n = scnprintf(ptr, count, "---------------- HDMI V4L2 SUPPORT FUNCTION (VER: %s) --------------\n", HDMI_V4L2_DRV_VERSION);
    ptr+=n; count-=n;

    n = scnprintf(ptr, count, "id  record exe_en call_cnt name\n");
    ptr+=n; count-=n;

    for(i = 0; i<g_v4l2_define_table_size; i++)
    {
        n = scnprintf(ptr, count, "[%2d]  %1d       %1d %8d    %s\n", i, p_v4l2_hdmi_func_table[i].history_record, p_v4l2_hdmi_func_table[i].execute_enable, p_v4l2_hdmi_func_table[i].total_call_cnt, p_v4l2_hdmi_func_table[i].entry_name);
        ptr+=n; count-=n;
    }
    n = scnprintf(ptr, count, "------------Other Command----------\n");
    ptr+=n; count-=n;
    n = scnprintf(ptr, count, "RECORD_ALL=x, x=0~1, force all function record is TRUE or FASLE\n");
    ptr+=n; count-=n;
    n = scnprintf(ptr, count, "RECORD_x=y, x=0~%d table index, y=0~1 TRUE or FALSE\n", (g_v4l2_define_table_size-1));
    ptr+=n; count-=n;
    n = scnprintf(ptr, count, "EXEEN_x=y, x=0~%d table index, y=0~1 TRUE or FALSE\n", (g_v4l2_define_table_size-1));
    ptr+=n; count-=n;

    return ptr - buf;
}

//===========================================================================
// Func : hdmi_attr_v4l2_store_support_function
//===========================================================================
static ssize_t hdmi_attr_v4l2_store_support_function(struct device *dev,
                   struct device_attribute *attr,
                   const char *buf, size_t count)
{
    int val;
    int index;

    if (strcmp(attr->attr.name, "support_func")==0)
    {
        if (sscanf(buf, "RECORD_ALL=%1d", &val)==1)
        {
            HDMI_V4L2_DEFINE_T* p_v4l2_hdmi_func_table = newbase_hdmi_v4l2_function_table();

            if (val>0)
            {
                unsigned int i = 0;
                for(i = 0; i<g_v4l2_define_table_size; i++)
                {
                    p_v4l2_hdmi_func_table[i].history_record = 1;
                }
            }
            else
            {
                unsigned int i = 0;
                for(i = 0; i<g_v4l2_define_table_size; i++)
                {
                    p_v4l2_hdmi_func_table[i].history_record = 0;
                }

            }
        }
        else if (sscanf(buf, "RECORD_%3d=%1d", &index, &val)==2)
        {
            HDMI_V4L2_DEFINE_T* p_v4l2_hdmi_func_table = newbase_hdmi_v4l2_function_table();

            if ((index>=0) && (index< g_v4l2_define_table_size))
            {
                p_v4l2_hdmi_func_table[index].history_record = (val) ? 1: 0;
            }
        }
        else if (sscanf(buf, "EXEEN_%3d=%1d", &index, &val)==2)
        {
            HDMI_V4L2_DEFINE_T* p_v4l2_hdmi_func_table = newbase_hdmi_v4l2_function_table();

            if ((index>=0) && (index< g_v4l2_define_table_size))
            {
                p_v4l2_hdmi_func_table[index].execute_enable = (val) ? 1: 0;
            }
        }
        else
        {
            return -1;
        }
    }
    else
    {
        return -1;
    }

    return count;
}

static DEVICE_ATTR(support_func, 0644, hdmi_attr_v4l2_show_support_function, hdmi_attr_v4l2_store_support_function);

//===========================================================================
// Func : hdmi_attr_v4l2_show_call_history
//===========================================================================
static ssize_t hdmi_attr_v4l2_show_call_history(
    struct device*              dev,
    struct device_attribute*    attr,
    char*                       buf
    )
{
    int  n;
    int  count = PAGE_SIZE;
    char *ptr = buf;
    unsigned int i = 0;
    HDMI_V4L2_HISTORY_T* p_v4l2_history = newbase_hdmi_v4l2_get_history_info();
    HDMI_V4L2_DEFINE_T* p_v4l2_hdmi_func_table = newbase_hdmi_v4l2_function_table();

    n = scnprintf(ptr, count, "---------------- HDMI V4L2 CALL HISTORY (Max: %d) --------------\n", HDMI_V4L2_HISTORY_QUEUE_SIZE);
    ptr+=n; count-=n;

    n = scnprintf(ptr, count, "id    time(ms) call_no   UI_CH  name\n");
    ptr+=n; count-=n;

    for(i = 0; i<HDMI_V4L2_HISTORY_QUEUE_SIZE; i++)
    {
        V4L2_HDMI_TYPE type = p_v4l2_history[i].type;
        unsigned int sub_id = p_v4l2_history[i].sub_id;
        unsigned int target_ui_ch  = p_v4l2_history[i].target_ui_ch;
        unsigned int call_index  = p_v4l2_history[i].current_call_cnt;
        unsigned int call_time_ms  = p_v4l2_history[i].last_call_ms;
        int target_index = newbase_hdmi_v4l2_function_table_index(type, sub_id);

        if(target_index <0)
        {
            continue;
        }

        n = scnprintf(ptr, count, "[%2d] %8d   %5d    %5d  %s\n", i, call_time_ms , call_index, target_ui_ch, p_v4l2_hdmi_func_table[target_index].entry_name);
        ptr+=n; count-=n;
    }

    return ptr - buf;
}


static DEVICE_ATTR(call_history, 0444, hdmi_attr_v4l2_show_call_history, NULL);

//////////////////////////////////////////////////////////////////////////////
// File Operations
//////////////////////////////////////////////////////////////////////////////

int hdmi_sysfs_v4l2_open(struct inode *inode, struct file *file)
{
    return 0;
}

int hdmi_sysfs_v4l2_release(struct inode *inode, struct file *file)
{
    return 0;
}



//////////////////////////////////////////////////////////////////////////////
// Module Init / Exit
//////////////////////////////////////////////////////////////////////////////

static struct file_operations hdmi_sysfs_v4l2_fops =
{
    .owner      = THIS_MODULE,
    .open       = hdmi_sysfs_v4l2_open,
    .release    = hdmi_sysfs_v4l2_release,
};

static struct miscdevice hdmi_sysfs_v4l2_miscdev =
{
    MISC_DYNAMIC_MINOR,
    "rtk_v4l2_hdmi",
    &hdmi_sysfs_v4l2_fops
};

int __init hdmi_sysfs_v4l2_module_init(void)
{
    if (misc_register(&hdmi_sysfs_v4l2_miscdev))
    {
        pr_warning("hdmi_sysfs_v4l2_module_init failed - register misc device failed\n");
        return -ENODEV;
    }

    device_create_file(hdmi_sysfs_v4l2_miscdev.this_device, &dev_attr_support_func);
    device_create_file(hdmi_sysfs_v4l2_miscdev.this_device, &dev_attr_call_history);

    return 0;
}

static void __exit hdmi_sysfs_v4l2_module_exit(void)
{
    device_remove_file(hdmi_sysfs_v4l2_miscdev.this_device, &dev_attr_support_func);
    device_remove_file(hdmi_sysfs_v4l2_miscdev.this_device, &dev_attr_call_history);

    misc_deregister(&hdmi_sysfs_v4l2_miscdev);
}

module_init(hdmi_sysfs_v4l2_module_init);
module_exit(hdmi_sysfs_v4l2_module_exit);

