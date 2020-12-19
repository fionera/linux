/*
 *      vt_v4l2_driver.c  -- 
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
#include <rbus/ppoverlay_reg.h>

#include <linux/mm.h>  //remap_pfn_range

//common
#include <ioctrl/scaler/vt_cmd_id.h>

#include <linux/v4l2-ext/v4l2-controls-ext.h>
#include <linux/v4l2-ext/videodev2-ext.h>
#include <tvscalercontrol/scaler_v4l2/vt_v4l2_api.h>
#include <common/include/scaler/scalerCommon.h>  //rtk_kdriver/
#include <tvscalercontrol/scaler/scalerstruct.h>

#define VT_V4L2_MAIN_DEVICE_NAME "video60" 

static unsigned char vt_v4l2_open_flag = FALSE;

extern unsigned char get_svp_protect_status(void);
extern unsigned char get_vt_function(void);
extern unsigned char do_vt_reqbufs(unsigned int buf_cnt);
extern unsigned char do_vt_streamoff(void);
/*-----------------v4l2_file_operations--------------------------*/

static int vt_v4l2_main_open(struct file *file)
{				
	if(FALSE == vt_v4l2_open_flag)
	{
		printk(KERN_NOTICE "[VT]fun:%s\n",__FUNCTION__);

#if 0		
		if ((Get_DisplayMode_Src(SLR_MAIN_DISPLAY) == VSC_INPUTSRC_VDEC)
		/*&&(Scaler_DispGetInputInfoByDisp(SLR_MAIN_DISPLAY,SLR_INPUT_STATE) ==  _MODE_STATE_ACTIVE)*/)
		{
			if(get_vdec_securestatus() == TRUE)
			{
				printk(KERN_ERR "fun:%s, [VT]unsupport VT capture in SVP video\n",__FUNCTION__);
				return	EACCES;
			}
		}
#endif
		if(get_svp_protect_status() == TRUE)
		{
			printk(KERN_ERR "fun:%s, [VT]unsupport VT capture in SVP video\n",__FUNCTION__);
			return -EACCES;
		}
		vt_v4l2_open_flag = TRUE;
		return 0;
	}
	else
	{
		printk(KERN_ERR "fun:%s, [VT]v4l2_vt device has open,don't open again\n",__FUNCTION__);
		return -EFAULT;   
	}
	
}


static int vt_v4l2_main_release(struct file *file)
{
	if(TRUE == vt_v4l2_open_flag)
	{
		if(get_vt_function() == TRUE)
		{
			printk(KERN_NOTICE "[VT]stream off and release memory before close vt\n");
			do_vt_streamoff();
			do_vt_reqbufs(0);
		}
		vt_v4l2_open_flag = FALSE;
		printk(KERN_NOTICE "[VT]fun:%s\n",__FUNCTION__);
		return 0;
	}
	else
	{
		printk(KERN_ERR "fun:%s, [VT]v4l2_vt device has release,don't release again\n",__FUNCTION__);
		return -EFAULT;   
	}
}


static ssize_t vt_v4l2_main_read(struct file *file, char __user *data, size_t count, loff_t *ppos)
{	
	return 0;
}

static unsigned int vt_v4l2_main_poll(struct file *file, poll_table *wait)
{
	return 0;
}

static int vt_v4l2_main_mmap(struct file *file, struct vm_area_struct *vma)
{	//	mapbuf = (char *)kmalloc(_ALIGN(1920 * 1080,__4KPAGE), GFP_KERNEL);
	vma->vm_flags |= VM_IO;
	//vma->vm_flags |= VM_RESERVED;
	if(remap_pfn_range(vma,
			vma->vm_start,
			vma->vm_pgoff,
			vma->vm_end - vma->vm_start,
			vma->vm_page_prot))	  
	{
		return -EAGAIN;
	}
	return 0;
}



static const struct v4l2_ioctl_ops vt_main_ioctl_ops = {

	/*-----3 Extended control information for implementation-----*/
    .vidioc_s_ext_ctrls = vt_v4l2_ioctl_s_ext_ctrls,
    .vidioc_g_ext_ctrls = vt_v4l2_ioctl_g_ext_ctrls,
    .vidioc_s_ctrl = vt_v4l2_ioctl_s_ctrl,
    .vidioc_g_ctrl = vt_v4l2_ioctl_g_ctrl,
    .vidioc_subscribe_event = vt_v4l2_ioctl_subscribe_event,
	.vidioc_unsubscribe_event = vt_v4l2_ioctl_unsubscribe_event,
	
	/*-----2 standard control information for implementation-----*/
	.vidioc_querycap = vt_v4l2_ioctl_querycap,
	.vidioc_reqbufs = vt_v4l2_ioctl_reqbufs, /* allocate buffers or release buffers*/
	.vidioc_querybuf = vt_v4l2_ioctl_querybuf,
	.vidioc_qbuf = vt_v4l2_ioctl_qbuf,
	.vidioc_dqbuf = vt_v4l2_ioctl_dqbuf,
	.vidioc_streamon = vt_v4l2_ioctl_streamon,
	.vidioc_streamoff = vt_v4l2_ioctl_streamoff,
	.vidioc_g_fmt_vid_cap = vt_v4l2_ioctl_g_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap = vt_v4l2_ioctl_s_fmt_vid_cap,

	.vidioc_g_fmt_vid_cap_mplane = vt_v4l2_ioctl_g_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap_mplane = vt_v4l2_ioctl_s_fmt_vid_cap,
};

static const struct v4l2_file_operations vt_main_fops = {
	.owner		= THIS_MODULE,
	.open		= vt_v4l2_main_open,
	.release		= vt_v4l2_main_release,
	.read		= vt_v4l2_main_read,
	.poll			= vt_v4l2_main_poll,
	.unlocked_ioctl  = video_ioctl2, /* V4L2 ioctl handler */
	.mmap		= vt_v4l2_main_mmap,
};

static struct video_device vt_main_v4l2_tmplate = {
	.name = VT_V4L2_MAIN_DEVICE_NAME,
	.fops	= &vt_main_fops,
	.ioctl_ops = &vt_main_ioctl_ops,
	.minor = -1,
	.release = video_device_release,
};

static int __init vt_v4l2_init(void)
{
	struct vt_v4l2_device *main_dev;
	struct video_device *main_vfd;
	int ret;
	main_dev = kzalloc(sizeof(*main_dev), GFP_KERNEL);
	if (!main_dev){
		printk(KERN_ERR "#####[%s(%d)]\n", __func__, __LINE__);
		return -ENOMEM;
	}

	strcpy(main_dev->v4l2_dev.name, V4L2_EXT_DEV_PATH_CAPTURE);
	
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
	
    memcpy(main_vfd->name, vt_main_v4l2_tmplate.name, 32);
    main_vfd->fops = vt_main_v4l2_tmplate.fops;
    main_vfd->ioctl_ops = vt_main_v4l2_tmplate.ioctl_ops;
    main_vfd->minor = vt_main_v4l2_tmplate.minor;
    main_vfd->release = video_device_release;
	main_vfd->vfl_dir = VFL_DIR_RX;
	main_vfd->v4l2_dev = &main_dev->v4l2_dev; /* here set the v4l2_device, you have already registered it */
	ret = video_register_device(main_vfd, VFL_TYPE_GRABBER, V4L2_EXT_DEV_NO_CAPTURE);//main scaler name
	if (ret) {
		v4l2_err(&main_dev->v4l2_dev, "Failed to register video device\n");
		goto rel_vdev;
	}

	video_set_drvdata(main_vfd, main_dev);
	snprintf(main_vfd->name, sizeof(main_vfd->name), "%s", vt_main_v4l2_tmplate.name);
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



static void __exit vt_v4l2_exit(void)
{
	return;
}

module_init(vt_v4l2_init);

module_exit(vt_v4l2_exit);

MODULE_LICENSE("GPL");
