/****************************************************************************
* This product contains one or more programs protected under international
* and U.S. copyright laws as unpublished works.  They are confidential and
* proprietary to Dolby Laboratories.  Their reproduction or disclosure, in
* whole or in part, or the production of derivative works therefrom without
* the express permission of Dolby Laboratories is prohibited.
*
*             Copyright 2011 - 2013 by Dolby Laboratories.
*                         All rights reserved.
****************************************************************************/

/*! Copyright &copy; 2013 Dolby Laboratories, Inc.
    All Rights Reserved

    @file  edbec_api.cpp
    @brief EDR decoder backend control software module implementation.

    This file implement the EDR decoder backend control software module.
*/
#ifndef WIN32
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
#include <mach/timex.h>

#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/random.h>
#ifdef CONFIG_CMA_RTK_ALLOCATOR
#include <linux/pageremap.h>
#endif
#include <linux/kthread.h>  /* for threads */
#include <linux/time.h>   /* for using jiffies */
#include <linux/hrtimer.h>


#include <linux/proc_fs.h>
#include <rtk_kdriver/RPCDriver.h>   /* register_kernel_rpc, send_rpc_command */
#ifdef CONFIG_KDRIVER_USE_NEW_COMMON
	#include <VideoRPC_System.h>
#else
	#include <rtk_kdriver/rpc/VideoRPC_System.h>
#endif
#include <rbus/vodma_reg.h>
#include <rbus/vodma2_reg.h>
#include <rbus/vgip_reg.h>
#include <rbus/timer_reg.h>
#include <rbus/dmato3dtable_reg.h>

#include "nwfpe/fpa11.h"	// for float
#include "nwfpe/fpopcode.h"	// for float

#ifdef CONFIG_KDRIVER_USE_NEW_COMMON
	#include <scaler/scalerDrvCommon.h>
#else
	#include <scalercommon/scalerDrvCommon.h>
#endif
#include <tvscalercontrol/scalerdrv/scalerdrv.h>
#include <tvscalercontrol/scaler/scalerstruct.h>
#include <tvscalercontrol/scalerdrv/scalermemory.h>
#include <rbus/dhdr_v_composer_reg.h>
#include <rbus/dm_reg.h>
#include <rbus/hdr_all_top_reg.h>
#include <target_display_config.h>
#include <tvscalercontrol/vip/scalerColor.h>

#endif


#include "dovi_bec_api.h"
#include "dovi_bec_osal_api.h"
#include "dovi_bec_dcd_queue_api.h"
#include "rpu_ext_config.h"
#ifdef RTK_EDBEC_1_5
#include "control_path_priv.h"
#else
#include "DmTypeDef.h"
#include "VdrDmAPI.h"
#include "VdrDmDataIo.h"
#include "VdrDmCli.h"
#endif

// default 3D LUT table
// 3d lut table
#include "GMLUT_100_R709_V65_YUV.bcms.h"
#include "GMLUT_100_0.1_R709_V66_PQ_RGB.bcms.h"	// gd_lut_a
#include "GMLUT_400_0.4_R709_V66_PQ_RGB.bcms.h"	// gd_lut_b

#define GMLUT_3DLUT GMLUT_100_R709_V65_YUV_bcms

// pq2lut table
#include "PQ2GammaLUT.bin.h"
#define PQ2GAMMA_LUT PQ2GammaLUT_bin

#include "dolbyvisionEDR_export.h"
#include "dolbyvisionEDR.h"

//>>>>> start ,20180131 will, hdmi vsif flow
#include <tvscalercontrol/hdmirx/hdmi_vfe.h>
extern void HDMI_TEST_VSIF(unsigned int wid, unsigned int len, unsigned char* mdAddr);
extern unsigned char get_cur_hdmi_dolby_apply_state(void);
//<<<<< start ,20180131 will, hdmi vsif flow

extern unsigned int get_query_start_address(unsigned char idx);

#if (defined(DOLBYPQ_DEBUG) && !defined(WIN32))
static ktime_t kt_periode;
#endif

//if source fps more than this value , set B05 in porch region, enable I2R.
unsigned int g_i2r_fps_thr = 180;

/* for HDMI interrupt control */
unsigned int g_hdmi_lastIdxRd = 0;
unsigned char hdmi_dolby_last_pair_num = 0;//When hdmi_dolby_apply_pair_num == hdmi_dolby_cur_pair_num. We know position and finish is pair
unsigned char hdmi_dolby_cur_pair_num = 0;//When hdmi position call hdmi test let hdmi_dolby_apply_pair_num = hdmi_dolby_cur_pair_num
extern volatile unsigned char hdmi_ui_change_flag;
extern volatile unsigned char hdmi_ui_change_delay_count;
extern spinlock_t* dolby_hdmi_ui_change_spinlock(void);
#ifndef WIN32
static struct class *dolbyvisionEDR_class;
DOLBYVISIONEDR_DEV *dolbyvisionEDR_devices;          /* allocated in dolbyvisionEDR_init_module */

//>>>>> start ,20180131 pinyen, hdmi vsif flow
vfe_hdmi_vsi_t hdmi_dolby_vsi_content;
//<<<<< end ,20180131 pinyen, hdmi vsif flow


#define DRV_NAME        "dolbyvisionEDR"
static const char  drv_name[] = DRV_NAME;

// for DOLBY picture mode
char *g_PicModeFilePath[3] = {NULL, NULL, NULL};

unsigned int  *b05_aligned_vir_addr = NULL;

#ifdef CONFIG_PM
static int dolbyvisionEDR_suspend(struct device *p_dev);
static int dolbyvisionEDR_resume(struct device *p_dev);
#endif

int dolbyvisionEDR_major = 0;
int dolbyvisionEDR_minor = 0;
int dolbyvisionEDR_nr_devs = 1;
#ifdef CONFIG_HIBERNATION
extern int in_suspend;
#endif
//RTK add fix compile error
UINT8 uPicMode;

#ifdef CONFIG_PM
static struct platform_device *dolbyvisionEDR_devs;

static const struct dev_pm_ops dolbyvisionEDR_pm_ops = {
	.suspend    = dolbyvisionEDR_suspend,
	.resume     = dolbyvisionEDR_resume,
#ifdef CONFIG_HIBERNATION
	.freeze     = dolbyvisionEDR_suspend,
	.thaw       = dolbyvisionEDR_resume,
	.poweroff   = dolbyvisionEDR_suspend,
	.restore    = dolbyvisionEDR_resume,
#endif
};

static struct platform_driver dolbyvisionEDR_driver = {
	.driver = {
		.name         = (char *)drv_name,
		.bus          = &platform_bus_type,
#ifdef CONFIG_PM
		.pm           = &dolbyvisionEDR_pm_ops,
#endif
	},
};
#endif /* CONFIG_PM */

#if defined(CONFIG_REALTEK_PCBMGR)
extern platform_info_t platform_info;
#endif

struct file_operations dolbyvisionEDR_fops
	= {
	.owner    =    THIS_MODULE,
	.llseek   =    dolbyvisionEDR_llseek,
	.read     =    dolbyvisionEDR_read,
	.write    =    dolbyvisionEDR_write,
	.unlocked_ioctl    =    (void *)dolbyvisionEDR_ioctl,
	.open     =    dolbyvisionEDR_open,
	.release  =    dolbyvisionEDR_release,
};

struct proc_dir_entry *rtkdv_proc_dir=0;
struct proc_dir_entry *rtkdv_proc_entry=0;

#define RTKDV_PROC_DIR "rtkdv"
#define RTKDV_PROC_ENTRY "dbg"
#define RTKDV_MAX_CMD_LENGTH (256)

void DV_init_debug_proc(void);
void DV_uninit_debug_proc(void);
static inline bool rtkdv_dbg_parse_value(char *cmd_pointer, long long *parsed_data);
static inline void rtkdv_dbg_EXECUTE(const int cmd_index, char **cmd_pointer);
int tty_num=0;
int g_log_level=0;
long g_dump_flag=0;
long dump_frame_cnt=0;
extern struct file *tty_fd ;
enum {
	DV_DBG_CMD_SET_CTF = 0,
	DV_DBG_CMD_SET_LOG_LEVEL = 1,
	DV_DBG_CMD_SET_TELNET_FD = 2,
	DV_DBG_CMD_SET_REG_A = 3,
	DV_DBG_CMD_SET_REG_D = 4,
	DV_DBG_CMD_RED_REG_A = 5,
	DV_DBG_CMD_CHK_STATUS = 6,
	DV_DBG_CMD_DUMP_MD = 7,
	DV_DBG_CMD_DUMP_DITHER_OUTPUT = 8,
	DV_DBG_CMD_TRIGGLE_NEXT_FRAME = 9,
	DV_DBG_CMD_I2R_FPS_THRESHOLD = 10,
	DV_DBG_CMD_SET_M_CAP_SINGLE_BUFFER = 11,
	DV_DBG_CMD_MAX,
} DV_DBG_CMD_INDEX_T;

static const char *rtkdv_cmd_str[] = {
	"cft=",			/* DV_DBG_CMD_SET_CTF */
	"level=",		/* DV_DBG_CMD_SET_LOG_LEVEL */
	"telnetfd=",	/* DV_DBG_CMD_SET_TELNET_FD */
	"rtdout_a=",	/* DV_DBG_CMD_SET_REG_A */
	"rtdout_d=",	/* DV_DBG_CMD_SET_REG_D */
	"rtdin_a=",		/* DV_DBG_CMD_RED_REG_A */
	"chk=",		    /* DV_DBG_CMD_CHK_STATUS */
	"dump_md=",		/* DV_DBG_CMD_DUMP_MD */
	"dump_output=",	/* DV_DBG_CMD_DUMP_DITHER_OUTPUT */
	"dump_frame_cnt=",	/* DV_DBG_CMD_TRIGGLE_NEXT_FRAME */
	"i2r_fps_thr=",	/* DV_DBG_CMD_I2R_FPS_THRESHOLD */
	"m_cap_single=",	/* DV_DBG_CMD_SET_M_CAP_SINGLE_BUFFER */
};
enum {
	DV_DBG_CMD_CTF_NONE=0,
	DV_DBG_CMD_CTF_5000 = 5000, //OTT
	DV_DBG_CMD_CTF_5001 = 5001, //OTT
	DV_DBG_CMD_CTF_5002 = 5002, //OTT
	DV_DBG_CMD_CTF_5003 = 5003, //OTT
	DV_DBG_CMD_CTF_5004 = 5004, //OTT
	DV_DBG_CMD_CTF_5005 = 5005, //OTT
	DV_DBG_CMD_CTF_5006 = 5006, //OTT
	DV_DBG_CMD_CTF_5007 = 5007, //OTT
	DV_DBG_CMD_CTF_5008 = 5008, //OTT
	DV_DBG_CMD_CTF_5009 = 5009, //OTT
	DV_DBG_CMD_CTF_5010 = 5010, //HDMI
	DV_DBG_CMD_CTF_5011 = 5011, //HDMI

	DV_DBG_CMD_CTF_5021 = 5021,
	DV_DBG_CMD_CTF_5022 = 5022,
	DV_DBG_CMD_CTF_5023 = 5023,
	DV_DBG_CMD_CTF_MAX,
} DV_DBG_CMD_CTF_NUMBER_T;

#endif



#define PTS_CLOCK_RATE                  90000
#define FRAME_RATE_DEFAULT              24
#define FRAME_PERIOD_DEFAULT            PTS_CLOCK_RATE/FRAME_RATE_DEFAULT
#define FOREVER                         for (;;)
#define ALIGN_POLLING_PERIOD            1

#define DEFAULT_USER_MS_WEIGHT          4095
#define USER_MS_WEIGHT_MIN              0
#define USER_MS_WEIGHT_MAX              4095

#define DEFAULT_IMAGE_WIDTH             1920
#define DEFAULT_IMAGE_HEIGHT            1080


static bool_t               f_init;
static bool_t               f_stop;
static bool_t               f_first_sample;
static bool_t               f_dovi_hw_proc_pending;
static bool_t               f_hdmi_src;
//static NameStr_t            str_lut_dir;            /* directory for LUT file */
//static NameStr_t            str_bw_normal_dm_3d_lut_file;
                                                      /* backward 3D LUT file */
cp_context_t *OTT_h_dm = NULL;
cp_context_t *hdmi_h_dm = NULL;

cp_context_t_half_st *OTT_h_dm_st = NULL;
cp_context_t_half_st *hdmi_h_dm_st = NULL;

#ifndef RTK_EDBEC_1_5
static uint16_t             user_ms_weight;
/* user specifies multi-scale blender weight */
static uint16_t             image_width  = DEFAULT_IMAGE_WIDTH;
static uint16_t             image_height = DEFAULT_IMAGE_HEIGHT;
static HDmCfgFxp_t          h_dm_cfg;           /* handle of DM configuration */
static HDmKsFxp_t           h_dm_ks;                  /* handle for DM kernel */
static Mmg_t                mmg;
#endif
MdsExt_t            mds_ext;

//static DmCfgFxp_t           dm_cfg;       /* display management configuration */
//DmCfgFxp_t           dm_cfg;       /* display management configuration */		// modified by smfan
DmCfgFxp_t           dm_bright_cfg; //Bright pic mode      /* display management configuration */		// modified by smfan
DmCfgFxp_t           dm_dark_cfg; //dark pic mode      /* display management configuration */		// modified by smfan
DmCfgFxp_t           dm_vivid_cfg; //vivid pic mode      /* display management configuration */		// modified by smfan
DmCfgFxp_t *p_dm_cfg = &dm_dark_cfg; //Point to PIC mode cfg
DmCfgFxp_t			 dm_hdmi_dark_cfg;	   /* display management configuration */		// modified by smfan
DmCfgFxp_t			 dm_hdmi_bright_cfg;	   /* display management configuration */		// modified by smfan
DmCfgFxp_t			 dm_hdmi_vivid_cfg;	   /* display management configuration */		// modified by smfan
DmCfgFxp_t *p_dm_hdmi_cfg = &dm_hdmi_dark_cfg; //Point to PIC mode cfg

uint16_t user_ms_weight = DEFAULT_USER_MS_WEIGHT;


static uint64_t             frm_period_90KHz;   /* frame period in 90 KHz clk */
static uint64_t             last_compose_stc;      /* STC at start of compose */
//static pts_t                last_pts;           /* PTS of last composed frame */
//static os_sem_handle_t      h_run_sgnl;   /* signal to launch composer thread */
//static os_sem_handle_t      h_rdy_sgnl;         /* composer is hardware ready */
//static os_mutex_handle_t    h_cfg_lock;            /* mutex for configuration */
//static os_thread_handle_t   h_composer_thread;      /* composer thread handle */
//static MdsExt_t             mds_ext;
MdsExt_t			   mds_ext;		// modifiedy smfan


//TgtGDCfg_t g_TgtGDCfg;		// added by Hari's suggestion//DOlby Hari 0616


/* metadata extracted from metadata stream */


#ifdef CONFIG_RTK_KDRV_DV
void dolby_trigger_timer6(void)
{//trigger timer 6 to apply dolby setting
	//  enable timer6 interrupt
	rtd_outl(TIMER_TCICR_reg, TIMER_TCICR_tc6ie_mask|1);
	// enable timer6
	rtd_outl(TIMER_TC6CR_reg, TIMER_TC6CR_tc6en_mask);
}
#endif

/*-----------------------------------------------------------------------------+
 |  Extract DM parameters from DM metadata byte-stream
 +----------------------------------------------------------------------------*/
//static void dm_metadata_2_dm_param
//static int dm_metadata_2_dm_param		// modified by smfan
/*
*  @return byte size of read
*/
//move to dovi_control_layer_api.c
//RTK MARK
#ifndef RTK_EDBEC_CONTROL
int dm_metadata_2_dm_param		// modified by smfan
    (
    dm_metadata_base_t  *p_dm_md,
    MdsExt_t            *p_mds_ext
    )
{
    uint32_t   ext_blk_len;
    uint8_t    ext_blk_lvl;
    uint8_t    i_blk;
    uint8_t   *p_ext_blk;
    TrimSet_t *p_trim_set;
    Trim_t    *p_trim;
	int i = 0, rbytes= 0;

    p_mds_ext->affected_dm_metadata_id = p_dm_md->dm_metadata_id;
    p_mds_ext->scene_refresh_flag = p_dm_md->scene_refresh_flag;
	rbytes += 2;

    p_mds_ext->m33Yuv2RgbScale2P = 13;
    p_mds_ext->m33Yuv2Rgb[0][0] = (p_dm_md->YCCtoRGB_coef0_hi << 8) |
                                 p_dm_md->YCCtoRGB_coef0_lo;
    p_mds_ext->m33Yuv2Rgb[0][1] = (p_dm_md->YCCtoRGB_coef1_hi << 8) |
                                 p_dm_md->YCCtoRGB_coef1_lo;
    p_mds_ext->m33Yuv2Rgb[0][2] = (p_dm_md->YCCtoRGB_coef2_hi << 8) |
                                 p_dm_md->YCCtoRGB_coef2_lo;
    p_mds_ext->m33Yuv2Rgb[1][0] = (p_dm_md->YCCtoRGB_coef3_hi << 8) |
                                 p_dm_md->YCCtoRGB_coef3_lo;
    p_mds_ext->m33Yuv2Rgb[1][1] = (p_dm_md->YCCtoRGB_coef4_hi << 8) |
                                 p_dm_md->YCCtoRGB_coef4_lo;
    p_mds_ext->m33Yuv2Rgb[1][2] = (p_dm_md->YCCtoRGB_coef5_hi << 8) |
                                 p_dm_md->YCCtoRGB_coef5_lo;
    p_mds_ext->m33Yuv2Rgb[2][0] = (p_dm_md->YCCtoRGB_coef6_hi << 8) |
                                 p_dm_md->YCCtoRGB_coef6_lo;
    p_mds_ext->m33Yuv2Rgb[2][1] = (p_dm_md->YCCtoRGB_coef7_hi << 8) |
                                 p_dm_md->YCCtoRGB_coef7_lo;
    p_mds_ext->m33Yuv2Rgb[2][2] = (p_dm_md->YCCtoRGB_coef8_hi << 8) |
                                 p_dm_md->YCCtoRGB_coef8_lo;
	rbytes += 18;

    p_mds_ext->v3Yuv2Rgb[0] = (p_dm_md->YCCtoRGB_offset0_byte3 << 24) |
                                  (p_dm_md->YCCtoRGB_offset0_byte2 << 16) |
                                  (p_dm_md->YCCtoRGB_offset0_byte1 <<  8) |
                                   p_dm_md->YCCtoRGB_offset0_byte0;
    p_mds_ext->v3Yuv2Rgb[1] = (p_dm_md->YCCtoRGB_offset1_byte3 << 24) |
                                  (p_dm_md->YCCtoRGB_offset1_byte2 << 16) |
                                  (p_dm_md->YCCtoRGB_offset1_byte1 <<  8) |
                                   p_dm_md->YCCtoRGB_offset1_byte0;
    p_mds_ext->v3Yuv2Rgb[2] = (p_dm_md->YCCtoRGB_offset2_byte3 << 24) |
                                  (p_dm_md->YCCtoRGB_offset2_byte2 << 16) |
                                  (p_dm_md->YCCtoRGB_offset2_byte1 <<  8) |
                                   p_dm_md->YCCtoRGB_offset2_byte0;
	rbytes += 12;

    p_mds_ext->m33Rgb2WpLmsScale2P = 14;
    p_mds_ext->m33Rgb2WpLms[0][0] = (p_dm_md->RGBtoLMS_coef0_hi << 8) |
                                 p_dm_md->RGBtoLMS_coef0_lo;
    p_mds_ext->m33Rgb2WpLms[0][1] = (p_dm_md->RGBtoLMS_coef1_hi << 8) |
                                 p_dm_md->RGBtoLMS_coef1_lo;
    p_mds_ext->m33Rgb2WpLms[0][2] = (p_dm_md->RGBtoLMS_coef2_hi << 8) |
                                 p_dm_md->RGBtoLMS_coef2_lo;
    p_mds_ext->m33Rgb2WpLms[1][0] = (p_dm_md->RGBtoLMS_coef3_hi << 8) |
                                 p_dm_md->RGBtoLMS_coef3_lo;
    p_mds_ext->m33Rgb2WpLms[1][1] = (p_dm_md->RGBtoLMS_coef4_hi << 8) |
                                 p_dm_md->RGBtoLMS_coef4_lo;
    p_mds_ext->m33Rgb2WpLms[1][2] = (p_dm_md->RGBtoLMS_coef5_hi << 8) |
                                 p_dm_md->RGBtoLMS_coef5_lo;
    p_mds_ext->m33Rgb2WpLms[2][0] = (p_dm_md->RGBtoLMS_coef6_hi << 8) |
                                 p_dm_md->RGBtoLMS_coef6_lo;
    p_mds_ext->m33Rgb2WpLms[2][1] = (p_dm_md->RGBtoLMS_coef7_hi << 8) |
                                 p_dm_md->RGBtoLMS_coef7_lo;
    p_mds_ext->m33Rgb2WpLms[2][2] = (p_dm_md->RGBtoLMS_coef8_hi << 8) |
                                 p_dm_md->RGBtoLMS_coef8_lo;
	rbytes += 18;

    p_mds_ext->signal_eotf        = (p_dm_md->signal_eotf_hi << 8) |
                                     p_dm_md->signal_eotf_lo;
    p_mds_ext->signal_eotf_param0 = (p_dm_md->signal_eotf_param0_hi << 8) |
                                     p_dm_md->signal_eotf_param0_lo;
    p_mds_ext->signal_eotf_param1 = (p_dm_md->signal_eotf_param1_hi << 8) |
                                     p_dm_md->signal_eotf_param1_lo;
    p_mds_ext->signal_eotf_param2 = (p_dm_md->signal_eotf_param2_byte3 << 24) |
                                    (p_dm_md->signal_eotf_param2_byte2 << 16) |
                                    (p_dm_md->signal_eotf_param2_byte1 <<  8) |
                                     p_dm_md->signal_eotf_param2_byte0;
	rbytes += 10;

		p_mds_ext->signal_bit_depth       = p_dm_md->signal_bit_depth;
    p_mds_ext->signal_color_space     = p_dm_md->signal_color_space;
    p_mds_ext->signal_chroma_format   = p_dm_md->signal_chroma_format;
    p_mds_ext->signal_full_range_flag = p_dm_md->signal_full_range_flag;

    p_mds_ext->source_min_PQ   = (p_dm_md->source_min_PQ_hi << 8) |
                                  p_dm_md->source_min_PQ_lo;
    p_mds_ext->source_max_PQ   = (p_dm_md->source_max_PQ_hi << 8) |
                                  p_dm_md->source_max_PQ_lo;
    p_mds_ext->source_diagonal = (p_dm_md->source_diagonal_hi << 8) |
                                  p_dm_md->source_diagonal_lo;
	rbytes += 10;

    /* initialize level 1 default */
    p_mds_ext->min_PQ = p_mds_ext->source_min_PQ;
    p_mds_ext->max_PQ = p_mds_ext->source_max_PQ;
    p_mds_ext->mid_PQ = (p_mds_ext->source_min_PQ + p_mds_ext->max_PQ) >> 1;

    // level 2 and 3 handling: default no trim
    p_trim_set = &(p_mds_ext->trimSets);
    p_trim_set->TrimSetNum = 0;
    p_trim_set->TrimSets[0].TrimNum = 0;
    p_trim_set->TrimSets[0].Trima[0] = p_mds_ext->source_max_PQ;

	p_mds_ext->lvl4GdAvail = 0;
    p_mds_ext->lvl5AoiAvail = 0;
#if EN_AOI
    p_mds_ext->active_area_left_offset   = (unsigned short)(dm_cfg.dmCtrl.AoiCol0);
    p_mds_ext->active_area_right_offset  = (unsigned short)(dm_cfg.srcSigEnv.ColNum - dm_cfg.dmCtrl.AoiCol1Plus1);
    p_mds_ext->active_area_top_offset    = (unsigned short)(dm_cfg.dmCtrl.AoiRow0);
    p_mds_ext->active_area_bottom_offset = (unsigned short)(dm_cfg.srcSigEnv.RowNum - dm_cfg.dmCtrl.AoiRow1Plus1);
#endif

    /* parse the extenstion block */
    p_mds_ext->num_ext_blocks = p_dm_md->num_ext_blocks;
	rbytes += 1;

    p_ext_blk = &p_dm_md->num_ext_blocks + sizeof(p_dm_md->num_ext_blocks);

    for (i_blk = 0; i_blk < p_mds_ext->num_ext_blocks; ++i_blk)
    {
        ext_blk_len = (p_ext_blk[0] << 24) |
                      (p_ext_blk[1] << 16) |
                      (p_ext_blk[2] <<  8) |
                       p_ext_blk[3];
        p_ext_blk += sizeof(uint32_t);

        ext_blk_lvl = p_ext_blk[0];
        p_ext_blk += sizeof(uint8_t);
		rbytes += 5;

        if (ext_blk_lvl == 1)
        {
            p_mds_ext->min_PQ = (p_ext_blk[0] << 8) | p_ext_blk[1];
            p_ext_blk += sizeof(uint16_t);
            p_mds_ext->max_PQ = (p_ext_blk[0] << 8) | p_ext_blk[1];
            p_ext_blk += sizeof(uint16_t);
            p_mds_ext->mid_PQ = (p_ext_blk[0] << 8) | p_ext_blk[1];
            p_ext_blk += sizeof(uint16_t);
			rbytes += 6;

            ext_blk_len -= 3 * sizeof(uint16_t);
        }
        else if ((ext_blk_lvl == 2) && !dm_cfg.tmCtrl.l2off)
        {
            p_trim = &p_trim_set->TrimSets[ext_blk_lvl - 2]; /* lvl = 2 in [0] */
            if (p_trim->TrimNum < p_trim->TrimNumMax)
            {
                /* only get the first TrimNumMax */
                ++(p_trim->TrimNum);
                for (int i = 0; i < p_trim->TrimTypeDim; ++i)
                {
                    p_trim->Trima[(p_trim->TrimNum * p_trim->TrimTypeDim) + i] = (p_ext_blk[0] << 8) | p_ext_blk[1];
                    p_ext_blk += sizeof(uint16_t);
					rbytes += 2;
                }

                /* update multi-scale blender weight if not specified */
                if (p_trim->Trima[(p_trim->TrimNum * p_trim->TrimTypeDim) + TrimTypeMsWeight] == 0xFFFF)
                    p_trim->Trima[(p_trim->TrimNum * p_trim->TrimTypeDim) + TrimTypeMsWeight] = user_ms_weight;

                ext_blk_len -= p_trim->TrimTypeDim * sizeof(uint16_t);
            }
            ++p_trim_set->TrimSetNum;
        }
        else if (ext_blk_lvl == 4)
        {
			p_mds_ext->lvl4GdAvail = 1;
			p_mds_ext->filtered_mean_PQ = (p_ext_blk[0] << 8) | p_ext_blk[1];
			p_ext_blk += sizeof(uint16_t);
			rbytes += 2;
            p_mds_ext->filtered_power_PQ = (p_ext_blk[0] << 8) | p_ext_blk[1];;
			p_ext_blk += sizeof(uint16_t);
			rbytes += 2;
            ext_blk_len -= 2 * sizeof(uint16_t);
		}
		else if (ext_blk_lvl == 5) {
			p_mds_ext->lvl5AoiAvail = 1;
#if EN_AOI
			p_mds_ext->active_area_left_offset = (p_ext_blk[0] << 8) | p_ext_blk[1];
			p_ext_blk += sizeof(uint16_t);
			rbytes += 2;
			p_mds_ext->active_area_right_offset = (p_ext_blk[0] << 8) | p_ext_blk[1];
			p_ext_blk += sizeof(uint16_t);
			rbytes += 2;
			p_mds_ext->active_area_top_offset = (p_ext_blk[0] << 8) | p_ext_blk[1];
			p_ext_blk += sizeof(uint16_t);
			rbytes += 2;
			p_mds_ext->active_area_bottom_offset = (p_ext_blk[0] << 8) | p_ext_blk[1];
			p_ext_blk += sizeof(uint16_t);
			rbytes += 2;
#endif
			ext_blk_len -= 4 * sizeof(uint16_t);
		}

        /* skip remaining bytes or if level is unknown */
        p_ext_blk += ext_blk_len;
		rbytes += ext_blk_len;
    }
	return rbytes;
}
#endif



#if 0	// mask by smfan


/*-----------------------------------------------------------------------------+
 |  This function determines if the decoded sample is valid and should be used
 |  for composing. Since the decoder status definition for the decoded sample
 |  is platform dependent. This function must be implemented for every decoder
 |  platform.
 +----------------------------------------------------------------------------*/

static bool_t decoded_sample_valid(dcd_sample_handle_t h_sample)
{
    //struct _dcd_stat_t_  *p_sample_stat = get_dcd_sample_stat(h_sample);
    struct _dcd_stat_t_  *p_stat;
    bool_t                f_compose = TRUE;

    p_stat = get_dcd_sample_stat(h_sample);
    /* TODO: analyze the decoded frame status to see if the frame should be
             used for composing and update the f_compose flag;
    */

    return f_compose;
}



/*-----------------------------------------------------------------------------+
 |  Initialize the DM LUT
 +----------------------------------------------------------------------------*/

static int init_dm_lut
    (
    //char *psz_lut_dir,
    //char *psz_bw_3d_lut_file
    void
    )
{
	// added by smfan
	int ret = -1;

#if 0	// mask by smfan
    /* fixed-point implementation specific LUTs */
    if (LoadL2PQLut(psz_lut_dir))
    {
        return -1;
    }

    if (LoadDeGammarLut(psz_lut_dir))
    {
        return -1;
    }
#endif
#ifdef SUPPORT_EDR_STB_MODE
    if (dm_cfg.dmCtrl.RunMode == RUN_MODE_BYPASS_EDR)
    {
        dm_cfg.dmCtrl.LutApproach = LUT_APPROACH;
    }
    else
#endif
    {
    	/* read .bcms file in file system */
		//ret = GMLUT_3DLUT_Update("/usr/local/etc/default.bcms", 0);

#ifdef WIN32
		if (ret != 0) {	/* no .bcms file in file system; added by smfan */
#else
		if (ret != DV_SUCCESS) {	/* no .bcms file in file system; added by smfan */
#endif
			if (GMLUT_3DLUT &&
				CreateLoadDmLut(GMLUT_3DLUT, &dm_cfg.bwDmLut, false))
			//if (dobly_3d_tab &&
			//	CreateLoadDmLut(dobly_3d_tab, &dm_cfg.bwDmLut, false))
	        {
	            return -1;
	        }
		}
    }

    return 0;
}



#ifdef SUPPORT_EDR_STB_MODE
/*-----------------------------------------------------------------------------+
 |  Turn on Graphic DM
 +----------------------------------------------------------------------------*/

static void display_management_graphic_on(void)
{
    dm_cfg.grEnv.HaveGr = TRUE;
    os_mutex_lock(h_cfg_lock);
    CommitDmCfg(dm_cfg, h_dm);
    os_mutex_unlock(h_cfg_lock);
}



/*-----------------------------------------------------------------------------+
 |  Turn off Graphic DM
 +----------------------------------------------------------------------------*/

static void display_management_graphic_off(void)
{
    dm_cfg.grEnv.HaveGr = FALSE;
    os_mutex_lock(h_cfg_lock);
    CommitDmCfg(dm_cfg, h_dm);
    os_mutex_unlock(h_cfg_lock);
}



/*-----------------------------------------------------------------------------+
 |  Configure the DM for BYPASS mode
 +----------------------------------------------------------------------------*/

static void display_management_bypass_config(void)
{
    dm_cfg.grEnv.HaveGr = f_graphic_on ? TRUE : FALSE;
    dm_cfg.dmCtrl.RunMode = RUN_MODE_BYPASS_EDR;
    dm_cfg.bldCtrl.DisBld   = 1;
    dm_cfg.bldCtrl.MSMethod = MS_METHOD_OFF;
    os_mutex_lock(h_cfg_lock);
    /* create all the look-up tables need */
    init_dm_lut(str_lut_dir, NULL);
    /* convert control parameters into data plane parameters */
    CommitDmCfg(dm_cfg, h_dm);
    os_mutex_unlock(h_cfg_lock);
}
#endif

#endif

/*-----------------------------------------------------------------------------+
 |  Configure the DM for NORMAL mode
 +----------------------------------------------------------------------------*/
#ifndef RTK_EDBEC_1_5
static void display_management_normal_config(void)
{
	CliParams_t cliParams;	// cli input params
	cliParams.bw3dLutFile = (char *)GMLUT_3DLUT;
#if EN_GLOBAL_DIMMING
	cliParams.bw3dLutFileA = (char *)GMLUT_100_0_1_R709_V66_PQ_RGB_bcms;
	cliParams.bw3dLutFileB = (char *)GMLUT_400_0_4_R709_V66_PQ_RGB_bcms;
	cliParams.pq2gLutFile = (char *)PQ2GAMMA_LUT;
#endif

#ifdef VDR_FPGA_IMPLEMENTATION
    dm_bright_cfg.dmCtrl.LowCmplxMode = TRUE;
	dm_dark_cfg.dmCtrl.LowCmplxMode = TRUE;
	dm_vivid_cfg.dmCtrl.LowCmplxMode = TRUE;
	dm_hdmi_dark_cfg.dmCtrl.LowCmplxMode = TRUE;
	dm_hdmi_bright_cfg.dmCtrl.LowCmplxMode = TRUE;
	dm_hdmi_vivid_cfg.dmCtrl.LowCmplxMode = TRUE;
#else
    dm_bright_cfg.dmCtrl.LowCmplxMode = FALSE;
	dm_dark_cfg.dmCtrl.LowCmplxMode = FALSE;
	dm_vivid_cfg.dmCtrl.LowCmplxMode = FALSE;
	dm_hdmi_dark_cfg.dmCtrl.LowCmplxMode = FALSE;
	dm_hdmi_bright_cfg.dmCtrl.LowCmplxMode = FALSE;
	dm_hdmi_vivid_cfg.dmCtrl.LowCmplxMode = FALSE;
#endif
    dm_bright_cfg.dmCtrl.RunMode = RUN_MODE_NORMAL;
	dm_dark_cfg.dmCtrl.RunMode = RUN_MODE_NORMAL;
	dm_vivid_cfg.dmCtrl.RunMode = RUN_MODE_NORMAL;
	dm_hdmi_dark_cfg.dmCtrl.RunMode = RUN_MODE_FROMHDMI;
	dm_hdmi_bright_cfg.dmCtrl.RunMode = RUN_MODE_FROMHDMI;
	dm_hdmi_vivid_cfg.dmCtrl.RunMode = RUN_MODE_FROMHDMI;
    //os_mutex_lock(h_cfg_lock);
    /* create all the look-up tables need */



	// 3d lut file is
	// src//dolby/libs/dm/dlb_dm_process/test/LUT/GMLUT_100_R709_V65_YUV.bcms
    //init_dm_lut();	// mask by smfan

	// added by smfan
	//**** refer to VdrDm function in VdrDM.c
#if EN_GLOBAL_DIMMING
	//dm_cfg.gdCtrl.GdOn = 1;
	//dm_cfg.gdCtrl.GdCap = 1;
#endif
	////// create init all the tables needed
	if (InitDmLut(&cliParams, &dm_bright_cfg)) {
		printf("Init LUT dm_bright_cfg failed!\n");
		//exit(-1);
	}
	if (InitDmLut(&cliParams, &dm_dark_cfg)) {
		printf("Init LUT dm_dark_cfg failed!\n");
		//exit(-1);
	}
	if (InitDmLut(&cliParams, &dm_vivid_cfg)) {
		printf("Init LUT dm_vivid_cfg failed!\n");
		//exit(-1);
	}
	if (InitDmLut(&cliParams, &dm_hdmi_dark_cfg)) {
		printf("Init LUT dm_hdmi_dark_cfg failed!\n");
		//exit(-1);
	}
	if (InitDmLut(&cliParams, &dm_hdmi_bright_cfg)) {
		printf("Init LUT dm_hdmi_bright_cfg failed!\n");
		//exit(-1);
	}
	if (InitDmLut(&cliParams, &dm_hdmi_vivid_cfg)) {
		printf("Init LUT dm_hdmi_vivid_cfg failed!\n");
		//exit(-1);
	}

#if 0//Mark by will Need to check
	dm_cfg.frmPri.PxlDefOut.pxlClr = 0;
	dm_cfg.frmPri.PxlDefOut.pxlChrm = 1;
	dm_cfg.frmPri.PxlDefOut.pxlDtp = 0;
	dm_cfg.frmPri.PxlDefOut.pxlBdp = 12;
	dm_cfg.frmPri.PxlDefOut.pxlWeav = 2;
	dm_cfg.frmPri.PxlDefOut.pxlLoc = 0;
	dm_cfg.frmPri.PxlDefOut.pxlEotf = 0;
	dm_cfg.frmPri.PxlDefOut.pxlRng = 1;
	SanityCheckDmCfg(&dm_cfg);
	//**** refer to VdrDm function in VdrDM.c ****//


    /* convert control parameters into data plane parameters */
    CommitDmCfg(&dm_cfg, h_dm);
    //os_mutex_unlock(h_cfg_lock);
#endif
}
#endif


#ifndef WIN32
#ifdef CONFIG_PM
static int dolbyvisionEDR_suspend (struct device *p_dev)
{
	return 0;
}
static int dolbyvisionEDR_resume (struct device *p_dev)
{
	// disable timer6 interrupt
	rtd_outl(TIMER_TCICR_reg, TIMER_TCICR_tc6ie_mask);

	//Enable Timer Mode
	rtk_timer_set_mode(6, COUNTER);//need to set for merlin3 for dolby timer interrupt
	rtk_timer6_11_route_to_SCPU(6, _ENABLE); // enable timer6 interrupt route to SCPU
	return 0;
}
#endif

loff_t dolbyvisionEDR_llseek(struct file *filp, loff_t off, int whence)
{
	return -EINVAL;
}
ssize_t dolbyvisionEDR_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	return -EFAULT;
}
ssize_t dolbyvisionEDR_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	char str[RTKDV_MAX_CMD_LENGTH] = {0};
	int cmd_index = 0;
	char *cmd_pointer = NULL;

	if (buf == NULL) {
		return -EFAULT;
	}
	if (count > RTKDV_MAX_CMD_LENGTH - 1)
		count = RTKDV_MAX_CMD_LENGTH - 1;
	if (copy_from_user(str, buf, RTKDV_MAX_CMD_LENGTH - 1)) {
		printk("rtkgdma_debug:%d copy_from_user failed! (buf=%p, count=%u)\n",
			   __LINE__, buf, count);
		return -EFAULT;
	}

	if (count > 0)
		str[count-1] = '\0';
	printk("rtkdv_debug:%d >>%s\n", __LINE__, str);


	for (cmd_index = 0; cmd_index < DV_DBG_CMD_MAX; cmd_index++) {
		if (strncmp(str,
					rtkdv_cmd_str[cmd_index],
					strlen(rtkdv_cmd_str[cmd_index])) == 0)
			break;
	}


	if (cmd_index < DV_DBG_CMD_MAX) {
		cmd_pointer = &str[strlen(rtkdv_cmd_str[cmd_index])];
	}

	rtkdv_dbg_EXECUTE(cmd_index, &cmd_pointer);

	return count;
}
int dolbyvisionEDR_open(struct inode *inode, struct file *filp)
{
	return 0;
}
int dolbyvisionEDR_release(struct inode *inode, struct file *filp)
{
	return 0;
}
#ifdef DOLBYPQ_DEBUG
enum hrtimer_restart dolbypq_debugMessage(struct hrtimer *hrtimer)
{
	printk("dm_cfg.tgtSigEnv.MinPq = %d \n", p_dm_cfg->tgtSigEnv.MinPq);
	printk("dm_cfg.tgtSigEnv.MaxPq = %d \n", p_dm_cfg->tgtSigEnv.MaxPq);
	printk("dm_cfg.tgtSigEnv.Gamma = %d \n", p_dm_cfg->tgtSigEnv.Gamma);
	printk("dm_cfg.tgtSigEnv.Eotf = %d \n", p_dm_cfg->tgtSigEnv.Eotf);
	printk("dm_cfg.tgtSigEnv.BitDepth = %d \n", p_dm_cfg->tgtSigEnv.BitDepth);
	printk("dm_cfg.tgtSigEnv.RgbRng = %d \n", p_dm_cfg->tgtSigEnv.RgbRng);
	printk("dm_cfg.tgtSigEnv.DiagSize = %d \n", p_dm_cfg->tgtSigEnv.DiagSize);
	printk("dm_cfg.dmCtrl.MinPQBias = 0x%x \n", p_dm_cfg->dmCtrl.MinPQBias);
	printk("dm_cfg.dmCtrl.MidPQBiasLut[0] = %d \n", p_dm_cfg->dmCtrl.MidPQBiasLut[0]);
	//printk("g_TgtGDCfg.gdEnable = %d \n", g_TgtGDCfg.gdEnable);
	//printk("g_TgtGDCfg.gdWMin = %d \n", g_TgtGDCfg.gdWMin);

	hrtimer_forward_now(hrtimer, kt_periode);
	return HRTIMER_RESTART;
}
#endif
int dolbyvisionEDR_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret =  0;
	DOLBYVISION_PATTERN data;
	long ret_copytouser;

	printk(KERN_INFO"dolbyvisionEDR: dolbyvisionEDR_ioctl, %d\n", _IOC_NR(cmd));
	if (_IOC_TYPE(cmd) != DOLBYVISIONEDR_IOC_MAGIC || _IOC_NR(cmd) > DOLBYVISIONEDR_IOC_MAXNR)
		return -ENOTTY;

	switch (cmd) {

	case DOLBYVISIONEDR_IOC_RB_INIT:
		{
			DOLBYVISION_INIT_STRUCT data;
			if (copy_from_user((void *)&data, (const void __user *)arg, sizeof(DOLBYVISION_INIT_STRUCT))) {
				ret = -EFAULT;
				printk(KERN_DEBUG"dolbyvisionEDR: ioctl code = %d failed!!!!!!!!!!!!!!!\n", _IOC_NR(cmd));
			} else {
				printk(KERN_DEBUG"data.inband_rbh_addr = 0x%x \n", data.inband_rbh_addr);
				printk(KERN_DEBUG"data.md_rbh_addr = 0x%x \n", data.md_rbh_addr);
				printk(KERN_DEBUG"data.md_output_buf_addr = 0x%x \n", data.md_output_buf_addr);
				printk(KERN_DEBUG"data.md_output_buf_size = 0x%x \n", data.md_output_buf_size);
				ret = DV_RingBuffer_Init(&data);
			}
			break;
		}

	case DOLBYVISIONEDR_IOC_RUN:
		{
			ret = DV_Run();
			break;
		}
	case DOLBYVISIONEDR_IOC_STOP:
		{
			ret = DV_Stop();
			break;
		}
	case DOLBYVISIONEDR_IOC_PAUSE:
		{
			ret = DV_Pause();
			break;
		}
	case DOLBYVISIONEDR_IOC_RB_FLUSH:
		{
			ret = DV_Flush();
			break;
		}
	case DOLBYVISIONEDR_IOC_3DLUT_UPDATE:
		{
			// put GMLUT.bcms in /tmp/usbmounts/sda1
			char filepath[256];
			if (copy_from_user((void *)&filepath, (const void __user *)arg, sizeof(filepath))) {
				ret = -EFAULT;
				printk(KERN_DEBUG"dolbyvisionEDR: ioctl code = %d failed!!!!!!!!!!!!!!!\n", _IOC_NR(cmd));
			} else {

				printk("\n\n");
				printk("3D LUT filepath=%s \n", filepath);

				//ret = GMLUT_3DLUT_Update(filepath, 0, 0, g_dv_picmode); //Mark by will. Need to check
			}
			break;
		}
	case DOLBYVISIONEDR_HDMI_IN_TEST:
		{
			printk(KERN_DEBUG"dolbyvisionEDR: DOLBYVISIONEDR_HDMI_IN_TEST ioctl code!!!!!!!!!!!!!!!\n");
			if (copy_from_user((void *)&data, (const void __user *)arg, sizeof(DOLBYVISION_PATTERN))) {
				ret = -EFAULT;
				printk(KERN_DEBUG"dolbyvisionEDR: ioctl code = %d failed!!!!!!!!!!!!!!!\n", _IOC_NR(cmd));
			} else {

				printk("\n\n");
				printk("pxlBdp=%d, RowNumTtl=%d, ColNumTtl=%d \n", data.pxlBdp, data.RowNumTtl, data.ColNumTtl);
				printk("file1Inpath=%s \n", data.file1Inpath);
				printk("fileOutpath=%s \n", data.fileOutpath);
				printk("fileMdInpath=%s \n", data.fileMdInpath);
				printk("file1InSize=%ld \n", data.file1InSize);
				printk("fileMdInSize=%ld \n", data.fileMdInSize);
				printk("frameNo=%d \n", data.frameNo);
				printk("caseNum=%s \n", data.caseNum);

				HDMI_In_TestFlow(&data);
			}
			break;
		}
	case DOLBYVISIONEDR_NORMAL_IN_TEST:
		{
			printk(KERN_DEBUG"dolbyvisionEDR: DOLBYVISIONEDR_NORMAL_IN_TEST ioctl code!!!!!!!!!!!!!!!\n");
			if (copy_from_user((void *)&data, (const void __user *)arg, sizeof(DOLBYVISION_PATTERN))) {
				ret = -EFAULT;
				printk(KERN_DEBUG"dolbyvisionEDR: ioctl code = %d failed!!!!!!!!!!!!!!!\n", _IOC_NR(cmd));
			} else {

				printk("\n\n");
				printk("pxlBdp=%d, RowNumTtl=%d, ColNumTtl=%d \n", data.pxlBdp, data.RowNumTtl, data.ColNumTtl);
				printk("BL_EL_ratio=%d \n", data.BL_EL_ratio);
				printk("file1Inpath=%s \n", data.file1Inpath);
				printk("file2Inpath=%s \n", data.file2Inpath);
				printk("fileOutpath=%s \n", data.fileOutpath);
				printk("fileMdInpath=%s \n", data.fileMdInpath);
				printk("file1InSize=%ld \n", data.file1InSize);
				printk("file2InSize=%ld \n", data.file2InSize);
				printk("fileMdInSize=%ld \n", data.fileMdInSize);
				printk("frameNo=%d \n", data.frameNo);
				printk("caseNum=%s \n", data.caseNum);

				Normal_In_TestFlow(&data);
			}
			break;
		}
	case DOLBYVISIONEDR_DM_CRF_420_TEST:
		{
			printk(KERN_DEBUG"dolbyvisionEDR: DOLBYVISIONEDR_DM_CRF_420_TEST ioctl code!!!!!!!!!!!!!!!\n");
			if (copy_from_user((void *)&data, (const void __user *)arg, sizeof(DOLBYVISION_PATTERN))) {
				ret = -EFAULT;
				printk(KERN_DEBUG"dolbyvisionEDR: ioctl code = %d failed!!!!!!!!!!!!!!!\n", _IOC_NR(cmd));
			} else {

				printk("\n\n");
				printk("pxlBdp=%d, RowNumTtl=%d, ColNumTtl=%d \n", data.pxlBdp, data.RowNumTtl, data.ColNumTtl);
				printk("BL_EL_ratio=%d \n", data.BL_EL_ratio);
				printk("file1Inpath=%s \n", data.file1Inpath);
				printk("file2Inpath=%s \n", data.file2Inpath);
				printk("fileOutpath=%s \n", data.fileOutpath);
				printk("fileMdInpath=%s \n", data.fileMdInpath);
				printk("file1InSize=%ld \n", data.file1InSize);
				printk("file2InSize=%ld \n", data.file2InSize);
				printk("fileMdInSize=%ld \n", data.fileMdInSize);
				printk("frameNo=%d \n", data.frameNo);
				printk("caseNum=%s \n", data.caseNum);

				DM_CRF_420_TestFlow(&data);
			}
			break;
		}
	case DOLBYVISIONEDR_DM_CRF_422_TEST:
		{
			printk(KERN_DEBUG"dolbyvisionEDR: DOLBYVISIONEDR_DM_CRF_422_TEST ioctl code!!!!!!!!!!!!!!!\n");
			if (copy_from_user((void *)&data, (const void __user *)arg, sizeof(DOLBYVISION_PATTERN))) {
				ret = -EFAULT;
				printk(KERN_DEBUG"dolbyvisionEDR: ioctl code = %d failed!!!!!!!!!!!!!!!\n", _IOC_NR(cmd));
			} else {

				printk("\n\n");
				printk("pxlBdp=%d, RowNumTtl=%d, ColNumTtl=%d \n", data.pxlBdp, data.RowNumTtl, data.ColNumTtl);
				printk("BL_EL_ratio=%d \n", data.BL_EL_ratio);
				printk("file1Inpath=%s \n", data.file1Inpath);
				printk("file2Inpath=%s \n", data.file2Inpath);
				printk("fileOutpath=%s \n", data.fileOutpath);
				printk("fileMdInpath=%s \n", data.fileMdInpath);
				printk("file1InSize=%ld \n", data.file1InSize);
				printk("file2InSize=%ld \n", data.file2InSize);
				printk("fileMdInSize=%ld \n", data.fileMdInSize);
				printk("frameNo=%d \n", data.frameNo);
				printk("caseNum=%s \n", data.caseNum);

				DM_CRF_422_TestFlow(&data);
			}
			break;
		}
	case DOLBYVISIONEDR_COMPOSER_CRF_TEST:
		{
			printk(KERN_DEBUG"dolbyvisionEDR: DOLBYVISIONEDR_COMPOSER_CRF_TEST ioctl code!!!!!!!!!!!!!!!\n");
			if (copy_from_user((void *)&data, (const void __user *)arg, sizeof(DOLBYVISION_PATTERN))) {
				ret = -EFAULT;
				printk(KERN_DEBUG"dolbyvisionEDR: ioctl code = %d failed!!!!!!!!!!!!!!!\n", _IOC_NR(cmd));
			} else {

				printk("\n\n");
				printk("pxlBdp=%d, RowNumTtl=%d, ColNumTtl=%d \n", data.pxlBdp, data.RowNumTtl, data.ColNumTtl);
				printk("BL_EL_ratio=%d \n", data.BL_EL_ratio);
				printk("file1Inpath=%s \n", data.file1Inpath);
				printk("file2Inpath=%s \n", data.file2Inpath);
				printk("fileOutpath=%s \n", data.fileOutpath);
				printk("fileMdInpath=%s \n", data.fileMdInpath);
				printk("file1InSize=%ld \n", data.file1InSize);
				printk("file2InSize=%ld \n", data.file2InSize);
				printk("fileMdInSize=%ld \n", data.fileMdInSize);
				printk("frameNo=%d \n", data.frameNo);
				printk("caseNum=%s \n", data.caseNum);

				Composer_CRF_TestFlow(&data);
			}
			break;
		}
	case DOLBYVISIONEDR_CRF_DUMP_TEST:
		{
			printk(KERN_DEBUG"dolbyvisionEDR: DOLBYVISIONEDR_CRF_DUMP_TEST ioctl code!!!!!!!!!!!!!!!\n");
			if (copy_from_user((void *)&data, (const void __user *)arg, sizeof(DOLBYVISION_PATTERN))) {
				ret = -EFAULT;
				printk(KERN_DEBUG"dolbyvisionEDR: ioctl code = %d failed!!!!!!!!!!!!!!!\n", _IOC_NR(cmd));
			} else {

				printk("\n\n");
				printk(KERN_ERR"pxlBdp=%d, RowNumTtl=%d, ColNumTtl=%d \n", data.pxlBdp, data.RowNumTtl, data.ColNumTtl);
				printk(KERN_ERR"BL_EL_ratio=%d \n", data.BL_EL_ratio);
				printk(KERN_ERR"file1Inpath=%s \n", data.file1Inpath);
				printk(KERN_ERR"file2Inpath=%s \n", data.file2Inpath);
				printk(KERN_ERR"fileOutpath=%s \n", data.fileOutpath);
				printk(KERN_ERR"fileMdInpath=%s \n", data.fileMdInpath);
				printk(KERN_ERR"file1InSize=%ld \n", data.file1InSize);
				printk(KERN_ERR"file2InSize=%ld \n", data.file2InSize);
				printk(KERN_ERR"fileMdInSize=%ld \n", data.fileMdInSize);
				printk(KERN_ERR"frameNo=%d \n", data.frameNo);
				printk(KERN_ERR"caseNum=%s \n", data.caseNum);
                //fox mark for compile
                //dv_dump_agent_init(DUMP_2K);
				DM_CRF_Dump_TestFlow(&data);
			}
			break;
		}
	case DOLBYVISIONEDR_CRF_TRIGGER_TEST:
		{
			unsigned int data, regvalue;
			if (copy_from_user((void *)&data, (const void __user *)arg, sizeof(int))) {
				ret = -EFAULT;
				printk(KERN_DEBUG"dolbyvisionEDR: ioctl code = %d failed!!!!!!!!!!!!!!!\n", _IOC_NR(cmd));
			} else {

				printk(KERN_DEBUG"dolbyvisionEDR: DOLBYVISIONEDR_CRF_TRIGGER_TEST ioctl code! data=%d \n", data);

				if (data == 0) { // for read trigger condition
					regvalue = rtd_inl(0xb800501c);

					ret_copytouser = copy_to_user((void __user *)arg, &regvalue, sizeof(regvalue));

				} else if (data == 1)	// for trigger next frame
					rtd_outl(0xb800501c, 0x1);
			}
			break;
		}
	case DOLBYVISIONEDR_TEST_TEST:
		{
			printk(KERN_DEBUG"dolbyvisionEDR: DOLBYVISIONEDR_TEST_TEST ioctl code!!!!!!!!!!!!!!!\n");
			if (copy_from_user((void *)&data, (const void __user *)arg, sizeof(DOLBYVISION_PATTERN))) {
				ret = -EFAULT;
				printk(KERN_DEBUG"dolbyvisionEDR: ioctl code = %d failed!!!!!!!!!!!!!!!\n", _IOC_NR(cmd));
			} else {
#if 0
				printk("\n\n");
				printk("pxlBdp=%d, RowNumTtl=%d, ColNumTtl=%d \n", data.pxlBdp, data.RowNumTtl, data.ColNumTtl);
				printk("BL_EL_ratio=%d \n", data.BL_EL_ratio);
				printk("file1Inpath=%s \n", data.file1Inpath);
				printk("file2Inpath=%s \n", data.file2Inpath);
				printk("fileOutpath=%s \n", data.fileOutpath);
				printk("fileMdInpath=%s \n", data.fileMdInpath);
				printk("file1InSize=%ld \n", data.file1InSize);
				printk("file2InSize=%ld \n", data.file2InSize);
				printk("fileMdInSize=%ld \n", data.fileMdInSize);
				printk("frameNo=%d \n", data.frameNo);
				printk("caseNum=%s \n", data.caseNum);

				Load_RpuMdTest(&data);
#endif
#if 1			// for MD output debug
				DV_MD_DumpOutput_Info(1);
#endif
			}
			break;
		}
	case DOLBYVISIONEDR_DRV_VPQ_SETDOLBYPICMODE:
		{
			//UINT8 uPicMode;
			//char tmpStr[500] = "";
            return 0;
			if (copy_from_user((void *)&uPicMode, (const void __user *)arg, sizeof(UINT8))) {
				ret = -EFAULT;
				pr_err("dolbyvisionEDR: ioctl code = %d failed!!!!!!!!!!!!!!!\n", _IOC_NR(cmd));
				return ret;
			}

			if(ui_dv_picmode == uPicMode)
			{
				pr_notice("\r\ndolbyvisionEDR: the same picture mode\n");
				break;
			}
			//Change picture mode //
			down(&g_dv_pq_sem);
			ui_dv_picmode = uPicMode;
			if(check_need_to_reload_config())
			{//This api is for dolby debug.
				DolbyPQ_1Model_1File_Config(g_PicModeFilePath[ui_dv_picmode], ui_dv_picmode);
				if(ui_dv_picmode == DV_PIC_DARK_MODE)
					memcpy((void *)&dm_hdmi_dark_cfg, (void *)&dm_dark_cfg, sizeof(DmCfgFxp_t)); // copy to HDMI dm_hdmi_cfg
				else if(ui_dv_picmode == DV_PIC_BRIGHT_MODE)
					memcpy((void *)&dm_hdmi_bright_cfg, (void *)&dm_bright_cfg, sizeof(DmCfgFxp_t)); // copy to HDMI dm_hdmi_cfg
				else
					memcpy((void *)&dm_hdmi_vivid_cfg, (void *)&dm_vivid_cfg, sizeof(DmCfgFxp_t)); // copy to HDMI dm_hdmi_cfg
			}
#if 0
			// copy to HDMI dm_hdmi_cfg
			if(ui_dv_picmode == DV_PIC_BRIGHT_MODE)
				p_dm_hdmi_cfg = &dm_hdmi_bright_cfg;
			else if(ui_dv_picmode == DV_PIC_VIVID_MODE)
				p_dm_hdmi_cfg = &dm_hdmi_vivid_cfg;
			else
				p_dm_hdmi_cfg = &dm_hdmi_dark_cfg;
			CommitDmCfg(p_dm_hdmi_cfg, hdmi_h_dm);
			// set UI PQ tuning flag
			g_picModeUpdateFlag = 3;
#endif

			up(&g_dv_pq_sem);
			spin_lock_irq(dolby_hdmi_ui_change_spinlock());
			hdmi_ui_change_flag = TRUE;
			spin_unlock_irq(dolby_hdmi_ui_change_spinlock());
			pr_notice("\r\nchange picture mode %d\n", uPicMode);
#if 0//Mark by Will
			down(&g_dv_pq_sem);


#ifdef DOLBYPQ_DEBUG
#if 0
			// temporary testing
			static struct hrtimer htimer;
			if (strcmp(g_PicModeFilePath[0], "/tmp/usb/sda/sda1/dolbypq/dark/")!=0) {
				kfree(g_PicModeFilePath[0]);
				kfree(g_PicModeFilePath[1]);
				kfree(g_PicModeFilePath[2]);
				g_PicModeFilePath[0] = g_PicModeFilePath[1] = g_PicModeFilePath[2] = NULL;
			}

			if (g_PicModeFilePath[0] == NULL) {
				g_PicModeFilePath[0] = kmalloc(strlen("/tmp/usb/sda/sda1/dolbypq/dark/")+1, GFP_KERNEL);
				g_PicModeFilePath[1] = kmalloc(strlen("/tmp/usb/sda/sda1/dolbypq/bright/")+1, GFP_KERNEL);
				g_PicModeFilePath[2] = kmalloc(strlen("/tmp/usb/sda/sda1/dolbypq/vivid/")+1, GFP_KERNEL);
				strcpy(g_PicModeFilePath[0], "/tmp/usb/sda/sda1/dolbypq/dark/");
				strcpy(g_PicModeFilePath[1], "/tmp/usb/sda/sda1/dolbypq/bright/");
				strcpy(g_PicModeFilePath[2], "/tmp/usb/sda/sda1/dolbypq/vivid/");

				kt_periode = ktime_set(6, 0); //seconds,nanoseconds
			    hrtimer_init(&htimer, CLOCK_REALTIME, HRTIMER_MODE_REL);
			    htimer.function = dolbypq_debugMessage;
			    hrtimer_start(&htimer, kt_periode, HRTIMER_MODE_REL);
			}
#endif

#if 1
			// temporary testing
			static struct hrtimer htimer;
			if (g_PicModeFilePath[0]) {
				if (strcmp(g_PicModeFilePath[0], "/tmp/usb/sda/sda1/dolbypq/DolbyPQ_PanelType.bin")!=0
					&& strcmp(g_PicModeFilePath[0], "/tmp/usb/sdb/sdb1/dolbypq/DolbyPQ_PanelType.bin")!=0) {
					kfree(g_PicModeFilePath[0]);
					kfree(g_PicModeFilePath[1]);
					kfree(g_PicModeFilePath[2]);
					g_PicModeFilePath[0] = g_PicModeFilePath[1] = g_PicModeFilePath[2] = NULL;
				}
			}

			if (g_PicModeFilePath[0] == NULL) {
				g_PicModeFilePath[0] = kmalloc(strlen("/tmp/usb/sda/sda1/dolbypq/DolbyPQ_PanelType.bin")+1, GFP_KERNEL);
				g_PicModeFilePath[1] = kmalloc(strlen("/tmp/usb/sda/sda1/dolbypq/DolbyPQ_PanelType.bin")+1, GFP_KERNEL);
				g_PicModeFilePath[2] = kmalloc(strlen("/tmp/usb/sda/sda1/dolbypq/DolbyPQ_PanelType.bin")+1, GFP_KERNEL);
				strcpy(g_PicModeFilePath[0], "/tmp/usb/sda/sda1/dolbypq/DolbyPQ_PanelType.bin");
				strcpy(g_PicModeFilePath[1], "/tmp/usb/sda/sda1/dolbypq/DolbyPQ_PanelType.bin");
				strcpy(g_PicModeFilePath[2], "/tmp/usb/sda/sda1/dolbypq/DolbyPQ_PanelType.bin");

				kt_periode = ktime_set(6, 0); //seconds,nanoseconds
			    hrtimer_init(&htimer, CLOCK_REALTIME, HRTIMER_MODE_REL);
			    htimer.function = dolbypq_debugMessage;
			    hrtimer_start(&htimer, kt_periode, HRTIMER_MODE_REL);
			}
#endif
#endif

#if 0//Mark by will
				if((g_dv_picmode == DV_PIC_NONE_MODE) && (uPicMode <= DV_PIC_VIVID_MODE)) { //SDR->EDR
#if (defined(CONFIG_CUSTOMER_TV006) && defined(CONFIG_SUPPORT_SCALER))
					scaler_dolby_hdmi_smooth_toggle(1);
#endif
				} else if((g_dv_picmode != DV_PIC_NONE_MODE) && (uPicMode > DV_PIC_VIVID_MODE)) { //EDR->SDR
#if (defined(CONFIG_CUSTOMER_TV006) && defined(CONFIG_SUPPORT_SCALER))
					scaler_dolby_hdmi_smooth_toggle(0);
#endif
					g_dv_picmode = DV_PIC_NONE_MODE;
				}
#endif
				if (uPicMode == DV_PIC_DARK_MODE) {	// Dark Mode
					if (g_PicModeFilePath[0] != NULL) {
						fileIdx = 0;
						g_dv_picmode = DV_PIC_DARK_MODE;
						printk(KERN_DEBUG"%s: Dark Mode \n", __FUNCTION__);
					} else {
						printk(KERN_DEBUG"%s: Please call DOLBYVISIONEDR_DRV_VPQ_INITDOLBYPICCONFIG first \n", __FUNCTION__);
						up(&g_dv_pq_sem);
						break;
					}
				} else if (uPicMode == DV_PIC_BRIGHT_MODE) {	// Bright Mode
					if (g_PicModeFilePath[1] != NULL) {
						fileIdx = 1;
						g_dv_picmode = DV_PIC_BRIGHT_MODE;
						printk(KERN_DEBUG"%s: Bright Mode \n", __FUNCTION__);
					} else {
						printk(KERN_DEBUG"%s: Please call DOLBYVISIONEDR_DRV_VPQ_INITDOLBYPICCONFIG first \n", __FUNCTION__);
						up(&g_dv_pq_sem);
						break;
					}
				} else if (uPicMode == DV_PIC_VIVID_MODE) {	// Vivid Mode
					if (g_PicModeFilePath[2] != NULL) {
						fileIdx = 2;
						g_dv_picmode = DV_PIC_VIVID_MODE;
						printk(KERN_DEBUG"%s: Vivid Mode \n", __FUNCTION__);
					} else {
						printk(KERN_DEBUG"%s: Please call DOLBYVISIONEDR_DRV_VPQ_INITDOLBYPICCONFIG first \n", __FUNCTION__);
						up(&g_dv_pq_sem);
						break;
					}
				} else {
					g_dv_picmode = DV_PIC_NONE_MODE;
					up(&g_dv_pq_sem);
					break;
				}

				if (g_PicModeFilePath[fileIdx]) {

#if 1
					printk("%s, MD parsed count = %d \n", __FUNCTION__, DV_MD_DumpOutput_Info(0));
					ret = DolbyPQ_1Model_1File_Config(g_PicModeFilePath[fileIdx], g_dv_picmode);
					if (ret == 0)
						ret = -EFAULT;
#else
					strcpy(tmpStr, g_PicModeFilePath[fileIdx]);
					strcat(tmpStr, "//default.bcms");
					GMLUT_3DLUT_Update(tmpStr/*"/usr/local/etc/DolbyPQ/55inch/dark/default.bcms"*/, 0, 0);
					strcpy(tmpStr, g_PicModeFilePath[fileIdx]);
					strcat(tmpStr, "//gdlut_a.bcms");
					GMLUT_3DLUT_Update(tmpStr/*"/usr/local/etc/DolbyPQ/55inch/dark/gdlut_a.bcms"*/, 0, 1);
					strcpy(tmpStr, g_PicModeFilePath[fileIdx]);
					strcat(tmpStr, "//gdlut_b.bcms");
					GMLUT_3DLUT_Update(tmpStr/*"/usr/local/etc/DolbyPQ/55inch/dark/gdlut_b.bcms"*/, 0, 2);
					strcpy(tmpStr, g_PicModeFilePath[fileIdx]);
					strcat(tmpStr, "//PQ2GammaLUT.bin");
					GMLUT_Pq2GLut_Update(tmpStr/*"/usr/local/etc/DolbyPQ/55inch/dark/PQ2GammaLUT.bin"*/, 0, (4 + 1024*4 + 16*3) * sizeof(int32_t));
					strcpy(tmpStr, g_PicModeFilePath[fileIdx]);
					strcat(tmpStr, "//dv_pq_para.bin");
					Target_Display_Parameters_Update(tmpStr/*"/usr/local/etc/DolbyPQ/55inch/dark/dv_pq_para.bin"*/, 0);

					CommitDmCfg(&dm_cfg, h_dm);
#endif
					// set UI PQ tuning flag
					g_picModeUpdateFlag = 1;
				//g_picModeApplyCnt = 300;
			}

			up(&g_dv_pq_sem);
#endif
			break;
		}
	case DOLBYVISIONEDR_DRV_VPQ_SETDOLBYBACKLIGHT:
		{
			UINT8 uBacklight;
			//uint16_t uBacklight;
			//baker, set 1Model1File, for IDK
			DolbyPQ_1Model_1File_Config(0,0);//Load vivid mode config

			if (copy_from_user((void *)&uBacklight, (const void __user *)arg, sizeof(UINT8))) {
				ret = -EFAULT;
				printk(KERN_DEBUG"dolbyvisionEDR: ioctl code = %d failed!!!!!!!!!!!!!!!\n", _IOC_NR(cmd));
			} else {
				if(ui_dv_backlight_value == uBacklight)
				{
					pr_notice("\r\ndolbyvisionEDR: the same backlight value\n");
					break;
				}
				if (uBacklight>100) {
					printk(KERN_DEBUG"dolbyvisionEDR: uBacklight=%d is too big !!!!!!!!!!!!!!!\n", uBacklight);
					break;
				}
			down(&g_dv_pq_sem);
			ui_dv_backlight_value = uBacklight;
/*//Need to check
			calculate_backlight_setting_for_cfg(p_dm_hdmi_cfg, ui_dv_backlight_value, DOLBY_SRC_HDMI);
			// set UI PQ tuning flag
			g_picModeUpdateFlag = 3;
*/
			up(&g_dv_pq_sem);
			spin_lock_irq(dolby_hdmi_ui_change_spinlock());
			hdmi_ui_change_flag = TRUE;
			spin_unlock_irq(dolby_hdmi_ui_change_spinlock());
			pr_notice("\r\nchange back light value %d\n", uBacklight);
#if 0// Mark by will
#if 0
				int16_t Y, Y1, Y2, X, X1;

				// interpolate
				X1 = (uBacklight>>3)<<3;
				X = uBacklight;

				Y2 = dm_cfg.dmCtrl.MidPQBiasLut[(uBacklight>>3) + 1];
				Y1 = dm_cfg.dmCtrl.MidPQBiasLut[uBacklight>>3];

				if ((X - X1) != 0)
					//Y = Y1 + ((Y2 - Y1) / (8 * (X - X1)));
					Y = (Y2-Y1) * (X-X1) / 8;
				else
					Y = Y1;

				printk(KERN_DEBUG"%s: prior dm_cfg.dmCtrl.MidPQBias=%d \n", __FUNCTION__, dm_cfg.dmCtrl.MidPQBias);
				printk(KERN_DEBUG"%s: uBacklight=%d, new MidPQBias=%d \n", __FUNCTION__, uBacklight, Y);
#else

				int16_t x_x1,x1,y2,y1, Y;
				int32_t temp;


				#define LOG2_UI_STEPSIZE 3

				printk(KERN_DEBUG"%s: uBacklight=%d \n", __FUNCTION__, uBacklight);
				uBacklight+=2;
				uBacklight = CLAMPS(uBacklight,0,100);

				x1	 = uBacklight>>LOG2_UI_STEPSIZE;
				x_x1 = uBacklight - (x1<<LOG2_UI_STEPSIZE);

				y1 = p_dm_cfg->dmCtrl.MidPQBiasLut[x1];
				y2 = p_dm_cfg->dmCtrl.MidPQBiasLut[x1+1];
				y2 = y2-y1 ; //Y2-Y1
				//X2-X1 = 8;
				temp = (y2 * x_x1) >>LOG2_UI_STEPSIZE	;//(y2-y1)/(x2-x1) * (x-x1)
				Y = (int16_t)(temp + y1);
				printk(KERN_DEBUG"%s: new MidPQBias=%d \n", __FUNCTION__, Y);
#endif

				dm_dark_cfg.dmCtrl.MidPQBias = Y;
				dm_bright_cfg.dmCtrl.MidPQBias = Y;
				dm_vivid_cfg.dmCtrl.MidPQBias = Y;

				h_dm->tcCtrl.tMidPQBias = dm_dark_cfg.dmCtrl.MidPQBias;
				// set UI PQ tuning flag
				g_picModeUpdateFlag = 1;
				//g_picModeApplyCnt = 300;
#endif
			}
			break;
		}
	case DOLBYVISIONEDR_DRV_VPQ_SETDOLBYCONTRAST:
		{
			UINT8 uContrast;
			if (copy_from_user((void *)&uContrast, (const void __user *)arg, sizeof(UINT8))) {
				ret = -EFAULT;
				printk(KERN_DEBUG"dolbyvisionEDR: ioctl code = %d failed!!!!!!!!!!!!!!!\n", _IOC_NR(cmd));
			} else {
				// do nothing
				printk(KERN_DEBUG"%s: uContrast=%d \n", __FUNCTION__, uContrast);
			}
			break;
		}
	case DOLBYVISIONEDR_DRV_VPQ_SETDOLBYBRIGHTNESS:
		{
			UINT8 uBrightness;
			if (copy_from_user((void *)&uBrightness, (const void __user *)arg, sizeof(UINT8))) {
				ret = -EFAULT;
				printk(KERN_DEBUG"dolbyvisionEDR: ioctl code = %d failed!!!!!!!!!!!!!!!\n", _IOC_NR(cmd));
			} else {
				// do nothing
				printk(KERN_DEBUG"%s: uBrightness=%d \n", __FUNCTION__, uBrightness);
			}
			break;
		}
	case DOLBYVISIONEDR_DRV_VPQ_SETDOLBYCOLOR:
		{
			UINT8 uColor;
			if (copy_from_user((void *)&uColor, (const void __user *)arg, sizeof(UINT8))) {
				ret = -EFAULT;
				printk(KERN_DEBUG"dolbyvisionEDR: ioctl code = %d failed!!!!!!!!!!!!!!!\n", _IOC_NR(cmd));
			} else {
				// do nothing
				//printk(KERN_EMERG"%s: prior dm_cfg.dmCtrl.SaturationGainBias=%d \n", __FUNCTION__, dm_cfg.dmCtrl.SaturationGainBias);
				//printk(KERN_EMERG"%s: uColor=%d, new SaturationGainBias=%d \n", __FUNCTION__, uColor, dm_cfg.dmCtrl.SaturationGainBiasLut[0]);

				// don't set this according to DOLBY suggestion
				//dm_cfg.dmCtrl.SaturationGainBias = dm_cfg.dmCtrl.SaturationGainBiasLut[idx];
				//CommitDmCfg(&dm_cfg, h_dm);
				//h_dm->dmExec.saturationGain = (uint16_t)CLAMPS((int32_t)h_dm->dmExec.saturationGain + (int32_t)dm_cfg.dmCtrl.SaturationGainBias, 2048, (int32_t)DLB_UINT_MAX(12) + 2048);
			}
			break;
		}
	case DOLBYVISIONEDR_DRV_VPQ_INITDOLBYPICCONFIG:
		{
			char *configFilePath[5];
			if (copy_from_user((void *)&configFilePath, (const void __user *)arg, sizeof(configFilePath))) {
				ret = -EFAULT;
				printk(KERN_DEBUG"dolbyvisionEDR: ioctl code = %d failed!!!!!!!!!!!!!!!\n", _IOC_NR(cmd));
				printk(KERN_DEBUG"dolbyvisionEDR: configFilePath[0]=%s\n", configFilePath[0]);
				printk(KERN_DEBUG"dolbyvisionEDR: configFilePath[1]=%s\n", configFilePath[1]);
				printk(KERN_DEBUG"dolbyvisionEDR: configFilePath[2]=%s\n", configFilePath[2]);
			} else {

				g_PicModeFilePath[0] = kmalloc(strnlen_user(configFilePath[0], PATH_MAX)+1, GFP_KERNEL);
				g_PicModeFilePath[1] = kmalloc(strnlen_user(configFilePath[1], PATH_MAX)+1, GFP_KERNEL);
				g_PicModeFilePath[2] = kmalloc(strnlen_user(configFilePath[2], PATH_MAX)+1, GFP_KERNEL);
				strncpy_from_user(g_PicModeFilePath[0], configFilePath[0], strnlen_user(configFilePath[0], PATH_MAX));
				strncpy_from_user(g_PicModeFilePath[1], configFilePath[1], strnlen_user(configFilePath[1], PATH_MAX));
				strncpy_from_user(g_PicModeFilePath[2], configFilePath[2], strnlen_user(configFilePath[2], PATH_MAX));
				printk(KERN_DEBUG"%s: DolbyHDRDark path=%s \n", __FUNCTION__, g_PicModeFilePath[0]);
				printk(KERN_DEBUG"%s: DolbyHDRBright path=%s \n", __FUNCTION__, g_PicModeFilePath[1]);
				printk(KERN_DEBUG"%s: DolbyHDRVivid path=%s \n", __FUNCTION__, g_PicModeFilePath[2]);

				down(&g_dv_pq_sem);
#ifdef DOLBYPQ_DEBUG
				ret = DolbyPQ_1Model_1File_Config("/tmp/usb/sda/sda1/dolbypq/DolbyPQ_PanelType.bin", DV_PIC_DARK_MODE);
				if (ret == 0)
					ret = -EFAULT;
				ret = DolbyPQ_1Model_1File_Config("/tmp/usb/sda/sda1/dolbypq/DolbyPQ_PanelType.bin", DV_PIC_BRIGHT_MODE);
				if (ret == 0)
					ret = -EFAULT;
				ret = DolbyPQ_1Model_1File_Config("/tmp/usb/sda/sda1/dolbypq/DolbyPQ_PanelType.bin", DV_PIC_VIVID_MODE);
				if (ret == 0)
					ret = -EFAULT;
#else
				ret = DolbyPQ_1Model_1File_Config(g_PicModeFilePath[DV_PIC_DARK_MODE], DV_PIC_DARK_MODE);//Load Dark mode config
				if (ret == 0)
					ret = -EFAULT;
				ret = DolbyPQ_1Model_1File_Config(g_PicModeFilePath[DV_PIC_BRIGHT_MODE], DV_PIC_BRIGHT_MODE);//Load bright mode config
				if (ret == 0)
					ret = -EFAULT;
				ret = DolbyPQ_1Model_1File_Config(g_PicModeFilePath[DV_PIC_VIVID_MODE], DV_PIC_VIVID_MODE);//Load vivid mode config
				if (ret == 0)
					ret = -EFAULT;
#endif
				// copy to HDMI dm_hdmi_cfg
				memcpy((void *)&dm_hdmi_dark_cfg, (void *)&dm_dark_cfg, sizeof(DmCfgFxp_t)); // copy to HDMI dm_hdmi_cfg
				memcpy((void *)&dm_hdmi_vivid_cfg, (void *)&dm_vivid_cfg, sizeof(DmCfgFxp_t)); // copy to HDMI dm_hdmi_cfg
				memcpy((void *)&dm_hdmi_bright_cfg, (void *)&dm_bright_cfg, sizeof(DmCfgFxp_t)); // copy to HDMI dm_hdmi_cfg
				up(&g_dv_pq_sem);
			}
			break;
		}
	case DOLBYVISIONEDR_DRV_VPQ_GETDOLBYSWVERSION:
		{
			char pstVersion[] = DOLBY_SW_VERSION;
			if (copy_to_user((void __user *)arg, &pstVersion[0], sizeof(pstVersion))) {
				ret = -EFAULT;
				printk(KERN_DEBUG"dolbyvisionEDR: ioctl code = %d failed!!!!!!!!!!!!!!!\n", _IOC_NR(cmd));
			} else {
				printk(KERN_DEBUG"dolbyvisionEDR: ioctl code = %d success!!!!!!!!!!!!!!!\n", _IOC_NR(cmd));
			}
			break;
		}
	case DOLBYVISIONEDR_DRV_SUPPORT_STATUS:
		{
			unsigned int st = (DV_PROFILE_DVHE_STN |
						DV_PROFILE_DVHE_ST |
						DV_PROFILE_DVAE_SE |
						DV_PROFILE_DVHE_DTR_MEL);
			if (copy_to_user((void __user *)arg, &st, sizeof(unsigned int))) {
				ret = -EFAULT;
				printk(KERN_DEBUG"dolbyvisionEDR: ioctl code = %d failed!!!!!!!!!!!!!!!\n", _IOC_NR(cmd));
			}
			ret = 0;
			break;
		}
	default:
		printk(KERN_DEBUG"dolbyvisionEDR: ioctl code = %d is invalid!!!!!!!!!!!!!!!\n", _IOC_NR(cmd));
		break;
	}

	return ret;
}

#ifndef RTK_EDBEC_1_5
void calculate_backlight_setting_for_cfg(DmCfgFxp_t *p_cfg, unsigned char backlight, unsigned char src)
{//Calculate the cfg seeting according to the uBacklight value
#define LOG2_UI_STEPSIZE 3
		int16_t x_x1,x1,y2,y1, Y;
		int32_t temp;

		backlight+=2;
		backlight = CLAMPS(backlight,0,100);

		x1	 = backlight>>LOG2_UI_STEPSIZE;
		x_x1 = backlight - (x1<<LOG2_UI_STEPSIZE);

		y1 = p_cfg->dmCtrl.MidPQBiasLut[x1];
		y2 = p_cfg->dmCtrl.MidPQBiasLut[x1+1];
		y2 = y2-y1 ; //Y2-Y1
		//X2-X1 = 8;
		temp = (y2 * x_x1) >>LOG2_UI_STEPSIZE	;//(y2-y1)/(x2-x1) * (x-x1)
		Y = (int16_t)(temp + y1);
		pr_debug("\r\n###%s: new MidPQBias=%d ####\n", __FUNCTION__, Y);

		p_cfg->dmCtrl.MidPQBias = Y;
		if(src == DOLBY_SRC_HDMI)
			hdmi_h_dm->tcCtrl.tMidPQBias = p_cfg->dmCtrl.MidPQBias;
		else
			OTT_h_dm->tcCtrl.tMidPQBias = p_cfg->dmCtrl.MidPQBias;
}
#endif

static void dolbyvisionEDR_setup_cdev(DOLBYVISIONEDR_DEV *dev, int index)
{
	int err, devno = MKDEV(dolbyvisionEDR_major, dolbyvisionEDR_minor + index);

	cdev_init(&dev->cdev, &dolbyvisionEDR_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops   = &dolbyvisionEDR_fops;
	err = cdev_add (&dev->cdev, devno, 1);

	if (err)
		printk(KERN_DEBUG "Error %d adding se%d", err, index);

	device_create(dolbyvisionEDR_class, NULL, MKDEV(dolbyvisionEDR_major, index), NULL, "dolbyvisionEDR%d", index);
}

static char *dolbyvisionEDR_devnode(struct device *dev, mode_t *mode)
{
	*mode = 0666;//set Permissions
	return NULL;
}

void dolbyvisionEDR_cleanup_module(void)
{
	int i = 0;
	dev_t devno = MKDEV(dolbyvisionEDR_major, dolbyvisionEDR_minor);
	unsigned int size;

	printk(KERN_EMERG"dolbyvisionEDR clean module dolbyvisionEDR_major = %d\n", dolbyvisionEDR_major);

	DV_uninit_debug_proc();

	if (dolbyvisionEDR_devices) {
		for (i = 0; i < dolbyvisionEDR_nr_devs; i++) {
			cdev_del(&dolbyvisionEDR_devices[i].cdev);
			device_destroy(dolbyvisionEDR_class, MKDEV(dolbyvisionEDR_major, i));
		}
		kfree(dolbyvisionEDR_devices);
	}


	class_destroy(dolbyvisionEDR_class);
	size = drvif_memory_get_data_align(VIP_DMAto3DTable_HDR_3D_LUT_SIZE, (1 << 12));
	dvr_unmap_memory((void *)b05_aligned_vir_addr, size);
	/* cleanup_module is never called if registering failed */
	unregister_chrdev_region(devno, dolbyvisionEDR_nr_devs);
}
#endif


unsigned long long gtestcnt = 0, gtestsum = 0;

int calculateTEST(void *qq)
{
#if 0 //Mark by will. Need to check
	int testcnt = 0, ret;
	char *mdptr;
	MdsChg_t mdsChg;
#ifndef WIN32
	struct timespec ts_start, ts_end, ts_diff;
	volatile unsigned int before_time = 0;
	volatile unsigned int after_time = 0;
	unsigned long long timediff;
	static unsigned int mdAddr = 0;
#else
	int mdidx = 0;
#endif

	dm_cfg.frmPri.PxlDefOut.pxlClr = 0;
	dm_cfg.frmPri.PxlDefOut.pxlChrm = 1;
	dm_cfg.frmPri.PxlDefOut.pxlDtp = 0;
	dm_cfg.frmPri.PxlDefOut.pxlBdp = 12;//14;
	dm_cfg.frmPri.PxlDefOut.pxlWeav = 2;
	dm_cfg.frmPri.PxlDefOut.pxlLoc = 0;
	dm_cfg.frmPri.PxlDefOut.pxlEotf = 0;
	dm_cfg.frmPri.PxlDefOut.pxlRng = 1;
	dm_cfg.frmPri.RowNumTtl = 2160;//288;//1080;
	dm_cfg.frmPri.ColNumTtl = 3840;//352;//1920;


	SanityCheckDmCfg(&dm_cfg);

#ifndef WIN32
#ifdef CONFIG_CMA_RTK_ALLOCATOR
	if (mdAddr == 0)
		mdAddr = (unsigned int)dvr_malloc_specific(0x100000, GFP_DCU2);

	printk(KERN_EMERG"mdAddr=0x%x\n", (unsigned int)dvr_to_phys((void *)mdAddr));
#endif

	while (1) {

		mdptr = (char *)mdAddr;

		for (testcnt = 0; testcnt < 92; testcnt++) {

			//ret = ReadMDS(mdptr, &mds_ext, 0, &dm_cfg);
			ret = dm_metadata_2_dm_param((dm_metadata_base_t *)mdptr, &mds_ext);

			if (ret < 0)
				printf("read metadata for frame fail\n");
			else
				mdptr += ret;

			mdsChg = CheckMds(&mds_ext, &h_dm->mdsExt);	// modified by smfan
			//if (mdsChg != MDS_CHG_NONE)
			//	printk("Frame %d \n", testcnt);

			getnstimeofday(&ts_start);
			before_time = rtd_inl(TIMER_SCPU_CLK90K_LO_reg);

			CommitMds(&mds_ext, h_dm); // CommitTrim() is called within CommitMds()

			getnstimeofday(&ts_end);
			after_time = rtd_inl(TIMER_SCPU_CLK90K_LO_reg);

			if (mdsChg != MDS_CHG_NONE) {
				ts_diff = timespec_sub(ts_end, ts_start);
				gtestcnt++;
				gtestsum += ts_diff.tv_nsec;

				timediff = after_time - before_time;
				timediff = timediff * 1000000000;
				do_div(timediff, 90000);
				if (ts_diff.tv_nsec > 700000)
				    printk("%s, Spent time: %d s %lu ns \n", __FUNCTION__, (int)ts_diff.tv_sec, (long unsigned int)ts_diff.tv_nsec );
			    //printk("%s, (%d->%d) Spent time: %d ns \n", __FUNCTION__, before_time, after_time, timediff );
			}
		}

		msleep(888);

		printk("%s, average time = %lld (%lld/%lld) \n", __FUNCTION__, div64_u64(gtestsum, gtestcnt), gtestsum, gtestcnt);
	}

#else

	for (testcnt = 0; testcnt < 92; testcnt++) {

		//Teststream_scrambled_3840x2160_UYVY_12b_00000_md  92 frames
		//ret = ReadMDS(&Teststream_scrambled_3840x2160_UYVY_12b_00000_md[mdidx], &mds_ext, 0, &dm_cfg);
		//ArtGlass_scrambled_1920x1080_UYVY_12b_0086756_md	120 frames
		//ret = ReadMDS(&ArtGlass_scrambled_1920x1080_UYVY_12b_0086756_md[mdidx], &mds_ext, 0, &dm_cfg);

		//ret = dm_metadata_2_dm_param((dm_metadata_base_t *)&Teststream_scrambled_3840x2160_UYVY_12b_00000_md[mdidx], &mds_ext);
		//ret = dm_metadata_2_dm_param((dm_metadata_base_t *)&ArtGlass_scrambled_1920x1080_UYVY_12b_0086756_md[mdidx], &mds_ext);

		if (ret < 0)
			printf("read metadata for frame fail\n");
		else
			mdidx += ret;

		mdsChg = CheckMds(&mds_ext, &h_dm->mdsExt);	// modified by smfan
		if (mdsChg != MDS_CHG_NONE)
			printk("Frame %d \n", testcnt);

		CommitMds(&mds_ext, h_dm); // CommitTrim() is called within CommitMds()
	}
#endif
#endif
	return 0;
}

#ifndef WIN32
void CreateTestThread(void)
{
	static struct task_struct *thread1;
	char our_thread1[18] = "calthread1";
	thread1 = kthread_create(calculateTEST, NULL, our_thread1);

	if ((thread1)) {
		printk(KERN_EMERG "in calculateTEST if1\n");
		wake_up_process(thread1);
	}
}
#endif



#ifndef WIN32
#ifndef CONFIG_SUPPORT_SCALER
unsigned int rpcVoHdrFrameInfo(unsigned long mode, unsigned long para1)
{
	/* 1:position; 2: finish */
	//unsigned int rpcType = (mode & 0xF00) >> 8;
	//mode = mode & 0xFF;

	if (mode == HDR_DOLBY_COMPOSER) {

		//unsigned int mdOutputAddr = Swap4Bytes(para1);//Scaler_ChangeUINT32Endian(para1);
		unsigned int mdOutputAddr = para1;
		unsigned int lineCnt_in, vo_vstart, vo_vend, vgip_lineCnt, v_length;

		lineCnt_in = rtd_inl(VODMA_VODMA_LINE_ST_reg) & 0xfff;
		vgip_lineCnt = VGIP_VGIP_CHN1_LC_get_ch1_line_cnt(rtd_inl(VGIP_VGIP_CHN1_LC_reg));
		vo_vstart = rtd_inl(VODMA_VODMA_V1VGIP_VACT1_reg) & 0xfff;
		vo_vend = (rtd_inl(VODMA_VODMA_V1VGIP_VACT1_reg) >> 16) & 0xfff;
		v_length = rtd_inl(VODMA_VODMA_V1_DSIZE_reg) & VODMA_VODMA_V1_DSIZE_p_y_nline_mask;


		DBG_PRINT(KERN_DEBUG"lineCnt_in=%d, v_length=%d \n", lineCnt_in, v_length);

		if ((v_length > 1080) && (lineCnt_in >= (vo_vend-1))) {
			;// 4k video don't flush
		} else {
			flush_cache_all();
		}

		Normal_TEST((DOLBYVISION_MD_OUTPUT *)phys_to_virt(mdOutputAddr), 0/*rpcType*/);

	} else {
		printk(KERN_DEBUG"%s, mode=%d \n", __FUNCTION__, mode);
	}

	return 0;
}
unsigned int rpcVoReady(unsigned long para1, unsigned long para2)
{
	return 0;
}
unsigned int rpcVoNoSignal(unsigned long para1, unsigned long para2)
{
	return 0;
}
#endif
#endif

#ifdef CONFIG_SUPPORT_SCALER
unsigned char OTT_dolby_update_run = FALSE;//if true, means OTT dolby update now
static irqreturn_t rtk_timer_dolbyvision_interrupt(int irq, void *dev_id)
{
	unsigned int idx;//, loop;
	DOLBYVISION_MD_OUTPUT *p_mdOutput;//, *p_mdOutput_tmp;
	unsigned int stc = 0, useTime;
	unsigned int vgipLcnt, vgipVend;
	unsigned char time6_trigger = FALSE;
	extern volatile unsigned int g_Md_output_buf_vaddr;
	extern volatile unsigned int g_Md_output_buf_phyaddr;
	unsigned int md_buffer_offset = 0;
	time6_trigger = (TIMER_ISR_get_tc6_int(rtd_inl(TIMER_ISR_reg)) && TIMER_TCICR_get_tc6ie(rtd_inl(TIMER_TCICR_reg))) ? TRUE : FALSE;
	// check timer6 irq

	if (time6_trigger && (get_OTT_HDR_mode() == HDR_DOLBY_COMPOSER) && (get_HDMI_HDR_mode() == HDR_MODE_DISABLE)) {


		VIDEO_RPC_DOBLBY_VISION_SHM *pOttDvShm = (VIDEO_RPC_DOBLBY_VISION_SHM *)Scaler_GetShareMemVirAddr(SCALERIOC_GET_OTT_HDR_FRAMEINFO);

		rtk_timer_set_target(6, TIMER_CLOCK/40000);	// 25 us

		// disable timer6
		rtd_outl(TIMER_TC6CR_reg, 0);
		// disable timer6 interrupt
		rtd_outl(TIMER_TCICR_reg, TIMER_TCICR_tc6ie_mask);
		// write 1 clear
		rtd_outl(TIMER_ISR_reg, TIMER_ISR_tc6_int_mask);

		OTT_dolby_update_run = TRUE;

		//lineCnt_in = rtd_inl(VODMA_VODMA_LINE_ST_reg) & 0xfff;
		//vo_vend = (rtd_inl(VODMA_VODMA_V1VGIP_VACT1_reg) >> 16) & 0xfff;

		if (pOttDvShm) {

			//if (pHdrDvShm->picQWr)
			//	Warning("TimeCost=%d \n", rtd_inl(SCPU_CLK90K_LO_reg) - Scaler_ChangeUINT32Endian(pHdrDvShm->picQWr));

			//flush_cache_all();

			idx = Scaler_ChangeUINT32Endian(pOttDvShm->picQWr);
			pOttDvShm->picQRd = Scaler_ChangeUINT32Endian(pOttDvShm->picQRd);
			//p_mdOutput = phys_to_virt(Scaler_ChangeUINT32Endian(pOttDvShm->frameInfo[idx].md_bufPhyAddr));

			md_buffer_offset = (Scaler_ChangeUINT32Endian(pOttDvShm->frameInfo[idx].md_bufPhyAddr) - g_Md_output_buf_phyaddr);
			p_mdOutput = (DOLBYVISION_MD_OUTPUT *)(g_Md_output_buf_vaddr + md_buffer_offset);//virtual address

			if (p_mdOutput) {

				//printk(KERN_ERR"%s, idx=0x%x, p_mdOutput=0x%x, md_bufPhyAddr=0x%x \n", __FUNCTION__, idx, (unsigned int)p_mdOutput, pOttDvShm->frameInfo[idx].md_bufPhyAddr);
#if 1//def CONFIG_RTK_KDEV_VGIP_INTERRUPT
				if(pOttDvShm->picQRd == OTT_STATE_POSITION)
				{
					pOttDvShm->picQRd = 0;//Let next to run finish
					Normal_TEST(p_mdOutput, OTT_STATE_POSITION);
					pr_debug("%s, picQWr=0x%x, picQRd=0x%x \n", __FUNCTION__, Scaler_ChangeUINT32Endian(pOttDvShm->picQWr), OTT_STATE_POSITION);
				}
				else//pOttDvShm->picQRd == 0  case
				{
					Normal_TEST(p_mdOutput, OTT_STATE_FINISH);
					pr_debug("%s, picQWr=0x%x, picQRd=0x%x \n", __FUNCTION__, Scaler_ChangeUINT32Endian(pOttDvShm->picQWr), OTT_STATE_FINISH);
				}
				pr_debug("%s, idx=0x%x, p_mdOutput=0x%x, md_bufPhyAddr=0x%x \n", __FUNCTION__, idx, (unsigned int)p_mdOutput, pOttDvShm->frameInfo[idx].md_bufPhyAddr);

#else
				Normal_TEST(p_mdOutput, pOttDvShm->picQRd);
				pr_debug(KERN_DEBUG"%s, picQWr=0x%x, picQRd=0x%x \n", __FUNCTION__, Scaler_ChangeUINT32Endian(pOttDvShm->picQWr), Scaler_ChangeUINT32Endian(pOttDvShm->picQRd));
				pr_debug(KERN_DEBUG"%s, idx=0x%x, p_mdOutput=0x%x, md_bufPhyAddr=0x%x \n", __FUNCTION__, idx, (unsigned int)p_mdOutput, pOttDvShm->frameInfo[idx].md_bufPhyAddr);
#endif
			}

		}
		OTT_dolby_update_run = FALSE;
	}
//>>>>> start ,20180131 pinyen, hdmi vsif flow
#ifdef CONFIG_SUPPORT_DOLBY_VSIF
else if (time6_trigger &&( (get_HDMI_HDR_mode()==HDR_DOLBY_HDMI) && (get_HDMI_Dolby_VSIF_mode()==DOLBY_HDMI_VSIF_LL))) {

		rtk_timer_set_target(6, TIMER_CLOCK/40000);	// 25 us
		// disable timer6
		rtd_outl(TIMER_TC6CR_reg, 0);
		// disable timer6 interrupt
		rtd_outl(TIMER_TCICR_reg, TIMER_TCICR_tc6ie_mask);
		// write 1 clear
		rtd_outl(TIMER_ISR_reg, TIMER_ISR_tc6_int_mask);

		vgipLcnt = VGIP_VGIP_CHN1_LC_get_ch1_line_cnt(rtd_inl(VGIP_VGIP_CHN1_LC_reg));;
		vgipVend = VGIP_VGIP_CHN1_ACT_VSTA_Length_get_ch1_iv_act_sta(rtd_inl(VGIP_VGIP_CHN1_ACT_VSTA_Length_reg))
										+ VGIP_VGIP_CHN1_ACT_VSTA_Length_get_ch1_iv_act_len(rtd_inl(VGIP_VGIP_CHN1_ACT_VSTA_Length_reg));

		//pr_debug("%s, vgipLcnt=%d, vgipVend=%d \n", __FUNCTION__, vgipLcnt, vgipVend);

		if(1){//(vgipLcnt >= (vgipVend-1)) {			   
			   HDMI_TEST_VSIF(Scaler_DispGetInputInfoByDisp(SLR_MAIN_DISPLAY, SLR_INPUT_IPH_ACT_WID_PRE),Scaler_DispGetInputInfoByDisp(SLR_MAIN_DISPLAY, SLR_INPUT_IPV_ACT_LEN_PRE), (((unsigned char*)&hdmi_dolby_vsi_content)+1));
        }
	}
#endif
//<<<<< end ,20180131 pinyen, hdmi vsif flow
	else if (time6_trigger && (get_HDMI_HDR_mode()==HDR_DOLBY_HDMI)) {
		rtk_timer_set_target(6, TIMER_CLOCK/40000);	// 25 us
		// disable timer6
		rtd_outl(TIMER_TC6CR_reg, 0);
		// disable timer6 interrupt
		rtd_outl(TIMER_TC6CR_reg, 0);
		// write 1 clear
		rtd_outl(TIMER_ISR_reg, TIMER_ISR_tc6_int_mask);

		/* check own_bit that driver will clear own_bit when playback switch to DOLBY HDMI */
		if(get_OTT_HDR_mode() == HDR_DOLBY_COMPOSER) {
			VIDEO_RPC_DOBLBY_VISION_SHM *pOttDvShm = (VIDEO_RPC_DOBLBY_VISION_SHM *)Scaler_GetShareMemVirAddr(SCALERIOC_GET_OTT_HDR_FRAMEINFO);
			if (pOttDvShm) {
				idx = Scaler_ChangeUINT32Endian(pOttDvShm->picQWr);
				pOttDvShm->picQRd = Scaler_ChangeUINT32Endian(pOttDvShm->picQRd);
				p_mdOutput = phys_to_virt(pOttDvShm->frameInfo[idx].md_bufPhyAddr);
				if (p_mdOutput) {
					if (pOttDvShm->picQRd == OTT_STATE_POSITION) {
						p_mdOutput->own_bit = 0;
					}
				}
			}
		}

		//pr_debug("[DOLBY] HDMI path NOW. \n");

                /*20170908, baker modify for dolby hdmi no need to check i3ddma path condition*/
		if (1/*(Scaler_DispGetInputInfoByDisp(SLR_MAIN_DISPLAY, SLR_INPUT_STATE) == _MODE_STATE_ACTIVE)&& (Scaler_DispGetInputInfo(SLR_INPUT_THROUGH_I3DDMA) == TRUE)*/) {
			VIDEO_RPC_DOBLBY_VISION_SHM *pHdrDvShm = (VIDEO_RPC_DOBLBY_VISION_SHM *)Scaler_GetShareMemVirAddr(SCALERIOC_GET_HDMI_HDR_FRAMEINFO);
			//static unsigned int lastStc=0;
			//SLR_VOINFO *pVOInfo = Scaler_VOInfoPointer(Scaler_Get_CurVoInfo_plane());

			if (pHdrDvShm) {
				unsigned int idxRd, idxWrt;
				VIDEO_RPC_DOLBY_VISION_FRAME_INFO frameInfo;
				static unsigned int last_pts;
				vgipLcnt = VGIP_VGIP_CHN1_LC_get_ch1_line_cnt(rtd_inl(VGIP_VGIP_CHN1_LC_reg));;
				vgipVend = VGIP_VGIP_CHN1_ACT_VSTA_Length_get_ch1_iv_act_sta(rtd_inl(VGIP_VGIP_CHN1_ACT_VSTA_Length_reg))
										+ VGIP_VGIP_CHN1_ACT_VSTA_Length_get_ch1_iv_act_len(rtd_inl(VGIP_VGIP_CHN1_ACT_VSTA_Length_reg));

				pr_debug("%s, vgipLcnt=%d, vgipVend=%d \n", __FUNCTION__, vgipLcnt, vgipVend);


				/* change endian */
				idxWrt = pHdrDvShm->picQWr;
				idxRd  = (idxWrt == 0? 15: idxWrt-1);
				pHdrDvShm->picQRd = idxRd;

				pr_debug("%s, idxWrt=%d, idxRd=%d \n", __FUNCTION__, idxWrt, idxRd);

				if (idxRd <= 15) {
					memcpy(&frameInfo, &pHdrDvShm->frameInfo[idxRd], sizeof(VIDEO_RPC_DOLBY_VISION_FRAME_INFO));
					// Dolby Vision HDMI mode DM setting
					//pr_debug("%s, lastWriteIdx=0x%x, idxWrt=0x%x \n", __FUNCTION__, lastWriteIdx, idxWrt);

					if ((frameInfo.md_bufPhyAddr & 0x1fffffff) && ((frameInfo.md_bufPhyAddr % 4) == 0) && frameInfo.md_pktSize && frameInfo.picWidth && frameInfo.picLen) {
						if (((hdmi_dolby_last_pair_num == hdmi_dolby_cur_pair_num) && (get_cur_hdmi_dolby_apply_state() == HDMI_STATE_FINISH)/*(vgipLcnt >= (vgipVend-1))*/)) {
							// Main VGIP data end ISR handler
							HDMI_TEST(frameInfo.picWidth, frameInfo.picLen, frameInfo.md_bufPhyAddr);

						} else if (get_cur_hdmi_dolby_apply_state() == HDMI_STATE_POSITION)/*(vgipLcnt < vgipVend)*/ {

							unsigned char bRptPkt = 0;
							if(hdmi_ui_change_flag)
							{//Picture mode or backlighr change
								hdmi_ui_change_delay_count = 0;
								spin_lock(dolby_hdmi_ui_change_spinlock());
								hdmi_ui_change_flag = FALSE;
								spin_unlock(dolby_hdmi_ui_change_spinlock());
							}
							hdmi_dolby_cur_pair_num ++;
							if(hdmi_dolby_cur_pair_num == 200)
								hdmi_dolby_cur_pair_num = 0;
							stc = rtd_inl(TIMER_SCPU_CLK90K_LO_reg);
							// check repeat packet: (repeat flag==1) && (write index continue) && (pts diff < 24hz)
							if ((frameInfo.md_pts & 0x1) && ((idxRd - g_hdmi_lastIdxRd == 1)|| (g_hdmi_lastIdxRd - idxRd == 15)) && ((frameInfo.md_pts - last_pts) < (42*90))) {
								// check in HDMI mode
								//if ((rtd_inl(DOLBY_V_TOP_TOP_CTL_reg) & DOLBY_V_TOP_TOP_CTL_dolby_mode_mask) == 1)//Mark by will
									bRptPkt = 1;
								//pr_debug("R.%d-%d(%d)\n", lastIdxRd, idxRd, (frameInfo.md_pts - last_pts) / 90);
							}

							if ((g_hdmi_lastIdxRd != idxRd) && !bRptPkt) {
								hdmi_dolby_last_pair_num = hdmi_dolby_cur_pair_num;	//Let finish be able apply. Means position already apply
								HDMI_TEST(frameInfo.picWidth, frameInfo.picLen, frameInfo.md_bufPhyAddr);
							} else if(g_picModeUpdateFlag || ((hdmi_ui_change_delay_count == HDMI_DOLBY_UI_CHANGE_DELAY_APPLY) && ((HDMI_last_md_picmode != ui_dv_picmode) || (HDMI_last_md_backlight != ui_dv_backlight_value)))) {
								//When picture mode or backlight change. We need to apply HDMI_TEST
								hdmi_dolby_last_pair_num = hdmi_dolby_cur_pair_num;//Let finish be able apply. Means position already apply
								HDMI_TEST(frameInfo.picWidth, frameInfo.picLen, frameInfo.md_bufPhyAddr);
							}
							if(hdmi_ui_change_delay_count < HDMI_DOLBY_UI_CHANGE_DELAY_APPLY)
							{
								hdmi_ui_change_delay_count++;
							}
							useTime = rtd_inl(TIMER_SCPU_CLK90K_LO_reg) - stc;
							last_pts = frameInfo.md_pts;
							g_hdmi_lastIdxRd = idxRd;
						}
					} else {
						//pr_debug("[DV]Info=%dx%d(%d)@%x\n", frameInfo.picWidth, frameInfo.picLen, frameInfo.md_pktSize, frameInfo.md_bufPhyAddr);
						pr_debug(KERN_NOTICE "[DV]Err=%x\n", frameInfo.md_status);
					}

#if 0 // dump packet body data
					if (1) {
						static unsigned int dbgCnt=0;
						frameInfo.md_bufPhyAddr = (frameInfo.md_bufPhyAddr & 0x1fffffff)|0xc0000000; // physical address cast to SCPU mapping address
						if (++dbgCnt % 20 == 0) {
							int i;
							unsigned int dumpCnt = (frameInfo.md_pktSize/4)+(frameInfo.md_pktSize%4? 1: 0);
							// dump packet body
							for (i=0; i<dumpCnt; i+=4) {
								pr_debug("\n%08x%08x%08x%08x",
									Scaler_ChangeUINT32Endian(*((unsigned int*)(frameInfo.md_bufPhyAddr+((i+0)*4)))),
									Scaler_ChangeUINT32Endian(*((unsigned int*)(frameInfo.md_bufPhyAddr+((i+1)*4)))),
									Scaler_ChangeUINT32Endian(*((unsigned int*)(frameInfo.md_bufPhyAddr+((i+2)*4)))),
									Scaler_ChangeUINT32Endian(*((unsigned int*)(frameInfo.md_bufPhyAddr+((i+3)*4)))));
							}
							pr_debug("\n");
						}
					}
#endif
				} else {
					pr_debug(KERN_DEBUG "[ERR][DV] idxRd[%d] > 15\n", idxRd);
				}
#if 0 // [TEST] Dolby Vision check DM LUT convert time
			// check (use time > 5ms)||(rpc/shm index not match)||(rpc stc diff > 17ms)
			if (/*(useTime > 5*90)||*/(para1 != idxRd)|| (stc - lastStc> ((10000/pVOInfo->v_freq)+1)*90)) {
				//pr_debug("%d/%d/%d=%d\n", para1, idxRd, idxWrt, (stc - lastStc)/90);
				if (para1 != idxRd)
					pr_debug("%d/%d\n", (unsigned int)para1, idxRd);
				else if (stc - lastStc> ((10000/pVOInfo->v_freq)+1)*90)
					pr_debug(KERN_DEBUG "dis=%d(%d)\n", (stc - lastStc)/90, ((10000/pVOInfo->v_freq)+1)*90);
			}
			lastStc = stc;
#endif

			}
		}
	}
	else if (time6_trigger) {
		rtk_timer_set_target(6, TIMER_CLOCK/40000);	// 25 us
		// disable timer6
		rtd_outl(TIMER_TC6CR_reg, 0);
		// disable timer6 interrupt
		rtd_outl(TIMER_TCICR_reg, TIMER_TCICR_tc6ie_mask);
		// write 1 clear
		rtd_outl(TIMER_ISR_reg, TIMER_ISR_tc6_int_mask);

		/* check own_bit that driver will clear own_bit when playback switch to HDR10 or else */
		if(get_OTT_HDR_mode() == HDR_DOLBY_COMPOSER) {
			VIDEO_RPC_DOBLBY_VISION_SHM *pOttDvShm = (VIDEO_RPC_DOBLBY_VISION_SHM *)Scaler_GetShareMemVirAddr(SCALERIOC_GET_OTT_HDR_FRAMEINFO);
			if (pOttDvShm) {
				idx = Scaler_ChangeUINT32Endian(pOttDvShm->picQWr);
				pOttDvShm->picQRd = Scaler_ChangeUINT32Endian(pOttDvShm->picQRd);
				p_mdOutput = phys_to_virt(pOttDvShm->frameInfo[idx].md_bufPhyAddr);
				if (p_mdOutput) {
					if (pOttDvShm->picQRd == 0x1abc) {
						p_mdOutput->own_bit = 0;
					}
				}
			}
		}


	} else {
		return IRQ_NONE;
	}

	return IRQ_HANDLED;
}

unsigned char check_ott_dolby_update_run(void)
{
	return OTT_dolby_update_run;
}
#endif

static int dolbyvisionEDR_register_device(DOLBYVISIONEDR_DEV *dolbyvisionEDR) {
	dev_t dev = 0;
    int result,i;

	if (dolbyvisionEDR_major) {
		dev = MKDEV(dolbyvisionEDR_major, dolbyvisionEDR_minor);
		result = register_chrdev_region(dev, dolbyvisionEDR_nr_devs, "dolbyvisionEDR");
	} else {
		result = alloc_chrdev_region(&dev, dolbyvisionEDR_minor, dolbyvisionEDR_nr_devs, "dolbyvisionEDR");
		dolbyvisionEDR_major = MAJOR(dev);
	}

	if (result < 0) {
		printk(KERN_DEBUG "dolbyvisionEDR: can't get major %d\n", dolbyvisionEDR_major);
		return result;
	}

	printk(KERN_INFO "dolbyvisionEDR init module major number = %d\n", dolbyvisionEDR_major);

	dolbyvisionEDR_class = class_create(THIS_MODULE, "dolbyvisionEDR");
	if (IS_ERR((dolbyvisionEDR_class)))
		return PTR_ERR(dolbyvisionEDR_class);

	dolbyvisionEDR_class->devnode = dolbyvisionEDR_devnode;

	dolbyvisionEDR_devices = kmalloc(dolbyvisionEDR_nr_devs * sizeof(DOLBYVISIONEDR_DEV), GFP_KERNEL);
	if (!(dolbyvisionEDR_devices)) {
		result = -ENOMEM;
		dolbyvisionEDR_cleanup_module();   /* fail */
		return result;
	}

#ifdef CONFIG_PM
	dolbyvisionEDR_devs = platform_device_register_simple(DRV_NAME, -1, NULL, 0);
	result = platform_driver_register(&dolbyvisionEDR_driver);
	if ((result) != 0) {
		printk(KERN_EMERG "Can't register dolbyvisionEDR device driver...\n");
	} else {
		printk(KERN_INFO "register dolbyvisionEDR device driver...\n");
	}
#endif


	memset(dolbyvisionEDR_devices, 0, dolbyvisionEDR_nr_devs * sizeof(DOLBYVISIONEDR_DEV));

	for (i = 0; i < dolbyvisionEDR_nr_devs; i++) {
		dolbyvisionEDR = &dolbyvisionEDR_devices[i];
		dolbyvisionEDR_setup_cdev(dolbyvisionEDR, i);
	}

#ifdef CONFIG_PM
	dolbyvisionEDR_devices->dev = &(dolbyvisionEDR_devs->dev);
	dolbyvisionEDR_devices->dev->dma_mask = &dolbyvisionEDR_devices->dev->coherent_dma_mask;
	if(dma_set_mask(dolbyvisionEDR_devices->dev, DMA_BIT_MASK(32))) {
		printk(KERN_EMERG "[dolbyvisionEDR] DMA not supported\n");
	}
#endif

	dev = MKDEV(dolbyvisionEDR_major, dolbyvisionEDR_minor + dolbyvisionEDR_nr_devs);

	return result;
}

extern u32 gic_irq_find_mapping(u32 hwirq);//cnange interrupt register way 

int dolbyvisionEDR_init_module(void)
{
	DOLBYVISIONEDR_DEV *dolbyvisionEDR = &dolbyvisionEDR_devices[0];
#ifdef RTK_EDBEC_1_5
    run_mode_t run_mode;
#endif
	unsigned int addr = 0, size;
	unsigned int addr_aligned = 0, size_aligned;
	unsigned int  *pVir_addr = NULL;
#ifdef CONFIG_ARM64 //ARM32 compatible
	unsigned long va_temp;
#else
	unsigned int va_temp;
#endif
	printk("\n\n\n\n *****************    dolbyvisionEDR init module  *********************\n\n\n\n");

	if(dolbyvisionEDR_register_device(dolbyvisionEDR) < 0) {
		return -1;
	}
	DV_init_debug_proc();
	/* dolbyvision EDR global variable initialization */
	f_stop = TRUE;
    f_dovi_hw_proc_pending = FALSE;
    f_first_sample = TRUE;

#ifdef RTK_EDBEC_1_5
	/* initialize DM configuration */
	OTT_h_dm = kmalloc(sizeof(cp_context_t)+sizeof(cp_context_t_half_st), GFP_KERNEL);
	OTT_h_dm_st = kmalloc(sizeof(cp_context_t_half_st), GFP_KERNEL);
	if (OTT_h_dm == NULL|| OTT_h_dm_st == NULL){
		printk(KERN_DEBUG"%s, OTT_h_dm failed \n", __FUNCTION__);
	}else{
		printk(KERN_INFO"%s, OTT h_dm addresss = 0x%x \n", __FUNCTION__, (unsigned int)OTT_h_dm);
		printk(KERN_INFO"%s, OTT h_dm_st addresss = 0x%x \n", __FUNCTION__, (unsigned int)OTT_h_dm_st);
	}

	memset(OTT_h_dm, 0, sizeof(cp_context_t));
	memset(OTT_h_dm_st, 0, sizeof(cp_context_t_half_st));
	memset(&run_mode, 0, sizeof(run_mode));

	printk("\n[DV]%s, OTT-h_dm=%p, h_mds=%p, h_ks=%p",__func__, OTT_h_dm->h_dm, OTT_h_dm->h_mds_ext , OTT_h_dm->h_ks);
	init_cp(OTT_h_dm, &run_mode , NULL, (char*)OTT_h_dm_st);
	printk("\n[DV]%s, OTT-h_dm=%p, h_mds=%p, h_ks=%p",__func__, OTT_h_dm->h_dm, OTT_h_dm->h_mds_ext , OTT_h_dm->h_ks);

	hdmi_h_dm = kmalloc(sizeof(cp_context_t)+sizeof(cp_context_t_half_st), GFP_KERNEL);
	hdmi_h_dm_st = kmalloc(sizeof(cp_context_t_half_st), GFP_KERNEL);
	if (hdmi_h_dm == NULL || hdmi_h_dm_st == NULL ){
		printk(KERN_DEBUG"%s, CreateDmFxp failed \n", __FUNCTION__);
	}else{
		printk(KERN_INFO"%s, HDMI h_dm addresss = 0x%x \n", __FUNCTION__, (unsigned int)hdmi_h_dm);
		printk(KERN_INFO"%s, HDMI h_dm_st addresss = 0x%x \n", __FUNCTION__, (unsigned int)hdmi_h_dm_st);
	}
	memset(hdmi_h_dm, 0, sizeof(cp_context_t));
	memset(hdmi_h_dm_st, 0, sizeof(cp_context_t_half_st));

	printk("\n[DV]%s, OTT-h_dm=%p, h_mds=%p, h_ks=%p",__func__, hdmi_h_dm->h_dm, hdmi_h_dm->h_mds_ext , hdmi_h_dm->h_ks);

	init_cp(hdmi_h_dm, &run_mode , NULL, (char*)hdmi_h_dm_st);

	printk("\n[DV]%s, OTT-h_dm=%p, h_mds=%p, h_ks=%p",__func__, hdmi_h_dm->h_dm, hdmi_h_dm->h_mds_ext , hdmi_h_dm->h_ks);
	printk("\n[DV]%s, OTT-BitDepth=%d, HDMI=%d\n",__func__, OTT_h_dm->dm_cfg.srcSigEnv.Bdp, hdmi_h_dm->dm_cfg.srcSigEnv.Bdp);
#endif

	/* set DM source for TV application mode */
	f_hdmi_src = FALSE;


    frm_period_90KHz = FRAME_PERIOD_DEFAULT;
    last_compose_stc = 0;

    f_init = TRUE;


#ifndef WIN32
#ifndef CONFIG_SUPPORT_SCALER
#if (defined(CONFIG_REALTEK_RPC))||defined(CONFIG_RTK_KDRV_RPC)
	/* register rpc handler */
	if (register_kernel_rpc(RPC_VCPU_ID_0x112_HDR_FRAMEINFO, (FUNC_PTR)rpcVoHdrFrameInfo) == 1)
		printk(KERN_EMERG"Register RPC HDR_FRAMEINFO failed!\n");
	if (register_kernel_rpc(RPC_VCPU_ID_0x110_VO_READY, (FUNC_PTR)rpcVoReady) == 1)
		printk("Register RPC VO_READY failed!\n");
	if (register_kernel_rpc(RPC_VCPU_ID_0x111_VO_NOSIGNAL, (FUNC_PTR)rpcVoNoSignal) == 1)
		printk("Register RPC VO_NOSIGNAL failed!\n");
	if (send_rpc_command(RPC_VIDEO, VIDEO_RPC_VOUT_ToAgent_VoDriverReady, 1, 0, &ret))
		printk(KERN_EMERG"VoReady RPC fail!!\n");
	if (register_kernel_rpc(RPC_VCPU_ID_0x115_VO_EOS, (FUNC_PTR)rpcVoEOS) == 1)
		printk("Register RPC VO_EOS failed!\n");
#endif
#endif
	/* initial semaphore for normal control flow */
	sema_init(&g_dv_sem, 1);
	sema_init(&g_dv_pq_sem, 1);
#endif

#ifdef WIN32
	//calculateTEST();
	MD_PARSER_STATE mdstate = MD_PARSER_INIT;
	MdsChg_t mdsChg;
	extern volatile unsigned int g_rpu_size;		// added by smfan
	int idx = 0, mdidx = 0, ret;
	unsigned char *mdptr;

	//display_management_normal_config();	// mask by smfan
#if EN_GLOBAL_DIMMING
	dm_dark_cfg.gdCtrl.GdOn = 1;
	dm_dark_cfg.gdCtrl.GdCap = 1;
	dm_bright_cfg.gdCtrl.GdOn = 1;
	dm_bright_cfg.gdCtrl.GdCap = 1;
	dm_vivid_cfg.gdCtrl.GdOn = 1;
	dm_vivid_cfg.gdCtrl.GdCap = 1;
	dm_hdmi_dark_cfg.gdCtrl.GdOn = 1;
	dm_hdmi_dark_cfg.gdCtrl.GdCap = 1;
	dm_hdmi_vivid_cfg.gdCtrl.GdOn = 1;
	dm_hdmi_vivid_cfg.gdCtrl.GdCap = 1;
	dm_hdmi_bright_cfg.gdCtrl.GdOn = 1;
	dm_hdmi_bright_cfg.gdCtrl.GdCap = 1;
#endif

	while (1) {
		g_rpu_size = 0;

		mdptr = &tmp_rpu[0];
		for (mdidx = 0; mdidx <= 100; mdidx++) {
			//ret = dm_metadata_2_dm_param((dm_metadata_base_t *)mdptr, &mds_ext);
			//mdptr += ret;
			if (mdidx == 0)
				ret = metadata_parser_main(mdptr, 0x200000, MD_PARSER_INIT);
			else
				ret = metadata_parser_main(mdptr, 0x200000, MD_PARSER_RUN);

			mdptr += ret;

		}

		for (mdidx = 0; mdidx <= 100; mdidx++)
			printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
		break;
	}

#endif
	//CreateTestThread();

	printf("composer structure size = %d \n", sizeof(rpu_ext_config_fixpt_main_t));


#ifdef CONFIG_SUPPORT_SCALER
	// added by smfan
	if (request_irq(gic_irq_find_mapping(IRQ_MISC), rtk_timer_dolbyvision_interrupt, IRQF_SHARED, "timer_dv", dolbyvisionEDR_devices)) {
		printk(KERN_EMERG"timer_dv: can't get assigned irq%d\n", IRQ_MISC);
	}

	// create timer to notice this kernel driver
	//create_timer(6, TIMER_CLOCK/10000, COUNTER);	// 100 us
	//create_timer(6, TIMER_CLOCK/20000, COUNTER);	// 50 us

	//Disable Interrupt
	rtk_timer_control(6, HWT_INT_DISABLE);
	//Set The Initial Value
	rtk_timer_set_value(6, 0);
	//Set The Target Value
	rtk_timer_set_target(6, TIMER_CLOCK/40000);// 25 us
	//Enable Timer Mode
	rtk_timer_set_mode(6, COUNTER);
	rtd_outl(TIMER_ISR_reg, TIMER_ISR_tc6_int_mask);// write 1 clear

    rtk_timer6_11_route_to_SCPU(6, _ENABLE); // enable timer6 interrupt route to SCPU
#endif

	//Get B05 DMA virtual address
	addr = get_query_start_address(QUERY_IDX_VIP_DMAto3DTABLE);
	addr = drvif_memory_get_data_align(addr, (1 << 12));
	size = drvif_memory_get_data_align(VIP_DMAto3DTable_HDR_3D_LUT_SIZE, (1 << 12));

	if (addr == 0) {
		printk("[Dolby] B05_start_address = NULL!!!\n");
	}
	else{
		pVir_addr = dvr_remap_uncached_memory(addr, size, __builtin_return_address(0));
		printk("[Dolby] B05_start_address = %x, vir_addr = %p!!!\n",addr,pVir_addr);
	}

	size_aligned = size;
	addr_aligned = addr;
#ifdef CONFIG_ARM64 //ARM32 compatible
	va_temp = (unsigned long)pVir_addr;
#else
	va_temp = (unsigned int)pVir_addr;
#endif
	b05_aligned_vir_addr = (unsigned int*)va_temp;

#ifdef CONFIG_BW_96B_ALIGNED
	size_aligned = dvr_size_alignment(size);
	addr_aligned = (unsigned int)dvr_memory_alignment((unsigned long)addr, size_aligned);
#ifdef CONFIG_ARM64 //ARM32 compatible
	va_temp = (unsigned long)pVir_addr;
#else //CONFIG_ARM64
	va_temp = (unsigned int)pVir_addr;
#endif //CONFIG_ARM64
	b05_aligned_vir_addr = (unsigned int*)dvr_memory_alignment((unsigned long)va_temp, size_aligned);
#endif
	/* DMA addr setting*/
	rtd_outl(DMATO3DTABLE_DMAto3DTable_Table0_Addr_reg, (addr_aligned/*-0x200*/));

	printk("dolbyvisionEDR module_init finish\n");


	return 0;
}



#ifndef WIN32

static inline bool rtkdv_dbg_parse_value(char *cmd_pointer, long long *parsed_data)
{
	if (kstrtoll(cmd_pointer, 0, parsed_data) == 0) {
		return true;
	} else {
		return false;
	}
}
static inline void rtkdv_dbg_EXECUTE(const int cmd_index, char **cmd_pointer)
{
	long long parsed_data = 0;

	long tmp_reg=0;

	switch (cmd_index) {
		case DV_DBG_CMD_SET_CTF:
			rtkdv_dbg_parse_value(*cmd_pointer, &parsed_data);
			//g_osdshift_ctrl.h_shift_pixel = (int)parsed_data;
			break;
		case DV_DBG_CMD_SET_LOG_LEVEL:
			rtkdv_dbg_parse_value(*cmd_pointer, &parsed_data);
			g_log_level = (int)parsed_data;
			printk("Set g_log_level=%d \n",g_log_level);
			DBG_PRINT("Set g_log_level=%d \n",g_log_level);
			break;
		case DV_DBG_CMD_SET_TELNET_FD:
			rtkdv_dbg_parse_value(*cmd_pointer, &parsed_data);
			tty_num = (int)parsed_data;
			if(tty_num == 0)
				DBG_PRINT_RESET();
			printk("Set tty_num=%d \n",tty_num);
			DBG_PRINT("Set tty_num=%d \n",tty_num);
			break;
		case DV_DBG_CMD_SET_REG_A:
			break;
		case DV_DBG_CMD_SET_REG_D:
			break;
		case DV_DBG_CMD_RED_REG_A:
			break;
		case DV_DBG_CMD_CHK_STATUS:
			break;
		case DV_DBG_CMD_DUMP_MD:
			rtkdv_dbg_parse_value(*cmd_pointer, &parsed_data);
			g_dump_flag = (long)parsed_data;

			break;
		case DV_DBG_CMD_DUMP_DITHER_OUTPUT:
			rtkdv_dbg_parse_value(*cmd_pointer, &parsed_data);
			dump_frame_cnt = (long)parsed_data;
			break;
		case DV_DBG_CMD_I2R_FPS_THRESHOLD:
			rtkdv_dbg_parse_value(*cmd_pointer, &parsed_data);
			g_i2r_fps_thr = (unsigned int)parsed_data;
			printk(KERN_ERR "[DV]set DV_DBG_CMD_I2R_FPS_THRESHOLD = %d\n", g_i2r_fps_thr);
			break;
		case DV_DBG_CMD_SET_M_CAP_SINGLE_BUFFER:
			rtkdv_dbg_parse_value(*cmd_pointer, &parsed_data);
			tmp_reg = rtd_inl(0xb8027220);
			if(parsed_data)
				rtd_outl(0xb8027220,(tmp_reg & ~(0x140)));
			else
                rtd_outl(0xb8027220,tmp_reg | (0x140));
			break;
		default:
			break;
	}
}

void DV_init_debug_proc(void)
{
	if (rtkdv_proc_dir == NULL && rtkdv_proc_entry == NULL) {
		rtkdv_proc_dir = proc_mkdir(RTKDV_PROC_DIR , NULL);

		if (rtkdv_proc_dir != NULL) {
			rtkdv_proc_entry =
				proc_create(RTKDV_PROC_ENTRY, 0666,
							rtkdv_proc_dir, &dolbyvisionEDR_fops);

			if (rtkdv_proc_entry == NULL) {
				proc_remove(rtkdv_proc_dir);
				rtkdv_proc_dir = NULL;

				return;
			}
		} else {

			return;
		}
	}

	return;
}

void DV_uninit_debug_proc(void)
{
	if (rtkdv_proc_entry) {
		proc_remove(rtkdv_proc_entry);
		rtkdv_proc_entry = NULL;
	}

	if (rtkdv_proc_dir) {
		proc_remove(rtkdv_proc_dir);
		rtkdv_proc_dir = NULL;
	}
}

module_init(dolbyvisionEDR_init_module);
module_exit(dolbyvisionEDR_cleanup_module);
#endif



#if 0	// mask by smfan


/*-----------------------------------------------------------------------------+
 |	Application interrupt handler for the COMPOSER interupt
 +----------------------------------------------------------------------------*/

static void composer_hw_rdy_intr_handler(void)
{
	os_sem_post(h_rdy_sgnl);
}


/*-----------------------------------------------------------------------------+
 |	HDMI mode processing function
 +----------------------------------------------------------------------------*/
#ifdef SUPPORT_EDR_TV_MODE
static void hdmi_src_process(void *arg)
{
	dm_metadata_base_t	  *p_dm_md = NULL;

/*
*	h_dm:		pointer to share memory where indicate DM information
*	p_dm_md:	pointer to the DDR memory where HW IDMA_HDMI_3D_DMA metadata parsing out
*				H3DDMA_md_m1_Start_reg(0xB8025898) or H3DDMA_md_m2_Start_reg(0xB802589C)
*				H3DDMA_md_m3_Start_reg(0xB80258A0) or H3DDMA_md_m4_Start_reg(0xB80258A4)
*/

	if (f_stop)
	{
		if (f_vdr_hw_proc_pending)
		{
			if (hal_vdr_hw_busy())
				os_sem_wait(h_rdy_sgnl);
			f_vdr_hw_proc_pending = FALSE;
		}
		os_sem_wait(h_run_sgnl);
		f_stop = FALSE;
	}

	/* TODO: define API and add the call to get the metadata descrambled
			 from HDMI for the next frame */

	/* convert HDMI metadata byte stream to metadata structure */
	dm_metadata_2_dm_param(p_dm_md, &mds_ext);

	/* generate DM parameters and tone curve LUT for DM */
	CommitMds(&mds_ext, h_dm);

	/* wait for vdr hardware to be ready or finish */
	if (f_vdr_hw_proc_pending && hal_vdr_hw_busy())
		os_sem_wait(h_rdy_sgnl);

	/* load parameters and LUTs into DM hardware for HDMI mode */
	hal_dm_normal_ld_param(&h_dm->dmExec);

	/* start the DM for HDMI mode */
	hal_dm_start();

	f_vdr_hw_proc_pending = TRUE;
}
#endif


/*-----------------------------------------------------------------------------+
 |	None HDMI mode processing function
 +----------------------------------------------------------------------------*/

static void none_hdmi_src_process(void *arg)
{
	dcd_sample_handle_t    bl_sample = NULL;
	dcd_sample_handle_t    el_sample = NULL;
	dcd_sample_handle_t    md_sample = NULL;
	char				  *p_metadata = NULL;
	pts_t				   pts = 0;
	EDBEC_compose_mode_t   frm_compose_mode = EL_FREE_COMPOSING_MODE;
	char				  *p_rpu_md;
	dm_metadata_base_t	  *p_dm_md;


	if (f_stop)
	{
		if (f_vdr_hw_proc_pending)
		{
			/* wait for composer hardware to be ready or finish */
			if (hal_vdr_hw_busy())
				os_sem_wait(h_rdy_sgnl);

			/* remove decoded sample from descriptor queues */
			remove_dcd_sample(BL_QID, last_pts);
			remove_dcd_sample(MD_QID, last_pts);
			remove_dcd_sample(EL_QID, last_pts);
			f_vdr_hw_proc_pending = FALSE;
		}
		os_sem_wait(h_run_sgnl);
		f_stop = FALSE;
	}

	if (BL_and_MD_sample_aligned(&bl_sample, &md_sample, &pts))
	{
		frm_compose_mode = det_compose_mode(&el_sample, pts);
		p_metadata = get_dcd_sample_frm_buf(md_sample);

		/* extract the composer(RPU) and DM metadata */
		p_rpu_md = p_metadata;
		p_dm_md  = (dm_metadata_base_t *)(p_metadata + sizeof(rpu_ext_config_fixpt_main_t));

		dm_metadata_2_dm_param(p_dm_md, &mds_ext);

#ifdef SUPPORT_EDR_TV_MODE
		/* NORMAL DM mode */
		if (app_mode == DV_TV_APP_MODE)
		{
			/* generate DM parameters and tone curve LUT for NORMAL DM */
			CommitMds(&mds_ext, h_dm);
		}
#endif
#ifdef SUPPORT_EDR_STB_MODE
		/* BYPASS DM mode */
		if (app_mode == DV_STB_APP_MODE)
		{
#ifndef DV_VIDEO_ONLY
			if (f_graphic_on)
			{
				if (mds_ext.min_PQ > GRAPHIC_MIN_PQ)
				{
					mds_ext.min_PQ = GRAPHIC_MIN_PQ;
				}
				if (mds_ext.max_PQ < GRAPHIC_MAX_PQ)
				{
					mds_ext.max_PQ = GRAPHIC_MAX_PQ;
				}
			}
#endif
			/* generate DM parameters and 3D LUT for Graphic DM */
			CommitMds(&mds_ext, h_dm);
			/* update DM metadata byte-stream for HDMI DM */
			bypass_metadata_update(p_dm_md, f_graphic_on ? true : false);
		}
#endif

		/* wait for composer hardware to be ready or finish */
		if (hal_vdr_hw_busy())
			os_sem_wait(h_rdy_sgnl);

#ifdef SUPPORT_EDR_TV_MODE
		/* NORMAL DM mode */
		if (app_mode == DV_TV_APP_MODE)
		{
			/* load parameters and LUTs into DM hardware for NORMAL mode */
			hal_dm_normal_ld_param(&h_dm->dmExec);
		}
#endif
#ifdef SUPPORT_EDR_STB_MODE
		/* BYPASS DM mode */
		if (app_mode == DV_STB_APP_MODE)
		{
			/* load parameters and LUTs into DM hardware for GRAPHIC mode */
			hal_dm_by_exec_ld_param(&h_dm->byExecRgb);
			/* load parameters into DM BYPASS block */
#ifndef DV_VIDEO_ONLY
			hal_dm_edr_exec_ld_param(&h_dm->edrExecBw);
#endif
			/* copy DM metadata byte-stream to EDR-over-HDMI block */
			/* TODO: */
		}
#endif
		compose(pts, bl_sample, el_sample, frm_compose_mode, p_rpu_md);

		if (f_vdr_hw_proc_pending)
		{
			/* remove decoded sample from descriptor queues */
			remove_dcd_sample(BL_QID, last_pts);
			remove_dcd_sample(MD_QID, last_pts);
			remove_dcd_sample(EL_QID, last_pts);
		}

		f_vdr_hw_proc_pending = TRUE;
		last_pts = pts;
		f_first_sample = FALSE;
	}
	else
	{

		if (f_vdr_hw_proc_pending)
		{
			/* wait for composer hardware to be ready or finish */
			if (hal_vdr_hw_busy())
				os_sem_wait(h_rdy_sgnl);

			/* remove decoded sample from descriptor queues */
			remove_dcd_sample(BL_QID, last_pts);
			remove_dcd_sample(MD_QID, last_pts);
			remove_dcd_sample(EL_QID, last_pts);
			f_vdr_hw_proc_pending = FALSE;
		}
		else
		{
			os_sleep_msec(ALIGN_POLLING_PERIOD);
		}
	}
}



/*-----------------------------------------------------------------------------+
 |	Determine if the samples at the head of the BL and MD sample descriptor
 |	queue are aligned
 +----------------------------------------------------------------------------*/

static bool_t BL_and_MD_sample_aligned
	(
	dcd_sample_handle_t *ph_bl_sample,
	dcd_sample_handle_t *ph_md_sample,
	uint64_t			*p_pts
	)
{
	uint64_t bl_sample_pts = 0;
	uint64_t md_sample_pts = 0;
	bool_t	 rc = FALSE;

	/* get the oldest decoded BL sample */
	if (((*ph_bl_sample) = get_next_dcd_sample(BL_QID)))
	{
		bl_sample_pts = get_dcd_sample_pts(*ph_bl_sample);
		if (!decoded_sample_valid(*ph_bl_sample))
		{
			remove_dcd_sample(BL_QID, bl_sample_pts);
			*ph_bl_sample = NULL;
		}
	}

	/* get the oldest decoded MD sample */
	if (((*ph_md_sample) = get_next_dcd_sample(MD_QID)))
	{
		md_sample_pts = get_dcd_sample_pts(*ph_md_sample);
		if (!decoded_sample_valid(*ph_md_sample))
		{
			remove_dcd_sample(MD_QID, md_sample_pts);
			*ph_md_sample = NULL;
		}
	}

	/* need at least a BL and MD sample */
	if ((*ph_bl_sample) && (*ph_md_sample))
	{
		if (bl_sample_pts > md_sample_pts)
		{
			/* MD sample is late, remove MD sample */
			remove_dcd_sample(MD_QID, md_sample_pts);
		}
		else if (bl_sample_pts < md_sample_pts)
		{
			/* BL sample is late, remove BL sample */
			remove_dcd_sample(BL_QID, bl_sample_pts);
		}
		else
		{
			/* BL and MD samples are aligned */
			*p_pts = bl_sample_pts;
			rc = TRUE;
		}
	}

	return rc;
}



/*-----------------------------------------------------------------------------+
 |	Determine composing mode
 +----------------------------------------------------------------------------*/

static EDBEC_compose_mode_t det_compose_mode
	(
	dcd_sample_handle_t *ph_el_sample,
	uint64_t			 pts
	)
{
	EDBEC_compose_mode_t frm_compose_mode = EL_FREE_COMPOSING_MODE;
	uint64_t			 el_sample_pts = 0;
	uint64_t			 stc_start = 0;
	uint64_t			 stc_delta = 0;

	switch (cfg_compose_mode)
	{
		case FULL_COMPOSING_MODE:
			stc_start = GetStc90KHzClk();
			while (TRUE)
			{
				*ph_el_sample = get_next_dcd_sample(EL_QID);
				if (*ph_el_sample)
				{
					el_sample_pts = get_dcd_sample_pts(*ph_el_sample);
					if (pts > el_sample_pts)
					{
						/* EL sample is late or BL sample was missing, remove EL sample */
						remove_dcd_sample(EL_QID, el_sample_pts);
					}
					else if (pts < el_sample_pts)
					{
						/* EL sample missing, will compose in EL_FREE mode */
						*ph_el_sample = NULL;
						break;
					}
					else
					{
						/* found an aligned EL sample */
						if (decoded_sample_valid(*ph_el_sample))
						{
							/* decoded EL sample is good, set to FULL mode */
							frm_compose_mode = FULL_COMPOSING_MODE;
						}
						else
						{
							/* EL sample is bad, remove EL sample */
							remove_dcd_sample(EL_QID, pts);
							*ph_el_sample = NULL;
						}
						break;
					}
				}
				else
				{
					stc_delta = GetStc90KHzClk() - (f_first_sample ? stc_start : last_compose_stc);
					if (stc_delta >= frm_period_90KHz)
						break;
					os_sleep_msec(ALIGN_POLLING_PERIOD);
				}
			}
			break;

		case EL_FREE_COMPOSING_MODE:
			*ph_el_sample = NULL;
			break;

		default:
			break;
	}

	return frm_compose_mode;
}




/*-----------------------------------------------------------------------------+
 | Program the hardware to compose
 +----------------------------------------------------------------------------*/

static void compose
	(
	uint64_t			   pts,
	dcd_sample_handle_t    bl_sample,
	dcd_sample_handle_t    el_sample,
	EDBEC_compose_mode_t   mode,
	char				  *p_rpu_md
	)
{
	/* load decoded sample buffer pointers into composer hardware */
	hal_composer_set_bl_buffer((intptr_t)get_dcd_sample_frm_buf(bl_sample));
	if (mode == FULL_COMPOSING_MODE)
	{
		hal_composer_set_el_buffer((intptr_t)get_dcd_sample_frm_buf(el_sample));
	}
	/* load associated PTS into composer hardware */
	hal_composer_set_pts(pts);
	/* load rpu parameters into composer hardware */
	if (hal_composer_hw_profile_read() == COMPOSER_HW_BASE_PROFILE)
	{
		hal_composer_ld_base_profile_param((rpu_ext_config_fixpt_base_t *)p_rpu_md);
	}
	else
	{
		hal_composer_ld_main_profile_param((rpu_ext_config_fixpt_main_t *)p_rpu_md);
	}

	/* tell the composer hardware to initiate composing sequence */
	last_compose_stc = GetStc90KHzClk();
	hal_composer_start((uint8_t)mode);
}




/*-----------------------------------------------------------------------------+
 | Composer thread entry function
 +----------------------------------------------------------------------------*/

static void composer_thread_entry(void * arg)
{
	FOREVER
	{
#ifdef SUPPORT_EDR_TV_MODE
		if (!f_hdmi_src)
		{
			none_hdmi_src_process(arg);
		}
		else
		{
			hdmi_src_process(arg);
		}
#else
		none_hdmi_src_process(arg);
#endif
	}
}



/*-----------------------------------------------------------------------------+
 | Initialize the EDBEC module
 +----------------------------------------------------------------------------*/

int EDBEC_initialize
    (
    EDBEC_app_mode_t  edbec_mode
    )
{
    int rc = 0;

    if (f_init)
    {
        return 0;
    }

#ifndef SUPPORT_EDR_TV_MODE
    edbec_mode = DV_STB_APP_MODE;
#endif
#ifndef SUPPORT_EDR_STB_MODE
    edbec_mode = DV_TV_APP_MODE;
#endif

    f_stop = TRUE;
    f_vdr_hw_proc_pending = FALSE;
    f_first_sample = TRUE;

    hal_composer_reset();

    /* create the kernel resources */
    h_run_sgnl = os_sem_create();
    h_rdy_sgnl = os_sem_create();
    h_cfg_lock = os_mutex_create();

    if (!h_run_sgnl || !h_rdy_sgnl || !h_cfg_lock)
    {
        if (h_run_sgnl)
            os_sem_delete(h_run_sgnl);
        if (h_rdy_sgnl)
            os_sem_delete(h_rdy_sgnl);
        if (h_cfg_lock)
            os_mutex_delete(h_cfg_lock);
        rc = -1;
        goto EDBEC_initialize_exit;
    }

    h_composer_thread = os_thread_create(composer_thread_entry, COMPOSER_THREAD_PRIO);
    if (!h_composer_thread)
    {
        os_sem_delete(h_run_sgnl);
        os_sem_delete(h_rdy_sgnl);
        os_mutex_delete(h_cfg_lock);
        rc = -1;
        goto EDBEC_initialize_exit;
    }

    if (os_install_intr(composer_hw_rdy_intr_handler, COMPOSER_INTR_LEVEL) != 0)
    {
        os_thread_delete(h_composer_thread);
        os_sem_delete(h_run_sgnl);
        os_sem_delete(h_rdy_sgnl);
        os_mutex_delete(h_cfg_lock);
        rc = -1;
        goto EDBEC_initialize_exit;
    }

    strcpy(str_lut_dir, "./");

    /* initialize DM configuration */
    h_dm = CreateDmFxp();
    DefaultDmCfg(&dm_cfg);

    /* initialize DM input signal format, it will be adjusted later with MDS */
    dm_cfg.frmPri.RowNum            = image_height;
    dm_cfg.frmPri.ColNum            = image_width;
    dm_cfg.frmPri.PxlDefIn.pxlClr   = PxlClrYuv;
    dm_cfg.frmPri.PxlDefIn.pxlChrm  = PxlChrm420;
    dm_cfg.frmPri.PxlDefIn.pxlWeav  = PxlWeavPlnr;
    /* initialize the DM output pixel format */
    dm_cfg.frmPri.PxlDefOut.pxlClr  = PxlClrYuv;
    dm_cfg.frmPri.PxlDefOut.pxlChrm = PxlChrm422;
    dm_cfg.frmPri.PxlDefOut.pxlWeav = PxlWeavUyVy;

    user_ms_weight = DEFAULT_USER_MS_WEIGHT;

    /* initialize default trim metadata */
    mds_ext.trim.TrimNum = 0;     /* no trim */
    /* [][0] for default value */
    mds_ext.trim.Trima[TrimTypeTMaxPq][0] = dm_cfg.srcSigEnv.MaxPq;
    mds_ext.trim.Trima[TrimTypeSlope][0] = (unsigned short)((1 - 0.5)*4096);
    mds_ext.trim.Trima[TrimTypeOffset][0] = (unsigned short)((0 + 0.5)*4096);
    mds_ext.trim.Trima[TrimTypePower][0] = (unsigned short)((1 - 0.5)*4096);
    mds_ext.trim.Trima[TrimTypeChromaWeight][0] = (unsigned short)((0 + 0.5)*4096);
    mds_ext.trim.Trima[TrimTypeSaturationGain][0] = (unsigned short)((1 - 0.5)*4096);
    mds_ext.trim.Trima[TrimTypeMsWeight][0] = (unsigned short)(dm_cfg.bldCtrl.MSWeight);

#ifdef SUPPORT_EDR_TV_MODE
    if (edbec_mode == DV_TV_APP_MODE)
    {
        strcpy(str_bw_normal_dm_3d_lut_file,  "GMLUT_100_R709_V65_RGB.bcms");

#ifdef VBUS_10_BITS
        /* disable the dither hardware block */
        hal_dither_enable();
#else
        /* disable the dither hardware block */
        hal_dither_disable();
#endif
        /* set DM source for TV application mode */
        f_hdmi_src = FALSE;
        /* enable display management (NORMAL MODE)*/
        display_management_normal_config();
    }
#endif
#ifdef SUPPORT_EDR_STB_MODE
    if (edbec_mode == DV_STB_APP_MODE)
    {
        /* turn off Graphic DM */
        f_graphic_on = FALSE;
        /* disable display management (BYPASS MODE)*/
        display_management_bypass_config();
    }
#endif
    app_mode = edbec_mode;

    cfg_compose_mode = FULL_COMPOSING_MODE;
    frm_period_90KHz = FRAME_PERIOD_DEFAULT;
    last_compose_stc = 0;

    f_init = TRUE;

EDBEC_initialize_exit:

    return rc;
}



/*-----------------------------------------------------------------------------+
 | Un-initialize the EDBEC module
 +----------------------------------------------------------------------------*/

void EDBEC_uninitialize(void)
{
    if (!f_init)
        return ;

    EDBEC_pause();
    hal_composer_intr_disable();
    os_thread_delete(h_composer_thread);
    os_sem_delete(h_run_sgnl);
    os_sem_delete(h_rdy_sgnl);
    os_mutex_delete(h_cfg_lock);

    DestroyDm(h_dm);
    h_dm = NULL;

    f_init = FALSE;
}



/*-----------------------------------------------------------------------------+
 | Start the EDBEC module
 +----------------------------------------------------------------------------*/

int EDBEC_start(void)
{
    if (!f_stop)
    {
        return 0;
    }

    f_first_sample = TRUE;
    last_compose_stc = 0;

    os_sem_post(h_run_sgnl);

    hal_composer_intr_enable();

    return 0;
}



/*-----------------------------------------------------------------------------+
 | Pause the EDBEC module
 +----------------------------------------------------------------------------*/

int EDBEC_pause(void)
{
    f_stop = TRUE;

    /* wait until the current frame composing is completed */
    while (f_vdr_hw_proc_pending)
    {
        os_sleep_msec(1);
    }
    hal_composer_intr_disable();

    return 0;
}



/*-----------------------------------------------------------------------------+
 | Reset the EDBEC module
 +----------------------------------------------------------------------------*/

int EDBEC_reset(void)
{
    hal_composer_reset();

    f_vdr_hw_proc_pending = FALSE;
    f_first_sample = TRUE;

    DefaultDmCfg(&dm_cfg);
    user_ms_weight = DEFAULT_USER_MS_WEIGHT;

    /* set DM source for TV application mode */
    f_hdmi_src = FALSE;

#ifdef SUPPORT_EDR_TV_MODE
    if (app_mode == DV_TV_APP_MODE)
    {
#ifdef VBUS_10_BITS
        /* disable the dither hardware block */
        hal_dither_enable();
#else
        /* disable the dither hardware block */
        hal_dither_disable();
#endif
        /* set DM source for TV application mode */
        f_hdmi_src = FALSE;

        /* enable display management (NORMAL MODE)*/
        display_management_normal_config();
    }
#endif
#ifdef SUPPORT_EDR_STB_MODE
    if (app_mode == DV_STB_APP_MODE)
    {
        /* turn off Graphic DM */
        f_graphic_on = FALSE;

        /* disable display management (BYPASS MODE)*/
        display_management_bypass_config();
    }
#endif

    cfg_compose_mode = FULL_COMPOSING_MODE;
    frm_period_90KHz = FRAME_PERIOD_DEFAULT;
    last_compose_stc = 0;

    return 0;
}



/*-----------------------------------------------------------------------------+
 | Set the EDBEC module parameter
 +----------------------------------------------------------------------------*/

int EDBEC_setParam(EDBEC_param_id_t  param_id, const void  *param_val, size_t  param_sz)
{
    int rc = -1;

    if (!f_init)
    {
        printf("ERROR: back-end control software is not initialized yet\n");
        return rc;
    }

    switch (param_id)
    {
        case EDBEC_HDMI_SRC_CTL_PARAM:
            if (app_mode != DV_TV_APP_MODE)
            {
                printf("ERROR: invalid param, must be in Dolby Vision TV mode\n");
                break;
            }

            if (param_sz == sizeof(uint32_t))
            {
                uint32_t  val;

                val = (*((uint32_t *)param_val)) ? TRUE : FALSE;
                if ((bool_t)val != f_hdmi_src)
                {
                    EDBEC_pause();
                    f_hdmi_src = (bool_t)val;
                    display_management_normal_config();
                    EDBEC_start();
                    rc = 0;
                }
            }
            else
            {
                printf("ERROR: invalid parameter size\n");
            }
            break;

        case EDBEC_COMPOSING_MODE_PARAM:
            if (param_sz == sizeof(EDBEC_compose_mode_t))
            {
                if ((*((EDBEC_compose_mode_t *)param_val)) < COMPOSING_MODE_MAX)
                {
                    cfg_compose_mode = *((EDBEC_compose_mode_t *)param_val);
                    rc = 0;
                }
                else
                {
                     printf("ERROR: invalid compose mode value (%d)\n",
                            (*((EDBEC_compose_mode_t *)param_val)));
                }
            }
            else
            {
                printf("ERROR: invalid parameter size (%d)\n", (int)param_sz);
            }
            break;

#ifdef SUPPORT_EDR_STB_MODE
        case EDBEC_GRAPHIC_CTL_PARAM:
            if (app_mode != DV_STB_APP_MODE)
            {
                printf("ERROR: invalid param, must be in Dolby Vision STB mode\n");
                break;
            }

            if (param_sz == sizeof(uint32_t))
            {
                uint32_t  val;

                val = (*((uint32_t *)param_val)) ? TRUE : FALSE;
                if ((bool_t)val != f_graphic_on)
                {
                    f_graphic_on = (bool_t)val;
                    if (f_graphic_on)
                    {
                        display_management_graphic_on();
                    }
                    else
                    {
                        display_management_graphic_off();
                    }
                    rc = 0;
                }
            }
            else
            {
                printf("ERROR: invalid parameter size\n");
            }
            break;
#endif

        case EDBEC_FRAME_RATE_PARAM:
            if (param_sz == sizeof(uint32_t))
            {
                if (((*((uint32_t *)param_val)) == 24) ||
                    ((*((uint32_t *)param_val)) == 50) )
                {
                    if (f_stop)
                    {
                        frm_period_90KHz = PTS_CLOCK_RATE/(*((uint32_t *)param_val));
                        rc = 0;
                    }
                    else
                    {
                        printf("ERROR: invalid state, EDBEC module is running\n");
                    }
                }
                else
                {
                    printf("ERROR: invalid parameter value\n");
                }
            }
            else
            {
                printf("ERROR: invalid parameter size\n");
            }
            break;

        case EDBEC_USER_MS_WEIGHT_PARAM:
            if (param_sz == sizeof(uint32_t))
            {
                if (((*((uint32_t *)param_val)) >= USER_MS_WEIGHT_MIN) &&
                    ((*((uint32_t *)param_val)) <= USER_MS_WEIGHT_MAX) )
                {
                    user_ms_weight = *((uint32_t *)param_val);
                }
                else
                {
                    printf("ERROR: invalid parameter value\n");
                }
            }
            else
            {
                printf("ERROR: invalid parameter size\n");
            }
            break;

        default:
            printf("ERROR: invalid parameter ID\n");
            break;
    }

    return rc;
}



/*-----------------------------------------------------------------------------+
 | Get the EDBEC module parameter
 +----------------------------------------------------------------------------*/

int EDBEC_getParam(EDBEC_param_id_t  param_id, void  *param_val,  size_t  *param_sz)
{
    int rc = -1;

    if (!f_init)
    {
        printf("ERROR: back-end control software is not initialized yet\n");
        return rc;
    }

    switch (param_id)
    {
        case EDBEC_HDMI_SRC_CTL_PARAM:
            if (*param_sz >= sizeof(uint32_t))
            {
                *((uint32_t *)param_val) = f_hdmi_src ? 1UL : 0;
                *param_sz = sizeof(uint32_t);
                rc = 0;
            }
            else
            {
                printf("ERROR: parameter size too small\n");
            }
            break;

        case EDBEC_COMPOSING_MODE_PARAM:
            if ((*param_sz) >= sizeof(EDBEC_compose_mode_t))
            {
                *((EDBEC_compose_mode_t *)param_val) = cfg_compose_mode;
                *param_sz = sizeof(EDBEC_compose_mode_t);
                rc = 0;
            }
            else
            {
                printf("ERROR: parameter size too small (%d)\n", (int)(*param_sz));
            }
            break;

#ifdef SUPPORT_EDR_STB_MODE
        case EDBEC_GRAPHIC_CTL_PARAM:
            if (app_mode != DV_STB_APP_MODE)
            {
                printf("ERROR: invalid param, must be in Dolby Vision STB mode\n");
                break;
            }

            if (*param_sz >= sizeof(uint32_t))
            {
                *((uint32_t *)param_val) = f_graphic_on ? 1UL : 0;
                *param_sz = sizeof(uint32_t);
                rc = 0;
            }
            else
            {
                printf("ERROR: parameter size too small\n");
            }
            break;
#endif

        case EDBEC_FRAME_RATE_PARAM:
            if (*param_sz >= sizeof(uint32_t))
            {
                *((uint32_t *)param_val) = (uint32_t)(PTS_CLOCK_RATE/frm_period_90KHz);
                *param_sz = sizeof(uint32_t);
                rc = 0;
            }
            else
            {
                printf("ERROR: parameter size too small\n");
            }
            break;

        case EDBEC_USER_MS_WEIGHT_PARAM:
            if (*param_sz >= sizeof(uint32_t))
            {
                *((uint32_t *)param_val) = user_ms_weight;
                *param_sz = sizeof(uint32_t);
                rc = 0;
            }
            else
            {
                printf("ERROR: parameter size too small\n");
            }

        default:
            printf("ERROR: invalid parameter ID\n");
            break;
    }

    return rc;
}
#endif
