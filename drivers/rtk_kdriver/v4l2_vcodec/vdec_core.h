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

#ifndef __V4L2_VDEC_CORE_H__
#define __V4L2_VDEC_CORE_H__

int vdec_initHandle(vdec_handle_t *handle);
int vdec_exitHandle(vdec_handle_t *handle);
int vdec_connectDisplay(vdec_handle_t *handle, int display, unsigned char realTimeSrc, unsigned char zeroBuffer);
int vdec_clearVideo(vdec_handle_t *handle);
int vdec_reset(vdec_handle_t *handle);

int vdec_setUserType(vdec_handle_t *handle, int userType);
int vdec_setChannel(vdec_handle_t *handle, int channel);
int vdec_setVTPPort(vdec_handle_t *handle, int port);
int vdec_setVTPBuffer(vdec_handle_t *handle, int port);
int vdec_setDecMode(vdec_handle_t *handle, int mode);
int vdec_setEventFlag(vdec_handle_t *handle, int event, int val);
int vdec_setAVSync(vdec_handle_t *handle, int enable);
int vdec_setFrameAdvance(vdec_handle_t *handle, int enable);
int vdec_setSpeed(vdec_handle_t *handle, int speed);
int vdec_setLowDelay(vdec_handle_t *handle, int mode);
int vdec_setFreeze(vdec_handle_t *handle, int val);
int vdec_setDisplayDelay(vdec_handle_t *handle, int delay);
int vdec_setDual3D(vdec_handle_t *handle, int enable);
int vdec_setVSyncThreshold(vdec_handle_t *handle, int val);
int vdec_setStcMode(vdec_handle_t *handle, int mode);
int vdec_setLipSyncMaster(vdec_handle_t *handle, int val);
int vdec_setAudioChannel(vdec_handle_t *handle, int val);
int vdec_setPVRMode(vdec_handle_t *handle, int mode);
int vdec_setFastIFrameMode(vdec_handle_t *handle, int mode);
int vdec_setHighFrameRateType(vdec_handle_t *handle, int HFR_type);
int vdec_setTemporalIDMax(vdec_handle_t *handle, int temporal_id_max);
int vdec_setDirectMode(vdec_handle_t *handle, int mode);
int vdec_writeESdata(vdec_handle_t *handle, void __user *userptr, int size);
int vdec_setDripDecodeMode(vdec_handle_t *handle, int mode);

int vdec_getPicInfo(vdec_handle_t *handle, void __user *userptr, int size);
int vdec_getPicInfo_ext(vdec_handle_t *handle, void __user *userptr, int size);
int vdec_getUserData(vdec_handle_t *handle, void __user *userptr, int size);
int vdec_getStreamInfo(vdec_handle_t *handle, struct v4l2_ext_stream_info *stream_info);
int vdec_getDecoderStatus(vdec_handle_t *handle, struct v4l2_ext_decoder_status *decoder_status);
int vdec_getVideoInfo(vdec_handle_t *handle, struct v4l2_control *ctrl);
int vdec_dripDecodePicture(vdec_handle_t *handle, void *ptr, int size);
int vdec_startDripDec(vdec_handle_t *handle, void *pDripData, int size, int dripType, int codecType);
int vdec_stopDripDec(vdec_handle_t *handle);
int vdec_saveDripDec(vdec_handle_t *handle, unsigned char *pDripData, int size, int dripType, int codecType, unsigned char **ppARGBdata, unsigned int *pDestSize, unsigned int *pDestWidth);
int vdec_saveDripDec_free(vdec_handle_t *handle, unsigned char *pARGBdata);

/* filter operations */
int vdec_flt_init(vdec_handle_t *handle);
int vdec_flt_sendCmd(vdec_handle_t *handle, int cmd);
int vdec_flt_run(vdec_handle_t *handle);
int vdec_flt_pause(vdec_handle_t *handle);
int vdec_flt_flush(vdec_handle_t *handle);
int vdec_flt_stop(vdec_handle_t *handle);
int vdec_flt_destroy(vdec_handle_t *handle);
int vdec_flt_exit(vdec_handle_t *handle);

int vdec_streamon(vdec_handle_t *handle);
int vdec_streamoff(vdec_handle_t *handle);

#endif
