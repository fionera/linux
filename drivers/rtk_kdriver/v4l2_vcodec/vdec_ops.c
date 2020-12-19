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
#include <media/v4l2-event.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-dma-contig.h>

#include "v4l2_vdec_ext.h"

#include "vcodec_conf.h"
#include "vcodec_def.h"
#include "vcodec_macros.h"
#include "vcodec_struct.h"
#include "vcodec_utils.h"
#include "vdec_core.h"
#include "vdec_out.h"
#include "vdec_cap.h"

#include "videobuf2-dma-carveout.h"
#include "vutils/vrpc.h"

static inline int str2int(char *str)
{
	int ret;
	if (str && kstrtoint(str, 0, &ret) == 0)
		return ret;
	return 0;
}

static int vdec_queue_setup(struct vb2_queue *q, const void *parg,
		unsigned int *num_buffers, unsigned int *num_planes,
		unsigned int sizes[], void *alloc_ctxs[])
{
	int i, ret;
	vdec_handle_t *handle;

	handle = vb2_get_drv_priv(q);

	dev_info(handle->dev, "queue type %d nBufs %d\n", q->type, *num_buffers);

	if (q->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		*num_planes = handle->pixfmt_cap.num_planes;
		ret = vcodec_initVB2AllocatorContext(handle->device, handle->alloc_ctx_cap, *num_planes);
		if (ret) return ret;
		for (i=0; i<*num_planes; i++) {
			sizes[i] = handle->pixfmt_cap.plane_fmt[i].sizeimage;
			alloc_ctxs[i] = handle->alloc_ctx_cap[i];
		}
	}

	return 0;
}

static int vdec_start_streaming(struct vb2_queue *q, unsigned int count)
{
	vdec_handle_t *handle;

	handle = vb2_get_drv_priv(q);

	dev_info(handle->dev, "start streaming type %d\n", q->type);

	if (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		handle->streamon_out = 1;
	if (q->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		handle->streamon_cap = 1;

	/* make sure all conditions were fulfilled */
	if (handle->streaming_out && !handle->streamon_out) return 0;
	if (handle->streaming_cap && !handle->streamon_cap) return 0;

	return vdec_streamon(handle);
}

static void vdec_stop_streaming(struct vb2_queue *q)
{
	vdec_handle_t *handle;
	vcodec_buffer_t *buf, *buf_tmp;

	handle = vb2_get_drv_priv(q);

	if (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		handle->streamon_out = 0;
	if (q->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		handle->streamon_cap = 0;

	/* make sure all conditions were fulfilled */
	if (handle->streaming_out && handle->streamon_out) return;
	if (handle->streaming_cap && handle->streamon_cap) return;

	vdec_streamoff(handle);

	list_for_each_entry_safe (buf, buf_tmp, &handle->buf_list, list) {
		list_del(&buf->list);
		vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
	}

}

static void vdec_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf;
	vdec_handle_t *handle;
	vcodec_buffer_t *buf;

	handle = vb2_get_drv_priv(vb->vb2_queue);
	vbuf = to_vb2_v4l2_buffer(vb);
	buf = container_of(vbuf, vcodec_buffer_t, vb);

	mutex_lock(&handle->thread_lock);

	if (vb->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		vdec_cap_qbuf(handle, buf);

	mutex_unlock(&handle->thread_lock);
}

static int vdec_buf_init(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf;
	vdec_handle_t *handle;
	vcodec_buffer_t *buf;

	handle = vb2_get_drv_priv(vb->vb2_queue);
	vbuf = to_vb2_v4l2_buffer(vb);
	buf = container_of(vbuf, vcodec_buffer_t, vb);

	if (handle->pixfmt_cap.pixelformat == V4L2_PIX_FMT_NV12) {
		buf->dma[0].size = vb2_plane_size(vb, 0);
		buf->dma[0].addr = vb2_dma_contig_plane_dma_addr(vb, 0);
		buf->bufW = handle->pixfmt_cap.width;  /* XXX: required by VBM AddBuf operations */
		buf->bufH = handle->pixfmt_cap.height; /* XXX: required by VBM AddBuf operations */
	}

	list_add_tail(&buf->reg_list, &handle->reg_buf_list);

	dev_info(handle->dev, "dma addr 0x%08x size %d\n", buf->dma[0].addr, buf->dma[0].size);

	return 0;
}

static const struct vb2_ops vdec_qops = {
	.queue_setup = vdec_queue_setup,
	.buf_init = vdec_buf_init,
	.start_streaming = vdec_start_streaming,
	.stop_streaming = vdec_stop_streaming,
	.buf_queue = vdec_buf_queue,
};

static int queue_init(void *priv, struct vb2_queue *out, struct vb2_queue *cap)
{
	vdec_handle_t *handle = (vdec_handle_t*)priv;
	int ret;

	out->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	out->io_modes = VB2_MMAP | VB2_DMABUF;
	out->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	out->ops = &vdec_qops;
	out->mem_ops = &vb2_dma_contig_memops;
	out->drv_priv = handle;
	out->buf_struct_size = sizeof(vcodec_buffer_t);
	out->allow_zero_bytesused = 1;
	out->min_buffers_needed = 1;
	ret = vb2_queue_init(out);
	if (ret)
		return ret;

	cap->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	cap->io_modes = VB2_MMAP | VB2_DMABUF;
	cap->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	cap->ops = &vdec_qops;
	//use vb2_dma_carveout_memops
	cap->mem_ops = &vb2_dma_carveout_memops;//(handle->device->dma_carveout_type)? &vb2_dma_carveout_memops : &vb2_dma_contig_memops;
	cap->drv_priv = handle;
	cap->buf_struct_size = sizeof(vcodec_buffer_t);
	cap->allow_zero_bytesused = 1;
	cap->min_buffers_needed = 1;
	ret = vb2_queue_init(cap);
	if (ret) {
		vb2_queue_release(out);
		return ret;
	}

	return 0;
}

/* NOTE: unused */
static void vdec_m2m_device_run(void *priv)
{
	vdec_handle_t *handle = priv;
	dev_info(handle->dev, "m2m device run\n");
}

static void vdec_m2m_job_abort(void *priv)
{
	vdec_handle_t *handle = priv;

	dev_info(handle->dev, "m2m job abort\n");
	v4l2_m2m_job_finish(handle->m2m_dev, handle->m2m_ctx);
}

static const struct v4l2_m2m_ops vdec_m2m_ops = {
	.device_run = vdec_m2m_device_run,
	.job_abort = vdec_m2m_job_abort,
};

static int fops_open(struct file *filp)
{
	int ret = 0;
	vcodec_device_t *device;
	vdec_handle_t *handle;

	device = video_drvdata(filp);

	if (mutex_lock_interruptible(&device->lock))
		return -ERESTARTSYS;

	handle = vzalloc(sizeof(vdec_handle_t));
	if (!handle) {
		ret = -ENOMEM;
		goto err;
	}

	handle->device = device;
	handle->dev = device->dev;

	handle->m2m_dev = v4l2_m2m_init(&vdec_m2m_ops);
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
	vdec_initHandle(handle);

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
	vdec_handle_t *handle;

	fh = filp->private_data;
	device = video_drvdata(filp);
	handle = container_of(fh, vdec_handle_t, fh);
	dev_info(handle->dev, "release[0]\n");
	mutex_lock(&device->lock);
	dev_info(handle->dev, "release[1]\n");
	vdec_exitHandle(handle);
	list_del(&handle->list);
	v4l2_fh_del(fh);
	v4l2_fh_exit(fh);
	v4l2_m2m_ctx_release(handle->m2m_ctx);
	v4l2_m2m_release(handle->m2m_dev);
	vcodec_exitVB2AllocatorContext(handle->device, handle->alloc_ctx_out, VIDEO_MAX_PLANES);
	vcodec_exitVB2AllocatorContext(handle->device, handle->alloc_ctx_cap, VIDEO_MAX_PLANES);
	vfree(handle);
	mutex_unlock(&device->lock);
        printk("~rtk-vcodec vdec: release[E]\n");
	return 0;
}

static void mp2sp(struct v4l2_pix_format *sp, struct v4l2_pix_format_mplane *mp)
{
	sp->width = mp->width;
	sp->height = mp->height;
	sp->pixelformat	= mp->pixelformat;
	sp->field = mp->field;
	sp->bytesperline = mp->plane_fmt[0].bytesperline;
	sp->sizeimage = mp->plane_fmt[0].sizeimage;
	sp->colorspace = mp->colorspace;
	sp->ycbcr_enc = mp->ycbcr_enc;
	sp->quantization = mp->quantization;
	sp->xfer_func = mp->xfer_func;
}

static void sp2mp(struct v4l2_pix_format_mplane *mp, struct v4l2_pix_format *sp)
{
	mp->width = sp->width;
	mp->height = sp->height;
	mp->pixelformat	= sp->pixelformat;
	mp->num_planes = 1;
	mp->field = sp->field;
	mp->plane_fmt[0].bytesperline = sp->bytesperline;
	mp->plane_fmt[0].sizeimage = sp->sizeimage;
	mp->colorspace = sp->colorspace;
	mp->ycbcr_enc = sp->ycbcr_enc;
	mp->quantization = sp->quantization;
	mp->xfer_func = sp->xfer_func;
}

static int user_allocMemory(vdec_handle_t *handle, struct v4l2_ext_vdec_mem_alloc __user *userptr)
{
	int ret;
	struct v4l2_ext_vdec_mem_alloc args;
	vrpc_mresult_t mres;

	if (copy_from_user(&args, userptr, sizeof(args)))
		return -EFAULT;

	if (!args.size)
		return -EFAULT;

	ret = vrpc_malloc(handle->userID, args.type, args.size, args.flags, &mres);
	if (ret) return ret;

	args.phyaddr = mres.phyaddr;

	if (copy_to_user(userptr, &args, sizeof(args)))
		return -EFAULT;

	return 0;
}

static int user_releaseMemory(vdec_handle_t *handle, struct v4l2_ext_vdec_mem_alloc __user *userptr)
{
	int ret;
	struct v4l2_ext_vdec_mem_alloc args;

	if (copy_from_user(&args, userptr, sizeof(args)))
		return -EFAULT;

	if (!args.phyaddr || !args.size)
		return -EFAULT;

	ret = vrpc_free(handle->userID, args.phyaddr);
	if (ret) return ret;

	return 0;
}

static int user_get_stream_info(vdec_handle_t *handle, void __user *userptr)
{
	int ret;
	struct v4l2_ext_stream_info stream_info;

	ret = vdec_getStreamInfo(handle, &stream_info);
	if (ret) {
		dev_err(handle->dev, "fail to get StreamInfo\n");
		return ret;
	}

	ret = copy_to_user(userptr, &stream_info, sizeof(struct v4l2_ext_stream_info));
	if (ret) {
		dev_err(handle->dev, "fail to copy stream_info\n");
		return -EFAULT;
	}

	return 0;
}

static int user_get_decoder_status (vdec_handle_t *handle, void __user *userptr)
{
	int ret;
	struct v4l2_ext_decoder_status dec_stat;

	ret = vdec_getDecoderStatus(handle, &dec_stat);
	if (ret) {
		dev_err(handle->dev, "fail to get DecoderStatus\n");
		return ret;
	}

	ret = copy_to_user(userptr, &dec_stat, sizeof(struct v4l2_ext_decoder_status));
	if (ret) {
		dev_err(handle->dev, "fail to copy dec_stat\n");
		return -EFAULT;
	}

	return 0;
}

static int user_get_video_info(vdec_handle_t *handle, void __user *userptr)
{
	int ret;
	struct v4l2_control ctrl;

	if (copy_from_user((void*)&ctrl, userptr, sizeof(struct v4l2_control)))
		return -EFAULT;

	ret = vdec_getVideoInfo(handle, &ctrl);
	if (ret) {
		dev_err(handle->dev, "fail to get VideoInfo\n");
		return ret;
	}

	if (copy_to_user(userptr, &ctrl, sizeof(struct v4l2_control)))
		return -EFAULT;

	return 0;
}

static int ioctl_querycap(struct file *file, void *fh, struct v4l2_capability *cap)
{
	snprintf(cap->driver, sizeof(cap->driver), "%s", VDEC_V4L2_NAME);
	snprintf(cap->card, sizeof(cap->card), "platform:%s", VDEC_V4L2_NAME);
	snprintf(cap->bus_info, sizeof(cap->bus_info), "platform:%s", VDEC_V4L2_NAME);

	cap->device_caps =
		  V4L2_CAP_VIDEO_OUTPUT_MPLANE
		| V4L2_CAP_VIDEO_CAPTURE_MPLANE
		| V4L2_CAP_STREAMING
		;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

	return 0;
}

static int ioctl_enum_fmt_vid_cap_mplane(struct file *file, void *fh, struct v4l2_fmtdesc *f)
{
	const vcodec_pixfmt_t *pixfmt;

	if (!(pixfmt = vdec_getCapPixelFormatByIndex(f->index)))
		return -EINVAL;

	f->pixelformat = pixfmt->pixelformat;

	return 0;
}

static int ioctl_g_fmt_vid_cap_mplane(struct file *file, void *fh, struct v4l2_format *f)
{
	struct v4l2_fh *vfh;
	struct v4l2_pix_format_mplane *mp;
	vdec_handle_t *handle;

	vfh = file->private_data;
	handle = container_of(vfh, vdec_handle_t, fh);
	mp = &f->fmt.pix_mp;

	memcpy(mp, &handle->pixfmt_cap, sizeof(*mp));

	return 0;
}

static int ioctl_try_fmt_vid_cap_mplane(struct file *file, void *fh, struct v4l2_format *f)
{
	struct v4l2_pix_format_mplane *mp;

	mp = &f->fmt.pix_mp;
	if (!vdec_getCapPixelFormat(mp->pixelformat))
		return -EINVAL;

	return 0;
}

static int ioctl_s_fmt_vid_cap_mplane(struct file *file, void *fh, struct v4l2_format *f)
{
	int ret;
	struct v4l2_fh *vfh;
	struct v4l2_pix_format_mplane *mp;
	vdec_handle_t *handle;

	vfh = file->private_data;
	handle = container_of(vfh, vdec_handle_t, fh);
	mp = &f->fmt.pix_mp;

	ret = vdec_cap_setFormat(mp, mp->pixelformat, mp->width, mp->height);
	if (ret) return ret;

	memcpy(&handle->pixfmt_cap, mp, sizeof(*mp));

	return 0;
}

static int ioctl_enum_fmt_vid_out_mplane(struct file *file, void *fh, struct v4l2_fmtdesc *f)
{
	const vcodec_pixfmt_t *pixfmt;

	if (!(pixfmt = vdec_getOutPixelFormatByIndex(f->index)))
		return -EINVAL;

	f->pixelformat = pixfmt->pixelformat;

	return 0;
}

static int ioctl_g_fmt_vid_out_mplane(struct file *file, void *fh, struct v4l2_format *f)
{
	struct v4l2_fh *vfh;
	struct v4l2_pix_format_mplane *mp;
	vdec_handle_t *handle;

	vfh = file->private_data;
	handle = container_of(vfh, vdec_handle_t, fh);
	mp = &f->fmt.pix_mp;

	memcpy(mp, &handle->pixfmt_out, sizeof(*mp));

	return 0;
}

static int ioctl_try_fmt_vid_out_mplane(struct file *file, void *fh, struct v4l2_format *f)
{
	struct v4l2_pix_format_mplane *mp;

	mp = &f->fmt.pix_mp;
	if (!vdec_getOutPixelFormat(mp->pixelformat))
		return -EINVAL;

	return 0;
}

static int ioctl_s_fmt_vid_out_mplane(struct file *file, void *fh, struct v4l2_format *f)
{
	struct v4l2_fh *vfh;
	struct v4l2_pix_format_mplane *mp;
	vdec_handle_t *handle;
	const vcodec_pixfmt_t *pixfmt;

	vfh = file->private_data;
	handle = container_of(vfh, vdec_handle_t, fh);
	mp = &f->fmt.pix_mp;

	pixfmt = vdec_getOutPixelFormat(mp->pixelformat);
	if (!pixfmt) {
		dev_err(handle->dev, "invalid pixelformat 0x%x\n", mp->pixelformat);
		return -EINVAL;
	}

	mp->num_planes = pixfmt->num_planes;

	memcpy(&handle->pixfmt_out, mp, sizeof(*mp));
	handle->bOutPixFmt = 1;

	return 0;
}

static int ioctl_g_fmt_vid_out(struct file *file, void *fh, struct v4l2_format *f)
{
	struct v4l2_fh *vfh;
	vdec_handle_t *handle;

	vfh = file->private_data;
	handle = container_of(vfh, vdec_handle_t, fh);
	mp2sp(&f->fmt.pix, &handle->pixfmt_out);

	return 0;
}

static int ioctl_try_fmt_vid_out(struct file *file, void *fh, struct v4l2_format *f)
{
	struct v4l2_pix_format *sp;

	sp = &f->fmt.pix;
	if (!vdec_getOutPixelFormat(sp->pixelformat))
		return -EINVAL;

	return 0;
}

static int ioctl_s_fmt_vid_out(struct file *file, void *fh, struct v4l2_format *f)
{
	struct v4l2_fh *vfh;
	struct v4l2_pix_format *sp;
	vdec_handle_t *handle;
	const vcodec_pixfmt_t *pixfmt;

	vfh = file->private_data;
	handle = container_of(vfh, vdec_handle_t, fh);
	sp = &f->fmt.pix;

	pixfmt = vdec_getOutPixelFormat(sp->pixelformat);
	if (!pixfmt || pixfmt->num_planes != 1) {
		dev_err(handle->dev, "invalid pixelformat 0x%x\n", sp->pixelformat);
		return -EINVAL;
	}

	sp2mp(&handle->pixfmt_out, sp);
	handle->bOutPixFmt = 1;

	return 0;
}

static int ioctl_g_ctrl(struct file *file, void *fh, struct v4l2_control *ctrl)
{
	struct v4l2_fh *vfh;
	vdec_handle_t *handle;

	vfh = file->private_data;
	handle = container_of(vfh, vdec_handle_t, fh);

	switch (ctrl->id) {

		case V4L2_CID_EXT_VDEC_DECODER_VERSION:
			ctrl->value = VDEC_VERSION;
			break;

		case V4L2_CID_EXT_VDEC_DISPLAY_DELAY:
			ctrl->value = handle->display_delay;
			break;

		case V4L2_CID_EXT_VDEC_ECP_OFFSET:
			ctrl->value = 0x0;
			break;

		case V4L2_CID_EXT_VDEC_ECP_SIZE:
			if (handle->state == ST_RUN)
				ctrl->value = 0x800000;
			else
				ctrl->value = 0x0;
			break;

		default:
			dev_err(handle->dev, "unknown id 0x%x\n", ctrl->id);
			return -EINVAL;
	}

	return 0;
}

static int ioctl_s_ctrl(struct file *file, void *fh, struct v4l2_control *ctrl)
{
	int ret;
	struct v4l2_fh *vfh;
	vdec_handle_t *handle;

	vfh = file->private_data;
	handle = container_of(vfh, vdec_handle_t, fh);

	switch (ctrl->id) {

		case V4L2_CID_EXT_VDEC_CHANNEL:
			ret = vdec_setChannel(handle, ctrl->value);
			break;

		#ifdef V4L2_CID_EXT_VDEC_VTP_PORT
		case V4L2_CID_EXT_VDEC_VTP_PORT:
			ret = vdec_setVTPPort(handle, ctrl->value);
			break;
		#endif

		case V4L2_CID_EXT_VDEC_RESETTING:
			ret = vdec_reset(handle);
			break;

		case V4L2_CID_EXT_VDEC_AV_SYNC:
			ret = vdec_setAVSync(handle, ctrl->value);
			break;

		case V4L2_CID_EXT_VDEC_FRAME_ADVANCE:
			ret = vdec_setFrameAdvance(handle, ctrl->value);
			break;

		case V4L2_CID_EXT_VDEC_DECODING_SPEED:
			ret = vdec_setSpeed(handle, ctrl->value);
			break;

		case V4L2_CID_EXT_VDEC_DECODE_MODE:
			ret = vdec_setDecMode(handle, ctrl->value);
			break;

		case V4L2_CID_EXT_VDEC_DISPLAY_DELAY:
			ret = vdec_setDisplayDelay(handle, ctrl->value);
			break;

		case V4L2_CID_EXT_VDEC_KR_DUAL_3D_MODE:
			ret = vdec_setDual3D(handle, ctrl->value);
			break;

		case V4L2_CID_EXT_VDEC_VSYNC_THRESHOLD:
			ret = vdec_setVSyncThreshold(handle, ctrl->value);
			break;

		case V4L2_CID_EXT_VDEC_HFR_TYPE:
			ret = vdec_setHighFrameRateType(handle, ctrl->value);
			break;

		case V4L2_CID_EXT_VDEC_TEMPORAL_ID_MAX:
			ret = vdec_setTemporalIDMax(handle, ctrl->value);
			break;

		case V4L2_CID_EXT_VDEC_FREEZE_MODE:
			ret = vdec_setFreeze(handle, ctrl->value);
			break;

		case V4L2_CID_EXT_VDEC_STC_MODE:
			ret = vdec_setStcMode(handle, ctrl->value);
			break;

		case V4L2_CID_EXT_VDEC_LIPSYNC_MASTER:
			ret = vdec_setLipSyncMaster(handle, ctrl->value);
			break;

		case V4L2_CID_EXT_VDEC_AUDIO_CHANNEL:
			ret = vdec_setAudioChannel(handle, ctrl->value);
			break;

		case V4L2_CID_EXT_VDEC_PVR_MODE:
			ret = vdec_setPVRMode(handle, ctrl->value);
			break;

		case V4L2_CID_EXT_VDEC_FAST_IFRAME_MODE:
			ret = vdec_setFastIFrameMode(handle, ctrl->value);
			break;

		case V4L2_CID_EXT_VDEC_DIRECT_MODE:
			ret = vdec_setDirectMode(handle, ctrl->value);
			break;

		case V4L2_CID_EXT_VDEC_DRIPDEC_MODE:
			ret = vdec_setDripDecodeMode(handle, ctrl->value);
			break;

		case V4L2_CID_EXT_VDEC_ECP_INFO_NOTI:
			ret = (handle->bChannel)? 0 : -EPERM;
			break;

		default:
			dev_err(handle->dev, "unknown id 0x%x val %d\n", ctrl->id, ctrl->value);
			return -EINVAL;
	}

	return ret;
}

static int ioctl_s_input(struct file *file, void *priv, unsigned int i)
{
	struct v4l2_fh *vfh;
	vdec_handle_t *handle;

	vfh = file->private_data;
	handle = container_of(vfh, vdec_handle_t, fh);

	dev_info(handle->dev, "input %d\n", i);

	return vdec_setVTPPort(handle, i);
}

static int ioctl_s_output(struct file *file, void *priv, unsigned int i)
{
	struct v4l2_fh *vfh;
	vdec_handle_t *handle;

	vfh = file->private_data;
	handle = container_of(vfh, vdec_handle_t, fh);
	handle->bOutputIdx = 1;
	handle->outputIdx = i;

	dev_info(handle->dev, "output %d\n", i);

	return 0;
}

static int ioctl_streamon(struct file *file, void *fh, enum v4l2_buf_type type)
{
	int ret;
	struct v4l2_fh *vfh;
	vdec_handle_t *handle;

	vfh = file->private_data;
	handle = container_of(vfh, vdec_handle_t, fh);

	dev_info(handle->dev, "streamon type %d\n", type);

	if (handle->streaming_out || handle->streaming_cap)
		return v4l2_m2m_ioctl_streamon(file, fh, type);

	ret = vdec_streamon(handle);
	dev_info(handle->dev, "streamon ret %d\n", ret);
	return ret;
}

static int ioctl_streamoff(struct file *file, void *fh, enum v4l2_buf_type type)
{
	int ret;
	struct v4l2_fh *vfh;
	vdec_handle_t *handle;

	vfh = file->private_data;
	handle = container_of(vfh, vdec_handle_t, fh);

	dev_info(handle->dev, "streamoff type %d\n", type);

	if (handle->streaming_out || handle->streaming_cap)
		return v4l2_m2m_ioctl_streamoff(file, fh, type);

	ret = vdec_streamoff(handle);
	dev_info(handle->dev, "streamoff ret %d\n", ret);
	return ret;
}

static int ioctl_g_ext_ctrls(struct file *file, void *fh, struct v4l2_ext_controls *ctrls)
{
	int ret, i;
	struct v4l2_fh *vfh;
	vdec_handle_t *handle;
	struct v4l2_ext_control *ctrl;

	vfh = file->private_data;
	handle = container_of(vfh, vdec_handle_t, fh);
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

		else switch (ctrl->id) {
			case V4L2_CID_EXT_VDEC_PICINFO_EVENT_DATA:
				mutex_lock(&handle->thread_lock);
				ret = vdec_getPicInfo(handle, ctrl->ptr, ctrl->size);
				mutex_unlock(&handle->thread_lock);
				break;
			case V4L2_CID_EXT_VDEC_PICINFO_EVENT_DATA_EXT:
				mutex_lock(&handle->thread_lock);
				ret = vdec_getPicInfo_ext(handle, ctrl->ptr, ctrl->size);
				mutex_unlock(&handle->thread_lock);
				break;
			case V4L2_CID_EXT_VDEC_USER_EVENT_DATA:
				mutex_lock(&handle->thread_lock);
				ret = vdec_getUserData(handle, ctrl->ptr, ctrl->size);
				mutex_unlock(&handle->thread_lock);
				break;
			case V4L2_CID_EXT_VDEC_STREAM_INFO:
				ret = user_get_stream_info(handle, ctrl->ptr);
				break;
			case V4L2_CID_EXT_VDEC_DECODER_STATUS:
				ret = user_get_decoder_status(handle, ctrl->ptr);
				break;
			case V4L2_CID_EXT_VDEC_VIDEO_INFO:
				ret = user_get_video_info(handle, ctrl->ptr);
				break;
			default:
				ret = -1;
				dev_err(handle->dev, "unknown ctrl id 0x%x\n", ctrl->id);
				break;
		}
	}

	return ret;
}

static int ioctl_s_ext_ctrls(struct file *file, void *fh, struct v4l2_ext_controls *ctrls)
{
	int ret, i;
	struct v4l2_fh *vfh;
	vdec_handle_t *handle;
	struct v4l2_ext_control *ctrl;

	vfh = file->private_data;
	handle = container_of(vfh, vdec_handle_t, fh);
	ret = 0;

	for (i=0; !ret && i<ctrls->count; i++) {
		ctrl = &ctrls->controls[i];
		switch (ctrl->id) {
			case V4L2_CID_EXT_VDEC_SRC_RINGBUFS:
				if (copy_from_user(&handle->src, ctrl->ptr, sizeof(struct v4l2_ext_src_ringbufs)))
					return -EFAULT;
				if (!handle->src.bs_hdr_addr || !handle->src.ib_hdr_addr)
					return -EFAULT;
				handle->bSource = 1;
				break;
			case V4L2_CID_EXT_VDEC_USERTYPE:
				ret = vdec_setUserType(handle, ctrl->value);
				break;
			case V4L2_CID_EXT_VDEC_MEM_ALLOC:
				ret = user_allocMemory(handle, ctrl->ptr);
				break;
			case V4L2_CID_EXT_VDEC_MEM_FREE:
				ret = user_releaseMemory(handle, ctrl->ptr);
				break;
			case V4L2_CID_EXT_VDEC_DIRECT_ESDATA:
				ret = vdec_writeESdata(handle, ctrl->ptr, ctrl->size);
				break;
			case V4L2_CID_EXT_VDEC_DRIPDEC_PICTURE:
				ret= vdec_dripDecodePicture(handle, ctrl->ptr, ctrl->size);
				break;
			default:
				ret = -1;
				dev_err(handle->dev, "unknown ctrl id 0x%x\n", ctrl->id);
				break;
		}
	}

	return ret;
}

static int ioctl_subscribe_event(struct v4l2_fh *fh, const struct v4l2_event_subscription *sub)
{
	vdec_handle_t *handle;

	handle = container_of(fh, vdec_handle_t, fh);

	if (sub->type == V4L2_EVENT_PRIVATE_EXT_VDEC_EVENT)
		vdec_setEventFlag(handle, sub->id, 1);

	v4l2_event_subscribe(fh, sub, VDEC_CONF_EVENT_QSIZE, NULL);

	return 0;
}

static int ioctl_unsubscribe_event(struct v4l2_fh *fh, const struct v4l2_event_subscription *sub)
{
	vdec_handle_t *handle;

	handle = container_of(fh, vdec_handle_t, fh);
	if (sub->type == V4L2_EVENT_PRIVATE_EXT_VDEC_EVENT)
		vdec_setEventFlag(handle, sub->id, 0);

	v4l2_event_unsubscribe(fh, sub);

	return 0;
}

static int ioctl_try_decoder_cmd(struct file *file, void *priv,
		struct v4l2_decoder_cmd *cmd)
{
	int ret;

	switch (cmd->cmd) {
		case V4L2_DEC_CMD_START:
		case V4L2_DEC_CMD_STOP:
		case V4L2_DEC_CMD_PAUSE:
		case V4L2_DEC_CMD_RESUME:
		#ifdef V4L2_DEC_CMD_FLUSH
		case V4L2_DEC_CMD_FLUSH:
		#endif
			ret = 0;
			break;

		default:
			return -EINVAL;
	}

	return ret;
}

static int ioctl_decoder_cmd(struct file *file, void *priv,
		struct v4l2_decoder_cmd *cmd)
{
	int ret;
	struct v4l2_fh *vfh;
	vdec_handle_t *handle;

	vfh = file->private_data;
	handle = container_of(vfh, vdec_handle_t, fh);

	ret = ioctl_try_decoder_cmd(file, priv, cmd);
	if (ret) return ret;

	switch (cmd->cmd) {
		case V4L2_DEC_CMD_START:
			ret = vdec_streamon(handle);
			break;

		case V4L2_DEC_CMD_STOP:
			ret = vdec_streamoff(handle);
			break;

		case V4L2_DEC_CMD_PAUSE:
			ret = vdec_flt_pause(handle);
			break;

		case V4L2_DEC_CMD_RESUME:
			ret = vdec_flt_run(handle);
			break;

		#ifdef V4L2_DEC_CMD_FLUSH
		case V4L2_DEC_CMD_FLUSH:
			ret = vdec_flt_flush(handle);
			break;
		#endif

		default:
			return -EINVAL;
	}

	return ret;
}

const struct v4l2_file_operations vdec_file_ops = {
	.owner = THIS_MODULE,
	.open = fops_open,
	.release = fops_release,
	.unlocked_ioctl = video_ioctl2,
	.poll = v4l2_m2m_fop_poll,
	.mmap = v4l2_m2m_fop_mmap,
};

const struct v4l2_ioctl_ops vdec_ioctl_ops = {

	.vidioc_querycap = ioctl_querycap,

	.vidioc_enum_fmt_vid_cap_mplane = ioctl_enum_fmt_vid_cap_mplane,
	.vidioc_g_fmt_vid_cap_mplane = ioctl_g_fmt_vid_cap_mplane,
	.vidioc_try_fmt_vid_cap_mplane = ioctl_try_fmt_vid_cap_mplane,
	.vidioc_s_fmt_vid_cap_mplane = ioctl_s_fmt_vid_cap_mplane,

	.vidioc_enum_fmt_vid_out_mplane = ioctl_enum_fmt_vid_out_mplane,
	.vidioc_g_fmt_vid_out_mplane = ioctl_g_fmt_vid_out_mplane,
	.vidioc_try_fmt_vid_out_mplane = ioctl_try_fmt_vid_out_mplane,
	.vidioc_s_fmt_vid_out_mplane = ioctl_s_fmt_vid_out_mplane,

	.vidioc_enum_fmt_vid_out = ioctl_enum_fmt_vid_out_mplane,
	.vidioc_g_fmt_vid_out = ioctl_g_fmt_vid_out,
	.vidioc_try_fmt_vid_out = ioctl_try_fmt_vid_out,
	.vidioc_s_fmt_vid_out = ioctl_s_fmt_vid_out,

	.vidioc_g_ctrl = ioctl_g_ctrl,
	.vidioc_s_ctrl = ioctl_s_ctrl,
	.vidioc_s_input	= ioctl_s_input,
	.vidioc_s_output = ioctl_s_output,
	.vidioc_streamon = ioctl_streamon,
	.vidioc_streamoff = ioctl_streamoff,
	.vidioc_g_ext_ctrls = ioctl_g_ext_ctrls,
	.vidioc_s_ext_ctrls = ioctl_s_ext_ctrls,
	.vidioc_subscribe_event	= ioctl_subscribe_event,
	.vidioc_unsubscribe_event = ioctl_unsubscribe_event,

	.vidioc_reqbufs	= v4l2_m2m_ioctl_reqbufs,
	.vidioc_querybuf = v4l2_m2m_ioctl_querybuf,
	.vidioc_create_bufs = v4l2_m2m_ioctl_create_bufs,
	.vidioc_prepare_buf = v4l2_m2m_ioctl_prepare_buf,
	.vidioc_qbuf = v4l2_m2m_ioctl_qbuf,
	.vidioc_expbuf = v4l2_m2m_ioctl_expbuf,
	.vidioc_dqbuf = v4l2_m2m_ioctl_dqbuf,

	.vidioc_try_decoder_cmd = ioctl_try_decoder_cmd,
	.vidioc_decoder_cmd = ioctl_decoder_cmd,
};

