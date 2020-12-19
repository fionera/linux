/* hdr10+ header */
#ifndef  __HDR10PLUS_H__
#define  __HDR10PLUS_H__

#ifdef __cplusplus
extern "C" {
#endif
/* some include about scaler*/
#if CONFIG_FIRMWARE_IN_KERNEL
#include <scaler/vipCommon.h>
#else
#include <scaler/vipCommon.h>
#endif

#define OETF_max 1048575 // 2^20 for bezier_curve
#define OETF_gamma22_Lo_Hi_th_HDR10p 1024
#define OETF_gamma22_Lo_HDR10p_size 33
#define OETF_gamma22_Hi_HDR10p_size 1024
#define distribution_maxrgb_bin_num 10
//#define bezier_curve_anchors_num 15
#define bezier_curve_anchors_num 10
#if 1/*========================== HDR10 +==========================================*/
typedef struct _long_int
{
	int high;
	unsigned int low;
} long_int;

typedef struct _HDR10PLUS_metadata
{
	unsigned int application_version;
	unsigned int targeted_system_display_maximum_luminance;
	unsigned int maxscl[3];
	unsigned int average_maxrgb;
	unsigned int num_distribution_maxrgb_percentiles;
	unsigned int distribution_maxrgb_percentages[distribution_maxrgb_bin_num];
	unsigned int distribution_maxrgb_percentiles[distribution_maxrgb_bin_num];
	unsigned int tone_mapping_flag;
	unsigned int knee_point_x;
	unsigned int knee_point_y;
	unsigned int num_bezier_curve_anchors;
	unsigned int bezier_curve_anchors[bezier_curve_anchors_num];

} HDR10PLUS_metadata;

typedef struct _VIP_HDR10p_DMA_CTRL {
	unsigned int size;
	unsigned int phy_addr_align;
	unsigned int *pVir_addr_align;

} VIP_HDR10p_DMA_CTRL;

typedef struct _VIP_HDR10p_CTRL {
	unsigned int Enable;

	unsigned int Debug_Log;
	unsigned int Debug_Delay;

	unsigned int Debug_basisOOTF_kneePx;
	unsigned int Debug_basisOOTF_kneePy;
	unsigned int Debug_basisOOTF_P[bezier_curve_anchors_num];
	unsigned int Debug_basisOOTF_Fixed_KneePoint;
	unsigned int Debug_basisOOTF_Fixed_Anchors;

	unsigned int Debug_GuidedOOTF_kneePx;
	unsigned int Debug_GuidedOOTF_kneePy;
	unsigned int Debug_GuidedOOTF_P[bezier_curve_anchors_num];

	unsigned int Debug_VGIP_Line_sta;
	unsigned int Debug_VGIP_Line_end;
	unsigned int Debug_VGIP_Line_diff;
	unsigned int Debug_90clk_sta;
	unsigned int Debug_90clk_end;
	unsigned int Debug_90clk_diff;

	VIP_HDR10p_DMA_CTRL EOTF_DMA;
	VIP_HDR10p_DMA_CTRL OETF_DMA;
	VIP_HDR10p_DMA_CTRL HDR_3D_LUT_DMA;

} VIP_HDR10p_CTRL;

typedef struct _VIP_HDR10p_TBL {
	unsigned int curve_sel;
	unsigned int EOTF_norm_div;
	unsigned int EOTF_Linear_Blend_W;
	unsigned int EOTF_Gamma22;
	unsigned int basis_peak;
	unsigned int product_peak;
	unsigned int product_min;

	unsigned int basisOOTF_order;

} VIP_HDR10p_TBL;

typedef struct _BasisOOTF_Params
{

	//==============================
	// Knee-Point (KP) parameters
	//==============================
	// KP ramp base thresholds (two bounds KP # 1 and KP # 2 are computed)
	unsigned int SY1_V1;
	unsigned int SY1_V2;
	unsigned int SY1_T1;
	unsigned int SY1_T2;

	unsigned int SY2_V1;
	unsigned int SY2_V2;
	unsigned int SY2_T1;
	unsigned int SY2_T2;

	// KP mixing gain (final KP from bounds KP # 1 and KP # 2 as a function of scene percentile)
	unsigned int KP_G_V1;
	unsigned int KP_G_V2;
	unsigned int KP_G_T1;
	unsigned int KP_G_T2;

	//==============================
	// P coefficient parameters
	//==============================
	// Thresholds of minimum bound of P1 coefficient
	unsigned int P1_LIMIT_V1;
	unsigned int P1_LIMIT_V2;
	unsigned int P1_LIMIT_T1;
	unsigned int P1_LIMIT_T2;

	// Thresholds to compute relative shape of curve (P2~P9 coefficient) by pre-defined bounds - as a function of scene percentile
	unsigned int P2To9_T1;
	unsigned int P2To9_T2;

	// Defined relative shape bounds (P2~P9 coefficient) for a given maximum TM dynamic compression (eg : 20x )
	unsigned int P2ToP9_MAX1[10-2];
	unsigned int P2ToP9_MAX2[10-2];

	// Ps mixing gain (obtain all Ps coefficients) - as a function of TM dynamic compression ratio
	unsigned int PS_G_T1;
	unsigned int PS_G_T2;

	// Post-processing : Reduce P1/P2 (to enhance mid tone) for high TM dynamic range compression cases
	unsigned int LOW_SY_T1;
	unsigned int LOW_SY_T2;
	unsigned int LOW_K_T1;
	unsigned int LOW_K_T2;
	unsigned int RED_P1_V1;
	unsigned int RED_P1_T1;
	unsigned int RED_P1_T2;
	unsigned int RED_P2_V1;
	unsigned int RED_P2_T1;
	unsigned int RED_P2_T2;

} BasisOOTF_Params;

typedef enum _VIP_HDR10p_Demo_Mode {
	HDR10p_Demo_OFF = 0,
	//HDR10p_Demo_ON_HDR10plus,
	HDR10p_Demo_ON_HDR10,

	VIP_HDR10p_Demo_Mode_MAX,

} VIP_HDR10p_Demo_Mode;

typedef enum _VIP_HDR10p_Demo_CTRL_LAYER {
	HDR10p_flow_layer = 0,
	HDR10p_engMenu_layer,

	VIP_HDR10p_layer_MAX,

} VIP_HDR10p_Demo_CTRL_LAYER;

#if 1/*========================== HDR10 +==========================================*/
unsigned char drvif_Get_HDR10plus_EFuse_Status(void);

void drvif_fwif_color_ScalerVIP_set_HDR10p_metadata2reg_frameSync(int display_Brightness, _RPC_system_setting_info *RPC_sys_info, VIP_HDR10p_CTRL* HDR10p_CTRL);
HDR10PLUS_metadata* drvif_fwif_color_Get_HDR10p_METADATA_Buff(void);
VIP_HDR10p_CTRL* drvif_fwif_color_Get_HDR10p_Ctrl(void);
VIP_HDR10p_TBL* drvif_fwif_color_Get_HDR10p_TBL(void);
int drvif_fwif_color_Get_HDR10p_demo_flag(unsigned char layer);
void drvif_fwif_color_Set_HDR10p_demo_flag(int flag, unsigned char layer);

void drvif_fwif_color_Set_HDR10plus_EOTF(unsigned int sourceMaxL, unsigned int *virAddr, unsigned int phyAddr, VIP_HDR10p_CTRL* HDR10p_CTRL);
void drvif_fwif_color_Set_HDR10plus_OETF(HDR10PLUS_metadata *metadata, unsigned int sourceMaxL, unsigned int *virAddr, unsigned int phyAddr, VIP_HDR10p_CTRL* HDR10p_CTRL);

void drvif_color_Set_HDR_OETF_Decrease( unsigned char dec );
void drvif_color_Set_HDR10plus_3Dlut_CInv(unsigned short shift_bit, unsigned char dim);

#endif

#endif/*========================== HDR10 +==========================================*/



#ifdef __cplusplus
}
#endif

#endif // __TCHDR_H__


