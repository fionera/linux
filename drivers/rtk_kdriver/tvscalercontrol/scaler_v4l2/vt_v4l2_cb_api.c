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
#include <asm/unaligned.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-common.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <mach/rtk_log.h>
#include <mach/rtk_platform.h>

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#define to_user_ptr(x)          compat_ptr((unsigned int)x)
#else
#define to_user_ptr(x)          ((void* __user)(x)) // convert 32 bit value to user pointer
#endif


//common
#include <ioctrl/scaler/vt_cmd_id.h>
#ifdef CONFIG_KDRIVER_USE_NEW_COMMON
#include <scaler/scalerCommon.h>
#else
#include <scalercommon/scalerCommon.h>
#endif

#include <linux/v4l2-ext/v4l2-controls-ext.h>
#include <linux/v4l2-ext/videodev2-ext.h>
#include <tvscalercontrol/scaler_v4l2/vt_v4l2_api.h>

#include <tvscalercontrol/scaler_vtdev.h>

#include <tvscalercontrol/scalerdrv/scalerdisplay.h>
#include <tvscalercontrol/panel/panelapi.h>
#include <linux/videodev2.h>
#include <ioctrl/scaler/vsc_cmd_id.h>


#define VT_CAP_SUPPORT_WIDTH_4K2K		(3840)
#define VT_CAP_SUPPORT_HEIGHT_4K2K		(2160)

#define BUFFERS_NUM  (5)
#define PLANE_NUM	 (2)
#define SCAL_DOWN_LIMIT_HEIGHT 34
#define SCAL_DOWN_LIMIT_WIDTH 60
#define VT_CAP_BUF_CURRENT_NUM 1
#define TAG_NAME "V4L2_VT" //log refine

/*4k and 96 align*/
#define __12KPAGE  0x3000
#define _ALIGN(val, align) (((val) + ((align) - 1)) & ~((align) - 1))

/*-----------------------------------------------------------*/

extern volatile unsigned int vfod_capture_out_W;
extern volatile unsigned int vfod_capture_out_H;
extern volatile unsigned int vfod_capture_location;
extern struct v4l2_ext_vsc_win_region main_win_info;
extern unsigned char IndexOfFreezedVideoFrameBuffer;
extern VT_CAPTURE_CTRL_T CaptureCtrl_VT;

extern unsigned short Scaler_DispGetInputInfoByDisp(unsigned char channel, SLR_INPUT_INFO infoList);

//extern void set_VT_Pixel_Format(VT_CAP_FMT value);
extern VT_CAP_FMT get_VT_Pixel_Format(void);

extern unsigned int get_vt_VtBufferNum(void);
extern void set_vt_VtBufferNum(unsigned int value);
extern void vt_enable_dc2h(unsigned char state);
extern unsigned char get_dc2h_capture_state(void);
//extern unsigned char HAL_VT_GetVideoFrameOutputDeviceFramerate(KADP_VT_VIDEO_WINDOW_TYPE_T videoWindowID, unsigned int *pVideoFrameOutputDeviceFramerate);
//extern unsigned int VT_get_input_wid(VT_CAP_SRC capSrc);
//extern unsigned int VT_get_input_height(VT_CAP_SRC capSrc);
extern unsigned int get_framerateDivide(void);
extern void set_framerateDivide(unsigned int value);
extern unsigned char rtk_hal_vsc_GetOutputRegion(KADP_VIDEO_WID_T wid, KADP_VIDEO_RECT_T * poutregion);
extern unsigned char rtk_hal_vsc_GetInputRegion(KADP_VIDEO_WID_T wid, KADP_VIDEO_RECT_T * pinregion);
extern void set_cap_buffer_size_by_AP(unsigned int usr_width, unsigned int usr_height);
unsigned int get_cap_buffer_Width(void);
unsigned int get_cap_buffer_Height(void);
extern unsigned char HAL_VT_WaitVsync(KADP_VT_VIDEO_WINDOW_TYPE_T videoWindowID);
extern unsigned short Get_VFOD_FrameRate(void);
extern unsigned char HAL_VT_EnableFRCMode(unsigned char bEnableFRC);
extern unsigned char force_enable_two_step_uzu(void);/* get d domain go two pixel mode? */

/*========= refine capture flow ======*/
extern void reset_vt_src_capture_status(void);
//extern unsigned char get_vt_src_cap_status(void);
extern unsigned char do_vt_capture_streamon(void);
extern unsigned char do_vt_reqbufs(unsigned int buf_cnt);
extern unsigned char do_vt_streamoff(void);
extern unsigned char do_vt_dqbuf(unsigned int *pdqbuf_Index);

/*-----3 Extended control information for implementation--reference spec 3.14-3.15--*/

int vt_v4l2_ioctl_s_ext_ctrls(struct file *file, void *fh, struct v4l2_ext_controls *ctrls)
{

	rtd_printk(KERN_NOTICE, TAG_NAME, "fun:%s,id=0x%x\n",  __FUNCTION__,ctrls->controls->id);				
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
	
	if(ctrls->controls->id == V4L2_CID_EXT_CAPTURE_FREEZE_MODE) //stop updating video frame in the capture buffer
	{	
		struct v4l2_ext_capture_freeze_mode mode;
		
		if(!ctrls->controls->ptr)
        {
            printk(KERN_ERR "func:%s [error] controls->ptr is null\r\n",__FUNCTION__);
            return -EFAULT;  
        }

		if(copy_from_user((void *)&mode, (const void __user *)ctrls->controls->ptr, sizeof(struct v4l2_ext_capture_freeze_mode)))
        {
            printk(KERN_ERR "func:%s [error] freeze_mode copy_from_user controls->ptr fail \r\n",__FUNCTION__);
            return -EFAULT; 
        }
		rtd_printk(KERN_NOTICE, TAG_NAME, "plane_index,val [%d,%d]\n",mode.plane_index, mode.val);
		vt_enable_dc2h(mode.val);
		
	}
	else if(ctrls->controls->id == V4L2_CID_EXT_CAPTURE_PLANE_PROP)
	{
		struct v4l2_ext_capture_plane_prop plane_prop;
		if(!ctrls->controls->ptr)
        {
            printk(KERN_ERR "func:%s [error] controls->ptr is null\r\n",__FUNCTION__);
            return -EFAULT;  
        }

		if(copy_from_user((void *)&plane_prop, (const void __user *)ctrls->controls->ptr, sizeof(struct v4l2_ext_capture_plane_prop)))
        {
            printk(KERN_ERR "func:%s [error] set plane property copy_from_user controls->ptr fail \r\n",__FUNCTION__);
            return -EFAULT; 
        }
		
		rtd_printk(KERN_NOTICE, TAG_NAME, "capture location, plane_w, plane_h, buf_count[%d, %d, %d, %d]\n",plane_prop.l, plane_prop.plane.w, plane_prop.plane.h, plane_prop.buf_count);

		if((plane_prop.l != V4L2_EXT_CAPTURE_SCALER_INPUT) && (plane_prop.l != V4L2_EXT_CAPTURE_DISPLAY_OUTPUT) && (plane_prop.l != V4L2_EXT_CAPTURE_SCALER_OUTPUT))
		{
			rtd_printk(KERN_NOTICE, TAG_NAME, "[error]capture location support (0~2),default 2(scaler output),will return over\n");
			
			return -EFAULT;
		}
		vfod_capture_out_W = plane_prop.plane.w;
		vfod_capture_out_H = plane_prop.plane.h;
		vfod_capture_location = plane_prop.l;
		set_vt_VtBufferNum(plane_prop.buf_count);
		//HAL_VT_SetVideoFrameOutputDeviceDumpLocation(KADP_VT_VIDEO_WINDOW_0, (KADP_VT_DUMP_LOCATION_TYPE_T)plane_prop.l);
		
	}
	
	return 0;
}

void get_vt_capability_info(struct v4l2_ext_capture_capability_info *cap)
{
	struct v4l2_ext_video_rect max_reslotion = {0,0,VT_CAP_SUPPORT_WIDTH_4K2K, VT_CAP_SUPPORT_HEIGHT_4K2K};
	cap->max_res = max_reslotion;
	cap->num_video_frame_buffer = get_vt_VtBufferNum();
	cap->scale_up_limit_w = 0;
	cap->scale_up_limit_h = 0;
	
	cap->scale_down_limit_h = SCAL_DOWN_LIMIT_HEIGHT;
	cap->scale_down_limit_w = SCAL_DOWN_LIMIT_WIDTH;
	
	cap->num_plane = V4L2_EXT_CAPTURE_VIDEO_FRAME_BUFFER_PLANE_SEMI_PLANAR; //temp 
		
	if(get_VT_Pixel_Format() == VT_CAP_NV12)
		cap->pixel_format =  V4L2_EXT_CAPTURE_VIDEO_FRAME_BUFFER_PIXEL_FORMAT_YUV420_SEMI_PLANAR; //NV12
	else if(get_VT_Pixel_Format() == VT_CAP_NV16)
		cap->pixel_format =  V4L2_EXT_CAPTURE_VIDEO_FRAME_BUFFER_PIXEL_FORMAT_YUV422_SEMI_PLANAR; //NV16
	else
		rtd_printk(KERN_NOTICE, TAG_NAME, "fun:%s=%d,[warning]only support nv12/nv16\n",  __FUNCTION__, __LINE__);

	cap->flags = (V4L2_EXT_CAPTURE_CAP_SCALE_DOWN | V4L2_EXT_CAPTURE_CAP_DIVIDE_FRAMERATE | V4L2_EXT_CAPTURE_CAP_DISPLAY_VIDEO_DEINTERLACE | V4L2_EXT_CAPTURE_CAP_INPUT_VIDEO_DEINTERLACE);
}

void get_capture_video_win_info(struct v4l2_ext_capture_video_win_info *info)
{
	if (Scaler_DispGetInputInfoByDisp(SLR_MAIN_DISPLAY, SLR_INPUT_STATE) == _MODE_STATE_ACTIVE) {
		struct v4l2_ext_video_rect OrginalVideo_in, VideoSize_out;
		
		if((Scaler_DispGetStatus(SLR_MAIN_DISPLAY, SLR_DISP_INTERLACE)) && (vfod_capture_location == V4L2_EXT_CAPTURE_SCALER_INPUT))
			info->type = V4L2_EXT_CAPTURE_VIDEO_INTERLACED;
		else
			info->type = V4L2_EXT_CAPTURE_VIDEO_PROGRESSIVE;

		memset(&OrginalVideo_in, 0, sizeof(struct v4l2_ext_video_rect));
		memset(&VideoSize_out, 0, sizeof(struct v4l2_ext_video_rect));
		
		rtk_hal_vsc_GetInputRegion(KADP_VT_VIDEO_WINDOW_0, (KADP_VIDEO_RECT_T *)(&OrginalVideo_in)); /* Orginal Video Size */
		rtk_hal_vsc_GetOutputRegion(KADP_VT_VIDEO_WINDOW_0, (KADP_VIDEO_RECT_T *)(&VideoSize_out));	/* Output Video Size */

		info->in = OrginalVideo_in;
		info->out = VideoSize_out;
		/*info->in.x = 0;
		info->in.y = 0;
		
		info->in.w = Scaler_DispGetInputInfoByDisp(KADP_VT_VIDEO_WINDOW_0, SLR_INPUT_IPH_ACT_WID);
		info->in.h = Scaler_DispGetInputInfoByDisp(KADP_VT_VIDEO_WINDOW_0, SLR_INPUT_IPV_ACT_LEN);

		info->out.x = 0;
		info->out.y = 0;
		info->out.w = Scaler_DispGetInputInfoByDisp(KADP_VT_VIDEO_WINDOW_0, SLR_INPUT_DISP_WID);
		info->out.h = Scaler_DispGetInputInfoByDisp(KADP_VT_VIDEO_WINDOW_0, SLR_INPUT_DISP_LEN);*/
	} else {
		info->type = V4L2_EXT_CAPTURE_VIDEO_PROGRESSIVE;
			
		info->in.x = 0; /* if no-signal, it should be 0 */
		info->in.y = 0;
		info->in.w = 0;
		info->in.h = 0;
		//info->in.w = _DISP_WID;
		//info->in.h = _DISP_LEN;

		info->out.x = 0;
		info->out.y = 0;
		info->out.w = _DISP_WID;
		info->out.h = _DISP_LEN;
	}
	info->panel.x = 0;
	info->panel.y = 0;
	info->panel.w = _DISP_WID;
	info->panel.h = _DISP_LEN;
}

int vt_v4l2_ioctl_g_ext_ctrls(struct file *file, void *fh, struct v4l2_ext_controls *ctrls)
{
	struct v4l2_ext_control controls;
	
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
	
	rtd_printk(KERN_DEBUG, TAG_NAME, "fun:%s,id=0x%x\n",  __FUNCTION__,controls.id);
	
	if(controls.id == V4L2_CID_EXT_CAPTURE_CAPABILITY_INFO) /* id 0*/
	{
		struct v4l2_ext_capture_capability_info vt_capability_info;
		memset(&vt_capability_info, 0, sizeof(struct v4l2_ext_capture_capability_info));
		
		if(!controls.ptr)
        {
            printk(KERN_ERR "func:%s [error] controls->ptr is null\r\n",__FUNCTION__);
            return -EFAULT;  
        }
		get_vt_capability_info(&vt_capability_info);
		
		if(copy_to_user(to_user_ptr((controls.ptr)), &vt_capability_info, sizeof(struct v4l2_ext_capture_capability_info)))
        {
            printk(KERN_ERR "func:%s [error] get vt_capability_info copy_to_user fail \r\n",__FUNCTION__);
            return -EFAULT;
        }
		
	}
	else if(controls.id == V4L2_CID_EXT_CAPTURE_PLANE_INFO) /* id=1*/
	{
		struct v4l2_ext_capture_plane_info vt_palne_info;
		memset(&vt_palne_info, 0, sizeof(struct v4l2_ext_capture_plane_info));
		if(!controls.ptr)
        {
            printk(KERN_ERR "func:%s [error] controls->ptr is null\r\n",__FUNCTION__);
            return -EFAULT;  
        }

		vt_palne_info.stride = vfod_capture_out_W; 
		vt_palne_info.plane_region.x = 0;
		vt_palne_info.plane_region.y = 0;
		vt_palne_info.plane_region.w = vfod_capture_out_W;
		vt_palne_info.plane_region.h = vfod_capture_out_H;
		
		if (Scaler_DispGetInputInfoByDisp(SLR_MAIN_DISPLAY, SLR_INPUT_STATE) == _MODE_STATE_ACTIVE) 
		{
			if((TRUE == force_enable_two_step_uzu()) && (BUFFERS_NUM == get_vt_VtBufferNum()))
			{
				vt_palne_info.active_region.x = 0;
				vt_palne_info.active_region.y = 0;
				vt_palne_info.active_region.w = VT_CAP_SUPPORT_WIDTH_4K2K/2;
				vt_palne_info.active_region.h = VT_CAP_SUPPORT_HEIGHT_4K2K/2;
			}
			else
				vt_palne_info.active_region = main_win_info.out;
		} else { /* if no signal, it should be all 0 */
			vt_palne_info.active_region.x = 0;  
			vt_palne_info.active_region.y = 0;
			vt_palne_info.active_region.w = 0;
			vt_palne_info.active_region.h = 0;
		}
		pr_debug("[VT] plane info1:(%d,%d,%d,%d,%d)",vt_palne_info.stride,vt_palne_info.plane_region.x,vt_palne_info.plane_region.y
		,vt_palne_info.plane_region.w,vt_palne_info.plane_region.h);

		pr_debug("[VT] plane info2:(%d,%d,%d,%d)",vt_palne_info.active_region.x,vt_palne_info.active_region.y,vt_palne_info.active_region.w
		,vt_palne_info.active_region.h);

		if(copy_to_user(to_user_ptr((controls.ptr)), &vt_palne_info, sizeof(struct v4l2_ext_capture_plane_info)))
        {
            printk(KERN_ERR "func:%s [error] get vt_palne_info copy_to_user fail \r\n",__FUNCTION__);
            return -EFAULT;
        }
		
	}
	else if(controls.id == V4L2_CID_EXT_CAPTURE_VIDEO_WIN_INFO) /* id=2*/
	{
		struct v4l2_ext_capture_video_win_info video_info;		
		memset(&video_info, 0 , sizeof(struct v4l2_ext_capture_video_win_info));
		
		if(!controls.ptr)
        {
            printk(KERN_ERR "func:%s [error] controls->ptr is null\r\n",__FUNCTION__);
            return -EFAULT;  
        }
		get_capture_video_win_info(&video_info);

		if(copy_to_user(to_user_ptr((controls.ptr)), &video_info, sizeof(struct v4l2_ext_capture_video_win_info)))
        {
            printk(KERN_ERR "func:%s [error] get video_win_info copy_to_user fail \r\n",__FUNCTION__);
            return -EFAULT;
        }
		
	}
	else if(controls.id == V4L2_CID_EXT_CAPTURE_FREEZE_MODE)/*id=4 */ //stop updating video frame in the capture buffer
	{
		struct v4l2_ext_capture_freeze_mode mode;
		memset(&mode, 0, sizeof(struct v4l2_ext_capture_freeze_mode));
		if(!controls.ptr)
        {
            printk(KERN_ERR "func:%s [error] controls->ptr is null\r\n",__FUNCTION__);
            return -EFAULT;  
        }
		memset(&mode, 0, sizeof(struct v4l2_ext_capture_freeze_mode));
		
		mode.plane_index = IndexOfFreezedVideoFrameBuffer;
		mode.val = get_dc2h_capture_state();
		
		if(copy_to_user(to_user_ptr((controls.ptr)), &mode, sizeof(struct v4l2_ext_capture_freeze_mode)))
		{
		    printk(KERN_ERR "func:%s [error] get freeze_mode copy_to_user fail \r\n",__FUNCTION__);
		    return -EFAULT;
		}
		
	}
	else if(controls.id == V4L2_CID_EXT_CAPTURE_PHYSICAL_MEMORY_INFO) /*id=6*/
	{
		unsigned int y_addr = 0;
		unsigned int c_addr = 0;
		struct v4l2_ext_capture_physical_memory_info info;
		memset(&info, 0, sizeof(struct v4l2_ext_capture_physical_memory_info));
		
		if(!controls.ptr)
		{
		    printk(KERN_ERR "func:%s [error] controls->ptr is null\r\n",__FUNCTION__);
		    return -EFAULT;  
		}

		if(copy_from_user((void *)&info, (const void __user *)controls.ptr, sizeof(struct v4l2_ext_capture_physical_memory_info)))
		{
		    printk(KERN_ERR "func:%s [error] get physical_memory copy_from_user controls->ptr fail \r\n",__FUNCTION__);
		    return -EFAULT; 
		}
		
		rtd_printk(KERN_DEBUG, TAG_NAME, "fun:%s=%d,index:%d\n",  __FUNCTION__, __LINE__, info.buf_index);

		if((info.buf_index < 0) || (info.buf_index > (get_vt_VtBufferNum()-1)))
		{
			rtd_printk(KERN_NOTICE, TAG_NAME, "fun:%s=%d, error buf_index\n",  __FUNCTION__, __LINE__);
			return -EFAULT;
		}
		if(info.y == NULL || info.c == NULL)
		{
			rtd_printk(KERN_NOTICE, TAG_NAME, "fun:%s=%d, y/c is null\n",  __FUNCTION__, __LINE__);
			return -EFAULT;
		}
		y_addr = (CaptureCtrl_VT.cap_buffer[info.buf_index].phyaddr);
		c_addr = (CaptureCtrl_VT.cap_buffer[info.buf_index].phyaddr + _ALIGN((get_cap_buffer_Width())* (get_cap_buffer_Height()),__12KPAGE));

		rtd_printk(KERN_DEBUG, TAG_NAME, "y/c:[%x,%x]\n", y_addr, c_addr);	

		if(copy_to_user(to_user_ptr((info.y)), (void *)&y_addr, sizeof(unsigned int)))
		{
	            printk(KERN_ERR "func:%s [error] y addr copy_to_user fail \r\n",__FUNCTION__);
	            return -EFAULT;
		}	
		
		if(copy_to_user(to_user_ptr((info.c)), (void *)&c_addr, sizeof(unsigned int)))
		{
		    printk(KERN_ERR "func:%s [error] c addr copy_to_user fail \r\n",__FUNCTION__);
		    return -EFAULT;
		}
	}
	else if(controls.id == V4L2_CID_EXT_CAPTURE_PLANE_PROP) /*id=3*/
	{
		struct v4l2_ext_capture_plane_prop plane_prop;
		memset(&plane_prop, 0, sizeof(struct v4l2_ext_capture_plane_prop));
		
		if(!controls.ptr)
        {
            printk(KERN_ERR "func:%s [error] controls->ptr is null\r\n",__FUNCTION__);
            return -EFAULT;  
        }
		plane_prop.l = (enum v4l2_ext_capture_location)vfod_capture_location;
		plane_prop.buf_count = get_vt_VtBufferNum();
		plane_prop.plane.x = 0;
		plane_prop.plane.y = 0;
		plane_prop.plane.w = vfod_capture_out_W;
		plane_prop.plane.h = vfod_capture_out_H;
		
		if(copy_to_user(to_user_ptr((controls.ptr)), &plane_prop, sizeof(struct v4l2_ext_capture_plane_prop)))
        {
            printk(KERN_ERR "func:%s [error] get freeze_mode copy_to_user fail \r\n",__FUNCTION__);
            return -EFAULT;
        }
	}

	return 0;
}
int vt_v4l2_ioctl_s_ctrl(struct file *file, void *fh, struct v4l2_control *ctrl)
{

	rtd_printk(KERN_NOTICE, TAG_NAME, "fun:%s=%d,id=0x%x\n", __FUNCTION__, __LINE__, ctrl->id);
	
    if(!ctrl)
    {
        printk(KERN_ERR "func:%s [error] ctrl is null\r\n",__FUNCTION__);
        return -EFAULT;  
    }

	if(ctrl->id == V4L2_CID_EXT_CAPTURE_DONE_USER_PROCESSING)////this command used in VTV
	{
		rtd_printk(KERN_NOTICE, TAG_NAME, "fun:%s=%d,unsupport this cmd\n",  __FUNCTION__, __LINE__);
		return -EFAULT;
	}
	else if(ctrl->id == V4L2_CID_EXT_CAPTURE_DIVIDE_FRAMERATE)
	{
		rtd_printk(KERN_NOTICE, TAG_NAME, "set_framerateDivide=%d\n",ctrl->value);
		set_framerateDivide(ctrl->value);
	}
	return 0;
}

int vt_v4l2_ioctl_g_ctrl(struct file *file, void *fh, struct v4l2_control *ctrl)
{	
	rtd_printk(KERN_NOTICE, TAG_NAME, "fun:%s=%d,id=0x%x\n", __FUNCTION__, __LINE__,ctrl->id);
	
    if(!ctrl)
	{
        printk(KERN_ERR "#####[%s(%d)][error] ctrl is null\r\n",__FUNCTION__,__LINE__);
        return -EFAULT;  
    }
	
	if(ctrl->id == V4L2_CID_EXT_CAPTURE_OUTPUT_FRAMERATE)
	{
		ctrl->value = Get_VFOD_FrameRate();
		
		rtd_printk(KERN_NOTICE, TAG_NAME, "get vfod_framerate=%d\n",ctrl->value);
	}
	else if(ctrl->id == V4L2_CID_EXT_CAPTURE_DIVIDE_FRAMERATE)
	{
		ctrl->value = get_framerateDivide();
		rtd_printk(KERN_NOTICE, TAG_NAME, "get_framerateDivide=%d\n",ctrl->value);
	}
	return 0;
}

int vt_v4l2_ioctl_subscribe_event(struct v4l2_fh *fh, const struct v4l2_event_subscription *sub)
{
	return 0;
}

int vt_v4l2_ioctl_unsubscribe_event(struct v4l2_fh *fh, const struct v4l2_event_subscription *sub)
{
	return 0;
}

/*-----2 standard control information for implementation-----*/
int vt_v4l2_ioctl_querycap(struct file *file, void *fh, struct v4l2_capability *cap)
{
	char driver_name[16]= {0};
	char cardName[32] = "/dev/video60";
	
	if(PLATFORM_KXL == get_platform())
		strcpy(driver_name, "k6hp");
	else
		strcpy(driver_name, "k6lp");
	
	rtd_printk(KERN_NOTICE, TAG_NAME, "%s\n", __FUNCTION__);
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


int vt_v4l2_ioctl_reqbufs(struct file *file, void *fh, struct v4l2_requestbuffers *b)
{ 

	rtd_printk(KERN_NOTICE, TAG_NAME, "fun:%s\n", __FUNCTION__);

	if(!b) 
	{
		printk(KERN_ERR "func:%s [error] pointer is null\r\n",__FUNCTION__);
        return -EFAULT;  
	}

	/*rtd_printk(KERN_NOTICE, TAG_NAME, "buf_cnt,type,memory [%d,%d,%d]\n",b->count, b->type, b->memory);*/

	if(b->count > BUFFERS_NUM)
	{
		pr_notice("fun: %s,line:%d,[Warning] max support allocate 5 buffers\n", __FUNCTION__, __LINE__);
		return -EFAULT;
	}
	if(b->memory != V4L2_MEMORY_MMAP)
	{
		rtd_printk(KERN_NOTICE, TAG_NAME, "fun:%s=%d,[error] only support V4L2_MEMORY_MMAP\n",  __FUNCTION__, __LINE__);
		return -EFAULT;
	}
	if(b->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
	{
		rtd_printk(KERN_NOTICE, TAG_NAME, "fun:%s=%d,[error] only support multi planar\n",  __FUNCTION__, __LINE__);
		return -EFAULT;
	}

	if(b->count == BUFFERS_NUM)
	{
		HAL_VT_EnableFRCMode(FALSE);
	}
	
	if(do_vt_reqbufs(b->count) == FALSE)
		return -EFAULT; 

	return  0;
}
int vt_v4l2_ioctl_querybuf(struct file *file, void *fh, struct v4l2_buffer *b)
{	

	if(!b)
	{		
		printk(KERN_ERR "func:%s [error] pointer is null\r\n",__FUNCTION__);
		return -EFAULT;  
	}

	if(b->index > (get_vt_VtBufferNum()-1))
	{	
		rtd_printk(KERN_NOTICE, TAG_NAME, "%s index %d,invalid index number\n",  __FUNCTION__, b->index);
		return -EFAULT;
	}
	if(b->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
	{
		rtd_printk(KERN_NOTICE, TAG_NAME, "%s type=%d,[error] only support multi planar\n",  __FUNCTION__, b->type);
		return -EFAULT;
	}
	if(b->length != PLANE_NUM)
	{
		rtd_printk(KERN_NOTICE, TAG_NAME, "%s length=%d,[error] only support semi-planar\n",  __FUNCTION__, b->length);
		return -EFAULT;
	}
	if(b->memory != V4L2_MEMORY_MMAP)
	{
		rtd_printk(KERN_NOTICE, TAG_NAME, "fun:%s=%d,[error] only support V4L2_MEMORY_MMAP\n",  __FUNCTION__, __LINE__);
		return -EFAULT;
	}
	rtd_printk(KERN_NOTICE, TAG_NAME, "user querybuf:index[%d]\n",b->index);

	b->m.planes[0].data_offset = 0;
	b->m.planes[0].m.mem_offset = CaptureCtrl_VT.cap_buffer[b->index].phyaddr;
	
	b->m.planes[0].length = (vfod_capture_out_W * vfod_capture_out_H);
	
	b->m.planes[1].data_offset = 0;
	b->m.planes[1].m.mem_offset = (CaptureCtrl_VT.cap_buffer[b->index].phyaddr +  _ALIGN((get_cap_buffer_Width())* (get_cap_buffer_Height()),__12KPAGE));
	
	if (get_VT_Pixel_Format() == VT_CAP_NV12) //yuv420
	{		
		b->m.planes[1].length = (vfod_capture_out_W * vfod_capture_out_H)/2;
	} 
	else if (get_VT_Pixel_Format() == VT_CAP_NV16)  //yuv422
	{
		b->m.planes[1].length = (vfod_capture_out_W * vfod_capture_out_H);		
	}

	return  0;
}

int vt_v4l2_ioctl_qbuf(struct file *file, void *fh, struct v4l2_buffer *b)
{
	if(!b)
	{	
		printk(KERN_ERR "func:%s [error] pointer is null\r\n",__FUNCTION__);
		return -EFAULT;  
	}
	
	if(b->index > (get_vt_VtBufferNum()-1))
	{
		rtd_printk(KERN_NOTICE, TAG_NAME, "fun:%s=%d,invalid index number\n",  __FUNCTION__, __LINE__);
		return -EFAULT;
	}
	if(b->memory != V4L2_MEMORY_MMAP)
	{
		rtd_printk(KERN_NOTICE, TAG_NAME, "fun:%s=%d,[error] only support V4L2_MEMORY_MMAP\n",  __FUNCTION__, __LINE__);
		return -EFAULT;
	}
	if(b->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
	{
		rtd_printk(KERN_NOTICE, TAG_NAME, "fun:%s=%d,[error] only support multi planar\n",  __FUNCTION__, __LINE__);
		return -EFAULT;
	}
	if(b->length != PLANE_NUM)
	{
		rtd_printk(KERN_NOTICE, TAG_NAME, "fun:%s=%d,[error] only support semi-planar\n",  __FUNCTION__, __LINE__);
		return -EFAULT;
	}
	rtd_printk(KERN_DEBUG, TAG_NAME, "qbuf:index=%d\n", b->index);

	return 0;
}

int vt_v4l2_ioctl_dqbuf  (struct file *file, void *fh, struct v4l2_buffer *b)
{
	if(!b)
	{	
		printk(KERN_ERR "func:%s [error] pointer is null\r\n",__FUNCTION__);
		return -EFAULT;  
	}
	if(get_vt_VtBufferNum() == VT_CAP_BUF_CURRENT_NUM)
	{
		rtd_printk(KERN_NOTICE, TAG_NAME, "%s\n",__FUNCTION__);
	}
/* permission denied: for   SVP(secure video path) ,return buffer index:0xFF*/
	if(get_svp_protect_status() == TRUE)
	{
		printk(KERN_ERR "fun:%s, [VT]unsupport VT capture in SVP video\n",__FUNCTION__);
		b->index = 0xFF;
		return	-EACCES;
	}

	if(b->memory != V4L2_MEMORY_MMAP)
	{
		rtd_printk(KERN_NOTICE, TAG_NAME, "fun:%s=%d,[error] only support V4L2_MEMORY_MMAP\n",  __FUNCTION__, __LINE__);
		return -EFAULT;
	}
	if(b->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
	{
		rtd_printk(KERN_NOTICE, TAG_NAME, "fun:%s=%d,[error] only support multi planar\n",  __FUNCTION__, __LINE__);
		return -EFAULT;
	}
	if(b->length != PLANE_NUM)
	{
		rtd_printk(KERN_NOTICE, TAG_NAME, "fun:%s=%d,[error] only support semi-planar\n",  __FUNCTION__, __LINE__);
		return -EFAULT;
	}
	HAL_VT_WaitVsync(KADP_VT_VIDEO_WINDOW_0);

	if(do_vt_dqbuf(&b->index) == FALSE)
	{
		rtd_printk(KERN_NOTICE, TAG_NAME, "EAGAIN:v4l2_ioctl dqbuf error\n");
		return -EAGAIN;
	}
	else
	{
 		rtd_printk(KERN_DEBUG, TAG_NAME, "dq_idx:%d\n",b->index);
		return 0;
 	}
	 			
}

int vt_v4l2_ioctl_streamon(struct file *file, void *fh, enum v4l2_buf_type i)
{	
	if(i != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
	{		
		printk(KERN_EMERG  "func:%s unsupport buffer type %d\n",__FUNCTION__,i);
		return -EFAULT;
	}
	rtd_printk(KERN_NOTICE, TAG_NAME, "%s\n",  __FUNCTION__);
	
	if(do_vt_capture_streamon() == FALSE)
		return -EFAULT;
		
	return 0;
}

int vt_v4l2_ioctl_streamoff(struct file *file, void *fh, enum v4l2_buf_type i)
{
	if(i != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
	{		
		printk(KERN_EMERG  "func:%s unsupport buffer type %d\n",__FUNCTION__,i);
		return -EFAULT;
	}
	rtd_printk(KERN_NOTICE, TAG_NAME, "%s\n",  __FUNCTION__);

	if(do_vt_streamoff() == FALSE)
		return -EFAULT;
	
	return 0;
}

int vt_v4l2_ioctl_s_fmt_vid_cap    (struct file *file, void *fh, struct v4l2_format *f)
{	
	rtd_printk(KERN_NOTICE, TAG_NAME, "%s,fmt.type:%d\n", __FUNCTION__, f->type);
	if(!f)
	{	
		printk(KERN_ERR "func:%s [error] pointer is null\r\n",__FUNCTION__);
		return -EFAULT;  
	}
			
	if(f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
	{
		//pixformat.pixelformat = f->fmt.pix.pixelformat;
		vfod_capture_out_W = f->fmt.pix.width;
		vfod_capture_out_H = f->fmt.pix.height;
					
		rtd_printk(KERN_DEBUG, TAG_NAME, "set capture:wid,height [%d,%d]\n", vfod_capture_out_W, vfod_capture_out_H);
	}
	else if(f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
	{
		//pixformat_mp.pixelformat = f->fmt.pix_mp.pixelformat;
		vfod_capture_out_W = f->fmt.pix_mp.width;
		vfod_capture_out_H = f->fmt.pix_mp.height;	
		rtd_printk(KERN_DEBUG, TAG_NAME, "set capture_mp:wid,height [%d,%d]\n", vfod_capture_out_W, vfod_capture_out_H);
	}
	else
	{
		rtd_printk(KERN_NOTICE, TAG_NAME, "func:%s invalid format->type\n",__FUNCTION__);
		return -EFAULT; 
	}

	set_cap_buffer_size_by_AP(vfod_capture_out_W, vfod_capture_out_H);
	
	return 0;
}

int vt_v4l2_ioctl_g_fmt_vid_cap    (struct file *file, void *fh, struct v4l2_format *f)
{
	enum v4l2_ext_capture_video_frame_buffer_pixel_format pixel_Format;
	rtd_printk(KERN_NOTICE, TAG_NAME, "%s,fmt.type:%d\n", __FUNCTION__, f->type);
	
	if(!f)
	{		
		printk(KERN_ERR "func:%s [error] pointer is null\r\n",__FUNCTION__);
		return -EFAULT;  
	}

	if(get_VT_Pixel_Format() == VT_CAP_NV12)
		pixel_Format = V4L2_EXT_CAPTURE_VIDEO_FRAME_BUFFER_PIXEL_FORMAT_YUV420_SEMI_PLANAR; //NV12
	else if(get_VT_Pixel_Format() == VT_CAP_NV16)
		pixel_Format = V4L2_EXT_CAPTURE_VIDEO_FRAME_BUFFER_PIXEL_FORMAT_YUV422_SEMI_PLANAR; //NV16
	else
	{
		rtd_printk(KERN_NOTICE, TAG_NAME, "fun:%s=%d,[warning]only support nv12 or nv16\n",  __FUNCTION__, __LINE__);
		pixel_Format = V4L2_EXT_CAPTURE_VIDEO_FRAME_BUFFER_PIXEL_FORMAT_YUV420_SEMI_PLANAR; 
	}
	
	if(f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
	{
		f->fmt.pix.pixelformat = pixel_Format;
		f->fmt.pix.width = vfod_capture_out_W;
		f->fmt.pix.height = vfod_capture_out_H;
	}
	else if(f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) //NV16
	{

		f->fmt.pix_mp.pixelformat = pixel_Format;
		f->fmt.pix_mp.width = vfod_capture_out_W;
		f->fmt.pix_mp.height = vfod_capture_out_H;
	}
	else
	{
		rtd_printk(KERN_NOTICE, TAG_NAME, "func:%s invalid format->type\n",__FUNCTION__);
		return -EFAULT; 
	}
		
	return 0;	
}


