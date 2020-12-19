/*
 *      v4l2_avd_api.c  --  Composite input(AVD) driver - V4L2 API
 *
 *      Copyright (C) 2018
 *
 */
#include <linux/atomic.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/freezer.h>/*freezing*/

#include <asm/unaligned.h>

#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-event.h>
#include <media/v4l2-common.h>

//#include <tvscalercontrol/avd_v4l2/v4l2_avd_device.h>
#include <tvscalercontrol/avd_v4l2/v4l2_avd_api.h>
#include <linux/v4l2-ext/v4l2-controls-ext.h>
#include <linux/v4l2-ext/videodev2-ext.h>
#include <mach/rtk_platform.h>

#define AVD_DEBUG_LEVEL KERN_NOTICE

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#define to_user_ptr(x)          compat_ptr((unsigned int)x)
#else
#define to_user_ptr(x)          ((void* __user)(x)) // convert 32 bit value to user pointer
#endif

//#ifndef V4L2_CID_EXT_AVD_ATV_CHANNEL_CHANGE
//#define V4L2_CID_EXT_AVD_ATV_CHANNEL_CHANGE (V4L2_CID_USER_EXT_AVD_BASE + 4)
//#endif

#ifndef V4L2_CID_EXT_AVD_ATV_DEMOD_TYPE
#define V4L2_CID_EXT_AVD_ATV_DEMOD_TYPE (V4L2_CID_USER_EXT_AVD_BASE + 5)
#endif

//static unsigned short AVD_Input = _AV_INPUT1;
static unsigned char g_apSrcIndex = V4L2_EXT_AVD_INPUT_SRC_NONE;
static unsigned int g_apPortIndex = 0;
static unsigned char v4l2_avd_open_count = 0;

static bool g_apAutoTuning = FALSE;
static v4l2_std_id g_apColorSys = V4L2_STD_ALL;
KADP_VFE_AVD_AVDECODER_VIDEO_MODE_T ColorSystem2 = KADP_VIDEO_AVDEC_MODE_UNKNOWN;
KADP_VFE_AVD_DEMOD_TYPE tDemodType = KADP_AVD_INTERNAL_DEMOD;


unsigned char get_avd_input_source(void)
{
	if(get_AVD_Input_Source() == _SRC_CVBS)
		return V4L2_EXT_AVD_INPUT_SRC_AV;
	else if(get_AVD_Input_Source() == _SRC_TV)
		return V4L2_EXT_AVD_INPUT_SRC_ATV;
	else
		return V4L2_EXT_AVD_INPUT_SRC_NONE;
}

unsigned char get_avd_open_count(void)
{
	return v4l2_avd_open_count;
}

unsigned char get_avd_port_number(void)
{
	return g_apPortIndex;
}

unsigned int get_avd_color_standard(void)
{
	Scaler_AVD_GetLGEColorSystem(&ColorSystem2);

	switch(ColorSystem2){
		case KADP_VIDEO_AVDEC_MODE_NTSC:
			return V4L2_STD_NTSC;

		case KADP_VIDEO_AVDEC_MODE_PAL:
			return V4L2_STD_PAL;

		case KADP_VIDEO_AVDEC_MODE_PAL_NC_358:
			return V4L2_STD_PAL_Nc;

		case KADP_VIDEO_AVDEC_MODE_PAL_M:
			return V4L2_STD_PAL_M;

		case KADP_VIDEO_AVDEC_MODE_SECAM:
			return V4L2_STD_SECAM;

		case KADP_VIDEO_AVDEC_MODE_NTSC_443:
			return V4L2_STD_NTSC_443;

		case KADP_VIDEO_AVDEC_MODE_PAL_60:
			return V4L2_STD_PAL_60;

		case KADP_VIDEO_AVDEC_MODE_UNKNOWN_525:
		case KADP_VIDEO_AVDEC_MODE_UNKNOWN_625:
		case KADP_VIDEO_AVDEC_MODE_UNKNOWN:
		default:
			return g_apColorSys|V4L2_STD_UNKNOWN;
	}
}

unsigned char get_avd_auto_tuning_mode(void)
{
	return g_apAutoTuning;
}


#if 0//rzhen@20190522
bool av_v4l2_device_init_flag = FALSE;
bool atv_v4l2_device_init_flag = FALSE;
bool avd_detect_tsk_create_flag = FALSE;

/* avd detect tsk  */
unsigned char v4l2_get_atv_v4l2_device_init_flag(void)//Get atv v4l2 device init flag.
{
	return atv_v4l2_device_init_flag;
}

void v4l2_set_atv_v4l2_device_init_flag(unsigned char enable)//Set atv v4l2 device init flag..
{
	if(atv_v4l2_device_init_flag != enable)
	{
		atv_v4l2_device_init_flag = enable;
	}
}

unsigned char v4l2_get_av_v4l2_device_init_flag(void)//Get atv v4l2 device init flag.
{
	return av_v4l2_device_init_flag;
}

void v4l2_set_av_v4l2_device_init_flag(unsigned char enable)//Set atv v4l2 device init flag..
{
	if(av_v4l2_device_init_flag != enable)
	{
		av_v4l2_device_init_flag = enable;
	}
}

unsigned char v4l2_get_avd_detect_tsk_create_flag(void)//Get AVD detect tsk create flag.
{
	return avd_detect_tsk_create_flag;
}

void v4l2_set_avd_detect_tsk_create_flag(unsigned char enable)//Set AVD  detect tsk create flag..
{
	if(avd_detect_tsk_create_flag != enable)
	{
		avd_detect_tsk_create_flag = enable;
	}
}


void v4l2_avd_register_callback(unsigned char enable, DETECTCALLBACK cb)//if enable is true, set the cb for AVD
{
	if(enable)
		VDC_DetectCB = cb;
	else
		VDC_DetectCB = NULL;
}


unsigned char v4l2_avd_get_detect_flag(void)//Get AVD detect timing flag.
{
	return VDC_DetectFlag;
}

void v4l2_avd_set_detect_flag(unsigned char enable)//Set AVD detect timing flag.
{
	if(VDC_DetectFlag != enable)
	{
		VDC_DetectFlag = enable;
	}
}

static int v4l2_avd_detect_tsk(void *p)//This task run AVD source timing detecting after source connect
{
    struct cpumask vsc_cpumask;
    rtd_printk(KERN_INFO, TAG_NAME,"v4l2_avd_detect_tsk\n");
    cpumask_clear(&vsc_cpumask);
    cpumask_set_cpu(0, &vsc_cpumask); // run task in core 0
    cpumask_set_cpu(2, &vsc_cpumask); // run task in core 2
    cpumask_set_cpu(3, &vsc_cpumask); // run task in core 3
    sched_setaffinity(0, &vsc_cpumask);
	current->flags &= ~PF_NOFREEZE;
    while (1)
    {
		msleep(10);

		down(get_vdc_detectsemaphore());
		if(VDC_DetectFlag && VDC_DetectCB) VDC_DetectCB();
		up(get_vdc_detectsemaphore());

 		if (freezing(current))
		{
			try_to_freeze();
		}
		if (kthread_should_stop()) {
         	break;
      	}
    }

    rtd_printk(KERN_INFO, TAG_NAME,"\r\n####v4l2_avd_detect_tsk: exit...####\n");
    do_exit(0);
    return 0;
}

void delete_avd_detect_tsk(void)
{
	int ret;
	if (avd_detect_tsk_running_flag) {
 		ret = kthread_stop(p_avd_detect_task);
 		if (!ret) {
 			p_avd_detect_task = NULL;
 			avd_detect_tsk_running_flag = FALSE;
			rtd_printk(KERN_INFO, TAG_NAME,"delete_avd_detect_tsk thread stopped\n");
 		}
	}
}

void create_avd_detect_tsk(void)
{
	int err;
	if (avd_detect_tsk_running_flag == FALSE) {
		p_avd_detect_task = kthread_create(v4l2_avd_detect_tsk, NULL, "v4l2_avd_detect_tsk");

		if (p_avd_detect_task) {
			wake_up_process(p_avd_detect_task);
			avd_detect_tsk_running_flag = TRUE;
		} else {
			err = PTR_ERR(p_avd_detect_task);
			rtd_printk(KERN_INFO, TAG_NAME,"Unable to start p_avd_detect_task (err_id = %d)./n", err);
		}
	}
}
#endif

/* avd function */
int avd_v4l2_atv_connect(unsigned short PortNumber)
{
	KADP_VFE_AVD_DEMOD_TYPE tDemodType = Scaler_AVD_GetDemodType();

	if (KADP_AVD_INTERNAL_DEMOD == tDemodType){

		if(PortNumber == 0){
			set_AVD_Input(_TV_INPUT1);
		}else if(PortNumber == 1){
			set_AVD_Input(_TV_INPUT2);
		}else if(PortNumber == 2)
		{
			set_AVD_Input(_TV_INPUT3);
		}else{
			rtd_printk(KERN_INFO, TAG_NAME,"#####[%s(%d)]Error Port Number!\n", __func__, __LINE__);
			return -EINVAL;
		}
	}else if (KADP_AVD_EXTERNAL_DEMOD == tDemodType){

		if(PortNumber == 0){
			set_AVD_Input(_AV_INPUT1);
		}else if(PortNumber == 1){
			set_AVD_Input(_AV_INPUT2);
		}else if(PortNumber == 2){
			set_AVD_Input(_AV_INPUT3);
		}else{
			rtd_printk(KERN_INFO, TAG_NAME,"#####[%s(%d)]Error Port Number!\n",__func__,__LINE__);
			return -EINVAL;
		}
	}else{
		rtd_printk(KERN_INFO, TAG_NAME,"#####[%s(%d)] demodType error. DemodType=%d\n", __func__, __LINE__, tDemodType);
	}

	rtd_printk(AVD_DEBUG_LEVEL, TAG_NAME,"#####[%s(%d)]tDemodType=%d,PortNumber=%d\n",__func__,__LINE__,tDemodType,PortNumber);

	if (0 == Scaler_AVD_ATV_Connect(get_AVD_Input())){

		set_AVD_Global_Status(SRC_CONNECT_DONE);
		set_ATV_Global_Status(SRC_CONNECT_DONE);
		VDC_set_detect_flag(TRUE);

		if (KADP_AVD_INTERNAL_DEMOD == tDemodType){
			rtd_printk(KERN_INFO, TAG_NAME,"#####[%s(%d)] Internal demod\n", __func__, __LINE__);
			VDC_register_callback(TRUE, Scaler_AVD_DetectTiming);
		}else if (KADP_AVD_EXTERNAL_DEMOD == tDemodType){
			rtd_printk(KERN_INFO, TAG_NAME,"#####[%s(%d)] External demod\n", __func__, __LINE__);
			VDC_register_callback(TRUE, Scaler_AVD_DetectTiming);
		}

		set_AVD_Input_Source(_SRC_TV);
		rtd_printk(KERN_INFO, TAG_NAME,"#####[%s(%d)] Scaler_AVD_ATV_Connect success\n", __func__, __LINE__);
		return 0;
	}else{
		rtd_printk(KERN_INFO, TAG_NAME,"#####[%s(%d)] Scaler_AVD_ATV_Connect fail\n", __func__, __LINE__);
		return -EFAULT;
	}
}

int avd_v4l2_av_connect(unsigned short port)
{
	if(port == 0){
		set_AVD_Input(_AV_INPUT1); //VIN4P
	}else if(port == 1){
		set_AVD_Input((PLATFORM_KXL == get_platform())?_AV_INPUT3:_AV_INPUT1);
	}else if(port == 2){
		set_AVD_Input((PLATFORM_KXL == get_platform())?_AV_INPUT3:_AV_INPUT1);
		//#define PLATFORM_KXLP	PLATFORM_K6LP, VIN4P (_AV_INPUT1),
		//#define PLATFORM_KXL	PLATFORM_K6HP, Yellow port VIN1P (_AV_INPUT3)
		//LG SoCTS script default use p2, so use platform judge pinmux
	}else{
		rtd_printk(KERN_INFO, TAG_NAME,"#####[%s(%d)]Error Port Number!\n",__func__,__LINE__);
		return -EINVAL;//Port number is wrong
		//set_AVD_Input(_AV_INPUT1); //VIN4P
	}

	rtd_printk(KERN_NOTICE, TAG_NAME,"#####[%s(%d)]port=%d\n",__func__,__LINE__,port);

	if (0 == Scaler_AVD_AV_Connect(get_AVD_Input())){

		down(get_vdc_detectsemaphore());
		set_AVD_Global_Status(SRC_CONNECT_DONE);
		set_AV_Global_Status(SRC_CONNECT_DONE);
		set_AVD_Input_Source(_SRC_CVBS);
		VDC_set_detect_flag(TRUE);
		VDC_register_callback(TRUE, Scaler_AVD_DetectTiming);
		rtd_printk(KERN_INFO, TAG_NAME, "%s(%d) Scaler_AVD_AV_Connect() success\n", __func__, __LINE__);
		up(get_vdc_detectsemaphore());
		return 0;
	}else{
		rtd_printk(KERN_INFO, TAG_NAME, "%s(%d) Scaler_AVD_AV_Connect() fail\n", __func__, __LINE__);
		return -EFAULT;
	}
}

/* ------------------------------------------------------------------------
 * V4L2 file operations
 */
 /*V4L2 open(device_name, O_RDWR); avd_v4l2_open
 *This function makes AVD HW resource enabled.
 *Other functions of AVD operate normally only when AVD is enabled by this function.
 *On success 0 is returned.
 *On error -1 and the errno variable is set appropriately.
 *The generic error codes are described at the Generic Error Codes chapter.
 */
static int avd_v4l2_driver_open(void)
{
	int ret = 0;

	if(SRC_NOTHING != get_AVD_Global_Status()){

		rtd_printk(KERN_INFO, TAG_NAME,"[%s(%d)] init fail, source is not SRC_NOTHING\n", __func__, __LINE__);
		ret = -EINVAL;
		return ret;
	}else{

		//rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) AVD_Global_Status is SRC_NOTHING!\n", __func__, __LINE__);
		Scaler_AVD_Init(KADP_VFE_AVD_CC_AND_TTX_ARE_SUPPORTED);
		set_AVD_Global_Status(SRC_INIT_DONE);
	}

	if (SRC_INIT_DONE != get_AVD_Global_Status()){

		rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) fail, AVD_Global_Status=%d\n",__func__, __LINE__,get_AVD_Global_Status());
		ret = -EINVAL;
		return ret;
	}else{
		if (0 == Scaler_AVD_Open()){
			set_AVD_Global_Status(SRC_OPEN_DONE);
			//rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) Success,\n", __func__, __LINE__);
			ret = 0;
		}else{
			rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) fail!\n", __func__, __LINE__);
			ret= -EFAULT;
			return ret;
		}
	}

	return ret;
}
/*V4L2 close(fd);avd_v4l2_release
*This function makes close the video device of AVD.
*Other functions of AVD don't operate normally and error is returned after a AVD is closed.
*all timing information should be reset to '0' when AVD is close by this function.
*On success 0 is returned.
*On error -1 and the errno variable is set appropriately.
*The generic error codes are described at the Generic Error Codes chapter.
*/
static int avd_v4l2_driver_close(void)
{
	int ret = 0;
	if (SRC_OPEN_DONE != get_AVD_Global_Status())
	{
		rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) fail, AVD_Global_Status=%d\n",__func__, __LINE__,get_AVD_Global_Status());
		ret = -EINVAL;
		return ret;
	}else{
		VDC_set_detect_flag(FALSE);
		set_AVD_Global_Status(SRC_INIT_DONE);
		ret = Scaler_AVD_Close();
		rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) Success-1,\n", __func__, __LINE__);
	}

	if(SRC_INIT_DONE != get_AVD_Global_Status()){
		rtd_printk(KERN_INFO, TAG_NAME,"[%s(%d)] fail, source is not SRC_INIT_DONE\n", __func__, __LINE__);
		ret = -EINVAL;
		return ret;
	}else{
		set_AVD_Global_Status(SRC_NOTHING);
		rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) Success-2,\n", __func__, __LINE__);
	}

	return ret;
}

static int avd_v4l2_open(struct file *file)
{
	int ret = 0;
	rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) device success.\n", __func__, __LINE__);
	return ret;
}

static int avd_v4l2_release(struct file *file)
{
	int ret = 0;
	//rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) Success,\n", __func__, __LINE__);
	return ret;
}

static ssize_t avd_v4l2_read(struct file *file, char __user *data, size_t count, loff_t *ppos)
{
	int ret = 0;
	//rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) Success,\n", __func__, __LINE__);
	return ret;
}

static unsigned int avd_v4l2_poll(struct file *file, poll_table *wait)
{
	int ret = 0;
	//rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) Success,\n", __func__, __LINE__);
	return ret;
}

static int avd_v4l2_mmap(struct file *file, struct vm_area_struct *vma)
{
	int ret = 0;
	//rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) Success,\n", __func__, __LINE__);
	return ret;
}

/* ------------------------------------------------------------------------
 * V4L2 ioctl Command
 */
/*ioctl command:VIDIOC_S_INPUT // Set Input
*parameter:
*V4L2_EXT_AVD_INPUT_SRC_NONE,
*V4L2_EXT_AVD_INPUT_SRC_ATV,
*V4L2_EXT_AVD_INPUT_SRC_AV,
*V4L2_EXT_AVD_INPUT_SRC_AVD_RESERVED1,
*V4L2_EXT_AVD_INPUT_SRC_AVD_RESERVED2,
*Connect(V4L2_EXT_AVD_INPUT_SRC_AV) : This function determines which external input AVD will be decoded in ATV / AV.
*Disconnect(V4L2_EXT_AVD_INPUT_SRC_NONE) : This function disconnects the connection between external input (ATV, AV) and Decoder (AVD).
*If this function is called, decoding will not occur even if close is not called yet. When AVD disconnect, all timing information should be reset to '0'.
*On success 0 is returned.
*On error -1 and the errno variable is set appropriately.
*The generic error codes are described at the Generic Error Codes chapter.
*/
static int avd_ioctl_s_input(struct file *file, void *priv, unsigned int input)
{
	int ret = 0;
	switch (input){
		case V4L2_EXT_AVD_INPUT_SRC_NONE:
			if (SRC_CONNECT_DONE != get_AVD_Global_Status()){

				if(SRC_OPEN_DONE == get_AVD_Global_Status()){
					rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) avd already disconnect, source is SRC_OPEN_DONE\n",__func__, __LINE__);
					return ret;
				}

				rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) avd disconnect fail, AVD_Global_Status=%d\n",__func__, __LINE__,get_AVD_Global_Status());
				ret = -EINVAL;
				return ret;
			}

			if(g_apSrcIndex == V4L2_EXT_AVD_INPUT_SRC_ATV){

				if (SRC_CONNECT_DONE != get_ATV_Global_Status()){
					rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) AV disconnect fail, ATV_Status=%d\n",__func__, __LINE__,get_ATV_Global_Status());
					ret = -EINVAL;
					return ret;
				}

				VDC_set_detect_flag(FALSE);
				Scaler_AVD_ATV_Disconnect();
				set_AVD_Global_Status(SRC_OPEN_DONE);
				set_ATV_Global_Status(SRC_OPEN_DONE);
				set_AVD_Input_Source(_SRC_MAX);
				Set_Reply_Zero_Timing_Flag(VSC_INPUTSRC_AVD, CLEAR_ZERO_FLAG);	
				rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) ATV disconnect g_apSrcIndex=%d\n", __func__, __LINE__,g_apSrcIndex);
			}else if(g_apSrcIndex == V4L2_EXT_AVD_INPUT_SRC_AV){

				if (SRC_CONNECT_DONE != get_AV_Global_Status()){
					rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) AV disconnect fail, AV_Status=%d\n",__func__, __LINE__,get_AV_Global_Status());
					ret = -EINVAL;
					return ret;
				}

				VDC_set_detect_flag(FALSE);
				Scaler_AVD_AV_Disconnect();
				set_AVD_Global_Status(SRC_OPEN_DONE);
				set_AV_Global_Status(SRC_OPEN_DONE);
				set_AVD_Input_Source(_SRC_MAX);
				Set_Reply_Zero_Timing_Flag(VSC_INPUTSRC_AVD, CLEAR_ZERO_FLAG);
				rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) AV disconnect g_apSrcIndex=%d\n", __func__, __LINE__,g_apSrcIndex);
			}else{
				rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) V4L2_EXT_AVD_INPUT_SRC_NONE disconnect g_apSrcIndex=%d fail!\n", __func__, __LINE__,g_apSrcIndex);
			}

			g_apSrcIndex = input;
			v4l2_avd_open_count = 0;
			avd_v4l2_driver_close();//call avd driver close
			break;
		case V4L2_EXT_AVD_INPUT_SRC_ATV:
			avd_v4l2_driver_open();//call avd driver open

			if (SRC_OPEN_DONE != get_AVD_Global_Status()){

				if(SRC_CONNECT_DONE == get_AVD_Global_Status()){
					rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) avd already connect, source is SRC_CONNECT_DONE\n",__func__, __LINE__);
					return ret;
				}

				rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) fail, AVD_Global_Status=%d\n",__func__, __LINE__,get_AVD_Global_Status());
				ret = -EINVAL;
				return ret;
			}
			g_apSrcIndex = input;
			v4l2_avd_open_count ++;
			ret = avd_v4l2_atv_connect(g_apPortIndex);
			break;
		case V4L2_EXT_AVD_INPUT_SRC_AV:
			avd_v4l2_driver_open();//call avd driver open

			if(SRC_OPEN_DONE != get_AVD_Global_Status()){

				if(SRC_CONNECT_DONE == get_AVD_Global_Status()){
					rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) avd already connect, source is SRC_CONNECT_DONE\n",__func__, __LINE__);
					return ret;
				}

				rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) fail, AVD_Global_Status=%d\n",__func__,__LINE__,get_AVD_Global_Status());
				ret = -EINVAL;
				return ret;
			}

			g_apSrcIndex = input;
			v4l2_avd_open_count ++;
			ret = avd_v4l2_av_connect(g_apPortIndex);
			break;
		case V4L2_EXT_AVD_INPUT_SRC_AVD_RESERVED1:
			g_apSrcIndex = input;
			break;
		case V4L2_EXT_AVD_INPUT_SRC_AVD_RESERVED2:
			g_apSrcIndex = input;
			break;
		default:
			ret = -EINVAL;
	}
	rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) VIDIOC_S_INPUT g_apSrcIndex=%d\n", __func__, __LINE__,g_apSrcIndex);
	return ret;
}

/*ioctl command:VIDIOC_G_INPUT //Get Input
*On success 0 is returned.
*On error -1 and the errno variable is set appropriately.
*The generic error codes are described at the Generic Error Codes chapter.
*/
static int avd_ioctl_g_input(struct file *file, void *priv, unsigned int *input)
{
	int ret = 0;
	switch(get_AVD_Input_Source()){
		case _SRC_CVBS:
			*input = V4L2_EXT_AVD_INPUT_SRC_AV;
			break;
		case _SRC_TV:
			*input = V4L2_EXT_AVD_INPUT_SRC_ATV;
			break;
		default:
			*input = V4L2_EXT_AVD_INPUT_SRC_NONE;
			break;
		}

	rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) VIDIOC_G_INPUT input=%d\n", __func__, __LINE__,*input);
	return ret;
}

/*ioctl command:VIDIOC_S_CTRL
*1)control id: V4L2_CID_EXT_AVD_PORT
*Select the HW port that is actually connected when AVD Connect.
*It is an item that changes depending on the circuit configuration and should be called before VIDIOC_S_INPUT.
*This value is related with "Ext. Input Adjust" Video Index in the Adjust menu.
*control value:0 ~ 255

*2)control id: V4L2_CID_EXT_AVD_AUTO_TUNING_MODE
*This function controls the sensitivity of V4L2_CID_EXT_AVD_VIDEO_SYNC.
*if V4L2_CID_EXT_AVD_AUTO_TUNING_MODE is necessary it used for AutoTuning in FE-Tuner Side because avoid tuning garbage channel.
*This function is called with "True" when ATV-Auto-Channel-Searching is performed.
*This function controls the sensitivity of detecting Analog-Video-Decoder Sync-Lock.
*When V4L2_CID_EXT_AVD_AUTO_TUNING_MODE is on, the sensitivity of Sync-detection becomes lower than before so that garbage channel may not be searched.
*During ATV-Auto-Channel-Searching, V4L2_CID_EXT_AVD_AUTO_TUNING_MODE is on.(if it necessary)
*During Normal-User-Using condition, V4L2_CID_EXT_AVD_AUTO_TUNING_MODE is off.
*this function is only for ATV, so it should not affect Composite and Scart.
control value:0 (Off), 1 (On)

*3)control id: V4L2_CID_EXT_AVD_VIDEO_SYNC
*This function says whether Sync exists or Not.
*The sensitivity of  V4L2_CID_EXT_AVD_VIDEO_SYNC is influenced by V4L2_CID_EXT_AVD_AUTO_TUNING_MODE.
*control value:0 (AVD Sync Not Locked), 1 (AVD Sync Locked)

*4)control id: V4L2_CID_EXT_AVD_ATV_CHANNEL_CHANGE
*This function says whether channel change start or Not.
*this function is only for ATV, so it should not affect Composite and Scart.
*control value:0 (no channel change stage), 1 (channel change start)

*5)control id: V4L2_CID_EXT_AVD_ATV_DEMOD_TYPE
*This function says which atv demod type.
*this function is only for ATV, so it should not affect Composite and Scart.
*control value:0 (internal demod), 1 (external demod)

*On success 0 is returned.
*On error -1 and the errno variable is set appropriately.
*The generic error codes are described at the Generic Error Codes chapter.
*/
static int avd_ioctl_s_ctrl(struct file *file, void *priv, struct v4l2_control *vc)
{
	int ret = 0;

	//rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) control.id = %d \n",__func__, __LINE__,vc->id);
	//rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) control.value = %d \n",__func__, __LINE__,vc->value);

	switch (vc->id) {
		case V4L2_CID_EXT_AVD_PORT:
			if(vc->value < 0 || vc->value > 255){
				ret = -EINVAL;
				rtd_printk(KERN_EMERG, TAG_NAME,"vc->value= %d value out of range\n",vc->value);
				break;
			}else{
				g_apPortIndex = vc->value;
			}

			rtd_printk(AVD_DEBUG_LEVEL, TAG_NAME,"#####[%s(%d)] V4L2_CID_EXT_AVD_PORT, set port = %d\n",__func__, __LINE__,vc->value);
			break;

		case V4L2_CID_EXT_AVD_AUTO_TUNING_MODE:
			if(vc->value <0 || vc->value >1){
				ret = -EINVAL;
				rtd_printk(KERN_EMERG, TAG_NAME,"vc->value= %d value out of range\n",vc->value);
				break;
			}else{
				g_apAutoTuning = vc->value;
			}
			break;

		case V4L2_CID_EXT_AVD_VIDEO_SYNC:
			break;

		case V4L2_CID_EXT_AVD_CHANNEL_CHANGE:
			if(vc->value == 1){

				if (0 == Scaler_AVD_SetChannelChange()){

					ret = 0;
				}else{

					rtd_printk(KERN_INFO, TAG_NAME, "#####[%s(%d)] Scaler_AVD_SetChannelChange Fail\n",__func__,__LINE__);
					ret = -EINVAL;
				}
			}else{

				rtd_printk(KERN_EMERG, TAG_NAME,"#####[%s(%d)] vc->value= %d out of range\n",__func__,__LINE__,vc->value);
				ret = -EINVAL;
			}

			break;

		case V4L2_CID_EXT_AVD_ATV_DEMOD_TYPE:
			if(vc->value == 0){
				tDemodType = KADP_AVD_INTERNAL_DEMOD;
			}else if (vc->value == 1){
				tDemodType = KADP_AVD_EXTERNAL_DEMOD;
			}else{
				rtd_printk(KERN_EMERG, TAG_NAME,"vc->value= %d value out of range\n",vc->value);
				ret = -EINVAL;
				break;
			}
			if (0 == Scaler_AVD_SetDemodType(tDemodType)){
				ret = 0;
			}else{
				rtd_printk(KERN_INFO, TAG_NAME, "Kernel Scaler_AVD_SetDemodType Fail\n");
				ret = -EINVAL;
			}
			break;
		default:
			ret = -EINVAL;
	}
	rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) VIDIOC_S_CTRL ret=%d,\n", __func__, __LINE__,ret);
	return ret;
}

/*ioctl command:VIDIOC_G_CTRL
*Description see VIDIOC_S_CTRL
*On success 0 is returned.
*On error -1 and the errno variable is set appropriately.
*The generic error codes are described at the Generic Error Codes chapter.
*/
static int avd_ioctl_g_ctrl(struct file *file, void *priv, struct v4l2_control *vc)
{
	int ret = 0;

	//rtd_printk(KERN_INFO, TAG_NAME,"%s(%d)control.id = %d \n",__func__, __LINE__,vc->id);

	switch (vc->id) {
		case V4L2_CID_EXT_AVD_PORT:
			vc->value = g_apPortIndex;
			break;

		case V4L2_CID_EXT_AVD_AUTO_TUNING_MODE:
			vc->value = g_apAutoTuning;
			break;

		case V4L2_CID_EXT_AVD_VIDEO_SYNC:
			vc->value = Scaler_AVD_DoesSyncExist();
			break;
		case V4L2_CID_EXT_AVD_CHANNEL_CHANGE:
			//vc->value = Scaler_AVD_GetIsChannelChange();
			break;
		case V4L2_CID_EXT_AVD_ATV_DEMOD_TYPE:
			vc->value = Scaler_AVD_GetDemodType();
			break;

		default:
			ret = -EINVAL;
	}

	//rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) VIDIOC_G_CTRL ret=%d vc->value=%d,\n", __func__, __LINE__,ret,vc->value);
	return ret;
}

/*ioctl command:VIDIOC_S_STD
*This function selects the color-system that should be supported.
*If you do not select color-system with this function and the composite signal is applied to the TV-Set, the image will be displayed abnormally.
*- When AVD is open, this function sets the color system.
*- If you select color system (NTSC, PAL-N, PAL-M) as a user menu in a region that supports Muti-Color-System, you can set this function through this function and select VBI function (TTX, CC, Rating , ...) are supported.
*- at ATV, it has a role similar to the vFreq at the other analog signal. because LGE-MW make a decision whether a signal is good or not by using color-system.
*it checks this function periodically, whether this information is changed or not.
*if it is changed from unknown to some valid color-system, we will decide current signal is stable
*if it is changed from unknown to "Known-standard", it unMute Video.
*- This means that the speed of recognizing Colorsystem affects the ATV-Channel-Change-Time.
*On success 0 is returned.
*On error -1 and the errno variable is set appropriately.
*The generic error codes are described at the Generic Error Codes chapter.
*/
static int avd_ioctl_s_std(struct file *file, void *priv, v4l2_std_id tvnorms)
{
	int ret = 0,g_ScalerColorSys=0xff;

	if(tvnorms == 0xffffffff){
		// V4L2_STD_ALL = 0x00ffffff
		rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) VIDIOC_S_STD tvnorms=0x%08x abnormal!!!\n", __func__, __LINE__, (unsigned int)tvnorms);
		ret = -EINVAL;
		return ret;
	}

	rtd_printk(KERN_INFO, TAG_NAME, "%s(norm = 0x%08x) name: [%s]\n",
		__func__,
		(unsigned int)tvnorms,
		(tvnorms==0x00ffb0ff)?"DVB-T":v4l2_norm_to_name(tvnorms));

	g_apColorSys = tvnorms;

	switch(tvnorms){
		/* one bit for each */
		case V4L2_STD_PAL:		//0x000000ff
		case V4L2_STD_PAL_DK:	//0x00000020 | 0x00000040 | 0x00000080
		case V4L2_STD_PAL_BG:	//0x00000001 | 0x00000002 | 0x00000004
		case V4L2_STD_PAL_B:	//0x00000001
		case V4L2_STD_PAL_B1:	//0x00000002
		case V4L2_STD_PAL_G:	//0x00000004
		case V4L2_STD_PAL_H:	//0x00000008
		case V4L2_STD_PAL_I:	//0x00000010
		case V4L2_STD_PAL_D:	//0x00000020
		case V4L2_STD_PAL_D1:	//0x00000040
		case V4L2_STD_PAL_K:	//0x00000080
			g_ScalerColorSys = KADP_COLOR_SYSTEM_PAL_G;
			rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) VIDIOC_S_STD tvnorms=V4L2_STD_PAL, g_ScalerColorSys=0x%x\n", __func__, __LINE__, g_ScalerColorSys);
			break;

		case V4L2_STD_PAL_M:	//0x00000100
			g_ScalerColorSys = KADP_COLOR_SYSTEM_PAL_M;
			rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) VIDIOC_S_STD tvnorms=V4L2_STD_PAL_M, g_ScalerColorSys=0x%x\n", __func__, __LINE__, g_ScalerColorSys);
			break;

		case V4L2_STD_PAL_N:	//0x00000200
			g_ScalerColorSys = KADP_COLOR_SYSTEM_PAL_N;
			rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) VIDIOC_S_STD tvnorms=V4L2_STD_PAL_N, g_ScalerColorSys=0x%x\n", __func__, __LINE__, g_ScalerColorSys);
			break;

		case V4L2_STD_PAL_Nc:	//0x00000400
			g_ScalerColorSys = KADP_COLOR_SYSTEM_PAL_NC_358;
			rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) VIDIOC_S_STD tvnorms=V4L2_STD_PAL_Nc, g_ScalerColorSys=0x%x\n", __func__, __LINE__, g_ScalerColorSys);
			break;

		case V4L2_STD_PAL_60:	//0x00000800
			g_ScalerColorSys = KADP_COLOR_SYSTEM_PAL_60;
			rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) VIDIOC_S_STD tvnorms=V4L2_STD_PAL_60, g_ScalerColorSys=0x%x\n", __func__, __LINE__, g_ScalerColorSys);
			break;

		/* ATSC/HDTV */
		case V4L2_STD_ATSC_8_VSB://0x01000000
		case V4L2_STD_ATSC_16_VSB://0x02000000
		case V4L2_STD_NTSC:		//0x0000b000
		case V4L2_STD_NTSC_M:	//0x00001000
		case V4L2_STD_NTSC_M_JP://0x00002000
		case V4L2_STD_NTSC_M_KR://0x00008000
			g_ScalerColorSys = KADP_COLOR_SYSTEM_NTSC_M;
			rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) VIDIOC_S_STD tvnorms=V4L2_STD_NTSC, g_ScalerColorSys=0x%x\n", __func__, __LINE__, g_ScalerColorSys);
			break;

		case V4L2_STD_NTSC_443:	//0x00004000
			g_ScalerColorSys = KADP_COLOR_SYSTEM_NTSC_443;
			rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) VIDIOC_S_STD tvnorms=V4L2_STD_NTSC_443, g_ScalerColorSys=0x%x\n", __func__, __LINE__, g_ScalerColorSys);
			break;

		/* All Secam Standards */
		case V4L2_STD_SECAM:	//0x00ff0000
		case V4L2_STD_SECAM_DK:	//0x00020000 | 0x00100000 | 0x00200000
		case V4L2_STD_SECAM_B:	//0x00010000
		case V4L2_STD_SECAM_D:	//0x00020000
		case V4L2_STD_SECAM_G:	//0x00040000
		case V4L2_STD_SECAM_H:	//0x00080000
		case V4L2_STD_SECAM_K:	//0x00100000
		case V4L2_STD_SECAM_K1:	//0x00200000
		case V4L2_STD_SECAM_L:	//0x00400000
		case V4L2_STD_SECAM_LC:	//0x00800000
		case V4L2_STD_L:		//0x00400000 | 0x00800000
			g_ScalerColorSys = KADP_COLOR_SYSTEM_SECAM;
			rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) VIDIOC_S_STD tvnorms=V4L2_STD_SECAM, g_ScalerColorSys=0x%x\n", __func__, __LINE__, g_ScalerColorSys);
			break;

		case V4L2_STD_B:
		case V4L2_STD_G:
		case V4L2_STD_H:
		case V4L2_STD_GH:
		case V4L2_STD_DK:
		case V4L2_STD_BG:
			g_ScalerColorSys = KADP_COLOR_SYSTEM_PAL_G |
							   KADP_COLOR_SYSTEM_SECAM;
			rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) VIDIOC_S_STD tvnorms=V4L2_STD_B or V4L2_STD_G, g_ScalerColorSys=0x%x\n", __func__, __LINE__, g_ScalerColorSys);
			break;

		/* Standards where MTS/BTSC stereo could be found */
		case V4L2_STD_MTS:
		case V4L2_STD_MN:
			g_ScalerColorSys = KADP_COLOR_SYSTEM_PAL_M |\
							   KADP_COLOR_SYSTEM_PAL_N |\
							   KADP_COLOR_SYSTEM_PAL_NC_358 |\
							   KADP_COLOR_SYSTEM_NTSC_M;
			rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) VIDIOC_S_STD tvnorms=V4L2_STD_MN or V4L2_STD_MTS, g_ScalerColorSys=0x%x\n", __func__, __LINE__, g_ScalerColorSys);
			break;

		/* Standards for Countries with 60Hz Line frequency */
		case V4L2_STD_525_60:
			g_ScalerColorSys = KADP_COLOR_SYSTEM_PAL_M |\
							   KADP_COLOR_SYSTEM_PAL_60 |\
							   KADP_COLOR_SYSTEM_NTSC_443 |\
							   KADP_COLOR_SYSTEM_NTSC_M;
			rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) VIDIOC_S_STD tvnorms=V4L2_STD_525_60, g_ScalerColorSys=0x%x\n", __func__, __LINE__, g_ScalerColorSys);
			break;

		/* Standards for Countries with 50Hz Line frequency */
		case V4L2_STD_625_50:
			g_ScalerColorSys = KADP_COLOR_SYSTEM_PAL_G |\
							   KADP_COLOR_SYSTEM_PAL_N |\
							   KADP_COLOR_SYSTEM_PAL_NC_358 |\
							   KADP_COLOR_SYSTEM_SECAM;
			rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) VIDIOC_S_STD tvnorms=V4L2_STD_625_50, g_ScalerColorSys=0x%x\n", __func__, __LINE__, g_ScalerColorSys);
			break;

		case V4L2_STD_ATSC:		//0x00ffffff
			g_ScalerColorSys = KADP_COLOR_SYSTEM_NTSC_M;
			rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) VIDIOC_S_STD tvnorms=V4L2_STD_ATSC, g_ScalerColorSys=0x%x\n", __func__, __LINE__, g_ScalerColorSys);
			break;

		case V4L2_STD_PAL|V4L2_STD_NTSC|V4L2_STD_SECAM://DVBT = 0x000000ff | 0x0000b000 | 0x00ff0000 = 0x00ffb0ff
			g_ScalerColorSys = KADP_COLOR_SYSTEM_NTSC_M | KADP_COLOR_SYSTEM_PAL_G | KADP_COLOR_SYSTEM_SECAM;
			rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) VIDIOC_S_STD tvnorms=V4L2_STD_DVBT, g_ScalerColorSys=0x%x\n", __func__, __LINE__, g_ScalerColorSys);
			break;

		case V4L2_STD_ALL^V4L2_STD_PAL_N:	//0x00ffdfff, for DVBT AV (ref. KTASKWBS-10781)
			g_ScalerColorSys = KADP_COLOR_SYSTEM_PAL_G |\
							   KADP_COLOR_SYSTEM_PAL_NC_358 |\
							   KADP_COLOR_SYSTEM_SECAM|\
							   KADP_COLOR_SYSTEM_PAL_M |\
							   KADP_COLOR_SYSTEM_PAL_60 |\
							   KADP_COLOR_SYSTEM_NTSC_443 |\
							   KADP_COLOR_SYSTEM_NTSC_M;
			rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) VIDIOC_S_STD for DVBT AV, tvnorms=0x00ffdfff, g_ScalerColorSys=0x%x\n", __func__, __LINE__, g_ScalerColorSys);
			break;

		case V4L2_STD_ALL:		//0x00ffffff
			g_ScalerColorSys = KADP_COLOR_SYSTEM_PAL_G |\
							   KADP_COLOR_SYSTEM_PAL_N |\
							   KADP_COLOR_SYSTEM_PAL_NC_358 |\
							   KADP_COLOR_SYSTEM_SECAM|\
							   KADP_COLOR_SYSTEM_PAL_M |\
							   KADP_COLOR_SYSTEM_PAL_60 |\
							   KADP_COLOR_SYSTEM_NTSC_443 |\
							   KADP_COLOR_SYSTEM_NTSC_M;
			rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) VIDIOC_S_STD tvnorms=V4L2_STD_ALL, g_ScalerColorSys=0x%x\n", __func__, __LINE__, g_ScalerColorSys);
			break;

		case V4L2_STD_NTSC|V4L2_STD_PAL_Nc|V4L2_STD_PAL_M://Multi = 0x0000b000 | 0x00000400 | 0x00000100 = 0x0000b500 (ref. WOSQRTK-12586)
			g_ScalerColorSys = KADP_COLOR_SYSTEM_NTSC_M | KADP_COLOR_SYSTEM_PAL_NC_358 | KADP_COLOR_SYSTEM_PAL_M;
			rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) VIDIOC_S_STD for Multi case, tvnorms=0x0000b500, g_ScalerColorSys=0x%x\n", __func__, __LINE__, g_ScalerColorSys);
			break;

		/* Macros with none and all analog standards */
		case V4L2_STD_UNKNOWN:
		default:
			g_ScalerColorSys = KADP_COLOR_SYSTEM_UNKNOWN;
			rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) VIDIOC_S_STD tvnorms=V4L2_STD_UNKNOWN,g_ScalerColorSys=0x%x\n", __func__, __LINE__, g_ScalerColorSys);
			break;
	}

	Scaler_AVD_SetLGEColorSystem(g_ScalerColorSys);

	return ret;
}

/*ioctl command:VIDIOC_G_STD
*Description see VIDIOC_S_STD
*On success 0 is returned.
*On error -1 and the errno variable is set appropriately.
*The generic error codes are described at the Generic Error Codes chapter.
*/
static int avd_ioctl_g_std(struct file *file, void *priv, v4l2_std_id *tvnorm)
{
	int ret = 0;

	Scaler_AVD_GetLGEColorSystem(&ColorSystem2);

	if(get_avd_print_flag()){
		rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) VIDIOC_G_STD ColorSystem2=%d,\n", __func__, __LINE__,ColorSystem2);
	}

	switch(ColorSystem2)
	{
		case KADP_VIDEO_AVDEC_MODE_NTSC:
			*tvnorm = V4L2_STD_NTSC;
			break;
		case KADP_VIDEO_AVDEC_MODE_PAL:
			*tvnorm = V4L2_STD_PAL;
			break;
		case KADP_VIDEO_AVDEC_MODE_PAL_NC_358:
			*tvnorm = V4L2_STD_PAL_Nc;
			break;
		case KADP_VIDEO_AVDEC_MODE_PAL_M:
			*tvnorm = V4L2_STD_PAL_M;
			break;
		case KADP_VIDEO_AVDEC_MODE_SECAM:
			*tvnorm = V4L2_STD_SECAM;
			break;
		case KADP_VIDEO_AVDEC_MODE_NTSC_443:
			*tvnorm = V4L2_STD_NTSC_443;
			break;
		case KADP_VIDEO_AVDEC_MODE_PAL_60:
			*tvnorm = V4L2_STD_PAL_60;
			break;
		case KADP_VIDEO_AVDEC_MODE_UNKNOWN_525:
		case KADP_VIDEO_AVDEC_MODE_UNKNOWN_625:
		case KADP_VIDEO_AVDEC_MODE_UNKNOWN:
		default:
			*tvnorm = g_apColorSys|V4L2_STD_UNKNOWN;
			break;
	}

	//rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) VIDIOC_G_STD end tvnorm=%s,\n", __func__, __LINE__,v4l2_norm_to_name(*tvnorm));
	return ret;
}

/*ioctl command:VIDIOC_S_EXT_CTRLS
*On success 0 is returned.
*On error -1 and the errno variable is set appropriately.
*The generic error codes are described at the Generic Error Codes chapter.
*/
static int avd_ioctl_s_ext_ctrls(struct file *file, void *priv,struct v4l2_ext_controls *ctls)
{
	int ret = 0;
	rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) VIDIOC_S_EXT_CTRLS Success,\n", __func__, __LINE__);
	return ret;
}

/*ioctl command:VIDIOC_G_EXT_CTRLS
*control id: V4L2_CID_EXT_AVD_TIMING_INFO
*This function carries the information of the input signal of the current Analog Video Decoder.
*If AVD is in the open / connect state and there is no signal change,
*decoding should continue and Timing information should be maintained even if VSC (main / sub scaler) is connected / disconnected.
*When the signal is physically changed (no signal-> good signal), all timing information is stabilized and updated at once.
*When input is switched to analog TV, active.w and active.h should be fixed value as follows, The other hFreq, vFreq,
*hPorch, and vProch must deliver the physical values detected in the real HW.
*This function is related to the color system setting of the VIDIOC_S_STD function.
*When the NTSC system is set up with the above function,if 576i-PAL-Signal is applied to the analog TV input,
*480i should be returned, If the PAL series is set, if 480i-NTSC-signal is applied to the analog TV input, 576i should be returned.
*If analog TV input of TV model supporting PAL 576i and NTSC 480i (PAL-M, PAL-N, NTSC) is white noise (no signal),
*active.w and active.h transmit the last valid signal information,
*If there is no valid signal information when the TV is turned on in white noise (no signal) state,
*active.w should transmit 704 and active.h should transmit 480.
*When AVD disconnect and close, all timing information should be reset to '0'.
*	 active.x active.y active.w active.h
*NTSC	0		0		704			480
*PAL	0		0		704			576

*On success 0 is returned.
*On error -1 and the errno variable is set appropriately.
*The generic error codes are described at the Generic Error Codes chapter.
*/
static int avd_ioctl_g_ext_ctrls(struct file *file, void *priv,struct v4l2_ext_controls *ctls)
{
	int ret = 0;
	extern unsigned char AVD_Reply_Zero_Timing;
	struct v4l2_ext_control ctrl;
	struct v4l2_ext_avd_timing_info v4l2_avd_timing_info;
	//struct v4l2_ext_video_rect avd_video_rect;
	KADP_VFE_AVD_TIMING_INFO_T *ptTimingInfo = NULL;
	//StructDisplayInfo *ptAVDScalerDispInfo  = NULL;

	if(!ctls)
	{
		rtd_printk(KERN_ERR, TAG_NAME,"func:%s(%d) [error] ctls is null\r\n",__FUNCTION__,__LINE__);
		return -EFAULT;
	}
	if(!ctls->controls)
	{
		rtd_printk(KERN_ERR, TAG_NAME,"func:%s(%d) [error] ctls->controls is null\r\n",__FUNCTION__,__LINE__);
		return -EFAULT;
	}
	memcpy(&ctrl, ctls->controls, sizeof(struct v4l2_ext_control));

	//rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) controls.id  = %d\n", __func__, __LINE__, ctrl.id);
	//rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) controls.size  = %d\n", __func__, __LINE__, ctrl.size);

	switch(ctrl.id)
	{
		case V4L2_CID_EXT_AVD_TIMING_INFO:
			if (SRC_CONNECT_DONE == get_AVD_Global_Status()){
				if(!ctrl.ptr){
					rtd_printk(KERN_ERR, TAG_NAME,"func:%s(%d) [error] ctrl->ptr is null\r\n",__FUNCTION__,__LINE__);
					return -EFAULT;
				}

				if(ctrl.size != sizeof(struct v4l2_ext_avd_timing_info)){
					rtd_printk(KERN_INFO, TAG_NAME,"[%s(%d)] v4l2_avd_timing_info size not match\n", __func__, __LINE__);
					return -EFAULT;
				}

				down(get_vdc_detectsemaphore());
				memset(&v4l2_avd_timing_info, 0 , sizeof(v4l2_avd_timing_info));

				//ptTimingInfo = Scaler_AVD_GetLGETimingInfo((unsigned char*)&ret);
				ptTimingInfo = Get_AVD_LGETiminginfo();
				//ptAVDScalerDispInfo = Get_AVD_ScalerDispinfo();

				if (0 == ret){
					if (AVD_Reply_Zero_Timing & REPORT_ZERO_TIMING){

						Set_Reply_Zero_Timing_Flag(VSC_INPUTSRC_AVD, CLEAR_ZERO_FLAG);
					} else if((ptTimingInfo->active.w != 0)&&(ptTimingInfo->active.h != 0)){
						if(get_avd_print_flag()){
							rtd_printk(KERN_INFO, TAG_NAME,"[%s(%d)] Scaler_AVD_GetLGETimingInfo OK!\r\n",__FUNCTION__,__LINE__);
						}
						v4l2_avd_timing_info.h_freq = ptTimingInfo->hFreq;
						v4l2_avd_timing_info.v_freq = ptTimingInfo->vFreq;
						v4l2_avd_timing_info.h_porch = ptTimingInfo->hPorch;
						v4l2_avd_timing_info.v_porch = ptTimingInfo->vPorch;
						v4l2_avd_timing_info.active.x = ptTimingInfo->active.x;
						v4l2_avd_timing_info.active.y = ptTimingInfo->active.y;
						v4l2_avd_timing_info.active.w = ptTimingInfo->active.w;
						v4l2_avd_timing_info.active.h = ptTimingInfo->active.h;
						v4l2_avd_timing_info.vd_lock = ptTimingInfo->vdLock;
						v4l2_avd_timing_info.h_lock = ptTimingInfo->HLock;
						v4l2_avd_timing_info.v_lock = ptTimingInfo->VLock;
					}else{
						if(get_avd_print_flag()){
							rtd_printk(KERN_INFO, TAG_NAME,"[%s(%d)] width and length is zero.\r\n",__FUNCTION__,__LINE__);
						}
					#if 0//rzhen
						v4l2_avd_timing_info.active.w = 704;
						v4l2_avd_timing_info.active.h = ((g_apColorSys&V4L2_STD_525_60)==V4L2_STD_525_60)?480:576;
						v4l2_avd_timing_info.v_freq = ((g_apColorSys&V4L2_STD_525_60)==V4L2_STD_525_60)?599:500;
						v4l2_avd_timing_info.h_freq = ((g_apColorSys&V4L2_STD_525_60)==V4L2_STD_525_60)?157:156;
					#endif
					}
				}else{
					rtd_printk(KERN_ERR, TAG_NAME,"func:%s(%d) [error] Scaler_AVD_GetLGETimingInfo is null\r\n",__FUNCTION__,__LINE__);
					return -EFAULT;
				}

				up(get_vdc_detectsemaphore());

				#if 0
				if(get_avd_print_flag()){
					rtd_printk(KERN_INFO, TAG_NAME,"avd_timing_info hFreq = %d\n", v4l2_avd_timing_info.h_freq);
					rtd_printk(KERN_INFO, TAG_NAME,"avd_timing_info vFreq = %d\n", v4l2_avd_timing_info.v_freq);
					rtd_printk(KERN_INFO, TAG_NAME,"avd_timing_info hPorch = %d\n", v4l2_avd_timing_info.h_porch);
					rtd_printk(KERN_INFO, TAG_NAME,"avd_timing_info vPorch = %d\n", v4l2_avd_timing_info.v_porch);
					rtd_printk(KERN_INFO, TAG_NAME,"avd_timing_info active w  = %d\n", v4l2_avd_timing_info.active.w);
					rtd_printk(KERN_INFO, TAG_NAME,"avd_timing_info active h  = %d\n", v4l2_avd_timing_info.active.h);
					rtd_printk(KERN_INFO, TAG_NAME,"avd_timing_info active x  = %d\n", v4l2_avd_timing_info.active.x);
					rtd_printk(KERN_INFO, TAG_NAME,"avd_timing_info active y  = %d\n", v4l2_avd_timing_info.active.y);
					rtd_printk(KERN_INFO, TAG_NAME,"avd_timing_info vdLock = %d\n", v4l2_avd_timing_info.vd_lock);
					rtd_printk(KERN_INFO, TAG_NAME,"avd_timing_info HLock = %d\n", v4l2_avd_timing_info.h_lock);
					rtd_printk(KERN_INFO, TAG_NAME,"avd_timing_info VLock = %d\n", v4l2_avd_timing_info.v_lock);
				}
				#endif

				if(copy_to_user(to_user_ptr(ctrl.ptr), &v4l2_avd_timing_info, sizeof(struct v4l2_ext_avd_timing_info))){
					rtd_printk(KERN_ERR, TAG_NAME,"func:%s(%d) [error] copy_to_user fail \r\n",__FUNCTION__,__LINE__);
					return -EFAULT;
				}
			}
			break;

		default:
			ret = -EINVAL;
	}

	//rtd_printk(KERN_INFO, TAG_NAME,"%s(%d) VIDIOC_G_EXT_CTRLS end ret=%d,\n", __func__, __LINE__,ret);
	return ret;
}

const struct v4l2_ioctl_ops avd_ioctl_ops = {

	//Input
	.vidioc_s_input = avd_ioctl_s_input,
	.vidioc_g_input = avd_ioctl_g_input,

	//Sync Status
	.vidioc_s_ctrl = avd_ioctl_s_ctrl,
	.vidioc_g_ctrl = avd_ioctl_g_ctrl,

	//AVD Timinginfo
	.vidioc_s_ext_ctrls = avd_ioctl_s_ext_ctrls,
	.vidioc_g_ext_ctrls = avd_ioctl_g_ext_ctrls,

	//Color System
	.vidioc_s_std = avd_ioctl_s_std,
	.vidioc_g_std = avd_ioctl_g_std,
};

const struct v4l2_file_operations avd_fops = {
	.owner		= THIS_MODULE,
	.open		= avd_v4l2_open,
	.release		= avd_v4l2_release,
	.read		= avd_v4l2_read,
	.poll			= avd_v4l2_poll,
	.unlocked_ioctl  = video_ioctl2, /* V4L2 ioctl handler */
	.mmap		= avd_v4l2_mmap,
};

