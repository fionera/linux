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

extern char vsc_v4l2_vdo_control(unsigned char display, struct v4l2_ext_vsc_vdo_mode mode);

static int connectDisplay(vcodec_device_t *device, int channel)
{
	vdec_handle_t *handle;

	vcodec_lockDecDevice(device);

	handle = (channel >= 0)? vcodec_getDecByChannel(device, channel):
		vcodec_getDecByDisplay(device, device->port);

	if (handle)
		vdec_connectDisplay(handle, (channel >= 0)? device->port : -1, ((handle->userType == V4L2_EXT_VDEC_USERTYPE_DTV)? 1: 0), 0);

	vcodec_unlockDecDevice(device);

	return (handle)? 0 : -EINVAL;
}

static int connectVDO(vcodec_device_t *device, int vdecPort)
{
    //vdo connect or disconnect
    struct v4l2_ext_vsc_vdo_mode mode = {V4L2_EXT_VSC_VDO_PORT_NONE, 0};
    unsigned char display = SLR_MAIN_DISPLAY;

    vcodec_lockDecDevice(device);

    if(device->port == 1) {
        display = SLR_SUB_DISPLAY;
    } else {
        display = SLR_MAIN_DISPLAY;
    }

    mode.vdec_port = vdecPort;
    mode.vdo_port = (vdecPort == -1)? V4L2_EXT_VSC_VDO_PORT_NONE: (device->port == 1 ? V4L2_EXT_VSC_VDO_PORT_1: V4L2_EXT_VSC_VDO_PORT_0);

    if(!vsc_v4l2_vdo_control(display, mode))
    {
        pr_err("func:%s [error] vsc_v4l2_vdo_control fail \r\n",__FUNCTION__);
        vcodec_unlockDecDevice(device);
        return -EFAULT;
    }

    vcodec_unlockDecDevice(device);

    return 0;
}

static int ioctl_s_ctrl(struct file *file, void *fh, struct v4l2_control *ctrl)
{
	int ret;
	vcodec_device_t *device;

	device = video_drvdata(file);

	switch (ctrl->id) {

		#ifdef V4L2_CID_EXT_VDO_VDEC_PORT
		case V4L2_CID_EXT_VDO_VDEC_PORT:
            pr_info("[VDO] V4L2_CID_EXT_VDO_VDEC_PORT @ %s(%d)\n", __FUNCTION__, __LINE__);
			ret = connectVDO(device, ctrl->value);
			break;
		#endif

		#ifdef V4L2_CID_EXT_VDEC_VDO_PORT
		case V4L2_CID_EXT_VDEC_VDO_PORT:
            pr_info("[VDO] V4L2_CID_EXT_VDEC_VDO_PORT @ %s(%d)\n", __FUNCTION__, __LINE__);
			ret = connectVDO(device, ctrl->value);
			break;
		#endif

		default:
			pr_err("%s: unknown id 0x%x val %d\n",
				__FUNCTION__, ctrl->id, ctrl->value);
			return -EINVAL;
	}

	return ret;
}

static int vdec_vdo_connection(vcodec_device_t *device, struct v4l2_ext_vdec_vdo_connection *connection)
{
    //vdo connect or disconnect
    struct v4l2_ext_vsc_vdo_mode mode = {V4L2_EXT_VSC_VDO_PORT_NONE, 0};
    unsigned char display = SLR_MAIN_DISPLAY;

    vcodec_lockDecDevice(device);

    if(connection->vdo_port == 1) {
        display = SLR_SUB_DISPLAY;
    } else {
        display = SLR_MAIN_DISPLAY;
    }

    mode.vdo_port = (connection->vdec_port == -1)? V4L2_EXT_VSC_VDO_PORT_NONE: (connection->vdo_port == 1 ? V4L2_EXT_VSC_VDO_PORT_1: V4L2_EXT_VSC_VDO_PORT_0);
    mode.vdec_port = connection->vdec_port;

    if(!vsc_v4l2_vdo_control(display, mode))
    {
        pr_err("func:%s [error] vsc_v4l2_vdo_control fail \r\n",__FUNCTION__);
        vcodec_unlockDecDevice(device);
        return -EFAULT;
    }

    vcodec_unlockDecDevice(device);

    return 0;
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
            pr_info("[VDO] V4L2_CID_EXT_VDO_VDEC_CONNECTING @ %s(%d)\n", __FUNCTION__, __LINE__);
            if (copy_from_user(&args, ctrl->ptr, sizeof(args)))
                return -EFAULT;
            ret = vdec_vdo_connection(device, &args);
            break;

        case V4L2_CID_EXT_VDO_VDEC_DISCONNECTING:
            pr_info("[VDO] V4L2_CID_EXT_VDO_VDEC_DISCONNECTING @ %s(%d)\n", __FUNCTION__, __LINE__);
            if (copy_from_user(&args, ctrl->ptr, sizeof(args)))
                return -EFAULT;

            args.vdec_port = -1;
            ret = vdec_vdo_connection(device, &args);
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

	device = video_drvdata(file);

	dev_info(device->dev, "connect vdo%d to vdec channel %d\n",
		device->port, channel);

	return connectDisplay(device, channel);
}

const struct v4l2_ioctl_ops vdo_ioctl_ops = {
	.vidioc_s_ctrl = ioctl_s_ctrl,
	.vidioc_s_input = ioctl_s_input,
	.vidioc_s_ext_ctrls = ioctl_s_ext_ctrls,
};

const struct v4l2_file_operations vdo_file_ops = {
	.owner = THIS_MODULE,
	.open = v4l2_fh_open,
	.release = v4l2_fh_release,
	.unlocked_ioctl = video_ioctl2,
};
