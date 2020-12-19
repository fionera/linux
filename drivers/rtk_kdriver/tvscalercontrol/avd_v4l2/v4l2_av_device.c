/*
 *      v4l2_av_device.c  --  Composite input(AVD) Video Class driver
 *
 *      Copyright (C) 2018
 *
 */
#include <linux/atomic.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/freezer.h>/*freezing*/
	 
#include <asm/unaligned.h>
	 
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-event.h>
#include <media/v4l2-common.h>

//#include <tvscalercontrol/avd_v4l2/v4l2_avd_device.h>
#include <tvscalercontrol/avd_v4l2/v4l2_avd_api.h>
#include <linux/v4l2-ext/v4l2-controls-ext.h>
#include <linux/v4l2-ext/videodev2-ext.h>


//common
#include <ioctrl/scaler/vfe_cmd_id.h>
#include <ioctrl/scaler/vsc_cmd_id.h>


#define AV_V4L2_DEVICE_NAME "video10" //AV V4L2 Device name
#define AV_V4L2_DEVICE_NUM  10 //AV V4L2 Device number(videoX)

/* V4L2 interface */
extern const struct v4l2_ioctl_ops avd_ioctl_ops;
extern const struct v4l2_file_operations avd_fops;

struct av_device {
	struct v4l2_device v4l2_dev;
	struct video_device *vfd;

	v4l2_std_id std; /* Current set standard */
};

struct avd_fh {
	struct v4l2_fh vfh;
	struct v4l2_control *control;
	struct v4l2_ext_control *ext_control;
	struct v4l2_ext_controls *ext_controls;
	struct v4l2_ext_avd_timing_info *avd_timing_info;
	enum v4l2_ext_avd_input_src src;
	v4l2_std_id std;
};

static struct video_device avd_v4l2_tmplate = {
	.name = AV_V4L2_DEVICE_NAME,
	.fops	= &avd_fops,
	.ioctl_ops = &avd_ioctl_ops,
	.minor = -1,
	.release = video_device_release,
	.tvnorms      = V4L2_STD_ALL,
};

static int __init av_v4l2_init(void)
{
	struct av_device *av_dev;
	struct video_device *av_vfd;
	int ret;
	av_dev = kzalloc(sizeof(*av_dev), GFP_KERNEL);
	if (!av_dev)
		return -ENOMEM;
	strcpy(av_dev->v4l2_dev.name, "/dev/"AV_V4L2_DEVICE_NAME);

	ret = v4l2_device_register(NULL, &av_dev->v4l2_dev);
	if (ret)
	{
		goto free_dev;
	}


	av_vfd = video_device_alloc();
	if (!av_vfd) {
		v4l2_err(&av_dev->v4l2_dev, "Failed to allocate video device\n");
		ret = -ENOMEM;
		goto unreg_dev;
	}

	*av_vfd = avd_v4l2_tmplate;
	av_vfd->v4l2_dev = &av_dev->v4l2_dev; /* here set the v4l2_device, you have already registered it */
	ret = video_register_device(av_vfd, VFL_TYPE_GRABBER, AV_V4L2_DEVICE_NUM);//AV V4L2 name
	if (ret) {
		v4l2_err(&av_dev->v4l2_dev, "Failed to register video device\n");
		goto rel_vdev;
	}

	video_set_drvdata(av_vfd, av_dev);
	snprintf(av_vfd->name, sizeof(av_vfd->name), "%s", avd_v4l2_tmplate.name);
	av_dev->vfd = av_vfd;
	av_dev->std = V4L2_STD_UNKNOWN;
	/* the debug message*/
   	v4l2_info(&av_dev->v4l2_dev, "[AVD_V4L2]%s(%d)V4L2 device registered as %s\n", __func__, __LINE__,video_device_node_name(av_vfd));
#if 0 //rzhen@20190522
	if(AVD_Global_Status == SRC_NOTHING){
		Scaler_AVD_Init(KADP_VFE_AVD_CC_AND_TTX_ARE_SUPPORTED);
		AVD_Global_Status = SRC_INIT_DONE;
	}

	if ((ret == 0)&&(v4l2_get_avd_detect_tsk_create_flag() !=TRUE)&&(avd_detect_tsk_running_flag ==FALSE)) {
		sema_init(&V4L2_AVD_DetectSemaphore, 1);
		rtd_printk(KERN_INFO, TAG_NAME, "#####[%s(%d)]\n",__func__,__LINE__);
		create_avd_detect_tsk();
		v4l2_set_avd_detect_tsk_create_flag(TRUE);
	}
	v4l2_set_av_v4l2_device_init_flag(TRUE);
#endif
	return 0;


rel_vdev:
	video_device_release(av_vfd);
unreg_dev:
	v4l2_device_unregister(&av_dev->v4l2_dev);
free_dev:
	kfree(av_dev);

	return ret;
}

static void __exit av_v4l2_exit(void)
{
	rtd_printk(KERN_INFO, TAG_NAME,"%s(%d),\n", __func__, __LINE__);
#if 0//rzhen@20190522
	if(v4l2_get_atv_v4l2_device_init_flag()==FALSE){
		AVD_Global_Status = SRC_NOTHING;
		delete_avd_detect_tsk();
		Scaler_AVD_Uninit();
	}
	v4l2_set_av_v4l2_device_init_flag(FALSE);
#endif
	return;
}

module_init(av_v4l2_init);
module_exit(av_v4l2_exit);
MODULE_LICENSE("GPL");
