#ifndef _AVD_V4L2_H_
#define _AVD_V4L2_H_
#include <generated/autoconf.h>
#include <mach/rtk_log.h>
#include <ioctrl/scaler/vfe_cmd_id.h>
#ifdef CONFIG_KDRIVER_USE_NEW_COMMON
#include <scaler/scalerDrvCommon.h>
#else
#include <scalercommon/scalerDrvCommon.h>
#endif
#include <tvscalercontrol/scaler/source.h>
#include <tvscalercontrol/scaler/scalerstruct.h>
#include <tvscalercontrol/avd/avdctrl.h>
#include <tvscalercontrol/scaler_vfedev.h>
#include <tvscalercontrol/vdc/video.h>

#define TAG_NAME "AVD_V4L2"

typedef unsigned char (*DETECTCALLBACK)(void);
extern void VDC_register_callback(unsigned char enable, DETECTCALLBACK cb);//if enable is true, set the cb for VDC
extern void VDC_set_detect_flag(unsigned char enable);
extern unsigned char VDC_get_detect_flag(void);
extern unsigned short get_AVD_Input(void);
extern void set_AVD_Input(unsigned short input_port);
extern unsigned char get_AVD_Global_Status(void);
extern void set_AVD_Global_Status(SOURCE_STATUS status);
extern unsigned char get_ATV_Global_Status(void);
extern void set_ATV_Global_Status(SOURCE_STATUS status);
extern unsigned char get_AV_Global_Status(void);
extern void set_AV_Global_Status(SOURCE_STATUS status);

extern struct semaphore *get_vdc_detectsemaphore(void);
extern void Set_Reply_Zero_Timing_Flag(unsigned char src, unsigned char flag);

extern KADP_VFE_AVD_TIMING_INFO_T *Get_AVD_LGETiminginfo(void);

#if 0//rzhen@20190522
extern unsigned short ATV_PinNum;
extern unsigned char AVD_Global_Status;
extern unsigned char ATV_Status;
extern unsigned char AV_Status;
extern unsigned char SCART_Status;
extern unsigned short AVD_Input_Source;
extern unsigned char AVD_Reply_Zero_Timing;/*If true, let webos get zero timing*/
extern unsigned char VDC_DetectFlag;//Decide VDC detect timing or not
extern DETECTCALLBACK VDC_DetectCB;
extern struct task_struct *p_avd_detect_task;
extern bool avd_detect_tsk_running_flag;//Record avd_detect_tsk status. True: Task is running

static struct semaphore V4L2_AVD_DetectSemaphore;/*This semaphore is set avd detect flag and callback*/

void create_avd_detect_tsk(void);
void delete_avd_detect_tsk(void);

unsigned char v4l2_get_atv_v4l2_device_init_flag(void);
void v4l2_set_atv_v4l2_device_init_flag(unsigned char enable);
unsigned char v4l2_get_av_v4l2_device_init_flag(void);
void v4l2_set_av_v4l2_device_init_flag(unsigned char enable);
unsigned char v4l2_get_avd_detect_tsk_create_flag(void);
void v4l2_set_avd_detect_tsk_create_flag(unsigned char enable);

#endif

#endif
