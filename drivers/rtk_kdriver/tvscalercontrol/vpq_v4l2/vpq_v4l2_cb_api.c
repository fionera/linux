/*
 *      vpq_v4l2_driver.c  related callback api--
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

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#define to_user_ptr(x)          compat_ptr((unsigned int)x)
#else
#define to_user_ptr(x)          ((void* __user)(x)) // convert 32 bit value to user pointer
#endif


//common
#include <ioctrl/vpq/vpq_cmd_id.h>
#ifdef CONFIG_KDRIVER_USE_NEW_COMMON
#include <scaler/scalerCommon.h>
#else
#include <scalercommon/scalerCommon.h>
#endif

#include <tvscalercontrol/vip/scalerColor_tv006.h>

//TVScaler Header file
#include <tvscalercontrol/vpq_v4l2/vpq_v4l2_structure.h>
#include <tvscalercontrol/vpq_v4l2/vpq_v4l2_api.h>
#include <tvscalercontrol/scaler_vpqmemcdev.h>
#include <tvscalercontrol/scaler_vpqleddev.h>
#include <tvscalercontrol/vip/scalerColor_tv006.h>
#include "tvscalercontrol/scalerdrv/scalerdisplay.h"


#include <mach/rtk_log.h>


#define TAG_NAME "VPQ"

#ifdef VPQ_V4L2

extern VPQ_SetPicCtrl_T pic_ctrl;// = {0, {100, 50, 50, 0}, {128, 128, 128, 128}};

int vpq_v4l2_main_ioctl_s_ext_ctrls(struct file *file, void *fh, struct v4l2_ext_controls *ctrls)
{
        int retval = 0;
        unsigned int cmd = 0xff;
        struct v4l2_ext_control ext_control;
        struct v4l2_vpq_cmn_data pqData;
        if(!ctrls)
        {
            printk(KERN_ERR "func:%s [error] ctrls is null\r\n",__FUNCTION__);
            retval = -EFAULT;
            return retval;
        }


	//memcpy(&hdmi_v4l2_ext_control,ctrls->controls,sizeof(struct v4l2_ext_control));

	memcpy(&ext_control,ctrls->controls,sizeof(struct v4l2_ext_control));

        rtd_printk(KERN_EMERG, TAG_NAME, "rord ori  addr1 =%p,addr2=%p\n",
               &ext_control,ctrls->controls);


        cmd = ext_control.id;

        pr_notice("vpq_v4l2_main_ioctl_s_ext_ctrl : cmd : %x \n", cmd);

        rtd_printk(KERN_EMERG, TAG_NAME, "rord ctrl id =%d \n",cmd);

        switch(cmd)
        {
                case V4L2_CID_EXT_VPQ_PICTURE_CTRL:
                {

                        struct v4l2_ext_picture_ctrl_data_RTK pictureCtrl;

                        if(!ext_control.ptr)
                        {
                                printk(KERN_ERR "func:%s line:%d [error] ext_control.ptr is null\r\n",__FUNCTION__, __LINE__);
                                retval = -EFAULT;
                                return retval;
                        }
                        else
                        {
                                //if(copy_from_user((void *)&pictureCtrl, (const void __user *)ext_control.ptr, sizeof(struct v4l2_ext_picture_ctrl_data)))


                                if(copy_from_user((void *)&pqData, to_user_ptr(ext_control.ptr), sizeof(struct v4l2_vpq_cmn_data)))
                                {

                                    printk(KERN_ERR "func:%s line:%d [error] copy_from_user controls fail \r\n",__FUNCTION__,__LINE__);
                                    retval = -EFAULT;
                                    return retval;
                                }
                                else
                                {
                                    rtd_printk(KERN_EMERG, TAG_NAME, "rord length=%d,version=%d,wid=%d, \n",
                                                pqData.length,
                                                pqData.version,
                                                pqData.wid );
                                }
                                if(copy_from_user((void *)&pictureCtrl, to_user_ptr(pqData.p_data), sizeof(struct v4l2_ext_picture_ctrl_data_RTK)))
                                {


                                }


                                //  memcpy((unsigned char*)&pictureCtrl,(unsigned char*)pqData.p_data,sizeof(struct v4l2_ext_picture_ctrl_data));

                                rtd_printk(KERN_EMERG, TAG_NAME, "rord bri=%d,cont=%d,sat=%d,hue=%d, \n",
                                        pictureCtrl.sBrightness,
                                        pictureCtrl.sContrast,
                                        pictureCtrl.sSaturation,
                                        pictureCtrl.sHue);

                                pic_ctrl.wId = pqData.wid;

                                pic_ctrl.Gain_Val[PIC_CTRL_Contrast] =pictureCtrl.sContrast;
                                pic_ctrl.Gain_Val[PIC_CTRL_Brightness] =pictureCtrl.sBrightness;
                                pic_ctrl.Gain_Val[PIC_CTRL_Color] =pictureCtrl.sSaturation;
                                pic_ctrl.Gain_Val[PIC_CTRL_Tint] =pictureCtrl.sHue;

                                pic_ctrl.siVal[0] =pictureCtrl.sPcVal[0];
                                pic_ctrl.siVal[1] =pictureCtrl.sPcVal[1];
                                pic_ctrl.siVal[2] =pictureCtrl.sPcVal[2];
                                pic_ctrl.siVal[3] =pictureCtrl.sPcVal[3];


                                // PictureMode_flg = 1;  // for MEMC wrt by JerryWang 20161125
                                retval = fwif_color_set_Picture_Control_tv006(0, &pic_ctrl);


                        }
                }
                break;


                case V4L2_CID_EXT_VPQ_DECONTOUR:
                {

                        extern unsigned char tv006_decontour_level;
                        extern RTK_DECONTOUR_T De_contour_level;

                        struct v4l2_ext_vpq_decontour_data de_contour;
                        CHIP_DECONTOUR_T De_Contour_Table;

                        if(!ext_control.ptr)
                        {
                                printk(KERN_ERR "func:%s line:%d [error] ext_control.ptr is null\r\n",__FUNCTION__, __LINE__);
                                retval = -EFAULT;
                                return retval;
                        }
                        else
                        {

                                if(copy_from_user((void *)&pqData, to_user_ptr(ext_control.ptr), sizeof(struct v4l2_vpq_cmn_data)))
                                {

                                    printk(KERN_ERR "func:%s line:%d [error] copy_from_user controls fail \r\n",__FUNCTION__,__LINE__);
                                    retval = -EFAULT;
                                    return retval;
                                }
                                else
                                {
                                    rtd_printk(KERN_EMERG, TAG_NAME, "rord V4L2_CID_EXT_VPQ_DECONTOUR \n");

                                    rtd_printk(KERN_EMERG, TAG_NAME, "rord length=%d,version=%d,wid=%d, \n",
                                                pqData.length,
                                                pqData.version,
                                                pqData.wid );
                                }
                                if(copy_from_user((void *)&de_contour, to_user_ptr(pqData.p_data), sizeof(struct v4l2_ext_vpq_decontour_data)))
                                {
                                        printk(KERN_ERR "func:%s line:%d [error] copy_from_user controls fail \r\n",__FUNCTION__,__LINE__);
                                        retval = -EFAULT;
                                        return retval;


                                }

                                if(copy_from_user((void *)&De_Contour_Table, to_user_ptr(de_contour.pst_chip_data), sizeof(CHIP_DECONTOUR_T)))
                                {
                                        printk(KERN_ERR "func:%s line:%d [error] copy_from_user controls fail \r\n",__FUNCTION__,__LINE__);
                                        retval = -EFAULT;
                                        return retval;
                                }

                                 memcpy((unsigned char*)&De_contour_level.De_Contour_Table,(unsigned char*)&De_Contour_Table,sizeof( CHIP_DECONTOUR_T));
                                 De_contour_level.UI_Lv = de_contour.ui_value;

                                rtd_printk(KERN_EMERG, TAG_NAME, "rord Idecontour_level=%d,ui=%d \n",
                                        De_Contour_Table.Idecontour_level,de_contour.ui_value);

                                tv006_decontour_level = De_contour_level.De_Contour_Table.Idecontour_level;

                                fwif_color_Set_De_Contour_tv006(&De_contour_level);


                        }
                }
                break;
                case V4L2_CID_EXT_VPQ_LOCALCONTRAST_DATA:
                {

                        struct v4l2_ext_vpq_localcontrast_data local_contrast;
                        CHIP_LOCAL_CONTRAST_T lc_param;

                        if(!ext_control.ptr)
                        {
                                printk(KERN_ERR "func:%s line:%d [error] ext_control.ptr is null\r\n",__FUNCTION__, __LINE__);
                                retval = -EFAULT;
                                return retval;
                        }
                        else
                        {

                                if(copy_from_user((void *)&pqData, to_user_ptr(ext_control.ptr), sizeof(struct v4l2_vpq_cmn_data)))
                                {

                                    printk(KERN_ERR "func:%s line:%d [error] copy_from_user controls fail \r\n",__FUNCTION__,__LINE__);
                                    retval = -EFAULT;
                                    return retval;
                                }
                                else
                                {
                                    rtd_printk(KERN_EMERG, TAG_NAME, "rord V4L2_CID_EXT_VPQ_LOCALCONTRAST_DATA \n");

                                    rtd_printk(KERN_EMERG, TAG_NAME, "rord length=%d,version=%d,wid=%d, \n",
                                                pqData.length,
                                                pqData.version,
                                                pqData.wid );
                                }
                                if(copy_from_user((void *)&local_contrast, to_user_ptr(pqData.p_data), sizeof(struct v4l2_ext_vpq_localcontrast_data)))
                                {
                                        printk(KERN_ERR "func:%s line:%d [error] copy_from_user controls fail \r\n",__FUNCTION__,__LINE__);
                                        retval = -EFAULT;
                                        return retval;


                                }

                                if(copy_from_user((void *)&lc_param, to_user_ptr(local_contrast.pst_chip_data), sizeof(CHIP_LOCAL_CONTRAST_T)))
                                {
                                        printk(KERN_ERR "func:%s line:%d [error] copy_from_user controls fail \r\n",__FUNCTION__,__LINE__);
                                        retval = -EFAULT;
                                        return retval;
                                }



                                rtd_printk(KERN_EMERG, TAG_NAME, "rord slope_unit=%d,ui=%d \n",
                                        lc_param.LC_tmap_slope_unit,local_contrast.ui_value);

                        fwif_color_set_LocalContrast_table_TV006(&lc_param);



                        }
                }
                break;
                case V4L2_CID_EXT_VPQ_NOISE_REDUCTION:
                {

                        struct v4l2_ext_vpq_noise_reduction_data noiseReduction;
                        CHIP_NOISE_REDUCTION_T tNOISE_TABLE;

                        if(!ext_control.ptr)
                        {
                                printk(KERN_ERR "func:%s line:%d [error] ext_control.ptr is null\r\n",__FUNCTION__, __LINE__);
                                retval = -EFAULT;
                                return retval;
                        }
                        else
                        {

                                if(copy_from_user((void *)&pqData, to_user_ptr(ext_control.ptr), sizeof(struct v4l2_vpq_cmn_data)))
                                {

                                    printk(KERN_ERR "func:%s line:%d [error] copy_from_user controls fail \r\n",__FUNCTION__,__LINE__);
                                    retval = -EFAULT;
                                    return retval;
                                }
                                else
                                {
                                    rtd_printk(KERN_EMERG, TAG_NAME, "rord V4L2_CID_EXT_VPQ_NOISE_REDUCTION \n");

                                    rtd_printk(KERN_EMERG, TAG_NAME, "rord length=%d,version=%d,wid=%d, \n",
                                                pqData.length,
                                                pqData.version,
                                                pqData.wid );
                                }
                                if(copy_from_user((void *)&noiseReduction, to_user_ptr(pqData.p_data), sizeof(struct v4l2_ext_vpq_noise_reduction_data)))
                                {
                                        printk(KERN_ERR "func:%s line:%d [error] copy_from_user controls fail \r\n",__FUNCTION__,__LINE__);
                                        retval = -EFAULT;
                                        return retval;


                                }

                                if(copy_from_user((void *)&tNOISE_TABLE, to_user_ptr(noiseReduction.pst_chip_data), sizeof(CHIP_NOISE_REDUCTION_T)))
                                {
                                        printk(KERN_ERR "func:%s line:%d [error] copy_from_user controls fail \r\n",__FUNCTION__,__LINE__);
                                        retval = -EFAULT;
                                        return retval;
                                }



                                rtd_printk(KERN_EMERG, TAG_NAME, "rord NR_LEVEL=%d,ui=%d \n",
                                       noiseReduction.ui_value[0],noiseReduction.ui_value[0]);


                       // fwif_color_Set_NR_Table_tv006(&NR_Level);


                        //fwif_color_SetDNR_tv006(NR_Level.NR_TABLE.NR_LEVEL);



                        }
                }
                break;
                case V4L2_CID_EXT_MEMC_MOTION_COMP:
                {

                        struct v4l2_ext_memc_motion_comp_info motion_comp_info;
                        VPQ_MEMC_TYPE_T motion;

                        if(!ext_control.ptr)
                        {
                                printk(KERN_ERR "func:%s line:%d [error] ext_control.ptr is null\r\n",__FUNCTION__, __LINE__);
                                retval = -EFAULT;
                                return retval;
                        }
                        else
                        {

                                if(copy_from_user((void *)&pqData, to_user_ptr(ext_control.ptr), sizeof(struct v4l2_vpq_cmn_data)))
                                {

                                    printk(KERN_ERR "func:%s line:%d [error] copy_from_user pqData fail \r\n",__FUNCTION__,__LINE__);
                                    retval = -EFAULT;
                                    return retval;
                                }

                                if(copy_from_user((void *)&motion_comp_info, to_user_ptr(pqData.p_data), sizeof(struct v4l2_ext_memc_motion_comp_info)))
                                {

                                        printk(KERN_ERR "func:%s line:%d [error] copy_from_user motion_comp_info fail \r\n",__FUNCTION__,__LINE__);
                                        retval = -EFAULT;
                                        return retval;

                                }
                                else
                                {
                                        rtd_printk(KERN_EMERG, TAG_NAME, "rord blur_level=%d,judder_level=%d,memc_type =%d \n",
                                                motion_comp_info.blur_level,
                                                motion_comp_info.judder_level,
                                                motion_comp_info.memc_type);

                                        motion = motion_comp_info.memc_type;
                                        retval = HAL_VPQ_MEMC_SetMotionComp(motion_comp_info.blur_level, motion_comp_info.judder_level, motion);

                                }

                        }
                }
                break;
                case V4L2_CID_EXT_LED_INIT:
                {
                        struct v4l2_ext_led_panel_info stRealInfo;
                        unsigned char src_idx = 0;
                        unsigned char TableIdx = 0;
                        HAL_LED_PANEL_INFO_T PANEL_INFO_T;

                        if(!ext_control.ptr)
                        {
                                printk(KERN_ERR "func:%s line:%d [error] ext_control.ptr is null\r\n",__FUNCTION__, __LINE__);
                                retval = -EFAULT;
                                return retval;
                        }
                        else
                        {

                                if(copy_from_user((void *)&pqData, to_user_ptr(ext_control.ptr), sizeof(struct v4l2_vpq_cmn_data)))
                                {

                                    printk(KERN_ERR "func:%s line:%d [error] copy_from_user pqData fail \r\n",__FUNCTION__,__LINE__);
                                    retval = -EFAULT;
                                    return retval;
                                }

                                if(copy_from_user((void *)&stRealInfo, to_user_ptr(pqData.p_data), sizeof(struct v4l2_ext_led_panel_info)))
                                {

                                        printk(KERN_ERR "func:%s line:%d [error] copy_from_user stRealInfo fail \r\n",__FUNCTION__,__LINE__);
                                        retval = -EFAULT;
                                        return retval;

                                }
                                else
                                {
                                        rtd_printk(KERN_EMERG, TAG_NAME, "rord panel_inch=%d,backlight_type=%d,bar_type =%d,module_maker=%d,local_dim_ic_type=%d,panel_type =%d \n",
                                                stRealInfo.panel_inch,
                                                stRealInfo.backlight_type,
                                                stRealInfo.bar_type,
                                                stRealInfo.module_maker,
                                                stRealInfo.local_dim_ic_type,
                                                stRealInfo.panel_type
                                        );

                                        PANEL_INFO_T.hal_inch = stRealInfo.panel_inch;
                                        PANEL_INFO_T.hal_bl_type = stRealInfo.backlight_type;
                                        PANEL_INFO_T.hal_bar_type = stRealInfo.bar_type;
                                        PANEL_INFO_T.hal_maker = stRealInfo.module_maker;
                                        PANEL_INFO_T.hal_icType = stRealInfo.local_dim_ic_type;
                                        PANEL_INFO_T.hal_panel_type = stRealInfo.panel_type;


                                        fwif_color_set_LD_Global_Ctrl(src_idx, TableIdx);
                                        fwif_color_set_LD_Backlight_Decision(src_idx, TableIdx);
                                        fwif_color_set_LD_Spatial_Filter(src_idx, TableIdx);
                                        fwif_color_set_LD_Spatial_Remap(src_idx, TableIdx);
                                        fwif_color_set_LD_Boost(src_idx,TableIdx);
                                        fwif_color_set_LD_Temporal_Filter(src_idx, TableIdx);
                                        fwif_color_set_LD_Backlight_Final_Decision(src_idx, TableIdx);
                                        fwif_color_set_LD_Data_Compensation(src_idx, TableIdx);
                                        fwif_color_set_LD_Data_Compensation_NewMode_2DTable(src_idx,TableIdx);
                                        fwif_color_set_LD_Backlight_Profile_Interpolation(src_idx, TableIdx);
                                        fwif_color_set_LD_BL_Profile_Interpolation_Table(src_idx, TableIdx);
                                        fwif_color_set_LD_Demo_Window(src_idx, TableIdx);
                                        fwif_color_set_LED_Initialize(PANEL_INFO_T);
                                        fwif_color_set_LD_CtrlSPI_init(PANEL_INFO_T); /*HW Dora provided this script*/
                                        rtd_printk(KERN_DEBUG, TAG_NAME, "kernel VPQ_LED_IOC_SET_LD_INIT success\n");
                                        retval = 0;

                                }

                        }
                }
                break;
                case V4L2_CID_EXT_LED_CONTROL_SPI:
                {
                        unsigned char LDCtrlSPI[2];
                        struct v4l2_ext_led_spi_ctrl_info stRealInfo;
                        //if (PQ_LED_Dev_Status != PQ_LED_DEV_INIT_DONE)
                        //      return -1;

                        if(!ext_control.ptr)
                        {
                                printk(KERN_ERR "func:%s line:%d [error] ext_control.ptr is null\r\n",__FUNCTION__, __LINE__);
                                retval = -EFAULT;
                                return retval;
                        }
                        else
                        {

                                if(copy_from_user((void *)&pqData, to_user_ptr(ext_control.ptr), sizeof(struct v4l2_vpq_cmn_data)))
                                {
                                        printk(KERN_ERR "func:%s line:%d [error] copy_from_user pqData fail \r\n",__FUNCTION__,__LINE__);
                                        retval = -EFAULT;
                                        return retval;
                                }


                                if(copy_from_user((void *)&stRealInfo, to_user_ptr(pqData.p_data), sizeof(struct v4l2_ext_led_spi_ctrl_info)))
                                {

                                        printk(KERN_ERR "func:%s line:%d [error] copy_from_user stRealInfo fail \r\n",__FUNCTION__,__LINE__);
                                        retval = -EFAULT;
                                        return retval;

                                }
                                else
                                {

                                        LDCtrlSPI[0] = stRealInfo.bitMask;
                                        LDCtrlSPI[1] = stRealInfo.ctrlValue;

                                        vpqled_HAL_VPQ_LED_LDCtrlSPI(&LDCtrlSPI[0]);
                                        rtd_printk(KERN_EMERG, TAG_NAME, "rord bitMask=%d,ctrlValue=%d \n",stRealInfo.bitMask,stRealInfo.ctrlValue);
                                        retval = 0;

                                }

                        }
                }
                break;

				case V4L2_CID_EXT_VPQ_EXTRA_PATTERN:
				{	
					struct v4l2_vpq_cmn_data stPqContainer;
					struct v4l2_vpq_ext_pattern_info stRealInfo;
					struct v4l2_vpq_ext_pattern_winbox_info ext_pattern_winbox_info;
					inner_pattern_winbox_win_attr inner_info_array[2] = {{0}, {0}};	
					if(copy_from_user((void *)&stPqContainer, to_user_ptr(ext_control.ptr), sizeof(struct v4l2_vpq_cmn_data)))
					{
						printk(KERN_ERR "func:%s [error] V4L2_CID_EXT_VPQ_EXTRA_PATTERN copy_from_user stPqContainer fail \r\n",__FUNCTION__);
						retval = -EFAULT;
						return retval;
					}
					if(stPqContainer.p_data)
					{
			            if(copy_from_user((void *)&stRealInfo, to_user_ptr(stPqContainer.p_data), sizeof(struct v4l2_vpq_ext_pattern_info)))
	                    {
                            printk(KERN_ERR "func:%s [error] V4L2_CID_EXT_VPQ_EXTRA_PATTERN copy_from_user stRealInfo fail \r\n",__FUNCTION__);
                            retval = -EFAULT;
                            return retval;

	                    }
	                    else
	                    {
							if(stRealInfo.eMode == V4L2_VPQ_EXT_PATTERN_WINBOX)
							{
								if(stRealInfo.pstWinboxInfo)
								{
									if(copy_from_user((void *)&ext_pattern_winbox_info, to_user_ptr(stRealInfo.pstWinboxInfo), sizeof(struct v4l2_vpq_ext_pattern_winbox_info)))
				                    {
			                            printk(KERN_ERR "func:%s [error] V4L2_CID_EXT_VPQ_EXTRA_PATTERN copy_from_user stRealInfo.pstWinboxInfo fail \r\n",__FUNCTION__);
			                            retval = -EFAULT;
			                            return retval;
				                    }
									else
									{
										if(stRealInfo.bOnOff)
										{
											inner_info_array[0].x = ext_pattern_winbox_info.stWinBoxAttr[0].x;
											inner_info_array[0].y = ext_pattern_winbox_info.stWinBoxAttr[0].y;
											inner_info_array[0].w = ext_pattern_winbox_info.stWinBoxAttr[0].w;
											inner_info_array[0].h = ext_pattern_winbox_info.stWinBoxAttr[0].h;
											inner_info_array[0].fill_R = ext_pattern_winbox_info.stWinBoxAttr[0].fill_R;
											inner_info_array[0].fill_G = ext_pattern_winbox_info.stWinBoxAttr[0].fill_G;
											inner_info_array[0].fill_B = ext_pattern_winbox_info.stWinBoxAttr[0].fill_B;
											if(MAX_EXT_PATTERN_WINBOX >= 2)
											{
												inner_info_array[1].x = ext_pattern_winbox_info.stWinBoxAttr[1].x;
												inner_info_array[1].y = ext_pattern_winbox_info.stWinBoxAttr[1].y;
												inner_info_array[1].w = ext_pattern_winbox_info.stWinBoxAttr[1].w;
												inner_info_array[1].h = ext_pattern_winbox_info.stWinBoxAttr[1].h;
												inner_info_array[1].fill_R = ext_pattern_winbox_info.stWinBoxAttr[1].fill_R;
												inner_info_array[1].fill_G = ext_pattern_winbox_info.stWinBoxAttr[1].fill_G;
												inner_info_array[1].fill_B = ext_pattern_winbox_info.stWinBoxAttr[1].fill_B;
											}
										}
										ctrl_inner_pattern(stRealInfo.bOnOff, inner_info_array);
									}
								}
								else
								{
									printk(KERN_ERR "func:%s [error] V4L2_CID_EXT_VPQ_EXTRA_PATTERN fail stRealInfo.pstWinboxInfo is null\r\n",__FUNCTION__);
			                        retval = -EFAULT;
			                        return retval;
								}
							}
							else
							{
								printk(KERN_ERR "func:%s [error] V4L2_CID_EXT_VPQ_EXTRA_PATTERN stRealInfo.eMode(%d) not support \r\n",__FUNCTION__, stRealInfo.eMode);
								retval = -EFAULT;
								return retval;
							}
								
	                    }
					}
					else
					{
						printk(KERN_ERR "func:%s [error] V4L2_CID_EXT_VPQ_EXTRA_PATTERN fail pqData.p_data is null\r\n",__FUNCTION__);
                        retval = -EFAULT;
                        return retval;
					}
						
					break;
				}

            default:
                    break;
        }

        return 0;
}



int vpq_v4l2_main_ioctl_g_ext_ctrls(struct file *file, void *fh, struct v4l2_ext_controls *ctrls)
{//vsc disconnect, get vdo info
#if 0
    struct v4l2_ext_control controls;
    struct v4l2_ext_vsc_connect_info info;
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
    if(copy_from_user((void *)&controls, (const void __user *)ctrls->controls, sizeof(struct v4l2_ext_control)))
    {

        printk(KERN_ERR "func:%s [error] copy_from_user controls fail \r\n",__FUNCTION__);
        return -EFAULT;
    }
    else
    {
        if(controls.id == V4L2_CID_EXT_VSC_CONNECT_INFO)
{
            if(!controls.ptr)
            {
                printk(KERN_ERR "func:%s [error] controls->ptr is null\r\n",__FUNCTION__);
                return -EFAULT;
            }
            if(copy_from_user((void *)&info, (const void __user *)controls.ptr, sizeof(struct v4l2_ext_vsc_connect_info)))
            {
                printk(KERN_ERR "func:%s [error] copy_from_user controls->ptr fail \r\n",__FUNCTION__);
                return -EFAULT;
            }
            if(info.in.src != V4L2_EXT_VSC_INPUT_SRC_NONE)
            {
                printk(KERN_ERR "func:%s [error] info.in.src(%d) is not none\r\n",__FUNCTION__, (unsigned int)info.in.src);
                return -EFAULT;
            }

            if(info.out != V4L2_EXT_VSC_DEST_NONE)
            {
                printk(KERN_ERR "func:%s [error] info.out(%d) is not none\r\n",__FUNCTION__, (unsigned int)info.out);
                return -EFAULT;
            }
            if(!vsc_v4l2_vsc_control(SLR_MAIN_DISPLAY, FALSE, info))
            {
                printk(KERN_ERR "func:%s [error] vsc_v4l2_vsc_connect_control fail \r\n",__FUNCTION__);
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
            if(copy_to_user(to_user_ptr((controls.ptr)), &main_vdo_mode, sizeof(struct v4l2_ext_vsc_vdo_mode)))
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
            if(copy_to_user(to_user_ptr((controls.ptr)), &main_win_info, sizeof(struct v4l2_ext_vsc_win_region)))
            {
                printk(KERN_ERR "func:%s [error] get in/out info copy_to_user fail \r\n",__FUNCTION__);
                return -EFAULT;
            }
        }
        else
        {
        }


    }
#endif
    return 0;
}

int vpq_v4l2_main_ioctl_s_ctrl(struct file *file, void *fh, struct v4l2_control *ctrl)
{
        int retval = 0;
        struct v4l2_control controls;
        unsigned int cmd = 0xff;

        if(!ctrl)
        {
                printk(KERN_ERR "func:%s [error] ctrls is null\r\n",__FUNCTION__);
                retval = -EFAULT;
                return retval;
        }
        memcpy(&controls, ctrl, sizeof(struct v4l2_control));


        cmd = controls.id;

        pr_notice("vbe_v4l2_main_ioctl_s_ctrl : cmd=%x \n", cmd);

        switch(cmd)
        {
                case V4L2_CID_EXT_MEMC_INIT:
                {
                        rtd_printk(KERN_EMERG, TAG_NAME, "rord ,V4L2_CID_EXT_MEMC_INIT " );
                        rtd_printk(KERN_DEBUG, TAG_NAME, "##############[MEMC]VPQ_V4L2_IOC_MEMC_INITILIZE\n");
                        HAL_VPQ_MEMC_Initialize();

                        break;
                }

                case V4L2_CID_EXT_MEMC_LOW_DELAY_MODE:
                {
                        UINT8 type;
                        type = controls.value;
                        retval = HAL_VPQ_MEMC_LowDelayMode(type);

                        rtd_printk(KERN_EMERG, TAG_NAME, "rord ,V4L2_CID_EXT_MEMC_LOW_DELAY_MODE " );

                }
                break;

                case V4L2_CID_EXT_VPQ_INIT:
                {
                    rtd_printk(KERN_EMERG, TAG_NAME, "rord ,V4L2_CID_EXT_VPQ_INIT " );



                }
                break;
                case V4L2_CID_EXT_LED_EN:
                {
                        extern unsigned char vpq_led_LDEnable;//Run memc mode

                        vpq_led_LDEnable = controls.value;
                        rtd_printk(KERN_EMERG, TAG_NAME, "rord ,V4L2_CID_EXT_LED_EN =%d ,ddd =%d  " ,vpq_led_LDEnable,controls.value);



                }
                break;
                case V4L2_CID_EXT_LED_DB_IDX:
                {
                        extern unsigned char vpq_led_LD_lutTableIndex;

                        vpq_led_LD_lutTableIndex = controls.value;
                        rtd_printk(KERN_EMERG, TAG_NAME, "rord ,V4L2_CID_EXT_LED_DB_IDX =%d \n" ,controls.value);

                        //if (PQ_LED_Dev_Status != PQ_LED_DEV_INIT_DONE)
                        //        return -1;

                        fwif_color_set_LDSetLUT(vpq_led_LD_lutTableIndex);

                }
                break;








                default:
                    break;
        }


    return retval;
}

int vpq_v4l2_main_ioctl_g_ctrl(struct file *file, void *fh, struct v4l2_control *ctrl)
{
        int retval = 0;
        struct v4l2_control controls;
        unsigned int cmd = 0xff;

        if(!ctrl)
        {
            printk(KERN_ERR "func:%s [error] ctrls is null\r\n",__FUNCTION__);
            retval = -EFAULT;
            return retval;
        }
        memcpy(&controls, ctrl, sizeof(struct v4l2_control));


        cmd = controls.id;


        switch (cmd)
        {

                case V4L2_CID_EXT_LED_EN:
                {

                        extern unsigned char vpq_led_LDEnable;
                        ctrl->value = vpq_led_LDEnable;
                        rtd_printk(KERN_EMERG, TAG_NAME, "rord ,vpq_led_LDEnable =%d \n " ,vpq_led_LDEnable);

                }
                break;
                case V4L2_CID_EXT_LED_FIN:
                {

                        extern unsigned char vpq_led_LDEnable;//Run memc mode
                        ctrl->value = 78;
                        rtd_printk(KERN_EMERG, TAG_NAME, "rord ,rrrrrrrrr 3333333333 =%d ,ddd =%d  " ,vpq_led_LDEnable,controls.value);
                 }
                case V4L2_CID_EXT_LED_DB_IDX:
                {
                        extern unsigned char vpq_led_LD_lutTableIndex;

                         ctrl->value =vpq_led_LD_lutTableIndex;
                        rtd_printk(KERN_EMERG, TAG_NAME, "rord ,get V4L2_CID_EXT_LED_DB_IDX =%d \n" ,vpq_led_LD_lutTableIndex);


                }
                break;
                 break;

        }
        return retval;
}

#endif

