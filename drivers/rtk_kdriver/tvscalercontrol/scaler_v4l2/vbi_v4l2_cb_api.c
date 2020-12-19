/*
 *      vbi_v4l2_driver.c  related callback api--
 *
 *      Copyright (C) 2018
 *
 */
//Kernel Header file
#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/version.h>
#include <asm/unaligned.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-common.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <uapi/linux/videodev2.h>

//common
#include <ioctrl/scaler/vfe_cmd_id.h>
#ifdef CONFIG_KDRIVER_USE_NEW_COMMON
#include <scaler/scalerCommon.h>
#else
#include <scalercommon/scalerCommon.h>
#endif

//TVScaler Header file
#include <tvscalercontrol/scaler_v4l2/vbi_v4l2_api.h>
#include <tvscalercontrol/scaler_v4l2/vbi_v4l2_structure.h>






