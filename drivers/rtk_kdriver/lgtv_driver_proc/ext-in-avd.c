#include <linux/syscalls.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <mach/io.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <lgtv_driver_proc/procfs.h>
#include <ioctrl/scaler/vfe_cmd_id.h>

extern unsigned char get_avd_input_source(void);
extern unsigned char get_avd_open_count(void);
extern unsigned char get_avd_port_number(void);
extern unsigned char get_avd_auto_tuning_mode(void);
extern unsigned char Scaler_AVD_DoesSyncExist(void);
extern unsigned int get_avd_color_standard(void);
extern KADP_VFE_AVD_TIMING_INFO_T *Get_AVD_LGETiminginfo(void);

static int v4l2_get_avd_status(void* private_data, char* buff, unsigned int buff_size)
{
    int len, total = 0;
	KADP_VFE_AVD_TIMING_INFO_T *ptTimingInfo = NULL;

	//---------------------------------------------------------------------
    // Title info
    //---------------------------------------------------------------------
    len = snprintf(buff, buff_size, "===== AVD STATUS =====\n");
    total += len; buff_size-=len; buff+= len;

    //---------------------------------------------------------------------
    // General info
    //---------------------------------------------------------------------
    len = snprintf(buff, buff_size, "open=%d\n", get_avd_open_count());
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "input_src=%d\n", get_avd_input_source());
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "hwport=%d\n", get_avd_port_number());
    total += len; buff_size-=len; buff+= len;

    //---------------------------------------------------------------------
    // Timing info
    //---------------------------------------------------------------------
    ptTimingInfo = Get_AVD_LGETiminginfo();
	if(!ptTimingInfo) return 0;

    len = snprintf(buff, buff_size, "timing_h_freq=%d\n", ptTimingInfo->hFreq);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "timing_v_freq=%d\n", ptTimingInfo->vFreq);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "timing_h_porch=%d\n", ptTimingInfo->hPorch);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "timing_v_porch=%d\n", ptTimingInfo->vPorch);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "timing_active_w=%d\n", ptTimingInfo->active.w);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "timing_active_h=%d\n", ptTimingInfo->active.h);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "timing_active_x=%d\n", ptTimingInfo->active.x);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "timing_active_y=%d\n", ptTimingInfo->active.y);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "timing_vd_lock=%d\n", ptTimingInfo->vdLock);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "timing_h_lock=%d\n", ptTimingInfo->HLock);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "timing_v_lock=%d\n", ptTimingInfo->VLock);
    total += len; buff_size-=len; buff+= len;

    //---------------------------------------------------------------------
    // Packet info
    //---------------------------------------------------------------------
    len = snprintf(buff, buff_size, "avd_sync=%d\n", Scaler_AVD_DoesSyncExist());//0: unlock, 1:locked
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "color_standard=0x%x\n", get_avd_color_standard());
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "auto_tuning_mode=%d\n", get_avd_auto_tuning_mode());
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "noise_level=%d\n", 499);
    total += len; buff_size-=len; buff+= len;

    return total;
}

///////////////////////////////////////////////////////////////////
// Module init/exit
///////////////////////////////////////////////////////////////////
static int lgtv_avd_proc_init(void)
{
    create_lgtv_avd_status_proc_entry("avd", v4l2_get_avd_status, (void*) 0);

    return 0;
}

late_initcall(lgtv_avd_proc_init);
