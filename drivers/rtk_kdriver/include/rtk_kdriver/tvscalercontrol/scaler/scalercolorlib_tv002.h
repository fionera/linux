#ifndef __SCALERCOLOR_LIB_TV002_H__
#define __SCALERCOLOR_LIB_TV002_H__

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * Header include
******************************************************************************/


//#include <tvscalercontrol/vip/icm.h>
//#include <tvscalercontrol/scaler/source.h>
//#include <tvscalercontrol/scaler/vipinclude.h>
//#include <tvscalercontrol/vip/color.h>
//#include <tvscalercontrol/vip/scalerColor.h>
#include <scaler/vipRPCCommon.h>


/*******************************************************************************
* Macro
******************************************************************************/



/*******************************************************************************
* Constant
******************************************************************************/
/*#define example  100000*/ /* 100Khz */




/*******************************************************************************
 * Structure
 ******************************************************************************/
/*typedef struct*/
/*{*/
/*} MID_example_Param_t;*/


/*******************************************************************************
* enumeration
******************************************************************************/


/*******************************************************************************
* Variable
******************************************************************************/
/*static unsigned char gExample = 100000;*/ /* 100K */

//extern Scaler_ICM_Block_Adj icm_block_adj;

/*******************************************************************************
* Program
******************************************************************************/


/*o---------------------------------------------------------------------------o*/
/*o-------------Scalercolor.cpp------------------------------------o*/
/*o---------------------------------------------------------------------------o*/


/*==================== Definitions ================= */

/************************************************************/
/******************* For TV002 use*****************************/
/************************************************************/
typedef enum _VIP_Project_Style_TV002{
	Project_TV002_OFF = 0, /* is not TV002 style*/
	Project_TV002_Style_1,
	Project_TV002_Style_2,

	VIP_Project_Style_TV002_MAX,

}VIP_Project_Style_TV002;

// id pattern TV002
typedef enum _ID_ITEM_TV002 {
	ID_Radio_TV002 =0,
	ID_FaceArtifactNoise_TV002,
	ID_Raining_0_TV002,
	ID_Raining_1_TV002,
	ID_Snow_TV002,
	ID_BlueArcBridge_TV002,
	ID_DarkScene_Road_Lamp_TV002,
	ID_MonoScope_TV002,
	ID_Tunnel_TV002,
	ID_RockMountain_TV002,
	ID_QtechBamboo_TV002,
	//ID_TempleZooming_TV002,
	//ID_DarkWheelNoise_TV002,
	//ID_BrightnessDarkness_11_TV002,
	//ID_TestChart_06_07_TV002,
	//ID_BlackWomenFace,
	ID_OsceanWave_TV002,
	ID_WaterfallStair_TV002,
	ID_MovingClockTower_TV002,
	ID_MovingPTG_Mono_TV002,
	ID_MovingCircle_TV002,
	ID_MovingRoof_TV002,
	ID_NoiseCat_TV002,
	ID_SaillingShip_TV002,
	ID_Grilled_Meat_TV002,

	ID_ITEM_TV002_MAX,

}ID_ITEM_TV002;

typedef struct _VIP_Dynamic_by_Srcdet_TSH_Hist_CTRL {
	unsigned int SHP_COEF_TH0_up;
	unsigned int SHP_COEF_TH0_down;
	unsigned int SNR_TH0_up;
	unsigned int SNR_TH0_down;
	unsigned int Debounce_TH;

	unsigned int Debug_Log;
	unsigned int Debug_Delay;
	unsigned int Debug_En;
	unsigned int Debug_StepCNT;

} VIP_Dynamic_by_Srcdet_TSH_Hist_CTRL;

typedef enum _VIP_Cal_Clarity_Resolution_ITEM_TV002 {
	// 2D Peaking
	Cal_Clarity_Resolution_RING_GEN_GAIN_TV002 = 0,
	// MB Peaking
	Cal_Clarity_Resolution_MB_Peaking_H_Gain_Pos_TV002,
	Cal_Clarity_Resolution_MB_Peaking_H_Gain_Neg_TV002,
	Cal_Clarity_Resolution_MB_Peaking_H_LV_TV002,
	Cal_Clarity_Resolution_MB_Peaking_H_Sy_P0_TV002,
	Cal_Clarity_Resolution_MB_Peaking_H_Sy_P1_TV002,
	Cal_Clarity_Resolution_MB_Peaking_H_Sy_P2_TV002,
	Cal_Clarity_Resolution_MB_Peaking_V_Gain_Pos_TV002,
	Cal_Clarity_Resolution_MB_Peaking_V_Gain_Neg_TV002,
	Cal_Clarity_Resolution_MB_Peaking_V_LV_TV002,
	Cal_Clarity_Resolution_MB_Peaking_V_Sy_P0_TV002,
	Cal_Clarity_Resolution_MB_Peaking_V_Sy_P1_TV002,
	Cal_Clarity_Resolution_MB_Peaking_V_Sy_P2_TV002,
	// SU Peaking
	Cal_Clarity_Resolution_SU_Peaking_H_Gain_Pos_TV002,
	Cal_Clarity_Resolution_SU_Peaking_H_Gain_Neg_TV002,
	Cal_Clarity_Resolution_SU_Peaking_H_LV_TV002,
	Cal_Clarity_Resolution_SU_Peaking_H_Sy_P0_TV002,
	Cal_Clarity_Resolution_SU_Peaking_H_Sy_P1_TV002,
	Cal_Clarity_Resolution_SU_Peaking_H_Sy_P2_TV002,
	Cal_Clarity_Resolution_SU_Peaking_V_Gain_Pos_TV002,
	Cal_Clarity_Resolution_SU_Peaking_V_Gain_Neg_TV002,
	Cal_Clarity_Resolution_SU_Peaking_V_LV_TV002,
	Cal_Clarity_Resolution_SU_Peaking_V_Sy_P0_TV002,
	Cal_Clarity_Resolution_SU_Peaking_V_Sy_P1_TV002,
	Cal_Clarity_Resolution_SU_Peaking_V_Sy_P2_TV002,

	Cal_Clarity_Resolution_ITEM_TV002_MAX,

} VIP_Cal_Clarity_Resolution_ITEM_TV002;

typedef struct _BL_CTRL_ITEM_INIT_T {
	unsigned char bBL_PWM_PARAM_SET;
	unsigned char bBL_CURRENT_PARAM_SET;
	unsigned char bBL_BLINK_PARAM_SET;
	unsigned char bBL_PWM_FLAT_PARAM_SET;
	unsigned char bBL_CONFIG_SET;
	unsigned char bBL_DBC_CONFIG_SET;
	unsigned char bBL_LD_LIB_OUT;
	unsigned char bBL_DBC_BLENDING_OUT;
} BL_CTRL_ITEM_INIT_T;

typedef struct _BL_DBC_ITEM_CHECK_T {
	unsigned short	DBC_APL_CTL;
	unsigned short	DBC_MAX_CTL;
	unsigned short	DBC_BK_CTL;
	unsigned short	DBC_BK2_CTL;
	unsigned short	DBC_GFX_GAIN;
	unsigned short	DBC4PS_BL_CONT;
	unsigned short	BL_GAIN;
	unsigned short	HEAT_BLSAVE;
	unsigned short	LD_PIXEL_GAIN;
} BL_DBC_ITEM_CHECK_T;

#define SOPQ_LIB_SHARE_MEM_SIZE 3670016 //3.5MB

/************************************************************/
/******************* For TV002 use*****************************/
/************************************************************/

/*=========================================================*/
void Scaler_set_Init_TV002(unsigned char mode);

/*==================== Functions ================== */
void Scaler_set_Intelligent_Picture_Enable(unsigned char En_flag);
unsigned char Scaler_get_Intelligent_Picture_Enable(void);
void Scaler_set_Intelligent_Picture_Optimisation(unsigned char Level);
unsigned char Scaler_get_Intelligent_Picture_Optimisation(void);
void Scaler_set_Intelligent_Picture_Signal_Level_Indicator_ONOFF(unsigned char En_Flag);
unsigned char Scaler_Intelligent_Picture_get_Signal_Level_Indicator_ONOFF(void);
unsigned char Scaler_Intelligent_Picture_get_Intelligent_Picture_Signal_Level(void);
VIP_MAGIC_GAMMA_Function* Scaler_get_Magic_Gamma(void);
void Scaler_set_BP_initial(VIP_SOURCE_TIMING VIP_SrcTiming, PICTURE_MODE Pic_Mode);
unsigned char Scaler_Get_ADV_API_LEVEL_TBL_Idx(VIP_SOURCE_TIMING VIP_SrcTiming, VIP_ADV_API_LEVEL API_Level);
char Scaler_get_sGamma(void);
void Scaler_set_sGamma(char level_index);
unsigned char Scaler_get_BLE(void);
void Scaler_set_BLE(unsigned char level_index);
unsigned char Scaler_get_LiveColor(void);
void Scaler_set_LiveColor(unsigned char level_index);
unsigned char Scaler_get_Clarity_RealityCreation(void);
void Scaler_set_Clarity_RealityCreation(unsigned char level);
unsigned char Scaler_get_Clarity_Resolution(void);
void Scaler_set_Clarity_Resolution(unsigned char level);
void Scaler_Set_Clarity_Resolution_TV002(void);
void Scaler_Cal_2D_Peaking_Clarity_Resolution_TV002(DRV_Sharpness_Table *pSharpness_Table, unsigned char idx);
void Scaler_Cal_MB_Peaking_Clarity_Resolution_TV002(VIP_MBPK_Table *pMBPK, unsigned char value);
void Scaler_Cal_SU_Peaking_Clarity_Resolution_TV002(VIP_MBPK_Table *pMBPK, unsigned char value);
unsigned char Scaler_get_Clarity_Mastered4K(void);
void Scaler_set_Clarity_Mastered4K(unsigned char level);

/************************************************************/
/******************* For TV002 use*****************************/
/************************************************************/
SLR_VIP_TABLE_CUSTOM_TV002* Scaler_get_tv002_Custom_struct(void);
char Scaler_set_tv002_Custom_TBL_to_other_TBL(SLR_VIP_TABLE_CUSTOM_TV002* VIP_TABLE_CUSTOM_TV002);
unsigned char Scaler_access_Project_TV002_Style(unsigned char isSet_Flag, unsigned char TV002_style_Mode);
unsigned char Scaler_get_DBC_blacklight_Value_TV002(void);
char Scaler_set_DBC_UI_blacklight_Value_TV002(unsigned char value);
char Scaler_set_DBC_POWER_Saving_Mode_TV002(unsigned char value);
char Scaler_set_UI_Picture_Mode_TV002(unsigned char value);

unsigned short Scaler_scalerVIP_DBC_get_APL_TV002(SLR_VIP_TABLE_CUSTOM_TV002 *vipCustomTBL_ShareMem, _system_setting_info* system_info_structure_table, _clues *SmartPic_clue, unsigned int MA_print_count);
unsigned char Scaler_scalerVIP_DBC_BASIC_BLIGHT_Cal_TV002(SLR_VIP_TABLE_CUSTOM_TV002 *vipCustomTBL_ShareMem, _system_setting_info* system_info_structure_table, unsigned int MA_print_count);
unsigned char Scaler_scalerVIP_DBC_DBC_Cal_TV002(SLR_VIP_TABLE_CUSTOM_TV002 *vipCustomTBL_ShareMem, _system_setting_info* system_info_structure_table, _clues *SmartPic_clue, unsigned int MA_print_count);
unsigned char Scaler_scalerVIP_DBC_Y_Max_Cal_TV002(_clues *SmartPic_clue, unsigned int MA_print_count);
unsigned char Scaler_scalerVIP_DBC_BackLight_Cal_TV002(SLR_VIP_TABLE_CUSTOM_TV002 *vipCustomTBL_ShareMem, _system_setting_info* system_info_structure_table, _clues *SmartPic_clue, unsigned int MA_print_count);
void Scaler_scalerVIP_DBC_CTRL_TV002(SLR_VIP_TABLE_CUSTOM_TV002 *vipCustomTBL_ShareMem, _system_setting_info* system_info_structure_table, _clues *SmartPic_clue, _RPC_clues* RPC_SmartPic_clue, unsigned int MA_print_count);
unsigned char Scaler_scalerVIP_DBC_LUT_TV002(SLR_VIP_TABLE_CUSTOM_TV002 *vipCustomTBL_ShareMem, unsigned char APL, unsigned char Dev, unsigned int MA_print_count);
unsigned char Scaler_scalerVIP_DBC_Smooth_TV002(unsigned char DBC, SLR_VIP_TABLE_CUSTOM_TV002 *vipCustomTBL_ShareMem, _system_setting_info* system_info_structure_table, _clues *SmartPic_clue, _RPC_clues* RPC_SmartPic_clue, unsigned int MA_print_count);
char Scaler_scalerVIP_DBC_DCC_S_High_Compensation_TV002(SLR_VIP_TABLE_CUSTOM_TV002 *vipCustomTBL_ShareMem, unsigned char APL, unsigned char Dev, unsigned char power_saving_mode, unsigned int MA_print_count);
char Scaler_scalerVIP_DBC_DCC_S_Index_Compensation_TV002(SLR_VIP_TABLE_CUSTOM_TV002 *vipCustomTBL_ShareMem, unsigned char APL, unsigned char Dev, unsigned char power_saving_mode, unsigned int MA_print_count);

void Scaler_scalerVIP_ZERO_D_CTRL_TV002(SLR_VIP_TABLE_CUSTOM_TV002 *vipCustomTBL_ShareMem, _system_setting_info* system_info_structure_table, _clues *SmartPic_clue, unsigned int MA_print_count);

/*BP*/
unsigned short Scaler_scalerVIP_Brightness_Plus_Cal_Gain_By_TBL_TV002(SLR_VIP_TABLE* vipTBL_shareMem, _clues* SmartPic_clue, _system_setting_info* sysInfo_TBL, SLR_VIP_TABLE_CUSTOM_TV002 *vipCustomTBL_ShareMem, unsigned char APL, unsigned int MA_print_count);
unsigned short Scaler_scalerVIP_Brightness_Plus_Cal_OverSat_Protect_Gain_TV002(unsigned short BP_Gain, SLR_VIP_TABLE_CUSTOM_TV002 *vipCustomTBL_ShareMem, _clues* SmartPic_clue, unsigned int MA_print_count);
unsigned short Scaler_scalerVIP_Brightness_Plus_Smooth_Gain_Cal_TV002(unsigned short BP_Gain, SLR_VIP_TABLE_CUSTOM_TV002 *vipCustomTBL_ShareMem, unsigned int MA_print_count);
short Scaler_scalerVIP_Brightness_Plus_Smooth_Offset_Cal_TV002(short RGB_Offset, SLR_VIP_TABLE_CUSTOM_TV002 *vipCustomTBL_ShareMem, unsigned int MA_print_count);
short Scaler_scalerVIP_Brightness_Plus_Compensation_RGB_Offset_Cal_TV002(unsigned short BP_Gain, unsigned short APL, SLR_VIP_TABLE_CUSTOM_TV002 *vipCustomTBL_ShareMem, _system_setting_info* sysInfo_TBL, unsigned int MA_print_count);
void Scaler_scalerVIP_Brightness_Plus_Function_TV002(SLR_VIP_TABLE* vipTBL_shareMem, _clues* SmartPic_clue, _system_setting_info* sysInfo_TBL, SLR_VIP_TABLE_CUSTOM_TV002 *vipCustomTBL_ShareMem, unsigned int MA_print_count);
void Scaler_scalerVIP_Set_YUV2RGB_By_Brightness_Plus_TV002(unsigned short BP_Gain, short BP_Offset, _system_setting_info* sysInfo_TBL, SLR_VIP_TABLE_CUSTOM_TV002 *vipCustomTBL_ShareMem, unsigned int MA_print_count);

void Scaler_scalerVIP_set_sharpness_level_Directly_TV002(DRV_Sharpness_Level *ptr);
unsigned char Scaler_scalerVIP_Access_sharpness_level_TBL_TV002(DRV_Sharpness_Level *ptr, unsigned char isSet_Flag);

void Scaler_scalerVIP_ID_TV002(SLR_VIP_TABLE* ShareMem_VIP_TBL, _RPC_clues* RPC_SmartPic_clue, _clues* SmartPic_clue, _RPC_system_setting_info* RPC_system_infoTBL, _system_setting_info* system_infoTBL, unsigned int MA_print_count);
void Scaler_scalerVIP_ID_Apply_TV002(SLR_VIP_TABLE* ShareMem_VIP_TBL, _RPC_system_setting_info* RPC_system_infoTBL, _system_setting_info* system_infoTBL, _clues* SmartPic_clue, unsigned int MA_print_count);
void Scaler_scalerVIP_ID_Apply_PQA_LV_TV002(SLR_VIP_TABLE* ShareMem_VIP_TBL, _RPC_system_setting_info* RPC_system_infoTBL, _system_setting_info* system_infoTBL, _clues* SmartPic_clue, unsigned int MA_print_count);
void Scaler_scalerVIP_ID_Apply_2dPeaking_TV002(SLR_VIP_TABLE* ShareMem_VIP_TBL, _RPC_system_setting_info* RPC_system_infoTBL, _system_setting_info* system_infoTBL, _clues* SmartPic_clue, unsigned int MA_print_count);
void Scaler_scalerVIP_ID_Apply_DI_TV002(SLR_VIP_TABLE* ShareMem_VIP_TBL, _RPC_system_setting_info* RPC_system_infoTBL, _system_setting_info* system_infoTBL, _clues* SmartPic_clue, unsigned int MA_print_count);
void Scaler_scalerVIP_ID_Apply_MPEG_TV002(SLR_VIP_TABLE* ShareMem_VIP_TBL, _RPC_system_setting_info* RPC_system_infoTBL, _system_setting_info* system_infoTBL, _clues* SmartPic_clue, unsigned int MA_print_count);
VIP_Dynamic_by_Srcdet_TSH_Hist_CTRL* Scaler_Get_Dynamic_setting_byStepCNT_CTRL_STRUCT(void);
void Scaler_scalerVIP_Dynamic_setting_byStepCNT(unsigned int step_cnt, _RPC_system_setting_info* RPC_system_infoTBL, SLR_VIP_TABLE* ShareMem_VIP_TBL, _system_setting_info* sysInfo_TBL, unsigned int MA_print_count);

void Scaler_scalerVIP_Dynamic_DCC_Step_byAPL_TV002(SLR_VIP_TABLE* vipTBL_shareMem, _clues* SmartPic_clue, _system_setting_info* sysInfo_TBL, SLR_VIP_TABLE_CUSTOM_TV002 *vipCustomTBL_ShareMem, unsigned int MA_print_count);
//unsigned char Scaler_get_colorspacergb2yuvtransfer_table_TV002(unsigned char channel, unsigned char nSrcType, unsigned char nSD_HD, bool bUse_HDMI_Range_Detect);
//void Scaler_scalerVIP_Dynamic_DI_RTNR_TV002(SLR_VIP_TABLE* ShareMem_VIP_TBL, _RPC_system_setting_info* RPC_system_infoTBL, _system_setting_info* system_infoTBL, _clues* SmartPic_clue, unsigned int MA_print_count);

unsigned char Scaler_get_gamma_decoded_TV002(unsigned short *In_R, unsigned short *In_G, unsigned short *In_B);

void Scaler_scalerVIP_compute_level_I_002_RFCVBS_RTNR(SLR_VIP_TABLE* vipTable_shareMem, _clues* SmartPic_clue, _system_setting_info* system_info_structure_table, unsigned int MA_print_count);
void Scaler_scalerVIP_compute_level_I_002_RTNR0_tv002(SLR_VIP_TABLE* vipTable_shareMem, _clues* SmartPic_clue, _system_setting_info* system_info_structure_table, unsigned int MA_print_count);
void Scaler_scalerVIP_compute_level_I_002_RTNR1_tv002(SLR_VIP_TABLE* vipTable_shareMem, _clues* SmartPic_clue, _system_setting_info* system_info_structure_table, unsigned int MA_print_count);
void Scaler_scalerVIP_compute_level_I_002_RTNR2_tv002(SLR_VIP_TABLE* vipTable_shareMem, _clues* SmartPic_clue, _system_setting_info* system_info_structure_table, unsigned int MA_print_count);
void Scaler_scalerVIP_compute_level_I_002_SHP0_tv002(SLR_VIP_TABLE* vipTable_shareMem, _clues* SmartPic_clue, _system_setting_info* system_info_structure_table, unsigned int MA_print_count);
void Scaler_scalerVIP_compute_level_I_002_SHP1_tv002(SLR_VIP_TABLE* vipTable_shareMem, _clues* SmartPic_clue, _system_setting_info* system_info_structure_table, unsigned int MA_print_count);
void Scaler_scalerVIP_compute_level_I_002_SNR0_tv002(SLR_VIP_TABLE* vipTable_shareMem, _clues* SmartPic_clue, _system_setting_info* system_info_structure_table, unsigned int MA_print_count);
void Scaler_scalerVIP_compute_level_I_002_SNR1_tv002(SLR_VIP_TABLE* vipTable_shareMem, _clues* SmartPic_clue, _system_setting_info* system_info_structure_table, unsigned int MA_print_count);
void Scaler_scalerVIP_compute_level_I_002_Y_Lv_0_tv002(SLR_VIP_TABLE* vipTable_shareMem, _clues* SmartPic_clue, _system_setting_info* system_info_structure_table, unsigned int MA_print_count);
void Scaler_scalerVIP_compute_level_I_002_SHA_gain(SLR_VIP_TABLE* vipTable_shareMem, _clues* SmartPic_clue, _system_setting_info* system_info_structure_table, unsigned int MA_print_count);
void Scaler_scalerVIP_compute_level_I_002_SHA_G_by_Y(SLR_VIP_TABLE* vipTable_shareMem, _clues* SmartPic_clue, _system_setting_info* system_info_structure_table, unsigned int MA_print_count);
/************************************************************/
/******************* For TV002 use*****************************/
/************************************************************/

void Scaler_DBC_Set_To_Device_TV002(unsigned char isNonISR);
int Scaler_Update_BL_DBC_Param_TV002(unsigned char cmd, unsigned int length, void *pData);
#ifdef CONFIG_CUSTOMER_TV002
unsigned char *Scaler_Get_SoPQLib_ShareMem(void);
unsigned int Scaler_Get_SoPQLib_ShareMem_Size(void);
#endif
#ifdef __cplusplus
}
#endif

#endif /* __SCALER_LIB_H__*/

