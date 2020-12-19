#ifndef _LINUX_RTKAUDIO_H
#define _LINUX_RTKAUDIO_H

#include <generated/autoconf.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/string.h>

#include <rtk_kdriver/rtkaudio_debug.h>
#include "AudioRPCBaseDS_data.h"
#include "AudioInbandAPI.h"
#include "ioctrl/audio/audio_cmd_id.h"

#define RTKAUDIO_MAJOR             0
#define RTKAUDIO_MINOR 0

#define RTKAUDIO_SUSPEND           100
#define RTKAUDIO_RESUME            101
#define RTKAUDIO_RESET_PREPARE     102
#define RTKAUDIO_RESET_DONE        103

#define REMOTE_MALLOC_DISABLE	0
#define REMOTE_MALLOC_ENABLE	1

#ifdef CONFIG_HIBERNATION
extern int in_suspend;
#endif

#define RTKAUDIO_PROC_DIR   "rtkaudio"
#define RTKAUDIO_PROC_ENTRY "debuginfo"

#define LGTV_DRIVER_PROC_DIR           "lgtv-driver"
#define LGTV_AUDIO_STATUS_PROC_DIR     "audio"
#define LGTV_AUDIO_STATUS_PROC_DIR_E   "lgtv-driver/audio"
#define LGTV_STATUS_PROC_ENTRY_ADEC_P0 "adec.p0"
#define LGTV_STATUS_PROC_ENTRY_ADEC_P1 "adec.p1"
#define LGTV_STATUS_PROC_ENTRY_SNDOUT  "sndout"
#define LGTV_STATUS_PROC_ENTRY_LGSE    "lgse"
#define LGTV_STATUS_PROC_ENTRY_AMIXER  "amixer"
#define LGTV_STATUS_PROC_ENTRY_HDMI    "hdmi"
#define LGTV_STATUS_PROC_ENTRY_ATV     "atv"
#define LGTV_STATUS_PROC_ENTRY_AENC    "aenc"

int register_rtkaudio_notifier(struct notifier_block *nb);
int unregister_rtkaudio_notifier(struct notifier_block *nb);
void halt_acpu(void);
long rtkaudio_send_hdmi_fs(long hdmi_fs);

/* Record memory use*/
struct buffer_info {
	struct list_head buffer_list;

	/* virtual address */
	unsigned long vir_addr;

	/* physical address */
	unsigned long phy_addr;

	/* request size by audio DSP */
	unsigned long request_size;

	/* kernel malloc size */
	unsigned long malloc_size;
	pid_t         task_pid;
};

/* Store Open list */
struct rtkaudio_open_list {
	struct list_head open_list;
	pid_t pid;
	struct gst_info info;
};

#define refc_info_size 8
struct refclock_info {
	unsigned int phy_addr;
	int pid;
	int port;
};

/* Store Gst RefClock list */
struct gst_refc_list {
	int size;
	int index;
	struct refclock_info info[refc_info_size];
};

typedef enum {
	LR_DIGITAL_VOLUME_CONTROL_STATUS = 0,
	CSW_DIGITAL_VOLUME_CONTROL_STATUS = 1,
	LSRS_DIGITAL_VOLUME_CONTROL_STATUS = 2,
	LRRR_DIGITAL_VOLUME_CONTROL_STATUS = 3,
	GST_CLOCK = 4,
	AMIXER_STATUS = 5,
	DEBUG_REGISTER = 6
} AUDIO_REGISTER_TYPE;

typedef struct
{
	AUDIO_REGISTER_TYPE reg_type;
	int reg_value;	/* register value */
	int data;   	/* value for data write */
} AUDIO_REGISTER_ACCESS_T;

#define ENUM_KERNEL_RPC_AUDIO_CONFIG 0x204

/* defined in kernel/printk.c */
extern int console_printk[4];

#define rtkaudio_must_print(fmt, ...)	\
{\
	int prev_printk_level = console_printk[0];\
	if (prev_printk_level == 0)\
		console_printk[0] = 1;\
	pr_emerg(fmt, ##__VA_ARGS__);\
	console_printk[0] = prev_printk_level;\
}

#endif

