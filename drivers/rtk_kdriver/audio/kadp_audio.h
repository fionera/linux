#ifndef _KADP_AUDIO_H_
#define _KADP_AUDIO_H_

#ifdef __cplusplus
extern "C" {
#endif

/************************************************************************
 *  Include files
 ************************************************************************/
#include "audio_inc.h"

/************************************************************************
 *  Definitions
 ************************************************************************/

#define AIN_VOL_0DB             0
#define AIN_VOL_STEP_MIN        1
#define AIN_VOL_STEP_1DB        2 /* 0.5dB per step */
#define AIN_MAX_VOL             ((30)*AIN_VOL_STEP_1DB)
#define AIN_MIN_VOL             (-70*AIN_VOL_STEP_1DB)

#ifndef KADP_STATUS_T
typedef enum
{
    KADP_OK                     =   0,
    KADP_ERROR                  =   -1,
    KADP_NOT_OK                 =   -1,
    KADP_PARAMETER_ERROR        =   -2,
    KADP_NOT_ENOUGH_RESOURCE    =   -3,
    KADP_NOT_SUPPORTED          =   -4,
    KADP_NOT_PERMITTED          =   -5,
    KADP_TIMEOUT                =   -6,
    KADP_NO_DATA_RECEIVED       =   -7,
    KADP_DN_BUF_OVERFLOW        =   -8,
    KADP_DLNA_NOT_CONNECTED     =   -9,
    KADP_UP_BUF_OVERFLOW        =   -10,
    KADP_FD_NOT_EXIST           =   -11,
} _KADP_STATUS_T;

#define KADP_STATUS_T _KADP_STATUS_T
#endif

typedef enum {
    KADP_AUDIO_CH_ID_L    = (0x1<<0),
    KADP_AUDIO_CH_ID_R    = (0x1<<1),
    KADP_AUDIO_CH_ID_LS   = (0x1<<2),
    KADP_AUDIO_CH_ID_RS   = (0x1<<3),
    KADP_AUDIO_CH_ID_C    = (0x1<<4),
    KADP_AUDIO_CH_ID_SW   = (0x1<<5),
    KADP_AUDIO_CH_ID_LSS  = (0x1<<6),
    KADP_AUDIO_CH_ID_RSS  = (0x1<<7),
    KADP_AUDIO_CH_ID_ALL = 0xFF
} KADP_AUDIO_CH_ID;

typedef enum  {
    KADP_AUDIO_DELAY_RAW,
    KADP_AUDIO_DELAY_SPEAKER,
    KADP_AUDIO_DELAY_FIXVOL,
    KADP_AUDIO_DELAY_MAX
} KADP_AUDIO_DELAY_MODE;


typedef enum  {
	 KADP_AUDIO_SPDIFO_SRC_FIFO = 0x0,
	 KADP_AUDIO_SPDIFO_SRC_IN = 0x1,
	 KADP_AUDIO_SPDIFO_SRC_HDMI = 0x2,
	 KADP_AUDIO_SPDIFO_SRC_DISABLE = 0x3,
}KADP_AUDIO_SPDIFO_SOURCE;

// SPDIF-related --------------------//
// copy from kadp_audio_hw.h
typedef enum {
    // identical to the order in register's field
    AIO_SPDIFI_IN       = 0x0,
    AIO_SPDIFI_HDMI     = 0x1,
    AIO_SPDIFI_LOOPBACK = 0x2,
    AIO_SPDIFI_DISABLE  = 0x3
} AIO_SPDIFI_SRC;

typedef union {
	struct {
		unsigned char      volume: 7;
		unsigned char      bMute: 1;
	} OSD;
	UINT8  bits;
} KADP_AUDIO_SOUNDBAR_OSD_VOL;

typedef union {
	struct {
		unsigned char    woofer_level: 4;
		unsigned char    auto_vol: 4;
	} OSD;
	UINT8  bits;
} KADP_AUDIO_SOUNDBAR_WOOFER_VOL;

typedef struct {
	UINT32                                  res_0;         //31:0
	UINT8                                   res_1;         //35:32
	UINT32                                  barId;         //59:36
	KADP_AUDIO_SOUNDBAR_OSD_VOL             OSD_Vol;       //67:60
	UINT8                                   command;       //75:68
	KADP_AUDIO_SOUNDBAR_WOOFER_VOL          woofer_Vol;    //83:76
	UINT16                                  reserved_data; //99:84
	UINT8                                   checksum;      //107:100
	UINT32                                  res_2;         //127:108
	UINT32                                  res_3;         //159:128
	UINT32                                  res_4;         //191:160
} KADP_AUDIO_SPDIF_CHANNEL_STATUS_SOUNDBAR;

typedef struct
{
    unsigned int professional;
    unsigned int copyright;
    unsigned int category_code;
    unsigned int channel_number;
    unsigned int source_number;
    unsigned int sampling_freq;
    unsigned int clock_accuracy;
    unsigned int word_length;
    unsigned int original_sampling_freq;
    unsigned int cgms_a;
    unsigned int mode;
    unsigned int pre_emphasis;
    unsigned int data_type;
} KADP_AO_SPDIF_CHANNEL_STATUS_BASIC;

/************************************************************************
*  Function Declaration
 ************************************************************************/
KADP_STATUS_T KADP_AUDIO_Init(void);
KADP_STATUS_T KADP_AUDIO_Finalize(void);

KADP_STATUS_T KADP_AUDIO_SB_SetChannelStatus(KADP_AUDIO_SPDIF_CHANNEL_STATUS_SOUNDBAR pCS);
KADP_STATUS_T KADP_AUDIO_SB_GetChannelStatus(KADP_AUDIO_SPDIF_CHANNEL_STATUS_SOUNDBAR *pCS);
KADP_STATUS_T KADP_AUDIO_GetAMixerRunningStatus(UINT32* status);

KADP_STATUS_T KADP_AUDIO_SetBassBack(AUDIO_RPC_TV_BASSBACK* info);
KADP_STATUS_T KADP_AUDIO_SetBassBackGain(AUDIO_RPC_TV_BASSBACK_CHANGE_BASS_GAIN *info);
KADP_STATUS_T KADP_AUDIO_SetBassBack_PCBU(AUDIO_RPC_PCBU_BASSBACK *info);

KADP_STATUS_T KADP_AUDIO_SwpSetSRS_TrusurroundHD(AUDIO_RPC_TSXT* pTSXT);

/* KADP for RPC */
KADP_STATUS_T KADP_AUDIO_PrivateInfo(struct AUDIO_RPC_PRIVATEINFO_PARAMETERS *parameter, AUDIO_RPC_PRIVATEINFO_RETURNVAL *ret);
KADP_STATUS_T KADP_AUDIO_SendConfig(struct AUDIO_CONFIG_COMMAND *audioConfig);
KADP_STATUS_T KADP_AUDIO_GetAudioSpdifChannelStatus(KADP_AO_SPDIF_CHANNEL_STATUS_BASIC *cs, AUDIO_MODULE_TYPE type);
KADP_STATUS_T KADP_AUDIO_SetAudioOptChannelSwap(AUDIO_AO_CHANNEL_OUT_SWAP sel);
KADP_STATUS_T KADP_AUDIO_PlaySoundEvent(AUDIO_SOUND_EVENT *event);
KADP_STATUS_T KADP_AUDIO_IsFinishPlayAudioTone(void);
KADP_STATUS_T KADP_AUDIO_SetAudioSpectrumData(AUDIO_SPECTRUM_CFG *config);
KADP_STATUS_T KADP_AUDIO_SetAudioAuthorityKey(long customer_key);
KADP_STATUS_T KADP_AUDIO_SetAVSyncOffset(AUDIO_HDMI_OUT_VSDB_DATA *info);
KADP_STATUS_T KADP_AUDIO_InitRingBufferHeader(AUDIO_RPC_RINGBUFFER_HEADER *header);
KADP_STATUS_T KADP_AUDIO_SetSpdifOutPinSrc(KADP_AUDIO_SPDIFO_SOURCE src);

KADP_STATUS_T KADP_AUDIO_SetTruSrndXParam(AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO *param, int index);
KADP_STATUS_T KADP_AUDIO_GetTruSrndXParam(AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO *param, int index);
KADP_STATUS_T KADP_AUDIO_SetTbhdXParam(AUDIO_VIRTUALX_TBHDX_PARAM_INFO *param);
KADP_STATUS_T KADP_AUDIO_GetTbhdXParam(AUDIO_VIRTUALX_TBHDX_PARAM_INFO *param);
KADP_STATUS_T KADP_AUDIO_SetMbhlParam(AUDIO_VIRTUALX_MBHL_PARAM_INFO *param);
KADP_STATUS_T KADP_AUDIO_GetMbhlParam(AUDIO_VIRTUALX_MBHL_PARAM_INFO *param);

// SDEC set 0/1 as ATP0/ATP1
KADP_STATUS_T KADP_AUDIO_SetTrickState(UINT32 index);
// ADEC need to lookup ATP index when using following APIs
KADP_STATUS_T KADP_AUDIO_GetTrickState(UINT32 atp_index, char* ret);
KADP_STATUS_T KADP_AUDIO_ResetTrickState(UINT32 atp_index);

KADP_STATUS_T ShowDSPAllocSummary(void);
KADP_STATUS_T ShowUsrAllocSummary(void);
#ifdef __cplusplus
}
#endif
#endif  /* _KADP_AUDIO_H_ */



