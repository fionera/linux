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
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>
#include <linux/file.h>
#include <linux/fdtable.h>

#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>
#include <media/v4l2-mem2mem.h>

#include <rbus/timer_reg.h>

#pragma scalar_storage_order big-endian
#include <rpc_common.h>
#include <InbandAPI.h>
#include <VideoRPC_System.h>
#include <VideoRPC_Agent.h>
#include <video_userdata.h>
#include <video_krpc_interface.h>
#pragma scalar_storage_order default

#include "rdvb/rdvb-dmx/rdvb_dmx_ctrl.h"

#include "v4l2_vdec_ext.h"

#include "vcodec_conf.h"
#include "vcodec_def.h"
#include "vcodec_macros.h"
#include "vcodec_struct.h"
#include "vcodec_utils.h"
#include "vcodec_weakref.h"
#include "vdec_core.h"
#include "vdec_conv.h"
#include "vdec_out.h"
#include "vdec_attrs.h"
#include "vdec_cap.h"
#include "vcap_core.h"

#include "vutils/vrpc.h"

/* XXX: DDTS demands success even if no valid input */
#define IGNORE_DDTS_INVALID_INPUT

#define RD_PIC 0
#define RD_FRM 1

#define PTS_ONE_SECOND      90000

static int readMsgInfo(vdec_handle_t *handle)
{
	RINGBUFFER_HEADER *pRBH;
	vfifo_t *msgQ;
	unsigned int rd, wr, cur_pts;
	struct v4l2_event event;
	int type;
	VIDEO_DEC_FRM_MSG bufOnWrap, *msg = NULL;

	msgQ = handle->msgQ;
	pRBH = (RINGBUFFER_HEADER*)msgQ->hdr.uncached;

	rd = msgQ->rd[RD_FRM];
	wr = msgQ->wr = pRBH->writePtr;

	/* update PicInfo's rd if no pending PicInfo */
	if (handle->nPicInfoIn == handle->nPicInfoOut) {
		if (msgQ->rd[RD_PIC] != rd)
			vfifo_syncRd(msgQ, RD_PIC, rd);
	}

	if(handle->state == ST_RUN)
	{
		cur_pts = rtd_inl(TIMER_VCPU_CLK90K_LO_reg);

		if( handle->Last_picinfo_PTS &&
			((cur_pts > handle->Last_picinfo_PTS && cur_pts - handle->Last_picinfo_PTS > PTS_ONE_SECOND) ||
			(handle->Last_picinfo_PTS > cur_pts && cur_pts > PTS_ONE_SECOND)) )
		{
			dev_info(handle->dev, "VDEC %d no pic info over 1 sec %d %d\n", handle->channel, cur_pts, handle->Last_picinfo_PTS);
			dev_info(handle->dev, "w/r %x %x %x pic %d %d\n", wr, rd, msgQ->rd[RD_PIC],handle->nPicInfoIn,handle->nPicInfoOut);
			handle->Last_picinfo_PTS = cur_pts;
		}

		if( handle->Last_frminfo_PTS &&
			((cur_pts > handle->Last_frminfo_PTS && cur_pts - handle->Last_frminfo_PTS > PTS_ONE_SECOND * 3) ||
			(handle->Last_frminfo_PTS > cur_pts && cur_pts > PTS_ONE_SECOND * 3)) )
		{
			dev_info(handle->dev, "VDEC %d no frm info over 3 sec %d %d\n", handle->channel, cur_pts, handle->Last_frminfo_PTS);
			handle->Last_frminfo_PTS = cur_pts;
		}
	}
	/* do nothing if empty */
	if (rd == wr)
		return 0;

	while (rd != wr) {

		vfifo_peek(msgQ, RD_FRM, &bufOnWrap, sizeof(bufOnWrap), (void**)&msg);
		type = msg->msgType;

		if (type == VIDEO_Pic_Info ) {
            
            if(handle->nPicInfoIn - handle->nPicInfoOut >= VDEC_CONF_EVENT_QSIZE)
            {
              //dev_info(handle->dev, "pic %d %d frm %d\n", handle->nPicInfoIn,handle->nPicInfoOut, handle->nFrmInfo);	
              break;
            }

			handle->nPicInfo++;
			handle->Last_picinfo_PTS = rtd_inl(TIMER_VCPU_CLK90K_LO_reg);
			if (msg != &bufOnWrap)
				handle->pPicMsg = msg;
			if (handle->nPicInfo == 1) {
				dev_info(handle->dev, "get 1st PicInfo\n");
			}

			vfifo_skip(msgQ, RD_FRM, sizeof(VIDEO_DEC_PIC_MSG));
			rd = msgQ->rd[RD_FRM];

			if (handle->event.bPicInfo) {
				event.type = V4L2_EVENT_PRIVATE_EXT_VDEC_EVENT;
				event.id = V4L2_SUB_EXT_VDEC_PICINFO;
				v4l2_event_queue_fh(&handle->fh, &event);
				handle->nPicInfoIn++;
			}

			else {
				vfifo_syncRd(msgQ, RD_PIC, rd);
			}
		}

		else if (type == VIDEO_Frm_Info) {

			handle->nFrmInfo++;
			handle->Last_frminfo_PTS = rtd_inl(TIMER_VCPU_CLK90K_LO_reg);
			if (handle->nFrmInfo == 1) {
				 dev_info(handle->dev, "get 1st FrmInfo\n");
			}

			if (handle->event.bFrmInfo) {
				event.type = V4L2_EVENT_PRIVATE_EXT_VDEC_EVENT;
				event.id = V4L2_SUB_EXT_VDEC_FRAME;
				event.u.data[0] = msg->frame_type;
				event.u.data[5] = msg->PTSH & 0xff;
				event.u.data[4] = (msg->PTSL >> 24) & 0xff;
				event.u.data[3] = (msg->PTSL >> 16) & 0xff;
				event.u.data[2] = (msg->PTSL >> 8) & 0xff;
				event.u.data[1] = msg->PTSL & 0xff;
				v4l2_event_queue_fh(&handle->fh, &event);
			}

			vfifo_skip(msgQ, RD_FRM, sizeof(VIDEO_DEC_FRM_MSG));
			rd = msgQ->rd[RD_FRM];
		}

		else {
			dev_err(handle->dev, "unknown type %d rd 0x%x wr 0x%x\n", type, rd, wr);
			vfifo_syncRd(msgQ, RD_FRM, wr);
			break;
		}
	}

	return 0;
}

static int readUserData(vdec_handle_t *handle)
{
	RINGBUFFER_HEADER *pRBH;
	vfifo_t *userDataQ;
	unsigned int rd, wr;
	struct v4l2_event event;
	cc_pack_t bufOnWrap, *pack = NULL;

	userDataQ = handle->userDataQ;
	pRBH = (RINGBUFFER_HEADER*)userDataQ->hdr.uncached;

	rd = userDataQ->rd[1];
	wr = userDataQ->wr = pRBH->writePtr;

	/* do nothing if empty */
	if (rd == wr)
		return 0;

	while (rd != wr) {

		handle->nUserData++;
		if (handle->nUserData == 1) {
			dev_info(handle->dev, "get 1st UserData\n");
		}

		vfifo_peek(userDataQ, 1, &bufOnWrap, sizeof(bufOnWrap), (void**)&pack);
		vfifo_skip(userDataQ, 1, sizeof(cc_pack_t));
		rd = userDataQ->rd[1];

		if (handle->event.bUserData) {
			VIDEO_CC_CALLBACK_HEADER *hdr = &pack->hdr;
			event.type = V4L2_EVENT_PRIVATE_EXT_VDEC_EVENT;
			event.id = V4L2_SUB_EXT_VDEC_USERDATA;
			*(int32_t*)&event.u.data[0] = hdr->size;
			event.u.data[4] = hdr->picture_coding_type;
			event.u.data[5] = hdr->repeat_first_field;
			event.u.data[6] = hdr->top_field_first;
			event.u.data[7] = hdr->temporal_ref;
			*(uint32_t*)&event.u.data[8] = (uint32_t)hdr->PTS;
			v4l2_event_queue_fh(&handle->fh, &event);
			handle->nUserDataIn++;
		}

		else {
			vfifo_syncRd(userDataQ, 0, rd);
		}

	}

	return 0;
}

static int pollMsg(void *arg)
{
	vdec_handle_t *handle;

	if (!arg)
		return -1;

	handle = (vdec_handle_t*)arg;

	while (!kthread_should_stop()) {
		mutex_lock(&handle->thread_lock);
		readMsgInfo(handle);
		readUserData(handle);
		mutex_unlock(&handle->thread_lock);
		msleep_interruptible(handle->thread_msecs);
	}

	return 0;
}

static int pollCap(void *arg)
{
	vdec_handle_t *handle;

	if (!arg)
		return -1;

	handle = (vdec_handle_t*)arg;

	while (!kthread_should_stop()) {
		mutex_lock(&handle->thread_lock);
		while (vdec_cap_dqbuf(handle) >= 0);
		mutex_unlock(&handle->thread_lock);
		msleep_interruptible(handle->thread_msecs);
	}

	return 0;
}

static int initThread(vdec_handle_t *handle,
		int (*func)(void*), char *desc, int msecs)
{
	struct task_struct *thread;

	if (!handle)
		return -EINVAL;

	if (handle->thread)
		return 0;

	thread = kthread_create(func, handle, "%s", desc);

	if (IS_ERR(thread)) {
		dev_err(handle->dev, "kthread_create fail\n");
		return -1;
	}

	handle->thread = thread;
	handle->thread_msecs = msecs;
	wake_up_process(thread);

	return 0;
}

static int exitThread(vdec_handle_t *handle)
{
	if (!handle)
		return -EINVAL;

	if (!handle->thread)
		return 0;

	kthread_stop(handle->thread);
	handle->thread = NULL;

	return 0;
}

#ifdef V4L2_SUB_EXT_VDEC_MEDIAINFO
static int sendMediaInfo(vdec_handle_t *handle, struct v4l2_event *event, void *data)
{
	VIDEO_RPC_DEC_MEDIA_INFO *src = data;
	struct v4l2_ext_vdec_media_info *dst = (void*)event->u.data;

	/* check sizeof */
	BUILD_BUG_ON(sizeof(struct v4l2_ext_vdec_media_info) > sizeof(event->u));

	dst->width = src->width;
	dst->height = src->height;
	dst->par_width = src->par_width;
	dst->par_height = src->par_height;
	dst->frame_rate = src->frame_rate;
	dst->aspect_ratio_n = src->aspect_ratio_n;
	dst->aspect_ratio_d = src->aspect_ratio_d;
	dst->level = src->level;
	dst->profile = src->profile;
	dst->type_3D = src->type_3D;
	dst->type_LR = src->type_LR;
	dst->type_scan = src->type_Scan;
	dst->afd = src->afd;

	event->type = V4L2_EVENT_PRIVATE_EXT_VDEC_EVENT;
	event->id = V4L2_SUB_EXT_VDEC_MEDIAINFO;
	v4l2_event_queue_fh(&handle->fh, event);

	return 0;
}
#endif

static int onService(int portID, int cmd, void *data, int size, void *priv)
{
	vdec_handle_t *handle = priv;
	struct v4l2_event event;

	/* TODO: handle rpc requests from vcpu */
	dev_info(handle->dev, "onService portID 0x%08x cmd %d data %p size %d\n",
		portID, cmd, data, size);

	switch (cmd) {
		case VIDEO_RPC_ToSystem_EndOfStream:
			event.type = V4L2_EVENT_EOS;
			event.id = 0;
			v4l2_event_queue_fh(&handle->fh, &event);
			break;

		#ifdef V4L2_SUB_EXT_VDEC_MEDIAINFO
		case VIDEO_RPC_DEC_ToSystem_Deliver_MediaInfo:
			sendMediaInfo(handle, &event, data);
			break;
		#endif
	}

	return 0;
}

static int notifyDemuxFlushBegin(vdec_handle_t *handle)
{
	int ret;

	if (!handle->bVTPPort) {
		dev_err(handle->dev, "%s: no vtp-port\n", __func__);
		return -EPERM;
	}

	if (__weakref_rdvb_dmx_ctrl_privateInfo) {
		ret = __weakref_rdvb_dmx_ctrl_privateInfo(DMX_PRIV_CMD_VIDEO_FLUSH_BEGIN,
		PIN_TYPE_VTP, handle->vtp_port, 0);
		IF_ERR_RET_VA(ret, "privateInfo fail");
	}

	return 0;
}

static int notifyDemuxFlushed(vdec_handle_t *handle)
{
	int ret;
	struct dmx_priv_data args;

	if (!handle->bVTPPort) {
		dev_err(handle->dev, "%s: no vtp-port\n", __func__);
		return -EPERM;
	}

	args.data.video_decode_mode = getDecMode(handle->decode_mode);

	if (__weakref_rdvb_dmx_ctrl_privateInfo) {
		ret = __weakref_rdvb_dmx_ctrl_privateInfo(DMX_PRIV_CMD_NOTIFY_FLUSHED,
		PIN_TYPE_VTP, handle->vtp_port, &args);
		IF_ERR_RET_VA(ret, "privateInfo fail");
	}

	return 0;
}

static int pending_operations(vdec_handle_t *handle)
{
	if (handle->pending_ops_display)
		vdec_connectDisplay(handle, handle->display, ((handle->userType == V4L2_EXT_VDEC_USERTYPE_DTV)? 1: 0) , 0);

	if (handle->pending_ops_freerun)
		vdec_setAVSync(handle, !handle->bFreerun);

	if (handle->pending_ops_display_delay)
		vdec_setDisplayDelay(handle, handle->display_delay);

	if (handle->pending_ops_stc_mode)
		vdec_setStcMode(handle, handle->stc_mode);

	if (handle->pending_ops_lipsync_master)
		vdec_setLipSyncMaster(handle, handle->lipsync_master);

	if (handle->pending_ops_audio_channel)
		vdec_setAudioChannel(handle, handle->audio_channel);

	if (handle->pending_ops_fast_iframe_mode)
		vdec_setFastIFrameMode(handle, handle->fast_iframe_mode);

	if (handle->pending_ops_decode_mode)
		vdec_setDecMode(handle, handle->decode_mode);

	if (handle->pending_ops_temporal_id_max)
		vdec_setTemporalIDMax(handle, handle->temporal_id_max);

	if (handle->pending_ops_low_delay)
		vdec_setLowDelay(handle, handle->low_delay);

	return 0;
}

static int set_resource_info(vdec_handle_t *handle, unsigned int flt_dec)
{
	int ret, width, height, framerate;

	width = 1920;
	height = 1088;
	framerate = 30000;

	if (handle->pixfmt_out.width > 1920 && handle->pixfmt_out.height > 1088) {
		width = handle->pixfmt_out.width;
		height = handle->pixfmt_out.height;
	}

	else if (handle->userType == V4L2_EXT_VDEC_USERTYPE_DTV) {
		if (handle->pixfmt_out.pixelformat == V4L2_PIX_FMT_HEVC) {
			width = 4096;
			height = 2176;
			framerate = 59940;
		}
	}

	ret = vrpc_sendRPC_payload_va(handle->userID,
		VIDEO_RPC_ToAgent_SetResourceInfo,
		VIDEO_RPC_RESOURCE_INFO,
			.core_type = VIDEO_RESOURCE_CORE_IP1,
			.video_port = handle->bChannel? handle->channel : 0,
			.max_width = -1,
			.max_height = -1,
			.instanceID = flt_dec,
			.width = width,
			.height = height,
			.framerate = framerate
		);
	IF_ERR_RET(ret);

	dev_info(handle->dev, "pixfmt %dx%d resourceInfo %dx%d fr %d\n",
		handle->pixfmt_out.width, handle->pixfmt_out.height,
		width, height, framerate);

	return 0;
}

int vdec_flt_init(vdec_handle_t *handle)
{
	int ret, userID, userType;
	unsigned int flt_dec, flt_out;
	char bServ, bMsgQ, bUserDataQ, bBitstreamQ, bIbCmdQ, bThread;
	char bBufQ_in, bBufQ_out;
	void *pRefClk;

	if (!handle || !handle->userID)
		return -EINVAL;

	if (handle->elem.enable) {
		dev_err(handle->dev, "can't init active flts\n");
		return -EPERM;
	}

	userID = handle->userID;
	userType = handle->userType;
	flt_dec = flt_out = 0;
	bServ = bMsgQ = bUserDataQ = bBitstreamQ = bIbCmdQ = bThread = 0;
	bBufQ_in = bBufQ_out = 0;
	pRefClk = NULL;

	/* register service at the beginning */
	bServ = !(ret = vrpc_registerService(userID, userID, onService, handle));
	IF_ERR_GOTO(ret, err);

	/* get msgQ/userDataQ ringbuf */
	if( (handle->QIndex = vcodec_getFreeMsgQ(handle->device)) != -1)
	{
		handle->msgQ = &handle->device->msgQ[handle->QIndex];
		handle->userDataQ = &handle->device->userDataQ[handle->QIndex];
		vfifo_reset(handle->msgQ, RINGBUFFER_MESSAGE1, 2);
		vfifo_reset(handle->userDataQ, RINGBUFFER_DTVCC, 2);
		bMsgQ = bUserDataQ = 1;
	}else
	{
		dev_err(handle->dev, "%s: getFreeBuf fail\n", __func__);
		goto err;
	}

	if (handle->bDirectMode || handle->bDripDecode)
	{
		/* alloc bitstreamQ ringbuf */
		bBitstreamQ = !(ret = vfifo_init(&handle->bitstreamQ, RINGBUFFER_STREAM,
			VDEC_CONF_DIRECTMODE_BS_SIZE, 1, VFIFO_F_WRONLY));
		IF_ERR_GOTO(ret, err);

		/* alloc ibCmdQ ringbuf */
		bIbCmdQ = !(ret = vfifo_init(&handle->ibCmdQ, RINGBUFFER_COMMAND,
			VDEC_CONF_DIRECTMODE_IB_SIZE, 1, VFIFO_F_WRONLY));
		IF_ERR_GOTO(ret, err);

		/* alloc refclock */
		pRefClk = handle->pRefClockCachedAddr = dvr_malloc_uncached_specific(
			sizeof(REFCLOCK), GFP_DCU1, &handle->pRefClockNonCachedLower);
		IF_ERR_GOTO(!handle->pRefClockCachedAddr, err);
		handle->nRefClockPhyAddr = dvr_to_phys(handle->pRefClockCachedAddr);

		dev_info(handle->dev, "pRefClockCachedAddr %p pRefClockNonCachedLower %p nRefClockPhyAddr %x\n",
			handle->pRefClockCachedAddr, handle->pRefClockNonCachedLower, handle->nRefClockPhyAddr);

		memset(handle->pRefClockNonCachedLower, 0, sizeof(REFCLOCK));

		//Set VO free run
		((REFCLOCK*)handle->pRefClockNonCachedLower)->mastership.videoMode =  AVSYNC_FORCED_MASTER;

		handle->src.bs_hdr_addr = handle->bitstreamQ.hdr.addr;
		handle->src.ib_hdr_addr = handle->ibCmdQ.hdr.addr;
		handle->src.refclk_addr = handle->nRefClockPhyAddr;
	}

	if (userType == V4L2_EXT_VDEC_USERTYPE_DTV && !handle->bDirectMode && !handle->bDripDecode) {

		/* import bitstream ringbuf */
		bBitstreamQ = !(ret = vfifo_init_importHdr(&handle->bitstreamQ, handle->src.bs_hdr_addr, 0));
		IF_ERR_GOTO(ret, err);
	}

	else if (userType == V4L2_EXT_VDEC_USERTYPE_MEDIA_CAPTURE_M2M) {
		bBufQ_in = !(ret = vfifo_init(&handle->bufQ_in, RINGBUFFER_MESSAGE, VCAP_CONF_BUFQ_IN_SIZE, 1, VFIFO_F_WRONLY));
		IF_ERR_GOTO(ret, err);
		bBufQ_out = !(ret = vfifo_init(&handle->bufQ_out, RINGBUFFER_MESSAGE1, VCAP_CONF_BUFQ_OUT_SIZE, 1, VFIFO_F_RDONLY));
		IF_ERR_GOTO(ret, err);
	}

	/* create filters */
	ret = !(flt_out = vrpc_createFilter(userID, getOutFltType(userType)));
	IF_ERR_GOTO(ret, err);
	ret = !(flt_dec = vrpc_createFilter(userID, VF_TYPE_VIDEO_MPEG2_DECODER));
	IF_ERR_GOTO(ret, err);

	/* set dec filter's ringbufs */
	ret = vrpc_postRingBuf(userID, flt_dec, handle->src.bs_hdr_addr);
	IF_ERR_GOTO(ret, err);
	ret = vrpc_postRingBuf(userID, flt_dec, handle->src.ib_hdr_addr);
	IF_ERR_GOTO(ret, err);
	if (bMsgQ) {
		ret = vrpc_postRingBuf(userID, flt_dec, handle->msgQ->hdr.addr);
		IF_ERR_GOTO(ret, err);
	}
	if (bUserDataQ) {
		/* NOTE:
		 * instanceID:	+2/-2 indicate init/de-init vdec port 1
		 * 		+1/-1 indicate init/de-init vdec port 0
		 * 		instanceID should not be 0
		 */
		ret = vrpc_postRingBuf(userID,
			1 + (handle->bChannel? handle->channel : 0),
			handle->userDataQ->hdr.addr);
		IF_ERR_GOTO(ret, err);
	}

	/* set out filter's ringbufs */
	if (bBufQ_in) {
		ret = vrpc_postRingBuf(userID, flt_out, handle->bufQ_in.hdr.addr);
		IF_ERR_GOTO(ret, err);
	}
	if (bBufQ_out) {
		ret = vrpc_postRingBuf(userID, flt_out, handle->bufQ_out.hdr.addr);
		IF_ERR_GOTO(ret, err);
	}

	/* connect filters */
	ret = vrpc_sendRPC_payload_va(userID,
		VIDEO_RPC_ToAgent_Connect,
		RPC_CONNECTION,
			.srcInstanceID = flt_dec,
			.desInstanceID = flt_out,
		);
	IF_ERR_GOTO(ret, err);

	if (userType == V4L2_EXT_VDEC_USERTYPE_DTV
		|| userType == V4L2_EXT_VDEC_USERTYPE_MEDIA_TUNNEL) {

		/* set output channel */
		ret = vrpc_sendRPC_payload_va(userID,
			VIDEO_RPC_VO_FILTER_ToAgent_ConnectVDec,
			VIDEO_RPC_VO_FILTER_ConnectVDec,
				.instanceID = flt_out,
				.nPort = handle->channel,
			);
		IF_ERR_GOTO(ret, err);

		/* set output refclk */
		ret = vrpc_sendRPC_payload_va(userID,
			VIDEO_RPC_ToAgent_SetRefClock,
			VIDEO_RPC_SET_REFCLOCK,
				.instanceID = flt_out,
				.pRefClock = handle->src.refclk_addr,
			);
		IF_ERR_GOTO(ret, err);
	}

	/* init decoder */
	ret = vrpc_sendRPC_payload_va(userID,
		VIDEO_RPC_DEC_ToAgent_Init,
		VIDEO_RPC_DEC_INIT,
			.instanceID = flt_dec,
			.type = getDecType(handle->pixfmt_out.pixelformat),
		);
	IF_ERR_GOTO(ret, err);

	/* set resource info */
	ret = set_resource_info(handle, flt_dec);
	IF_ERR_GOTO(ret, err);

	if (bUserDataQ) {
		/* set DecoderCCBypassMode */
		ret = vrpc_sendRPC_payload_va(userID,
			VIDEO_RPC_DEC_ToAgent_SetDecoderCCBypass,
			VIDEO_RPC_DEC_CC_BYPASS_MODE,
				.instanceID = flt_dec,
				.cc_mode = VIDEODECODER_CC_CALLBACK,
			);
		IF_ERR_GOTO(ret, err);
	}

	/* notify decoder of VBM configuration */
	if (userType == V4L2_EXT_VDEC_USERTYPE_MEDIA_CAPTURE
		|| userType == V4L2_EXT_VDEC_USERTYPE_MEDIA_CAPTURE_M2M) {
		ret = vrpc_sendRPC_cmdID_payload_va(userID,
			RPC_VCPU_ID_0x151_SYSTEM_TO_VIDEO_VDEC,
			SUBID_VDEC_SET_NV12_BUFFERPOOL,
			video_krpc_vdec_set_nv12_bufferpool_t,
				.instanceID = flt_dec,
			);
		IF_ERR_GOTO(ret, err);
	}

	/* create poll thread */
	if (userType == V4L2_EXT_VDEC_USERTYPE_DTV) {
		bThread = !(ret = initThread(handle, pollMsg, "vdec_pollMsg", 10));
		IF_ERR_GOTO(ret, err);
	} else if (userType == V4L2_EXT_VDEC_USERTYPE_MEDIA_CAPTURE_M2M) {
		bThread = !(ret = initThread(handle, pollCap, "vdec_pollCap", 10));
		IF_ERR_GOTO(ret, err);
	}

	/* finalize */
	handle->elem.msgQ = bMsgQ;
	handle->elem.userDataQ = bUserDataQ;
	handle->elem.bitstreamQ = bBitstreamQ;
	handle->elem.ibCmdQ = bIbCmdQ;
	handle->elem.bufQ_in = bBufQ_in;
	handle->elem.bufQ_out = bBufQ_out;
	handle->elem.thread = bThread;
	handle->elem.flt_dec = !!flt_dec;
	handle->elem.flt_out = !!flt_out;
	handle->elem.enable = 1;
	handle->flt_dec = flt_dec;
	handle->flt_out = flt_out;
	handle->nPicInfo =
	handle->nPicInfoIn =
	handle->nPicInfoOut =
	handle->nFrmInfo =
	handle->nUserData =
	handle->nUserDataIn =
	handle->nUserDataOut = 0;
	handle->stc_mode = 1;
	handle->lipsync_master = 0;
	handle->audio_channel = 0;
	handle->pvr_mode = 0;
	handle->fast_iframe_mode = 0;
	handle->state = ST_INIT;
	handle->pre_speed = 1000; //VDEC_DECODE_SPEED_NORMAL
	handle->Last_frminfo_PTS =
	handle->Last_picinfo_PTS = 0;

	pending_operations(handle);

	dev_info(handle->dev, "%s: ch %d dec 0x%x out 0x%x\n",
		__func__, handle->channel, flt_dec, flt_out);

	return 0;

err:

	if (bThread) exitThread(handle);
	if (flt_dec || flt_out) vrpc_releaseFilter(handle->userID);
	if (bBitstreamQ) vfifo_exit(&handle->bitstreamQ);
	if (bIbCmdQ) vfifo_exit(&handle->ibCmdQ);
	if (pRefClk) dvr_free(pRefClk);
	if (bBufQ_in) vfifo_exit(&handle->bufQ_in);
	if (bBufQ_out) vfifo_exit(&handle->bufQ_out);
	if (bServ) vrpc_unregisterService(userID, userID);
    if(handle->QIndex != -1) 
    {
      vcodec_putMsgQ(handle->device, handle->QIndex);	
      handle->QIndex = -1;
    } 
	dev_err(handle->dev, "%s: fail\n", __func__);

	return ret;
}

int vdec_flt_sendCmd(vdec_handle_t *handle, int cmd)
{
	int ret;

	if (handle->elem.flt_out) {
		ret = vrpc_sendRPC_param(handle->userID, cmd, handle->flt_out, 0);
		IF_ERR_RET(ret);
	}

	if (handle->elem.flt_dec) {
		ret = vrpc_sendRPC_param(handle->userID, cmd, handle->flt_dec, 0);
		IF_ERR_RET(ret);
	}

	return 0;
}

int vdec_flt_run(vdec_handle_t *handle)
{
	int ret;

	if (!handle)
		return -EINVAL;

	if (!handle->elem.enable) {
		dev_err(handle->dev, "%s: no elem\n", __func__);
		return -EPERM;
	}

	if (handle->state == ST_RUN)
		return 0;

	ret = vdec_flt_sendCmd(handle, VIDEO_RPC_ToAgent_Run);
	IF_ERR_RET(ret);

	handle->state = ST_RUN;
	handle->dtv_vdec_state = V4L2_EXT_VDEC_STATE_PLAYING;

	return 0;
}

int vdec_flt_pause(vdec_handle_t *handle)
{
	int ret;

	if (!handle)
		return -EINVAL;

	if (!handle->elem.enable) {
		dev_err(handle->dev, "%s: no elem\n", __func__);
		return -EPERM;
	}

	if (handle->state == ST_PAUSE)
		return 0;

	ret = vdec_flt_sendCmd(handle, VIDEO_RPC_ToAgent_Pause);
	IF_ERR_RET(ret);

	handle->state = ST_PAUSE;

	return 0;
}

int vdec_flt_flush(vdec_handle_t *handle)
{
	int ret;

	if (!handle)
		return -EINVAL;

	if (!handle->elem.enable) {
		dev_err(handle->dev, "%s: no elem\n", __func__);
		return -EPERM;
	}

	//ret = vdec_flt_sendCmd(handle, VIDEO_RPC_ToAgent_Flush);
	//IF_ERR_RET(ret);
	if (handle->bVTPPort)
		notifyDemuxFlushBegin(handle);

	if (handle->elem.flt_dec) {
		ret = vrpc_sendRPC_param(handle->userID, VIDEO_RPC_ToAgent_Flush, handle->flt_dec, 0);
		IF_ERR_RET(ret);
	}

	if (handle->elem.flt_out) {
		ret = vrpc_sendRPC_param(handle->userID, VIDEO_RPC_ToAgent_Flush, handle->flt_out, 0);
		IF_ERR_RET(ret);
	}

	if (handle->bVTPPort)
		notifyDemuxFlushed(handle);

	return 0;
}

int vdec_flt_stop(vdec_handle_t *handle)
{
	int ret;

	if (!handle)
		return -EINVAL;

	if (!handle->elem.enable) {
		dev_err(handle->dev, "%s: no elem\n", __func__);
		return -EPERM;
	}

	if (handle->state == ST_STOP)
		return 0;

	if (handle->userType == V4L2_EXT_VDEC_USERTYPE_DTV) {
		if (handle->elem.flt_dec) {
			ret = vrpc_sendRPC_param(handle->userID, VIDEO_RPC_ToAgent_Stop, handle->flt_dec, 0);
			IF_ERR_RET(ret);
		}
		if (handle->elem.flt_out) {
			vdec_clearVideo(handle);
			vdec_connectDisplay(handle, VO_VIDEO_PLANE_NONE, ((handle->userType == V4L2_EXT_VDEC_USERTYPE_DTV)? 1: 0), 1);
			ret = vrpc_sendRPC_param(handle->userID, VIDEO_RPC_ToAgent_Stop, handle->flt_out, 0);
			IF_ERR_RET(ret);
		}
	}
	else {
		ret = vdec_flt_sendCmd(handle, VIDEO_RPC_ToAgent_Stop);
		IF_ERR_RET(ret);
	}

	handle->state = ST_STOP;
	handle->dtv_vdec_state = V4L2_EXT_VDEC_STATE_STOPPED;

	return 0;
}

int vdec_flt_destroy(vdec_handle_t *handle)
{
	int ret;

	if (!handle)
		return -EINVAL;

	if (!handle->elem.enable) {
		dev_err(handle->dev, "%s: no elem\n", __func__);
		return -EPERM;
	}

	ret = vdec_flt_sendCmd(handle, VIDEO_RPC_ToAgent_Destroy);
	IF_ERR_RET(ret);

	return 0;
}

int vdec_flt_exit(vdec_handle_t *handle)
{
	int ret;

	if (!handle)
		return -EINVAL;

	if (!handle->elem.enable) {
		dev_err(handle->dev, "%s: no elem\n", __func__);
		return -EPERM;
	}

	if (handle->elem.thread) {
		ret = exitThread(handle);
		IF_ERR_RET(ret);
		handle->elem.thread = 0;
	}

	vdec_flt_pause(handle);
	vdec_flt_stop(handle);
	vdec_flt_destroy(handle);

	if (handle->elem.msgQ) {		
		handle->elem.msgQ = 0;
	}

	if (handle->elem.userDataQ) {
		/* NOTE:
		 * instanceID:	+2/-2 indicate init/de-init vdec port 1
		 * 		+1/-1 indicate init/de-init vdec port 0
		 * 		instanceID should not be 0
		 */
		ret = vrpc_postRingBuf(handle->userID,
			-(1 + (handle->bChannel? handle->channel : 0)),
			handle->userDataQ->hdr.addr);
		IF_ERR_RET(ret);		
		handle->elem.userDataQ = 0;
	}

    if(handle->QIndex != -1) 
    {
      vcodec_putMsgQ(handle->device, handle->QIndex);	
      handle->QIndex = -1;
    } 

	if (handle->elem.bitstreamQ) {
		vfifo_exit(&handle->bitstreamQ);
		handle->elem.bitstreamQ = 0;
	}

	if (handle->elem.ibCmdQ) {
		vfifo_exit(&handle->ibCmdQ);
		handle->elem.ibCmdQ = 0;
	}

	if (handle->pRefClockCachedAddr) {
		dvr_free(handle->pRefClockCachedAddr);
		handle->pRefClockCachedAddr = 0;
		handle->pRefClockNonCachedLower = 0;
		handle->nRefClockPhyAddr = 0;
	}

	ret = vrpc_unregisterService(handle->userID, handle->userID);
	IF_ERR_RET(ret);

	#if 0
	/* flush pending events */
	struct v4l2_event event;
	while (v4l2_event_dequeue(&handle->fh, &event, 1) == 0) {}
	#endif

	handle->nPicInfoIn =
	handle->nPicInfoOut=
	handle->nUserDataIn=
	handle->nUserDataOut=0;

	handle->Last_frminfo_PTS =
	handle->Last_picinfo_PTS = 0;

	handle->elem.flt_dec =
	handle->elem.flt_out =
	handle->elem.enable = 0;

	handle->state = 0;
	handle->bDirectMode = 0;
	handle->bDripDecode = 0;

	return 0;
}

int vdec_initHandle(vdec_handle_t *handle)
{
	int userPID, userSockfd;
	const vcodec_pixfmt_t *inst;

	userPID = current->pid;
	userSockfd = current->files->next_fd - 1; /* peek current fd */
	handle->userID = vrpc_generateUserID(userPID, userSockfd);
	vdec_setUserType(handle, V4L2_EXT_VDEC_USERTYPE_DTV);
	mutex_init(&handle->thread_lock);
	INIT_LIST_HEAD(&handle->buf_list);
	INIT_LIST_HEAD(&handle->reg_buf_list);

	if (!(inst = vdec_getOutPixelFormat(V4L2_PIX_FMT_H264)))
		return -EINVAL;

	handle->pixfmt_out.pixelformat = inst->pixelformat;
	handle->pixfmt_out.num_planes = inst->num_planes;
	handle->pixfmt_out.field = V4L2_FIELD_NONE;
	handle->pixfmt_out.colorspace = V4L2_COLORSPACE_REC709;
	handle->pixfmt_out.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	handle->pixfmt_out.quantization = V4L2_QUANTIZATION_DEFAULT;
	handle->pixfmt_out.xfer_func = V4L2_XFER_FUNC_DEFAULT;

	inst = vdec_getCapPixelFormat(V4L2_PIX_FMT_NV12);

	handle->pixfmt_cap.pixelformat = inst->pixelformat;
	handle->pixfmt_cap.num_planes = inst->num_planes;
	handle->pixfmt_cap.colorspace = V4L2_COLORSPACE_REC709;
	handle->pixfmt_cap.field = V4L2_FIELD_NONE;
	handle->pixfmt_cap.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	handle->pixfmt_cap.quantization = V4L2_QUANTIZATION_DEFAULT;
	handle->pixfmt_cap.xfer_func = V4L2_XFER_FUNC_DEFAULT;

	handle->dtv_vdec_state = V4L2_EXT_VDEC_STATE_INIT;

	dev_info(handle->dev, "init handle %p userID %x\n", handle, handle->userID);

	return 0;
}

int vdec_exitHandle(vdec_handle_t *handle)
{
	if (!handle)
		return -EINVAL;

	if (handle->elem.enable) {

		dev_warn(handle->dev, "handle is closed abnormally. send Recovery Cmds\n");

		#if 0
		/* TODO: video firmware support port range */
		vrpc_sendRecoveryCmds((handle->userID & 0xffff0000), (handle->userID | 0xffff));
		handle->elem.flt_dec = handle->elem.flt_out = 0;
		#endif

		vdec_flt_exit(handle);
	}

	if (handle->bVTPPort) {
		vdec_setVTPBuffer(handle, -1);
		vdec_setVTPPort(handle, -1);
	}

	vdec_setChannel(handle, -1);

	vrpc_releaseMemory(handle->userID);

	dev_info(handle->dev, "exit handle %p userID %x\n", handle, handle->userID);

	return 0;
}

int vdec_streamon(vdec_handle_t *handle)
{
	int ret;
	vcodec_buffer_t *buf;

	if (handle->userType == V4L2_EXT_VDEC_USERTYPE_DTV) {
		if (!handle->bChannel || !handle->bOutPixFmt) {
			dev_err(handle->dev, "%s: no channel/OutPixFmt\n", __func__);
			return -EPERM;
		}

		if (handle->bVTPPort && !handle->bSource) {
			ret = vdec_setVTPBuffer(handle, handle->vtp_port);
			#ifdef IGNORE_DDTS_INVALID_INPUT
			if (ret == -ESTRPIPE)
				return 0;
			#endif
			IF_ERR_RET(ret);
		}
	}

	if (!handle->elem.enable) {
		ret = vdec_flt_init(handle);
		IF_ERR_RET(ret);
		if (handle->bVTPPort)
			notifyDemuxFlushed(handle);
	}

	/* deliver all pending buffers */
	if (handle->elem.bufQ_in) {
		list_for_each_entry (buf, &handle->buf_list, list) {
			if (!buf->nIn)
				vcap_fifo_qbuf(&handle->bufQ_in, buf);
		}
	}

	vdec_connectDisplay(handle, VO_VIDEO_PLANE_V1, ((handle->userType == V4L2_EXT_VDEC_USERTYPE_DTV)? 1: 0), 0);

	ret = vdec_flt_run(handle);
	IF_ERR_RET(ret);

	return 0;
}

int vdec_streamoff(vdec_handle_t *handle)
{
	if (handle->elem.enable)
		vdec_flt_exit(handle);

	if (handle->bVTPPort) {
		vdec_setVTPBuffer(handle, -1);
	}

	/* reset some settings */
	handle->HFR_type = 0;

	return 0;
}

int vdec_setUserType(vdec_handle_t *handle, int userType)
{
	if (!handle)
		return -EINVAL;

	if (handle->elem.enable) {
		dev_err(handle->dev, "%s: elem is enabled\n", __func__);
		return -EPERM;
	}

	switch (userType) {
		case V4L2_EXT_VDEC_USERTYPE_DTV:
			handle->streaming_out = 0;
			handle->streaming_cap = 0;
			break;
		case V4L2_EXT_VDEC_USERTYPE_MEDIA_TUNNEL:
			handle->streaming_out = 0;
			handle->streaming_cap = 0;
			break;
		case V4L2_EXT_VDEC_USERTYPE_MEDIA_CAPTURE:
			handle->streaming_out = 0;
			handle->streaming_cap = 0;
			break;
		case V4L2_EXT_VDEC_USERTYPE_MEDIA_CAPTURE_M2M:
			handle->streaming_out = 0;
			handle->streaming_cap = 1;
			break;
		default:
			dev_err(handle->dev, "%s: unknown userType %d", __func__, userType);
			return -EINVAL;
	}

	handle->userType = userType;
	v4l2_m2m_set_src_buffered(handle->m2m_ctx, !handle->streaming_out);
	v4l2_m2m_set_dst_buffered(handle->m2m_ctx, !handle->streaming_cap);

	return 0;
}

int vdec_setChannel(vdec_handle_t *handle, int channel)
{
	if (!handle || channel >= VDEC_CONF_MAX_CHANNEL)
		return -EINVAL;

	/* no change */
	if (TO_SIGNED(handle->bChannel, handle->channel) == channel)
		return 0;

	if (channel >= 0 && vcodec_getDecByChannel(handle->device, channel)) {
		dev_err(handle->dev, "%s: channel %d is occupied", __func__, channel);
		return -EPERM;
	}

	/* release old */
	if (handle->bChannel) {
		vdec_delAttr(handle, &handle->vattr_channel);
		handle->bChannel = 0;
	}

	/* apply new */
	if (channel >= 0) {
		handle->channel = channel;
		handle->bChannel = 1;
		vdec_addChannelAttr(handle, &handle->vattr_channel, channel);
	}

	dev_info(handle->dev, "channel = %d\n", channel);

	return 0;
}

int vdec_setVTPPort(vdec_handle_t *handle, int port)
{
	if (!handle || port > VDEC_CONF_MAX_VTPPORT)
		return -EINVAL;

	if (!handle->bChannel) {
		dev_err(handle->dev, "%s: no channel\n", __func__);
		return -EPERM;
	}

	/* no change */
	if (TO_SIGNED(handle->bVTPPort, handle->vtp_port) == port)
		return 0;

	handle->vtp_port = port;
	handle->bVTPPort = (port >= 0);

	dev_info(handle->dev, "%s: vtp_port %d\n", __func__, port);

	return 0;
}

int vdec_setVTPBuffer(vdec_handle_t *handle, int port)
{
	int ret;
	struct dmx_buff_info_t info;

	if (!handle || port >= VDEC_CONF_MAX_VTPPORT)
		return -EINVAL;

	if (!handle->bChannel) {
		dev_err(handle->dev, "%s: no channel\n", __func__);
		return -EPERM;
	}

	/* no change */
	if (TO_SIGNED(handle->bSource, handle->vtp_port) == port)
		return 0;

	if (!__weakref_rdvb_dmx_ctrl_releaseBuffer || !__weakref_rdvb_dmx_ctrl_getBufferInfo) {
		dev_err(handle->dev, "%s: no rdvb_dmx_ctrl symbols\n", __func__);
		return -ENOSYS;
	}

	/* release old */
	if (handle->bSource) {
		iounmap(handle->pVTPRefClk);
		handle->pVTPRefClk = NULL;
		__weakref_rdvb_dmx_ctrl_releaseBuffer(PIN_TYPE_VTP, handle->vtp_port,
			HOST_TYPE_VDEC, handle->channel);
		handle->bSource = 0;
	}

	/* apply new */
	if (port >= 0) {
		ret = __weakref_rdvb_dmx_ctrl_getBufferInfo(PIN_TYPE_VTP, port,
			HOST_TYPE_VDEC, handle->channel, &info);
		if (ret || !info.bsPhyAddr || !info.ibPhyAddr || !info.refClockPhyAddr) {
			dev_err(handle->dev, "%s: getBufferInfo ret %d\n", __func__, ret);
			return -ESTRPIPE;
		}

		handle->src.bs_hdr_addr = info.bsPhyAddr;
		handle->src.ib_hdr_addr = info.ibPhyAddr;
		handle->src.refclk_addr = info.refClockPhyAddr;
		dev_info(handle->dev, "bs 0x%x ib 0x%x refclk 0x%x\n",
			handle->src.bs_hdr_addr,
			handle->src.ib_hdr_addr,
			handle->src.refclk_addr);

		handle->pVTPRefClk = ioremap_nocache(handle->src.refclk_addr, sizeof(REFCLOCK));
		handle->vtp_port = port;
		handle->bSource = 1;
	}

	dev_info(handle->dev, "%s: vtp_port %d\n", __func__, port);

	return 0;
}

int vdec_setDecMode(vdec_handle_t *handle, int mode)
{
	int ret;

	if (handle->elem.flt_dec) {
		ret = vrpc_sendRPC_payload_va(handle->userID,
			VIDEO_RPC_DEC_ToAgent_SetDecodeMode,
			VIDEO_RPC_DEC_SET_DECODEMODE,
				.instanceID = handle->flt_dec,
				.mode = getDecMode(mode),
			);
		IF_ERR_RET(ret);
	}

	handle->decode_mode = mode;
	handle->pending_ops_decode_mode = (!handle->elem.flt_dec);
	dev_info(handle->dev, "decode_mode = %d\n", mode);

	return 0;
}

int vdec_setDripDecodeMode(vdec_handle_t *handle, int mode)
{
	if (!handle) {
		pr_err("-- %s fail: no handle, line %d\n", __FUNCTION__, __LINE__);
		return -EINVAL;
	}
	dev_info(handle->dev, "++ %s line %d\n", __FUNCTION__, __LINE__);

	handle->bDripDecode = mode; //0: normal 1: I-only

	dev_info(handle->dev, "%s mode = %d\n", __FUNCTION__, mode);

	dev_info(handle->dev, "-- %s line %d\n", __FUNCTION__, __LINE__);
	return 0;	
}

int vdec_setHighFrameRateType(vdec_handle_t *handle, int HFR_type)
{
	handle->HFR_type = HFR_type;

	if (HFR_type <= 0)
		return 0;
	else
		return -1;
}

int vdec_setTemporalIDMax(vdec_handle_t *handle, int temporal_id_max)
{
	int ret;

	if (handle->elem.flt_dec) {
		ret = vrpc_sendRPC_payload_va(handle->userID,
			VIDEO_DEC_RPC_ToAgent_Set_TempIDMax,
			VIDEO_RPC_DEC_SET_TEMPORAL_ID_MAX,
				.instanceID = handle->flt_dec,
				.temporal_id_max = temporal_id_max,
			);
		IF_ERR_RET(ret);
	}

	handle->temporal_id_max = temporal_id_max;
	handle->pending_ops_temporal_id_max = (!handle->elem.flt_dec);
	dev_info(handle->dev, "temporal_id_max = %d\n", temporal_id_max);

	return 0;
}

int vdec_setEventFlag(vdec_handle_t *handle, int eventID, int val)
{
	switch(eventID) {
		case V4L2_SUB_EXT_VDEC_FRAME:
			handle->event.bFrmInfo = val;
			break;
		case V4L2_SUB_EXT_VDEC_PICINFO:
			handle->event.bPicInfo = val;
			break;
		case V4L2_SUB_EXT_VDEC_USERDATA:
			handle->event.bUserData = val;
			break;
	}

	return 0;
}

int vdec_setLowDelay(vdec_handle_t *handle, int mode)
{
	int ret;

	if (handle->elem.flt_dec) {
		ret = vrpc_sendRPC_payload_va(handle->userID,
			VIDEO_RPC_ToAgent_ConfigLowDelay,
			VIDEO_RPC_LOW_DELAY,
				.mode = mode,
				.instanceID = handle->flt_dec,
			);
		IF_ERR_RET(ret);
	}

	handle->low_delay = mode;
	handle->pending_ops_low_delay = (!handle->elem.flt_dec);
	dev_info(handle->dev, "low_delay = %d\n", mode);

	return 0;
}

int vdec_setDirectMode(vdec_handle_t *handle, int mode)
{
	if (!handle)
		return -EINVAL;

	if (handle->elem.enable) {
		dev_err(handle->dev, "%s: elem is enabled\n", __func__);
		return -EPERM;
	}

	handle->bDirectMode = 1;
	handle->bDirectDataOn = 0;

	return 0;
}

int vdec_writeESdata(vdec_handle_t *handle, void __user *userptr, int size)
{
	int ret = 0;

	if (!handle || !userptr || !size)
		return -EINVAL;

	if (!handle->elem.flt_dec) {
		dev_err(handle->dev, "%s: no flt_dec\n", __func__);
		return -EPERM;
	}

	if (!handle->bDirectMode) {
		dev_err(handle->dev, "%s: not DirectMode\n", __func__);
		return -EPERM;
	}

	if (!handle->bDirectDataOn) {
		NEW_SEG new_seg;
		DECODE decode;

		new_seg.header.type = INBAND_CMD_TYPE_NEW_SEG;
		new_seg.header.size = sizeof(new_seg);
		new_seg.wPtr = handle->bitstreamQ.base;
		ret = vfifo_write(&handle->ibCmdQ, &new_seg, sizeof(NEW_SEG));
		IF_ERR_RET(ret);

		decode.header.type = INBAND_CMD_TYPE_DECODE;
		decode.header.size = sizeof(decode);
		decode.RelativePTSH = 0;
		decode.RelativePTSL = 0;
		decode.PTSDurationH = (-1LL) >> 32;
		decode.PTSDurationL = (-1LL);
		decode.skip_GOP = 0;
		decode.mode = NORMAL_DECODE;
		decode.isHM91 = 0;
		ret = vfifo_write(&handle->ibCmdQ, &decode, sizeof(DECODE));
		IF_ERR_RET(ret);

		handle->bDirectDataOn = 1;
	}

	if (size > vfifo_space(&handle->bitstreamQ))
		return -EAGAIN;

	ret = vfifo_write_from_user(&handle->bitstreamQ, userptr, size);
	if (ret) {
		dev_err(handle->dev, "%s: vfifo_write_from_user ret %d\n", __func__, ret);
		return ret;
	}

	return 0;
}

int vdec_reset(vdec_handle_t *handle)
{
	if (!handle)
		return -EINVAL;

	handle->decode_mode = V4L2_EXT_VDEC_DECODE_ALL_FRAME;

	if (handle->elem.enable)
		vdec_flt_flush(handle);

	dev_info(handle->dev, "reset 0x%08x\n", handle->userID);

	return 0;
}

int vdec_getPicInfo(vdec_handle_t *handle, void __user *userptr, int size)
{
	int ret, type;
	struct v4l2_ext_picinfo_msg info;
	vfifo_t *msgQ;
	VIDEO_DEC_PIC_MSG bufOnWrap, *msg = NULL;
	unsigned int rd, wr;

	if (!handle || !userptr || size < sizeof(info))
		return -EINVAL;

	if (handle->nPicInfoIn <= handle->nPicInfoOut)
		return -1;

	msgQ = handle->msgQ;
	rd = msgQ->rd[RD_PIC];
	wr = msgQ->wr;
	ret = 0;

	/* find next */
	while (rd != wr) {
		vfifo_peek(msgQ, RD_PIC, &bufOnWrap, sizeof(bufOnWrap), (void**)&msg);
		type = msg->msgType;
		if (type == VIDEO_Frm_Info) {
			vfifo_skip(msgQ, RD_PIC, sizeof(VIDEO_DEC_FRM_MSG));
			rd = msgQ->rd[RD_PIC];
		} else if (type == VIDEO_Pic_Info) {
			/* GOT IT */
			break;
		} else {
			dev_err(handle->dev, "%s: unknown type %d rd 0x%x wr 0x%x\n",
				__func__, type, rd, wr);
			vfifo_syncRd(msgQ, RD_PIC, wr);
			return -1;
		}
	}

	if (rd == wr) {
		dev_err(handle->dev, "%s: no PicInfo\n", __func__);
		vfifo_syncRd(msgQ, RD_PIC, wr);
		return -1;
	}

	info.version = VDEC_VERSION;
	info.channel = handle->channel;
	info.frame_rate = msg->frame_rate;
	info.aspect_ratio = msg->ratio_info >> 16;
	info.h_size = msg->resol >> 16;
	info.v_size = msg->resol & 0xffff;
	info.bitrate = msg->bit_rate;
	info.afd = msg->afd_3d >> 16;
	info.progressive_seq = msg->prog_info >> 16;
	info.progressive_frame = msg->prog_info & 0xffff;
	info.active_x = msg->actXY >> 16;
	info.active_y = msg->actXY & 0xffff;
	info.active_w = msg->actWH >> 16;
	info.active_h = msg->actWH & 0xffff;
	info.display_h_size = msg->disp_resol >> 16;
	info.display_v_size = msg->disp_resol & 0xffff;
	info.aspect_ratio_idc = msg->ratio_info & 0xffff;
	info.sar_width = msg->sarWH >> 16;
	info.sar_height = msg->sarWH & 0xffff;
	info.three_d_info = msg->afd_3d & 0xffff;
	info.colour_primaries = msg->color_transf >> 16;
	info.transfer_chareristics = msg->color_transf & 0xffff;
	info.matrix_coeffs = msg->coeffs_overscan >> 16;
	info.display_primaries_x0 = msg->dispXY0 >> 16;
	info.display_primaries_y0 = msg->dispXY0 & 0xffff;
	info.display_primaries_x1 = msg->dispXY1 >> 16;
	info.display_primaries_y1 = msg->dispXY1 & 0xffff;
	info.display_primaries_x2 = msg->dispXY2 >> 16;
	info.display_primaries_y2 = msg->dispXY2 & 0xffff;
	info.white_point_x = msg->whitePointXY >> 16;
	info.white_point_y = msg->whitePointXY & 0xffff;
	info.max_display_mastering_luminance = msg->maxL;
	info.min_display_mastering_luminance = msg->minL;
	info.overscan_appropriate = msg->coeffs_overscan & 0xffff;
	info.video_full_range_flag = msg->videoFullRangeFlag;
	info.hdr_transfer_characteristic_idc = msg->hdrTransferCharacteristics;

	if (copy_to_user(userptr, &info, sizeof(struct v4l2_ext_picinfo_msg)))
		ret = -EFAULT;

	/* finalize */
	vfifo_skip(msgQ, RD_PIC, sizeof(VIDEO_DEC_PIC_MSG));
	handle->nPicInfoOut++;

	return ret;
}

int vdec_getPicInfo_ext(vdec_handle_t *handle, void __user *userptr, int size)
{
	int ret, type;
	struct v4l2_ext_picinfo_msg_ext info;
	struct v4l2_fract framerate;
	vfifo_t *msgQ;
	VIDEO_DEC_PIC_MSG bufOnWrap, *msg = NULL;
	unsigned int rd, wr;

	if (!handle || !userptr || size < sizeof(info))
		return -EINVAL;

	if (handle->nPicInfoIn <= handle->nPicInfoOut)
		return -1;

	msgQ = handle->msgQ;
	rd = msgQ->rd[RD_PIC];
	wr = msgQ->wr;
	ret = 0;

	/* find next */
	while (rd != wr) {
		vfifo_peek(msgQ, RD_PIC, &bufOnWrap, sizeof(bufOnWrap), (void**)&msg);
		type = msg->msgType;
		if (type == VIDEO_Frm_Info) {
			vfifo_skip(msgQ, RD_PIC, sizeof(VIDEO_DEC_FRM_MSG));
			rd = msgQ->rd[RD_PIC];
		} else if (type == VIDEO_Pic_Info) {
			/* GOT IT */
			break;
		} else {
			dev_err(handle->dev, "unknown type %d rd 0x%x wr 0x%x\n", type, rd, wr);
			vfifo_syncRd(msgQ, RD_PIC, wr);
			return -1;
		}
	}

	if (rd == wr) {
		dev_err(handle->dev, "no PicInfo\n");
		vfifo_syncRd(msgQ, RD_PIC, wr);
		return -1;
	}

	info.channel = handle->channel;
	info.aspect_ratio = msg->ratio_info >> 16;
	info.h_size = msg->resol >> 16;
	info.v_size = msg->resol & 0xffff;
	info.bitrate = msg->bit_rate;
	info.afd = msg->afd_3d >> 16;
	info.progressive_seq = msg->prog_info >> 16;
	info.progressive_frame = msg->prog_info & 0xffff;
	info.active_x = msg->actXY >> 16;
	info.active_y = msg->actXY & 0xffff;
	info.active_w = msg->actWH >> 16;
	info.active_h = msg->actWH & 0xffff;
	info.display_h_size = msg->disp_resol >> 16;
	info.display_v_size = msg->disp_resol & 0xffff;
	info.aspect_ratio_idc = msg->ratio_info & 0xffff;
	info.sar_width = msg->sarWH >> 16;
	info.sar_height = msg->sarWH & 0xffff;
	info.three_d_info = msg->afd_3d & 0xffff;
	info.colour_primaries = msg->color_transf >> 16;
	info.transfer_chareristics = msg->color_transf & 0xffff;
	info.matrix_coeffs = msg->coeffs_overscan >> 16;
	info.display_primaries_x0 = msg->dispXY0 >> 16;
	info.display_primaries_y0 = msg->dispXY0 & 0xffff;
	info.display_primaries_x1 = msg->dispXY1 >> 16;
	info.display_primaries_y1 = msg->dispXY1 & 0xffff;
	info.display_primaries_x2 = msg->dispXY2 >> 16;
	info.display_primaries_y2 = msg->dispXY2 & 0xffff;
	info.white_point_x = msg->whitePointXY >> 16;
	info.white_point_y = msg->whitePointXY & 0xffff;
	info.max_display_mastering_luminance = msg->maxL;
	info.min_display_mastering_luminance = msg->minL;
	info.overscan_appropriate = msg->coeffs_overscan & 0xffff;
	info.video_full_range_flag = msg->videoFullRangeFlag;
	info.hdr_transfer_characteristic_idc = msg->hdrTransferCharacteristics;
	framerate.numerator = getFramerateBycode(msg->frame_rate);
	framerate.denominator = 1000;
	info.frame_rate = framerate;

	if (copy_to_user(userptr, &info, sizeof(struct v4l2_ext_picinfo_msg_ext)))
		ret = -EFAULT;

	/* finalize */
	vfifo_skip(msgQ, RD_PIC, sizeof(VIDEO_DEC_PIC_MSG));
	handle->nPicInfoOut++;

	return ret;
}

int vdec_getUserData(vdec_handle_t *handle, void __user *userptr, int size)
{
	int ret, len;
	vfifo_t *userDataQ;
	cc_pack_t bufOnWrap, *pack = NULL;
	unsigned int rd, wr;

	if (!handle || !userptr || !size)
		return -EINVAL;

	if (handle->nUserDataIn <= handle->nUserDataOut) {
		dev_err(handle->dev, "no user-data\n");
		return -EPERM;
	}

	userDataQ = handle->userDataQ;
	rd = userDataQ->rd[0];
	wr = userDataQ->wr;
	ret = 0;

	if (rd == wr) {
		dev_err(handle->dev, "no user data\n");
		return -1;
	}

	vfifo_peek(userDataQ, 0, &bufOnWrap, sizeof(bufOnWrap), (void**)&pack);

	len = pack->hdr.size;
	if (size < len) {
		dev_err(handle->dev, "user buffer is not enough (%d < %d)\n", size, pack->hdr.size);
		len = size;
		ret = -ENOMEM;
	}

	if (copy_to_user(userptr, pack->data + sizeof(VIDEO_CC_CALLBACK_HEADER), len))
		ret = -EFAULT;

	/* finalize */
	vfifo_skip(userDataQ, 0, sizeof(cc_pack_t));
	handle->nUserDataOut++;

	return ret;
}

int vdec_getStreamInfo(vdec_handle_t *handle, struct v4l2_ext_stream_info *stream_info)
{
	int ret;
	VIDEO_RPC_DEC_SEQ_INFO info;

	if (!handle || !stream_info)
		return -EINVAL;

	if (!handle->elem.flt_dec) {
		dev_err(handle->dev, "no flt_dec");
		return -EPERM;
	}

	ret = vrpc_sendRPC_result_param(handle->userID,
		VIDEO_RPC_DEC_ToAgent_GetVideoSequenceInfo,
		&info, handle->flt_dec, 0
		);
	IF_ERR_RET(ret);

	stream_info->h_size = info.hor_size;
	stream_info->v_size = info.ver_size;
	stream_info->video_format =
		(info.hor_size >= 1920 && info.ver_size >= 1080)? 0:
		(info.hor_size >= 1280 && info.ver_size >= 720)? 1:
		2;
	stream_info->frame_rate =
		(info.frame_rate == 23970)? 1:
		(info.frame_rate == 24000)? 2:
		(info.frame_rate == 25000)? 3:
		(info.frame_rate == 29970)? 4:
		(info.frame_rate == 30000)? 5:
		(info.frame_rate == 50000)? 6:
		(info.frame_rate == 59940)? 7:
		(info.frame_rate == 60000)? 8:
		(info.frame_rate == 99880)? 9:
		(info.frame_rate == 100000)? 9:
		(info.frame_rate == 119880)? 10:
		(info.frame_rate == 120000)? 10:
		0;
	stream_info->aspect_ratio = info.aspect_ratio;
	stream_info->bit_rate = info.bit_rate;
	stream_info->sequence_header_scan_type = info.prog_sequence;
	stream_info->frame_scan_type = info.isProg;
	stream_info->picture_type = info.pic_type;
	stream_info->temporal_reference = info.temp_ref;
	stream_info->picture_struct = info.pic_struct;
	stream_info->top_field_first = info.top_ff;
	stream_info->repeat_field_first = info.rept_ff;

	return 0;
}

int vdec_getDecoderStatus(vdec_handle_t *handle, struct v4l2_ext_decoder_status *dec_stat)
{
	if (!handle || !dec_stat)
		return -EINVAL;

	if (!handle->elem.flt_dec) {
		dev_err(handle->dev, "%s: no flt_dec\n", __func__);
		#ifdef IGNORE_DDTS_INVALID_INPUT
		return 0;
		#endif
		return -EPERM;
	}

	dec_stat->vdec_state = handle->dtv_vdec_state;
	dec_stat->codec_type = getCodecType(handle->pixfmt_out.pixelformat);
	dec_stat->still_picture = 0;
	dec_stat->drip_decoding = handle->bDripDecode;
	dec_stat->es_data_size = vfifo_cnt(&handle->bitstreamQ, 0);
	dec_stat->mb_dec_error_count = 0;

	if (handle->pVTPRefClk)
	{
		int64_t audioPTS, videoPTS;
		REFCLOCK *pVTPRefClk = (REFCLOCK*)handle->pVTPRefClk;
		audioPTS = pVTPRefClk->audioSystemPTS;
		videoPTS = pVTPRefClk->videoSystemPTS;

		if (audioPTS > videoPTS)
			dec_stat->av_libsync = audioPTS - videoPTS > 4500 ? 0 : 1;
		else
			dec_stat->av_libsync = videoPTS - audioPTS > 4500 ? 0 : 1;

		if (videoPTS == -1 || audioPTS == -1)
		{
			dec_stat->pts_matched = 0;
		}
		else
		{
			if (audioPTS > videoPTS)
				dec_stat->pts_matched = audioPTS - videoPTS > 15015 ? 0 : 1;
			else
				dec_stat->pts_matched = videoPTS - audioPTS > 15015 ? 0 : 1;
		}

		dec_stat->decoded_pic_num = pVTPRefClk->decodedQueueDepth;
	}
	else
	{
		dec_stat->av_libsync = 0;
		dec_stat->pts_matched = 0;
		dec_stat->decoded_pic_num = 0;
	}

	return 0;
}

int vdec_getVideoInfo(vdec_handle_t *handle, struct v4l2_control *ctrl)
{
	int ret;
	struct v4l2_ext_stream_info *stream_info;

	if (!handle || !ctrl)
		return -EINVAL;

	if (!handle->elem.flt_dec) {
		dev_err(handle->dev, "%s: no flt_dec\n", __func__);
		#ifdef IGNORE_DDTS_INVALID_INPUT
		return 0;
		#endif
		return -EPERM;
	}

	stream_info = &handle->stream_info;

	ret = vdec_getStreamInfo(handle, stream_info);
	IF_ERR_RET_VA(ret, "no StreamInfo");

	switch (ctrl->id) {
		case VDEC_V_SIZE:
			ctrl->value = stream_info->v_size;
			break;
		case VDEC_H_SIZE:
			ctrl->value = stream_info->h_size;
			break;
		case VDEC_FRAME_RATE:
			ctrl->value = getFramerateBycode(stream_info->frame_rate) / 100;
			break;
		case VDEC_ASPECT_RATIO:
			ctrl->value = stream_info->aspect_ratio;
			break;
		case VDEC_PROGRESSIVE_SEQUENCE:
			ctrl->value = stream_info->sequence_header_scan_type;
			break;
		default:
			dev_err(handle->dev, "undefine video info\n");
			return -ENOENT;
	}

	return 0;
}

int vdec_dripDecodePicture(vdec_handle_t *handle, void *ptr, int size)
{
	int ret;
	int dripType;
	unsigned int *dripData;

	if (!handle || !ptr || size <=0){
		pr_err("-- %s line %d\n", __FUNCTION__, __LINE__);
		return -EINVAL;
	}

	//alloc buffer for Dripdata
	dripData = (unsigned int *)kmalloc(size, GFP_KERNEL);
	if (!dripData) {
		dev_info(handle->dev, "-- %s kmalloc fail line %d\n", __FUNCTION__, __LINE__);
		return -EFAULT;
	}

	if (copy_from_user((void*)dripData, (void *)ptr, size)){
		dev_info(handle->dev, "-- %s copy fail line %d\n", __FUNCTION__, __LINE__);
		kfree(dripData);
		return -EFAULT;
	}

	dripType = handle->bDripDecode ? DRIP_I_ONLY_DECODE : NORMAL_DECODE;

	dev_info(handle->dev, "%s dripType %d, ptr %x size %x\n", __FUNCTION__, dripType, *dripData, size);
	ret = vdec_startDripDec(handle, (void *)dripData, size, dripType, getCodecType(handle->pixfmt_out.pixelformat));
	if (ret) {
		dev_err(handle->dev, "-- %s fail: ret %d line %d\n", __FUNCTION__, ret, __LINE__);
		kfree(dripData);
		return ret;
	}

	kfree(dripData);
	dev_info(handle->dev, "-- %s line %d\n", __FUNCTION__, __LINE__);
	return ret;
}
int vdec_startDripDec(vdec_handle_t *handle, void *pDripData, int size, int dripType, int codecType)
{
	int ret;
	NEW_SEG new_seg;
	DECODE decode;
	EOS eos;
	dev_info(handle->dev, "++ %s line %d\n", __FUNCTION__, __LINE__);

	if (!handle->elem.enable) {
		ret = vdec_flt_init(handle);
		if (ret) {
			dev_err(handle->dev, "-- %s fail: ret %d line %d\n", __FUNCTION__, ret, __LINE__);
			handle->bDripDecode = 0;
			return ret;
		}
	}

	new_seg.header.type = INBAND_CMD_TYPE_NEW_SEG;
	new_seg.header.size = sizeof(new_seg);
	new_seg.wPtr = handle->bitstreamQ.base;
	ret = vfifo_write(&handle->ibCmdQ, &new_seg, sizeof(NEW_SEG));
	IF_ERR_RET(ret);

	decode.header.type = INBAND_CMD_TYPE_DECODE;
	decode.header.size = sizeof(decode);
	decode.RelativePTSH = 0;
	decode.RelativePTSL = 0;
	decode.PTSDurationH = (-1LL) >> 32;
	decode.PTSDurationL = (-1LL);
	decode.skip_GOP = 0;
	decode.mode = dripType;
	decode.isHM91 = 0;
	ret = vfifo_write(&handle->ibCmdQ, &decode, sizeof(DECODE));
	IF_ERR_RET(ret);

	eos.header.type = INBAND_CMD_TYPE_EOS;
	eos.header.size = sizeof(EOS);
	eos.eventID = 0;
	eos.wPtr = handle->bitstreamQ.base + size;
	ret = vfifo_write(&handle->ibCmdQ, &eos, sizeof(EOS));
	IF_ERR_RET(ret);

	ret = vfifo_write(&handle->bitstreamQ, pDripData, size);

	vdec_connectDisplay(handle, VO_VIDEO_PLANE_V1, ((handle->userType == V4L2_EXT_VDEC_USERTYPE_DTV)? 1: 0), 0);

	ret = vdec_flt_run(handle);
	if (ret) {
		dev_err(handle->dev, "-- %s fail: ret %d line %d\n", __FUNCTION__, ret, __LINE__);
		//handle->bDripDecode = 0;
		return ret;
	}

	dev_info(handle->dev, "-- %s line %d\n", __FUNCTION__, __LINE__);
	return 0;
}

int vdec_stopDripDec(vdec_handle_t *handle) //stopdrip in stream_off
{
	dev_info(handle->dev, "++ %s line %d\n", __FUNCTION__, __LINE__);

	handle->bDripDecode = 0;

	dev_info(handle->dev, "-- %s line %d\n", __FUNCTION__, __LINE__);
	return 0;
}

int vdec_saveDripDec(vdec_handle_t *handle, unsigned char *pDripData, int size, int dripType, int codecType, unsigned char **ppARGBdata, unsigned int *pDestSize, unsigned int *pDestWidth)
{
	int ret;
	VIDEO_VF_TYPE vf_type;
	VIDEO_STREAM_TYPE stream_type;
	VIDEO_RPC_DEC_PV_RESULT result;
	VIDEO_RPC_DEC_DRIP_RESULT drip_result;

	if (!handle)
		return -EINVAL;

	/* alloc bitstreamQ ringbuf */
	ret = vfifo_init(&handle->bitstreamQ, RINGBUFFER_STREAM, 0x100000, 1, VFIFO_F_RDONLY);
	IF_ERR_RET(ret);

	ret = vfifo_write(&handle->bitstreamQ, pDripData, size);
	IF_ERR_RET(ret);

	if (codecType == V4L2_PIX_FMT_MPEG2)
		vf_type = VF_TYPE_VIDEO_MPEG2_DECODER;
	else if (codecType == V4L2_PIX_FMT_H264)
		vf_type = VF_TYPE_VIDEO_H264_DECODER;
	else if (codecType == V4L2_PIX_FMT_AVS)
		vf_type = VF_TYPE_VIDEO_AVS_DECODER;
	else if (codecType == V4L2_PIX_FMT_HEVC)
		vf_type = VF_TYPE_VIDEO_H265_DECODER;
	else
		vf_type = VF_TYPE_VIDEO_MPEG2_DECODER;

	ret = vrpc_sendRPC_result_payload_va(handle->userID,
				VIDEO_RPC_DEC_ToAgent_ParseResolution,
				&result,
				VIDEO_RPC_DEC_BITSTREAM_BUFFER,
				  .bsBase = handle->bitstreamQ.base,
				  .bsSize = size,
				  .type   = vf_type,
				);
	IF_ERR_RET(ret);

	if (vf_type == VF_TYPE_VIDEO_MPEG2_DECODER)
		stream_type = VIDEO_STREAM_MPEG2;
	else if (vf_type == VF_TYPE_VIDEO_H264_DECODER)
		stream_type = VIDEO_STREAM_H264;
	else if (vf_type == VF_TYPE_VIDEO_AVS_DECODER)
		stream_type = VIDEO_STREAM_AVS;
	else if (vf_type == VF_TYPE_VIDEO_H265_DECODER)
		stream_type = VIDEO_STREAM_H265;

	ret = vrpc_sendRPC_result_payload_va(handle->userID,
				VIDEO_RPC_DEC_ToAgent_SetDripDec,
				&drip_result,
				VIDEO_RPC_DEC_DRIP_INFO,
				  .pDripData     = handle->bitstreamQ.base,
				  .dripSize      = size,
				  .dripType      = dripType,
				  .targetFormat  = 1,
				  .targetWidth   = result.width,
				  .targetHeight  = result.height,
				  .targetBufSize = result.width,
				  .pTargetAddr   = 0,
				  .codecType     = stream_type,
				);
	IF_ERR_RET(ret);

	if (drip_result.length > 0)
	{
		handle->pDripOutBufPhyAddr = drip_result.hr;
		handle->pDripNonCachedOutBuf = (unsigned char *)ioremap_nocache(drip_result.hr, drip_result.length);
		*ppARGBdata = handle->pDripNonCachedOutBuf;
		*pDestSize  = drip_result.length;
		*pDestWidth = result.width;
	}
	else
	{
		*pDestSize  =
		*pDestWidth = 0;
	}

	dev_info(handle->dev, "-- %s line %d\n", __FUNCTION__, __LINE__);
	return 0;
}

int vdec_saveDripDec_free(vdec_handle_t *handle, unsigned char *pARGBdata)
{
	int ret;
	VIDEO_RPC_DEC_DRIP_RESULT drip_result;

	if (!handle)
		return -EINVAL;

	if (!pARGBdata || pARGBdata != handle->pDripNonCachedOutBuf)
	{
		dev_err(handle->dev, "-- %s fail: line %d, drip out data buf\n", __FUNCTION__, __LINE__);
		return -EINVAL;
	}

	ret = vrpc_sendRPC_result_payload_va(handle->userID,
				VIDEO_RPC_DEC_ToAgent_SetDripDec,
				&drip_result,
				VIDEO_RPC_DEC_DRIP_INFO,
				  .targetFormat  = 1,
				  .targetBufSize = 0,
				  .pTargetAddr   = handle->pDripOutBufPhyAddr,
				);
	IF_ERR_RET(ret);

	if (drip_result.length > 0)
	{
		iounmap(pARGBdata);
		pARGBdata = 0;
		handle->pDripNonCachedOutBuf = 0;
		handle->pDripOutBufPhyAddr = 0;
	}
	else
	{
		dev_err(handle->dev, "-- %s fail: line %d, length %d\n", __FUNCTION__, __LINE__, drip_result.length);
		return -EINVAL;
	}

	dev_info(handle->dev, "-- %s line %d\n", __FUNCTION__, __LINE__);
	return 0;
}
