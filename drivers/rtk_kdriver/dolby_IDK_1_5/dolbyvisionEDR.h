#ifndef _DOLBYVISION_EDR_DRIVER_H_
#define _DOLBYVISION_EDR_DRIVER_H_
#include <InbandAPI.h>

//RTK MARK
#ifdef RTK_EDBEC_CONTROL
#include <linux/cdev.h>
#include "dolbyvisionEDR_export.h"
#include "dolby_flowCtrl.h"
#endif

#include "rpu_ext_config.h"

#define DOLBY_SW_VERSION "0.9.0"

typedef enum MdsChg_t_ { MDS_CHG_NONE, MDS_CHG_TC, MDS_CHG_CFG } MdsChg_t;


typedef struct {
    int initialized;
//RTK MARK
#ifdef RTK_EDBEC_CONTROL
	struct cdev      cdev ;         /* Char device structure          */
	struct device *dev;
#endif
	int reserved;
} DOLBYVISIONEDR_DEV;

//RTK MARK
#ifdef RTK_EDBEC_CONTROL



#define VRPC_PTS2_FLAG_IDR_FRAME              (0x00010000)

// color mode for different PQ setting
typedef enum e_dv_picture_mode_ {
	DV_PIC_DARK_MODE = 0,
	DV_PIC_BRIGHT_MODE,
	DV_PIC_VIVID_MODE,
	DV_PIC_NONE_MODE,
} E_DV_PICTURE_MODE;

// test api & structure
typedef enum e_dv_mode_ {
	DV_DISABLE_MODE = 0x0,
	DV_HDMI_MODE = 0x1,
	DV_DM_CRF_422_MODE = 0x4,
	DV_DM_CRF_420_MODE = 0x5,
	DV_NORMAL_MODE = 0x2,
	DV_COMPOSER_CRF_MODE = 0x6,
} E_DV_MODE;

typedef struct {

	int pxlBdp;	/* output pixel definition as a bit field */
	int RowNumTtl;
	int ColNumTtl;

	int BL_EL_ratio;	// for normal mode; 0=> 1:1;  1=> 4:1

	char file1Inpath[100/*500*/];
	char file2Inpath[100/*500*/];
	char fileOutpath[100/*500*/];
	char fileMdInpath[100/*500*/];

	unsigned long file1InSize;
	unsigned long file2InSize;
	unsigned long fileMdInSize;

	int frameNo;

	E_DV_MODE dolby_mode;
	char caseNum[10];

} DOLBYVISION_PATTERN;


typedef enum DV_SUPPORT_ITEM {
	DV_PROFILE_DVAV_PER = 0x1,
	DV_PROFILE_DVAV_PEN = 0x2,
	DV_PROFILE_DVHE_DER = 0x4,
	DV_PROFILE_DVHE_DEN = 0x8,
	DV_PROFILE_DVHE_DTR = 0x10,
	DV_PROFILE_DVHE_STN = 0x20,
	DV_PROFILE_DVHE_DTH = 0x40,
	DV_PROFILE_DVHE_DTB = 0x80,
	DV_PROFILE_DVHE_ST = 0x100,
	DV_PROFILE_DVAE_SE = 0x200,
	DV_PROFILE_DVHE_DTR_MEL = 0x00100000,
} DOLBYVISION_SUPPORT;

// API proto-type
loff_t dolbyvisionEDR_llseek(struct file *filp, loff_t off, int whence);
ssize_t dolbyvisionEDR_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
ssize_t dolbyvisionEDR_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
int dolbyvisionEDR_open(struct inode *inode, struct file *filp);
int dolbyvisionEDR_release(struct inode *inode, struct file *filp);
int dolbyvisionEDR_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
void HDMI_In_TestFlow(DOLBYVISION_PATTERN *dolby);
void Normal_In_TestFlow(DOLBYVISION_PATTERN *dolby);
void DM_CRF_420_TestFlow(DOLBYVISION_PATTERN *dolby);
void DM_CRF_422_TestFlow(DOLBYVISION_PATTERN *dolby);
void Composer_CRF_TestFlow(DOLBYVISION_PATTERN *dolby);
void DM_CRF_Dump_TestFlow(DOLBYVISION_PATTERN *dolby);
void MetaData_WriteBack_Check(void);
void MetaData_WriteBack_file(char *name,unsigned char *buffer,int size,int end);
void Rpu_Md_WriteBack_Check(unsigned int addr, int size);
int GMLUT_3DLUT_Update(char *path, uint32_t foffset, int type, E_DV_PICTURE_MODE uPicMode);
int GMLUT_Pq2GLut_Update(char *path, uint32_t foffset, uint32_t size, E_DV_PICTURE_MODE uPicMode);
int Target_Display_Parameters_Update(char *path, uint32_t foffset);
void Backlight_DelayTiming_Update(char *path, uint32_t foffset);
void Backlight_LUT_Update(char *path, uint32_t foffset, E_DV_PICTURE_MODE uPicMode);
int DolbyPQ_1Model_1File_Config(char *picModeFilePath, E_DV_PICTURE_MODE uPicMode);
unsigned char check_need_to_reload_config(void);//This is for debug. If yes, reload config
void DM_B05_Set(uint16_t *lutMap, uint32_t forceUpdate);
void Load_RpuMdTest(DOLBYVISION_PATTERN *dolby);
void Normal_TEST(DOLBYVISION_MD_OUTPUT *p_mdOutput, unsigned int rpcType);
void HDMI_TEST(unsigned int wid, unsigned int len, unsigned int mdAddr);
void hdr10_to_md_parser(void *phdr10_to_md,int len);
#ifdef CONFIG_SUPPORT_SCALER
extern int create_timer(unsigned char id, unsigned int interval, unsigned char mode);
extern int rtk_timer_control(unsigned char id, unsigned int cmd);
#endif

#endif

typedef enum _e_md_parser_state_ {
	MD_PARSER_INIT = 0,
	MD_PARSER_RUN,
	MD_PARSER_EOS
} MD_PARSER_STATE;


typedef enum _e_dolby_src_ {
	DOLBY_SRC_OTT = 0,
	DOLBY_SRC_HDMI
} DOLBY_SRC;

typedef enum {
	OTT_STATE_POSITION = 0x1abc,//apply position
	OTT_STATE_FINISH = 0x2abc,//apply finish
	OTT_STATE_WHOLE = 0x2379,//apply position and finish.
} DOLBY_STATE;

// API proto-type
int metadata_parser_main(unsigned char *mdInPtr, unsigned int fileMdInSize, MD_PARSER_STATE mdparser_state);
int dm_metadata_2_dm_param(dm_metadata_base_t *p_dm_md, MdsExt_t *p_mds_ext);
extern MdsChg_t CheckMds(const MdsExt_t *mdsExt, const MdsExt_t *mdsExtRef);
extern void InitGdCtrlFromConfig(GdCtrlFxp_t *pGdCtrl , TgtGDCfg_t *pCfg);

// variables
extern DOLBYVISIONEDR_DEV *dolbyvisionEDR_devices;
extern MdsExt_t mds_ext;
extern cp_context_t *OTT_h_dm;/*This is for OTT*/
extern cp_context_t *hdmi_h_dm;/*This is for HDMI*/
extern cp_context_t_half_st *hdmi_h_dm_st;
extern DmCfgFxp_t dm_bright_cfg; //Bright pic mode      /* display management configuration */		// modified by smfan
extern DmCfgFxp_t dm_dark_cfg; //dark pic mode      /* display management configuration */		// modified by smfan
extern DmCfgFxp_t dm_vivid_cfg; //vivid pic mode      /* display management configuration */		// modified by smfan

extern DmCfgFxp_t dm_hdmi_dark_cfg;	   /* display management configuration hdmi dark mode*/		// modified by smfan
extern DmCfgFxp_t dm_hdmi_bright_cfg;	   /* display management configuration hdmi bright mode*/		// modified by smfan
extern DmCfgFxp_t dm_hdmi_vivid_cfg;	   /* display management configuration hdmi vivid mode*/		// modified by smfan
extern DmCfgFxp_t *p_dm_hdmi_cfg; //Point to PIC mode hdmi cfg

extern DmCfgFxp_t dm_bright_cfg; //Bright pic mode      /* display management configuration */		// modified by smfan
extern DmCfgFxp_t dm_dark_cfg; //dark pic mode      /* display management configuration */		// modified by smfan
extern DmCfgFxp_t dm_vivid_cfg; //vivid pic mode      /* display management configuration */		// modified by smfan
extern DmCfgFxp_t *p_dm_cfg; //Point to PIC mode cfg

//extern TgtGDCfg_t g_TgtGDCfg;
extern char *g_PicModeFilePath[3];

/****** extern global variable  ******/

/* rpu MD size per frame */
extern volatile unsigned int g_rpu_size, g_more_eos_err;
extern int g_mdparser_output_type;
extern unsigned int g_dvframeCnt;

/* buffer for MD parser */
extern unsigned int gRpuMDAddr;
extern unsigned int gRpuMDCacheAddr;
extern volatile unsigned int g_forceUpdate_3DLUT;
extern volatile unsigned int g_forceUpdateFirstTime;
extern volatile unsigned int g_picModeUpdateFlag;
extern volatile unsigned int HDMI_Parser_NeedUpdate;
extern volatile unsigned int OTT_Parser_NeedUpdate;//When IDR frame and PIC mode or backlight change to TRUE
extern struct file *gFrame1Outfp;	/* output for HDMI or Y-block */

#ifndef WIN32
extern E_DV_PICTURE_MODE ui_dv_picmode;
extern E_DV_PICTURE_MODE current_dv_picmode ;//Current apply pic mode
extern unsigned char ui_dv_backlight_value;//Record UI backlight value
extern unsigned char current_dv_backlight_value;//Current apply backlight value
extern E_DV_PICTURE_MODE HDMI_last_md_picmode;
extern unsigned char HDMI_last_md_backlight;

extern volatile DOLBYVISION_FLOW_STRUCT *g_MdFlow;
extern PTS_INFO2 g_pts_info;
#endif

/* MD parser state control */
extern MD_PARSER_STATE g_MD_parser_state;

extern unsigned int g_dmMDAddr, g_compMDAddr;
extern unsigned int g_recordDMAddr, g_dvframeCnt;

/* record Lv4 exist in video stream */
extern unsigned int g_lvl4_exist;

#ifdef IDK_CRF_USE
extern unsigned int g_forceB05Update_IDK;
#endif

#define HDMI_DOLBY_UI_CHANGE_DELAY_APPLY 10

#endif		/* end of _DOLBYVISION_EDR_DRIVER_H_ */
