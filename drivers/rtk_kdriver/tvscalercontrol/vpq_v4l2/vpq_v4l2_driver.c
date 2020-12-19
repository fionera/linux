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
#include <media/v4l2-event.h>

#include <linux/module.h>
#include <linux/fs.h>		/* everything... */
#include <linux/cdev.h>
#include <linux/platform_device.h>
//common
#include <ioctrl/vpq/vpq_cmd_id.h>



//TVScaler Header file
#include <tvscalercontrol/vpq_v4l2/vpq_v4l2_structure.h>
#include <tvscalercontrol/vpq_v4l2/vpq_v4l2_api.h>

#include <tvscalercontrol/scaler_vpqdev.h>

#include <mach/rtk_log.h>

#define VPQ_V4L2_MAIN_DEVICE_NAME "/dev/video50" //main scaler
#define TAG_NAME "VPQ"
static int vpq_v4l2_main_open(struct file *file)
{	return 0;
}

static int vpq_v4l2_main_release(struct file *file)
{
	return 0;
}

static ssize_t vpq_v4l2_main_read(struct file *file, char __user *data, size_t count, loff_t *ppos)
{
	return 0;
}

static unsigned int vpq_v4l2_main_poll(struct file *file, poll_table *wait)
{
	return 0;
}

static int vpq_v4l2_main_mmap(struct file *file, struct vm_area_struct *vma)
{
       return 0;
}


static const struct v4l2_ioctl_ops vpq_main_ioctl_ops = {
    .vidioc_s_ext_ctrls = vpq_v4l2_main_ioctl_s_ext_ctrls,
    .vidioc_g_ext_ctrls = vpq_v4l2_main_ioctl_g_ext_ctrls,
    .vidioc_s_ctrl = vpq_v4l2_main_ioctl_s_ctrl,
    .vidioc_g_ctrl = vpq_v4l2_main_ioctl_g_ctrl,

};

static const struct v4l2_file_operations vpq_main_fops = {
	.owner		= THIS_MODULE,
	.open		= vpq_v4l2_main_open,
	.release	= vpq_v4l2_main_release,
	.read		= vpq_v4l2_main_read,
	.poll		= vpq_v4l2_main_poll,
	.unlocked_ioctl = video_ioctl2, /* V4L2 ioctl handler */
	.mmap		= vpq_v4l2_main_mmap,
};

static struct video_device vpq_main_v4l2_tmplate = {
	.name = "video50",
	.fops	= &vpq_main_fops,
	.ioctl_ops = &vpq_main_ioctl_ops,
	.minor = -1,
	.release = video_device_release,
};

static int __init vpq_v4l2_init(void)
{
	struct vpq_v4l2_device *main_dev=NULL;
	struct video_device *main_vfd=NULL;
	int ret;
	main_dev = kzalloc(sizeof(*main_dev), GFP_KERNEL);
	if (!main_dev)
		return -ENOMEM;
	strcpy(main_dev->v4l2_dev.name, "/dev/video50");

	ret = v4l2_device_register(NULL, &main_dev->v4l2_dev);
	if (ret)
	{
		goto free_dev;
	}

        rtd_printk(KERN_EMERG, TAG_NAME, "vpq_v4l2_init \n");

	main_vfd = video_device_alloc();
	if (!main_vfd) {
		v4l2_err(&main_dev->v4l2_dev, "Failed to allocate video device\n");


		rtd_printk(KERN_EMERG, TAG_NAME, "v4l2 open fail \n");
		ret = -ENOMEM;
		goto unreg_dev;
	}

	*main_vfd = vpq_main_v4l2_tmplate;
	main_vfd->v4l2_dev = &main_dev->v4l2_dev; /* here set the v4l2_device, you have already registered it */
	ret = video_register_device(main_vfd, VFL_TYPE_GRABBER, 50);//main scaler name
	if (ret) {
		v4l2_err(&main_dev->v4l2_dev, "Failed to register video device\n");
		rtd_printk(KERN_EMERG, TAG_NAME, "register v4l2 open fail \n");


		goto rel_vdev;
	}

	video_set_drvdata(main_vfd, main_dev);
	snprintf(main_vfd->name, sizeof(main_vfd->name), "%s", vpq_main_v4l2_tmplate.name);
	main_dev->vfd = main_vfd;
	/* the debug message*/
   	 v4l2_info(&main_dev->v4l2_dev, "V4L2 device registered as %s\n", video_device_node_name(main_vfd));


        return 0;


rel_vdev:
	video_device_release(main_vfd);
unreg_dev:
	v4l2_device_unregister(&main_dev->v4l2_dev);
free_dev:
	kfree(main_dev);

	return ret;
}



static void __exit vpq_v4l2_exit(void)
{
	return;
}

module_init(vpq_v4l2_init);

module_exit(vpq_v4l2_exit);

MODULE_LICENSE("GPL");
