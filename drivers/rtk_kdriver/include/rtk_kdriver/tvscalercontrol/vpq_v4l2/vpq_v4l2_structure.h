
/*
 *
 *	VPQ v4l2 structure header file
 *
 *  from customer
 */
#ifndef _VPQ_V4L2_STRUCT_H
#define _VPQ_V4L2_STRUCT_H

//***************************************//
//************* control id **************//
//***************************************//
// You have to make the new control id as the below.

#include <tvscalercontrol/vip/scalerColor_tv006.h>
//#include <common/linux/v4l2-ext/videodev2-ext.h>

#if 0 //use command in v4l2-controls-ext.h, remove this
/* User-class control Bases */
#define V4L2_CID_USER_EXT_PQ_BASE (V4L2_CID_USER_BASE + 0x3000)

/* PQ class control IDs */
#define V4L2_CID_EXT_LED_BASE                            (V4L2_CID_USER_EXT_PQ_BASE)
#define V4L2_CID_EXT_LED_INIT                            (V4L2_CID_EXT_LED_BASE + 0)
#define V4L2_CID_EXT_LED_DB_IDX                          (V4L2_CID_EXT_LED_BASE + 1)
#define V4L2_CID_EXT_LED_DEMOMODE                        (V4L2_CID_EXT_LED_BASE + 2)
#define V4L2_CID_EXT_LED_EN                              (V4L2_CID_EXT_LED_BASE + 3)
#define V4L2_CID_EXT_LED_FIN                             (V4L2_CID_EXT_LED_BASE + 4)
#define V4L2_CID_EXT_LED_DB_DATA                         (V4L2_CID_EXT_LED_BASE + 5)
#define V4L2_CID_EXT_LED_CONTROL_SPI                     (V4L2_CID_EXT_LED_BASE + 6)
#define V4L2_CID_EXT_LED_APL_DATA                        (V4L2_CID_EXT_LED_BASE + 7)

#define V4L2_CID_EXT_MEMC_BASE                           (V4L2_CID_USER_EXT_PQ_BASE + 0x100)
#define V4L2_CID_EXT_MEMC_INIT                           (V4L2_CID_EXT_MEMC_BASE + 0)
#define V4L2_CID_EXT_MEMC_LOW_DELAY_MODE                 (V4L2_CID_EXT_MEMC_BASE + 1)
#define V4L2_CID_EXT_MEMC_MOTION_COMP                    (V4L2_CID_EXT_MEMC_BASE + 2)
#define V4L2_CID_EXT_MEMC_MOTION_PRO                     (V4L2_CID_EXT_MEMC_BASE + 3)

#define V4L2_CID_EXT_HDR_BASE                            (V4L2_CID_USER_EXT_PQ_BASE + 0x200)
#define V4L2_CID_EXT_HDR_INV_GAMMA                       (V4L2_CID_EXT_HDR_BASE + 0)
#define V4L2_CID_EXT_HDR_PIC_INFO                        (V4L2_CID_EXT_HDR_BASE + 1)
#define V4L2_CID_EXT_HDR_3DLUT                           (V4L2_CID_EXT_HDR_BASE + 2)
#define V4L2_CID_EXT_HDR_EOTF                            (V4L2_CID_EXT_HDR_BASE + 3)
#define V4L2_CID_EXT_HDR_OETF                            (V4L2_CID_EXT_HDR_BASE + 4)
#define V4L2_CID_EXT_HDR_TONEMAP                         (V4L2_CID_EXT_HDR_BASE + 5)
#define V4L2_CID_EXT_HDR_COLOR_CORRECTION                (V4L2_CID_EXT_HDR_BASE + 6)
#define V4L2_CID_EXT_HDR_HLG_Y_GAIN_TBL                  (V4L2_CID_EXT_HDR_BASE + 7)

#define V4L2_CID_EXT_DOLBY_BASE                          (V4L2_CID_USER_EXT_PQ_BASE + 0x300)
#define V4L2_CID_EXT_DOLBY_CFG_PATH                      (V4L2_CID_EXT_DOLBY_BASE + 0)
#define V4L2_CID_EXT_DOLBY_PICTURE_MODE                  (V4L2_CID_EXT_DOLBY_BASE + 1)
#define V4L2_CID_EXT_DOLBY_PICTURE_MENU                  (V4L2_CID_EXT_DOLBY_BASE + 2)
#define V4L2_CID_EXT_DOLBY_SW_VERSION                    (V4L2_CID_EXT_DOLBY_BASE + 3)
#define V4L2_CID_EXT_DOLBY_PWM_RATIO                     (V4L2_CID_EXT_DOLBY_BASE + 4)
#define V4L2_CID_EXT_DOLBY_GD_DELAY                      (V4L2_CID_EXT_DOLBY_BASE + 5)
#define V4L2_CID_EXT_DOLBY_AMBIENT_LIGHT                 (V4L2_CID_EXT_DOLBY_BASE + 6)
#define V4L2_CID_EXT_DOLBY_CONTENTS_TYPE                 (V4L2_CID_EXT_DOLBY_BASE + 7)

#define V4L2_CID_EXT_VPQ_BASE                            (V4L2_CID_USER_EXT_PQ_BASE + 0x400)
#define V4L2_CID_EXT_VPQ_INIT                            (V4L2_CID_EXT_VPQ_BASE + 0)
#define V4L2_CID_EXT_VPQ_PICTURE_CTRL                    (V4L2_CID_EXT_VPQ_BASE + 1)
#define V4L2_CID_EXT_VPQ_SHARPNESS                       (V4L2_CID_EXT_VPQ_BASE + 2)
#define V4L2_CID_EXT_VPQ_HISTO_DATA                      (V4L2_CID_EXT_VPQ_BASE + 3)
#define V4L2_CID_EXT_VPQ_DYNAMIC_CONTRAST                (V4L2_CID_EXT_VPQ_BASE + 4)
#define V4L2_CID_EXT_VPQ_DYNAMIC_CONTRAST_LUT            (V4L2_CID_EXT_VPQ_BASE + 5)
#define V4L2_CID_EXT_VPQ_CM_DB_DATA                      (V4L2_CID_EXT_VPQ_BASE + 6)
#define V4L2_CID_EXT_VPQ_NOISE_REDUCTION                 (V4L2_CID_EXT_VPQ_BASE + 7)
#define V4L2_CID_EXT_VPQ_MPEG_NOISE_REDUCTION            (V4L2_CID_EXT_VPQ_BASE + 8)
#define V4L2_CID_EXT_VPQ_BYPASS_BLOCK                    (V4L2_CID_EXT_VPQ_BASE + 9)
#define V4L2_CID_EXT_VPQ_BLACK_LEVEL                     (V4L2_CID_EXT_VPQ_BASE + 10)
#define V4L2_CID_EXT_VPQ_GAMMA_DATA                      (V4L2_CID_EXT_VPQ_BASE + 11)
#define V4L2_CID_EXT_VPQ_SUPER_RESOLUTION                (V4L2_CID_EXT_VPQ_BASE + 12)
#define V4L2_CID_EXT_VPQ_NOISE_LEVEL                     (V4L2_CID_EXT_VPQ_BASE + 13)
#define V4L2_CID_EXT_VPQ_LOW_DELAY_MODE                  (V4L2_CID_EXT_VPQ_BASE + 14)
#define V4L2_CID_EXT_VPQ_DYNAMIC_CONTRAST_BYPASS_LUT     (V4L2_CID_EXT_VPQ_BASE + 15)
#define V4L2_CID_EXT_VPQ_DYNAMIC_CONTRAST_COLOR_GAIN     (V4L2_CID_EXT_VPQ_BASE + 16)
#define V4L2_CID_EXT_VPQ_TESTPATTERN                     (V4L2_CID_EXT_VPQ_BASE + 17)
#define V4L2_CID_EXT_VPQ_COLORTEMP_DATA                  (V4L2_CID_EXT_VPQ_BASE + 18)
#define V4L2_CID_EXT_VPQ_REAL_CINEMA                     (V4L2_CID_EXT_VPQ_BASE + 19)
#define V4L2_CID_EXT_VPQ_GAMUT_3DLUT                     (V4L2_CID_EXT_VPQ_BASE + 20)
#define V4L2_CID_EXT_VPQ_OD_TABLE                        (V4L2_CID_EXT_VPQ_BASE + 21)
#define V4L2_CID_EXT_VPQ_OD_EXTENSION                    (V4L2_CID_EXT_VPQ_BASE + 22)
#define V4L2_CID_EXT_VPQ_LOCALCONTRAST_TABLE             (V4L2_CID_EXT_VPQ_BASE + 23)
#define V4L2_CID_EXT_VPQ_LOCALCONTRAST_DATA              (V4L2_CID_EXT_VPQ_BASE + 24)
#define V4L2_CID_EXT_VPQ_PSP                             (V4L2_CID_EXT_VPQ_BASE + 25)
#define V4L2_CID_EXT_VPQ_GAMUT_MATRIX_PRE                (V4L2_CID_EXT_VPQ_BASE + 26)
#define V4L2_CID_EXT_VPQ_GAMUT_MATRIX_POST               (V4L2_CID_EXT_VPQ_BASE + 27)
#define V4L2_CID_EXT_VPQ_PQ_MODE_INFO                    (V4L2_CID_EXT_VPQ_BASE + 28)
#define V4L2_CID_EXT_VPQ_DEGAMMA_DATA                    (V4L2_CID_EXT_VPQ_BASE + 29)
#define V4L2_CID_EXT_VPQ_LUMINANCE_BOOST                 (V4L2_CID_EXT_VPQ_BASE + 30)
#define V4L2_CID_EXT_VPQ_OBC_DATA                        (V4L2_CID_EXT_VPQ_BASE + 31)
#define V4L2_CID_EXT_VPQ_OBC_LUT                         (V4L2_CID_EXT_VPQ_BASE + 32)
#define V4L2_CID_EXT_VPQ_OBC_CTRL                        (V4L2_CID_EXT_VPQ_BASE + 33)
#define V4L2_CID_EXT_VPQ_DECONTOUR                       (V4L2_CID_EXT_VPQ_BASE + 34)
#define V4L2_CID_EXT_VPQ_EXTRA_PATTERN (V4L2_CID_EXT_VPQ_BASE + 35)
#endif

/*-----------------------------------------------------------------------------
	Local Constant Definitions
------------------------------------------------------------------------------*/
//***************************************//
//*************** parameter *************//
//***************************************//


struct v4l2_ext_picture_ctrl_data_RTK{
 signed int sPcVal[4];
 signed int sContrast;
 signed int sBrightness;
 signed int sSaturation;
 signed int sHue;
};

/* PQ */
// histogram bin, chroma bin num
#define V4L2_EXT_VPQ_BIN_NUM			64
#define V4L2_EXT_VPQ_C_BIN_NUM			32
#define V4L2_EXT_VPQ_H_BIN_NUM		32


// PQ COMMON DATA
struct v4l2_vpq_cmn_data
{
 unsigned int version; ///< version = 0 : wild card(default data)
 unsigned int length; ///< pData Length
 unsigned char wid; ///< 0 : main
 union
 {
  unsigned char *p_data; ///< p_data
  unsigned int compat_data;
  unsigned long long sizer;
 };
};

enum v4l2_ext_hdr_mode_RTK {
    RTK_V4L2_EXT_HDR_MODE_SDR,
    RTK_V4L2_EXT_HDR_MODE_DOLBY,
    RTK_V4L2_EXT_HDR_MODE_HDR10,
    RTK_V4L2_EXT_HDR_MODE_HLG,
    RTK_V4L2_EXT_HDR_MODE_TECHNICOLOR,
    RTK_V4L2_EXT_HDR_MODE_HDREFFECT,
    RTK_V4L2_EXT_HDR_MODE_MAX
};

struct v4l2_ext_hdr_3dlut_RTK {
	//enum v4l2_ext_hdr_mode hdr_mode;
	enum v4l2_ext_hdr_mode_RTK hdr_mode;
	unsigned int data_size;
	union {
		unsigned short *p3dlut; ///< p_data
		unsigned int compat_data;
		unsigned long long sizer;
	};
};

struct v4l2_ext_pq_mode_info_RTK {
	unsigned int hdrStatus; // see v4l2_ext_hdr_mode
	unsigned int colorimetry; // see v4l2_ext_colorimetry_info
	unsigned int peakLuminance; // peakLuminance of panel
	unsigned int supportPrime; // support Prime
	unsigned int reserved;
};

#define CHIP_NUM_TRANSCURVE_RTK 32
#define PQL_DC_SATURATION_LUT_SIZE_RTK 16
#define DYNAMIC_CONTRAST_SATURATION_XY_RTK 1

struct v4l2_vpq_dynamnic_contrast_lut_RTK {
	signed int sLumaLUTxy[CHIP_NUM_TRANSCURVE_RTK];
//#if DYNAMIC_CONTRAST_SATURATION_XY
	unsigned int uSaturationX[PQL_DC_SATURATION_LUT_SIZE_RTK];
	unsigned int uSaturationY[PQL_DC_SATURATION_LUT_SIZE_RTK];
//#endif
} ;



/**
  * Local dimming Demo Mode
  */
struct v4l2_ext_led_apl_info_RTK
{
   unsigned short block_apl_min;
   unsigned short block_apl_max;
};

/**
 *	structure for HAL_VPQ_LED_LDSetDBLUT
 */
enum v4l2_ext_led_ui_adj_type_RTK
{
	v4l2_led_ui_adj_off = 0,
	v4l2_led_ui_adj_sdr_low = 1,
	v4l2_led_ui_adj_sdr_medium = 2,
	v4l2_led_ui_adj_sdr_high = 3,
	v4l2_led_ui_adj_hdr_low = 4,
	v4l2_led_ui_adj_hdr_medium = 5,
	v4l2_led_ui_adj_hdr_high = 6,
	v4l2_led_ui_adj_max
};

struct v4l2_ext_led_lut_RTK
{
   unsigned int lutVersion;
   enum v4l2_ext_led_ui_adj_type_RTK ui_index;
   /* backlight descision */
   unsigned char maxgain;
   unsigned char avrgain;
   unsigned char histoshiftbit;
   unsigned char histogain[8];
   /* spatial filter */
   unsigned char spatialCoeff[27];
   unsigned char temporal_pos_thd_0;
   unsigned char temporal_pos_thd_1;
   unsigned char temporal_pos_gain_min;
   unsigned char temporal_pos_gain_max;
   unsigned char temporal_neg_thd_0;
   unsigned char temporal_neg_thd_1;
   unsigned char temporal_neg_gain_min;
   unsigned char temporal_neg_gain_max;
   unsigned char temporal_maxdiff;
   unsigned char temporal_sencechange_gain;
   /* remap curver */
   unsigned char spatial_remap_en;
   unsigned short spatial_remap_tab[65];
   /* data comp */
   unsigned int comp_2d_tbl[17][17];
   /* Global Control */
   unsigned char blk_Hnum;
   unsigned char blk_Vnum;
   unsigned short ld_blk_Hsize;
   unsigned short ld_blk_Vsize;
};



struct v4l2_ext_dynamnic_contrast_ctrl_rtk {
    unsigned short uDcVal;
    union {
        unsigned char *pst_chip_data;
        unsigned int compat_data;
        unsigned long long sizer;
    };
};


struct v4l2_ext_vpq_localcontrast_data{
        unsigned char ui_value;
        union {
                unsigned char *pst_chip_data;
                unsigned int compat_data;
                unsigned long long sizer;
        };
 };


struct v4l2_vpq_dynamic_contrast {
	CHIP_DCC_CHROMA_GAIN_T	stChromaGain;
	CHIP_DCC_SKINTONE_T	stSkinTone;

 };


typedef struct
{
	signed int FreshContrastHalLUT[33];
} FreshContrastHalLUT_T;

struct v4l2_ext_RTK_real_cinema_data{
        unsigned char ui_value;
        union {
                unsigned char *pst_chip_data;
                unsigned int compat_data;
                unsigned long long sizer;
        };
 };

struct v4l2_colortemp_info {
   unsigned char rgb_gain[V4L2_EXT_VPQ_RGB_MAX];
   unsigned char rgb_offset[V4L2_EXT_VPQ_RGB_MAX];
};


#define CSC_MUX_LUT_SIZE 4
struct v4l2_vpq_csc_mux_lut{
    unsigned int mux_l3d_in;
    unsigned int mux_blend_in;
    unsigned int mux_4p_lut_in;
    unsigned int mux_oetf_out;
    unsigned int b4p_lut_x[CSC_MUX_LUT_SIZE ];
    unsigned int b4p_lut_y[CSC_MUX_LUT_SIZE ];
};

struct v4l2_vpq_gamut_post{
    unsigned char gamma;
    unsigned char degamma;
    short matrix[9];
    struct v4l2_vpq_csc_mux_lut mux_blend;
};




struct v4l2_vpq_dynamic_color_gain {
	unsigned int 	pSaturationX[16];
	unsigned int 	pSaturationY[16];
};

typedef struct {
	bool bOnOff;
	char uPictureMode;
} HAL_DOLBY_PICTURE_MODE_T;

/** * dolby picture menu */
typedef enum { HAL_VPQ_DOLBY_BACKLIGHT = 0, ///< backlight
	HAL_VPQ_DOLBY_BRIGHTNESS, ///brightness
	HAL_VPQ_DOLBY_COLOR, ///< color
	HAL_VPQ_DOLBY_CONTRAST, ///< contrast
	HAL_VPQ_DOLBY_PICTURE_MENU_MAX ///< max num
} HAL_VPQ_DOLBY_PICTURE_MENU_T;

/** * dolby picture menu data */
typedef struct {
	HAL_VPQ_DOLBY_PICTURE_MENU_T pictureMenu;
	bool bOnOff;
	char uValue;
} HAL_VPQ_DOLBY_PICTURE_DATA_T;

/*
 * dolby global dimming delay data
 */
struct v4l2_dolby_gd_delay
{
	unsigned short ott_24;
	unsigned short ott_30;
	unsigned short ott_50;
	unsigned short ott_60;
	unsigned short hdmi_24;
	unsigned short hdmi_30;
	unsigned short hdmi_50;
	unsigned short hdmi_60;
};

struct v4l2_dolby_gd_delay_lut
{
	struct v4l2_dolby_gd_delay standard_frc_off;
	struct v4l2_dolby_gd_delay standard_frc_on;
	struct v4l2_dolby_gd_delay vivid_frc_off;
	struct v4l2_dolby_gd_delay vivid_frc_on;
	struct v4l2_dolby_gd_delay cinema_home_frc_off;
	struct v4l2_dolby_gd_delay cinema_home_frc_on;
	struct v4l2_dolby_gd_delay cinema_frc_off;
	struct v4l2_dolby_gd_delay cinema_frc_on;
	struct v4l2_dolby_gd_delay game_frc_off;
	struct v4l2_dolby_gd_delay game_frc_on;
};

struct v4l2_dolby_gd_delay_param
{
	struct v4l2_dolby_gd_delay_lut dolby_GD_standard;
	struct v4l2_dolby_gd_delay_lut dolby_GD_latency;
};

/*
 * dolby ambient light param
 */
struct v4l2_dolby_ambient_light_param
{
	unsigned int onoff;
	unsigned int luxdata;
	union {
		unsigned int *rawdata; ///< p_data
		unsigned int compat_data;
		unsigned long long sizer;
	};
};

// for VPQ bypass
typedef enum _v4l2_vpq_bypass_mask_RTK {
	V4L2_VPQ_BYPASS_MASK_NONE_RTK = 0x00000000,
	V4L2_VPQ_BYPASS_MASK_SHARP_ENHANCE_RTK = 0x00000001, // BIT0:Sharpness Enhancer
	V4L2_VPQ_BYPASS_MASK_OBJECT_CONTRAST_RTK = 0x00000002, // BIT1: Object Contrast
	V4L2_VPQ_BYPASS_MASK_CONTRAST_COLOR_ENHANCE_RTK = 0x00000004, // BIT2: Contrast,	Local Contrast, Color Enhancer
	V4L2_VPQ_BYPASS_MASK_GAMMA_LOCALDIMMING_RTK = 0x00000008, // BIT3: Local	dimming, Gamma UI
	V4L2_VPQ_BYPASS_MASK_NEAR_BE_RTK = 0x00000010, // BIT4: WB,	DGA-4CH,POD,PCID,ODC
	V4L2_VPQ_BYPASS_MASK_HDR_ALL_RTK = 0x00000020, // BIT5: HDR
	V4L2_VPQ_BYPASS_MASK_HDR_EXCEPT_PCC_RTK = 0x00000040, // BIT6: HDR	bypass, HDR-PCC enalbe
} v4l2_vpq_bypass_mask_RTK;

#endif
