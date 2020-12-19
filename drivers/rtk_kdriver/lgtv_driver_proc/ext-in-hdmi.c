#include <linux/syscalls.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <mach/io.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <lgtv_driver_proc/procfs.h>
#include "ext-in-hdmi.h"
#include <tvscalercontrol/hdmirx/hdmi_vfe.h>

static int _get_hdmi_status(unsigned char hdmi_ch, LGTV_EXT_IN_HDMI_STATUS* p_hdmi_status)
{
    memset(p_hdmi_status, 0, sizeof(*p_hdmi_status));

    // General Info
    p_hdmi_status->open = v4l2_vfe_hdmi_get_open_count();
    p_hdmi_status->connection_state = v4l2_vfe_hdmi_get_connection_status(hdmi_ch);
    vfe_hdmi_v4l2_get_hpd_flag_port(hdmi_ch,&p_hdmi_status->hpd_state);
    p_hdmi_status->hwport = hdmi_ch;

    // Get timing info
    p_hdmi_status->timing_info.port = hdmi_ch;
    v4l2_vfe_hdmi_ext_get_timing_info(&p_hdmi_status->timing_info);

    // drm info
    p_hdmi_status->drm_info.port = hdmi_ch;
    v4l2_vfe_hdmi_ext_get_drm_info(&p_hdmi_status->drm_info);

    //vsi_info
    p_hdmi_status->vsi_info.port = hdmi_ch;
    v4l2_vfe_hdmi_ext_get_vsi_info(&p_hdmi_status->vsi_info);

    // SPD info
    p_hdmi_status->spd_info.port = hdmi_ch;
    v4l2_vfe_hdmi_ext_get_spd_info(&p_hdmi_status->spd_info);

    // AVI info
    p_hdmi_status->avi_info.port = hdmi_ch;
    v4l2_vfe_hdmi_ext_get_avi_info(&p_hdmi_status->avi_info);

    // Dolby
    p_hdmi_status->dolby_hdr.port = hdmi_ch;
    v4l2_vfe_hdmi_get_dolby_hdr_info(&p_hdmi_status->dolby_hdr);

    // VRR
    p_hdmi_status->vrr_freq.port = hdmi_ch;
    v4l2_vfe_hdmi_ext_get_vrr_frequency(&p_hdmi_status->vrr_freq);

    // EMP
    //TBD...

    // Phy Status
    p_hdmi_status->phy_status.port = hdmi_ch;
    v4l2_vfe_hdmi_ext_phy_status(&p_hdmi_status->phy_status);

    // Link Status
    p_hdmi_status->link_status.port = hdmi_ch;
    v4l2_vfe_hdmi_ext_link_status(&p_hdmi_status->link_status);

    // video status
    p_hdmi_status->video_status.port = hdmi_ch;
    v4l2_vfe_hdmi_ext_video_status(&p_hdmi_status->video_status);

    // audio status
    p_hdmi_status->audio_status.port = hdmi_ch;
    v4l2_vfe_hdmi_ext_audio_status(&p_hdmi_status->audio_status);

    // hdcp status
    p_hdmi_status->hdcp_status.port = hdmi_ch;
    v4l2_vfe_hdmi_ext_hdcp_status(&p_hdmi_status->hdcp_status);

    // SCDC Status
    p_hdmi_status->scdc_status.port = hdmi_ch;
    v4l2_vfe_hdmi_ext_scdc_status(&p_hdmi_status->scdc_status);

    // error status
    p_hdmi_status->err_status.port = hdmi_ch;
    v4l2_vfe_hdmi_ext_error_status(&p_hdmi_status->err_status);

    // edid info
    p_hdmi_status->edid_info.size = 2;
    p_hdmi_status->edid_info.pData = p_hdmi_status->edid;
    return 0;
}


static int v4l2_get_hdmi_status(void* private_data, char* buff, unsigned int buff_size)
{
    int len, total = 0;
    unsigned int hdmiIndex = (unsigned int) private_data;
    LGTV_EXT_IN_HDMI_STATUS* p_hdmi_status = (LGTV_EXT_IN_HDMI_STATUS*) kzalloc(sizeof(LGTV_EXT_IN_HDMI_STATUS), GFP_KERNEL);

    if (p_hdmi_status==NULL) {
        pr_err("v4l2_get_hdmi_status failed, out of memory\n");
        return 0;
    }

    _get_hdmi_status(hdmiIndex, p_hdmi_status);

    len = snprintf(buff, buff_size, "===== HDMI STATUS =====\n");
    total += len; buff_size-=len; buff+= len;

    //----------------------------------------
    // General Info
    //----------------------------------------
    len = snprintf(buff, buff_size, "open:%d\n", p_hdmi_status->open);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "connection_state:%d\n", p_hdmi_status->connection_state);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "hpd_state : %d\n", p_hdmi_status->hpd_state);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "hwport:%d\n\n", p_hdmi_status->hwport);
    total += len; buff_size-=len; buff+= len;

    //----------------------------------------
    // Timing Info
    //----------------------------------------
    len = snprintf(buff, buff_size, "timing_h_freq:%d\n", p_hdmi_status->timing_info.h_freq);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "timing_v_freq:%d\n", p_hdmi_status->timing_info.v_vreq);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "timing_h_total:%d\n", p_hdmi_status->timing_info.h_total);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "timing_v_total:%d\n", p_hdmi_status->timing_info.v_total);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "timing_active_w:%d\n", p_hdmi_status->timing_info.active.w);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "timing_active_h:%d\n", p_hdmi_status->timing_info.active.h);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "timing_scan_type:%d\n", p_hdmi_status->timing_info.scan_type);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "dvi_hdmi:%d\n", p_hdmi_status->timing_info.dvi_hdmi);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "color_depth:%d\n", p_hdmi_status->timing_info.color_depth);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "allm_mode:%d\n\n", p_hdmi_status->timing_info.allm_mode);
    total += len; buff_size-=len; buff+= len;

    //----------------------------------------
    // DRM Info
    //----------------------------------------

    len = snprintf(buff, buff_size, "drm_version:%d\n", p_hdmi_status->drm_info.version);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "drm_length:%d\n", p_hdmi_status->drm_info.length);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "drm_eotf_type:%d\n", p_hdmi_status->drm_info.eotf_type);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "drm_meta_desc:%d\n", p_hdmi_status->drm_info.meta_desc);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "drm_display_primaries_x0:%d\n", p_hdmi_status->drm_info.display_primaries_x0);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "drm_display_primaries_y0:%d\n", p_hdmi_status->drm_info.display_primaries_y0);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "drm_display_primaries_x1:%d\n", p_hdmi_status->drm_info.display_primaries_x1);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "drm_display_primaries_y1:%d\n", p_hdmi_status->drm_info.display_primaries_x1);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "drm_display_primaries_x2:%d\n", p_hdmi_status->drm_info.display_primaries_x2);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "drm_display_primaries_y2:%d\n", p_hdmi_status->drm_info.display_primaries_y2);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "drm_white_point_x:%d\n", p_hdmi_status->drm_info.white_point_x);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "drm_white_point_y:%d\n", p_hdmi_status->drm_info.white_point_y);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "drm_max_display_mastering_luminance:%d\n", p_hdmi_status->drm_info.max_display_mastering_luminance );
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "drm_min_display_mastering_luminance:%d\n", p_hdmi_status->drm_info.min_display_mastering_luminance );
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "drm_maximum_content_light_level:%d\n", p_hdmi_status->drm_info.maximum_content_light_level);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "drm_maximum_frame_average_light_level:%d\n\n", p_hdmi_status->drm_info.maximum_frame_average_light_level);
    total += len; buff_size-=len; buff+= len;

    //----------------------------------------
    // VSI Info
    //----------------------------------------
    len = snprintf(buff, buff_size, "vsi_video_format:%d\n", p_hdmi_status->vsi_info.video_format);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "vsi_st_3d:%d\n", p_hdmi_status->vsi_info.st_3d);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "vsi_ext_data_3d:%d\n", p_hdmi_status->vsi_info.ext_data_3d);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "vsi_vic:%d\n", p_hdmi_status->vsi_info.vic);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "vsi_regid:0x%02x,0x%02x,0x%02x\n", p_hdmi_status->vsi_info.regid[0], p_hdmi_status->vsi_info.regid[1], p_hdmi_status->vsi_info.regid[2]);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "vsi_payload:0x%02x,0x%02x,0x%02x...\n\n", p_hdmi_status->vsi_info.payload[0], p_hdmi_status->vsi_info.payload[1], p_hdmi_status->vsi_info.payload[2]);
    total += len; buff_size-=len; buff+= len;

    //----------------------------------------
    // SPD Info
    //----------------------------------------
    len = snprintf(buff, buff_size, "spd_vendor_name:\"%s\"\n",p_hdmi_status->spd_info.vendor_name);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "spd_product_description:\"%s\"\n", p_hdmi_status->spd_info.product_description);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "spd_source_device_info:%d\n\n", p_hdmi_status->spd_info.source_device_info);
    total += len; buff_size-=len; buff+= len;

    //----------------------------------------
    // AVI Info
    //----------------------------------------
    len = snprintf(buff, buff_size, "avi_mode:%d\n", p_hdmi_status->avi_info.mode);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "avi_pixel_encoding:%d\n", p_hdmi_status->avi_info.pixel_encoding);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "avi_active_info:%d\n", p_hdmi_status->avi_info.active_info);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "avi_bar_info:%d\n", p_hdmi_status->avi_info.bar_info);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "avi_scan_info:%d\n", p_hdmi_status->avi_info.scan_info);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "avi_colorimetry:%d\n", p_hdmi_status->avi_info.colorimetry);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "avi_picture_aspect_ratio:%d\n", p_hdmi_status->avi_info.picture_aspect_ratio);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "avi_active_format_aspect_ratio:%d\n", p_hdmi_status->avi_info.active_format_aspect_ratio);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "avi_scaling:%d\n", p_hdmi_status->avi_info.scaling);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "avi_vic:%d\n", p_hdmi_status->avi_info.vic);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "avi_pixel_repeat:%d\n", p_hdmi_status->avi_info.pixel_repeat);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "avi_it_content:%d\n", p_hdmi_status->avi_info.it_content);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "avi_extended_colorimetry:%d\n", p_hdmi_status->avi_info.extended_colorimetry);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "avi_rgb_quantization_range:%d\n", p_hdmi_status->avi_info.rgb_quantization_range);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "avi_ycc_quantization_range:%d\n", p_hdmi_status->avi_info.ycc_quantization_range);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "avi_content_type:%d\n", p_hdmi_status->avi_info.content_type);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "avi_top_bar_end_line_number:%d\n", p_hdmi_status->avi_info.top_bar_end_line_number);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "avi_bottom_bar_start_line_number:%d\n", p_hdmi_status->avi_info.bottom_bar_start_line_number);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "avi_left_bar_end_pixel_number:%d\n", p_hdmi_status->avi_info.left_bar_end_pixel_number);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "avi_right_bar_end_pixel_number:%d\n\n", p_hdmi_status->avi_info.right_bar_end_pixel_number);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "packet_status:%d\n", p_hdmi_status->avi_info.packet_status);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "packet_type:%d\n", p_hdmi_status->avi_info.packet.type);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "packet_version:%d\n", p_hdmi_status->avi_info.packet.version);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "packet_length:%d\n", p_hdmi_status->avi_info.packet.length);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "packet_data_bytes:0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x ...\n\n",
    p_hdmi_status->avi_info.packet.data_bytes[0], p_hdmi_status->avi_info.packet.data_bytes[1], p_hdmi_status->avi_info.packet.data_bytes[2],
    p_hdmi_status->avi_info.packet.data_bytes[3], p_hdmi_status->avi_info.packet.data_bytes[4]);
    total += len; buff_size-=len; buff+= len;

    //----------------------------------------
    // Dolby HDR
    //----------------------------------------
    len = snprintf(buff, buff_size, "dolby_hdr_type:%d\n\n", p_hdmi_status->dolby_hdr.type);
    total += len; buff_size-=len; buff+= len;

    //----------------------------------------
    // VRR
    //----------------------------------------
    len = snprintf(buff, buff_size, "vrr_frequency:%d\n\n", p_hdmi_status->vrr_freq.frequency);
    total += len; buff_size-=len; buff+= len;

    //----------------------------------------
    // EMP
    //----------------------------------------
    len = snprintf(buff, buff_size, "emp_type:4096\n");
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "emp_total_packet_number:1\n");
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "emp_data: 0x00,0x00, 0x00 ...\n\n");
    total += len; buff_size-=len; buff+= len;

    //----------------------------------------
    // Phy Status
    //----------------------------------------
    len = snprintf(buff, buff_size, "phy_lock_status:%d\n", p_hdmi_status->phy_status.lock_status);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "phy_tmds_clk_khz:%d\n", p_hdmi_status->phy_status.tmds_clk_khz);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "phy_link_type:%d\n",p_hdmi_status->phy_status.link_type);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "phy_link_lane:%d\n", p_hdmi_status->phy_status.link_lane);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "phy_link_rate:%d\n", p_hdmi_status->phy_status.link_rate);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "phy_ctle_eq_min_range:0x%x, 0x%x, 0x%x, 0x%x\n",
        p_hdmi_status->phy_status.ctle_eq_min_range[0],
        p_hdmi_status->phy_status.ctle_eq_min_range[1],
        p_hdmi_status->phy_status.ctle_eq_min_range[2],
        p_hdmi_status->phy_status.ctle_eq_min_range[3]);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "phy_ctle_eq_max_range:0x%x, 0x%x, 0x%x, 0x%x\n",
        p_hdmi_status->phy_status.ctle_eq_max_range[0],
        p_hdmi_status->phy_status.ctle_eq_max_range[1],
        p_hdmi_status->phy_status.ctle_eq_max_range[2],
        p_hdmi_status->phy_status.ctle_eq_max_range[3]);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "phy_ctle_eq_result:0x%x, 0x%x, 0x%x, 0x%x\n",
        p_hdmi_status->phy_status.ctle_eq_result[0],
        p_hdmi_status->phy_status.ctle_eq_result[1],
        p_hdmi_status->phy_status.ctle_eq_result[2],
        p_hdmi_status->phy_status.ctle_eq_result[3]);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "phy_error:0x%x, 0x%x, 0x%x, 0x%x\n\n",
        p_hdmi_status->phy_status.error[0],
        p_hdmi_status->phy_status.error[1],
        p_hdmi_status->phy_status.error[2],
        p_hdmi_status->phy_status.error[3]);
    total += len; buff_size-=len; buff+= len;

    //----------------------------------------
    // Link Status
    //----------------------------------------
    len = snprintf(buff, buff_size, "link_hpd:%d\n", p_hdmi_status->link_status.hpd);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "link_hdmi_5v:%d\n", p_hdmi_status->link_status.hdmi_5v);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "link_rx_sense:%d\n", p_hdmi_status->link_status.rx_sense);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "link_frame_rate_x100_hz:%d\n", p_hdmi_status->link_status.frame_rate_x100_hz);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "link_dvi_hdmi_mode:%d\n", p_hdmi_status->link_status.dvi_hdmi_mode);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "link_video_width:%d\n", p_hdmi_status->link_status.video_width);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "link_video_height:%d\n", p_hdmi_status->link_status.video_height);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "link_color_space:%d\n", p_hdmi_status->link_status.color_space);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "link_color_depth:%d\n", p_hdmi_status->link_status.color_depth);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "link_colorimetry:%d\n", p_hdmi_status->link_status.colorimetry);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "link_ext_colorimetry:%d\n", p_hdmi_status->link_status.ext_colorimetry);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "link_additional_colorimetry:%d\n", p_hdmi_status->link_status.additional_colorimetry);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "link_hdr_type:%d\n", p_hdmi_status->link_status.hdr_type);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "link_audio_format:%d\n", p_hdmi_status->link_status.audio_format);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "link_audio_sampling_freq:%d\n", p_hdmi_status->link_status.audio_sampling_freq);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "link_audio_channel_number:%d\n\n", p_hdmi_status->link_status.audio_channel_number);
    total += len; buff_size-=len; buff+= len;

    //----------------------------------------
    // Video Status
    //----------------------------------------
    len = snprintf(buff, buff_size, "video_width_real:%d\n", p_hdmi_status->video_status.video_width_real);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "video_htotal_real:%d\n", p_hdmi_status->video_status.video_htotal_real);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "video_height_real:%d\n", p_hdmi_status->video_status.video_height_real );
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "video_vtotal_real:%d\n", p_hdmi_status->video_status.video_vtotal_real);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "pixel_clock_khz:%d\n", p_hdmi_status->video_status.pixel_clock_khz);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "current_vrr_refresh_rate:%d\n\n", p_hdmi_status->video_status.current_vrr_refresh_rate);
    total += len; buff_size-=len; buff+= len;

    //----------------------------------------
    // Audio Status
    //----------------------------------------
    len = snprintf(buff, buff_size, "audio_pcm_N:%d\n", p_hdmi_status->audio_status.pcm_N);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "audio_pcm_CTS:%d\n", p_hdmi_status->audio_status.pcm_CTS);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "audio_LayoutBitValue:%d\n", p_hdmi_status->audio_status.LayoutBitValue);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "audio_ChannelStatusBits:%d\n\n", p_hdmi_status->audio_status.ChannelStatusBits);
    total += len; buff_size-=len; buff+= len;

    //----------------------------------------
    // HDCP Status
    //----------------------------------------
    len = snprintf(buff, buff_size, "hdcp_version:%d\n", p_hdmi_status->hdcp_status.hdcp_version);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "hdcp_auth_status:%d\n", p_hdmi_status->hdcp_status.auth_status);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "hdcp_encEn:%d\n", p_hdmi_status->hdcp_status.encEn);
    total += len; buff_size-=len; buff+= len;

    p_hdmi_status->hdcp_status.hdcp14_status.Aksv[0] = 0xdf;
    p_hdmi_status->hdcp_status.hdcp14_status.Aksv[1] = 0x63;
    p_hdmi_status->hdcp_status.hdcp14_status.Aksv[2] = 0x49;
    p_hdmi_status->hdcp_status.hdcp14_status.Aksv[3] = 0x4c;
    p_hdmi_status->hdcp_status.hdcp14_status.Aksv[4] = 0x98;
    p_hdmi_status->hdcp_status.hdcp14_status.Bksv[0] = 0x3c;
    p_hdmi_status->hdcp_status.hdcp14_status.Bksv[1] = 0xa0;
    p_hdmi_status->hdcp_status.hdcp14_status.Bksv[2] = 0xf4;
    p_hdmi_status->hdcp_status.hdcp14_status.Bksv[3] = 0x66;
    p_hdmi_status->hdcp_status.hdcp14_status.Bksv[4] = 0xf4;
    p_hdmi_status->hdcp_status.hdcp14_status.Ri[0] = 0xfe;
    p_hdmi_status->hdcp_status.hdcp14_status.Ri[1] = 0xff;


    len = snprintf(buff, buff_size, "hdcp14_status.port:%d\n", p_hdmi_status->hdcp_status.hdcp14_status.port);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "hdcp14_status.An:0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
         p_hdmi_status->hdcp_status.hdcp14_status.An[0],
         p_hdmi_status->hdcp_status.hdcp14_status.An[1],
         p_hdmi_status->hdcp_status.hdcp14_status.An[2],
         p_hdmi_status->hdcp_status.hdcp14_status.An[3],
         p_hdmi_status->hdcp_status.hdcp14_status.An[4],
         p_hdmi_status->hdcp_status.hdcp14_status.An[5],
         p_hdmi_status->hdcp_status.hdcp14_status.An[6],
         p_hdmi_status->hdcp_status.hdcp14_status.An[7]);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "hdcp14_status.Aksv:0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
        p_hdmi_status->hdcp_status.hdcp14_status.Aksv[0],
        p_hdmi_status->hdcp_status.hdcp14_status.Aksv[1],
        p_hdmi_status->hdcp_status.hdcp14_status.Aksv[2],
        p_hdmi_status->hdcp_status.hdcp14_status.Aksv[3],
        p_hdmi_status->hdcp_status.hdcp14_status.Aksv[4]);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "hdcp14_status.Bksv:0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
        p_hdmi_status->hdcp_status.hdcp14_status.Bksv[0],
        p_hdmi_status->hdcp_status.hdcp14_status.Bksv[1],
        p_hdmi_status->hdcp_status.hdcp14_status.Bksv[2],
        p_hdmi_status->hdcp_status.hdcp14_status.Bksv[3],
        p_hdmi_status->hdcp_status.hdcp14_status.Bksv[4]);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "hdcp14_status.Ri:0x%02x 0x%02x\n",
        p_hdmi_status->hdcp_status.hdcp14_status.Ri[0],
        p_hdmi_status->hdcp_status.hdcp14_status.Ri[1]);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "hdcp14_status.Bcaps:%d\n", p_hdmi_status->hdcp_status.hdcp14_status.Bcaps);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "hdcp14_status.Bstatus:0x%02x 0x%02x\n",
        p_hdmi_status->hdcp_status.hdcp14_status.Bstatus[0],
        p_hdmi_status->hdcp_status.hdcp14_status.Bstatus[1]);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "hdcp22_status.port:%d\n", p_hdmi_status->hdcp_status.hdcp22_status.port);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "hdcp22_status.ake_init_count_since_5v:%d\n",p_hdmi_status->hdcp_status.hdcp22_status.ake_init_count_since_5v);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "hdcp22_status.reauth_req_count_since_5v:%d\n\n", p_hdmi_status->hdcp_status.hdcp22_status.reauth_req_count_since_5v);
    total += len; buff_size-=len; buff+= len;

    //----------------------------------------
    // SCDC Status
    //----------------------------------------
    len = snprintf(buff, buff_size, "scdc_source_version:%d\n", p_hdmi_status->scdc_status.source_version);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_sink_version:%d\n", p_hdmi_status->scdc_status.sink_version);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_rsed_update:%d\n", p_hdmi_status->scdc_status.rsed_update);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_flt_update:%d\n", p_hdmi_status->scdc_status.flt_update);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_frl_start:%d\n", p_hdmi_status->scdc_status.frl_start);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_source_test_update:%d\n", p_hdmi_status->scdc_status.source_test_update);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_rr_test:%d\n", p_hdmi_status->scdc_status.rr_test);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_ced_update:%d\n", p_hdmi_status->scdc_status.ced_update);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_status_update:%d\n", p_hdmi_status->scdc_status.status_update);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_tmds_bit_clock_ratio:%d\n", p_hdmi_status->scdc_status.tmds_bit_clock_ratio);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_scrambling_enable:%d\n", p_hdmi_status->scdc_status.scrambling_enable);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_tmds_scrambler_status:%d\n", p_hdmi_status->scdc_status.tmds_scrambler_status);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_flt_no_retrain:%d\n", p_hdmi_status->scdc_status.flt_no_retrain);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_rr_enable:%d\n", p_hdmi_status->scdc_status.rr_enable);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_ffe_levels:%d\n", p_hdmi_status->scdc_status.ffe_levels);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_frl_rate:%d\n", p_hdmi_status->scdc_status.frl_rate);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_dsc_decode_fail:%d\n", p_hdmi_status->scdc_status.dsc_decode_fail);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_flt_ready:%d\n", p_hdmi_status->scdc_status.flt_ready);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_clk_detect:%d\n", p_hdmi_status->scdc_status.clk_detect);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_ch0_locked:%d\n", p_hdmi_status->scdc_status.ch0_locked);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_ch1_locked:%d\n", p_hdmi_status->scdc_status.ch1_locked);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_ch2_locked:%d\n", p_hdmi_status->scdc_status.ch2_locked);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_ch3_locked:%d\n", p_hdmi_status->scdc_status.ch3_locked);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_lane0_ltp_request:%d\n", p_hdmi_status->scdc_status.lane0_ltp_request);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_lane1_ltp_request:%d\n", p_hdmi_status->scdc_status.lane1_ltp_request);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_lane2_ltp_request:%d\n", p_hdmi_status->scdc_status.lane2_ltp_request);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_lane3_ltp_request:%d\n", p_hdmi_status->scdc_status.lane3_ltp_request);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_ch0_ced_valid:%d\n", p_hdmi_status->scdc_status.ch0_ced_valid);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_ch1_ced_valid:%d\n", p_hdmi_status->scdc_status.ch1_ced_valid);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_ch2_ced_valid:%d\n", p_hdmi_status->scdc_status.ch2_ced_valid);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_ch3_ced_valid:%d\n", p_hdmi_status->scdc_status.ch3_ced_valid);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_ch0_ced:%d\n", p_hdmi_status->scdc_status.ch0_ced);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_ch1_ced:%d\n", p_hdmi_status->scdc_status.ch1_ced);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_ch2_ced:%d\n", p_hdmi_status->scdc_status.ch2_ced);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_ch3_ced:%d\n", p_hdmi_status->scdc_status.ch3_ced);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_rs_correction_valid:%d\n", p_hdmi_status->scdc_status.rs_correction_valid);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "scdc_rs_correcton_count:%d\n\n", p_hdmi_status->scdc_status.rs_correcton_count);
    total += len; buff_size-=len; buff+= len;

    //----------------------------------------
    // Error Status
    //----------------------------------------
    len = snprintf(buff, buff_size, "error : %d\n", p_hdmi_status->err_status.error);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "error_param1 : %d\n", p_hdmi_status->err_status.param1);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "error_param2 : %d\n\n", p_hdmi_status->err_status.param2);
    total += len; buff_size-=len; buff+= len;

    //----------------------------------------
    // EDID Status
    //----------------------------------------
    len = snprintf(buff, buff_size, "edid_size :%d\n", p_hdmi_status->edid_info.size);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "edid_data_block0 : 0x00, 0xff, 0xff, 0xff ..\n");
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "edid_data_block1 : 0x02, 0x03, 0x5b,0xf1 ..\n");
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "edid_data_block3 : ..\n");
    total += len; buff_size-=len; buff+= len;

    kfree(p_hdmi_status);
    return total;
}


///////////////////////////////////////////////////////////////////
// Module init/exit
///////////////////////////////////////////////////////////////////

static EXT_INPUT_PROC_ENTY *g_hdmi_proc_entry[4];


static int lgtv_hdmi_proc_init(void)
{
    int i;

    memset(&g_hdmi_proc_entry, 0, sizeof(g_hdmi_proc_entry));

    for (i=0; i<4; i++)
        g_hdmi_proc_entry[i] = create_lgtv_hdmi_status_proc_entry(i, v4l2_get_hdmi_status, (void*) i+1);

    return 0;
}

late_initcall(lgtv_hdmi_proc_init);
