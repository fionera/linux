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

#ifndef __VCODEC_STRUCT_H__
#define __VCODEC_STRUCT_H__

#include <media/v4l2-device.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-v4l2.h>

#include "vcodec_conf.h"
#include "v4l2_vdec_ext.h"
#include "vutils/vfifo.h"

enum {
	ST_NONE = 0,
	ST_INIT,
	ST_RUN,
	ST_PAUSE,
	ST_STOP,
};

typedef struct vcodec_share_t {
	struct vcodec_device_t *vdec;
	struct vcodec_device_t *vcap;
} vcodec_share_t;

typedef struct vcodec_pixfmt_t {
	u32 pixelformat;
	u32 num_planes;
} vcodec_pixfmt_t;

typedef struct vcodec_device_t {
	struct platform_device *pdev;
	struct device *dev;
	struct v4l2_device v4l2_dev;
	struct video_device vdev;
	struct list_head list;
	struct list_head handle_list;
	struct mutex lock;
	vcodec_share_t *share;
	int port;
	int dma_carveout_type;

    /* msgQ and userDataQ */
    vfifo_t msgQ[VDEC_NUM_MSGQ];
	vfifo_t userDataQ[VDEC_NUM_MSGQ];
    char bQUsed[VDEC_NUM_MSGQ];	
    char bMsgQ[VDEC_NUM_MSGQ];
    char bUserDataQ[VDEC_NUM_MSGQ];

} vcodec_device_t;

typedef struct vcodec_buffer_dma_t {
	dma_addr_t addr;
	int size;
	int offset;
} vcodec_buffer_dma_t;

typedef struct vcodec_attr_t {
	struct device_attribute dev_attr;
	char name[64];
	int nr;
} vcodec_attr_t;

typedef struct vcodec_buffer_t {
	struct vb2_v4l2_buffer vb;
	struct list_head list;
	struct list_head reg_list;
	struct v4l2_ext_vcap_picobj picobj;
	vcodec_buffer_dma_t dma[2];
	vcodec_buffer_dma_t cmprs_hdr[2];
	unsigned int nIn;
	unsigned int nOut;
	unsigned int bufW;
	unsigned int bufH;
} vcodec_buffer_t;

typedef struct vdec_handle_t {

	struct v4l2_fh fh;
	struct v4l2_m2m_dev *m2m_dev;
	struct v4l2_m2m_ctx *m2m_ctx;
	struct vb2_alloc_ctx *alloc_ctx_out[VIDEO_MAX_PLANES];
	struct vb2_alloc_ctx *alloc_ctx_cap[VIDEO_MAX_PLANES];
	struct list_head list;
	struct list_head buf_list;
	struct list_head reg_buf_list;
	struct device *dev;
	vcodec_device_t *device;
	int userID;
	int userType;

	bool streaming_out;
	bool streaming_cap;
	bool streamon_out;
	bool streamon_cap;

	/* fmt */
	struct v4l2_pix_format_mplane pixfmt_out;
	struct v4l2_pix_format_mplane pixfmt_cap;

	/* elements */
	struct {
		unsigned int enable:1;
		unsigned int thread:1;
		unsigned int msgQ:1;
		unsigned int userDataQ:1;
		unsigned int bitstreamQ:1;
		unsigned int ibCmdQ:1;
		unsigned int bufQ_in:1;
		unsigned int bufQ_out:1;
		unsigned int flt_dec:1;
		unsigned int flt_out:1;
	} elem;

	/* thread related */
	struct mutex thread_lock;
	struct task_struct *thread;
	int thread_msecs;

	/* ringbuf related */
	vfifo_t *msgQ;
	vfifo_t *userDataQ;
	vfifo_t bitstreamQ;
	vfifo_t ibCmdQ;
	vfifo_t bufQ_in;
	vfifo_t bufQ_out;
    int QIndex;
    
	/* filter IDs */
	unsigned int flt_dec;
	unsigned int flt_out;

	/* attrs */
	vcodec_attr_t vattr_channel;

	/* NOTE: clear all data from __reset_start to __reset_end during reset */
	char __reset_start[0];

	unsigned int bChannel:1;
	unsigned int bVTPPort:1;
	unsigned int bSource:1;
	unsigned int bDisplay:1;
	unsigned int bFrameAdvance:1;
	unsigned int bFreeze:1;
	unsigned int bDual3D:1;
	unsigned int bFreerun:1;
	unsigned int bOutputIdx:1;
	unsigned int bOutPixFmt:1;
	unsigned int bDirectMode:1;
	unsigned int bDirectDataOn:1;
	unsigned int bDripDecode:1;
	unsigned int bSVP:1;

	/* pending operations */
	unsigned int pending_ops_display:1;
	unsigned int pending_ops_freerun:1;
	unsigned int pending_ops_display_delay:1;
	unsigned int pending_ops_lipsync_master:1;
	unsigned int pending_ops_stc_mode:1;
	unsigned int pending_ops_audio_channel:1;
	unsigned int pending_ops_fast_iframe_mode:1;
	unsigned int pending_ops_decode_mode:1;
	unsigned int pending_ops_low_delay:1;
	unsigned int pending_ops_temporal_id_max:1;

	/* subscribed events */
	struct {
		unsigned int bFrmInfo:1;
		unsigned int bPicInfo:1;
		unsigned int bUserData:1;
	} event;

	int channel;
	int vtp_port;
	int display;
	int display_delay;
	int decode_mode;
	int speed;
	int pre_speed;
	int vsync_threshold;
	int HFR_type;
	int temporal_id_max;
	int outputIdx;
	int state;
	int stc_mode;
	int lipsync_master;
	int audio_channel;
	int pvr_mode;
	int fast_iframe_mode;
	int low_delay;
	int dtv_vdec_state;

	/* source related */
	struct v4l2_ext_src_ringbufs src;
	void *pVTPRefClk;
	unsigned int nRefClockPhyAddr; //for dripdecode
	int *pRefClockCachedAddr; //for dripdecode
	void *pRefClockNonCachedLower; //for dripdecode

	/* stat */
	unsigned int nPicInfo;
	unsigned int nPicInfoIn;
	unsigned int nPicInfoOut;
	unsigned int nFrmInfo;
	unsigned int nUserData;
	unsigned int nUserDataIn;
	unsigned int nUserDataOut;

	unsigned int Last_frminfo_PTS;
	unsigned int Last_picinfo_PTS;

	struct v4l2_ext_stream_info stream_info;
	void *pPicMsg;

    unsigned int pDripOutBufPhyAddr;
    unsigned char *pDripNonCachedOutBuf;

	/* NOTE: clear all data from __reset_start to __reset_end during reset */
	char __reset_end[0];

} vdec_handle_t;

typedef struct vcap_handle_t {

	struct v4l2_fh fh;
	struct v4l2_m2m_dev *m2m_dev;
	struct v4l2_m2m_ctx *m2m_ctx;
	struct vb2_alloc_ctx *alloc_ctx[VIDEO_MAX_PLANES];
	struct list_head list;
	struct list_head buf_list;
	struct list_head reg_buf_list;
	struct device *dev;
	struct mutex queue_lock;
	vcodec_device_t *device;
	int userID;

	/* fmt */
	struct v4l2_pix_format_mplane pixfmt;

	/* thread related */
	struct mutex thread_lock;
	struct task_struct *thread;
	int thread_msecs;

	unsigned int max_width;
	unsigned int max_height;

	/* NOTE: clear all data from __reset_start to __reset_end during reset */
	char __reset_start[0];

	unsigned int bInputIdx:1;
	unsigned int bBufQ:1;

	unsigned int inputIdx;

	struct v4l2_captureparm cap_parm;

	/* bufQ */
	vfifo_t q_in;
	vfifo_t q_out;

	/* NOTE: clear all data from __reset_start to __reset_end during reset */
	char __reset_end[0];

} vcap_handle_t;

#endif
