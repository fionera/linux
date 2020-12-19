/*******************************************************************************
* @file    scalerColorLib.cpp
* @brief
* @note    Copyright (c) 2014 RealTek Technology Co., Ltd.
*		   All rights reserved.
*		   No. 2, Innovation Road II,
*		   Hsinchu Science Park,
*		   Hsinchu 300, Taiwan
*
* @log
* Revision 0.1	2014/01/27
* create
*******************************************************************************/
/*******************************************************************************
 * Header include
******************************************************************************/

#include <linux/delay.h>
#include <generated/autoconf.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/mtd/mtd.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <asm/cacheflush.h>
//#include <mach/RPCDriver.h>
#include <linux/pageremap.h>
#include <mach/system.h>
#include <linux/hrtimer.h>

#include <tvscalercontrol/vip/vip_reg_def.h>
//#include <rbus/rbusHistogramReg.h>
//#include <rbus/scaler/rbusPpOverlayReg.h>
#include <rbus/ppoverlay_reg.h>

//#include <rbus/rbusODReg.h>
//#include <rbus/rbus_DesignSpec_MISC_LSADCReg.h>
//#include <rbus/rbusColorReg.h>
//#include <rbus/rbusCon_briReg.h>
//#include <rbus/rbusYuv2rgbReg.h>
//#include <rbus/rbusScaleupReg.h>
//#include <rbus/rbusColor_dccReg.h>
//#include <rbus/rbusGammaReg.h>
//#include <rbus/rbusInv_gammaReg.h>
#include <rbus/dm_reg.h>
#include <rbus/hdr_all_top_reg.h>
#include <rbus/vgip_reg.h>
#include <rbus/vodma_reg.h>
//#include "rbus/rbusHDMIReg.h"
#include <rbus/h3ddma_hsd_reg.h>


#if CONFIG_FIRMWARE_IN_KERNEL
/*#include </merlin_rbus/hdr/hdr_all_top_reg.h>*/ //Need SW sync,  SW Team not sync from TV001
#else
#include <rbus/hdr_all_top_reg.h>
#endif

//#include <rbus/dtg_reg.h>

/*#include <Application/AppClass/SetupClass.h>*/

/*#include "tvscalercontrol/scaler/scalerSuperResolution.h"*/
/*#include <tvscalercontrol/scaler/scalertimer.h>*/
#include <tvscalercontrol/scaler/scalercolorlib.h>
#include <tvscalercontrol/scaler/scalercolorlib_tv001.h>
/*#include <tvscalercontrol/scaler/scalerlib.h>*/
#include <tvscalercontrol/scalerdrv/scalerdisplay.h>
#include <tvscalercontrol/scalerdrv/pipmp.h>
//#include <tvscalercontrol/scalerdrv/scalerdrv.h>

#include <tvscalercontrol/vip/ultrazoom.h>
#include <tvscalercontrol/vip/scalerColor.h>
#include <tvscalercontrol/vip/scalerColor_tv006.h>
#include <tvscalercontrol/vip/dcc.h>
#include <tvscalercontrol/vip/intra.h>
#include <tvscalercontrol/vip/ultrazoom.h>
#include <tvscalercontrol/vip/color.h>
#include <tvscalercontrol/vip/peaking.h>
#include <tvscalercontrol/vip/nr.h>
#include <tvscalercontrol/vip/xc.h>
#include <tvscalercontrol/vip/di_ma.h>
#include <tvscalercontrol/vip/gibi_od.h>
#include <tvscalercontrol/vip/viptable.h>

#include <tvscalercontrol/hdmirx/hdmifun.h>

#include <tvscalercontrol/vdc/video.h>
#include <tvscalercontrol/io/ioregdrv.h>
#include <tvscalercontrol/panel/panelapi.h>
/*#include <Platform_Lib/Board/pcbMgr.h>*/
/*#include <Platform_Lib/Board/pcb.h>*/

#ifdef CONFIG_KDRIVER_USE_NEW_COMMON
	#include <scaler/scalerDrvCommon.h>
#else
#include <scalercommon/scalerDrvCommon.h>
#endif

#include <tvscalercontrol/scaler_vscdev.h>
#include <tvscalercontrol/i3ddma/i3ddma_drv.h>
#include <mach/rtk_log.h>

#include "tvscalercontrol/scaler_vpqmemcdev.h"
#include <tvscalercontrol/vip/localcontrast.h>

#include "memc_isr/scalerMEMC.h"


#define TAG_NAME "VPQ"

/*******************************************************************************
* Macro
******************************************************************************/
#define GET_USER_INPUT_SRC()					(Scaler_GetUserInputSrc(SLR_MAIN_DISPLAY))

#define _SUCCESS		1
#define _FAIL			0
#define _ENABLE 			1
#define _DISABLE			0
#define _TRUE			1
#define _FALSE			0

/*******************************************************************************
* Constant
******************************************************************************/
/*#define example  100000 */ /* 100Khz */

/* Enable or Disable VIP flag*/



/*******************************************************************************
 * Structure
 ******************************************************************************/

/*typedef struct*/
/*{*/
/*} MID_example_Param_t;*/


/*******************************************************************************
* enumeration
******************************************************************************/
/*typedef enum*/
/*{*/
/*    MID_Example_SLEEPING = 0,*/
/*    MID_Example_RUNNING,*/
/*} MID_Example_Status_t;*/

/*******************************************************************************
* Variable
******************************************************************************/
//static unsigned char m_nDRLevel;
//static unsigned char m_nSRLevel;
//static unsigned char m_nCGLevel;
//static unsigned char m_nFTLevel;

/*******************************************************************************
* Program
******************************************************************************/
//======================add for TCL Project======================
#define Bin_Num_Gamma_TCL 1024
#define Vmax_Gamma_TCL 4095
UINT16 GOut_R_TCL[Bin_Num_Gamma_TCL + 1], GOut_G_TCL[Bin_Num_Gamma_TCL + 1], GOut_B_TCL[Bin_Num_Gamma_TCL + 1];
UINT16 gamma_scale_TCL = (Vmax_Gamma_TCL + 1) / Bin_Num_Gamma_TCL;
unsigned int final_GAMMA_R_TCL[Bin_Num_Gamma_TCL / 2], final_GAMMA_G_TCL[Bin_Num_Gamma_TCL / 2], final_GAMMA_B_TCL[Bin_Num_Gamma_TCL / 2]; // Num of index: Mac2=128, Sirius=512 (jyyang_2013/12/30)

//******************************************************************************/

/*static void rtd_part_outl(unsigned int reg_addr, unsigned int endBit, unsigned int startBit, unsigned int value)
{
	unsigned int X,A,result;
	X=(1<<(endBit-startBit+1))-1;
	A=rtd_inl(reg_addr);
	result = (A & (~(X<<startBit))) | (value<<startBit);
	rtd_outl(reg_addr,result);
}*/


unsigned int Scaler_GetColorTemp_level_type(TV001_COLORTEMP_E * colorTemp)
{
	unsigned char nLevel;
	nLevel=scaler_get_color_temp_level_type(GET_USER_INPUT_SRC());

	if(nLevel == SLR_COLORTEMP_NORMAL )
		*colorTemp=TV001_COLORTEMP_NATURE;
	else if(nLevel == SLR_COLORTEMP_COOL || nLevel == SLR_COLORTEMP_COOLER)
		*colorTemp=TV001_COLORTEMP_COOL;
	else if(nLevel == SLR_COLORTEMP_WARM || nLevel == SLR_COLORTEMP_WARMER)
		*colorTemp=TV001_COLORTEMP_WARM;
	else if(nLevel == SLR_COLORTEMP_USER )
		*colorTemp=TV001_COLORTEMP_USER;
	else
		*colorTemp=TV001_COLORTEMP_NATURE;
	return _SUCCESS;
}
void Scaler_SetColorTemp_level_type(TV001_COLORTEMP_E colorTemp)
{
	if(colorTemp == TV001_COLORTEMP_NATURE )
		Scaler_SetColorTemp(SLR_COLORTEMP_NORMAL);
	else if(colorTemp == TV001_COLORTEMP_COOL )
		Scaler_SetColorTemp(SLR_COLORTEMP_COOL);
	else if(colorTemp == TV001_COLORTEMP_WARM )
		Scaler_SetColorTemp(SLR_COLORTEMP_WARM);
	else if(colorTemp == TV001_COLORTEMP_USER )
		Scaler_SetColorTemp(SLR_COLORTEMP_USER);
	else
		Scaler_SetColorTemp(SLR_COLORTEMP_NORMAL);
}
unsigned int Scaler_GetColorTempData(TV001_COLOR_TEMP_DATA_S *colorTemp)
{
	unsigned char src_idx = 0;
	unsigned char display = 0;
	Scaler_Get_Display_info(&display, &src_idx);

	colorTemp->redGain = Scaler_get_color_temp_r_type(src_idx);
	colorTemp->greenGain = Scaler_get_color_temp_g_type(src_idx);
	colorTemp->blueGain = Scaler_get_color_temp_b_type(src_idx);
	colorTemp->redOffset = Scaler_get_color_temp_r_offset(src_idx);
	colorTemp->greenOffset = Scaler_get_color_temp_g_offset(src_idx);
	colorTemp->blueOffset = Scaler_get_color_temp_b_offset(src_idx);
	return _SUCCESS;
}
void Scaler_SetColorTempData(TV001_COLOR_TEMP_DATA_S *colorTemp)
{
if(Scaler_VOFromJPEG(Scaler_Get_CurVoInfo_plane()) == 1)
{
         fwif_color_setrgbcontrast_By_Table(512, 512, 512, 0);
	  fwif_color_setrgbbrightness_By_Table(512, 512,512);
}
else
{
	fwif_color_setrgbcontrast_By_Table(colorTemp->redGain, colorTemp->greenGain, colorTemp->blueGain, 0);
       fwif_color_setrgbbrightness_By_Table(colorTemp->redOffset, colorTemp->greenOffset,colorTemp->blueOffset);
}
}

void Scaler_SetBlackPatternOutput(unsigned char isBlackPattern)
{
	fwif_color_set_WB_Pattern_IRE(isBlackPattern, 0);
}
void Scaler_SetWhitePatternOutput(unsigned char isWhitePattern)
{
	fwif_color_set_WB_Pattern_IRE(isWhitePattern, 22);
}
unsigned int Scaler_GetPQModule(TV001_PQ_MODULE_E pqModule,unsigned char * onOff)
{
	*onOff = TRUE;
	return _SUCCESS;
}
void Scaler_SetPQModule(TV001_PQ_MODULE_E pqModule,unsigned char onOff)
{
#ifdef ANDTV_MAC5_COMPILE_FIX
	switch(pqModule) {
#ifdef ANDTV_PQ_COMPILE_FIX
		case TV001_PQ_MODULE_FMD:
		drvif_color_set_PQ_Module(PQ_ByPass_Fmd,onOff);
		break;
#endif
		case TV001_PQ_MODULE_NR:
		drvif_color_set_PQ_Module(PQ_ByPass_RTNR_Y,onOff);
		drvif_color_set_PQ_Module(PQ_ByPass_SNR_Y,onOff);
		break;

		case TV001_PQ_MODULE_DB:
		drvif_color_set_PQ_Module(PQ_ByPass_MPEG_NR,onOff);
		break;

		case TV001_PQ_MODULE_DR:
		drvif_color_set_PQ_Module(PQ_ByPass_MosquiutoNR,onOff);
		break;

		case TV001_PQ_MODULE_HSHARPNESS:
		drvif_color_set_PQ_Module(PQ_ByPass_SHP_DLTI,onOff);
		break;

		case TV001_PQ_MODULE_SHARPNESS:
		drvif_color_set_PQ_Module(PQ_ByPass_SHP,onOff);
		drvif_color_set_PQ_Module(PQ_ByPass_SHP_DLTI,onOff);
		drvif_color_set_PQ_Module(PQ_ByPass_MB_Peaking,onOff);
		break;

#ifdef ANDTV_PQ_COMPILE_FIX
		case TV001_PQ_MODULE_CCCL:
		drvif_color_set_PQ_Module(PQ_ByPass_CCCL,onOff);
		break;
		case TV001_PQ_MODULE_COLOR_CORING:
		drvif_color_set_PQ_Module(PQ_ByPass_COLOR_CORING,onOff);
		break;
#endif
		case TV001_PQ_MODULE_BLUE_STRETCH:
		if(onOff){
			IoReg_SetBits(0xB802C130, 0x00000003);//_BIT1
		}
		else{
			IoReg_ClearBits(0xB802C130, 0x00000003);//_BIT1
		}
		break;

		case TV001_PQ_MODULE_GAMMA:
		drvif_color_set_PQ_Module(PQ_ByPass_Gamma,onOff);
		break;
#ifdef ANDTV_PQ_COMPILE_FIX
		case TV001_PQ_MODULE_DBC:
		drvif_color_set_PQ_Module(PQ_ByPass_DBC,onOff);
		break;
#endif
		case TV001_PQ_MODULE_DCI:
		drvif_color_set_PQ_Module(PQ_ByPass_DCC,onOff);
		break;

		case TV001_PQ_MODULE_COLOR:
		drvif_color_set_PQ_Module(PQ_ByPass_ICM,onOff);
		break;

		case TV001_PQ_MODULE_ES:
		//drvif_color_set_PQ_Module(PQ_ByPass_RTNR_Y,onOff);
		break;

		case TV001_PQ_MODULE_SR:
		drvif_color_set_PQ_Module(PQ_ByPass_MB_Peaking,onOff);
		break;

		case TV001_PQ_MODULE_FRC:
		HAL_VPQ_MEMC_MotionCompOnOff(onOff);
		break;

		case TV001_PQ_MODULE_WCG:
		drvif_color_set_PQ_Module(PQ_ByPass_ColorTemp,onOff);
		break;

		case TV001_PQ_MODULE_ALL:
		drvif_color_set_PQ_Module(PQ_ByPass_RTNR_Y,onOff);
		drvif_color_set_PQ_Module(PQ_ByPass_SNR_Y,onOff);

		drvif_color_set_PQ_Module(PQ_ByPass_SHP,onOff);
		drvif_color_set_PQ_Module(PQ_ByPass_SHP_DLTI,onOff);
		drvif_color_set_PQ_Module(PQ_ByPass_MB_Peaking,onOff);

		drvif_color_set_PQ_Module(PQ_ByPass_Gamma,onOff);
		drvif_color_set_PQ_Module(PQ_ByPass_DCC,onOff);
		drvif_color_set_PQ_Module(PQ_ByPass_ICM,onOff);
		drvif_color_set_PQ_Module(PQ_ByPass_ColorTemp,onOff);
		HAL_VPQ_MEMC_MotionCompOnOff(onOff);
		if(onOff){
			IoReg_SetBits(0xB802C130, 0x00000003);//_BIT1
		}
		else{
			IoReg_ClearBits(0xB802C130, 0x00000003);//_BIT1
		}
		break;
	}
#endif
}
unsigned int Scaler_GetSDR2HDR(unsigned char * onOff)
{
#ifdef ANDTV_PQ_COMPILE_FIX
	_system_setting_info *VIP_system_info_structure_table;
	VIP_system_info_structure_table = (_system_setting_info *)Scaler_GetShareMemVirAddr(SCALERIOC_VIP_system_info_structure);
	*onOff = VIP_system_info_structure_table->SDR2HDR_flag;
#endif
	return _SUCCESS;
}
void Scaler_SetSDR2HDR(unsigned char onOff)
{
	//Scaler_LGE_ONEKEY_SDR2HDR_Enable(onOff);
	drvif_color_set_LC_Enable(onOff);
}
unsigned int Scaler_GetHdr10Enable(unsigned char * bEnable)
{
	//_system_setting_info *VIP_system_info_structure_table;
	//VIP_system_info_structure_table = (_system_setting_info *)Scaler_GetShareMemVirAddr(SCALERIOC_VIP_system_info_structure);
	_RPC_system_setting_info *VIP_RPC_system_info_structure_table;
	VIP_RPC_system_info_structure_table = (_RPC_system_setting_info *)Scaler_GetShareMemVirAddr(SCALERIOC_VIP_RPC_system_info_structure);

	if(VIP_RPC_system_info_structure_table == NULL){
		printk("[%s:%d] Warning here!! RPC_system_info=NULL! \n",__FILE__, __LINE__);
		return _FAIL;
	}

	*bEnable = VIP_RPC_system_info_structure_table->HDR_info.Ctrl_Item[TV006_HDR_En];
	return _SUCCESS;
}
void Scaler_SetHdr10Enable(unsigned char bEnable)
{
	Scaler_LGE_ONEKEY_HDR10_Enable(bEnable);
}


unsigned int Scaler_GetSrcHdrInfo(unsigned int * pGammaType)
{
	*pGammaType = TRUE;
	return _SUCCESS;
}
unsigned int Scaler_GetHdrType(TV001_HDR_TYPE_E * pHdrType)
{
#ifdef ANDTV_PQ_COMPILE_FIX
	_RPC_system_setting_info *VIP_RPC_system_info_structure_table;
	VIP_RPC_system_info_structure_table = (_system_setting_info *)Scaler_GetShareMemVirAddr(SCALERIOC_VIP_RPC_system_info_structure);

	if(VIP_RPC_system_info_structure_table == NULL){
		printk("[%s:%d] Warning here!! RPC_system_info=NULL! \n",__FILE__, __LINE__);
		return _FAIL;
	}

	if(VIP_RPC_system_info_structure_table->SDR2HDR_flag)
		*pHdrType = TV001_HDR_TYPE_SDR_TO_HDR;
	else if(VIP_RPC_system_info_structure_table->DolbyHDR_flag)
		*pHdrType = TV001_HDR_TYPE_DOLBY_HDR;
	else if(VIP_RPC_system_info_structure_table->HDR_info.Ctrl_Item[TV006_HDR_En])
		*pHdrType = TV001_HDR_TYPE_HDR10;
	else
		*pHdrType = TV001_HDR_TYPE_SDR;
#endif
	return _SUCCESS;
}
unsigned int Scaler_GetDOLBYHDREnable(unsigned char * bEnable)
{
	_system_setting_info *VIP_system_info_structure_table;
	VIP_system_info_structure_table = (_system_setting_info *)Scaler_GetShareMemVirAddr(SCALERIOC_VIP_system_info_structure);
	*bEnable = VIP_system_info_structure_table->DolbyHDR_flag;
	return _SUCCESS;
}
void Scaler_SetDOLBYHDREnable(unsigned char bEnable)
{
	unsigned char en[HDR_ITEM_MAX] = {0};

	en[HDR_EN] = bEnable;
	Scaler_set_HDR10_Enable(en);
}


void Scaler_SetDemoMode(TV001_DEMO_MODE_E demoMode,unsigned char onOff)
{
	_system_setting_info *VIP_system_info_structure_table;
	_RPC_system_setting_info *VIP_system_RPC_info_structure_table;
	VIP_system_info_structure_table = (_system_setting_info *)Scaler_GetShareMemVirAddr(SCALERIOC_VIP_system_info_structure);
	VIP_system_RPC_info_structure_table = (_RPC_system_setting_info *)Scaler_GetShareMemVirAddr(SCALERIOC_VIP_RPC_system_info_structure);

	if(VIP_system_RPC_info_structure_table == NULL){
		printk("[%s:%d] Warning here!! RPC_system_info=NULL! \n",__FILE__, __LINE__);
		return;
	}


	if(!onOff){
#ifdef ANDTV_PQ_COMPILE_FIX
		if(VIP_system_info_structure_table->SDR2HDR_flag)
			Scaler_LGE_ONEKEY_SDR2HDR_Enable(onOff);
		else
#endif
			if(VIP_system_RPC_info_structure_table->HDR_info.Ctrl_Item[TV006_HDR_En])
				Scaler_LGE_ONEKEY_HDR10_Enable(onOff);

		Scaler_SetMagicPicture(SLR_MAGIC_OFF);
		return;
	}
	switch(demoMode) {
		case TV001_DEMO_DBDR :
		break;
		case TV001_DEMO_NR:
		break;
		case TV001_DEMO_SHARPNESS:
			Scaler_SetMagicPicture(SLR_MAGIC_STILLDEMO_INVERSE);
		break;
		case TV001_DEMO_DCI:
		break;
		case TV001_DEMO_WCG:
			//Scaler_SetMagicPicture(SLR_MAGIC_FULLGAMUT);
		break;
		case TV001_DEMO_MEMC:
			//Scaler_SetMagicPicture(SLR_MAGIC_MEMC_STILLDEMO_INVERSE);
		break;
		case TV001_DEMO_COLOR:
		break;
		case TV001_DEMO_SR:
			Scaler_SetMagicPicture(SLR_MAGIC_STILLDEMO_INVERSE);
		break;
		case TV001_DEMO_ALL:
		break;
		case TV001_DEMO_HDR:
			Scaler_SetMagicPicture(SLR_MAGIC_STILLDEMO_INVERSE);
		break;
		case TV001_DEMO_SDR_TO_HDR:
			Scaler_SetMagicPicture(SLR_MAGIC_STILLDEMO_INVERSE);
			Scaler_LGE_ONEKEY_SDR2HDR_Enable(TRUE);
		break;
		default:
			break;
	}


}



unsigned int Scaler_GetMemcEnable(unsigned char * bEnable)
{
	unsigned char  nMode;
	nMode = (unsigned char)MEMC_LibGetMEMCMode();
	if(nMode == 3)
		*bEnable = FALSE;
	else
		*bEnable = TRUE;
	return _SUCCESS;
}
unsigned int Scaler_GetMemcRange(TV001_MEMC_RANGE_S *range)
{
	range->min= 0;
	range->max= 128;
	return _SUCCESS;
}
void Scaler_SetMemcLevel(TV001_LEVEL_E level)
{
	if(level == TV001_LEVEL_OFF)
		HAL_VPQ_MEMC_SetMotionComp(0,0,VPQ_MEMC_TYPE_OFF);
	else if(level == TV001_LEVEL_LOW)
		HAL_VPQ_MEMC_SetMotionComp(16,32,VPQ_MEMC_TYPE_LOW);
	else if(level == TV001_LEVEL_MID)
		HAL_VPQ_MEMC_SetMotionComp(94,94,VPQ_MEMC_TYPE_LOW);
	else if(level == TV001_LEVEL_HIGH)
		HAL_VPQ_MEMC_SetMotionComp(128,128,VPQ_MEMC_TYPE_HIGH);
	else if(level == TV001_LEVEL_AUTO)
		HAL_VPQ_MEMC_SetMotionComp(128,128,VPQ_MEMC_TYPE_HIGH);
	else
		HAL_VPQ_MEMC_SetMotionComp(128,128,VPQ_MEMC_TYPE_HIGH);
}

void fwif_color_gamma_encode_TableSize(RTK_TableSize_Gamma *pData)
{
	unsigned short i;
	unsigned short gamma_temp[4];
	unsigned char g_scaler = 4;		// TCL input gamma is 10-bits and 256-scaler      1024 / 256 = 4
	unsigned char j;

	SLR_VIP_TABLE *gVip_Table = fwif_colo_get_AP_vip_table_gVIP_Table();
	if (gVip_Table == NULL) {
		printk("~get vipTable Error return, %s->%d, %s~\n", __FILE__, __LINE__, __FUNCTION__);
		return;
	}

	for(i = 0 ; i < 256 ; i++)
	{
		//printk("-lhh001- TCL_g[%d] = %x    %x    %x\n", i, pData->pu16Gamma_r[i], pData->pu16Gamma_g[i], pData->pu16Gamma_b[i]);
		if(i < 255)
		{
			for(j = 0 ; j < 4 ; j++)
			{
				gamma_temp[j] = (pData->pu16Gamma_r[i] + (pData->pu16Gamma_r[i + 1] - pData->pu16Gamma_r[i]) * j / g_scaler) << 2;
			}
			gVip_Table->tGAMMA[0].R[i * 2] = ((gamma_temp[0] << 16) | ((gamma_temp[1] - gamma_temp[0]) << 8) | (0x04));
			gVip_Table->tGAMMA[0].R[i * 2 + 1] = ((gamma_temp[2] << 16) | ((gamma_temp[3] - gamma_temp[2]) << 8) | (0x04));

			for(j = 0 ; j < 4 ; j++)
			{
				gamma_temp[j] = (pData->pu16Gamma_g[i] + (pData->pu16Gamma_g[i + 1] - pData->pu16Gamma_g[i]) * j / g_scaler) << 2;
			}
			gVip_Table->tGAMMA[0].G[i * 2] = ((gamma_temp[0] << 16) | ((gamma_temp[1] - gamma_temp[0]) << 8) | (0x04));
			gVip_Table->tGAMMA[0].G[i * 2 + 1] = ((gamma_temp[2] << 16) | ((gamma_temp[3] - gamma_temp[2]) << 8) | (0x04));

			for(j = 0 ; j < 4 ; j++)
			{
				gamma_temp[j] = (pData->pu16Gamma_b[i] + (pData->pu16Gamma_b[i + 1] - pData->pu16Gamma_b[i]) * j / g_scaler) << 2;
			}
			gVip_Table->tGAMMA[0].B[i * 2] = ((gamma_temp[0] << 16) | ((gamma_temp[1] - gamma_temp[0]) << 8) | (0x04));
			gVip_Table->tGAMMA[0].B[i * 2 + 1] = ((gamma_temp[2] << 16) | ((gamma_temp[3] - gamma_temp[2]) << 8) | (0x04));
		}
		else
		{
			for(j = 0 ; j < 4 ; j++)
			{
				gamma_temp[j] = (pData->pu16Gamma_r[i] + (1024 - pData->pu16Gamma_r[i]) * j / g_scaler) << 2;
			}
			gVip_Table->tGAMMA[0].R[i * 2] = ((gamma_temp[0] << 16) | ((gamma_temp[1] - gamma_temp[0]) << 8) | (0x04));
			gVip_Table->tGAMMA[0].R[i * 2 + 1] = ((gamma_temp[2] << 16) | ((gamma_temp[3] - gamma_temp[2]) << 8) | (0x04));

			for(j = 0 ; j < 4 ; j++)
			{
				gamma_temp[j] = (pData->pu16Gamma_g[i] + (1024 - pData->pu16Gamma_g[i]) * j / g_scaler) << 2;
			}
			gVip_Table->tGAMMA[0].G[i * 2] = ((gamma_temp[0] << 16) | ((gamma_temp[1] - gamma_temp[0]) << 8) | (0x04));
			gVip_Table->tGAMMA[0].G[i * 2 + 1] = ((gamma_temp[2] << 16) | ((gamma_temp[3] - gamma_temp[2]) << 8) | (0x04));

			for(j = 0 ; j < 4 ; j++)
			{
				gamma_temp[j] = (pData->pu16Gamma_b[i] + (1024 - pData->pu16Gamma_b[i]) * j / g_scaler) << 2;
			}
			gVip_Table->tGAMMA[0].B[i * 2] = ((gamma_temp[0] << 16) | ((gamma_temp[1] - gamma_temp[0]) << 8) | (0x04));
			gVip_Table->tGAMMA[0].B[i * 2 + 1] = ((gamma_temp[2] << 16) | ((gamma_temp[3] - gamma_temp[2]) << 8) | (0x04));
		}


	}

	//for(i=0 ; i<512 ; i++)
		//printk("-lhh002- VIP_g[%d] = %x    %x    %x \n", i, gVip_Table->tGAMMA[0].R[i], gVip_Table->tGAMMA[0].G[i], gVip_Table->tGAMMA[0].B[i]  );


}

