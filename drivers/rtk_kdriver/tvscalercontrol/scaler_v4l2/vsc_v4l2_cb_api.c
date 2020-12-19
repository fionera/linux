/*
 *      vsc_v4l2_driver.c  related callback api-- 
 *
 *      Copyright (C) 2018
 *
 */
//Kernel Header file
#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/videodev2.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/pageremap.h>
#include <linux/wait.h>
#include <linux/version.h>
#include <linux/spinlock_types.h>
#include <asm/unaligned.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-common.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#define to_user_ptr(x)          compat_ptr((unsigned int)x)
#else
#define to_user_ptr(x)          ((void* __user)(x)) // convert 32 bit value to user pointer
#endif


//common
#include <ioctrl/scaler/vsc_cmd_id.h>
#ifdef CONFIG_KDRIVER_USE_NEW_COMMON
#include <scaler/scalerCommon.h>
#else
#include <scalercommon/scalerCommon.h>
#endif

#include <linux/v4l2-ext/v4l2-controls-ext.h>
#include <linux/v4l2-ext/videodev2-ext.h>
#include <tvscalercontrol/scaler_v4l2/vsc_v4l2_api.h>
#include <tvscalercontrol/scalerdrv/scalerdrv.h>
#include <tvscalercontrol/scaler_vpqmemcdev.h>

extern void SET_BBD_STAGE(unsigned char display, unsigned char stage);
static UINT8 main_vsc_source_index = 0;//save input source index;
static enum v4l2_ext_vsc_input_src main_vsc_source = V4L2_EXT_VSC_INPUT_SRC_NONE;//save last vsc source
static enum v4l2_ext_vsc_dest main_vsc_dest_type = V4L2_EXT_VSC_DEST_NONE;//save last vsc dest type
struct v4l2_ext_vsc_vdo_mode main_vdo_mode = {V4L2_EXT_VSC_VDO_PORT_NONE, 0};//save last vdo vdec port
struct v4l2_ext_vsc_active_win_info main_bbd_active_info = {{0}, {0}};
struct v4l2_ext_vsc_active_win_info sub_bbd_active_info = {{0}, {0}};
static DEFINE_SPINLOCK(Main_BBD_INFO_Spinlock);/*Spin lock no context switch. This is for copy paramter*/
static DEFINE_SPINLOCK(Sub_BBD_INFO_Spinlock);/*Spin lock no context switch. This is for copy paramter*/



static enum v4l2_ext_vsc_win_color main_blank_color = V4L2_EXT_VSC_WIN_COLOR_NORMAL;//save last blank color
static enum v4l2_ext_vsc_hdr_type main_hdr_type = V4L2_EXT_VSC_HDR_TYPE_SDR;//save hdr type
/*static struct v4l2_ext_vsc_win_region main_win_info = {0}; KTASKWBS-13251 update active region,remove static */
struct v4l2_ext_vsc_win_region main_win_info = {0};//save input type
static __s32 main_freeze_status = 0;//save freeze status;
static __s32 rgb444_status = 0;//save RGB444 status;
static __s32 main_adaptive_stream_status = 0;//save adaptive stream status;
static __s32 main_frame_delay_buffer = 0;//save frame delay buffer;
enum v4l2_ext_vsc_pattern main_vsc_pattern = V4L2_EXT_VSC_PATTERN_OFF;//save vsc pattern

static UINT8 sub_vsc_source_index = 0;//save input source index;
static enum v4l2_ext_vsc_input_src sub_vsc_source = V4L2_EXT_VSC_INPUT_SRC_NONE;//save last vsc source
static enum v4l2_ext_vsc_dest sub_vsc_dest_type = V4L2_EXT_VSC_DEST_NONE;//save last vsc dest type
struct v4l2_ext_vsc_vdo_mode sub_vdo_mode = {V4L2_EXT_VSC_VDO_PORT_NONE, 0};//save last vdo vdec port
static enum v4l2_ext_vsc_win_color sub_blank_color = V4L2_EXT_VSC_WIN_COLOR_NORMAL;//save last blank color
static enum v4l2_ext_vsc_hdr_type sub_hdr_type = V4L2_EXT_VSC_HDR_TYPE_SDR;//save hdr type
static struct v4l2_ext_vsc_win_region sub_win_info = {0};//save input type
static __s32 sub_freeze_status = 0;//save freeze status;
static __s32 sub_adaptive_stream_status = 0;//save adaptive stream status;
static __s32 sub_frame_delay_buffer = 0;//save frame delay buffer;
enum v4l2_ext_vsc_pattern sub_vsc_pattern = V4L2_EXT_VSC_PATTERN_OFF;//save vsc pattern

static struct v4l2_ext_vsc_win_prop subwin_prop  = {V4L2_EXT_VSC_WIN_MODE_NONE,V4L2_EXT_VSC_MIRROR_MODE_NONE,V4L2_EXT_VSC_MEMORY_TYPE_NONE};

static struct v4l2_ext_vsc_zorder mainzorder = {0,255};
static struct v4l2_ext_vsc_zorder subzorder = {0,255};

static struct v4l2_ext_vsc_latency_pattern_info mainlatencypattern = {{0,0,0,0},V4L2_EXT_VSC_PATTERN_BLACK};
static struct v4l2_ext_vsc_latency_pattern_info sublatencypattern = {{0,0,0,0},V4L2_EXT_VSC_PATTERN_BLACK};

static __s32 main_freezeframebuffer_status = 0;
static __s32 sub_freezeframebuffer_status = 0;

static char vsc_v4l2_vsc_control(unsigned char display, unsigned char connect, struct v4l2_ext_vsc_connect_info info)
{
    KADP_VSC_INPUT_SRC_INFO_T inputSrcInfo = {0};
    KADP_VSC_OUTPUT_MODE_T outputMode;
	enum v4l2_ext_vsc_input_src disconnect_src;
	enum v4l2_ext_vsc_dest disconnect_type;
    if(connect)//vsc connect
    {
        switch(info.in.src)
        {
            case V4L2_EXT_VSC_INPUT_SRC_AVD:
                inputSrcInfo.type = KADP_VSC_INPUTSRC_AVD;
                break;

            case V4L2_EXT_VSC_INPUT_SRC_ADC:
                inputSrcInfo.type = KADP_VSC_INPUTSRC_ADC;
                break;

            case V4L2_EXT_VSC_INPUT_SRC_HDMI:
                inputSrcInfo.type = KADP_VSC_INPUTSRC_HDMI;
                break;

            case V4L2_EXT_VSC_INPUT_SRC_VDEC:
                inputSrcInfo.type = KADP_VSC_INPUTSRC_VDEC;
                break;

            case V4L2_EXT_VSC_INPUT_SRC_JPEG:
                inputSrcInfo.type = KADP_VSC_INPUTSRC_JPEG;
                break;

            default:
                printk(KERN_ERR "func:%s [error] connect info.in.src no source\r\n",__FUNCTION__);
                return FALSE; 
                break;
        }
        
        switch(info.out)
        {
            case V4L2_EXT_VSC_DEST_DISPLAY:
                outputMode = KADP_VSC_OUTPUT_DISPLAY_MODE;
                break;

            case V4L2_EXT_VSC_DEST_VENC:
                outputMode = KADP_VSC_OUTPUT_VENC_MODE;
                break;
                
            case V4L2_EXT_VSC_DEST_MEMORY:
                outputMode = KADP_VSC_OUTPUT_MEMORY_MODE;
                break;

            case V4L2_EXT_VSC_DEST_AVE:
                outputMode = KADP_VSC_OUTPUT_AVE_MODE;
                break;

            default:
                printk(KERN_ERR "func:%s [error] connect info.out no DEST type\r\n",__FUNCTION__);
                return FALSE; 
                break;
        }
		if (display == SLR_MAIN_DISPLAY) {
	        main_vsc_source = info.in.src;
	        main_vsc_source_index = info.in.index;
	        main_vsc_dest_type = info.out;
		} else if (display == SLR_SUB_DISPLAY) {
	        sub_vsc_source = info.in.src;
	        sub_vsc_source_index = info.in.index;
	        sub_vsc_dest_type = info.out;
		} 
        rtk_hal_vsc_open((display == SLR_MAIN_DISPLAY)? VIDEO_WID_0 : VIDEO_WID_1);
        rtk_hal_vsc_Connect((display == SLR_MAIN_DISPLAY)? VIDEO_WID_0 : VIDEO_WID_1, inputSrcInfo, outputMode);
    }
    else//disconnect
    {
    	if (display == SLR_MAIN_DISPLAY)
    	{
    		disconnect_src = main_vsc_source;
			disconnect_type = main_vsc_dest_type;
    	}
		else
		{
			disconnect_src = sub_vsc_source;
			disconnect_type = sub_vsc_dest_type;
		}
        switch(disconnect_src)
        {
            case V4L2_EXT_VSC_INPUT_SRC_AVD:
                inputSrcInfo.type = KADP_VSC_INPUTSRC_AVD;
                break;

            case V4L2_EXT_VSC_INPUT_SRC_ADC:
                inputSrcInfo.type = KADP_VSC_INPUTSRC_ADC;
                break;

            case V4L2_EXT_VSC_INPUT_SRC_HDMI:
                inputSrcInfo.type = KADP_VSC_INPUTSRC_HDMI;
                break;

            case V4L2_EXT_VSC_INPUT_SRC_VDEC:
                inputSrcInfo.type = KADP_VSC_INPUTSRC_VDEC;
                break;

            case V4L2_EXT_VSC_INPUT_SRC_JPEG:
                inputSrcInfo.type = KADP_VSC_INPUTSRC_JPEG;
                break;

            default:
                printk(KERN_ERR "func:%s [error] disconnect info.in.src no source\r\n",__FUNCTION__);
                return FALSE; 
                break;
        }
        
        switch(disconnect_type)
        {
            case V4L2_EXT_VSC_DEST_DISPLAY:
                outputMode = KADP_VSC_OUTPUT_DISPLAY_MODE;
                break;

            case V4L2_EXT_VSC_DEST_VENC:
                outputMode = KADP_VSC_OUTPUT_VENC_MODE;
                break;
                
            case V4L2_EXT_VSC_DEST_MEMORY:
                outputMode = KADP_VSC_OUTPUT_MEMORY_MODE;
                break;

            case V4L2_EXT_VSC_DEST_AVE:
                outputMode = KADP_VSC_OUTPUT_AVE_MODE;
                break;

            default:
                printk(KERN_ERR "func:%s [error] disconnect info.out no DEST type\r\n",__FUNCTION__);
                return FALSE;  
                break;
        }

		if (display == SLR_MAIN_DISPLAY) {
			main_vsc_source = V4L2_EXT_VSC_INPUT_SRC_NONE;
			main_vsc_source_index = 0;
			main_vsc_dest_type = V4L2_EXT_VSC_DEST_NONE;
		} else if (display == SLR_SUB_DISPLAY) {
	        sub_vsc_source = V4L2_EXT_VSC_INPUT_SRC_NONE;
	        sub_vsc_source_index = 0;
	        sub_vsc_dest_type = V4L2_EXT_VSC_DEST_NONE;
		} 		
        rtk_hal_vsc_Disconnect((display == SLR_MAIN_DISPLAY)? VIDEO_WID_0 : VIDEO_WID_1, inputSrcInfo, outputMode);
        rtk_hal_vsc_close((display == SLR_MAIN_DISPLAY)? VIDEO_WID_0 : VIDEO_WID_1);
    }
    return TRUE;
}

char vsc_v4l2_vdo_control(unsigned char display, struct v4l2_ext_vsc_vdo_mode mode)
{
    switch(mode.vdo_port)
    {
        case V4L2_EXT_VSC_VDO_PORT_NONE://vdo disconnect
            vdo_disconnect((display == SLR_MAIN_DISPLAY)? VIDEO_WID_0 : VIDEO_WID_1, (unsigned char)mode.vdec_port);
            break;

        case V4L2_EXT_VSC_VDO_PORT_0://vdo connect main
            if(display != SLR_MAIN_DISPLAY)
            {
                printk(KERN_ERR "func:%s [error] vdo port not match main\r\n",__FUNCTION__);
                return FALSE; 
            }
            vdo_connect(VIDEO_WID_0, (unsigned char)mode.vdec_port);
            break; 

        case V4L2_EXT_VSC_VDO_PORT_1://vdo connect sub
            if(display == SLR_MAIN_DISPLAY)
            {
                printk(KERN_ERR "func:%s [error] vdo port not match sub\r\n",__FUNCTION__);
                return FALSE; 
            }
            vdo_connect(VIDEO_WID_1, (unsigned char)mode.vdec_port);
            break;

        default:
            return FALSE; 
            break;
    }

    if (display == SLR_MAIN_DISPLAY) {
        main_vdo_mode.vdec_port = mode.vdec_port;
        main_vdo_mode.vdo_port = mode.vdo_port;
    } else if (display == SLR_SUB_DISPLAY) {
        sub_vdo_mode.vdec_port = mode.vdec_port;
        sub_vdo_mode.vdo_port = mode.vdo_port;
    }

    return TRUE;
}
EXPORT_SYMBOL(vsc_v4l2_vdo_control);

static char vsc_v4l2_winblank_ctrl(unsigned char display, enum v4l2_ext_vsc_win_color value)
{
    switch(value)
    {
        case V4L2_EXT_VSC_WIN_COLOR_BLACK:
            rtk_hal_vsc_SetWinBlank((display == SLR_MAIN_DISPLAY)? VIDEO_WID_0 : VIDEO_WID_1, TRUE, KADP_VIDEO_DDI_WIN_COLOR_BLACK);
            break;
	    case V4L2_EXT_VSC_WIN_COLOR_BLUE:
            rtk_hal_vsc_SetWinBlank((display == SLR_MAIN_DISPLAY)? VIDEO_WID_0 : VIDEO_WID_1, TRUE, KADP_VIDEO_DDI_WIN_COLOR_BLUE);
            break;
	    case V4L2_EXT_VSC_WIN_COLOR_GRAY:
            rtk_hal_vsc_SetWinBlank((display == SLR_MAIN_DISPLAY)? VIDEO_WID_0 : VIDEO_WID_1, TRUE, KADP_VIDEO_DDI_WIN_COLOR_GRAY);
            break;
        case V4L2_EXT_VSC_WIN_COLOR_NORMAL://mute off
            rtk_hal_vsc_SetWinBlank((display == SLR_MAIN_DISPLAY)? VIDEO_WID_0 : VIDEO_WID_1, FALSE, KADP_VIDEO_DDI_WIN_COLOR_BLACK);
            break;
        default:
            printk(KERN_ERR "func:%s [error] vsc_v4l2_winblank_ctrl wrong value(%d)\r\n",__FUNCTION__, value);
            return FALSE; 
            break;
    }
    if(display == SLR_MAIN_DISPLAY)
        main_blank_color = value;
    else
        sub_blank_color = value;
    return TRUE;
}

static char vsc_v4l2_hdr_control(unsigned char display, enum v4l2_ext_vsc_hdr_type type)
{
    KADP_VSC_HDR_TYPE_T eHdrMode;
#ifdef CONFIG_RTK_KDRV_DV_IDK_DUMP
    {
	    extern bool dolby_adapter_is_force_dolby(void);
	    if (dolby_adapter_is_force_dolby()) {
		    type = V4L2_EXT_VSC_HDR_TYPE_DOLBY;
	    }
    }
#endif
	if((display == SLR_MAIN_DISPLAY) && ( main_hdr_type == type)) {
		return TRUE;
	}
	if((display == SLR_SUB_DISPLAY) && ( sub_hdr_type == type)) {
		return TRUE;
	}
    switch(type)
    {
        case V4L2_EXT_VSC_HDR_TYPE_SDR://hdr disconnect
            rtk_hal_vsc_dm_disconnect(display);
            rtk_hal_vsc_dm_close(display);
            if(display == SLR_MAIN_DISPLAY)
                main_hdr_type = type;
            else
                sub_hdr_type = type;
            return TRUE;
            break;
        case V4L2_EXT_VSC_HDR_TYPE_HDR10:
            eHdrMode = KADP_VSC_HDR_HDR10;
            break;
        case V4L2_EXT_VSC_HDR_TYPE_DOLBY:
            eHdrMode = KADP_VSC_HDR_DOLBY;
            break;
        case V4L2_EXT_VSC_HDR_TYPE_HLG:
            eHdrMode = KADP_VSC_HDR_HLG;
            break;
        case V4L2_EXT_VSC_HDR_TYPE_PRIME:
            eHdrMode = KADP_VSC_HDR_PRIME;
            break;
        case V4L2_EXT_VSC_HDR_TYPE_DOLBY_LL:
            eHdrMode = KADP_VSC_HDR_DOLBY_LL;
            break;
        default:
            printk(KERN_ERR "func:%s [error] vsc_v4l2_hdr_control wrong type(%d)\r\n",__FUNCTION__, type);
            return FALSE; 
            break;
    }
    rtk_hal_vsc_dm_open(display);
    rtk_hal_vsc_dm_connect(display, eHdrMode);
    if(display == SLR_MAIN_DISPLAY)
        main_hdr_type = type;
    else
        sub_hdr_type = type;
    return TRUE;
}

static char vsc_v4l2_in_ouput_control(unsigned char display, struct v4l2_ext_vsc_win_region win)
{
    KADP_VSC_ROTATE_T rotate_type;
    KADP_VIDEO_RECT_T  inregion;
    KADP_VIDEO_RECT_T originalInput;
    KADP_VIDEO_RECT_T outregion;
    //original input
    originalInput.x = win.in.res.x;
    originalInput.y = win.in.res.y;
    originalInput.w = win.in.res.w;
    originalInput.h = win.in.res.h;

    //crop input
    inregion.x = win.in.crop.x;
    inregion.y = win.in.crop.y;
    inregion.w = win.in.crop.w;
    inregion.h = win.in.crop.h;

    //output
    outregion.x = win.out.x;
    outregion.y = win.out.y;
    outregion.w = win.out.w;
    outregion.h = win.out.h;

    
    switch(win.rotation)
    {
        case V4L2_EXT_VSC_ROTATE_0:
            rotate_type = KADP_VSC_ROTATE_0;
            break;
        case V4L2_EXT_VSC_ROTATE_90:
            rotate_type = KADP_VSC_ROTATE_90;
            break;
        case V4L2_EXT_VSC_ROTATE_180:
            rotate_type = KADP_VSC_ROTATE_180;
            break;
        case V4L2_EXT_VSC_ROTATE_270:
            rotate_type = KADP_VSC_ROTATE_270;
            break;

        default:
            printk(KERN_ERR "func:%s [error] win.rotation wrong value(%d)\r\n",__FUNCTION__, win.rotation);
            return FALSE; 
            break;
    }
    rtk_hal_vsc_SetInputRegion_OutputRegion((display == SLR_MAIN_DISPLAY)? VIDEO_WID_0 : VIDEO_WID_1, rotate_type, inregion, originalInput, outregion, FALSE, FALSE);
    return TRUE;
}

static char vsc_v4l2_winprop_control(unsigned char display,struct v4l2_ext_vsc_win_prop winprop)
{
	if((winprop.win_mode<V4L2_EXT_VSC_WIN_MODE_NONE)||(winprop.win_mode>V4L2_EXT_VSC_WIN_MODE_PBP))
		return FALSE;
	if((winprop.mem_type<V4L2_EXT_VSC_MEMORY_TYPE_NONE)||(winprop.mem_type>V4L2_EXT_VSC_MEMORY_TYPE_MULTI))
		return FALSE;
	if((winprop.mirror_mode<V4L2_EXT_VSC_MIRROR_MODE_NONE)||(winprop.mirror_mode>V4L2_EXT_VSC_MIRROR_MODE_OFF))
		return FALSE;

	if (display == SLR_SUB_DISPLAY){
		VSC_SET_SUB_WINDOW_MODE_TYPE vsc_set_sub_win_mode;
		vsc_set_sub_win_mode.mode = winprop.win_mode;
		vsc_set_sub_win_mode.memoryUse = winprop.mem_type;
		vsc_set_sub_win_mode.connectType = winprop.mirror_mode;

		rtk_hal_vsc_setwinprop(vsc_set_sub_win_mode);
	}
	return TRUE;
}

static char vsc_v4l2_zorder_control(unsigned char display, struct v4l2_ext_vsc_zorder zorder)
{
	VSC_SET_ZORDER_T zOrderMain;
	VSC_SET_ZORDER_T zOrderSub;
	if(display == SLR_MAIN_DISPLAY) {
		zOrderMain.uAlpha = zorder.alpha;
		zOrderMain.uZorder = zorder.zorder;
		zOrderSub.uAlpha = subzorder.alpha;
		zOrderSub.uZorder = subzorder.zorder;
	} else {
		zOrderSub.uAlpha = zorder.alpha;
		zOrderSub.uZorder = zorder.zorder;
		zOrderMain.uAlpha = mainzorder.alpha;
		zOrderMain.uZorder = mainzorder.zorder;
	}
	rtk_hal_vsc_SetZorder(zOrderMain,zOrderSub);
	return TRUE;
}

static char vsc_v4l2_latencypattern_control(unsigned char display, struct v4l2_ext_vsc_latency_pattern_info latencypattern)
{
	/*need owner added function*/
	VSC_VIDEO_LATENCY_PATTERN_T set_video_latency_pattern;
	
	if(display != SLR_MAIN_DISPLAY) return TRUE;
	if(latencypattern.p == V4L2_EXT_VSC_PATTERN_BLACK)
		set_video_latency_pattern.bPatternType = KADP_VSC_PATTERN_BLACK;
	else
		set_video_latency_pattern.bPatternType = KADP_VSC_PATTERN_WHITE;
	set_video_latency_pattern.overlayWindow.x = latencypattern.r.x;
	set_video_latency_pattern.overlayWindow.y = latencypattern.r.y;
	set_video_latency_pattern.overlayWindow.w = latencypattern.r.w;
	set_video_latency_pattern.overlayWindow.h = latencypattern.r.h;

	set_video_latency_pattern.bOnOff = ((latencypattern.r.w == 0) ||  (latencypattern.r.h == 0))? 0 : 1;
	set_latency_pattern_info(set_video_latency_pattern);
	return TRUE;
}

static char vsc_v4l2_freeze_control(unsigned char display, unsigned int freeze)
{
    rtk_hal_vsc_SetWinFreeze((display == SLR_MAIN_DISPLAY)? VIDEO_WID_0 : VIDEO_WID_1, freeze? TRUE: FALSE);
    return TRUE;
}

static char vsc_v4l2_RGB444_control(__s32 mode)
{
    rtk_hal_vsc_SetRGB444Mode(mode? TRUE: FALSE);
    return TRUE;
}

static char vsc_v4l2_adaptive_stream_control(unsigned char display, __s32 mode)
{
    rtk_hal_vsc_SetAdaptiveStreamEX((display == SLR_MAIN_DISPLAY)? VIDEO_WID_0 : VIDEO_WID_1, mode);
    return TRUE;
}

static char vsc_v4l2_DelayBuffer_control(unsigned char display, UINT8 buffer)
{
    rtk_hal_vsc_SetDelayBuffer((display == SLR_MAIN_DISPLAY)? VIDEO_WID_0 : VIDEO_WID_1, buffer);
    return TRUE;
}

static char vsc_v4l2_pattern_control(unsigned char display, enum v4l2_ext_vsc_pattern pattern)
{ 
    VSC_VIDEO_PATTERN_LOCATION_T pattern_location;
    enum v4l2_ext_vsc_pattern disable_pattern; 
    unsigned char on_off = TRUE;//decide enable or disable
    switch(pattern)
    {
        case V4L2_EXT_VSC_PATTERN_OFF:
            if(display == SLR_MAIN_DISPLAY)
            {
                if(main_vsc_pattern == V4L2_EXT_VSC_PATTERN_OFF)
                {
                    printk(KERN_NOTICE "func:%s main_vsc_pattern already disable\r\n",__FUNCTION__);
                    return TRUE;  
                }
                disable_pattern = main_vsc_pattern;
            }
            else
            {
                if(sub_vsc_pattern == V4L2_EXT_VSC_PATTERN_OFF)
                {
                    printk(KERN_NOTICE "func:%s sub_vsc_pattern already disable\r\n",__FUNCTION__);
                    return TRUE;  
                }
                disable_pattern = sub_vsc_pattern;
            }    
            switch(disable_pattern)
            {
                case V4L2_EXT_VSC_PATTERN_MUX:
                    pattern_location = VSC_VIDEO_PATTERN_MUX;
                    break;
                case V4L2_EXT_VSC_PATTERN_DI_NR:
                    pattern_location = VSC_VIDEO_PATTERN_DI_NR;
                    break;
                case V4L2_EXT_VSC_PATTERN_SCALER:
                    pattern_location = VSC_VIDEO_PATTERN_SCALER;
                    break;
                case V4L2_EXT_VSC_PATTERN_MEMC:
                    pattern_location = VSC_VIDEO_PATTERN_MEMC;
                    break;
                case V4L2_EXT_VSC_PATTERN_DISPLAY:
                    pattern_location = VSC_VIDEO_PATTERN_DISPLAY;
                    break;

                default:
                    printk(KERN_ERR "func:%s [error] disable_pattern wrong value(%d)\r\n",__FUNCTION__, disable_pattern);
                    return FALSE; 
                    break;
            }
            on_off = FALSE;//disable case
            break;
        case V4L2_EXT_VSC_PATTERN_MUX:
            pattern_location = VSC_VIDEO_PATTERN_MUX;
            break;
        case V4L2_EXT_VSC_PATTERN_DI_NR:
            pattern_location = VSC_VIDEO_PATTERN_DI_NR;
            break;
        case V4L2_EXT_VSC_PATTERN_SCALER:
            pattern_location = VSC_VIDEO_PATTERN_SCALER;
            break;
        case V4L2_EXT_VSC_PATTERN_MEMC:
            pattern_location = VSC_VIDEO_PATTERN_MEMC;
            break;
        case V4L2_EXT_VSC_PATTERN_DISPLAY:
            pattern_location = VSC_VIDEO_PATTERN_DISPLAY;
            break;

        default:
            printk(KERN_ERR "func:%s [error] pattern wrong value(%d)\r\n",__FUNCTION__, pattern);
            return FALSE; 
            break;
    }
    
    rtk_hal_vsc_SetPattern(on_off, (display == SLR_MAIN_DISPLAY)? VIDEO_WID_0 : VIDEO_WID_1, pattern_location);
    return TRUE;
}

static char vsc_v4l2_freezeframebuffer_control(unsigned char display, UINT8 freeze)
{
    rtk_hal_vsc_FreezeVideoFrameBuffer((display == SLR_MAIN_DISPLAY)? VIDEO_WID_0 : VIDEO_WID_1, freeze);
    return TRUE;
}

//vsc v4l2 main ioctrl callback
int vsc_v4l2_ioctl_s_ext_ctrls(struct file *file, void *fh, struct v4l2_ext_controls *ctrls)
{//vsc connect, vdo connect/disconnect, set input/output  
    struct v4l2_ext_control controls;
    struct v4l2_ext_vsc_connect_info info;
    struct v4l2_ext_vsc_vdo_mode mode;
    struct v4l2_ext_vsc_win_region win;
	struct v4l2_ext_vsc_win_prop winprop;
	struct v4l2_ext_vsc_zorder zorder;
	struct v4l2_ext_vsc_latency_pattern_info latencypatterninfo;
    bool flag;
	unsigned char display = SLR_MAIN_DISPLAY;

	struct vsc_v4l2_device *vsc_dev = video_drvdata(file);
	//printk(KERN_EMERG "\r\n####func:%s line:%d num:%d ####\r\n",__FUNCTION__,__LINE__,vsc_dev->vfd->num);

	if(vsc_dev->vfd->num == V4L2_EXT_DEV_NO_SCALER0) {
		display = SLR_MAIN_DISPLAY;
	} else if(vsc_dev->vfd->num == V4L2_EXT_DEV_NO_SCALER1){
		display = SLR_SUB_DISPLAY;
	} else {
		display = SLR_MAIN_DISPLAY;
	}
	
    if(!ctrls)
    {
        printk(KERN_ERR "func:%s [error] ctrls is null\r\n",__FUNCTION__);
        return -EFAULT; 
    }
    if(!ctrls->controls)
    {
        printk(KERN_ERR "func:%s [error] ctrls->controls is null\r\n",__FUNCTION__);
        return -EFAULT;  
    }
    memcpy(&controls, ctrls->controls, sizeof(struct v4l2_ext_control));
    if(controls.id == V4L2_CID_EXT_VSC_CONNECT_INFO)
    {//vsc connect
        if(!controls.ptr)
        {
            printk(KERN_ERR "func:%s [error] vsc controls->ptr is null\r\n",__FUNCTION__);
            return -EFAULT;  
        }
        if(copy_from_user((void *)&info, (const void __user *)controls.ptr, sizeof(struct v4l2_ext_vsc_connect_info)))
        {
            printk(KERN_ERR "func:%s [error] vsc copy_from_user controls->ptr fail \r\n",__FUNCTION__);
            return -EFAULT; 
        }

        flag = (info.in.src == V4L2_EXT_VSC_INPUT_SRC_NONE)?false:true;
		printk(KERN_INFO "V4L2_CID_EXT_VSC_CONNECT_INFO display:%d src:%d idx:%d mode:%d\r\n", display, info.in.src, info.in.index, info.out);
        if(!vsc_v4l2_vsc_control(display, flag, info))
        {
            printk(KERN_ERR "func:%s [error] display:%d vsc_v4l2_vsc_connect_control fail \r\n",__FUNCTION__,display);
            return -EFAULT; 
        }
    }
    else if(controls.id == V4L2_CID_EXT_VSC_VDO_MODE)
    {//vdo connect or disconnect
        if(!controls.ptr)
        {
            printk(KERN_ERR "func:%s [error] vdo controls->ptr is null\r\n",__FUNCTION__);
            return -EFAULT;  
        }
        if(copy_from_user((void *)&mode, (const void __user *)controls.ptr, sizeof(struct v4l2_ext_vsc_vdo_mode)))
        {
            printk(KERN_ERR "func:%s [error] vdo copy_from_user controls->ptr fail \r\n",__FUNCTION__);
            return -EFAULT; 
        }
		printk(KERN_INFO "V4L2_CID_EXT_VSC_VDO_MODE display:%d vdo:%d vdec:%d\r\n", display, mode.vdo_port, mode.vdec_port);

        if(!vsc_v4l2_vdo_control(display, mode))
        {
            printk(KERN_ERR "func:%s [error] vsc_v4l2_vdo_control fail \r\n",__FUNCTION__);
            return -EFAULT;
        }
    }
    else if(controls.id == V4L2_CID_EXT_VSC_WIN_REGION)
    {//set input/ourput rotation
        if(!controls.ptr)
        {
            printk(KERN_ERR "func:%s [error] in/output controls->ptr is null\r\n",__FUNCTION__);
            return -EFAULT;  
        }
        if(copy_from_user((void *)&win, (const void __user *)controls.ptr, sizeof(struct v4l2_ext_vsc_win_region)))
        {
            printk(KERN_ERR "func:%s [error] in/output copy_from_user controls->ptr fail \r\n",__FUNCTION__);
            return -EFAULT; 
        }
		printk(KERN_INFO "V4L2_CID_EXT_VSC_WIN_REGION display:%d crop(%d %d %d %d) ori(%d %d) out(%d %d %d %d) rota:%d\r\n", display, win.in.crop.x, win.in.crop.y, win.in.crop.w, win.in.crop.h, 
			win.in.res.w, win.in.res.h, win.out.x, win.out.y, win.out.w, win.out.h, win.rotation);

        if(!vsc_v4l2_in_ouput_control(display, win))
        {
            printk(KERN_ERR "func:%s [error] vsc_v4l2_in_ouput_control fail \r\n",__FUNCTION__);
            return -EFAULT;
        }
		if (display == SLR_MAIN_DISPLAY) {
			memcpy(&main_win_info, &win, sizeof(struct v4l2_ext_vsc_win_region));//save the win info
		} else if (display == SLR_SUB_DISPLAY) {	
			memcpy(&sub_win_info, &win, sizeof(struct v4l2_ext_vsc_win_region));//save the win info
		} 		
    }
    else if(controls.id == V4L2_CID_EXT_VSC_WIN_PROP)
    {//sub window types
        if(!controls.ptr)
        {
            printk(KERN_ERR "func:%s [error] sub prop controls->ptr is null\r\n",__FUNCTION__);
            return -EFAULT;  
        }
        if(copy_from_user((void *)&winprop, (const void __user *)controls.ptr, sizeof(struct v4l2_ext_vsc_win_prop)))
        {
            printk(KERN_ERR "func:%s [error] sub prop copy_from_user controls->ptr fail \r\n",__FUNCTION__);
            return -EFAULT; 
        }
		printk(KERN_INFO "V4L2_CID_EXT_VSC_WIN_PROP display:%d win_mode:%d mir_mode:%d mem:%d\r\n", display, winprop.win_mode, winprop.mirror_mode, winprop.mem_type);

        if(!vsc_v4l2_winprop_control(display, winprop))
        {
            printk(KERN_ERR "func:%s [error] vsc_v4l2_in_ouput_control fail \r\n",__FUNCTION__);
            return -EFAULT;
        }
		if (display == SLR_SUB_DISPLAY)
		 memcpy(&subwin_prop, &winprop, sizeof(struct v4l2_ext_vsc_win_prop));//save the win_prop info
	
    }	
    else if(controls.id == V4L2_CID_EXT_VSC_ZORDER)
    {
        if(!controls.ptr)
        {
            printk(KERN_ERR "func:%s [error] ZORDER controls->ptr is null\r\n",__FUNCTION__);
            return -EFAULT;  
        }
        if(copy_from_user((void *)&zorder, (const void __user *)controls.ptr, sizeof(struct v4l2_ext_vsc_zorder)))
        {
            printk(KERN_ERR "func:%s [error] ZORDER copy_from_user controls->ptr fail \r\n",__FUNCTION__);
            return -EFAULT; 
        }
		printk(KERN_INFO "V4L2_CID_EXT_VSC_ZORDER display:%d zoder:%d alpha:%d\r\n", display, zorder.zorder, zorder.alpha);

        if(!vsc_v4l2_zorder_control(display,zorder))
        {
            printk(KERN_ERR "func:%s [error] vsc_v4l2_zorder_control fail \r\n",__FUNCTION__);
            return -EFAULT;
        }
		if (display == SLR_MAIN_DISPLAY) {
			memcpy(&mainzorder, &zorder, sizeof(struct v4l2_ext_vsc_zorder));
		} else if (display == SLR_SUB_DISPLAY) {	
			memcpy(&subzorder, &zorder, sizeof(struct v4l2_ext_vsc_zorder));
		} 
    }
    else if(controls.id == V4L2_CID_EXT_VSC_LATENCY_PATTERN)
    {
        if(!controls.ptr)
        {
            printk(KERN_ERR "func:%s [error] LATENCY_PATTERN controls->ptr is null\r\n",__FUNCTION__);
            return -EFAULT;  
        }
        if(copy_from_user((void *)&latencypatterninfo, (const void __user *)controls.ptr, sizeof(struct v4l2_ext_vsc_latency_pattern_info)))
        {
            printk(KERN_ERR "func:%s [error] LATENCY_PATTERN copy_from_user controls->ptr fail \r\n",__FUNCTION__);
            return -EFAULT; 
        }
		printk(KERN_INFO "V4L2_CID_EXT_VSC_LATENCY_PATTERN display:%d rect(%d %d %d %d) pattern:%d\r\n", display, latencypatterninfo.r.x, latencypatterninfo.r.y, latencypatterninfo.r.w, latencypatterninfo.r.h, latencypatterninfo.p);

        if(!vsc_v4l2_latencypattern_control(display,latencypatterninfo))
        {
            printk(KERN_ERR "func:%s [error] vsc_v4l2_latencypattern_control fail \r\n",__FUNCTION__);
            return -EFAULT;
        }
		if (display == SLR_MAIN_DISPLAY) {
			memcpy(&mainlatencypattern, &latencypatterninfo, sizeof(struct v4l2_ext_vsc_latency_pattern_info));
		} else if (display == SLR_SUB_DISPLAY) {	
			memcpy(&sublatencypattern, &latencypatterninfo, sizeof(struct v4l2_ext_vsc_latency_pattern_info));
		} 
    }	
    else
    {
        printk(KERN_ERR "func:%s [error] controls.id(%d) is wrong\r\n",__FUNCTION__, controls.id);
        return -EFAULT;
    }
        
	return 0;
}

int vsc_v4l2_ioctl_g_ext_ctrls(struct file *file, void *fh, struct v4l2_ext_controls *ctrls)
{//vsc disconnect, get vdo info
    struct v4l2_ext_control controls;
    struct v4l2_ext_vsc_connect_info v4l2_vsc_connect_info;
	struct v4l2_ext_vsc_vdo_mode v4l2_vsc_vdo_mode;
	struct v4l2_ext_vsc_win_region v4l2_vsc_win_region;
	struct v4l2_ext_vsc_win_prop winprop;
	struct v4l2_ext_vsc_zorder zorder;
	struct v4l2_ext_vsc_scaler_ratio scalerratio;
	struct v4l2_ext_vsc_pixel_color_info pixelcolorinfo;

	unsigned char display = SLR_MAIN_DISPLAY;

	struct vsc_v4l2_device *vsc_dev = video_drvdata(file);
	//printk(KERN_EMERG "\r\n####func:%s line:%d num:%d ####\r\n",__FUNCTION__,__LINE__,vsc_dev->vfd->num);

	if(vsc_dev->vfd->num == V4L2_EXT_DEV_NO_SCALER0) {
		display = SLR_MAIN_DISPLAY;
	} else if(vsc_dev->vfd->num == V4L2_EXT_DEV_NO_SCALER1){
		display = SLR_SUB_DISPLAY;
	} else {
		display = SLR_MAIN_DISPLAY;
	}


    if(!ctrls)
    {
        printk(KERN_ERR "func:%s [error] ctrls is null\r\n",__FUNCTION__);
        return -EFAULT;  
    }
    if(!ctrls->controls)
    {
        printk(KERN_ERR "func:%s [error] ctrls->controls is null\r\n",__FUNCTION__);
        return -EFAULT;  
    }
    memcpy(&controls, ctrls->controls, sizeof(struct v4l2_ext_control));
    if(controls.id == V4L2_CID_EXT_VSC_CONNECT_INFO)
    {
        if(!controls.ptr)
        {
            printk(KERN_ERR "func:%s [error] controls->ptr is null\r\n",__FUNCTION__);
            return -EFAULT;  
        }

		if (display == SLR_MAIN_DISPLAY) {
			v4l2_vsc_connect_info.in.src = main_vsc_source;
			v4l2_vsc_connect_info.in.index = main_vsc_source_index;
			v4l2_vsc_connect_info.out = main_vsc_dest_type;
		} else if (display == SLR_SUB_DISPLAY) {	
			v4l2_vsc_connect_info.in.src = sub_vsc_source;
			v4l2_vsc_connect_info.in.index = sub_vsc_source_index;
			v4l2_vsc_connect_info.out = sub_vsc_dest_type;
		} 

        if(copy_to_user(to_user_ptr((controls.ptr)), &v4l2_vsc_connect_info, sizeof(struct v4l2_ext_vsc_connect_info)))
        {
            printk(KERN_ERR "func:%s [error] get vsc info copy_to_user fail \r\n",__FUNCTION__);
            return -EFAULT;
        }
    }
    else if(controls.id == V4L2_CID_EXT_VSC_VDO_MODE)
    {
        if(!controls.ptr)
        {
            printk(KERN_ERR "func:%s [error] controls->ptr is null\r\n",__FUNCTION__);
            return -EFAULT;  
        }
		if (display == SLR_MAIN_DISPLAY) {
			v4l2_vsc_vdo_mode = main_vdo_mode;
		} else if (display == SLR_SUB_DISPLAY) {	
			v4l2_vsc_vdo_mode = sub_vdo_mode;
		} 
        if(copy_to_user(to_user_ptr((controls.ptr)), &v4l2_vsc_vdo_mode, sizeof(struct v4l2_ext_vsc_vdo_mode)))
        {
            printk(KERN_ERR "func:%s [error] get vdo info copy_to_user fail \r\n",__FUNCTION__);
            return -EFAULT; 
        }
        
    }
    else if(controls.id == V4L2_CID_EXT_VSC_WIN_REGION)
    {//set input/ourput rotation
        if(!controls.ptr)
        {
            printk(KERN_ERR "func:%s [error] in/output controls->ptr is null\r\n",__FUNCTION__);
            return -EFAULT;  
        }
		
		if (display == SLR_MAIN_DISPLAY) {
			v4l2_vsc_win_region = main_win_info;
		} else if (display == SLR_SUB_DISPLAY) {	
			v4l2_vsc_win_region = sub_win_info;
		} 	
        if(copy_to_user(to_user_ptr((controls.ptr)), &v4l2_vsc_win_region, sizeof(struct v4l2_ext_vsc_win_region)))
        {
            printk(KERN_ERR "func:%s [error] get in/out info copy_to_user fail \r\n",__FUNCTION__);
            return -EFAULT; 
        }
    }
    else if(controls.id == V4L2_CID_EXT_VSC_WIN_PROP)
    {
        if(!controls.ptr)
        {
            printk(KERN_ERR "func:%s [error] sub prop controls->ptr is null\r\n",__FUNCTION__);
            return -EFAULT;  
        }	
		winprop = subwin_prop;
        if(copy_to_user(to_user_ptr((controls.ptr)), &winprop, sizeof(struct v4l2_ext_vsc_win_prop)))
        {
            printk(KERN_ERR "func:%s [error] get sub prop info copy_to_user fail \r\n",__FUNCTION__);
            return -EFAULT; 
        }
    }	
    else if(controls.id == V4L2_CID_EXT_VSC_ZORDER)
    {
        if(!controls.ptr)
        {
            printk(KERN_ERR "func:%s [error] ZORDER controls->ptr is null\r\n",__FUNCTION__);
            return -EFAULT;  
        }	
		if (display == SLR_MAIN_DISPLAY) {
			zorder = mainzorder;
		} else if (display == SLR_SUB_DISPLAY) {	
			zorder = subzorder;
		} 		
		
        if(copy_to_user(to_user_ptr((controls.ptr)), &zorder, sizeof(struct v4l2_ext_vsc_zorder)))
        {
            printk(KERN_ERR "func:%s [error] get ZORDER info copy_to_user fail \r\n",__FUNCTION__);
            return -EFAULT; 
        }
    }
	else if(controls.id == V4L2_CID_EXT_VSC_LIMITED_WIN_RATIO)
    {
        if(!controls.ptr)
        {
            printk(KERN_ERR "func:%s [error] LIMITED_WIN_RATIO controls->ptr is null\r\n",__FUNCTION__);
            return -EFAULT;  
        }	
		/*main and sub all sopport scale down and up*/
		if (display == SLR_MAIN_DISPLAY) {
			scalerratio.h_scaledown_ratio = true;
			scalerratio.v_scaledown_ratio = true;
			scalerratio.h_scaleup_ratio = true;
			scalerratio.v_scaleup_ratio = true;
		
		} else if (display == SLR_SUB_DISPLAY) {	
			scalerratio.h_scaledown_ratio = true;
			scalerratio.v_scaledown_ratio = true;
			scalerratio.h_scaleup_ratio = true;
			scalerratio.v_scaleup_ratio = true;
		}
		printk(KERN_INFO "V4L2_CID_EXT_VSC_LIMITED_WIN_RATIO display:%d\r\n", display);

        if(copy_to_user(to_user_ptr((controls.ptr)), &scalerratio, sizeof(struct v4l2_ext_vsc_scaler_ratio)))
        {
            printk(KERN_ERR "func:%s [error] get LIMITED_WIN_RATIO info copy_to_user fail \r\n",__FUNCTION__);
            return -EFAULT; 
        }
    }	
	else if(controls.id == V4L2_CID_EXT_VSC_READ_FRAME_BUFFER_INFO) 
    {
		VIDEO_RECT_T kernelInregion = {0, 0, 0, 0};
		KADP_VIDEO_DDI_PIXEL_STANDARD_COLOR_T *kernelPRead;
		KADP_VIDEO_DDI_COLOR_STANDARD_T Color_standard ;
		KADP_VIDEO_DDI_PIXEL_COLOR_FORMAT_T PixelColorFormat;
		    	
        if(!controls.ptr)
        {
            printk(KERN_ERR "func:%s [error] READ_FRAME_BUFFER controls->ptr is null\r\n",__FUNCTION__);
            return -EFAULT;  
        }
        if(copy_from_user((void *)&pixelcolorinfo, (const void __user *)controls.ptr, sizeof(struct v4l2_ext_vsc_pixel_color_info)))
        {
            printk(KERN_ERR "func:%s [error] READ_FRAME_BUFFER copy_from_user controls->ptr fail \r\n",__FUNCTION__);
            return -EFAULT; 
        }	
		kernelPRead = (KADP_VIDEO_DDI_PIXEL_STANDARD_COLOR_T *)dvr_malloc_specific(kernelInregion.w * kernelInregion.h * sizeof(KADP_VIDEO_DDI_PIXEL_STANDARD_COLOR_T), GFP_DCU1_FIRST);
		Color_standard = KADP_VIDEO_DDI_COLOR_STANDARD_YUV;
		PixelColorFormat = KADP_VIDEO_DDI_PIXEL_8BIT;
		rtk_hal_vsc_ReadVideoFrameBuffer(display, &kernelInregion, kernelPRead, &Color_standard, &PixelColorFormat);
/* marked for struct v4l2_ext_vsc_pixel_color_info no address for format and depth		
		if(copy_to_user(to_user_ptr(readaction.pcolor_standard), (void *)&Color_standard, sizeof(KADP_VIDEO_DDI_COLOR_STANDARD_T)))
		{
			retval = -EFAULT;
			rtd_printk(KERN_DEBUG, TAG_NAME_VSC, "scaler vsc ioctl code=VSC_IOC_READ_VIDEOFRAMEBUFFER pcolor_standard copy_to_user failed!!!!!!!!!!!!!!!\n");
		}
		
		if(copy_to_user(to_user_ptr(readaction.ppixelcolorformat), (void *)&PixelColorFormat, sizeof(KADP_VIDEO_DDI_PIXEL_COLOR_FORMAT_T)))
		{
			retval = -EFAULT;
			rtd_printk(KERN_DEBUG, TAG_NAME_VSC, "scaler vsc ioctl code=VSC_IOC_READ_VIDEOFRAMEBUFFER ppixelcolorformat copy_to_user failed!!!!!!!!!!!!!!!\n");
		}
*/		
		printk(KERN_INFO "V4L2_CID_EXT_VSC_READ_FRAME_BUFFER_INFO \r\n");

		if(copy_to_user(to_user_ptr(pixelcolorinfo.p_data), (void *)kernelPRead, kernelInregion.w * kernelInregion.h * sizeof(KADP_VIDEO_DDI_PIXEL_STANDARD_COLOR_T)))
		{
            printk(KERN_ERR "func:%s [error] READ_FRAME_BUFFER copy_to_user controls->ptr fail \r\n",__FUNCTION__);
			dvr_free((void *)kernelPRead);
			return -EFAULT; 
        }
		dvr_free((void *)kernelPRead);

    }
	else if(controls.id == V4L2_CID_EXT_VSC_ACTIVE_WIN_INFO)
    {
    	struct v4l2_ext_vsc_active_win_info info = {{0}, {0}};
        if(!controls.ptr)
        {
            printk(KERN_ERR "func:%s [error] controls->ptr is null\r\n",__FUNCTION__);
            return -EFAULT;  
        }
		if (display == SLR_MAIN_DISPLAY) {
			unsigned long flags;//for spin_lock_irqsave
			spin_lock_irqsave(&Main_BBD_INFO_Spinlock, flags);
			info.original.w = main_bbd_active_info.original.w;
			info.original.h = main_bbd_active_info.original.h;
			info.active.w = main_bbd_active_info.active.w;
			info.active.h = main_bbd_active_info.active.h;
			info.active.x = main_bbd_active_info.active.x;
			info.active.y = main_bbd_active_info.active.y;
			spin_unlock_irqrestore(&Main_BBD_INFO_Spinlock, flags);

		}
		else
		{
			unsigned long flags;//for spin_lock_irqsave
			spin_lock_irqsave(&Sub_BBD_INFO_Spinlock, flags);
			info.original.w = sub_bbd_active_info.original.w;
			info.original.h = sub_bbd_active_info.original.h;
			info.active.w = sub_bbd_active_info.active.w;
			info.active.h = sub_bbd_active_info.active.h;
			info.active.x = sub_bbd_active_info.active.x;
			info.active.y = sub_bbd_active_info.active.y;
			spin_unlock_irqrestore(&Sub_BBD_INFO_Spinlock, flags);
		}
		
		if(copy_to_user(to_user_ptr((controls.ptr)), &info, sizeof(struct v4l2_ext_vsc_active_win_info)))
        {
            printk(KERN_ERR "func:%s [error] get main_bbd_active_info copy_to_user fail \r\n",__FUNCTION__);
            return -EFAULT; 
        }
		printk(KERN_INFO "#### V4L2_CID_EXT_VSC_ACTIVE_WIN_INFO display:%d size:(%d %d %d %d)####\r\n", display, info.active.x, info.active.y, info.active.w, info.active.h);
    }
    return 0;
}

int vsc_v4l2_ioctl_s_ctrl(struct file *file, void *fh, struct v4l2_control *ctrl)
{//set winblank. set dm connect
	unsigned char display = SLR_MAIN_DISPLAY;

	struct vsc_v4l2_device *vsc_dev = video_drvdata(file);
	//printk(KERN_EMERG "\r\n####func:%s line:%d num:%d ####\r\n",__FUNCTION__,__LINE__,vsc_dev->vfd->num);

	if(vsc_dev->vfd->num == V4L2_EXT_DEV_NO_SCALER0) {
		display = SLR_MAIN_DISPLAY;
	} else if(vsc_dev->vfd->num == V4L2_EXT_DEV_NO_SCALER1){
		display = SLR_SUB_DISPLAY;
	} else {
		display = SLR_MAIN_DISPLAY;
	}


    if(!ctrl)
    {
        printk(KERN_ERR "func:%s [error] ctrl is null\r\n",__FUNCTION__);
        return -EFAULT;  
    }
    if(ctrl->id == V4L2_CID_BG_COLOR)
    {
    	printk(KERN_INFO "V4L2_CID_BG_COLOR display:%d color:%d\r\n", display, ctrl->value);
        if(!vsc_v4l2_winblank_ctrl(display, (enum v4l2_ext_vsc_win_color)ctrl->value))
        {
            printk(KERN_ERR "func:%s [error] vsc_v4l2_winblank_ctrl fail \r\n",__FUNCTION__);
            return -EFAULT; 
        }
    }
    else if(ctrl->id == V4L2_CID_EXT_VSC_HDR_TYPE) 
    {
    	printk(KERN_INFO "V4L2_CID_EXT_VSC_HDR_TYPE display:%d type:%d\r\n", display, ctrl->value);
        if(!vsc_v4l2_hdr_control(display, (enum v4l2_ext_vsc_hdr_type)ctrl->value))
        {
            printk(KERN_ERR "func:%s [error] vsc_v4l2_hdr_control fail \r\n",__FUNCTION__);
            return -EFAULT; 
        }
    }
    else if(ctrl->id == V4L2_CID_EXT_VSC_FREEZE) 
    {
    	printk(KERN_INFO "V4L2_CID_EXT_VSC_FREEZE display:%d freeze:%d\r\n", display, ctrl->value);
        vsc_v4l2_freeze_control(display, (unsigned int)ctrl->value);
		if (display == SLR_MAIN_DISPLAY) {
			main_freeze_status = ctrl->value; 
		} else if (display == SLR_SUB_DISPLAY) {	
			sub_freeze_status = ctrl->value; 
		} 
    }
    else if(ctrl->id == V4L2_CID_EXT_VSC_RGB444) 
    {
    	printk(KERN_INFO "V4L2_CID_EXT_VSC_RGB444 display:%d RGB444:%d\r\n", display, ctrl->value);
        vsc_v4l2_RGB444_control(ctrl->value);
	 HAL_VPQ_MEMC_SetRGBYUVMode(ctrl->value);
        rgb444_status = ctrl->value; 
    }
    else if(ctrl->id == V4L2_CID_EXT_VSC_ADAPTIVE_STREAM) 
    {
    	printk(KERN_INFO "V4L2_CID_EXT_VSC_ADAPTIVE_STREAM display:%d flag:%d\r\n", display, ctrl->value);
        vsc_v4l2_adaptive_stream_control(display, ctrl->value);
        
		if (display == SLR_MAIN_DISPLAY) {
			main_adaptive_stream_status = ctrl->value; 
		} else if (display == SLR_SUB_DISPLAY) {	
			sub_adaptive_stream_status = ctrl->value; 
		} 
    }
    else if(ctrl->id == V4L2_CID_EXT_VSC_FRAME_DELAY) 
    {
    	printk(KERN_INFO "V4L2_CID_EXT_VSC_FRAME_DELAY display:%d buffer:%d\r\n", display, ctrl->value);
        vsc_v4l2_DelayBuffer_control(display, (UINT8)ctrl->value);
        
		if (display == SLR_MAIN_DISPLAY) {
			main_frame_delay_buffer = ctrl->value; 
		} else if (display == SLR_SUB_DISPLAY) {	
			sub_frame_delay_buffer = ctrl->value; 
		} 		
    }
    else if(ctrl->id == V4L2_CID_EXT_VSC_PATTERN) 
    {
    	printk(KERN_INFO "V4L2_CID_EXT_VSC_PATTERN display:%d pattern:%d\r\n", display, ctrl->value);
        if(!vsc_v4l2_pattern_control(display, (enum v4l2_ext_vsc_pattern)ctrl->value))
        {
            printk(KERN_ERR "func:%s [error] vsc_v4l2_pattern_control fail \r\n",__FUNCTION__);
            return -EFAULT; 
        }
		if (display == SLR_MAIN_DISPLAY) {
			main_vsc_pattern = (enum v4l2_ext_vsc_pattern)ctrl->value;
		} else if (display == SLR_SUB_DISPLAY) {	
			sub_vsc_pattern = (enum v4l2_ext_vsc_pattern)ctrl->value;
		} 	
    }
	else if(ctrl->id == V4L2_CID_EXT_VSC_FREEZE_FRAME_BUFFER) 
    {
    	printk(KERN_INFO "V4L2_CID_EXT_VSC_FREEZE_FRAME_BUFFER display:%d buffer:%d\r\n", display, ctrl->value);
        if(!vsc_v4l2_freezeframebuffer_control(display, (UINT8)ctrl->value))
        {
            printk(KERN_ERR "func:%s [error] vsc_v4l2_freezeframebuffer_control fail \r\n",__FUNCTION__);
            return -EFAULT; 
        }
		if (display == SLR_MAIN_DISPLAY) {
			main_freezeframebuffer_status = ctrl->value;
		} else if (display == SLR_SUB_DISPLAY) {	
			sub_freezeframebuffer_status = ctrl->value;
		} 	
    }
	else if(ctrl->id == V4L2_CID_EXT_VSC_OCCUPATION_SUB_SCALER) 
    {
    	printk(KERN_INFO "V4L2_CID_EXT_VSC_OCCUPATION_SUB_SCALER display:%d flag:%d\r\n", display, ctrl->value);
    	if(display == SLR_MAIN_DISPLAY)
    	{
			if(ctrl->value)    
				set_HFR_mode(1);
			else
				set_HFR_mode(0);
    	}
    }

    return 0;
}

int vsc_v4l2_ioctl_g_ctrl(struct file *file, void *fh, struct v4l2_control *ctrl)
{//get winblank, get dm type
	unsigned char display = SLR_MAIN_DISPLAY;

	struct vsc_v4l2_device *vsc_dev = video_drvdata(file);
	//printk(KERN_EMERG "\r\n####func:%s line:%d num:%d ####\r\n",__FUNCTION__,__LINE__,vsc_dev->vfd->num);

	if(vsc_dev->vfd->num == V4L2_EXT_DEV_NO_SCALER0) {
		display = SLR_MAIN_DISPLAY;
	} else if(vsc_dev->vfd->num == V4L2_EXT_DEV_NO_SCALER1){
		display = SLR_SUB_DISPLAY;
	} else {
		display = SLR_MAIN_DISPLAY;
	}


    if(!ctrl){
        printk(KERN_ERR "#####[%s(%d)][error] ctrl is null\r\n",__FUNCTION__,__LINE__);
        return -EFAULT;  
    }

	if(display == SLR_MAIN_DISPLAY) {
	    if(ctrl->id == V4L2_CID_BG_COLOR){
	        ctrl->value = (__s32)main_blank_color;
	        printk(KERN_DEBUG "#####[%s(%d)] display:%d V4L2_CID_BG_COLOR \r\n",__FUNCTION__,__LINE__,display);
	    }else if(ctrl->id == V4L2_CID_EXT_VSC_HDR_TYPE){
	        ctrl->value = (__s32)main_hdr_type;
	        printk(KERN_DEBUG "#####[%s(%d)] display:%d V4L2_CID_EXT_VSC_HDR_TYPE \r\n",__FUNCTION__,__LINE__,display);
	    }else if(ctrl->id == V4L2_CID_EXT_VSC_FREEZE){
	        ctrl->value = main_freeze_status;
	        printk(KERN_DEBUG "#####[%s(%d)] display:%d V4L2_CID_EXT_VSC_FREEZE \r\n",__FUNCTION__,__LINE__,display);
	    }else if(ctrl->id == V4L2_CID_EXT_VSC_RGB444){
	        ctrl->value = rgb444_status;
	        printk(KERN_DEBUG "#####[%s(%d)] display:%d V4L2_CID_EXT_VSC_RGB444 \r\n",__FUNCTION__,__LINE__,display);
	    }else if(ctrl->id == V4L2_CID_EXT_VSC_ADAPTIVE_STREAM){
	        ctrl->value = main_adaptive_stream_status;
	        printk(KERN_DEBUG "#####[%s(%d)] display:%d V4L2_CID_EXT_VSC_ADAPTIVE_STREAM \r\n",__FUNCTION__,__LINE__,display);
	    }else if(ctrl->id == V4L2_CID_EXT_VSC_FRAME_DELAY){
	        ctrl->value = main_frame_delay_buffer;
	        printk(KERN_DEBUG "#####[%s(%d)] display:%d V4L2_CID_EXT_VSC_FRAME_DELAY \r\n",__FUNCTION__,__LINE__,display);
	    }else if(ctrl->id == V4L2_CID_EXT_VSC_PATTERN){
	        ctrl->value = (__s32)main_vsc_pattern;
	        printk(KERN_DEBUG "#####[%s(%d)] display:%d V4L2_CID_EXT_VSC_PATTERN \r\n",__FUNCTION__,__LINE__,display);
	    } else if(ctrl->id == V4L2_CID_EXT_VSC_FREEZE_FRAME_BUFFER){
	        ctrl->value = main_freezeframebuffer_status;
	        printk(KERN_DEBUG "#####[%s(%d)] display:%d V4L2_CID_EXT_VSC_FREEZE_FRAME_BUFFER \r\n",__FUNCTION__,__LINE__,display);
	    } else if(ctrl->id == V4L2_CID_EXT_VSC_OCCUPATION_SUB_SCALER) {
	    	ctrl->value = get_HFR_mode()? 1 : 0;
	    }
	} else if (display == SLR_SUB_DISPLAY) {
		if(ctrl->id == V4L2_CID_BG_COLOR){
			ctrl->value = (__s32)sub_blank_color;
			printk(KERN_DEBUG "#####[%s(%d)] display:%d V4L2_CID_BG_COLOR \r\n",__FUNCTION__,__LINE__,display);
		}else if(ctrl->id == V4L2_CID_EXT_VSC_HDR_TYPE){
			ctrl->value = (__s32)sub_hdr_type;
			printk(KERN_DEBUG "#####[%s(%d)] display:%d V4L2_CID_EXT_VSC_HDR_TYPE \r\n",__FUNCTION__,__LINE__,display);
		}else if(ctrl->id == V4L2_CID_EXT_VSC_FREEZE){
			ctrl->value = sub_freeze_status;
			printk(KERN_DEBUG "#####[%s(%d)] display:%d V4L2_CID_EXT_VSC_FREEZE \r\n",__FUNCTION__,__LINE__,display);
		}else if(ctrl->id == V4L2_CID_EXT_VSC_RGB444){
			ctrl->value = rgb444_status;
			printk(KERN_DEBUG "#####[%s(%d)] display:%d V4L2_CID_EXT_VSC_RGB444 \r\n",__FUNCTION__,__LINE__,display);
		}else if(ctrl->id == V4L2_CID_EXT_VSC_ADAPTIVE_STREAM){
			ctrl->value = sub_adaptive_stream_status;
			printk(KERN_DEBUG "#####[%s(%d)] display:%d V4L2_CID_EXT_VSC_ADAPTIVE_STREAM \r\n",__FUNCTION__,__LINE__,display);
		}else if(ctrl->id == V4L2_CID_EXT_VSC_FRAME_DELAY){
			ctrl->value = sub_frame_delay_buffer;
			printk(KERN_DEBUG "#####[%s(%d)] display:%d V4L2_CID_EXT_VSC_FRAME_DELAY \r\n",__FUNCTION__,__LINE__,display);
		}else if(ctrl->id == V4L2_CID_EXT_VSC_PATTERN){
			ctrl->value = (__s32)sub_vsc_pattern;
			printk(KERN_DEBUG "#####[%s(%d)] display:%d V4L2_CID_EXT_VSC_PATTERN \r\n",__FUNCTION__,__LINE__,display);
		}else if(ctrl->id == V4L2_CID_EXT_VSC_FREEZE_FRAME_BUFFER){
	        ctrl->value = sub_freezeframebuffer_status;
	        printk(KERN_DEBUG "#####[%s(%d)] display:%d V4L2_CID_EXT_VSC_FREEZE_FRAME_BUFFER \r\n",__FUNCTION__,__LINE__,display);
	    }else if(ctrl->id == V4L2_CID_EXT_VSC_OCCUPATION_SUB_SCALER) {
	    	ctrl->value = get_HFR_mode()? 1 : 0;
	    }
	}
    return 0;
}


//event callback
static unsigned char main_muteoff_event_subscribe = FALSE;
static unsigned char sub_muteoff_event_subscribe = FALSE;
static unsigned char main_activewin_event_subscribe = FALSE;
static unsigned char sub_activewin_event_subscribe = FALSE;
static unsigned char main_win_apply_done_event_subscribe = FALSE;
static unsigned char sub_win_apply_done_event_subscribe = FALSE;
extern void scalerVIP_Set_BlackDetection_EN(unsigned char bEnable_main, unsigned char bEnable_sub);

int vsc_v4l2_main_subscribe_event(struct v4l2_fh *fh, const struct v4l2_event_subscription *sub)
{
	int ret;
	if(sub->type == V4L2_EVENT_CTRL)
	{
		if(sub->id == V4L2_CID_EXT_VSC_SUBSCRIBE_MUTE_OFF)
		{
		   printk(KERN_INFO "V4L2_CID_EXT_VSC_SUBSCRIBE_MUTE_OFF main path subscribe\r\n");
		   main_muteoff_event_subscribe = TRUE;
		}
		else if(sub->id == V4L2_CID_EXT_VSC_SUBSCRIBE_ACTIVE_WIN)
		{
			printk(KERN_INFO "V4L2_CID_EXT_VSC_SUBSCRIBE_ACTIVE_WIN main path subscribe\r\n");
			main_activewin_event_subscribe = TRUE;
			SET_BBD_STAGE(SLR_MAIN_DISPLAY, BBD_FUNCTION_REQ_ENABLE);
			scalerVIP_Set_BlackDetection_EN(main_activewin_event_subscribe,sub_activewin_event_subscribe);
		}
		else if(sub->id == V4L2_CID_EXT_VSC_SUBSCRIBE_APPLYING_DONE_WINDOW_REGION)
		{
		   printk(KERN_INFO "V4L2_CID_EXT_VSC_SUBSCRIBE_APPLYING_DONE_WINDOW_REGION main path subscribe\r\n");
		   main_win_apply_done_event_subscribe = TRUE;
		   scaler_vsc_set_window_callback_lowdelay_mode(main_win_apply_done_event_subscribe);
		}
		else
		{
			printk(KERN_ERR "func:%s [error] sub->id[%d] fail \r\n",__FUNCTION__, sub->id);
			return -EFAULT; 
		}
	}
	else
	{
		printk(KERN_ERR "func:%s [error] sub->type[%d] fail \r\n",__FUNCTION__, sub->type);
		return -EFAULT;  
	}
	ret = v4l2_event_subscribe(fh, sub, 0, NULL);
	printk(KERN_NOTICE "func:%s result:%d id:%x \r\n",__FUNCTION__, ret, sub->id);

	return ret;

}

int vsc_v4l2_main_event_unsubscribe(struct v4l2_fh *fh, const struct v4l2_event_subscription *sub)
{
	int ret;
	if(sub->type == V4L2_EVENT_CTRL)
	{
		if(sub->id == V4L2_CID_EXT_VSC_SUBSCRIBE_MUTE_OFF)
		{
			printk(KERN_INFO "V4L2_CID_EXT_VSC_CLEAR_SUBSCRIBE_MUTE_OFF main path unsubscribe\r\n");
			main_muteoff_event_subscribe = FALSE;
		}
		else if(sub->id == V4L2_CID_EXT_VSC_SUBSCRIBE_ACTIVE_WIN)
		{
			printk(KERN_INFO "V4L2_CID_EXT_VSC_CLEAR_SUBSCRIBE_ACTIVE_WIN main path unsubscribe\r\n");
			main_activewin_event_subscribe = FALSE;
			Set_poll_event(SLR_MAIN_DISPLAY, 1, ACTIVE_SIZE_EVENT);
			SET_BBD_STAGE(SLR_MAIN_DISPLAY, BBD_FUNCTION_DONE);
			scalerVIP_Set_BlackDetection_EN(main_activewin_event_subscribe,sub_activewin_event_subscribe);
		}
		else if(sub->id == V4L2_CID_EXT_VSC_SUBSCRIBE_APPLYING_DONE_WINDOW_REGION)
		{
			printk(KERN_INFO "V4L2_CID_EXT_VSC_SUBSCRIBE_APPLYING_DONE_WINDOW_REGION main path unsubscribe\r\n");
			main_win_apply_done_event_subscribe = FALSE;
			Set_poll_event(SLR_MAIN_DISPLAY, 1, ARC_APPLY_DONE_EVENT);
			scaler_vsc_set_window_callback_lowdelay_mode(main_win_apply_done_event_subscribe);
		}
		else
		{
			printk(KERN_ERR "func:%s [error] sub->id[%d] fail \r\n",__FUNCTION__, sub->id);
			return -EFAULT; 
		}
	}
	else
	{
		printk(KERN_ERR "func:%s [error] sub->type[%d] fail \r\n",__FUNCTION__, sub->type);
		return -EFAULT;  
	}
	ret = v4l2_event_unsubscribe(fh, sub);
	printk(KERN_NOTICE "func:%s result:%d id:%x \r\n",__FUNCTION__, ret, sub->id);
	return ret;

}


int vsc_v4l2_sub_subscribe_event(struct v4l2_fh *fh, const struct v4l2_event_subscription *sub)
{
	int ret;
	if(sub->type == V4L2_EVENT_CTRL)
	{
		if(sub->id == V4L2_CID_EXT_VSC_SUBSCRIBE_MUTE_OFF)
		{
			printk(KERN_INFO "V4L2_CID_EXT_VSC_SUBSCRIBE_MUTE_OFF sub path subscribe\r\n");
			sub_muteoff_event_subscribe = TRUE;
		}
		else if(sub->id == V4L2_CID_EXT_VSC_SUBSCRIBE_ACTIVE_WIN)
		{
			printk(KERN_INFO "V4L2_CID_EXT_VSC_SUBSCRIBE_ACTIVE_WIN sub path subscribe\r\n");
			sub_activewin_event_subscribe = TRUE;
			SET_BBD_STAGE(SLR_SUB_DISPLAY, BBD_FUNCTION_REQ_ENABLE);
			scalerVIP_Set_BlackDetection_EN(main_activewin_event_subscribe,sub_activewin_event_subscribe);
		}
		else if(sub->id == V4L2_CID_EXT_VSC_SUBSCRIBE_APPLYING_DONE_WINDOW_REGION)
		{
			printk(KERN_INFO "V4L2_CID_EXT_VSC_SUBSCRIBE_APPLYING_DONE_WINDOW_REGION sub path subscribe\r\n");
			sub_win_apply_done_event_subscribe = TRUE;
		}
		else
		{
			printk(KERN_ERR "func:%s [error] sub->id[%d] fail \r\n",__FUNCTION__, sub->id);
			return -EFAULT; 
		}
	}
	else
	{
		printk(KERN_ERR "func:%s [error]  sub->type[%d] fail \r\n",__FUNCTION__, sub->type);
		return -EFAULT;  
	}
	ret = v4l2_event_subscribe(fh, sub, 0, NULL);
	printk(KERN_NOTICE "func:%s result:%d id:%x \r\n",__FUNCTION__, ret, sub->id);
	return ret;

}

int vsc_v4l2_sub_event_unsubscribe(struct v4l2_fh *fh, const struct v4l2_event_subscription *sub)
{
	int ret;
	if(sub->type == V4L2_EVENT_CTRL)
	{
		if(sub->id == V4L2_CID_EXT_VSC_SUBSCRIBE_MUTE_OFF)
		{
			printk(KERN_INFO "V4L2_CID_EXT_VSC_SUBSCRIBE_MUTE_OFF sub path unsubscribe\r\n");
			sub_muteoff_event_subscribe = FALSE;
		}
		else if(sub->id == V4L2_CID_EXT_VSC_SUBSCRIBE_ACTIVE_WIN)
		{
			printk(KERN_INFO "V4L2_CID_EXT_VSC_CLEAR_SUBSCRIBE_ACTIVE_WIN sub path unsubscribe\r\n");
			sub_activewin_event_subscribe = FALSE;
			Set_poll_event(SLR_SUB_DISPLAY, 1, ACTIVE_SIZE_EVENT);
			SET_BBD_STAGE(SLR_SUB_DISPLAY, BBD_FUNCTION_DONE);
			scalerVIP_Set_BlackDetection_EN(main_activewin_event_subscribe,sub_activewin_event_subscribe);
		}
		else if(sub->id == V4L2_CID_EXT_VSC_SUBSCRIBE_APPLYING_DONE_WINDOW_REGION)
		{
			printk(KERN_INFO "V4L2_CID_EXT_VSC_SUBSCRIBE_APPLYING_DONE_WINDOW_REGION sub path unsubscribe\r\n");
			sub_win_apply_done_event_subscribe = FALSE;
			Set_poll_event(SLR_SUB_DISPLAY, 1, ARC_APPLY_DONE_EVENT);
		}
		else
		{
			printk(KERN_ERR "func:%s [error] sub->id[%d] fail \r\n",__FUNCTION__, sub->id);
			return -EFAULT; 
		}
	}
	else
	{
		printk(KERN_ERR "func:%s [error] sub->type[%d] fail \r\n",__FUNCTION__, sub->type);
		return -EFAULT;  
	}
	ret = v4l2_event_unsubscribe(fh, sub);
	printk(KERN_NOTICE "func:%s result:%d id:%x \r\n",__FUNCTION__, ret, sub->id);
	return ret;

}


void vsc_v4l2_muteoff_event_wakeup(unsigned char display)
{//mute off event
	extern struct video_device *main_vsc_vfd; 
	extern struct video_device *sub_vsc_vfd;
	struct v4l2_event event;
	if(display == SLR_MAIN_DISPLAY)
	{	
		if(main_muteoff_event_subscribe)
		{
			if(main_vsc_vfd)
			{
				event.type = V4L2_EVENT_CTRL;
				event.id = V4L2_CID_EXT_VSC_SUBSCRIBE_MUTE_OFF;
				v4l2_event_queue(main_vsc_vfd, &event);
			}
			else
			{
				printk(KERN_ERR "func:%s [error] main device null \r\n",__FUNCTION__);
			}
		}
	}
	else
	{
		if(sub_muteoff_event_subscribe)
		{
			if(sub_vsc_vfd)
			{
				event.type = V4L2_EVENT_CTRL;
				event.id = V4L2_CID_EXT_VSC_SUBSCRIBE_MUTE_OFF;
				v4l2_event_queue(sub_vsc_vfd, &event);
			}			
			else
			{
				printk(KERN_ERR "func:%s [error] sub device null \r\n",__FUNCTION__);
			}
		}
	}
}

void vsc_v4l2_win_apply_done_event_wakeup(unsigned char display)
{//mute off event
	extern void update_win_apply_delay_info_for_cb(unsigned char display, KADP_SCALER_WIN_CALLBACK_DELAY_INFO *p_scaler_win_delay_cb_info);
	extern struct video_device *main_vsc_vfd; 
	extern struct video_device *sub_vsc_vfd;
	struct v4l2_event event;
	KADP_SCALER_WIN_CALLBACK_DELAY_INFO scaler_win_delay_cb_info;
	if(display == SLR_MAIN_DISPLAY)
	{	
		if(main_win_apply_done_event_subscribe)
		{

			if(main_vsc_vfd)
			{
				update_win_apply_delay_info_for_cb(SLR_MAIN_DISPLAY, &scaler_win_delay_cb_info);
				event.type = V4L2_EVENT_CTRL;
				event.id = V4L2_CID_EXT_VSC_SUBSCRIBE_APPLYING_DONE_WINDOW_REGION;
				event.u.data[0] = 0;//wid
				
				event.u.data[1] = scaler_win_delay_cb_info.OutputRegion.x & 0xff;//x position lsb
				event.u.data[2] = (scaler_win_delay_cb_info.OutputRegion.x & 0xff00)>>8;//x position msb

				event.u.data[3] = scaler_win_delay_cb_info.OutputRegion.y & 0xff;//y position lsb
				event.u.data[4] = (scaler_win_delay_cb_info.OutputRegion.y & 0xff00)>>8;//y position msb

				event.u.data[5] = scaler_win_delay_cb_info.OutputRegion.w & 0xff;//w size lsb
				event.u.data[6] = (scaler_win_delay_cb_info.OutputRegion.w & 0xff00)>>8;//w size msb

				event.u.data[7] = scaler_win_delay_cb_info.OutputRegion.h & 0xff;//h size lsb
				event.u.data[8] = (scaler_win_delay_cb_info.OutputRegion.h & 0xff00)>>8;//h size msb
				
				event.u.data[9] = scaler_win_delay_cb_info.delayTime & 0xff;//delay ms lsb
				event.u.data[10] = (scaler_win_delay_cb_info.delayTime & 0xff00)>>8;//delay ms msb
				v4l2_event_queue(main_vsc_vfd, &event);
				Set_poll_event(SLR_MAIN_DISPLAY, 0, ARC_APPLY_DONE_EVENT);
			}
			else
			{
				printk(KERN_ERR "func:%s [error] main device null \r\n",__FUNCTION__);
			}
		}
	}
	else
	{
		if(sub_win_apply_done_event_subscribe)
		{
			if(sub_vsc_vfd)
			{
				Set_poll_event(SLR_SUB_DISPLAY, 0, ARC_APPLY_DONE_EVENT);
				event.type = V4L2_EVENT_CTRL;
				event.id = V4L2_CID_EXT_VSC_SUBSCRIBE_APPLYING_DONE_WINDOW_REGION;
				v4l2_event_queue(sub_vsc_vfd, &event);
			}			
			else
			{
				printk(KERN_ERR "func:%s [error] sub device null \r\n",__FUNCTION__);
			}
		}
	}
}

void update_bbd_active_info(unsigned char display, StructRect active_size, StructRect ori_size)
{
	unsigned long flags;//for spin_lock_irqsave
	if(display == SLR_MAIN_DISPLAY)
	{
		spin_lock_irqsave(&Main_BBD_INFO_Spinlock, flags);
		main_bbd_active_info.original.w = ori_size.width;
		main_bbd_active_info.original.h = ori_size.height;
		main_bbd_active_info.active.w = active_size.width;
		main_bbd_active_info.active.h = active_size.height;
		main_bbd_active_info.active.x = active_size.x;
		main_bbd_active_info.active.y = active_size.y;
		spin_unlock_irqrestore(&Main_BBD_INFO_Spinlock, flags);
	}
	else
	{
		spin_lock_irqsave(&Sub_BBD_INFO_Spinlock, flags);
		sub_bbd_active_info.original.w = ori_size.width;
		sub_bbd_active_info.original.h = ori_size.height;
		sub_bbd_active_info.active.w = active_size.width;
		sub_bbd_active_info.active.h = active_size.height;
		sub_bbd_active_info.active.x = active_size.x;
		sub_bbd_active_info.active.y = active_size.y;
		spin_unlock_irqrestore(&Sub_BBD_INFO_Spinlock, flags);
	}
}

void vsc_v4l2_active_win_event_wakeup(unsigned char display, StructRect active_size, StructRect ori_size)
{//active win change event
	extern struct video_device *main_vsc_vfd; 
	extern struct video_device *sub_vsc_vfd;
	struct v4l2_event event;
	if(display == SLR_MAIN_DISPLAY)
	{	
		if(main_activewin_event_subscribe)
		{
			Set_poll_event(SLR_MAIN_DISPLAY, 0, ACTIVE_SIZE_EVENT);
			update_bbd_active_info(SLR_MAIN_DISPLAY, active_size, ori_size);
			if(main_vsc_vfd)
			{
				event.type = V4L2_EVENT_CTRL;
				event.id = V4L2_CID_EXT_VSC_SUBSCRIBE_ACTIVE_WIN;
				v4l2_event_queue(main_vsc_vfd, &event);
				printk(KERN_INFO "#### func:%s main wake up ####\r\n",__FUNCTION__);
			}
			else
			{
				printk(KERN_ERR "func:%s [error] main device null \r\n",__FUNCTION__);
			}
		}
	}
	else
	{
		if(sub_activewin_event_subscribe)
		{
			Set_poll_event(SLR_SUB_DISPLAY, 0, ACTIVE_SIZE_EVENT);
			update_bbd_active_info(SLR_SUB_DISPLAY, active_size, ori_size);
			if(sub_vsc_vfd)
			{
				event.type = V4L2_EVENT_CTRL;
				event.id = V4L2_CID_EXT_VSC_SUBSCRIBE_ACTIVE_WIN;
				v4l2_event_queue(sub_vsc_vfd, &event);
				printk(KERN_INFO "#### func:%s sub wake up ####\r\n",__FUNCTION__);
			}			
			else
			{
				printk(KERN_ERR "func:%s [error] sub device null \r\n",__FUNCTION__);
			}
		}
	}
}


/*-----2 standard control information for implementation-----*/
int vsc_main_v4l2_ioctl_querycap(struct file *file, void *fh, struct v4l2_capability *cap)
{
	char driver_name[16]= {0};
	char cardName[32] = "/dev/video30";
	
	if(PLATFORM_KXL == get_platform())
		strcpy(driver_name, "k6hp");
	else
		strcpy(driver_name, "k6lp");
	
	//rtd_printk(KERN_NOTICE, TAG_NAME, "fun:%s=%d\n",  __FUNCTION__, __LINE__);
	if(!cap)
	{
		printk(KERN_ERR "func:%s [error] ctrl is null\r\n",__FUNCTION__);
        return -EFAULT;  
	}
	memcpy(cap->driver, driver_name, sizeof(char)*16);
	memcpy(cap->card, cardName, sizeof(char)*32);
	
	cap->version = KERNEL_VERSION(4, 4, 3);
	cap->capabilities = (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING | V4L2_CAP_DEVICE_CAPS);
	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE_MPLANE;  /* must set device_caps field */
	
	return 0;
}

/*-----2 standard control information for implementation-----*/
int vsc_sub_v4l2_ioctl_querycap(struct file *file, void *fh, struct v4l2_capability *cap)
{
	char driver_name[16]= {0};
	char cardName[32] = "/dev/video31";
	
	if(PLATFORM_KXL == get_platform())
		strcpy(driver_name, "k6hp");
	else
		strcpy(driver_name, "k6lp");
	
	//rtd_printk(KERN_NOTICE, TAG_NAME, "fun:%s=%d\n",  __FUNCTION__, __LINE__);
	if(!cap)
	{
		printk(KERN_ERR "func:%s [error] ctrl is null\r\n",__FUNCTION__);
        return -EFAULT;  
	}
	memcpy(cap->driver, driver_name, sizeof(char)*16);
	memcpy(cap->card, cardName, sizeof(char)*32);
	
	cap->version = KERNEL_VERSION(4, 4, 3);
	cap->capabilities = (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING | V4L2_CAP_DEVICE_CAPS);
	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE_MPLANE;  /* must set device_caps field */
	
	return 0;
}

unsigned char vsc_get_main_win_apply_done_event_subscribe(void){
	return main_win_apply_done_event_subscribe;
}

