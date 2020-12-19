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
#include <linux/freezer.h>//For wake_up and wait
#include <linux/wait.h>
#include <linux/version.h>
#include <linux/spinlock_types.h>
#include <linux/poll.h>
#include <linux/spinlock_types.h>/*For spinlock*/
#include <asm/unaligned.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-common.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>

//common
#include <ioctrl/scaler/vsc_cmd_id.h>

#include <linux/v4l2-ext/v4l2-controls-ext.h>
#include <linux/v4l2-ext/videodev2-ext.h>
#include <tvscalercontrol/scaler_v4l2/vsc_v4l2_api.h>
#include <scaler/scalerCommon.h>

#define VSC_V4L2_MAIN_DEVICE_NAME "video30" //main scaler /tmp use

DECLARE_WAIT_QUEUE_HEAD(main_vsc_read_wait);
DECLARE_WAIT_QUEUE_HEAD(sub_vsc_read_wait);

static DECLARE_WAIT_QUEUE_HEAD(VSC_V4L2_POLL_MAIN_EVENT);
static DECLARE_WAIT_QUEUE_HEAD(VSC_V4L2_POLL_SUB_EVENT);

static DEFINE_SPINLOCK(VSC_V4L2_POLL_MAIN_EVENT_SPINLOCK);
static DEFINE_SPINLOCK(VSC_V4L2_POLL_SUB_EVENT_SPINLOCK);

static unsigned char MAIN_POLL_EVENT_VALUE = 0;
static unsigned char SUB_POLL_EVENT_VALUE = 0;

void Set_poll_event(unsigned char display, unsigned char clear, unsigned char value)
{
	
	if(display == SLR_MAIN_DISPLAY)
	{
		unsigned long flags;//for spin_lock_irqsave	
		spin_lock_irqsave(&VSC_V4L2_POLL_MAIN_EVENT_SPINLOCK, flags);
		if(value == 0)
			MAIN_POLL_EVENT_VALUE = 0;
		else
		{
			if(clear)
				MAIN_POLL_EVENT_VALUE &= (~value);
			else
				MAIN_POLL_EVENT_VALUE |= value;
		}
		spin_unlock_irqrestore(&VSC_V4L2_POLL_MAIN_EVENT_SPINLOCK, flags);
		if(value && (clear == 0))
		{
			wake_up(&main_vsc_read_wait);
		}
	}
	else
	{
		unsigned long flags;//for spin_lock_irqsave
		spin_lock_irqsave(&VSC_V4L2_POLL_SUB_EVENT_SPINLOCK, flags);
		if(value == 0)
			SUB_POLL_EVENT_VALUE = 0;
		else
		{
			if(clear)
				SUB_POLL_EVENT_VALUE &= (~value);
			else
				SUB_POLL_EVENT_VALUE |= value;
		}
		spin_unlock_irqrestore(&VSC_V4L2_POLL_SUB_EVENT_SPINLOCK, flags);
		if(value && (clear == 0))
		{
			wake_up(&sub_vsc_read_wait);
		}
	}
}


static int vsc_v4l2_mainorsub_open(struct file *file)
{	
	int ret;
	struct vsc_v4l2_device *vsc_dev = video_drvdata(file);
	rtk_hal_vsc_initialize();
	mutex_lock(&vsc_dev->lock);
	ret = v4l2_fh_open(file);
	mutex_unlock(&vsc_dev->lock);

    return ret;
}

static int vsc_v4l2_mainorsub_release(struct file *file)
{	
	int ret;
	struct vsc_v4l2_device *vsc_dev = video_drvdata(file);
	mutex_lock(&vsc_dev->lock);
	ret = v4l2_fh_release(file);
	mutex_unlock(&vsc_dev->lock);
    return ret;
}

static ssize_t vsc_v4l2_main_read(struct file *file, char __user *data, size_t count, loff_t *ppos)
{	
	return 0;
}

static unsigned int vsc_v4l2_main_poll(struct file *file, poll_table *wait)
{
	poll_wait(file, &main_vsc_read_wait, wait);
	if(MAIN_POLL_EVENT_VALUE)
	{
		Set_poll_event(SLR_MAIN_DISPLAY, 1 , 0);
		return POLLIN;
	}
	return 0;
}

static int vsc_v4l2_main_mmap(struct file *file, struct vm_area_struct *vma)
{	
       return 0;
}


static const struct v4l2_ioctl_ops vsc_main_ioctl_ops = {
    .vidioc_s_ext_ctrls = vsc_v4l2_ioctl_s_ext_ctrls,
    .vidioc_g_ext_ctrls = vsc_v4l2_ioctl_g_ext_ctrls,
    .vidioc_s_ctrl = vsc_v4l2_ioctl_s_ctrl,
    .vidioc_g_ctrl = vsc_v4l2_ioctl_g_ctrl,
	.vidioc_subscribe_event = vsc_v4l2_main_subscribe_event,
	.vidioc_unsubscribe_event = vsc_v4l2_main_event_unsubscribe,
	.vidioc_querycap = vsc_main_v4l2_ioctl_querycap,
};

static const struct v4l2_file_operations vsc_main_fops = {
	.owner		= THIS_MODULE,
	.open		= vsc_v4l2_mainorsub_open,
	.release		= vsc_v4l2_mainorsub_release,
	.read		= vsc_v4l2_main_read,
	.poll			= vsc_v4l2_main_poll,
	.unlocked_ioctl  = video_ioctl2, /* V4L2 ioctl handler */
	.mmap		= vsc_v4l2_main_mmap,
};

static struct video_device vsc_main_v4l2_tmplate = {
	.name = VSC_V4L2_MAIN_DEVICE_NAME,
	.fops	= &vsc_main_fops,
	.ioctl_ops = &vsc_main_ioctl_ops,
	.minor = -1,
	.release = video_device_release,
};

struct video_device *main_vsc_vfd = NULL;//for event use

static int __init vsc_v4l2_init(void)
{
	struct vsc_v4l2_device *main_dev;
	struct video_device *main_vfd;
	int ret;
	main_dev = kzalloc(sizeof(*main_dev), GFP_KERNEL);
	if (!main_dev){
		printk(KERN_ERR "#####[%s(%d)]\n", __func__, __LINE__);
		return -ENOMEM;
	}

	strcpy(main_dev->v4l2_dev.name, V4L2_EXT_DEV_PATH_SCALER0);
	
	ret = v4l2_device_register(NULL, &main_dev->v4l2_dev);
	if (ret){
		printk(KERN_ERR "#####[%s(%d)]\n", __func__, __LINE__);
		goto free_dev;
	}


	main_vfd = video_device_alloc();
	if (!main_vfd) {
		v4l2_err(&main_dev->v4l2_dev, "Failed to allocate video device\n");
		ret = -ENOMEM;
		goto unreg_dev;
	}

	//mutex
	mutex_init(&main_dev->lock);
	main_vfd->lock = &main_dev->lock;

	
	//*main_vfd = vsc_main_v4l2_tmplate;
    memcpy(main_vfd->name, vsc_main_v4l2_tmplate.name, 32);
    main_vfd->fops = vsc_main_v4l2_tmplate.fops;
    main_vfd->ioctl_ops = vsc_main_v4l2_tmplate.ioctl_ops;
    main_vfd->minor = vsc_main_v4l2_tmplate.minor;
    main_vfd->release = video_device_release;

	main_vfd->v4l2_dev = &main_dev->v4l2_dev; /* here set the v4l2_device, you have already registered it */
	ret = video_register_device(main_vfd, VFL_TYPE_GRABBER, V4L2_EXT_DEV_NO_SCALER0);//main scaler name
	if (ret) {
		v4l2_err(&main_dev->v4l2_dev, "Failed to register video device\n");
		goto rel_vdev;
	}

	video_set_drvdata(main_vfd, main_dev);
	snprintf(main_vfd->name, sizeof(main_vfd->name), "%s", vsc_main_v4l2_tmplate.name);
	main_dev->vfd = main_vfd;
	/* the debug message*/
   	 v4l2_info(&main_dev->v4l2_dev, "V4L2 device registered as %s\n", video_device_node_name(main_vfd));

	spin_lock_init(&main_vfd->fh_lock);
	main_vsc_vfd = main_vfd;
	
	
	return 0;


rel_vdev:
	video_device_release(main_vfd);
unreg_dev:
	v4l2_device_unregister(&main_dev->v4l2_dev);
free_dev:
	kfree(main_dev);

	return ret;
}



static void __exit vsc_v4l2_exit(void)
{
	return;
}

module_init(vsc_v4l2_init);

module_exit(vsc_v4l2_exit);

/*=========================================================================================
									sub scaler for v4l2 driver 
 ==========================================================================================*/

#define VSC_V4L2_SUB_DEVICE_NAME "video31" //sub scaler

static ssize_t vsc_v4l2_sub_read(struct file *file, char __user *data, size_t count, loff_t *ppos)
{	
	return 0;
}

static unsigned int vsc_v4l2_sub_poll(struct file *file, poll_table *wait)
{
	poll_wait(file, &sub_vsc_read_wait, wait);

	if(SUB_POLL_EVENT_VALUE)
	{
		Set_poll_event(SLR_SUB_DISPLAY, 1 , 0);
		return POLLIN;
	}
	return 0;
}

static int vsc_v4l2_sub_mmap(struct file *file, struct vm_area_struct *vma)
{	
       return 0;
}



static const struct v4l2_ioctl_ops vsc_sub_ioctl_ops = {
    .vidioc_s_ext_ctrls = vsc_v4l2_ioctl_s_ext_ctrls,
    .vidioc_g_ext_ctrls = vsc_v4l2_ioctl_g_ext_ctrls,
    .vidioc_s_ctrl = vsc_v4l2_ioctl_s_ctrl,
    .vidioc_g_ctrl = vsc_v4l2_ioctl_g_ctrl, 
    .vidioc_subscribe_event = vsc_v4l2_sub_subscribe_event,
    .vidioc_unsubscribe_event = vsc_v4l2_sub_event_unsubscribe,
    .vidioc_querycap = vsc_sub_v4l2_ioctl_querycap,

};

static const struct v4l2_file_operations vsc_sub_fops = {
	.owner		= THIS_MODULE,
	.open		= vsc_v4l2_mainorsub_open,
	.release		= vsc_v4l2_mainorsub_release,
	.read		= vsc_v4l2_sub_read,
	.poll			= vsc_v4l2_sub_poll,
	.unlocked_ioctl  = video_ioctl2, /* V4L2 ioctl handler */
	.mmap		= vsc_v4l2_sub_mmap,
};

static struct video_device vsc_sub_v4l2_tmplate = {
	.name = VSC_V4L2_SUB_DEVICE_NAME,
	.fops	= &vsc_sub_fops,
	.ioctl_ops = &vsc_sub_ioctl_ops,
	.minor = -1,
	.release = video_device_release,
};

struct video_device *sub_vsc_vfd = NULL;//for event use
static int __init vscsub_v4l2_init(void)
{
	struct vsc_v4l2_device *sub_dev;
	struct video_device *sub_vfd;
	int ret;
	sub_dev = kzalloc(sizeof(*sub_dev), GFP_KERNEL);
	if (!sub_dev){
		printk(KERN_ERR "#####[%s(%d)]\n", __func__, __LINE__);
		return -ENOMEM;
	}

	strcpy(sub_dev->v4l2_dev.name, V4L2_EXT_DEV_PATH_SCALER1);
	
	ret = v4l2_device_register(NULL, &sub_dev->v4l2_dev);
	if (ret){
		printk(KERN_ERR "#####[%s(%d)]\n", __func__, __LINE__);
		goto free_dev;
	}


	sub_vfd = video_device_alloc();
	if (!sub_vfd) {
		v4l2_err(&sub_dev->v4l2_dev, "Failed to allocate video device\n");
		ret = -ENOMEM;
		goto unreg_dev;
	}

	
	//mutex
	mutex_init(&sub_dev->lock);
	sub_vfd->lock = &sub_dev->lock;

	
	//*sub_vfd = vsc_sub_v4l2_tmplate;
    memcpy(sub_vfd->name, vsc_sub_v4l2_tmplate.name, 32);
    sub_vfd->fops = vsc_sub_v4l2_tmplate.fops;
    sub_vfd->ioctl_ops = vsc_sub_v4l2_tmplate.ioctl_ops;
    sub_vfd->minor = vsc_sub_v4l2_tmplate.minor;
    sub_vfd->release = video_device_release;

	sub_vfd->v4l2_dev = &sub_dev->v4l2_dev; /* here set the v4l2_device, you have already registered it */
	ret = video_register_device(sub_vfd, VFL_TYPE_GRABBER, V4L2_EXT_DEV_NO_SCALER1);//sub scaler name
	if (ret) {
		v4l2_err(&sub_dev->v4l2_dev, "Failed to register video device\n");
		goto rel_vdev;
	}

	video_set_drvdata(sub_vfd, sub_dev);
	snprintf(sub_vfd->name, sizeof(sub_vfd->name), "%s", vsc_sub_v4l2_tmplate.name);
	sub_dev->vfd = sub_vfd;
	/* the debug message*/
   	 v4l2_info(&sub_dev->v4l2_dev, "V4L2 device registered as %s\n", video_device_node_name(sub_vfd));
	spin_lock_init(&sub_vfd->fh_lock);
	sub_vsc_vfd = sub_vfd;
	return 0;


rel_vdev:
	video_device_release(sub_vfd);
unreg_dev:
	v4l2_device_unregister(&sub_dev->v4l2_dev);
free_dev:
	kfree(sub_dev);

	return ret;
}


static void __exit vscsub_v4l2_exit(void)
{
	return;
}


module_init(vscsub_v4l2_init);

module_exit(vscsub_v4l2_exit);


MODULE_LICENSE("GPL");
