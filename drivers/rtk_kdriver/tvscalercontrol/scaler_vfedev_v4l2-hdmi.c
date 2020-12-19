
//Kernel Header file
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/fs.h>		/* everything... */
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/kthread.h>
#include <linux/string.h>/*memset*/
#include <linux/videodev2.h>

#include <mach/rtk_platform.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#define to_user_ptr(x)          compat_ptr((UINT32) x)
#else
#define to_user_ptr(x)          ((void* __user)(x)) // convert 32 bit value to user pointer
#endif

#include <mach/rtk_log.h>
#define TAG_NAME "V4L2_VFE"


#include <media/v4l2-ctrls.h>
#include <media/v4l2-common.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>


#include "scaler_vfedev_v4l2-hdmi.h"
#include <tvscalercontrol/hdmirx/hdmi_common.h>
#include <tvscalercontrol/hdmirx/hdmi_vfe.h>
#include <tvscalercontrol/hdmirx/hdmi_vfe_config.h>
#include <tvscalercontrol/hdmirx/hdmi_emp.h>
#include <tvscalercontrol/hdmirx/hdmi_info_packet.h>
#include <tvscalercontrol/hdmirx/hdmi_scdc.h>
#include <tvscalercontrol/io/ioregdrv.h>

#include <linux/freezer.h>
#include <rtk_kdriver/rtk_crt.h>
#include <tvscalercontrol/hdmirx/hdmi_arc.h>
#include <tvscalercontrol/scaler_vfedev.h>
#include <include/rtk_kdriver/tvscalercontrol/hdcp2_2/hdcp2_interface.h>
#include <rbus/hdmi_p0_reg.h>

struct v4l2_vfe_hdmi_device {
	struct v4l2_device v4l2_dev;
	struct video_device *vfd;
};

static HDMI_V4L2_DEFINE_T v4l2_define_table[] =
{//type, sub_id, entry_name, history_record, execute_enable, total_call_cnt
	{V4L2_HDMI_OPEN, NO_SUB_ID, "OPEN", TRUE, TRUE, 0},
	{V4L2_HDMI_RELEASE, NO_SUB_ID, "RELEASE", TRUE, TRUE, 0},
	{V4L2_HDMI_READ, NO_SUB_ID, "READ", TRUE, TRUE, 0},
	{V4L2_HDMI_POLL, NO_SUB_ID, "POLL", TRUE, TRUE, 0},
	{V4L2_HDMI_MMAP, NO_SUB_ID, "MMAP", TRUE, TRUE, 0},
	{V4L2_HDMI_S_INPUT, NO_SUB_ID, "S_INPUT_CONNECT", TRUE, TRUE, 0},
	{V4L2_HDMI_G_INPUT, NO_SUB_ID, "G_INPUT", TRUE, TRUE, 0},
	{V4L2_HDMI_S_CTRL, V4L2_CID_EXT_HDMI_POWER_OFF, "S_CTRL_HDMI_POWER_OFF", TRUE, TRUE, 0},
	{V4L2_HDMI_S_CTRL, V4L2_CID_EXT_HDMI_DISCONNECT, "S_CTRL_HDMI_DISCONNECT", TRUE, TRUE, 0},
	{V4L2_HDMI_S_EXT_CTRL, V4L2_CID_EXT_HDMI_EDID, "S_EXT_CTRL_HDMI_EDID",TRUE, TRUE, 0},
	{V4L2_HDMI_S_EXT_CTRL, V4L2_CID_EXT_HDMI_HPD, "S_EXT_CTRL_HDMI_HPD", TRUE, TRUE, 0},
	{V4L2_HDMI_S_EXT_CTRL, V4L2_CID_EXT_HDMI_HDCP_KEY, "S_EXT_CTRL_HDMI_HDCP_KEY", TRUE, TRUE, 0},
	{V4L2_HDMI_S_EXT_CTRL, V4L2_CID_EXT_HDMI_EXPERT_SETTING, "S_EXT_CTRL_HDMI_EXPERT_SETTING", TRUE, TRUE, 0},
	{V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_TIMING_INFO, "G_EXT_CTRL_HDMI_TIMING_INFO", FALSE, TRUE, 0},
	{V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_DRM_INFO, "G_EXT_CTRL_HDMI_DRM_INFO", FALSE, TRUE, 0},
	{V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_VSI_INFO, "G_EXT_CTRL_HDMI_VSI_INFO", FALSE, TRUE, 0},
	{V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_SPD_INFO, "G_EXT_CTRL_HDMI_SPD_INFO", FALSE, TRUE, 0},
	{V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_AVI_INFO, "G_EXT_CTRL_HDMI_AVI_INFO", FALSE, TRUE, 0},
	{V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_PACKET_INFO, "G_EXT_CTRL_HDMI_PACKET_INFO", FALSE, TRUE, 0},
	{V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_EDID, "G_EXT_CTRL_HDMI_EDID", TRUE, TRUE, 0},
	{V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_CONNECTION_STATE, "G_EXT_CTRL_HDMI_CONNECTION_STATE", FALSE, TRUE, 0},
	{V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_HPD, "G_EXT_CTRL_HDMI_HPD", TRUE, TRUE, 0},
	{V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_DOLBY_HDR, "G_EXT_CTRL_HDMI_DOLBY_HDR", FALSE, TRUE, 0},
	{V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_HDCP_KEY, "G_EXT_CTRL_HDMI_HDCP_KEY", TRUE, TRUE, 0},
	{V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_VRR_FREQUENCY, "G_EXT_CTRL_HDMI_VRR_FREQUENCY", FALSE, TRUE, 0},
	{V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_EMP_INFO, "G_EXT_CTRL_HDMI_EMP_INFO", FALSE, TRUE, 0},
	{V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_DIAGNOSTICS_STATUS, "G_EXT_CTRL_HDMI_DIAGNOSTICS_STATUS", FALSE, TRUE, 0},
	{V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_PHY_STATUS, "G_EXT_CTRL_HDMI_PHY_STATUS", FALSE, TRUE, 0},
	{V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_LINK_STATUS, "G_EXT_CTRL_HDMI_LINK_STATUS", FALSE, TRUE, 0},
	{V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_VIDEO_STATUS, "G_EXT_CTRL_HDMI_VIDEO_STATUS", FALSE, TRUE, 0},
	{V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_AUDIO_STATUS, "G_EXT_CTRL_HDMI_AUDIO_STATUS", FALSE, TRUE, 0},
	{V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_HDCP_STATUS, "G_EXT_CTRL_HDMI_HDCP_STATUS", FALSE, TRUE, 0},
	{V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_SCDC_STATUS, "G_EXT_CTRL_HDMI_SCDC_STATUS", FALSE,  TRUE, 0},
	{V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_ERROR_STATUS, "G_EXT_CTRL_HDMI_ERROR_STATUS", FALSE, TRUE, 0},
};
const unsigned char g_v4l2_define_table_size = sizeof(v4l2_define_table)/sizeof(HDMI_V4L2_DEFINE_T);


static HDMI_V4L2_HISTORY_T m_v4l2_call_history_queue[HDMI_V4L2_HISTORY_QUEUE_SIZE];
static unsigned int m_call_history_queue_top_index = 0;

static int connect_port =0;
static unsigned char m_emp_data_buf[MAX_EM_HDR_INFO_LEN];

#define IS_VALID_CH(x)       ((x > V4L2_EXT_HDMI_INPUT_PORT_NONE) && (x < V4L2_EXT_HDMI_INPUT_PORT_ALL))


typedef unsigned char (*DETECTCALLBACK)(void);

//extern from scaler
extern void HDMI_register_callback(unsigned char enable, DETECTCALLBACK cb);
extern void HDMI_set_detect_flag(unsigned char enable);
extern  struct semaphore SetSource_Semaphore;/*This semaphore is for set source type*/
extern  struct semaphore HDMI_DetectSemaphore;/*This semaphore is set hdmi detect flag*/
extern  unsigned char HDMI_Reply_Zero_Timing ;
static unsigned char HDMI_V4L2_Reply_Zero_Timing_mute_cnt = ACTUAL_DETECT_RESULT;
/*
static unsigned char HDMI_V4L2_Reply_Zero_Timing_mute_cnt = ACTUAL_DETECT_RESULT;
static unsigned int HDMI_V4L2_ZERO_REPORT_RECORD_TIME = 0;//when ap polling the timing and the zero timing flag is ture. Record the time
*/
extern unsigned short HDMI_Input_Source;
extern  unsigned char HDMI_Global_Status;
extern void Set_Reply_Zero_Timing_Flag(unsigned char src, unsigned char flag);
extern unsigned char check_polling_time_period(unsigned int record_time, unsigned int cur_time, unsigned threshould);

static V4L2_MULTI_OPEN_CRTL_T m_multi_open =
{
	.open_cnt = 0,
	.init_cnt = 0,
};

#ifdef CONFIG_OPTEE_HDCP2
extern int rtk_hal_hdcp2_VFE_HDMI_WriteHDCP22(unsigned char *pdata, unsigned int size);
#endif

//-------------------------------------------------------
// LGTV procfs releated function
//-------------------------------------------------------

static pid_t connect_port_pid =0;       // process id of the connect port. requested by LGE

int v4l2_vfe_hdmi_get_open_count(void)
{
    return m_multi_open.open_cnt;
}

int v4l2_vfe_hdmi_get_connection_status(unsigned char port)
{
    return (port==connect_port) ? connect_port_pid : 0;   // return pid of the process that calling connection
}

int v4l2_vfe_hdmi_get_dolby_hdr_info(struct v4l2_ext_hdmi_dolby_hdr* p_hdr)
{
    p_hdr->type = V4L2_EXT_HDMI_DOLBY_HDR_TYPE_SDR;

    if (p_hdr->port==connect_port)
    {
        switch (get_HDMI_Dolby_VSIF_mode())
        {
        case DOLBY_HDMI_VSIF_DISABLE:
            p_hdr->type = V4L2_EXT_HDMI_DOLBY_HDR_TYPE_SDR;
            break;

        case DOLBY_HDMI_h14B_VSIF:
            p_hdr->type = V4L2_EXT_HDMI_DOLBY_HDR_TYPE_STANDARD_VSIF_1;
            break;

        case DOLBY_HDMI_VSIF_STD:
            p_hdr->type = V4L2_EXT_HDMI_DOLBY_HDR_TYPE_STANDARD_VSIF_2;
            break;

        case DOLBY_HDMI_VSIF_LL:
            p_hdr->type = V4L2_EXT_HDMI_DOLBY_HDR_TYPE_LOW_LATENCY;
            break;
        }
    }

    return 0;
}


//-------------------------------------------------------
// Main Function
//-------------------------------------------------------

static enum v4l2_ext_hdmi_color_depth _to_v4l2_color_depth(enum vfe_hdmi_color_depth_e cd)
{
    switch(cd) {
    case VFE_HDMI_COLOR_DEPTH_8B:  return V4L2_EXT_HDMI_COLOR_DEPTH_8BIT;
    case VFE_HDMI_COLOR_DEPTH_10B: return V4L2_EXT_HDMI_COLOR_DEPTH_10BIT;
    case VFE_HDMI_COLOR_DEPTH_12B: return V4L2_EXT_HDMI_COLOR_DEPTH_12BIT;
    case VFE_HDMI_COLOR_DEPTH_16B: return V4L2_EXT_HDMI_COLOR_DEPTH_16BIT;    
    }
    
    return V4L2_EXT_HDMI_COLOR_DEPTH_RESERVED;
}

int v4l2_vfe_hdmi_ext_link_status(struct v4l2_ext_hdmi_link_status* p_status)
{
	int ret = 0;
	unsigned char ch = 0;
	unsigned char port = 0xF;
	HDMI_AVI_T avi_info;
	HDMI_DRM_T drm_status;
	HDMI_AUDIO_ST audio_status;

	if (p_status == NULL)
	{
		HDMI_EMG("[v4l2_vfe_hdmi_ext_link_status][- ARG ERR] data is NULL \n");
		return EFAULT;
	}

	ch = p_status->port-1;

	if (hdmi_vfe_get_connected_channel(&ch)<0 || hdmi_vfe_get_hdmi_port(ch, &port)<0)
	{
		HDMI_WARN("[v4l2_vfe_hdmi_ext_link_status] input pamareter error, ch=%d\n", ch);
		return EFAULT;
	}

	memset(&avi_info, 0, sizeof(HDMI_AVI_T));
	memset(&drm_status, 0, sizeof(HDMI_DRM_T));

	newbase_hdmi_get_avi_infoframe(port, &avi_info);

	//---------------------------------------
	// Link Status
	//---------------------------------------
	p_status->hpd = (BOOLEAN)newbase_hdmi_get_hpd(port);
	p_status->hdmi_5v = (BOOLEAN)newbase_hdmi_get_5v_state(port);
	p_status->rx_sense = (BOOLEAN)newbase_hdmi_get_hpd(port);
	p_status->frame_rate_x100_hz =  (GET_MODE_IVFREQ()*10);
	p_status->dvi_hdmi_mode =  ((newbase_hdmi_get_hdmi_mode(port)==0)? 0 : 1);
	p_status->video_width =  (GET_MODE_IHWIDTH());//timing_info.h_act_len;
	p_status->video_height =  (GET_MODE_IVHEIGHT());//timing_info.v_act_len;

	if (newbase_hdmi_get_is_interlace(port))
		p_status->video_height <<= 1;	

	p_status->color_space = newbase_hdmi_get_colorspace_reg(port);
	switch(newbase_hdmi_get_colordepth(port))
	{
	case 0:
		p_status->color_depth = 8;
		break;
	case 1:
		p_status->color_depth = 10;
		break;
	case 2:
		p_status->color_depth = 12;
		break;
	default:
		p_status->color_depth = 8;
		HDMI_EMG("[v4l2_vfe_hdmi_ext_link_status port:%d][- ARG ERR] newbase_hdmi_get_colordepth=%d \n", port, newbase_hdmi_get_colordepth(port));
		break;
	}
	p_status->colorimetry = avi_info.C; // (v4l2_ext_hdmi_avi_colorimetry)
	p_status->ext_colorimetry = avi_info.EC; // (v4l2_ext_hdmi_avi_ext_colorimetry)
	p_status->additional_colorimetry = avi_info.ACE; // (v4l2_ext_hdmi_avi_additional_colorimetry)

	if (newbase_hdmi_get_drm_infoframe(port, &drm_status))
	{
		switch (drm_status.eEOTFtype)
		{
		case 0:
		case 1:
			p_status->hdr_type = V4L2_EXT_HDR_MODE_SDR;
			break;
		case 2:
			p_status->hdr_type  = V4L2_EXT_HDR_MODE_HDR10;
			break;
		case 3:
			p_status->hdr_type  = V4L2_EXT_HDR_MODE_HLG;
			break;
		case 4:
			p_status->hdr_type  = V4L2_EXT_HDR_MODE_DOLBY;
			break;
		default:
			HDMI_EMG("[v4l2_vfe_hdmi_ext_link_status port:%d] Invalid DRM EOTF type =%d\n", port, drm_status.eEOTFtype);
			p_status->hdr_type  = V4L2_EXT_HDR_MODE_SDR;	//UNKNOW
			break;
		}
	}
	else
	{
              p_status->hdr_type = KADP_VFE_HDMI_HDR_TYPE_SDR;
	}

	if (newbase_hdmi_audio_get_audio_status(port, &audio_status)==0)
	{
		switch(audio_status.coding_type)
		{
		case AUDIO_FORMAT_DVI:
		case AUDIO_FORMAT_NO_AUDIO:
			p_status->audio_format = V4L2_EXT_HDMI_AUDIO_FORMAT_NOAUDIO;
			break;
		case AUDIO_FORMAT_PCM:
			p_status->audio_format = V4L2_EXT_HDMI_AUDIO_FORMAT_PCM;
			break;
		case AUDIO_FORMAT_AC3:
			p_status->audio_format = V4L2_EXT_HDMI_AUDIO_FORMAT_AC3;
			break;
		case AUDIO_FORMAT_DTS:
			p_status->audio_format = V4L2_EXT_HDMI_AUDIO_FORMAT_DTS;
			break;
		case AUDIO_FORMAT_AAC:
			p_status->audio_format = V4L2_EXT_HDMI_AUDIO_FORMAT_AAC;
			break;
		case AUDIO_FORMAT_MPEG:
			p_status->audio_format = V4L2_EXT_HDMI_AUDIO_FORMAT_MPEG;
			break;
		case AUDIO_FORMAT_DTS_HD_MA:
			p_status->audio_format = V4L2_EXT_HDMI_AUDIO_FORMAT_DTS_HD_MA;
			break;
		case AUDIO_FORMAT_DTS_EXPRESS:
			p_status->audio_format = V4L2_EXT_HDMI_AUDIO_FORMAT_DTS_EXPRESS;
			break;
		case AUDIO_FORMAT_DTS_CD:
			p_status->audio_format = V4L2_EXT_HDMI_AUDIO_FORMAT_DTS_CD;
			break;
		case AUDIO_FORMAT_EAC3:
			p_status->audio_format = V4L2_EXT_HDMI_AUDIO_FORMAT_EAC3;
			break;
		case AUDIO_FORMAT_EAC3_ATMOS:
			p_status->audio_format = V4L2_EXT_HDMI_AUDIO_FORMAT_EAC3_ATMOS;
			break;
		case AUDIO_FORMAT_MAT:
			p_status->audio_format = V4L2_EXT_HDMI_AUDIO_FORMAT_MAT;
			break;
		case AUDIO_FORMAT_MAT_ATMOS:
			p_status->audio_format = V4L2_EXT_HDMI_AUDIO_FORMAT_MAT_ATMOS;
			break;
		case AUDIO_FORMAT_TRUEHD:
			p_status->audio_format = V4L2_EXT_HDMI_AUDIO_FORMAT_TRUEHD;
			break;
		case AUDIO_FORMAT_TRUEHD_ATMOS:
			p_status->audio_format = V4L2_EXT_HDMI_AUDIO_FORMAT_TRUEHD_ATMOS;
			break;
		case AUDIO_FORMAT_DEFAULT:
		default:
			HDMI_WARN("[v4l2_vfe_hdmi_ext_diagnostic_info port:%d] undefined audio coding type: %d\n", port, audio_status.coding_type);
			p_status->audio_format = V4L2_EXT_HDMI_AUDIO_FORMAT_NOAUDIO;
			break;
		}
		p_status->audio_sampling_freq = audio_status.acr_info.acr_freq;
		p_status->audio_channel_number = audio_status.channel_num;
	}
	else
	{
		p_status->audio_format = V4L2_EXT_HDMI_AUDIO_FORMAT_NOAUDIO;
		p_status->audio_sampling_freq = 0;
		p_status->audio_channel_number = 0;
	}

	return ret;
}

int v4l2_vfe_hdmi_ext_phy_status(struct v4l2_ext_hdmi_phy_status* p_status)
{
	int ret = 0;
	unsigned char ch = 0;
	unsigned char port = 0xF;
	HDMIRX_PHY_STRUCT_T* p_phy_st;
	unsigned char tmds_ch_num = 0;

	if (p_status == NULL)
	{
		HDMI_EMG("[v4l2_vfe_hdmi_ext_phy_status][- ARG ERR] data is NULL \n");
		return EFAULT;
	}

	ch = p_status->port-1;

	if (hdmi_vfe_get_connected_channel(&ch)<0 || hdmi_vfe_get_hdmi_port(ch, &port)<0)
	{
		HDMI_WARN("[v4l2_vfe_hdmi_ext_phy_status] input pamareter error, ch=%d\n", ch);
		return EFAULT;
	}

	//---------------------------------------
	// Phy Status
	//---------------------------------------
	p_phy_st = newbase_rxphy_get_status(port);


	p_status->lock_status = newbase_rxphy_get_setphy_done(port);
	p_status->tmds_clk_khz = newbase_hdmi_get_tmds_clockx10(port)*100;
	p_status->link_type = (p_phy_st->frl_mode == MODE_TMDS) ? V4L2_EXT_HDMI_LINK_TYPE_TMDS : V4L2_EXT_HDMI_LINK_TYPE_FRL;
	p_status->link_lane = (newbase_hdmi_hd21_get_frl_lane(p_phy_st->frl_mode) == HDMI_3LANE) ? V4L2_EXT_HDMI_LINK_LANE_NUMBER_3 : V4L2_EXT_HDMI_LINK_LANE_NUMBER_4;
	switch (p_phy_st->frl_mode)
	{
	case MODE_FRL_3G_3_LANE:
		p_status->link_rate = V4L2_EXT_HDMI_LINK_RATE_3G;
		break;
	case MODE_FRL_6G_3_LANE:
		p_status->link_rate = V4L2_EXT_HDMI_LINK_RATE_6G;
		break;
	case MODE_FRL_6G_4_LANE:
		p_status->link_rate = V4L2_EXT_HDMI_LINK_RATE_6G;
		break;
	case MODE_FRL_8G_4_LANE:
		p_status->link_rate = V4L2_EXT_HDMI_LINK_RATE_8G;
		break;
	case MODE_FRL_10G_4_LANE:
		p_status->link_rate = V4L2_EXT_HDMI_LINK_RATE_10G;
		break;
	case MODE_FRL_12G_4_LANE:
		p_status->link_rate = V4L2_EXT_HDMI_LINK_RATE_12G;
		break;
	default:
		HDMI_WARN("[v4l2_vfe_hdmi_ext_phy_status port:%d] undefined frl mode: %d\n", port, p_phy_st->frl_mode);
		p_status->link_rate = V4L2_EXT_HDMI_LINK_RATE_3G;
		break;
	}

	for (tmds_ch_num=0; tmds_ch_num<V4L2_EXT_HDMI_TMDS_CH_NUM; tmds_ch_num++)
	{
		p_status->ctle_eq_min_range[tmds_ch_num] = 0;
		p_status->ctle_eq_max_range[tmds_ch_num] = 0;
		p_status->ctle_eq_result[tmds_ch_num] = 0;
		p_status->error[tmds_ch_num] = 0;
	}

	return ret;
}

int v4l2_vfe_hdmi_ext_video_status(struct v4l2_ext_hdmi_video_status* p_status)
{
	int ret = 0;
	unsigned char ch = 0;
	unsigned char port = 0xF;
	unsigned int vfreq = 0;
	unsigned char is_interlace = 0;

	if (p_status == NULL)
	{
		HDMI_EMG("[v4l2_vfe_hdmi_ext_video_status][- ARG ERR] data is NULL \n");
		return EFAULT;
	}

	ch = p_status->port-1;

	if (hdmi_vfe_get_connected_channel(&ch)<0 || hdmi_vfe_get_hdmi_port(ch, &port)<0)
	{
		HDMI_WARN("[v4l2_vfe_hdmi_ext_video_status] input pamareter error, ch=%d\n", ch);
		return EFAULT;
	}

	is_interlace = newbase_hdmi_get_is_interlace(port);

	//---------------------------------------
	// Video Status
	//---------------------------------------
	vfreq = GET_MODE_IVFREQ();  // unit = 0.1 Hz

	p_status->video_width_real = (GET_MODE_IHWIDTH());
	p_status->video_htotal_real = (GET_MODE_IHTOTAL());
	p_status->video_height_real = (GET_MODE_IVHEIGHT());
	p_status->video_vtotal_real = (GET_MODE_IVTOTAL());

	if (is_interlace)
	{
		p_status->video_height_real <<= 1;
		p_status->video_vtotal_real <<= 1;
		p_status->pixel_clock_khz = p_status->video_htotal_real * (p_status->video_vtotal_real>>1) * vfreq/10000;  //unit kHz (where GET_MODE_IVFREQ unit is 0.1 Hz)
	}
	else
	{
		p_status->pixel_clock_khz = p_status->video_htotal_real * p_status->video_vtotal_real * vfreq/10000;  //unit kHz (where GET_MODE_IVFREQ unit is 0.1 Hz)
	}

	p_status->current_vrr_refresh_rate = (drvif_Hdmi_GetVRREnable()==1) ? get_scaler_ivs_framerate(): 0;
	return ret;
}

int v4l2_vfe_hdmi_ext_audio_status(struct v4l2_ext_hdmi_audio_status* p_status)
{
	int ret = 0;
	unsigned char ch = 0;
	unsigned char port = 0xF;
	HDMI_AUDIO_ST audio_status;

	if (p_status == NULL)
	{
		HDMI_EMG("[v4l2_vfe_hdmi_ext_audio_status][- ARG ERR] data is NULL \n");
		return EFAULT;
	}

	ch = p_status->port-1;

	if (hdmi_vfe_get_connected_channel(&ch)<0 || hdmi_vfe_get_hdmi_port(ch, &port)<0)
	{
		HDMI_WARN("[v4l2_vfe_hdmi_ext_audio_status] input pamareter error, ch=%d\n", ch);
		return EFAULT;
	}

	//---------------------------------------
	// Audio Status
	//---------------------------------------
	if (newbase_hdmi_audio_get_audio_status(port, &audio_status)==0)
	{
		p_status->pcm_N = audio_status.acr_info.n;
		p_status->pcm_CTS= audio_status.acr_info.cts;
		p_status->LayoutBitValue = (audio_status.channel_mode)?1:0;
		p_status->ChannelStatusBits= audio_status.channel_num;// like 2ch -2, 5.1ch - 6, 7.1ch - 8
	}
	else
	{
		p_status->pcm_N = 0;
		p_status->pcm_CTS= 0;
		p_status->LayoutBitValue = 0;
		p_status->ChannelStatusBits= 0;// like 2ch -2, 5.1ch - 6, 7.1ch - 8
	}

	return ret;
}

int v4l2_vfe_hdmi_ext_hdcp_status(struct v4l2_ext_hdmi_hdcp_status* p_status)
{
	int ret = 0;
	unsigned char ch = 0;
	unsigned char port = 0xF;
	HDMI_HDCP_E newbase_hdcp_version = HDCP_OFF;
	unsigned char aksv[5] = {0};
	unsigned char ri[2] = {0};
	HDMI_PORT_INFO_T* p_hdmi_rx_st = NULL;
	HDMI_HDCP_ST* p_hdcp_st = NULL;

	if (p_status == NULL)
	{
		HDMI_EMG("[v4l2_vfe_hdmi_ext_hdcp_status][- ARG ERR] data is NULL \n");
		return EFAULT;
	}

	ch = p_status->port-1;

	if (hdmi_vfe_get_connected_channel(&ch)<0 || hdmi_vfe_get_hdmi_port(ch, &port)<0)
	{
		HDMI_WARN("[v4l2_vfe_hdmi_ext_hdcp_status] input pamareter error, ch=%d\n", ch);
		return EFAULT;
	}

	//---------------------------------------
	// HDCP Status
	//---------------------------------------
	p_hdmi_rx_st = newbase_hdmi_get_rx_port_info(port);
	newbase_hdcp_version = newbase_hdcp_get_auth_mode(port);
	p_hdcp_st = newbase_hdcp_get_hdcp_status(port);

	switch(newbase_hdcp_version)
	{
	case HDCP14:
		p_status->hdcp_version = V4L2_EXT_HDMI_HDCP_VERSION_14;
		break;
	case HDCP22:
		p_status->hdcp_version = V4L2_EXT_HDMI_HDCP_VERSION_22;
		break;
	default:
		p_status->hdcp_version = V4L2_EXT_HDMI_HDCP_VERSION_RESERVED;
		break;
	}

	switch(newbase_hdcp_version)
	{
	case HDCP14:
		if (newbase_hdmi_hdcp14_read_aksv(port, aksv) && newbase_hdmi_hdcp14_read_ri(port,ri))
			p_status->auth_status = V4L2_EXT_HDMI_HDCP_AUTH_STATUS_AUTHENTICATED;
		else if(newbase_hdmi_hdcp14_read_aksv(port, aksv))
			p_status->auth_status = V4L2_EXT_HDMI_HDCP_AUTH_STATUS_IN_PROGRESS;
		else
			p_status->auth_status = V4L2_EXT_HDMI_HDCP_AUTH_STATUS_UNAUTHENTICATED;
		break;
	case HDCP22:
		if(p_hdcp_st->hdcp2_state == HDCP2_SEND_EKS)
			p_status->auth_status = V4L2_EXT_HDMI_HDCP_AUTH_STATUS_AUTHENTICATED;
		else if(p_hdcp_st->hdcp2_state == HDCP2_STOREE_KM || p_hdcp_st->hdcp2_state == HDCP2_NO_STOREE_KM)
			p_status->auth_status = V4L2_EXT_HDMI_HDCP_AUTH_STATUS_IN_PROGRESS;
		else
			p_status->auth_status = V4L2_EXT_HDMI_HDCP_AUTH_STATUS_UNAUTHENTICATED;
		break;
	case NO_HDCP:
	default:
		p_status->auth_status = V4L2_EXT_HDMI_HDCP_AUTH_STATUS_UNAUTHENTICATED;
		break;
	}

	p_status->encEn= p_hdcp_st->hdcp_enc;

	newbase_hdmi_hdcp14_read_an(port, p_status->hdcp14_status.An);
	newbase_hdmi_hdcp14_read_aksv(port, p_status->hdcp14_status.Aksv);
	newbase_hdmi_hdcp14_read_bksv(port, p_status->hdcp14_status.Bksv);
	newbase_hdmi_hdcp14_read_ri(port , p_status->hdcp14_status.Ri);
	newbase_hdmi_hdcp14_read_bcaps(port ,(unsigned char*) &(p_status->hdcp14_status.Bcaps));
	newbase_hdmi_hdcp14_read_bstatus(port ,(unsigned char*) &(p_status->hdcp14_status.Bstatus));

	p_status->hdcp22_status.ake_init_count_since_5v = p_hdcp_st->hdcp2_auth_count;
	p_status->hdcp22_status.reauth_req_count_since_5v = p_hdcp_st->hdcp_reauth_cnt;

	return ret;
}

int v4l2_vfe_hdmi_ext_scdc_status(struct v4l2_ext_hdmi_scdc_status* p_status)
{
	int ret = 0;
	unsigned char ch = 0;
	unsigned char port = 0xF;
	HDMIRX_PHY_STRUCT_T* p_phy_st;
	unsigned char scdc_10 = 0;
	unsigned char scdc_20 = 0;
	unsigned char scdc_21 = 0;
	unsigned char scdc_30 = 0;
	unsigned char scdc_31 = 0;
	unsigned char scdc_40 = 0;
	unsigned char scdc_41 = 0;
	unsigned char scdc_42 = 0;
	unsigned char scdc_51 = 0;
	unsigned char scdc_53 = 0;
	unsigned char scdc_55 = 0;
	unsigned char scdc_58 = 0;
	unsigned char scdc_5a = 0;
	unsigned int scdc_ced_err[4] = {0, 0, 0, 0};

	if (p_status == NULL)
	{
		HDMI_EMG("[v4l2_vfe_hdmi_ext_scdc_status][- ARG ERR] data is NULL \n");
		return EFAULT;
	}

	ch = p_status->port-1;

	if (hdmi_vfe_get_connected_channel(&ch)<0 || hdmi_vfe_get_hdmi_port(ch, &port)<0)
	{
		HDMI_WARN("[v4l2_vfe_hdmi_ext_scdc_status] input pamareter error, ch=%d\n", ch);
		return EFAULT;
	}

	//---------------------------------------
	// SCDC Status
	//---------------------------------------
	scdc_10 = lib_hdmi_scdc_read(port,  SCDC_UPDATE_FLAGS);
	scdc_20 = lib_hdmi_scdc_read(port,  SCDC_TMDS_CONFIGURATION);
	scdc_21 = lib_hdmi_scdc_read(port,  SCDC_TMDS_SCRAMBLER_STATUS);
	scdc_30 = lib_hdmi_scdc_read(port,  SCDC_CONFIGURATION);
	scdc_31 = lib_hdmi_scdc_read(port,  SCDC_CONFIGURATION_1);
	scdc_40 = lib_hdmi_scdc_read(port,  SCDC_STATUS_FLAGS);
	scdc_41 = lib_hdmi_scdc_read(port,  SCDC_STATUS_FLAGS_1);
	scdc_42 = lib_hdmi_scdc_read(port,  SCDC_STATUS_FLAGS_2);
	scdc_51 = lib_hdmi_scdc_read(port,  SCDC_LN0_CED_ERROR_H);
	scdc_53 = lib_hdmi_scdc_read(port,  SCDC_LN1_CED_ERROR_H);
	scdc_55 = lib_hdmi_scdc_read(port,  SCDC_LN2_CED_ERROR_H);
	scdc_58 = lib_hdmi_scdc_read(port,  SCDC_LN3_CED_ERROR_H);
	scdc_5a = lib_hdmi_scdc_read(port,  SCDC_RS_CORRECTION_CNT_H);

	p_phy_st = newbase_rxphy_get_status(port);
	lib_hdmi_char_err_get_scdc_ced(port, p_phy_st->frl_mode, scdc_ced_err);

	p_status->source_version = lib_hdmi_scdc_read(port,  SCDC_SOURCE_VERSION);
	p_status->sink_version = lib_hdmi_scdc_read(port,  SCDC_SINK_VERSION);

	//SCDC 0x10
	p_status->rsed_update = (scdc_10&SCDC_UPDATE_FLAGS_RSED_UPDATE) ? 1 : 0;
	p_status->flt_update= (scdc_10&SCDC_UPDATE_FLAGS_FLT_UPDATE) ? 1 : 0;
	p_status->frl_start= (scdc_10&SCDC_UPDATE_FLAGS_FRL_START) ? 1 : 0;
	p_status->source_test_update=  (scdc_10&SCDC_UPDATE_FLAGS_SOURCE_TEST_UPDATE) ? 1 : 0;
	p_status->rr_test= (scdc_10&SCDC_UPDATE_FLAGS_RR_TEST) ? 1 : 0;
	p_status->ced_update= (scdc_10&SCDC_UPDATE_FLAGS_CED_UPDATE) ? 1 : 0;
	p_status->status_update= (scdc_10&SCDC_UPDATE_FLAGS_STATUS_UPDATE) ? 1 : 0;

	//SCDC 0x20
	p_status->tmds_bit_clock_ratio= (scdc_20&SCDC_TMDS_BIT_CLOCK_RATIO)>>1;
	p_status->scrambling_enable= (scdc_20&SCDC_SCAMBLING_ENABLE);

	//SCDC 0x21
	p_status->tmds_scrambler_status= (scdc_21&SCDC_SCAMBLING_STATUS);

	//SCDC 0x30
	p_status->flt_no_retrain= (scdc_30&SCDC_FLT_NO_RETRAIN);
	p_status->rr_enable= (scdc_30&SCDC_RR_ENABLE);

	//SCDC 0x31
	p_status->ffe_levels= (scdc_31&SCDC_FFE_LEVELS)>>4 ;
	p_status->frl_rate= (scdc_31&SCDC_FRL_RATE);

	//SCDC 0x40
	p_status->dsc_decode_fail= (scdc_40&SCDC_STATUS_DSC_DECODE_FAIL) ? 1 : 0;
	p_status->flt_ready= (scdc_40&SCDC_STATUS_FLT_READY) ? 1 : 0;
	p_status->clk_detect= (scdc_40&SCDC_STATUS_CLOCK_DETECTED) ? 1 : 0;
	p_status->ch0_locked= (scdc_40&SCDC_STATUS_CH0_LN0_LOCK) ? 1 : 0;
	p_status->ch1_locked= (scdc_40&SCDC_STATUS_CH1_LN1_LOCK) ? 1 : 0;
	p_status->ch2_locked= (scdc_40&SCDC_STATUS_CH2_LN2_LOCK) ? 1 : 0;
	p_status->ch3_locked= (scdc_40&SCDC_STATUS_LANE3_LOCKED) ? 1 : 0;

	//SCDC 0x41
	p_status->lane0_ltp_request= (scdc_41&SCDC_STATUS_LN0_LTP_REQ);
	p_status->lane1_ltp_request= (scdc_41&SCDC_STATUS_LN1_LTP_REQ)>>4;

	//SCDC 0x42
	p_status->lane2_ltp_request= (scdc_42&SCDC_STATUS_LN2_LTP_REQ);
	p_status->lane3_ltp_request= (scdc_42&SCDC_STATUS_LN3_LTP_REQ)>>4;

	//SCDC CED
	p_status->ch0_ced_valid= (scdc_51&SCDC_CED_ERROR_LN1_VALID)>>7;
	p_status->ch1_ced_valid= (scdc_53&SCDC_CED_ERROR_LN1_VALID)>>7;
	p_status->ch2_ced_valid= (scdc_55&SCDC_CED_ERROR_LN2_VALID)>>7;
	p_status->ch3_ced_valid= (scdc_58&SCDC_CED_ERROR_LN3_VALID)>>7;

	p_status->ch0_ced= scdc_ced_err[0];
	p_status->ch1_ced= scdc_ced_err[1];
	p_status->ch2_ced= scdc_ced_err[2];
	p_status->ch3_ced= scdc_ced_err[3];

	//RS C
	p_status->rs_correction_valid= (scdc_5a&SCDC_RS_C_VALID)>>7;
	p_status->rs_correcton_count = lib_hdmi_hd21_get_rs_err_cnt(port);

	return ret;
}

int v4l2_vfe_hdmi_ext_diagnostic_info(struct v4l2_ext_hdmi_diagnostics_status* hdmi_v4l2_ext_diagnostics_status)
{
	int ret = 0;
	unsigned char ch = 0;
	unsigned char port = 0xF;
	struct v4l2_ext_hdmi_link_status ext_hdmi_link_status;
	struct v4l2_ext_hdmi_phy_status ext_hdmi_phy_status;
	struct v4l2_ext_hdmi_video_status ext_hdmi_video_status;
	struct v4l2_ext_hdmi_audio_status ext_hdmi_audio_status;
	struct v4l2_ext_hdmi_hdcp_status ext_hdmi_hdcp_status;
	struct v4l2_ext_hdmi_scdc_status ext_hdmi_scdc_status;

	if (hdmi_v4l2_ext_diagnostics_status == NULL)
	{
		HDMI_EMG("[v4l2_vfe_hdmi_ext_diagnostic_info][- ARG ERR] data is NULL \n");
		return EFAULT;
	}

	ch = hdmi_v4l2_ext_diagnostics_status->port-1;

	if (hdmi_vfe_get_connected_channel(&ch)<0 || hdmi_vfe_get_hdmi_port(ch, &port)<0)
	{
		HDMI_WARN("get_diagnostic  info forget to connect hdmi port\n");
		return EFAULT;
	}

	if (newbase_hdmi_get_video_state(port) < MAIN_FSM_HDMI_DISPLAY_ON)
	{
		HDMI_WARN("get_diagnostic info is not ready\n");
		return EFAULT;
	}

	//---------------------------------------
	// Link Status
	//---------------------------------------
	ext_hdmi_link_status.port = hdmi_v4l2_ext_diagnostics_status->port;
	ret |= v4l2_vfe_hdmi_ext_link_status(&ext_hdmi_link_status);
	memcpy(&(hdmi_v4l2_ext_diagnostics_status->link_status), &ext_hdmi_link_status, sizeof(struct v4l2_ext_hdmi_link_status));

	//---------------------------------------
	// Phy Status
	//---------------------------------------
	ext_hdmi_phy_status.port = hdmi_v4l2_ext_diagnostics_status->port;
	ret |= v4l2_vfe_hdmi_ext_phy_status(&ext_hdmi_phy_status);
	memcpy(&(hdmi_v4l2_ext_diagnostics_status->phy_status), &ext_hdmi_phy_status, sizeof(struct v4l2_ext_hdmi_phy_status));

	//---------------------------------------
	// Video Status
	//---------------------------------------
	ext_hdmi_video_status.port = hdmi_v4l2_ext_diagnostics_status->port;
	ret |= v4l2_vfe_hdmi_ext_video_status(&ext_hdmi_video_status);
	memcpy(&(hdmi_v4l2_ext_diagnostics_status->video_status), &ext_hdmi_video_status, sizeof(struct v4l2_ext_hdmi_video_status));

	//---------------------------------------
	// Audio Status
	//---------------------------------------
	ext_hdmi_audio_status.port = hdmi_v4l2_ext_diagnostics_status->port;
	ret |= v4l2_vfe_hdmi_ext_audio_status(&ext_hdmi_audio_status);
	memcpy(&(hdmi_v4l2_ext_diagnostics_status->audio_status), &ext_hdmi_audio_status, sizeof(struct v4l2_ext_hdmi_audio_status));

	//---------------------------------------
	// HDCP Status
	//---------------------------------------
	ext_hdmi_hdcp_status.port = hdmi_v4l2_ext_diagnostics_status->port;
	ret |= v4l2_vfe_hdmi_ext_hdcp_status(&ext_hdmi_hdcp_status);
	memcpy(&(hdmi_v4l2_ext_diagnostics_status->hdcp_status), &ext_hdmi_hdcp_status, sizeof(struct v4l2_ext_hdmi_hdcp_status));

	//---------------------------------------
	// SCDC Status
	//---------------------------------------
	ext_hdmi_scdc_status.port = hdmi_v4l2_ext_diagnostics_status->port;
	ret |= v4l2_vfe_hdmi_ext_scdc_status(&ext_hdmi_scdc_status);
	memcpy(&(hdmi_v4l2_ext_diagnostics_status->scdc_status), &ext_hdmi_scdc_status, sizeof(struct v4l2_ext_hdmi_scdc_status));

	return ret;
}

int v4l2_vfe_hdmi_ext_error_status(struct v4l2_ext_hdmi_error_status* p_status)
{
	int ret = 0;
	unsigned char ch = 0;
	unsigned char port = 0xF;
	HDMI_PORT_INFO_T* p_hdmi_rx_st;
	HDMIRX_PHY_STRUCT_T* p_phy_st;
	HDMI_HDCP_ST* p_hdcp_st;
	HDMI_HDCP_E newbase_hdcp_version = HDCP_OFF;

	if (p_status == NULL)
	{
		HDMI_EMG("[v4l2_vfe_hdmi_ext_error_status][- ARG ERR] data is NULL \n");
		return EFAULT;
	}

	ch = p_status->port-1;

	if (hdmi_vfe_get_connected_channel(&ch)<0 || hdmi_vfe_get_hdmi_port(ch, &port)<0)
	{
		HDMI_WARN("[v4l2_vfe_hdmi_ext_error_status] input pamareter error, ch=%d\n", ch);
		p_status->error= V4L2_EXT_HDMI_ERROR_TYPE_FAILED;//KADP_HDMI_ERROR_FAILED
		return EFAULT;
	}

	//---------------------------------------
	// Error Status
	//---------------------------------------
	p_hdmi_rx_st = newbase_hdmi_get_rx_port_info(port);
	p_hdcp_st = newbase_hdcp_get_hdcp_status(port);
	p_phy_st = newbase_rxphy_get_status(port);
	newbase_hdcp_version = newbase_hdcp_get_auth_mode(port);

	if (p_hdmi_rx_st==NULL || p_hdcp_st == NULL)
	{
		HDMI_EMG("[v4l2_vfe_hdmi_ext_error_status][- ARG ERR] p_hdmi_rx_st or p_hdcp_st is NULL \n");
		return EFAULT;
	}

	if (p_phy_st == NULL)
	{
		HDMI_EMG("[v4l2_vfe_hdmi_ext_error_status][- ARG ERR] p_phy_st is NULL \n");
		return EFAULT;
	}
	
	if(newbase_hdmi_gcp_err_det(port, DIAGNOSTIC_GCP_ERROR_THD, 0xff ,50))//GCP_ERROR
	{
		p_status->error |= V4L2_EXT_HDMI_ERROR_TYPE_GCP_ERROR;
		HDMI_EMG("[v4l2_vfe_hdmi_ext_error_status port:%d] GCP error\n", port);
	}

	if((newbase_hdcp_version == HDCP22) && (p_hdcp_st->hdcp_reauth_cnt>0))
	{
		p_status->error |= V4L2_EXT_HDMI_ERROR_TYPE_HDCP22_REAUTH;
		HDMI_EMG("[v4l2_vfe_hdmi_ext_error_status port:%d] HDCP22_REAUTH error, hdcp_reauth_cnt=%d\n", port, p_hdcp_st->hdcp_reauth_cnt);
	}

	if((p_phy_st->frl_mode == MODE_TMDS) && (newbase_hdmi_tmds_err_thd_check(port, DIAGNOSTIC_TMDS_ERROR_THD)))//TMDS_ERROR //4 frame(64ms) will get one result
	{
		p_status->error |= V4L2_EXT_HDMI_ERROR_TYPE_TMDS_ERROR;
		//HDMI_EMG("[v4l2_vfe_hdmi_ext_error_status port:%d] TMDS error\n", port);
	}

	//V4L2_EXT_HDMI_ERROR_TYPE_PHY_LOW_RANGE
	//TO DO

	//V4L2_EXT_HDMI_ERROR_TYPE_PHY_ABNORMAL
	//TO DO

	if(newbase_hdmi_ced_err_thd_check(port, DIAGNOSTIC_CED_ERROR_THD))//CED_ERROR //4 frame(64ms) will get one result
	{
		p_status->error |= V4L2_EXT_HDMI_ERROR_TYPE_CED_ERROR;
		//HDMI_EMG("[v4l2_vfe_hdmi_ext_error_status port:%d] Diagnostics CED error\n", port);
	}

	//V4L2_EXT_HDMI_ERROR_TYPE_AUDIO_BUFFER
	//TO DO

	if(newbase_hdmi_get_online_measure_error_flag(port) && (newbase_hdmi_get_video_state(port) >= MAIN_FSM_HDMI_DISPLAY_ON))
	{
		p_status->error |= V4L2_EXT_HDMI_ERROR_TYPE_UNSTABLE_SYNC;
	}

	if((p_hdmi_rx_st->bch_err_cnt >DIAGNOSTIC_BCH_ERROR_THD))
	{
		p_status->error |= V4L2_EXT_HDMI_ERROR_TYPE_BCH;
	}

	if((p_phy_st->frl_mode != MODE_TMDS) && (lt_fsm_status[port].lt_error_status != LT_ERROR_NONE))
	{
		p_status->error |= V4L2_EXT_HDMI_ERROR_TYPE_FLT;
	}

	return ret;
}

int vfe_hdmi_v4l2_set_expert_setting(struct v4l2_ext_hdmi_expert_setting* p_expert_setting)
{
	int ret = 0;
	unsigned char ch = 0;
	unsigned char port = 0xF;

	if (p_expert_setting == NULL)
	{
		HDMI_EMG("[vfe_hdmi_v4l2_set_expert_setting][- ARG ERR] data is NULL \n");
		return EFAULT;
	}
	ch = p_expert_setting->port - 1;

	if (hdmi_vfe_get_connected_channel(&ch)<0 || hdmi_vfe_get_hdmi_port(ch, &port)<0)
	{
		HDMI_WARN("[vfe_hdmi_v4l2_set_expert_setting] input pamareter error, ch=%d\n", ch);
		return EFAULT;
	}

	switch(p_expert_setting->type)
	{
	case V4L2_EXT_HDMI_EXPERT_SETTING_TYPE_HPD_LOW_DURATION:
		break;
	case V4L2_EXT_HDMI_EXPERT_SETTING_TYPE_TMDS_MANUAL_EQ_MODE:
		if(p_expert_setting->param1 > 1)
			p_expert_setting->param1 = 1;

		newbase_hdmi_set_eq_mode(port, p_expert_setting->param1);
		newbase_hdmi_manual_eq(port,7,7,7); 
		break;
	case V4L2_EXT_HDMI_EXPERT_SETTING_TYPE_TMDS_MANUAL_EQ_CH0:
		if(p_expert_setting->param1>255)
			p_expert_setting->param1 = 255;

		newbase_hdmi_manual_eq_ch(port, 0, p_expert_setting->param1);
		break;
	case V4L2_EXT_HDMI_EXPERT_SETTING_TYPE_TMDS_MANUAL_EQ_CH1:
		if(p_expert_setting->param1>255)
			p_expert_setting->param1 = 255;

		newbase_hdmi_manual_eq_ch(port, 1, p_expert_setting->param1);
		break;
	case V4L2_EXT_HDMI_EXPERT_SETTING_TYPE_TMDS_MANUAL_EQ_CH2:
		if(p_expert_setting->param1>255)
			p_expert_setting->param1 = 255;

		newbase_hdmi_manual_eq_ch(port, 2, p_expert_setting->param1);
		break;
	case V4L2_EXT_HDMI_EXPERT_SETTING_TYPE_TMDS_MANUAL_EQ_CH3:
		if(p_expert_setting->param1>255)
			p_expert_setting->param1 = 255;

		newbase_hdmi_manual_eq_ch(port, 3, p_expert_setting->param1);
		break;
	case V4L2_EXT_HDMI_EXPERT_SETTING_TYPE_TMDS_EQ_PERIOD:
		break;
	case V4L2_EXT_HDMI_EXPERT_SETTING_TYPE_VIDEO_STABLE_COUNT:
		if(p_expert_setting->param1 > newbase_hdmi_calculate_entry_max_value(HDMI_FLOW_CFG_GENERAL, HDMI_FLOW_CFG0_WAITSYNC_STABLE_CNT_THD))
			p_expert_setting->param1 = newbase_hdmi_calculate_entry_max_value(HDMI_FLOW_CFG_GENERAL, HDMI_FLOW_CFG0_WAITSYNC_STABLE_CNT_THD);

		SET_FLOW_CFG(HDMI_FLOW_CFG_GENERAL, HDMI_FLOW_CFG0_WAITSYNC_STABLE_CNT_THD, p_expert_setting->param1);
		break;
	case V4L2_EXT_HDMI_EXPERT_SETTING_TYPE_AUDIO_STABLE_COUNT:
		break;
	case V4L2_EXT_HDMI_EXPERT_SETTING_TYPE_DISABLE_HDCP22:
		newbase_hdmi_hdcp_disable_hdcp2(port, (p_expert_setting->param1) ? 1 : 0);
		break;
	case V4L2_EXT_HDMI_EXPERT_SETTING_TYPE_REAUTH_HDCP22:
		newbase_hdmi_hdcp22_set_reauth(port);
		break;
	case V4L2_EXT_HDMI_EXPERT_SETTING_TYPE_ON_TO_RXSENSE_TIME:
		break;
	case V4L2_EXT_HDMI_EXPERT_SETTING_TYPE_RXSENSE_TO_HPD_TIME:
		break;
	default:
		ret = EFAULT;
		HDMI_EMG("[vfe_hdmi_v4l2_set_expert_setting port:%d][- ARG ERR] Wrong type=%d \n", port, p_expert_setting->type);
		break;
	}


	return ret;
}


HDMI_V4L2_HISTORY_T* newbase_hdmi_v4l2_get_history_info(void)
{
	return m_v4l2_call_history_queue;
}

HDMI_V4L2_DEFINE_T* newbase_hdmi_v4l2_function_table(void)
{
	return v4l2_define_table;
}

int newbase_hdmi_v4l2_function_table_index(V4L2_HDMI_TYPE type, unsigned int sub_id)
{
	unsigned int i =0;
	int target_index = -1;

	//Find target index of define table
	for(i=0; i< (sizeof(v4l2_define_table)/sizeof(HDMI_V4L2_DEFINE_T));i++)
	{
		if((type == v4l2_define_table[i].type) && (sub_id ==v4l2_define_table[i].sub_id))
		{
			target_index = i;
			break;
		}
	}

	if(target_index == -1)
	{
		HDMI_EMG("[HDMI][V4L2][ERR] Invalid parameter, type=%d, sub_id=%d\n", type, sub_id);
		return -1;
	}
	return target_index;
}

static int v4l2_hdmi_manager(V4L2_HDMI_TYPE type, unsigned int sub_id, unsigned int* ui_ch)
{
	int target_index = newbase_hdmi_v4l2_function_table_index(type, sub_id);

	if((target_index <0) || (target_index> (sizeof(v4l2_define_table)/sizeof(HDMI_V4L2_DEFINE_T))))
	{
		rtd_printk(KERN_ERR, TAG_NAME,"[HDMI][V4L2][ERR] Invalid parameter, type=%d, sub_id=%d\n", type, sub_id);
		return -1;
	}

	v4l2_define_table[target_index].total_call_cnt ++;

	//Record to history
	if(v4l2_define_table[target_index].history_record == TRUE)
	{
		if(m_call_history_queue_top_index<(HDMI_V4L2_HISTORY_QUEUE_SIZE - 1))
		{
			m_v4l2_call_history_queue[m_call_history_queue_top_index].type = type;
			m_v4l2_call_history_queue[m_call_history_queue_top_index].sub_id = sub_id;
			m_v4l2_call_history_queue[m_call_history_queue_top_index].target_ui_ch= (ui_ch!=NULL) ? *ui_ch : V4L2_HDMI_MGR_NG;
			m_v4l2_call_history_queue[m_call_history_queue_top_index].current_call_cnt = v4l2_define_table[target_index].total_call_cnt;
			m_v4l2_call_history_queue[m_call_history_queue_top_index].last_call_ms = hdmi_get_system_time_ms();
			m_call_history_queue_top_index ++;
		}
		else
		{
			unsigned int j = 0;
			for(j=1; j<=(HDMI_V4L2_HISTORY_QUEUE_SIZE-1); j++)
			{
				m_v4l2_call_history_queue[j-1].type = m_v4l2_call_history_queue[j].type;
				m_v4l2_call_history_queue[j-1].sub_id = m_v4l2_call_history_queue[j].sub_id;
				m_v4l2_call_history_queue[j-1].target_ui_ch = m_v4l2_call_history_queue[j].target_ui_ch;
				m_v4l2_call_history_queue[j-1].current_call_cnt = m_v4l2_call_history_queue[j].current_call_cnt;
				m_v4l2_call_history_queue[j-1].last_call_ms = m_v4l2_call_history_queue[j].last_call_ms;
			}
			m_v4l2_call_history_queue[m_call_history_queue_top_index].type = type;
			m_v4l2_call_history_queue[m_call_history_queue_top_index].sub_id = sub_id;
			m_v4l2_call_history_queue[m_call_history_queue_top_index].target_ui_ch= (ui_ch!=NULL) ? *ui_ch : V4L2_HDMI_MGR_NG;
			m_v4l2_call_history_queue[m_call_history_queue_top_index].current_call_cnt = v4l2_define_table[target_index].total_call_cnt;
			m_v4l2_call_history_queue[m_call_history_queue_top_index].last_call_ms = hdmi_get_system_time_ms();
		}
	}

	//Exception Handler
	switch(type)
	{
		case V4L2_HDMI_S_INPUT:
		case V4L2_HDMI_S_CTRL:
		case V4L2_HDMI_S_EXT_CTRL:
		case V4L2_HDMI_G_EXT_CTRL:
		{
			#if FORCE_FIX_ERROR_UI_CH
			if((ui_ch != NULL) &&IS_VALID_CH(*ui_ch) == 0)
			{
				//ret = -1;
				if(v4l2_define_table[target_index].history_record == TRUE)
				{
					HDMI_EMG("[V4L2][ERR] type=%d, sub_id= %s, Invalid ch=%d, force set ch=%d\n",
						v4l2_define_table[target_index].type , v4l2_define_table[target_index].entry_name, *ui_ch, FORCE_FIX_ERROR_UI_CH);
				}
				*ui_ch = FORCE_FIX_ERROR_UI_CH;
			}
			#endif
		}
			break;
		default:
			break;
	}

	if(v4l2_define_table[target_index].execute_enable == FALSE)
		return -1;
	else
		return 0;
}

static int v4l2_vfe_hdmi_open(struct file *file)
{
	int result = -1; // -1:fail, 0: pass,
	HDMI_EMG("v4l2_vfe_open, open_cnt=%d, init_cnt=%d\n", m_multi_open.open_cnt, m_multi_open.init_cnt);
	if(v4l2_hdmi_manager(V4L2_HDMI_OPEN, NO_SUB_ID, NULL) ==-1)
	{
		return 0;
	}
	down(&HDMI_DetectSemaphore);

	if(m_multi_open.open_cnt == 0)
	{
		if (vfe_hdmi_drv_init() != 0)
		{
			rtd_printk(KERN_WARNING, TAG_NAME,"\r\n vfe_hdmi_drv_init failed");
			result = -1;
		}
		else
		{
			m_multi_open.init_cnt ++;
			HDMI_register_callback(TRUE, vfe_hdmi_drv_detect_mode);
			HDMI_set_detect_flag(TRUE);
			set_HDMI_Global_Status(SRC_OPEN_DONE);
			result = 0;
		}
	}
	else
	{
		HDMI_EMG("[v4l2_vfe_hdmi_open]  it has been initialized already fsm = %d\n", vfe_hdmi_drv_get_state());
		result = 0;
	}

	m_multi_open.open_cnt ++;
	up(&HDMI_DetectSemaphore);

	return result;
}


static int v4l2_vfe_hdmi_release(struct file *file)
{
	HDMI_EMG("v4l2_vfe_hdmi_release, open_cnt=%d, init_cnt=%d\n", m_multi_open.open_cnt, m_multi_open.init_cnt);

	if(v4l2_hdmi_manager(V4L2_HDMI_RELEASE, NO_SUB_ID, NULL) ==-1)
	{
		return 0;
	}

	down(&HDMI_DetectSemaphore);
	m_multi_open.open_cnt --;

	if(m_multi_open.open_cnt>0)
	{
		HDMI_EMG("[v4l2_vfe_hdmi_release] still other open cnt= %d\n", m_multi_open.open_cnt);
	}
	else
	{
		vfe_hdmi_drv_close();
		vfe_hdmi_drv_uninit();
		set_HDMI_Global_Status(SRC_NOTHING);
		m_multi_open.init_cnt --;
	}
	up(&HDMI_DetectSemaphore);

	return 0;
}

static ssize_t v4l2_vfe_hdmi_read(struct file *file, char __user *data, size_t count, loff_t *ppos)
{
	HDMI_EMG("v4l2_vfe_hdmi_read, state=%d\n", vfe_hdmi_drv_get_state());
	if(v4l2_hdmi_manager(V4L2_HDMI_READ, NO_SUB_ID, NULL) ==-1)
	{
		return 0;
	}

	return 0;
}

static unsigned int v4l2_vfe_hdmi_poll(struct file *file, poll_table *wait)
{
	HDMI_EMG("v4l2_vfe_hdmi_poll, state=%d\n", vfe_hdmi_drv_get_state());
	if(v4l2_hdmi_manager(V4L2_HDMI_POLL, NO_SUB_ID, NULL) ==-1)
	{
		return 0;
	}

	return 0;
}

static int v4l2_vfe_hdmi_mmap(struct file *file, struct vm_area_struct *vma)
{
	HDMI_EMG("v4l2_vfe_hdmi_mmap, state=%d\n", vfe_hdmi_drv_get_state());
	if(v4l2_hdmi_manager(V4L2_HDMI_MMAP, NO_SUB_ID, NULL) ==-1)
	{
		return 0;
	}

	return 0;
}

static int v4l2_vfe_hdmi_ioctl_s_ext_ctrls(struct file *file, void *fh, struct v4l2_ext_controls *ctrls)
{
	struct v4l2_ext_control   hdmi_v4l2_ext_control;
	unsigned char ret = EFAULT;

	//HDMI_EMG("[hdmi_v4l2_ext_control.id =%d\n", ctrls->controls->id);
	memcpy(&hdmi_v4l2_ext_control,ctrls->controls,sizeof(struct v4l2_ext_control));

	switch(hdmi_v4l2_ext_control.id)
	{
	case V4L2_CID_EXT_HDMI_EDID:
	{
		struct v4l2_ext_hdmi_edid hdmi_v4l2_ext_edid;

		if(!(copy_from_user(&hdmi_v4l2_ext_edid, to_user_ptr(hdmi_v4l2_ext_control.ptr), sizeof(struct v4l2_ext_hdmi_edid))))
		{
			vfe_hdmi_edid_read_write_ex edid_rw;
			unsigned char* p_edid =NULL;
			if(v4l2_hdmi_manager(V4L2_HDMI_S_EXT_CTRL, V4L2_CID_EXT_HDMI_EDID, &hdmi_v4l2_ext_edid.port) ==-1)
			{
				ret = 0;
				break;
			}

			if(hdmi_v4l2_ext_edid.size == V4L2_EXT_HDMI_EDID_SIZE_128)
				edid_rw.len =128;
			else if(hdmi_v4l2_ext_edid.size == V4L2_EXT_HDMI_EDID_SIZE_256)
				edid_rw.len =256;
			else if(hdmi_v4l2_ext_edid.size == V4L2_EXT_HDMI_EDID_SIZE_512)
				edid_rw.len =512;

			edid_rw.port =hdmi_v4l2_ext_edid.port-1;
			p_edid = (unsigned char*) kmalloc(edid_rw.len, GFP_KERNEL);
			if((p_edid == NULL) ||	((edid_rw.len != 256) && (edid_rw.len !=512)))
			{
				rtd_printk(KERN_ERR, TAG_NAME,"[ERR] read edid to HDMI-%d failed, invalid size - %d\n", edid_rw.port,  edid_rw.len);
				ret = -EFAULT;
			}
			else if(!(copy_from_user(p_edid, to_user_ptr(hdmi_v4l2_ext_edid.pData), sizeof(unsigned char)*(edid_rw.len))))
			{
				ret =0;
				if (vfe_hdmi_drv_write_edid(edid_rw.port, p_edid, edid_rw.len ,APPLY_EDID_IMMEDIATELY)!=0)
				{
					rtd_printk(KERN_ERR, TAG_NAME,"[ERR] read edid to HDMI-%d failed, copy to user space edid data failed\n", edid_rw.port);
					ret = -EFAULT;
				}
			}

			if(p_edid)
				kfree(p_edid);

		}
		break;
	}

	case V4L2_CID_EXT_HDMI_HPD:
	{
		struct v4l2_ext_hdmi_hpd hdmi_v4l2_ext_hpd;

		if(!(copy_from_user(&hdmi_v4l2_ext_hpd, to_user_ptr(hdmi_v4l2_ext_control.ptr), sizeof(struct v4l2_ext_hdmi_hpd))))
		{
			vfe_hdmi_hpd_t hpd_control;
			if(v4l2_hdmi_manager(V4L2_HDMI_S_EXT_CTRL, V4L2_CID_EXT_HDMI_HPD, &hdmi_v4l2_ext_hpd.port) ==-1)
			{
				ret = 0;
				break;
			}

			if (IS_VALID_CH(hdmi_v4l2_ext_hpd.port))
			{
				hpd_control.port= hdmi_v4l2_ext_hpd.port-1;
				ret = vfe_hdmi_v4l2_set_hpd_flag_port(hpd_control.port,(unsigned char)hdmi_v4l2_ext_hpd.hpd_state);
			}
			else
			{
				ret = -1;
				rtd_printk(KERN_ERR, TAG_NAME,"[HDMI][V4L2][ERR] Set V4L2_CID_EXT_HDMI_HPD, Invalid ch=%d \n", hdmi_v4l2_ext_hpd.port);
			}
		}
		break;
	}

	case V4L2_CID_EXT_HDMI_HDCP_KEY:
	{
		struct v4l2_ext_hdmi_hdcp_key hdmi_v4l2_ext_hdcp;

		if(!(copy_from_user(&hdmi_v4l2_ext_hdcp, to_user_ptr(hdmi_v4l2_ext_control.ptr), sizeof(struct v4l2_ext_hdmi_hdcp_key))))
		{
			char *p_hdcp1P4_key =NULL;
			char *p_hdcp2P2_key =NULL;

			ret = 0;
			if(v4l2_hdmi_manager(V4L2_HDMI_S_EXT_CTRL, V4L2_CID_EXT_HDMI_HDCP_KEY, NULL) ==-1)
			{
				ret = 0;
				break;
			}

			if(hdmi_v4l2_ext_hdcp.version == V4L2_EXT_HDMI_HDCP_VERSION_14)
			{
				p_hdcp1P4_key = kmalloc(HDCP14_KEY_SIZE, GFP_KERNEL);
				memset(p_hdcp1P4_key, 0, HDCP14_KEY_SIZE);
				if(p_hdcp1P4_key == NULL )
				{
					rtd_printk(KERN_ERR, TAG_NAME,"[ERR] read hdcp to HDMI failed, invalid size \n");
					ret = -EFAULT;
				}
				else if(!(copy_from_user(p_hdcp1P4_key, to_user_ptr(hdmi_v4l2_ext_hdcp.pData), sizeof(unsigned char)*(HDCP14_KEY_SIZE))))
				{
					if (vfe_hdmi_drv_write_hdcp(p_hdcp1P4_key, (p_hdcp1P4_key+5)) != 0){
						rtd_printk(KERN_ERR, TAG_NAME,"[ERR] read hdcp to HDMI failed, copy to user space edid data failed\n");
						ret = -EFAULT;
					}
				}
				if(p_hdcp1P4_key)
					kfree(p_hdcp1P4_key);
			}
			else if(hdmi_v4l2_ext_hdcp.version == V4L2_EXT_HDMI_HDCP_VERSION_22)
			{
				p_hdcp2P2_key = kmalloc(HDCP22_KEY_SIZE, GFP_KERNEL);
				memset(p_hdcp2P2_key, 0, HDCP22_KEY_SIZE);
				if(p_hdcp2P2_key == NULL )
				{
					rtd_printk(KERN_ERR, TAG_NAME,"[ERR] read hdcp to HDMI failed, invalid size \n");
					ret = -EFAULT;
				}
				else if(!(copy_from_user(p_hdcp2P2_key, to_user_ptr(hdmi_v4l2_ext_hdcp.pData), sizeof(unsigned char)*(HDCP22_KEY_SIZE))))
				{
					#ifdef CONFIG_OPTEE_HDCP2
					rtk_hal_hdcp2_VFE_HDMI_WriteHDCP22(p_hdcp2P2_key, HDCP22_KEY_SIZE);
					#else
					spu_SetHdmiKey(p_hdcp2P2_key);
					#endif
				}
				if(p_hdcp2P2_key)
					kfree(p_hdcp2P2_key);
			}
		}
		break;
	}


	case V4L2_CID_EXT_HDMI_EXPERT_SETTING:
	{
		struct v4l2_ext_hdmi_expert_setting hdmi_v4l2_ext_expert_setting;

		if(!(copy_from_user(&hdmi_v4l2_ext_expert_setting, to_user_ptr(hdmi_v4l2_ext_control.ptr), sizeof(struct v4l2_ext_hdmi_expert_setting))))
		{
			if(v4l2_hdmi_manager(V4L2_HDMI_S_EXT_CTRL, V4L2_CID_EXT_HDMI_EXPERT_SETTING, &hdmi_v4l2_ext_expert_setting.port) ==-1)
			{
				ret = 0;
				break;
			}

			ret = vfe_hdmi_v4l2_set_expert_setting(&hdmi_v4l2_ext_expert_setting);
		}
		break;
	}
	default:
		rtd_printk(KERN_ERR, TAG_NAME,"[ERR][v4l2_vfe_hdmi_ioctl_s_ext_ctrls] Invalid ID=%d\n", hdmi_v4l2_ext_control.id);
		break;
	}

	return ret;
}


int v4l2_vfe_hdmi_ext_get_timing_info(struct v4l2_ext_hdmi_timing_info* p_timing_info)
{
    vfe_hdmi_timing_info_t vfe_timing;
    vfe_timing.port = p_timing_info->port-1;

    memset(p_timing_info, 0 , sizeof(*p_timing_info));
    p_timing_info->port = vfe_timing.port+1;
    if (vfe_hdmi_drv_get_port_timing_info(&vfe_timing) != 0) {
        return -1;
    }

    p_timing_info->h_freq      = vfe_timing.h_freq;
    p_timing_info->v_vreq      = vfe_timing.v_freq;
    p_timing_info->h_porch     = vfe_timing.h_porch;
    p_timing_info->v_porch     = vfe_timing.v_porch;
    p_timing_info->scan_type   = vfe_timing.scan_type;
    p_timing_info->h_total     = vfe_timing.h_total;
    p_timing_info->v_total     = vfe_timing.v_total;
    p_timing_info->active.h    = vfe_timing.active.h;
    p_timing_info->active.w    = vfe_timing.active.w;
    p_timing_info->active.x    = vfe_timing.active.x;
    p_timing_info->active.y    = vfe_timing.active.y;
    p_timing_info->dvi_hdmi    = vfe_timing.dvi_hdmi;
    p_timing_info->color_depth = _to_v4l2_color_depth(vfe_timing.color_depth);
    p_timing_info->allm_mode   = vfe_timing.isALLM;

    if (vfe_timing.scan_type==0) // double the hight if this is interlace timing
    {
        p_timing_info->h_freq   <<= 1;
        p_timing_info->v_porch  <<= 1;
        p_timing_info->v_total  <<= 1;
        p_timing_info->active.h <<= 1;
    }

    return 0;
}

int v4l2_vfe_hdmi_ext_get_drm_info(struct v4l2_ext_hdmi_drm_info* p_drm_info)
{
    vfe_hdmi_drm_t drm_info;
    unsigned char ch = p_drm_info->port -1;

	if (hdmi_vfe_is_valid_channel(ch)<0 || vfe_hdmi_drv_get_drm_info(&drm_info) != 0)
	{
	    memset(p_drm_info, 0, sizeof(*p_drm_info));
	    p_drm_info->port = ch+1;
		return -1;
	}

	p_drm_info->version = drm_info.ver;
	p_drm_info->length = drm_info.len;
	p_drm_info->eotf_type = drm_info.eEOTFtype;
	p_drm_info->meta_desc = drm_info.eMeta_Desc;
	p_drm_info->display_primaries_x0 = drm_info.display_primaries_x0;
	p_drm_info->display_primaries_y0 = drm_info.display_primaries_y0;
	p_drm_info->display_primaries_x1 = drm_info.display_primaries_x1;
	p_drm_info->display_primaries_y1 = drm_info.display_primaries_y1;
	p_drm_info->display_primaries_x2 = drm_info.display_primaries_x2;
	p_drm_info->display_primaries_y2 = drm_info.display_primaries_y2;
	p_drm_info->white_point_x = drm_info.white_point_x;
	p_drm_info->white_point_y = drm_info.white_point_y;
	p_drm_info->max_display_mastering_luminance = drm_info.max_display_mastering_luminance;
	p_drm_info->min_display_mastering_luminance = drm_info.min_display_mastering_luminance;
	p_drm_info->maximum_content_light_level = drm_info.maximum_content_light_level;
	p_drm_info->maximum_frame_average_light_level = drm_info.maximum_frame_average_light_level;

    return 0;
}

int v4l2_vfe_hdmi_ext_get_vsi_info(struct v4l2_ext_hdmi_vsi_info* p_vsi_info)
{
	vfe_hdmi_vsi_t vsi_info;
	vsi_info.port= p_vsi_info->port-1;

	if (vfe_hdmi_drv_get_port_vsi_info(&vsi_info) != 0)
	{
	    memset(p_vsi_info, 0, sizeof(*p_vsi_info));
		p_vsi_info->port = vsi_info.port +1;
		return -1;
	}

	p_vsi_info->video_format         = vsi_info.vidoe_fmt;
	p_vsi_info->st_3d                = vsi_info.struct_3d;
	p_vsi_info->ext_data_3d          = vsi_info.extdata_3d;
	p_vsi_info->vic                  = vsi_info.vic;

	memcpy(p_vsi_info->regid, vsi_info.ieee_reg_id, V4L2_HDMI_VENDOR_SPECIFIC_REGID_LEN);
	memcpy(p_vsi_info->payload, vsi_info.payload, V4L2_HDMI_VENDOR_SPECIFIC_PAYLOAD_LEN);

	p_vsi_info->packet.type          = vsi_info.header.type;
	p_vsi_info->packet.version       = vsi_info.header.version;
	p_vsi_info->packet.length        = vsi_info.header.length;
	p_vsi_info->packet.data_bytes[0] = vsi_info.checksum;

	memcpy(p_vsi_info->packet.data_bytes+1,vsi_info.ieee_reg_id,V4L2_HDMI_VENDOR_SPECIFIC_REGID_LEN);
	memcpy(p_vsi_info->packet.data_bytes+1+V4L2_HDMI_VENDOR_SPECIFIC_REGID_LEN,vsi_info.payload,V4L2_HDMI_VENDOR_SPECIFIC_PAYLOAD_LEN-1);

	p_vsi_info->packet_status        = V4L2_EXT_HDMI_PACKET_STATUS_UPDATED;

    return 0;
}

int v4l2_vfe_hdmi_ext_get_spd_info(struct v4l2_ext_hdmi_spd_info* p_spd_info)
{
	vfe_hdmi_spd_t spd_info;
	spd_info.port= p_spd_info->port -1;

	memset(p_spd_info, 0, sizeof(*p_spd_info));
	p_spd_info->port = spd_info.port +1;

	if (vfe_hdmi_drv_get_port_spd_info(&spd_info) != 0)
		return -1;

	p_spd_info->source_device_info = spd_info.src_dev_info;
	memcpy(p_spd_info->vendor_name, spd_info.vendor_name, V4L2_HDMI_SPD_IF_VENDOR_LEN);

	p_spd_info->vendor_name[V4L2_HDMI_SPD_IF_VENDOR_LEN]='\0';
	memcpy(p_spd_info->product_description, spd_info.product_desc, V4L2_HDMI_SPD_IF_DESC_LEN);

	p_spd_info->product_description[V4L2_HDMI_SPD_IF_DESC_LEN]='\0';
	p_spd_info->source_device_info = spd_info.src_dev_info;

	memcpy(p_spd_info->packet.data_bytes,spd_info.vendor_name,V4L2_HDMI_SPD_IF_VENDOR_LEN);
	//memcpy(p_spd_info->packet.data_bytes+V4L2_HDMI_SPD_IF_VENDOR_LEN,vsi_info.payload,V4L2_HDMI_PACKET_DATA_LENGTH-V4L2_HDMI_SPD_IF_VENDOR_LEN-1);

	p_spd_info->packet.type     = spd_info.header.type;
	p_spd_info->packet.version  = spd_info.header.version;
	p_spd_info->packet.length   = spd_info.header.length;
	p_spd_info->packet_status   = V4L2_EXT_HDMI_PACKET_STATUS_UPDATED;

	return 0;
}

int v4l2_vfe_hdmi_ext_get_avi_info(struct v4l2_ext_hdmi_avi_info* p_avi_info)
{
    vfe_hdmi_avi_t avi_info;
    avi_info.port = p_avi_info->port-1;

    memset(p_avi_info, 0, sizeof(*p_avi_info));
    p_avi_info->port = avi_info.port +1;

	if (vfe_hdmi_drv_get_port_avi_info(&avi_info) != 0)
        return -1;

	p_avi_info->mode                         = avi_info.mode;
	p_avi_info->pixel_encoding               = avi_info.pixel_encoding;
	p_avi_info->active_info                  = avi_info.active_info;
	p_avi_info->bar_info                     = avi_info.bar_info;
	p_avi_info->scan_info                    = avi_info.scan_info;
	p_avi_info->colorimetry                  = avi_info.colorimetry;
	p_avi_info->picture_aspect_ratio         = avi_info.picture_aspect_ratio;
	p_avi_info->active_format_aspect_ratio   = avi_info.active_format_aspect_ratio;
	p_avi_info->scaling                      = avi_info.scaling;
	p_avi_info->vic                          = avi_info.video_id_code;
	p_avi_info->pixel_repeat                 = avi_info.pixel_repeat;
	p_avi_info->it_content                   = avi_info.it_content;
	p_avi_info->extended_colorimetry         = avi_info.extended_colorimetry;
	p_avi_info->rgb_quantization_range       = avi_info.rgb_quantization_range;
	p_avi_info->ycc_quantization_range       = avi_info.ycc_quantization_range;
	p_avi_info->content_type                 = avi_info.content_type;
	p_avi_info->top_bar_end_line_number      = avi_info.TopBarEndLineNumber;
	p_avi_info->bottom_bar_start_line_number = avi_info.BottomBarStartLineNumber;
	p_avi_info->left_bar_end_pixel_number    = avi_info.LeftBarEndPixelNumber;
	p_avi_info->right_bar_end_pixel_number   = avi_info.RightBarEndPixelNumber;
	p_avi_info->packet.type                  = avi_info.header.type;
	p_avi_info->packet.version               = avi_info.header.version;
	p_avi_info->packet.length                = avi_info.header.length;
	p_avi_info->packet_status                = V4L2_EXT_HDMI_PACKET_STATUS_UPDATED;

    return 0;
}

int v4l2_vfe_hdmi_ext_get_vrr_frequency(struct v4l2_ext_hdmi_vrr_frequency* p_vrr_freq)
{
	unsigned short vrr_freq = 0;
	unsigned char ch = p_vrr_freq->port-1;

	if (hdmi_vfe_is_valid_channel(ch)<0 || vfe_hdmi_drv_get_currentVRRVFrequency(ch, &vrr_freq) != 0) 
		return -1;

	p_vrr_freq->frequency=vrr_freq;
    return 0;
}

static bool quirk_check_dolby_standard_packet(void)
{
	int ret;

	if (get_HDMI_HDR_mode() ==  HDR_DOLBY_HDMI)
		ret = true;
	else
		ret = (rtd_inl(HDMI_P0_MDD_SR_reg) & _BIT0) ? true : false;

	return ret;
}

static int v4l2_vfe_hdmi_ioctl_g_ext_ctrls(struct file *file, void *fh, struct v4l2_ext_controls *ctrls)
{
	struct v4l2_ext_control  hdmi_v4l2_ext_control;
	unsigned char ret = EFAULT;
/*
    if(copy_from_user((void *)&hdmi_v4l2_ext_control, ctrls->controls, sizeof(struct v4l2_ext_control)))
	{
		HDMI_EMG("[ERR] copy_from_user error1\n");
		ret = EFAULT;
	}
	else
*/
	memcpy(&hdmi_v4l2_ext_control,ctrls->controls,sizeof(struct v4l2_ext_control));

	switch (hdmi_v4l2_ext_control.id)
	{
	case V4L2_CID_EXT_HDMI_TIMING_INFO:
	{
		struct v4l2_ext_hdmi_timing_info hdmi_v4l2_ext_timing;

		if((copy_from_user(&hdmi_v4l2_ext_timing, to_user_ptr(hdmi_v4l2_ext_control.ptr), sizeof(struct v4l2_ext_hdmi_timing_info))) == 0)
		{
			unsigned int report_zero = TRUE;
			if(v4l2_hdmi_manager(V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_TIMING_INFO, &hdmi_v4l2_ext_timing.port) ==-1)
			{
				ret = 0;
				break;
			}

			if (hdmi_v4l2_ext_timing.port == (connect_port+1))
			{//main port
				down(&HDMI_DetectSemaphore);

				if (HDMI_Reply_Zero_Timing & REPORT_ZERO_TIMING)
				{
					Set_Reply_Zero_Timing_Flag(VSC_INPUTSRC_HDMI, CLEAR_ZERO_FLAG);
					HDMI_V4L2_Reply_Zero_Timing_mute_cnt = DIRECT_REPORT_ZERO;
					pr_info("### V4L2_CID_EXT_HDMI_TIMING_INFO report 0 cnt:%d (proc_name=%s)###\r\n", HDMI_V4L2_Reply_Zero_Timing_mute_cnt, current->comm);
				}
				else if((HDMI_V4L2_Reply_Zero_Timing_mute_cnt == DIRECT_REPORT_ZERO) || (HDMI_V4L2_Reply_Zero_Timing_mute_cnt == CHECK_TIME_PERIOD))
				{
					HDMI_V4L2_Reply_Zero_Timing_mute_cnt --;			
					Set_Reply_Zero_Timing_Flag(VSC_INPUTSRC_HDMI, CLEAR_ZERO_FLAG);
					pr_info("r### V4L2_CID_EXT_HDMI_TIMING_INFO report 0 cnt:%d  (proc_name=%s)###\r\n", HDMI_V4L2_Reply_Zero_Timing_mute_cnt, current->comm);
				}
				else
				{
					//pr_err("get timing AAA = %d / %d (HDMI_Reply_Zero_Timing=%08x)\n", hdmi_v4l2_ext_timing.port, connect_port, HDMI_Reply_Zero_Timing);
					ret = v4l2_vfe_hdmi_ext_get_timing_info(&hdmi_v4l2_ext_timing);
					report_zero = FALSE;
					if (ret == 0)
					{
						/*
						pr_err("get timing  = %d / %d (htotal=%d, vtotal=%d)\n", 
							hdmi_v4l2_ext_timing.port, connect_port, 
							hdmi_v4l2_ext_timing.h_total, hdmi_v4l2_ext_timing.v_total);

						if (hdmi_v4l2_ext_timing.h_total==0 && hdmi_v4l2_ext_timing.v_total==0)
						{
							pr_err("vfe_hdmi report zero timing, setup report zero timing flag\n");
						}
						*/
					}
				}
				up(&HDMI_DetectSemaphore);
				if (report_zero)
				{
					unsigned char port = hdmi_v4l2_ext_timing.port;
					memset(&hdmi_v4l2_ext_timing, 0 , sizeof(hdmi_v4l2_ext_timing));
					hdmi_v4l2_ext_timing.port = port;
					//pr_err("reply zero timing = %d / %d\n", hdmi_v4l2_ext_timing.port, connect_port);
					ret = 0;
				}
			}
			else
			{//not main port case
				ret = v4l2_vfe_hdmi_ext_get_timing_info(&hdmi_v4l2_ext_timing);
			}

			if (copy_to_user(to_user_ptr(hdmi_v4l2_ext_control.ptr), &hdmi_v4l2_ext_timing, sizeof(struct v4l2_ext_hdmi_timing_info)) )
			{
				rtd_printk(KERN_DEBUG, TAG_NAME,"[ERR] copy_to_user error\n");
				ret = EFAULT;
			}
		}
		break;
	}

	case V4L2_CID_EXT_HDMI_DRM_INFO:
	{
		struct v4l2_ext_hdmi_drm_info hdmi_v4l2_ext_drm;

		if(!(copy_from_user(&hdmi_v4l2_ext_drm, to_user_ptr(hdmi_v4l2_ext_control.ptr), sizeof(struct v4l2_ext_hdmi_drm_info))))
		{
			if(v4l2_hdmi_manager(V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_DRM_INFO, &hdmi_v4l2_ext_drm.port) ==-1)
			{
				ret = 0;
				break;
			}

		    ret = v4l2_vfe_hdmi_ext_get_drm_info(&hdmi_v4l2_ext_drm);

			if (copy_to_user(to_user_ptr(hdmi_v4l2_ext_control.ptr), &hdmi_v4l2_ext_drm, sizeof(struct v4l2_ext_hdmi_drm_info)))
			{
				rtd_printk(KERN_DEBUG, TAG_NAME,"[ERR] copy_to_user error\n");
				ret = EFAULT;
			}
		}
		break;
	}

	case V4L2_CID_EXT_HDMI_VSI_INFO:
	{
		struct v4l2_ext_hdmi_vsi_info hdmi_v4l2_ext_vsi;

		if(!(copy_from_user(&hdmi_v4l2_ext_vsi, to_user_ptr(hdmi_v4l2_ext_control.ptr), sizeof(struct v4l2_ext_hdmi_vsi_info))))
		{
			if(v4l2_hdmi_manager(V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_VSI_INFO, &hdmi_v4l2_ext_vsi.port) ==-1)
			{
				ret = 0;
				break;
			}

			ret = v4l2_vfe_hdmi_ext_get_vsi_info(&hdmi_v4l2_ext_vsi);

			if (copy_to_user(to_user_ptr(hdmi_v4l2_ext_control.ptr), &hdmi_v4l2_ext_vsi, sizeof(struct v4l2_ext_hdmi_vsi_info)))
			{
				rtd_printk(KERN_DEBUG, TAG_NAME,"[ERR] copy_to_user error\n");
				ret = EFAULT;
			}
		}
		break;
	}

	case V4L2_CID_EXT_HDMI_SPD_INFO:
	{
		struct v4l2_ext_hdmi_spd_info hdmi_v4l2_ext_spd;

		if(!(copy_from_user(&hdmi_v4l2_ext_spd, to_user_ptr(hdmi_v4l2_ext_control.ptr), sizeof(struct v4l2_ext_hdmi_spd_info))))
		{
			if(v4l2_hdmi_manager(V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_SPD_INFO, &hdmi_v4l2_ext_spd.port)==-1)
			{
				ret = 0;
				break;
			}

			ret = v4l2_vfe_hdmi_ext_get_spd_info(&hdmi_v4l2_ext_spd);

			if (copy_to_user(to_user_ptr(hdmi_v4l2_ext_control.ptr), &hdmi_v4l2_ext_spd, sizeof(struct v4l2_ext_hdmi_spd_info)))
			{
				rtd_printk(KERN_DEBUG, TAG_NAME,"[ERR] copy_to_user error\n");
				ret = EFAULT;
			}
		}
		break;
	}

	case V4L2_CID_EXT_HDMI_AVI_INFO:
	{
		struct v4l2_ext_hdmi_avi_info hdmi_v4l2_ext_avi;

		if(!(copy_from_user(&hdmi_v4l2_ext_avi, to_user_ptr(hdmi_v4l2_ext_control.ptr), sizeof(struct v4l2_ext_hdmi_avi_info))))
		{
			vfe_hdmi_avi_t avi_info;
			if(v4l2_hdmi_manager(V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_AVI_INFO, &hdmi_v4l2_ext_avi.port)==-1)
			{
				ret = 0;
				break;
			}

			avi_info.port= hdmi_v4l2_ext_avi.port-1;
			if (vfe_hdmi_drv_get_port_avi_info(&avi_info) != 0)
			{
				hdmi_v4l2_ext_avi.port= avi_info.port +1;
				ret = -1;
			}
			else
			{
				hdmi_v4l2_ext_avi.mode = avi_info.mode;
				hdmi_v4l2_ext_avi.pixel_encoding = avi_info.pixel_encoding;
				hdmi_v4l2_ext_avi.active_info = avi_info.active_info;
				hdmi_v4l2_ext_avi.bar_info = avi_info.bar_info;
				hdmi_v4l2_ext_avi.scan_info = avi_info.scan_info;
				hdmi_v4l2_ext_avi.colorimetry = avi_info.colorimetry;
				hdmi_v4l2_ext_avi.picture_aspect_ratio = avi_info.picture_aspect_ratio;
				hdmi_v4l2_ext_avi.active_format_aspect_ratio = avi_info.active_format_aspect_ratio;
				hdmi_v4l2_ext_avi.scaling = avi_info.scaling;
				hdmi_v4l2_ext_avi.vic = avi_info.video_id_code;
				hdmi_v4l2_ext_avi.pixel_repeat = avi_info.pixel_repeat;
				hdmi_v4l2_ext_avi.it_content = avi_info.it_content;
				hdmi_v4l2_ext_avi.extended_colorimetry = avi_info.extended_colorimetry;
				hdmi_v4l2_ext_avi.rgb_quantization_range = avi_info.rgb_quantization_range;
				hdmi_v4l2_ext_avi.ycc_quantization_range = avi_info.ycc_quantization_range;
				hdmi_v4l2_ext_avi.content_type = avi_info.content_type;
				hdmi_v4l2_ext_avi.top_bar_end_line_number = avi_info.TopBarEndLineNumber;
				hdmi_v4l2_ext_avi.bottom_bar_start_line_number = avi_info.BottomBarStartLineNumber;
				hdmi_v4l2_ext_avi.left_bar_end_pixel_number = avi_info.LeftBarEndPixelNumber;
				hdmi_v4l2_ext_avi.right_bar_end_pixel_number = avi_info.RightBarEndPixelNumber;
				hdmi_v4l2_ext_avi.packet.type = avi_info.header.type;
				hdmi_v4l2_ext_avi.packet.version = avi_info.header.version;
				hdmi_v4l2_ext_avi.packet.length = avi_info.header.length;
				hdmi_v4l2_ext_avi.packet_status = V4L2_EXT_HDMI_PACKET_STATUS_UPDATED;
				ret =0;

			}
			if (copy_to_user(to_user_ptr(hdmi_v4l2_ext_control.ptr), &hdmi_v4l2_ext_avi, sizeof(struct v4l2_ext_hdmi_avi_info)))
			{
				rtd_printk(KERN_DEBUG, TAG_NAME,"[ERR] copy_to_user error\n");
				ret = EFAULT;
			}
		}
		break;
	}

	case V4L2_CID_EXT_HDMI_PACKET_INFO:
	{
		struct v4l2_ext_hdmi_packet_info hdmi_v4l2_ext_packet;

		if(!(copy_from_user(&hdmi_v4l2_ext_packet, to_user_ptr(hdmi_v4l2_ext_control.ptr), sizeof(struct v4l2_ext_hdmi_packet_info))))
		{
			vfe_hdmi_avi_t avi_info;
			vfe_hdmi_spd_t spd_info;
			vfe_hdmi_vsi_t vsi_info;
			if(v4l2_hdmi_manager(V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_PACKET_INFO, &hdmi_v4l2_ext_packet.port)==-1)
			{
				ret = 0;
				break;
			}

			vsi_info.port = hdmi_v4l2_ext_packet.port-1;
			spd_info.port = hdmi_v4l2_ext_packet.port-1;
			avi_info.port = hdmi_v4l2_ext_packet.port-1;
			if (vfe_hdmi_drv_get_port_vsi_info(&vsi_info) != 0 || vfe_hdmi_drv_get_port_spd_info(&spd_info) != 0 || vfe_hdmi_drv_get_port_avi_info(&avi_info) != 0)
			{
				ret = -1;
			}
			else
			{
				hdmi_v4l2_ext_packet.mode = avi_info.mode;
				hdmi_v4l2_ext_packet.avi.mode = avi_info.mode;
				hdmi_v4l2_ext_packet.avi.pixel_encoding = avi_info.pixel_encoding;
				hdmi_v4l2_ext_packet.avi.active_info = avi_info.active_info;
				hdmi_v4l2_ext_packet.avi.bar_info = avi_info.bar_info;
				hdmi_v4l2_ext_packet.avi.scan_info = avi_info.scan_info;
				hdmi_v4l2_ext_packet.avi.colorimetry = avi_info.colorimetry;
				hdmi_v4l2_ext_packet.avi.picture_aspect_ratio = avi_info.picture_aspect_ratio;
				hdmi_v4l2_ext_packet.avi.active_format_aspect_ratio = avi_info.active_format_aspect_ratio;
				hdmi_v4l2_ext_packet.avi.scaling = avi_info.scaling;
				hdmi_v4l2_ext_packet.avi.vic = avi_info.video_id_code;
				hdmi_v4l2_ext_packet.avi.pixel_repeat = avi_info.pixel_repeat;
				hdmi_v4l2_ext_packet.avi.it_content = avi_info.it_content;
				hdmi_v4l2_ext_packet.avi.extended_colorimetry = avi_info.extended_colorimetry;
				hdmi_v4l2_ext_packet.avi.rgb_quantization_range = avi_info.rgb_quantization_range;
				hdmi_v4l2_ext_packet.avi.ycc_quantization_range = avi_info.ycc_quantization_range;
				hdmi_v4l2_ext_packet.avi.content_type = avi_info.content_type;
				hdmi_v4l2_ext_packet.avi.top_bar_end_line_number = avi_info.TopBarEndLineNumber;
				hdmi_v4l2_ext_packet.avi.bottom_bar_start_line_number = avi_info.BottomBarStartLineNumber;
				hdmi_v4l2_ext_packet.avi.left_bar_end_pixel_number = avi_info.LeftBarEndPixelNumber;
				hdmi_v4l2_ext_packet.avi.right_bar_end_pixel_number = avi_info.RightBarEndPixelNumber;
				hdmi_v4l2_ext_packet.avi.packet.type = avi_info.header.type;
				hdmi_v4l2_ext_packet.avi.packet.version = avi_info.header.version;
				hdmi_v4l2_ext_packet.avi.packet.length = avi_info.header.length;
				hdmi_v4l2_ext_packet.avi.packet_status = V4L2_EXT_HDMI_PACKET_STATUS_UPDATED;

				hdmi_v4l2_ext_packet.spd.source_device_info = spd_info.src_dev_info;
				memcpy(hdmi_v4l2_ext_packet.spd.vendor_name, spd_info.vendor_name, V4L2_HDMI_SPD_IF_VENDOR_LEN);
				hdmi_v4l2_ext_packet.spd.vendor_name[V4L2_HDMI_SPD_IF_VENDOR_LEN]='\0';
				memcpy(hdmi_v4l2_ext_packet.spd.product_description, spd_info.product_desc, V4L2_HDMI_SPD_IF_DESC_LEN);
				hdmi_v4l2_ext_packet.spd.product_description[V4L2_HDMI_SPD_IF_DESC_LEN]='\0';
				hdmi_v4l2_ext_packet.spd.source_device_info = spd_info.src_dev_info;
				memcpy(hdmi_v4l2_ext_packet.spd.packet.data_bytes,spd_info.vendor_name,V4L2_HDMI_SPD_IF_VENDOR_LEN);
				memcpy(hdmi_v4l2_ext_packet.spd.packet.data_bytes+V4L2_HDMI_SPD_IF_VENDOR_LEN,vsi_info.payload,V4L2_HDMI_PACKET_DATA_LENGTH-V4L2_HDMI_SPD_IF_VENDOR_LEN-1);
				hdmi_v4l2_ext_packet.spd.packet.type = spd_info.header.type;
				hdmi_v4l2_ext_packet.spd.packet.version = spd_info.header.version;
				hdmi_v4l2_ext_packet.spd.packet.length = spd_info.header.length;
				hdmi_v4l2_ext_packet.spd.packet_status   = V4L2_EXT_HDMI_PACKET_STATUS_UPDATED;
				hdmi_v4l2_ext_packet.vsi.video_format = vsi_info.vidoe_fmt;
				hdmi_v4l2_ext_packet.vsi.st_3d = vsi_info.struct_3d;
				hdmi_v4l2_ext_packet.vsi.ext_data_3d =  vsi_info.extdata_3d;
				hdmi_v4l2_ext_packet.vsi.vic = vsi_info.vic;
				memcpy(hdmi_v4l2_ext_packet.vsi.regid, vsi_info.ieee_reg_id, V4L2_HDMI_VENDOR_SPECIFIC_REGID_LEN);
				memcpy(hdmi_v4l2_ext_packet.vsi.payload, vsi_info.payload, V4L2_HDMI_VENDOR_SPECIFIC_PAYLOAD_LEN);
				hdmi_v4l2_ext_packet.vsi.packet.type = vsi_info.header.type;
				hdmi_v4l2_ext_packet.vsi.packet.version = vsi_info.header.version;
				hdmi_v4l2_ext_packet.vsi.packet.length = vsi_info.header.length;
				hdmi_v4l2_ext_packet.vsi.packet.data_bytes[0]=vsi_info.checksum;
				memcpy(hdmi_v4l2_ext_packet.vsi.packet.data_bytes+1,vsi_info.ieee_reg_id,V4L2_HDMI_VENDOR_SPECIFIC_REGID_LEN);
				memcpy(hdmi_v4l2_ext_packet.vsi.packet.data_bytes+1+V4L2_HDMI_VENDOR_SPECIFIC_REGID_LEN,vsi_info.payload,V4L2_HDMI_VENDOR_SPECIFIC_PAYLOAD_LEN-1);
				hdmi_v4l2_ext_packet.vsi.packet_status	 = V4L2_EXT_HDMI_PACKET_STATUS_UPDATED;
				ret =0;
			}

			if (copy_to_user(to_user_ptr(hdmi_v4l2_ext_control.ptr), &hdmi_v4l2_ext_packet, sizeof(struct v4l2_ext_hdmi_packet_info)))
			{
				rtd_printk(KERN_DEBUG, TAG_NAME,"[ERR] copy_to_user error\n");
				ret = EFAULT;
			}

		}
		break;
	}

	case V4L2_CID_EXT_HDMI_EDID:
	{
		struct v4l2_ext_hdmi_edid hdmi_v4l2_ext_edid;

		if(!(copy_from_user(&hdmi_v4l2_ext_edid, to_user_ptr(hdmi_v4l2_ext_control.ptr), sizeof(struct v4l2_ext_hdmi_edid))))
		{
			vfe_hdmi_edid_read_write_ex edid_rw;
			unsigned char* p_edid =NULL;
			if(v4l2_hdmi_manager(V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_EDID, &hdmi_v4l2_ext_edid.port)==-1)
			{
				ret = 0;
				break;
			}

			if(hdmi_v4l2_ext_edid.size == V4L2_EXT_HDMI_EDID_SIZE_128)
				edid_rw.len =128;
			else if(hdmi_v4l2_ext_edid.size == V4L2_EXT_HDMI_EDID_SIZE_256)
				edid_rw.len =256;
			else if(hdmi_v4l2_ext_edid.size == V4L2_EXT_HDMI_EDID_SIZE_512)
				edid_rw.len =512;
			edid_rw.port =hdmi_v4l2_ext_edid.port-1;
			p_edid = (unsigned char*) kmalloc(edid_rw.len, GFP_KERNEL);
			if((p_edid == NULL) ||	((edid_rw.len != 256) && (edid_rw.len !=512)))
			{
				rtd_printk(KERN_ERR, TAG_NAME,"[ERR] read edid to HDMI-%d failed, invalid size - %d\n", edid_rw.port,  edid_rw.len);
				ret = -EFAULT;
			}
			else if(!(copy_from_user(p_edid, to_user_ptr(hdmi_v4l2_ext_edid.pData), sizeof(unsigned char)*(edid_rw.len))))
			{
				if (vfe_hdmi_drv_read_edid(edid_rw.port, p_edid, edid_rw.len)!=0 ||
					copy_to_user(to_user_ptr(hdmi_v4l2_ext_edid.pData), p_edid, sizeof(unsigned char)*(edid_rw.len))!=0 ||
					copy_to_user(to_user_ptr(hdmi_v4l2_ext_control.ptr), &hdmi_v4l2_ext_edid, sizeof(struct v4l2_ext_hdmi_edid))!=0)

				{
					rtd_printk(KERN_ERR, TAG_NAME,"[ERR] read edid to HDMI-%d failed, copy to user space edid data failed\n", edid_rw.port);
					ret = -EFAULT;
				}
				else
					ret = 0;
			}
			if(p_edid)
				kfree(p_edid);

		}
		break;
	}

	case V4L2_CID_EXT_HDMI_CONNECTION_STATE:
	{
		struct v4l2_ext_hdmi_connection_state hdmi_v4l2_ext_connection_state;

		if(!(copy_from_user(&hdmi_v4l2_ext_connection_state, to_user_ptr(hdmi_v4l2_ext_control.ptr), sizeof(struct v4l2_ext_hdmi_connection_state))))
		{
			vfe_hdmi_connect_state_t con_state = {0, 0};

			if(v4l2_hdmi_manager(V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_CONNECTION_STATE, &hdmi_v4l2_ext_connection_state.port)==-1)
			{
				ret = 0;
				break;
			}

			con_state.port= hdmi_v4l2_ext_connection_state.port-1;
			if (hdmi_vfe_is_valid_channel(con_state.port)<0) {
			    //HDMI_PRINTF("[WARNING1] drm_info forget to connect hdmi port\n");
			    ret = -1;
			}
			else
			{
				if (vfe_hdmi_drv_get_connection_state(&con_state) != 0)
				{
					ret = -1;
				}
				else
				{
					hdmi_v4l2_ext_connection_state.state =con_state.state;
					ret =0;
				}
			}
			if (copy_to_user(to_user_ptr(hdmi_v4l2_ext_control.ptr), &hdmi_v4l2_ext_connection_state, sizeof(struct v4l2_ext_hdmi_connection_state)))
			{
				rtd_printk(KERN_DEBUG, TAG_NAME,"[ERR] copy_to_user error\n");
				ret = EFAULT;
			}
		}
		break;
	}

	case V4L2_CID_EXT_HDMI_HPD:
	{
		struct v4l2_ext_hdmi_hpd hdmi_v4l2_ext_hpd;

		if(!(copy_from_user(&hdmi_v4l2_ext_hpd, to_user_ptr(hdmi_v4l2_ext_control.ptr), sizeof(struct v4l2_ext_hdmi_hpd))))
		{
			unsigned char ch = 0xF;

			if(v4l2_hdmi_manager(V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_HPD, &hdmi_v4l2_ext_hpd.port)==-1)
			{
				ret = 0;
				break;
			}

			if(IS_VALID_CH(hdmi_v4l2_ext_hpd.port))
			{
				ch = hdmi_v4l2_ext_hpd.port-1;
				//TO DO
				vfe_hdmi_v4l2_get_hpd_flag_port(ch,(unsigned char*)&hdmi_v4l2_ext_hpd.hpd_state);
				//hdmi_v4l2_ext_hpd.hpd_state =1;
				ret =0;
				if (copy_to_user(to_user_ptr(hdmi_v4l2_ext_control.ptr), &hdmi_v4l2_ext_hpd, sizeof(struct v4l2_ext_hdmi_hpd)))
				{
					rtd_printk(KERN_DEBUG, TAG_NAME,"[ERR] copy_to_user error\n");
					ret = EFAULT;
				}
			}
			else
			{
				ret = -1;
				rtd_printk(KERN_ERR, TAG_NAME,"[HDMI][V4L2][ERR] Get V4L2_CID_EXT_HDMI_HPD, Invalid ch=%d \n", hdmi_v4l2_ext_hpd.port);
			}
		}
		break;
	}

	case V4L2_CID_EXT_HDMI_DOLBY_HDR:
	{
		struct v4l2_ext_hdmi_dolby_hdr hdmi_v4l2_ext_dolby_hdr;

		if(!(copy_from_user(&hdmi_v4l2_ext_dolby_hdr, to_user_ptr(hdmi_v4l2_ext_control.ptr), sizeof(struct v4l2_ext_hdmi_dolby_hdr))))
		{
			unsigned char ch = 0xF;

			if(v4l2_hdmi_manager(V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_DOLBY_HDR, &hdmi_v4l2_ext_dolby_hdr.port)==-1)
			{
				ret = 0;
				break;
			}

			ch = hdmi_v4l2_ext_dolby_hdr.port-1;
			if (hdmi_vfe_is_valid_channel(ch)<0) {
			    //HDMI_PRINTF("[WARNING1] drm_info forget to connect hdmi port\n");
			    ret = -1;
			} else if (get_platform() == PLATFORM_K6HP) {
				DOLBY_HDMI_VSIF_T dobly_mode = get_HDMI_Dolby_VSIF_mode();
				switch(dobly_mode)
				{
				case DOLBY_HDMI_VSIF_DISABLE:
					hdmi_v4l2_ext_dolby_hdr.type = V4L2_EXT_HDMI_DOLBY_HDR_TYPE_SDR;
					break;
				case DOLBY_HDMI_VSIF_STD:
					hdmi_v4l2_ext_dolby_hdr.type = V4L2_EXT_HDMI_DOLBY_HDR_TYPE_STANDARD_VSIF_2;
					break;
				case DOLBY_HDMI_VSIF_LL:
					hdmi_v4l2_ext_dolby_hdr.type = V4L2_EXT_HDMI_DOLBY_HDR_TYPE_LOW_LATENCY;
					break;
				case DOLBY_HDMI_h14B_VSIF:
					if (quirk_check_dolby_standard_packet())
						hdmi_v4l2_ext_dolby_hdr.type = V4L2_EXT_HDMI_DOLBY_HDR_TYPE_STANDARD_VSIF_1;
					else
						hdmi_v4l2_ext_dolby_hdr.type = V4L2_EXT_HDMI_DOLBY_HDR_TYPE_SDR;
					break;
				default:
					hdmi_v4l2_ext_dolby_hdr.type = V4L2_EXT_HDMI_DOLBY_HDR_TYPE_SDR;
					HDMI_WARN("[V4L2_CID_EXT_HDMI_DOLBY_HDR] Invalid Dolby VSIF Type=%d, current ch=%d\n", dobly_mode, ch);
					break;
				}
				ret =0;
			} else {
				hdmi_v4l2_ext_dolby_hdr.type = V4L2_EXT_HDMI_DOLBY_HDR_TYPE_SDR;

				ret = 0;
			}

			if (copy_to_user(to_user_ptr(hdmi_v4l2_ext_control.ptr), &hdmi_v4l2_ext_dolby_hdr, sizeof(struct v4l2_ext_hdmi_dolby_hdr)))
			{
				rtd_printk(KERN_DEBUG, TAG_NAME,"[ERR] copy_to_user error\n");
				ret = EFAULT;
			}
		}
		break;
	}

	case V4L2_CID_EXT_HDMI_HDCP_KEY:
	{
		struct v4l2_ext_hdmi_hdcp_key hdmi_v4l2_ext_hdcp_key;

		if(!(copy_from_user(&hdmi_v4l2_ext_hdcp_key, to_user_ptr(hdmi_v4l2_ext_control.ptr), sizeof(struct v4l2_ext_hdmi_hdcp_key))))
		{
			char *p_hdcp1P4_key =NULL;

			if(v4l2_hdmi_manager(V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_HDCP_KEY, NULL)==-1)
			{
				ret = 0;
				break;
			}

			p_hdcp1P4_key = kmalloc(HDCP14_KEY_SIZE, GFP_KERNEL);
			memset(p_hdcp1P4_key, 0, HDCP14_KEY_SIZE);
			if(p_hdcp1P4_key == NULL )
			{
				rtd_printk(KERN_ERR, TAG_NAME,"[ERR] read hdcp to HDMI failed, invalid size \n");
				ret = EFAULT;
			}
			else if(!(copy_from_user(p_hdcp1P4_key, to_user_ptr(hdmi_v4l2_ext_hdcp_key.pData), sizeof(unsigned char)*(HDCP14_KEY_SIZE))))
			{
								ret =0;
				if (vfe_hdmi_drv_read_hdcp(p_hdcp1P4_key, (p_hdcp1P4_key+5)) != 0 ||
					copy_to_user(to_user_ptr(hdmi_v4l2_ext_hdcp_key.pData), p_hdcp1P4_key, sizeof(unsigned char)*(HDCP14_KEY_SIZE))!=0 ||
					copy_to_user(to_user_ptr(hdmi_v4l2_ext_control.ptr), &hdmi_v4l2_ext_hdcp_key, sizeof(struct v4l2_ext_hdmi_hdcp_key))!=0)
				{
					rtd_printk(KERN_ERR, TAG_NAME,"[ERR] read hdcp to HDMI failed, copy to user space edid data failed\n");
					ret = EFAULT;
				}
			}
			if(p_hdcp1P4_key)
				kfree(p_hdcp1P4_key);

		}
		break;
	}

	case V4L2_CID_EXT_HDMI_VRR_FREQUENCY:
	{
		struct v4l2_ext_hdmi_vrr_frequency hdmi_v4l2_ext_vrr_frequency;

		if(!(copy_from_user(&hdmi_v4l2_ext_vrr_frequency, to_user_ptr(hdmi_v4l2_ext_control.ptr), sizeof(struct v4l2_ext_hdmi_vrr_frequency))))
		{
			if(v4l2_hdmi_manager(V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_HDCP_KEY, &hdmi_v4l2_ext_vrr_frequency.port)==-1)
			{
				ret = 0;
				break;
			}

			ret = 0;

			if (v4l2_vfe_hdmi_ext_get_vrr_frequency(&hdmi_v4l2_ext_vrr_frequency)<0)
				ret = EFAULT;
			
			if (copy_to_user(to_user_ptr(hdmi_v4l2_ext_control.ptr), &hdmi_v4l2_ext_vrr_frequency, sizeof(struct v4l2_ext_hdmi_vrr_frequency)))
			{
				rtd_printk(KERN_DEBUG, TAG_NAME,"[ERR] copy_to_user error\n");
				ret = EFAULT;
			}
		}
		break;
	}

	case V4L2_CID_EXT_HDMI_EMP_INFO:
	{
		struct v4l2_ext_hdmi_emp_info hdmi_v4l2_ext_emp_info;

		if((copy_from_user(&hdmi_v4l2_ext_emp_info, to_user_ptr(hdmi_v4l2_ext_control.ptr), sizeof(struct v4l2_ext_hdmi_emp_info))) == 0)
		{
			unsigned char ch = 0;
			unsigned short current_packet_index = hdmi_v4l2_ext_emp_info.current_packet_index;
			vfe_hdmi_em_type_e emp_type = KADP_VFE_HDMI_EMP_UNKNOW;
			int real_packet_total_length = 0;

			if(v4l2_hdmi_manager(V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_EMP_INFO, &hdmi_v4l2_ext_emp_info.port)==-1)
			{
				ret = 0;
				break;
			}

			ch = hdmi_v4l2_ext_emp_info.port-1;
			memset(m_emp_data_buf, 0, MAX_EM_HDR_INFO_LEN);

			switch (hdmi_v4l2_ext_emp_info.type)
			{
			case V4L2_EXT_HDMI_EMP_TYPE_VSEM:
				emp_type = KADP_VFE_HDMI_EMP_VSEMDS;
				break;
			case V4L2_EXT_HDMI_EMP_TYPE_DYNAMICHDREM:
				emp_type = KADP_VFE_HDMI_EMP_HDRDM;
				break;
			case V4L2_EXT_HDMI_EMP_TYPE_VTEM:
				emp_type = KADP_VFE_HDMI_EMP_VTEM;
				break;
			case V4L2_EXT_HDMI_EMP_TYPE_CVTEM:
				emp_type = KADP_VFE_HDMI_EMP_CVTEM;
				break;
			case V4L2_EXT_HDMI_EMP_TYPE_UNDEFINED:
			default:
				emp_type = KADP_VFE_HDMI_EMP_UNKNOW;
				ret = EFAULT;
				break;
			}

			if(emp_type != KADP_VFE_HDMI_EMP_UNKNOW)
			{
				real_packet_total_length = vfe_hdmi_get_emp(ch, emp_type, &m_emp_data_buf[0], MAX_EM_HDR_INFO_LEN);

				if(real_packet_total_length <= 0)
				{//No emp packet, KTASKWBS-14335 requirement ret=0.
					int i = 0;
					ret = 0;
					for(i= 0; i< 31 ; i++)
					{
						hdmi_v4l2_ext_emp_info.data[i] = 0;
					}
					hdmi_v4l2_ext_emp_info.total_packet_number = 0;
					//HDMI_WARN("V4L2_CID_EXT_HDMI_EMP_INFO Get emp fail, ch=%d, emp_type=%d. real_packet_total_length=%d\n", ch, emp_type, real_packet_total_length);
				}
				else
				{
					hdmi_v4l2_ext_emp_info.total_packet_number = (int)(real_packet_total_length/32);
					if((current_packet_index >= hdmi_v4l2_ext_emp_info.total_packet_number) ||
					(hdmi_v4l2_ext_emp_info.total_packet_number*32 > MAX_EM_HDR_INFO_LEN))
					{
						ret = EFAULT;
						HDMI_WARN("V4L2_CID_EXT_HDMI_EMP_INFO Invalid current_packet_index=%d, real_packet_total_length=%d, ch=%d, emp_type=%d.\n",
							current_packet_index, real_packet_total_length,  ch, emp_type);
					}
					else
					{
						int i = 0;
						for(i= 0; i< 31 ; i++)
						{
							hdmi_v4l2_ext_emp_info.data[i] = m_emp_data_buf[current_packet_index*32+i];
						}
						ret =0;
					}
				}

			}

			if (copy_to_user(to_user_ptr(hdmi_v4l2_ext_control.ptr), &hdmi_v4l2_ext_emp_info, sizeof(struct v4l2_ext_hdmi_emp_info)))
			{
				rtd_printk(KERN_DEBUG, TAG_NAME,"[ERR] copy_to_user error\n");
				ret = EFAULT;
			}
		}
		break;
	}

	case V4L2_CID_EXT_HDMI_DIAGNOSTICS_STATUS:
	{
		struct v4l2_ext_hdmi_diagnostics_status hdmi_v4l2_ext_diagnostics_status;

		if(!(copy_from_user(&hdmi_v4l2_ext_diagnostics_status, to_user_ptr(hdmi_v4l2_ext_control.ptr), sizeof(struct v4l2_ext_hdmi_diagnostics_status))))
		{
			unsigned char ch = 0xF;

			if(v4l2_hdmi_manager(V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_DIAGNOSTICS_STATUS, &hdmi_v4l2_ext_diagnostics_status.port)==-1)
			{
				ret = 0;
				break;
			}

			ch = hdmi_v4l2_ext_diagnostics_status.port - 1;
			if (hdmi_vfe_is_valid_channel(ch)<0)
			{
				ret = EFAULT;
			}
			else
			{
				if (v4l2_vfe_hdmi_ext_diagnostic_info(&hdmi_v4l2_ext_diagnostics_status)!= 0)
				{
					rtd_printk(KERN_DEBUG, TAG_NAME,"[ERR] v4l2_vfe_hdmi_ext_diagnostic_info error\n");
					ret = EFAULT; //bad addr
				}
				else
				{
					ret =0;
				}
			}

			if (copy_to_user(to_user_ptr(hdmi_v4l2_ext_control.ptr), &hdmi_v4l2_ext_diagnostics_status, sizeof(struct v4l2_ext_hdmi_diagnostics_status)))
			{
				rtd_printk(KERN_DEBUG, TAG_NAME,"[ERR] copy_to_user error\n");
				ret = EFAULT;
			}

		}
		else
		{
			rtd_printk(KERN_DEBUG, TAG_NAME,"[V4L2_CID_EXT_HDMI_DIAGNOSTICS_STATUS][ERR] copy_from_user error\n");
			ret = EFAULT; //bad addr

		}
		break;
	}

	case V4L2_CID_EXT_HDMI_PHY_STATUS:
	{
		struct v4l2_ext_hdmi_phy_status hdmi_v4l2_ext_phy_status;

		if(!(copy_from_user(&hdmi_v4l2_ext_phy_status, to_user_ptr(hdmi_v4l2_ext_control.ptr), sizeof(struct v4l2_ext_hdmi_phy_status))))
		{
			unsigned char ch = 0xF;

			if(v4l2_hdmi_manager(V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_PHY_STATUS, &hdmi_v4l2_ext_phy_status.port)==-1)
			{
				ret = 0;
				break;
			}

			ch = hdmi_v4l2_ext_phy_status.port - 1;
			if (hdmi_vfe_is_valid_channel(ch)<0)
			{
				ret = EFAULT;
			}
			else
			{
				if (v4l2_vfe_hdmi_ext_phy_status(&hdmi_v4l2_ext_phy_status)!= 0)
				{
					rtd_printk(KERN_DEBUG, TAG_NAME,"[HDMI][ERR] V4L2_CID_EXT_HDMI_PHY_STATUS error\n");
					ret = EFAULT; //bad addr
				}
				else
				{
					ret =0;
				}
			}

			if (copy_to_user(to_user_ptr(hdmi_v4l2_ext_control.ptr), &hdmi_v4l2_ext_phy_status, sizeof(struct v4l2_ext_hdmi_phy_status)))
			{
				rtd_printk(KERN_DEBUG, TAG_NAME,"[HDMI][ERR] V4L2_CID_EXT_HDMI_PHY_STATUS copy_to_user error\n");
				ret = EFAULT;
			}

		}
		else
		{
			rtd_printk(KERN_DEBUG, TAG_NAME,"[HDMI][ERR] V4L2_CID_EXT_HDMI_PHY_STATUS copy_from_user error\n");
			ret = EFAULT; //bad addr

		}
		break;
	}

	case V4L2_CID_EXT_HDMI_LINK_STATUS:
	{
		struct v4l2_ext_hdmi_link_status hdmi_v4l2_ext_link_status;

		if(!(copy_from_user(&hdmi_v4l2_ext_link_status, to_user_ptr(hdmi_v4l2_ext_control.ptr), sizeof(struct v4l2_ext_hdmi_link_status))))
		{
			unsigned char ch = 0xF;

			if(v4l2_hdmi_manager(V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_LINK_STATUS, &hdmi_v4l2_ext_link_status.port)==-1)
			{
				ret = 0;
				break;
			}

			ch = hdmi_v4l2_ext_link_status.port - 1;
			if (hdmi_vfe_is_valid_channel(ch)<0)
			{
				ret = EFAULT;
			}
			else
			{
				if (v4l2_vfe_hdmi_ext_link_status(&hdmi_v4l2_ext_link_status)!= 0)
				{
					rtd_printk(KERN_DEBUG, TAG_NAME,"[HDMI][ERR] V4L2_CID_EXT_HDMI_LINK_STATUS error\n");
					ret = EFAULT; //bad addr
				}
				else
				{
					ret =0;
				}
			}

			if (copy_to_user(to_user_ptr(hdmi_v4l2_ext_control.ptr), &hdmi_v4l2_ext_link_status, sizeof(struct v4l2_ext_hdmi_link_status)))
			{
				rtd_printk(KERN_DEBUG, TAG_NAME,"[HDMI][ERR] V4L2_CID_EXT_HDMI_LINK_STATUS copy_to_user error\n");
				ret = EFAULT;
			}
		}
		else
		{
			rtd_printk(KERN_DEBUG, TAG_NAME,"[HDMI][ERR] V4L2_CID_EXT_HDMI_LINK_STATUS copy_from_user error\n");
			ret = EFAULT; //bad addr

		}
		break;
	}

	case V4L2_CID_EXT_HDMI_VIDEO_STATUS:
	{
		struct v4l2_ext_hdmi_video_status hdmi_v4l2_ext_video_status;

		if(!(copy_from_user(&hdmi_v4l2_ext_video_status, to_user_ptr(hdmi_v4l2_ext_control.ptr), sizeof(struct v4l2_ext_hdmi_video_status))))
		{
			unsigned char ch = 0xF;

			if(v4l2_hdmi_manager(V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_VIDEO_STATUS, &hdmi_v4l2_ext_video_status.port)==-1)
			{
				ret = 0;
				break;
			}

			ch = hdmi_v4l2_ext_video_status.port - 1;
			if (hdmi_vfe_is_valid_channel(ch)<0)
			{
				ret = EFAULT;
			}
			else
			{
				if (v4l2_vfe_hdmi_ext_video_status(&hdmi_v4l2_ext_video_status)!= 0)
				{
					rtd_printk(KERN_DEBUG, TAG_NAME,"[HDMI][ERR] V4L2_CID_EXT_HDMI_VIDEO_STATUS error\n");
					ret = EFAULT; //bad addr
				}
				else
				{
					ret =0;
				}
			}

			if (copy_to_user(to_user_ptr(hdmi_v4l2_ext_control.ptr), &hdmi_v4l2_ext_video_status, sizeof(struct v4l2_ext_hdmi_video_status)))
			{
				rtd_printk(KERN_DEBUG, TAG_NAME,"[HDMI][ERR] V4L2_CID_EXT_HDMI_VIDEO_STATUS copy_to_user error\n");
				ret = EFAULT;
			}
		}
		else
		{
			rtd_printk(KERN_DEBUG, TAG_NAME,"[HDMI][ERR] V4L2_CID_EXT_HDMI_VIDEO_STATUS copy_from_user error\n");
			ret = EFAULT; //bad addr

		}
		break;
	}

	case V4L2_CID_EXT_HDMI_AUDIO_STATUS:
	{
		struct v4l2_ext_hdmi_audio_status hdmi_v4l2_ext_audio_status;

		if(!(copy_from_user(&hdmi_v4l2_ext_audio_status, to_user_ptr(hdmi_v4l2_ext_control.ptr), sizeof(struct v4l2_ext_hdmi_audio_status))))
		{
			unsigned char ch = 0xF;

			if(v4l2_hdmi_manager(V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_AUDIO_STATUS, &hdmi_v4l2_ext_audio_status.port)==-1)
			{
				ret = 0;
				break;
			}

			ch = hdmi_v4l2_ext_audio_status.port - 1;
			if (hdmi_vfe_is_valid_channel(ch)<0)
			{
				ret = EFAULT;
			}
			else
			{
				if (v4l2_vfe_hdmi_ext_audio_status(&hdmi_v4l2_ext_audio_status)!= 0)
				{
					rtd_printk(KERN_DEBUG, TAG_NAME,"[HDMI][ERR] V4L2_CID_EXT_HDMI_AUDIO_STATUS error\n");
					ret = EFAULT; //bad addr
				}
				else
				{
					ret =0;
				}
			}

			if (copy_to_user(to_user_ptr(hdmi_v4l2_ext_control.ptr), &hdmi_v4l2_ext_audio_status, sizeof(struct v4l2_ext_hdmi_audio_status)))
			{
				rtd_printk(KERN_DEBUG, TAG_NAME,"[HDMI][ERR] V4L2_CID_EXT_HDMI_AUDIO_STATUS copy_to_user error\n");
				ret = EFAULT;
			}
		}
		else
		{
			rtd_printk(KERN_DEBUG, TAG_NAME,"[HDMI][ERR] V4L2_CID_EXT_HDMI_AUDIO_STATUS copy_from_user error\n");
			ret = EFAULT; //bad addr

		}
		break;
	}

	case V4L2_CID_EXT_HDMI_HDCP_STATUS:
	{
		struct v4l2_ext_hdmi_hdcp_status hdmi_v4l2_ext_hdcp_status;

		if(!(copy_from_user(&hdmi_v4l2_ext_hdcp_status, to_user_ptr(hdmi_v4l2_ext_control.ptr), sizeof(struct v4l2_ext_hdmi_hdcp_status))))
		{
			unsigned char ch = 0xF;

			if(v4l2_hdmi_manager(V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_HDCP_STATUS, &hdmi_v4l2_ext_hdcp_status.port)==-1)
			{
				ret = 0;
				break;
			}

			ch = hdmi_v4l2_ext_hdcp_status.port - 1;
			if (hdmi_vfe_is_valid_channel(ch)<0)
			{
				ret = EFAULT;
			}
			else
			{
				if (v4l2_vfe_hdmi_ext_hdcp_status(&hdmi_v4l2_ext_hdcp_status)!= 0)
				{
					rtd_printk(KERN_DEBUG, TAG_NAME,"[HDMI][ERR] V4L2_CID_EXT_HDMI_HDCP_STATUS error\n");
					ret = EFAULT; //bad addr
				}
				else
				{
					ret =0;
				}
			}

			if (copy_to_user(to_user_ptr(hdmi_v4l2_ext_control.ptr), &hdmi_v4l2_ext_hdcp_status, sizeof(struct v4l2_ext_hdmi_hdcp_status)))
			{
				rtd_printk(KERN_DEBUG, TAG_NAME,"[HDMI][ERR] V4L2_CID_EXT_HDMI_HDCP_STATUS copy_to_user error\n");
				ret = EFAULT;
			}
		}
		else
		{
			rtd_printk(KERN_DEBUG, TAG_NAME,"[HDMI][ERR] V4L2_CID_EXT_HDMI_HDCP_STATUS copy_from_user error\n");
			ret = EFAULT; //bad addr
		}
		break;
	}

	case V4L2_CID_EXT_HDMI_SCDC_STATUS:
	{
		struct v4l2_ext_hdmi_scdc_status hdmi_v4l2_ext_scdc_status;

		if(!(copy_from_user(&hdmi_v4l2_ext_scdc_status, to_user_ptr(hdmi_v4l2_ext_control.ptr), sizeof(struct v4l2_ext_hdmi_scdc_status))))
		{
			unsigned char ch = 0xF;

			if(v4l2_hdmi_manager(V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_SCDC_STATUS, &hdmi_v4l2_ext_scdc_status.port)==-1)
			{
				ret = 0;
				break;
			}

			ch = hdmi_v4l2_ext_scdc_status.port - 1;
			if (hdmi_vfe_is_valid_channel(ch)<0)
			{
				ret = EFAULT;
			}
			else
			{
				if (v4l2_vfe_hdmi_ext_scdc_status(&hdmi_v4l2_ext_scdc_status)!= 0)
				{
					rtd_printk(KERN_DEBUG, TAG_NAME,"[HDMI][ERR] V4L2_CID_EXT_HDMI_SCDC_STATUS error\n");
					ret = EFAULT; //bad addr
				}
				else
				{
					ret =0;
				}
			}

			if (copy_to_user(to_user_ptr(hdmi_v4l2_ext_control.ptr), &hdmi_v4l2_ext_scdc_status, sizeof(struct v4l2_ext_hdmi_scdc_status)))
			{
				rtd_printk(KERN_DEBUG, TAG_NAME,"[HDMI][ERR] V4L2_CID_EXT_HDMI_SCDC_STATUS copy_to_user error\n");
				ret = EFAULT;
			}
		}
		else
		{
			rtd_printk(KERN_DEBUG, TAG_NAME,"[HDMI][ERR] V4L2_CID_EXT_HDMI_SCDC_STATUS copy_from_user error\n");
			ret = EFAULT; //bad addr
		}
		break;
	}

	case V4L2_CID_EXT_HDMI_ERROR_STATUS:
	{
		struct v4l2_ext_hdmi_error_status hdmi_v4l2_ext_error_status;

		if(!(copy_from_user(&hdmi_v4l2_ext_error_status, to_user_ptr(hdmi_v4l2_ext_control.ptr), sizeof(struct v4l2_ext_hdmi_error_status))))
		{
			unsigned char ch = 0xF;

			if(v4l2_hdmi_manager(V4L2_HDMI_G_EXT_CTRL, V4L2_CID_EXT_HDMI_ERROR_STATUS, &hdmi_v4l2_ext_error_status.port)==-1)
			{
				ret = 0;
				break;
			}

			ch = hdmi_v4l2_ext_error_status.port - 1;
			if (hdmi_vfe_is_valid_channel(ch)<0)
			{
				ret = EFAULT;
			}
			else
			{
				if (v4l2_vfe_hdmi_ext_error_status(&hdmi_v4l2_ext_error_status)!= 0)
				{
					rtd_printk(KERN_DEBUG, TAG_NAME,"[HDMI][ERR] V4L2_CID_EXT_HDMI_ERROR_STATUS error\n");
					ret = EFAULT; //bad addr
				}
				else
				{
					ret =0;
				}
			}

			if (copy_to_user(to_user_ptr(hdmi_v4l2_ext_control.ptr), &hdmi_v4l2_ext_error_status, sizeof(struct v4l2_ext_hdmi_error_status)))
			{
				rtd_printk(KERN_DEBUG, TAG_NAME,"[HDMI][ERR] V4L2_CID_EXT_HDMI_ERROR_STATUS copy_to_user error\n");
				ret = EFAULT;
			}
		}
		else
		{
			rtd_printk(KERN_DEBUG, TAG_NAME,"[HDMI][ERR] V4L2_CID_EXT_HDMI_ERROR_STATUS copy_from_user error\n");
			ret = EFAULT; //bad addr
		}
		break;
	}

	default:
		break;
	}
	memcpy(ctrls->controls,&hdmi_v4l2_ext_control,sizeof(struct v4l2_ext_control));
	/* update info to user */
	//if (copy_to_user((void __user *)ctrls, (const void * )&hdmi_v4l2_ext_controls, sizeof(hdmi_v4l2_ext_controls)))
	//{
		//ret = EFAULT;
	//}
	return ret;

}

static int v4l2_vfe_hdmi_ioctl_g_input(struct file *file, void *fh, unsigned int *arg)
{
	int ret = 0;
	unsigned char connect_ch=0;

	if(v4l2_hdmi_manager(V4L2_HDMI_G_INPUT, NO_SUB_ID, NULL)==-1)
	{
		ret = 0;
		return ret;
	}

	ret = hdmi_vfe_get_connected_channel(&connect_ch);
	*arg =connect_ch+1;
	return ret;
}
static int v4l2_vfe_hdmi_ioctl_s_input(struct file *file, void *fh, unsigned int arg)
{
	int ret = 0;
	int inputval =0;
	inputval =arg;
	HDMI_EMG("v4l2_vfe_hdmi_ioctl_s_input, arg=%d\n", arg);

/*
	if (copy_from_user((void *)&inputval, (const void __user *)arg, sizeof(int)))
	{
		rtd_printk(KERN_NOTICE, TAG_NAME, "[ERR] VIDIOC_S_INPUT copy_from_user error\n");
		ret = EFAULT;
	}
*/
	/* excute driver api */
	if(inputval ==0)
	{
		if(v4l2_hdmi_manager(V4L2_HDMI_S_INPUT, NO_SUB_ID, NULL)==-1)
		{
			ret = 0;
			return ret;
		}

		/* excute driver api */
		down(&HDMI_DetectSemaphore);
		//HDMI_Global_Status = SRC_OPEN_DONE;
		//FIXME WEBOS5.0 never call HDMI disconnect, this function will never enter in.
		set_HDMI_Global_Status(SRC_OPEN_DONE);
		up(&HDMI_DetectSemaphore);


		if (vfe_hdmi_drv_disconnect(connect_port) != 0)
		{
			//connect_port=-1;			 
			ret = -1;
		} else {
			down(&SetSource_Semaphore);
			HDMI_Input_Source = _SRC_MAX;
			Set_Reply_Zero_Timing_Flag(VSC_INPUTSRC_HDMI, CLEAR_ZERO_FLAG);
			up(&SetSource_Semaphore);
		}
		//HDMI_EMG("DIS = %d\n",ret);	
		connect_port_pid = 0;
	}
	else
	{
		if(v4l2_hdmi_manager(V4L2_HDMI_S_INPUT, NO_SUB_ID, &inputval)==-1)
		{
			ret = 0;
			return ret;
		}

		down(&HDMI_DetectSemaphore);
		connect_port = inputval-1;	
		connect_port_pid = task_pid_nr(current);  // store pid of the connected process	

		if (vfe_hdmi_drv_connect(connect_port) == 0)
		{
			//[KTASKWBS-5100]P_only mode ,change HDMI source can't display
			//P_only mode  hdmo disconnect /close  =>vsc disconnet => hdmi open/connect => vsc open/connect
			//Normal mode  vsc disconnect =>HDMI_set_detect_flag=1 =>hdmi disconnect /close /open/ connect =>vsc open/connect
			HDMI_set_detect_flag(TRUE);
			set_HDMI_Global_Status(SRC_CONNECT_DONE);
			HDMI_EMG("set SRC_CONNECT_DONE, connect_port= %d\n",connect_port);
		}
		else
		{
			ret = -1;
		}
		up(&HDMI_DetectSemaphore);

		if (ret == 0) {
			down(&SetSource_Semaphore);
			HDMI_Input_Source = _SRC_HDMI;
			up(&SetSource_Semaphore);
		}
		//HDMI_EMG("CON = %d\n",ret);
	}
	return ret;

}
static int v4l2_vfe_hdmi_ioctl_s_ctrl(struct file *file, void *fh, struct v4l2_control *argp)
{
	int ret = -1;

	if(argp->id == V4L2_CID_EXT_HDMI_POWER_OFF)
	{
		HDMI_INFO("[v4l2_vfe_hdmi_ioctl_s_ctrl], V4L2_POWER_OFF, argp->value=%d\n", argp->value);
		if(v4l2_hdmi_manager(V4L2_HDMI_S_CTRL, V4L2_CID_EXT_HDMI_POWER_OFF, NULL)==-1)
		{
			ret = 0;
			return ret;
		}

	    //0 (Ignore), 1 (Set Power Off)
		if(argp->value == 1)
		{
			rtd_printk(KERN_INFO, TAG_NAME,"V4L2_POWER_OFF\n");
			//TO DO
			//vfe_hdmi_drv_suspend();
			ret =0;
		}
	}
	else if(argp->id == V4L2_CID_EXT_HDMI_DISCONNECT)
	{
		unsigned int ch = argp->value;
		HDMI_INFO("[v4l2_vfe_hdmi_ioctl_s_ctrl], V4L2_CID_EXT_HDMI_DISCONNECT, ch=%d\n", ch);

		if(v4l2_hdmi_manager(V4L2_HDMI_S_CTRL, V4L2_CID_EXT_HDMI_DISCONNECT, &ch)==-1)
		{
			ret = 0;
			return ret;
		}

		if(IS_VALID_CH(ch))
		{
			ret =0;

			ch = ch -1;
			if (vfe_hdmi_drv_disconnect(ch) != 0)
			{
				ret = -1;
			} else {
				//FIXME WEBOS5.0 never call HDMI disconnect, this function will never enter in.
				set_HDMI_Global_Status(SRC_OPEN_DONE);
				down(&SetSource_Semaphore);
				HDMI_Input_Source = _SRC_MAX;
				Set_Reply_Zero_Timing_Flag(VSC_INPUTSRC_HDMI, CLEAR_ZERO_FLAG);
				up(&SetSource_Semaphore);
			}
		}
		else
		{
			rtd_printk(KERN_ERR, TAG_NAME,"[HDMI][V4L2][ERR] V4L2_CID_EXT_HDMI_DISCONNECT, Invalid ch=%d\n", ch);
			ret = -1;
		}
	}
	return ret;
}

static const struct v4l2_ioctl_ops v4l2_vfe_hdmi_ioctl_ops = {
    .vidioc_s_ext_ctrls = v4l2_vfe_hdmi_ioctl_s_ext_ctrls,
    .vidioc_g_ext_ctrls = v4l2_vfe_hdmi_ioctl_g_ext_ctrls,
    .vidioc_s_input = v4l2_vfe_hdmi_ioctl_s_input,
    .vidioc_g_input = v4l2_vfe_hdmi_ioctl_g_input,
    .vidioc_s_ctrl = v4l2_vfe_hdmi_ioctl_s_ctrl,
};

static const struct v4l2_file_operations v4l2_vfe_hdmi_fops= {
	.owner =    THIS_MODULE,
	.open  =    v4l2_vfe_hdmi_open,
	.release =  v4l2_vfe_hdmi_release,
	.read  =    v4l2_vfe_hdmi_read,
	.poll			= v4l2_vfe_hdmi_poll,
	.unlocked_ioctl  = video_ioctl2, /* V4L2 ioctl handler */
	.mmap		= v4l2_vfe_hdmi_mmap,

};


static struct video_device v4l2_vfe_hdmi_tmplate = {
	.name = VFE_V4L2_HDMI_NAME,
	.fops	= &v4l2_vfe_hdmi_fops,
	.ioctl_ops = &v4l2_vfe_hdmi_ioctl_ops,
	.minor = -1,
	.release = video_device_release,
};

static int __init v4l2_vfe_hdmi_init(void)
{
	struct v4l2_vfe_hdmi_device *main_dev;
	struct video_device *main_vfd = NULL;
	int ret;
	main_dev = kzalloc(sizeof(*main_dev), GFP_KERNEL);
	if (!main_dev)
		return -ENOMEM;
	strcpy(main_dev->v4l2_dev.name, V4L2_EXT_DEV_PATH_HDMI);

	ret = v4l2_device_register(NULL, &main_dev->v4l2_dev);
	if (ret)
	{
		goto free_dev;
	}

	main_vfd = video_device_alloc();
	if (!main_vfd) {
		v4l2_err(&main_dev->v4l2_dev, "Failed to allocate video device\n");
		ret = -ENOMEM;
		goto unreg_dev;
	}

	*main_vfd = v4l2_vfe_hdmi_tmplate;
	main_vfd->v4l2_dev = &main_dev->v4l2_dev; /* here set the v4l2_device, you have already registered it */
	ret = video_register_device(main_vfd, VFL_TYPE_GRABBER, V4L2_EXT_DEV_NO_HDMI);
	if (ret) {
		v4l2_err(&main_dev->v4l2_dev, "Failed to register video device\n");
		goto rel_vdev;
	}

	video_set_drvdata(main_vfd, main_dev);
	snprintf(main_vfd->name, sizeof(main_vfd->name), "%s", v4l2_vfe_hdmi_tmplate.name);
	main_dev->vfd = main_vfd;
	/* the debug message*/
    v4l2_info(&main_dev->v4l2_dev, "V4L2 device registered as %s\n", video_device_node_name(main_vfd));
	return 0;

rel_vdev:
	video_device_release(main_vfd);
unreg_dev:
	v4l2_device_unregister(&main_dev->v4l2_dev);
free_dev:
	kfree(main_dev);

	return ret;
}

static void __exit v4l2_vfe_hdmi_exit(void)
{
	return;
}


module_init(v4l2_vfe_hdmi_init);
module_exit(v4l2_vfe_hdmi_exit);


