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

#include <linux/kthread.h>
#include <linux/delay.h>
#include <asm/div64.h>

#include <linux/file.h>
#include <linux/fdtable.h>

#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/videobuf2-dma-contig.h>

#pragma scalar_storage_order big-endian
#include <rpc_common.h>
#include <InbandAPI.h>
#include <VideoRPC_System.h>
#include <VideoRPC_Agent.h>
#include <video_bufferpool.h>
#pragma scalar_storage_order default

#include "vcodec_conf.h"
#include "vcodec_def.h"
#include "vcodec_struct.h"
#include "vcodec_utils.h"
#include "vcap_core.h"

#include "vutils/vrpc.h"
#include "vutils/vfifo.h"

static const vcodec_pixfmt_t pixfmt_cap[] = {
	{
		.pixelformat = V4L2_PIX_FMT_NV12,
		.num_planes = 1,
	},
	{
		.pixelformat = V4L2_PIX_FMT_NV12M,
		.num_planes = 2,
	},		
};

static int pollCap(void *arg)
{
	vcap_handle_t *handle;

	if (!arg)
		return -1;

	handle = (vcap_handle_t*)arg;

	while (!kthread_should_stop()) {
		mutex_lock(&handle->thread_lock);
		while (vcap_dqbuf(handle) >= 0);
		mutex_unlock(&handle->thread_lock);
		msleep_interruptible(handle->thread_msecs);
	}

	return 0;
}

static int returnAllBuf(vcap_handle_t *handle)
{
	vcodec_buffer_t *buf, *buf_tmp;

	list_for_each_entry_safe (buf, buf_tmp, &handle->buf_list, list) {
		printk("~~%s %x \n",__FUNCTION__, buf->dma[0].addr);
		list_del(&buf->list);
		vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
	}

	return 0;
}

static vcodec_buffer_t* getQBufByAddr(struct list_head *buf_list, unsigned int addr, char bExact)
{
	vcodec_buffer_t *buf, *buf_after, *buf_before;
	int offset, offset_after, offset_before;

	buf_after = buf_before = NULL;
	offset_after = offset_before = INT_MAX;

	list_for_each_entry(buf, buf_list, list) {
		offset = addr - (buf->dma[0].addr + buf->dma[0].offset);
		if (!offset)
			return buf;
		else if (offset > 0) {
			if (offset_after > offset) {
				buf_after = buf;
				offset_after = offset;
			}
		}
		else {
			if (offset_before > offset) {
				buf_before = buf;
				offset_before = offset;
			}
		}
	}

	if (!bExact) {
		buf = (buf_after)? buf_after : buf_before;
		if (buf) {
			if (!buf->nOut && buf_after) {
				buf->dma[0].offset = offset_after;
				pr_info("vcap: dma addr 0x%08x offset %d\n",
					buf->dma[0].addr, offset_after);
			}
			else
				pr_warn("vcap: no exact match found: req 0x%08x ret 0x%08x\n",
					addr, buf->dma[0].addr);

			return buf;
		}
	}

	pr_err("vcap: addr 0x%08x not found\n", addr);

	return NULL;
}

static int initThread(vcap_handle_t *handle,
		int (*func)(void*), char *desc, int msecs)
{
	struct task_struct *thread;

	if (!handle)
		return -EINVAL;

	if (handle->thread)
		return 0;

	thread = kthread_create(func, handle, "%s",desc);

	if (IS_ERR(thread)) {
		dev_err(handle->dev, "kthread_create fail\n");
		return -1;
	}

	handle->thread = thread;
	handle->thread_msecs = msecs;
	wake_up_process(thread);

	return 0;
}

static int exitThread(vcap_handle_t *handle)
{
	if (!handle)
		return -EINVAL;

	if (!handle->thread)
		return 0;

	kthread_stop(handle->thread);
	handle->thread = NULL;

	return 0;
}

static int initBufQ(vcap_handle_t *handle)
{
	int ret;
		
#ifndef GPSC_ALONE	
	vdec_handle_t *vdec;
#endif	
	char q_in, q_out;
    printk("+++%s: line %d \n", __FUNCTION__, __LINE__);
	if (!handle->bInputIdx)
		return -EPERM;

	q_in = q_out = 0;
#ifndef GPSC_ALONE
	vcodec_lockDecDevice(handle->device);

	vdec = vcodec_getDecByOutputIdx(handle->device, handle->inputIdx);

	if (!vdec || !vdec->elem.enable || !vdec->elem.flt_out)
		goto err;

	q_in = !(ret = vfifo_init(&handle->q_in, RINGBUFFER_MESSAGE, VCAP_CONF_BUFQ_IN_SIZE, 1, VFIFO_F_WRONLY));
	if (ret) goto err;

	q_out = !(ret = vfifo_init(&handle->q_out, RINGBUFFER_MESSAGE1, VCAP_CONF_BUFQ_OUT_SIZE, 1, VFIFO_F_RDONLY));
	if (ret) goto err;

	ret = vrpc_postRingBuf(vdec->userID, vdec->flt_out, handle->q_in.hdr.addr);
	if (ret) goto err;

	ret = vrpc_postRingBuf(vdec->userID, vdec->flt_out, handle->q_out.hdr.addr);
	if (ret) goto err;

	handle->bBufQ = 1;

	vcodec_unlockDecDevice(handle->device);
#else  
	q_in = !(ret = vfifo_init(&handle->q_in, RINGBUFFER_QUEUE_IN, VCAP_CONF_BUFQ_IN_SIZE, 1, VFIFO_F_WRONLY));
	if (ret) goto err;

	q_out = !(ret = vfifo_init(&handle->q_out, RINGBUFFER_QUEUE_OUT, VCAP_CONF_BUFQ_OUT_SIZE, 1, VFIFO_F_RDONLY));
	if (ret) goto err;
  
	ret = vrpc_postRingBuf(handle->userID, handle->inputIdx, handle->q_in.hdr.addr);
	if (ret) goto err;

	ret = vrpc_postRingBuf(handle->userID, handle->inputIdx, handle->q_out.hdr.addr);
	if (ret) goto err;
  
	handle->bBufQ = 1;
#endif
	printk("---%s: line %d \n", __FUNCTION__, __LINE__);
	return 0;

err:
#ifndef GPSC_ALONE	
	vcodec_unlockDecDevice(handle->device);
#endif	
	if (q_in) vfifo_exit(&handle->q_in);
	if (q_out) vfifo_exit(&handle->q_out);
	dev_err(handle->dev, "fail to init bufQ\n");
	printk("---%s: fail to init bufQ \n", __FUNCTION__);
	return -1;
}

static int exitBufQ(vcap_handle_t *handle)
{
#ifndef GPSC_ALONE		
	vdec_handle_t *vdec;
  
	vcodec_lockDecDevice(handle->device);

	vdec = vcodec_getDecByOutputIdx(handle->device, handle->inputIdx);

	if (vdec) {
		/* notify the release of bufQ */
		vrpc_postRingBuf(vdec->userID, vdec->flt_out, 0);
	}

	vcodec_unlockDecDevice(handle->device);
#else	
	vrpc_postRingBuf(handle->userID, handle->inputIdx, 0);	
#endif
	vfifo_exit(&handle->q_in);
	vfifo_exit(&handle->q_out);
	handle->bBufQ = 0;

	return 0;
}

static void syncPicObj(struct v4l2_ext_vcap_picobj *obj, vbp_dqbuf_t *buf)
{
	int i;

	for (i=0; i<2; i++) {
		obj->addr[i] = buf->dma[i].addr;
		obj->cmprs_addr[i] = buf->cmprs_hdr[i].addr;
	}

	i = 0;
	if (buf->progressive_seq) i |= PICOBJ_F_PROGRESSIVE_SEQ;
	if (buf->lossy_en) i |= PICOBJ_F_LOSSY_EN;
	if (buf->errMBs) i |= PICOBJ_F_ERRMBS;
	obj->flags = i;

	obj->pic_mode = buf->pic_mode;
	obj->pic_struct = buf->pic_struct;

	obj->bitDepthY = buf->bitDepthY;
	obj->bitDepthC = buf->bitDepthC;
	obj->sampleW = buf->sampleW;
	obj->sampleH = buf->sampleH;
	obj->picW = buf->picW;
	obj->picH = buf->picH;
	obj->picX = buf->picX;
	obj->picY = buf->picY;
	obj->pitchY = buf->pitchY;
	obj->pitchC = buf->pitchC;

	obj->vbi_aspect_ratio = buf->vbi_aspect_ratio;
	obj->afd = buf->afd;
	obj->par_width = buf->par_width;
	obj->par_height = buf->par_height;

	obj->framerate = buf->framerate;
	obj->pts = buf->pts;
	obj->pts2 = buf->pts2;
	obj->sei_pts = buf->sei_pts;

	obj->hdr_type = buf->hdr_type;
	obj->color_primaries = buf->color_primaries;
	obj->transfer_characteristics = buf->transfer_characteristics;
	obj->matrix_coeffs = buf->matrix_coeffs;
	obj->video_full_range_flag = buf->video_full_range_flag;

	for (i=0; i<3; i++) {
		obj->display_primaries_x[i] = buf->display_primaries_x[i];
		obj->display_primaries_y[i] = buf->display_primaries_y[i];
	}

	obj->white_point_x = buf->white_point_x;
	obj->white_point_y = buf->white_point_y;
	obj->max_display_mastering_luminance = buf->max_display_mastering_luminance;
	obj->min_display_mastering_luminance = buf->min_display_mastering_luminance;
	obj->max_content_light_level = buf->max_content_light_level;
	obj->max_pic_average_light_level = buf->max_pic_average_light_level;

}

int vcap_initHandle(vcap_handle_t *handle)
{
	int userPID, userSockfd;
	const vcodec_pixfmt_t *inst;

	userPID = current->pid;
	userSockfd = current->files->next_fd - 1; /* peek current fd */
	handle->userID = vrpc_generateUserID(userPID, userSockfd);
	handle->bInputIdx = 0;
	INIT_LIST_HEAD(&handle->buf_list);
	INIT_LIST_HEAD(&handle->reg_buf_list);
	mutex_init(&handle->thread_lock);

	inst = vcap_getPixelFormat(V4L2_PIX_FMT_NV12M);

	handle->pixfmt.pixelformat = inst->pixelformat;
	handle->pixfmt.num_planes = inst->num_planes;
	handle->pixfmt.colorspace = V4L2_COLORSPACE_REC709;
	handle->pixfmt.field = V4L2_FIELD_NONE;
	handle->pixfmt.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	handle->pixfmt.quantization = V4L2_QUANTIZATION_DEFAULT;
	handle->pixfmt.xfer_func = V4L2_XFER_FUNC_DEFAULT;

	/* set default width 1280  height 720 */
	handle->pixfmt.width = 1280;
	handle->pixfmt.height= 720 ;
	handle->pixfmt.plane_fmt[0].bytesperline = 1280;
	handle->pixfmt.plane_fmt[0].sizeimage = 1280 * 720 ;//+ VCAP_CONF_BUF_PADDING_SIZE;
	handle->pixfmt.plane_fmt[1].bytesperline = 1280;
	handle->pixfmt.plane_fmt[1].sizeimage = 1280 * (720 >> 1) ;//+ VCAP_CONF_BUF_PADDING_SIZE;

	return 0;
}

int vcap_exitHandle(vcap_handle_t *handle)
{
	exitThread(handle);
	returnAllBuf(handle);

	if (handle->bBufQ)
		exitBufQ(handle);

    handle->max_width = 0;
    handle->max_height= 0;

	return 0;
}

const vcodec_pixfmt_t* vcap_getPixelFormat(u32 pixelformat)
{
	int i;

	for (i=0; i<ARRAY_SIZE(pixfmt_cap); i++) {
		if (pixfmt_cap[i].pixelformat == pixelformat)
			return &pixfmt_cap[i];
	}

	return NULL;
}

const vcodec_pixfmt_t* vcap_getPixelFormatByIndex(int index)
{
	if (index >=0 && index < ARRAY_SIZE(pixfmt_cap))
		return &pixfmt_cap[index];

	return NULL;
}

int vcap_setFormat(struct v4l2_pix_format_mplane *mp, u32 pixfmt, int picW, int picH)
{
	int pitchY=0, pitchC=0, bufW=0, bufH=0;
	const vcodec_pixfmt_t *inst;
    printk("~~~%s w/h %d %d out w/h %d %d\n",__FUNCTION__,picW, picH, mp->width, mp->height);
	inst = vcap_getPixelFormat(pixfmt);
	if (!inst) {
		pr_err("invalid pixfmt 0x%08x\n", pixfmt);
		return -EINVAL;
	}

	/* load instance */
	mp->pixelformat = inst->pixelformat;
	mp->num_planes = inst->num_planes;

	bufW = roundup(picW, VCAP_CONF_BUF_WIDTH_ALIGN); /* TODO: 10 bits and align */
	bufH = roundup(picH, VCAP_CONF_BUF_HEIGHT_ALIGN);

	/* calculate bytesperline and sizeimage */
	if (pixfmt == V4L2_PIX_FMT_NV12M) {
		pitchY = bufW;
		pitchC = bufW;
		mp->width = picW;
		mp->height = picH;
		mp->plane_fmt[0].bytesperline = pitchY;
		mp->plane_fmt[0].sizeimage = pitchY * picH;//pitchY * bufH + VCAP_CONF_BUF_PADDING_SIZE;
		mp->plane_fmt[1].bytesperline = pitchC;
		mp->plane_fmt[1].sizeimage = pitchC * (picH>>1);//pitchC * (bufH >> 1) + VCAP_CONF_BUF_PADDING_SIZE;
	}

	/* calculate bytesperline and sizeimage */
	else if (pixfmt == V4L2_PIX_FMT_NV12) {
		pitchY = bufW;
		pitchC = bufW;
		mp->width = picW;
		mp->height = picH;
		mp->plane_fmt[0].bytesperline = pitchY;
		mp->plane_fmt[0].sizeimage = pitchY * (bufH + (bufH >> 1)) + VCAP_CONF_BUF_PADDING_SIZE;
	}
    printk("---%s w/h %d %d Y pitch %d sz %d\n",__FUNCTION__,picW, picH, pitchY, mp->plane_fmt[0].sizeimage);
	return 0;
}

int vcap_qbuf(vcap_handle_t *handle, vcodec_buffer_t *buf)
{
	if (handle->bBufQ)
		vcap_fifo_qbuf(&handle->q_in, buf);

	list_add_tail(&buf->list, &handle->buf_list);

	return 0;
}

int vcap_fifo_qbuf(vfifo_t *q, vcodec_buffer_t *buf)
{
	vbp_qbuf_t qbuf = {0};
  int i;
	qbuf.hdr.type = VBP_QBUF;
	qbuf.hdr.size = sizeof(vbp_qbuf_t);
	
	for(i = 0; i < 2 ;i++)
	{
		qbuf.dma[i].addr = buf->dma[i].addr + buf->dma[i].offset;
		qbuf.dma[i].size = buf->dma[i].size - buf->dma[i].offset;
		qbuf.cmprs_hdr[i].addr = buf->cmprs_hdr[i].addr;
		qbuf.cmprs_hdr[i].size = buf->cmprs_hdr[i].size;		
	}
	qbuf.flags = (!buf->nIn)? VBP_F_ADDBUF : 0;	
	qbuf.bufW = buf->bufW; /* XXX: required by VBM AddBuf operations */
	qbuf.bufH = buf->bufH; /* XXX: required by VBM AddBuf operations */

	if (vfifo_space(q) < sizeof(vbp_qbuf_t)) {
		pr_err("vcap: input queue is overflow. Plz enlarge it\n");
		return -1;
	}

	vfifo_write(q, &qbuf, sizeof(qbuf));
	buf->nIn++;

	return 0;
}

int vcap_fifo_dqbuf(vfifo_t *q, struct list_head *buf_list)
{
	int i, idx = -1, sizeY, sizeC;
	vbp_u_out_t bufOnWrap, *msg;
	vcodec_buffer_t *buf;

	if (!vfifo_cnt(q, 0))
		return -1;

	vfifo_peek(q, 0, &bufOnWrap, sizeof(vbp_u_out_t), (void**)&msg);

	if (msg->u.hdr.type == (INBAND_CMD_TYPE)VBP_DQBUF) {

		buf = getQBufByAddr(buf_list, msg->u.dqbuf.dma[0].addr, 0);
		if (buf) {
			buf->nOut++;
			list_del(&buf->list);
			if (buf->vb.vb2_buf.num_planes == 1 || buf->vb.vb2_buf.num_planes == 2) {
				long long div;
				long mod;
				idx = buf->vb.vb2_buf.index;
				sizeY = msg->u.dqbuf.pitchY * msg->u.dqbuf.picH;
				sizeC = sizeY >> 1;
				buf->vb.vb2_buf.planes[0].data_offset = buf->dma[0].offset;
				buf->vb.vb2_buf.planes[1].data_offset = buf->dma[1].offset;
				
				buf->vb.vb2_buf.planes[0].bytesused = buf->dma[0].offset + sizeY ;				
				buf->vb.vb2_buf.planes[1].bytesused = buf->dma[1].offset + sizeC ;
				
                buf->vb.field = V4L2_FIELD_NONE;/* Progressive */

				if( buf->vb.vb2_buf.num_planes == 1 )
				  buf->vb.vb2_buf.planes[0].bytesused += sizeC ;
				  
				/* convert PTS (in 90K) to struct timeval */
				div = msg->u.dqbuf.pts;
				mod = do_div(div, 90000);
				buf->vb.timestamp.tv_sec = div;
				buf->vb.timestamp.tv_usec = mod * 100 / 9;
				syncPicObj(&buf->picobj, &msg->u.dqbuf);
			}
			vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_DONE);
		}
	}

	else if (msg->u.hdr.type == (INBAND_CMD_TYPE)VBP_EOS) {
		idx = VCAP_IDX_EOS;
		if (!(buf = list_first_entry_or_null(buf_list, vcodec_buffer_t, list)))
			return -1;
		list_del(&buf->list);
		for (i=0; i<buf->vb.vb2_buf.num_planes; i++)
			buf->vb.vb2_buf.planes[i].bytesused = 0;
		buf->vb.flags |= V4L2_BUF_FLAG_LAST;
		vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_DONE);
	}

	vfifo_skip(q, 0, msg->u.hdr.size);

	return idx;
}

int vcap_dqbuf(vcap_handle_t *handle)
{
	int idx;

	idx = vcap_fifo_dqbuf(&handle->q_out, &handle->buf_list);
	if (idx == VCAP_IDX_EOS) {
		struct v4l2_event event;
#ifndef GPSC_ALONE
		vdec_handle_t *vdec;
#endif

		event.type = V4L2_EVENT_EOS;
		event.id = 0;
		v4l2_event_queue_fh(&handle->fh, &event);

		#ifndef GPSC_ALONE
		vcodec_lockDecDevice(handle->device);
		vdec = vcodec_getDecByOutputIdx(handle->device, handle->inputIdx);
		if (vdec)
			v4l2_event_queue_fh(&vdec->fh, &event);
		vcodec_unlockDecDevice(handle->device);
		#endif

		dev_info(handle->dev, "EOS\n");
	}
	return idx;
}

int vcap_streamon(vcap_handle_t *handle)
{
	vcodec_buffer_t *buf;
	printk("+++%s: line %d \n", __FUNCTION__,__LINE__);
	if (!handle->bBufQ && initBufQ(handle) < 0)
	{
		/* socts-gpsc -u : streamon should be OK even no VDEC instance */
		printk("+++%s: initBufQ fail \n", __FUNCTION__);
		return 0;
    }
	/* deliver all pending buffers */
	list_for_each_entry (buf, &handle->buf_list, list) {
		if (!buf->nIn)
			vcap_fifo_qbuf(&handle->q_in, buf);
	}

	initThread(handle, pollCap, "vcap_pollCap", 4);
	printk("---%s: line %d \n", __FUNCTION__,__LINE__);
	return 0;
}

int vcap_streamoff(vcap_handle_t *handle)
{
	printk("+++%s: line %d \n", __FUNCTION__,__LINE__);
	exitThread(handle);

	if (handle->bBufQ)
		exitBufQ(handle);

	returnAllBuf(handle);

	printk("---%s: line %d \n", __FUNCTION__,__LINE__);
	return 0;
}
