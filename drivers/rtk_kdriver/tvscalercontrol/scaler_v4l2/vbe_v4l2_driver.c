/*
 *      vsc_v4l2_driver.c  --
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
#include <linux/videodev2.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/version.h>
#include <asm/unaligned.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-common.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>

//common
#include <ioctrl/scaler/vbe_cmd_id.h>

#include <linux/v4l2-ext/v4l2-controls-ext.h>
#include <linux/v4l2-ext/videodev2-ext.h>

//TVScaler Header file
#include <tvscalercontrol/scaler_v4l2/vbe_v4l2_api.h>


#define VBE_V4L2_MAIN_DEVICE_NAME "video40" //main scaler

static int vbe_v4l2_main_open(struct file *file)
{	return 0;
}

static int vbe_v4l2_main_release(struct file *file)
{
	return 0;
}

static ssize_t vbe_v4l2_main_read(struct file *file, char __user *data, size_t count, loff_t *ppos)
{
	return 0;
}

static unsigned int vbe_v4l2_main_poll(struct file *file, poll_table *wait)
{
	return 0;
}

static int vbe_v4l2_main_mmap(struct file *file, struct vm_area_struct *vma)
{
       return 0;
}


static const struct v4l2_ioctl_ops vbe_main_ioctl_ops = {
	.vidioc_s_ctrl = vbe_v4l2_main_ioctl_s_ctrl,
	.vidioc_s_ext_ctrls = vbe_v4l2_main_ioctl_s_ext_ctrl,
        .vidioc_g_ext_ctrls = vbe_v4l2_main_ioctl_g_ext_ctrl,
};

static const struct v4l2_file_operations vbe_main_fops = {
	.owner		= THIS_MODULE,
	.open		= vbe_v4l2_main_open,
	.release		= vbe_v4l2_main_release,
	.read		= vbe_v4l2_main_read,
	.poll			= vbe_v4l2_main_poll,
	.unlocked_ioctl  = video_ioctl2, /* V4L2 ioctl handler */
	.mmap		= vbe_v4l2_main_mmap,
};

static struct video_device vbe_main_v4l2_tmplate = {
	.name = VBE_V4L2_MAIN_DEVICE_NAME,
	.fops	= &vbe_main_fops,
	.ioctl_ops = &vbe_main_ioctl_ops,
	.minor = -1,
	.release = video_device_release,
};

static int __init vbe_v4l2_init(void)
{
	struct vbe_v4l2_device *main_dev;
	struct video_device *main_vfd=NULL;
	int ret;
	main_dev = kzalloc(sizeof(*main_dev), GFP_KERNEL);
	if (!main_dev)
		return -ENOMEM;
	strcpy(main_dev->v4l2_dev.name, V4L2_EXT_DEV_PATH_BACKEND);

	ret = v4l2_device_register(NULL, &main_dev->v4l2_dev);
	if (ret)
	{
		goto free_dev;
	}


	main_vfd = video_device_alloc();
	if (!main_vfd) {
		v4l2_err(&main_dev->v4l2_dev, "Failed to allocate video device\n");
		ret = -ENOMEM;
		goto unreg_dev;
	}

	//*main_vfd = vbe_main_v4l2_tmplate;

	memcpy(main_vfd->name, vbe_main_v4l2_tmplate.name, 32);
	main_vfd->fops = vbe_main_v4l2_tmplate.fops;
	main_vfd->ioctl_ops = vbe_main_v4l2_tmplate.ioctl_ops;
	main_vfd->minor = vbe_main_v4l2_tmplate.minor;
	main_vfd->release = video_device_release;
	main_vfd->v4l2_dev = &main_dev->v4l2_dev; /* here set the v4l2_device, you have already registered it */

	ret = video_register_device(main_vfd, VFL_TYPE_GRABBER, 40);//main scaler name
	if (ret) {
		v4l2_err(&main_dev->v4l2_dev, "Failed to register video device\n");
		goto rel_vdev;
	}

	video_set_drvdata(main_vfd, main_dev);
	snprintf(main_vfd->name, sizeof(main_vfd->name), "%s", vbe_main_v4l2_tmplate.name);
	main_dev->vfd = main_vfd;
	/* the debug message*/
   	 v4l2_info(&main_dev->v4l2_dev, "V4L2 device registered as %s\n", video_device_node_name(main_vfd));
	return 0;


rel_vdev:
	if(main_vfd)
		video_device_release(main_vfd);
unreg_dev:
	v4l2_device_unregister(&main_dev->v4l2_dev);
free_dev:
	if(main_dev)
		kfree(main_dev);

	return ret;
}



static void __exit vbe_v4l2_exit(void)
{
	return;
}

module_init(vbe_v4l2_init);

module_exit(vbe_v4l2_exit);

MODULE_LICENSE("GPL");
