/**
* This product contains one or more programs protected under international
* and U.S. copyright laws as unpublished works.  They are confidential and
* proprietary to Dolby Laboratories.  Their reproduction or disclosure, in
* whole or in part, or the production of derivative works therefrom without
* the express permission of Dolby Laboratories is prohibited.
*
*             Copyright 2011 - 2015 by Dolby Laboratories.
*                           All rights reserved.
*
*
* @brief Control Path private header file.
* @file control_path_priv.h
*
*/

#ifndef CONTROL_PATH_PRIV_H_
#define CONTROL_PATH_PRIV_H_
//RTK MARK
#ifdef RTK_EDBEC_CONTROL
#include <generated/autoconf.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <asm/barrier.h>
#include <asm/cacheflush.h>
#include <mach/irqs.h>
#include <mach/platform.h>
#include <rtk_kdriver/panelConfigParameter.h>
#include <mach/system.h>

#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/random.h>
#include <linux/pageremap.h>
#include <linux/kthread.h>
#include <linux/time.h>
#include "typedefs.h"		// added by smfan
#include "control_path_api.h"
#else
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#endif
#include <control_path_std.h>
#include <KdmTypeFxp.h>
#include <VdrDmApi.h>
#include <CdmTypePriFxp.h>
#include <dm2_x/VdrDmAPIpFxp.h>
#include <target_display_config.h>

#ifdef __cplusplus
extern "C" {
#endif


#define DM_MS_WEIGHT_UNDEFINED_VALUE    0x1FFF
#define LOG2_UI_STEPSIZE                3
#define BL_SCALER_SCALE                 12 /* scale for the backlight scaler is 12 */
#define MAX_GD_RISE_FALLWEIGHT          4096 /* GD rise fall weight range is 0 to 1 (12 bit scale) */

#define SDR_DEFAULT_GAMMA      39322 /* 39322 = Gamma 2.4 */
#define SDR_DEFAULT_MIN_LUM     1310 /*     1310 = 0.005 nits*/
#define SDR_DEFAULT_MAX_LUM 26214400 /* 26214400 =   100 nits */

#define WP_D65x 20984942 /* = 0.3127 * 2^26 */
#define WP_D65y 22078816 /* = 0.3290 * 2^26 */
#define RGBW_SCALE 26

typedef struct cp_context_s cp_context_t;

/*RTK add , for kmalloc without internal fragmention*/

typedef struct cp_context_s_half_st cp_context_t_half_st;
struct cp_context_s_half_st
{
    DmKsFxp_t h_ks_st;
    MdsExt_t h_mds_ext_st;
    DmCtxtFxp_t h_dm_st;
};

/*! @brief Library context.
*/
struct cp_context_s
{
    HDmFxp_t      h_dm;      /**<@brief DM context handle        */
    HMdsExt_t     h_mds_ext; /**<@brief DM metadata context      */
    HDmKsFxp_t    h_ks;      /**<@brief DM kernel handle         */
    DmCfgFxp_t    dm_cfg;    /**<@brief DM config handle         */
    Mmg_t         mmg;       /**<@brief Context of DM memory manager */
#ifdef RTK_EDBEC_1_5
    DmExecFxp_t   h_dmExec;
    DmExecFxp_t   dmExec;
    DmExecFxp_t_rtk   dmExec_rtk;
    dm_lut_t             dm_lut;
    composer_register_t dst_comp_reg;
    dm_register_t        dm_reg;
#endif

    #if DM_VER_LOWER_THAN212
    GmLut_t       gm_lut;         /**<@brief Color gamut mapping LUT */
    GmLut_t       gm_lut_mode1;   /**<@brief Color gamut mapping LUT for run_mode 1 (absolute) */
    GmLut_t       gm_lut_mode2;   /**<@brief Color gamut mapping LUT for run_mode 2 (relative) */
    #if ALIGN_LUTS
    uint16_t tmp_lut[GM_LUT_SIZE/2+1]; /* temporary aligned buffer for the three LUTs */
    #endif
    #endif

    pq_config_t* cur_pq_config;
    #if IPCORE
    uint32_t active_area_left_offset  ;
    uint32_t active_area_right_offset ;
    uint32_t active_area_top_offset   ;
    uint32_t active_area_bottom_offset;
    #endif

    rpu_ext_config_fixpt_main_t tmp_comp_metadata;
    int last_view_mode_id;
    int last_bl_scaler;
    signal_format_t last_input_format;
    ui_menu_params_t last_ui_menu_params;
    int dbgExecParamsPrintPeriod;
};
#ifdef RTK_EDBEC_CONTROL
// 3d Lut
#define DM_LUT_VERSION_CHARS        4
#define DM_LUT_ENDIAN_CHARS         1
#define DM_LUT_TYPE_CHARS           8
#define LUT3D_BCMS_FILE_SIZE       ( DM_LUT_VERSION_CHARS + DM_LUT_ENDIAN_CHARS + DM_LUT_TYPE_CHARS + (9*2) + (2 * 3*17*17*17)    )
#define DEF_L2PQ_LUT_X_SCALE2P      18	//14.18
#define DEF_L2PQ_LUT_A_SCALE2P      23	// 5.23
#define DEF_L2PQ_LUT_B_SCALE2P      16	// 0.16
#define DEF_L2PQ_LUT_NODES          128

typedef struct DmLut_t_ {
    ////// lut hdr( chars+1 for '\0' )
    char Version[DM_LUT_VERSION_CHARS + 1]; // 4 CC
    char Endian[DM_LUT_ENDIAN_CHARS + 1];    // 1 CC, b: big or l: little endian
    char Type[DM_LUT_TYPE_CHARS + 1];        // 8 CC, ???2???_

    short DimC1, DimC2, DimC3; // dim for C1, C2, C3
    short IdxMinC1, IdxMaxC1, IdxMinC2, IdxMaxC2, IdxMinC3, IdxMaxC3;  // the min, max idx

    ////// lut data
    unsigned short *LutMap; // the LUT, type depends on version and type

    unsigned short crc16;
    unsigned short reserved[8];
} DmLut_t;


typedef enum ImpMethod_t_ {
	IMP_METHOD_C = 0,
	IMP_METHOD_GPU,
} ImpMethod_t;

typedef enum AlgMethod_t_ {
	ALG_METHOD_ALG = 0,
	ALG_METHOD_LUT,
	ALG_METHOD_NUM
} AlgMethod_t;


#if 0
// added by smfan for MD output structure
typedef struct __attribute__ ((__packed__)) DmExecFxp_MdOutput_t_
{
	////// processing block control
	// if GPU or CPU impl:IMP_METHOD_C, IMP_METHOD_GPU
	ImpMethod_t implMethod; // NOT USED IN FIXED POINT INSTANCE

	////// kernel control
	// run mode
	input_mode_t runMode;
	int16_t lowCmplxMode;
	uint16_t haveGr; // if graphic enabled

	// source range
	uint16_t sRangeMin, sRange;
	uint32_t sRangeInv;

	//// forward
	// yuv2rgb xfer
	int16_t m33Yuv2RgbScale2P;
	int16_t m33Yuv2Rgb[3][3];
	// v3Yuv2RgbOffInRgb: scaled by 1<<m33Yuv2RgbScale2P
	// v3Rgb = (m33Yuv2Rgb * v3Yuv - v3Yuv2RgbOffInRgb)/(1<<m33Yuv2RgbScale2P)
	int32_t v3Yuv2RgbOffInRgb[3];
	// gamma stuff
	cp_eotf_t sEotf;		// EOTF_MODE_BT1886, EOTF_MODE_PQ coded
	uint16_t sA, sB, sGamma;
	uint32_t sG;
	// rgb=>lms or other
	int16_t m33Rgb2OptScale2P;
	int16_t m33Rgb2Opt[3][3];
	// lms or other to ipt or other
	int16_t m33Opt2OptScale2P;
	int16_t m33Opt2Opt[3][3];
	int32_t Opt2OptOffset;
	// Helmoltz-Kohlrasush scaling
	uint16_t chromaWeight; // 64K - 1 scale
	// lut in the context
	int32_t fwPrf;			// NOT USED IN FIXED POINT INSTANCE

	//// TC
	//TcCtrlFxp_t tcCtrl;
	uint16_t tcLut[DEF_FXP_TC_LUT_SIZE];	// modified by smfan ; array instead of pointer

	//// blending
	uint16_t msFilterScale;
	int16_t msFilterRow[11];
	int16_t msFilterCol[5];
	int16_t msFilterEdgeRow[11];
	int16_t msFilterEdgeCol[5];
	uint16_t msWeight, msWeightEdge;

	//// backward
	// if use LUT or alg: ALG_METHOD_ALG, ALG_METHOD_LUT
	AlgMethod_t bwMethod;	// NOT USED IN FIXED POINT INSTANCE
	DmLutFxp_t bwDmLut;
	// gain/offset stuff
	uint16_t gain, offset; // 4K - 1 scale
	// Saturation control
	uint16_t saturationGain;
	// Gamma stuff
	cp_eotf_t tEotf;			// NOT USED IN FIXED POINT INSTANCE
	uint16_t tRangeMin, tRange;
	uint32_t tRangeOverOne;
	int32_t bwPrf;			// NOT USED IN FIXED POINT INSTANCE

	// LUTs for low complexity
	uint16_t gamma2PqLutSrc[512];
	uint16_t gamma2PqLutTgt[512];
	uint16_t pq2GammaLut[512];

#if EN_GLOBAL_DIMMING
	// added by smfan
	uint16_t pwm_duty;
	uint8_t update3DLut;
	int16_t lvl5AoiAvail;
	unsigned short active_area_left_offset, active_area_right_offset;
	unsigned short active_area_top_offset, active_area_bottom_offset;
#endif
} DmExecFxp_MdOutput_t;
#endif
//typedef struct __attribute__ ((__packed__)) DmExecFxp_MdOutput_part1_t_
typedef struct DmExecFxp_MdOutput_part1_t_
{
	RunMode_t runMode;
	uint32_t colNum;
	uint32_t rowNum;
	uint32_t in_clr;
	uint32_t in_bdp;
	uint32_t in_chrm;

	// source range
	uint16_t sRangeMin, sRange;
	uint32_t sRangeInv;

	//// forward
	// yuv2rgb xfer
	int16_t m33Yuv2RgbScale2P;
	int16_t m33Yuv2Rgb[3][3];
	// v3Yuv2RgbOffInRgb: scaled by 1<<m33Yuv2RgbScale2P
	// v3Rgb = (m33Yuv2Rgb * v3Yuv - v3Yuv2RgbOffInRgb)/(1<<m33Yuv2RgbScale2P)
	int32_t v3Yuv2RgbOffInRgb[3];
#if REDUCED_COMPLEXITY
	uint32_t g2L[DEF_G2L_LUT_SIZE];
#endif
	// gamma stuff
	KEotf_t sEotf;		// EOTF_MODE_BT1886, EOTF_MODE_PQ coded
	uint16_t sA, sB, sGamma;
	uint32_t sG;
	// rgb=>lms or other
	int16_t m33Rgb2OptScale2P;
	int16_t m33Rgb2Opt[3][3];
	// lms or other to ipt or other
	int16_t m33Opt2OptScale2P;
	int16_t m33Opt2Opt[3][3];
	int32_t Opt2OptOffset;
	// Helmoltz-Kohlrasush scaling
	uint16_t chromaWeight; // 64K - 1 scale
	// lut in the context
} DmExecFxp_MdOutput_part1_t;


// added by smfan for MD output structure
typedef struct __attribute__ ((__packed__)) DmExecFxp_MdOutput_part3_t_
{
	//// blending
	uint16_t msFilterScale;
	int16_t msFilterRow[11];
	int16_t msFilterCol[5];
	int16_t msFilterEdgeRow[11];
	int16_t msFilterEdgeCol[5];
	uint16_t msWeight, msWeightEdge;

	//// backward
	// if use LUT or alg: ALG_METHOD_ALG, ALG_METHOD_LUT
	//AlgMethod_t bwMethod;	// NOT USED IN FIXED POINT INSTANCE

	DmLutFxp_t bwDmLut;
	// gain/offset stuff
	uint16_t gain, offset; // 4K - 1 scale
	// Saturation control
	uint16_t saturationGain;
	uint16_t tRangeMin, tRange;
	uint32_t tRangeOverOne;

	uint32_t tRangeInv;
	uint32_t minC[3]; //idk15 removed
	uint32_t maxC[3]; //idk15 removed
	uint32_t CInv[3]; //idk15 removed
	// to support output xyz/rgb=>yuv conversion.
	int16_t m33Rgb2YuvScale2P;
	int16_t m33Rgb2Yuv[3][3];
	int32_t v3Rgb2YuvOff[3];
	uint32_t out_clr;
	uint32_t out_bdp;

	uint32_t lut_type;

# if EN_AOI
   // def of aoi[aoiRow0, aoiCol0; aoiRow1Plus1, aoiCol1Plus1)
	int16_t aoiRow0, aoiCol0, aoiRow1Plus1, aoiCol1Plus1;
# endif

	//uint16_t gamma2PqLutSrc[512];//idk15 removed
	//uint16_t gamma2PqLutTgt[512];//idk15 removed
	//uint16_t pq2GammaLut[512];//idk15 removed

#if EN_GLOBAL_DIMMING
	// added by smfan
	uint16_t pwm_duty;
	uint8_t update3DLut;
	int16_t lvl5AoiAvail;
	unsigned short active_area_left_offset, active_area_right_offset;
	unsigned short active_area_top_offset, active_area_bottom_offset;
#endif
} DmExecFxp_MdOutput_part3_t;


int rtk_dm_exec_params_to_DmExecFxp(DmKsFxp_t *pKs,
	DmExecFxp_t *pDmExecFxp_t,
	DmExecFxp_t_rtk *pDmExecFxp_t_rtk,
	MdsExt_t *p_MdsExt_t,
	dm_lut_t *p_dm_lut,
	pq_config_t* pq_config,
	input_mode_t mode);
#endif

#ifdef __cplusplus
}
#endif


#endif //CONTROL_PATH_API_H_
