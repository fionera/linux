/*
 *
 *	VSC v4l2 related api header file
 *
 *  drievr vsc internal api from vsc device
 */
#ifndef _VSC_V4L2_API_H
#define _VSC_V4L2_API_H

struct vsc_v4l2_device {
	struct v4l2_device v4l2_dev;
	struct video_device *vfd;
	struct mutex lock;
};

typedef enum{
	ACTIVE_SIZE_EVENT = 0x1,//miracast
	ARC_APPLY_DONE_EVENT = 0x2,//arc done
	MUTE_OFF_EVENT = 0x4//mute off
}VSV_V4L2_EVENT_TYPE;


//v4l2 ioctrl callback api
int vsc_v4l2_ioctl_s_ext_ctrls(struct file *file, void *fh, struct v4l2_ext_controls *ctrls);
int vsc_v4l2_ioctl_g_ext_ctrls(struct file *file, void *fh, struct v4l2_ext_controls *ctrls);
int vsc_v4l2_ioctl_s_ctrl(struct file *file, void *fh, struct v4l2_control *ctrl);
int vsc_v4l2_ioctl_g_ctrl(struct file *file, void *fh, struct v4l2_control *ctrl);
/*-----2 standard control information for implementation-----*/
int vsc_main_v4l2_ioctl_querycap(struct file *file, void *fh, struct v4l2_capability *cap);
/*-----2 standard control information for implementation-----*/
int vsc_sub_v4l2_ioctl_querycap(struct file *file, void *fh, struct v4l2_capability *cap);

unsigned char vsc_get_main_win_apply_done_event_subscribe(void);

//vsc internal api
extern unsigned char rtk_hal_vsc_initialize(void);
extern unsigned char rtk_hal_vsc_open(VIDEO_WID_T wid);
extern unsigned char rtk_hal_vsc_close(VIDEO_WID_T wid);
extern unsigned char rtk_hal_vsc_Connect(VIDEO_WID_T wid, KADP_VSC_INPUT_SRC_INFO_T inputSrcInfo, KADP_VSC_OUTPUT_MODE_T outputMode);
extern unsigned char rtk_hal_vsc_Disconnect(VIDEO_WID_T wid, KADP_VSC_INPUT_SRC_INFO_T inputSrcInfo, KADP_VSC_OUTPUT_MODE_T outputMode);
extern unsigned char vdo_connect(unsigned char display, unsigned char vdec_port);
extern unsigned char vdo_disconnect(unsigned char display, unsigned char vdec_port);
extern unsigned char rtk_hal_vsc_SetWinBlank(VIDEO_WID_T wid, bool bonoff, KADP_VIDEO_DDI_WIN_COLOR_T color);
extern unsigned char rtk_hal_vsc_dm_open(unsigned char display);
extern unsigned char rtk_hal_vsc_dm_close(unsigned char display);
extern unsigned char rtk_hal_vsc_dm_connect(unsigned char display, KADP_VSC_HDR_TYPE_T eHdrMode);
extern unsigned char rtk_hal_vsc_dm_disconnect(unsigned char display);
extern unsigned char rtk_hal_vsc_SetInputRegion_OutputRegion(KADP_VIDEO_WID_T wid, KADP_VSC_ROTATE_T rotate_type, KADP_VIDEO_RECT_T  inregion,
	KADP_VIDEO_RECT_T originalInput, KADP_VIDEO_RECT_T outregion, unsigned char null_input, unsigned char null_output);
extern unsigned char rtk_hal_vsc_SetWinFreeze(VIDEO_WID_T wid, bool bonoff);
extern unsigned char rtk_hal_vsc_SetRGB444Mode(bool bonoff);
extern unsigned char rtk_hal_vsc_SetZorder(VSC_SET_ZORDER_T zOrderMain,VSC_SET_ZORDER_T zOrderSub);
extern unsigned char rtk_hal_vsc_setwinprop(VSC_SET_SUB_WINDOW_MODE_TYPE vsc_set_sub_win_mode);
extern unsigned char rtk_hal_vsc_SetAdaptiveStreamEX(VIDEO_WID_T wid,unsigned char bOnOff);
extern unsigned char rtk_hal_vsc_SetDelayBuffer(VIDEO_WID_T wId, UINT8 buffer);
extern unsigned char rtk_hal_vsc_SetPattern(BOOLEAN on_off, VIDEO_WID_T winID, VSC_VIDEO_PATTERN_LOCATION_T pattern_location);
extern unsigned char rtk_hal_vsc_FreezeVideoFrameBuffer(VIDEO_WID_T wid, bool bonoff);
extern unsigned char rtk_hal_vsc_ReadVideoFrameBuffer(VIDEO_WID_T wid, VIDEO_RECT_T * pin,KADP_VIDEO_DDI_PIXEL_STANDARD_COLOR_T * pRead, KADP_VIDEO_DDI_COLOR_STANDARD_T *pColor_standard, KADP_VIDEO_DDI_PIXEL_COLOR_FORMAT_T * pPixelColorFormat);
int vsc_v4l2_main_subscribe_event(struct v4l2_fh *fh, const struct v4l2_event_subscription *sub);
int vsc_v4l2_main_event_unsubscribe(struct v4l2_fh *fh, const struct v4l2_event_subscription *sub);
int vsc_v4l2_sub_subscribe_event(struct v4l2_fh *fh, const struct v4l2_event_subscription *sub);
int vsc_v4l2_sub_event_unsubscribe(struct v4l2_fh *fh, const struct v4l2_event_subscription *sub);
extern void set_HFR_mode(unsigned char enable);
extern unsigned char get_HFR_mode(void);
extern void set_latency_pattern_info(VSC_VIDEO_LATENCY_PATTERN_T set_video_latency_pattern);
void Set_poll_event(unsigned char display, unsigned char clear, unsigned char value);
extern void scaler_vsc_set_window_callback_lowdelay_mode(UINT8 bOnOff);
#endif
