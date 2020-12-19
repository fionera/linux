#ifndef __EXT_IN_HDMI_H__
#define __EXT_IN_HDMI_H__

#include <linux/v4l2-ext/videodev2-ext.h>
#include <linux/v4l2-ext/v4l2-controls-ext.h>
#include <ioctrl/scaler/vfe_cmd_id_v4l2-hdmi.h>
#include "../tvscalercontrol/scaler_vfedev_v4l2-hdmi.h"

typedef struct
{
    unsigned int open;
    unsigned int connection_state;
    unsigned char hpd_state;
    unsigned char hwport;
    struct v4l2_ext_hdmi_timing_info timing_info;

    // Info Packet
    struct v4l2_ext_hdmi_drm_info drm_info;
    struct v4l2_ext_hdmi_vsi_info vsi_info;
    struct v4l2_ext_hdmi_spd_info spd_info;
    struct v4l2_ext_hdmi_avi_info avi_info;

    // dolby hdr
    struct v4l2_ext_hdmi_dolby_hdr dolby_hdr;

    // VRR
    struct v4l2_ext_hdmi_vrr_frequency vrr_freq;

    // Phy Status
    struct v4l2_ext_hdmi_phy_status phy_status;

    // Link Status
    struct v4l2_ext_hdmi_link_status link_status;

    // Video Status
    struct v4l2_ext_hdmi_video_status video_status;

    // Audio Status
    struct v4l2_ext_hdmi_audio_status audio_status;

    // hdcp status
    struct v4l2_ext_hdmi_hdcp_status  hdcp_status;

    // SCDC
    struct v4l2_ext_hdmi_scdc_status  scdc_status;

    // Error Status
    struct v4l2_ext_hdmi_error_status err_status;

    // EDID info
    struct v4l2_ext_hdmi_edid edid_info;
    unsigned char edid[512];
}LGTV_EXT_IN_HDMI_STATUS;


#endif //__EXT_IN_HDMI_H__