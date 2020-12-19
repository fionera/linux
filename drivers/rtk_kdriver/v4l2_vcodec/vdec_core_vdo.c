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

#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>

#pragma scalar_storage_order big-endian
#include <rpc_common.h>
#include <InbandAPI.h>
#include <VideoRPC_System.h>
#include <VideoRPC_Agent.h>
#pragma scalar_storage_order default

#include "v4l2_vdec_ext.h"
#include "rdvb/rdvb-dmx/rdvb_dmx_ctrl.h"

#include "vcodec_conf.h"
#include "vcodec_def.h"
#include "vcodec_macros.h"
#include "vcodec_struct.h"
#include "vcodec_utils.h"
#include "vcodec_weakref.h"
#include "vdec_core.h"
#include "vdec_conv.h"

#include "vutils/vrpc.h"

int vdec_clearVideo(vdec_handle_t *handle)
{
	int ret;
	VO_COLOR voColor = {
		.isRGB = 1,
	};

	if (handle->elem.flt_out) {
		ret = vrpc_sendRPC_payload_va(handle->userID,
			VIDEO_RPC_VO_FILTER_ToAgent_ClearVideo,
			VIDEO_RPC_VO_FILTER_CLEAR_VIDEO,
				.instanceID = handle->flt_out,
				.fillColor  = voColor,
			);
		IF_ERR_RET(ret);
	}

	return 0;
}

int vdec_connectDisplay(vdec_handle_t *handle, int display, unsigned char realTimeSrc, unsigned char zeroBuffer)
{
	int ret, bDisplay;

	if (!handle)
		return -ENOENT;

	bDisplay = (display >= 0);

	if (handle->elem.flt_out) {
		ret = vrpc_sendRPC_payload_va(handle->userID,
			VIDEO_RPC_VO_FILTER_ToAgent_Display,
			VIDEO_RPC_VO_FILTER_DISPLAY,
				.instanceID = handle->flt_out,
				.videoPlane = getPlane(display),
				.realTimeSrc = realTimeSrc,
				.zeroBuffer  = zeroBuffer,
			);
		IF_ERR_RET(ret);
	}

	/*
	 * NOTE:
	 * set bDisplayPending if display setting arrives before vdec_flt_init
	 */
	handle->bDisplay = bDisplay;
	handle->display = display;
	handle->pending_ops_display = (!handle->elem.flt_out && bDisplay);

	return 0;
}

int vdec_setAVSync(vdec_handle_t *handle, int enable)
{
	int ret;

	if (handle->elem.flt_out) {
		ret = vrpc_sendRPC_payload_va(handle->userID,
			VIDEO_RPC_VO_FILTER_ToAgent_ConfigSyncAV,
			VIDEO_RPC_VO_FILTER_Config_SyncAV,
				.instanceID = handle->flt_out,
				.enable = enable,
			);
		IF_ERR_RET(ret);
	}

	handle->bFreerun = !enable;
	handle->pending_ops_freerun = (!handle->elem.flt_out && !enable);

	return 0;
}

/* Deprecated */
int vdec_setFrameAdvance(vdec_handle_t *handle, int enable)
{
	int ret;

	if (handle->elem.flt_out) {
		if (enable)
			ret = vrpc_sendRPC_param(handle->userID,
				VIDEO_RPC_VO_FILTER_ToAgent_Step,
				handle->flt_out, 0);
		else
			ret = vrpc_sendRPC_param(handle->userID,
				VIDEO_RPC_ToAgent_Run,
				handle->flt_out, 0);
		IF_ERR_RET(ret);
	}

	handle->bFrameAdvance = enable;

	return 0;
}

/* Deprecated */
int vdec_setSpeed(vdec_handle_t *handle, int speed)
{
	int ret=0;

	if(speed == 0) //VDEC_DECODE_SPEED_PAUSE
	{
		ret = vdec_flt_pause(handle);
		IF_ERR_RET(ret);
	}
	else
	{
		if (handle->elem.flt_out)
		{
			ret = vrpc_sendRPC_payload_va(handle->userID,
				VIDEO_RPC_VO_FILTER_ToAgent_SetSpeed,
				VIDEO_RPC_VO_FILTER_SET_SPEED,
					.instanceID = handle->flt_out,
					.speed = getSpeed(speed),
				);
			IF_ERR_RET(ret);
		}

		if (handle->elem.flt_dec)
		{
			ret = vrpc_sendRPC_payload_va(handle->userID,
				VIDEO_RPC_DEC_ToAgent_SetSpeed,
				VIDEO_RPC_DEC_SET_SPEED,
					.instanceID = handle->flt_dec,
					.displaySpeed = getSpeed(speed),
					.decodeSkip = 0,
				);
			IF_ERR_RET(ret);
		}

		if(handle->pre_speed == 0 && !handle->bFrameAdvance && handle->elem.enable)
		{
			ret = vdec_flt_run(handle);
			IF_ERR_RET(ret);
		}
	}

	handle->pre_speed = speed;
	handle->speed = getSpeed(speed);

	return 0;
}

/* Deprecated */
int vdec_setFreeze(vdec_handle_t *handle, int freeze)
{
	int ret;

	if (handle->elem.flt_out) {
		ret = vrpc_sendRPC_payload_va(handle->userID,
			VIDEO_RPC_VO_FILTER_ToAgent_ConfigVoFreeze,
			VIDEO_RPC_VO_FILTER_Config_VoFreeze,
				.instanceID = handle->flt_out,
				.enable = freeze,
			);
		IF_ERR_RET(ret);
	}

	handle->bFreeze = freeze;

	return 0;
}

int vdec_setDisplayDelay(vdec_handle_t *handle, int display_delay)
{
	int ret;

	if (handle->elem.flt_out) {
		ret = vrpc_sendRPC_payload_va(handle->userID,
			VIDEO_RPC_VO_FILTER_ToAgent_ConfigDisplayDelay,
			VIDEO_RPC_VO_FILTER_Config_DisplayDelay,
				.instanceID = handle->flt_out,
				.delay = display_delay,
			);
		IF_ERR_RET(ret);
	}

	handle->display_delay = display_delay;
	handle->pending_ops_display_delay = (!handle->elem.flt_out && display_delay);

	return 0;
}

/* Deprecated */
int vdec_setDual3D(vdec_handle_t *handle, int enable)
{
	int ret;

	if (handle->elem.flt_out) {
		ret = vrpc_sendRPC_payload_va(handle->userID,
			VIDEO_RPC_VO_FILTER_ToAgent_ConfigDual3D,
			VIDEO_RPC_VO_FILTER_Config_Dual3D,
				.instanceID = handle->flt_out,
				.enable = enable,
			);
		IF_ERR_RET(ret);
	}

	handle->bDual3D = enable;

	return 0;
}

int vdec_setVSyncThreshold(vdec_handle_t *handle, int val)
{
	int ret;
	struct dmx_priv_data dmx_args;

	dmx_args.data.video_freerun_threshlod = val;

	if (__weakref_rdvb_dmx_ctrl_privateInfo) {
		ret = __weakref_rdvb_dmx_ctrl_privateInfo(DMX_PRIV_CMD_VIDEO_FREERUN_THRESHOLD,
			PIN_TYPE_VTP, handle->vtp_port, &dmx_args);
		if (ret) {
			#if 0
			RET_ERR_VA(ret, "privateInfo fail");
			#else
			/* XXX: DDTS demands success even if no valid input */
			RET_ERR_VA(0, "privateInfo fail");
			#endif
		}
	}

	handle->vsync_threshold = val;

	return 0;
}

int vdec_setStcMode(vdec_handle_t *handle, int mode)
{
	int ret;

	if (handle->elem.flt_out) {
		ret = vrpc_sendRPC_payload_va(handle->userID,
			VIDEO_RPC_VO_FILTER_ToAgent_SetSTCMode,
			VIDEO_RPC_VO_FILTER_SET_STC_MODE,
				.instanceID = handle->flt_out,
				.useSTC = mode,
			);
		IF_ERR_RET(ret);
	}

	handle->stc_mode = mode;
	handle->pending_ops_stc_mode = (!handle->elem.flt_out);

	return 0;
}

int vdec_setLipSyncMaster(vdec_handle_t *handle, int val)
{
	int ret;

	if (handle->elem.flt_out) {
		ret = vrpc_sendRPC_payload_va(handle->userID,
			VIDEO_RPC_VO_FILTER_ToAgent_SetLipSyncMaster,
			VIDEO_RPC_VO_FILTER_SET_LIPSYNC_MASTER,
				.instanceID = handle->flt_out,
				.lipsyncMaster = val,
			);
		IF_ERR_RET(ret);
	}

	handle->lipsync_master = val;
	handle->pending_ops_lipsync_master = (!handle->elem.flt_out);

	return 0;
}

int vdec_setAudioChannel(vdec_handle_t *handle, int val)
{
	int ret;

	if (handle->elem.flt_out) {
		ret = vrpc_sendRPC_payload_va(handle->userID,
			VIDEO_RPC_VO_FILTER_ToAgent_SetAudioChannel,
			VIDEO_RPC_VO_FILTER_SET_AUDIO_CHANNEL,
				.instanceID = handle->flt_out,
				.audioChannel = val,
			);
		IF_ERR_RET(ret);
	}

	handle->audio_channel = val;
	handle->pending_ops_audio_channel = (!handle->elem.flt_out);

	return 0;
}

/* Deprecated */
int vdec_setPVRMode(vdec_handle_t *handle, int mode)
{
	int ret;

	if (handle->elem.flt_out) {
		ret = vrpc_sendRPC_payload_va(handle->userID,
			VIDEO_RPC_VO_FILTER_ToAgent_ConfigPVRMode,
			VIDEO_RPC_VO_FILTER_CONFIG_PVR_MODE,
				.instanceID = handle->flt_out,
				.mode = mode,
			);
		IF_ERR_RET(ret);
	}

	handle->pvr_mode = mode;

	return 0;
}

int vdec_setFastIFrameMode(vdec_handle_t *handle, int mode)
{
	int ret;

	if (handle->elem.flt_out) {
		ret = vrpc_sendRPC_payload_va(handle->userID,
			VIDEO_RPC_VO_FILTER_ToAgent_ConfigFastIFrameMode,
			VIDEO_RPC_VO_FILTER_CONFIG_FAST_IFRAME_MODE,
				.instanceID = handle->flt_out,
				.mode = mode,
			);
		IF_ERR_RET(ret);
	}

	handle->fast_iframe_mode = mode;
	handle->pending_ops_fast_iframe_mode = (!handle->elem.flt_out);

	return 0;
}

