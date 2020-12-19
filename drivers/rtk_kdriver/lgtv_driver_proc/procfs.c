#include <linux/syscalls.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <mach/io.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <common/linux/linuxtv-ext-ver.h>
#include <lgtv_driver_proc/procfs.h>

static struct proc_dir_entry *lgtv_driver_proc_dir = NULL;
static struct proc_dir_entry *lgtv_driver_external_input_dir = NULL;
static struct proc_dir_entry *lgtv_driver_hdmi_status_dir = NULL;
static struct proc_dir_entry *lgtv_driver_adc_status_dir = NULL;
static struct proc_dir_entry *lgtv_driver_avd_status_dir = NULL;

//===============================================================
// LGE External Input Proc Fs
//===============================================================

static int _external_input_proc_read_ops(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    EXT_INPUT_PROC_ENTY* p_entry = (EXT_INPUT_PROC_ENTY*) PDE_DATA(file_inode(file));
    char *str = kmalloc(4096, GFP_KERNEL);
    int len, total = 0, remain = 4096;

    if (str == NULL) {
        pr_err ("[rtkaudio] malloc memory failed, %s:%d\n", __FUNCTION__, __LINE__);
        return 0;
    }

    len = sprintf(str+total, "version=linuxtv-ext-header ver.%d.%d.%d (submissions/%d)\n",
          LINUXTV_EXT_VER_MAJOR, LINUXTV_EXT_VER_MINOR, LINUXTV_EXT_VER_PATCH, LINUXTV_EXT_VER_SUBMISSION);
    total += len; remain -= len;

    if (p_entry->proc_read)
    {
        len = p_entry->proc_read(p_entry->private_data, str+total, remain);
        total += len; remain -= len;
    }

    if (copy_to_user(buf, str, total)) {
        pr_err("rtkaudio:%s:%d Copy proc data to user space failed\n", __FUNCTION__, __LINE__);
        kfree(str);
        return 0;
    }

    if (*ppos == 0)
        *ppos += total;
    else
        total = 0;

    kfree(str);
    return total;
}

EXT_INPUT_PROC_ENTY* create_lgtv_external_input_status_proc_entry(
    const char*                 name,
    int (*proc_read)(void* private_data, char* buff, unsigned int buff_size),
    void* data)
{
    EXT_INPUT_PROC_ENTY* p_entry = NULL;

    p_entry = kzalloc(sizeof(EXT_INPUT_PROC_ENTY), GFP_KERNEL);

    if (p_entry)
    {
        p_entry->proc_read = proc_read;
        p_entry->private_data = data;

        p_entry->f_ops.owner = THIS_MODULE;
        p_entry->f_ops.read  = _external_input_proc_read_ops;

        p_entry->pde = proc_create_data(name, 0444, lgtv_driver_external_input_dir, &p_entry->f_ops, (void*) p_entry);

        if (p_entry->pde==NULL)
        {
            pr_err("create external input proc entry - %s failed\n", name);
            kfree(p_entry);
            p_entry = NULL;
        }
    }

    return p_entry;
}

EXT_INPUT_PROC_ENTY* create_lgtv_hdmi_status_proc_entry(
    const int port,
    int (*proc_read)(void* private_data, char* buff, unsigned int buff_size),
    void* data)
{
    EXT_INPUT_PROC_ENTY* p_entry = NULL;

    p_entry = kzalloc(sizeof(EXT_INPUT_PROC_ENTY), GFP_KERNEL);

    if (p_entry)
    {
        unsigned char name[20];
        memset(name, 0, sizeof(name));

        p_entry->proc_read = proc_read;
        p_entry->private_data = data;

        p_entry->f_ops.owner = THIS_MODULE;
        p_entry->f_ops.read  = _external_input_proc_read_ops;

        snprintf(name, sizeof(name), "hdmi.p%d", port);

        p_entry->pde = proc_create_data(name, 0444, lgtv_driver_hdmi_status_dir, &p_entry->f_ops, (void*) p_entry);

        if (p_entry->pde==NULL)
        {
            pr_err("create hdmi status proc entry - %s failed\n", name);
            kfree(p_entry);
            p_entry = NULL;
        }
    }

    return p_entry;
}

EXT_INPUT_PROC_ENTY* create_lgtv_adc_status_proc_entry(
    const char*                 name,
    int (*proc_read)(void* private_data, char* buff, unsigned int buff_size),
    void* data)
{
    EXT_INPUT_PROC_ENTY* p_entry = NULL;

    p_entry = kzalloc(sizeof(EXT_INPUT_PROC_ENTY), GFP_KERNEL);

    if (p_entry)
    {
        p_entry->proc_read = proc_read;
        p_entry->private_data = data;

        p_entry->f_ops.owner = THIS_MODULE;
        p_entry->f_ops.read  = _external_input_proc_read_ops;

        p_entry->pde = proc_create_data(name, 0444, lgtv_driver_adc_status_dir, &p_entry->f_ops, (void*) p_entry);

        if (p_entry->pde==NULL)
        {
            pr_err("create adc status proc entry - %s failed\n", name);
            kfree(p_entry);
            p_entry = NULL;
        }
    }

    return p_entry;
}

EXT_INPUT_PROC_ENTY* create_lgtv_avd_status_proc_entry(
    const char*                 name,
    int (*proc_read)(void* private_data, char* buff, unsigned int buff_size),
    void* data)
{
    EXT_INPUT_PROC_ENTY* p_entry = NULL;

    p_entry = kzalloc(sizeof(EXT_INPUT_PROC_ENTY), GFP_KERNEL);

    if (p_entry)
    {
        p_entry->proc_read = proc_read;
        p_entry->private_data = data;

        p_entry->f_ops.owner = THIS_MODULE;
        p_entry->f_ops.read  = _external_input_proc_read_ops;

        p_entry->pde = proc_create_data(name, 0444, lgtv_driver_avd_status_dir, &p_entry->f_ops, (void*) p_entry);

        if (p_entry->pde==NULL)
        {
            pr_err("create avd status proc entry - %s failed\n", name);
            kfree(p_entry);
            p_entry = NULL;
        }
    }

    return p_entry;
}

//===============================================================
// LGE External Input Proc Fs Module Init / Exit
//===============================================================

static int lgtv_external_input_status_proc_init(void)
{
    /*
     * proc_mkdir() behavior:
     * If folder already exist, proc_mkdir() will return NLLL.
     * So, if we need to create a sub-dir under the exist path,
     * we need to use [exist-parent-dir]/[sub-dir] path when
     * calling proc_mkdir()
     *
     * EX: if "proc/lgtv-driver" path exists, and our target is
     * "proc/lgtv-driver/external-input", should pass "lgtv-driver/external-input"
     * to proc_mkdir()
     */
    // create "lgtv-driver"
    if (lgtv_driver_proc_dir == NULL)
    {
        lgtv_driver_proc_dir = proc_mkdir(LGTV_DRIVER_PROC_DIR, NULL);

        if (lgtv_driver_proc_dir == NULL)
        {
            pr_info("create lgtv proc entry (%s) exist\n",
                LGTV_DRIVER_PROC_DIR);
            // if proc_mkdir() return NULL, usually means the folder already exist.
        }
    }

    // create "lgtv-driver/external-input"
    if (lgtv_driver_external_input_dir == NULL)
    {
        const char* ext_input_dir = (lgtv_driver_proc_dir == NULL)? LGTV_EXTERNAL_INPUT_PROC_DIR_E : LGTV_EXTERNAL_INPUT_PROC_DIR;
        lgtv_driver_external_input_dir = proc_mkdir(ext_input_dir, lgtv_driver_proc_dir);

        if (lgtv_driver_external_input_dir == NULL)
        {
            pr_err("create lgtv external input proc entry (%s) failed\n",
                LGTV_EXTERNAL_INPUT_PROC_DIR_E);
            return -1;
        }
    }

    // create "lgtv-driver/hdmi_status"
    if (lgtv_driver_hdmi_status_dir == NULL)
    {
        const char* name = (lgtv_driver_proc_dir == NULL)? LGTV_HDMI_STATUS_PROC_DIR_E : LGTV_HDMI_STATUS_PROC_DIR;
        lgtv_driver_hdmi_status_dir = proc_mkdir(name, lgtv_driver_proc_dir);

        if (lgtv_driver_hdmi_status_dir == NULL)
        {
            pr_err("create lgtv hdmi status proc entry (%s) failed\n",
                LGTV_HDMI_STATUS_PROC_DIR_E);
            return -1;
        }
    }

    // create "lgtv-driver/adc_status"
    if (lgtv_driver_adc_status_dir == NULL)
    {
        const char* name = (lgtv_driver_proc_dir == NULL)? LGTV_ADC_STATUS_PROC_DIR_E : LGTV_ADC_STATUS_PROC_DIR;
        lgtv_driver_adc_status_dir = proc_mkdir(name, lgtv_driver_proc_dir);

        if (lgtv_driver_adc_status_dir == NULL)
        {
            pr_err("create lgtv adc status proc entry (%s) failed\n",
                LGTV_ADC_STATUS_PROC_DIR_E);
            return -1;
        }
    }

    // create "lgtv-driver/avd_status"
    if (lgtv_driver_avd_status_dir == NULL)
    {
        const char* name = (lgtv_driver_proc_dir == NULL)? LGTV_AVD_STATUS_PROC_DIR_E : LGTV_AVD_STATUS_PROC_DIR;
        lgtv_driver_avd_status_dir = proc_mkdir(name, lgtv_driver_proc_dir);

        if (lgtv_driver_avd_status_dir == NULL)
        {
            pr_err("create lgtv avd status proc entry (%s) failed\n",
                LGTV_AVD_STATUS_PROC_DIR_E);
            return -1;
        }
    }

    return 0;
}


static void lgtv_external_input_status_proc_exit(void)
{
    if (lgtv_driver_external_input_dir) {
        proc_remove(lgtv_driver_external_input_dir);
        lgtv_driver_external_input_dir = NULL;
    }

    if (lgtv_driver_hdmi_status_dir) {
        proc_remove(lgtv_driver_hdmi_status_dir);
        lgtv_driver_hdmi_status_dir = NULL;
    }

    if (lgtv_driver_adc_status_dir) {
        proc_remove(lgtv_driver_adc_status_dir);
        lgtv_driver_adc_status_dir = NULL;
    }

    if (lgtv_driver_avd_status_dir) {
        proc_remove(lgtv_driver_avd_status_dir);
        lgtv_driver_avd_status_dir = NULL;
    }

    if (lgtv_driver_proc_dir) {
        proc_remove(lgtv_driver_proc_dir);
        lgtv_driver_proc_dir = NULL;
    }
}

fs_initcall(lgtv_external_input_status_proc_init);
module_exit(lgtv_external_input_status_proc_exit);
