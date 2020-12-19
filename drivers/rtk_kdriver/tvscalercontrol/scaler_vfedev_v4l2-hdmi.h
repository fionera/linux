#ifndef __V4L2_VFE_HDMI_DEV_H
#define  __V4L2_VFE_HDMI_DEV_H

#include <linux/v4l2-ext/videodev2-ext.h>
#include <linux/v4l2-ext/v4l2-controls-ext.h>
#include <ioctrl/scaler/vfe_cmd_id_v4l2-hdmi.h>

#define FORCE_FIX_ERROR_UI_CH    0 //0: disable, 1~4: enable and force set UI ch,for internal debugging, 


typedef struct
{
	unsigned int open_cnt;
	unsigned int init_cnt;
}V4L2_MULTI_OPEN_CRTL_T;

typedef enum
{
	V4L2_HDMI_S_EXT_CTRL = 0,
	V4L2_HDMI_G_EXT_CTRL,
	V4L2_HDMI_S_CTRL,
	V4L2_HDMI_G_CTRL,
	V4L2_HDMI_S_INPUT,
	V4L2_HDMI_G_INPUT,
	V4L2_HDMI_OPEN,
	V4L2_HDMI_RELEASE,
	V4L2_HDMI_READ,
	V4L2_HDMI_POLL,
	V4L2_HDMI_MMAP,
}V4L2_HDMI_TYPE;


typedef struct
{
	const V4L2_HDMI_TYPE type; 
	const unsigned int sub_id;
	const char entry_name[64];
	unsigned char history_record;
	unsigned char execute_enable;
	unsigned int total_call_cnt;
}HDMI_V4L2_DEFINE_T;

typedef struct
{
	V4L2_HDMI_TYPE type; 
	unsigned int sub_id;
	unsigned int target_ui_ch;
	unsigned int current_call_cnt;
	unsigned int last_call_ms;

}HDMI_V4L2_HISTORY_T;
#define HDMI_V4L2_HISTORY_QUEUE_SIZE	64
#define V4L2_HDMI_MGR_NG    0xFFFF

#define NO_SUB_ID    0

extern const unsigned char g_v4l2_define_table_size;

extern HDMI_V4L2_HISTORY_T* newbase_hdmi_v4l2_get_history_info(void);
extern HDMI_V4L2_DEFINE_T* newbase_hdmi_v4l2_function_table(void);
extern int newbase_hdmi_v4l2_function_table_index(V4L2_HDMI_TYPE type, unsigned int sub_id);
extern int v4l2_vfe_hdmi_ext_link_status(struct v4l2_ext_hdmi_link_status* p_status);
extern int v4l2_vfe_hdmi_ext_phy_status(struct v4l2_ext_hdmi_phy_status* p_status);
extern int v4l2_vfe_hdmi_ext_video_status(struct v4l2_ext_hdmi_video_status* p_status);
extern int v4l2_vfe_hdmi_ext_audio_status(struct v4l2_ext_hdmi_audio_status* p_status);
extern int v4l2_vfe_hdmi_ext_hdcp_status(struct v4l2_ext_hdmi_hdcp_status* p_status);
extern int v4l2_vfe_hdmi_ext_scdc_status(struct v4l2_ext_hdmi_scdc_status* p_status);
extern int v4l2_vfe_hdmi_ext_diagnostic_info(struct v4l2_ext_hdmi_diagnostics_status* hdmi_v4l2_ext_diagnostics_status);
extern int v4l2_vfe_hdmi_ext_error_status(struct v4l2_ext_hdmi_error_status* p_status);

extern int v4l2_vfe_hdmi_ext_get_timing_info(struct v4l2_ext_hdmi_timing_info* p_v4l2_ext_timing);
extern int v4l2_vfe_hdmi_ext_get_drm_info(struct v4l2_ext_hdmi_drm_info* p_drm_info);
extern int v4l2_vfe_hdmi_ext_get_vsi_info(struct v4l2_ext_hdmi_vsi_info* p_vsi_info);
extern int v4l2_vfe_hdmi_ext_get_spd_info(struct v4l2_ext_hdmi_spd_info* p_spd_info);
extern int v4l2_vfe_hdmi_ext_get_avi_info(struct v4l2_ext_hdmi_avi_info* p_avi_info);
extern int v4l2_vfe_hdmi_ext_get_vrr_frequency(struct v4l2_ext_hdmi_vrr_frequency* p_vrr_freq);

// LGTV procfs releated function
extern int v4l2_vfe_hdmi_get_open_count(void);
extern int v4l2_vfe_hdmi_get_connection_status(unsigned char port);
extern int v4l2_vfe_hdmi_get_dolby_hdr_info(struct v4l2_ext_hdmi_dolby_hdr* p_hdr);

#endif // __V4L2_VFE_HDMI_DEV_H

