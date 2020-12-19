/*
 *      vbi_v4l2_driver.c  -- 
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
#include <linux/wait.h>
#include <linux/version.h>
#include <asm/unaligned.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-common.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <linux/videodev2.h>
#include <linux/kthread.h>
#include <asm/cacheflush.h>
#include <linux/freezer.h>
#include <linux/v4l2-ext/v4l2-controls-ext.h>
#include <linux/v4l2-ext/videodev2-ext.h>
//common
#include <ioctrl/scaler/vfe_cmd_id.h>
#ifdef CONFIG_KDRIVER_USE_NEW_COMMON
#include <scaler/scalerCommon.h>
#else
#include <scalercommon/scalerCommon.h>
#endif

//TVScaler Header file
#include <tvscalercontrol/scaler_v4l2/vbi_v4l2_structure.h>
#include <tvscalercontrol/scaler_v4l2/vbi_v4l2_api.h>
#include <tvscalercontrol/vbi/vbi_slicer_driver.h>
#include <tvscalercontrol/scaler_vfedev.h>

#include <rtk_kdriver/rtk_crt.h>
#include <mach/rtk_log.h>

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#define to_user_ptr(x)          compat_ptr((unsigned int)x)
#else
#define to_user_ptr(x)          ((void* __user)(x)) // convert 32 bit value to user pointer
#endif

unsigned int vbi_vps_buffer = 0;
unsigned int vbi_ttx_buffer = 0;
unsigned int vbi_cc_buffer = 0;
MVD_SUBTITLE_BUF *cc_mvdSubBuf = NULL;
unsigned char *cc_sDataVirAddr;
#define MAX_PASSED_BYTES 48  //this value fix later
#define VBI_VPS_FRAME_BYTES_CUSTOM 15  //for LG

//unsigned int VBI_service = 0; /*tt,cc,vps,wss...*/
struct v4l2_format vbi_format;
unsigned char ttx_slicer_fieldBuff[VBI_TTX_BUF_LENGTH][TTX_PACKET_SIZE_CUSTOMER];
unsigned char vps_slicer_fieldBuff[VBI_VPS_BUF_LENGTH][VBI_VPS_FRAME_BYTES_CUSTOM];
unsigned char cc_slicer_fieldBuff[VBI_CC_BUF_LENGTH][VBI_CC_FRAME_BYTES];

unsigned short vbi_ttx_last_pkt = 0;
unsigned short vbi_vps_last_pkt = 0;
unsigned short vbi_cc_last_pkt = 0;
unsigned short pal_wss_data = 0;
unsigned char ttx_curr_bit2_data;
unsigned char vbi_init_setting = 0;
static struct semaphore VBI_Semaphore;
static unsigned char VBI_STREAM_ENABLE;

#define TAG_VBI_NAME "VBI"

static unsigned int m_subtitleNonCachedAddr = 0;
static unsigned int m_ttxBufNonCachedAddr = 0;
static unsigned int m_vpsBufNonCachedAddr = 0;

extern KADP_VFE_AVD_CC_TTX_STATUS_T Scaler_VBI_GetLGEColorSystem(void);
unsigned char VBI_stream_data_Get_ONOFF(void)
{
	return VBI_STREAM_ENABLE;
}

void VBI_stream_data_Set_ONOFF(unsigned char enable)
{
	 VBI_STREAM_ENABLE=enable;
}

void VBI_flush_buffer_data(unsigned int  type)
{
	if(type == V4L2_SLICED_TELETEXT_B)
	{
		memset(&ttx_slicer_fieldBuff[0][0],0,VBI_TTX_BUF_LENGTH*TTX_PACKET_SIZE_CUSTOMER);
		vbi_ttx_last_pkt = 0;
		ttx_curr_bit2_data=0;
	}
	else if(type == V4L2_SLICED_VPS)
	{
		memset(&vps_slicer_fieldBuff[0][0],0,VBI_VPS_BUF_LENGTH*VBI_VPS_FRAME_BYTES_CUSTOM);
		vbi_vps_last_pkt = 0;
	}
	else if(type ==V4L2_SLICED_CAPTION_525)
	{
		memset(&cc_slicer_fieldBuff[0][0],0,VBI_CC_BUF_LENGTH*VBI_CC_FRAME_BYTES);
		vbi_cc_last_pkt = 0;
	}
	else if(type == V4L2_SLICED_WSS_625)
	{
		pal_wss_data = 0;
	}
	
}

static unsigned short reverseInt16(unsigned short value)
{
    unsigned short b0 = (value & 0x00ff) << 8;
    unsigned short b1 = (value & 0xff00) >> 8;

    return (b0 | b1);
}


//vbi v4l2 ioctrl callback
int vbi_v4l2_ioctl_s_ctrl(struct file *file, void *fh,	struct v4l2_control *ctrl)
{
	if(!ctrl)
	{
	    printk(KERN_ERR "func:%s [error] ctrl is null\r\n",__FUNCTION__);
	    return -EFAULT;  
	}	
	
	rtd_printk(KERN_NOTICE,TAG_VBI_NAME,"%s(%d) ctrl->id = %x,value=%x\n", __func__, __LINE__,ctrl->id,ctrl->value);
	switch(ctrl->id){
		case V4L2_CID_EXT_VBI_FLUSH:
		{
			if (ctrl->value ==V4L2_SLICED_CAPTION_525) {
				VBI_flush_buffer_data(V4L2_SLICED_CAPTION_525);
			} else if (ctrl->value ==V4L2_SLICED_VBI_625) {
				VBI_flush_buffer_data(V4L2_SLICED_TELETEXT_B);
				VBI_flush_buffer_data(V4L2_SLICED_VPS);
				VBI_flush_buffer_data(V4L2_SLICED_WSS_625);
			} else if (ctrl->value ==V4L2_SLICED_TELETEXT_B) {
				VBI_flush_buffer_data(V4L2_SLICED_TELETEXT_B);
			} else if (ctrl->value ==V4L2_SLICED_WSS_625) {
				VBI_flush_buffer_data(V4L2_SLICED_WSS_625);
			} else if (ctrl->value ==V4L2_SLICED_VPS) {
				VBI_flush_buffer_data(V4L2_SLICED_VPS);
			}
			break;
		}
		default:
	        break;
	}

	return 0;
}

int vbi_v4l2_ioctl_g_ctrl(struct file *file, void *fh, struct v4l2_control *ctrl)
{
	int ret = 0;

    if(!ctrl)
    {
        printk(KERN_ERR "func:%s [error] ctrl is null\r\n",__FUNCTION__);
        return -EFAULT;  
    }
	
	//printk(KERN_ERR "%s(%d) ctrl->id=%d,\n", __func__, __LINE__,ctrl->id);
	switch(ctrl->id){
		case VIDIOC_G_SLICED_VBI_CAP:
		{
			struct v4l2_sliced_vbi_cap sliced_vbi_cap;	
			sliced_vbi_cap.service_set = 0; //?? fix later
			sliced_vbi_cap.service_lines[0][0] = 0;
			sliced_vbi_cap.service_lines[1][0] = 0;
			sliced_vbi_cap.type = V4L2_BUF_TYPE_SLICED_VBI_CAPTURE;
			sliced_vbi_cap.reserved[0] = 0;
			sliced_vbi_cap.reserved[1] = 0;
			sliced_vbi_cap.reserved[2] = 0;
			
			if(copy_to_user((void __user *)ctrl->value, &sliced_vbi_cap, sizeof(struct v4l2_sliced_vbi_cap)))
		    {
		        printk(KERN_ERR "func:%s [error] copy_to_user VIDIOC_G_SLICED_VBI_CAP fail \r\n",__FUNCTION__);
		        return -EFAULT; 
		    }
			break;
		}
	    default:
	        break;
	}

	printk(KERN_ERR "%s(%d) VIDIOC_G_CTRL ret=%d ctrl->id = %d,\n", __func__, __LINE__,ret,ctrl->id);
	return ret;
}

int vbi_v4l2_ioctl_s_ext_ctrls(struct file *file, void *fh, struct v4l2_ext_controls *ctrls)
{

    if(!ctrls)
    {
        printk(KERN_ERR "func:%s [error] ctrl is null\r\n",__FUNCTION__);
        return -EFAULT;  
    }

	return 0;
}

int vbi_v4l2_ioctl_g_ext_ctrls(struct file *file, void *fh, struct v4l2_ext_controls *ctrls)
{
	if(!ctrls)
	{
		printk(KERN_ERR "func:%s [error] ctrls is null\r\n",__FUNCTION__);
		return -EFAULT;
	}
	
	//printk(KERN_ERR "%s(%d) ctrls->controls->id = %d,\n", __func__, __LINE__,ctrls->controls->id);
	switch(ctrls->controls->id)
	{
		case V4L2_CID_EXT_VBI_COPY_PROTECTION_INFO:
		{	
			struct v4l2_ext_vbi_copy_protection copy_protection_info;
			unsigned char cgms_data;
			
			memset(&copy_protection_info, 0, sizeof(struct v4l2_ext_vbi_copy_protection));
			cgms_data = (Scaler_VbiGet_cgms()>>10)&0x3;
			
			if(1 == cgms_data)
			{
				copy_protection_info.aps_cp_info = V4L2_EXT_VBI_APS_ON_BURST_OFF;
				copy_protection_info.macrovision_cp_info = V4L2_EXT_VBI_MACROVISION_PSP_ON_BURST_OFF;
			}else if(2 == cgms_data){
				copy_protection_info.aps_cp_info = V4L2_EXT_VBI_APS_ON_BURST_2;
				copy_protection_info.macrovision_cp_info = V4L2_EXT_VBI_MACROVISION_PSP_ON_BURST_2;
			}else if(3 == cgms_data){
				copy_protection_info.aps_cp_info = V4L2_EXT_VBI_APS_ON_BURST_4;
				copy_protection_info.macrovision_cp_info = V4L2_EXT_VBI_MACROVISION_PSP_ON_BURST_4;
			}else{
				copy_protection_info.aps_cp_info = V4L2_EXT_VBI_APS_OFF;
				copy_protection_info.macrovision_cp_info = V4L2_EXT_VBI_MACROVISION_PSP_OFF;
			}
			cgms_data = (Scaler_VbiGet_cgms()>>12)&0x3;
			if (1 == cgms_data) {
				copy_protection_info.cgms_cp_info = V4L2_EXT_VBI_CGMS_ONCE;//no more copy
			} else if (2 == cgms_data) {
				copy_protection_info.cgms_cp_info = V4L2_EXT_VBI_CGMS_RESERVED;
			} else if (3 == cgms_data) {
				copy_protection_info.cgms_cp_info = V4L2_EXT_VBI_CGMS_NO_PERMIT;
			} else {
				copy_protection_info.cgms_cp_info = V4L2_EXT_VBI_CGMS_PERMIT;
			}
			
			if(copy_to_user(ctrls->controls->ptr, &copy_protection_info, sizeof(struct v4l2_ext_vbi_copy_protection)))
			{
				printk(KERN_ERR "%s = V4L2_CID_EXT_VBI_COPY_PROTECTION_INFO failed!!!!!!!!!!!!!!!\n",__FUNCTION__);
				return -EFAULT;
			}
			break;
		}
		default:
			break;
	}

	return 0;
}

int vbi_v4l2_ioctl_s_fmt_vbi_cap   (struct file *file, void *fh, struct v4l2_format *fmt)
{	
	if(!fmt)
    {
        printk(KERN_ERR "func:%s [error] fmt is null\r\n",__FUNCTION__);
        return -EFAULT;
    }
	memcpy(&vbi_format, fmt, sizeof(struct v4l2_format));
	
	return 0;
}

int vbi_v4l2_ioctl_g_fmt_vbi_cap   (struct file *file, void *fh, struct v4l2_format *fmt)
{		
	struct v4l2_sliced_vbi_format fmt_sliced_vbi;
	memset(&fmt_sliced_vbi, 0, sizeof(struct v4l2_sliced_vbi_format));
	if(!fmt)
	{
		printk(KERN_ERR "func:%s [error] fmt is null\r\n",__FUNCTION__);
		return -EFAULT;
	}
	
	rtd_printk(KERN_NOTICE,TAG_VBI_NAME,"%s(%d) fmt->type = %d,\n", __func__, __LINE__,fmt->type);
	if ((fmt->type ==V4L2_BUF_TYPE_VBI_CAPTURE) ||(fmt->type ==V4L2_BUF_TYPE_SLICED_VBI_CAPTURE)) {
		//fmt->fmt.vbi.sampling_rate = 27000000;
	#if 0
		fmt->fmt.vbi.offset = 24;
		fmt->fmt.vbi.samples_per_line = 1440;
		fmt->fmt.vbi.sample_format = V4L2_PIX_FMT_GREY;
		fmt->fmt.vbi.start[0] = is_60hz ? V4L2_VBI_ITU_525_F1_START + 9 : V4L2_VBI_ITU_625_F1_START + 5;
		fmt->fmt.vbi.start[1] = is_60hz ? V4L2_VBI_ITU_525_F2_START + 9 : V4L2_VBI_ITU_625_F2_START + 5;
		fmt->fmt.vbi.count[0] = fmt->fmt.vbi.count[1] = is_60hz ? 12 : 18;
		fmt->fmt.vbi.flags = 1; //dev->vbi_cap_interlaced ? V4L2_VBI_INTERLACED : 0;
		fmt->fmt.vbi.reserved[0] = 0;
		fmt->fmt.vbi.reserved[1] = 0;
	#endif
		fmt_sliced_vbi.service_set = V4L2_SLICED_VBI_625 |V4L2_SLICED_VBI_525;
		fmt_sliced_vbi.io_size = sizeof(struct v4l2_sliced_vbi_data) * 24;
		if (fmt_sliced_vbi.service_set & V4L2_SLICED_CAPTION_525) {
			fmt_sliced_vbi.service_lines[0][21] = V4L2_SLICED_CAPTION_525;
			fmt_sliced_vbi.service_lines[1][21] = V4L2_SLICED_CAPTION_525;
		}
		
		if (fmt_sliced_vbi.service_set & V4L2_SLICED_WSS_625) {
			fmt_sliced_vbi.service_lines[0][23] = V4L2_SLICED_WSS_625;
		}
		if (fmt_sliced_vbi.service_set & V4L2_SLICED_TELETEXT_B) {
			unsigned i;

			for (i = 6; i <= 22; i++)
			{
				fmt_sliced_vbi.service_lines[0][i] = V4L2_SLICED_TELETEXT_B;
				fmt_sliced_vbi.service_lines[1][i] = V4L2_SLICED_TELETEXT_B;
			}
		}
		if (fmt_sliced_vbi.service_set & V4L2_SLICED_VPS) {
			fmt_sliced_vbi.service_lines[0][16] = V4L2_SLICED_VPS;
		}
		fmt->fmt.sliced=fmt_sliced_vbi;		
	}else{
		printk(KERN_ERR "func:%s unvalid type = %d",__FUNCTION__,fmt->type);
	}
	return 0;
}

int vbi_v4l2_ioctl_streamon(struct file *file, void *fh, enum v4l2_buf_type type)
{	   
	rtd_printk(KERN_NOTICE,TAG_VBI_NAME,"%s(%d) type = %d,\n", __func__, __LINE__,type);
	if((V4L2_BUF_TYPE_VBI_CAPTURE == type) ||(V4L2_BUF_TYPE_SLICED_VBI_CAPTURE == type))
	{
		down(&VBI_Semaphore);
		VBI_flush_buffer_data(V4L2_SLICED_TELETEXT_B);
		VBI_flush_buffer_data(V4L2_SLICED_VPS);
		VBI_flush_buffer_data(V4L2_SLICED_CAPTION_525);
		VBI_flush_buffer_data(V4L2_SLICED_WSS_625);
		up(&VBI_Semaphore);
		if ((vbi_format.fmt.sliced.service_set & V4L2_SLICED_TELETEXT_B) &&
			(vbi_format.fmt.sliced.service_set & V4L2_SLICED_CAPTION_525)) {
			VBI_support_type_set(KADP_VFE_AVD_CC_AND_TTX_ARE_SUPPORTED,1);
		} else if (vbi_format.fmt.sliced.service_set & V4L2_SLICED_TELETEXT_B) {
			VBI_support_type_set(KADP_VFE_AVD_TTX_IS_SUPPORTED,1);
		} else if (vbi_format.fmt.sliced.service_set & V4L2_SLICED_CAPTION_525) {
			VBI_support_type_set(KADP_VFE_AVD_CC_IS_SUPPORTED,1);
		}else{
			printk(KERN_ERR "func:%s sliced.service_set unvalid =%d",__FUNCTION__,vbi_format.fmt.sliced.service_set);
			VBI_support_type_set(KADP_VFE_AVD_NOTHING_IS_SUPPORTED,0);
		}
		VBI_stream_data_Set_ONOFF(1);
	}else{
		printk(KERN_ERR "func:%s sliced.v4l2_buf_type unvalid = %d",__FUNCTION__,type);
	}
	return 0;
}

int vbi_v4l2_ioctl_streamoff(struct file *file, void *fh, enum v4l2_buf_type type)
{
	rtd_printk(KERN_NOTICE,TAG_VBI_NAME,"%s(%d) type = %d,\n", __func__, __LINE__,type);
	if((V4L2_BUF_TYPE_VBI_CAPTURE == type) ||(V4L2_BUF_TYPE_SLICED_VBI_CAPTURE == type))
	{
		VBI_stream_data_Set_ONOFF(0);
	}else{
		printk(KERN_ERR "func:%s sliced.v4l2_buf_type unvalid = %d",__FUNCTION__,type);
	}
	return 0;
}

int vbi_v4l2_get_vbi_service(void)
{
	return vbi_format.fmt.sliced.service_set;
}

int vbi_v4l2_ioctl_g_fmt_sliced_vbi_cap(struct file *file, void *fh, struct v4l2_format *fmt)
{
	struct v4l2_sliced_vbi_format fmt_sliced_vbi;
	unsigned i=0;
	if(!fmt)
	{
		printk(KERN_ERR "func:%s [error] fmt is null\r\n",__FUNCTION__);
		return -EFAULT;
	}
	if ((fmt->type ==V4L2_BUF_TYPE_VBI_CAPTURE) ||(fmt->type ==V4L2_BUF_TYPE_SLICED_VBI_CAPTURE)) {
		memset(&fmt_sliced_vbi, 0, sizeof(struct v4l2_sliced_vbi_format));
	#if 0
		if (Scaler_VBI_GetLGEColorSystem() == KADP_VFE_AVD_CC_IS_SUPPORTED) {
			fmt_sliced_vbi.service_set = V4L2_SLICED_CAPTION_525;
		} else if (Scaler_VBI_GetLGEColorSystem() == KADP_VFE_AVD_TTX_IS_SUPPORTED){
			fmt_sliced_vbi.service_set = V4L2_SLICED_VBI_625;
		} else {
			fmt_sliced_vbi.service_set = 0;
		}
	
		if (fmt_sliced_vbi.service_set == 0){
			printk(KERN_ERR "func:%s vbi service_set is unvalid\r\n",__FUNCTION__);
			return -EINVAL;
		}
	#else
		fmt_sliced_vbi.service_set = vbi_format.fmt.sliced.service_set;
	#endif
		fmt_sliced_vbi.io_size = sizeof(struct v4l2_sliced_vbi_data) * 24;
		fmt_sliced_vbi.service_lines[0][0] = 0;
		fmt_sliced_vbi.service_lines[1][0] = 0;	
		if (Scaler_VBI_GetLGEColorSystem() == KADP_VFE_AVD_CC_IS_SUPPORTED) {
			fmt_sliced_vbi.service_lines[0][1] = V4L2_VBI_ITU_525_F1_START;
			fmt_sliced_vbi.service_lines[1][1] = V4L2_VBI_ITU_525_F2_START;
			for (i = 2; i < 24; i++)
			{
				fmt_sliced_vbi.service_lines[0][i] = V4L2_VBI_ITU_525_F1_START+i-1;
				fmt_sliced_vbi.service_lines[1][i] = V4L2_VBI_ITU_525_F2_START+i-1;
			}
		} else if (Scaler_VBI_GetLGEColorSystem() == KADP_VFE_AVD_TTX_IS_SUPPORTED){
			fmt_sliced_vbi.service_lines[0][1] = V4L2_VBI_ITU_625_F1_START;
			fmt_sliced_vbi.service_lines[1][1] = V4L2_VBI_ITU_625_F2_START;
			for (i = 2; i < 24; i++)
			{
				fmt_sliced_vbi.service_lines[0][i] = V4L2_VBI_ITU_625_F1_START+i-1;
				fmt_sliced_vbi.service_lines[1][i] = V4L2_VBI_ITU_625_F2_START+i-1;
			}
		}
		fmt->fmt.sliced=fmt_sliced_vbi;
		rtd_printk(KERN_NOTICE, TAG_VBI_NAME,"g_fmt_sliced_vbi_cap service_set=%x\n",fmt_sliced_vbi.service_set);
	}else{
		printk(KERN_ERR "func:%s vbi cap buffer type unvalid = %d ",__FUNCTION__,fmt->type);
	}
	return 0;
}

int vbi_v4l2_ioctl_s_fmt_sliced_vbi_cap(struct file *file, void *fh, struct v4l2_format *fmt)
{
	if(!fmt)
	{
		printk(KERN_ERR "func:%s [error] fmt is null\r\n",__FUNCTION__);
		return -EFAULT;
	}
	rtd_printk(KERN_NOTICE,TAG_VBI_NAME,"func:%s vbi@%d@ %x@\r\n",__FUNCTION__,fmt->type,fmt->fmt.sliced.service_set);
	vbi_format.type = fmt->type;
	if ((fmt->fmt.sliced.service_set & V4L2_SLICED_TELETEXT_B) &&
		(fmt->fmt.sliced.service_set & V4L2_SLICED_CAPTION_525)) {
		VBI_support_type_set(KADP_VFE_AVD_CC_AND_TTX_ARE_SUPPORTED,1);
	} else if (fmt->fmt.sliced.service_set & V4L2_SLICED_TELETEXT_B) {
		VBI_support_type_set(KADP_VFE_AVD_TTX_IS_SUPPORTED,1);
	} else if (fmt->fmt.sliced.service_set & V4L2_SLICED_CAPTION_525) {
		VBI_support_type_set(KADP_VFE_AVD_CC_IS_SUPPORTED,1);
	}else{
		printk(KERN_ERR "func:%s sliced.service_set unvalid =%d",__FUNCTION__,vbi_format.fmt.sliced.service_set);
		VBI_support_type_set(KADP_VFE_AVD_NOTHING_IS_SUPPORTED,0);
	}
	if (V4L2_SLICED_VPS & fmt->fmt.sliced.service_set) {
		if (vbi_clk_get_enable()) {
			
		} else {
			CRT_CLK_OnOff(VBI, CLK_ON, NULL);
		}
		Scaler_vbi_vps_init();
		rtd_printk(KERN_NOTICE, TAG_VBI_NAME,"open vps driver\n");
	} else if(V4L2_SLICED_VPS & vbi_format.fmt.sliced.service_set){
		if (vbi_clk_get_enable()) {
		} else {
			CRT_CLK_OnOff(VBI, CLK_ON, NULL);
		}
		Scaler_vbi_vps_uninit();
		rtd_printk(KERN_NOTICE, TAG_VBI_NAME,"close vps driver\n");
	}
	vbi_format.fmt.sliced.service_set = fmt->fmt.sliced.service_set;
	return 0;
}

int vidioc_s_fmt_sliced_vbi_cap(struct file *file, void *fh, struct v4l2_format *fmt)
{
	if(!fmt)
	{
		printk(KERN_ERR "func:%s [error] fmt is null\r\n",__FUNCTION__);
		return -EFAULT;
	}
	
	rtd_printk(KERN_NOTICE, TAG_VBI_NAME,"func:%s \r\n",__FUNCTION__);
	vbi_format.type = fmt->type;
	vbi_format.fmt.sliced.service_set = fmt->fmt.sliced.service_set;
	return 0;
}

void vbi_ttx_data_handle(void)
{
	unsigned char **vbi_buf = NULL;
	unsigned short *vbi_buf_idx = NULL;
	unsigned int i=0;
	const unsigned char (*ptr)[][PPR_PTNGEN_PKT_LEN_BYTES];
	unsigned short start_idx, end_idx;
	//static unsigned short tmp = 0;
	vbi_buf = (unsigned char**)m_ttxBufNonCachedAddr;
	if (vbi_buf == NULL) {
		pr_emerg("vbi_ttx_buffer is NULL\n");
		return;
	}
	vbi_buf_idx = (unsigned short *)((unsigned char *)m_ttxBufNonCachedAddr + (VBI_TTX_BUF_LENGTH*PPR_PTNGEN_PKT_LEN_BYTES));
 //  	outer_flush_range(vbi_ttx_buffer,vbi_ttx_buffer+(VBI_TTX_BUF_LENGTH*PPR_PTNGEN_PKT_LEN_BYTES));
	
	start_idx = *((unsigned short*) vbi_buf_idx);
	end_idx   =  reverseInt16(*(((unsigned short*) vbi_buf_idx)+1));
	
	//if(end_idx != start_idx)
//	pr_emerg("@@@tt handle01 = %d,start_idx= %d,end_idx= %d\n",vbi_ttx_last_pkt,start_idx,end_idx);

	if((start_idx<VBI_TTX_BUF_LENGTH) && (end_idx<VBI_TTX_BUF_LENGTH))
	{
		if	(start_idx >  end_idx) {
			ptr = (const unsigned char (*)[][PPR_PTNGEN_PKT_LEN_BYTES])(((unsigned char*) vbi_buf+ (start_idx*PPR_PTNGEN_PKT_LEN_BYTES)));
			for(i=0;i<(VBI_TTX_BUF_LENGTH-start_idx);++i)
			{
				if(((*ptr)[i][0] == 0x00)&&((*ptr)[i][1] == 0xFF)&&((*ptr)[i][2] == 0xFF)&&((((*ptr)[i][3]>>1) &0x7F)== (unsigned char)(0x22>>1))&&((*ptr)[i][4] == 0x2a))
				{//valid ttx data
					if (vbi_ttx_last_pkt >= VBI_TTX_BUF_LENGTH) {
						printk(KERN_ERR "vbi_ttx_last_pkt overflow=%d\n",vbi_ttx_last_pkt);
						vbi_ttx_last_pkt=0;
					}
					memcpy(&(ttx_slicer_fieldBuff[vbi_ttx_last_pkt][0]), &((*ptr)[i][5]), TTX_SLICER_PACKET_SIZE);
				#if 0
					ttx_slicer_fieldBuff[vbi_ttx_last_pkt][0]=0xff;
					ttx_slicer_fieldBuff[vbi_ttx_last_pkt][1]=0xff;
					ttx_slicer_fieldBuff[vbi_ttx_last_pkt][3]=0x2a;
					if ((*ptr)[i][3]==0x22) {
						ttx_slicer_fieldBuff[vbi_ttx_last_pkt][2]=0x26;
						if(vbi_ttx_last_pkt!=0) {
							if (ttx_slicer_fieldBuff[vbi_ttx_last_pkt-1][2]==0x26) {
								ttx_slicer_fieldBuff[vbi_ttx_last_pkt][4]=0x4e;
							} else {
								ttx_slicer_fieldBuff[vbi_ttx_last_pkt][4]=0x4d;
							}
						} else {
							if (ttx_curr_bit2_data==0x26) {
								ttx_slicer_fieldBuff[vbi_ttx_last_pkt][4]=0x4e;
							} else {
								ttx_slicer_fieldBuff[vbi_ttx_last_pkt][4]=0x4d;
							}
						}
						ttx_slicer_fieldBuff[vbi_ttx_last_pkt][5]=0x1;
					} else {
						ttx_slicer_fieldBuff[vbi_ttx_last_pkt][2]=0x27;
						if(vbi_ttx_last_pkt!=0) {
							if (ttx_slicer_fieldBuff[vbi_ttx_last_pkt-1][2]==0x27) {
								ttx_slicer_fieldBuff[vbi_ttx_last_pkt][4]=0x15;
							} else {
								ttx_slicer_fieldBuff[vbi_ttx_last_pkt][4]=0x14;
							}
						} else {
							if (ttx_curr_bit2_data==0x27) {
								ttx_slicer_fieldBuff[vbi_ttx_last_pkt][4]=0x15;
							} else {
								ttx_slicer_fieldBuff[vbi_ttx_last_pkt][4]=0x14;
							}
						}
						ttx_slicer_fieldBuff[vbi_ttx_last_pkt][5]=0x0;
					}
				#else
					ttx_slicer_fieldBuff[vbi_ttx_last_pkt][42]=0;
					ttx_slicer_fieldBuff[vbi_ttx_last_pkt][43]=0;
					ttx_slicer_fieldBuff[vbi_ttx_last_pkt][44]=0;
					ttx_slicer_fieldBuff[vbi_ttx_last_pkt][45]=0;
					ttx_slicer_fieldBuff[vbi_ttx_last_pkt][46]=0;
					ttx_slicer_fieldBuff[vbi_ttx_last_pkt][47]=0;
				#endif
					vbi_ttx_last_pkt+=1;
				}
			}
				start_idx = 0;
		}
		if ( start_idx < end_idx )
		{	
				ptr = (const unsigned char (*)[][PPR_PTNGEN_PKT_LEN_BYTES])(((unsigned char*) vbi_buf+ (start_idx*PPR_PTNGEN_PKT_LEN_BYTES)));
				for(i=0;i<(end_idx-start_idx);++i)
				{
					if(((*ptr)[i][0] == 0x00)&&((*ptr)[i][1] == 0xFF)&&((*ptr)[i][2] == 0xFF)&&((((*ptr)[i][3]>>1) &0x7F)== (unsigned char)(0x22>>1))&&((*ptr)[i][4] == 0x2a))
					{ //valid ttx data
						if (vbi_ttx_last_pkt >= VBI_TTX_BUF_LENGTH) {
							printk(KERN_ERR "vbi_ttx_last_pkt overflow=%d\n",vbi_ttx_last_pkt);
							vbi_ttx_last_pkt=0;
						}
						memcpy(&(ttx_slicer_fieldBuff[vbi_ttx_last_pkt][0]), &((*ptr)[i][5]), TTX_SLICER_PACKET_SIZE);
					#if 0
						ttx_slicer_fieldBuff[vbi_ttx_last_pkt][0]=0xff;
						ttx_slicer_fieldBuff[vbi_ttx_last_pkt][1]=0xff;
						ttx_slicer_fieldBuff[vbi_ttx_last_pkt][3]=0x2a;
						if ((*ptr)[i][3]==0x22) {
							ttx_slicer_fieldBuff[vbi_ttx_last_pkt][2]=0x26;
							if(vbi_ttx_last_pkt!=0) {
								if (ttx_slicer_fieldBuff[vbi_ttx_last_pkt-1][2]==0x26) {
									ttx_slicer_fieldBuff[vbi_ttx_last_pkt][4]=0x4e;
								} else {
									ttx_slicer_fieldBuff[vbi_ttx_last_pkt][4]=0x4d;
								}
							} else {
								if (ttx_curr_bit2_data==0x26) {
									ttx_slicer_fieldBuff[vbi_ttx_last_pkt][4]=0x4e;
								} else {
									ttx_slicer_fieldBuff[vbi_ttx_last_pkt][4]=0x4d;
								}
							}
							ttx_slicer_fieldBuff[vbi_ttx_last_pkt][5]=0x1;
						} else {
							ttx_slicer_fieldBuff[vbi_ttx_last_pkt][2]=0x27;
							if(vbi_ttx_last_pkt!=0) {
								if (ttx_slicer_fieldBuff[vbi_ttx_last_pkt-1][2]==0x27) {
									ttx_slicer_fieldBuff[vbi_ttx_last_pkt][4]=0x15;
								} else {
									ttx_slicer_fieldBuff[vbi_ttx_last_pkt][4]=0x14;
								}
							} else {
								if (ttx_curr_bit2_data==0x27) {
									ttx_slicer_fieldBuff[vbi_ttx_last_pkt][4]=0x15;
								} else {
									ttx_slicer_fieldBuff[vbi_ttx_last_pkt][4]=0x14;
								}
							}
							ttx_slicer_fieldBuff[vbi_ttx_last_pkt][5]=0x0;
						}
					#else
						ttx_slicer_fieldBuff[vbi_ttx_last_pkt][42]=0;
						ttx_slicer_fieldBuff[vbi_ttx_last_pkt][43]=0;
						ttx_slicer_fieldBuff[vbi_ttx_last_pkt][44]=0;
						ttx_slicer_fieldBuff[vbi_ttx_last_pkt][45]=0;
						ttx_slicer_fieldBuff[vbi_ttx_last_pkt][46]=0;
						ttx_slicer_fieldBuff[vbi_ttx_last_pkt][47]=0;
					#endif
						vbi_ttx_last_pkt+=1;
					}
				}
			start_idx = end_idx;
			}
		// update global start index
		*((unsigned short*) vbi_buf_idx) = start_idx;
	} else {
		printk(KERN_ERR "ttx sharememory index overflow=%d,%d\n",start_idx,end_idx);
	}
	
}

void vbi_vps_data_handle(void)
{
	unsigned char **vbi_buf = NULL;
	unsigned short *vbi_buf_idx = NULL;
	unsigned int i=0;
	const unsigned char (*ptr1)[][VBI_VPS_FRAME_BYTES];
	unsigned short start_idx, end_idx;
	vbi_buf = (unsigned char**)m_vpsBufNonCachedAddr;
	if (vbi_buf == NULL) {
		pr_emerg("vbi_vps_buffer is NULL\n");
		return;
	}
	vbi_buf_idx = (unsigned short *)(m_vpsBufNonCachedAddr + (VBI_VPS_BUF_LENGTH*VBI_VPS_FRAME_BYTES));
//	outer_flush_range(Scaler_VbiVPSGetPhyBufAddr(),Scaler_VbiVPSGetPhyBufAddr()+(VBI_VPS_BUF_LENGTH*VBI_VPS_FRAME_BYTES));
	// Mark a processing area by recording current start index and end index
	// because ISR will update them all the time.
	start_idx = *((unsigned short*) vbi_buf_idx);
	end_idx   =  reverseInt16(*(((unsigned short*) vbi_buf_idx)+1));

	//pr_emerg("vps start_idx=%d,end_idx=%d\n",start_idx,end_idx);
	if((start_idx<VBI_VPS_BUF_LENGTH)&&(end_idx<VBI_VPS_BUF_LENGTH))
	{
		if ( start_idx > end_idx )
		{
			ptr1 = (const unsigned char (*)[][VBI_VPS_FRAME_BYTES])(((unsigned char*) vbi_buf+ (start_idx*VBI_VPS_FRAME_BYTES)));
			if ((vbi_vps_last_pkt+(VBI_VPS_BUF_LENGTH-start_idx))>=VBI_VPS_BUF_LENGTH) {
					printk(KERN_ERR"vbi_vps_lastpkt overflow=%d\n",vbi_vps_last_pkt);
					vbi_vps_last_pkt=0;
	        	}
			for(i =0; i<(VBI_VPS_BUF_LENGTH-start_idx); i++){   
				memcpy(&(vps_slicer_fieldBuff[vbi_vps_last_pkt][0]), &((*ptr1)[i][0]), VBI_VPS_FRAME_BYTES);
			#if 0
				vps_slicer_fieldBuff[vbi_vps_last_pkt][0] = 0; //LG vps data format is 15 bytes and invalid data value is 0,not 0xff.
				vps_slicer_fieldBuff[vbi_vps_last_pkt][1] = 0;
				vps_slicer_fieldBuff[vbi_vps_last_pkt][2] = 0;
				vps_slicer_fieldBuff[vbi_vps_last_pkt][3] = 0;
				vps_slicer_fieldBuff[vbi_vps_last_pkt][5] = 0;
				vps_slicer_fieldBuff[vbi_vps_last_pkt][6] = 0;
				vps_slicer_fieldBuff[vbi_vps_last_pkt][7] = 0;
				vps_slicer_fieldBuff[vbi_vps_last_pkt][8] = 0;
				vps_slicer_fieldBuff[vbi_vps_last_pkt][9] = 0;
			#else
				vps_slicer_fieldBuff[vbi_vps_last_pkt][13] = 0;
				vps_slicer_fieldBuff[vbi_vps_last_pkt][14] = 0;
			#endif
				vbi_vps_last_pkt++;
			}
			start_idx = 0;
		}
		if ( start_idx < end_idx )
		{
			ptr1 = (const unsigned char (*)[][VBI_VPS_FRAME_BYTES])(((unsigned char*) vbi_buf+ (start_idx*VBI_VPS_FRAME_BYTES)));
			if ((vbi_vps_last_pkt+(end_idx-start_idx))>=VBI_VPS_BUF_LENGTH) {
					printk(KERN_ERR"vbi_vps_last_pkt overflow=%d\n",vbi_vps_last_pkt);
					vbi_vps_last_pkt=0;
			}
			for(i =0; i<(end_idx-start_idx); i++){   
				memcpy(&(vps_slicer_fieldBuff[vbi_vps_last_pkt][0]), &((*ptr1)[i][0]), VBI_VPS_FRAME_BYTES);
			#if 0
				vps_slicer_fieldBuff[vbi_vps_last_pkt][0] = 0; //LG vps data format is 15 bytes and invalid data value is 0,not 0xff.
				vps_slicer_fieldBuff[vbi_vps_last_pkt][1] = 0;
				vps_slicer_fieldBuff[vbi_vps_last_pkt][2] = 0;
				vps_slicer_fieldBuff[vbi_vps_last_pkt][3] = 0;
				vps_slicer_fieldBuff[vbi_vps_last_pkt][5] = 0;
				vps_slicer_fieldBuff[vbi_vps_last_pkt][6] = 0;
				vps_slicer_fieldBuff[vbi_vps_last_pkt][7] = 0;
				vps_slicer_fieldBuff[vbi_vps_last_pkt][8] = 0;
				vps_slicer_fieldBuff[vbi_vps_last_pkt][9] = 0;
			#else
				vps_slicer_fieldBuff[vbi_vps_last_pkt][13] = 0;
				vps_slicer_fieldBuff[vbi_vps_last_pkt][14] = 0;
			#endif
				vbi_vps_last_pkt++;
			}
			start_idx = end_idx;
		}
		// update global start index
		*((unsigned short*) vbi_buf_idx) = start_idx;
	} else {
		printk(KERN_ERR"vps sharememory index overflow=%d,%d\n",start_idx,end_idx);
	}
}

#define	INCR(x,y)			((x+y)%MVD_SUBTITLE_BUF_SIZE)
#define NTSC_FIELD_1		0xFC 		///< b'1111_1100' = NTSC line 21 field 1 closed-captions
#define NTSC_FIELD_2		0xFD 		///< b'1111_1101' = NTSC line 21 field 2 closed-captions
unsigned long reverseInteger1(unsigned long value)
{
	unsigned long b0 = value & 0x000000ff;
	unsigned long b1 = (value & 0x0000ff00) >> 8;
	unsigned long b2 = (value & 0x00ff0000) >> 16;
	unsigned long b3 = (value & 0xff000000) >> 24;

	return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
}

UINT32 Get32_be(UINT32 data)
{
	if ( data == 0 ) {
		return 0;
	} else {
		volatile UINT32 A;  // prevent gcc -O3 optimization to create non-atomic access
		A = reverseInteger1(data);
		return A;
	}
}
unsigned int ccBuf_GetReadPoint(void)
{
 	return  cc_mvdSubBuf->rPtr;
}

unsigned int ccBuf_GetWritePoint(void)
{
    return cc_mvdSubBuf->wPtr;
}

unsigned int ccBuf_GetBufferSize(void)
{
  return MVD_SUBTITLE_BUF_SIZE;
}

unsigned int ccBuf_SetReadPoint(unsigned int readPtr)
{
    cc_mvdSubBuf->rPtr = readPtr;
    return 0;
}

unsigned int	ccBuf_GetOverflowFlag(void)
{
	return cc_mvdSubBuf->overflowFlag;
}

unsigned int	ccBuf_SetOverflowFlag(unsigned int overflow)
{
	return 	cc_mvdSubBuf->overflowFlag = overflow;
}

unsigned int cc_getDataLen(void)
{
	unsigned int	wPtr = Get32_be(ccBuf_GetWritePoint());
	unsigned int	rPtr = Get32_be(ccBuf_GetReadPoint());

	if(wPtr == rPtr)	//CID-19488
		return 0;
	else if(wPtr > rPtr)	// NOT ring over yet
		return (wPtr - rPtr - 1);		// change rPtr to stay in readed position
	else				// ring over!
		return (ccBuf_GetBufferSize() - rPtr - 1 + wPtr);		// change rPtr to stay in readed position
}

unsigned char cc_noDataReason(void)
{
	if(Get32_be(ccBuf_GetOverflowFlag())) {				// buffer overflow!
		ccBuf_SetReadPoint(Get32_be((Get32_be(ccBuf_GetWritePoint()) > 0) ? (Get32_be(ccBuf_GetWritePoint()) - 1) : (ccBuf_GetBufferSize() - 1)));
		ccBuf_SetOverflowFlag(Get32_be(0));	// clear overflow flag
		return	0;//DECODER_BUF_OVERFLOW;
	} else {
		return	0;//DECODER_NO_DATA;					// buffer starving
	}
}

void vbi_cc_data_handle(void)
{
	unsigned int			rPtr;			// Subtitle Buffer read pointer kept by subtitle codec
	int 		data_len = 0;	// Unsigned -> Signed when data_len reduce to 0 will error
	unsigned char *pBuff = NULL;
	unsigned char cc_field;
	pBuff = (unsigned char*)m_subtitleNonCachedAddr;
	if (NULL == pBuff) {
		pr_emerg("vbi cc buffer is NULL\n");
		return;
	}
//	outer_flush_range(vbi_cc_buffer,vbi_cc_buffer+(sizeof(unsigned char) * MVD_SUBTITLE_BUF_SIZE+ sizeof(MVD_SUBTITLE_BUF)));
_FindUserDataID:
	rPtr = INCR(Get32_be(ccBuf_GetReadPoint()), 0); 	// forward read
	data_len = cc_getDataLen(); 		// calculate new coming data length in Subtitle Buffer
	if (data_len <= 1)
	{
		goto _COMPLEMENT;
	}

//---- Go-on process next packet ----//
_NextPackets:
	if (vbi_cc_last_pkt>=VBI_CC_BUF_LENGTH) {
		vbi_cc_last_pkt=0;
		//pr_emerg("vbi_cc_last_pkt is full\n");
	}
	while(*(pBuff + rPtr) != 0x03) {
		if(*(pBuff + rPtr) == NTSC_FIELD_1 || *(pBuff + rPtr) == NTSC_FIELD_2){
			break;
		}

		if (data_len > 3) {
			rPtr = INCR(rPtr, 1);
			data_len --;
		} else {
			//	to avoid MVD to set overflow flag
			if (rPtr == Get32_be(ccBuf_GetWritePoint())) {
			//	STC_MESSAGE_DEBUG("Buffer Underflow , rPtr=[%x], wPtr=[%x]\n", rPtr, Get32_be(ccBuf_GetWritePoint()));
				rPtr = ((rPtr > 0) ? (rPtr - 1) : (ccBuf_GetBufferSize() - 1));
			}
			ccBuf_SetReadPoint(Get32_be(rPtr)); 	// update buffer read pointer
			goto _COMPLEMENT;
		}
	}

	goto _ComputeData;

_ComputeData:
	if(data_len < 3) {// each ccData has 3 bytes including 'data_valid' and 'data_type'
		goto _COMPLEMENT;
	} else {
		cc_field=*(unsigned char *)(pBuff + INCR(rPtr, 0));
		if(cc_field==NTSC_FIELD_1) {
			cc_slicer_fieldBuff[vbi_cc_last_pkt][2]=1;
		} else {
			cc_slicer_fieldBuff[vbi_cc_last_pkt][2]=0;
		}
		cc_slicer_fieldBuff[vbi_cc_last_pkt][0] = *(unsigned char *)(pBuff + INCR(rPtr, 1));
		cc_slicer_fieldBuff[vbi_cc_last_pkt][1] = *(unsigned char *)(pBuff + INCR(rPtr, 2));
//		pr_emerg("cc=%x,%x,%x\n",cc_slicer_fieldBuff[vbi_cc_last_pkt][0],cc_slicer_fieldBuff[vbi_cc_last_pkt][1],cc_slicer_fieldBuff[vbi_cc_last_pkt][2]);
		vbi_cc_last_pkt++;
		rPtr = INCR(rPtr, 3);				// cc data size = 3 bytes
		data_len -= 3;
	}	
	ccBuf_SetReadPoint(Get32_be(rPtr)); 			// update buffer read pointer

	if (data_len > 1)
	{
		goto _NextPackets;
	}
	else
		goto _FindUserDataID;
_COMPLEMENT:
	cc_noDataReason();
}
static unsigned char do_v4l2_vbi_intial = 0;	

int vbi_v4l2_open(struct file *file)
{
	rtd_printk(KERN_NOTICE, TAG_VBI_NAME,"vbi_v4l2_open\n");
	if (!do_v4l2_vbi_intial) {
		do_v4l2_vbi_intial = 1;
		if (0==vbi_vps_buffer) {
			vbi_vps_buffer = (unsigned int)dvr_malloc_uncached((sizeof(unsigned char) * (VBI_VPS_BUF_LENGTH*VBI_VPS_FRAME_BYTES + 4)),(void**)&m_vpsBufNonCachedAddr);
			if (!vbi_vps_buffer)
				return -ENOMEM;
			memset((void *)vbi_vps_buffer, 0, sizeof(unsigned char) *(VBI_VPS_BUF_LENGTH*VBI_VPS_FRAME_BYTES+4));
			Scaler_VbiVPSSetPhyBufAddr((unsigned long)dvr_to_phys((void*)vbi_vps_buffer));
		}
		if (0==vbi_ttx_buffer) {
			vbi_ttx_buffer = (unsigned int)dvr_malloc_uncached((sizeof(unsigned char) * (VBI_TTX_BUF_LENGTH*PPR_PTNGEN_PKT_LEN_BYTES + 4)),(void**)&m_ttxBufNonCachedAddr);
			if (!vbi_ttx_buffer)
				return -EFAULT;
			memset((void *)vbi_ttx_buffer, 0, (VBI_TTX_BUF_LENGTH*PPR_PTNGEN_PKT_LEN_BYTES + 4));
			Scaler_VbiTtxSetPhyBufAddr((unsigned long)dvr_to_phys((void*)vbi_ttx_buffer));
		}
		if (0==vbi_cc_buffer) {
			vbi_cc_buffer = (unsigned int)dvr_malloc_uncached((sizeof(MVD_SUBTITLE_BUF) + (sizeof(unsigned char) * MVD_SUBTITLE_BUF_SIZE)),(void**)&m_subtitleNonCachedAddr);
			if (!vbi_cc_buffer)
				return -ENOMEM;
			Scaler_VbiCcSetPhyBufAddr((unsigned long)dvr_to_phys((void*)vbi_cc_buffer));
			cc_mvdSubBuf = (MVD_SUBTITLE_BUF*)(m_subtitleNonCachedAddr + (sizeof(unsigned char) * MVD_SUBTITLE_BUF_SIZE));
			memset(cc_mvdSubBuf, 0, sizeof(MVD_SUBTITLE_BUF));
			cc_sDataVirAddr = (unsigned char*)m_subtitleNonCachedAddr;
			memset(cc_sDataVirAddr, 0, sizeof(unsigned char) * MVD_SUBTITLE_BUF_SIZE);
			cc_mvdSubBuf->sData = Get32_be(Scaler_VbiCcGetPhyBufAddr());
		}
		init_vbi_rpc();
	}
	return 0;
}

int vbi_v4l2_release(struct file *file)
{
	rtd_printk(KERN_NOTICE, TAG_VBI_NAME,"vbi_v4l2_release\n");
	VBI_support_type_set(KADP_VFE_AVD_NOTHING_IS_SUPPORTED,0);
	do_v4l2_vbi_intial = 0;
#if 0
	if (vbi_cc_buffer) {
		dvr_free((void *)vbi_cc_buffer);
		vbi_cc_buffer = 0;
		m_subtitleNonCachedAddr =0;
	}
	if (vbi_ttx_buffer) {
		dvr_free((void *)vbi_ttx_buffer);
		vbi_ttx_buffer = 0;
		m_ttxBufNonCachedAddr = 0;
	}
	if (vbi_vps_buffer) {
		dvr_free((void *)vbi_vps_buffer);
		vbi_vps_buffer = 0;
		m_vpsBufNonCachedAddr = 0;
	}
#endif
	return 0;
}
extern unsigned char AVD_Global_Status;

int vbi_v4l2_read(struct file *file, char __user *data, size_t count, loff_t *ppos)
{
	struct v4l2_sliced_vbi_data* vbi_data_tmp;
	struct v4l2_sliced_vbi_data vbi_data;
	int i;
	size_t vbi_pkt;
	unsigned int vbi_type_data_count=0,vbi_type_max_count=0;
//	unsigned int k=0;
	if(!data)
	{
		printk(KERN_ERR "func:%s [error] data is null\r\n",__FUNCTION__);
		return -EFAULT;
	}
	if(!ppos)
	{
		printk(KERN_ERR "func:%s [error] ppos is null\r\n",__FUNCTION__);
		return -EFAULT;
	}
	
	vbi_pkt  = count/(sizeof(struct v4l2_sliced_vbi_data));//stand for read how many lines data
	vbi_data_tmp = kmalloc(count,GFP_KERNEL);
	memset(vbi_data_tmp, 0, count);
	vbi_type_max_count = vbi_pkt;
	memset(&vbi_data, 0, sizeof(struct v4l2_sliced_vbi_data));
	
//	printk(KERN_ERR "vbi service=%x,count=%d\n",vbi_format.fmt.sliced.service_set,vbi_pkt);
//    printk(KERN_ERR "%s read data start, hh vbi_pkt = %d ,vbi_data.id = %d vbi_init_setting=%d @@\n",__func__,vbi_pkt,vbi_data.id,vbi_init_setting);
	if (V4L2_SLICED_TELETEXT_B & vbi_format.fmt.sliced.service_set) {
    		down(&VBI_Semaphore);
    		if(1 ==VBI_stream_data_Get_ONOFF()) {
    			vbi_ttx_data_handle();
    		}
		if(0 == vbi_ttx_last_pkt)
		{
			up(&VBI_Semaphore);	
		}else{
			vbi_data.reserved = 0;
			vbi_data.field = 0;
			vbi_data.id = V4L2_SLICED_TELETEXT_B;
			vbi_data.line = 6;
			if(vbi_ttx_last_pkt <= vbi_type_max_count){  
				for(i = 0; i < vbi_ttx_last_pkt; i++){
					vbi_data.field = ttx_slicer_fieldBuff[i][5];
					memcpy(vbi_data.data,&ttx_slicer_fieldBuff[i][0],TTX_PACKET_SIZE_CUSTOMER);
					memcpy(vbi_data_tmp,&vbi_data,sizeof(struct v4l2_sliced_vbi_data));
				//	for(k=0;k<48;k++)
				//	rtd_printk(KERN_EMERG, TAG_VBI_NAME,"ttx data:%x\n",vbi_data_tmp->data[k]);
					vbi_data_tmp++;
					vbi_type_data_count++;
				}
				vbi_ttx_last_pkt = 0;
			}else{
				for(i = 0; i < vbi_type_max_count; i++){
					vbi_data.field = ttx_slicer_fieldBuff[i][5];
					memcpy(vbi_data.data,&ttx_slicer_fieldBuff[i][0],TTX_PACKET_SIZE_CUSTOMER);
					memcpy(vbi_data_tmp,&vbi_data,sizeof(struct v4l2_sliced_vbi_data));
					vbi_data_tmp++;
					vbi_type_data_count++;
				}				
				//update 
				vbi_ttx_last_pkt -= vbi_type_max_count;
				memcpy(ttx_slicer_fieldBuff[0],&ttx_slicer_fieldBuff[vbi_type_max_count][0],TTX_PACKET_SIZE_CUSTOMER*vbi_ttx_last_pkt);
			}
			up(&VBI_Semaphore);
			memset(&vbi_data, 0, sizeof(struct v4l2_sliced_vbi_data));
		}
	}
	if (vbi_type_data_count== vbi_pkt) {
		vbi_data_tmp -=vbi_pkt;
		if(copy_to_user(to_user_ptr(data), vbi_data_tmp, count))
	       {
			printk(KERN_ERR "func:%s [error vbi_ttx] copy_to_user fail \r\n",__FUNCTION__);
			kfree(vbi_data_tmp);
			return -EFAULT;
	       }
		kfree(vbi_data_tmp);
		return count;
	}

	//continue to receive data
	vbi_type_max_count = vbi_pkt-vbi_type_data_count;
	if(V4L2_SLICED_VPS & vbi_format.fmt.sliced.service_set) {
    		down(&VBI_Semaphore);
    		if(1 ==VBI_stream_data_Get_ONOFF()) {
    			vbi_vps_data_handle();
    		}
		if(0 == vbi_vps_last_pkt)
		{
			up(&VBI_Semaphore);	
		}else{
			vbi_data.reserved = 0;
			vbi_data.field = 0;
			vbi_data.id = V4L2_SLICED_VPS;
			vbi_data.line = 16;
			if(vbi_vps_last_pkt <= vbi_type_max_count){  
				for(i = 0; i < vbi_vps_last_pkt; i++){
					memcpy(vbi_data.data,&vps_slicer_fieldBuff[i][0],VBI_VPS_FRAME_BYTES_CUSTOM);
					memcpy(vbi_data_tmp,&vbi_data,sizeof(struct v4l2_sliced_vbi_data));
					//for(k=0;k<15;k++)
					//rtd_printk(KERN_EMERG, TAG_VBI_NAME,"vps data:%x\n",vbi_data_tmp->data[k]);
					vbi_data_tmp++;
					vbi_type_data_count++;
				}
				vbi_vps_last_pkt = 0;
			}else{
				for(i = 0; i < vbi_type_max_count; i++){
					memcpy(vbi_data.data,&vps_slicer_fieldBuff[i][0],VBI_VPS_FRAME_BYTES_CUSTOM);
					memcpy(vbi_data_tmp,&vbi_data,sizeof(struct v4l2_sliced_vbi_data));
					vbi_data_tmp++;
					vbi_type_data_count++;
				}				
				//update 
				vbi_vps_last_pkt -= vbi_type_max_count;
				memcpy(vps_slicer_fieldBuff[0],&vps_slicer_fieldBuff[vbi_type_max_count][0],VBI_VPS_FRAME_BYTES_CUSTOM*vbi_vps_last_pkt);
			}
			up(&VBI_Semaphore);
			memset(&vbi_data, 0, sizeof(struct v4l2_sliced_vbi_data));
		}
	}
	if (vbi_type_data_count== vbi_pkt) {
		vbi_data_tmp -=vbi_pkt;
		if(copy_to_user(to_user_ptr(data), vbi_data_tmp, count))
		{
			printk(KERN_ERR "func:%s [error vbi_vps] copy_to_user fail \r\n",__FUNCTION__);
			kfree(vbi_data_tmp);
			return -EFAULT;
		}
		kfree(vbi_data_tmp);
		return count;
	}
//	printk(KERN_ERR "[V4L2_VBI]ttx_coun=%d\r\n",vbi_type_data_count);
	//continue to receive data
	vbi_type_max_count = vbi_pkt-vbi_type_data_count;
	if(V4L2_SLICED_WSS_625 & vbi_format.fmt.sliced.service_set) {
		unsigned int wss_data=0;
		if(1 ==VBI_stream_data_Get_ONOFF()) {
			vbi_data.reserved = 0;
			vbi_data.field = 0;
			vbi_data.line = 23;
			vbi_data.id = V4L2_SLICED_WSS_625;
			wss_data=Scaler_VbiGet_576I_WSS();
			vbi_data.data[0] = wss_data & 0xff; //lower 8 bit data
			vbi_data.data[1] = wss_data >> 8;   //higher 8 bit data
			memcpy(vbi_data_tmp,&vbi_data,sizeof(struct v4l2_sliced_vbi_data));
			vbi_data_tmp++;
			vbi_type_data_count++;
			memset(&vbi_data, 0, sizeof(struct v4l2_sliced_vbi_data));
		}
	}
//	printk(KERN_ERR "[V4L2_VBI]wss_coun=%d\r\n",vbi_type_data_count);
	if (vbi_type_data_count== vbi_pkt) {
		vbi_data_tmp -=vbi_pkt;
		if(copy_to_user(to_user_ptr(data), vbi_data_tmp, count))
	        {
			printk(KERN_ERR "func:%s [error vbi_wss] copy_to_user fail \r\n",__FUNCTION__);
			kfree(vbi_data_tmp);
			return -EFAULT;
	        }
		kfree(vbi_data_tmp);
		return count;
	}

	//continue to receive data
	vbi_type_max_count = vbi_pkt-vbi_type_data_count;
	if(V4L2_SLICED_CAPTION_525 & vbi_format.fmt.sliced.service_set) {
		//printk(KERN_ERR "@@@vbi_cc_last_pkt read = %d\n",vbi_cc_last_pkt);
		down(&VBI_Semaphore);
		if(1 ==VBI_stream_data_Get_ONOFF()) {
			vbi_cc_data_handle();
		}
		if(0 == vbi_cc_last_pkt)
		{
			up(&VBI_Semaphore);	
		}else{
			vbi_data.reserved = 0;
			vbi_data.field = 0;
			vbi_data.id = V4L2_SLICED_CAPTION_525;
			vbi_data.line = 21;
			if(vbi_cc_last_pkt <= vbi_type_max_count){  
				for(i = 0; i < vbi_cc_last_pkt; i++){
					vbi_data.data[0] = cc_slicer_fieldBuff[i][0];
					vbi_data.data[1] = cc_slicer_fieldBuff[i][1];
					//vbi_data.field =  cc_slicer_fieldBuff[i][2];
					if (cc_slicer_fieldBuff[i][2] ==1) {
						vbi_data.line =21;
						vbi_data.field = 1;
					} else {
						vbi_data.line =284;
						vbi_data.field = 0;
					}
					//memcpy(vbi_data.data,&cc_slicer_fieldBuff[i][0],VBI_CC_FRAME_BYTES);
					memcpy(vbi_data_tmp,&vbi_data,sizeof(struct v4l2_sliced_vbi_data));
					//rtd_printk(KERN_EMERG, TAG_VBI_NAME,"cc data:%x,%x,%d\n",vbi_data_tmp->data[0],vbi_data_tmp->data[1],vbi_data_tmp->field);
					vbi_data_tmp++;
					vbi_type_data_count++;
				}
				vbi_cc_last_pkt = 0;			
			}else{
				for(i = 0; i < vbi_type_max_count; i++){
					vbi_data.data[0] = cc_slicer_fieldBuff[i][0];
					vbi_data.data[1] = cc_slicer_fieldBuff[i][1];
					vbi_data.field =  cc_slicer_fieldBuff[i][2];
					memcpy(vbi_data_tmp,&vbi_data,sizeof(struct v4l2_sliced_vbi_data));
					vbi_data_tmp++;
					vbi_type_data_count++;
				}
				//update 
				vbi_cc_last_pkt -= vbi_type_max_count;
				memcpy(cc_slicer_fieldBuff[0],&cc_slicer_fieldBuff[vbi_type_max_count][0],VBI_CC_FRAME_BYTES*vbi_cc_last_pkt);
			}
			up(&VBI_Semaphore);
		}		
	}

	vbi_data_tmp -=vbi_type_data_count;
	if(copy_to_user(to_user_ptr(data), vbi_data_tmp, count))
       {
		printk(KERN_ERR "func:%s [error vbi_read_end] copy_to_user fail \r\n",__FUNCTION__);
		kfree(vbi_data_tmp);
		return -EFAULT;
       }
#if 0
	for(k=0;k<vbi_pkt;k++) {
		rtd_printk(KERN_EMERG, TAG_VBI_NAME,"%x:%x,%x,%d-%x,%x\n",vbi_data_tmp->id,vbi_data_tmp->data[0],vbi_data_tmp->data[1],vbi_data_tmp->field,vbi_data_tmp->data[2],vbi_data_tmp->data[3]);
		vbi_data_tmp++;
	//	for(j=2;j<48;j++)
	//		rtd_printk(KERN_EMERG, TAG_VBI_NAME,"%x\n",vbi_data_tmp[vbi_type_data_count].data[j]);
	}
	vbi_data_tmp=vbi_data_tmp-vbi_pkt;
#endif

	
	kfree(vbi_data_tmp);
	return (vbi_type_data_count*sizeof(struct v4l2_sliced_vbi_data));
}

unsigned int vbi_v4l2_poll(struct file *file, poll_table *wait)
{
	return 0;
}

int vbi_v4l2_mmap(struct file *file, struct vm_area_struct *vma)
{	
    return 0;
}

const struct v4l2_ioctl_ops vbi_ioctl_ops = {
    .vidioc_s_ext_ctrls   = vbi_v4l2_ioctl_s_ext_ctrls,
    .vidioc_g_ext_ctrls   = vbi_v4l2_ioctl_g_ext_ctrls,
    .vidioc_s_ctrl        = vbi_v4l2_ioctl_s_ctrl,
    .vidioc_g_ctrl        = vbi_v4l2_ioctl_g_ctrl,
    .vidioc_s_fmt_vbi_cap = vbi_v4l2_ioctl_s_fmt_vbi_cap,
    .vidioc_g_fmt_vbi_cap = vbi_v4l2_ioctl_g_fmt_vbi_cap,
    .vidioc_s_fmt_sliced_vbi_cap = vbi_v4l2_ioctl_s_fmt_sliced_vbi_cap,
    .vidioc_g_fmt_sliced_vbi_cap = vbi_v4l2_ioctl_g_fmt_sliced_vbi_cap,
    .vidioc_streamon      = vbi_v4l2_ioctl_streamon,
    .vidioc_streamoff     = vbi_v4l2_ioctl_streamoff,  
};

const struct v4l2_file_operations vbi_fops = {
	.owner		= THIS_MODULE,
	.open		= vbi_v4l2_open,
	.release		= vbi_v4l2_release,
	.read		= vbi_v4l2_read,
	.poll			= vbi_v4l2_poll,
	.unlocked_ioctl  = video_ioctl2, /* V4L2 ioctl handler */
	.mmap		= vbi_v4l2_mmap,
};

static struct video_device vbi_v4l2_tmplate = {
	.name = VBI_V4L2_MAIN_DEVICE_NAME,
	.fops	= &vbi_fops,
	.ioctl_ops = &vbi_ioctl_ops,
	.minor = -1,
	.release = video_device_release,
	.tvnorms = V4L2_STD_ALL,
};
#if 0	
static bool vbi_tsk_running_flag = FALSE;//Record vsc_scaler_tsk status. True: Task is running
static struct task_struct *p_vbi_tsk = NULL;


VBI_SLICED_DATA_T slicer_data;
unsigned char pal_wss_count;
unsigned char ntsc_wss_count;
#define WSS_WAIT_COUNT 1


static int vbi_slice_data_tsk(void *p)//This task slice vbi data
{
	struct cpumask vbi_data_cpumask;
	pr_notice("vbi_slice_data_tsk()\n");
	cpumask_clear(&vbi_data_cpumask);
	cpumask_set_cpu(0, &vbi_data_cpumask); // run task in core 0
	cpumask_set_cpu(2, &vbi_data_cpumask); // run task in core 2
	cpumask_set_cpu(3, &vbi_data_cpumask); // run task in core 3
	sched_setaffinity(0, &vbi_data_cpumask);
	current->flags &= ~PF_NOFREEZE;
	while(1)
	{ 
	  //  if(SRC_CONNECT_DONE == AVD_Global_Status){
			if(0 == vbi_init_setting){
				Scaler_VbiCcSetPhyBufAddr((unsigned long)vbi_cc_buffer);
				//	Scaler_VbiTtxSetPhyBufAddr((unsigned long)vbi_ttx_buffer);
				Scaler_VbiVPSSetPhyBufAddr((unsigned long)vbi_vps_buffer);
				init_vbi_rpc();
				VBI_support_type_set(KADP_VFE_AVD_CC_AND_TTX_ARE_SUPPORTED,1);
				vbi_init_setting = 1;
			}else{
				vbi_data.line = 0;
				vbi_data.reserved = 0;
				vbi_data.field = 1;
				vbi_data.id = vbi_format.fmt.sliced.service_set;
				if(V4L2_SLICED_TELETEXT_B == vbi_data.id){
				#if 0
					down(&VBI_Semaphore);
					vbi_ttx_data_handle();			
					if(vbi_ttx_last_pkt!=0)
					{
						if(1 ==VBI_stream_data_Get_ONOFF())
						{
							ttx_curr_bit2_data=ttx_slicer_fieldBuff[vbi_ttx_last_pkt-1][2]; //record last bit2 value
						} else {
							vbi_ttx_last_pkt=0;
						}
					}
					up(&VBI_Semaphore);
				#endif
				}else if(V4L2_SLICED_VPS == vbi_data.id){
				//	vbi_vps_data_handle();
				}else if(V4L2_SLICED_CAPTION_525 == vbi_data.id){
				//	vbi_cc_data_handle();
				}else if(V4L2_SLICED_WSS_625 == vbi_data.id){
					if (pal_wss_count == 0)	{
						pal_wss_data = Scaler_VbiGet_576I_WSS();
					}
					if (pal_wss_count >= WSS_WAIT_COUNT) {
						pal_wss_count = 0;
					} else {
						pal_wss_count++;
					}
				}
		}
		msleep(10); //wait 10ms
		if (freezing(current))
		{
			try_to_freeze();
		}
		if (kthread_should_stop()) {
			break;
		}
	}
   return 0;
}

static void create_vbi_tsk(void)
{
	int err;
	if (vbi_tsk_running_flag == FALSE) {
		p_vbi_tsk = kthread_create(vbi_slice_data_tsk, NULL, "vbi_slice_data_tsk");

	    if (p_vbi_tsk) {
			wake_up_process(p_vbi_tsk);
			vbi_tsk_running_flag = TRUE;
	    } else {
	    	err = PTR_ERR(p_vbi_tsk);
	    	printk(KERN_ERR "Unable to start %s (err_id = %d)./n",__FUNCTION__,err);
	    }
	}
}

static void delete_vbi_tsk(void)
{
	int ret;
	if (vbi_tsk_running_flag) {
 		ret = kthread_stop(p_vbi_tsk);
 		if (!ret) {
 			p_vbi_tsk = NULL;
 			vbi_tsk_running_flag = FALSE;
			printk(KERN_ERR "vbi_tsk_running_flag thread stopped\n");
 		}
	}
}
#endif

static int __init vbi_v4l2_init(void)
{
	struct vbi_v4l2_device *vbi_dev;
	struct video_device *vbi_vfd;
//	unsigned int vir_addr_noncache;
	int ret;
	vbi_dev = kzalloc(sizeof(*vbi_dev), GFP_KERNEL);
	if (!vbi_dev)
		return -ENOMEM;
	strcpy(vbi_dev->v4l2_dev.name, "/dev/vbi1");
	
	ret = v4l2_device_register(NULL, &vbi_dev->v4l2_dev);
	if (ret)
	{
		goto free_dev;
	}


	vbi_vfd = video_device_alloc();
	if (!vbi_vfd) {
		v4l2_err(&vbi_dev->v4l2_dev, "Failed to allocate video device\n");
		ret = -ENOMEM;
		goto unreg_dev;
	}

    *vbi_vfd = vbi_v4l2_tmplate;
   
	vbi_vfd->v4l2_dev = &vbi_dev->v4l2_dev; /* here set the v4l2_device, you have already registered it */
	ret = video_register_device(vbi_vfd, VFL_TYPE_VBI, 1);// device name /dev/vbi1
	if (ret) {
		v4l2_err(&vbi_dev->v4l2_dev, "Failed to register video device\n");
		goto rel_vdev;
	}

	video_set_drvdata(vbi_vfd, vbi_dev);
	snprintf(vbi_vfd->name, sizeof(vbi_vfd->name), "%s", vbi_v4l2_tmplate.name);
	vbi_dev->vfd = vbi_vfd;
	/* the debug message*/
   	v4l2_info(&vbi_dev->v4l2_dev, "V4L2 device registered as %s\n", video_device_node_name(vbi_vfd));

	sema_init(&VBI_Semaphore, 1);
	//create_vbi_tsk();/*Create VBI task*/
	printk(KERN_ERR "%s init done\n",__FUNCTION__);

	return 0;

rel_vdev:
	video_device_release(vbi_vfd);
unreg_dev:
	v4l2_device_unregister(&vbi_dev->v4l2_dev);
free_dev:
	kfree(vbi_dev);	

	return ret;
}

static void __exit vbi_v4l2_exit(void)
{
//	delete_vbi_tsk();/*delete vbi task*/
	return;
}

module_init(vbi_v4l2_init);
module_exit(vbi_v4l2_exit);
MODULE_LICENSE("GPL");
