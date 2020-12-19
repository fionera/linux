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
#include <linux/wait.h>
#include <linux/version.h>
#include <asm/unaligned.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-common.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>

//common
#include <ioctrl/scaler/vbe_cmd_id.h>
#ifdef CONFIG_KDRIVER_USE_NEW_COMMON
#include <scaler/scalerCommon.h>
#else
#include <scalercommon/scalerCommon.h>
#endif


#include <linux/v4l2-ext/v4l2-controls-ext.h>
#include <linux/v4l2-ext/videodev2-ext.h>

//TVScaler Header file
#include <tvscalercontrol/scaler_v4l2/vbe_v4l2_api.h>

#include <tvscalercontrol/panel/panelapi.h>

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#define to_user_ptr(x)          compat_ptr((unsigned int)x)
#else
#define to_user_ptr(x)          ((void* __user)(x)) // convert 32 bit value to user pointer
#endif


#include <rtk_kdriver/rtk_v4l2_pwm.h>
//vbe v4l2 main ioctrl callback

//extern void fwif_k6_sld(int Y1, int Y2, int Y3, int gain_low,unsigned char UIsel_L, unsigned int cnt_th);
extern int memc_logo_to_demura_drop_limit;

#define V4L2_EXT_ALPAH_OSD_DATA_LENGTH 16
extern void GDMA_ConfigRGBGain(unsigned int on_off, unsigned int RGB_level);
extern void GDMA_GetGlobalAlpha(unsigned int *alpha,int count);

extern int m_v4l2_pwm_ioctl_printk_get(void);

unsigned int  pstParams[23] =	{0x3, 0x64, 0x4, 0x6c2, 0xd2, 0x2, 0x3840, 0x1, 0x5, 0x1, 0x1c2, 0x2a30, 0x34bc0, 0x350, 0x2E5, 0x0, 0x3, 0x0, 0x792, 0x36b0, 0x50, 0x18, 0x34bc0};



int vbe_v4l2_main_ioctl_querycap(struct file *file, void *fh, struct v4l2_capability *cap)
{
	return 0;
}
UINT16 Mplus_Value_V4L2[1336] = {1};
int vbe_v4l2_main_ioctl_s_ctrl(struct file *file, void *fh,	struct v4l2_control *ctrls)
{
    int retval = 0;
    struct v4l2_control controls;
    unsigned int cmd = 0xff;

    if(!ctrls)
    {
        printk(KERN_ERR "func:%s [error] ctrls is null\r\n",__FUNCTION__);
        retval = -EFAULT;
        return retval;
    }
	memcpy(&controls, ctrls, sizeof(struct v4l2_control));


	cmd = controls.id;

	pr_notice("vbe_v4l2_main_ioctl_s_ctrl : cmd=%x \n", cmd);

	switch(cmd)
	{
		case V4L2_CID_EXT_VBE_DISPLAYOUTPUT:
			HAL_VBE_DISP_SetDisplayOutput(controls.value);
			break;

		case V4L2_CID_EXT_VBE_MUTE:
			HAL_VBE_DISP_SetMute(controls.value);
			break;

		case V4L2_CID_EXT_VBE_EPI_SCRAMBLE:
			break;

		case V4L2_CID_EXT_VBE_EPI_10BIT:
			break;

		case V4L2_CID_EXT_VBE_PWM_INIT:
			printk(KERN_ERR "V4L2_CID_EXT_VBE_PWM_INIT rtk_v4l2_pwm_s_ioctl\r\n");
			retval = rtk_v4l2_pwm_init();
			break;
		case V4L2_CID_EXT_VBE_MPLUS_BOEBYPASS:

                        HAL_VBE_DISP_SetBOERGBWBypass(controls.value);
                        break;

		default:
			break;
	}


	return 0;
}

#define	dvr_malloc(x)	kmalloc(x, GFP_KERNEL)
#define	dvr_free(x)	kfree(x)

int vbe_v4l2_main_ioctl_s_ext_ctrl(struct file *file, void *fh, struct v4l2_ext_controls *ctrls)
{
    int retval = 0;
    unsigned int cmd = 0xff;
    struct v4l2_ext_controls ext_controls;
    struct v4l2_ext_control ext_control;
    struct v4l2_ext_vbe_panel_info panelInfo;
    struct v4l2_ext_vbe_vcom_pat_draw vcompat;
    struct v4l2_ext_vbe_pwm_duty v4l2_pwm_duty;
    struct v4l2_ext_vbe_pwm_param v4l2_pwm_param;
    struct v4l2_ext_vbe_pwm_param_data pwm_param_data;
    enum v4l2_ext_vbe_pwm_pin_sel_mask pwmApplyParm;


    if(!ctrls)
    {
        printk(KERN_ERR "func:%s [error] ctrls is null\r\n",__FUNCTION__);
        retval = -EFAULT;
        return retval;
    }

	memcpy(&ext_controls, ctrls, sizeof(struct v4l2_ext_controls));

	if(!ext_controls.controls)
	{
		printk(KERN_ERR "func:%s line:%d [error] ext_control.ptr is null\r\n",__FUNCTION__, __LINE__);
		retval = -EFAULT;
		return retval;
	}

	memcpy(&ext_control, ext_controls.controls, sizeof(struct v4l2_ext_control));

	cmd = ext_control.id;

	pr_notice("vbe_v4l2_main_ioctl_s_ext_ctrl : cmd : %x \n", cmd);

	switch(cmd)
	{
		case V4L2_CID_EXT_VBE_INIT:

			if(!ext_control.ptr)
			{
				printk(KERN_ERR "func:%s line:%d [error] ext_control.ptr is null\r\n",__FUNCTION__, __LINE__);
				retval = -EFAULT;
				return retval;
			}
			else
			{
				if(copy_from_user((void *)&panelInfo, (const void __user *)ext_control.ptr, sizeof(struct v4l2_ext_vbe_panel_info)))
				{

					printk(KERN_ERR "func:%s line:%d [error] copy_from_user controls fail \r\n",__FUNCTION__,__LINE__);
					retval = -EFAULT;
					return retval;
				}
				else
				{
					KADP_DISP_PANEL_INFO_T kadpPanelInfo;

					kadpPanelInfo.panel_inch = panelInfo.panelInch;
					kadpPanelInfo.panel_bl_type = panelInfo.panelBacklightType;
					kadpPanelInfo.panel_maker = panelInfo.panelMaker;
					kadpPanelInfo.led_bar_type = panelInfo.panelLedBarType;
					kadpPanelInfo.panelInterface = panelInfo.panelInterface;
					kadpPanelInfo.panelResolution = panelInfo.panelResolution;
					kadpPanelInfo.panel_version = panelInfo.panelVersion;
					kadpPanelInfo.frc_type = panelInfo.frcType;
					kadpPanelInfo.cell_type = panelInfo.panelCellType;
					kadpPanelInfo.lvds_bit_mode = panelInfo.lvdsColorDepth;
					kadpPanelInfo.lvds_type = panelInfo.lvdsType;
					//kadpPanelInfo.disp_out_resolution = 0;
					kadpPanelInfo.disp_out_lane_bw = panelInfo.dispOutLaneBW;
					kadpPanelInfo.panelFrameRate = panelInfo.panelFrameRate;
					kadpPanelInfo.user_specific_option.all = panelInfo.userSpecificOption.all;
                    /*
					pr_notice("panel info (%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d\n",
                        kadpPanelInfo.panel_inch 			  ,
    					kadpPanelInfo.panel_bl_type           ,
    					kadpPanelInfo.panel_maker 	          ,
    					kadpPanelInfo.led_bar_type 	          ,
    					kadpPanelInfo.panelInterface          ,
    					kadpPanelInfo.panelResolution         ,
    					kadpPanelInfo.panel_version           ,
    					kadpPanelInfo.frc_type                ,
    					kadpPanelInfo.cell_type               ,
    					kadpPanelInfo.lvds_bit_mode           ,
    					kadpPanelInfo.lvds_type               ,
    					kadpPanelInfo.disp_out_lane_bw        ,
    					kadpPanelInfo.panelFrameRate
                        );
                    */
					HAL_VBE_DISP_Initialize((KADP_DISP_PANEL_INFO_T) kadpPanelInfo);

				}
			}
			break;

		case V4L2_CID_EXT_VBE_RESUME:

			if(!ext_control.ptr)
			{
				printk(KERN_ERR "func:%s line:%d [error] ext_control.ptr is null\r\n",__FUNCTION__, __LINE__);
				retval = -EFAULT;
				return retval;
			}
			else
			{
				if(copy_from_user((void *)&panelInfo, (const void __user *)ext_control.ptr, sizeof(struct v4l2_ext_vbe_panel_info)))
				{

					printk(KERN_ERR "func:%s line:%d [error] copy_from_user controls fail \r\n",__FUNCTION__,__LINE__);
					retval = -EFAULT;
					return retval;
				}
				else
				{
					KADP_DISP_PANEL_INFO_T kadpPanelInfo;

					kadpPanelInfo.panel_inch = panelInfo.panelInch;
					kadpPanelInfo.panel_bl_type = panelInfo.panelBacklightType;
					kadpPanelInfo.panel_maker = panelInfo.panelMaker;
					kadpPanelInfo.led_bar_type = panelInfo.panelLedBarType;
					kadpPanelInfo.panelInterface = panelInfo.panelInterface;
					kadpPanelInfo.panelResolution = panelInfo.panelResolution;
					kadpPanelInfo.panel_version = panelInfo.panelVersion;
					kadpPanelInfo.frc_type = panelInfo.frcType;
					kadpPanelInfo.cell_type = panelInfo.panelCellType;
					kadpPanelInfo.lvds_bit_mode = panelInfo.lvdsColorDepth;
					kadpPanelInfo.lvds_type = panelInfo.lvdsType;
					//kadpPanelInfo.disp_out_resolution = 0;
					kadpPanelInfo.disp_out_lane_bw = panelInfo.dispOutLaneBW;
					kadpPanelInfo.panelFrameRate = panelInfo.panelFrameRate;
					kadpPanelInfo.user_specific_option.all = panelInfo.userSpecificOption.all;
                    /*
					pr_notice("panel info %d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d\n",
                        kadpPanelInfo.panel_inch 			  ,
    					kadpPanelInfo.panel_bl_type           ,
    					kadpPanelInfo.panel_maker 	          ,
    					kadpPanelInfo.led_bar_type 	          ,
    					kadpPanelInfo.panelInterface          ,
    					kadpPanelInfo.panelResolution         ,
    					kadpPanelInfo.panel_version           ,
    					kadpPanelInfo.frc_type                ,
    					kadpPanelInfo.cell_type               ,
    					kadpPanelInfo.lvds_bit_mode           ,
    					kadpPanelInfo.lvds_type               ,
    					kadpPanelInfo.disp_out_lane_bw        ,
    					kadpPanelInfo.panelFrameRate
                        );
                    */
					HAL_VBE_DISP_Resume((KADP_DISP_PANEL_INFO_T) kadpPanelInfo);

				}
			}
			break;

		case V4L2_CID_EXT_VBE_SSC:

			if(!ext_control.ptr)
			{
				printk(KERN_ERR "func:%s line:%d [error] ext_control.ptr is null\r\n",__FUNCTION__, __LINE__);
				retval = -EFAULT;
				return retval;
			}
			else
			{
				struct v4l2_ext_vbe_ssc vbe_ssc_ctrl;

				if(copy_from_user((void *)&vbe_ssc_ctrl, (const void __user *)ext_control.ptr, sizeof(struct v4l2_ext_vbe_ssc)))
				{
					printk(KERN_ERR "func:%s line:%d [error] copy_from_user controls fail \r\n",__FUNCTION__,__LINE__);
					retval = -EFAULT;
					return retval;
				}
				else
				{
					pr_notice("V4L2_CID_EXT_VBE_SSC (%d.%d.%d) \n", vbe_ssc_ctrl.on_off, vbe_ssc_ctrl.percent, vbe_ssc_ctrl.period);
					HAL_VBE_DISP_SetSpreadSpectrum(vbe_ssc_ctrl.on_off, vbe_ssc_ctrl.percent, vbe_ssc_ctrl.period);
				}
			}
			break;

		case V4L2_CID_EXT_VBE_MIRROR:

			if(!ext_control.ptr)
			{
				printk(KERN_ERR "func:%s line:%d [error] ext_control.ptr is null\r\n",__FUNCTION__, __LINE__);
				retval = -EFAULT;
				return retval;
			}
			else
			{
				struct v4l2_ext_vbe_mirror vbe_mirror_ctrl;

				if(copy_from_user((void *)&vbe_mirror_ctrl, (const void __user *)ext_control.ptr, sizeof(struct v4l2_ext_vbe_mirror)))
				{
					printk(KERN_ERR "func:%s line:%d [error] copy_from_user controls fail \r\n",__FUNCTION__,__LINE__);
					retval = -EFAULT;
					return retval;
				}
				else
				{
                pr_notice("V4L2_CID_EXT_VBE_MIRROR (%d.%d)\n", vbe_mirror_ctrl.bIsH, vbe_mirror_ctrl.bIsV);
					HAL_VBE_DISP_SetVideoMirror(vbe_mirror_ctrl.bIsH, vbe_mirror_ctrl.bIsV);
				}
			}
			break;

		case V4L2_CID_EXT_VBE_INNER_PATTERN:


			if(!ext_control.ptr)
			{
				printk(KERN_ERR "func:%s line:%d [error] ext_control.ptr is null\r\n",__FUNCTION__, __LINE__);
				retval = -EFAULT;
				return retval;
			}
			else
			{
				struct v4l2_ext_vbe_inner_pattern vbe_innerptg_ctrl;

				if(copy_from_user((void *)&vbe_innerptg_ctrl, (const void __user *)ext_control.ptr, sizeof(struct v4l2_ext_vbe_inner_pattern)))
				{
					printk(KERN_ERR "func:%s line:%d [error] copy_from_user controls fail \r\n",__FUNCTION__,__LINE__);
					retval = -EFAULT;
					return retval;
				}
				else
				{
                    pr_notice("V4L2_CID_EXT_VBE_INNER_PATTERN (%d.%d.%d)\n", vbe_innerptg_ctrl.bOnOff, vbe_innerptg_ctrl.ip, vbe_innerptg_ctrl.type);
					HAL_VBE_DISP_SetInnerPattern(vbe_innerptg_ctrl.bOnOff, vbe_innerptg_ctrl.ip, vbe_innerptg_ctrl.type);
				}
			}
			break;

		case V4L2_CID_EXT_VBE_VCOM_PAT_DRAW:

			if(!ext_control.ptr)
			{
				printk(KERN_ERR "func:%s line:%d [error] ext_control.ptr is null\r\n",__FUNCTION__, __LINE__);
				retval = -EFAULT;
				return retval;
			}
			else
			{
				if(copy_from_user((void *)&vcompat, (const void __user *)ext_control.ptr, sizeof(struct v4l2_ext_vbe_vcom_pat_draw)))
				{
					printk(KERN_ERR "func:%s line:%d [error] copy_from_user controls fail \r\n",__FUNCTION__,__LINE__);
					retval = -EFAULT;
					return retval;
				}
				else
				{
					UINT16 vcomPattern[96];

					if(vcompat.nSize != 96)
					{
						printk(KERN_ERR "func:%s line:%d [error] vcompat size is not 96.\r\n",__FUNCTION__,__LINE__);
					}
					else
					{
					    //int i = 0;

                        pr_notice("V4L2_CID_EXT_VBE_VCOM_PAT_DRAW: %d\n", vcompat.nSize);

                        if(copy_from_user((void *)&vcomPattern[0], (const void __user *)vcompat.vcomPattern, sizeof(UINT16)*vcompat.nSize))
                        {
                            printk(KERN_ERR "func:%s line:%d [error] copy_from_user controls fail \r\n",__FUNCTION__,__LINE__);
                            retval = -EFAULT;
                            return retval;
                        }

                        //for(i=0; i<64; i++)
                          //  pr_notice("vcompat[%d] : %x\n", i, vcomPattern[i]);

						HAL_VBE_DISP_VCOMPatternDraw(vcomPattern, vcompat.nSize);
					}
				}
			}
			break;

		case V4L2_CID_EXT_VBE_VCOM_PAT_CTRL:

			if(!ext_control.ptr)
			{
				printk(KERN_ERR "func:%s line:%d [error] ext_control.ptr is null\r\n",__FUNCTION__, __LINE__);
				retval = -EFAULT;
				return retval;
			}
			else
			{
				enum v4l2_ext_vbe_vcom_pat_ctrl vcom_ctrl;

				if(copy_from_user((void *)&vcom_ctrl, (const void __user *)ext_control.ptr, sizeof(enum v4l2_ext_vbe_vcom_pat_ctrl)))
				{
					printk(KERN_ERR "func:%s line:%d [error] copy_from_user controls fail \r\n",__FUNCTION__,__LINE__);
					retval = -EFAULT;
					return retval;
				}
				else
				{
				    pr_notice("V4L2_CID_EXT_VBE_VCOM_PAT_CTRL: %d\n", vcom_ctrl);
					HAL_VBE_DISP_VCOMPatternCtrl(vcom_ctrl);
				}
			}
			break;

                case V4L2_CID_EXT_VBE_MPLUS_MODE:

                        if(!ext_control.ptr)
                        {
                                printk(KERN_ERR "func:%s line:%d [error] ext_control.ptr is null\r\n",__FUNCTION__, __LINE__);
                                retval = -EFAULT;
                                return retval;
                        }
                        else
                        {
                                enum v4l2_ext_vbe_mplus_mode mplusMode;
                                if(copy_from_user((void *)&mplusMode, (const void __user *)ext_control.ptr, sizeof(mplusMode)))
                                {
                                        printk(KERN_ERR "func:%s line:%d [error] copy_from_user controls fail \r\n",__FUNCTION__,__LINE__);
                                        retval = -EFAULT;
                                        return retval;
                                }
                                else
                                {
                                        pr_notice("V4L2_CID_EXT_VBE_MPLUS_MODE: %d\n", mplusMode);
                                        HAL_VBE_DISP_SetMLEMode(mplusMode);	/* for LGD M+ */
										HAL_VBE_DISP_SetBOEMode((unsigned char)mplusMode); /* for BOE M+ */
                                }
                        }
                        break;
                case V4L2_CID_EXT_VBE_MPLUS_PARAM:

                        if(!ext_control.ptr)
                        {
                                printk(KERN_ERR "func:%s line:%d [error] ext_control.ptr is null\r\n",__FUNCTION__, __LINE__);
                                retval = -EFAULT;
                                return retval;
                        }
                        else
                        {
                                struct v4l2_ext_vbe_mplus_param mplus_param;

                                if(copy_from_user((void *)&mplus_param, (const void __user *)ext_control.ptr, sizeof(struct v4l2_ext_vbe_mplus_param)))
                                {
                                        printk(KERN_ERR "func:%s line:%d [error] copy_from_user controls fail \r\n",__FUNCTION__,__LINE__);
                                        retval = -EFAULT;
                                        return retval;
                                }
                                else
                                {
                                        pr_notice("V4L2_CID_EXT_VBE_MPLUS_PARAM: nFrameGainLimit %x,nPixelGainLimit=%x \n",mplus_param.nFrameGainLimit, mplus_param.nPixelGainLimit);

                                        HAL_VBE_DISP_SetFrameGainLimit(mplus_param.nFrameGainLimit);
                                        HAL_VBE_DISP_SetPixelGainLimit(mplus_param.nPixelGainLimit);


                                }
                        }
                        break;
                case V4L2_CID_EXT_VBE_MPLUS_DATA:
                        if(!ext_control.ptr)
                        {
                                printk(KERN_ERR "func:%s line:%d [error] ext_control.ptr is null\r\n",__FUNCTION__, __LINE__);
                                retval = -EFAULT;
                                return retval;
                        }
                        else
                        {
                                struct v4l2_ext_vbe_mplus_data mplusData;
                                UINT16 regSize = 0;


                                if(copy_from_user((void *)&mplusData, (const void __user *)ext_control.ptr, sizeof(struct v4l2_ext_vbe_mplus_data)))
                                {
                                        printk(KERN_ERR "func:%s line:%d [error] copy_from_user controls fail \r\n",__FUNCTION__,__LINE__);
                                        retval = -EFAULT;
                                        return retval;
                                }
                                else
                                {
                                        pr_notice("V4L2_CID_EXT_VBE_MPLUS_DATA : nPanelMaker = %d \n",mplusData.nPanelMaker);

										if(mplusData.nPanelMaker == 0) // LGD M+
                                        {
                                                regSize = 1152/2;
                                                pr_notice(" === [VBE_IOC_MPLUSSET] Panel is made by LGD\n");
                                        }
										else if(mplusData.nPanelMaker == 3) // BOE SiW
                                        {
                                                regSize = 668 * 2;	/* LG sent 32 bit data */
                                                pr_notice(" === [VBE_IOC_MPLUSSET] Panel is made by BOE\n");
                                        }
                                        else
                                        {
                                                regSize = 668;
                                                pr_notice("=== [VBE_IOC_MPLUSSET] Doesn't support this panel\n");
                                        }

                                        if(copy_from_user(Mplus_Value_V4L2, (const void __user *)((unsigned long)mplusData.pRegisterSet), regSize*sizeof(UINT16)))
                                        {
                                                pr_notice("scaler vbe ioctl code=v4l2_VBE_IOC_MPLUSSET pointer copy from user failed!!!!!!!!!!!!!!!\n");
												break;
                                        }
                                        else{

                                               // int i = 0;
                                                HAL_VBE_DISP_MplusSet(Mplus_Value_V4L2, mplusData.nPanelMaker);

                                                //for(i=0;i<280;i++) //it is for debug
                                                // pr_notice(" value[%d]=%x  \n",i,Mplus_Value_V4L2[i]);

                                        }
                                }
                        }
                        break;
                case V4L2_CID_EXT_VBE_DGA4CH:
                        if(!ext_control.ptr)
                        {
                                printk(KERN_ERR "func:%s line:%d [error] ext_control.ptr is null\r\n",__FUNCTION__, __LINE__);
                                retval = -EFAULT;
                                return retval;
                        }
                        else
                        {
                                struct v4l2_ext_vbe_dga4ch dga4ch;



                                if(copy_from_user((void *)&dga4ch, (const void __user *)ext_control.ptr, sizeof(struct v4l2_ext_vbe_dga4ch)))
                                {
                                        printk(KERN_ERR "func:%s line:%d [error] copy_from_user controls fail \r\n",__FUNCTION__,__LINE__);
                                        retval = -EFAULT;
                                        return retval;
                                }
                                else
                                {
                                        UINT16 regSize = 1024;
                                        unsigned int	*m_pCacheStartAddr_red = NULL;
                                        unsigned int	*m_pCacheStartAddr_green = NULL;
                                        unsigned int	*m_pCacheStartAddr_blue = NULL;
                                        unsigned int	*m_pCacheStartAddr_white = NULL;

                                        pr_notice("V4L2_CID_EXT_VBE_DGA4CH  \n");

                                        m_pCacheStartAddr_red = (unsigned int *)dvr_malloc(regSize*sizeof(UINT32));
                                        m_pCacheStartAddr_green = (unsigned int *)dvr_malloc(regSize*sizeof(UINT32));
                                        m_pCacheStartAddr_blue = (unsigned int *)dvr_malloc(regSize*sizeof(UINT32));
                                        m_pCacheStartAddr_white = (unsigned int *)dvr_malloc(regSize*sizeof(UINT32));

                                        if((m_pCacheStartAddr_red == NULL)||(m_pCacheStartAddr_green == NULL)||
                                                (m_pCacheStartAddr_blue == NULL)||(m_pCacheStartAddr_white == NULL)){
                                                pr_notice( "[ERROR]VBE_IOC_SETDGA4CH Allocate table_size=%x fail\n",4*regSize*sizeof(UINT32));

                                                if(m_pCacheStartAddr_red){
                                                        dvr_free((void *)m_pCacheStartAddr_red);
                                                }
                                                if(m_pCacheStartAddr_green){
                                                        dvr_free((void *)m_pCacheStartAddr_green);
                                                }
                                                if(m_pCacheStartAddr_blue){
                                                        dvr_free((void *)m_pCacheStartAddr_blue);
                                                }
                                                if(m_pCacheStartAddr_white){
                                                        dvr_free((void *)m_pCacheStartAddr_white);
                                                }

                                                return FALSE;
                                        }

                                        if(copy_from_user(m_pCacheStartAddr_red, (const void __user *)((unsigned int*)dga4ch.pRedGammaTable), regSize*sizeof(UINT32)))
                                        {

                                                if(m_pCacheStartAddr_red){
                                                        dvr_free((void *)m_pCacheStartAddr_red);
                                                }
                                                if(m_pCacheStartAddr_green){
                                                        dvr_free((void *)m_pCacheStartAddr_green);
                                                }
                                                if(m_pCacheStartAddr_blue){
                                                        dvr_free((void *)m_pCacheStartAddr_blue);
                                                }
                                                if(m_pCacheStartAddr_white){
                                                        dvr_free((void *)m_pCacheStartAddr_white);
                                                }



                                                pr_notice("scaler vbe ioctl code=v4l2_VBE_IOC_MPLUSSET pointer copy from user failed!!!!!!!!!!!!!!!\n");
                                                break;
                                        }
                                        else{

                                                pr_emerg("table_r[0]=%d,table_r[1]=%d,table_r[2]=%d,table_r[3]=%d \n",*m_pCacheStartAddr_red,*(m_pCacheStartAddr_red+1)
                                                        ,*(m_pCacheStartAddr_red+2),*(m_pCacheStartAddr_red+3));

                                        }

                                        if(copy_from_user(m_pCacheStartAddr_green, (const void __user *)((unsigned int*)dga4ch.pGreenGammaTable), regSize*sizeof(UINT32)))
                                        {

                                                if(m_pCacheStartAddr_red){
                                                        dvr_free((void *)m_pCacheStartAddr_red);
                                                }
                                                if(m_pCacheStartAddr_green){
                                                        dvr_free((void *)m_pCacheStartAddr_green);
                                                }
                                                if(m_pCacheStartAddr_blue){
                                                        dvr_free((void *)m_pCacheStartAddr_blue);
                                                }
                                                if(m_pCacheStartAddr_white){
                                                        dvr_free((void *)m_pCacheStartAddr_white);
                                                }

                                                pr_notice("scaler vbe ioctl code=v4l2_VBE_IOC_MPLUSSET pointer copy from user failed!!!!!!!!!!!!!!!\n");
                                                break;
                                        }
                                        else{

                                                pr_notice("table_g[0]=%d,table_g[1]=%d,table_g[2]=%d,table_g[3]=%d \n",*m_pCacheStartAddr_green,*(m_pCacheStartAddr_green+1)
                                                 ,*(m_pCacheStartAddr_green+2),*(m_pCacheStartAddr_green+3));
                                        }

                                        if(copy_from_user(m_pCacheStartAddr_blue, (const void __user *)((unsigned int*)dga4ch.pBlueGammaTable), regSize*sizeof(UINT32)))
                                        {

                                                if(m_pCacheStartAddr_red){
                                                        dvr_free((void *)m_pCacheStartAddr_red);
                                                }
                                                if(m_pCacheStartAddr_green){
                                                        dvr_free((void *)m_pCacheStartAddr_green);
                                                }
                                                if(m_pCacheStartAddr_blue){
                                                        dvr_free((void *)m_pCacheStartAddr_blue);
                                                }
                                                if(m_pCacheStartAddr_white){
                                                        dvr_free((void *)m_pCacheStartAddr_white);
                                                }

                                                pr_notice("scaler vbe ioctl code=V4L2_CID_EXT_VBE_DGA4CH blue pointer copy from user failed!!!!!!!!!!!!!!!\n");
                                                break;
                                        }
                                        else{

                                                pr_notice("table_b[0]=%d,table_b[1]=%d,table_b[2]=%d,table_b[3]=%d \n",*m_pCacheStartAddr_blue,*(m_pCacheStartAddr_blue+1)
                                                ,*(m_pCacheStartAddr_blue+2),*(m_pCacheStartAddr_blue+3));
                                        }

                                        if(copy_from_user(m_pCacheStartAddr_white, (const void __user *)((unsigned int*)dga4ch.pWhiteGammaTable), regSize*sizeof(UINT32)))
                                        {

                                                if(m_pCacheStartAddr_red){
                                                        dvr_free((void *)m_pCacheStartAddr_red);
                                                }
                                                if(m_pCacheStartAddr_green){
                                                        dvr_free((void *)m_pCacheStartAddr_green);
                                                }
                                                if(m_pCacheStartAddr_blue){
                                                        dvr_free((void *)m_pCacheStartAddr_blue);
                                                }
                                                if(m_pCacheStartAddr_white){
                                                        dvr_free((void *)m_pCacheStartAddr_white);
                                                }
                                                pr_notice("scaler vbe ioctl code=V4L2_CID_EXT_VBE_DGA4CH white pointer copy from user failed!!!!!!!!!!!!!!!\n");
                                                break;
                                        }
                                        else{

                                               pr_notice("table_w[0]=%d,table_w[1]=%d,table_w[2]=%d,table_w[3]=%d \n",*m_pCacheStartAddr_white,*(m_pCacheStartAddr_white+1)
                                               ,*(m_pCacheStartAddr_white+2),*(m_pCacheStartAddr_white+3));
                                        }

                                        HAL_VBE_SetDGA4CH((UINT32 *)m_pCacheStartAddr_red, (UINT32 *)(m_pCacheStartAddr_green), (UINT32 *)(m_pCacheStartAddr_blue),
                                                (UINT32 *)(m_pCacheStartAddr_white), 1024);

                                        if(m_pCacheStartAddr_red){
                                                dvr_free((void *)m_pCacheStartAddr_red);
                                        }
                                        if(m_pCacheStartAddr_green){
                                                dvr_free((void *)m_pCacheStartAddr_green);
                                        }
                                        if(m_pCacheStartAddr_blue){
                                                dvr_free((void *)m_pCacheStartAddr_blue);
                                        }
                                        if(m_pCacheStartAddr_white){
                                                dvr_free((void *)m_pCacheStartAddr_white);
                                        }
                                }


                        }
                        break;
                case V4L2_CID_EXT_VBE_TSCIC:
                        if(!ext_control.ptr)
                        {
                                printk(KERN_ERR "func:%s line:%d [error] ext_control.ptr is null\r\n",__FUNCTION__, __LINE__);
                                retval = -EFAULT;
                                return retval;
                        }
                        else
                        {
                                struct v4l2_ext_vbe_panel_tscic panel_tscic;



                                if(copy_from_user((void *)&panel_tscic, (const void __user *)ext_control.ptr, sizeof(struct v4l2_ext_vbe_panel_tscic)))
                                {
                                        printk(KERN_ERR "func:%s line:%d [error] copy_from_user controls fail \r\n",__FUNCTION__,__LINE__);
                                        retval = -EFAULT;
                                        return retval;
                                }
                                else
                                {
                                        unsigned char *m_pCacheStartAddr_tscic_Crtl = NULL;
                                        unsigned int  *m_pCacheStartAddr_tscic_data = NULL;

                                        pr_notice("V4L2_CID_EXT_VBE_TSCIC  \n");
                                        if (( panel_tscic.u32Ctrlsize==0) || (panel_tscic.u32Tscicsize!=0x5C30C)) {
                                                fwif_color_set_fcic_TV006(NULL, panel_tscic.u32Tscicsize, NULL, panel_tscic.u32Ctrlsize, 0);
                                                pr_notice( "[ERROR]V4L2_CID_EXT_VBE_TSCIC table_size error ");

                                        }
                                        else
                                        {
                                                panel_tscic.u32Tscicsize = panel_tscic.u32Tscicsize/4;
                                                                                        //0x5C30C/4 =0x170c3
                                                m_pCacheStartAddr_tscic_Crtl = (unsigned char *)dvr_malloc(panel_tscic.u32Ctrlsize*sizeof(UINT8));
                                                m_pCacheStartAddr_tscic_data = (unsigned int *)dvr_malloc(panel_tscic.u32Tscicsize*sizeof(UINT32));

                                                if((m_pCacheStartAddr_tscic_Crtl == NULL)||(m_pCacheStartAddr_tscic_data == NULL)){

                                                        pr_notice( "[ERROR]Crtl_tscic Allocate table_size fail\n");

                                                        if(m_pCacheStartAddr_tscic_Crtl){
                                                                dvr_free((void *)m_pCacheStartAddr_tscic_Crtl);
                                                        }
                                                        if(m_pCacheStartAddr_tscic_data){
                                                                dvr_free((void *)m_pCacheStartAddr_tscic_data);
                                                        }

                                                        return FALSE;

                                                }


                                                if(copy_from_user(m_pCacheStartAddr_tscic_Crtl, (const void __user *)((unsigned char*)panel_tscic.u8pControlTbl), panel_tscic.u32Ctrlsize*sizeof(UINT8)))
                                                {
                                                        if(m_pCacheStartAddr_tscic_Crtl){
                                                                dvr_free((void *)m_pCacheStartAddr_tscic_Crtl);
                                                        }
                                                        if(m_pCacheStartAddr_tscic_data){
                                                                dvr_free((void *)m_pCacheStartAddr_tscic_data);
                                                        }
                                                        pr_notice("scaler vbe ioctl code=V4L2_CID_EXT_VBE_TSCIC pointer copy from user failed!!!!!!!!!!!!!!!\n");
                                                        break;
                                                }
                                                else{

                                                        pr_emerg("Ctrl[0]=%d,Ctrl[1]=%d,Ctrl[2]=%d,Ctrl[3]=%d \n",*m_pCacheStartAddr_tscic_Crtl,*(m_pCacheStartAddr_tscic_Crtl+1)
                                                                ,*(m_pCacheStartAddr_tscic_Crtl+2),*(m_pCacheStartAddr_tscic_Crtl+3));

                                                }

                                                if(copy_from_user(m_pCacheStartAddr_tscic_data, (const void __user *)((unsigned int*)panel_tscic.u32pTSCICTbl),panel_tscic.u32Tscicsize*sizeof(UINT32)))
                                                {
                                                        if(m_pCacheStartAddr_tscic_Crtl){
                                                                dvr_free((void *)m_pCacheStartAddr_tscic_Crtl);
                                                        }
                                                        if(m_pCacheStartAddr_tscic_data){
                                                                dvr_free((void *)m_pCacheStartAddr_tscic_data);
                                                        }

                                                        pr_notice("scaler vbe ioctl code=V4L2_CID_EXT_VBE_TSCIC pointer copy from user failed!!!!!!!!!!!!!!!\n");
                                                        break;
                                                }
                                                else{

                                                        pr_emerg("data[0]=%d,data[1]=%d,data[2]=%d,data[3]=%d \n",*m_pCacheStartAddr_tscic_data,*(m_pCacheStartAddr_tscic_data+1)
                                                         ,*(m_pCacheStartAddr_tscic_data+2),*(m_pCacheStartAddr_tscic_data+3));
                                                }


                                                HAL_VBE_TSCIC_Load((UINT32 *)m_pCacheStartAddr_tscic_data, panel_tscic.u32Tscicsize, m_pCacheStartAddr_tscic_Crtl, panel_tscic.u32Ctrlsize);

                                                if(m_pCacheStartAddr_tscic_Crtl){
                                                        dvr_free((void *)m_pCacheStartAddr_tscic_Crtl);
                                                }
                                                if(m_pCacheStartAddr_tscic_data){
                                                        dvr_free((void *)m_pCacheStartAddr_tscic_data);
                                                }
                                        }

                                }


                        }
                        break;
                case V4L2_CID_EXT_VBE_ORBIT:

                    if(!ext_control.ptr)
                    {
                            printk(KERN_ERR "func:%s line:%d [error] ext_control.ptr is null\r\n",__FUNCTION__, __LINE__);
                            retval = -EFAULT;
                            return retval;
                    }
                    else
                    {

                        struct v4l2_ext_vbe_panel_orbit_info orbitInfo;

                        if(copy_from_user((void *)&orbitInfo, (const void __user *)ext_control.ptr, sizeof(struct v4l2_ext_vbe_panel_orbit_info)))
                        {
                                printk(KERN_ERR "func:%s line:%d [error] copy_from_user controls fail \r\n",__FUNCTION__,__LINE__);
                                retval = -EFAULT;
                                return retval;
                        }
                        else
                        {
                            pr_notice("V4L2_CID_EXT_VBE_ORBIT: orbit mode = %d \n",orbitInfo.orbitmode);
                            HAL_VBE_DISP_OLED_SetOrbit(orbitInfo.on_off,orbitInfo.orbitmode);

                        }
                    }
                    break;
                        case V4L2_CID_EXT_VBE_LSR:

                        if(!ext_control.ptr)
                        {
                                printk(KERN_ERR "func:%s line:%d [error] ext_control.ptr is null\r\n",__FUNCTION__, __LINE__);
                                retval = -EFAULT;
                                return retval;
                        }
                        else
                        {
                                struct v4l2_ext_vbe_panel_lsr_info lsrinfo;
                                UINT32 lsr_table[4];

                                if(copy_from_user((void *)&lsrinfo, (const void __user *)ext_control.ptr, sizeof(struct v4l2_ext_vbe_panel_lsr_info)))
                                {
                                        printk(KERN_ERR "func:%s line:%d [error] copy_from_user controls fail \r\n",__FUNCTION__,__LINE__);
                                        retval = -EFAULT;
                                        return retval;
                                }
                                else
                                {

                                        if(copy_from_user(&lsr_table, (const void __user *)((unsigned int*)lsrinfo.pLsrTable), 4*sizeof(UINT32)))
                                        {
                                                pr_notice("scaler vbe ioctl code=V4L2_CID_EXT_VBE_LSR pointer copy from user failed!!!!!!!!!!!!!!!\n");

                                        }

                                        pr_notice("LSR [0]=%d,[1]=%d,[2]=%d,[3]=%d , lsrstep =%d",lsr_table[0],lsr_table[1],lsr_table[2],lsr_table[3],lsrinfo.lsrstep);

                                        //fwif_k6_sld(lsr_table[0], lsr_table[1], lsr_table[2], lsrinfo.lsrstep,1, lsr_table[3]);
                                        memc_logo_to_demura_drop_limit = lsr_table[0];
                                }


                        }
                        break;

                        case V4L2_CID_EXT_VBE_GSR:

                        if(!ext_control.ptr)
                        {
                                printk(KERN_ERR "func:%s line:%d [error] ext_control.ptr is null\r\n",__FUNCTION__, __LINE__);
                                retval = -EFAULT;
                                return retval;
                        }
                        else
                        {
                        
                                struct v4l2_ext_vbe_panel_gsr_info gsrinfo;
                                //UINT32 gsr_table[23];



                                if(copy_from_user((void *)&gsrinfo, (const void __user *)ext_control.ptr, sizeof(struct v4l2_ext_vbe_panel_gsr_info)))
                                {
                                        printk(KERN_ERR "func:%s line:%d [error] copy_from_user controls fail \r\n",__FUNCTION__,__LINE__);
                                        retval = -EFAULT;
                                        return retval;
                                }
                                else
                                {
                                        if(copy_from_user(&pstParams, (const void __user *)((unsigned int*)gsrinfo.pGsrTable), 23*sizeof(UINT32)))
                                        {
                                                pr_notice("scaler vbe ioctl code=V4L2_CID_EXT_VBE_GSR pointer copy from user failed!!!!!!!!!!!!!!!\n");

                                        }
                                        pr_notice("GSR [0]=%d,[1]=%d,[2]=%d,[3]=%d ",pstParams[0],pstParams[1],pstParams[2],pstParams[3]);
                                        // drvif_color_get_LC_APL();

                                }


                        }
                        break;

		case V4L2_CID_EXT_VBE_PWM_SET_DUTY:
			if(!ext_control.ptr)
			{
				printk(KERN_ERR "V4L2_CID_EXT_VBE_PWM_SET_DUTY func:%s line:%d [error] ext_control.ptr is null\r\n",__FUNCTION__, __LINE__);
				retval = -EFAULT;
				return retval;
			}else
			{
				if(copy_from_user((void *)&v4l2_pwm_duty, (const void __user *)ext_control.ptr, sizeof(struct v4l2_ext_vbe_pwm_duty)))
				{
					printk(KERN_ERR " pwm func:%s line:%d [error] copy_from_user controls fail \r\n",__FUNCTION__,__LINE__);
					retval = -EFAULT;
					return retval;
				}
				else
				{
					if(m_v4l2_pwm_ioctl_printk_get() > 0) printk(KERN_ERR "V4L2_CID_EXT_VBE_PWM_SET_DUTY: %d\n", v4l2_pwm_duty.pwm_duty);
					retval = rtk_v4l2_pwm_set_duty(&v4l2_pwm_duty);
				}

			}
			break;
		case V4L2_CID_EXT_VBE_PWM_APPLY_PARAM:
			if(!ext_control.ptr)
			{
				printk(KERN_ERR "func:%s line:%d [error] ext_control.ptr is null\r\n",__FUNCTION__, __LINE__);
				retval = -EFAULT;
				return retval;
			}else
			{
				if(copy_from_user((void *)&pwmApplyParm, (const void __user *)ext_control.ptr, sizeof(enum v4l2_ext_vbe_pwm_pin_sel_mask)))
				{
					printk(KERN_ERR " pwm func:%s line:%d [error] copy_from_user controls fail \r\n",__FUNCTION__,__LINE__);
					retval = -EFAULT;
					return retval;
				}
				if(m_v4l2_pwm_ioctl_printk_get() > 0) printk(KERN_ERR " pwm func:%s line:%d  index:%d\r\n",__FUNCTION__,__LINE__,pwmApplyParm);
				rtk_v4l2_pwm_ApplyParam(pwmApplyParm);
			}
			break;
		case V4L2_CID_EXT_VBE_PWM_PARAM:
			if(!ext_control.ptr)
			{
				printk(KERN_ERR "pwm func:%s line:%d [error] ext_control.ptr is null\r\n",__FUNCTION__, __LINE__);
				retval = -EFAULT;
				return retval;
			}else
			{
				if(copy_from_user((void *)&v4l2_pwm_param, (const void __user *)ext_control.ptr, sizeof(struct v4l2_ext_vbe_pwm_param)))
				{
					printk(KERN_ERR " pwm func:%s line:%d [error] copy_from_user controls fail \r\n",__FUNCTION__,__LINE__);
					retval = -EFAULT;
					return retval;
				}
				else
				{
					if(copy_from_user((void *)&pwm_param_data, (const void __user *)v4l2_pwm_param.pstPWMParam  , sizeof(struct v4l2_ext_vbe_pwm_param_data)))
					{
						printk(KERN_ERR " pwm func:%s line:%d [error] copy_from_user controls fail \r\n",__FUNCTION__,__LINE__);
						retval = -EFAULT;
						return retval;
					}

					if (v4l2_pwm_param.pstPWMParam ==NULL)
					{
						printk(KERN_ERR "v4l2_pwm_param is null\n");
					}
					v4l2_pwm_param.pstPWMParam = &pwm_param_data;

					if(v4l2_pwm_param.pwmIndex < 0 || v4l2_pwm_param.pwmIndex > 9)
					{
						printk(KERN_ERR " pwm func:%s line:%d [error]  pwmIndex \r\n",__FUNCTION__,__LINE__);
						return -1;
					}
					retval = rtk_v4l2_pwm_ApplyParamSet(&v4l2_pwm_param);
					return retval;
				}
			}
			break;
		case V4L2_CID_EXT_VBE_OSD_GAIN:
			if(!ext_control.ptr)
			{
				printk(KERN_ERR "func:%s line:%d [error] ext_control.ptr is null\r\n", __FUNCTION__, __LINE__);
				retval = -EFAULT;
				return retval;
			}
			else
			{
				struct v4l2_ext_vbe_panel_osd_gain_info panel_osd_gain_info;
				UINT32 RGB_level;

				if(copy_from_user((void *)&panel_osd_gain_info, (const void __user *)ext_control.ptr, sizeof(struct v4l2_ext_vbe_panel_osd_gain_info)))
				{
					printk(KERN_ERR "func:%s line:%d [error] copy_from_user controls fail \r\n", __FUNCTION__, __LINE__);
					retval = -EFAULT;
					return retval;
				}
				else
				{
					if(copy_from_user((void *)&RGB_level, (const void __user *)((unsigned int *)panel_osd_gain_info.levelval), sizeof(UINT32)))
					{
						printk(KERN_ERR "func:%s line:%d [error] copy_from_user controls fail \r\n", __FUNCTION__, __LINE__);
						retval = -EFAULT;
						return retval;
					}

					pr_notice("V4L2_CID_EXT_VBE_OSD_GAIN (%d.%d)\n", panel_osd_gain_info.on_off, RGB_level);
					GDMA_ConfigRGBGain(panel_osd_gain_info.on_off, RGB_level);
				}
			}
			break;

		default:
			break;
	}
	return 0;
}

int vbe_v4l2_main_ioctl_g_ext_ctrl(struct file *file, void *fh, struct v4l2_ext_controls *ctrls)
{

        int retval = 0;
        unsigned int cmd = 0xff;
       // struct v4l2_ext_controls ext_controls;
        struct v4l2_ext_control ext_control;
        if(!ctrls)
        {
                printk(KERN_ERR "func:%s [error] ctrls is null\r\n",__FUNCTION__);
                return -EFAULT;
        }


        memcpy(&ext_control,ctrls->controls,sizeof(struct v4l2_ext_control));

        cmd = ext_control.id;
        switch(cmd)
        {
                case V4L2_CID_EXT_VBE_MPLUS_PARAM:
                {
                        struct v4l2_ext_vbe_mplus_param mplus_param;
			unsigned short nPixelGainLimit=0;
			unsigned short nFrameGainLimit=0;

			HAL_VBE_DISP_GetPixelGainLimit(&nPixelGainLimit);
			HAL_VBE_DISP_GetFrameGainLimit(&nFrameGainLimit);


                       /* if(copy_from_user((void *)&mplus_param, (const void __user *)ext_control.ptr, sizeof(struct v4l2_ext_vbe_mplus_param)))
                        {
                                printk(KERN_ERR "func:%s line:%d [error] copy_from_user controls fail \r\n",__FUNCTION__,__LINE__);
                                retval = -EFAULT;
                                return retval;
                        }*/

                        mplus_param.nFrameGainLimit = nFrameGainLimit;
                        mplus_param.nPixelGainLimit = nPixelGainLimit;
                        if(copy_to_user(to_user_ptr((ext_control.ptr)), &mplus_param, sizeof(struct v4l2_ext_vbe_mplus_param)))
                        {
                                printk(KERN_ERR "func:%s [error] v4l2_ext_vpq_dc2p_histodata_info copy_to_user fail \r\n",__FUNCTION__);
                                return -EFAULT;
                        }

                }
                break;
                case V4L2_CID_EXT_VBE_MPLUS_DATA:
                {
                        struct v4l2_ext_vbe_mplus_data mplusData;
                        UINT16 regSize = 0;
                        int i;


                        if(copy_from_user((void *)&mplusData, (const void __user *)ext_control.ptr, sizeof(struct v4l2_ext_vbe_mplus_data)))
                        {
                                printk(KERN_ERR "func:%s line:%d [error] copy_from_user controls fail \r\n",__FUNCTION__,__LINE__);
                                retval = -EFAULT;
                                return retval;
                        }
                        else
                        {
                                pr_notice("V4L2_CID_EXT_VBE_MPLUS_DATA : nPanelMaker = %d \n",mplusData.nPanelMaker);


                                HAL_VBE_DISP_MplusGet(Mplus_Value_V4L2, mplusData.nPanelMaker);

                                if(Get_DISPLAY_PANEL_MPLUS_RGBW() == 1) // M+
                                {
                                        regSize = 928>>1;

                                        // match LG format
                                        // Mplus_Value[0] = 0x1234;// --> print 0x34, 0x12
                                        for(i=0;i<regSize;i++)
                                        {
                                                Mplus_Value_V4L2[i] = (Mplus_Value_V4L2[i*2+1]<<8) + Mplus_Value_V4L2[i*2];
                                        }

                                }
                                else if(Get_DISPLAY_PANEL_BOW_RGBW() == 1) // BOE SiW
                                {
                                        regSize = 256;

                                        // merge 8bits to 16 bits
                                        for(i=0;i<regSize/2;i++)
                                        {
                                                Mplus_Value_V4L2[i] = (Mplus_Value_V4L2[i*2]<<8) + Mplus_Value_V4L2[i*2+1];
                                        }
                                }
                                else // default
                                {
                                        regSize = 256;
                                }

                                if(copy_to_user(to_user_ptr(mplusData.pRegisterSet), (void *)Mplus_Value_V4L2, regSize*sizeof(UINT16)))
                                {
                                          retval = -EFAULT;
                                }

                        }

                }
                break;

                case V4L2_CID_EXT_VBE_PWM_PARAM:
                {
                        struct v4l2_ext_vbe_pwm_param v4l2_pwm_param;
                        struct v4l2_ext_vbe_pwm_param v4l2_pwm_param_kernel;
                        struct v4l2_ext_vbe_pwm_param_data pwm_param_data;
                        if(copy_from_user((void *)&v4l2_pwm_param, (const void __user *)ext_control.ptr, sizeof(struct v4l2_ext_vbe_pwm_param)))
                        {
                               printk(KERN_ERR " pwm func:%s line:%d [error] copy_from_user controls fail \r\n",__FUNCTION__,__LINE__);
                               retval = -EFAULT;
                               return retval;
                        }
                        memcpy(&v4l2_pwm_param_kernel, &v4l2_pwm_param, sizeof(struct v4l2_ext_vbe_pwm_param));

                        if(copy_from_user((void *)&pwm_param_data, (const void __user *)v4l2_pwm_param.pstPWMParam  , sizeof(struct v4l2_ext_vbe_pwm_param_data)))
                        {
                               printk(KERN_ERR " pwm func:%s line:%d [error] copy_from_user controls fail \r\n",__FUNCTION__,__LINE__);
                               retval = -EFAULT;
                               return retval;
                        }
                        v4l2_pwm_param_kernel.pstPWMParam = &pwm_param_data;

                        if(v4l2_pwm_param_kernel.pwmIndex < 0 || v4l2_pwm_param_kernel.pwmIndex > 9)
                        {
                               printk(KERN_ERR " pwm func:%s line:%d [error]  pwmIndex \r\n",__FUNCTION__,__LINE__);
                               return -1;
                        }
                        retval = rtk_v4l2_pwm_ApplyParamGet(&v4l2_pwm_param_kernel);

                        if(copy_to_user((struct v4l2_ext_vbe_pwm_param_data __user *)v4l2_pwm_param.pstPWMParam, &pwm_param_data, sizeof( struct v4l2_ext_vbe_pwm_param_data)))
                        {
                                 printk(KERN_ERR "2func:%s [error] rtk_v4l2_pwm_ApplyParamGet copy_to_user fail \r\n",__FUNCTION__);
                                 //return -EFAULT;
                        }
                        v4l2_pwm_param.pwmIndex = v4l2_pwm_param_kernel.pwmIndex;

                        if(copy_to_user(to_user_ptr(ext_control.ptr), &v4l2_pwm_param, sizeof( struct v4l2_ext_vbe_pwm_param)))
                        {
                                 printk(KERN_ERR "1func:%s [error] rtk_v4l2_pwm_ApplyParamGet copy_to_user fail \r\n",__FUNCTION__);
                                 return -EFAULT;
                        }
                        return retval;
                }
                break;
		case V4L2_CID_EXT_VBE_ALPHA_OSD:
		{
			struct v4l2_ext_vbe_panel_alpha_osd_info alpha_osd_info;
			unsigned int alphaTable[V4L2_EXT_ALPAH_OSD_DATA_LENGTH];
			alpha_osd_info.size = 4;

			GDMA_GetGlobalAlpha((unsigned int *)alphaTable, V4L2_EXT_ALPAH_OSD_DATA_LENGTH);

			if(copy_from_user((void *)&alpha_osd_info, (const void __user *)ext_control.ptr, sizeof(struct v4l2_ext_vbe_panel_alpha_osd_info)))
			{
				printk(KERN_ERR "func:%s line:%d [error] copy_from_user controls fail \r\n", __FUNCTION__, __LINE__);
				retval = -EFAULT;
				return retval;
			}

			if(copy_to_user(to_user_ptr(alpha_osd_info.alphaTable), &alphaTable, sizeof(UINT32)*16)) {
				printk(KERN_ERR "func:%s [error] V4L2_CID_EXT_VBE_ALPHA_OSD copy_to_user fail \r\n", __FUNCTION__);
				return -EFAULT;
			}
			break;
		}

                default:
                        break;


        }
        return 0;
}



