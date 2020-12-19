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

#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <linux/mm.h>

#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-ctrls.h>
#include <media/videobuf2-v4l2.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-dma-contig.h>

#pragma scalar_storage_order big-endian
#include <video_bufferpool.h>
#pragma scalar_storage_order default

#include "vcodec_conf.h"
#include "vcodec_def.h"
#include "vcodec_struct.h"
#include "vcodec_utils.h"
#include "vcap_core.h"

#include "videobuf2-dma-carveout.h"
#include "vutils/vdma_carveout.h"


/* support 4 input, this can be changed */
#define MAX_CAP_INS 4
#define MIN_CAP_BUFS 4
#define MIN_CAP_WIDTH  64
#define MIN_CAP_HEIGHT 64
#define MAX_CAP_WIDTH  1920
#define MAX_CAP_HEIGHT 1088


static int vcap_queue_setup(struct vb2_queue *q, const void *parg,
			   unsigned int *num_buffers, unsigned int *num_planes,
			   unsigned int sizes[], void *alloc_ctxs[])
{
	int i, ret;
	vcap_handle_t *handle;

	handle = vb2_get_drv_priv(q);

	
    printk("+++%s:%d queue type %d nBufs %d\n", __FUNCTION__, __LINE__, q->type, *num_buffers);
	if (q->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		*num_planes = handle->pixfmt.num_planes;
		ret = vcodec_initVB2AllocatorContext(handle->device, handle->alloc_ctx, *num_planes);
		if (ret)
		{
			printk("%s: error line %d \n", __FUNCTION__, __LINE__);
		   return ret;
		}	
		for (i=0; i<*num_planes; i++) {
			sizes[i] = handle->pixfmt.plane_fmt[i].sizeimage;
			alloc_ctxs[i] = handle->alloc_ctx[i];
		}
	}
    printk("---%s:%d \n", __FUNCTION__, __LINE__);
	return 0;
}

static int vcap_buf_init(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf;
	vcap_handle_t *handle;
	vcodec_buffer_t *buf;

	handle = vb2_get_drv_priv(vb->vb2_queue);
	vbuf = to_vb2_v4l2_buffer(vb);
	buf = container_of(vbuf, vcodec_buffer_t, vb);
    
    printk("+++%s:%d \n", __FUNCTION__, __LINE__);

	if (handle->pixfmt.pixelformat == V4L2_PIX_FMT_NV12 || handle->pixfmt.pixelformat == V4L2_PIX_FMT_NV12M) {
		buf->dma[0].size = vb2_plane_size(vb, 0);
		buf->dma[0].addr = vb2_dma_contig_plane_dma_addr(vb, 0);
		
		if(handle->pixfmt.pixelformat == V4L2_PIX_FMT_NV12M){
			buf->dma[1].size = vb2_plane_size(vb, 1);
			buf->dma[1].addr = vb2_dma_contig_plane_dma_addr(vb, 1);		
		}
				
		buf->bufW = handle->pixfmt.width;  /* XXX: required by VBM AddBuf operations */
		buf->bufH = handle->pixfmt.height; /* XXX: required by VBM AddBuf operations */
	}

	list_add_tail(&buf->reg_list, &handle->reg_buf_list);

	dev_info(handle->dev, "dma[0] addr 0x%08x size %d\n", buf->dma[0].addr, buf->dma[0].size);
    dev_info(handle->dev, "dma[1] addr 0x%08x size %d\n", buf->dma[1].addr, buf->dma[1].size);
    printk("---%s:%d \n", __FUNCTION__, __LINE__);
	return 0;
}

static int vcap_start_streaming(struct vb2_queue *q, unsigned int count)
{
	vcap_handle_t *handle;
    pr_err("%s:%d \n", __FUNCTION__, __LINE__);
	handle = vb2_get_drv_priv(q);
    pr_err("%s:%d handle \n", __FUNCTION__, __LINE__);
	return vcap_streamon(handle);
}

static void vcap_stop_streaming(struct vb2_queue *q)
{
	vcap_handle_t *handle;
    printk("+++%s:%d \n", __FUNCTION__, __LINE__);
	handle = vb2_get_drv_priv(q);
    printk("---%s:%d \n", __FUNCTION__, __LINE__);
	vcap_streamoff(handle);
	//vb2_core_streamoff(q, q->type);
}

static void vcap_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf;
	vcap_handle_t *handle;
	vcodec_buffer_t *buf;

	handle = vb2_get_drv_priv(vb->vb2_queue);
	vbuf = to_vb2_v4l2_buffer(vb);
	buf = container_of(vbuf, vcodec_buffer_t, vb);
    //printk("+++%s:%d \n", __FUNCTION__, __LINE__);
	mutex_lock(&handle->thread_lock);

	if (vb->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		vcap_qbuf(handle, buf);
    //printk("---%s:%d \n", __FUNCTION__, __LINE__);
	mutex_unlock(&handle->thread_lock);
}

static const struct vb2_ops vcap_qops = {
	.queue_setup = vcap_queue_setup,
	.buf_init = vcap_buf_init,
	.start_streaming = vcap_start_streaming,
	.stop_streaming = vcap_stop_streaming,
	.buf_queue = vcap_buf_queue,
};

static int queue_init(void *priv, struct vb2_queue *out, struct vb2_queue *cap)
{
	vcap_handle_t *handle = priv;
	int ret;
    printk("+++%s:%d \n", __FUNCTION__, __LINE__);
	out->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	out->io_modes = VB2_MMAP | VB2_DMABUF;
	out->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	out->ops = &vcap_qops;
	out->mem_ops = &vb2_dma_contig_memops;
	out->drv_priv = handle;
	out->buf_struct_size = sizeof(vcodec_buffer_t);
	out->allow_zero_bytesused = 1;
	out->min_buffers_needed = MIN_CAP_BUFS;
	ret = vb2_queue_init(out);
	if (ret)
	{
	    printk("%s: error line %d \n", __FUNCTION__, __LINE__);	
		return ret;
    }
	cap->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	cap->io_modes = VB2_MMAP | VB2_DMABUF;
	cap->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	cap->ops = &vcap_qops;
	//use vb2_dma_carveout_memops	
	cap->mem_ops = &vb2_dma_carveout_memops;//(handle->device->dma_carveout_type)? &vb2_dma_carveout_memops : &vb2_dma_contig_memops;
	cap->drv_priv = handle;
	cap->buf_struct_size = sizeof(vcodec_buffer_t);
	cap->allow_zero_bytesused = 1;
	cap->min_buffers_needed = MIN_CAP_BUFS;
	cap->lock = &handle->queue_lock;
	ret = vb2_queue_init(cap);
	if (ret) {
		vb2_queue_release(out);
		printk("%s: error line %d \n", __FUNCTION__, __LINE__);
		return ret;
	}
    printk("---%s:%d \n", __FUNCTION__, __LINE__);
	return 0;
}

static int vcap_ioctl_reqbufs(struct file *file, void *priv,
				struct v4l2_requestbuffers *rb)
{
	int ret;
	if(rb->count > 0  && rb->count < MIN_CAP_BUFS )
	{
		pr_err("%s:%d reqbuf < 4 and not zero\n", __FUNCTION__, rb->count);
		return -ENOMEM;
	}	
	printk("+++%s bufcnt %d\n",__func__, rb->count);
    ret = v4l2_m2m_ioctl_reqbufs(file, priv, rb);
    printk("---%s ret %d %d\n",__func__, ret, rb->count);
	return ret;
}

/* NOTE: unused */
static void vcap_m2m_device_run(void *priv)
{
	vcap_handle_t *handle = priv;
	dev_info(handle->dev, "m2m device run\n");
}

static void vcap_m2m_job_abort(void *priv)
{
	vcap_handle_t *handle = priv;

	dev_info(handle->dev, "m2m job abort\n");
	v4l2_m2m_job_finish(handle->m2m_dev, handle->m2m_ctx);
}

static const struct v4l2_m2m_ops vcap_m2m_ops = {
	.device_run = vcap_m2m_device_run,
	.job_abort = vcap_m2m_job_abort,
};

static int fops_open(struct file *filp)
{
	int ret = 0;
	vcodec_device_t *device;
	vcap_handle_t *handle;

	device = video_drvdata(filp);

	if (mutex_lock_interruptible(&device->lock))
		return -ERESTARTSYS;

	handle = vzalloc(sizeof(vcap_handle_t));
	if (!handle) {
		ret = -ENOMEM;
		goto err;
	}

	handle->device = device;
	handle->dev = device->dev;

	handle->m2m_dev = v4l2_m2m_init(&vcap_m2m_ops);
	if (IS_ERR(handle->m2m_dev)) {
		ret = PTR_ERR(handle->m2m_dev);
		handle->m2m_dev = NULL;
		goto err;
	}

	handle->m2m_ctx = v4l2_m2m_ctx_init(handle->m2m_dev, handle, queue_init);
	if (IS_ERR(handle->m2m_ctx)) {
		ret = PTR_ERR(handle->m2m_ctx);
		handle->m2m_ctx = NULL;
		goto err;
	}

	filp->private_data = &handle->fh;
	v4l2_fh_init(&handle->fh, &device->vdev);
	v4l2_fh_add(&handle->fh);
	handle->fh.m2m_ctx = handle->m2m_ctx; //> for m2m
	list_add(&handle->list, &device->handle_list);
	mutex_init(&handle->queue_lock);

	vcap_initHandle(handle);

	mutex_unlock(&device->lock);

	return 0;

err:
	if (handle) {
		if (handle->m2m_ctx) v4l2_m2m_ctx_release(handle->m2m_ctx);
		if (handle->m2m_dev) v4l2_m2m_release(handle->m2m_dev);
		vfree(handle);
	}
	mutex_unlock(&device->lock);
	return ret;
}

static int fops_release(struct file *filp)
{
	struct v4l2_fh *fh;
	vcodec_device_t *device;
	vcap_handle_t *handle;

	fh = filp->private_data;
	device = video_drvdata(filp);
	handle = container_of(fh, vcap_handle_t, fh);

	mutex_lock(&device->lock);
	vcap_exitHandle(handle);
	list_del(&handle->list);
	v4l2_fh_del(fh);
	v4l2_fh_exit(fh);
	v4l2_m2m_ctx_release(handle->m2m_ctx);
	v4l2_m2m_release(handle->m2m_dev);
	vcodec_exitVB2AllocatorContext(handle->device, handle->alloc_ctx, VIDEO_MAX_PLANES);
	vfree(handle);
	mutex_unlock(&device->lock);

	return 0;
}

static int vcap_ioctl_querycap(struct file *file, void *fh, struct v4l2_capability *cap)
{
	snprintf(cap->driver, sizeof(cap->driver), "%s", VCAP_V4L2_NAME);
	snprintf(cap->card, sizeof(cap->card), "platform:%s", VCAP_V4L2_NAME);
	snprintf(cap->bus_info, sizeof(cap->bus_info), "platform:%s", VCAP_V4L2_NAME);

	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

	return 0;
}

static int vcap_ioctl_queryctrl(struct file *file, void *fh, struct v4l2_queryctrl *ctrl)
{
    pr_err("~~~%s id %d\n", __FUNCTION__, ctrl->id);
    switch(ctrl->id)
    {
        case V4L2_CID_MIN_BUFFERS_FOR_CAPTURE:
        ctrl->minimum = MIN_CAP_BUFS;
        break;

        default:
        pr_err("~~~%s id %d not handle\n", __FUNCTION__, ctrl->id);
        return  -EINVAL;
    }
    return 0;
}

static int vcap_ioctl_querymenu(struct file *file, void *fh, struct v4l2_querymenu *menu)
{
    pr_err("~~~%s id %d\n", __FUNCTION__, menu->id);
    switch(menu->id)
    {
        case V4L2_CID_MIN_BUFFERS_FOR_CAPTURE:
        menu->value = MIN_CAP_BUFS;
        break;

        default:
        pr_err("~~~%s id %d not handle\n", __FUNCTION__, menu->id);
        return  -EINVAL;
    }
    return 0;
}



static int vcap_ioctl_enum_fmt_vid_cap_mplane(struct file *file, void *fh, struct v4l2_fmtdesc *f)
{
	const vcodec_pixfmt_t *pixfmt;

	if (!(pixfmt = vcap_getPixelFormatByIndex(f->index)))
		return -EINVAL;

	f->pixelformat = pixfmt->pixelformat;

	return 0;
}

static int vcap_ioctl_enum_input(struct file *file, void *fh, struct v4l2_input *f)
{  
	printk("%s: index %d \n", __FUNCTION__, f->index);
  if(f->index > MAX_CAP_INS - 1)
   return -EINVAL;    

  return 0;
}

static int vcap_ioctl_enum_framesizes(struct file *file, void *fh, struct v4l2_frmsizeenum *f)
{
    pr_err("~~~%s \n", __FUNCTION__);
    if(!f->index)
    {
        /* capture framesize min 16x16, max 1920x1080, step 16x8, it can be changed */        
        f->type = V4L2_FRMSIZE_TYPE_STEPWISE;

        f->stepwise.min_width  = MIN_CAP_WIDTH  ;
        f->stepwise.max_width  = MAX_CAP_WIDTH;
        f->stepwise.step_width = 1  ;
        f->stepwise.min_height = MIN_CAP_HEIGHT  ;
        f->stepwise.max_height = MAX_CAP_HEIGHT;
        f->stepwise.step_height= 1   ;
    }else
        return -EINVAL;   
    
  return 0;
}


static int vcap_ioctl_enum_frameintervals(struct file *file, void *fh, struct v4l2_frmivalenum *f)
{
    pr_err("~~~%s \n", __FUNCTION__);
    if(!f->index)
    {
        /* capture min 15 fps, max 60 fps, step 15 fps, it can be changed */
        f->type = V4L2_FRMIVAL_TYPE_STEPWISE ;
        f->stepwise.min.numerator   = 1 ;
        f->stepwise.min.denominator = 15; 
        f->stepwise.max.numerator   = 1 ;
        f->stepwise.max.denominator = 60;
        f->stepwise.step.numerator  = 1 ;
        f->stepwise.step.denominator= 15;
    }else
        return -EINVAL;

  return 0;  
}

static int vcap_ioctl_g_fmt_vid_cap_mplane(struct file *file, void *fh, struct v4l2_format *f)
{
	struct v4l2_fh *vfh;
	struct v4l2_pix_format_mplane *mp;
	vcap_handle_t *handle;

	vfh = file->private_data;
	handle = container_of(vfh, vcap_handle_t, fh);
	mp = &f->fmt.pix_mp;

	memcpy(mp, &handle->pixfmt, sizeof(*mp));

	return 0;
}

static int vcap_ioctl_try_fmt_vid_cap_mplane(struct file *file, void *fh, struct v4l2_format *f)
{
	struct v4l2_pix_format_mplane *mp;

	mp = &f->fmt.pix_mp;
	if (!vcap_getPixelFormat(mp->pixelformat))
		return -EINVAL;

	return 0;
}

static int vcap_ioctl_s_fmt_vid_cap_mplane(struct file *file, void *fh, struct v4l2_format *f)
{
	int ret;
	struct v4l2_fh *vfh;
	struct v4l2_pix_format_mplane *mp;
	vcap_handle_t *handle;

	vfh = file->private_data;
	handle = container_of(vfh, vcap_handle_t, fh);
	mp = &f->fmt.pix_mp;
        
    if(mp->width < MIN_CAP_WIDTH || mp->width > MAX_CAP_WIDTH || mp->height < MIN_CAP_HEIGHT || mp->height > MAX_CAP_HEIGHT)
    {
       pr_err("~~ %s:invalid w/h %d %d\n", __FUNCTION__, mp->width, mp->height);
       return -EINVAL;
    }
  	ret = vcap_setFormat(mp, mp->pixelformat, mp->width, mp->height);
	if (ret) return ret;

	memcpy(&handle->pixfmt, mp, sizeof(*mp));

	return 0;
}



static int vcap_ioctl_s_input(struct file *file, void *priv, unsigned int i)
{
	struct v4l2_fh *vfh;
	vcap_handle_t *handle;
#ifndef GPSC_ALONE
	vdec_handle_t *vdec;
#endif
   if(i > MAX_CAP_INS - 1)
   {	
   	 pr_err("~~ %s:invalid input %d \n", __FUNCTION__, i);
     return -EINVAL;    
   }

	vfh = file->private_data;
	handle = container_of(vfh, vcap_handle_t, fh);
	handle->bInputIdx = 1;
	handle->inputIdx = i;
#ifndef GPSC_ALONE
	/* init vcap format based on vdec format */
	if (!handle->pixfmt.width || !handle->pixfmt.height) {
		vcodec_lockDecDevice(handle->device);
		vdec = vcodec_getDecByOutputIdx(handle->device, handle->inputIdx);
		if (vdec) {
			vcap_setFormat(&handle->pixfmt, V4L2_PIX_FMT_NV12,
				vdec->pixfmt_out.width, vdec->pixfmt_out.height);
		}
		vcodec_unlockDecDevice(handle->device);
	}
#endif
	pr_err("~~gpsc %s:index %d\n", __FUNCTION__, i);

	return 0;
}


static int vcap_ioctl_g_input(struct file *file, void *priv, unsigned int *i)
{
    struct v4l2_fh *vfh;
    vcap_handle_t *handle;

    vfh = file->private_data;
    handle = container_of(vfh, vcap_handle_t, fh);

    *i = handle->inputIdx;
    dev_info(handle->dev, "g input %d\n", *i);
    return 0;
}

static int vcap_ioctl_s_ctrl(struct file *file, void *fh, struct v4l2_control *ctrl)
{
    int ret=0;
    
    struct v4l2_fh *vfh;
    vcap_handle_t *handle;

    printk("+++%s:ctrl id %d \n", __FUNCTION__, ctrl->id);    

    vfh = file->private_data;
    handle = container_of(vfh, vcap_handle_t, fh);
    switch(ctrl->id)
    {
    	case V4L2_CID_EXT_GPSCALER_MAX_FRAME_SIZE:
        handle->max_width = (unsigned int)(ctrl->value>>16 & 0xffff);
        handle->max_height = (unsigned int)(ctrl->value & 0xffff);
        printk("+++ max w/h %d %d\n",handle->max_width,handle->max_height );
        break;

        default:
        pr_err("%s:ctrl id %d not handle\n", __FUNCTION__, ctrl->id);
        ret = -EINVAL;
        break;    	
    }
    printk("---%s:ret %d \n", __FUNCTION__, ret);    
    return ret;    	
}


static int vcap_ioctl_g_ctrl(struct file *file, void *fh, struct v4l2_control *ctrl)
{
    int ret=0;
    
    switch(ctrl->id)
    {
        case V4L2_CID_MIN_BUFFERS_FOR_CAPTURE:
        ctrl->value =  MIN_CAP_BUFS;
        break;

        default:
        pr_err("%s:ctrl id %d not handle\n", __FUNCTION__, ctrl->id);
        ret = -EINVAL;
        break;
    }

    return ret;
}


static int vcap_ioctl_s_parm(struct file *file, void *fh, struct v4l2_streamparm *s)
{
    struct v4l2_fh *vfh;
    vcap_handle_t *handle;

    vfh = file->private_data;
    handle = container_of(vfh, vcap_handle_t, fh);
    if(s->type==V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
    {
        struct v4l2_captureparm *pCap=&handle->cap_parm; 
        memcpy(&handle->cap_parm, (struct v4l2_captureparm*)&s->parm, sizeof(struct v4l2_captureparm));

        pr_err("capability %d capturemode %d timeperframe %d %d\n",pCap->capability, pCap->capturemode, pCap->timeperframe.numerator, pCap->timeperframe.denominator);
        pr_err("extendedmode %d readbuffers %d\n",pCap->extendedmode, pCap->readbuffers);
    }
    return 0;
}

static int vcap_ioctl_g_parm(struct file *file, void *fh, struct v4l2_streamparm *g)
{
    struct v4l2_fh *vfh;
    vcap_handle_t *handle;

    vfh = file->private_data;
    handle = container_of(vfh, vcap_handle_t, fh);
    if(g->type==V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
    {
        struct v4l2_captureparm *pCap=&handle->cap_parm;
        memcpy((struct v4l2_captureparm*)&g->parm, &handle->cap_parm ,sizeof(struct v4l2_captureparm));

        pr_err("capability %d capturemode %d timeperframe %d %d\n",pCap->capability, pCap->capturemode, pCap->timeperframe.numerator, pCap->timeperframe.denominator);
        pr_err("extendedmode %d readbuffers %d\n",pCap->extendedmode, pCap->readbuffers);
    }
    return 0;
}


static int vcap_ioctl_g_ext_ctrls(struct file *file, void *fh, struct v4l2_ext_controls *ctrls)
{
	int ret, i;
	struct v4l2_fh *vfh;
	vcap_handle_t *handle;
	struct v4l2_ext_control *ctrl;

	vfh = file->private_data;
	handle = container_of(vfh, vcap_handle_t, fh);
	ret = 0;

	for (i=0; !ret && i<ctrls->count; i++) {
		ctrl = &ctrls->controls[i];
		if (ctrl->id >= V4L2_CID_EXT_VCAP_PICOBJ_IDX0 && ctrl->id <= V4L2_CID_EXT_VCAP_PICOBJ_IDX63) {
			int idx = ctrl->id - V4L2_CID_EXT_VCAP_PICOBJ_IDX0;
			vcodec_buffer_t *buf;
			list_for_each_entry (buf, &handle->reg_buf_list, reg_list) {
				if (buf->vb.vb2_buf.index == idx) {
					if (copy_to_user((void*)ctrl->ptr, &buf->picobj, sizeof(struct v4l2_ext_vcap_picobj)))
						dev_err(handle->dev, "fail to copy picobj\n");
					break;
				}
			}
		}
	}

	return ret;
}

const struct v4l2_file_operations vcap_file_ops = {
	.owner = THIS_MODULE,
	.open = fops_open,
	.release = fops_release,
	.unlocked_ioctl = video_ioctl2,
	.poll = v4l2_m2m_fop_poll,
	.mmap = v4l2_m2m_fop_mmap,
};

const struct v4l2_ioctl_ops vcap_ioctl_ops = {

	.vidioc_querycap = vcap_ioctl_querycap,
    .vidioc_queryctrl= vcap_ioctl_queryctrl,
    .vidioc_querymenu= vcap_ioctl_querymenu,

	.vidioc_enum_fmt_vid_cap_mplane = vcap_ioctl_enum_fmt_vid_cap_mplane,
	.vidioc_g_fmt_vid_cap_mplane = vcap_ioctl_g_fmt_vid_cap_mplane,
	.vidioc_try_fmt_vid_cap_mplane = vcap_ioctl_try_fmt_vid_cap_mplane,
	.vidioc_s_fmt_vid_cap_mplane = vcap_ioctl_s_fmt_vid_cap_mplane,

    .vidioc_enum_input = vcap_ioctl_enum_input,
    .vidioc_enum_framesizes = vcap_ioctl_enum_framesizes,
    .vidioc_enum_frameintervals = vcap_ioctl_enum_frameintervals,

    .vidioc_s_parm = vcap_ioctl_s_parm,
    .vidioc_g_parm = vcap_ioctl_g_parm,
	
    .vidioc_s_input	= vcap_ioctl_s_input,
    .vidioc_g_input = vcap_ioctl_g_input,
    .vidioc_s_ctrl = vcap_ioctl_s_ctrl,
    .vidioc_g_ctrl = vcap_ioctl_g_ctrl,
	.vidioc_g_ext_ctrls = vcap_ioctl_g_ext_ctrls,

	.vidioc_reqbufs	= vcap_ioctl_reqbufs,
	.vidioc_querybuf = v4l2_m2m_ioctl_querybuf,
	.vidioc_create_bufs = v4l2_m2m_ioctl_create_bufs,
	.vidioc_prepare_buf = v4l2_m2m_ioctl_prepare_buf,
	.vidioc_qbuf = v4l2_m2m_ioctl_qbuf,
	.vidioc_expbuf = v4l2_m2m_ioctl_expbuf,
	.vidioc_dqbuf = v4l2_m2m_ioctl_dqbuf,
	.vidioc_streamon = v4l2_m2m_ioctl_streamon,
	.vidioc_streamoff = v4l2_m2m_ioctl_streamoff,

};

