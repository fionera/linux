/*
 * Copyright (c) 2018 Jias Huang <jias_huang@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>

#include "vcodec_conf.h"
#include "vcodec_struct.h"
#include "vcodec_utils.h"
#include "vdec_core.h"

#include <linux/v4l2-ext/videodev2-ext.h>
#include <scaler/scalerCommon.h>
#include <ioctrl/scaler/vsc_cmd_id.h>

extern void scaler_vsc_set_force_pst_lowdelay_mode(unsigned char bOnOff);
extern void scaler_vsc_set_adaptive_pst_lowdelay_mode(unsigned char bOnOff);
extern unsigned char rtk_hal_vsc_SetAdaptiveStreamEX(VIDEO_WID_T wid,unsigned char bOnOff);

static int ioctl_s_ctrl(struct file *file, void *fh, struct v4l2_control *ctrl)
{
	int ret;
	vcodec_device_t *device;

	device = video_drvdata(file);

	switch (ctrl->id) {

	#ifdef V4L2_CID_EXT_VDO_VDEC_PORT
		case V4L2_CID_EXT_VDO_VDEC_PORT:
			pr_notice("[VDOGAV] V4L2_CID_EXT_VDO_VDEC_PORT (%d) @ %s(%d)\n", ctrl->value, __FUNCTION__, __LINE__);
			//ret = connectVDO(device, ctrl->value);
			//////// hack for demo ///////////////////
			if(ctrl->value != -1){
				pr_notice("[VDOGAV] connect GAV, enter adaptive stream flow\n");
				scaler_vsc_set_force_pst_lowdelay_mode(1);
				scaler_vsc_set_adaptive_pst_lowdelay_mode(1);
				rtk_hal_vsc_SetAdaptiveStreamEX(VIDEO_WID_0, 1);
			}else{
				pr_notice("[VDOGAV] disconnect GAV, exit adaptive stream flow\n");
				scaler_vsc_set_force_pst_lowdelay_mode(0);
				scaler_vsc_set_adaptive_pst_lowdelay_mode(0);
				rtk_hal_vsc_SetAdaptiveStreamEX(VIDEO_WID_0, 0);
			}
			/////////////////////////////////////////
			ret = 0;
			break;
	#endif

	#ifdef V4L2_CID_EXT_VDEC_VDO_PORT
		case V4L2_CID_EXT_VDEC_VDO_PORT:
			pr_notice("[VDOGAV] V4L2_CID_EXT_VDEC_VDO_PORT (%d) @ %s(%d)\n", ctrl->value, __FUNCTION__, __LINE__);
			//ret = connectVDO(device, ctrl->value);
			ret = 0;
			break;
	#endif

		default:
			pr_err("%s: unknown id 0x%x val %d\n",
				__FUNCTION__, ctrl->id, ctrl->value);
			return -EINVAL;
	}

	return ret;
}



static int ioctl_s_ext_ctrls(struct file *file, void *fh, struct v4l2_ext_controls *ctrls)
{
	int ret, i;
	struct v4l2_ext_control *ctrl;
	vcodec_device_t *device;
	struct v4l2_ext_vdec_vdo_connection args;

	device = video_drvdata(file);

	ret = 0;
	for (i=0; !ret && i<ctrls->count; i++) {
		ctrl = &ctrls->controls[i];

		switch (ctrl->id) {

		case V4L2_CID_EXT_VDO_VDEC_CONNECTING:
			pr_notice("[VDOGAV] V4L2_CID_EXT_VDO_VDEC_CONNECTING @ %s(%d)\n", __FUNCTION__, __LINE__);
			if (copy_from_user(&args, ctrl->ptr, sizeof(args)))
				return -EFAULT;
			//ret = vdec_vdo_connection(device, &args);
			ret = 0;
			break;

		case V4L2_CID_EXT_VDO_VDEC_DISCONNECTING:
			pr_notice("[VDOGAV] V4L2_CID_EXT_VDO_VDEC_DISCONNECTING @ %s(%d)\n", __FUNCTION__, __LINE__);
			if (copy_from_user(&args, ctrl->ptr, sizeof(args)))
				return -EFAULT;

			args.vdec_port = -1;
			//ret = vdec_vdo_connection(device, &args);
			ret = 0;
			break;

		default:
			pr_err("%s: unknown id 0x%x\n",
				__FUNCTION__, ctrl->id);
			return -EINVAL;
		}
	}

	return ret;
}


static int ioctl_s_input(struct file *file, void *priv, unsigned int channel)
{
	vcodec_device_t *device;

	pr_notice("[VDOGAV] %s(%d)\n", __FUNCTION__, __LINE__);

	device = video_drvdata(file);

	dev_info(device->dev, "connect vdo%d to vdec channel %d\n",
		device->port, channel);

	//return connectDisplay(device, channel);
	return 0;
}


const struct v4l2_ioctl_ops vdogav_ioctl_ops = {
	.vidioc_s_ctrl = ioctl_s_ctrl,
	.vidioc_s_input = ioctl_s_input,
	.vidioc_s_ext_ctrls = ioctl_s_ext_ctrls,
};

const struct v4l2_file_operations vdogav_file_ops = {
	.owner = THIS_MODULE,
	.open = v4l2_fh_open,
	.release = v4l2_fh_release,
	.unlocked_ioctl = video_ioctl2,
};
