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


/////////////////////////////////////////////////////
// adc headler file
#include <linux/videodev2.h>


#include <media/v4l2-common.h>

#include <linux/v4l2-ext/v4l2-controls-ext.h>
#include <linux/v4l2-ext/videodev2-ext.h>

//#include <tvscalercontrol/adc_v4l2/v4l2_adc_api.h>

//common
#include <ioctrl/scaler/vfe_cmd_id.h>
#include <ioctrl/scaler/vsc_cmd_id.h>

#ifdef CONFIG_KDRIVER_USE_NEW_COMMON
	#include <scaler/scalerCommon.h>
#else
	#include <scalercommon/scalerCommon.h>
#endif

#include <tvscalercontrol/scaler/scalerstruct.h>
#include <tvscalercontrol/scaler/source.h>

#include <tvscalercontrol/scalerdrv/power.h>
#include "tvscalercontrol/scalerdrv/auto.h"

#include <tvscalercontrol/vip/viptable.h>

#include <tvscalercontrol/scaler_vfedev.h>

#include <tvscalercontrol/adcsource/adcctrl.h>
#include <tvscalercontrol/adcsource/ypbpr.h>
#include <mach/rtk_log.h>

/////////////////////////////////////////////////////
typedef unsigned char (*DETECTCALLBACK)(void);
extern void ADC_register_callback(unsigned char enable, DETECTCALLBACK cb);
extern void ADC_set_detect_flag(unsigned char enable);
//extern void Set_Reply_Zero_Timing_Flag(unsigned char src, unsigned char flag);
extern void ADC_Set_GainOffset(ADCGainOffset *adcinfo);
extern void ADC_Get_GainOffset(ADCGainOffset *adcinfo);
extern void Get_ADC_Gain_Offset_From_OTP(int src, ADCGainOffset *pGainOffsetData);

//vsc internal api
extern unsigned char rtk_hal_vsc_initialize(void);
extern unsigned char rtk_hal_vsc_uninitialize(void);
extern unsigned char rtk_hal_vsc_open(VIDEO_WID_T wid);
extern unsigned char rtk_hal_vsc_close(VIDEO_WID_T wid);
extern unsigned char rtk_hal_vsc_Connect(VIDEO_WID_T wid, KADP_VSC_INPUT_SRC_INFO_T inputSrcInfo, KADP_VSC_OUTPUT_MODE_T outputMode);
extern unsigned char rtk_hal_vsc_Disconnect(VIDEO_WID_T wid, KADP_VSC_INPUT_SRC_INFO_T inputSrcInfo, KADP_VSC_OUTPUT_MODE_T outputMode);
extern unsigned char rtk_hal_vsc_SetInputRegionEx(VIDEO_WID_T wid, VIDEO_RECT_T  inregion, VIDEO_RECT_T originalInput);
extern unsigned char rtk_hal_vsc_SetOutputRegion(VIDEO_WID_T wid, KADP_VIDEO_RECT_T outregion, unsigned short Wide, unsigned short High);
extern unsigned char rtk_hal_vsc_SetWinBlank(VIDEO_WID_T wid, bool bonoff, KADP_VIDEO_DDI_WIN_COLOR_T color);
extern void Set_Reply_Zero_Timing_Flag(unsigned char src, unsigned char flag);

extern struct semaphore *get_adc_detectsemaphore(void);
extern struct semaphore *get_setsource_semaphore(void);

static unsigned char v4l2_adc_calibration_type = 0;
static unsigned char v4l2_adc_fast_switch_mode = 0;


#define V4L2_EXT_DEVICE_NO_ADC  11
#define V4L2_EXT_DEVICE_NAME_ADC "video11"
#define V4L2_EXT_DEVICE_PATH_ADC "/dev/video11"

#define TAG_NAME_V4L2_ADC "V4L2-ADC"


struct v4l2_adc_device{
	struct v4l2_device v4l2_dev;
	struct video_device *vfd;
};

unsigned char get_adc_input_source(void)
{
	if(get_ADC_Input_Source() == _SRC_YPBPR)
		return V4L2_EXT_ADC_INPUT_SRC_COMP;
	else
		return V4L2_EXT_ADC_INPUT_SRC_NONE;
}

unsigned char get_adc_calibration_type(void)
{
	return v4l2_adc_calibration_type;
}

unsigned char get_adc_fast_switch_status(void)
{
	return v4l2_adc_fast_switch_mode;
}

/////////////////////////////////////////////////////

static int v4l2_adc_fast_switch_on(void)
{
	int ret = 0;
	unsigned int count = 500;
	KADP_VSC_INPUT_SRC_INFO_T InputsourceInfo = {KADP_VSC_INPUTSRC_ADC, 0 , 0};
	VIDEO_RECT_T intRect = {0, 0, 0, 0};
	KADP_VIDEO_RECT_T outRect = {0, 0, 3840, 2160};
	StructDisplayInfo *adc_timing_info = NULL;

	// ADC connect ====================>
	if(get_ADC_Global_Status() != SRC_OPEN_DONE){
		rtd_printk(KERN_ERR, TAG_NAME_V4L2_ADC,"[%s(%d)] adc connect error, source is not SRC_OPEN_DONE\n", __func__, __LINE__);
		ret = -EINVAL;
		return ret;
	}

	set_ADC_Input(_YPBPR_INPUT1);

	if(!ADC_Connect(_SRC_YPBPR, get_ADC_Input())){
		printk(KERN_ERR "[V4L2-ADC][%s(%d)]adc connect _YPBPR_INPUT1 error\n",__func__,__LINE__);
		ret = -EINVAL;
		return ret;
	}

	down(get_setsource_semaphore());
	set_ADC_Input_Source(_SRC_YPBPR);
	up(get_setsource_semaphore());
	down(get_adc_detectsemaphore());
	set_ADC_Global_Status(SRC_CONNECT_DONE);
	ADC_set_detect_flag(TRUE);
	ADC_register_callback(TRUE, YPbPr_DetectTiming);
	up(get_adc_detectsemaphore());

	// vsc connect ====================>
	rtk_hal_vsc_initialize();
	rtk_hal_vsc_open(VIDEO_WID_0);
	rtk_hal_vsc_Connect(VIDEO_WID_0, InputsourceInfo, KADP_VSC_OUTPUT_DISPLAY_MODE);

	// check adc timing ====================>
	do{
		adc_timing_info = Get_ADC_Dispinfo();
		if((adc_timing_info->IPH_ACT_WID_PRE != 0)&&(adc_timing_info->IPV_ACT_LEN_PRE != 0))
			break;

		rtd_printk(KERN_INFO, TAG_NAME_V4L2_ADC,"[%s(%d)] active width and length is zero\n", __func__, __LINE__);
		msleep(1);
	}while(count --);

	intRect.x = adc_timing_info->IPH_ACT_STA_PRE;
	intRect.y = adc_timing_info->IPV_ACT_STA_PRE;
	intRect.w = adc_timing_info->IPH_ACT_WID_PRE;
	intRect.h = adc_timing_info->IPV_ACT_LEN_PRE;

	rtk_hal_vsc_SetInputRegionEx(VIDEO_WID_0, intRect, intRect);
	rtk_hal_vsc_SetOutputRegion(VIDEO_WID_0, outRect, 0, 0);
	rtk_hal_vsc_SetWinBlank(VIDEO_WID_0, 0, KADP_VIDEO_DDI_WIN_COLOR_BLACK);

	return ret;
}

static int v4l2_adc_fast_switch_off(void)
{
	int ret = 0;
	KADP_VSC_INPUT_SRC_INFO_T InputsourceInfo = {KADP_VSC_INPUTSRC_ADC, 0 , 0};

	// VSC disconnect ====================>
	rtk_hal_vsc_Disconnect(VIDEO_WID_0, InputsourceInfo, 0);
	rtk_hal_vsc_close(VIDEO_WID_0);
	rtk_hal_vsc_uninitialize();

	// ADC disconnect ====================>
	if(get_ADC_Global_Status() != SRC_CONNECT_DONE){
		printk(KERN_ERR "[V4L2-ADC][%s(%d)]adc disconnect error\n",__func__,__LINE__);
		ret = -EINVAL;
		return ret;
	}

	down(get_setsource_semaphore());
	set_ADC_Input_Source(_SRC_MAX);
	up(get_setsource_semaphore());
	down(get_adc_detectsemaphore());
	ADC_Reset_TimingInfo();
	set_ADC_Global_Status(SRC_OPEN_DONE);
	ADC_register_callback(FALSE, NULL);
	ADC_set_detect_flag(FALSE);
	ADC_Disconnect();
	up(get_adc_detectsemaphore());

	return ret;
}

static int v4l2_adc_drv_open(void)
{
	int ret = 0;

	if(get_ADC_Global_Status() != SRC_NOTHING){

		rtd_printk(KERN_INFO, TAG_NAME_V4L2_ADC,"[%s(%d)] init fail, source is not SRC_NOTHING\n", __func__, __LINE__);
		ret = -EINVAL;
		return ret;
	}else{

		ADC_Initial();
		set_ADC_Global_Status(SRC_INIT_DONE);

		rtd_printk(KERN_INFO, TAG_NAME_V4L2_ADC, "[%s(%d)] init success\n", __func__, __LINE__);
	}

	if(get_ADC_Global_Status() != SRC_INIT_DONE){

		rtd_printk(KERN_INFO, TAG_NAME_V4L2_ADC,"[%s(%d)] fail, source is not SRC_INIT_DONE\n", __func__, __LINE__);
		ret = -EINVAL;
		return ret;
	}else{

		down(get_adc_power_semaphore());
		set_ADC_Global_Status(SRC_OPEN_DONE);
		up(get_adc_power_semaphore());
		ADC_Open();

		rtd_printk(KERN_INFO, TAG_NAME_V4L2_ADC, "[%s(%d)] success\n", __func__, __LINE__);
	}

	return ret;
}

static int v4l2_adc_drv_close(void)
{
	int ret = 0;

	if(get_ADC_Global_Status() != SRC_OPEN_DONE){

		rtd_printk(KERN_INFO, TAG_NAME_V4L2_ADC,"[%s(%d)] fail, source is not SRC_OPEN_DONE\n", __func__, __LINE__);
		ret = -EINVAL;
		return ret;
	}else{

		down(get_adc_power_semaphore());
		set_ADC_Global_Status(SRC_INIT_DONE);
		up(get_adc_power_semaphore());
		ADC_Close();
		down(get_adc_power_semaphore());
		if (get_ADC_Global_Status() == SRC_NOTHING || get_ADC_Global_Status() == SRC_INIT_DONE)
			drvif_adc_power_control (ADC_POWER_ALL_DISABLE_CONTROL,__func__,__LINE__);
		else
			drvif_adc_power_control (ADC_POWER_ADC_DISABLE_VDC_ALIVE_CONTROL,__func__,__LINE__);
		up(get_adc_power_semaphore());

		rtd_printk(KERN_INFO, TAG_NAME_V4L2_ADC, "[%s(%d)] success\n", __func__, __LINE__);
	}

	if(get_ADC_Global_Status() != SRC_INIT_DONE){
		rtd_printk(KERN_INFO, TAG_NAME_V4L2_ADC,"[%s(%d)] fail, source is not SRC_INIT_DONE\n", __func__, __LINE__);
		ret = -EINVAL;
		return ret;
	}else{
		set_ADC_Global_Status(SRC_NOTHING);
	}

	return ret;
}

static ssize_t v4l2_adc_open(struct file *file)
{
	return 0;
}

static ssize_t v4l2_adc_release(struct file *file)
{
	return 0;
}

static ssize_t v4l2_adc_read(struct file *file, char __user *data, size_t count, loff_t *ppos)
{
	return 0;
}

static unsigned int v4l2_adc_poll(struct file *file, poll_table *wait)
{
	return 0;
}

static int v4l2_adc_mmap(struct file *file, struct vm_area_struct *vma)
{
	return 0;
}

static int v4l2_adc_ioctl_s_ext_ctrls(struct file *file, void *priv, struct v4l2_ext_controls *ctls)
{
	int ret = 0;
	struct v4l2_ext_control *ctrl = ctls->controls;
	struct v4l2_ext_adc_calibration_data v4l2_cal_data;
	ADCGainOffset rtk_adc_info;

	printk(KERN_INFO "[V4L2-ADC][%s(%d)] controls.id  = %d\n", __func__, __LINE__, ctrl->id);
	printk(KERN_INFO "[V4L2-ADC][%s(%d)] controls.size  = %d\n", __func__, __LINE__,ctrl->size);

	switch(ctrl->id){
		case V4L2_CID_EXT_ADC_CALIBRATION_DATA:

			if(ctrl->size != sizeof(struct v4l2_ext_adc_calibration_data)){
				rtd_printk(KERN_INFO, TAG_NAME_V4L2_ADC,"[%s(%d)] v4l2_cal_data size not match\n", __func__, __LINE__);
				ret = -EINVAL;
				return ret;
			}

			if(copy_from_user((void *)&v4l2_cal_data, (const void __user *)ctrl->ptr, sizeof(struct v4l2_ext_adc_calibration_data))){
				rtd_printk(KERN_INFO, TAG_NAME_V4L2_ADC,"[%s(%d)] cal_data copy_from_user fail\n", __func__, __LINE__);
				ret = EFAULT;
				return ret;
			}else{
				rtk_adc_info.Gain_R = v4l2_cal_data.r_gain;
				rtk_adc_info.Gain_G = v4l2_cal_data.g_gain;
				rtk_adc_info.Gain_B = v4l2_cal_data.b_gain;
				rtk_adc_info.Offset_R = v4l2_cal_data.r_offset;
				rtk_adc_info.Offset_G = v4l2_cal_data.g_offset;
				rtk_adc_info.Offset_B = v4l2_cal_data.b_offset;
				ADC_Set_GainOffset(&rtk_adc_info);
			}

			break;

		default:
			ret = -EINVAL;
			break;
	}

	printk(KERN_INFO "[V4L2-ADC][%s(%d)] Success\n", __func__, __LINE__);
	return ret;
}


static int v4l2_adc_ioctl_g_ext_ctrls(struct file *file, void *priv, struct v4l2_ext_controls *ctls)
{
	int ret =0;
	unsigned char disp_scan_type = 0;
	extern unsigned char ADC_Reply_Zero_Timing;
	struct v4l2_ext_control *ctrl = ctls->controls;
	struct v4l2_ext_adc_timing_info v4l2_adc_timing_info;
	struct v4l2_ext_adc_calibration_data v4l2_cal_data;
	struct v4l2_ext_adc_calibration_data v4l2_otp_data;
	ADCGainOffset rtk_adc_info;
	StructDisplayInfo *p_dispinfo = NULL;

	//printk(KERN_INFO "[V4L2-ADC][%s(%d)] controls.id  = %d\n", __func__, __LINE__, ctrl->id);
	//printk(KERN_INFO "[V4L2-ADC][%s(%d)] controls.size  = %d\n", __func__, __LINE__,ctrl->size);
	//printk(KERN_INFO "[V4L2-ADC][%s(%d)] controls.ptr  = %d\n", __func__, __LINE__, ctrl->ptr);

	switch(ctrl->id){

		case V4L2_CID_EXT_ADC_TIMING_INFO:

			if(ctrl->size != sizeof(struct v4l2_ext_adc_timing_info)){
				rtd_printk(KERN_INFO, TAG_NAME_V4L2_ADC,"[%s(%d)] v4l2_adc_timing_info size not match\n", __func__, __LINE__);
				ret = -EINVAL;
				return ret;
			}

			if(get_ADC_Global_Status() != SRC_CONNECT_DONE){
				rtd_printk(KERN_INFO, TAG_NAME_V4L2_ADC,"[%s(%d)] adc have not connect done!\n", __func__, __LINE__);
				ret = -EINVAL;
				return ret;
			}

			memset(&v4l2_adc_timing_info, 0 , sizeof(v4l2_adc_timing_info));
			if(ADC_Reply_Zero_Timing & REPORT_ZERO_TIMING)
			{
				Set_Reply_Zero_Timing_Flag(VSC_INPUTSRC_ADC, CLEAR_ZERO_FLAG);
			}
			else	
			{
				down(get_adc_detectsemaphore());
				p_dispinfo = Get_ADC_Dispinfo();

				if(p_dispinfo->IPH_ACT_WID_PRE != 0 && p_dispinfo->IPV_ACT_LEN_PRE != 0){
					if(p_dispinfo->IPH_ACT_WID_PRE == 4095 && p_dispinfo->IPV_ACT_LEN_PRE == 4095){
						v4l2_adc_timing_info.active.w = p_dispinfo->IPH_ACT_WID_PRE;
						v4l2_adc_timing_info.active.h = p_dispinfo->IPV_ACT_LEN_PRE;
						v4l2_adc_timing_info.v_freq = p_dispinfo->IVFreq ? p_dispinfo->IVFreq : 600;
					}else{
						disp_scan_type = Scaler_GetDispStatusFromSource(p_dispinfo, SLR_DISP_INTERLACE);
						v4l2_adc_timing_info.h_freq = p_dispinfo->IHFreq *100;
						v4l2_adc_timing_info.v_freq = p_dispinfo->IVFreq;
						v4l2_adc_timing_info.h_total = p_dispinfo->IHTotal;
						v4l2_adc_timing_info.v_total = p_dispinfo->IVTotal << disp_scan_type;
						v4l2_adc_timing_info.h_porch = p_dispinfo->IPH_ACT_STA_PRE;
						v4l2_adc_timing_info.v_porch = p_dispinfo->IPV_ACT_STA_PRE << disp_scan_type;
						v4l2_adc_timing_info.active.x = p_dispinfo->IPH_ACT_STA_PRE;
						v4l2_adc_timing_info.active.y = p_dispinfo->IPV_ACT_STA_PRE;
						v4l2_adc_timing_info.active.w = p_dispinfo->IPH_ACT_WID_PRE;
						v4l2_adc_timing_info.active.h = p_dispinfo->IPV_ACT_LEN_PRE << disp_scan_type;
						v4l2_adc_timing_info.scan_type =  disp_scan_type ? 0 : 1; //0 : Interlaced, 1 : Progressive  LGE SPEC
						v4l2_adc_timing_info.phase = Get_ADC_PhaseValue();
					}
				}
				up(get_adc_detectsemaphore());
			}
			#if 0
			printk(KERN_INFO "[V4L2-ADC] v4l2_adc_timing_info.h_freq = %d\n", v4l2_adc_timing_info.h_freq);
			printk(KERN_INFO "[V4L2-ADC] v4l2_adc_timing_info.v_freq = %d\n", v4l2_adc_timing_info.v_freq);
			printk(KERN_INFO "[V4L2-ADC] v4l2_adc_timing_info.h_total = %d\n", v4l2_adc_timing_info.h_total);
			printk(KERN_INFO "[V4L2-ADC] v4l2_adc_timing_info.v_total = %d\n", v4l2_adc_timing_info.v_total);
			printk(KERN_INFO "[V4L2-ADC] v4l2_adc_timing_info.h_porch = %d\n", v4l2_adc_timing_info.h_porch);
			printk(KERN_INFO "[V4L2-ADC] v4l2_adc_timing_info.v_porch = %d\n", v4l2_adc_timing_info.v_porch);
			printk(KERN_INFO "[V4L2-ADC] v4l2_adc_timing_info.active.x  = %d\n", v4l2_adc_timing_info.active.x);
			printk(KERN_INFO "[V4L2-ADC] v4l2_adc_timing_info.active.y  = %d\n", v4l2_adc_timing_info.active.y);
			printk(KERN_INFO "[V4L2-ADC] v4l2_adc_timing_info.active.w  = %d\n", v4l2_adc_timing_info.active.w);
			printk(KERN_INFO "[V4L2-ADC] v4l2_adc_timing_info.active.h  = %d\n", v4l2_adc_timing_info.active.h);
			printk(KERN_INFO "[V4L2-ADC] v4l2_adc_timing_info.scan_type = %d\n", v4l2_adc_timing_info.scan_type);
			printk(KERN_INFO "[V4L2-ADC] v4l2_adc_timing_info.phase = %d\n", v4l2_adc_timing_info.phase);
			#endif
			
			if(copy_to_user(ctrl->ptr, &v4l2_adc_timing_info, sizeof(struct v4l2_ext_adc_timing_info))){
				rtd_printk(KERN_INFO, TAG_NAME_V4L2_ADC,"[%s(%d)] v4l2_adc_timing_info copy_to_user fail\n", __func__, __LINE__);
				ret = -EINVAL;
				return ret;
			}

			break;

		case V4L2_CID_EXT_ADC_CALIBRATION_DATA:

			if(ctrl->size != sizeof(struct v4l2_ext_adc_calibration_data)){
				rtd_printk(KERN_INFO, TAG_NAME_V4L2_ADC,"[%s(%d)] v4l2_cal_data size not match\n", __func__, __LINE__);
				ret = -EINVAL;
				return ret;
			}

			ADC_Get_GainOffset(&rtk_adc_info);
			v4l2_cal_data.r_gain = rtk_adc_info.Gain_R;
			v4l2_cal_data.g_gain = rtk_adc_info.Gain_G;
			v4l2_cal_data.b_gain = rtk_adc_info.Gain_B;
			v4l2_cal_data.r_offset = rtk_adc_info.Offset_R;
			v4l2_cal_data.g_offset = rtk_adc_info.Offset_G;
			v4l2_cal_data.b_offset = rtk_adc_info.Offset_B;

			if(copy_to_user(ctrl->ptr, &v4l2_cal_data, sizeof(struct v4l2_ext_adc_calibration_data))){
				rtd_printk(KERN_INFO, TAG_NAME_V4L2_ADC,"[%s(%d)] cal_data copy_to_user fail\n", __func__, __LINE__);
				ret = -EINVAL;
				return ret;
			}

			break;

		case V4L2_CID_EXT_ADC_OTP_DATA:

			if(ctrl->size != sizeof(struct v4l2_ext_adc_calibration_data)){
				rtd_printk(KERN_INFO, TAG_NAME_V4L2_ADC,"[%s(%d)] v4l2_otp_data size not match\n", __func__, __LINE__);
				ret = -EINVAL;
				return ret;
			}

			Get_ADC_Gain_Offset_From_OTP(get_ADC_Input_Source(), &rtk_adc_info);
			v4l2_otp_data.r_gain = rtk_adc_info.Gain_R;
			v4l2_otp_data.g_gain = rtk_adc_info.Gain_G;
			v4l2_otp_data.b_gain = rtk_adc_info.Gain_B;
			v4l2_otp_data.r_offset = rtk_adc_info.Offset_R;
			v4l2_otp_data.g_offset = rtk_adc_info.Offset_G;
			v4l2_otp_data.b_offset = rtk_adc_info.Offset_B;

			if(copy_to_user(ctrl->ptr, &v4l2_otp_data, sizeof(struct v4l2_ext_adc_calibration_data))){
				rtd_printk(KERN_INFO, TAG_NAME_V4L2_ADC,"[%s(%d)] otp_data copy_to_user fail\n", __func__, __LINE__);
				ret = -EINVAL;
				return ret;
			}

			break;

		default:
			ret = -EINVAL;
			break;
	}

	//printk(KERN_INFO "[V4L2-ADC][%s(%d)] Success\n", __func__, __LINE__);
	return ret;
}


static int v4l2_adc_ioctl_s_ctrl(struct file *file, void *priv, struct v4l2_control *vc)
{
	int ret = 0;

	//printk(KERN_INFO "[V4L2-ADC][%s(%d)]controls.id  = %d\n", __func__, __LINE__, vc->id);
	//printk(KERN_INFO "[V4L2-ADC][%s(%d)]controls.value  = %d\n", __func__, __LINE__,vc->value);

	switch (vc->id){
		case V4L2_CID_EXT_ADC_CALIBRATION_TYPE:
			if(vc->value == V4L2_EXT_ADC_CALIBRATION_TYPE_OTP){
				rtd_printk(KERN_INFO, TAG_NAME_V4L2_ADC,"[%s(%d)] V4L2_EXT_ADC_CALIBRATION_TYPE_OTP\n", __func__, __LINE__);
				v4l2_adc_calibration_type = V4L2_EXT_ADC_CALIBRATION_TYPE_OTP;
				OTP_ADC_Calibration_proc();
			}else if(vc->value == V4L2_EXT_ADC_CALIBRATION_TYPE_EXTERNAL){
				rtd_printk(KERN_INFO, TAG_NAME_V4L2_ADC,"[%s(%d)] V4L2_EXT_ADC_CALIBRATION_TYPE_EXTERNAL\n", __func__, __LINE__);
				v4l2_adc_calibration_type = V4L2_EXT_ADC_CALIBRATION_TYPE_EXTERNAL;

				if(External_ADC_Calibration_proc(get_ADC_Input_Source()) == FALSE){
					rtd_printk(KERN_INFO, TAG_NAME_V4L2_ADC,"[%s(%d)] Run External_ADC_Calibration_proc fail\n", __func__, __LINE__);
					ret = -EINVAL;
					return ret;
				}
			}else if(vc->value == V4L2_EXT_ADC_CALIBRATION_TYPE_INTERNAL){
				rtd_printk(KERN_INFO, TAG_NAME_V4L2_ADC,"[%s(%d)] V4L2_EXT_ADC_CALIBRATION_TYPE_INTERNAL\n", __func__, __LINE__);
				v4l2_adc_calibration_type = V4L2_EXT_ADC_CALIBRATION_TYPE_INTERNAL;

				if(Internal_ADC_Calibration_proc(get_ADC_Input_Source()) == FALSE){
					rtd_printk(KERN_INFO, TAG_NAME_V4L2_ADC,"[%s(%d)] Run Internal_ADC_Calibration_proc fail\n", __func__, __LINE__);
					ret = EFAULT;
					return ret;
				}
			}else if(vc->value == V4L2_EXT_ADC_CALIBRATION_TYPE_USER){
				rtd_printk(KERN_INFO, TAG_NAME_V4L2_ADC,"[%s(%d)] V4L2_EXT_ADC_CALIBRATION_TYPE_USER\n", __func__, __LINE__);
				v4l2_adc_calibration_type = V4L2_EXT_ADC_CALIBRATION_TYPE_USER;
			}else{
				rtd_printk(KERN_INFO, TAG_NAME_V4L2_ADC,"[%s(%d)] V4L2_CID_EXT_ADC_CALIBRATION_TYPE error!\n", __func__, __LINE__);
				ret = EFAULT;
				return ret;
			}
			break;
		
		case V4L2_CID_EXT_ADC_RESET_CALIBRATION:
			if(vc->value == 1){
				rtd_printk(KERN_INFO, TAG_NAME_V4L2_ADC,"[%s(%d)] reset calibration data to default\n", __func__, __LINE__);
				Reset_ADC_Gain_Offset(get_ADC_Input_Source());
			}else{
				rtd_printk(KERN_INFO, TAG_NAME_V4L2_ADC,"[%s(%d)] Ignore reset calibration data\n", __func__, __LINE__);
				ret = -EINVAL;
				return ret;
			}
			break;
		
		case V4L2_CID_EXT_ADC_FAST_SWITCH:
			if(vc->value == 1){
				rtd_printk(KERN_INFO, TAG_NAME_V4L2_ADC,"[%s(%d)] adc fast switch mode on\n", __func__, __LINE__);
				v4l2_adc_fast_switch_mode = 1;
				v4l2_adc_fast_switch_on();
			}else if(vc->value == 0){
				rtd_printk(KERN_INFO, TAG_NAME_V4L2_ADC,"[%s(%d)] adc fast switch mode off\n", __func__, __LINE__);
				v4l2_adc_fast_switch_mode = 0;
				v4l2_adc_fast_switch_off();
			}else{
				rtd_printk(KERN_INFO, TAG_NAME_V4L2_ADC,"[%s(%d)] abnormal operation for fast switch!\n", __func__, __LINE__);
				ret = -EINVAL;
				return ret;
			}
			break;

		default:
			ret = -EINVAL;
			break;
	}

	//printk(KERN_INFO "[V4L2-ADC]%s(%d) VIDIOC_S_CTRL ret=%d,\n", __func__, __LINE__,ret);
	return ret;
}

static int v4l2_adc_ioctl_g_ctrl(struct file *file, void *priv, struct v4l2_control *vc)
{
	int ret = 0;

	//printk(KERN_INFO "[V4L2-ADC][%s(%d)]controls.id  = %d\n", __func__, __LINE__, vc->id);

	switch (vc->id){
		case V4L2_CID_EXT_ADC_CALIBRATION_TYPE:
			vc->value = v4l2_adc_calibration_type;
			printk(KERN_INFO "[V4L2-ADC][%s(%d)]adc calibration type = %d\n", __func__, __LINE__,vc->value);
			break;

		case V4L2_CID_EXT_ADC_FAST_SWITCH:
			vc->value = v4l2_adc_fast_switch_mode;
			printk(KERN_INFO "[V4L2-ADC][%s(%d)]adc fast switch mode = %d\n", __func__, __LINE__,vc->value);
			break;

		default:
			ret = -EINVAL;
			printk(KERN_INFO "[V4L2-ADC]%s(%d) VIDIOC_G_CTRL ret=-EINVAL\n", __func__, __LINE__);
			break;
	}

	return ret;
}

static int v4l2_adc_ioctl_s_input(struct file *file, void *priv, unsigned int input)
{
	int ret = 0;

	rtd_printk(KERN_ERR, TAG_NAME_V4L2_ADC,"[%s(%d)] input=%d\n", __func__, __LINE__,input);

	switch (input){
		case V4L2_EXT_ADC_INPUT_SRC_NONE:
			if(get_ADC_Global_Status() != SRC_CONNECT_DONE){
				printk(KERN_ERR "[V4L2-ADC][%s(%d)]adc disconnect error\n",__func__,__LINE__);
				ret = -EINVAL;
				return ret;
			}

			down(get_setsource_semaphore());
			set_ADC_Input_Source(_SRC_MAX);
			up(get_setsource_semaphore());
			down(get_adc_detectsemaphore());
			ADC_Reset_TimingInfo();
			set_ADC_Global_Status(SRC_OPEN_DONE);
			ADC_register_callback(FALSE, NULL);
			ADC_set_detect_flag(FALSE);
			ADC_Disconnect();
			Set_Reply_Zero_Timing_Flag(VSC_INPUTSRC_ADC, CLEAR_ZERO_FLAG);
			up(get_adc_detectsemaphore());
			v4l2_adc_drv_close();//call adc driver close
			break;

		case V4L2_EXT_ADC_INPUT_SRC_COMP:
			v4l2_adc_drv_open();//call adc driver initial, open

			if(get_ADC_Global_Status() == SRC_CONNECT_DONE){
				rtd_printk(KERN_ERR, TAG_NAME_V4L2_ADC,"[%s(%d)] adc already connect, source is SRC_CONNECT_DONE\n", __func__, __LINE__);
				return ret;
			}

			if(get_ADC_Global_Status() != SRC_OPEN_DONE){
				rtd_printk(KERN_ERR, TAG_NAME_V4L2_ADC,"[%s(%d)] adc connect error, source is not SRC_OPEN_DONE\n", __func__, __LINE__);
				ret = -EINVAL;
				return ret;
			}

			set_ADC_Input(_YPBPR_INPUT1);

			if(!ADC_Connect(_SRC_YPBPR, get_ADC_Input())){
				printk(KERN_ERR "[V4L2-ADC][%s(%d)]adc connect _YPBPR_INPUT1 error\n",__func__,__LINE__);
				ret = -EINVAL;
				return ret;
			}

			down(get_setsource_semaphore());
			set_ADC_Input_Source(_SRC_YPBPR);
			up(get_setsource_semaphore());
			down(get_adc_detectsemaphore());
			set_ADC_Global_Status(SRC_CONNECT_DONE);
			ADC_set_detect_flag(TRUE);
			ADC_register_callback(TRUE, YPbPr_DetectTiming);
			up(get_adc_detectsemaphore());
			break;

		//case V4L2_EXT_ADC_INPUT_SRC_COMP_RESERVED1:
		//case V4L2_EXT_ADC_INPUT_SRC_COMP_RESERVED2:
		default:
			ret = -EINVAL;
			break;
	}

	return ret;
}

static int v4l2_adc_ioctl_g_input(struct file *file, void *priv, unsigned int *input)
{
	int ret = 0;

	if(get_ADC_Input_Source() == _SRC_YPBPR)
		*input = V4L2_EXT_ADC_INPUT_SRC_COMP;
	//rtd_printk(KERN_INFO "[V4L2-ADC][%s(%d)] VIDIOC_G_INPUT input = %x\n", __func__, __LINE__, *input);
	return ret;
}

static const struct v4l2_ioctl_ops v4l2_adc_ioctl_ops = {
	.vidioc_s_input = v4l2_adc_ioctl_s_input,
	.vidioc_g_input = v4l2_adc_ioctl_g_input,
    .vidioc_s_ext_ctrls = v4l2_adc_ioctl_s_ext_ctrls,
    .vidioc_g_ext_ctrls = v4l2_adc_ioctl_g_ext_ctrls,
    .vidioc_s_ctrl = v4l2_adc_ioctl_s_ctrl,
    .vidioc_g_ctrl = v4l2_adc_ioctl_g_ctrl,
};

static const struct v4l2_file_operations v4l2_adc_fops = {
	.owner		= THIS_MODULE,
	.open		= v4l2_adc_open,
	.release	= v4l2_adc_release,
	.read		= v4l2_adc_read,
	.poll		= v4l2_adc_poll,
	.unlocked_ioctl  = video_ioctl2, /* V4L2 ioctl handler */
	.mmap		= v4l2_adc_mmap,
};

static struct video_device v4l2_adc_tmplate = {
	.name = V4L2_EXT_DEVICE_NAME_ADC,
	.fops	= &v4l2_adc_fops,
	.ioctl_ops = &v4l2_adc_ioctl_ops,
	.minor = -1,
	.release = video_device_release,
};

static int __init v4l2_adc_init(void)
{
	struct v4l2_adc_device *dev;
	struct video_device *vfd;

	int ret;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev){
		//printk(KERN_ERR "##### [V4L2-ADC][%s(%h)]Unable to alloc adc dev\n",__func__,__LINE__);
		return -ENOMEM;
	}

	/* set the device name*/
	strcpy(dev->v4l2_dev.name, V4L2_EXT_DEVICE_PATH_ADC);
	/*register the v4l2_device*/
	ret = v4l2_device_register(NULL, &dev->v4l2_dev);
	if (ret){
		goto free_dev;
	}
	/* initialize locks */
	//spin_lock_init(&dev->slock);
	//mutex_init(&dev->mutex);

	/* before register the video_device, init the video_device data*/
	//ret = -ENOMEM;

	//struct vsc_v4l2_device *main_dev;
	//struct video_device *main_vfd;

	vfd = video_device_alloc();
	if (!vfd) {
		v4l2_err(&dev->v4l2_dev, "##### [V4L2-ADC][%s(%d)]Failed to allocate video device\n",__func__,__LINE__);
		ret = -ENOMEM;
		goto unreg_dev;
	}

	*vfd = v4l2_adc_tmplate;
	/* here set the v4l2_device, you have already registered it */
	vfd->v4l2_dev = &dev->v4l2_dev;

	/*Provide a mutex to v4l2 core to protect all fops and v4l2 ioctls*/
	//vdev->lock = &dev->mutex;

	//main scaler name
	ret = video_register_device(vfd, VFL_TYPE_GRABBER, V4L2_EXT_DEVICE_NO_ADC);
	if (ret < 0) {
		v4l2_err(&dev->v4l2_dev, "##### [%s(%d)]Failed to register video device\n",__func__,__LINE__);
		goto rel_vdev;
	}


	/* set driver data */
	video_set_drvdata(vfd, dev);
	//snprintf(main_vfd->name, sizeof(main_vfd->name), "%s", vsc_main_v4l2_tmplate.name);
	dev->vfd = vfd;

	/* debug message */
   	v4l2_info(&dev->v4l2_dev, "[V4L2-ADC][%s(%d)] device registered as %s\n", __func__, __LINE__, video_device_node_name(vfd));

	return 0;

rel_vdev:
	video_device_release(vfd);
unreg_dev:
	v4l2_device_unregister(&dev->v4l2_dev);
free_dev:
	kfree(dev);

	return ret;
}



static void __exit v4l2_adc_exit(void)
{
	printk(KERN_INFO "##### [V4L2-ADC][%s(%d)]\n",__func__,__LINE__);
	return;
}

module_init(v4l2_adc_init);
module_exit(v4l2_adc_exit);
MODULE_LICENSE("GPL");
