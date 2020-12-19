#ifndef __SCALERCOLOR_LIB_TV001_H__
#define __SCALERCOLOR_LIB_TV001_H__

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * Header include
******************************************************************************/


#include <tvscalercontrol/vip/icm.h>
#include <tvscalercontrol/scaler/source.h>
#include <tvscalercontrol/scaler/vipinclude.h>
#include <tvscalercontrol/vip/color.h>
#include <tvscalercontrol/vip/scalerColor.h>


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


typedef struct
{
    unsigned int redGain;
    unsigned int greenGain;
    unsigned int blueGain;
    unsigned int redOffset;
    unsigned int greenOffset;
    unsigned int blueOffset;
}TV001_COLOR_TEMP_DATA_S;


typedef struct
{
    unsigned int min;
    unsigned int max;
}TV001_MEMC_RANGE_S;


/*******************************************************************************
* enumeration
******************************************************************************/
//===============common =====================
typedef enum
{
    TV001_COLORTEMP_NATURE = 0,
    TV001_COLORTEMP_COOL,
    TV001_COLORTEMP_WARM,
    TV001_COLORTEMP_USER,

    TV001_COLORTEMP_MAX
}TV001_COLORTEMP_E;
typedef enum
{
    TV001_LEVEL_OFF = 0,
    TV001_LEVEL_LOW,
    TV001_LEVEL_MID,
    TV001_LEVEL_HIGH,
    TV001_LEVEL_AUTO,

    TV001_LEVEL_MAX
}TV001_LEVEL_E;


typedef enum
{
    TV001_HDR_TYPE_SDR,        /**< SDR*/
    TV001_HDR_TYPE_HDR10,  /**< HDR10 */
    TV001_HDR_TYPE_DOLBY_HDR,  /**< DOLBY  VISION */
    TV001_HDR_TYPE_SDR_TO_HDR, /**< SDR to HDR */

    TV001_HDR_TYPE_MAX,
}TV001_HDR_TYPE_E;
//============================================


typedef enum
{
    TV001_DEMOLEVEL_OFF = 0,
    TV001_DEMOLEVEL_ON,
    TV001_DEMOLEVEL_DEMO,

    TV001_DEMOLEVEL_MAX
}TV001_DEMOLEVEL_E;
typedef enum
{
    TV001_DEMO_DBDR = 0,
    TV001_DEMO_NR,
    TV001_DEMO_SHARPNESS,
    TV001_DEMO_DCI,
    TV001_DEMO_WCG,
    TV001_DEMO_MEMC,
    TV001_DEMO_COLOR,
    TV001_DEMO_SR,
    TV001_DEMO_ALL,
    TV001_DEMO_HDR,
    TV001_DEMO_SDR_TO_HDR,

    TV001_DEMO_MAX
}TV001_DEMO_MODE_E;
typedef enum
{
     TV001_PQ_MODULE_FMD = 0,
     TV001_PQ_MODULE_NR,
     TV001_PQ_MODULE_DB,
     TV001_PQ_MODULE_DR,
     TV001_PQ_MODULE_HSHARPNESS,
     TV001_PQ_MODULE_SHARPNESS,
     TV001_PQ_MODULE_CCCL,
     TV001_PQ_MODULE_COLOR_CORING,
     TV001_PQ_MODULE_BLUE_STRETCH,
     TV001_PQ_MODULE_GAMMA,
     TV001_PQ_MODULE_DBC,
     TV001_PQ_MODULE_DCI,
     TV001_PQ_MODULE_COLOR,
     TV001_PQ_MODULE_ES,
     TV001_PQ_MODULE_SR,
     TV001_PQ_MODULE_FRC,
     TV001_PQ_MODULE_WCG,
     TV001_PQ_MODULE_ALL,

     TV001_PQ_MODULE_MAX
}TV001_PQ_MODULE_E;



typedef enum {
	
	TV030_LINEAR,
	TV030_NONLINEAR,

	TV030_RATIO_TYPE_BUTT,
}TV030_RATIO_TYPE_E;






typedef struct _RTK_TableSize_Gamma {
	unsigned short nTableSize;	
	unsigned short pu16Gamma_r[256];
	unsigned short pu16Gamma_g[256];
	unsigned short pu16Gamma_b[256];
		
} RTK_TableSize_Gamma;


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



/*=========================================================*/


/*==================== Functions ================== */




/*o===========================================================o*/
/*o==================== OSD MENU Start =======================o*/
/*o===========================================================o*/
unsigned int Scaler_GetColorTemp_level_type(TV001_COLORTEMP_E * colorTemp);
void Scaler_SetColorTemp_level_type(TV001_COLORTEMP_E colorTemp);
unsigned int Scaler_GetColorTempData(TV001_COLOR_TEMP_DATA_S *colorTemp);
void Scaler_SetColorTempData(TV001_COLOR_TEMP_DATA_S *colorTemp);

/*o===========================================================o*/
/*o==================== OSD MENU End = =======================o*/
/*o===========================================================o*/
/*o===========================================================o*/
/*o==================== DemoMode Start =======================o*/
/*o===========================================================o*/

void Scaler_SetDemoMode(TV001_DEMO_MODE_E demoMode,unsigned char onOff);

/*o===========================================================o*/
/*o==================== DemoMode End = =======================o*/
/*o===========================================================o*/

void Scaler_SetBlackPatternOutput(unsigned char isBlackPattern);
void Scaler_SetWhitePatternOutput(unsigned char isWhitePattern);
unsigned int Scaler_GetPQModule(TV001_PQ_MODULE_E pqModule,unsigned char * onOff);
void Scaler_SetPQModule(TV001_PQ_MODULE_E pqModule,unsigned char onOff);


void Scaler_SetPanoramaType(TV030_RATIO_TYPE_E ratio);


/*==================== HDR ================== */
unsigned int Scaler_GetSDR2HDR(unsigned char * onOff);
void Scaler_SetSDR2HDR(unsigned char onOff);
unsigned int Scaler_GetHdr10Enable(unsigned char * bEnable);
void Scaler_SetHdr10Enable(unsigned char bEnable);
unsigned int Scaler_GetDOLBYHDREnable(unsigned char * bEnable);
void Scaler_SetDOLBYHDREnable(unsigned char bEnable);
unsigned int Scaler_GetSrcHdrInfo(unsigned int * pGammaType);
unsigned int Scaler_GetHdrType(TV001_HDR_TYPE_E * pHdrType);

/*==================== Localdimming ================== */


/*==================== MEMC ================== */

unsigned int Scaler_GetMemcEnable(unsigned char * bEnable);
unsigned int Scaler_GetMemcRange(TV001_MEMC_RANGE_S *range);
void Scaler_SetMemcLevel(TV001_LEVEL_E level);



/*========================================= */
void fwif_color_gamma_encode_TableSize(RTK_TableSize_Gamma *pData);


#ifdef __cplusplus
}
#endif

#endif /* __SCALER_LIB_H__*/

