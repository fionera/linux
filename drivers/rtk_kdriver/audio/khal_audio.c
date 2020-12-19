#include <linux/string.h>
#include <linux/unistd.h>
#include <linux/delay.h>

/* khal audio */
#include <hal_common.h>
#include <hal_audio.h>

#include "hresult.h"
#include "audio_flow.h"
#include "audio_process.h"
#include <linux/slab.h>
#include <linux/kthread.h>
#include "audio_rpc.h"
#include "audio_inc.h"
#include "kadp_audio.h"
#include "rtk_kdriver/rtkaudio_debug.h"
#include "audio_base.h"
#include "resample_coef.h"
#include "rbus/audio_reg.h"

/* external driver */
#include "rdvb/rdvb-dmx/rdvb_dmx_ctrl.h"
#include "audio_hw/audio_hw_khal.h"

#include "tvscalercontrol/hdmirx/hdmifun.h"
/************************************************************************
 *  Config Definitions
 ************************************************************************/

/* use for avoid pop noise when adjust volume */
#define AVOID_USELESS_RPC

// auto mute decoder when no output
#define DEC_AUTO_MUTE

// auto mute decoder when no output
#define AMIXER_AUTO_MUTE

#define SEARCH_PAPB_WHEN_STOP
#if !defined(SEARCH_PAPB_WHEN_STOP)
#define START_DECODING_WHEN_GET_FORMAT
#endif

/* enable LGSE detail debug message */
//#define LGSE_PRINT

/* End of Config Definitions
 ************************************************************************/

// according to  layout / LG setting

#define  AUDIO_BBADC_SRC_AIN1_PORT_NUM  (0)
#define  AUDIO_BBADC_SRC_AIN2_PORT_NUM  (1)
#define  AUDIO_BBADC_SRC_AIN3_PORT_NUM  (2)
#define  AUDIO_BBADC_SRC_AIO_PORT_NUM   (5)


#define TV006_SOUNDBAR_ID (0xf048a6)
//=========================================================
// ALL module

static bool ipt_Is_HDMI = FALSE;
#ifdef START_DECODING_WHEN_GET_FORMAT
static bool At_Stop_To_Start_Decode = FALSE;
#endif

extern long rtkaudio_send_audio_config(AUDIO_CONFIG_COMMAND_RTKAUDIO * cmd);
int Aud_initial = 0;
BOOLEAN b_get_sdec_buffer[2] = {0};

BOOLEAN IsAudioInitial(void);
#define AUDIO_HAL_CHECK_INITIAL_OK IsAudioInitial
DTV_STATUS_T RHAL_AUDIO_AENC_Start(HAL_AUDIO_AENC_INDEX_T aencIndex, HAL_AUDIO_AENC_ENCODING_FORMAT_T audioType);
DTV_STATUS_T RHAL_AUDIO_AENC_Stop(HAL_AUDIO_AENC_INDEX_T aencIndex);
DTV_STATUS_T RHAL_AUDIO_AENC_SetInfo(HAL_AUDIO_AENC_INDEX_T aencIndex, HAL_AUDIO_AENC_INFO_T info);

static void GetConnectOutputSource(HAL_AUDIO_RESOURCE_T resID,
                                   HAL_AUDIO_RESOURCE_T* outputConnectResId,
                                   int numOutputConnectArray,
                                   int* totalOutputConnectResource);
DTV_STATUS_T RHAL_AUDIO_StartDecoding(HAL_AUDIO_ADEC_INDEX_T adecIndex, HAL_AUDIO_SRC_TYPE_T audioType, int force2Restart, int autostart);
DTV_STATUS_T RHAL_AUDIO_StopDecoding(HAL_AUDIO_ADEC_INDEX_T adecIndex, int autostop);
static BOOLEAN IsDecoderConnectedToEncoder(HAL_AUDIO_ADEC_INDEX_T adecIndex);
DTV_STATUS_T RHAL_AUDIO_SetDolbyDRCMode(HAL_AUDIO_ADEC_INDEX_T adecIndex, adec_dolbydrc_mode_ext_type_t drcMode);
DTV_STATUS_T RHAL_AUDIO_SetDownMixMode(HAL_AUDIO_ADEC_INDEX_T adecIndex, adec_downmix_mode_ext_type_t downmixMode);
DTV_STATUS_T RHAL_AUDIO_SB_SetOpticalIDData(HAL_AUDIO_SB_SET_INFO_T info, unsigned int *ret_checksum);

#if !defined(SEARCH_PAPB_WHEN_STOP)
static DTV_STATUS_T HdmiStartDecoding(int adecIndex);
static DTV_STATUS_T HdmiStopDecoding(int adecIndex);
#endif

#define HAL_MAX_OUTPUT (10)
#define HAL_MAX_RESOURCE_NUM (HAL_AUDIO_RESOURCE_OUT_WISA + 1) /* max resource in hal_audio.h */
#define HAL_DEC_MAX_OUTPUT_NUM (7) // dec maybe connect with spdif, spdif_ES, HP, SE(speak), scart, SB_PCM(BT), SB_CANVAS

//#define VIRTUAL_ADEC_ENABLED /* https://harmony.lge.com:8443/issue/browse/KTASKWBS-7776 request to disable it */
#if defined(VIRTUAL_ADEC_ENABLED)
#define AUD_ADEC_MAX (HAL_AUDIO_ADEC_MAX + 1)
#else
#define AUD_ADEC_MAX (HAL_AUDIO_ADEC1 + 1)
#endif
#define AUD_AMIX_MAX (HAL_AUDIO_MIXER_MAX + 1)
#define AUD_AENC_MAX (HAL_AUDIO_AENC0 + 1) /* Currently only support 1 encoder */

#define AUD_MAX_CHANNEL (8)
#define AUD_MAX_ADEC_DELAY   (250) /* ms */
#define AUD_MAX_SNDOUT_DELAY (400) /* ms */
/*

 open <->  connect    <->      run <-> pause
 ^          ^                   ^
 |          |                   |

 close <-> disconnect  <->     stop <-> pause

*/
typedef  enum
{
    HAL_AUDIO_RESOURCE_CLOSE            = 0,
    HAL_AUDIO_RESOURCE_OPEN             = 1,
    HAL_AUDIO_RESOURCE_CONNECT          = 2,
    HAL_AUDIO_RESOURCE_DISCONNECT       = 3,
    HAL_AUDIO_RESOURCE_STOP             = 4,
    HAL_AUDIO_RESOURCE_PAUSE            = 5,
    HAL_AUDIO_RESOURCE_RUN              = 6,
    HAL_AUDIO_RESOURCE_STATUS_UNKNOWN   = 7,
} HAL_AUDIO_RESOURCE_STATUS;

pfnAdecoderClipDone Aud_ClipDecoderCallBack = NULL;
pfnAmixerClipDone Aud_ClipAmixerCallBack[AUD_AMIX_MAX] = {NULL};

// All resource
char *ResourceName[] = {
    "SDEC0",
    "SDEC1",
    "ATP0",
    "ATP1",
    "ADC",
    "HDMI",
    "AAD",
    "SYSTEM",
    "ADEC0",
    "ADEC1",
    "AENC0",
    "AENC1",
    "SE",
    "SPK",
    "SPDIF",
    "SB_SPDIF",
    "SB_PCM",
    "SB_CANVAS",
    "HP",
    "ARC",
    "AMIXER",
    "MIXER1",
    "MIXER2",
    "MIXER3",
    "MIXER4",
    "MIXER5",
    "MIXER6",
    "MIXER7",
    "SPDIF_ES",
    "HDMI0",
    "HDMI1",
    "HDMI2",
    "HDMI3",
    "SWITCH",
    "ADEC2",
    "ADEC3",
    "WISA",
    "UNKNOWN",// unknown
};

char* ResourceStatusSrting[] = {
    "CLOSE",
    "OPEN",
    "CONNECT",
    "DISCONNECT",
    "STOP",
    "PAUSE",
    "RUN",
    "UNKNOWN",// unknown
};

typedef struct
{
    char name[12];
    HAL_AUDIO_RESOURCE_STATUS connectStatus[HAL_MAX_OUTPUT];
    HAL_AUDIO_RESOURCE_T inputConnectResourceId[HAL_MAX_OUTPUT]; //HAL_AUDIO_RESOURCE_T
    HAL_AUDIO_RESOURCE_T outputConnectResourceID[HAL_MAX_OUTPUT];//HAL_AUDIO_RESOURCE_T
    int numOptconnect;
    int numIptconnect;
    int maxVaildIptNum; // output moduel used
} HAL_AUDIO_MODULE_STATUS;

#define AUDIO_HAL_CHECK_PLAY_NOTAVAILABLE(res, inputID) \
    ((res.connectStatus[inputID] != HAL_AUDIO_RESOURCE_CONNECT) && \
     (res.connectStatus[inputID] != HAL_AUDIO_RESOURCE_STOP))

#define AUDIO_HAL_CHECK_STOP_NOTAVAILABLE(res, inputID) \
    ((res.connectStatus[inputID] != HAL_AUDIO_RESOURCE_RUN) && \
     (res.connectStatus[inputID] != HAL_AUDIO_RESOURCE_PAUSE))

#define AUDIO_HAL_CHECK_PAUSE_NOTAVAILABLE(res, inputID) \
    (res.connectStatus[inputID] != HAL_AUDIO_RESOURCE_RUN)

#define AUDIO_HAL_CHECK_RESUME_NOTAVAILABLE(res, inputID) \
    (res.connectStatus[inputID] != HAL_AUDIO_RESOURCE_PAUSE)

#define AUDIO_HAL_CHECK_ISATRUNSTATE(res, inputID) \
    ((res.connectStatus[inputID] == HAL_AUDIO_RESOURCE_RUN) || \
    (res.connectStatus[inputID] == HAL_AUDIO_RESOURCE_PAUSE))

#define AUDIO_HAL_CHECK_ISATSTOPSTATE(res, inputID) \
    ((res.connectStatus[inputID] == HAL_AUDIO_RESOURCE_CONNECT) || \
    (res.connectStatus[inputID] == HAL_AUDIO_RESOURCE_STOP))

#define AUDIO_HAL_CHECK_ISATPAUSESTATE(res, inputID) \
    ((res.connectStatus[inputID] == HAL_AUDIO_RESOURCE_PAUSE))

// Flow
typedef struct
{
    unsigned char IsAINExist;
    int AinConnectDecIndex;
    unsigned char IsSubAINExist;
    int subAinConnectDecIndex;
    unsigned char IsDEC0Exist;
    unsigned char IsDEC1Exist;
    unsigned char IsDEC2Exist;
    unsigned char IsDEC3Exist;
    unsigned char IsEncExist;
    unsigned char IsScartOutBypas;
    int ScartConnectDecIndex;
    int mainDecIndex;
    unsigned char IsSystemOutput0;
    unsigned char IsSystemOutput1;
    unsigned char IsSystemOutput2;
    unsigned char IsSystemOutput3;
    unsigned char IsDTV0SourceRead; // ATP
    unsigned char IsDTV1SourceRead;
    unsigned char IsMainPPAOExist;
    unsigned char IsSubPPAOExist;
} HAL_AUDIO_FLOW_STATUS;

//=========================================================
//  DEC
typedef struct
{
    UINT32 decMute;
    UINT32 decDelay;
    UINT32 decVol[AUD_MAX_CHANNEL];
    UINT32 decFine[AUD_MAX_CHANNEL];
} ADSP_DEC_INFO;

typedef struct
{
    BOOLEAN spk_mute;
    BOOLEAN spdif_mute;
    BOOLEAN spdifes_mute;
    BOOLEAN hp_mute;
    BOOLEAN scart_mute;
    UINT32 spk_delay;
    UINT32 spdif_delay;
    UINT32 spdifes_delay;
    UINT32 hp_delay;
    UINT32 scart_delay;
} ADSP_SNDOUT_INFO;

typedef struct
{
    int spdifouttype;
    int trickPauseState;
    BOOLEAN trickPauseEnable;
    BOOLEAN spdifESMute;
    BOOLEAN decInMute;
    BOOLEAN sysmemoryExist;
    BOOLEAN userSetRun; // internal will auto stop flow, use this flag to store user setting.
    HAL_AUDIO_VOLUME_T decInVolume;
    HAL_AUDIO_VOLUME_T decOutVolume[AUD_MAX_CHANNEL];
    HAL_AUDIO_DOLBY_DRC_MODE_T drcMode;
    HAL_AUDIO_ES_MODE_T espushMode;
    HAL_AUDIO_DOWNMIX_MODE_T downmixMode;
} HAL_AUDIO_DEC_MODULE_STATUS;

ADSP_SNDOUT_INFO adsp_sndout_info;

BOOLEAN isAutoSetMainAdec = FALSE;
BOOLEAN m_IsSetMainDecOptByWebOs = FALSE;

int Aud_mainDecIndex     = 0;
int Aud_prevMainDecIndex = -1;
int Aud_descriptionMode  = 0;

BOOLEAN m_IsHbbTV = FALSE;
HAL_AUDIO_INDEX_T Aud_mainAudioIndex = HAL_AUDIO_INDEX0;

HAL_AUDIO_FLOW_STATUS flowStatus[AUD_ADEC_MAX];
HAL_AUDIO_ADEC_INFO_T adec_info[AUD_ADEC_MAX];
HAL_AUDIO_ADEC_STATUS_T adec_status[AUD_ADEC_MAX];

HAL_AUDIO_DEC_MODULE_STATUS Aud_decstatus[AUD_ADEC_MAX];
HAL_AUDIO_MODULE_STATUS ResourceStatus[HAL_MAX_RESOURCE_NUM];

int gst_owner_process[AUD_ADEC_MAX];

static ADSP_DEC_INFO gAudioDecInfo[AUD_ADEC_MAX] = {0};

static UINT32 Sndout_Devices = 0;

//=========================================================
// Ain
#define MAIN_AIN_ADC_PIN (9)
typedef struct
{
    int ainPinStatus[MAIN_AIN_ADC_PIN];
} HAL_AUDIO_AIN_MODULE_STATUS;
static HAL_AUDIO_AIN_MODULE_STATUS gAinStatus = {0};

#define AUDIO_CHECK_ADC_PIN_OPEN_NOTAVAILABLE(status)       ((status != HAL_AUDIO_RESOURCE_CLOSE) && ( status != HAL_AUDIO_RESOURCE_OPEN))
#define AUDIO_CHECK_ADC_PIN_CLOSE_NOTAVAILABLE(status)      ((status != HAL_AUDIO_RESOURCE_OPEN) && (status != HAL_AUDIO_RESOURCE_DISCONNECT) && (status != HAL_AUDIO_RESOURCE_OPEN))
#define AUDIO_CHECK_ADC_PIN_CONNECT_NOTAVAILABLE(status)    ((status != HAL_AUDIO_RESOURCE_OPEN) && (status != HAL_AUDIO_RESOURCE_DISCONNECT))
#define AUDIO_CHECK_ADC_PIN_DISCONNECT_NOTAVAILABLE(status) (status != HAL_AUDIO_RESOURCE_CONNECT)

//=========================================================
//  AMIX
// AMIX no open close api

#define AUDIO_HAL_CHECK_AMIX_PLAY_NOTAVAILABLE(res) \
    ((res.connectStatus[0] != HAL_AUDIO_RESOURCE_CONNECT) && \
     (res.connectStatus[0] != HAL_AUDIO_RESOURCE_STOP))

#define AUDIO_HAL_CHECK_AMIX_STOP_NOTAVAILABLE(res) \
    ((res.connectStatus[0] != HAL_AUDIO_RESOURCE_RUN) && \
     (res.connectStatus[0] != HAL_AUDIO_RESOURCE_PAUSE))

#define AUDIO_HAL_CHECK_AMIX_PAUSE_NOTAVAILABLE(res) \
    (res.connectStatus[0] != HAL_AUDIO_RESOURCE_RUN)

#define AUDIO_HAL_CHECK_AMIX_RESUME_NOTAVAILABLE(res) \
    (res.connectStatus[0] != HAL_AUDIO_RESOURCE_PAUSE)

DTV_STATUS_T RHAL_AUDIO_AMIX_Connect(HAL_AUDIO_RESOURCE_T currentConnect);
DTV_STATUS_T RHAL_AUDIO_AMIX_Disconnect(HAL_AUDIO_RESOURCE_T currentConnect);

//=========================================================

//--- spdif out channel status setting ---//
// copy from cppaout.h
#define HAL_SPDIF_CONSUMER_USE          0x0
#define HAL_SPDIF_PROFESSIONAL_USE      0x1

#define HAL_SPDIF_COPYRIGHT_NEVER       0x0
#define HAL_SPDIF_COPYRIGHT_FREE        0x1

#define HAL_SPDIF_CATEGORY_GENERAL      0x00
#define HAL_SPDIF_CATEGORY_EUROPE       0x0C
#define HAL_SPDIF_CATEGORY_USA          0x64
#define HAL_SPDIF_CATEGORY_JAPAN        0x04

#define HAL_SPDIF_CATEGORY_L_BIT_IS_1   0x80
#define HAL_SPDIF_CATEGORY_L_BIT_IS_0   0x00

#define HAL_SPDIF_WORD_LENGTH_NONE      0x0
#define HAL_SPDIF_WORD_LENGTH_16        0x2
#define HAL_SPDIF_WORD_LENGTH_18        0x4
#define HAL_SPDIF_WORD_LENGTH_19        0x8
#define HAL_SPDIF_WORD_LENGTH_20_0      0xA
#define HAL_SPDIF_WORD_LENGTH_17        0xC
#define HAL_SPDIF_WORD_LENGTH_20_1      0x3
#define HAL_SPDIF_WORD_LENGTH_22        0x5
#define HAL_SPDIF_WORD_LENGTH_23        0x9
#define HAL_SPDIF_WORD_LENGTH_24        0xB
#define HAL_SPDIF_WORD_LENGTH_21        0xD
//======================================================

#define ENC_DATA_SIZE (2048*4)
typedef struct
{
    UINT32                index;       // Encoder index
    HAL_AUDIO_AENC_INFO_T info;        // Encoder info
    Base*                 pEnc;
    Base*                 pMemOut;
    pfnAENCDataHandling   pfnCallBack; // Encoder Callback
    struct semaphore      callback_sem;
} RHAL_AUDIO_AENC_T;
static RHAL_AUDIO_AENC_T* Aud_aenc[AUD_AENC_MAX] = {NULL};

typedef struct {
    sndout_arc_copyprotection_ext_type_t curARCCopyInfo;
    UINT8                     curARCCategoryCode;
    UINT32                    curARCOutDelay;
    HAL_AUDIO_VOLUME_T        curARCOutVolume;
    BOOLEAN                   curARCMuteStatus;
    UINT32                    curARCOutGain;
    HAL_AUDIO_ARC_MODE_T      curAudioArcMode;
} RHAL_ARC_INFO_T;

static RHAL_ARC_INFO_T g_ARCStatusInfo;
static HAL_AUDIO_COMMON_INFO_T g_AudioStatusInfo;  //initial ???
static UINT32 _AudioARCMode   = ENABLE_DOWNMIX;
static UINT32 _AudioEARCMode  = ENABLE_DOWNMIX;
static UINT32 _AudioSPDIFMode = ENABLE_DOWNMIX;
static BOOLEAN g_update_by_ARC = FALSE;

HAL_AUDIO_VOLUME_T g_mixer_gain[AUD_AMIX_MAX] = {{.mainVol = 0x7F, .fineVol = 0x0}};
HAL_AUDIO_VOLUME_T g_mixer_out_gain = {.mainVol = 0x7F, .fineVol = 0x0}; // only one ?
HAL_AUDIO_VOLUME_T g_enc_volume[AUD_AENC_MAX] = {{.mainVol = 0x7F, .fineVol = 0x0}};

BOOLEAN g_mixer_user_mute[AUD_AMIX_MAX] = {FALSE};
BOOLEAN g_mixer_curr_mute[AUD_AMIX_MAX] = {FALSE};

#define DB2MIXGAIN_TABLE_SIZE (52)
const UINT32 dB2mixgain_table[DB2MIXGAIN_TABLE_SIZE] = {
    0x7FFFFFFF , 0x721482BF , 0x65AC8C2E , 0x5A9DF7AA , //  0  ~ -3dB
    0x50C335D3 , 0x47FACCF0 , 0x3FFFFFFF , 0x392CED8D , // -4  ~ -7dB
    0x32F52CFE , 0x2D6A866F , 0x287A26C4 , 0x241346F5 , // -8  ~ -11dB
    0x2026F30F , 0x1CA7D767 , 0x198A1357 , 0x16C310E3 , // -12 ~ -15dB
    0x144960C5 , 0x12149A5F , 0x101D3F2D , 0xE5CA14C  , // -16 ~ -19dB
    0xCCCCCCC  , 0xB687379  , 0xA2ADAD1  , 0x90FCBF7  , // -20 ~ -23dB
    0x8138561  , 0x732AE17  , 0x66A4A52  , 0x5B7B15A  , // -24 ~ -27dB
    0x518847F  , 0x48AA70B  , 0x40C3713  , 0x39B8718  , // -28 ~ -31dB
    0x337184E  , 0x2DD958A  , 0x28DCEBB  , 0x246B4E3  , // -32 ~ -35dB
    0x207567A  , 0x1CEDC3C  , 0x19C8651  , 0x16FA9BA  , // -36 ~ -39dB
    0x147AE14  , 0x1240B8C  , 0x1044914  , 0xE7FACB   , // -40 ~ -43dB
    0xCEC089   , 0xB8449B   , 0xA43AA1   , 0x925E89   , // -44 ~ -47dB
    0x8273A6   , 0x7443E7   , 0x679F1B   , 0x5C5A4F     // -48 ~ -51dB
};
#define ADEC_DSP_MIX_GAIN_MUTE (0x00000000)
#define ADEC_DSP_MIX_GAIN_0DB  (0x7FFFFFFF)
HAL_AUDIO_VOLUME_T currMixADVol = {.mainVol = 0x7F, .fineVol = 0x0};
HAL_AUDIO_VOLUME_T currDecADVol = {.mainVol = 0x7F, .fineVol = 0x0};

static Base* Aud_MainAin = NULL;
static Base* Aud_SubAin = NULL;
static Base* Aud_dec[AUD_ADEC_MAX] = {NULL};
static Base* Aud_ppAout = NULL;
static Base* Aud_subPPAout = NULL;
static Base* Aud_ESPlay[AUD_ADEC_MAX] = {NULL};
static Base* Aud_DTV[AUD_ADEC_MAX] = {NULL};

static FlowManager* Aud_flow[AUD_ADEC_MAX];

static char AUD_StringBuffer[128];

static char* GetResourceString(HAL_AUDIO_RESOURCE_T resId);
static char* GetResourceStatusString(int  statusId);
static UINT32 ADSP_SNDOut_SetMute(UINT32 dev_id,  BOOLEAN bMute);

static BOOLEAN SetConnectSourceAndStatus(HAL_AUDIO_MODULE_STATUS resourceStatus[HAL_MAX_RESOURCE_NUM],
                                      HAL_AUDIO_RESOURCE_T currentConnect,
                                      HAL_AUDIO_RESOURCE_T inputConnect);

char *SRCTypeName[] = {
    "UNKNOWN",     /* = 0  */
    "PCM",         /* = 1  */
    "AC3",         /* = 2  */
    "EAC3",        /* = 3  */
    "MPEG",        /* = 4  */
    "AAC",         /* = 5  */
    "HEAAC",       /* = 6  */
    "DRA",         /* = 7  */
    "MP3",         /* = 8  */
    "DTS",         /* = 9  */
    "SIF",         /* = 10 */
    "SIF_BTSC",    /* = 11 */
    "SIF_A2",      /* = 12 */
    "DEFAULT",     /* = 13 */
    "NONE",        /* = 14 */
    "DTS_HD_MA",   /* = 15 */
    "DTS_EXPRESS", /* = 16 */
    "DTS_CD",      /* = 17 */
    "EAC3_ATMOS",  /* = 18 */
    "AC4",         /* = 19 */
    "AC4_ATMOS",   /* = 20 */
    "MPEG_H",      /* = 21 */
    "MAT",         /* = 22 */
    "MAT_ATMOS",   /* = 23 */
    "TRUEHD",      /* = 24 */
    "TRUEHD_ATMOS",/* = 25 */
};

static bool HDMI_fmt_change_transition = FALSE;
static ahdmi_type_ext_type_t prevHDMIMode = AHDMI_PCM;

static bool HDMI_arcOnOff	= FALSE;
static bool HDMI_earcOnOff	= FALSE;
static bool ATV_open    	= FALSE;
static bool ATV_connect 	= FALSE;

/***********************************************************************************
 * Static resource checking function
 **********************************************************************************/
static char* GetResourceString(HAL_AUDIO_RESOURCE_T resId)
{
    if( (resId >= 0) && (resId < HAL_MAX_RESOURCE_NUM))
        return ResourceName[resId];
    else
        return ResourceName[HAL_MAX_RESOURCE_NUM];
}

static char* GetResourceStatusString(int statusId)
{
    int maxid = (sizeof(ResourceStatusSrting)/sizeof(ResourceStatusSrting[0]));
    if((statusId >= 0) && (statusId < maxid))
        return ResourceStatusSrting[statusId];
    else
        return ResourceStatusSrting[maxid -1];
}

static char* GetSRCTypeName(HAL_AUDIO_SRC_TYPE_T SRCType)
{
    if(SRCType < 0 || SRCType > HAL_AUDIO_SRC_TYPE_TRUEHD_ATMOS)
        return SRCTypeName[0];
    else
        return SRCTypeName[SRCType];
}

static inline HAL_AUDIO_AENC_INDEX_T res2aenc(HAL_AUDIO_RESOURCE_T res_id)
{
    return (HAL_AUDIO_AENC_INDEX_T)(res_id - HAL_AUDIO_RESOURCE_AENC0);
}

static inline HAL_AUDIO_RESOURCE_T aenc2res(HAL_AUDIO_AENC_INDEX_T enc_id)
{
    return (HAL_AUDIO_RESOURCE_T)(enc_id + HAL_AUDIO_RESOURCE_AENC0);
}

// inputConnect -> current Connect
static void CleanConnectInputSourceAndStatus(HAL_AUDIO_MODULE_STATUS resourceStatus[HAL_MAX_RESOURCE_NUM],
                                             HAL_AUDIO_RESOURCE_T currentConnect,
                                             HAL_AUDIO_RESOURCE_T inputConnect)
{
    int i, index;

    // current  module 's input pin
    index = resourceStatus[currentConnect].numIptconnect;

    if(index <=  0)
    {
        AUDIO_FATAL("[AUDH-FATAL] Inputs of %s = (%d)\n", GetResourceString(currentConnect), index);
        return;
    }

    for( i = 0; i < resourceStatus[currentConnect].numIptconnect; i++)
    {
        if(resourceStatus[currentConnect].inputConnectResourceId[i] == inputConnect)
        {
            break;
        }
    }

    for(; i < (resourceStatus[currentConnect].numIptconnect -1); i++)
    {
        resourceStatus[currentConnect].inputConnectResourceId[i] = resourceStatus[currentConnect].inputConnectResourceId[i+1];
        resourceStatus[currentConnect].connectStatus[i] = resourceStatus[currentConnect].connectStatus[i+1];
    }

    if(resourceStatus[currentConnect].numIptconnect <= 0)
    {
        AUDIO_ERROR( "%s  %d %d \n", __FUNCTION__, currentConnect, inputConnect);
    }

    resourceStatus[currentConnect].inputConnectResourceId[index-1] = HAL_AUDIO_RESOURCE_NO_CONNECTION;
    resourceStatus[currentConnect].connectStatus[index-1] = HAL_AUDIO_RESOURCE_DISCONNECT;
    resourceStatus[currentConnect].numIptconnect--;

    if(HAL_AUDIO_RESOURCE_NO_CONNECTION != inputConnect)
	{
		// input module 's output pin
		index = resourceStatus[inputConnect].numOptconnect;

		if(index <=  0)
		{
			AUDIO_FATAL("[AUDH-FATAL] Outputs of %s = (%d)\n", GetResourceString(inputConnect), index);
			AUDIO_FATAL("[AUDH][%s] %s -> %s\n", __FUNCTION__,
					GetResourceString(inputConnect), GetResourceString(currentConnect));
			return;
		}

		for( i = 0; i < resourceStatus[inputConnect].numOptconnect; i++)
		{
			if(resourceStatus[inputConnect].outputConnectResourceID[i] == currentConnect)
			{
				break;
			}
		}

		for(; i < (resourceStatus[inputConnect].numOptconnect -1); i++)
		{
			resourceStatus[inputConnect].outputConnectResourceID[i] = resourceStatus[inputConnect].outputConnectResourceID[i+1];
		}

		resourceStatus[inputConnect].outputConnectResourceID[index -1] = HAL_AUDIO_RESOURCE_NO_CONNECTION;
		resourceStatus[inputConnect].numOptconnect--;
	}
}

static void GetConnectOutputSource(HAL_AUDIO_RESOURCE_T resID,
                                   HAL_AUDIO_RESOURCE_T* outputConnectResId,
                                   int numOutputConnectArray,
                                   int* totalOutputConnectResource)
{
    int minArraySize;
    int i;

    *totalOutputConnectResource = ResourceStatus[resID].numOptconnect;

    if(numOutputConnectArray <= *totalOutputConnectResource)
        minArraySize = numOutputConnectArray;
    else
        minArraySize = *totalOutputConnectResource;

    for(i = 0; i < minArraySize; i++)
    {
        outputConnectResId[i] = ResourceStatus[resID].outputConnectResourceID[i];
    }
    return;
}

static void GetInputConnectSource(HAL_AUDIO_RESOURCE_T resID,
                                  HAL_AUDIO_RESOURCE_T *inputIDArr,
                                  int array_length,
                                  int *num_of_inputs)
{
    int minArraySize, i;

    *num_of_inputs = ResourceStatus[resID].numIptconnect;

    if(array_length <= *num_of_inputs)
        minArraySize = array_length;
    else
        minArraySize = *num_of_inputs;

    for(i = 0; i < minArraySize; i++)
    {
        inputIDArr[i] = ResourceStatus[resID].inputConnectResourceId[i];
    }
    return;
}

int GetCurrentADCConnectPin(void)
{
    int revalue = -1;
    int i;
    for(i = 0; i < MAIN_AIN_ADC_PIN; i++)
    {
        AUDIO_DEBUG("ADC pin %d is at  %s state \n", i, GetResourceStatusString(gAinStatus.ainPinStatus[i]));
        if(gAinStatus.ainPinStatus[i] == HAL_AUDIO_RESOURCE_CONNECT)
        {
            if(revalue != -1)
            {
                AUDIO_ERROR("Error ADC too much connect %d %d \n", revalue, i);
                return -1;
            }
            revalue =  i;
        }
    }

    return revalue;
}

int GetCurrentHDMIConnectPin(void)
{
    int revalue = -1;
    int i;

    if(ResourceStatus[HAL_AUDIO_RESOURCE_HDMI].connectStatus[0] == HAL_AUDIO_RESOURCE_CONNECT)
    {
        revalue =  HAL_AUDIO_RESOURCE_HDMI;
    }

    for(i = HAL_AUDIO_RESOURCE_HDMI0; i <= HAL_AUDIO_RESOURCE_SWITCH; i++)
    {
        if(ResourceStatus[i].connectStatus[0] == HAL_AUDIO_RESOURCE_CONNECT)
        {
            if(revalue != -1)
            {
                AUDIO_ERROR("Error HDMI too much connect %d %d \n", revalue, i);
                return -1;
            }
            revalue =  i;
        }
    }

    if(revalue == -1)
        AUDIO_DEBUG("no  HDMI connect\n");
    return revalue;
}

static inline HAL_AUDIO_ADEC_INDEX_T res2adec(HAL_AUDIO_RESOURCE_T res_id)
{
    HAL_AUDIO_ADEC_INDEX_T adec_id = HAL_AUDIO_ADEC0;

    switch(res_id)
    {
        case HAL_AUDIO_RESOURCE_ADEC0:
            adec_id = HAL_AUDIO_ADEC0;
            break;
        case HAL_AUDIO_RESOURCE_ADEC1:
            adec_id = HAL_AUDIO_ADEC1;
            break;
#if defined(VIRTUAL_ADEC_ENABLED)
        case HAL_AUDIO_RESOURCE_ADEC2:
            adec_id = HAL_AUDIO_ADEC2;
            break;
        case HAL_AUDIO_RESOURCE_ADEC3:
            adec_id = HAL_AUDIO_ADEC3;
            break;
#endif
        default:
            break;
    }

    return adec_id;
}

static inline adec_src_codec_ext_type_t codec_type_hal2alsa(HAL_AUDIO_SRC_TYPE_T codec_type)
{
	adec_src_codec_ext_type_t codec_alsa = ADEC_SRC_CODEC_UNKNOWN;
	switch(codec_type)
	{
	case HAL_AUDIO_SRC_TYPE_PCM:
		codec_alsa = ADEC_SRC_CODEC_PCM; break;
	case HAL_AUDIO_SRC_TYPE_AC3:
		codec_alsa = ADEC_SRC_CODEC_AC3; break;
	case HAL_AUDIO_SRC_TYPE_EAC3:
		codec_alsa = ADEC_SRC_CODEC_EAC3; break;
	case HAL_AUDIO_SRC_TYPE_MPEG:
	case HAL_AUDIO_SRC_TYPE_MP3:
		codec_alsa = ADEC_SRC_CODEC_MPEG; break;
	case HAL_AUDIO_SRC_TYPE_AAC:
		codec_alsa = ADEC_SRC_CODEC_AAC; break;
	case HAL_AUDIO_SRC_TYPE_HEAAC:
		codec_alsa = ADEC_SRC_CODEC_HEAAC; break;
	case HAL_AUDIO_SRC_TYPE_DRA:
		codec_alsa = ADEC_SRC_CODEC_DRA; break;
	case HAL_AUDIO_SRC_TYPE_DTS:
		codec_alsa = ADEC_SRC_CODEC_DTS; break;
	case HAL_AUDIO_SRC_TYPE_SIF:
		codec_alsa = ADEC_SRC_CODEC_SIF; break;
	case HAL_AUDIO_SRC_TYPE_SIF_BTSC:
		codec_alsa = ADEC_SRC_CODEC_SIF_BTSC; break;
	case HAL_AUDIO_SRC_TYPE_SIF_A2:
		codec_alsa = ADEC_SRC_CODEC_SIF_A2; break;
	case HAL_AUDIO_SRC_TYPE_DTS_HD_MA:
		codec_alsa = ADEC_SRC_CODEC_DTS_HD_MA; break;
	case HAL_AUDIO_SRC_TYPE_DTS_EXPRESS:
		codec_alsa = ADEC_SRC_CODEC_DTS_EXPRESS; break;
	case HAL_AUDIO_SRC_TYPE_DTS_CD:
		codec_alsa = ADEC_SRC_CODEC_DTS_CD; break;
	case HAL_AUDIO_SRC_TYPE_EAC3_ATMOS:
		codec_alsa = ADEC_SRC_CODEC_EAC3_ATMOS; break;
	case HAL_AUDIO_SRC_TYPE_AC4:
		codec_alsa = ADEC_SRC_CODEC_AC4; break;
	case HAL_AUDIO_SRC_TYPE_AC4_ATMOS:
		codec_alsa = ADEC_SRC_CODEC_AC4_ATMOS; break;
	case HAL_AUDIO_SRC_TYPE_MPEG_H:
		codec_alsa = ADEC_SRC_CODEC_MPEG_H; break;
	case HAL_AUDIO_SRC_TYPE_MAT:
		codec_alsa = ADEC_SRC_CODEC_MAT; break;
	case HAL_AUDIO_SRC_TYPE_MAT_ATMOS:
		codec_alsa = ADEC_SRC_CODEC_MAT_ATMOS; break;
	case HAL_AUDIO_SRC_TYPE_TRUEHD:
		codec_alsa = ADEC_SRC_CODEC_TRUEHD; break;
	case HAL_AUDIO_SRC_TYPE_TRUEHD_ATMOS:
		codec_alsa = ADEC_SRC_CODEC_TRUEHD_ATMOS; break;
	case HAL_AUDIO_SRC_TYPE_DEFAULT:
	case HAL_AUDIO_SRC_TYPE_NONE:
	case HAL_AUDIO_SRC_TYPE_WMA_PRO:
	case HAL_AUDIO_SRC_TYPE_VORBIS:
	case HAL_AUDIO_SRC_TYPE_AMR_WB:
	case HAL_AUDIO_SRC_TYPE_AMR_NB:
	case HAL_AUDIO_SRC_TYPE_ADPCM:
	case HAL_AUDIO_SRC_TYPE_RA8:
	case HAL_AUDIO_SRC_TYPE_FLAC:
	default:
		codec_alsa = ADEC_SRC_CODEC_UNKNOWN; break;
	}
	return codec_alsa;
}

static inline HAL_AUDIO_SRC_TYPE_T codec_type_alsa2hal(adec_src_codec_ext_type_t codec_alsa)
{
    HAL_AUDIO_RESOURCE_T codec_type = HAL_AUDIO_SRC_TYPE_UNKNOWN;

    switch(codec_alsa)
    {
        case ADEC_SRC_CODEC_PCM:
            codec_type = HAL_AUDIO_SRC_TYPE_PCM;
            break;
        case ADEC_SRC_CODEC_AC3:
            codec_type = HAL_AUDIO_SRC_TYPE_AC3;
            break;
        case ADEC_SRC_CODEC_EAC3:
            codec_type = HAL_AUDIO_SRC_TYPE_EAC3;
            break;
        case ADEC_SRC_CODEC_EAC3_ATMOS:
            codec_type = HAL_AUDIO_SRC_TYPE_EAC3_ATMOS;
            break;
        case ADEC_SRC_CODEC_AC4:
            codec_type = HAL_AUDIO_SRC_TYPE_AC4;
            break;
        case ADEC_SRC_CODEC_AC4_ATMOS:
            codec_type = HAL_AUDIO_SRC_TYPE_AC4_ATMOS;
            break;
        case ADEC_SRC_CODEC_MAT:
            codec_type = HAL_AUDIO_SRC_TYPE_MAT;
            break;
        case ADEC_SRC_CODEC_MAT_ATMOS:
            codec_type = HAL_AUDIO_SRC_TYPE_MAT_ATMOS;
            break;
        case ADEC_SRC_CODEC_TRUEHD:
            codec_type = HAL_AUDIO_SRC_TYPE_TRUEHD;
            break;
        case ADEC_SRC_CODEC_TRUEHD_ATMOS:
            codec_type = HAL_AUDIO_SRC_TYPE_TRUEHD_ATMOS;
            break;
        case ADEC_SRC_CODEC_AAC:
            codec_type = HAL_AUDIO_SRC_TYPE_AAC;
            break;
        case ADEC_SRC_CODEC_HEAAC:
            codec_type = HAL_AUDIO_SRC_TYPE_HEAAC;
            break;

        case ADEC_SRC_CODEC_MPEG:
            codec_type = HAL_AUDIO_SRC_TYPE_MPEG;
            break;
        case ADEC_SRC_CODEC_MPEG_H:
            codec_type = HAL_AUDIO_SRC_TYPE_MPEG_H;
            break;
        case ADEC_SRC_CODEC_DRA:
            codec_type = HAL_AUDIO_SRC_TYPE_DRA;
            break;

        case ADEC_SRC_CODEC_DTS:
            codec_type = HAL_AUDIO_SRC_TYPE_DTS;
            break;
        case ADEC_SRC_CODEC_DTS_HD_MA:
            codec_type = HAL_AUDIO_SRC_TYPE_DTS_HD_MA;
            break;
        case ADEC_SRC_CODEC_DTS_EXPRESS:
            codec_type = HAL_AUDIO_SRC_TYPE_DTS_EXPRESS;
            break;
        case ADEC_SRC_CODEC_DTS_CD:
            codec_type = HAL_AUDIO_SRC_TYPE_DTS_CD;
            break;

        case ADEC_SRC_CODEC_SIF:
            codec_type = HAL_AUDIO_SRC_TYPE_SIF;
            break;
        case ADEC_SRC_CODEC_SIF_BTSC:
            codec_type = HAL_AUDIO_SRC_TYPE_SIF_BTSC;
            break;
        case ADEC_SRC_CODEC_SIF_A2:
            codec_type = HAL_AUDIO_SRC_TYPE_SIF_A2;
            break;

        default:
            break;
    }

    return codec_type;
}

static inline HAL_AUDIO_AENC_BITRATE_T bitrate_type_alsa2hal(aenc_bitrate_t bitrate_alsa)
{
    HAL_AUDIO_AENC_BITRATE_T res = HAL_AUDIO_AENC_BIT_48K;

    switch(bitrate_alsa)
    {
        case AENC_BIT_48K:
            res = HAL_AUDIO_AENC_BIT_48K;
            break;
        case AENC_BIT_56K:
            res = HAL_AUDIO_AENC_BIT_56K;
            break;
        case AENC_BIT_64K:
            res = HAL_AUDIO_AENC_BIT_64K;
            break;
        case AENC_BIT_80K:
            res = HAL_AUDIO_AENC_BIT_80K;
            break;
        case AENC_BIT_112K:
            res = HAL_AUDIO_AENC_BIT_112K;
            break;
        case AENC_BIT_128K:
            res = HAL_AUDIO_AENC_BIT_128K;
            break;
        case AENC_BIT_160K:
            res = HAL_AUDIO_AENC_BIT_160K;
            break;
        case AENC_BIT_192K:
            res = HAL_AUDIO_AENC_BIT_192K;
            break;
        case AENC_BIT_224K:
            res = HAL_AUDIO_AENC_BIT_224K;
            break;
        case AENC_BIT_256K:
            res = HAL_AUDIO_AENC_BIT_256K;
            break;
        case AENC_BIT_320K:
            res = HAL_AUDIO_AENC_BIT_320K;
            break;
        default:
            res = HAL_AUDIO_AENC_BIT_48K;
            break;
    }

    return res;
}

static inline HAL_AUDIO_RESOURCE_T res_type_alsa2hal(adec_src_port_index_ext_type_t res_alsa)
{
    HAL_AUDIO_RESOURCE_T res = HAL_AUDIO_RESOURCE_NO_CONNECTION;

    switch(res_alsa)
    {
        case ADEC_SRC_ATP0:
            res = HAL_AUDIO_RESOURCE_ATP0;
            break;
        case ADEC_SRC_ATP1:
            res = HAL_AUDIO_RESOURCE_ATP1;
            break;
        case ADEC_SRC_ADC:
            res = HAL_AUDIO_RESOURCE_ADC;
            break;
        case ADEC_SRC_AAD:
            res = HAL_AUDIO_RESOURCE_AAD;
            break;
        case ADEC_SRC_HDMI_PORT0:
            res = HAL_AUDIO_RESOURCE_HDMI0;
            break;
        case ADEC_SRC_HDMI_PORT1:
            res = HAL_AUDIO_RESOURCE_HDMI1;
            break;
        case ADEC_SRC_HDMI_PORT2:
            res = HAL_AUDIO_RESOURCE_HDMI2;
            break;
        case ADEC_SRC_HDMI_PORT3:
            res = HAL_AUDIO_RESOURCE_HDMI3;
            break;
        case ADEC_SRC_HDMI_DEFAULT:
            res = HAL_AUDIO_RESOURCE_HDMI;
            break;
        default:
            break;
    }

    return res;
}

static inline HAL_AUDIO_RESOURCE_T opt2res(common_output_ext_type_t opt_id)
{
    HAL_AUDIO_RESOURCE_T res_id = HAL_AUDIO_RESOURCE_NO_CONNECTION;

    switch(opt_id)
    {
        case COMMON_SPK:
            res_id = HAL_AUDIO_RESOURCE_OUT_SPK;
            break;
        case COMMON_OPTIC:
            res_id = HAL_AUDIO_RESOURCE_OUT_SPDIF;
            break;
        case COMMON_OPTIC_LG:
            res_id = HAL_AUDIO_RESOURCE_OUT_SB_SPDIF;
            break;
        case COMMON_SE_BT:
        case COMMON_BLUETOOTH:
            res_id = HAL_AUDIO_RESOURCE_OUT_SB_PCM;
            break;
        case COMMON_HP:
            res_id = HAL_AUDIO_RESOURCE_OUT_HP;
            break;
        case COMMON_ARC:
            res_id = HAL_AUDIO_RESOURCE_OUT_ARC;
            break;
        case COMMON_WISA:
            res_id = HAL_AUDIO_RESOURCE_OUT_WISA;
            break;
        default:
            break;
    }

    return res_id;
}

static inline HAL_AUDIO_RESOURCE_T ipt2res(adec_src_port_index_ext_type_t ipt_id)
{
    HAL_AUDIO_RESOURCE_T res_id = HAL_AUDIO_RESOURCE_NO_CONNECTION;

    switch(ipt_id)
    {
        case ADEC_SRC_ATP0:
            res_id = HAL_AUDIO_RESOURCE_ATP0;
            break;
        case ADEC_SRC_ATP1:
            res_id = HAL_AUDIO_RESOURCE_ATP1;
            break;
        case ADEC_SRC_ADC:
            res_id = HAL_AUDIO_RESOURCE_ADC;
            break;
        case ADEC_SRC_AAD:
            res_id = HAL_AUDIO_RESOURCE_AAD;
            break;
        case ADEC_SRC_HDMI_PORT0:
            res_id = HAL_AUDIO_RESOURCE_HDMI0;
            break;
        case ADEC_SRC_HDMI_PORT1:
            res_id = HAL_AUDIO_RESOURCE_HDMI1;
            break;
        case ADEC_SRC_HDMI_PORT2:
            res_id = HAL_AUDIO_RESOURCE_HDMI2;
            break;
        case ADEC_SRC_HDMI_PORT3:
            res_id = HAL_AUDIO_RESOURCE_HDMI3;
            break;
        default:
            break;
    }

    return res_id;
}

static inline HAL_AUDIO_RESOURCE_T adec2res(HAL_AUDIO_ADEC_INDEX_T dec_id)
{
    HAL_AUDIO_RESOURCE_T res_id = HAL_AUDIO_RESOURCE_ADEC0;

    switch(dec_id)
    {
        case HAL_AUDIO_ADEC0:
            res_id = HAL_AUDIO_RESOURCE_ADEC0;
            break;
        case HAL_AUDIO_ADEC1:
            res_id = HAL_AUDIO_RESOURCE_ADEC1;
            break;
#if defined(VIRTUAL_ADEC_ENABLED)
        case HAL_AUDIO_ADEC2:
            res_id = HAL_AUDIO_RESOURCE_ADEC2;
            break;
        case HAL_AUDIO_ADEC3:
            res_id = HAL_AUDIO_RESOURCE_ADEC3;
            break;
#endif
        default:
            break;
    }

    return res_id;
}

static inline BOOLEAN IsAOutSource(HAL_AUDIO_RESOURCE_T res_id)
{
    BOOLEAN reval;
    switch (res_id)
    {
        case HAL_AUDIO_RESOURCE_SE:
        case HAL_AUDIO_RESOURCE_OUT_SPK:
        case HAL_AUDIO_RESOURCE_OUT_SPDIF:
        case HAL_AUDIO_RESOURCE_OUT_SB_SPDIF:
        case HAL_AUDIO_RESOURCE_OUT_SB_PCM:
        case HAL_AUDIO_RESOURCE_OUT_WISA:
        case HAL_AUDIO_RESOURCE_OUT_SB_CANVAS:
        case HAL_AUDIO_RESOURCE_OUT_HP:
        case HAL_AUDIO_RESOURCE_OUT_ARC:
        //case HAL_AUDIO_RESOURCE_OUT_SCART:
        case HAL_AUDIO_RESOURCE_OUT_SPDIF_ES:
            reval = TRUE;
            break;
        default:
            reval = FALSE;
            break;
    }
    return reval;
}

static inline HAL_AUDIO_MIXER_INDEX_T res2amixer(HAL_AUDIO_RESOURCE_T res_id)
{
    return (HAL_AUDIO_MIXER_INDEX_T)(res_id - HAL_AUDIO_RESOURCE_MIXER0);
}

static inline HAL_AUDIO_RESOURCE_T amixer2res(HAL_AUDIO_MIXER_INDEX_T amixer_id)
{
    return (HAL_AUDIO_RESOURCE_T)(amixer_id + HAL_AUDIO_RESOURCE_MIXER0);
}

static inline BOOLEAN IsAMIXSource(HAL_AUDIO_RESOURCE_T res_id)
{
    if((res_id <= HAL_AUDIO_RESOURCE_MIXER7) && (res_id >= HAL_AUDIO_RESOURCE_MIXER0))
        return TRUE;
    else
        return FALSE;
}

static inline BOOLEAN IsADCSource(HAL_AUDIO_RESOURCE_T res_id)
{
    return (res_id == HAL_AUDIO_RESOURCE_ADC);
}

static inline BOOLEAN IsHDMISource(HAL_AUDIO_RESOURCE_T res_id)
{
    if (res_id == HAL_AUDIO_RESOURCE_HDMI)
        return TRUE;
    else if ((res_id >= HAL_AUDIO_RESOURCE_HDMI0) && (res_id <= HAL_AUDIO_RESOURCE_SWITCH))
        return TRUE;
    else
        return FALSE;
}

static inline BOOLEAN IsATVSource(HAL_AUDIO_RESOURCE_T res_id)
{
    return (res_id == HAL_AUDIO_RESOURCE_AAD);
}

static inline BOOLEAN IsSystemSource(HAL_AUDIO_RESOURCE_T res_id)
{
    return (res_id == HAL_AUDIO_RESOURCE_SYSTEM);
}

static inline BOOLEAN IsDTVSource(HAL_AUDIO_RESOURCE_T res_id)
{
    if(res_id == HAL_AUDIO_RESOURCE_ATP0 || res_id == HAL_AUDIO_RESOURCE_ATP1)
        return TRUE;
    else
        return FALSE;
}
static inline BOOLEAN IsADECSource(HAL_AUDIO_RESOURCE_T res_id)
{
    if(res_id == HAL_AUDIO_RESOURCE_ADEC0 || res_id == HAL_AUDIO_RESOURCE_ADEC1)
        return TRUE;
#if defined(VIRTUAL_ADEC_ENABLED)
    else if(res_id == HAL_AUDIO_RESOURCE_ADEC2 || res_id == HAL_AUDIO_RESOURCE_ADEC3)
        return TRUE;
#endif
    else
        return FALSE;
}

static inline BOOLEAN IsSESource(HAL_AUDIO_RESOURCE_T res_id)
{
    return (res_id == HAL_AUDIO_RESOURCE_SE);
}

static inline BOOLEAN IsEncoderSource(HAL_AUDIO_RESOURCE_T res_id)
{
    if((res_id == HAL_AUDIO_RESOURCE_AENC0) /*|| (res_id >= HAL_AUDIO_RESOURCE_AENC1)*/)
        return TRUE;
    else
        return FALSE;
}

static inline BOOLEAN IsValidSEOpts(HAL_AUDIO_RESOURCE_T res_id)
{
    if((res_id == HAL_AUDIO_RESOURCE_OUT_SPK) ||
       (res_id == HAL_AUDIO_RESOURCE_OUT_SPDIF))
        return TRUE;
    else
        return FALSE;
}

static inline BOOLEAN IsAinSource(HAL_AUDIO_RESOURCE_T res_id)
{
    if(IsADCSource(res_id))
        return TRUE;
    else if(IsHDMISource(res_id))
        return TRUE;
    else if(IsATVSource(res_id))
        return TRUE;
    else
        return FALSE;
}

static inline BOOLEAN IsValidAdecIdx(HAL_AUDIO_ADEC_INDEX_T adecIndex)
{
    BOOL ret = FALSE;
    if(adecIndex == HAL_AUDIO_ADEC0 || adecIndex == HAL_AUDIO_ADEC1) {
        ret = TRUE;
    }
#if defined(VIRTUAL_ADEC_ENABLED)
    else if(adecIndex == HAL_AUDIO_ADEC2 || adecIndex == HAL_AUDIO_ADEC3) {
        ret = TRUE;
    }
#endif
    return ret;
}

static inline BOOLEAN IsValidADECIpts(HAL_AUDIO_RESOURCE_T res_id)
{
    if(IsAinSource(res_id))
        return TRUE;
    else if(IsDTVSource(res_id))
        return TRUE;
    else if(IsSystemSource(res_id))
        return TRUE;
    else
        return FALSE;
}

static inline BOOLEAN RangeCheck(UINT32 target, UINT32 min, UINT32 max)
{
    return (target >= min && target < max);
}

/*********** End of Resource related static functions *****************************/

/**********************************************************************************
 * Static variables access functions
 **********************************************************************************/

static inline UINT32 GetDecInMute(UINT32 index)
{
    return Aud_decstatus[index].decInMute;
}

static inline UINT32 GetDecESMute(UINT32 index)
{
    return Aud_decstatus[index].spdifESMute;
}

static inline HAL_AUDIO_VOLUME_T GetDecInVolume(UINT32 index)
{
    return Aud_decstatus[index].decInVolume;
}

static inline HAL_AUDIO_VOLUME_T GetDecOutVolume(UINT32 index, UINT32 ch)
{
    return Aud_decstatus[index].decOutVolume[ch];
}

static inline UINT32 GetTrickState(UINT32 index)
{
    return Aud_decstatus[index].trickPauseState;
}

static inline UINT32 GetEsMode(UINT32 index)
{
    return Aud_decstatus[index].espushMode;
}

static inline UINT32 GetSysMemoryExist(UINT32 index)
{
    return Aud_decstatus[index].sysmemoryExist;
}

static inline void SetDecEsPushMode(UINT32 adecIndex, HAL_AUDIO_ES_MODE_T mode)
{
    Aud_decstatus[adecIndex].espushMode = mode;
}

static inline void SetSysMemoryExist(UINT32 adecIndex, BOOLEAN IsExist)
{
    Aud_decstatus[adecIndex].sysmemoryExist = IsExist;
}

static inline void SetDecInMute(UINT32 index, UINT32 bMute)
{
    Aud_decstatus[index].decInMute = bMute;
    adec_status[index].Mute = bMute;
}

static inline void SetDecInVolume(UINT32 index, HAL_AUDIO_VOLUME_T vol)
{
    Aud_decstatus[index].decInVolume = vol;
}

static inline void SetDecOutVolume(UINT32 index, UINT32 ch, HAL_AUDIO_VOLUME_T vol)
{
    Aud_decstatus[index].decOutVolume[ch] = vol;
}

static inline void SetDecESMute(UINT32 index, UINT32 bMute)
{
    Aud_decstatus[index].spdifESMute = bMute;
}

static inline void SetSPDIFOutType(UINT32 index, UINT32 type)
{
    Aud_decstatus[index].spdifouttype = type;
}

char *TrickModeName[] = {
    "NONE",
    "PAUSE",
    "PLAY",
    "SLOW_MOTION_OP25X",
    "SLOW_MOTION_OP50X",
    "SLOW_MOTION_OP80X",
    "FAST_FORWARD_1P20X",
    "FAST_FORWARD_1P50X",
    "FAST_FORWARD_2P00X",
};

static inline void SetTrickState(UINT32 index, UINT32 state)
{
    Aud_decstatus[index].trickPauseState = state;
    adec_status[index].TrickMode = state;
}

static inline void SetDecUserState(UINT32 index, UINT32 state)
{
    Aud_decstatus[index].userSetRun = state;
}
static inline UINT32 GetDectUserState(UINT32 adecIndex)
{
    return Aud_decstatus[adecIndex].userSetRun;
}

static inline UINT32 GetUserFormat(UINT32 adecIndex)
{
    return adec_info[adecIndex].userAdecFormat;
}

char *DrcModeName[] = {
    "LINE",
    "RF",
    "DRC_OFF",
};

static inline void SetDecDrcMode(UINT32 adecIndex, HAL_AUDIO_DOLBY_DRC_MODE_T mode)
{
    Aud_decstatus[adecIndex].drcMode = mode;
    adec_status[adecIndex].DolbyDRCMode = mode;
}

static inline HAL_AUDIO_DOLBY_DRC_MODE_T GetDecDrcMode(UINT32 adecIndex)
{
    return Aud_decstatus[adecIndex].drcMode;
}

char *DownMixModeName[] = {
    "LORO",
    "LTRT",
};

static inline void SetDecDownMixMode(UINT32 adecIndex, HAL_AUDIO_DOWNMIX_MODE_T mode)
{
    Aud_decstatus[adecIndex].downmixMode = mode;
    adec_status[adecIndex].Downmix = mode;
}

static inline HAL_AUDIO_DOWNMIX_MODE_T GetDecDownMixMode(UINT32 adecIndex)
{
    return Aud_decstatus[adecIndex].downmixMode;
}

static inline UINT32 GetAmixerUserMute(HAL_AUDIO_MIXER_INDEX_T index)
{
    /*assert(index < AUD_AMIX_MAX);*/
    return g_mixer_user_mute[index];
}

static inline void SetAmixerUserMute(HAL_AUDIO_MIXER_INDEX_T index, UINT32 bMute)
{
    /*assert(index < AUD_AMIX_MAX);*/
    g_mixer_user_mute[index] = bMute;
}

static BOOLEAN IsMainAudioADEC(HAL_AUDIO_INDEX_T audioIndex)
{
    if((audioIndex != HAL_AUDIO_INDEX0) && (audioIndex != HAL_AUDIO_INDEX1) &&
       (audioIndex != HAL_AUDIO_INDEX2)	&& (audioIndex != HAL_AUDIO_INDEX3))
    {
        return FALSE;
    }
    else
    {
        return TRUE;
    }
}

/*********** End of Static variables access functions ****************************/

/**********************************************************************************
 * HAL Volume and ADSP Gain related functions
 **********************************************************************************/
SINT32 Volume_Compare(HAL_AUDIO_VOLUME_T v1, HAL_AUDIO_VOLUME_T v2)
{
    if((v1.mainVol == v2.mainVol) && (v1.fineVol == v2.fineVol))
        return TRUE;
    else
        return FALSE;
}

HAL_AUDIO_VOLUME_T Volume_Add(HAL_AUDIO_VOLUME_T v1, HAL_AUDIO_VOLUME_T v2)
{
    HAL_AUDIO_VOLUME_T result;
    SINT16 mainVol, fineVol;

    mainVol = (SINT16)(v1.mainVol - 127) + (SINT16)(v2.mainVol - 127);
    fineVol = v1.fineVol + v2.fineVol;
    //fix coverity 306, Event uninit_use: Using uninitialized value "result.fineVol"
    //if(result.fineVol >= 16)
    if(fineVol >= 16)
    {
        mainVol += 1;
        fineVol -= 16;
    }

    if(mainVol < -127)
    {
        //AUDIO_VERBOSE("[HAL_AUD][WARNING] (%d dB) is under support range\n", mainVol);
        result.mainVol = 0x0;
        result.fineVol = 0x0;
    }
    else if(mainVol > 127)
    {
        //AUDIO_VERBOSE("[HAL_AUD][WARNING] (%d dB) is over support range\n", mainVol);
        result.mainVol = 0xFF;
        result.fineVol = 0x0;
    }
    else
    {
        result.mainVol = (UINT8)mainVol + 0x7F;
        result.fineVol = (UINT8)fineVol;
    }
    return result;
}

SINT32 Volume_to_DSPGain(HAL_AUDIO_VOLUME_T volume)
{
    SINT32 dsp_gain = ENUM_AUDIO_DVOL_K0p0DB;
    if(volume.fineVol%4 != 0)
    {
        //AUDIO_DEBUG("[HAL_AUD][WARNING] fineVol(%d) not support, approximate it\n", volume.fineVol);
    }

    /* SW dsp_gain scale: 0.25 dB
     * mainVol scale: 1 dB
     * fineVol scale: 0.0625 dB
     */
    dsp_gain += ((volume.mainVol - 127)*4);
    dsp_gain += (volume.fineVol/4);

    if(dsp_gain > ENUM_AUDIO_DVOL_K30p0DB)
    {
        //AUDIO_VERBOSE("[HAL_AUD][WARNING] volume(%d) over support range, set to +30dB\n",dsp_gain);
        dsp_gain = ENUM_AUDIO_DVOL_K30p0DB;
    }
    else if(dsp_gain < ENUM_AUDIO_DVOL_KMINUS72p0DB)
    {
        //AUDIO_VERBOSE("[HAL_AUD][WARNING] volume(%d) under support range, set to -72dB\n",dsp_gain);
        dsp_gain = ENUM_AUDIO_DVOL_KMINUS72p0DB;
    }

    return dsp_gain;
}

UINT32 Volume_to_MixerGain(HAL_AUDIO_VOLUME_T volume)
{
    SINT32 Adsp_gainume_dB = (SINT32)volume.mainVol-0x7F;
    if(Adsp_gainume_dB > 0)
    {
        return ADEC_DSP_MIX_GAIN_0DB;
    }
    else if(Adsp_gainume_dB <= -DB2MIXGAIN_TABLE_SIZE) /* under -51dB */
    {
        return ADEC_DSP_MIX_GAIN_MUTE;
    }
    else
    {
        return dB2mixgain_table[(-Adsp_gainume_dB)];
    }
}

/*********** End of HAL Volume to ADSP Gain related functions *********************/

void HAL_AUDIO_MEMOUT_ReleaseData(long dataSize)
{
    RHAL_AUDIO_AENC_T *aenc = Aud_aenc[HAL_AUDIO_AENC0];

    ((MemOut*)aenc->pMemOut->pDerivedObj)->ReleaseData(aenc->pMemOut, dataSize);
}

long RHAL_AUDIO_Encoder_DataDeliver_loop(int aenc_index, HAL_AUDIO_AENC_DATA_T *Msg)
{
    RHAL_AUDIO_AENC_T *aenc = Aud_aenc[aenc_index];
    UINT32 dataSize = 0;
    UINT64 pts      = 0;
    UINT32 frameNum = 0;
    UINT8  *pData   = NULL;
    UINT32 bufBase  = 0;
    UINT32 bufSize  = 0;
    long long prevPrintTime = 0L;

    if(Aud_aenc[aenc_index]->info.status == HAL_AUDIO_AENC_STATUS_STOP)
        return 0;

    ((ENC*)aenc->pEnc->pDerivedObj)->DeliverAudioFrame(aenc->pEnc);

    ((MemOut*)aenc->pMemOut->pDerivedObj)->GetRingBuffer(aenc->pMemOut, &bufBase, &bufSize);
    Msg->index   = aenc->index;
    Msg->pRStart = (UINT8*)bufBase;
    Msg->pREnd   = (UINT8*)(bufBase+bufSize);

    ((MemOut*)aenc->pMemOut->pDerivedObj)->ReadFrame(aenc->pMemOut, &pData, &dataSize, &pts, &frameNum);

    if(frameNum == 0)
    {
        AUDIO_DEBUG("[ENC] frameNum == 0\n");
        return 0;
    }

    Msg->pts     = pts;
    Msg->pData   = pData;
    Msg->dataLen = dataSize;
    AUDIO_DEBUG("[ENC] pData: 0x%x dataSize %d \n", (unsigned int)Msg->pData , Msg->dataLen);
    AUDIO_DEBUG("[ENC] pRStart: 0x%x pREnd 0x%x \n", (unsigned int)Msg->pRStart , (int)Msg->pREnd);

    if(pli_getPTS() >=( prevPrintTime + (15*90*1000)))
    {
        AUDIO_DEBUG("[ENC] audio_pts: %x size %x \n", (int)pts, dataSize);
        prevPrintTime = pli_getPTS();
    }

    if( (Msg->pData  < Msg->pRStart) || (Msg->pData > Msg->pREnd) )
    {
        AUDIO_INFO("[ENC] Check the pData Range!\n");
        AUDIO_INFO("[ENC] pData: 0x%x dataSize %d \n", (unsigned int)Msg->pData , Msg->dataLen);
        AUDIO_INFO("[ENC] pRStart: 0x%x pREnd %d \n", (unsigned int)Msg->pRStart , (int)Msg->pREnd);
        return 0;
    }

    aenc->info.inputCount++;
    aenc->info.pts = pts;
    return (long)dataSize;
}

static void InitialResourceStatus(void)
{
	int i,j;

	for(i=0; i < HAL_MAX_RESOURCE_NUM; i++)
	{
		snprintf(ResourceStatus[i].name, strlen(ResourceName[i])+1, "%s", ResourceName[i]);
		for(j=0; j < HAL_MAX_OUTPUT; j++)
		{
			ResourceStatus[i].inputConnectResourceId[j] = HAL_AUDIO_RESOURCE_NO_CONNECTION;
			ResourceStatus[i].outputConnectResourceID[j] = HAL_AUDIO_RESOURCE_NO_CONNECTION;
			ResourceStatus[i].connectStatus[j] = HAL_AUDIO_RESOURCE_CLOSE;
		}

		ResourceStatus[i].numOptconnect = 0;
		ResourceStatus[i].numIptconnect = 0;

		if(IsAOutSource((HAL_AUDIO_RESOURCE_T)i))
			ResourceStatus[i].maxVaildIptNum = 10;//adec 0 , 1, amix0~7
		else
			ResourceStatus[i].maxVaildIptNum = 1; // normal module
	}

	Aud_mainDecIndex = 0;// no main dec now
	Aud_prevMainDecIndex = -1;
	Aud_descriptionMode = 0;

    for(i = 0; i < AUD_ADEC_MAX; i++) {
        memset(&flowStatus[i], 0, sizeof(HAL_AUDIO_FLOW_STATUS));
    }

	memset(&g_AudioStatusInfo,0, sizeof(g_AudioStatusInfo));
	g_AudioStatusInfo.curSPKOutGain   = ALSA_GAIN_0dB;
	g_AudioStatusInfo.curSPDIFOutGain = ALSA_GAIN_0dB;
	g_AudioStatusInfo.curHPOutGain    = ALSA_GAIN_0dB;
	g_AudioStatusInfo.curBTOutGain    = ALSA_GAIN_0dB;
	g_AudioStatusInfo.curWISAOutGain  = ALSA_GAIN_0dB;
	g_AudioStatusInfo.curSEBTOutGain  = ALSA_GAIN_0dB;
	g_ARCStatusInfo.curARCOutGain     = ALSA_GAIN_0dB;
	g_AudioStatusInfo.curSPKMuteStatus       = TRUE;
	g_AudioStatusInfo.curSPDIFMuteStatus     = TRUE;
	g_AudioStatusInfo.curSPDIF_LG_MuteStatus = TRUE;
	g_AudioStatusInfo.curBTMuteStatus        = TRUE;
	g_AudioStatusInfo.curHPMuteStatus        = TRUE;
	g_ARCStatusInfo.curARCMuteStatus         = TRUE;
	g_AudioStatusInfo.curWISAMuteStatus      = TRUE;
	g_AudioStatusInfo.curSEBTMuteStatus      = TRUE;

	for(i=0; i < AUD_ADEC_MAX; i++)
	{
		memset(&adec_info[i], 0,  sizeof(HAL_AUDIO_ADEC_INFO_T));
        memset(&adec_status[i], 0,  sizeof(HAL_AUDIO_ADEC_STATUS_T));
		SetDecDrcMode(i, HAL_AUDIO_DOLBY_DRC_OFF);// initial
		SetDecDownMixMode(i, HAL_AUDIO_LTRT_MODE);
        adec_status[i].Gain = ALSA_GAIN_0dB;
	}

	Aud_ClipDecoderCallBack = NULL;

	for(i=0; i < AUD_AMIX_MAX; i++) // max is index 7
		Aud_ClipAmixerCallBack[i] = NULL;
}

static void InitialADCStatus(void)
{
    int i;
    for(i = 0; i < MAIN_AIN_ADC_PIN; i++)
    {
        gAinStatus.ainPinStatus[i] = HAL_AUDIO_RESOURCE_CLOSE;
    }
}

static void InitialDecStatus(void)
{
    int i,j;
    HAL_AUDIO_VOLUME_T default_in_volume = {.mainVol = 0x0, .fineVol = 0x0}; // same as fw default value.
    HAL_AUDIO_VOLUME_T default_out_volume = {.mainVol = 0x7F, .fineVol = 0x0}; // remain 0db, not using.
    for(i = 0; i < AUD_ADEC_MAX; i++)
    {
        SetSPDIFOutType(i, HAL_AUDIO_SPDIF_PCM);
        //SetTrickPause(i, FALSE);
        SetTrickState(i, HAL_AUDIO_TRICK_NONE);

        SetDecInMute(i, FALSE);
        SetDecESMute(i, FALSE);
        SetDecInVolume(i, default_in_volume);
        for(j = 0; j < AUD_MAX_CHANNEL; j++)
            SetDecOutVolume(i, j, default_out_volume);
    }
}

static DTV_STATUS_T CreateAINFilter(void)
{
    Aud_MainAin = new_AIN();
    if(Aud_MainAin == NULL)
    {
        AUDIO_ERROR("create main AIN failed\n");
        return NOT_OK;
    }
    Aud_SubAin = new_AIN();
    if(Aud_SubAin == NULL)
    {
        AUDIO_ERROR("create sub AIN failed\n");
        return NOT_OK;
    }
    return OK;
}

static void DeleteAINFilter(void)
{
    AUDIO_DEBUG("%s \n", __FUNCTION__);

    if(Aud_SubAin != NULL)
    {
        Aud_SubAin->Stop(Aud_SubAin);
        Aud_SubAin->Delete(Aud_SubAin);
        Aud_SubAin = NULL;
    }

    if(Aud_MainAin != NULL)
    {
        Aud_MainAin->Stop(Aud_MainAin);
        Aud_MainAin->Delete(Aud_MainAin);
        Aud_MainAin = NULL;
    }
}

static DTV_STATUS_T CreateFlowManagerFilter(void)
{
    int i;
	for(i = 0; i < AUD_ADEC_MAX; i++)
	{
        if((Aud_flow[i] = new_flow()) == NULL)
        {
            AUDIO_ERROR("create audio flow DEC%d failed\n",i);
            return NOT_OK;
        }
    }
    return OK;
}

static void DeleteFlowManagerFilter(void)
{
    int i;
	for(i = 0; i < AUD_ADEC_MAX; i++)
    {
        if(Aud_flow[i] != NULL)
        {
            Aud_flow[i]->Stop(Aud_flow[i]);
            Aud_flow[i]->Delete(Aud_flow[i]);
            Aud_flow[i] = NULL;
        }
    }
}

static DTV_STATUS_T CreatePPAOFilter(void)
{
    if(Aud_ppAout == NULL)
    {
        Aud_ppAout = new_PPAO(PPAO_FULL);
        if(Aud_ppAout == NULL)
        {
            AUDIO_ERROR("create audio ppao failed\n");
            return NOT_OK;
        }
    }

    if(Aud_subPPAout == NULL)
    {
        Aud_subPPAout = new_PPAO(PPAO_FULL);
        if(Aud_subPPAout == NULL)
        {
            AUDIO_ERROR("create audio sub  ppao failed\n");
            return NOT_OK;
        }
    }
    return OK;
}


static DTV_STATUS_T DeletePPAOFilter(void)
{
   AUDIO_DEBUG("%s \n", __FUNCTION__);

   if(Aud_subPPAout != NULL)
   {
       Aud_subPPAout->Stop(Aud_subPPAout);
       Aud_subPPAout->Delete(Aud_subPPAout);
       Aud_subPPAout = NULL;
   }

   if(Aud_ppAout != NULL)
   {
       Aud_ppAout->Stop(Aud_ppAout);
       Aud_ppAout->Delete(Aud_ppAout);
       Aud_ppAout = NULL;
   }

   return OK;
}

static DTV_STATUS_T CreateDecFilter(void)
{
    UINT32 i;
	for(i = 0; i < AUD_ADEC_MAX; i++)
	{
		if(Aud_dec[i] == NULL)
		{
			Aud_dec[i] = new_DEC(i);
			if(Aud_dec[i] == NULL)
			{
				AUDIO_ERROR("create audio dec %d  failed\n", i);
				return NOT_OK;
			}
		}

		Aud_DTV[i] = new_DtvCom();
		if(Aud_DTV[i] == NULL)
		{
			AUDIO_ERROR("create audio dtv dec %d failed\n", i);
			return NOT_OK;
		}
	}
    return OK;
}

static void DeleteDecFilter(void)
{
    UINT32 i;
    AUDIO_DEBUG("%s \n", __FUNCTION__);

    for(i = 0; i < AUD_ADEC_MAX; i++)
    {
        if(Aud_dec[i] != NULL)
        {
            Aud_dec[i]->Stop(Aud_dec[i]);
            Aud_dec[i]->Delete(Aud_dec[i]);
            Aud_dec[i] = NULL;
        }

        if(Aud_DTV[i] != NULL)
        {
            Aud_DTV[i]->Stop(Aud_DTV[i]);
            Aud_DTV[i]->Delete(Aud_DTV[i]);
            Aud_DTV[i] = NULL;
        }

        if(Aud_ESPlay[i] != NULL)
        {
            Aud_ESPlay[i]->Stop(Aud_ESPlay[i]);
            Aud_ESPlay[i]->Delete(Aud_ESPlay[i]);
            Aud_ESPlay[i] = NULL;
        }
    }
}

static DTV_STATUS_T CreateEncFilter(void)
{
    int i;
    for(i = 0; i < AUD_AENC_MAX; i++)
    {
        if(Aud_aenc[i] == NULL)
        {
            Aud_aenc[i] = (RHAL_AUDIO_AENC_T*)kmalloc(sizeof(RHAL_AUDIO_AENC_T), (GFP_KERNEL));

            if(Aud_aenc[i] == NULL)
                return NOT_OK;

            memset(Aud_aenc[i], 0, sizeof(RHAL_AUDIO_AENC_T));

            Aud_aenc[i]->index        = i;
            Aud_aenc[i]->pEnc         = new_ENC(AUDIO_AAC_ENCODER);
            Aud_aenc[i]->pMemOut      = new_MemOut();

            Aud_aenc[i]->info.codec   = (HAL_AUDIO_AENC_ENCODING_FORMAT_T)-1;
            Aud_aenc[i]->info.status  = HAL_AUDIO_AENC_STATUS_STOP;
            Aud_aenc[i]->info.channel = HAL_AUDIO_AENC_STEREO;
            Aud_aenc[i]->info.bitrate = HAL_AUDIO_AENC_BIT_48K;

        }
    }

    return OK;
}

static void DeleteEncFilter(void)
{
    int i;
    for(i = 0; i < AUD_AENC_MAX; i++)
    {
        if(Aud_aenc[i] != NULL)
        {
#if 0
            if(Aud_aenc[i]->pThread != NULL)
            {
                if(Aud_aenc[i]->pThread->IsRun(Aud_aenc[i]->pThread))
                    Aud_aenc[i]->pThread->Exit(Aud_aenc[i]->pThread, TRUE);
            }

            if(Aud_aenc[i]->pThread != NULL) Aud_aenc[i]->pThread->Delete(Aud_aenc[i]->pThread);
#endif

            if(Aud_aenc[i]->pEnc    != NULL)
            {
                ((ENC*)Aud_aenc[i]->pEnc->pDerivedObj)->StopEncode(Aud_aenc[i]->pEnc);
                Aud_aenc[i]->pEnc->Stop(Aud_aenc[i]->pEnc);
                Aud_aenc[i]->pEnc->Delete(Aud_aenc[i]->pEnc);
            }
            if(Aud_aenc[i]->pMemOut != NULL) Aud_aenc[i]->pMemOut->Delete(Aud_aenc[i]->pMemOut);

            kfree(Aud_aenc[i]);
            Aud_aenc[i] = NULL;
        }
    }
}

HAL_AUDIO_RESOURCE_T GetNonOutputModuleSingleInputResource(HAL_AUDIO_MODULE_STATUS resourceStatus)
{
    /*assert(resourceStatus.numIptconnect <= 1);*/
    return resourceStatus.inputConnectResourceId[0];
}

BOOLEAN IsHbbTV(HAL_AUDIO_ADEC_INDEX_T newAdecIndex, HAL_AUDIO_ADEC_INDEX_T oldAdecIndex)
{
    HAL_AUDIO_RESOURCE_T newAdecResourceId = adec2res(newAdecIndex);
    HAL_AUDIO_RESOURCE_T oldAdecResourceId = adec2res(oldAdecIndex);
    HAL_AUDIO_RESOURCE_T decIptResId = GetNonOutputModuleSingleInputResource(ResourceStatus[oldAdecResourceId]);// dec input source
    // 1 new main dec has no input
    if(ResourceStatus[newAdecResourceId].numIptconnect > 0)
    {
        pr_debug("new Adec=ADEC%d, numIptconnect=%d. \n", newAdecIndex, ResourceStatus[newAdecResourceId].numIptconnect);
        return FALSE;
    }
    // 2 new sub dec is DTV
    if(IsDTVSource(decIptResId) == FALSE)
    {
        pr_debug("old Adec=ADEC%d is not DTV. \n", oldAdecIndex);
        return FALSE;
    }
    // 3 new sub dec have output source connected
    if(ResourceStatus[oldAdecResourceId].numOptconnect == 0)
    {
        pr_debug("old Adec=ADEC%d, numOptconnect=%d. \n", oldAdecIndex, ResourceStatus[oldAdecResourceId].numOptconnect);
        return FALSE;
    }
    pr_debug("Is HbbTV case. \n");
    return TRUE;
}

BOOLEAN IsResourceRunningByProcess(HAL_AUDIO_ADEC_INDEX_T adecIndex, HAL_AUDIO_MODULE_STATUS resourceStatus)
{
    // check resource status
    if(resourceStatus.connectStatus[0] == HAL_AUDIO_RESOURCE_RUN ||
       resourceStatus.connectStatus[0] == HAL_AUDIO_RESOURCE_PAUSE)
    {
        // check if flow has module or not
        Base* pFlowBaseObj;

        switch(adecIndex)
        {
            case HAL_AUDIO_ADEC0:
                pFlowBaseObj = Aud_flow[HAL_AUDIO_ADEC0]->pBaseObj;
                break;
            case HAL_AUDIO_ADEC1:
                pFlowBaseObj = Aud_flow[HAL_AUDIO_ADEC1]->pBaseObj;
                break;
#if defined(VIRTUAL_ADEC_ENABLED)
            case HAL_AUDIO_ADEC2:
                pFlowBaseObj = Aud_flow[HAL_AUDIO_ADEC2]->pBaseObj;
                break;
            case HAL_AUDIO_ADEC3:
                pFlowBaseObj = Aud_flow[HAL_AUDIO_ADEC3]->pBaseObj;
                break;
#endif
            default:
                return FALSE;
        }
        if(list_empty(&pFlowBaseObj->flowList))
        {
            AUDIO_INFO("[%s] ADEC%d is running but no module found in FlowManager", __FUNCTION__, adecIndex);
            return FALSE;
        }
        else
            return TRUE;
    }

    return FALSE;
}

static int GetConnectInputSourceIndex(HAL_AUDIO_MODULE_STATUS resourceStatus, HAL_AUDIO_RESOURCE_T searchId)
{
	int i;
	// check if reconnect
	for(i = 0; i< resourceStatus.numIptconnect; i++) {
		if(resourceStatus.inputConnectResourceId[i] == searchId) {
			return i;
		}
	}
	return -1;
}

static BOOLEAN SetConnectSourceAndStatus(HAL_AUDIO_MODULE_STATUS resourceStatus[HAL_MAX_RESOURCE_NUM],
                                      HAL_AUDIO_RESOURCE_T currentConnect,
                                      HAL_AUDIO_RESOURCE_T inputConnect)
{
    int i, index;
    // set current module's input id
    index = resourceStatus[currentConnect].numIptconnect;
    // check if has been connected
    i = GetConnectInputSourceIndex(resourceStatus[currentConnect], inputConnect);
	if(i != -1) {
		AUDIO_ERROR("[OnConnect] %s -- %s.in(%d) exist, return fail\n",
				GetResourceString(inputConnect), GetResourceString(currentConnect), i);
        return FALSE;
    }
    resourceStatus[currentConnect].inputConnectResourceId[index] = inputConnect;
    resourceStatus[currentConnect].connectStatus[index]          = HAL_AUDIO_RESOURCE_CONNECT;
    resourceStatus[currentConnect].numIptconnect++;

    if(HAL_AUDIO_RESOURCE_NO_CONNECTION != inputConnect)
	{
		index = resourceStatus[inputConnect].numOptconnect;
		if(index >= HAL_DEC_MAX_OUTPUT_NUM) {
			AUDIO_FATAL("[OnConnect] Outputs of %s = (%d) over maximum\n", GetResourceString(inputConnect), index);
			return FALSE;
		}
		resourceStatus[inputConnect].outputConnectResourceID[index] = currentConnect;
		resourceStatus[inputConnect].numOptconnect++;
	}
    return TRUE;
}

static HAL_AUDIO_RESOURCE_T GetInputConnect(HAL_AUDIO_MODULE_STATUS resourceStatus)
{
    int i;
    for(i = 0; i< resourceStatus.numIptconnect; i++) {
        if(resourceStatus.inputConnectResourceId[i] != HAL_AUDIO_RESOURCE_NO_CONNECTION) {
            return resourceStatus.inputConnectResourceId[i];
        }
    }
    return HAL_AUDIO_RESOURCE_NO_CONNECTION;
}

static BOOLEAN inline SetResourceOpen(HAL_AUDIO_RESOURCE_T id)
{
    int i = 0;
    for(i = 0; i < HAL_MAX_OUTPUT; i++) {
        if((ResourceStatus[id].connectStatus[i] != HAL_AUDIO_RESOURCE_CLOSE)) {
            AUDIO_INFO("[OnOpen] %s status=%s, can't open again\n",
                    GetResourceString(id), GetResourceStatusString(ResourceStatus[id].connectStatus[0]));
            return FALSE;
        }
    }
    for(i = 0; i < HAL_MAX_OUTPUT; i++) {
        ResourceStatus[id].connectStatus[i] = HAL_AUDIO_RESOURCE_OPEN;
    }
    return TRUE;
}

static BOOLEAN inline SetResourceClose(HAL_AUDIO_RESOURCE_T id)
{
	int i, statusFailed = 0;
	while(ResourceStatus[id].numIptconnect > 0 || ResourceStatus[id].numOptconnect > 0) {
		if(ResourceStatus[id].connectStatus[0] == HAL_AUDIO_RESOURCE_CONNECT) {
			AUDIO_INFO("[OnClose] %s -- %s.in, disconnect it\n",
					GetResourceString(GetInputConnect(ResourceStatus[id])), GetResourceString(id));
			CleanConnectInputSourceAndStatus(ResourceStatus, id, GetInputConnect(ResourceStatus[id]));
		}
		if(ResourceStatus[id].numOptconnect > 0 && ResourceStatus[id].numIptconnect == 0) {
			int output_res = ResourceStatus[id].outputConnectResourceID[0];
			AUDIO_INFO("[OnClose] %s -- %s, still connected\n",
					GetResourceString(id), GetResourceString(output_res));
			break;
		}
	}
	for(i = 0; i < HAL_MAX_OUTPUT; i++) {
		switch(ResourceStatus[id].connectStatus[i])
		{
			case HAL_AUDIO_RESOURCE_OPEN:
			case HAL_AUDIO_RESOURCE_DISCONNECT:
				break;
			default:
				statusFailed = 1;
				break;
		}
		ResourceStatus[id].connectStatus[i] = HAL_AUDIO_RESOURCE_CLOSE;
	}
	if(statusFailed) {
		AUDIO_INFO("[OnClose] %s status=%s, return fail\n",
				GetResourceString(id), GetResourceStatusString(ResourceStatus[id].connectStatus[0]));
		return FALSE;
	}
	return TRUE;
}

static BOOLEAN inline SetResourceConnect(HAL_AUDIO_RESOURCE_T id, HAL_AUDIO_RESOURCE_T inputConnect)
{
    int i;
    if(IsAOutSource(id)) //  connect according to pin
    {
        // check nerver been connected
        if((i = GetConnectInputSourceIndex(ResourceStatus[id], inputConnect)) != -1) {
			AUDIO_INFO("[OnConnect] %s -- %s.in(%d), return fail\n",
					GetResourceString(inputConnect), GetResourceString(id), i);
            return FALSE;
        }
        if((ResourceStatus[id].connectStatus[0] == HAL_AUDIO_RESOURCE_CLOSE)) {
            AUDIO_INFO("[OnConnect] %s status=%s, return fail\n",GetResourceString(id),
                    GetResourceStatusString((int)(ResourceStatus[id].connectStatus[0])));
            return FALSE;
        }
    }
    else
	{
        // normal case only need to check  connectStatus[0]
		switch(ResourceStatus[id].connectStatus[0])
		{
			case HAL_AUDIO_RESOURCE_OPEN:
			case HAL_AUDIO_RESOURCE_DISCONNECT:
				break;
			default:
				AUDIO_INFO("[OnConnect] %s status=%s, return fail\n",
						GetResourceString(id), GetResourceStatusString(ResourceStatus[id].connectStatus[0]));
				return FALSE;
        }
    }
    return SetConnectSourceAndStatus(ResourceStatus, id, inputConnect);
}

static BOOLEAN inline SetResourceDisconnect(HAL_AUDIO_RESOURCE_T id, HAL_AUDIO_RESOURCE_T inputConnect)
{
    int i;
    if((i = GetConnectInputSourceIndex(ResourceStatus[id], inputConnect)) == -1) {
        AUDIO_ERROR("[OnDisconnect] %s --x-- %s\n", GetResourceString(id), GetResourceString(inputConnect));
        return FALSE;
    }
    CleanConnectInputSourceAndStatus(ResourceStatus, id, inputConnect);
    return TRUE;
}

BOOLEAN ADEC_SPK_CheckOpen(void)
{
    int i = 0;

    /* check SPK open status */
    for(i = 0; i < HAL_MAX_OUTPUT; i++) {
        if(ResourceStatus[HAL_AUDIO_RESOURCE_OUT_SPK].connectStatus[i] == HAL_AUDIO_RESOURCE_OPEN) {
            return TRUE;
        }
    }

    return FALSE;
}

/*! Check a Speaker Connect Status to mute Speaker Output.
 */
BOOLEAN ADEC_SPK_CheckConnect(void)
{
    HAL_AUDIO_RESOURCE_T inputIDArr[HAL_MAX_OUTPUT];
    int num_of_inputs = 0;

    /* check SPK input status */
    GetInputConnectSource(HAL_AUDIO_RESOURCE_OUT_SPK, inputIDArr, HAL_MAX_OUTPUT, &num_of_inputs);

    return (num_of_inputs > 0);
}

BOOLEAN ADEC_SPDIF_CheckOpen(void)
{
    int i = 0;

    /* check SPDIF open status */
    for(i = 0; i < HAL_MAX_OUTPUT; i++) {
        if(ResourceStatus[HAL_AUDIO_RESOURCE_OUT_SPDIF].connectStatus[i] == HAL_AUDIO_RESOURCE_OPEN) {
            return TRUE;
        }
    }

    return FALSE;
}

/*! Check a SPDIF Connect Status to mute SPDIF Output.
 */
BOOLEAN ADEC_SPDIF_CheckConnect(void)
{
    HAL_AUDIO_RESOURCE_T inputIDArr[HAL_MAX_OUTPUT];
    int num_of_inputs = 0;

    /* check SPDIF input status */
    GetInputConnectSource(HAL_AUDIO_RESOURCE_OUT_SPDIF, inputIDArr, HAL_MAX_OUTPUT, &num_of_inputs);

    return (num_of_inputs > 0);
}

BOOLEAN ADEC_SB_SPDIF_CheckOpen(void)
{
    int i = 0;

    /* check SB_SPDIF open status */
    for(i = 0; i < HAL_MAX_OUTPUT; i++) {
        if(ResourceStatus[HAL_AUDIO_RESOURCE_OUT_SB_SPDIF].connectStatus[i] == HAL_AUDIO_RESOURCE_OPEN) {
            return TRUE;
        }
    }

    return FALSE;
}

/*! Check a SB_SPDIF Connect Status to mute SPDIF Output.
 */
BOOLEAN ADEC_SB_SPDIF_CheckConnect(void)
{
    HAL_AUDIO_RESOURCE_T inputIDArr[HAL_MAX_OUTPUT];
    int num_of_inputs = 0;

    /* check SPDIF SB input status */
    GetInputConnectSource(HAL_AUDIO_RESOURCE_OUT_SB_SPDIF, inputIDArr, HAL_MAX_OUTPUT, &num_of_inputs);

    return (num_of_inputs > 0);
}

BOOLEAN ADEC_HP_CheckOpen(void)
{
    int i = 0;

    /* check HP open status */
    for(i = 0; i < HAL_MAX_OUTPUT; i++) {
        if(ResourceStatus[HAL_AUDIO_RESOURCE_OUT_HP].connectStatus[i] == HAL_AUDIO_RESOURCE_OPEN) {
            return TRUE;
        }
    }

    return FALSE;
}

/*! Check a HP Connect Status to mute HP Output.
 */
BOOLEAN ADEC_HP_CheckConnect(void)
{
    HAL_AUDIO_RESOURCE_T inputIDArr[HAL_MAX_OUTPUT];
    int num_of_inputs = 0;

    /* check HP input status */
    GetInputConnectSource(HAL_AUDIO_RESOURCE_OUT_HP, inputIDArr, HAL_MAX_OUTPUT, &num_of_inputs);

    return (num_of_inputs > 0);
}

BOOLEAN ADEC_ARC_CheckOpen(void)
{
    int i = 0;

    /* check ARC open status */
    for(i = 0; i < HAL_MAX_OUTPUT; i++) {
        if(ResourceStatus[HAL_AUDIO_RESOURCE_OUT_ARC].connectStatus[i] == HAL_AUDIO_RESOURCE_OPEN) {
            return TRUE;
        }
    }

    return FALSE;
}

/*! Check a ARC Connect Status to mute ARC Output.
 */
BOOLEAN ADEC_ARC_CheckConnect(void)
{
    HAL_AUDIO_RESOURCE_T inputIDArr[HAL_MAX_OUTPUT];
    int num_of_inputs = 0;

    /* check ARC input status */
    GetInputConnectSource(HAL_AUDIO_RESOURCE_OUT_ARC, inputIDArr, HAL_MAX_OUTPUT, &num_of_inputs);

    return (num_of_inputs > 0);
}

/*! Check a SPDIF_ES Connect Status to mute SPDIF_ES Output.
 */
BOOLEAN ADEC_SPDIF_ES_CheckConnect(void)
{
    HAL_AUDIO_RESOURCE_T inputIDArr[HAL_MAX_OUTPUT];
    int num_of_inputs = 0;

    /* check SPDIF_ES input status */
    GetInputConnectSource(HAL_AUDIO_RESOURCE_OUT_SPDIF_ES, inputIDArr, HAL_MAX_OUTPUT, &num_of_inputs);

    if (num_of_inputs > 0) // has connected
    {
        return TRUE;
    }

    /* check SPDIF SB input status */
    GetInputConnectSource(HAL_AUDIO_RESOURCE_OUT_SB_SPDIF, inputIDArr, HAL_MAX_OUTPUT, &num_of_inputs);

    return (num_of_inputs > 0);
}

BOOLEAN ADEC_Statue_CheckConnect(HAL_AUDIO_ADEC_INDEX_T adecIndex)
{
    HAL_AUDIO_RESOURCE_T res_id = adec2res(adecIndex);
    HAL_AUDIO_RESOURCE_T outputIDArr[HAL_MAX_OUTPUT];
    int num_of_outputs = 0;

    GetConnectOutputSource(res_id, outputIDArr, HAL_MAX_OUTPUT, &num_of_outputs);

    return (num_of_outputs > 0);
}

/*! Check ADEC Connect Status to auto-mute DecoderInput
 */
BOOLEAN ADEC_CheckConnect(HAL_AUDIO_ADEC_INDEX_T adecIndex)
{
#ifdef DEC_AUTO_MUTE
    return ADEC_Statue_CheckConnect(adecIndex);
#else
    return TRUE;
#endif
}

BOOLEAN AMIXER_Status_CheckConnect(HAL_AUDIO_RESOURCE_T amixIndex)
{
    HAL_AUDIO_RESOURCE_T outputIDArr[HAL_MAX_OUTPUT];
    int num_of_outputs = 0;

    GetConnectOutputSource(amixIndex, outputIDArr, HAL_MAX_OUTPUT, &num_of_outputs);

    return (num_of_outputs > 0);
}

BOOLEAN AMIXER_CheckConnect(HAL_AUDIO_RESOURCE_T amixIndex)
{
#ifdef AMIXER_AUTO_MUTE
    return AMIXER_Status_CheckConnect(amixIndex);
#else
    return TRUE;
#endif
}

HAL_AUDIO_RESOURCE_T ADEC_GetConnectedATP(HAL_AUDIO_ADEC_INDEX_T adecIndex)
{
    HAL_AUDIO_RESOURCE_T adec_res_id = adec2res(adecIndex);
    HAL_AUDIO_RESOURCE_T output_id;
    int num_of_outputs = 0;
    int i;
    //use ATP output to lookup which adec is connected to
    for(i=0 ; i<2 ; i++)
    {
        HAL_AUDIO_RESOURCE_T atp_res = (HAL_AUDIO_RESOURCE_T)(i+(int)HAL_AUDIO_RESOURCE_ATP0);
        GetConnectOutputSource(atp_res, &output_id, 1, &num_of_outputs);
        if(output_id == adec_res_id)
            return atp_res;
    }

    return HAL_AUDIO_RESOURCE_NO_CONNECTION;
}

/*! Get ADEC volume setting by DecInVol, DecOutVol, DecAdsp_gain
 */
void ADEC_Calculate_DSPGain(UINT32 adecIndex, SINT32 dsp_gain[AUD_MAX_CHANNEL])
{
    int i;
    HAL_AUDIO_VOLUME_T tempVol = GetDecInVolume(adecIndex);

    /* Apply DecAdsp_gain for sub decoder */
    if(Aud_descriptionMode & (adecIndex != (UINT32)Aud_mainDecIndex))
    {
        tempVol = Volume_Add(tempVol, currDecADVol);
        AUDIO_DEBUG("AD dec in vol 0x%x dec out vol[0] 0x%x  ad vol 0x%x\n", ((HAL_AUDIO_VOLUME_T)(GetDecInVolume(adecIndex))).mainVol, ((HAL_AUDIO_VOLUME_T)(GetDecOutVolume(adecIndex, 0))).mainVol, currDecADVol.mainVol);
    }
    else
    {
        AUDIO_DEBUG("Main dec in vol 0x%x dec out vol[0] 0x%x\n",((HAL_AUDIO_VOLUME_T)(GetDecInVolume(adecIndex))).mainVol, ((HAL_AUDIO_VOLUME_T)(GetDecOutVolume(adecIndex, 0))).mainVol);
    }

    for(i = 0; i < AUD_MAX_CHANNEL; i++)
    {
        dsp_gain[i] = Volume_to_DSPGain(Volume_Add(GetDecOutVolume(adecIndex, i), tempVol));
    }
    return;
}


/****************************************************************************************
 * Static Audio DSP communication functions
 ****************************************************************************************/
static UINT32 SendRPC_AudioConfig(AUDIO_CONFIG_COMMAND_RTKAUDIO *audioConfig)
{
    UINT32 ret;
    ret = (UINT32)rtkaudio_send_audio_config(audioConfig);

    if(ret != S_OK){
        AUDIO_ERROR("%s ret!=S_OK, %x\n",__FUNCTION__,ret);
    }

    return ret;
}

static UINT32 ADSP_SNDOut_AddDevice(common_output_ext_type_t opt_id)
{
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;
    if(Sndout_Devices & opt_id) {
        AUDIO_DEBUG("output type %d already open\n",opt_id);
        return S_OK;
    }
    Sndout_Devices |= opt_id;

    audioConfig.msg_id = AUDIO_CONFIG_CMD_SNDOUT_DEVICE;
    audioConfig.value[0] = Sndout_Devices;
    if(SendRPC_AudioConfig(&audioConfig) != S_OK) return NOT_OK;
    return S_OK;
}

static UINT32 ADSP_SNDOut_RemoveDevice(common_output_ext_type_t opt_id)
{
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;
    if((Sndout_Devices & opt_id) == 0) {
        AUDIO_DEBUG("output type %d already close\n",opt_id);
        return S_OK;
    }
    Sndout_Devices &= (~opt_id);

    audioConfig.msg_id = AUDIO_CONFIG_CMD_SNDOUT_DEVICE;
    audioConfig.value[0] = Sndout_Devices;
    if(SendRPC_AudioConfig(&audioConfig) != S_OK) return NOT_OK;
    return S_OK;
}

static UINT32 ADSP_DEC_SetDelay(UINT32 index, UINT32 delay)
{
    UINT32 i;
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;
    Base* pPPAO;
    memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));

    if(delay > AUD_MAX_ADEC_DELAY)
    {
        AUDIO_ERROR("Set (ADEC%d) delay %d ms, exceed max delayTime\n", index, delay);
        return NOT_OK;
    }
    if(index == (UINT32)Aud_mainDecIndex)
        pPPAO = Aud_ppAout;
    else
        pPPAO = Aud_subPPAout;

    audioConfig.msg_id = AUDIO_CONFIG_CMD_SET_DELAY;
    audioConfig.value[0] = pPPAO->GetAgentID(pPPAO);
    audioConfig.value[1] = ENUM_DEVICE_DECODER;
    audioConfig.value[2] = pPPAO->GetInPinID(pPPAO);
    audioConfig.value[3] = 0xFF;
    for(i = 0; i < AUD_MAX_CHANNEL; i++)
        audioConfig.value[4+i] = delay;

    if(SendRPC_AudioConfig(&audioConfig) != S_OK) return NOT_OK;

    return S_OK;
}

static UINT32 ADSP_DEC_SetVolume(UINT32 index, int ch_vol[AUD_MAX_CHANNEL])
{
    UINT32 i, update = FALSE;
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;

    memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));

    audioConfig.msg_id = AUDIO_CONFIG_CMD_SET_VOLUME;
    audioConfig.value[0] = Aud_dec[index]->GetAgentID(Aud_dec[index]);
    //audioConfig.value[1] = ENUM_DEVICE_DECODER;
    switch(index)
    {
        case 0:
            audioConfig.value[1] = ENUM_DEVICE_DECODER0;
            break;
        case 1:
            audioConfig.value[1] = ENUM_DEVICE_DECODER1;
            break;
        case 2:
            audioConfig.value[1] = ENUM_DEVICE_DECODER2;
            break;
        case 3:
            audioConfig.value[1] = ENUM_DEVICE_DECODER3;
            break;
        default:
            break;
    }

    audioConfig.value[2] = 0;
    audioConfig.value[3] = 0xFF;
    if(index < AUD_ADEC_MAX)
        for(i = 0; i < AUD_MAX_CHANNEL; i++)
        {
            if(ch_vol[i] != (int)gAudioDecInfo[index].decVol[i])
                update = TRUE;
            audioConfig.value[4+i] = ch_vol[i];
            gAudioDecInfo[index].decVol[i] = ch_vol[i];
            AUDIO_DEBUG("adec %d channel %d = %x  \n", index, i, ch_vol[i]);

        }
#ifdef AVOID_USELESS_RPC
    if(update == FALSE) return S_OK;
#endif
    if(SendRPC_AudioConfig(&audioConfig) != S_OK) return NOT_OK;

    return S_OK;
}

static UINT32 ADSP_DEC_SetFineVolume(UINT32 index, int ch_vol[AUD_MAX_CHANNEL], UINT32 dsp_fine)
{
    UINT32 i, update = FALSE;
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;

    memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));

    audioConfig.msg_id = AUDIO_CONFIG_CMD_SET_VOLUME;
    audioConfig.value[0] = Aud_dec[index]->GetAgentID(Aud_dec[index]);
    switch(index)
    {
        case 0:
            audioConfig.value[1] = ENUM_DEVICE_DECODER0;
            break;
        case 1:
            audioConfig.value[1] = ENUM_DEVICE_DECODER1;
            break;
        case 2:
            audioConfig.value[1] = ENUM_DEVICE_DECODER2;
            break;
        case 3:
            audioConfig.value[1] = ENUM_DEVICE_DECODER3;
            break;
        default:
            break;
    }

    audioConfig.value[2] = 0;
    audioConfig.value[3] = 0xFF;
    if(index < AUD_ADEC_MAX)
        for(i = 0; i < AUD_MAX_CHANNEL; i++)
        {
            if((ch_vol[i] != (int)gAudioDecInfo[index].decVol[i]) || dsp_fine != (int)gAudioDecInfo[index].decFine[i])
                update = TRUE;
            audioConfig.value[4+i] = ch_vol[i];
            gAudioDecInfo[index].decVol[i]  = ch_vol[i];
            gAudioDecInfo[index].decFine[i] = dsp_fine;
            AUDIO_DEBUG("adec %d channel %d = %x  , fine %x\n", index, i, ch_vol[i], dsp_fine);
        }

    audioConfig.value[12] = dsp_fine%4;  // 0~3

#ifdef AVOID_USELESS_RPC
    if(update == FALSE) return S_OK;
#endif
    if(SendRPC_AudioConfig(&audioConfig) != S_OK) return NOT_OK;

    return S_OK;
}

static UINT32 ADSP_DEC_GetMute(UINT32 index)
{
    return gAudioDecInfo[index].decMute;
}

static UINT32 ADSP_DEC_SetMute(UINT32 index, BOOLEAN bConnected, BOOLEAN bMute)
{
    UINT32 i;
    BOOLEAN bAutoMute;
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;
    memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));

    bAutoMute = (bConnected)? FALSE : TRUE;

#ifdef AVOID_USELESS_RPC
    if((bAutoMute|bMute) == ADSP_DEC_GetMute(index))
    {
        AUDIO_DEBUG("Skip this time mute\n");
        return S_OK;
    }
#endif

    AUDIO_INFO("Set (ADEC%d) %s [AutoMute(%s)|UserMute(%s)]\n", index,
              (bAutoMute|bMute)? "MUTE":"UN-MUTE", bAutoMute? "ON":"OFF", bMute? "ON":"OFF");

    audioConfig.msg_id = AUDIO_CONFIG_CMD_SET_MUTE;
    audioConfig.value[0] = Aud_dec[index]->GetAgentID(Aud_dec[index]);
    //audioConfig.value[1] = ENUM_DEVICE_DECODER;
    switch(index)
    {
        case 0:
            audioConfig.value[1] = ENUM_DEVICE_DECODER0;
            break;
        case 1:
            audioConfig.value[1] = ENUM_DEVICE_DECODER1;
            break;
        case 2:
            audioConfig.value[1] = ENUM_DEVICE_DECODER2;
            break;
        case 3:
            audioConfig.value[1] = ENUM_DEVICE_DECODER3;
            break;
        default:
            break;
    }

    audioConfig.value[2] = 0;
    audioConfig.value[3] = 0xFF;
    for(i = 0; i < 8; i++)
        audioConfig.value[4+i] = (bAutoMute|bMute);

    if(SendRPC_AudioConfig(&audioConfig) != S_OK) return NOT_OK;

    gAudioDecInfo[index].decMute = (bAutoMute|bMute);
    return S_OK;
}

static UINT32 ADSP_DEC_SetHdmiFmt(UINT32 index, AUDIO_DEC_TYPE dec_type)
{
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;
    memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));

    AUDIO_INFO("ADSP_DEC_SetHdmiFmt=%d=\n", dec_type);
    audioConfig.msg_id = AUDIO_CONFIG_CMD_NTFY_HAL_HDMI_FMT;
    audioConfig.value[0] = Aud_dec[index]->GetAgentID(Aud_dec[index]);
    switch(index)
    {
    case 0:
        audioConfig.value[1] = ENUM_DEVICE_DECODER0;
        break;
    case 1:
        audioConfig.value[1] = ENUM_DEVICE_DECODER1;
        break;
    case 2:
        audioConfig.value[1] = ENUM_DEVICE_DECODER2;
        break;
    case 3:
        audioConfig.value[1] = ENUM_DEVICE_DECODER3;
        break;
    default:
        break;
    }
    audioConfig.value[2] = 0;
    audioConfig.value[3] = 0xff;
    audioConfig.value[4] = (u_int)(dec_type);
    audioConfig.value[6] = 1 << 7; // hdmi mode

    if(SendRPC_AudioConfig(&audioConfig) != S_OK) {
        return NOT_OK;
    }
    return S_OK;
}

static UINT32 ADSP_TSK_GetStarted(void)
{
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;
    memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));

    audioConfig.msg_id = AUDIO_CONFIG_CMD_GET_TASK_STARTED;

    return rtkaudio_send_audio_config(&audioConfig);
}

static BOOLEAN ADSP_AMIXER_GetMute(UINT32 mixerIndex)
{
    /*assert(mixerIndex < AUD_AMIX_MAX);*/
    return  g_mixer_curr_mute[mixerIndex] ;
}

static UINT32 ADSP_AMIXER_SetMute(HAL_AUDIO_MIXER_INDEX_T mixerIndex, BOOLEAN bConnected, BOOLEAN bMute)
{
    UINT32 i;
    BOOLEAN bAutoMute;
    /*assert(mixerIndex < AUD_AMIX_MAX);*/
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;
    memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));

    bAutoMute = (bConnected)? FALSE : TRUE;
#ifdef AVOID_USELESS_RPC
    if((bAutoMute|bMute) == ADSP_AMIXER_GetMute(mixerIndex))
    {
        return S_OK;
    }
#endif
    AUDIO_INFO("Set (AMIXER%d) %s [AutoMute(%s)|UserMute(%s)]\n", mixerIndex,
            (bAutoMute|bMute)? "MUTE":"UN-MUTE", bAutoMute? "ON":"OFF", bMute? "ON":"OFF");

    audioConfig.msg_id = AUDIO_CONFIG_CMD_SET_MUTE;
    audioConfig.value[0] = ((PPAO*)Aud_ppAout->pDerivedObj)->GetAOAgentID(Aud_ppAout);
    audioConfig.value[1] = ENUM_DEVICE_FLASH_PIN;
    audioConfig.value[2] = (UINT32)mixerIndex;
    audioConfig.value[3] = 0xFF;

    for(i = 0; i < 8; i++)
    {
        audioConfig.value[4+i] = (bAutoMute|bMute);
    }

    if(SendRPC_AudioConfig(&audioConfig) != S_OK) return NOT_OK;

    g_mixer_curr_mute[mixerIndex]= (bAutoMute|bMute);
    return S_OK;
}

static UINT32 ADSP_DEC_SetADMode(BOOLEAN bOnOff, UINT32 mainIndex, UINT32 subStreamId)
{
    DUAL_DEC_INFO decADMode;
    UINT32 info_type = INFO_AUDIO_MIX_INFO;
    int i;

    decADMode.bEnable     = bOnOff;
    decADMode.subStreamId = subStreamId;
    for(i = 0; i < AUD_ADEC_MAX; i++)
    {
        decADMode.subDecoderMode = (bOnOff && i != mainIndex)? DEC_IS_SUB : DEC_IS_MAIN;
        Aud_dec[i]->PrivateInfo(Aud_dec[i], info_type, (BYTE*)&decADMode, sizeof(decADMode));
    }
    return S_OK;
}


static BOOLEAN GetCurrSNDOutMute(UINT32 dev_id)
{
    switch(dev_id)
    {
        case ENUM_DEVICE_SPEAKER:
            return adsp_sndout_info.spk_mute;
        case ENUM_DEVICE_SPDIF:
            return adsp_sndout_info.spdif_mute;
        case ENUM_DEVICE_SPDIF_ES:
            return adsp_sndout_info.spdifes_mute;
        case ENUM_DEVICE_HEADPHONE:
            return adsp_sndout_info.hp_mute;
        case ENUM_DEVICE_SCART:
            return adsp_sndout_info.scart_mute;
        default:
            return FALSE;
    }
}

static void SetCurrSNDOutMute(UINT32 dev_id, BOOLEAN mute)
{
    switch(dev_id)
    {
        case ENUM_DEVICE_SPEAKER:
            adsp_sndout_info.spk_mute = mute; break;
        case ENUM_DEVICE_SPDIF:
            adsp_sndout_info.spdif_mute = mute; break;
        case ENUM_DEVICE_SPDIF_ES:
            adsp_sndout_info.spdifes_mute = mute; break;
        case ENUM_DEVICE_HEADPHONE:
            adsp_sndout_info.hp_mute = mute; break;
        case ENUM_DEVICE_SCART:
            adsp_sndout_info.scart_mute = mute; break;
        default:
            return;
    }
    return;
}

static UINT32 GetCurrSNDOutDelay(UINT32 dev_id)
{
    switch(dev_id)
    {
        case ENUM_DEVICE_SPEAKER:
            return adsp_sndout_info.spk_delay;
        case ENUM_DEVICE_SPDIF:
            return adsp_sndout_info.spdif_delay;
        case ENUM_DEVICE_SPDIF_ES:
            return adsp_sndout_info.spdifes_delay;
        case ENUM_DEVICE_HEADPHONE:
            return adsp_sndout_info.hp_delay;
        case ENUM_DEVICE_SCART:
            return adsp_sndout_info.scart_delay;
        default:
            return FALSE;
    }
}

static void SetCurrSNDOutDelay(UINT32 dev_id, UINT32 delay)
{
    switch(dev_id)
    {
        case ENUM_DEVICE_SPEAKER:
            adsp_sndout_info.spk_delay = delay; break;
        case ENUM_DEVICE_SPDIF:
            adsp_sndout_info.spdif_delay = delay; break;
        case ENUM_DEVICE_SPDIF_ES:
            adsp_sndout_info.spdifes_delay = delay; break;
        case ENUM_DEVICE_HEADPHONE:
            adsp_sndout_info.hp_delay = delay; break;
        case ENUM_DEVICE_SCART:
            adsp_sndout_info.scart_delay = delay; break;
        default:
            return;
    }
    return;
}

static UINT32 ADSP_SNDOut_SetMute(UINT32 dev_id,  BOOLEAN bMute)
{
    UINT32 i;
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;
    char dev_name[ENUM_DEVICE_MAX][8] = {
        "NONE", "ENC", "DEC", "SPDIF", "SPK", "HP", "SCART", "PCMCap", "Mixer", "Flash", "SPDIFES", "DEC0", "DEC1"};
    memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));


    AUDIO_DEBUG("Set (%s) %s \n", dev_name[dev_id],(bMute)? "MUTE":"UN-MUTE" );

    audioConfig.msg_id = AUDIO_CONFIG_CMD_SET_MUTE;
    audioConfig.value[0] = ((PPAO*)Aud_ppAout->pDerivedObj)->GetAOAgentID(Aud_ppAout);
    audioConfig.value[1] = dev_id;
    audioConfig.value[2] = 0;
    audioConfig.value[3] = 0xFF;
    for(i = 0; i < 8; i++)
    {
        audioConfig.value[4+i] = (bMute);
    }
    SetCurrSNDOutMute(dev_id, (bMute));

    return SendRPC_AudioConfig(&audioConfig);

}

static UINT32 ADSP_Switch_OnOff_SetMuteProtect(int duration)
{
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;
    memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));

#ifdef AVOID_USELESS_RPC
    if(GetCurrSNDOutMute(ENUM_DEVICE_SPEAKER) == true)
    {
        AUDIO_DEBUG("Skip Set SPK MUTE for switch on/off protection \n");
        return S_OK;
    }
#endif

    AUDIO_INFO("Set SPK MUTE for switch on/off protection %dms\n",duration);

    audioConfig.msg_id = AUDIO_CONFIG_CMD_SET_VX_MUTE;
    audioConfig.value[0] = ((PPAO*)Aud_ppAout->pDerivedObj)->GetAOAgentID(Aud_ppAout);
    audioConfig.value[1] = ENUM_DEVICE_SPEAKER;
    audioConfig.value[2] = duration;

    return SendRPC_AudioConfig(&audioConfig);
}

static UINT32 SNDOut_SetMute(UINT32 dev_id, BOOLEAN bConnected, BOOLEAN bMute)
{
    BOOLEAN bAutoMute;
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;
    char dev_name[ENUM_DEVICE_MAX][8] = {
        "NONE", "ENC", "DEC", "SPDIF", "SPK", "HP", "SCART", "PCMCap", "Mixer", "Flash", "SPDIFES"};
    memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));

    bAutoMute = (bConnected)? FALSE : TRUE;

#ifdef AVOID_USELESS_RPC
    if((bAutoMute|bMute) == GetCurrSNDOutMute(dev_id))
    {
        AUDIO_DEBUG("Skip (%s) [AutoMute|UserMute] (%x) == GetCurrSNDOutMute(%x ) \n", dev_name[dev_id],
                (bAutoMute|bMute), GetCurrSNDOutMute(dev_id) );
        return S_OK;
    }
#endif
    AUDIO_INFO("Set (%s) %s [AutoMute(%s)|UserMute(%s)]\n", dev_name[dev_id],
            (bAutoMute|bMute)? "MUTE":"UN-MUTE", bAutoMute? "ON":"OFF", bMute? "ON":"OFF");

    return ADSP_SNDOut_SetMute(dev_id, (bAutoMute|bMute));
}

static UINT32 ADSP_SNDOut_SetDelay(UINT32 dev_id, UINT32 delay)
{
    UINT32 i;
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;
    /*char dev_name[ENUM_DEVICE_MAX][8] = {*/
        /*"NONE", "ENC", "DEC", "SPDIF", "SPK", "HP", "SCART", "PCMCap", "Mixer", "Flash"};*/
    memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));

    if(delay > AUD_MAX_SNDOUT_DELAY)
    {
        AUDIO_ERROR("Set dev_id %d delay %d ms, exceed max delayTime\n", dev_id, delay);
        return NOT_OK;
    }

    if(dev_id == ENUM_DEVICE_SPDIF) {
        /* Set SPDIF_ES delay */
        struct AUDIO_RPC_PRIVATEINFO_PARAMETERS parameter;
        AUDIO_RPC_PRIVATEINFO_RETURNVAL ret;
        memset(&parameter, 0, sizeof(struct AUDIO_RPC_PRIVATEINFO_PARAMETERS));
        memset(&ret, 0, sizeof(AUDIO_RPC_PRIVATEINFO_RETURNVAL));

        /* RAW delay */
        parameter.privateInfo[0] = KADP_AUDIO_DELAY_RAW;
        parameter.privateInfo[1] = TRUE;
        parameter.privateInfo[2] = delay; // no limitaion

        if (KADP_AUDIO_PrivateInfo(&parameter, &ret) != KADP_OK) {
            AUDIO_ERROR("[%s,%d] set es delay failed\n", __FUNCTION__, __LINE__);
            return NOT_OK;
        }
    }

    audioConfig.msg_id = AUDIO_CONFIG_CMD_SET_DELAY;
    audioConfig.value[0] = ((PPAO*)Aud_ppAout->pDerivedObj)->GetAOAgentID(Aud_ppAout);
    audioConfig.value[1] = dev_id;
    audioConfig.value[2] = 0;
    audioConfig.value[3] = 0x03;
    for(i = 0; i < 8; i++)
    {
        audioConfig.value[4+i] = delay;
    }
    SetCurrSNDOutDelay(dev_id, delay);
    return SendRPC_AudioConfig(&audioConfig);
}
static UINT32 SNDOut_SetDelay(UINT32 dev_id, UINT32 delay)
{
#ifdef AVOID_USELESS_RPC
    if(delay == GetCurrSNDOutDelay(dev_id))
    {
        return S_OK;
    }
#endif
    return ADSP_SNDOut_SetDelay(dev_id, delay);
}

static UINT32 ADSP_SNDOut_SetVolume(UINT32 dev_id, UINT32 dsp_gain)
{
    UINT32 i;
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;
    memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));

    audioConfig.msg_id = AUDIO_CONFIG_CMD_SET_VOLUME;
    audioConfig.value[0] = ((PPAO*)Aud_ppAout->pDerivedObj)->GetAOAgentID(Aud_ppAout);
    audioConfig.value[1] = dev_id;
    audioConfig.value[2] = 0;
    audioConfig.value[3] = 0xFF;
    for(i = 0; i < 8; i++)
    {
        audioConfig.value[4+i] = dsp_gain;
    }
    return SendRPC_AudioConfig(&audioConfig);
}

static UINT32 ADSP_SNDOut_SetFineVolume(UINT32 dev_id, UINT32 dsp_gain, UINT32 dsp_fine)
{
    UINT32 i;
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;
    memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));

    audioConfig.msg_id = AUDIO_CONFIG_CMD_SET_VOLUME;
    audioConfig.value[0] = ((PPAO*)Aud_ppAout->pDerivedObj)->GetAOAgentID(Aud_ppAout);
    audioConfig.value[1] = dev_id;
    audioConfig.value[2] = 0;
    audioConfig.value[3] = 0xFF;
    for(i = 0; i < 8; i++)
    {
        audioConfig.value[4+i] = dsp_gain;
    }

    audioConfig.value[12] = dsp_fine%4;  // 0~3

    return SendRPC_AudioConfig(&audioConfig);
}

/*! Set PP mixer mode and apply mixer gain to un-focus pin
 */
static UINT32 ADSP_PPMix_ConfigMixer(BOOLEAN bOnOff, UINT32 mixer_gain)
{
    DUAL_DEC_MIXING audioMixing;
    UINT32 info_type = INFO_AUDIO_START_MIXING;
    audioMixing.mode = bOnOff;

    AUDIO_DEBUG("%s  PPMix = %d mixer_gain = 0x%x \n", __FUNCTION__, bOnOff, mixer_gain);

    audioMixing.volume = mixer_gain;
    Aud_subPPAout->PrivateInfo(Aud_subPPAout, info_type, (BYTE*)&audioMixing, sizeof(audioMixing));

    /* Keep main pin (focus pin) been mixed with 0dB */
    audioMixing.volume = ADEC_DSP_MIX_GAIN_0DB;
    Aud_ppAout->PrivateInfo(Aud_ppAout, info_type, (BYTE*)&audioMixing, sizeof(audioMixing));

    return S_OK;
}

static void ADSP_SetChannelStatus(void)
{
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;
    int category_code, copy_info;

    if(ADEC_ARC_CheckConnect()) {
        category_code = g_ARCStatusInfo.curARCCategoryCode;
        copy_info = g_ARCStatusInfo.curARCCopyInfo;
    } else {
        category_code = g_AudioStatusInfo.curSpdifCategoryCode;
        copy_info = g_AudioStatusInfo.curSpdifCopyInfo;
    }

    memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));
    audioConfig.msg_id = AUDIO_CONFIG_CMD_SET_SPDIF_CS_INFO;
    audioConfig.value[0] = TRUE;
    audioConfig.value[1] = HAL_SPDIF_CONSUMER_USE;
    audioConfig.value[5] = HAL_SPDIF_WORD_LENGTH_16; // 16 bit
    audioConfig.value[3] = category_code;
    if(copy_info == SNDOUT_OPTIC_COPY_FREE)
    {
        audioConfig.value[2] = HAL_SPDIF_COPYRIGHT_FREE;
        audioConfig.value[4] = HAL_SPDIF_CATEGORY_L_BIT_IS_0;
    }
    else
    {
        audioConfig.value[2] = HAL_SPDIF_COPYRIGHT_NEVER;
        if(copy_info == SNDOUT_OPTIC_COPY_ONCE)
        {
            audioConfig.value[4] = HAL_SPDIF_CATEGORY_L_BIT_IS_0;
        }
        else
        {
            //RTWTV-247 nerver is the same with  no more
            audioConfig.value[4] = HAL_SPDIF_CATEGORY_L_BIT_IS_1;  //HAL_AUDIO_SPDIF_COPY_NEVER
        }
    }

    SendRPC_AudioConfig(&audioConfig);
    return;
}

static UINT32 ADSP_SetRawMode(int mode)
{
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;
    memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));
    audioConfig.msg_id = AUDIO_CONFIG_CMD_SPDIF;
    audioConfig.value[0] = audioConfig.value[1] = mode;
    audioConfig.value[2] = 0;
    audioConfig.value[3] = TRUE;

    if(SendRPC_AudioConfig(&audioConfig) != S_OK)
    {
        AUDIO_ERROR("[%s,%d] arc auto aac failed\n", __FUNCTION__, __LINE__);
        return NOT_OK;
    }
    return S_OK;
}

static BOOLEAN IsFlowRunningState(HAL_AUDIO_ADEC_INDEX_T adecIndex)
{
    HAL_AUDIO_RESOURCE_T curResourceId = adec2res((HAL_AUDIO_ADEC_INDEX_T)adecIndex);
    // open ADEC ready check
    if(AUDIO_HAL_CHECK_ISATRUNSTATE(ResourceStatus[curResourceId], 0))// self status is at index zero
        return TRUE;
    else
        return FALSE;
}

static void Update_RawMode_by_connection(void)
{
    if(ADEC_SB_SPDIF_CheckConnect()) {
        ADSP_SetRawMode(ENABLE_DOWNMIX);//Force to set pcm out if SB was connected
#if defined(MIXER_PORT_AUTO_PCM)
    } else if(!IsMainAudioADEC(Aud_mainAudioIndex)) {
        ADSP_SetRawMode(ENABLE_DOWNMIX);//Force to set pcm out if main audio out is mixer port
#endif
    } else if(ADEC_ARC_CheckConnect()) {
        if(HDMI_earcOnOff == 1 && g_update_by_ARC == FALSE)
        {
            ADSP_SetRawMode(_AudioEARCMode);
        }
        else if(HDMI_earcOnOff == 0 || g_update_by_ARC == TRUE)
        {
            ADSP_SetRawMode(_AudioARCMode);
            g_update_by_ARC = FALSE;
        }
    } else if(ADEC_SPDIF_CheckConnect() || ADEC_SPDIF_ES_CheckConnect()) {
        ADSP_SetRawMode(_AudioSPDIFMode);
    }

    if(ADEC_ARC_CheckConnect() && !ADEC_SPDIF_CheckConnect() && !ADEC_SPDIF_ES_CheckConnect())
    {
        KADP_AUDIO_SetSpdifOutPinSrc(KADP_AUDIO_SPDIFO_SRC_DISABLE);
    }
    else
    {
        KADP_AUDIO_SetSpdifOutPinSrc(KADP_AUDIO_SPDIFO_SRC_FIFO);
    }
}

void Switch_OnOff_MuteProtect(int duration)
{
	ADSP_Switch_OnOff_SetMuteProtect(duration);
	msleep(10); // prevent next RPC comes too fast
}

/***************** End of Audio DSP communication functions *****************************/

/* HAL index conversion */

static int ConvertSNDOUTIndexToResourceId(HAL_AUDIO_SNDOUT_T soundOutType)
{
    int id;
    switch(soundOutType)
    {
        case HAL_AUDIO_NO_OUTPUT:
        default :
            id = -1;
            AUDIO_ERROR("unknow sndout id %d\n", soundOutType);
            break;
        case HAL_AUDIO_SPK:
            id = HAL_AUDIO_RESOURCE_OUT_SPK;
            break;
        case HAL_AUDIO_SPDIF:
            id = HAL_AUDIO_RESOURCE_OUT_SPDIF;
            break;
        case HAL_AUDIO_SB_SPDIF:
            id = HAL_AUDIO_RESOURCE_OUT_SB_SPDIF;
            break;
        case HAL_AUDIO_SB_PCM:
            id = HAL_AUDIO_RESOURCE_OUT_SB_PCM;
            break;
        case HAL_AUDIO_HP:
            id = HAL_AUDIO_RESOURCE_OUT_HP;
            break;
        case HAL_AUDIO_ARC:
            id = HAL_AUDIO_RESOURCE_OUT_ARC;
            break;
        case HAL_AUDIO_WISA:
#if defined(WISA) //if chip does not support wisa, it should be works as HAL_AUDIO_SB_PCM
            id = HAL_AUDIO_RESOURCE_OUT_WISA;
#else
            id = HAL_AUDIO_RESOURCE_OUT_SB_PCM;
#endif
            break;
    }

    return id;
}

/* End of index conversion */

DTV_STATUS_T RHAL_AUDIO_SetAudioDescriptionMode(int mainIndex, BOOLEAN bOnOff)
{
    int i, ch_vol[AUD_MAX_CHANNEL];
    /* Set Dual-Decode mode */
    ADSP_DEC_SetADMode(bOnOff, mainIndex, 0);/* current HAL API doesn't support substream ID */

    /* Set Mixer mode */
    ADSP_PPMix_ConfigMixer(bOnOff, Volume_to_MixerGain(currMixADVol));
    Aud_descriptionMode = bOnOff;

    for(i = 0; i < AUD_ADEC_MAX; i++)
    {
        ADEC_Calculate_DSPGain(i, ch_vol);

        if(IsDecoderConnectedToEncoder((HAL_AUDIO_ADEC_INDEX_T)i))
        {
            //Skip the user volume setting to fw in AENC flow
            AUDIO_DEBUG("[AUDH]%s:Skip setting decoder_%d ch_vol[0]= %d in AENC flow.\n",__FUNCTION__,i,ch_vol[0]);
        }
        else
        {
            ADSP_DEC_SetVolume(i, ch_vol);
        }
    }
    adec_status[mainIndex].AudioDescription = bOnOff;

    return OK;
}

RESAMPLE_COEF_INFO AUD_22To48Info;
RESAMPLE_COEF_INFO AUD_11To48Info;

void CreateResampleCoef(void)
{
    CreateResample22To48Info(&AUD_22To48Info);
    CreateResample11To48Info(&AUD_11To48Info);

    SetResampleCoef(&AUD_22To48Info);
    SetResampleCoef(&AUD_11To48Info);
}

void DeleteResampleCoef(void)
{
    RESAMPLE_COEF_INFO tempInfo;

    memcpy(&tempInfo, &AUD_22To48Info, sizeof(RESAMPLE_COEF_INFO));
    tempInfo.phyCoefAddress = (UINT32)NULL;
    SetResampleCoef(&tempInfo); // notify fw hal delete buffer

    memcpy(&tempInfo, &AUD_11To48Info, sizeof(RESAMPLE_COEF_INFO));
    tempInfo.phyCoefAddress = (UINT32)NULL;
    SetResampleCoef(&tempInfo); //// notify fw hal delete buffer

    DeleteResampleInfo(&AUD_22To48Info);
    DeleteResampleInfo(&AUD_11To48Info);
}


DTV_STATUS_T RHAL_AUDIO_InitializeModule(void)
{
    int64_t timerstamp[5] = {0};
    int i;
    int Aud_wait_cnt = 10;

    if(Aud_initial == 1) {
        AUDIO_ERROR("%s retry\n", __FUNCTION__);
        return OK;
    }
    AUDIO_ERROR("%s Start\n", __FUNCTION__);

    timerstamp[0] = pli_getPTS();

    InitialResourceStatus();

	InitialADCStatus();
	InitialDecStatus();

    while(ADSP_TSK_GetStarted() != ST_AUD_TSK_STRTD && Aud_wait_cnt > 0)
    {
        Aud_wait_cnt--;
        AUDIO_INFO("FW Task not init finished, count down %d\n", Aud_wait_cnt);
        msleep(20);
    }

    timerstamp[1] = pli_getPTS();

	if(CreateAINFilter() != OK)
	{
		AUDIO_ERROR("initial AIN Filter Failed\n");
		HAL_AUDIO_FinalizeModule();
		return NOT_OK;
	}

    if(CreateFlowManagerFilter() != OK)
    {
        AUDIO_ERROR("create FlowManager failed\n");
        HAL_AUDIO_FinalizeModule();
        return NOT_OK;
    }

    if(CreatePPAOFilter() != OK)
    {
        AUDIO_ERROR("create PPAO failed\n");
        HAL_AUDIO_FinalizeModule();
        return NOT_OK;
    }

    timerstamp[2] = pli_getPTS();

    if(CreateDecFilter() != OK)
    {
        AUDIO_ERROR("create DEC filter failed\n");
        HAL_AUDIO_FinalizeModule();
        return NOT_OK;
    }

    if(CreateEncFilter() != OK)
    {
        AUDIO_ERROR("create ENC filter failed\n");
        HAL_AUDIO_FinalizeModule();
        return NOT_OK;
    }

    timerstamp[3] = pli_getPTS();

    Aud_initial = 1;

	//webos doesn't want to set info of encoder, we set in the init.
	if(RHAL_AUDIO_AENC_SetInfo(HAL_AUDIO_AENC0, Aud_aenc[HAL_AUDIO_AENC0]->info) != OK)
	{
		AUDIO_ERROR(" enc auto set info failed \n");
		return NOT_OK;
	}

	// initial state is at disconnected so need to mute it.
	ADSP_SNDOut_SetMute(ENUM_DEVICE_SPEAKER,  TRUE);
	ADSP_SNDOut_SetMute(ENUM_DEVICE_HEADPHONE,  TRUE);
	ADSP_SNDOut_SetMute(ENUM_DEVICE_SCART,  TRUE);
	ADSP_SNDOut_SetMute(ENUM_DEVICE_SPDIF,  TRUE);
	ADSP_SNDOut_SetMute(ENUM_DEVICE_SPDIF_ES,  TRUE);

	// auto connect (KTASKWBS-469)
	for(i = (int)HAL_AUDIO_RESOURCE_MIXER0; i <= (int)HAL_AUDIO_RESOURCE_MIXER7; i++) {
		RHAL_AUDIO_AMIX_Connect((HAL_AUDIO_RESOURCE_T)i);
	}

	// WOSQRTK-3390
#ifdef AMIXER_AUTO_MUTE
	for(i = HAL_AUDIO_MIXER0; i <  AUD_AMIX_MAX; i++ )
	{
		ADSP_AMIXER_SetMute((HAL_AUDIO_MIXER_INDEX_T)i, FALSE, FALSE  );// default no connect, so  mute
	}
#endif

	for(i = 0; i <AUD_AMIX_MAX; i++  )
	{
		g_mixer_gain[i].mainVol = 0x7F;
		g_mixer_gain[i].fineVol = 0x0;
	}

    // set decoder volume to make sure fw dec default value is 0db
    {
        HAL_AUDIO_VOLUME_T default_volume = {.mainVol = 0x7F, .fineVol = 0x0};
        HAL_AUDIO_SetDecoderInputGain(HAL_AUDIO_ADEC0, default_volume, 0x8000000);
        HAL_AUDIO_SetDecoderInputGain(HAL_AUDIO_ADEC1, default_volume, 0x8000000);
    }

    CreateResampleCoef();

	init_lgse_gloal_var();

	timerstamp[4] = pli_getPTS();

	AUDIO_ERROR("[%s] %lld~%lld, %dms\n",__FUNCTION__,timerstamp[0],timerstamp[4],(int)(timerstamp[4]-timerstamp[0])/90);
	if((int)(timerstamp[4]-timerstamp[0])/90 > 300) {
		AUDIO_ERROR(" %d/%d/%d/%d ms\n",(int)(timerstamp[1]-timerstamp[0])/90, (int)(timerstamp[2]-timerstamp[1])/90,
				(int)(timerstamp[3]-timerstamp[2])/90, (int)(timerstamp[4]-timerstamp[3])/90);
	}

    for(i = 0; i <AUD_ADEC_MAX; i++  )
    {
        gst_owner_process[i] = 0;
    }

	return OK;
}

DTV_STATUS_T HAL_AUDIO_InitializeModule(HAL_AUDIO_SIF_TYPE_T eSifType)
{
    return RHAL_AUDIO_InitializeModule();
}

DTV_STATUS_T HAL_AUDIO_FinalizeModule(void)
{
	int i;
	AUDIO_INFO("[AUD] %s start\n", __FUNCTION__);
	if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) {
		return NOT_OK;
	}

	// auto connect (KTASKWBS-469)
	for(i = (int)HAL_AUDIO_RESOURCE_MIXER0; i <= (int)HAL_AUDIO_RESOURCE_MIXER7; i++) {
		RHAL_AUDIO_AMIX_Disconnect((HAL_AUDIO_RESOURCE_T)i);
	}

	DeleteFlowManagerFilter();
	DeleteAINFilter();
	DeletePPAOFilter();
	DeleteDecFilter();
    DeleteEncFilter();

	Aud_initial = 0;
	AUDIO_INFO("%s finish\n", __FUNCTION__);

	return OK;
}

/* Open, Close */
DTV_STATUS_T HAL_AUDIO_TP_Open(HAL_AUDIO_TP_INDEX_T tpIndex)
{
    HAL_AUDIO_RESOURCE_T id = (HAL_AUDIO_RESOURCE_T)(HAL_AUDIO_RESOURCE_ATP0 + tpIndex);
    AUDIO_DEBUG("%s port %d \n", __FUNCTION__, tpIndex);

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(tpIndex > HAL_AUDIO_TP_MAX)
    {
        AUDIO_ERROR("error tp port %d \n", tpIndex);
        return NOT_OK;
    }

    if(SetResourceOpen(id) != TRUE)
    {
        return NOT_OK;
    }

    return OK;
}

DTV_STATUS_T HAL_AUDIO_TP_Close(HAL_AUDIO_TP_INDEX_T tpIndex)
{
    HAL_AUDIO_RESOURCE_T id = (HAL_AUDIO_RESOURCE_T)(HAL_AUDIO_RESOURCE_ATP0 + tpIndex);
    AUDIO_DEBUG("%s port %d \n", __FUNCTION__, tpIndex);

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(tpIndex > HAL_AUDIO_TP_MAX)
    {
        AUDIO_ERROR("error tp port %d \n", tpIndex);
        return NOT_OK;
    }

    if(SetResourceClose(id) != TRUE)
    {
        return NOT_OK;
    }

    RHAL_AUDIO_SetAudioDescriptionMode(Aud_mainDecIndex, FALSE);

    return OK;
}

DTV_STATUS_T HAL_AUDIO_ADC_Open(void)
{
    HAL_AUDIO_RESOURCE_T id = HAL_AUDIO_RESOURCE_ADC;
    int i=0;
    AUDIO_DEBUG("%s\n", __FUNCTION__);

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    for(i = 0; i < MAIN_AIN_ADC_PIN; i++)
    {
        if(AUDIO_CHECK_ADC_PIN_OPEN_NOTAVAILABLE(gAinStatus.ainPinStatus[i]))
        {
            AUDIO_ERROR("Error ADC pin %d  is still at %s state \n",i, GetResourceStatusString(gAinStatus.ainPinStatus[i]));
            return NOT_OK;
        }
    }

    if(SetResourceOpen(id) != TRUE)
    {
        return NOT_OK;
    }

    for(i = 0; i < MAIN_AIN_ADC_PIN; i++)
    {
        gAinStatus.ainPinStatus[i] = HAL_AUDIO_RESOURCE_OPEN;
    }

    return OK;
}

DTV_STATUS_T HAL_AUDIO_ADC_Close(void)
{
    int i=0;
    AUDIO_DEBUG("%s \n", __FUNCTION__);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(SetResourceClose(HAL_AUDIO_RESOURCE_ADC) != TRUE)
    {
        return NOT_OK;
    }

    for(i = 0; i < MAIN_AIN_ADC_PIN; i++)
    {
        gAinStatus.ainPinStatus[i] = HAL_AUDIO_RESOURCE_CLOSE;
    }

    return OK;
}

DTV_STATUS_T HAL_AUDIO_HDMI_Open(void)
{
    HAL_AUDIO_RESOURCE_T id = HAL_AUDIO_RESOURCE_HDMI;
    AUDIO_DEBUG("%s port  \n", __FUNCTION__);

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(SetResourceOpen(id) != TRUE)
    {
        return NOT_OK;
    }

    return OK;
}

DTV_STATUS_T HAL_AUDIO_HDMI_Close(void)
{
    HAL_AUDIO_RESOURCE_T id = HAL_AUDIO_RESOURCE_HDMI;
    AUDIO_DEBUG("%s port \n", __FUNCTION__);

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(SetResourceClose(id) != TRUE)
    {
        return NOT_OK;
    }

    return OK;
}

DTV_STATUS_T HAL_AUDIO_HDMI_OpenPort(HAL_AUDIO_HDMI_INDEX_T hdmiIndex)
{
    HAL_AUDIO_RESOURCE_T id = (HAL_AUDIO_RESOURCE_T)(HAL_AUDIO_RESOURCE_HDMI0 + hdmiIndex);
    AUDIO_DEBUG("%s port %d \n", __FUNCTION__, hdmiIndex);

    if(hdmiIndex > HAL_AUDIO_HDMI_MAX)
    {
        AUDIO_ERROR("error hdmi port \n");
        return NOT_OK;
    }

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(SetResourceOpen(id) != TRUE)
    {
        return NOT_OK;
    }

    return OK;
}

DTV_STATUS_T HAL_AUDIO_HDMI_ClosePort(HAL_AUDIO_HDMI_INDEX_T hdmiIndex)
{
    HAL_AUDIO_RESOURCE_T id = (HAL_AUDIO_RESOURCE_T) (HAL_AUDIO_RESOURCE_HDMI0 + hdmiIndex);
    AUDIO_DEBUG("%s port %d \n", __FUNCTION__, hdmiIndex);

    if(hdmiIndex > HAL_AUDIO_HDMI_MAX)
    {
        AUDIO_ERROR("error hdmi port \n");
        return NOT_OK;
    }

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(SetResourceClose(id) != TRUE)
    {
        return NOT_OK;
    }

    return OK;
}

DTV_STATUS_T HAL_AUDIO_SNDOUT_Open(common_output_ext_type_t soundOutType)
{
    int id = opt2res(soundOutType);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    AUDIO_DEBUG("%s %s\n", __FUNCTION__, GetResourceString((HAL_AUDIO_RESOURCE_T) (id)));
    if(id >= 0)
    {
        if(SetResourceOpen((HAL_AUDIO_RESOURCE_T)id) != TRUE)
        {
            return NOT_OK;
        }
        ADSP_SNDOut_AddDevice(soundOutType);
        return OK;
    }
    else
    {
        AUDIO_ERROR("unknow sndout type %d \n", soundOutType);
        return NOT_OK;
    }
}

DTV_STATUS_T HAL_AUDIO_SNDOUT_Close(common_output_ext_type_t soundOutType)
{
    int id = (int)opt2res(soundOutType);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    AUDIO_DEBUG("%s %s \n", __FUNCTION__, GetResourceString((HAL_AUDIO_RESOURCE_T)id));
    if(id >= 0)
    {
        if(SetResourceClose((HAL_AUDIO_RESOURCE_T)id) != TRUE)
        {
            return NOT_OK;
        }

        switch(id)
        {
            case HAL_AUDIO_RESOURCE_OUT_SPK:
                SNDOut_SetMute(ENUM_DEVICE_SPEAKER, ADEC_SPK_CheckConnect(), g_AudioStatusInfo.curSPKMuteStatus);
                break;
            case HAL_AUDIO_RESOURCE_OUT_SB_SPDIF:
                SNDOut_SetMute(ENUM_DEVICE_SPDIF,    ADEC_SB_SPDIF_CheckConnect(), g_AudioStatusInfo.curSPDIFMuteStatus);
                SNDOut_SetMute(ENUM_DEVICE_SPDIF_ES, ADEC_SB_SPDIF_CheckConnect(), g_AudioStatusInfo.curSPDIFMuteStatus);
                Update_RawMode_by_connection();
                break;
            case HAL_AUDIO_RESOURCE_OUT_SPDIF:
                SNDOut_SetMute(ENUM_DEVICE_SPDIF,    ADEC_SPDIF_CheckConnect(), g_AudioStatusInfo.curSPDIFMuteStatus);
                SNDOut_SetMute(ENUM_DEVICE_SPDIF_ES, ADEC_SPDIF_CheckConnect(), g_AudioStatusInfo.curSPDIFMuteStatus);
                Update_RawMode_by_connection();
                break;
            case HAL_AUDIO_RESOURCE_OUT_ARC:
                SNDOut_SetMute(ENUM_DEVICE_SPDIF,    ADEC_ARC_CheckConnect(), g_ARCStatusInfo.curARCMuteStatus);
                SNDOut_SetMute(ENUM_DEVICE_SPDIF_ES, ADEC_ARC_CheckConnect(), g_ARCStatusInfo.curARCMuteStatus);
                break;
            case HAL_AUDIO_RESOURCE_OUT_HP:
                SNDOut_SetMute(ENUM_DEVICE_HEADPHONE, ADEC_HP_CheckConnect(), g_AudioStatusInfo.curHPMuteStatus);
                break;
            default:
                break;
        }

        ADSP_SNDOut_RemoveDevice(soundOutType);
        return OK;
    }
    else
    {
        return NOT_OK;
    }
}

DTV_STATUS_T HAL_AUDIO_AAD_Open(void)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;
    if(SetResourceOpen(HAL_AUDIO_RESOURCE_AAD) != TRUE)
        return NOT_OK;

    ATV_open = TRUE;

    return OK;
}

DTV_STATUS_T HAL_AUDIO_AAD_Close(void)
{
	AUDIO_SIF_RESET_DATA();
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;
    if(SetResourceClose(HAL_AUDIO_RESOURCE_AAD) != TRUE)
        return NOT_OK;

    ATV_open = FALSE;

    return OK;
}

DTV_STATUS_T HAL_AUDIO_DIRECT_SYSTEM_Open(void)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;
    if(SetResourceOpen(HAL_AUDIO_RESOURCE_SYSTEM) != TRUE)
        return NOT_OK;
    return OK;
}

DTV_STATUS_T HAL_AUDIO_DIRECT_SYSTEM_Close(void)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;
    if(SetResourceClose(HAL_AUDIO_RESOURCE_SYSTEM) != TRUE)
        return NOT_OK;
    return OK;
}

DTV_STATUS_T HAL_AUDIO_ADEC_Open(HAL_AUDIO_ADEC_INDEX_T adecIndex)
{
    AUDIO_DEBUG( "%s %d \n", __FUNCTION__, adecIndex);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;
    if(!IsValidAdecIdx(adecIndex))
    {
        AUDIO_ERROR("ADEC index error  %d \n", adecIndex);
        return NOT_OK;
    }
    if(SetResourceOpen(adec2res(adecIndex)) != TRUE)
        return NOT_OK;
    SetSPDIFOutType(adecIndex, HAL_AUDIO_SPDIF_PCM);
    adec_status[adecIndex].Open = TRUE;
    return OK;
}

DTV_STATUS_T HAL_AUDIO_ADEC_Close(HAL_AUDIO_ADEC_INDEX_T adecIndex)
{
    AUDIO_DEBUG("%s %d \n", __FUNCTION__,  adecIndex);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;
    if(!IsValidAdecIdx(adecIndex))
    {
        AUDIO_ERROR("ADEC index error  %d \n", adecIndex);
        return NOT_OK;
    }
    if(IsFlowRunningState(adecIndex)) {
        AUDIO_ERROR("[OnClose] ADEC%d is at running state, return failed\n", adecIndex);
        return NOT_OK;
    }
    if(SetResourceClose(adec2res(adecIndex)) != TRUE)
        return NOT_OK;
    adec_status[adecIndex].Open = FALSE;
    adec_status[adecIndex].Connect = ADEC_SRC_UNKNOWN;
    return OK;
}

DTV_STATUS_T HAL_AUDIO_AENC_Open(HAL_AUDIO_AENC_INDEX_T aencIndex)
{
    // TODO: add condition check
    AUDIO_INFO("%s %d \n", __FUNCTION__, aencIndex);

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(!RangeCheck(aencIndex, 0, AUD_AENC_MAX))
    {
        AUDIO_ERROR("aenc index error  %d \n", aencIndex);
        return NOT_OK;
    }

    if(SetResourceOpen(aenc2res(aencIndex)) != TRUE)
    {
        AUDIO_ERROR("%s open fail\n", __FUNCTION__);
       return NOT_OK;
    }

    return OK;
}

DTV_STATUS_T HAL_AUDIO_AENC_Close(HAL_AUDIO_AENC_INDEX_T aencIndex)
{
    AUDIO_INFO("%s %d \n", __FUNCTION__, aencIndex);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(!RangeCheck(aencIndex, 0, AUD_AENC_MAX))
    {
        AUDIO_ERROR("aenc index error  %d \n", aencIndex);
        return NOT_OK;
    }

    if(SetResourceClose(aenc2res(aencIndex)) != TRUE)
    {
        return NOT_OK;
    }

    if(Aud_aenc[aencIndex] == NULL) return NOT_OK;

    return OK;
}

DTV_STATUS_T HAL_AUDIO_SE_Open(void)
{
    /* SE should have been opened at init */
    return OK;
}

DTV_STATUS_T HAL_AUDIO_SE_Close(void)
{
    /* SE should have never been closed  */
    return OK;
}

/* Connect & Disconnect */
DTV_STATUS_T HAL_AUDIO_TP_Connect(HAL_AUDIO_RESOURCE_T currentConnect, HAL_AUDIO_RESOURCE_T inputConnect, HAL_AUDIO_ADEC_INDEX_T adecIndex)
{
    /*int totalOutputConnectResource;*/

    AUDIO_INFO("%s ADEC%d -- %s -- %s\n", __FUNCTION__,adecIndex, GetResourceString(currentConnect), GetResourceString(inputConnect));

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(!IsValidAdecIdx(adecIndex))
    {
        AUDIO_ERROR("ADEC index error  %d \n", adecIndex);
        return NOT_OK;
    }
    //for SoCTS
    if(ResourceStatus[adec2res(adecIndex)].connectStatus[0] == HAL_AUDIO_RESOURCE_CLOSE){
        AUDIO_ERROR("ADEC%d status %s , return NOT_OK\n", adecIndex,
                GetResourceStatusString((int)(ResourceStatus[adec2res(adecIndex)].connectStatus[0])));
        return NOT_OK;
    }
#if 0
    GetConnectOutputSource(inputConnect, &outputId, 1, &totalOutputConnectResource);// tp's iutput is sdec

    if(totalOutputConnectResource > 1)// sdec's output
    {
        AUDIO_ERROR("sdec output connect too much %d != 1\n", totalOutputConnectResource);
        return NOT_OK;
    }
#endif

    if(SetResourceConnect(currentConnect, inputConnect) != TRUE)
        return NOT_OK;

#if 0
    GetConnectOutputSource(currentConnect, &outputId, 1, &totalOutputConnectResource);// tp's output is dec
#endif

    return OK;
}

DTV_STATUS_T HAL_AUDIO_SETUP_ATP(HAL_AUDIO_ADEC_INDEX_T adecIndex)
{
    HAL_AUDIO_RESOURCE_T atp_res = ADEC_GetConnectedATP(adecIndex);
    int atpindex = -1;
    struct dmx_buff_info_t dmx_buff_info;

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(!IsValidAdecIdx(adecIndex))
    {
        AUDIO_ERROR("ADEC index error  %d \n", adecIndex);
        return NOT_OK;
    }

    if(atp_res == HAL_AUDIO_RESOURCE_ATP0)
        atpindex = HAL_AUDIO_TP0;
    else if (atp_res == HAL_AUDIO_RESOURCE_ATP1)
        atpindex = HAL_AUDIO_TP1;

    if(b_get_sdec_buffer[atpindex] == TRUE){
        AUDIO_DEBUG("[%s] ATP%d already get buffer info\n",__FUNCTION__,atpindex);
        return OK;
    }

    if(rdvb_dmx_ctrl_getBufferInfo(PIN_TYPE_ATP, atpindex, HOST_TYPE_ADEC, adecIndex, &dmx_buff_info) != OK)
    {
        AUDIO_ERROR("sdec connect failed\n");
        return NOT_OK;
    } else{
        b_get_sdec_buffer[atpindex] = TRUE;
    }

    ((DtvCom*)Aud_DTV[atpindex]->pDerivedObj)->SetSDECInfo(Aud_DTV[atpindex], (UINT32)atpindex);

    AUDIO_DEBUG("IBand add %x  size %x \n", dmx_buff_info.ibPhyAddr, dmx_buff_info.ibHeaderSize);
    AUDIO_DEBUG("BS add %x  size %x \n", dmx_buff_info.bsPhyAddr, dmx_buff_info.bsHeaderSize);
    AUDIO_DEBUG("Ref add %x  size %x \n", dmx_buff_info.refClockPhyAddr, dmx_buff_info.refClockHeaderSize);

    if(dmx_buff_info.refClockHeaderSize != sizeof(REFCLOCK)) // sdec's reference min size is 16 align
    {
        //AUDIO_DEBUG("error ref clock size %x %x \n", AddressInfo.refClockHeaderSize, sizeof(REFCLOCK));
        //return NOT_OK;
    }

    ((DtvCom*)Aud_DTV[atpindex]->pDerivedObj)->SetICQRingBufPhyAddress(Aud_DTV[atpindex], dmx_buff_info.ibPhyAddr, (UINT32)__va(dmx_buff_info.ibPhyAddr), dmx_buff_info.ibPhyAddr, dmx_buff_info.ibHeaderSize);
    /*PrintRingBuffer((RINGBUFFER_HEADER*)nonCacheAddress, (unsigned long) AddressInfo.ibRingHeader);*/

    ((DtvCom*)Aud_DTV[atpindex]->pDerivedObj)->SetBSRingBufPhyAddress(Aud_DTV[atpindex], dmx_buff_info.bsPhyAddr, (UINT32)__va(dmx_buff_info.bsPhyAddr), dmx_buff_info.bsPhyAddr, dmx_buff_info.bsHeaderSize);
    /*PrintRingBuffer((RINGBUFFER_HEADER*)nonCacheAddress, (unsigned long) AddressInfo.bsRingHeader);*/

    ((DtvCom*)Aud_DTV[atpindex]->pDerivedObj)->SetRefClockPhyAddress(Aud_DTV[atpindex], dmx_buff_info.refClockPhyAddr, (UINT32)__va(dmx_buff_info.refClockPhyAddr), dmx_buff_info.refClockPhyAddr, dmx_buff_info.refClockHeaderSize);

    return OK;

}

DTV_STATUS_T HAL_AUDIO_TP_Disconnect(HAL_AUDIO_RESOURCE_T currentConnect, HAL_AUDIO_RESOURCE_T inputConnect, HAL_AUDIO_ADEC_INDEX_T adecIndex)
{
    int atpindex = currentConnect - HAL_AUDIO_RESOURCE_ATP0;
    unsigned int mapAddress;
    unsigned int mapSize;

    AUDIO_DEBUG("%s ADEC%d -x- %s -- %s\n", __FUNCTION__,adecIndex, GetResourceString(currentConnect), GetResourceString(inputConnect));

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(SetResourceDisconnect(currentConnect, inputConnect) != TRUE)
        return NOT_OK;

    // release mmap address
    ((DtvCom*)Aud_DTV[atpindex]->pDerivedObj)->GetBSRingBufMapInfo(Aud_DTV[atpindex],  &mapAddress, &mapSize);
    ((DtvCom*)Aud_DTV[atpindex]->pDerivedObj)->GetICQRingBufMapInfo(Aud_DTV[atpindex],  &mapAddress, &mapSize);
    ((DtvCom*)Aud_DTV[atpindex]->pDerivedObj)->GetRefClockMapInfo(Aud_DTV[atpindex],  &mapAddress, &mapSize);

    // clean
    ((DtvCom*)Aud_DTV[atpindex]->pDerivedObj)->SetICQRingBufPhyAddress(Aud_DTV[atpindex],0, 0, 0, 0);
    ((DtvCom*)Aud_DTV[atpindex]->pDerivedObj)->SetBSRingBufPhyAddress(Aud_DTV[atpindex], 0, 0, 0,0);
    ((DtvCom*)Aud_DTV[atpindex]->pDerivedObj)->SetRefClockPhyAddress(Aud_DTV[atpindex], 0, 0, 0, 0);

    if(IsDTVSource(currentConnect))// tp's output is dec
    {
        rdvb_dmx_ctrl_releaseBuffer(PIN_TYPE_ATP, atpindex, HOST_TYPE_ADEC, adecIndex);
        b_get_sdec_buffer[atpindex] = FALSE;
    }

    return OK;
}

DTV_STATUS_T HAL_AUDIO_ADC_Connect(UINT8 portNum)
{
    HAL_AUDIO_RESOURCE_T optResourceId[2];
    int totalOutputConnectResource;
    int i=0;

    AUDIO_DEBUG("%s port %d \n", __FUNCTION__, portNum);
    //AUDIO_INFO("force to change pin id %d -> 1   \n", portNum);
    //portNum = 1;

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(portNum >= MAIN_AIN_ADC_PIN)
        return NOT_OK;

    // check if other pins are also at connect status
    for(i = 0; i < MAIN_AIN_ADC_PIN; i++)
    {
        if(gAinStatus.ainPinStatus[i] == HAL_AUDIO_RESOURCE_CONNECT)
        {
            AUDIO_ERROR("Error ADC too much connect %d %d \n",i, gAinStatus.ainPinStatus[i]);
            return NOT_OK;
        }
    }

    // check adc is at correct status
    if( AUDIO_CHECK_ADC_PIN_CONNECT_NOTAVAILABLE(gAinStatus.ainPinStatus[portNum]))
    {
         AUDIO_ERROR("Error ADC pin %d  is still at %s state \n",portNum, GetResourceStatusString(gAinStatus.ainPinStatus[portNum]));
         return NOT_OK;
    }

#if 1 /* move check to decoder module*/
    GetConnectOutputSource(HAL_AUDIO_RESOURCE_ADC, optResourceId, 2, &totalOutputConnectResource);
    if(totalOutputConnectResource > 2)
    {
        AUDIO_ERROR("ADC output connect error %d \n", totalOutputConnectResource);
        return NOT_OK;
    }
#endif

    if(SetResourceConnect(HAL_AUDIO_RESOURCE_ADC, HAL_AUDIO_RESOURCE_NO_CONNECTION) != TRUE)
        return NOT_OK;

    gAinStatus.ainPinStatus[portNum] = HAL_AUDIO_RESOURCE_CONNECT;

    return OK;
}

DTV_STATUS_T HAL_AUDIO_ADC_Disconnect(UINT8 portNum)
{
    AUDIO_DEBUG("%s port %d\n", __FUNCTION__,  portNum);

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(portNum >= MAIN_AIN_ADC_PIN)
        return NOT_OK;

    if(AUDIO_CHECK_ADC_PIN_DISCONNECT_NOTAVAILABLE(gAinStatus.ainPinStatus[portNum]))
    {
        AUDIO_ERROR("Error ADC pin %d  is still at %s state \n",portNum, GetResourceStatusString(gAinStatus.ainPinStatus[portNum]));
        return NOT_OK;
    }

    if(SetResourceDisconnect(HAL_AUDIO_RESOURCE_ADC, HAL_AUDIO_RESOURCE_NO_CONNECTION) != TRUE)
        return NOT_OK;

    gAinStatus.ainPinStatus[portNum] = HAL_AUDIO_RESOURCE_DISCONNECT;

    return OK;
}

DTV_STATUS_T HAL_AUDIO_HDMI_Connect(void)
{
    HAL_AUDIO_RESOURCE_T id;
    int i;
    AUDIO_DEBUG("%s \n", __FUNCTION__);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    id = HAL_AUDIO_RESOURCE_HDMI;

    // check if other hdmi is at connect status
    for(i = HAL_AUDIO_RESOURCE_HDMI0; i <= HAL_AUDIO_RESOURCE_SWITCH; i++)
    {
        if(ResourceStatus[i].connectStatus[0] == HAL_AUDIO_RESOURCE_CONNECT)
        {
            AUDIO_ERROR("Error HDMI too much connect %d %d \n",
                          i, ResourceStatus[i].connectStatus[0]);
            return NOT_OK;
        }
    }

    // do not need to update hdmi port of adecstatus api

    if(SetResourceConnect(id, HAL_AUDIO_RESOURCE_NO_CONNECTION) != TRUE)
        return NOT_OK;

    ipt_Is_HDMI = TRUE;
    return OK;
}

DTV_STATUS_T HAL_AUDIO_HDMI_Disconnect(void)
{
    HAL_AUDIO_RESOURCE_T id;
    AUDIO_DEBUG("%s \n", __FUNCTION__);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    id = HAL_AUDIO_RESOURCE_HDMI;
    HDMI_fmt_change_transition = FALSE;

    if(SetResourceDisconnect(id, HAL_AUDIO_RESOURCE_NO_CONNECTION) != TRUE)
        return NOT_OK;

    ipt_Is_HDMI = FALSE;
#ifdef START_DECODING_WHEN_GET_FORMAT
    At_Stop_To_Start_Decode = FALSE;
#endif
    return OK;
}

DTV_STATUS_T HAL_AUDIO_HDMI_ConnectPort(HAL_AUDIO_HDMI_INDEX_T hdmiIndex)
{
    HAL_AUDIO_RESOURCE_T id;
    HAL_AUDIO_RESOURCE_T optResourceId[2];
    int totalOutputConnectResource;

    AUDIO_DEBUG("%s %d \n", __FUNCTION__, hdmiIndex);

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(hdmiIndex > HAL_AUDIO_HDMI_MAX)
    {
        AUDIO_ERROR("error hdmi port\n");
        return NOT_OK;
    }
    id = (HAL_AUDIO_RESOURCE_T)(HAL_AUDIO_RESOURCE_HDMI0 + hdmiIndex);

    if(GetCurrentHDMIConnectPin() != -1)
    {
        AUDIO_ERROR("error too many hdmi connect \n");
        return NOT_OK;
    }

    GetConnectOutputSource(id, optResourceId, 2, &totalOutputConnectResource);
    if(totalOutputConnectResource > 2)
    {
        AUDIO_ERROR("hdmiport output connect error %d \n", totalOutputConnectResource);
        return NOT_OK;
    }

    if(SetResourceConnect(id, HAL_AUDIO_RESOURCE_NO_CONNECTION) != TRUE)
        return NOT_OK;

    return OK;
}

DTV_STATUS_T HAL_AUDIO_HDMI_DisconnectPort(HAL_AUDIO_HDMI_INDEX_T hdmiIndex)
{
    HAL_AUDIO_RESOURCE_T id;
    AUDIO_DEBUG("%s %d \n", __FUNCTION__ , hdmiIndex);

    if(hdmiIndex > HAL_AUDIO_HDMI_MAX)
    {
        AUDIO_ERROR("error hdmi port \n");
        return NOT_OK;
    }

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    id = (HAL_AUDIO_RESOURCE_T)((UINT32)HAL_AUDIO_RESOURCE_HDMI0 + (UINT32)hdmiIndex);
    HDMI_fmt_change_transition = FALSE;

    if(SetResourceDisconnect(id, HAL_AUDIO_RESOURCE_NO_CONNECTION) != TRUE)
        return NOT_OK;

    return OK;
}

DTV_STATUS_T HAL_AUDIO_AAD_Connect(void)
{
    AUDIO_DEBUG("%s \n", __FUNCTION__);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(SetResourceConnect(HAL_AUDIO_RESOURCE_AAD, HAL_AUDIO_RESOURCE_NO_CONNECTION) != TRUE)
        return NOT_OK;

    KHAL_SIF_CONNECT();

    ATV_connect = TRUE;

    return OK;
}

DTV_STATUS_T HAL_AUDIO_AAD_Disconnect(void)
{
    AUDIO_SIF_STOP_DETECT(TRUE);

    AUDIO_DEBUG("%s port\n", __FUNCTION__);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
    {
        // release sem
        AUDIO_SIF_STOP_DETECT(FALSE);
        return NOT_OK;
    }

    if(SetResourceDisconnect(HAL_AUDIO_RESOURCE_AAD, HAL_AUDIO_RESOURCE_NO_CONNECTION) != TRUE)
    {
        // release sem
        AUDIO_SIF_STOP_DETECT(FALSE);
        return NOT_OK;
    }

    AUDIO_SIF_RESET_DATA();
    AUDIO_SIF_STOP_DETECT(FALSE);

    ATV_connect = FALSE;

    return OK;
}

DTV_STATUS_T HAL_AUDIO_DIRECT_SYSTEM_Connect(void)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;
    if(SetResourceConnect(HAL_AUDIO_RESOURCE_SYSTEM, HAL_AUDIO_RESOURCE_NO_CONNECTION) != TRUE)
        return NOT_OK;
    return OK;
}

DTV_STATUS_T HAL_AUDIO_DIRECT_SYSTEM_Disconnect(void)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;
    if(SetResourceDisconnect(HAL_AUDIO_RESOURCE_SYSTEM, HAL_AUDIO_RESOURCE_NO_CONNECTION) != TRUE)
        return NOT_OK;
    return OK;
}

static HAL_AUDIO_RESOURCE_T GetDecInput(HAL_AUDIO_ADEC_INDEX_T adecIndex)
{
    return GetNonOutputModuleSingleInputResource(ResourceStatus[adecIndex]);// dec input source
}

DTV_STATUS_T RHAL_AUDIO_ADEC_Connect(HAL_AUDIO_RESOURCE_T currentConnect, HAL_AUDIO_RESOURCE_T inputConnect)
{
    AUDIO_DEBUG("%s cur %s ipt %s\n", __FUNCTION__,
                GetResourceString(currentConnect), GetResourceString(inputConnect));

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(!IsADECSource(currentConnect))
    {
        AUDIO_ERROR("[AUDH][Error] %s != ADEC\n", GetResourceString(currentConnect));
        return NOT_OK;
    }

    if(IsValidADECIpts(inputConnect) != TRUE)
    {
        AUDIO_ERROR("[AUDH][Error] Invalid input %s\n", GetResourceString(inputConnect));
        return NOT_OK;
    }

    if(SetResourceConnect(currentConnect, inputConnect) != TRUE)
        return NOT_OK;

#if !defined(SEARCH_PAPB_WHEN_STOP)
    if(inputConnect >= HAL_AUDIO_RESOURCE_HDMI0 && inputConnect <= HAL_AUDIO_RESOURCE_HDMI3)
    {
        // start HDMI decoder when connect, speed up HDMI_GetAudioMode()
        HdmiStartDecoding(res2adec(currentConnect));
    }
#endif

    return OK;
}

DTV_STATUS_T HAL_AUDIO_ADEC_Connect(int adec_port, adec_src_port_index_ext_type_t inputConnect)
{
    if(!IsValidAdecIdx(adec_port))
    {
        AUDIO_ERROR("ADEC index error  %d \n", adec_port);
        return NOT_OK;
    }
    adec_info[adec_port].curAdecInputPort  = inputConnect;
    adec_status[adec_port].Connect = inputConnect;
    return RHAL_AUDIO_ADEC_Connect(adec2res(adec_port), ipt2res(inputConnect));
}

DTV_STATUS_T RHAL_AUDIO_ADEC_Disconnect(HAL_AUDIO_RESOURCE_T currentConnect, HAL_AUDIO_RESOURCE_T inputConnect)
{
    HAL_AUDIO_ADEC_INDEX_T adecId = HAL_AUDIO_ADEC0;
    HAL_AUDIO_RESOURCE_T decIptResId;
    AUDIO_DEBUG("%s cur %s ipt %s\n", __FUNCTION__,
                GetResourceString(currentConnect), GetResourceString(inputConnect));
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(!IsADECSource(currentConnect))
        return NOT_OK;

    adecId = res2adec(currentConnect);

    // current ADEC flow is running, auto stop (KTASKWBS-12249)
    if( IsFlowRunningState(adecId) ) //
    {
        decIptResId = GetDecInput(currentConnect);
        AUDIO_ERROR("%s--%s is at Running state, stop it first\n", GetResourceString(decIptResId), GetResourceString(currentConnect));
        RHAL_AUDIO_StopDecoding(adecId, 0);
    }

    if(SetResourceDisconnect(currentConnect, inputConnect) != TRUE)
        return NOT_OK;

    adec_info[adecId].prevAdecInputPort = adec_info[adecId].curAdecInputPort;
    adec_info[adecId].curAdecInputPort  = ADEC_SRC_UNKNOWN;
    adec_status[adecId].Connect = ADEC_SRC_UNKNOWN;
    return OK;
}

DTV_STATUS_T HAL_AUDIO_ADEC_Disconnect(int adec_port, adec_src_port_index_ext_type_t inputConnect)
{
    HAL_AUDIO_RESOURCE_T input;
    input = GetInputConnect(ResourceStatus[adec2res(adec_port)]);
    AUDIO_DEBUG("%s cur %s ipt %d->%d\n", __FUNCTION__,
                GetResourceString(adec2res(adec_port)), inputConnect, input);
    return RHAL_AUDIO_ADEC_Disconnect(adec2res(adec_port), input);
}

DTV_STATUS_T HAL_AUDIO_DIRECT_ADEC_Connect(int adec_port)
{
    if(!IsValidAdecIdx(adec_port)) {
        AUDIO_ERROR("ADEC index error %d \n", adec_port);
        return NOT_OK;
    }
    return RHAL_AUDIO_ADEC_Connect(adec2res(adec_port), HAL_AUDIO_RESOURCE_SYSTEM);
}

DTV_STATUS_T HAL_AUDIO_DIRECT_ADEC_Disconnect(int adec_port)
{
    if(!IsValidAdecIdx(adec_port)) {
        AUDIO_ERROR("ADEC index error %d \n", adec_port);
        return NOT_OK;
    }
    return RHAL_AUDIO_ADEC_Disconnect(adec2res(adec_port), HAL_AUDIO_RESOURCE_SYSTEM);
}

DTV_STATUS_T RHAL_AUDIO_AMIX_Connect(HAL_AUDIO_RESOURCE_T currentConnect)
{
    DTV_STATUS_T ret = NOT_OK;

    if(!IsAMIXSource(currentConnect)) {
        AUDIO_ERROR("[AUDH][Error] %s != AMIX\n", GetResourceString(currentConnect));
        return NOT_OK;
    }

	ret = SetResourceOpen(currentConnect);
	if(SetResourceConnect(currentConnect, HAL_AUDIO_RESOURCE_NO_CONNECTION) != TRUE)
		return NOT_OK;
	return ret;
}

DTV_STATUS_T RHAL_AUDIO_AMIX_Disconnect(HAL_AUDIO_RESOURCE_T currentConnect)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;
    if(!IsAMIXSource(currentConnect)) {
        AUDIO_ERROR("[AUDH][Error] %s != AMIX\n", GetResourceString(currentConnect));
        return NOT_OK;
    }
	if(SetResourceDisconnect(currentConnect, HAL_AUDIO_RESOURCE_NO_CONNECTION) != TRUE)
		return NOT_OK;
	if(SetResourceClose(currentConnect) != TRUE)
		return NOT_OK;
    return OK;
}

DTV_STATUS_T HAL_AUDIO_AMIX_Connect(HAL_AUDIO_RESOURCE_T currentConnect, HAL_AUDIO_RESOURCE_T inputConnect)
{
    return OK;
}

DTV_STATUS_T HAL_AUDIO_AMIX_Disconnect(HAL_AUDIO_RESOURCE_T currentConnect, HAL_AUDIO_RESOURCE_T inputConnect)
{
    return OK;
}

DTV_STATUS_T HAL_AUDIO_AENC_Connect(HAL_AUDIO_RESOURCE_T currentConnect, HAL_AUDIO_RESOURCE_T inputConnect)
{
    AUDIO_INFO("%s cur %s ipt %s\n", __FUNCTION__,
                GetResourceString(currentConnect), GetResourceString(inputConnect));
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(!IsEncoderSource(currentConnect))
    {
        AUDIO_ERROR("[AUDH][Error] %s is not Valid AENC\n", GetResourceString(currentConnect));
        return NOT_OK;
    }

    if(!IsADECSource(inputConnect))
    {
        AUDIO_ERROR("[AUDH][Error] %s is not enc valid ipt src\n", GetResourceString(inputConnect));
        return NOT_OK;
    }

    if(SetResourceConnect(currentConnect, inputConnect) != TRUE){
        AUDIO_ERROR("%s connect fail\n", __FUNCTION__);
        return NOT_OK;
    }

    return OK;
}

DTV_STATUS_T HAL_AUDIO_AENC_Disconnect(HAL_AUDIO_RESOURCE_T currentConnect, HAL_AUDIO_RESOURCE_T inputConnect)
{
    AUDIO_INFO("%s cur %s ipt %s\n", __FUNCTION__,
                GetResourceString(currentConnect), GetResourceString(inputConnect));
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(!IsEncoderSource(currentConnect))
    {
        AUDIO_ERROR("[AUDH][Error] %s is not Valid AENC\n", GetResourceString(currentConnect));
        return NOT_OK;
    }

    if(SetResourceDisconnect(currentConnect, inputConnect) != TRUE)
        return NOT_OK;

    //Restore the user volume setting
    if(IsADECSource(inputConnect))
    {
        UINT32 iAdec = (UINT32)res2adec(inputConnect);
        int ch_vol[AUD_MAX_CHANNEL];
        BOOLEAN bMute = FALSE;

        HAL_AUDIO_VOLUME_T tempVol = GetDecInVolume(iAdec);

        ADEC_Calculate_DSPGain(iAdec, ch_vol);
        if(ADSP_DEC_SetVolume(iAdec, ch_vol) != S_OK) return NOT_OK;

        bMute = (BOOLEAN)GetDecInMute(iAdec);
        ADSP_DEC_SetMute(iAdec, ADEC_CheckConnect((HAL_AUDIO_ADEC_INDEX_T)iAdec), bMute);// revert ori dec mute status

        AUDIO_INFO("Restore the user volume[main, fine]/mute=[%d, %d]/%s before exit AENC flow.", tempVol.mainVol, tempVol.fineVol, bMute? "ON":"OFF");
    }

    return OK;
}

DTV_STATUS_T RHAL_AUDIO_SNDOUT_Connect(HAL_AUDIO_RESOURCE_T currentConnect, HAL_AUDIO_RESOURCE_T inputConnect)
{
    AUDIO_DEBUG("%s cur %s ipt %s\n", __FUNCTION__,
                GetResourceString(currentConnect), GetResourceString(inputConnect));
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if((IsAOutSource(currentConnect) != TRUE) || IsSESource(currentConnect))
    {
        AUDIO_ERROR("[AUDH][Error] %s != SNDOUT device\n", GetResourceString(currentConnect));
        return NOT_OK;
    }

    // check input resource is valid
    if(IsADECSource(inputConnect) || IsAMIXSource(inputConnect))
    {
        // valid input resource
    }
    else if(IsSESource(inputConnect) && IsValidSEOpts(currentConnect))
    {
        /* SPK & SE  should have been connected at init */
        if(currentConnect == HAL_AUDIO_RESOURCE_OUT_SPK)
        {
            return OK;
        }
        // valid input resource
    }
    else
    {
        AUDIO_INFO("Error:: Invalid connect %s -> %s \n", GetResourceString(inputConnect), GetResourceString(currentConnect));
        return NOT_OK;
    }

    if(SetResourceConnect(currentConnect, inputConnect) != TRUE)
    {
        return NOT_OK;
    }

    switch(currentConnect)
    {
        case HAL_AUDIO_RESOURCE_OUT_SPK:
            SNDOut_SetMute(ENUM_DEVICE_SPEAKER, ADEC_SPK_CheckConnect(), g_AudioStatusInfo.curSPKMuteStatus);
            break;
        case HAL_AUDIO_RESOURCE_OUT_SB_SPDIF:
            SNDOut_SetMute(ENUM_DEVICE_SPDIF,    ADEC_SB_SPDIF_CheckConnect(), g_AudioStatusInfo.curSPDIF_LG_MuteStatus);
            SNDOut_SetMute(ENUM_DEVICE_SPDIF_ES, ADEC_SB_SPDIF_CheckConnect(), g_AudioStatusInfo.curSPDIF_LG_MuteStatus);
            Update_RawMode_by_connection();
            break;
        case HAL_AUDIO_RESOURCE_OUT_SPDIF:
            SNDOut_SetMute(ENUM_DEVICE_SPDIF,    ADEC_SPDIF_CheckConnect(), g_AudioStatusInfo.curSPDIFMuteStatus);
            SNDOut_SetMute(ENUM_DEVICE_SPDIF_ES, ADEC_SPDIF_CheckConnect(), g_AudioStatusInfo.curSPDIFMuteStatus);
            Update_RawMode_by_connection();
            break;
        case HAL_AUDIO_RESOURCE_OUT_ARC:
            SNDOut_SetMute(ENUM_DEVICE_SPDIF,    ADEC_ARC_CheckConnect(), g_ARCStatusInfo.curARCMuteStatus);
            SNDOut_SetMute(ENUM_DEVICE_SPDIF_ES, ADEC_ARC_CheckConnect(), g_ARCStatusInfo.curARCMuteStatus);
            Update_RawMode_by_connection();
            break;
        case HAL_AUDIO_RESOURCE_OUT_HP:
            SNDOut_SetMute(ENUM_DEVICE_HEADPHONE, ADEC_HP_CheckConnect(), g_AudioStatusInfo.curHPMuteStatus);
            break;
        default:
            break;
    }

    // set delay value
    switch(currentConnect)
    {
        case HAL_AUDIO_RESOURCE_OUT_SB_SPDIF:
            if(!ADEC_ARC_CheckConnect() && ADEC_SB_SPDIF_CheckConnect()) {
                SNDOut_SetDelay(ENUM_DEVICE_SPDIF, g_AudioStatusInfo.curSPDIFSBOutDelay);
            }
            break;
        case HAL_AUDIO_RESOURCE_OUT_SPDIF:
            if(!ADEC_ARC_CheckConnect() && ADEC_SPDIF_CheckConnect()) {
                SNDOut_SetDelay(ENUM_DEVICE_SPDIF, g_AudioStatusInfo.curSPDIFOutDelay);
            }
            break;
        default:
            break;
    }

    if(IsADECSource(inputConnect))
    {
        HAL_AUDIO_ADEC_INDEX_T adecIndex = res2adec(inputConnect);
        RHAL_AUDIO_SetDolbyDRCMode(res2adec(inputConnect), GetDecDrcMode((int) res2adec(inputConnect) )); // revert ori DRC setting after multi-view switch
        RHAL_AUDIO_SetDownMixMode( res2adec(inputConnect), GetDecDownMixMode((int) res2adec(inputConnect) )); // revert ori DownMix setting after multi-view switch

        ADSP_DEC_SetMute(adecIndex, ADEC_CheckConnect(adecIndex), GetDecInMute(adecIndex));// connect , check if need to unmute
    }
    else if(IsAMIXSource(inputConnect) == TRUE)
    {
        HAL_AUDIO_MIXER_INDEX_T amixerIndex = res2amixer(inputConnect);
        ADSP_AMIXER_SetMute(amixerIndex, AMIXER_CheckConnect(inputConnect), GetAmixerUserMute(amixerIndex));
    }

    return OK;
}

DTV_STATUS_T HAL_AUDIO_SNDOUT_Connect(common_output_ext_type_t output_type, int res_input)
{
    return RHAL_AUDIO_SNDOUT_Connect(opt2res(output_type), res_input);
}

#define ALSA_MAX_SNDOUT_CONNECT (16)
#define ALSA_MAX_SNDOUT_GET (4)
static int connected_device_order[ALSA_MAX_SNDOUT_CONNECT];
void get_resource_type_and_port(HAL_AUDIO_RESOURCE_T target, unsigned int* connected_device)
{
    switch(target)
    {
        case HAL_AUDIO_RESOURCE_ADEC0:
        case HAL_AUDIO_RESOURCE_ADEC1:
            connected_device[0] = COMMON_INPUT_ADEC;
            connected_device[1] = target - HAL_AUDIO_RESOURCE_ADEC0;
            break;
        case HAL_AUDIO_RESOURCE_MIXER0:
        case HAL_AUDIO_RESOURCE_MIXER1:
        case HAL_AUDIO_RESOURCE_MIXER2:
        case HAL_AUDIO_RESOURCE_MIXER3:
        case HAL_AUDIO_RESOURCE_MIXER4:
        case HAL_AUDIO_RESOURCE_MIXER5:
        case HAL_AUDIO_RESOURCE_MIXER6:
        case HAL_AUDIO_RESOURCE_MIXER7:
            connected_device[0] = COMMON_INPUT_AMIXER;
            connected_device[1] = target - HAL_AUDIO_RESOURCE_MIXER0;
            break;
        default:
            break;
    }
}

DTV_STATUS_T KHAL_AUDIO_SNDOUT_eARC_OutputType(ALSA_SNDOUT_EARC_INFO earc_info)
{
    AUDIO_INFO("[%s] set_type %d codec_type %d channel_num %d sample_rate %d mix_option %d\n",
            __FUNCTION__, earc_info.set_type, earc_info.codec_type,
            earc_info.channel_num, earc_info.sample_rate, earc_info.mix_option);

    if(earc_info.set_type == SNDOUT_EARC_OUTPUT_SET_NONE){
        //Output the same as the ARC output set by "Sndout Spdif OutputType"
        g_update_by_ARC = TRUE;
    }else if(earc_info.set_type == SNDOUT_EARC_OUTPUT_SET_ORG){
        //apply mix option
        if(earc_info.mix_option == SNDOUT_EARC_MIXOPTION_NONE){
            switch(earc_info.codec_type)
            {
                case SNDOUT_EARC_CODEC_EAC3:
                    _AudioEARCMode = NON_PCM_OUT_EN_AUTO_BYPASS_DDP;
                    break;
                case SNDOUT_EARC_CODEC_AAC:
                    _AudioEARCMode = NON_PCM_OUT_EN_AUTO_BYPASS_AAC;
                    break;
                case SNDOUT_EARC_CODEC_TRUEHD:
                case SNDOUT_EARC_CODEC_MAT2PCM:
                    _AudioEARCMode = NON_PCM_OUT_EN_AUTO_BYPASS_MAT;
                    break;
                default:
                    _AudioEARCMode = NON_PCM_OUT_EN_AUTO_BYPASS;
                    break;
            }
        }else if(earc_info.mix_option == SNDOUT_EARC_MIXOPTION_ALL){
            switch(earc_info.codec_type)
            {
                case SNDOUT_EARC_CODEC_AC3:
                    _AudioEARCMode = NON_PCM_OUT_EN_AUTO;
                    break;
                case SNDOUT_EARC_CODEC_EAC3:
                    _AudioEARCMode = NON_PCM_OUT_EN_AUTO_DDP;
                    break;
                case SNDOUT_EARC_CODEC_AAC:
                    _AudioEARCMode = NON_PCM_OUT_EN_AUTO_AAC;
                    break;
                case SNDOUT_EARC_CODEC_TRUEHD:
                case SNDOUT_EARC_CODEC_MAT2PCM:
                    _AudioEARCMode = NON_PCM_OUT_EN_AUTO_MAT;
                    break;
                default:
                    _AudioEARCMode = ENABLE_DOWNMIX;
                    break;
            }
        }
    }else if(earc_info.set_type == SNDOUT_EARC_OUTPUT_SET_USE){
        //Apply the set parameters (codec type, number of channel, sampling rate and mix option).
        if(earc_info.mix_option == SNDOUT_EARC_MIXOPTION_NONE){
            switch(earc_info.codec_type)
            {
                case SNDOUT_EARC_CODEC_EAC3:
                    _AudioEARCMode = NON_PCM_OUT_EN_AUTO_BYPASS_DDP;
                    break;
                case SNDOUT_EARC_CODEC_AAC:
                    _AudioEARCMode = NON_PCM_OUT_EN_AUTO_BYPASS_AAC;
                    break;
                case SNDOUT_EARC_CODEC_TRUEHD:
                case SNDOUT_EARC_CODEC_MAT2PCM:
                    _AudioEARCMode = NON_PCM_OUT_EN_AUTO_BYPASS_MAT;
                    break;
                default:
                    _AudioEARCMode = NON_PCM_OUT_EN_AUTO_BYPASS;
                    break;
            }
        }else if(earc_info.mix_option == SNDOUT_EARC_MIXOPTION_ALL){
            switch(earc_info.codec_type)
            {
                case SNDOUT_EARC_CODEC_AC3:
                    _AudioEARCMode = NON_PCM_OUT_EN_AUTO_FORCED_AC3;
                    break;
                case SNDOUT_EARC_CODEC_EAC3:
                    _AudioEARCMode = NON_PCM_OUT_EN_AUTO_FORCED_DDP;
                    break;
                case SNDOUT_EARC_CODEC_AAC:
                    _AudioEARCMode = NON_PCM_OUT_EN_AUTO_AAC;
                    break;
                case SNDOUT_EARC_CODEC_TRUEHD:
                case SNDOUT_EARC_CODEC_MAT2PCM:
                    _AudioEARCMode = NON_PCM_OUT_EN_AUTO_FORCED_MAT;
                    break;
                default:
                    _AudioEARCMode = ENABLE_DOWNMIX;
                    break;
            }
        }
    }
    Update_RawMode_by_connection();

    return OK;
}

DTV_STATUS_T KHAL_AUDIO_SNDOUT_Connect_Set_Status_to_ALSA(common_output_ext_type_t output_type, unsigned int* connected_device)
{
    int i, j, connected_set=1;
    int num_of_connected;
    //reset
    connected_device[0] = 0;
    memset(connected_device, 0, sizeof(unsigned int)*9); // clear Sndout Connect get parameter

    //record the order of connect, first in last out
    for(i = 0; i < ALSA_MAX_SNDOUT_CONNECT; i++){
        if(connected_device_order[i] == 0) break;
    }
    if(i == ALSA_MAX_SNDOUT_CONNECT){
        pr_warn("[%s:%d][WARNING] Sndout too many connect!\n",__FUNCTION__,__LINE__);
        i = ALSA_MAX_SNDOUT_CONNECT-1;
    }
    for(; i > 0; i--){
        connected_device_order[i] = connected_device_order[i-1];
    }
    connected_device_order[0] = (int)output_type;

    //set resource connect status by the order
    for(i = 0; connected_device_order[i] != 0; i++){
        HAL_AUDIO_RESOURCE_T target;
        HAL_AUDIO_RESOURCE_T *INPUT = NULL;
        HAL_AUDIO_RESOURCE_STATUS *STATUS = NULL;
        int connected_device_res = opt2res(connected_device_order[i]);

        if(connected_device_res == HAL_AUDIO_RESOURCE_NO_CONNECTION) return NOT_OK;
        INPUT = ResourceStatus[connected_device_res].inputConnectResourceId;
        STATUS = ResourceStatus[connected_device_res].connectStatus;

        //get numIptconnect
        num_of_connected = 0;
        for(j=i; j < ALSA_MAX_SNDOUT_CONNECT; j++)
            if(connected_device_order[j] == connected_device_order[i])
                num_of_connected++;

        if(STATUS[num_of_connected-1] == HAL_AUDIO_RESOURCE_CONNECT){
            target = INPUT[num_of_connected-1];

            get_resource_type_and_port(target, &connected_device[connected_set]);
            connected_device[0] |= connected_device_order[i];
        } else{ //reorder
            for(j=i; j < (ALSA_MAX_SNDOUT_CONNECT - 1); j++)
                connected_device_order[j] = connected_device_order[j+1];
            connected_device_order[ALSA_MAX_SNDOUT_CONNECT-1] = 0;
            i--;
            continue;
        }

        connected_set +=2;
        if((connected_set-1)/2 >= ALSA_MAX_SNDOUT_GET)
            break;
    }
    return OK;
}

DTV_STATUS_T KHAL_AUDIO_ALSA_SNDOUT_Connect(int opened_device, int mixer_pin)
{
    int output_device = 0;
    int i = 0x1;
    int mixer_res = mixer_pin - FLASH_AUDIO_PIN_1 + HAL_AUDIO_RESOURCE_MIXER0;
    DTV_STATUS_T ret = NOT_OK;
    AUDIO_DEBUG("[%s] opened_device %d mixer pin %d\n",__FUNCTION__, opened_device, (mixer_pin - FLASH_AUDIO_PIN_1));

    if((mixer_pin - FLASH_AUDIO_PIN_1) < 0)
        return NOT_OK;

    while(i != COMMON_MAX_OUTPUT){
        output_device = opened_device & i;
        if(output_device){ // get target opened output device
            int output_device_res = opt2res(output_device);
            if(output_device_res == HAL_AUDIO_RESOURCE_NO_CONNECTION) return NOT_OK;
            //check connect status
            if(GetConnectInputSourceIndex(ResourceStatus[output_device_res], mixer_res) == -1){
                AUDIO_DEBUG("%s %s connect to %s \n", __FUNCTION__,
                        GetResourceString(mixer_res) ,GetResourceString(output_device_res));
                ret = RHAL_AUDIO_SNDOUT_Connect(output_device_res,mixer_res);
                if(ret == NOT_OK) return NOT_OK;
            }
        }
        i = i << 1;
    }
    return OK;
}

DTV_STATUS_T KHAL_AUDIO_ALSA_SNDOUT_Disconnect(int opened_device, int mixer_pin)
{
    int output_device = 0;
    int i = 0x1;
    int mixer_res = mixer_pin - FLASH_AUDIO_PIN_1 + HAL_AUDIO_RESOURCE_MIXER0;
    DTV_STATUS_T ret = NOT_OK;
    AUDIO_DEBUG("[%s] opened_device %d mixer pin %d\n",__FUNCTION__, opened_device, (mixer_pin - FLASH_AUDIO_PIN_1));

    if((mixer_pin - FLASH_AUDIO_PIN_1) < 0)
        return NOT_OK;

    while(i != COMMON_MAX_OUTPUT){
        output_device = opened_device & i;
        if(output_device){ // get target opened output device
            int output_device_res = opt2res(output_device);
            if(output_device_res == HAL_AUDIO_RESOURCE_NO_CONNECTION) return NOT_OK;
            //check connect status
            if(GetConnectInputSourceIndex(ResourceStatus[output_device_res], mixer_res) != -1){
                AUDIO_DEBUG("%s %s disconnect to %s \n", __FUNCTION__,
                        GetResourceString(mixer_res) ,GetResourceString(output_device_res));
                ret = HAL_AUDIO_SNDOUT_Disconnect(output_device_res, mixer_res);
                if(ret == NOT_OK) return NOT_OK;
            }
        }
        i = i << 1;
    }
    return OK;
}

DTV_STATUS_T HAL_AUDIO_SNDOUT_Disconnect(common_output_ext_type_t output_type, int res_input)
{
    HAL_AUDIO_RESOURCE_T currentConnect, inputConnect;
    currentConnect = opt2res(output_type);
    inputConnect   = res_input;
    AUDIO_DEBUG("%s cur %s ipt %s\n", __FUNCTION__,
                GetResourceString(currentConnect), GetResourceString(inputConnect));
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    switch(currentConnect)
    {
        case HAL_AUDIO_RESOURCE_OUT_SPK:
            /* SPK & SE should have never been disconnected */
            if(IsSESource(inputConnect))
            {
                return OK;
            }
            break;
        case HAL_AUDIO_RESOURCE_OUT_SPDIF:
            /* check SE input status before disconnect */
            if(IsSESource(inputConnect) && ADEC_SPDIF_CheckConnect() == TRUE)
            {
                AUDIO_INFO("SE inputs > 0, Keep %s - %s connected\n",
                        GetResourceString(inputConnect), GetResourceString(currentConnect));
                return OK;
            }
        default:
            break;
    }

	if(SetResourceDisconnect(currentConnect, inputConnect) != TRUE)
	{
		return NOT_OK;
	}

    if(IsADECSource(inputConnect))
    {
        HAL_AUDIO_ADEC_INDEX_T adecIndex;

		adecIndex = res2adec(inputConnect);
		if(ADEC_CheckConnect(adecIndex) == FALSE)
		{
            ADSP_DEC_SetMute(adecIndex, ADEC_CheckConnect(adecIndex), GetDecInMute(adecIndex));// remove atv mute effect
        }
    }

    switch(currentConnect)
    {
        case HAL_AUDIO_RESOURCE_OUT_SPK:
            SNDOut_SetMute(ENUM_DEVICE_SPEAKER, ADEC_SPK_CheckConnect(), g_AudioStatusInfo.curSPKMuteStatus);
            break;
        case HAL_AUDIO_RESOURCE_OUT_SPDIF:
            SNDOut_SetMute(ENUM_DEVICE_SPDIF,    ADEC_SPDIF_CheckConnect(), g_AudioStatusInfo.curSPDIFMuteStatus);
            SNDOut_SetMute(ENUM_DEVICE_SPDIF_ES, ADEC_SPDIF_CheckConnect(), g_AudioStatusInfo.curSPDIFMuteStatus);
            Update_RawMode_by_connection();
            break;
        case HAL_AUDIO_RESOURCE_OUT_ARC:
            SNDOut_SetMute(ENUM_DEVICE_SPDIF,    ADEC_ARC_CheckConnect(), g_ARCStatusInfo.curARCMuteStatus);
            SNDOut_SetMute(ENUM_DEVICE_SPDIF_ES, ADEC_ARC_CheckConnect(), g_ARCStatusInfo.curARCMuteStatus);
            Update_RawMode_by_connection();
            break;
        case HAL_AUDIO_RESOURCE_OUT_SB_SPDIF:
            // clean lg opt spdif status
            if(ADEC_SB_SPDIF_CheckConnect() == FALSE)// no connect
            {
                HAL_AUDIO_SB_SET_INFO_T info;
                UINT32 checksum;
                memcpy(&info,  &(g_AudioStatusInfo.curSoundBarInfo), sizeof(info));
                info.barId = 0;
                AUDIO_INFO("%s: SB_CheckConnect() == FALSE, info.barId = 0 !!!!!!! \n",__FUNCTION__);
                RHAL_AUDIO_SB_SetOpticalIDData(info, &checksum);
            }
            SNDOut_SetMute(ENUM_DEVICE_SPDIF,    ADEC_SB_SPDIF_CheckConnect(), g_AudioStatusInfo.curSPDIF_LG_MuteStatus);
            SNDOut_SetMute(ENUM_DEVICE_SPDIF_ES, ADEC_SB_SPDIF_CheckConnect(), g_AudioStatusInfo.curSPDIF_LG_MuteStatus);
            Update_RawMode_by_connection();
            break;
        case HAL_AUDIO_RESOURCE_OUT_HP:
            SNDOut_SetMute(ENUM_DEVICE_HEADPHONE, ADEC_HP_CheckConnect(), g_AudioStatusInfo.curHPMuteStatus);
            break;
        default:
            break;
    }

    if(IsADECSource(inputConnect))
    {
        HAL_AUDIO_ADEC_INDEX_T adecIndex = res2adec(inputConnect);
        ADSP_DEC_SetMute(adecIndex, ADEC_CheckConnect(adecIndex), GetDecInMute(adecIndex));// disconnect , auto mute
    }
    else if(IsAMIXSource(inputConnect) == TRUE)
    {
        HAL_AUDIO_MIXER_INDEX_T amixerIndex= res2amixer(inputConnect);
        ADSP_AMIXER_SetMute(amixerIndex, AMIXER_CheckConnect(inputConnect), GetAmixerUserMute(amixerIndex));
    }

    return OK;
}

static BOOLEAN AddAndConnectDecAinToFlow(FlowManager* pFlowManager, HAL_AUDIO_ADEC_INDEX_T adecIndex, Base* pAin, HAL_AUDIO_FLOW_STATUS* pFlowStatus)
{
    if(pFlowManager == NULL)
        return FALSE;

    if(!IsValidAdecIdx(adecIndex))
        return FALSE;

    if(pAin == NULL)
        return FALSE;

    if(pFlowStatus == NULL)
        return FALSE;

    if(adecIndex == Aud_mainDecIndex)
    {
        pFlowStatus->IsAINExist = TRUE;
    }
    else
    {
        pFlowStatus->IsSubAINExist = TRUE;
    }

    if((adecIndex == HAL_AUDIO_ADEC0))
    {
        pFlowStatus->IsDEC0Exist = TRUE;
    }
    if((adecIndex == HAL_AUDIO_ADEC1))
    {
        pFlowStatus->IsDEC1Exist = TRUE;
    }
#if defined(VIRTUAL_ADEC_ENABLED)
    if((adecIndex == HAL_AUDIO_ADEC2))
    {
        pFlowStatus->IsDEC2Exist = TRUE;
    }
    if((adecIndex == HAL_AUDIO_ADEC3))
    {
        pFlowStatus->IsDEC3Exist = TRUE;
    }
#endif

    pFlowManager->Connect(pFlowManager, pAin, Aud_dec[adecIndex]);

    pFlowStatus->AinConnectDecIndex = adecIndex;

    return TRUE;
}

static BOOLEAN AddAndConnectSourceFilterDecToFlow(FlowManager* pFlowManager, HAL_AUDIO_ADEC_INDEX_T adecIndex, Base* pSource, HAL_AUDIO_FLOW_STATUS* pFlowStatus)
{
    if(pFlowManager == NULL)
        return FALSE;

    if(!IsValidAdecIdx(adecIndex))
        return FALSE;

    if(pFlowStatus == NULL)
        return FALSE;


    if((adecIndex == HAL_AUDIO_ADEC0))
    {
        pFlowStatus->IsDEC0Exist = TRUE;

        if(GetSysMemoryExist(adecIndex))
        {
            pFlowStatus->IsSystemOutput0 = TRUE;
        }
    }
    if((adecIndex == HAL_AUDIO_ADEC1))
    {
        pFlowStatus->IsDEC1Exist = TRUE;

        if(GetSysMemoryExist(adecIndex))
        {
            pFlowStatus->IsSystemOutput1 = TRUE;
        }
    }
#if defined(VIRTUAL_ADEC_ENABLED)
    if((adecIndex == HAL_AUDIO_ADEC2))
    {
        pFlowStatus->IsDEC2Exist = TRUE;

        if(GetSysMemoryExist(adecIndex))
        {
            pFlowStatus->IsSystemOutput2 = TRUE;
        }
    }
    if((adecIndex == HAL_AUDIO_ADEC3))
    {
        pFlowStatus->IsDEC3Exist = TRUE;

        if(GetSysMemoryExist(adecIndex))
        {
            pFlowStatus->IsSystemOutput3 = TRUE;
        }
    }
#endif

    return TRUE;
}

static BOOLEAN AddAndConnectDTVSourceFilterDecToFlow(FlowManager* pFlowManager, HAL_AUDIO_ADEC_INDEX_T adecIndex, Base* pAud_DTV, HAL_AUDIO_FLOW_STATUS* pFlowStatus)
{
    HAL_AUDIO_RESOURCE_T curResourceId, decIptResId;

    if(pFlowManager == NULL)
        return FALSE;

    if(!IsValidAdecIdx(adecIndex))
        return FALSE;

    if(pFlowStatus == NULL)
        return FALSE;

    /*assert(adecIndex < 4);*/
    curResourceId = adec2res(adecIndex);

    decIptResId = GetNonOutputModuleSingleInputResource(ResourceStatus[curResourceId]);// dec input source

    if(decIptResId == HAL_AUDIO_RESOURCE_ATP0)
        pFlowStatus->IsDTV0SourceRead = TRUE;
    else if(decIptResId == HAL_AUDIO_RESOURCE_ATP1)
        pFlowStatus->IsDTV1SourceRead = TRUE;
    else
    {
        AUDIO_FATAL("[AUDH-FATAL] unknow atp resource id %d \n", decIptResId);
        /*assert(0);*/
    }

    if((adecIndex == HAL_AUDIO_ADEC0))
    {
        pFlowStatus->IsDEC0Exist = TRUE;
    }
    if((adecIndex == HAL_AUDIO_ADEC1))
    {
        pFlowStatus->IsDEC1Exist = TRUE;
    }
#if defined(VIRTUAL_ADEC_ENABLED)
    if((adecIndex == HAL_AUDIO_ADEC2))
    {
        pFlowStatus->IsDEC2Exist = TRUE;
    }
    if((adecIndex == HAL_AUDIO_ADEC3))
    {
        pFlowStatus->IsDEC3Exist = TRUE;
    }
#endif

    return TRUE;
}

static BOOLEAN AddAndConnectEncFilterToFlow(FlowManager* pFlowManager, HAL_AUDIO_ADEC_INDEX_T adecIndex, HAL_AUDIO_AENC_INDEX_T aencIndex, HAL_AUDIO_FLOW_STATUS* pFlowStatus)
{
    if(pFlowManager == NULL)
        return FALSE;

    if(!IsValidAdecIdx(adecIndex))
        return FALSE;

    if(!RangeCheck(aencIndex, 0, AUD_AENC_MAX))
        return FALSE;

    if(pFlowStatus == NULL)
        return FALSE;

    pFlowStatus->IsEncExist = TRUE;

    pFlowManager->Connect(pFlowManager, Aud_dec[adecIndex], Aud_aenc[aencIndex]->pEnc);
    pFlowManager->Connect(pFlowManager, Aud_aenc[aencIndex]->pEnc, Aud_aenc[aencIndex]->pMemOut);

    return TRUE;
}

static BOOLEAN AddAndConnectPPAOToFlow(FlowManager* pFlowManager, HAL_AUDIO_ADEC_INDEX_T adecIndex, Base* pPPAO, HAL_AUDIO_FLOW_STATUS* pFlowStatus)
{
    if(pFlowManager == NULL)
        return FALSE;

    if(!IsValidAdecIdx(adecIndex))
        return FALSE;

    if((pPPAO == NULL) /*&& (aencIndex != HAL_AUDIO_AENC1)*/)
        return FALSE;

    if(pFlowStatus == NULL)
        return FALSE;

    if(pPPAO == Aud_ppAout)
        pFlowStatus->IsMainPPAOExist = TRUE;
    else
    {
        AUDIO_DEBUG("set sub pp\n");
        pFlowStatus->IsSubPPAOExist = TRUE;
    }

    pFlowManager->Connect(pFlowManager, Aud_dec[adecIndex], pPPAO);

    return TRUE;
}

static BOOLEAN SwitchHDMIFocus(Base* pAin)
{
    int hdmiInputPin;
    if((hdmiInputPin = GetCurrentHDMIConnectPin()) >= 0)
    {
        AUDIO_IPT_SRC audioInput = {0};

        audioInput.focus[0] = AUDIO_IPT_SRC_SPDIF;
        audioInput.focus[1] = AUDIO_IPT_SRC_UNKNOWN;
        audioInput.focus[2] = AUDIO_IPT_SRC_UNKNOWN;
        audioInput.focus[3] = AUDIO_IPT_SRC_UNKNOWN;
        audioInput.mux_in = AIO_SPDIFI_HDMI;
        pAin->SwitchFocus(pAin, &audioInput);
        return TRUE;
    }
    else
    {
        AUDIO_ERROR("find hdmi pin failed\n");
        return FALSE;
    }
}

static BOOLEAN SwitchADCFocus(Base* pAin)
{
    int adcInputPin;
    if((adcInputPin = GetCurrentADCConnectPin()) >= 0)
    {
        AUDIO_IPT_SRC audioInput = {0};

        audioInput.focus[0] = AUDIO_IPT_SRC_BBADC;
        audioInput.focus[1] = AUDIO_IPT_SRC_UNKNOWN;
        audioInput.focus[2] = AUDIO_IPT_SRC_UNKNOWN;
        audioInput.focus[3] = AUDIO_IPT_SRC_UNKNOWN;

        if(adcInputPin == AUDIO_BBADC_SRC_AIO_PORT_NUM)
            audioInput.mux_in = AUDIO_BBADC_SRC_AIO1;
        else if(adcInputPin == AUDIO_BBADC_SRC_AIN1_PORT_NUM)
            audioInput.mux_in = AUDIO_BBADC_SRC_AIN1;
        else if(adcInputPin == AUDIO_BBADC_SRC_AIN2_PORT_NUM)
            audioInput.mux_in = AUDIO_BBADC_SRC_AIN2;
        else if(adcInputPin == AUDIO_BBADC_SRC_AIN3_PORT_NUM)
            audioInput.mux_in = AUDIO_BBADC_SRC_AIN3;
        else
        {
            audioInput.mux_in = AUDIO_BBADC_SRC_MUTE_ALL;
            AUDIO_DEBUG("ain mut in set mute %x \n",  audioInput.mux_in);
        }

        audioInput.mux_in = AUDIO_BBADC_SRC_AIN1;
        AUDIO_DEBUG("Force let adc input pin is ain1 for demo\n");
        AUDIO_DEBUG("ADC connect pin %d  set fw ain mut in index = %x \n",  adcInputPin, audioInput.mux_in);

        pAin->SwitchFocus(pAin, &audioInput);
        return TRUE;
    }
    else
    {
        AUDIO_ERROR("find adc pin failed\n");
        return FALSE;
    }
}

static BOOLEAN SwitchATVFocus(Base* pAin)
{
    AUDIO_IPT_SRC audioInput = {0};

    audioInput.focus[0] = AUDIO_IPT_SRC_ATV;
    audioInput.focus[1] = AUDIO_IPT_SRC_UNKNOWN;
    audioInput.focus[2] = AUDIO_IPT_SRC_UNKNOWN;
    audioInput.focus[3] = AUDIO_IPT_SRC_UNKNOWN;
    pAin->SwitchFocus(pAin, &audioInput);

    return TRUE;
}

BOOLEAN RemoveFilter(HAL_AUDIO_FLOW_STATUS* prevStatus, FlowManager* pFlowManager)
{
    if(prevStatus->IsAINExist)
    {
        AUDIO_DEBUG("remove main ain\n");
        pFlowManager->Remove(pFlowManager, Aud_MainAin);
    }
    prevStatus->IsAINExist =0;

    if(prevStatus->IsSubAINExist)
    {
        AUDIO_DEBUG("remove sub ain\n");
        pFlowManager->Remove(pFlowManager, Aud_SubAin);
    }
    prevStatus->IsSubAINExist = 0;

    prevStatus->IsSystemOutput0 = 0;
    prevStatus->IsSystemOutput1 = 0;
#if defined(VIRTUAL_ADEC_ENABLED)
    prevStatus->IsSystemOutput2 = 0;
    prevStatus->IsSystemOutput3 = 0;
#endif
    if(prevStatus->IsDTV0SourceRead)
    {
        /* Reset external reference clock */
        pFlowManager->SetExtRefClock(pFlowManager, 0);
    }
    prevStatus->IsDTV0SourceRead = 0;

    if(prevStatus->IsDTV1SourceRead)
    {
        /* Reset external reference clock */
        pFlowManager->SetExtRefClock(pFlowManager, 0);
    }
    prevStatus->IsDTV1SourceRead = 0;

    if(prevStatus->IsDEC0Exist)
    {
        pFlowManager->Remove(pFlowManager, Aud_dec[0]);
    }
    prevStatus->IsDEC0Exist = 0;

    if(prevStatus->IsDEC1Exist)
    {
        pFlowManager->Remove(pFlowManager, Aud_dec[1]);
    }
    prevStatus->IsDEC1Exist = 0;

#if defined(VIRTUAL_ADEC_ENABLED)
    if(prevStatus->IsDEC2Exist)
    {
        pFlowManager->Remove(pFlowManager, Aud_dec[2]);
    }
    prevStatus->IsDEC2Exist = 0;

    if(prevStatus->IsDEC3Exist)
    {
        pFlowManager->Remove(pFlowManager, Aud_dec[3]);
    }
    prevStatus->IsDEC3Exist = 0;
#endif
    if(prevStatus->IsEncExist)
    {
        pFlowManager->Remove(pFlowManager, Aud_aenc[0]->pEnc);
        pFlowManager->Remove(pFlowManager, Aud_aenc[0]->pMemOut);
    }
    prevStatus->IsEncExist = 0;

    if(prevStatus->IsMainPPAOExist)
    {
        pFlowManager->Remove(pFlowManager, Aud_ppAout);
    }
    prevStatus->IsMainPPAOExist = 0;

    if(prevStatus->IsSubPPAOExist)
    {
        pFlowManager->Remove(pFlowManager, Aud_subPPAout);
    }
    prevStatus->IsSubPPAOExist = 0;

    pFlowManager->ShowCurrentExitModule(pFlowManager, 0);


    return TRUE;
}

void InitAudioFormat(HAL_AUDIO_SRC_TYPE_T inputType, AUDIO_FORMAT* a_format)
{
    if(a_format == NULL)
        return;

    switch(inputType)
    {
    case HAL_AUDIO_SRC_TYPE_PCM:
        a_format->type = RHALTYPE_PCM_LITTLE_ENDIAN;
        break;
    case HAL_AUDIO_SRC_TYPE_AC3 :
        a_format->type = RHALTYPE_DOLBY_AC3;
        break;
    case HAL_AUDIO_SRC_TYPE_EAC3 :
        a_format->type = RHALTYPE_DDP;
        break;
    case HAL_AUDIO_SRC_TYPE_MP3:
    case HAL_AUDIO_SRC_TYPE_MPEG:
        a_format->type = RHALTYPE_MPEG_AUDIO;
        break;
    case HAL_AUDIO_SRC_TYPE_HEAAC:
    case HAL_AUDIO_SRC_TYPE_AAC:
        a_format->type = RHALTYPE_AAC;
        break;
    case HAL_AUDIO_SRC_TYPE_DRA:
        a_format->type = RHALTYPE_DRA;
        break;
    case HAL_AUDIO_SRC_TYPE_DTS:
        a_format->type = RHALTYPE_DTS;
        break;
    case HAL_AUDIO_SRC_TYPE_WMA_PRO:
        a_format->type = RHALTYPE_WMAPRO;
        break;
    case HAL_AUDIO_SRC_TYPE_AMR_NB:
        a_format->type = RHALTYPE_AMRNB_AUDIO;
        break;
    case HAL_AUDIO_SRC_TYPE_AMR_WB:
        a_format->type = RHALTYPE_AMRWB_AUDIO;
        break;
    case HAL_AUDIO_SRC_TYPE_RA8:
        a_format->type = RHALTYPE_RA_COOK;
        break;
    case HAL_AUDIO_SRC_TYPE_VORBIS:
        a_format->type = RHALTYPE_OGG_AUDIO;
        break;
    case HAL_AUDIO_SRC_TYPE_FLAC:
        a_format->type = RHALTYPE_FLAC;
        break;
    default :
        AUDIO_ERROR("Error get unknow type %d \n", inputType);
        a_format->type = RHALTYPE_NONE;
        break;
    }
    return;
}

AUDIO_DEC_TYPE SRCType_to_ADECType(HAL_AUDIO_SRC_TYPE_T audioSrcType)
{
	AUDIO_DEC_TYPE dec_type = AUDIO_UNKNOWN_TYPE;
    switch(audioSrcType)
    {
    default :
        AUDIO_ERROR("unknow type %x\n", audioSrcType);
    case HAL_AUDIO_SRC_TYPE_UNKNOWN:
    case HAL_AUDIO_SRC_TYPE_NONE:
        dec_type = AUDIO_UNKNOWN_TYPE;
        break;
    case HAL_AUDIO_SRC_TYPE_PCM:
        dec_type = AUDIO_LPCM_DECODER_TYPE;
        break;
    case HAL_AUDIO_SRC_TYPE_AC3:
        dec_type = AUDIO_AC3_DECODER_TYPE;
        break;
    case HAL_AUDIO_SRC_TYPE_EAC3:
    case HAL_AUDIO_SRC_TYPE_EAC3_ATMOS:
        dec_type = AUDIO_DDP_DECODER_TYPE;
        break;
    case HAL_AUDIO_SRC_TYPE_MP3:
    case HAL_AUDIO_SRC_TYPE_MPEG:
        dec_type = AUDIO_MPEG_DECODER_TYPE;
        break;
    case HAL_AUDIO_SRC_TYPE_AAC:
    case HAL_AUDIO_SRC_TYPE_HEAAC:
        dec_type = AUDIO_AAC_DECODER_TYPE;
        break;
    case HAL_AUDIO_SRC_TYPE_DRA:
        dec_type = AUDIO_DRA_DECODER_TYPE;
        break;
    case HAL_AUDIO_SRC_TYPE_MPEG_H:
        dec_type = AUDIO_MPEGH_DECODER_TYPE;
        break;
    case HAL_AUDIO_SRC_TYPE_AC4:
    case HAL_AUDIO_SRC_TYPE_AC4_ATMOS:
        dec_type = AUDIO_AC4_DECODER_TYPE;
        break;
    case HAL_AUDIO_SRC_TYPE_DTS:
    case HAL_AUDIO_SRC_TYPE_DTS_CD:
        dec_type = AUDIO_DTS_DECODER_TYPE;
        break;
    case HAL_AUDIO_SRC_TYPE_DTS_EXPRESS:
    case HAL_AUDIO_SRC_TYPE_DTS_HD_MA:
        dec_type = AUDIO_DTS_HD_DECODER_TYPE;
        break;
    case HAL_AUDIO_SRC_TYPE_MAT:
    case HAL_AUDIO_SRC_TYPE_MAT_ATMOS:
        dec_type = AUDIO_MAT_DECODER_TYPE;
        break;
    case HAL_AUDIO_SRC_TYPE_TRUEHD:
    case HAL_AUDIO_SRC_TYPE_TRUEHD_ATMOS:
        dec_type = AUDIO_MAT_DECODER_TYPE;
        break;
    }
    return dec_type;
}

HAL_AUDIO_SRC_TYPE_T ADECType_to_SRCType(AUDIO_DEC_TYPE audioSrcType, long reserved)
{
    HAL_AUDIO_SRC_TYPE_T  src_type = HAL_AUDIO_SRC_TYPE_UNKNOWN;
    switch(audioSrcType)
    {
    default :
    case AUDIO_UNKNOWN_TYPE :
        break;
    case AUDIO_MPEG_DECODER_TYPE :
        src_type = HAL_AUDIO_SRC_TYPE_MPEG;
        break;
    case AUDIO_AC3_DECODER_TYPE :
        src_type = HAL_AUDIO_SRC_TYPE_AC3;
        break;
    case AUDIO_LPCM_DECODER_TYPE :
        src_type = HAL_AUDIO_SRC_TYPE_PCM;
        break;
    case AUDIO_DTS_HIGH_RESOLUTION_DECODER_TYPE :
    case AUDIO_DTS_LBR_DECODER_TYPE :
    case AUDIO_DTS_MASTER_AUDIO_DECODER_TYPE :
    case AUDIO_DTS_DECODER_TYPE :
        src_type = HAL_AUDIO_SRC_TYPE_DTS;
        break;
    case AUDIO_WMA_DECODER_TYPE :
        break;
    case AUDIO_MP4AAC_DECODER_TYPE :
    case AUDIO_MP4HEAAC_DECODER_TYPE :
    case AUDIO_AAC_DECODER_TYPE :
    case AUDIO_RAW_AAC_DECODER_TYPE :
        {
            int version;
            //-- reserved[1] = ((VERSION<<8) & 0xFF00) | (FORMAT & 0x00FF)
            // bit [0:7]  for format  /* LOAS/LATM = 0x0, ADTS = 0x1 */
            // bit [8:15]  for version   /* AAC = 0x0, HE-AACv1 = 0x1, HE-AACv2 = 0x2 */
            version = (reserved >> 8) & 0xFF;
            if(version != 0) {
                src_type = HAL_AUDIO_SRC_TYPE_HEAAC;
            } else {
                src_type = HAL_AUDIO_SRC_TYPE_AAC;
            }
            break;
        }
    case AUDIO_VORBIS_DECODER_TYPE :
        break;
    case AUDIO_DV_DECODER_TYPE :
        break;
    case AUDIO_DDP_DECODER_TYPE :
        {
            int atmos = (reserved) & 0x1;
            if(atmos) {
                src_type = HAL_AUDIO_SRC_TYPE_EAC3_ATMOS;
            } else {
                src_type = HAL_AUDIO_SRC_TYPE_EAC3;
            }
            break;
        }
    case AUDIO_DTS_HD_DECODER_TYPE :
        src_type = HAL_AUDIO_SRC_TYPE_DTS_HD_MA;
        break;
    case AUDIO_WMA_PRO_DECODER_TYPE :
        src_type = HAL_AUDIO_SRC_TYPE_WMA_PRO;
        break;
    case AUDIO_MP3_PRO_DECODER_TYPE :
        break;
    case AUDIO_RA1_DECODER_TYPE :
        break;
    case AUDIO_RA2_DECODER_TYPE :
        break;
    case AUDIO_ATRAC3_DECODER_TYPE :
        break;
    case AUDIO_COOK_DECODER_TYPE :
        src_type = HAL_AUDIO_SRC_TYPE_RA8;
        break;
    case AUDIO_LSD_DECODER_TYPE :
        break;
    case AUDIO_ADPCM_DECODER_TYPE :
        src_type = HAL_AUDIO_SRC_TYPE_ADPCM;
        break;
    case AUDIO_FLAC_DECODER_TYPE :
        src_type = HAL_AUDIO_SRC_TYPE_FLAC;
        break;
    case AUDIO_ULAW_DECODER_TYPE :
        break;
    case AUDIO_ALAW_DECODER_TYPE :
        break;
    case AUDIO_ALAC_DECODER_TYPE :
        break;
    case AUDIO_AMRNB_DECODER_TYPE :
        src_type = HAL_AUDIO_SRC_TYPE_AMR_NB;
        break;
    case AUDIO_MIDI_DECODER_TYPE :
        break;
    case AUDIO_APE_DECODER_TYPE :
        break;
    case AUDIO_AVS_DECODER_TYPE :
        break;
    case AUDIO_NELLYMOSER_DECODER_TYPE :
        break;
    case AUDIO_WMA_LOSSLESS_DECODER_TYPE :
        break;
    case AUDIO_UNCERTAIN_DECODER_TYPE :
        break;
    case AUDIO_UNCERTAIN_HDMV_DECODER_TYPE :
        break;
    case AUDIO_ILBC_DECODER_TYPE :
        break;
    case AUDIO_SILK_DECODER_TYPE :
        break;
    case AUDIO_AMRWB_DECODER_TYPE :
        src_type = HAL_AUDIO_SRC_TYPE_AMR_WB;
        break;
    case AUDIO_G729_DECODER_TYPE :
        break;
    case AUDIO_DRA_DECODER_TYPE :
        src_type = HAL_AUDIO_SRC_TYPE_DRA;
        break;
    case AUDIO_OPUS_DECODER_TYPE :
        break;
    case AUDIO_MPEGH_DECODER_TYPE:
        src_type = HAL_AUDIO_SRC_TYPE_MPEG_H;
        break;
    case AUDIO_AC4_DECODER_TYPE:
        {
            int atmos = (reserved) & 0x1;
            if(atmos) {
                src_type = HAL_AUDIO_SRC_TYPE_AC4_ATMOS;
            } else {
                src_type = HAL_AUDIO_SRC_TYPE_AC4;
            }
        }
        break;
    case AUDIO_MLP_DECODER_TYPE :
    case AUDIO_MAT_DECODER_TYPE:
        {
            int atmos  = (reserved) & 0x1;
            int truehd = (reserved>>1) & 0x1;
            if(truehd) {
                if(atmos) {
                    src_type = HAL_AUDIO_SRC_TYPE_TRUEHD_ATMOS;
                } else {
                    src_type = HAL_AUDIO_SRC_TYPE_TRUEHD;
                }
            } else {
                if(atmos) {
                    src_type = HAL_AUDIO_SRC_TYPE_MAT_ATMOS;
                } else {
                    src_type = HAL_AUDIO_SRC_TYPE_MAT;
                }
            }
        }
        break;
    }

    return src_type;
}

ahdmi_type_ext_type_t SRCType_to_HDMIMode(HAL_AUDIO_SRC_TYPE_T src_type)
{
    ahdmi_type_ext_type_t HDMIMode;
    switch(src_type)
    {
    case HAL_AUDIO_SRC_TYPE_PCM:
        HDMIMode = AHDMI_PCM;
        break;
    case HAL_AUDIO_SRC_TYPE_AC3:
        HDMIMode = AHDMI_AC3;
        break;
    case HAL_AUDIO_SRC_TYPE_MPEG:
        HDMIMode = AHDMI_MPEG;
        break;
    case HAL_AUDIO_SRC_TYPE_AAC:
        HDMIMode = AHDMI_AAC;
        break;
    case HAL_AUDIO_SRC_TYPE_DTS:
        HDMIMode = AHDMI_DTS;
        break;
    case HAL_AUDIO_SRC_TYPE_DTS_HD_MA:
        HDMIMode = AHDMI_DTS_HD_MA;
        break;
    case HAL_AUDIO_SRC_TYPE_DTS_EXPRESS:
        HDMIMode = AHDMI_DTS_EXPRESS;
        break;
    case HAL_AUDIO_SRC_TYPE_DTS_CD:
        HDMIMode = AHDMI_DTS_CD;
        break;
    case HAL_AUDIO_SRC_TYPE_EAC3:
    case HAL_AUDIO_SRC_TYPE_EAC3_ATMOS:
        HDMIMode = AHDMI_EAC3;
        break;
    case HAL_AUDIO_SRC_TYPE_MAT:
    case HAL_AUDIO_SRC_TYPE_MAT_ATMOS:
    case HAL_AUDIO_SRC_TYPE_TRUEHD:
    case HAL_AUDIO_SRC_TYPE_TRUEHD_ATMOS:
        HDMIMode = AHDMI_MAT;
        break;
    default:
        HDMIMode = AHDMI_UNKNOWN;
        break;
    }
    return HDMIMode;
}

DTV_STATUS_T RHAL_AUDIO_GetDecodingType(HAL_AUDIO_ADEC_INDEX_T adecIndex, HAL_AUDIO_SRC_TYPE_T *pAudioType)
{
    AUDIO_RPC_DEC_FORMAT_INFO dec_fomat;
    UINT32 retStatus;

    HAL_AUDIO_RESOURCE_T curResourceId = adec2res(adecIndex);
    HAL_AUDIO_RESOURCE_T decIptResId;

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(pAudioType == NULL)
        return NOT_OK;

    if(!IsValidAdecIdx(adecIndex))
        return NOT_OK;

    if(AUDIO_HAL_CHECK_STOP_NOTAVAILABLE(ResourceStatus[curResourceId], 0))
    {
        AUDIO_DEBUG("%s play check failed %d \n",
                      __FUNCTION__, ResourceStatus[curResourceId].connectStatus[0]);
        return NOT_OK;
    }

    retStatus = ((DEC*)Aud_dec[adecIndex]->pDerivedObj)->GetAudioFormatInfo(Aud_dec[adecIndex], &dec_fomat);

    if((retStatus != S_OK) )
    {
        ( *pAudioType = GetUserFormat(adecIndex)); //WOSQRTK-3050 , return default setting
        AUDIO_DEBUG("get return fail return %s\n",  GetSRCTypeName(*pAudioType));
        return OK;
    }

    *pAudioType = ADECType_to_SRCType(dec_fomat.type, dec_fomat.reserved[1]);

    if((dec_fomat.type == AUDIO_UNKNOWN_TYPE) )
    {
        ( *pAudioType = GetUserFormat(adecIndex)); //WOSQRTK-3050 , return default setting
        AUDIO_DEBUG("get return format is unknown return %s\n ", GetSRCTypeName(*pAudioType) );
        return OK;
    }

    decIptResId = GetNonOutputModuleSingleInputResource(ResourceStatus[curResourceId]);// dec input source
    if(IsDTVSource(decIptResId)){
        if(*pAudioType != GetUserFormat(adecIndex)) {
            AUDIO_DEBUG("DTV start with %s, but get %s, return default\n",
                    GetSRCTypeName(GetUserFormat(adecIndex)),GetSRCTypeName(*pAudioType));
            ( *pAudioType = GetUserFormat(adecIndex)); //KTASKWBS-9070, return hal start format
        }
    }

    return OK;
}

#if 1
void FillDecInput(HAL_AUDIO_RESOURCE_T adecId)
{
    HAL_AUDIO_RESOURCE_T decIptResId = GetNonOutputModuleSingleInputResource(ResourceStatus[adecId]);

    if(decIptResId < HAL_MAX_RESOURCE_NUM)
    {
        // dec input

        // pipe2

        HAL_AUDIO_RESOURCE_T sdecResId = GetNonOutputModuleSingleInputResource(ResourceStatus[decIptResId]);

        // drow pipe 1
        if(sdecResId  < HAL_MAX_RESOURCE_NUM)
        {
            strcat(AUD_StringBuffer, GetResourceString(sdecResId));
            strcat(AUD_StringBuffer, "---");
        }

        strcat(AUD_StringBuffer, GetResourceString(decIptResId));

        strcat(AUD_StringBuffer, "---");

        // draw pipe 3
        strcat(AUD_StringBuffer, GetResourceString(adecId));
    }
    else
    {
        strcat(AUD_StringBuffer, "no input---");
        strcat(AUD_StringBuffer, GetResourceString(adecId));
    }
}


void FillConnectedOutput(HAL_AUDIO_RESOURCE_T resId)
{
    HAL_AUDIO_RESOURCE_T decOptResourceId[5];
    int totalDecOutputConnectResource;
    int i;
    char buffer[10];

    GetConnectOutputSource(resId, decOptResourceId, 5, &totalDecOutputConnectResource);
    if(totalDecOutputConnectResource > 0)
    {
        strcat(AUD_StringBuffer, "---(");
        for(i = 0; i < totalDecOutputConnectResource; i++)
        {
            sprintf(&buffer[0]," %d.", i);
            strcat(AUD_StringBuffer, buffer);
            strcat(AUD_StringBuffer, GetResourceString(decOptResourceId[i]));

            if(IsSESource(decOptResourceId[i]))
            {
                HAL_AUDIO_RESOURCE_T seOptResourceId[5];
                int totalSeOutputConnectResource;
                GetConnectOutputSource(decOptResourceId[i], seOptResourceId, 5, &totalSeOutputConnectResource);

                if(totalSeOutputConnectResource >= 1)
                {

                    strcat(AUD_StringBuffer, "---");
                    strcat(AUD_StringBuffer, GetResourceString(seOptResourceId[0]));
                }
                else
                    strcat(AUD_StringBuffer, "---X");

            }
            strcat(AUD_StringBuffer, ",");
        }
        if(totalDecOutputConnectResource > 0)
            strcat(AUD_StringBuffer, ")");
    }
    else
    {
        strcat(AUD_StringBuffer, "---no output");
    }
}

void ShowFlow(HAL_AUDIO_RESOURCE_T adecResId, HAL_AUDIO_RESOURCE_STATUS newStatus, int forcePrint)
{
    /*int stringLength;*/
    /*char* resString;*/
    /*int strpos;*/
    /*int pipe0offset = 0;*/
    /*HAL_AUDIO_RESOURCE_T subDecResId, decInputResId;*/
    /*HAL_AUDIO_RESOURCE_T outputResId[3];*/
    /*int totalOutputConnectResource;*/
    /*int isSubDechasoutput  = 0;*/
    /*int isMaixDechasoutput  = 0;*/
    int i;
    int mainDecResId;
    HAL_AUDIO_RESOURCE_T decId[AUD_ADEC_MAX];
    HAL_AUDIO_RESOURCE_T mixerOptResourceId[5];
    int totalmixerOutputConnectResource;

    // check input
    // Amixer connect data

    mainDecResId = adec2res((HAL_AUDIO_ADEC_INDEX_T)Aud_mainDecIndex);

    for(i = (int)HAL_AUDIO_RESOURCE_MIXER0; i <= HAL_AUDIO_RESOURCE_MIXER7 ; i++)
    {
        GetConnectOutputSource(((HAL_AUDIO_RESOURCE_T) i), mixerOptResourceId, 5, &totalmixerOutputConnectResource);
        memset(AUD_StringBuffer, 0, sizeof(AUD_StringBuffer));
        sprintf(AUD_StringBuffer, "AMixer%d:: status (%s)", (i-HAL_AUDIO_RESOURCE_MIXER0), GetResourceStatusString((int)(ResourceStatus[((HAL_AUDIO_RESOURCE_T) i)].connectStatus[0])));
        //if(totalmixerOutputConnectResource >= 1)
        {
            FillConnectedOutput((HAL_AUDIO_RESOURCE_T) i);
        }
        // else
        {
            //strcat(AUD_StringBuffer, "AMixer%d  no connect to output", i);
        }

        if(forcePrint)
            pr_err("%s\n", AUD_StringBuffer);
        else
            AUDIO_DEBUG("%s\n", AUD_StringBuffer);
    }

    // dec 0
    decId[0] = HAL_AUDIO_RESOURCE_ADEC0;
    decId[1] = HAL_AUDIO_RESOURCE_ADEC1;
#if defined(VIRTUAL_ADEC_ENABLED)
    decId[2] = HAL_AUDIO_RESOURCE_ADEC2;
    decId[3] = HAL_AUDIO_RESOURCE_ADEC3;
#endif
    for(i = 0; i < AUD_ADEC_MAX; i++)
    {
        int DecResId =  decId[i];
        memset(AUD_StringBuffer, 0, sizeof(AUD_StringBuffer));// clean

        if(mainDecResId == DecResId)
        {
            strcat(AUD_StringBuffer, "Main:: ");
        }
        else
        {
            if(mainDecResId != -1)
            {
                strcat(AUD_StringBuffer, " Sub:: ");
            }
            else
            {
                strcat(AUD_StringBuffer, "Unknown Main ID:: ");
            }
        }

        FillDecInput(decId[i]);

        if(ResourceStatus[decId[i]].connectStatus[0] == HAL_AUDIO_RESOURCE_RUN)
        {
            if(decId[i] == adecResId)
            {
                if(newStatus == HAL_AUDIO_RESOURCE_RUN)
                    strcat(AUD_StringBuffer, "(Run  to  Run)");
                else if(newStatus == HAL_AUDIO_RESOURCE_STOP)
                    strcat(AUD_StringBuffer, "(Run  to  Stop)");
                else if(newStatus == HAL_AUDIO_RESOURCE_PAUSE)
                    strcat(AUD_StringBuffer, "(Run  to  Pause)");
                else
                    strcat(AUD_StringBuffer, "(Running State)");
            }
            else
                strcat(AUD_StringBuffer, "(Running State)");


            if(GetDectUserState(i) == 0)
                strcat(AUD_StringBuffer, "(WebOS setting is Stop)");

        }
        else if(ResourceStatus[decId[i]].connectStatus[0] == HAL_AUDIO_RESOURCE_PAUSE)
        {
            if(decId[i] == adecResId)
            {
                if(newStatus == HAL_AUDIO_RESOURCE_RUN)
                    strcat(AUD_StringBuffer, "(Pause  to Run)");
                else if(newStatus == HAL_AUDIO_RESOURCE_STOP)
                    strcat(AUD_StringBuffer, "(Pause  to  Stop)");
                else if(newStatus == HAL_AUDIO_RESOURCE_PAUSE)
                    strcat(AUD_StringBuffer, "(Pause  to  Pause)");
                else
                    strcat(AUD_StringBuffer, "(Pause   State)");
            }
            else
                strcat(AUD_StringBuffer, "(Pause   State)");
        }
        else
        {
            if(decId[i] == adecResId)
            {
                if(newStatus == HAL_AUDIO_RESOURCE_RUN)
                    strcat(AUD_StringBuffer, "(Stop  to  Run)");
                else if(newStatus == HAL_AUDIO_RESOURCE_STOP)
                    strcat(AUD_StringBuffer, "(Stop  to  Stop)");
                else if(newStatus == HAL_AUDIO_RESOURCE_PAUSE)
                    strcat(AUD_StringBuffer, "(Stop  to  Pause)");
                else
                    strcat(AUD_StringBuffer, "(Stop    State)");
            }
            else
                strcat(AUD_StringBuffer, "(Stop    State)");

            if(GetDectUserState(i) == 1)
                strcat(AUD_StringBuffer, "(WebOS setting is Run)");
        }
        FillConnectedOutput(decId[i]);
        if(forcePrint)
            pr_err("%s\n", AUD_StringBuffer);
        else
            AUDIO_INFO("%s\n", AUD_StringBuffer);
    }
}
#endif

static BOOLEAN IsDecoderConnectedToEncoder(HAL_AUDIO_ADEC_INDEX_T adecIndex)
{
    HAL_AUDIO_RESOURCE_T curResourceId;
    HAL_AUDIO_RESOURCE_T outputResId[HAL_DEC_MAX_OUTPUT_NUM];
    HAL_AUDIO_AENC_INDEX_T curEncoderIndex = HAL_AUDIO_AENC0;
    int i;

    int num_of_outputs;

    curResourceId = adec2res(adecIndex);

    /* Check Output source and Error handling */
    GetConnectOutputSource(curResourceId, outputResId, HAL_DEC_MAX_OUTPUT_NUM, &num_of_outputs);

    if(num_of_outputs > HAL_DEC_MAX_OUTPUT_NUM)
    {
        AUDIO_ERROR("[AUDH][Error] decode output connect error %d\n", num_of_outputs);
        return FALSE;
    }

    for(i = 0; i < num_of_outputs; i++)
    {
        if(IsEncoderSource(outputResId[i]))
        {
            curEncoderIndex = res2aenc(outputResId[i]);
            AUDIO_DEBUG("%s:%d DecoderConnectedToEncoder (AENC%d)\n", __FUNCTION__,__LINE__, (int)curEncoderIndex);
            return TRUE;
        }
    }
    return FALSE;
}

BOOL IsAdecOpen(HAL_AUDIO_ADEC_INDEX_T adecIndex)
{
    int i = 0;
    HAL_AUDIO_RESOURCE_T id = adec2res(adecIndex);

    // check all status is correct
    for(i = 0; i < HAL_MAX_OUTPUT; i++)
    {
        if((ResourceStatus[id].connectStatus[i] == HAL_AUDIO_RESOURCE_CLOSE)||
            (ResourceStatus[id].connectStatus[i] == HAL_AUDIO_RESOURCE_STATUS_UNKNOWN))
        {
            AUDIO_DEBUG("status is at %s \n",
                    GetResourceStatusString(ResourceStatus[id].connectStatus[0]));
            return FALSE;
        }
    }
    return TRUE;
}

BOOL IsAdecAvailable(HAL_AUDIO_ADEC_INDEX_T adecIndex)
{
    int i = 0;
    HAL_AUDIO_RESOURCE_T id = adec2res(adecIndex);

    // check all status is correct
    for(i = 0; i < HAL_MAX_OUTPUT; i++)
    {
        if((ResourceStatus[id].connectStatus[i] == HAL_AUDIO_RESOURCE_OPEN)||
            (ResourceStatus[id].connectStatus[i] == HAL_AUDIO_RESOURCE_CONNECT)||
            (ResourceStatus[id].connectStatus[i] == HAL_AUDIO_RESOURCE_RUN))
        {
            AUDIO_DEBUG("status is at %s, not available\n",
                    GetResourceStatusString(ResourceStatus[id].connectStatus[0]));
            return FALSE;
        }
    }
    return TRUE;
}

static DTV_STATUS_T RHAL_AUDIO_ResumeDecoding(HAL_AUDIO_ADEC_INDEX_T adecIndex, HAL_AUDIO_SRC_TYPE_T audioType)
{
    int ret;
    HAL_AUDIO_RESOURCE_T curResourceId = adec2res(adecIndex);
    HAL_AUDIO_RESOURCE_T atp_res = ADEC_GetConnectedATP(adecIndex);
    int atpindex = atp_res - HAL_AUDIO_RESOURCE_ATP0;

    AUDIO_INFO("%s ADEC%d type %s\n", __FUNCTION__, adecIndex, GetSRCTypeName(audioType));

    if(!IsValidAdecIdx(adecIndex))
        return NOT_OK;

    if(!AUDIO_HAL_CHECK_ISATPAUSESTATE(ResourceStatus[curResourceId], 0))
    {
        AUDIO_ERROR("%s status is at %s \n", GetResourceString(curResourceId),
                GetResourceStatusString(ResourceStatus[curResourceId].connectStatus[0]));
        return NOT_OK;
    }

    ret = rdvb_dmx_ctrl_privateInfo(DMX_PRIV_CMD_AUDIO_UNPAUSE, PIN_TYPE_ATP, atpindex, 0);
    if (ret) {
        AUDIO_ERROR("[%s:%d] dmx privateInfo atp %d ret %d\n", __FUNCTION__, __LINE__, atpindex, ret);
    }

    ShowFlow(curResourceId, HAL_AUDIO_RESOURCE_RUN, 0);

    Aud_flow[adecIndex]->Run(Aud_flow[adecIndex]);

    ResourceStatus[curResourceId].connectStatus[0] = HAL_AUDIO_RESOURCE_RUN;

    return OK;
}

static DTV_STATUS_T RHAL_AUDIO_PauseDecoding(HAL_AUDIO_ADEC_INDEX_T adecIndex)
{
    HAL_AUDIO_RESOURCE_T curResourceId = adec2res(adecIndex);

    AUDIO_INFO("%s ADEC%d\n", __FUNCTION__, adecIndex);

    if(!IsValidAdecIdx(adecIndex))
        return NOT_OK;

    if(AUDIO_HAL_CHECK_ISATPAUSESTATE(ResourceStatus[curResourceId], 0))
    {
        return OK;
    }

    if(!AUDIO_HAL_CHECK_ISATRUNSTATE(ResourceStatus[curResourceId], 0))
    {
        AUDIO_ERROR("%s status is at %s \n", GetResourceString(curResourceId),
                GetResourceStatusString(ResourceStatus[curResourceId].connectStatus[0]));
        return NOT_OK;
    }

    ShowFlow(adec2res(adecIndex), HAL_AUDIO_RESOURCE_PAUSE, 0);

    Aud_flow[adecIndex]->Pause(Aud_flow[adecIndex]);

    ResourceStatus[curResourceId].connectStatus[0] = HAL_AUDIO_RESOURCE_PAUSE;

    return OK;
}

DTV_STATUS_T RHAL_AUDIO_StartDecoding(HAL_AUDIO_ADEC_INDEX_T adecIndex, HAL_AUDIO_SRC_TYPE_T audioType, int force2reStart, int autostart)
{
    HAL_AUDIO_RESOURCE_T curResourceId, decIptResId;
    /*HAL_AUDIO_RESOURCE_T dec0IptResId, dec1IptResId;*/
    HAL_AUDIO_FLOW_STATUS FlowStatus_backup;
    HAL_AUDIO_FLOW_STATUS* pCurFlowStatus = NULL;
    Base* pAin;
    Base* pPPAo;
    FlowManager* pFlow;
    int i;
    HAL_AUDIO_RESOURCE_T outputResId[HAL_DEC_MAX_OUTPUT_NUM];
    int num_of_outputs;
    int output_enc_resID = -1;
    unsigned int exist_module = 0;
    DEC* pDEC = NULL;

#if defined(VIRTUAL_ADEC_ENABLED)
#define MAX_MODULE 14
#else
#define MAX_MODULE 10
/*#define MAX_MODULE 8 // test for SEETV without ENC*/
#endif
    Base *pModule[]={
        Aud_MainAin,
        Aud_SubAin,
        Aud_ESPlay[0],
        Aud_ESPlay[1],
        Aud_dec[0],
        Aud_dec[1],
        Aud_aenc[0]->pEnc,
        Aud_aenc[0]->pMemOut,
        Aud_ppAout,
        Aud_subPPAout
#if defined(VIRTUAL_ADEC_ENABLED)
       ,Aud_ESPlay[2]
       ,Aud_ESPlay[3]
       ,Aud_dec[2]
       ,Aud_dec[3]
#endif
    };

    AUDIO_ERROR("%s ADEC%d type %s\n", __FUNCTION__, adecIndex, GetSRCTypeName(audioType));
    curResourceId = adec2res(adecIndex);

    ipt_Is_HDMI = FALSE;
#ifdef START_DECODING_WHEN_GET_FORMAT
    At_Stop_To_Start_Decode = FALSE;
#endif

    if(!IsValidAdecIdx(adecIndex))
        return NOT_OK;

    if(GetTrickState(adecIndex) == HAL_AUDIO_TRICK_NORMAL_PLAY && AUDIO_HAL_CHECK_ISATPAUSESTATE(ResourceStatus[curResourceId], 0))
    {
        char ret = 0;
        HAL_AUDIO_RESOURCE_T atp_res = ADEC_GetConnectedATP(adecIndex);
        KADP_AUDIO_GetTrickState(atp_res, &ret);
        if(ret)
        {
            // for KTASKWBS-8067, need to stop flow first
            AUDIO_INFO("%s-%s resume from trick state, re-start flow \n", GetResourceString(atp_res), GetResourceString(curResourceId));
            RHAL_AUDIO_StopDecoding(adecIndex, 0);
        }
        else
        {
            AUDIO_INFO("start change to resume API \n");
            return RHAL_AUDIO_ResumeDecoding(adecIndex, audioType);
        }
    }

    ShowFlow(adec2res(adecIndex), HAL_AUDIO_RESOURCE_RUN, 0);

    // open ADEC ready check
    if(AUDIO_HAL_CHECK_ISATRUNSTATE(ResourceStatus[curResourceId], 0))
    {
        if((force2reStart == 0) && audioType == GetUserFormat(adecIndex))
        {
            AUDIO_INFO("ADEC %d is at run state and format %d is same \n", adecIndex, audioType);
#ifdef START_DECODING_WHEN_GET_FORMAT
            decIptResId = GetNonOutputModuleSingleInputResource(ResourceStatus[curResourceId]);// dec input source
            if(IsHDMISource(decIptResId))
            {
                ADSP_DEC_SetHdmiFmt(adecIndex, SRCType_to_ADECType(audioType));
            }
#endif
            return NOT_OK;
        }
        else
        {
            AUDIO_INFO("ADEC %d is at run state and format change(%d->%d) , need auto stop \n", adecIndex, GetUserFormat(adecIndex), audioType);
            RHAL_AUDIO_StopDecoding(adecIndex, 0);
        }
    }

    if(AUDIO_HAL_CHECK_PLAY_NOTAVAILABLE(ResourceStatus[curResourceId], 0))
    {
        AUDIO_ERROR("%s status is at %s \n", GetResourceString(curResourceId),
                GetResourceStatusString(ResourceStatus[curResourceId].connectStatus[0]));
        return NOT_OK;
    }

    if((m_IsSetMainDecOptByWebOs == FALSE && isAutoSetMainAdec == FALSE) || !IsValidAdecIdx((HAL_AUDIO_ADEC_INDEX_T)Aud_mainDecIndex))
    {
        isAutoSetMainAdec = TRUE;
        Aud_mainDecIndex = adecIndex;
        AUDIO_INFO("%s:%d MainDecOutput is not set. Now, set auto, adecIndex=%d,Aud_mainDecIndex=%d\n",__FUNCTION__,__LINE__, adecIndex, Aud_mainDecIndex);
    }

    pFlow = Aud_flow[adecIndex];
    pCurFlowStatus = &flowStatus[adecIndex];

    if(adecIndex == Aud_mainDecIndex)
    {
        pAin  = Aud_MainAin;
        pPPAo = Aud_ppAout;
    }
    else
    {
        pAin  = Aud_SubAin;
        pPPAo = Aud_subPPAout;
    }

    for(i = 0; i < AUD_ADEC_MAX; i++) {
        AUDIO_DEBUG("before Start Decoding flow DEC%d (%s):: \n", i, (i == Aud_mainAudioIndex? "main" : "sub"));
        Aud_flow[i]->ShowCurrentExitModule(Aud_flow[i], 0);
    }

    memcpy(&FlowStatus_backup, pCurFlowStatus, sizeof(HAL_AUDIO_FLOW_STATUS));
    pFlow->Stop(pFlow);
    RemoveFilter(pCurFlowStatus, pFlow);

    exist_module = pFlow->ShowCurrentExitModule(pFlow, 0);
    if(exist_module != 0){
        ERROR("[%s] module exist in flow %x!\n",__FUNCTION__,exist_module);
        ERROR("module %d/%d/%d/%d\n",FlowStatus_backup.IsAINExist,FlowStatus_backup.IsSubAINExist,FlowStatus_backup.IsSystemOutput0,FlowStatus_backup.IsSystemOutput1);
#if defined(VIRTUAL_ADEC_ENABLED)
        ERROR("module %d/%d/%d/%d\n",FlowStatus_backup.IsSystemOutput2,FlowStatus_backup.IsSystemOutput3,FlowStatus_backup.IsDEC2Exist,FlowStatus_backup.IsDEC3Exist);
#endif
        ERROR("module %d/%d/%d/%d\n",FlowStatus_backup.IsDTV0SourceRead,FlowStatus_backup.IsDTV1SourceRead,FlowStatus_backup.IsDEC0Exist,FlowStatus_backup.IsDEC1Exist);
        ERROR("module %d/%d/%d\n",FlowStatus_backup.IsEncExist,FlowStatus_backup.IsMainPPAOExist,FlowStatus_backup.IsSubPPAOExist);
        for(i=0; i<MAX_MODULE; i++){
            if(pFlow->CheckExistModule(pFlow, pModule[i]))
                pFlow->Remove(pFlow, pModule[i]);
        }
    }

    if(GetSysMemoryExist(adecIndex) == FALSE)
    {
        //free Aud_sysmem[adec]
        if(Aud_ESPlay[adecIndex] != NULL)
        {
            Aud_ESPlay[adecIndex]->Stop(Aud_ESPlay[adecIndex]);
            Aud_ESPlay[adecIndex]->Delete(Aud_ESPlay[adecIndex]);
            Aud_ESPlay[adecIndex] = NULL;
        }
    }

    /* Check Input source and Error handling */
    decIptResId = GetNonOutputModuleSingleInputResource(ResourceStatus[curResourceId]);// dec input source

    if(IsAinSource(decIptResId) || IsDTVSource(decIptResId) || IsSystemSource(decIptResId))
    {
        AUDIO_DEBUG("dec input is %s\n", GetResourceString(decIptResId));
    }
    else
    {
        AUDIO_ERROR("[AUDH][Error] Invalid dec input src %s\n", GetResourceString(decIptResId));
        goto error;
    }
    /* End of Input source checking */

    /* Check Output source and Error handling */
    GetConnectOutputSource(curResourceId, outputResId, HAL_DEC_MAX_OUTPUT_NUM, &num_of_outputs);
    if(num_of_outputs > HAL_DEC_MAX_OUTPUT_NUM)
    {
        AUDIO_ERROR("[AUDH][Error] decode output connect error %d\n", num_of_outputs);
        goto error;
    }

    for(i = 0; i < num_of_outputs; i++)
    {
        if(IsEncoderSource(outputResId[i]))
            output_enc_resID = outputResId[i];
    }
    /* End of Output source checking */

    /* Connect to Input Source */
    pDEC = (DEC*)Aud_dec[adecIndex]->pDerivedObj;

    if(IsAinSource(decIptResId) == TRUE)
    {
        // analog flow
        AddAndConnectDecAinToFlow(pFlow, adecIndex, pAin, pCurFlowStatus);

        if(IsADCSource(decIptResId) && SwitchADCFocus(pAin) != TRUE)
        {
            goto error;
        }
        else if(IsHDMISource(decIptResId))
        {
            HDMI_fmt_change_transition = FALSE;
            if(SwitchHDMIFocus(pAin) != TRUE)
                goto error;

            ADSP_DEC_SetHdmiFmt(adecIndex, SRCType_to_ADECType(audioType));
        }
        else if(IsATVSource(decIptResId))
        {
            SwitchATVFocus(pAin);

            if((adecIndex == Aud_mainDecIndex)) {
                Audio_SIF_SetAudioInHandle(pAin->GetAgentID(pAin));
            }
            else {
                Audio_SIF_SetSubAudioInHandle(pAin->GetAgentID(pAin));// sub ain
            }
        }
    }
    else if(IsDTVSource(decIptResId))
    {
        int atpindex = decIptResId - HAL_AUDIO_RESOURCE_ATP0;
        DtvCom* pDtvCom = (DtvCom*)Aud_DTV[atpindex]->pDerivedObj;
        AUDIO_DEBUG("DTV flow \n");

        AddAndConnectDTVSourceFilterDecToFlow(pFlow, adecIndex, Aud_DTV[atpindex], pCurFlowStatus);

        pFlow->SetExtRefClock(pFlow, pDtvCom->GetRefClockPhyAddress(Aud_DTV[atpindex]));
        pDEC->InitBSRingBuf(Aud_dec[adecIndex], pDtvCom->GetBSRingBufPhyAddress(Aud_DTV[atpindex]));
        pDEC->InitICQRingBuf(Aud_dec[adecIndex], pDtvCom->GetICQRingBufPhyAddress(Aud_DTV[atpindex]));
    }
    else if(IsSystemSource(decIptResId))
    {
        ESPlay* pESPlay = (ESPlay*)Aud_ESPlay[adecIndex]->pDerivedObj;
        AUDIO_DEBUG("Do system Clip flow, EsPushMode%d \n", GetEsMode(adecIndex));

        AddAndConnectSourceFilterDecToFlow(pFlow, adecIndex, Aud_ESPlay[adecIndex], pCurFlowStatus);
        pDEC->InitBSRingBuf(Aud_dec[adecIndex], pESPlay->GetBSHeaderPhy(Aud_ESPlay[adecIndex]));
    }

    Aud_dec[adecIndex]->InitOutRingBuf(Aud_dec[adecIndex]);
    pDEC->InitDwnStrmQueue(Aud_dec[adecIndex]);

    /* Connect to Output source */
    if(output_enc_resID != -1)
    {
        /* Connect to encoder */
        HAL_AUDIO_AENC_INDEX_T aencIndex = res2aenc((HAL_AUDIO_RESOURCE_T)output_enc_resID);
        AddAndConnectEncFilterToFlow(pFlow, adecIndex, aencIndex, pCurFlowStatus);
    }
    else
    {
        /* Connect to PPAO */
        AddAndConnectPPAOToFlow(pFlow, adecIndex, pPPAo, pCurFlowStatus);

        if((adecIndex == Aud_mainDecIndex))
        {
            AUDIO_DEBUG("focus \n");
            pPPAo->SwitchFocus(pPPAo, NULL);
            //wait AO finish swithing focus
            msleep(20);
        }
        else if(m_IsHbbTV)
        {
            AUDIO_INFO("focus for HbbTV \n");
            pPPAo->SwitchFocus(pPPAo, NULL);
            m_IsHbbTV = FALSE;
            //wait AO finish swithing focus
            msleep(100);
            SNDOut_SetMute(ENUM_DEVICE_SPEAKER, ADEC_SPK_CheckConnect(), g_AudioStatusInfo.curSPKMuteStatus);
            SNDOut_SetMute(ENUM_DEVICE_HEADPHONE, ADEC_SPK_CheckConnect(), g_AudioStatusInfo.curSPKMuteStatus);
            SNDOut_SetMute(ENUM_DEVICE_SCART, ADEC_SPK_CheckConnect(), g_AudioStatusInfo.curSPKMuteStatus);
            SNDOut_SetMute(ENUM_DEVICE_SPDIF, ADEC_SPK_CheckConnect(), g_AudioStatusInfo.curSPKMuteStatus);
            SNDOut_SetMute(ENUM_DEVICE_SPDIF_ES, ADEC_SPK_CheckConnect(), g_AudioStatusInfo.curSPKMuteStatus);
        }
    }

    // update adec info status
    adec_info[adecIndex].bAdecStart     = TRUE;
    adec_status[adecIndex].Start        = TRUE;

    /* update resource status */
    ResourceStatus[curResourceId].connectStatus[0] = HAL_AUDIO_RESOURCE_RUN;

    SetTrickState(adecIndex, HAL_AUDIO_TRICK_NONE);

    {
        int atpindex = decIptResId - HAL_AUDIO_RESOURCE_ATP0;
        DEC* pDEC = (DEC*)Aud_dec[adecIndex]->pDerivedObj;
        uint8_t isPVR;
        int ret;
        isPVR = 0;
        ret = rdvb_dmx_ctrl_currentSrc_isMTP(PIN_TYPE_ATP, atpindex, &isPVR);
        /*AUDIO_INFO("%d, isPVR:%d, autostart:%d, atpindex:%d\n", ret, isPVR, autostart, atpindex);*/
        if ((ret == 0) && isPVR && autostart)
        {
            AUDIO_INFO("Run into PVR autoswitch case\n");
            pFlow->SetSkipDECFlush(pFlow, TRUE);
            pDEC->SetSkipFlushMode(Aud_dec[adecIndex], TRUE);
        }
    }

    pFlow->Flush(pFlow);
    pFlow->SetSkipDECFlush(pFlow, FALSE);

    // Recovery
    pFlow->Run(pFlow);

    if(IsDTVSource(decIptResId))
    {
        int newfmt[8] = {0};
        int ret;
        int atpindex = decIptResId - HAL_AUDIO_RESOURCE_ATP0;
        struct dmx_priv_data priv_audio_data;

        priv_audio_data.data.audio_format.audio_format = SRCType_to_ADECType(audioType);
        if(priv_audio_data.data.audio_format.audio_format == AUDIO_UNKNOWN_TYPE)
            return NOT_OK;

        newfmt[6] = 1 << 1; // DTV mode

        memcpy(priv_audio_data.data.audio_format.private_data, newfmt, sizeof(newfmt));
        ret = rdvb_dmx_ctrl_privateInfo(DMX_PRIV_CMD_AUDIO_FORMAT, PIN_TYPE_ATP, atpindex, &priv_audio_data);
        if (ret) {
            AUDIO_DEBUG("[%s:%d] dmx privateInfo atp %d ret %d\n", __FUNCTION__, __LINE__, atpindex, ret);
        }
    }

    if(GetEsMode(adecIndex) == HAL_AUDIO_ES_ARIB_UHD)
    {
        AUDIO_FORMAT a_format;
        AUDIO_DEBUG("Do system Clip flow privateinfo\n");

        memset(&a_format, 0, sizeof(AUDIO_FORMAT));
        InitAudioFormat(audioType, &a_format);
        Aud_dec[adecIndex]->PrivateInfo(Aud_dec[adecIndex],INFO_AUDIO_FORMAT, (BYTE*)&a_format, sizeof(a_format));
    }
    else if(GetEsMode(adecIndex) == HAL_AUDIO_ES_PLAY_ALSA)
    {
        AUDIO_FORMAT a_format;
        ESPlay* pESPlay = (ESPlay*)Aud_ESPlay[adecIndex]->pDerivedObj;
        AUDIO_DEBUG("Send ESPlay newformat\n");

        memset(&a_format, 0, sizeof(AUDIO_FORMAT));
        InitAudioFormat(audioType, &a_format);
        a_format.wptr = pESPlay->GetBSWptr(Aud_ESPlay[adecIndex]);
        if(audioType == HAL_AUDIO_SRC_TYPE_PCM) {
            int fs, nch, bitPerSample;
            pESPlay->GetPCMFormat(Aud_ESPlay[adecIndex],&fs,&nch,&bitPerSample);
            a_format.numberOfChannels = nch;
            a_format.bitsPerSample = bitPerSample;
            a_format.samplingRate = fs;
        }
        Aud_dec[adecIndex]->PrivateInfo(Aud_dec[adecIndex],INFO_AUDIO_FORMAT, (BYTE*)&a_format, sizeof(a_format));
    }

    for(i = 0; i < AUD_ADEC_MAX; i++) {
        AUDIO_DEBUG("after Start Decoding flow DEC%d (%s):: \n", i, (i == Aud_mainAudioIndex? "main" : "sub"));
        Aud_flow[i]->ShowCurrentExitModule(Aud_flow[i], 0);
    }

    AUDIO_ERROR("%s ADEC%d type %s success\n", __FUNCTION__, adecIndex, GetSRCTypeName(audioType));
    return OK;

error:
    /* update modified flow status */

    return NOT_OK;
}

DTV_STATUS_T RHAL_AUDIO_StopDecoding(HAL_AUDIO_ADEC_INDEX_T adecIndex, int autostop)
{
    HAL_AUDIO_FLOW_STATUS* pCurFlowStatus;
    FlowManager* pFlow;
    HAL_AUDIO_RESOURCE_T curResourceId;
    HAL_AUDIO_RESOURCE_T decIptResId;
    int i;

    AUDIO_INFO("%s ADEC%d\n", __FUNCTION__, adecIndex);

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(!IsValidAdecIdx(adecIndex))
        return NOT_OK;

    if(GetTrickState(adecIndex) == HAL_AUDIO_TRICK_PAUSE)
    {
        AUDIO_INFO("Stop change to Pause API \n");
        return RHAL_AUDIO_PauseDecoding(adecIndex);
    }

    for(i = 0; i < AUD_ADEC_MAX; i++) {
        AUDIO_DEBUG("before Start Decoding flow DEC%d (%s):: \n", i, (i == Aud_mainAudioIndex? "main" : "sub"));
        Aud_flow[i]->ShowCurrentExitModule(Aud_flow[i], 0);
    }

    curResourceId = adec2res(adecIndex);

    if(AUDIO_HAL_CHECK_ISATSTOPSTATE(ResourceStatus[curResourceId], 0))
    {
        AUDIO_ERROR("%s ADEC%d finish, already in stop state\n", __FUNCTION__, adecIndex);
        return NOT_OK;
    }

    if(AUDIO_HAL_CHECK_STOP_NOTAVAILABLE(ResourceStatus[curResourceId], 0)) // decoder only has one input
    {
        AUDIO_ERROR("%s ADEC%d finish, %s still at %s status\n", __FUNCTION__, adecIndex,
                GetResourceString(curResourceId), GetResourceStatusString(ResourceStatus[curResourceId].connectStatus[0]));
        return NOT_OK;
    }

    if(AUDIO_HAL_CHECK_ISATPAUSESTATE(ResourceStatus[curResourceId], 0)) // notify SDEC UNPAUSE state
    {
        int ret;
        HAL_AUDIO_RESOURCE_T atp_res = ADEC_GetConnectedATP(adecIndex);
        int atpindex = atp_res - HAL_AUDIO_RESOURCE_ATP0;

        ret = rdvb_dmx_ctrl_privateInfo(DMX_PRIV_CMD_AUDIO_UNPAUSE, PIN_TYPE_ATP, atpindex, 0);
        if (ret) {
            AUDIO_ERROR("[%s:%d] dmx privateInfo atp %d ret %d\n", __FUNCTION__, __LINE__, atpindex, ret);
        }
    }

    ShowFlow(adec2res(adecIndex), HAL_AUDIO_RESOURCE_STOP, 0); // HAL_AUDIO_RESOURCE_NO_CONNECTION for show current state

    ResourceStatus[curResourceId].connectStatus[0] = HAL_AUDIO_RESOURCE_STOP;

    // update adec status info
    adec_info[adecIndex].bAdecStart = FALSE;
    adec_status[adecIndex].Start    = FALSE;

    decIptResId = GetNonOutputModuleSingleInputResource(ResourceStatus[curResourceId]);// dec input source

    pFlow = Aud_flow[adecIndex];
    pCurFlowStatus = &flowStatus[adecIndex];

    {
        int atpindex = decIptResId - HAL_AUDIO_RESOURCE_ATP0;
        DEC* pDEC = (DEC*)Aud_dec[adecIndex]->pDerivedObj;
        uint8_t isPVR;
        int ret;
        isPVR = 0;
        ret = rdvb_dmx_ctrl_currentSrc_isMTP(PIN_TYPE_ATP, atpindex, &isPVR);
        if ((ret == 0) && isPVR && autostop)
        {
            AUDIO_INFO("Run into PVR stop autoswitch case\n");
            pDEC->SetSkipFlushMode(Aud_dec[adecIndex], TRUE);
        }
    }

    if(adecIndex == Aud_mainDecIndex)
    {
        pFlow->Stop(pFlow);

        if(IsATVSource(decIptResId))
        {
            Audio_SIF_SetAudioInHandle(0);
        }

        if(IsAinSource(decIptResId))
        {
            AUDIO_IPT_SRC audioInput = {0};

            audioInput.focus[0] = AUDIO_IPT_SRC_UNKNOWN;
            audioInput.focus[1] = AUDIO_IPT_SRC_UNKNOWN;
            audioInput.focus[2] = AUDIO_IPT_SRC_UNKNOWN;
            audioInput.focus[3] = AUDIO_IPT_SRC_UNKNOWN;
            Aud_MainAin->SwitchFocus(Aud_MainAin, &audioInput); // unfocus
        }
    }
    else
    {
        pFlow->Stop(pFlow);

        if(IsATVSource(decIptResId))
        {
            Audio_SIF_SetSubAudioInHandle(0);
        }

        if(IsAinSource(decIptResId))
        {
            AUDIO_IPT_SRC audioInput = {0};

            audioInput.focus[0] = AUDIO_IPT_SRC_UNKNOWN;
            audioInput.focus[1] = AUDIO_IPT_SRC_UNKNOWN;
            audioInput.focus[2] = AUDIO_IPT_SRC_UNKNOWN;
            audioInput.focus[3] = AUDIO_IPT_SRC_UNKNOWN;
            Aud_SubAin->SwitchFocus(Aud_SubAin, &audioInput); // unfocus
        }
    }

    if(IsDTVSource(decIptResId))
    {
        KADP_AUDIO_ResetTrickState(ADEC_GetConnectedATP(adecIndex));
    }

    if(IsHDMISource(decIptResId))
        HDMI_fmt_change_transition = TRUE;

    RemoveFilter(pCurFlowStatus, pFlow);

    for(i = 0; i < AUD_ADEC_MAX; i++) {
        AUDIO_DEBUG("after Start Decoding flow DEC%d (%s):: \n", i, (i == Aud_mainAudioIndex? "main" : "sub"));
        Aud_flow[i]->ShowCurrentExitModule(Aud_flow[i], 0);
    }

    AUDIO_INFO("%s ADEC%d finished\n", __FUNCTION__, adecIndex);
    return OK;
}

DTV_STATUS_T HAL_AUDIO_StopDecoding(HAL_AUDIO_ADEC_INDEX_T adecIndex)
{
    DTV_STATUS_T status;

    AUDIO_INFO("%s ADEC%d\n", __FUNCTION__, adecIndex);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

#ifdef START_DECODING_WHEN_GET_FORMAT
    if((ipt_Is_HDMI == TRUE) && (At_Stop_To_Start_Decode == TRUE) && (g_AudioStatusInfo.curSPKMuteStatus == FALSE))
    {
        if(!IsValidAdecIdx(adecIndex))
            return NOT_OK;
        AUDIO_INFO("Skip stop isHdmiInput %d audioStop2Start%d spk mute %d \n", ipt_Is_HDMI, At_Stop_To_Start_Decode, g_AudioStatusInfo.curSPKMuteStatus);
        SetDecUserState(adecIndex, 0);
        return OK;
    }
#endif
    status = RHAL_AUDIO_StopDecoding(adecIndex, 0);
    if(status == OK)
        SetDecUserState(adecIndex, FALSE);
    return status;
}

int KHAL_AUDIO_TrickStopDecoding(int index)
{
    DTV_STATUS_T status;
    HAL_AUDIO_ADEC_INDEX_T adecIndex = (HAL_AUDIO_ADEC_INDEX_T)index;

    AUDIO_INFO("%s ADEC%d\n", __FUNCTION__, adecIndex);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    status = RHAL_AUDIO_StopDecoding(adecIndex, 0);
    if(status == OK)
        SetDecUserState(adecIndex, FALSE);
    else
        AUDIO_ERROR("%s call RHAL_AUDIO_StopDecoding fail\n", __FUNCTION__);

    return (int)status;
}
EXPORT_SYMBOL(KHAL_AUDIO_TrickStopDecoding);

DTV_STATUS_T HAL_AUDIO_ADEC_CODEC(int adec_port, adec_src_codec_ext_type_t codec_type)
{
	int i;
	//adec is not opened and
	//is not connected to input
	for(i = 0; i < HAL_MAX_OUTPUT; i++){
		if(!((ResourceStatus[adec2res(adec_port)].connectStatus[i] == HAL_AUDIO_RESOURCE_OPEN)||
				(ResourceStatus[adec2res(adec_port)].connectStatus[i] == HAL_AUDIO_RESOURCE_CONNECT)||
				(ResourceStatus[adec2res(adec_port)].connectStatus[i] == HAL_AUDIO_RESOURCE_PAUSE)||
				(ResourceStatus[adec2res(adec_port)].connectStatus[i] == HAL_AUDIO_RESOURCE_STOP)))
		{
			AUDIO_ERROR("[%s] ADEC%d %s! input source %s\n",__FUNCTION__, adec_port,
					GetResourceStatusString(ResourceStatus[adec2res(adec_port)].connectStatus[i]),
					GetResourceString(ResourceStatus[adec2res(adec_port)].inputConnectResourceId[0]));
			if(((ResourceStatus[adec2res(adec_port)].connectStatus[i] == HAL_AUDIO_RESOURCE_RUN))&&
				((ResourceStatus[adec2res(adec_port)].inputConnectResourceId[0] == HAL_AUDIO_RESOURCE_HDMI)||
				(ResourceStatus[adec2res(adec_port)].inputConnectResourceId[0] == HAL_AUDIO_RESOURCE_HDMI0)||
				(ResourceStatus[adec2res(adec_port)].inputConnectResourceId[0] == HAL_AUDIO_RESOURCE_HDMI1)||
				(ResourceStatus[adec2res(adec_port)].inputConnectResourceId[0] == HAL_AUDIO_RESOURCE_HDMI2)||
				(ResourceStatus[adec2res(adec_port)].inputConnectResourceId[0] == HAL_AUDIO_RESOURCE_HDMI3))){
				return OK;
			}else
				return NOT_OK;
		}
	}

	if (codec_type_alsa2hal(codec_type) == HAL_AUDIO_SRC_TYPE_UNKNOWN){  //wrong codec type
		AUDIO_ERROR("Wrong codec type %d!\n",codec_type);
		return NOT_OK;
	}
	adec_info[adec_port].userAdecFormat = codec_type_alsa2hal(codec_type);

	return OK;
}

DTV_STATUS_T HAL_AUDIO_StartDecoding(HAL_AUDIO_ADEC_INDEX_T adecIndex, adec_src_codec_ext_type_t audioType)
{
    DTV_STATUS_T status;

    AUDIO_DEBUG("%s ADEC%d type %d\n", __FUNCTION__, adecIndex, codec_type_alsa2hal(audioType));
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if (codec_type_alsa2hal(audioType) == HAL_AUDIO_SRC_TYPE_UNKNOWN){  //wrong codec type
        AUDIO_ERROR("Wrong codec type!\n");
        return NOT_OK;
    }

    status = RHAL_AUDIO_StartDecoding(adecIndex, codec_type_alsa2hal(audioType), 0, 0);
    if(status == OK)
        SetDecUserState(adecIndex, TRUE);
    else
        SetDecUserState(adecIndex, FALSE);

    return status ;
}

DTV_STATUS_T HAL_AUDIO_DIRECT_StartDecoding(HAL_AUDIO_ADEC_INDEX_T adecIndex, HAL_AUDIO_SRC_TYPE_T audioType)
{
    DTV_STATUS_T status;

    AUDIO_INFO("%s ADEC%d type %d\n", __FUNCTION__, adecIndex, audioType);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    status = RHAL_AUDIO_StartDecoding(adecIndex, audioType, 0, 0);
    if(status == OK)
        SetDecUserState(adecIndex, TRUE);
    else
        SetDecUserState(adecIndex, FALSE);

    return status ;
}

// add swap Peter
DTV_STATUS_T RHAL_AUDIO_SetMainDecoderOutput(HAL_AUDIO_ADEC_INDEX_T adecIndex)
{
    int isNeedAutoStart = 0;
    int isNeedSubAutoStart = 0;
    int isNeedADAutoStart = 0;
    HAL_AUDIO_RESOURCE_T id;
    AUDIO_INFO("%s to ADEC%d\n", __FUNCTION__, adecIndex);

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE){
        return NOT_OK;
    }

    if(!IsValidAdecIdx(adecIndex)){
        return NOT_OK;
    }

    if((int)adecIndex != Aud_mainDecIndex)
    {
        if((Aud_mainDecIndex != -1))
        {
            id = adec2res((HAL_AUDIO_ADEC_INDEX_T)Aud_mainDecIndex);
            // The dec might be started by other process, so check here. We'll do auto re-start if it is own by this process
            if(IsResourceRunningByProcess((HAL_AUDIO_ADEC_INDEX_T)Aud_mainDecIndex, ResourceStatus[id]))
            {
                if((ResourceStatus[id].connectStatus[0] != HAL_AUDIO_RESOURCE_RUN))
                {
                    AUDIO_ERROR("%s: main %d dismatch, status is %s but Flow is not Empty\n", __FUNCTION__, Aud_mainDecIndex, GetResourceStatusString(ResourceStatus[id].connectStatus[0]));
                }

                if(ResourceStatus[id].connectStatus[0] == HAL_AUDIO_RESOURCE_RUN)
                    isNeedSubAutoStart = 1; // no sub dec type , so need to check sub decoder is at run state
                if(IsHbbTV((HAL_AUDIO_ADEC_INDEX_T)adecIndex, (HAL_AUDIO_ADEC_INDEX_T)Aud_mainDecIndex))
                {
                    // mute SPK before focus change to avoid pop sound
                    ADSP_SNDOut_SetMute(ENUM_DEVICE_SPEAKER, TRUE);
                    ADSP_SNDOut_SetMute(ENUM_DEVICE_HEADPHONE, TRUE);
                    ADSP_SNDOut_SetMute(ENUM_DEVICE_SCART, TRUE);
                    ADSP_SNDOut_SetMute(ENUM_DEVICE_SPDIF, TRUE);
                    ADSP_SNDOut_SetMute(ENUM_DEVICE_SPDIF_ES, TRUE);
                }
                RHAL_AUDIO_StopDecoding((HAL_AUDIO_ADEC_INDEX_T)Aud_mainDecIndex, 0);
            }
        }

        id = adec2res(adecIndex);
        if(IsResourceRunningByProcess((HAL_AUDIO_ADEC_INDEX_T)adecIndex, ResourceStatus[id]))
        {
            if((ResourceStatus[id].connectStatus[0] != HAL_AUDIO_RESOURCE_RUN))
            {
                AUDIO_ERROR("%s: sub %d  dismatch,  status is %s but Flow is not Empty\n", __FUNCTION__,adecIndex,  GetResourceStatusString(ResourceStatus[id].connectStatus[0]));
            }

            isNeedAutoStart = 1;
            // If AD already on, need to reset AD pin.
            if(Aud_descriptionMode == TRUE)
                isNeedADAutoStart = 1;
            RHAL_AUDIO_StopDecoding((HAL_AUDIO_ADEC_INDEX_T)adecIndex, 1);
        }
    }

    id = adec2res(adecIndex);

    Aud_prevMainDecIndex = Aud_mainDecIndex;
    Aud_mainDecIndex     = adecIndex;

    SetSPDIFOutType(adecIndex, HAL_AUDIO_SPDIF_AUTO);

    // need to add auto deceoding
    if(isNeedAutoStart == 1)
    {
        if(AUDIO_HAL_CHECK_PLAY_NOTAVAILABLE(ResourceStatus[id], 0))
        {
            AUDIO_ERROR("dec failed status != stop %d \n", ResourceStatus[id].connectStatus[0]);
            return  NOT_OK;
        }
        RHAL_AUDIO_StartDecoding((HAL_AUDIO_ADEC_INDEX_T)adecIndex, GetUserFormat(adecIndex), 0, 1);
    }
    ADSP_DEC_SetDelay(adecIndex, gAudioDecInfo[adecIndex].decDelay);

    // need to add auto deceoding
    if(isNeedSubAutoStart == 1)
    {
        // If HbbTV, switch focus
        if(IsHbbTV((HAL_AUDIO_ADEC_INDEX_T)Aud_mainDecIndex, (HAL_AUDIO_ADEC_INDEX_T)Aud_prevMainDecIndex))
        {
            m_IsHbbTV = TRUE;
        }
        RHAL_AUDIO_StartDecoding((HAL_AUDIO_ADEC_INDEX_T)Aud_prevMainDecIndex, GetUserFormat(Aud_prevMainDecIndex), 0, 0);
    }
    ADSP_DEC_SetDelay(Aud_prevMainDecIndex, gAudioDecInfo[Aud_prevMainDecIndex].decDelay);

    if(isNeedADAutoStart == 1)
    {
        AUDIO_ERROR("at description mode now, reset AD mode\n");
        RHAL_AUDIO_SetAudioDescriptionMode(Aud_mainDecIndex, TRUE);
    }
    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetMainDecoderOutput(HAL_AUDIO_ADEC_INDEX_T adecIndex)
{
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;
    m_IsSetMainDecOptByWebOs = TRUE;

    /* Inform ADSP about the dec port for SPDIF/ARC output */
    audioConfig.msg_id = AUDIO_CONFIG_CMD_SPDIF_OUTPUT_SWITCH;
    audioConfig.value[0] = adecIndex;
    SendRPC_AudioConfig(&audioConfig);

    return RHAL_AUDIO_SetMainDecoderOutput(adecIndex);
}

DTV_STATUS_T HAL_AUDIO_SetMainAudioOutput(HAL_AUDIO_INDEX_T audioIndex)
{
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;
    HAL_AUDIO_ADEC_INDEX_T adecIndex = HAL_AUDIO_ADEC0;

    if(!IsMainAudioADEC(audioIndex))
    {
        AUDIO_INFO("%s to audioIndex(%d), force SPDIF/ARC output PCM!!!\n", __FUNCTION__,audioIndex);
        return NOT_OK;
    }

    m_IsSetMainDecOptByWebOs = TRUE;
    Aud_mainAudioIndex = audioIndex;

    /* Inform ADSP about the dec port for SPDIF/ARC output */
    audioConfig.msg_id = AUDIO_CONFIG_CMD_SPDIF_OUTPUT_SWITCH;
    audioConfig.value[0] = audioIndex;
    SendRPC_AudioConfig(&audioConfig);

    Update_RawMode_by_connection();

    switch(audioIndex)
    {
        case HAL_AUDIO_INDEX0:
            adecIndex = HAL_AUDIO_ADEC0;
            break;
        case HAL_AUDIO_INDEX1:
            adecIndex = HAL_AUDIO_ADEC1;
            break;
#if defined(VIRTUAL_ADEC_ENABLED)
        case HAL_AUDIO_INDEX2:
            adecIndex = HAL_AUDIO_ADEC2;
            break;
        case HAL_AUDIO_INDEX3:
            adecIndex = HAL_AUDIO_ADEC3;
            break;
#endif
        default:
            break;
    }

    return RHAL_AUDIO_SetMainDecoderOutput(adecIndex);
}

#if defined(SEARCH_PAPB_WHEN_STOP)
ahdmi_type_ext_type_t check_hdmimode_search(void)
{
	audio_hdmi_in_pattern_search1_RBUS  pattern_search1;
	audio_hdmi_in_pattern_search2_RBUS  pattern_search2;
	audio_hdmi_in_papb_search_RBUS      papb_search;
	audio_hdmi_in_papb_val_RBUS			papb_val;
	audio_hdmi_to_audio_lock_RBUS       h2a_reg;

	pattern_search1.regValue = rtd_inl(AUDIO_hdmi_in_pattern_search1_reg);
	pattern_search2.regValue = rtd_inl(AUDIO_hdmi_in_pattern_search2_reg);
	papb_search.regValue     = rtd_inl(AUDIO_hdmi_in_PaPb_search_reg);
	papb_val.regValue        = rtd_inl(AUDIO_hdmi_in_PaPb_val_reg);
	h2a_reg.regValue         = rtd_inl(AUDIO_hdmi_to_audio_lock_reg);

	if(drvif_Hdmi_GetHDMIMode() == 0)  //MODE_DVI
	{
		/*AUDIO_DEBUG("[%s:%d] HDMI MODE :DVI\n", __FUNCTION__,__LINE__);*/
		return AHDMI_DVI;
	}
	else if(pattern_search1.pattern_search_catch1 || pattern_search2.pattern_search_catch2)
	{
		/*AUDIO_INFO("[HDMI-Mode] Pattern 1-%d, 2-%d\n",pattern_search1.pattern_search_catch1,pattern_search2.pattern_search_catch2);*/
		return AHDMI_DTS;
	}
    else if(!h2a_reg.hdmi2aud_lock)
    {
        /*AUDIO_INFO("[HDMI-Mode] NO AUDIO dnum:%d\n", h2a_reg.hdmi2aud_dnum);*/
        return AHDMI_NO_AUDIO;
    }
	else
	{
        /*AUDIO_INFO("[HDMI-Mode] PAPB catch:%d, val:%d, dnum:%d\n", papb_search.papb_search_catch, papb_val.pc_val&0x1F, h2a_reg.hdmi2aud_dnum);*/
		switch(papb_val.pc_val&0x1F)
		{
		case 1:                      // AC3
			return AHDMI_AC3;break;
		case 4:                      // MPEG-1 layer-1 data
		case 5:                      // MPEG-1 layer-2 data or MPEG-2-without extension
		case 6:                      // MPEG-2-witho extension
		case 8:                      // MPEG-2, layer-1 low sampling freq
		case 9:                      // MPEG-2, layer-2 low sampling freq
		case 10:                     // MPEG-2, layer-3 low sampling freq
			return AHDMI_MPEG;break;
		case 7:                      // MPEG-2 AAC
		case 19:                     // MPEG-4 AAC low freq
		case 20:                     // MPEG-4 AAC
			return AHDMI_AAC;break;
		case 11:                     // DTS type I
		case 12:                     // DTS type II
		case 13:                     // DTS type III
		case 17:                     // DTS type IV
			return AHDMI_DTS;break;
		case 21:
			return AHDMI_EAC3;break;
		case 22:
			return AHDMI_MAT;break;
		case 0:                      // NULL data
		case 2:                      // Refer to SMPTE 338M
		case 3:                      // Pause
		case 14:                     // ATRAC
		case 15:                     // ATRAC 2/3
		case 16:                     // ATRAC-X
		case 18:                     // WMA
		case 23:                     // MPEG4-ALS
		default:
			return AHDMI_PCM;break;
		};
	}
}

#else

static DTV_STATUS_T HdmiStartDecoding(int adecIndex)
{
    HAL_AUDIO_FLOW_STATUS* pCurFlowStatus;
    Base* pAin;
    Base* pPPAo;
    FlowManager* pFlow = NULL;

    AUDIO_ERROR("[AUDH](%s) ADEC%d\n", __FUNCTION__, adecIndex);

    if(!IsValidAdecIdx(adecIndex))
        return NOT_OK;

    pFlow = Aud_flow[adecIndex];
    pCurFlowStatus = &flowStatus[adecIndex];

    if((m_IsSetMainDecOptByWebOs == FALSE && isAutoSetMainAdec == FALSE) || !IsValidAdecIdx((HAL_AUDIO_ADEC_INDEX_T)Aud_mainDecIndex))
    {
        isAutoSetMainAdec = TRUE;
        Aud_mainDecIndex = adecIndex;
        AUDIO_INFO("%s:%d MainDecOutput is not set. Now, set auto, adecIndex=%d,Aud_mainDecIndex=%d\n",__FUNCTION__,__LINE__, adecIndex, Aud_mainDecIndex);
    }

    if(adecIndex == Aud_mainDecIndex)
    {
        pAin = Aud_MainAin;
#ifdef START_DECODING_WHEN_GET_FORMAT
        pPPAo = Aud_ppAout;
#endif
    }
    else
    {
        pAin = Aud_SubAin;
#ifdef START_DECODING_WHEN_GET_FORMAT
        pPPAo = Aud_subPPAout;
#endif
    }

    pFlow->Stop(pFlow);
    RemoveFilter(pCurFlowStatus, pFlow);

    /* Connect to Input Source */
    pFlow->Connect(pFlow, pAin, Aud_dec[adecIndex]);
    SwitchHDMIFocus(pAin);

#ifdef START_DECODING_WHEN_GET_FORMAT
    if(adecIndex == Aud_mainDecIndex)
    {
        pCurFlowStatus->IsAINExist = TRUE;
    }
    else
    {
        pCurFlowStatus->IsSubAINExist = TRUE;
    }

	pFlow->Connect(pFlow, Aud_dec[adecIndex],pPPAo );

    if((adecIndex == HAL_AUDIO_ADEC0))
    {
        pCurFlowStatus->IsDEC0Exist = TRUE;
    }
    if((adecIndex == HAL_AUDIO_ADEC1))
    {
        pCurFlowStatus->IsDEC1Exist = TRUE;
    }
#if defined(VIRTUAL_ADEC_ENABLED)
    if((adecIndex == HAL_AUDIO_ADEC2))
    {
        pCurFlowStatus->IsDEC2Exist = TRUE;
    }
    if((adecIndex == HAL_AUDIO_ADEC3))
    {
        pCurFlowStatus->IsDEC3Exist = TRUE;
    }
#endif

    if(adecIndex == Aud_mainDecIndex)
    {
        AUDIO_DEBUG("focus \n");
        pPPAo->SwitchFocus(pPPAo, NULL);
		pCurFlowStatus->IsMainPPAOExist = TRUE;
        //wait AO finish swithing focus
        msleep(20);
    }else
	    pCurFlowStatus->IsSubPPAOExist = TRUE;

    ResourceStatus[adec2res((HAL_AUDIO_ADEC_INDEX_T)adecIndex)].connectStatus[0] = HAL_AUDIO_RESOURCE_RUN;
#endif

    ADSP_DEC_SetHdmiFmt(adecIndex, SRCType_to_ADECType(HAL_AUDIO_SRC_TYPE_UNKNOWN));

    pFlow->Flush(pFlow);
    pFlow->Run(pFlow);

    return OK;
}

static DTV_STATUS_T HdmiStopDecoding(int adecIndex)
{
    FlowManager* pFlow = NULL;
    AUDIO_ERROR("[AUDH](%s) ADEC%d\n", __FUNCTION__, adecIndex);

    switch(adecIndex)
    {
        case HAL_AUDIO_ADEC0:
            pFlow = Aud_flow[HAL_AUDIO_ADEC0];
            break;
        case HAL_AUDIO_ADEC1:
            pFlow = Aud_flow[HAL_AUDIO_ADEC1];
            break;
#if defined(VIRTUAL_ADEC_ENABLED)
        case HAL_AUDIO_ADEC2:
            pFlow = Aud_flow[HAL_AUDIO_ADEC2];
            break;
        case HAL_AUDIO_ADEC3:
            pFlow = Aud_flow[HAL_AUDIO_ADEC2];
            break;
#endif
        default:
            break;
    }

    if(adecIndex == Aud_mainDecIndex)
    {
        if(pFlow != NULL)
        {
            pFlow->Stop(pFlow);
            pFlow->Remove(pFlow, Aud_MainAin);
            pFlow->Remove(pFlow, Aud_dec[adecIndex]);
        }
    }
    else
    {
        if(pFlow != NULL)
        {
            pFlow->Stop(pFlow);
            pFlow->Remove(pFlow, Aud_SubAin);
            pFlow->Remove(pFlow, Aud_dec[adecIndex]);
        }
    }
    return OK;
}

ahdmi_type_ext_type_t GetHDMIModeWhenStop(int iDec)
{
    ahdmi_type_ext_type_t HDMIMode;
    HAL_AUDIO_SRC_TYPE_T SRCType = HAL_AUDIO_SRC_TYPE_UNKNOWN;
    AUDIO_RPC_DEC_FORMAT_INFO dec_fomat;
    int limit_x_10ms = 1; /* x*10ms */
    int n_10ms = 0;
    int64_t timerstamp[2];

    if(HDMI_fmt_change_transition) {
        HDMIMode = prevHDMIMode;
        return HDMIMode;
    }
    timerstamp[0] = pli_getPTS();

    HdmiStartDecoding(iDec);

    while(SRCType == HAL_AUDIO_SRC_TYPE_UNKNOWN && n_10ms < limit_x_10ms) {
        ((DEC*)Aud_dec[iDec]->pDerivedObj)->GetAudioFormatInfo(Aud_dec[iDec], &dec_fomat);
        SRCType = ADECType_to_SRCType(dec_fomat.type, dec_fomat.reserved[1]);
        n_10ms++;
        msleep(10);
    }
    if(n_10ms >= limit_x_10ms) {
        HDMIMode = AHDMI_NO_AUDIO;
        timerstamp[1] = pli_getPTS();
        AUDIO_ERROR("%s decoding type is %s in %d ms\n", __FUNCTION__, "NO_AUDIO", (int)(timerstamp[1] - timerstamp[0])/90);
    } else {
        HDMIMode = SRCType_to_HDMIMode(SRCType);
        if(SRCType != HAL_AUDIO_SRC_TYPE_UNKNOWN) {
            prevHDMIMode = HDMIMode;
            HDMI_fmt_change_transition = TRUE;
        }
        timerstamp[1] = pli_getPTS();
        AUDIO_ERROR("%s decoding type is %s in %d ms\n", __FUNCTION__, GetSRCTypeName(SRCType), (int)(timerstamp[1] - timerstamp[0])/90);
    }

#ifdef START_DECODING_WHEN_GET_FORMAT
    At_Stop_To_Start_Decode = TRUE;
    adec_info[iDec].userAdecFormat = SRCType;
#else
    HdmiStopDecoding(iDec);
#endif

    return HDMIMode;
}
#endif

DTV_STATUS_T RHAL_AUDIO_HDMI_GetAudioMode(ahdmi_type_ext_type_t *pHDMIMode)
{
    HAL_AUDIO_RESOURCE_T upstreamResourceId0, upstreamResourceId1;
    HAL_AUDIO_ADEC_INDEX_T iDec = HAL_AUDIO_ADEC0;
    int ocup_cnt = 0;

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(pHDMIMode == NULL)
        return NOT_OK;

    /* finding the decoder which is used by checking the upstream source is HDMI or not. */

    // checkikng ADEC0 and ADEC1/2/3
    upstreamResourceId0 = GetNonOutputModuleSingleInputResource(ResourceStatus[HAL_AUDIO_RESOURCE_ADEC0]);
    upstreamResourceId1 = GetNonOutputModuleSingleInputResource(ResourceStatus[HAL_AUDIO_RESOURCE_ADEC1]);
#if defined(VIRTUAL_ADEC_ENABLED)
    upstreamResourceId2 = GetNonOutputModuleSingleInputResource(ResourceStatus[HAL_AUDIO_RESOURCE_ADEC2]);
    upstreamResourceId3 = GetNonOutputModuleSingleInputResource(ResourceStatus[HAL_AUDIO_RESOURCE_ADEC3]);
#endif

    if(IsHDMISource(upstreamResourceId0))
        ocup_cnt++;
    if(IsHDMISource(upstreamResourceId1))
        ocup_cnt++;
#if defined(VIRTUAL_ADEC_ENABLED)
    if(IsHDMISource(upstreamResourceId2))
        ocup_cnt++;
    if(IsHDMISource(upstreamResourceId3))
        ocup_cnt++;
#endif

    if(ocup_cnt >= 2)
    {
        AUDIO_ERROR("%s:%d Two decoders are both connected to HDMI.\n", __FUNCTION__,__LINE__);
        return NOT_PERMITTED;
    }
    else if(IsHDMISource(upstreamResourceId0))
    {
        iDec = HAL_AUDIO_ADEC0;
    }
    else if(IsHDMISource(upstreamResourceId1))
    {
        iDec = HAL_AUDIO_ADEC1;
    }
#if defined(VIRTUAL_ADEC_ENABLED)
    else if(IsHDMISource(upstreamResourceId2))
    {
        iDec = HAL_AUDIO_ADEC2;
    }
    else if(IsHDMISource(upstreamResourceId3))
    {
        iDec = HAL_AUDIO_ADEC3;
    }
#endif
    else
    {
        if(drvif_Hdmi_GetHDMIMode() == 0){  //MODE_DVI
            AUDIO_DEBUG("[%s:%d] HDMI MODE :DVI\n", __FUNCTION__,__LINE__);
            *pHDMIMode = AHDMI_DVI;
            return OK;

        }else{
            *pHDMIMode = AHDMI_NO_AUDIO;
            AUDIO_ERROR("%s:%d No ADEC connect to HDMI\n", __FUNCTION__,__LINE__);
            return OK;
        }
    }

    if(AUDIO_HAL_CHECK_ISATSTOPSTATE(ResourceStatus[adec2res(iDec)], 0))
    {
#if defined(SEARCH_PAPB_WHEN_STOP)
        Base* pAin;
        pAin = Aud_MainAin;
        if(pAin->GetState(pAin) == STATE_STOP) {
            SwitchHDMIFocus(pAin);
            pAin->Flush(pAin);
            pAin->Pause(pAin);
            pAin->Run(pAin);
        }
        *pHDMIMode = check_hdmimode_search();
        prevHDMIMode = *pHDMIMode;
#else
        *pHDMIMode = GetHDMIModeWhenStop(iDec);
        prevHDMIMode = *pHDMIMode;
#endif
        return OK;
    }
    else
    {
#if defined(SEARCH_PAPB_WHEN_STOP)
        *pHDMIMode = check_hdmimode_search();
        if (prevHDMIMode != *pHDMIMode)
        {
            AUDIO_INFO("[AUDH](%s) detect format change %d -> %d\n",__FUNCTION__, prevHDMIMode, *pHDMIMode);
        }

        prevHDMIMode = *pHDMIMode;
        return OK;
#else
        HAL_AUDIO_SRC_TYPE_T code_type;
        DTV_STATUS_T Ret = NOT_OK;
        //get the audio coding type via audio decoder
        Ret = RHAL_AUDIO_GetDecodingType(iDec, &code_type);

        if(Ret == OK)
        {
            if(code_type == HAL_AUDIO_SRC_TYPE_UNKNOWN) {
                code_type = GetUserFormat(iDec);
                AUDIO_INFO("[AUDH](%s) return user format %s\n",__FUNCTION__,GetSRCTypeName(code_type));
                check_hdmimode_search();
            }
            if(code_type != GetUserFormat(iDec)) {
                AUDIO_INFO("[AUDH](%s) detect format change %s -> %s\n",__FUNCTION__,
                        GetSRCTypeName(GetUserFormat(iDec)),GetSRCTypeName(code_type));
                check_hdmimode_search();
            }

            *pHDMIMode = SRCType_to_HDMIMode(code_type);
            prevHDMIMode = *pHDMIMode;
            return OK;
        }
        else
        {
            AUDIO_ERROR("%s Fail to get the decoding type!!!\n", __FUNCTION__);
            return NOT_OK;
        }
#endif
    }
}

extern int hdmi_arc_enable(unsigned char en);
extern int hdmi_arc_get_status(void);
DTV_STATUS_T RHAL_AUDIO_HDMI_SetAudioReturnChannel(BOOLEAN bOnOff)
{
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;
    AUDIO_INFO("%s %d  \n", __FUNCTION__, bOnOff);

    memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));
    audioConfig.msg_id   = AUDIO_CONFIG_CMD_HDMI_ARC;
    if(bOnOff) {
        audioConfig.value[0] = TRUE;
        audioConfig.value[1] = 1<<AUDIO_HDMI_CODING_TYPE_DDP;
        audioConfig.value[2] = FALSE;
    } else {
        audioConfig.value[0] = FALSE;
        audioConfig.value[1] = FALSE;
        audioConfig.value[2] = FALSE;
    }

    if(SendRPC_AudioConfig(&audioConfig) != S_OK) {
        AUDIO_ERROR("[%s,%d] hdmi arc failed\n", __FUNCTION__, __LINE__);
        return NOT_OK;
    }

    HDMI_arcOnOff = bOnOff;

    return OK;
}

DTV_STATUS_T RHAL_AUDIO_HDMI_SetEnhancedAudioReturnChannel(BOOLEAN bOnOff)
{
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;
    AUDIO_INFO("%s %d  \n", __FUNCTION__, bOnOff);

    memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));
    audioConfig.msg_id   = AUDIO_CONFIG_CMD_HDMI_EARC;
    if(bOnOff) {
        audioConfig.value[0] = TRUE;
        audioConfig.value[1] = 1<<AUDIO_HDMI_CODING_TYPE_DDP;
        audioConfig.value[2] = FALSE;
    } else {
        audioConfig.value[0] = FALSE;
        audioConfig.value[1] = FALSE;
        audioConfig.value[2] = FALSE;
    }

    if (SendRPC_AudioConfig(&audioConfig) != S_OK) {
        AUDIO_ERROR("[%s,%d] hdmi earc on failed\n", __FUNCTION__, __LINE__);
        return NOT_OK;
    }

    HDMI_earcOnOff = bOnOff;

    return OK;  //OK or NOT_OK
}

DTV_STATUS_T RHAL_AUDIO_HDMI_GetCopyInfo(ahdmi_copyprotection_ext_type_t *pCopyInfo)
{
    UINT8 copyright = 0;
    UINT8 category_code = 0;
    AUDIO_SPDIF_CS spdif_cs;
    /*AUDIO_DEBUG("[AUDH] %s %d  \n", __FUNCTION__, (int)pCopyInfo);*/

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;
    if(pCopyInfo == NULL)
    {
        AUDIO_ERROR("address error \n");
        return NOT_OK;
    }
#if 0
    HAL_AUDIO_SPDIF_COPY_FREE       = 0,    /* cp-bit : 1, L-bit : 0 */
    HAL_AUDIO_SPDIF_COPY_NO_MORE    = 1,    /* cp-bit : 0, L-bit : 1 */
    HAL_AUDIO_SPDIF_COPY_ONCE       = 2,    /* cp-bit : 0, L-bit : 0 */
    HAL_AUDIO_SPDIF_COPY_NEVER      = 3,    /* cp-bit : 0, L-bit : 1 */
#endif
    if(ipt_Is_HDMI == FALSE){
        /*AUDIO_DEBUG("[AUDH] no HDMI connect\n");*/
        *pCopyInfo  = AHDMI_COPY_FREE;
        return OK;
    }

    memset(&spdif_cs, 0, sizeof(AUDIO_SPDIF_CS));
    spdif_cs.module_type = AUDIO_IN;
    RTKAUDIO_RPC_TOAGENT_GET_SPDIF_CS_SVC(&spdif_cs);
    copyright     = spdif_cs.copyright;
    category_code = spdif_cs.category_code;
    AUDIO_DEBUG("%s AI copyright %d category_code %d\n",__FUNCTION__, copyright, category_code);

    if(copyright == HAL_SPDIF_COPYRIGHT_FREE)
    {
        *pCopyInfo  = AHDMI_COPY_FREE; /* cp-bit : 1, L-bit : 0 */
    }
    else if( (category_code & HAL_SPDIF_CATEGORY_L_BIT_IS_1) == HAL_SPDIF_CATEGORY_L_BIT_IS_1)
    {
        *pCopyInfo = AHDMI_COPY_NEVER;
    }
    else
    {
        *pCopyInfo = AHDMI_COPY_ONCE; /* cp-bit : 0, L-bit : 0 */
    }
    return OK;
}

DTV_STATUS_T HAL_AUDIO_HDMI_GetPortAudioMode(HAL_AUDIO_HDMI_INDEX_T hdmiIndex, ahdmi_type_ext_type_t *pHDMIMode)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;
    return RHAL_AUDIO_HDMI_GetAudioMode(pHDMIMode);
}

DTV_STATUS_T HAL_AUDIO_HDMI_SetPortAudioReturnChannel(HAL_AUDIO_HDMI_INDEX_T hdmiIndex, BOOLEAN bOnOff)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;
    return RHAL_AUDIO_HDMI_SetAudioReturnChannel(bOnOff);
}

DTV_STATUS_T HAL_AUDIO_HDMI_GetPortCopyInfo(HAL_AUDIO_HDMI_INDEX_T hdmiIndex, ahdmi_copyprotection_ext_type_t *pCopyInfo)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;
    return RHAL_AUDIO_HDMI_GetCopyInfo(pCopyInfo);
}

/* Decoder */
DTV_STATUS_T HAL_AUDIO_SetSyncMode(HAL_AUDIO_ADEC_INDEX_T adecIndex, BOOLEAN bOnOff)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(!IsValidAdecIdx(adecIndex))
        return NOT_OK;

    if(!IsAdecOpen(adecIndex))
        return NOT_OK;

    AUDIO_DEBUG("skip sync mode setting %d \n", bOnOff);// PVR will control reference clock

    if(!(AUDIO_CHECK_ISBOOLEAN(bOnOff)))
        return NOT_OK;
    else {
        adec_status[adecIndex].SyncMode = bOnOff;
        return OK;
    }
}

DTV_STATUS_T RHAL_AUDIO_SetDolbyDRCMode(HAL_AUDIO_ADEC_INDEX_T adecIndex, adec_dolbydrc_mode_ext_type_t drcMode)
{
    HAL_AUDIO_RESOURCE_T curResourceId;
    HAL_AUDIO_RESOURCE_T outputResId[HAL_DEC_MAX_OUTPUT_NUM];
    int num_of_outputs=0;
    int isConnect2OputputModule = 0;
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;
    int i;

    AUDIO_DEBUG("%s ADEC%d mode %d\n", __FUNCTION__, adecIndex, drcMode);

    curResourceId = adec2res(adecIndex);
    /* Check Output source and Error handling */
    GetConnectOutputSource(curResourceId, outputResId, HAL_DEC_MAX_OUTPUT_NUM, &num_of_outputs);

    if((num_of_outputs < HAL_DEC_MAX_OUTPUT_NUM) && (num_of_outputs > 0))
    {
        for(i = 0; i < num_of_outputs; i++)
        {
            if(IsAOutSource(outputResId[i]))
            {
                isConnect2OputputModule = 1;
                break;
            }
        }
    }

    if(isConnect2OputputModule == 1)
    {
        if(ADEC_DOLBY_LINE_MODE == drcMode)
        {
            memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));
            audioConfig.msg_id = AUDIO_CONFIG_CMD_DD_COMP;
            audioConfig.value[0] = COMP_LINEOUT;

            if(SendRPC_AudioConfig(&audioConfig) != S_OK)
            {
                AUDIO_ERROR("[%s,%d] set compressmode failed\n", __FUNCTION__, __LINE__);
                return NOT_OK;
            }

            memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));
            audioConfig.msg_id = AUDIO_CONFIG_CMD_DD_SCALE;
            audioConfig.value[0] = COMP_SCALE_FULL;
            audioConfig.value[1] = COMP_SCALE_FULL;

            if(SendRPC_AudioConfig(&audioConfig) != S_OK)
            {
                AUDIO_ERROR("[%s,%d] set scalehilo failed\n", __FUNCTION__, __LINE__);
                return NOT_OK;
            }
        }
        else if(ADEC_DOLBY_RF_MODE == drcMode)
        {
            memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));
            audioConfig.msg_id = AUDIO_CONFIG_CMD_DD_COMP;
            audioConfig.value[0] = COMP_RF;

            if(SendRPC_AudioConfig(&audioConfig) != S_OK)
            {
                AUDIO_ERROR("[%s,%d] set compressmode failed\n", __FUNCTION__, __LINE__);
                return NOT_OK;
            }

            memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));
            audioConfig.msg_id = AUDIO_CONFIG_CMD_DD_SCALE;
            audioConfig.value[0] = COMP_SCALE_FULL;
            audioConfig.value[1] = COMP_SCALE_FULL;

            if(SendRPC_AudioConfig(&audioConfig) != S_OK)
            {
                AUDIO_ERROR("[%s,%d] set scalehilo failed\n", __FUNCTION__, __LINE__);
                return NOT_OK;
            }
        }
        else if(ADEC_DOLBY_DRC_OFF == drcMode)
        {
            memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));
            audioConfig.msg_id = AUDIO_CONFIG_CMD_DD_COMP;
            audioConfig.value[0] = COMP_LINEOUT;

            if(SendRPC_AudioConfig(&audioConfig) != S_OK)
            {
                AUDIO_ERROR("[%s,%d] set compressmode failed\n", __FUNCTION__, __LINE__);
                return NOT_OK;
            }

            memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));
            audioConfig.msg_id = AUDIO_CONFIG_CMD_DD_SCALE;
            audioConfig.value[0] = COMP_SCALE_NONE;
            audioConfig.value[1] = COMP_SCALE_NONE;

            if(SendRPC_AudioConfig(&audioConfig) != S_OK)
            {
                AUDIO_ERROR("[%s,%d] set scalehilo failed\n", __FUNCTION__, __LINE__);
                return NOT_OK;
            }
        }
        else
        {
            AUDIO_ERROR("unknow drc type %d \n", drcMode);
            return NOT_OK;
        }
    }
    if(drcMode > ADEC_DOLBY_DRC_OFF)
    {
        AUDIO_ERROR("unknow drc type %d \n", drcMode);
        return NOT_OK;
    }

    SetDecDrcMode((int)adecIndex, drcMode);// initial

    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetDolbyDRCMode(HAL_AUDIO_ADEC_INDEX_T adecIndex, adec_dolbydrc_mode_ext_type_t drcMode)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(!IsValidAdecIdx(adecIndex))
        return NOT_OK;

    if(!IsAdecOpen(adecIndex))
        return NOT_OK;

    return RHAL_AUDIO_SetDolbyDRCMode(adecIndex, drcMode);
}

DTV_STATUS_T RHAL_AUDIO_SetDownMixMode(HAL_AUDIO_ADEC_INDEX_T adecIndex, adec_downmix_mode_ext_type_t downmixMode)
{
    HAL_AUDIO_RESOURCE_T curResourceId;
    HAL_AUDIO_RESOURCE_T outputResId[HAL_DEC_MAX_OUTPUT_NUM];
    int num_of_outputs=0;
    int isConnect2OputputModule = 0;
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;
    int i;

    AUDIO_DEBUG("%s ADEC%d mode %d\n", __FUNCTION__, adecIndex, downmixMode);

    if(downmixMode > ADEC_LTRT_MODE)
    {
        AUDIO_ERROR("unknow dmx type %d \n", downmixMode);
        return NOT_OK;
    }

    curResourceId = adec2res(adecIndex);
    /* Check Output source and Error handling */
    GetConnectOutputSource(curResourceId, outputResId, HAL_DEC_MAX_OUTPUT_NUM, &num_of_outputs);

    if((num_of_outputs < HAL_DEC_MAX_OUTPUT_NUM) && (num_of_outputs > 0))
    {
        for(i = 0; i < num_of_outputs; i++)
        {
            if(IsAOutSource(outputResId[i]))
            {
                isConnect2OputputModule = 1;
                break;
            }
        }
    }

    if(isConnect2OputputModule == 1)
    {
        memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));
        audioConfig.msg_id = AUDIO_CONFIG_CMD_DD_DOWNMIXMODE;
        if(ADEC_LORO_MODE == downmixMode)
        {
            audioConfig.value[0] = MODE_STEREO;
            audioConfig.value[1] = LFE_OFF;
            audioConfig.value[2] = 0x00002379;
        }
        else if(ADEC_LTRT_MODE == downmixMode)
        {
            audioConfig.value[0] = MODE_DOLBY_SURROUND;
            audioConfig.value[1] = LFE_OFF;
            audioConfig.value[2] = 0x00002379;
        }
        else
        {
            AUDIO_ERROR("unknow downmix type %d \n", downmixMode);
            return NOT_OK;
        }
    }

    if(SendRPC_AudioConfig(&audioConfig) != S_OK) {
        AUDIO_ERROR("[%s,%d] set dd donwmix failed\n", __FUNCTION__, __LINE__);
        return NOT_OK;
    }

    SetDecDownMixMode((int)adecIndex, downmixMode);

    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetDownMixMode(HAL_AUDIO_ADEC_INDEX_T adecIndex, adec_downmix_mode_ext_type_t downmixMode)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(!IsValidAdecIdx(adecIndex))
        return NOT_OK;

    if(!IsAdecOpen(adecIndex))
        return NOT_OK;

    return RHAL_AUDIO_SetDownMixMode(adecIndex, downmixMode) ;
}

DTV_STATUS_T HAL_AUDIO_SetEsPushMode(HAL_AUDIO_ADEC_INDEX_T adecIndex, HAL_AUDIO_ES_MODE_T esMode, UINT32 BSHeader)
{
    AUDIO_INFO("%s ADEC%d  SetEsMode%d, BSHeader %x \n", __FUNCTION__, adecIndex, esMode, BSHeader);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(!IsValidAdecIdx(adecIndex))
        return NOT_OK;

    if(esMode >= HAL_AUDIO_ES_MAX)
        return NOT_OK;

    SetDecEsPushMode(adecIndex, esMode);

    if(esMode != HAL_AUDIO_ES_NORMAL)
    {
        ESPlay* pESPlay;
        if(Aud_ESPlay[adecIndex] == NULL) {
            Aud_ESPlay[adecIndex] = new_ESPlay();
            if(Aud_ESPlay[adecIndex] == NULL) {
                AUDIO_ERROR("create ESPlay fail \n");
                return NOT_OK;
            }
        }
        pESPlay = (ESPlay*)Aud_ESPlay[adecIndex]->pDerivedObj;
        pESPlay->SetBSHeaderPhy(Aud_ESPlay[adecIndex], BSHeader);
        SetSysMemoryExist(adecIndex, TRUE);
    }
    else
    {
        SetSysMemoryExist(adecIndex, FALSE);
    }

    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetEsPushWptr(HAL_AUDIO_ADEC_INDEX_T adecIndex, UINT32 wptr)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(!IsValidAdecIdx(adecIndex))
        return NOT_OK;

    if(Aud_ESPlay[adecIndex] != NULL) {
        ESPlay* pESPlay = (ESPlay*)Aud_ESPlay[adecIndex]->pDerivedObj;
        pESPlay->SetBSWptr(Aud_ESPlay[adecIndex], wptr);
        AUDIO_INFO("[ESPlay] wptr %x\n",wptr);
    }
    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetEsPush_PCMFormat(HAL_AUDIO_ADEC_INDEX_T adecIndex, int fs, int nch, int bitPerSample)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(!IsValidAdecIdx(adecIndex))
        return NOT_OK;

    if(Aud_ESPlay[adecIndex] != NULL) {
        ESPlay* pESPlay = (ESPlay*)Aud_ESPlay[adecIndex]->pDerivedObj;
        pESPlay->SetPCMFormat(Aud_ESPlay[adecIndex], fs, nch, bitPerSample);
        AUDIO_INFO("[ESPlay] fs(%d), nch(%d), bitPerSample(%d)\n",fs,nch,bitPerSample);
    }
    return OK;
}

DTV_STATUS_T HAL_AUDIO_GetDecodingType(HAL_AUDIO_ADEC_INDEX_T adecIndex, HAL_AUDIO_SRC_TYPE_T *pAudioType)
{
    return RHAL_AUDIO_GetDecodingType(adecIndex, pAudioType);
}

DTV_STATUS_T HAL_AUDIO_GetAdecStatus(HAL_AUDIO_ADEC_INDEX_T adecIndex, HAL_AUDIO_ADEC_INFO_T *pAudioAdecInfo)
{
    HAL_AUDIO_RESOURCE_T curResourceId = adec2res(adecIndex);
	AUDIO_RPC_DEC_FORMAT_INFO dec_fomat = {0};

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(!IsValidAdecIdx(adecIndex))
        return NOT_OK;

    if(pAudioAdecInfo == NULL)
        return NOT_OK;

    if(AUDIO_HAL_CHECK_STOP_NOTAVAILABLE(ResourceStatus[curResourceId], 0))
    {
		adec_info[adecIndex].IsEsExist = FALSE;
		adec_info[adecIndex].curAdecFormat = HAL_AUDIO_SRC_TYPE_UNKNOWN;
		adec_info[adecIndex].sourceAdecFormat = adec_info[adecIndex].curAdecFormat;
    }
	else
	{
        memset(&dec_fomat, 0, sizeof(AUDIO_RPC_DEC_FORMAT_INFO));
		adec_info[adecIndex].IsEsExist = ((DEC*)Aud_dec[adecIndex]->pDerivedObj)->IsESExist(Aud_dec[adecIndex]);

		if (((DEC*)Aud_dec[adecIndex]->pDerivedObj)->GetAudioFormatInfo(Aud_dec[adecIndex], &dec_fomat) != S_OK) {
			adec_info[adecIndex].curAdecFormat = HAL_AUDIO_SRC_TYPE_UNKNOWN;
			adec_info[adecIndex].sourceAdecFormat = adec_info[adecIndex].curAdecFormat;
			goto status_finish;
		}

		if(dec_fomat.type == AUDIO_UNKNOWN_TYPE && dec_fomat.nChannels != 0) {
			adec_info[adecIndex].curAdecFormat = HAL_AUDIO_SRC_TYPE_UNKNOWN;
			adec_info[adecIndex].sourceAdecFormat = adec_info[adecIndex].curAdecFormat;
			goto status_finish;
		}

		if(adec_info[adecIndex].IsEsExist == 0) {
			adec_info[adecIndex].curAdecFormat = HAL_AUDIO_SRC_TYPE_UNKNOWN;
			adec_info[adecIndex].sourceAdecFormat = adec_info[adecIndex].curAdecFormat;
			goto status_finish;
		}

        if(dec_fomat.type != SRCType_to_ADECType(GetUserFormat(adecIndex)))
        {
            AUDIO_INFO("dec_fomat.type(%d) != GetUserFormat(%d) Chg to Unknown!!!!\n", dec_fomat.type, SRCType_to_ADECType(GetUserFormat(adecIndex)));
            adec_info[adecIndex].curAdecFormat = HAL_AUDIO_SRC_TYPE_UNKNOWN;
            adec_info[adecIndex].sourceAdecFormat = adec_info[adecIndex].curAdecFormat;
            goto status_finish;
        }

		adec_info[adecIndex].curAdecFormat = ADECType_to_SRCType(dec_fomat.type, dec_fomat.reserved[1]);
		adec_info[adecIndex].sourceAdecFormat = adec_info[adecIndex].curAdecFormat;

		if(dec_fomat.nChannels == 0) {
			adec_info[adecIndex].audioMode = ADEC_TP_MODE_UNKNOWN;
		} else if(dec_fomat.nChannels == 1) {
			adec_info[adecIndex].audioMode = ADEC_TP_MODE_MONO;
		} else if(dec_fomat.nChannels == 2) {
			adec_info[adecIndex].audioMode = ADEC_TP_MODE_STEREO;
		} else if(dec_fomat.nChannels >  2) {
			adec_info[adecIndex].audioMode = ADEC_TP_MODE_MULTI;
		}

		if(dec_fomat.type == AUDIO_MPEG_DECODER_TYPE)
		{
			// bit [0:7]  for mpegN // bit [8:15]  for layerN
			// bit [16:23] for mode (0x0:stereo,0x1:joint stereo,0x2:dual,0x3:mono)
			int layerN, stereoMode;
			layerN = (dec_fomat.reserved[0] & 0xFF00) >> 8;
			stereoMode = (dec_fomat.reserved[0] & 0xFF0000) >> 16;

			adec_info[adecIndex].mpeg_bitrate    = (dec_fomat.nAvgBytesPerSec) / 4000;
			adec_info[adecIndex].mpeg_sampleRate = (dec_fomat.nSamplesPerSec / 1000);
			adec_info[adecIndex].mpeg_layer      = layerN;
			adec_info[adecIndex].mpeg_channelNum = dec_fomat.nChannels;

			if(stereoMode == 1) {
				adec_info[adecIndex].audioMode = ADEC_TP_MODE_JOINT_STEREO;
			} else if(stereoMode == 2) {
				adec_info[adecIndex].audioMode = ADEC_TP_MODE_DUALMONO;
			} else if(stereoMode == 3) {
				adec_info[adecIndex].audioMode = ADEC_TP_MODE_MONO;
			} else {
				adec_info[adecIndex].audioMode = ADEC_TP_MODE_STEREO;
			}
		}
		else if((dec_fomat.type == AUDIO_AC3_DECODER_TYPE) || (dec_fomat.type == AUDIO_DDP_DECODER_TYPE))
		{
			// bit [0:7]  for lfeon // bit [8:15]  for acmod
			int acmode;
			acmode = (dec_fomat.reserved[0] & 0xFF00) >> 8;

			adec_info[adecIndex].ac3_bitrate    = (dec_fomat.nAvgBytesPerSec) / 4000;
			adec_info[adecIndex].ac3_sampleRate = (dec_fomat.nSamplesPerSec / 1000);
			adec_info[adecIndex].ac3_EAC3       = (dec_fomat.type == AUDIO_DDP_DECODER_TYPE)? 1 : 0;

			if(acmode == 0) {
				adec_info[adecIndex].audioMode      = ADEC_TP_MODE_DUALMONO;
			} else if(acmode == 1) {
				adec_info[adecIndex].audioMode      = ADEC_TP_MODE_MONO;
				adec_info[adecIndex].ac3_channelnum = 1;
			} else if(acmode == 2) {
				adec_info[adecIndex].audioMode      = ADEC_TP_MODE_STEREO;
				adec_info[adecIndex].ac3_channelnum = 2;
			} else {
				adec_info[adecIndex].audioMode      = ADEC_TP_MODE_MULTI;
				adec_info[adecIndex].ac3_channelnum = dec_fomat.nChannels;
			}
		}
		else if(dec_fomat.type == AUDIO_AAC_DECODER_TYPE)
		{
			int channel_mode, version, format;
			channel_mode = (dec_fomat.reserved[0] & 0xFF00) >> 8;
			version = (dec_fomat.reserved[1] & 0xFF00) >> 8;
			format = (dec_fomat.reserved[1] & 0xFF);

			if(channel_mode == MODE_11) {
				adec_info[adecIndex].audioMode        = ADEC_TP_MODE_DUALMONO;
				adec_info[adecIndex].heaac_channelNum = 2;
			} else if(channel_mode == MODE_10) {
				adec_info[adecIndex].audioMode        = ADEC_TP_MODE_MONO;
				adec_info[adecIndex].heaac_channelNum = 1;
			} else if(channel_mode == MODE_20) {
				adec_info[adecIndex].audioMode        = ADEC_TP_MODE_STEREO;
				adec_info[adecIndex].heaac_channelNum = 2;
			} else {
				adec_info[adecIndex].audioMode        = ADEC_TP_MODE_MULTI;
				adec_info[adecIndex].heaac_channelNum = dec_fomat.nChannels;
			}

			if(version == 0) {
				adec_info[adecIndex].heaac_version = 0; // AAC
			} else if(version == 1) {
				adec_info[adecIndex].heaac_version = 1; // HEAACv1
			} else {
				adec_info[adecIndex].heaac_version = 2; // HEAACv2
			}

			if(format == 0) {
				adec_info[adecIndex].heaac_trasmissionformat = 0;
			} else if(format == 1) {
				adec_info[adecIndex].heaac_trasmissionformat = 1;
			} else {
				AUDIO_ERROR("unknown AAC format %d, 0:LATM/LOAS, 1:ADTS\n", format);
				adec_info[adecIndex].heaac_trasmissionformat = 0;
			}
		}
		else if(dec_fomat.type == AUDIO_RAW_AAC_DECODER_TYPE)
		{
			adec_info[adecIndex].curAdecFormat = HAL_AUDIO_SRC_TYPE_UNKNOWN;
		}
		else if(dec_fomat.type == AUDIO_DRA_DECODER_TYPE)
		{
			// bit [0:15]  for lfeon // bit [16:31]  for main channel number
			long tmp = dec_fomat.reserved[0];
			int lfeon = (int)(tmp & 0x0000FFFF);
			int main_channel = (int)((tmp>>16) & 0x0000FFFF);
			if(dec_fomat.nChannels != (main_channel + lfeon)) {
				AUDIO_ERROR("[AUDH][ERROR]:  dec_fomat.nChannels = %d, main_channel+lfeon = %d\n", dec_fomat.nChannels,(main_channel + lfeon));
				goto status_finish;
			}

			if(lfeon == 1) {
				if(main_channel == 2)
					adec_info[adecIndex].audioMode = (int)ADEC_TP_MODE_2_1_FL_FR_LFE;
				else if(main_channel == 3)
					adec_info[adecIndex].audioMode = (int)ADEC_TP_MODE_3_1_FL_FR_RC_LFE;
				else if(main_channel == 4)
					adec_info[adecIndex].audioMode = (int)ADEC_TP_MODE_4_1_FL_FR_RL_RR_LFE;
				else if(main_channel == 5)
					adec_info[adecIndex].audioMode = (int)ADEC_TP_MODE_5_1_FL_FR_FC_RL_RR_LFE;
				else
					AUDIO_ERROR("Invalid DRA feedback info: lfeon=%d, main_channel=%d\n", (int)lfeon, (int)main_channel);
			} else {
				if(main_channel == 1)
					adec_info[adecIndex].audioMode = (int)ADEC_TP_MODE_MONO;
				else if(main_channel == 2)
					adec_info[adecIndex].audioMode = (int)ADEC_TP_MODE_STEREO;
				else if(main_channel == 3)
					adec_info[adecIndex].audioMode = (int)ADEC_TP_MODE_3_0_FL_FR_RC;
				else if(main_channel == 4)
					adec_info[adecIndex].audioMode = (int)ADEC_TP_MODE_4_0_FL_FR_RL_RR;
				else if(main_channel == 5)
					adec_info[adecIndex].audioMode = (int)ADEC_TP_MODE_5_0_FL_FR_FC_RL_RR;
				else
					AUDIO_ERROR("Invalid DRA feedback info: lfeon=%d, main_channel=%d\n", (int)lfeon, (int)main_channel);
			}
		}
	}

status_finish:
    memcpy(pAudioAdecInfo, &adec_info[adecIndex], sizeof(HAL_AUDIO_ADEC_INFO_T));
	pAudioAdecInfo->adec_port_number = adecIndex;
    pAudioAdecInfo->userAdecFormat   = codec_type_hal2alsa(pAudioAdecInfo->userAdecFormat);
    pAudioAdecInfo->curAdecFormat    = codec_type_hal2alsa(pAudioAdecInfo->curAdecFormat);
    pAudioAdecInfo->sourceAdecFormat = codec_type_hal2alsa(pAudioAdecInfo->sourceAdecFormat);
    adec_status[adecIndex].UserCodec = pAudioAdecInfo->userAdecFormat;
    adec_status[adecIndex].CurCodec  = pAudioAdecInfo->curAdecFormat;
    adec_status[adecIndex].SrcCodec  = pAudioAdecInfo->sourceAdecFormat;
    adec_status[adecIndex].SrcBitrates = (dec_fomat.nAvgBytesPerSec) / 4000;
    adec_status[adecIndex].SrcSamplingRate = (dec_fomat.nSamplesPerSec / 1000);
    adec_status[adecIndex].SrcChannel = dec_fomat.nChannels;
    adec_status[adecIndex].OutputCodec = ADEC_SRC_CODEC_PCM;
    adec_status[adecIndex].OutputChannel = dec_fomat.nChannels;
    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetDualMonoOutMode(HAL_AUDIO_ADEC_INDEX_T adecIndex, adec_dualmono_mode_ext_type_t outputMode)
{
    Base *pDec;
    const char modeName[4][4] = {"LR","LL","RR","MIX"};
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    AUDIO_DEBUG("%s ADEC%d to %s mode\n",__FUNCTION__, adecIndex, modeName[outputMode]);

    if(!IsValidAdecIdx(adecIndex))
    {
        AUDIO_ERROR("error dec index %d\n", adecIndex);
        return NOT_OK;
    }

    if(!IsAdecOpen(adecIndex))
        return NOT_OK;

    if(outputMode > ADEC_DUALMONO_MODE_MIX)
        return NOT_OK;

    pDec = Aud_dec[adecIndex];

    if(outputMode == ADEC_DUALMONO_MODE_LR)
        ((DEC*)pDec->pDerivedObj)->SetChannelSwap(pDec, AUDIO_CHANNEL_OUT_SWAP_STEREO);
    else if(outputMode == ADEC_DUALMONO_MODE_LL)
        ((DEC*)pDec->pDerivedObj)->SetChannelSwap(pDec, AUDIO_CHANNEL_OUT_SWAP_L_TO_R);
    else if(outputMode == ADEC_DUALMONO_MODE_RR)
        ((DEC*)pDec->pDerivedObj)->SetChannelSwap(pDec, AUDIO_CHANNEL_OUT_SWAP_R_TO_L);
    else  //ADEC_DUALMONO_MODE_MIX
        ((DEC*)pDec->pDerivedObj)->SetChannelSwap(pDec, AUDIO_CHANNEL_OUT_SWAP_LR_MIXED);

    adec_info[adecIndex].curAdecDualMonoMode = outputMode;
    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetDolbyOTTMode(BOOLEAN bIsOTTEnable, BOOLEAN bIsATMOSLockingEnable)
{
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;
    AUDIO_INFO("%s OTT %d ATMOSLock %d\n", __FUNCTION__, bIsOTTEnable, bIsATMOSLockingEnable);

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(bIsOTTEnable == FALSE && bIsATMOSLockingEnable == TRUE)  //not enabled, invalid
        return NOT_OK;

    audioConfig.msg_id = AUDIO_CONFIG_CMD_SET_DOLBY_OTT_MODE;
    audioConfig.value[0] = bIsOTTEnable;             /* OTT mode */
    audioConfig.value[1] = bIsATMOSLockingEnable;    /* ATMOS Locking */

    if(SendRPC_AudioConfig(&audioConfig) != S_OK) return NOT_OK;

    return OK;
}

DTV_STATUS_T HAL_AUDIO_TP_GetAudioPTS(HAL_AUDIO_ADEC_INDEX_T adecIndex, UINT32 *pPts)
{
    int64_t pts;

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(pPts == NULL)
        return NOT_OK;

    if(!IsValidAdecIdx(adecIndex))
    {
        AUDIO_ERROR("error dec index %d \n",  adecIndex);
        return NOT_OK;
    }

    if(adec_info[adecIndex].bAdecStart != TRUE)
    {
        AUDIO_ERROR("ADEC%d is not running\n", adecIndex);
        return NOT_OK;
    }

    ((DEC*)Aud_dec[adecIndex]->pDerivedObj)->GetCurrentPTS(Aud_dec[adecIndex], &pts);
    *pPts = (UINT32) pts;

    AUDIO_DEBUG("%s ADEC%d PTS: %x\n", __FUNCTION__, adecIndex, *pPts);

    return OK;
}

DTV_STATUS_T HAL_AUDIO_TP_SetAudioDescriptionMain(HAL_AUDIO_ADEC_INDEX_T main_adecIndex, BOOLEAN bOnOff)
{
    AUDIO_INFO("%s type %d \n", __FUNCTION__, bOnOff);

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(!IsValidAdecIdx(main_adecIndex))
    {
        AUDIO_ERROR("ADEC index error  %d \n", main_adecIndex);
        return NOT_OK;
    }

    // For socts, need to check both adec 0/1 are opened.
    // ref: https://harmony.lge.com:8443/issue/browse/KTASKWBS-10490
    if(!IsAdecOpen(HAL_AUDIO_ADEC0) || !IsAdecOpen(HAL_AUDIO_ADEC1))
        return NOT_OK;

    RHAL_AUDIO_SetAudioDescriptionMode(main_adecIndex, bOnOff);

    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetTrickMode(HAL_AUDIO_ADEC_INDEX_T adecIndex, adec_trick_mode_ext_type_t eTrickMode)
{
    HAL_AUDIO_RESOURCE_T curResourceId;
    HAL_AUDIO_RESOURCE_T decIptResId;// dec input source
    UINT32 flow_state;
    FlowManager* pFlow = NULL;

    AUDIO_DEBUG("%s ADEC%d mode %d\n", __FUNCTION__, adecIndex, eTrickMode);

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(!IsValidAdecIdx(adecIndex))
    {
        AUDIO_ERROR("ADEC index error  %d \n", adecIndex);
        return NOT_OK;
    }

    if(!IsAdecOpen(adecIndex))
        return NOT_OK;

    if(eTrickMode > ADEC_TRICK_FAST_FORWARD_2P00X)
    {
        AUDIO_ERROR("trick mode  error %d \n", eTrickMode);
        return NOT_OK;
    }

    curResourceId = adec2res(adecIndex);
    decIptResId = GetNonOutputModuleSingleInputResource(ResourceStatus[curResourceId]);// dec input source

    if((eTrickMode == ADEC_TRICK_PAUSE) && IsDTVSource(decIptResId))
    {
        //SetTrickPause(adecIndex, TRUE);
        int atpindex = decIptResId - HAL_AUDIO_RESOURCE_ATP0;
        rdvb_dmx_ctrl_privateInfo(DMX_PRIV_CMD_AUDIO_PAUSE, PIN_TYPE_ATP, atpindex, 0);
    }
    else
    {
        //SetTrickPause(adecIndex, FALSE);
    }

    pFlow = Aud_flow[adecIndex];
    flow_state = pFlow->GetState(pFlow);

    if(GetTrickState(adecIndex) != eTrickMode) // mode change
    {
        if(eTrickMode == ADEC_TRICK_PAUSE)
        {
            //if(flow_state == STATE_RUN)
                //pFlow->Pause(pFlow);
        }
        else if((eTrickMode == ADEC_TRICK_NONE) || (eTrickMode == ADEC_TRICK_NORMAL_PLAY))
        {
            //if(flow_state == STATE_PAUSE)
                //pFlow->Run(pFlow);//resume
            //pFlow->SetRate(pFlow, Convert2SpeedRate(eTrickMode));
        }
        else
        {
            //if((flow_state == STATE_RUN) || (flow_state == STATE_PAUSE))
                //pFlow->SetRate(pFlow, Convert2SpeedRate(eTrickMode));

            //if(flow_state == STATE_PAUSE)
                //pFlow->Run(pFlow);//resume
        }
    }

    SetTrickState(adecIndex, eTrickMode);

    return OK;
}

static void GetRingBufferInfo(RINGBUFFER_HEADER* pRingBuffer, unsigned long* pbase,
    unsigned long* prp, unsigned long* pwp , unsigned long* psize
    )
{
    unsigned long base, rp, wp, size;

    AUDIO_DEBUG("ring buffer %x  \n", (UINT32)pRingBuffer);
    if(pRingBuffer == NULL)
        return;

    rp   = IPC_ReadU32((BYTE*) &(pRingBuffer->readPtr[0]));
    base = IPC_ReadU32((BYTE*) &(pRingBuffer->beginAddr));
    wp   = IPC_ReadU32((BYTE*) &(pRingBuffer->writePtr));
    size  = IPC_ReadU32((BYTE*) &(pRingBuffer->size));
    AUDIO_DEBUG("TP buffer baddr %x rp %x wp %x size %x \n",  (UINT32)base, (UINT32)rp, (UINT32)wp, (UINT32)size);

    if(pbase)
        *pbase = base;

    if(pwp)
        *pwp = wp;

    if(prp)
        *prp = rp;

    if(psize)
        *psize = size;

}

DTV_STATUS_T HAL_AUDIO_TP_GetBufferStatus(HAL_AUDIO_ADEC_INDEX_T adecIndex, UINT32 *pMaxSize, UINT32 *pFreeSize)
{
    HAL_AUDIO_RESOURCE_T curResourceId, decIptResId, atpIptResId;
    unsigned long base , rp , wp, size;
    DTV_STATUS_T retValue = NOT_OK;

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    curResourceId = adec2res(adecIndex);
    decIptResId = GetNonOutputModuleSingleInputResource(ResourceStatus[curResourceId]);
    if(IsDTVSource(decIptResId))
    {
        atpIptResId = GetNonOutputModuleSingleInputResource(ResourceStatus[decIptResId]);

        if((atpIptResId == HAL_AUDIO_RESOURCE_SDEC0) || (atpIptResId == HAL_AUDIO_RESOURCE_SDEC1))
        {
            int atpindex = decIptResId - HAL_AUDIO_RESOURCE_ATP0;
            RINGBUFFER_HEADER* pRingBufferHeader;
            DtvCom* pDtvCom = (DtvCom*)Aud_DTV[atpindex]->pDerivedObj;

            pRingBufferHeader = (RINGBUFFER_HEADER*)(pDtvCom->GetBSRingBufVirAddress(Aud_DTV[atpindex]));
            GetRingBufferInfo(pRingBufferHeader, &base, &rp, &wp, &size);

            if(wp >= rp)
            {
                if(pFreeSize)
                    *pFreeSize = size - (wp - rp) - 4; // for 4 byte align
            }
            else
            {
                if(pFreeSize)
                    *pFreeSize = (rp - wp) - 4 ; // for 4 byte align
            }

            if(pMaxSize)
                *pMaxSize = size;

            retValue =  OK;
        }
    }
    return retValue;
}

/* AC-4 decoder */
/* AC-4 Auto Presenetation Selection. Refer to "Selection using system-level preferences" of "Dolby MS12 Multistream Decoder Implementation integration manual" */
static int IsValidLang(HAL_AUDIO_LANG_CODE_TYPE_T enCodeType, UINT32 lang)
{
    UINT8 ch_1, ch_2, ch_3, ch_4;

    if (enCodeType != HAL_AUDIO_LANG_CODE_ISO639_1 &&
        enCodeType != HAL_AUDIO_LANG_CODE_ISO639_2)
        return FALSE;

    ch_1 = (lang & 0xFF000000) >> 24;
    ch_2 = (lang & 0x00FF0000) >> 16;
    ch_3 = (lang & 0x0000FF00) >> 8;
    ch_4 = (lang & 0x000000FF);

    if ((ch_1 < 0x61) || (ch_1 > 0x7a)) return FALSE;
    if ((ch_2 < 0x61) || (ch_2 > 0x7a)) return FALSE;
    if (enCodeType == HAL_AUDIO_LANG_CODE_ISO639_1 && ch_3 != 0) return FALSE;
    else if (enCodeType == HAL_AUDIO_LANG_CODE_ISO639_2 &&
            ((ch_3 < 0x61) || (ch_3 > 0x7a)))
        return FALSE;
    if (ch_4 != 0) return FALSE;

    return TRUE;
}

DTV_STATUS_T HAL_AUDIO_AC4_SetAutoPresentationFirstLanguage(HAL_AUDIO_ADEC_INDEX_T adecIndex, HAL_AUDIO_LANG_CODE_TYPE_T enCodeType, UINT32 firstLang)
{
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(!IsValidAdecIdx(adecIndex))
        return NOT_OK;

    if(!IsAdecOpen(adecIndex))
        return NOT_OK;

    if (!IsValidLang(enCodeType, firstLang))
        return NOT_OK;

    memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));

    audioConfig.msg_id = AUDIO_CONFIG_CMD_AC4_FIRST_LANGUAGE;
    audioConfig.value[0] = (u_int)firstLang;
    audioConfig.value[1] = (u_int)enCodeType;

    if(SendRPC_AudioConfig(&audioConfig) != S_OK) return NOT_OK;

    return OK;
}

DTV_STATUS_T HAL_AUDIO_AC4_SetAutoPresentationSecondLanguage(HAL_AUDIO_ADEC_INDEX_T adecIndex, HAL_AUDIO_LANG_CODE_TYPE_T enCodeType, UINT32 secondLang)
{
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(!IsValidAdecIdx(adecIndex))
        return NOT_OK;

    if(!IsAdecOpen(adecIndex))
        return NOT_OK;

    if (!IsValidLang(enCodeType, secondLang))
        return NOT_OK;

    memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));

    audioConfig.msg_id = AUDIO_CONFIG_CMD_AC4_SECOND_LANGUAGE;
    audioConfig.value[0] = (u_int)secondLang;
    audioConfig.value[1] = (u_int)enCodeType;

    if(SendRPC_AudioConfig(&audioConfig) != S_OK) return NOT_OK;

    return OK;
}

DTV_STATUS_T HAL_AUDIO_AC4_SetAutoPresentationADMixing(HAL_AUDIO_ADEC_INDEX_T adecIndex, BOOLEAN bIsEnable)
{
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(!IsValidAdecIdx(adecIndex))
        return NOT_OK;

    if(!IsAdecOpen(adecIndex))
        return NOT_OK;

    memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));

    audioConfig.msg_id = AUDIO_CONFIG_CMD_AC4_ADMIXING;
    audioConfig.value[0] = (u_int)bIsEnable;

    if(SendRPC_AudioConfig(&audioConfig) != S_OK) return NOT_OK;

    return OK;
}

DTV_STATUS_T HAL_AUDIO_AC4_SetAutoPresentationADType(HAL_AUDIO_ADEC_INDEX_T adecIndex, adec_ac4_ad_ext_type_t enADType)
{
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(!IsValidAdecIdx(adecIndex))
        return NOT_OK;

    if(!IsAdecOpen(adecIndex))
        return NOT_OK;

    if(enADType < ADEC_AC4_AD_TYPE_NONE || enADType > ADEC_AC4_AD_TYPE_VO)
        return NOT_OK;

    memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));

    audioConfig.msg_id = AUDIO_CONFIG_CMD_AC4_ADTYPE;
    audioConfig.value[0] = (u_int)enADType;

    if(SendRPC_AudioConfig(&audioConfig) != S_OK) return NOT_OK;
    return OK;
}

DTV_STATUS_T HAL_AUDIO_AC4_SetAutoPresentationPrioritizeADType(HAL_AUDIO_ADEC_INDEX_T adecIndex, BOOLEAN bIsEnable)
{
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(!IsValidAdecIdx(adecIndex))
        return NOT_OK;

    if(!IsAdecOpen(adecIndex))
        return NOT_OK;

    memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));


    audioConfig.msg_id = AUDIO_CONFIG_CMD_AC4_PRIORITIZE_ADTYPE;
    audioConfig.value[0] = (u_int)bIsEnable;

    if(SendRPC_AudioConfig(&audioConfig) != S_OK) return NOT_OK;
    return OK;
}

/* AC-4 Dialogue Enhancement */
DTV_STATUS_T HAL_AUDIO_AC4_SetDialogueEnhancementGain(HAL_AUDIO_ADEC_INDEX_T adecIndex, UINT32 dialEnhanceGain)
{
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(!IsValidAdecIdx(adecIndex))
        return NOT_OK;

    //if(!IsAdecOpen(adecIndex))
    //    return NOT_OK;

    if(dialEnhanceGain < 0 || dialEnhanceGain > 12)
        return NOT_OK;

    memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));

    audioConfig.msg_id = AUDIO_CONFIG_CMD_AC4_DIALOGUE_ENHANCEMENT_GAIN;
    audioConfig.value[0] = (UINT32)dialEnhanceGain;

    if(SendRPC_AudioConfig(&audioConfig) != S_OK) return NOT_OK;
    return OK;
}

DTV_STATUS_T HAL_AUDIO_AC4_SetPresentationGroupIndex(HAL_AUDIO_ADEC_INDEX_T adecIndex, UINT32 pres_group_idx)
{
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(!IsValidAdecIdx(adecIndex))
        return NOT_OK;

    if(!IsAdecOpen(adecIndex))
        return NOT_OK;

    memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));


    audioConfig.msg_id = AUDIO_CONFIG_CMD_AC4_PRESENTATION_GROUP_INDEX;
    audioConfig.value[0] = (UINT32)adecIndex;
    audioConfig.value[1] = (UINT32)pres_group_idx;

    if(SendRPC_AudioConfig(&audioConfig) != S_OK) return NOT_OK;
    return OK;
}

/* Volume, Mute & Delay */
DTV_STATUS_T HAL_AUDIO_SetDecoderInputGain(HAL_AUDIO_ADEC_INDEX_T adecIndex, HAL_AUDIO_VOLUME_T volume, UINT32 gain)
{
    int ch_vol[AUD_MAX_CHANNEL];

    if(!RangeCheck(adecIndex, 0, AUD_ADEC_MAX)) return NOT_OK;
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;

    AUDIO_INFO("%s ADEC%d = main %d fine %d \n", __FUNCTION__, adecIndex, volume.mainVol, volume.fineVol);

#ifdef AVOID_USELESS_RPC
    if (Volume_Compare(volume, GetDecInVolume(adecIndex)))
    {
        AUDIO_DEBUG("skip ADEC%d  volume %d ==  GetDecInVolume %d \n",
                adecIndex, volume.mainVol, (((HAL_AUDIO_VOLUME_T)GetDecInVolume(adecIndex)).mainVol));
        adec_status[adecIndex].Gain = gain;
        return OK;
    }
#endif

    SetDecInVolume(adecIndex, volume);

    if(IsDecoderConnectedToEncoder(adecIndex))
    {
        //Skip the user volume setting to fw in AENC flow
        AUDIO_DEBUG("Skip the user volume [main, fine]=[%d, %d] in AENC flow.", volume.mainVol, volume.fineVol);
    }
    else
    {
        ADEC_Calculate_DSPGain(adecIndex, ch_vol);
        if(ADSP_DEC_SetFineVolume(adecIndex, ch_vol, volume.fineVol) != S_OK) return NOT_OK;
    }

    adec_status[adecIndex].Gain = gain;
    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetDecoderDelayTime(HAL_AUDIO_ADEC_INDEX_T adecIndex, UINT32 delayTime)
{
    AUDIO_INFO("%s, %d, %d\n", __FUNCTION__, adecIndex, delayTime);
    if(!RangeCheck(adecIndex, 0, AUD_ADEC_MAX)) return NOT_OK;
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;

    if(ADSP_DEC_SetDelay(adecIndex, delayTime) != S_OK) return NOT_OK;
    gAudioDecInfo[adecIndex].decDelay = delayTime;
    adec_status[adecIndex].Delay = delayTime;

    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetMixerInputGain(HAL_AUDIO_MIXER_INDEX_T mixerIndex, HAL_AUDIO_VOLUME_T volume)
{
    SINT32 dsp_gain = Volume_to_DSPGain(Volume_Add(volume, g_mixer_out_gain));

    UINT32 i;
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;

    if(!RangeCheck(mixerIndex, 0, AUD_AMIX_MAX)) return NOT_OK;
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;
    AUDIO_INFO("%s AMIXER%d = main %d fine %d \n", __FUNCTION__,
            mixerIndex, volume.mainVol, volume.fineVol);

    memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));

    audioConfig.msg_id = AUDIO_CONFIG_CMD_SET_VOLUME;
    audioConfig.value[0] = ((PPAO*)Aud_ppAout->pDerivedObj)->GetAOAgentID(Aud_ppAout);
    audioConfig.value[1] = ENUM_DEVICE_FLASH_PIN;
    audioConfig.value[2] = (UINT32)mixerIndex;
    audioConfig.value[3] = 0xFF;
    for(i = 0; i < AUD_MAX_CHANNEL; i++)
    {
        audioConfig.value[4+i] = dsp_gain;
    }

    if(SendRPC_AudioConfig(&audioConfig) != S_OK) return NOT_OK;

    g_mixer_gain[mixerIndex] = volume;
    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetSPKOutVolume(HAL_AUDIO_VOLUME_T volume, UINT32 gain)//PB
{
    SINT32 dsp_gain = Volume_to_DSPGain(volume);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;
    AUDIO_INFO("%s volume = main %d fine %d \n", __FUNCTION__, volume.mainVol, volume.fineVol);

#ifdef AVOID_USELESS_RPC
    if (Volume_Compare(volume, g_AudioStatusInfo.curSPKOutVolume))
    {
        AUDIO_DEBUG("%s Skip volume = main %d fine %d \n", __FUNCTION__, volume.mainVol, volume.fineVol);
        return OK;
    }
#endif

    g_AudioStatusInfo.curSPKOutVolume.mainVol = volume.mainVol;
    g_AudioStatusInfo.curSPKOutVolume.fineVol = volume.fineVol;
    g_AudioStatusInfo.curSPKOutGain = gain;
    AUDIO_DEBUG("Spk volume index = %d \n", dsp_gain);
    if(ADSP_SNDOut_SetFineVolume(ENUM_DEVICE_SPEAKER, dsp_gain, volume.fineVol) != S_OK)
        return NOT_OK;

    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetSPDIFOutVolume(HAL_AUDIO_VOLUME_T volume, UINT32 gain)//TS
{
    SINT32 dsp_gain = Volume_to_DSPGain(volume);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;
    AUDIO_INFO("%s volume = main %d fine %d \n", __FUNCTION__, volume.mainVol, volume.fineVol);

#ifdef AVOID_USELESS_RPC
    if (Volume_Compare(volume, g_AudioStatusInfo.curSPDIFOutVolume))
    {
        AUDIO_DEBUG("%s Skip volume = main %d fine %d \n", __FUNCTION__, volume.mainVol, volume.fineVol);
        return OK;
    }
#endif

    g_AudioStatusInfo.curSPDIFOutVolume.mainVol = volume.mainVol;
    g_AudioStatusInfo.curSPDIFOutVolume.fineVol = volume.fineVol;
    g_AudioStatusInfo.curSPDIFOutGain = gain;
    if(!ADEC_ARC_CheckConnect()) {
        if(ADSP_SNDOut_SetFineVolume(ENUM_DEVICE_SPDIF, dsp_gain, volume.fineVol) != S_OK)
            return NOT_OK;
    }

    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetHPOutVolume(HAL_AUDIO_VOLUME_T volume, BOOLEAN bForced, UINT32 gain)
{
    SINT32 dsp_gain = Volume_to_DSPGain(volume);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;
    AUDIO_INFO("%s volume = main %d fine %d \n", __FUNCTION__, volume.mainVol, volume.fineVol);

#ifdef AVOID_USELESS_RPC
    if (Volume_Compare(volume, g_AudioStatusInfo.curHPOutVolume))
    {
        AUDIO_DEBUG("%s Skip volume = main %d fine %d \n", __FUNCTION__, volume.mainVol, volume.fineVol);
        return OK;
    }
#endif

    g_AudioStatusInfo.curHPOutVolume.mainVol = volume.mainVol;
    g_AudioStatusInfo.curHPOutVolume.fineVol = volume.fineVol;
    g_AudioStatusInfo.curHPOutGain = gain;
    if(ADSP_SNDOut_SetFineVolume(ENUM_DEVICE_HEADPHONE, dsp_gain, volume.fineVol) != S_OK)
        return NOT_OK;

    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetAudioDescriptionVolume(HAL_AUDIO_ADEC_INDEX_T adecIndex, HAL_AUDIO_VOLUME_T volume)
{
    int i, ch_vol[AUD_MAX_CHANNEL];
    HAL_AUDIO_VOLUME_T volume_0dB = {.mainVol=0x7F, .fineVol=0x0};

    AUDIO_INFO("%s volume = main %d fine %d \n", __FUNCTION__, volume.mainVol, volume.fineVol);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;

    if(Aud_descriptionMode == FALSE)
    {
        AUDIO_ERROR("Audio Descriptioin mode is OFF, save the setting\n");
        currDecADVol = volume;
        return OK;
    }

    currMixADVol = volume_0dB;
    currDecADVol = volume;
    AUDIO_DEBUG("%s currMixADVol = 0x%x currDecADVol = 0x%x \n", __FUNCTION__, currMixADVol.mainVol, currDecADVol.mainVol);
    ADSP_PPMix_ConfigMixer(Aud_descriptionMode, Volume_to_MixerGain(currMixADVol));

    for(i = 0; i < AUD_ADEC_MAX; i++)
    {
        ADEC_Calculate_DSPGain(i, ch_vol);
        ADSP_DEC_SetVolume(i, ch_vol);
    }

    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetDecoderInputMute(HAL_AUDIO_ADEC_INDEX_T adecIndex, BOOLEAN bOnOff)
{
    AUDIO_INFO("%s ADEC%d onoff=%d\n", __FUNCTION__, adecIndex, bOnOff);
    if(!RangeCheck(adecIndex, 0, AUD_ADEC_MAX)) return NOT_OK;
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;

    if(GetTrickState(adecIndex) == HAL_AUDIO_TRICK_PAUSE)
    {
        AUDIO_DEBUG("Skip mute at pause state \n");// pause play will have cutting sound
        return OK;
    }

    if(IsDecoderConnectedToEncoder(adecIndex))
    {
        //Skip the user mute status to fw in AENC flow
        AUDIO_DEBUG("Skip the user mute status [%s] in AENC flow.", bOnOff? "ON":"OFF");
    }
    else
    {
        if(ADSP_DEC_SetMute(adecIndex, ADEC_CheckConnect(adecIndex), bOnOff) != S_OK)
            return NOT_OK;
    }

    SetDecInMute(adecIndex, (UINT32)bOnOff);

    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetMixerInputMute(HAL_AUDIO_MIXER_INDEX_T mixerIndex, BOOLEAN bOnOff)
{
    HAL_AUDIO_RESOURCE_T resID= amixer2res(mixerIndex);

    AUDIO_INFO("%s AMIXER%d onoff=%d \n", __FUNCTION__, mixerIndex, bOnOff);
    if(!RangeCheck(mixerIndex, 0, AUD_AMIX_MAX)) return NOT_OK;
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;

    SetAmixerUserMute(mixerIndex, (UINT32)bOnOff);

    ADSP_AMIXER_SetMute(mixerIndex, AMIXER_CheckConnect(resID), GetAmixerUserMute(mixerIndex));
    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetSPKOutMute(BOOLEAN bOnOff)
{
    AUDIO_INFO("%s   onoff=%d curr %d \n", __FUNCTION__, bOnOff, g_AudioStatusInfo.curSPKMuteStatus);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;

    g_AudioStatusInfo.curSPKMuteStatus = bOnOff;
    if(SNDOut_SetMute(ENUM_DEVICE_SPEAKER, ADEC_SPK_CheckConnect(), bOnOff) != S_OK)
        return NOT_OK;

    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetSPDIFOutMute(BOOLEAN bOnOff, UINT32 output_device)
{
    UINT32 res = S_OK;
    AUDIO_INFO("%s   onoff=%d \n", __FUNCTION__, bOnOff);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;

    if(output_device == COMMON_OPTIC)
        g_AudioStatusInfo.curSPDIFMuteStatus = bOnOff;
    else if (output_device == COMMON_OPTIC_LG)
        g_AudioStatusInfo.curSPDIF_LG_MuteStatus = bOnOff;
    if(!ADEC_ARC_CheckConnect()) {
        if(output_device == COMMON_OPTIC && ADEC_SPDIF_CheckConnect()){
            res = SNDOut_SetMute(ENUM_DEVICE_SPDIF, ADEC_SPDIF_CheckConnect(), bOnOff);
            res |= SNDOut_SetMute(ENUM_DEVICE_SPDIF_ES, ADEC_SPDIF_CheckConnect(), bOnOff);
            if(res != S_OK) return NOT_OK;
        }
        if(output_device == COMMON_OPTIC_LG && ADEC_SB_SPDIF_CheckConnect()){
            res = SNDOut_SetMute(ENUM_DEVICE_SPDIF, ADEC_SB_SPDIF_CheckConnect(), bOnOff);
            res |= SNDOut_SetMute(ENUM_DEVICE_SPDIF_ES, ADEC_SB_SPDIF_CheckConnect(), bOnOff);
            if(res != S_OK) return NOT_OK;
        }
    }

    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetHPOutMute(BOOLEAN bOnOff)
{
    AUDIO_INFO("%s   onoff=%d \n", __FUNCTION__, bOnOff);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;

    g_AudioStatusInfo.curHPMuteStatus = bOnOff;
    if(SNDOut_SetMute(ENUM_DEVICE_HEADPHONE, ADEC_HP_CheckConnect(), bOnOff) != S_OK)
        return NOT_OK;

    return OK;
}

DTV_STATUS_T HAL_AUDIO_GetSPKOutMuteStatus(BOOLEAN *pOnOff)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;
    if(pOnOff == NULL) return NOT_OK;

    *pOnOff = g_AudioStatusInfo.curSPKMuteStatus;
    return OK;
}

DTV_STATUS_T HAL_AUDIO_GetSPDIFOutMuteStatus(BOOLEAN *pOnOff)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;
    if(pOnOff == NULL) return NOT_OK;

    *pOnOff = g_AudioStatusInfo.curSPDIFMuteStatus;
    return OK;
}

DTV_STATUS_T HAL_AUDIO_GetHPOutMuteStatus(BOOLEAN *pOnOff)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;
    if(pOnOff == NULL) return NOT_OK;

    *pOnOff = g_AudioStatusInfo.curHPMuteStatus;
    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetSPKOutDelayTime(UINT32 delayTime, BOOLEAN bForced)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;

    g_AudioStatusInfo.curSPKOutDelay = delayTime;
    if(SNDOut_SetDelay(ENUM_DEVICE_SPEAKER, delayTime) != S_OK)
        return NOT_OK;

    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetSPDIFOutDelayTime(UINT32 delayTime, BOOLEAN bForced)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;

    g_AudioStatusInfo.curSPDIFOutDelay = delayTime;
    if(!ADEC_ARC_CheckConnect() && ADEC_SPDIF_CheckConnect()) {
        if(SNDOut_SetDelay(ENUM_DEVICE_SPDIF, delayTime) != S_OK)
            return NOT_OK;
    }

    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetSPDIFSBOutDelayTime(UINT32 delayTime, BOOLEAN bForced)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;

    g_AudioStatusInfo.curSPDIFSBOutDelay = delayTime;
    if(!ADEC_ARC_CheckConnect() && ADEC_SB_SPDIF_CheckConnect()) {
        if(SNDOut_SetDelay(ENUM_DEVICE_SPDIF, delayTime) != S_OK)
            return NOT_OK;
    }

    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetHPOutDelayTime(UINT32 delayTime, BOOLEAN bForced)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;

    g_AudioStatusInfo.curHPOutDelay = delayTime;
    if(SNDOut_SetDelay(ENUM_DEVICE_HEADPHONE, delayTime) != S_OK)
        return NOT_OK;

    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetInputOutputDelay(HAL_AUDIO_INPUT_DELAY_PARAM_T inputParam, HAL_AUDIO_OUTPUT_DELAY_PARAM_T outputParam)
{
    HAL_AUDIO_RESOURCE_T curResourceId = adec2res(inputParam.adecIndex);
    HAL_AUDIO_RESOURCE_T decIptResId = GetNonOutputModuleSingleInputResource(ResourceStatus[curResourceId]);

    int delay_change_flush = FALSE;

    AUDIO_INFO("%s   input = (%d,%d), output = (%d,%d)\n",
        __FUNCTION__, inputParam.adecIndex, inputParam.delayTime, outputParam.soundOutType, outputParam.delayTime);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;

    if(!RangeCheck(inputParam.adecIndex, 0, AUD_ADEC_MAX)) {
        AUDIO_ERROR("input adec index output of range(%d)\n", inputParam.adecIndex);
        return NOT_OK;
    }

    //input delay
    if(gAudioDecInfo[inputParam.adecIndex].decDelay != inputParam.delayTime)
    {
        if(ADSP_DEC_SetDelay(inputParam.adecIndex, inputParam.delayTime) != S_OK)   return NOT_OK;
        gAudioDecInfo[inputParam.adecIndex].decDelay = inputParam.delayTime;
        AUDIO_DEBUG("apply InputDelay: adecIdex %d delay %d \n", inputParam.adecIndex, inputParam.delayTime);
        delay_change_flush++;
        adec_status[inputParam.adecIndex].Delay = inputParam.delayTime;
    }

    //output delay
    switch(outputParam.soundOutType)
    {
        case HAL_AUDIO_NO_OUTPUT:
            break;
        case HAL_AUDIO_SPK:
            if(outputParam.delayTime != g_AudioStatusInfo.curSPKOutDelay)
                delay_change_flush++;
            g_AudioStatusInfo.curSPKOutDelay = outputParam.delayTime;
            if(SNDOut_SetDelay(ENUM_DEVICE_SPEAKER, outputParam.delayTime) != S_OK)   return NOT_OK;
            AUDIO_DEBUG("apply OutputDelay: SPK,  delay %d \n", outputParam.delayTime);
            break;
        case HAL_AUDIO_SB_SPDIF:
            if(outputParam.delayTime != g_AudioStatusInfo.curSPDIFSBOutDelay)
                delay_change_flush++;
            g_AudioStatusInfo.curSPDIFSBOutDelay = outputParam.delayTime;
            if(!ADEC_ARC_CheckConnect() && ADEC_SB_SPDIF_CheckConnect()) {
                if(SNDOut_SetDelay(ENUM_DEVICE_SPDIF, outputParam.delayTime) != S_OK)   return NOT_OK;
                AUDIO_DEBUG("apply OutputDelay: SB_SPDIF,  delay %d \n", outputParam.delayTime);
            }
            break;
        case HAL_AUDIO_SPDIF:
            if(outputParam.delayTime != g_AudioStatusInfo.curSPDIFOutDelay)
                delay_change_flush++;
            g_AudioStatusInfo.curSPDIFOutDelay = outputParam.delayTime;
            if(!ADEC_ARC_CheckConnect() && ADEC_SPDIF_CheckConnect()) {
                if(SNDOut_SetDelay(ENUM_DEVICE_SPDIF, outputParam.delayTime) != S_OK)   return NOT_OK;
                AUDIO_DEBUG("apply OutputDelay: SPDIF,  delay %d \n", outputParam.delayTime);
            }
            break;
        case HAL_AUDIO_HP:
            if(outputParam.delayTime != g_AudioStatusInfo.curHPOutDelay)
                delay_change_flush++;
            g_AudioStatusInfo.curHPOutDelay = outputParam.delayTime;
            if(SNDOut_SetDelay(ENUM_DEVICE_HEADPHONE, outputParam.delayTime) != S_OK)   return NOT_OK;
            AUDIO_DEBUG("apply OutputDelay: HP,  delay %d \n", outputParam.delayTime);
            break;
        case HAL_AUDIO_ARC:
            if(outputParam.delayTime != g_ARCStatusInfo.curARCOutDelay)
                delay_change_flush++;
            g_ARCStatusInfo.curARCOutDelay = outputParam.delayTime;
            if(ADEC_ARC_CheckConnect()) {
                if(SNDOut_SetDelay(ENUM_DEVICE_SPDIF, outputParam.delayTime) != S_OK) return NOT_OK;
                AUDIO_DEBUG("apply OutputDelay: ARC,  delay %d \n", outputParam.delayTime);
            }
            break;
        case HAL_AUDIO_SB_PCM: //BT
            g_AudioStatusInfo.curBTOutDelay = outputParam.delayTime;
            break;
        case HAL_AUDIO_WISA:
            g_AudioStatusInfo.curWISAOutDelay = outputParam.delayTime;
            break;
        case HAL_AUDIO_SE_BT:
            g_AudioStatusInfo.curSEBTOutDelay = outputParam.delayTime;
            break;
        default:
            AUDIO_INFO("%s NOT support! Need to check soundOutType %d Delay %d \n", __FUNCTION__, outputParam.soundOutType, inputParam.delayTime);
            break;
    }

    if(delay_change_flush >=2)
    {
        switch(outputParam.soundOutType)
        {
        case HAL_AUDIO_SPK:
            //SNDOut_SetMute(ENUM_DEVICE_SPEAKER, ADEC_SPK_CheckConnect(), TRUE);
            Switch_OnOff_MuteProtect(50);
            break;
        case HAL_AUDIO_SB_SPDIF:
        case HAL_AUDIO_SPDIF:
            SNDOut_SetMute(ENUM_DEVICE_SPDIF,    (ADEC_SPDIF_CheckConnect() || ADEC_SB_SPDIF_CheckConnect()), TRUE);
            SNDOut_SetMute(ENUM_DEVICE_SPDIF_ES, (ADEC_SPDIF_CheckConnect() || ADEC_SB_SPDIF_CheckConnect()), TRUE);
            break;
        case HAL_AUDIO_ARC:
            SNDOut_SetMute(ENUM_DEVICE_SPDIF,    ADEC_ARC_CheckConnect(), TRUE);
            SNDOut_SetMute(ENUM_DEVICE_SPDIF_ES, ADEC_ARC_CheckConnect(), TRUE);
            break;
        case HAL_AUDIO_HP:
            SNDOut_SetMute(ENUM_DEVICE_HEADPHONE, ADEC_HP_CheckConnect(), TRUE);
            break;
        case HAL_AUDIO_SB_PCM:
        case HAL_AUDIO_SE_BT:
        case HAL_AUDIO_WISA:
        default:
            break;
        }
        msleep(10);
        if(IsDTVSource(decIptResId)) {
            AUDIO_DEBUG("delay_change_flush ! \n");
            ((PPAO*)Aud_ppAout->pDerivedObj)->AO_Flush(Aud_ppAout);
        }
        msleep(10);
        switch(outputParam.soundOutType)
        {
        case HAL_AUDIO_SPK:
            //SNDOut_SetMute(ENUM_DEVICE_SPEAKER, ADEC_SPK_CheckConnect(), g_AudioStatusInfo.curSPKMuteStatus);
            break;
        case HAL_AUDIO_SB_SPDIF:
            SNDOut_SetMute(ENUM_DEVICE_SPDIF,    ADEC_SB_SPDIF_CheckConnect(), g_AudioStatusInfo.curSPDIF_LG_MuteStatus);
            SNDOut_SetMute(ENUM_DEVICE_SPDIF_ES, ADEC_SB_SPDIF_CheckConnect(), g_AudioStatusInfo.curSPDIF_LG_MuteStatus);
            break;
        case HAL_AUDIO_SPDIF:
            SNDOut_SetMute(ENUM_DEVICE_SPDIF,    ADEC_SPDIF_CheckConnect(), g_AudioStatusInfo.curSPDIFMuteStatus);
            SNDOut_SetMute(ENUM_DEVICE_SPDIF_ES, ADEC_SPDIF_CheckConnect(), g_AudioStatusInfo.curSPDIFMuteStatus);
            break;
        case HAL_AUDIO_ARC:
            SNDOut_SetMute(ENUM_DEVICE_SPDIF,    ADEC_ARC_CheckConnect(), g_ARCStatusInfo.curARCMuteStatus);
            SNDOut_SetMute(ENUM_DEVICE_SPDIF_ES, ADEC_ARC_CheckConnect(), g_ARCStatusInfo.curARCMuteStatus);
            break;
        case HAL_AUDIO_HP:
            SNDOut_SetMute(ENUM_DEVICE_HEADPHONE, ADEC_HP_CheckConnect(), g_AudioStatusInfo.curHPMuteStatus);
            break;
        case HAL_AUDIO_SB_PCM:
        case HAL_AUDIO_SE_BT:
        case HAL_AUDIO_WISA:
        default:
            break;
        }
    }
    return OK;
}

DTV_STATUS_T RHAL_AUDIO_GetStatusInfo(HAL_AUDIO_COMMON_INFO_T *pAudioStatusInfo)
{
    KADP_AO_SPDIF_CHANNEL_STATUS_BASIC sc;

    AUDIO_DEBUG( "%s \n", __FUNCTION__);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;
    if(pAudioStatusInfo == NULL) return NOT_OK;

    if(g_AudioStatusInfo.curAudioSpdifMode == HAL_AUDIO_SPDIF_PCM)
        g_AudioStatusInfo.bCurAudioSpdifOutPCM  = TRUE;
    else
        g_AudioStatusInfo.bCurAudioSpdifOutPCM  = FALSE;

    //GetAudioSpdifChannelStatus(&sc, AUDIO_OUT);
    memset(&sc, 0, sizeof(KADP_AO_SPDIF_CHANNEL_STATUS_BASIC));
    if (KADP_AUDIO_GetAudioSpdifChannelStatus(&sc, AUDIO_OUT))
    {
        AUDIO_ERROR("[%s,%d] get spdif channel status failed\n", __FUNCTION__, __LINE__);
        return NOT_OK;
    }

    if(sc.data_type == 0) // pcm
        g_AudioStatusInfo.bAudioSpdifOutPCM  = TRUE;
    else
        g_AudioStatusInfo.bCurAudioSpdifOutPCM  = FALSE;

    memcpy(pAudioStatusInfo, &g_AudioStatusInfo, sizeof(HAL_AUDIO_COMMON_INFO_T));
    return OK;
}

DTV_STATUS_T HAL_AUDIO_GetStatusInfo(HAL_AUDIO_COMMON_INFO_T *pAudioStatusInfo)
{
    return RHAL_AUDIO_GetStatusInfo(pAudioStatusInfo);
}

char* SPDIFMODEString[]=
{
    "NONE",
    "PCM",
    "AUTO",
    "AUTO_AAC",
    "HALF_AUTO",
    "HALF_AUTO_AAC",
    "FORCED_AC3_5_1",
    "BYPASS",
    "BYPASS_AAC",
    "Unknown"
};
char* GetSpdifModeName(sndout_optic_mode_ext_type_t index)
{
   int id;
   id = (int)index;
   if((int)index >= (sizeof(SPDIFMODEString)/sizeof(SPDIFMODEString[0])))
   {
       AUDIO_ERROR("unknown spidf mode %d \n", index);
       id = (sizeof(SPDIFMODEString)/sizeof(SPDIFMODEString[0])) - 1;// unknown
   }
   return SPDIFMODEString[id];
}

/* SPDIF(Sound Bar) */
DTV_STATUS_T HAL_AUDIO_SPDIF_SetOutputType(sndout_optic_mode_ext_type_t eSPDIFMode, BOOLEAN bForced)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    AUDIO_INFO("%s  mode %d (%s) isForced (%d)\n", __FUNCTION__, (int)eSPDIFMode ,GetSpdifModeName(eSPDIFMode), bForced);

    switch(eSPDIFMode)
    {
    case SNDOUT_OPTIC_AUTO:
    case SNDOUT_OPTIC_HALF_AUTO:
        _AudioSPDIFMode = NON_PCM_OUT_EN_AUTO; break;
    case SNDOUT_OPTIC_AUTO_AAC:
    case SNDOUT_OPTIC_HALF_AUTO_AAC:
        _AudioSPDIFMode = NON_PCM_OUT_EN_AUTO_AAC; break;
    case SNDOUT_OPTIC_FORCED_AC3_5_1:
        _AudioSPDIFMode = NON_PCM_OUT_EN_AUTO_FORCED_AC3; break;
    case SNDOUT_OPTIC_BYPASS:
        _AudioSPDIFMode = NON_PCM_OUT_EN_AUTO_BYPASS; break;
    case SNDOUT_OPTIC_BYPASS_AAC:
        _AudioSPDIFMode = NON_PCM_OUT_EN_AUTO_BYPASS_AAC; break;
    case SNDOUT_OPTIC_PCM:
    case SNDOUT_OPTIC_NONE:
        _AudioSPDIFMode = ENABLE_DOWNMIX;
        break;
    default:
        AUDIO_ERROR("error type %d \n", eSPDIFMode);
        return NOT_OK;
    }
    Update_RawMode_by_connection();

    g_AudioStatusInfo.curAudioSpdifMode = eSPDIFMode;

    return OK;
}

char* ARCMODEString[]=
{
    "NONE",
    "PCM",
    "AUTO",
    "AUTO_AAC",
    "AUTO_EAC3",
    "AUTO_EAC3_AAC",
    "HALF_AUTO",
    "HALF_AUTO_AAC",
    "HALF_AUTO_EAC3",
    "HALF_AUTO_EAC3_AAC",
    "FORCED_AC3",
    "FORCED_EAC3",
    "BYPASS",
    "BYPASS_AAC",
    "BYPASS_EAC3",
    "BYPASS_EAC3_AAC",
    "Unknown"
};
char* GetARCModeName(HAL_AUDIO_ARC_MODE_T index)
{
   int id;
   id = (int)index;
   if((int)index >= sizeof(ARCMODEString)/sizeof(ARCMODEString[0]))
   {
       AUDIO_ERROR("unknown spidf mode %d \n", index);
       id = (sizeof(ARCMODEString)/sizeof(ARCMODEString[0])) - 1;// unknown
   }
   return ARCMODEString[id];
}

// for TB24 ARC DD+ output 2016/06/4 by ykwang.kim
DTV_STATUS_T HAL_AUDIO_ARC_SetOutputType(HAL_AUDIO_ARC_MODE_T eARCMode, BOOLEAN bForced)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    AUDIO_INFO("%s  mode %d (%s) isForced (%d)\n", __FUNCTION__, (int)eARCMode ,GetARCModeName(eARCMode), bForced);

    switch(eARCMode)
    {
    case HAL_AUDIO_ARC_AUTO:
    case HAL_AUDIO_ARC_HALF_AUTO:
        _AudioARCMode = NON_PCM_OUT_EN_AUTO; break;
    case HAL_AUDIO_ARC_AUTO_AAC:
    case HAL_AUDIO_ARC_HALF_AUTO_AAC:
        _AudioARCMode = NON_PCM_OUT_EN_AUTO_AAC; break;
    case HAL_AUDIO_ARC_AUTO_EAC3:
    case HAL_AUDIO_ARC_HALF_AUTO_EAC3:
        _AudioARCMode = NON_PCM_OUT_EN_AUTO_DDP; break;
    case HAL_AUDIO_ARC_AUTO_EAC3_AAC:
    case HAL_AUDIO_ARC_HALF_AUTO_EAC3_AAC:
        _AudioARCMode = NON_PCM_OUT_EN_AUTO_DDP_AAC; break;
    case HAL_AUDIO_ARC_FORCED_AC3:
        _AudioARCMode = NON_PCM_OUT_EN_AUTO_FORCED_AC3; break;
    case HAL_AUDIO_ARC_FORCED_EAC3:
        _AudioARCMode = NON_PCM_OUT_EN_AUTO_FORCED_DDP; break;
    case HAL_AUDIO_ARC_BYPASS:
        _AudioARCMode = NON_PCM_OUT_EN_AUTO_BYPASS; break;
    case HAL_AUDIO_ARC_BYPASS_AAC:
        _AudioARCMode = NON_PCM_OUT_EN_AUTO_BYPASS_AAC; break;
    case HAL_AUDIO_ARC_BYPASS_EAC3:
        _AudioARCMode = NON_PCM_OUT_EN_AUTO_BYPASS_DDP; break;
    case HAL_AUDIO_ARC_BYPASS_EAC3_AAC:
        _AudioARCMode = NON_PCM_OUT_EN_AUTO_BYPASS_DDP_AAC; break;
    case HAL_AUDIO_ARC_PCM:
    case HAL_AUDIO_ARC_NONE:
        _AudioARCMode = ENABLE_DOWNMIX;
        break;
    default:
        AUDIO_ERROR("error type %d \n", eARCMode);
        return NOT_OK;
    }
    Update_RawMode_by_connection();

    g_ARCStatusInfo.curAudioArcMode = eARCMode;

    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetARCOutVolume(HAL_AUDIO_VOLUME_T volume, UINT32 gain)
{
    SINT32 dsp_gain = Volume_to_DSPGain(volume);

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;

    AUDIO_INFO("%s volume = main %d fine %d \n", __FUNCTION__, volume.mainVol, volume.fineVol);

#ifdef AVOID_USELESS_RPC
    if (Volume_Compare(volume, g_ARCStatusInfo.curARCOutVolume))
    {
        AUDIO_DEBUG("%s Skip volume = main %d fine %d \n", __FUNCTION__, volume.mainVol, volume.fineVol);
        return OK;
    }
#endif

    g_ARCStatusInfo.curARCOutVolume.mainVol = volume.mainVol;
    g_ARCStatusInfo.curARCOutVolume.fineVol = volume.fineVol;
    g_ARCStatusInfo.curARCOutGain = gain;
    if(ADEC_ARC_CheckConnect()) {
        if(ADSP_SNDOut_SetVolume(ENUM_DEVICE_SPDIF, dsp_gain) != S_OK)
            return NOT_OK;
    }

    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetARCOutMute(BOOLEAN bOnOff)
{
    AUDIO_INFO("%s   onoff=%d \n", __FUNCTION__, bOnOff);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;

    g_ARCStatusInfo.curARCMuteStatus = bOnOff;
    if(ADEC_ARC_CheckConnect()) {
        if(SNDOut_SetMute(ENUM_DEVICE_SPDIF, ADEC_ARC_CheckConnect(), bOnOff) != S_OK)
            return NOT_OK;
        if(SNDOut_SetMute(ENUM_DEVICE_SPDIF_ES, ADEC_ARC_CheckConnect(), bOnOff) != S_OK)
            return NOT_OK;
    }
    return OK;
}

DTV_STATUS_T HAL_AUDIO_GetARCOutMuteStatus(BOOLEAN *pOnOff)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;
    if(pOnOff == NULL) return NOT_OK;

    *pOnOff = g_ARCStatusInfo.curARCMuteStatus;
    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetARCOutDelayTime(UINT32 delayTime, BOOLEAN bForced)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;

    g_ARCStatusInfo.curARCOutDelay = delayTime;
    if(ADEC_ARC_CheckConnect()) {
        if(SNDOut_SetDelay(ENUM_DEVICE_SPDIF, delayTime) != S_OK)
            return NOT_OK;
    }

    return OK;
}

DTV_STATUS_T HAL_AUDIO_ARC_SetCopyInfo(sndout_arc_copyprotection_ext_type_t copyInfo)
{
    AUDIO_DEBUG("%s  %d   \n", __FUNCTION__, copyInfo);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(copyInfo < SNDOUT_ARC_COPY_UNKNOWN || copyInfo > SNDOUT_ARC_COPY_NEVER)
    {
        AUDIO_ERROR("error type %d \n", copyInfo);
        return NOT_OK;
    }

    g_ARCStatusInfo.curARCCopyInfo = copyInfo;

    if(ADEC_ARC_CheckConnect()) {
        ADSP_SetChannelStatus();
    }
    return OK;
}

DTV_STATUS_T HAL_AUDIO_ARC_SetCategoryCode(UINT8 categoryCode)
{
    AUDIO_DEBUG("%s  %d   \n", __FUNCTION__, categoryCode);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    categoryCode = (categoryCode & 0x7F);// category occupy 7 bits7
    g_ARCStatusInfo.curARCCategoryCode = categoryCode;

    if(ADEC_ARC_CheckConnect()) {
        ADSP_SetChannelStatus();
    }
    return OK;
}

DTV_STATUS_T HAL_AUDIO_SPDIF_SetCopyInfo(sndout_optic_copyprotection_ext_type_t copyInfo)
{
    AUDIO_DEBUG("%s  %d   \n", __FUNCTION__, copyInfo);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(copyInfo < SNDOUT_OPTIC_COPY_UNKNOWN || copyInfo > SNDOUT_OPTIC_COPY_NEVER)
    {
        AUDIO_ERROR("error type %d \n", copyInfo);
        return NOT_OK;
    }

    g_AudioStatusInfo.curSpdifCopyInfo = copyInfo;

    if(!ADEC_ARC_CheckConnect()) {
        ADSP_SetChannelStatus();
    }
    return OK;
}

DTV_STATUS_T HAL_AUDIO_SPDIF_SetCategoryCode(UINT8 categoryCode)
{
    AUDIO_DEBUG("%s  %d   \n", __FUNCTION__, categoryCode);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    categoryCode = (categoryCode & 0x7F);// category occupy 7 bits7
    g_AudioStatusInfo.curSpdifCategoryCode = categoryCode;

    if(!ADEC_ARC_CheckConnect()) {
        ADSP_SetChannelStatus();
    }
    return OK;
}

extern int rtk_spdif_enable(unsigned char en);
DTV_STATUS_T HAL_AUDIO_SPDIF_SetLightOnOff(BOOLEAN bOnOff)
{
    AUDIO_INFO("%s  %d   \n", __FUNCTION__, bOnOff);

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(bOnOff == TRUE)
        rtk_spdif_enable(TRUE);
    else
        rtk_spdif_enable(FALSE);

    if(bOnOff == TRUE)
        g_AudioStatusInfo.curSpdifLightOnOff = TRUE;
    else
        g_AudioStatusInfo.curSpdifLightOnOff = FALSE;

    return OK;
}

DTV_STATUS_T RHAL_AUDIO_SB_SetOpticalIDData(HAL_AUDIO_SB_SET_INFO_T info, unsigned int *ret_checksum)
{
    int ret = 0;
    UINT8 checkSum;

    KADP_AUDIO_SPDIF_CHANNEL_STATUS_SOUNDBAR cs;
    KADP_AUDIO_SB_GetChannelStatus(&cs);
    AUDIO_INFO("%s  \n", __FUNCTION__);

    cs.barId = info.barId&0x00FFFFFF;

    AUDIO_INFO("%s barID %x \n", __FUNCTION__, cs.barId);

    cs.OSD_Vol.OSD.bMute = info.bMuteOnOff;
    cs.OSD_Vol.OSD.volume = info.volume;

    if(info.bPowerOnOff == false)
        cs.command = 0xA1;
    else
        cs.command = info.bPowerOnOff;

    checkSum = (UINT8)cs.barId;
    checkSum ^= (UINT8)(cs.barId>>8);
    checkSum ^= (UINT8)(cs.barId>>16);
    checkSum ^= cs.OSD_Vol.bits;
    checkSum ^= cs.command;
    checkSum ^= cs.woofer_Vol.bits;
    checkSum ^= (UINT8)cs.reserved_data;
    checkSum ^= (UINT8)(cs.reserved_data>>8);
    cs.checksum = checkSum;
    *ret_checksum = (UINT32)checkSum;

    ret = KADP_AUDIO_SB_SetChannelStatus(cs);
    if(ret != S_OK){
        AUDIO_ERROR("%s fail!!!\n", __FUNCTION__);
    }

    return OK;
}

DTV_STATUS_T HAL_AUDIO_SB_SetOpticalIDData(HAL_AUDIO_SB_SET_INFO_T info, unsigned int *ret_checksum)
{
    UINT32 ori_BarID;

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
         return NOT_OK;

    ori_BarID = g_AudioStatusInfo.curSoundBarInfo.barId;

    memcpy(&g_AudioStatusInfo.curSoundBarInfo ,  &info, sizeof(HAL_AUDIO_SB_SET_INFO_T));

    AUDIO_INFO("%s barID %x \n", __FUNCTION__, info.barId);

	if(ori_BarID != info.barId)
		AUDIO_ERROR("barID Change %x -> %x \n", ori_BarID, info.barId);

    if(info.barId != TV006_SOUNDBAR_ID) // LG syound sync id
        AUDIO_ERROR("barID %x != %x \n", info.barId, TV006_SOUNDBAR_ID);

    if(ADEC_SB_SPDIF_CheckOpen() == FALSE)// no connect
    {
        // for socts test, SB NG case will pass "correct" parameter at closed status
        // we need to return fail
        AUDIO_ERROR("SB_SPDIF is not open\n");
        return NOT_OK;
    }

    if(ADEC_SB_SPDIF_CheckConnect() == FALSE)// no connect
    {
        info.barId = 0;// no connect no send to  sound bar
        AUDIO_ERROR("no connect force barID =0 \n");
    }

    return RHAL_AUDIO_SB_SetOpticalIDData(info, ret_checksum);
}

DTV_STATUS_T HAL_AUDIO_SB_GetOpticalStatus(HAL_AUDIO_SB_GET_INFO_T *pInfo)
{
    int ret = 0;
    KADP_AUDIO_SPDIF_CHANNEL_STATUS_SOUNDBAR cs;

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    ret = KADP_AUDIO_SB_GetChannelStatus(&cs);

    if(ret != 0){
        AUDIO_ERROR("KADP_AUDIO_SB_GetChannelStatus fail!!!\n");
        return NOT_OK;
    }

    if(cs.barId != TV006_SOUNDBAR_ID) // LG syound sync id
        AUDIO_ERROR("get barID %x != %x \n", cs.barId, TV006_SOUNDBAR_ID);

    pInfo->subFrameID = cs.barId;
    pInfo->subFrameData = cs.OSD_Vol.bits;
    pInfo->subFramePowerData = cs.command;
    pInfo->subframeChecksum = cs.checksum;

    return OK;
}

DTV_STATUS_T HAL_AUDIO_SB_SetCommand(HAL_AUDIO_SB_SET_CMD_T info)
{
    int ret = 0;

    BOOLEAN bSoundBarMode = false;
    BOOLEAN bSoundCanvasMode = false;
    int id = 0;
    int i = 0;

    AUDIO_INFO("%s   \n", __FUNCTION__);

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    //TODO
    id = ConvertSNDOUTIndexToResourceId(HAL_AUDIO_SB_SPDIF);
    for(i = 0; i< HAL_MAX_OUTPUT; i++)
    {
        if(ResourceStatus[id].connectStatus[i] == HAL_AUDIO_RESOURCE_CONNECT)
        {
            bSoundBarMode = true;
            break;
        }
    }

    if(bSoundBarMode == true || bSoundCanvasMode == true)
    {
        KADP_AUDIO_SPDIF_CHANNEL_STATUS_SOUNDBAR cs;
        UINT8 checkSum;

        memcpy(&g_AudioStatusInfo.curSoundBarCommand ,  &info, sizeof(HAL_AUDIO_SB_SET_CMD_T));

        KADP_AUDIO_SB_GetChannelStatus(&cs);

        cs.woofer_Vol.OSD.auto_vol = info.bAutoVolume;
        cs.woofer_Vol.OSD.woofer_level = info.wooferLevel;
        cs.reserved_data = info.reservedField & 0x0000FFFF;

        if(cs.barId != TV006_SOUNDBAR_ID) // LG syound sync id
             AUDIO_ERROR("get barID %x != %x  \n",  cs.barId, TV006_SOUNDBAR_ID);

        checkSum = (UINT8)cs.barId;
        checkSum ^= (UINT8)(cs.barId>>8);
        checkSum ^= (UINT8)(cs.barId>>16);
        checkSum ^= cs.OSD_Vol.bits;
        checkSum ^= cs.command;
        checkSum ^= cs.woofer_Vol.bits;
        checkSum ^= (UINT8)cs.reserved_data;
        checkSum ^= (UINT8)(cs.reserved_data>>8);
        cs.checksum = checkSum;

        ret = KADP_AUDIO_SB_SetChannelStatus(cs);

        if(ret != S_OK){
            AUDIO_ERROR("%s fail!!!\n", __FUNCTION__);
        }
    }else
    {
        AUDIO_ERROR("%s fail @ NOT SB mode!!!\n", __FUNCTION__);
        return NOT_OK;
    }

    return OK;
}

DTV_STATUS_T HAL_AUDIO_SB_GetCommandStatus(HAL_AUDIO_SB_GET_CMD_T *pInfo)
{
    int ret = 0;
    KADP_AUDIO_SPDIF_CHANNEL_STATUS_SOUNDBAR cs;

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    ret = KADP_AUDIO_SB_GetChannelStatus(&cs);

    if(cs.barId != TV006_SOUNDBAR_ID) // LG syound sync id
         AUDIO_ERROR("get barID %x != %x  \n", cs.barId, TV006_SOUNDBAR_ID);

    pInfo->subFrameID = cs.barId;
    pInfo->subFrameData = cs.woofer_Vol.bits;
    pInfo->subframeChecksum = cs.checksum;

    if(ret != 0){
        AUDIO_ERROR("%s fail!!!\n", __FUNCTION__);
    }
    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetSPKOutput(UINT8 i2sNumber, HAL_AUDIO_SAMPLING_FREQ_T samplingFreq)
{
   char para[20];
   UINT32 len = sizeof(para);
   AUDIO_INFO("%s  %d  %d \n", __FUNCTION__, i2sNumber, samplingFreq);

   if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
       return NOT_OK;

   if((samplingFreq==HAL_AUDIO_SAMPLING_FREQ_96_KHZ)/*&&(access("/home/nooutclk96", F_OK) != 0)*/){
       sprintf(para,"outclk 96");
       RTKAUDIO_RPC_TOAGENT_DEBUG_SVC(para, len);
       sprintf(para,"LGSE_I2S %d", i2sNumber);
       RTKAUDIO_RPC_TOAGENT_DEBUG_SVC(para, len);
       AUDIO_DEBUG("[AUDH]SR=%d,I2S num=%d\n",samplingFreq,i2sNumber);
   }else if(samplingFreq==HAL_AUDIO_SAMPLING_FREQ_48_KHZ){
       sprintf(para,"outclk 48");
       RTKAUDIO_RPC_TOAGENT_DEBUG_SVC(para, len);
       sprintf(para,"LGSE_I2S %d", i2sNumber);
       RTKAUDIO_RPC_TOAGENT_DEBUG_SVC(para, len);
       AUDIO_DEBUG("[AUDH]SR=%d,I2S num=%d\n",samplingFreq,i2sNumber);
   }else{
       AUDIO_INFO("[AUDH]no support(I2S number= %d,SR:%d)\n",i2sNumber,samplingFreq);
   }
   g_AudioStatusInfo.i2sNumber = (UINT32)i2sNumber;

   return OK;
}

DTV_STATUS_T HAL_AUDIO_AENC_Start(HAL_AUDIO_AENC_INDEX_T aencIndex, HAL_AUDIO_AENC_ENCODING_FORMAT_T audioType)
{
    AUDIO_INFO("%s aenc %d  %d \n", __FUNCTION__, aencIndex, audioType);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(!RangeCheck(aencIndex, 0, AUD_AENC_MAX))
    {
        AUDIO_ERROR("aenc index error  %d \n", aencIndex);
        return NOT_OK;
    }

    if(Aud_aenc[aencIndex] == NULL) return NOT_OK;

    return RHAL_AUDIO_AENC_Start(aencIndex,audioType);
}

DTV_STATUS_T RHAL_AUDIO_AENC_Start(HAL_AUDIO_AENC_INDEX_T aencIndex, HAL_AUDIO_AENC_ENCODING_FORMAT_T audioType)
{
#define HACK_APVR_MUTE
    HAL_AUDIO_RESOURCE_T resEncId = aenc2res(aencIndex);
#ifdef HACK_APVR_MUTE
    int i, ch_vol[AUD_MAX_CHANNEL];
    HAL_AUDIO_ADEC_INDEX_T adecIndex = res2adec(GetNonOutputModuleSingleInputResource(ResourceStatus[resEncId]));
    HAL_AUDIO_VOLUME_T volume_0dB = {.mainVol = 0x7F, .fineVol = 0x0};
#endif

    if(AUDIO_HAL_CHECK_PLAY_NOTAVAILABLE(ResourceStatus[resEncId], 0)) // encoder   can only have one input
    {
        AUDIO_ERROR("%s enc  play check failed %d %d \n",
                      __FUNCTION__, ResourceStatus[resEncId].connectStatus[0], ResourceStatus[resEncId].connectStatus[0]);
        return NOT_OK;
    }

    /*osal_SemWait(&Aud_aenc[aencIndex]->callback_sem, TIME_INFINITY);*/
    ResourceStatus[resEncId].connectStatus[0] = HAL_AUDIO_RESOURCE_RUN; // encoder  can only have one input

    Aud_aenc[aencIndex]->info.codec = audioType;

    Aud_aenc[aencIndex]->info.errorCount = 0;
    Aud_aenc[aencIndex]->info.inputCount = 0;
    Aud_aenc[aencIndex]->info.underflowCount = 0;
    Aud_aenc[aencIndex]->info.overflowCount = 0;

#if 0
    if(Aud_aenc[aencIndex]->pThread->IsRun(Aud_aenc[aencIndex]->pThread) == FALSE)
    {
        Aud_aenc[aencIndex]->pThread->Run(Aud_aenc[aencIndex]->pThread);
    }
#endif

#ifdef HACK_APVR_MUTE
    for(i = 0; i < AUD_MAX_CHANNEL; i++)
        ch_vol[i] = Volume_to_DSPGain(volume_0dB);

    ADSP_DEC_SetVolume(adecIndex, ch_vol);
    ADSP_DEC_SetMute(adecIndex, ADEC_CheckConnect(adecIndex), FALSE/*user mute*/);// encoder auto unmute
#endif

    ((ENC*)Aud_aenc[aencIndex]->pEnc->pDerivedObj)->StartEncode(Aud_aenc[aencIndex]->pEnc);

    Aud_aenc[aencIndex]->info.status = HAL_AUDIO_AENC_STATUS_PLAY;
    /*osal_SemGive(&Aud_aenc[aencIndex]->callback_sem);*/
    return OK;
}

DTV_STATUS_T HAL_AUDIO_AENC_Stop(HAL_AUDIO_AENC_INDEX_T aencIndex)
{
    AUDIO_INFO("%s aenc %d \n", __FUNCTION__, aencIndex);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(!RangeCheck(aencIndex, 0, AUD_AENC_MAX))
    {
        AUDIO_ERROR("aenc index error  %d \n", aencIndex);
        return NOT_OK;
    }

    return RHAL_AUDIO_AENC_Stop(aencIndex);
}

DTV_STATUS_T RHAL_AUDIO_AENC_Stop(HAL_AUDIO_AENC_INDEX_T aencIndex)
{
    HAL_AUDIO_RESOURCE_T resEncId = aenc2res(aencIndex);
#ifdef HACK_APVR_MUTE
    int ch_vol[AUD_MAX_CHANNEL];
    HAL_AUDIO_ADEC_INDEX_T adecIndex = res2adec(GetNonOutputModuleSingleInputResource(ResourceStatus[resEncId]));
#endif

    // TODO: add connect status checking
    if(AUDIO_HAL_CHECK_STOP_NOTAVAILABLE(ResourceStatus[resEncId], 0))//decoder can only have one input
    {
        AUDIO_ERROR("enc stop check failed %d %d \n",
                      ResourceStatus[resEncId].connectStatus[0], ResourceStatus[resEncId].connectStatus[0]);
        return NOT_OK;
    }

    if(Aud_aenc[aencIndex] == NULL) return NOT_OK;

    ((ENC*)Aud_aenc[aencIndex]->pEnc->pDerivedObj)->StopEncode(Aud_aenc[aencIndex]->pEnc);

    ResourceStatus[resEncId].connectStatus[0] = HAL_AUDIO_RESOURCE_STOP; //can only have one input

    Aud_aenc[aencIndex]->info.status = HAL_AUDIO_AENC_STATUS_STOP;

#ifdef HACK_APVR_MUTE
    ADEC_Calculate_DSPGain(adecIndex, ch_vol);
    ADSP_DEC_SetVolume(adecIndex, ch_vol);
    ADSP_DEC_SetMute(adecIndex, ADEC_CheckConnect(adecIndex), GetDecInMute(adecIndex));// revert ori mute status
#endif

    return OK;
}

DTV_STATUS_T RHAL_AUDIO_AENC_SetInfo(HAL_AUDIO_AENC_INDEX_T aencIndex, HAL_AUDIO_AENC_INFO_T info)
{
    HAL_AUDIO_RESOURCE_T id = aenc2res(aencIndex);
    AUDIO_ENC_CFG ecfg;

    AUDIO_INFO("%s aenc %d  %x \n", __FUNCTION__, aencIndex, info.codec);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(!RangeCheck(aencIndex, 0, AUD_AENC_MAX))
    {
        AUDIO_ERROR("aenc index error  %d \n", aencIndex);
        return NOT_OK;
    }

    if((ResourceStatus[id].connectStatus[0] == HAL_AUDIO_RESOURCE_RUN) ||
       (ResourceStatus[id].connectStatus[0] == HAL_AUDIO_RESOURCE_PAUSE))//encoder can only have one input
    {
        AUDIO_ERROR("aenc  error status %d \n", ResourceStatus[id].connectStatus[0]);
        return NOT_OK;
    }

    if(Aud_aenc[aencIndex] == NULL || Aud_aenc[aencIndex]->pEnc == NULL) return NOT_OK;

    //memcpy(&(Aud_aenc[aencIndex]->info), &info, sizeof(Aud_aenc[aencIndex]->info));
    Aud_aenc[aencIndex]->info.channel = info.channel;
    Aud_aenc[aencIndex]->info.bitrate = info.bitrate;
    Aud_aenc[aencIndex]->info.dec_port = info.dec_port;
    Aud_aenc[aencIndex]->info.gain = info.gain;

    switch(Aud_aenc[aencIndex]->info.channel)
    {
        case HAL_AUDIO_AENC_MONO:
            ecfg.inputmode = MONO;
            ecfg.outputmode = MONO;
            break;
        case HAL_AUDIO_AENC_STEREO:
            ecfg.inputmode = STEREO;
            ecfg.outputmode = STEREO;
            break;
        default:
            return NOT_OK;
    }
    switch(Aud_aenc[aencIndex]->info.bitrate)
    {
        case HAL_AUDIO_AENC_BIT_48K:  ecfg.datarate = 48000; break;
        case HAL_AUDIO_AENC_BIT_56K:  ecfg.datarate = 56000; break;
        case HAL_AUDIO_AENC_BIT_64K:  ecfg.datarate = 64000; break;
        case HAL_AUDIO_AENC_BIT_80K:  ecfg.datarate = 80000; break;
        case HAL_AUDIO_AENC_BIT_112K: ecfg.datarate = 112000; break;
        case HAL_AUDIO_AENC_BIT_128K: ecfg.datarate = 128000; break;
        case HAL_AUDIO_AENC_BIT_160K: ecfg.datarate = 160000; break;
        case HAL_AUDIO_AENC_BIT_192K: ecfg.datarate = 192000; break;
        case HAL_AUDIO_AENC_BIT_224K: ecfg.datarate = 224000; break;
        case HAL_AUDIO_AENC_BIT_256K: ecfg.datarate = 256000; break;
        case HAL_AUDIO_AENC_BIT_320K: ecfg.datarate = 320000; break;
        default: return NOT_OK;
    }

    ecfg.DRC1 = 0;
    ecfg.DRC2 = 0;
    ecfg.LorR= 0;
    ecfg.samprate = 48000;
    ((ENC*)Aud_aenc[aencIndex]->pEnc->pDerivedObj)->EncoderConfig(Aud_aenc[aencIndex]->pEnc, (void*)&ecfg);

    return OK;
}


DTV_STATUS_T HAL_AUDIO_AENC_SetCodec(HAL_AUDIO_AENC_INDEX_T aencIndex, HAL_AUDIO_AENC_ENCODING_FORMAT_T codec)
{
    AUDIO_INFO("%s aenc %d  %x \n", __FUNCTION__, aencIndex, codec);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    if(!RangeCheck(aencIndex, 0, AUD_AENC_MAX))
    {
        AUDIO_ERROR("aenc index error  %d \n", aencIndex);
        return NOT_OK;
    }

    /* If support switching AAC/MP3 encoding */
    if(codec != Aud_aenc[aencIndex]->info.codec)
    {
        AUDIO_MODULE_TYPE encode_type = AUDIO_AAC_ENCODER;
        switch(codec)
        {
            case HAL_AUDIO_AENC_ENCODE_PCM:
                encode_type = AUDIO_LPCM_ENCODER;
                break;
            case HAL_AUDIO_AENC_ENCODE_MP3:
                encode_type = AUDIO_MP3_ENCODER;
                break;
            case HAL_AUDIO_AENC_ENCODE_AAC:
            default:
                encode_type = AUDIO_AAC_ENCODER;
                break;
        }
        AUDIO_INFO("[AENC] encode type switch to %d\n", encode_type);
        ((ENC*)Aud_aenc[aencIndex]->pEnc->pDerivedObj)
            ->SetEncodeType(Aud_aenc[aencIndex]->pEnc, encode_type);
    }
    Aud_aenc[aencIndex]->info.codec = codec;
    /* End of switching encoding type */

    return OK;
}

DTV_STATUS_T HAL_AUDIO_AENC_SetVolume(HAL_AUDIO_AENC_INDEX_T aencIndex, HAL_AUDIO_VOLUME_T volume, UINT32 gain)
{
    UINT32 i;
    SINT32 dsp_gain = Volume_to_DSPGain(volume);
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;
    if(!RangeCheck(aencIndex, 0, AUD_AENC_MAX))
        return NOT_OK;
    if(Aud_aenc[aencIndex] == NULL)
        return NOT_OK;
    AUDIO_INFO("%s aenc %d  \n", __FUNCTION__, aencIndex);

#ifdef AVOID_USELESS_RPC
    if (Volume_Compare(volume, g_enc_volume[aencIndex]))
    {
        AUDIO_INFO("skip aenc %d volume %d =  g_enc_volume %d  \n",  aencIndex, volume.mainVol, g_enc_volume[aencIndex].mainVol);
        return OK;
    }
#endif

    memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));

    audioConfig.msg_id = AUDIO_CONFIG_CMD_SET_VOLUME;
    audioConfig.value[0] = Aud_aenc[aencIndex]->pEnc->GetAgentID(Aud_aenc[aencIndex]->pEnc);
    audioConfig.value[1] = ENUM_DEVICE_ENCODER;
    audioConfig.value[2] = PCM_IN_RTK;
    audioConfig.value[3] = 0xFF;
    for(i = 0; i < 8; i++)
    {
        audioConfig.value[4+i] = dsp_gain;
    }

    if(SendRPC_AudioConfig(&audioConfig) != S_OK) return NOT_OK;

    g_enc_volume[aencIndex] = volume;
    Aud_aenc[aencIndex]->info.gain = gain;
    return OK;  //OK or NOT_OK.
}

BOOLEAN IsAudioInitial(void)
{
    if(Aud_initial == 0)
    {
        AUDIO_ERROR("audio not yet initial \n");
        return FALSE;
    }
    return TRUE;
}

void SNDOutFroceMute(AUDIO_CONFIG_DEVICE deviceID, BOOLEAN muteflag, AUDIO_DVOL_LEVEL dbValue)
{
    if(ADSP_SNDOut_SetMute(deviceID, muteflag) != S_OK)
        AUDIO_ERROR("[%s:%d] fail!\n",__FUNCTION__,__LINE__);
    if(ADSP_SNDOut_SetVolume(deviceID, dbValue) != S_OK)
        AUDIO_ERROR("[%s:%d] fail!\n",__FUNCTION__,__LINE__);
}

DTV_STATUS_T KHAL_AUDIO_Get_OUTPUT_MuteStatus(UINT32 *status)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;
    if(status == NULL) return NOT_OK;

    status[INDEX_SPK]      = g_AudioStatusInfo.curSPKMuteStatus;     //spk_mute_status
    status[INDEX_OPTIC]    = g_AudioStatusInfo.curSPDIFMuteStatus;   //optic_mute_status
    status[INDEX_OPTIC_LG] = g_AudioStatusInfo.curSPDIF_LG_MuteStatus;   //optic_lg_mute_status
    status[INDEX_HP]       = g_AudioStatusInfo.curHPMuteStatus;      //hp_mute_status
    status[INDEX_ARC]      = g_ARCStatusInfo.curARCMuteStatus;       //arc_mute_status
    status[INDEX_BLUETOOTH] = g_AudioStatusInfo.curBTMuteStatus;
    status[INDEX_WISA]      = g_AudioStatusInfo.curWISAMuteStatus;
    status[INDEX_SE_BT]     = g_AudioStatusInfo.curSEBTMuteStatus;

    return OK;
}

DTV_STATUS_T KHAL_AUDIO_Get_INPUT_MuteStatus(UINT32 *status)
{
    int i;
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;
    if(status == NULL) return NOT_OK;

    for(i = 0; i < AUD_ADEC_MAX; i++)
    {
        if(status[(i)*2] != 0xFF)
            status[(i)*2+1] = GetDecInMute(i);
    }
    for(i = 0; i < AUD_AMIX_MAX; i++)
    {
        if(status[ALSA_AMIXER_BASE+(i)*2] != 0xFF)
            status[ALSA_AMIXER_BASE+(i)*2+1] = GetAmixerUserMute(i);
    }

    return OK;
}

void show_resource_st(HAL_AUDIO_RESOURCE_T res)
{

    int i;
    int totalmixerOutputConnectResource;
    HAL_AUDIO_RESOURCE_STATUS *STATUS = ResourceStatus[res].connectStatus;
    HAL_AUDIO_RESOURCE_T *INPUT = ResourceStatus[res].inputConnectResourceId;
    HAL_AUDIO_RESOURCE_T *OUTPUT = ResourceStatus[res].outputConnectResourceID;
    HAL_AUDIO_RESOURCE_T mixerOptResourceId[5];
    memset(AUD_StringBuffer, 0, sizeof(AUD_StringBuffer));// clean

    if(IsADECSource(res)){
        pr_emerg("%s connectStatus %s\n",GetResourceString(res), GetResourceStatusString(STATUS[0]));
        FillDecInput(res);
        FillConnectedOutput(res);

        pr_emerg("%s\n", AUD_StringBuffer);
        pr_emerg("numOptconnect %d numIptconnect %d\n",ResourceStatus[res].numOptconnect,
                ResourceStatus[res].numIptconnect);
    }else if(IsAMIXSource(res)){
        for(i = (int)HAL_AUDIO_RESOURCE_MIXER0; i <= HAL_AUDIO_RESOURCE_MIXER7 ; i++)
        {
            GetConnectOutputSource(((HAL_AUDIO_RESOURCE_T) i), mixerOptResourceId, 5, &totalmixerOutputConnectResource);
            memset(AUD_StringBuffer, 0, sizeof(AUD_StringBuffer));// clean
            sprintf(AUD_StringBuffer, "AMixer%d:: status (%s)", (i-HAL_AUDIO_RESOURCE_MIXER0), GetResourceStatusString((int)(ResourceStatus[((HAL_AUDIO_RESOURCE_T) i)].connectStatus[0])));
            {
                FillConnectedOutput((HAL_AUDIO_RESOURCE_T) i);
            }
            pr_emerg("\n%s\n", AUD_StringBuffer);
        }

    }else if(IsAOutSource(res)){
        pr_emerg("\n %s connectStatus %s\n",GetResourceString(res), GetResourceStatusString(STATUS[0]));
        for(i = 0; i < ResourceStatus[res].numIptconnect; i++){
            pr_emerg("%s\n", GetResourceString(INPUT[i]));
        }
    }else{
        pr_emerg("connectStatus %d %d %d %d %d %d %d %d %d %d\n",
                STATUS[0],STATUS[1],STATUS[2],STATUS[3],STATUS[4],STATUS[5],
                STATUS[6],STATUS[7],STATUS[8],STATUS[9]);
        pr_emerg("intput %d %d %d %d %d %d %d %d %d %d\n",
                INPUT[0],INPUT[1],INPUT[2],INPUT[3],INPUT[4],INPUT[5],
                INPUT[6],INPUT[7],INPUT[8],INPUT[9]);
        pr_emerg("output %d %d %d %d %d %d %d %d %d %d\n",
                OUTPUT[0],OUTPUT[1],OUTPUT[2],OUTPUT[3],OUTPUT[4],OUTPUT[5],
                OUTPUT[6],OUTPUT[7],OUTPUT[8],OUTPUT[9]);
        pr_emerg("numOptconnect %d numIptconnect %d\n",ResourceStatus[res].numOptconnect,
                ResourceStatus[res].numIptconnect);
    }
}

/* HAL decoder for GST path */
UINT32 HAL_AUDIO_GetDecoderInstanceID(HAL_AUDIO_ADEC_INDEX_T adecIndex, int proceddID, long *instanceID)
{
    AUDIO_INFO("%s for ADEC%d, owner PID(%d)\n", __FUNCTION__, adecIndex, proceddID);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    *instanceID = Aud_dec[adecIndex]->GetAgentID(Aud_dec[adecIndex]);
    adec_status[adecIndex].UsingByGst = TRUE;
    gst_owner_process[adecIndex] = proceddID;
    ((DEC*)Aud_dec[adecIndex]->pDerivedObj)->StopDwnStrmQMonitorThread(Aud_dec[adecIndex]);
    return S_OK;
}

UINT32 HAL_AUDIO_ReleaseDecoderInstanceID(HAL_AUDIO_ADEC_INDEX_T adecIndex)
{
    AUDIO_INFO("%s for ADEC%d\n", __FUNCTION__, adecIndex);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    ((DEC*)Aud_dec[adecIndex]->pDerivedObj)->StartDwnStrmQMonitorThread(Aud_dec[adecIndex]);
    adec_status[adecIndex].UsingByGst = FALSE;
    gst_owner_process[adecIndex] = 0;
    return S_OK;
}

static int ReleaseGstDecoderThread(void* data)
{
    int proceddID = (int)data;
    int i;
    AUDIO_INFO("%s for process ID %d\n", __FUNCTION__, proceddID);
    for(i = 0; i <AUD_ADEC_MAX; i++)
    {
        if(gst_owner_process[i] == proceddID)
        {
            HAL_AUDIO_ReleaseDecoderInstanceID((HAL_AUDIO_ADEC_INDEX_T)i);
            break;
        }
    }
    return S_OK;
}

UINT32 HAL_AUDIO_ReleaseDecoderInstanceByPID(int proceddID)
{
    struct task_struct *release_tsk;
    AUDIO_INFO("%s for process ID %d\n", __FUNCTION__, proceddID);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    release_tsk = kthread_create(ReleaseGstDecoderThread, (void*)proceddID, "release_gst_tsk");
    if (IS_ERR(release_tsk)) {
        AUDIO_ERROR("[%s] fail to create kthread\n",__FUNCTION__);
        return NOT_OK;
    }
    wake_up_process(release_tsk);
    return S_OK;
}

UINT32 HAL_AUDIO_GetDecoderOutBuffer(HAL_AUDIO_ADEC_INDEX_T adecIndex, UINT32 pin_index, UINT32 *bufferHeaderPhyAddr, UINT32 *bufferPhyAddr, UINT32 *size)
{
    AUDIO_INFO("%s for ADEC%d pin%d\n", __FUNCTION__, adecIndex, pin_index);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;
    if(!IsValidAdecIdx(adecIndex))
    {
        AUDIO_ERROR("ADEC index error  %d \n", adecIndex);
        return NOT_OK;
    }

    ((DEC*)Aud_dec[adecIndex]->pDerivedObj)->GetOUTRingBufPhyAddr(Aud_dec[adecIndex], pin_index, bufferHeaderPhyAddr, bufferPhyAddr, size);
    AUDIO_INFO("get Header 0x%x, buffer 0x%x, size 0x%x\n", *bufferHeaderPhyAddr, *bufferPhyAddr, *size);
    return S_OK;
}

UINT32 HAL_AUDIO_GetDecoderIcqBuffer(HAL_AUDIO_ADEC_INDEX_T adecIndex, UINT32 *bufferHeaderPhyAddr, UINT32 *bufferPhyAddr, UINT32 *size)
{
    AUDIO_INFO("%s for ADEC%d\n", __FUNCTION__, adecIndex);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;
    if(!IsValidAdecIdx(adecIndex))
    {
        AUDIO_ERROR("ADEC index error  %d \n", adecIndex);
        return NOT_OK;
    }

    ((DEC*)Aud_dec[adecIndex]->pDerivedObj)->GetICQRingBufPhyAddr(Aud_dec[adecIndex], bufferHeaderPhyAddr, bufferPhyAddr, size);
    AUDIO_INFO("get Header 0x%x, buffer 0x%x, size 0x%x\n", *bufferHeaderPhyAddr, *bufferPhyAddr, *size);
    return S_OK;
}

UINT32 HAL_AUDIO_GetDecoderDsqBuffer(HAL_AUDIO_ADEC_INDEX_T adecIndex, UINT32 *bufferHeaderPhyAddr, UINT32 *bufferPhyAddr, UINT32 *size)
{
    AUDIO_INFO("%s for ADEC%d\n", __FUNCTION__, adecIndex);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;
    if(!IsValidAdecIdx(adecIndex))
    {
        AUDIO_ERROR("ADEC index error  %d \n", adecIndex);
        return NOT_OK;
    }

    ((DEC*)Aud_dec[adecIndex]->pDerivedObj)->GetDSQRingBufPhyAddr(Aud_dec[adecIndex], bufferHeaderPhyAddr, bufferPhyAddr, size);
    AUDIO_INFO("get Header 0x%x, buffer 0x%x, size 0x%x\n", *bufferHeaderPhyAddr, *bufferPhyAddr, *size);
    return S_OK;
}

DTV_STATUS_T KHAL_AUDIO_SetEAC3_ATMOS_ENCODE(BOOLEAN bOnOff)
{
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;

    AUDIO_INFO("[%s] bOnOff %d\n", __FUNCTION__, bOnOff);
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE)
        return NOT_OK;

    audioConfig.msg_id = AUDIO_CONFIG_CMD_EAC3_ATMOS_ENCODE_ONOFF;
    audioConfig.value[0] = bOnOff;
    if(SendRPC_AudioConfig(&audioConfig) != S_OK) return NOT_OK;

    return OK;
}

BOOLEAN aenc_flow_start(HAL_AUDIO_ADEC_INDEX_T adecIndex, aenc_encoding_format_t format, aenc_bitrate_t bitrate, adec_src_port_index_ext_type_t inputConnect, HAL_AUDIO_VOLUME_T volume, UINT32 gain)
{
    // PVR flow: (AIN-ADEC-AENC)
    int ret = -1;
    HAL_AUDIO_AENC_INFO_T aenc_info;
    HAL_AUDIO_RESOURCE_T adecResourceId = adec2res(adecIndex);

    // check target adec idx and main output idx, should not be the same.
    if(adecIndex == Aud_mainDecIndex)
    {
        AUDIO_ERROR("[AENC] decoder idx(%d) is the same as main output(%d)!!!!!!\n", adecIndex, Aud_mainDecIndex);
    }

#ifdef AUTO_START_AENC_FLOW
    // open sub ADEC and connect sub dec to main ADEC input connect
    ret = HAL_AUDIO_ADEC_Open(adecIndex);
    if(ret != OK) {
        AUDIO_ERROR("[AENC] HAL_AUDIO_ADEC_Open() failed\n");
        return FALSE;
    }
    ret = HAL_AUDIO_ADEC_Connect(adecIndex, inputConnect);
    if(ret != OK) {
        AUDIO_ERROR("[AENC] HAL_AUDIO_ADEC_Connect() failed\n");
        return FALSE;
    }
#endif
    // open AENC connect sub ADEC
    ret = HAL_AUDIO_AENC_Open(HAL_AUDIO_AENC0);
    if(ret != OK) {
        AUDIO_ERROR("[AENC] HAL_AUDIO_AENC_Open() failed\n");
        return FALSE;
    }
    ret = HAL_AUDIO_AENC_Connect(HAL_AUDIO_RESOURCE_AENC0, adecIndex + HAL_AUDIO_RESOURCE_ADEC0); // use old hal resource idx
    if(ret != OK) {
        AUDIO_ERROR("[AENC] HAL_AUDIO_AENC_Connect() failed\n");
        return FALSE;
    }
    // set volume
    ret = HAL_AUDIO_AENC_SetVolume(HAL_AUDIO_AENC0, volume, gain);
    if(ret != OK) {
        AUDIO_ERROR("[AENC] HAL_AUDIO_AENC_SetVolume() failed\n");
        return FALSE;
    }

    memcpy(&aenc_info, &(Aud_aenc[HAL_AUDIO_AENC0]->info), sizeof(HAL_AUDIO_AENC_INFO_T));
    aenc_info.bitrate = bitrate_type_alsa2hal(bitrate);
    aenc_info.dec_port = adecIndex;
    aenc_info.gain = gain;
    ret = RHAL_AUDIO_AENC_SetInfo(HAL_AUDIO_AENC0, aenc_info);
    if(ret != OK) {
        AUDIO_ERROR("[AENC] RHAL_AUDIO_AENC_SetInfo() failed\n");
        return FALSE;
    }

    // start ADEC & AENC
    // Even if webos call "Adec Start" before this, we should call HAL_AUDIO_StartDecoding() again,
    // becasue ADEC connect to AENC flow is actually processed in HAL_AUDIO_StartDecoding()
    if(AUDIO_HAL_CHECK_ISATRUNSTATE(ResourceStatus[adecResourceId], 0)) {
        //avoid start decoding on the same type 2 times
        RHAL_AUDIO_StopDecoding(adecIndex, 0);
    }
    ret = HAL_AUDIO_StartDecoding(adecIndex, ADEC_SRC_CODEC_PCM);
    if(ret != OK) {
        AUDIO_ERROR("[AENC] HAL_AUDIO_StartDecoding() failed\n");
        return FALSE;
    }
    ret = HAL_AUDIO_AENC_Start(HAL_AUDIO_AENC0, HAL_AUDIO_AENC_ENCODE_AAC);
    if(ret != OK) {
        AUDIO_ERROR("[AENC] HAL_AUDIO_AENC_Start() failed\n");
        return FALSE;
    }
    return TRUE;
}

void aenc_flow_stop(HAL_AUDIO_ADEC_INDEX_T adecIndex, adec_src_port_index_ext_type_t inputConnect)
{
#ifdef AUTO_START_AENC_FLOW
    HAL_AUDIO_StopDecoding(adecIndex);
    HAL_AUDIO_ADEC_Disconnect(adecIndex, inputConnect);
#endif
    HAL_AUDIO_AENC_Stop(HAL_AUDIO_AENC0);
    HAL_AUDIO_AENC_Disconnect(HAL_AUDIO_RESOURCE_AENC0, adecIndex + HAL_AUDIO_RESOURCE_ADEC0);
#ifdef AUTO_START_AENC_FLOW
    HAL_AUDIO_ADEC_Close(adecIndex);
#endif
    HAL_AUDIO_AENC_Close(HAL_AUDIO_AENC0);
}

DTV_STATUS_T HAL_AUDIO_BT_Connect(int input_port)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;

    if ((input_port < 0) || (input_port > (HAL_AUDIO_INDEX_MAX))) {
        AUDIO_ERROR("[%s] invalid input port(%d)\n", __FUNCTION__, input_port);
        return NOT_OK;
    }

    g_AudioStatusInfo.curBTConnectStatus[input_port] = TRUE;
    return OK;
}

DTV_STATUS_T HAL_AUDIO_BT_Disconnect(int input_port)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;

    if ((input_port < 0) || (input_port > HAL_AUDIO_INDEX_MAX)) {
        AUDIO_ERROR("[%s] invalid input port(%d)\n", __FUNCTION__, input_port);
        return NOT_OK;
    }

    g_AudioStatusInfo.curBTConnectStatus[input_port] = FALSE;
    return OK;
}

BOOLEAN ADEC_BT_CheckOpen(common_output_ext_type_t bt_deivce)
{
    int i = 0;
    HAL_AUDIO_RESOURCE_T BT_res = opt2res(bt_deivce);
    if(BT_res == HAL_AUDIO_RESOURCE_NO_CONNECTION) return FALSE;

    /* check BT open status */
    for(i = 0; i < HAL_MAX_OUTPUT; i++) {
        if(ResourceStatus[BT_res].connectStatus[i] == HAL_AUDIO_RESOURCE_OPEN) {
            return TRUE;
        }
    }

    return FALSE;
}

BOOLEAN IS_BT_Connected(void)
{
    int ret = FALSE, i, max_index;

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return FALSE;

    max_index = HAL_AUDIO_INDEX_MAX + 1;
    for (i=0;i<max_index;i++) {
        if (g_AudioStatusInfo.curBTConnectStatus[i]) {
            ret = TRUE;
            break;
        }
    }

    return ret;
}

DTV_STATUS_T HAL_AUDIO_WISA_Connect(int input_port)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;

    if ((input_port < 0) || (input_port > HAL_AUDIO_INDEX_MAX)) {
        AUDIO_ERROR("[%s] invalid input port(%d)\n", __FUNCTION__, input_port);
        return NOT_OK;
    }

    g_AudioStatusInfo.curWISAConnectStatus[input_port] = TRUE;
    return OK;
}

DTV_STATUS_T HAL_AUDIO_WISA_Disconnect(int input_port)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;

    if ((input_port < 0) || (input_port > HAL_AUDIO_INDEX_MAX)) {
        AUDIO_ERROR("[%s] invalid input port(%d)\n", __FUNCTION__, input_port);
        return NOT_OK;
    }

    g_AudioStatusInfo.curWISAConnectStatus[input_port] = FALSE;
    return OK;
}

BOOLEAN IS_WISA_Connected(void)
{
    int ret = FALSE, i, max_index;

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return FALSE;

    max_index = HAL_AUDIO_INDEX_MAX + 1;
    for (i=0;i<max_index;i++) {
        if (g_AudioStatusInfo.curWISAConnectStatus[i]) {
            ret = TRUE;
            break;
        }
    }

    return ret;
}

DTV_STATUS_T HAL_AUDIO_SE_BT_Connect(int input_port)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;

    if ((input_port < 0) || (input_port > HAL_AUDIO_INDEX_MAX)) {
        AUDIO_ERROR("[%s] invalid input port(%d)\n", __FUNCTION__, input_port);
        return NOT_OK;
    }

    g_AudioStatusInfo.curSEBTConnectStatus[input_port] = TRUE;
    return OK;
}

DTV_STATUS_T HAL_AUDIO_SE_BT_Disconnect(int input_port)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;

    if ((input_port < 0) || (input_port > HAL_AUDIO_INDEX_MAX)) {
        AUDIO_ERROR("[%s] invalid input port(%d)\n", __FUNCTION__, input_port);
        return NOT_OK;
    }

    g_AudioStatusInfo.curSEBTConnectStatus[input_port] = FALSE;
    return OK;
}

BOOLEAN IS_SE_BT_Connected(void)
{
    int ret = FALSE, i, max_index;

    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return FALSE;

    max_index = HAL_AUDIO_INDEX_MAX + 1;
    for (i=0;i<max_index;i++) {
        if (g_AudioStatusInfo.curSEBTConnectStatus[i]) {
            ret = TRUE;
            break;
        }
    }

    return ret;
}

DTV_STATUS_T HAL_AUDIO_SetBTOutMute(BOOLEAN bOnOff)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;

    g_AudioStatusInfo.curBTMuteStatus = bOnOff;
    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetWISAOutMute(BOOLEAN bOnOff)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;

    g_AudioStatusInfo.curWISAMuteStatus = bOnOff;
    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetSEBTOutMute(BOOLEAN bOnOff)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;

    g_AudioStatusInfo.curSEBTMuteStatus = bOnOff;
    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetBTOutVolume(UINT32 gain)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;

    g_AudioStatusInfo.curBTOutGain = gain;
    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetWISAOutVolume(UINT32 gain)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;

    g_AudioStatusInfo.curWISAOutGain = gain;
    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetSEBTOutVolume(UINT32 gain)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;

    g_AudioStatusInfo.curSEBTOutGain = gain;
    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetBTOutDelayTime(UINT32 delayTime)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;
    if(delayTime > AUD_MAX_SNDOUT_DELAY)
    {
        AUDIO_ERROR("%s set delay %d ms, exceed max delayTime(%d)\n", __FUNCTION__, delayTime, AUD_MAX_SNDOUT_DELAY);
        return NOT_OK;
    }

    g_AudioStatusInfo.curBTOutDelay = delayTime;
    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetWISAOutDelayTime(UINT32 delayTime)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;
    if(delayTime > AUD_MAX_SNDOUT_DELAY)
    {
        AUDIO_ERROR("%s set delay %d ms, exceed max delayTime(%d)\n", __FUNCTION__, delayTime, AUD_MAX_SNDOUT_DELAY);
        return NOT_OK;
    }

    g_AudioStatusInfo.curWISAOutDelay = delayTime;
    return OK;
}

DTV_STATUS_T HAL_AUDIO_SetSEBTOutDelayTime(UINT32 delayTime)
{
    if(AUDIO_HAL_CHECK_INITIAL_OK() != TRUE) return NOT_OK;
    if(delayTime > AUD_MAX_SNDOUT_DELAY)
    {
        AUDIO_ERROR("%s set delay %d ms, exceed max delayTime(%d)\n", __FUNCTION__, delayTime, AUD_MAX_SNDOUT_DELAY);
        return NOT_OK;
    }

    g_AudioStatusInfo.curSEBTOutDelay = delayTime;
    return OK;
}

char *ChannelName[] = {
    "0",
    "1",
    "2",
    "3",
    "4",
    "5",
    "5.1",
    "7",
    "7.1",
};

int32_t get_adec_status(HAL_AUDIO_ADEC_INDEX_T adecIndex, char* str)
{
    int len, total = 0;
    HAL_AUDIO_ADEC_INFO_T pAudioAdecInfo;

    /* get runtime information */
    HAL_AUDIO_GetAdecStatus(adecIndex, &pAudioAdecInfo);
    ((DEC*)Aud_dec[adecIndex]->pDerivedObj)->GetCurrentPTS(Aud_dec[adecIndex], &adec_status[adecIndex].PTS);

    /* build status string */
    len = sprintf(str+total, "AdecPortNum=%d\n", adecIndex);
    total += len;
    len = sprintf(str+total, "Open=%d\n", adec_status[adecIndex].Open);
    total += len;
    len = sprintf(str+total, "Connect=%s\n", GetResourceString(res_type_alsa2hal(adec_status[adecIndex].Connect)));
    total += len;
    len = sprintf(str+total, "Start=%d\n", adec_status[adecIndex].Start);
    total += len;
    len = sprintf(str+total, "UserCodec=%s\n", GetSRCTypeName(codec_type_alsa2hal(adec_status[adecIndex].UserCodec)));
    total += len;
    len = sprintf(str+total, "CurCodec=%s\n", GetSRCTypeName(codec_type_alsa2hal(adec_status[adecIndex].CurCodec)));
    total += len;
    len = sprintf(str+total, "SrcCodec=%s\n", GetSRCTypeName(codec_type_alsa2hal(adec_status[adecIndex].SrcCodec)));
    total += len;
    len = sprintf(str+total, "SrcBitrates=%d\n", (adec_status[adecIndex].Start)?adec_status[adecIndex].SrcBitrates:0);
    total += len;
    len = sprintf(str+total, "SrcSamplingRate=%d\n", adec_status[adecIndex].SrcSamplingRate);
    total += len;
    len = sprintf(str+total, "SrcChannel=%s\n", ChannelName[adec_status[adecIndex].SrcChannel]);
    total += len;
    len = sprintf(str+total, "OutputCodec=%s\n", GetSRCTypeName(codec_type_alsa2hal(adec_status[adecIndex].OutputCodec)));
    total += len;
    len = sprintf(str+total, "OutputChannel=%s\n", ChannelName[adec_status[adecIndex].OutputChannel]);
    total += len;
    len = sprintf(str+total, "PTS=%lld\n", adec_status[adecIndex].PTS);
    total += len;
    len = sprintf(str+total, "Language=%s\n", "None");
    total += len;
    len = sprintf(str+total, "DolbyDRCMode=%s\n", DrcModeName[adec_status[adecIndex].DolbyDRCMode]);
    total += len;
    len = sprintf(str+total, "DownmixMode=%s\n", DownMixModeName[adec_status[adecIndex].Downmix]);
    total += len;
    len = sprintf(str+total, "SyncMode=%d\n", adec_status[adecIndex].SyncMode);
    total += len;
    len = sprintf(str+total, "TrickMode=%s\n", TrickModeName[adec_status[adecIndex].TrickMode]);
    total += len;
    len = sprintf(str+total, "Gain=0x%x\n", adec_status[adecIndex].Gain);
    total += len;
    len = sprintf(str+total, "Mute=%d\n", adec_status[adecIndex].Mute);
    total += len;
    len = sprintf(str+total, "Delay=%d\n", adec_status[adecIndex].Delay);
    total += len;
    len = sprintf(str+total, "AudioDescription=%s\n", adec_status[adecIndex].AudioDescription?"On":"Off");
    total += len;

    return total;
}

char* SPDIFCOPYINFOString[]=
{
    "UNKNOWN",
    "FREE",
    "NO_MORE",
    "",
    "ONCE",
    "",
    "",
    "",
    "NEVER",
    "",
};
char* GetSpdifCopyInfoName(sndout_optic_copyprotection_ext_type_t index)
{
   int id;
   id = (int)index;
   if((int)index >= (sizeof(SPDIFCOPYINFOString)/sizeof(SPDIFCOPYINFOString[0])))
   {
       AUDIO_ERROR("unknown spidf copyinfo %d \n", index);
       id = (sizeof(SPDIFCOPYINFOString)/sizeof(SPDIFCOPYINFOString[0])) - 1;// unknown
   }
   return SPDIFCOPYINFOString[id];
}

char *HalIndexName[] = {
    "ADEC0",
    "ADEC1",
    "ADEC2",
    "ADEC3",
    "AMIXER1",
    "AMIXER2",
    "AMIXER3",
    "AMIXER4",
    "AMIXER5",
    "AMIXER6",
    "AMIXER7",
    "AMIXER8",
    "",
};
char* GetHalIndexName(HAL_AUDIO_INDEX_T index)
{
   int id;
   id = (int)index;
   if((int)index >= (sizeof(HalIndexName)/sizeof(HalIndexName[0])))
   {
       AUDIO_ERROR("unknown hal index %d \n", index);
       id = (sizeof(HalIndexName)/sizeof(HalIndexName[0])) - 1;// unknown
   }
   return HalIndexName[id];
}

int32_t get_sndout_status(char* str)
{
    int i, len, total = 0, spdif_out_ch = 0, arc_out_ch = 0;
    HAL_AUDIO_SRC_TYPE_T spdif_out_type = HAL_AUDIO_SRC_TYPE_UNKNOWN;
    HAL_AUDIO_SRC_TYPE_T arc_out_type = HAL_AUDIO_SRC_TYPE_UNKNOWN;
    UINT32 retStatus;
    AUDIO_RPC_AO_TRSENC_FORMAT_INFO trsenc_fomat = {0};

    retStatus = ((PPAO*)Aud_ppAout->pDerivedObj)->GetTrsencAudioFormatInfo(Aud_ppAout, &trsenc_fomat);
    if(retStatus == S_OK)
    {
        if(ADEC_ARC_CheckConnect())
        {
            arc_out_ch = trsenc_fomat.nChannels;
            arc_out_type = ADECType_to_SRCType(trsenc_fomat.type, trsenc_fomat.reserved[1]);
        }
        else if (ADEC_SPDIF_CheckConnect())
        {
            spdif_out_ch = (trsenc_fomat.nChannels >= 6) ? 6 : trsenc_fomat.nChannels;
            spdif_out_type = ADECType_to_SRCType(trsenc_fomat.type, trsenc_fomat.reserved[1]);
        }
    }

    /* SPK */
    len = sprintf(str+total, "[SPK]\n");
    total += len;
    len = sprintf(str+total, "Open=%d\n", (ADEC_SPK_CheckOpen() || ADEC_SPK_CheckConnect()));
    total += len;
    len = sprintf(str+total, "Connect=%d\n", ADEC_SPK_CheckConnect());
    total += len;
    len = sprintf(str+total, "SpkOutputChannel=%d\n",g_AudioStatusInfo.i2sNumber);
    total += len;
    len = sprintf(str+total, "Gain=0x%x\n", g_AudioStatusInfo.curSPKOutGain);
    total += len;
    len = sprintf(str+total, "Mute=%d\n", g_AudioStatusInfo.curSPKMuteStatus);
    total += len;
    len = sprintf(str+total, "Delay=%d\n", g_AudioStatusInfo.curSPKOutDelay);
    total += len;
    len = sprintf(str+total, "ProcessingDelay=%d\n", adec_status[Aud_mainAudioIndex].Delay +
            get_ProcessingDelay(Aud_mainAudioIndex, COMMON_SPK) + g_AudioStatusInfo.curSPKOutDelay);
    total += len;

    /* OPTICAL */
    len = sprintf(str+total, "\n[OPTIC]\n");
    total += len;
    len = sprintf(str+total, "Open=%d\n", (ADEC_SPDIF_CheckOpen() || ADEC_SPDIF_CheckConnect()));
    total += len;
    len = sprintf(str+total, "Connect=%d\n", ADEC_SPDIF_CheckConnect());
    total += len;
    len = sprintf(str+total, "Light=%d\n", g_AudioStatusInfo.curSpdifLightOnOff);
    total += len;
    len = sprintf(str+total, "OpticMode=%s\n", GetSpdifModeName(g_AudioStatusInfo.curAudioSpdifMode));
    total += len;
    len = sprintf(str+total, "OutputCodec=%s\n", GetSRCTypeName(spdif_out_type));
    total += len;
    len = sprintf(str+total, "OutputChannel=%s\n", ChannelName[spdif_out_ch]);
    total += len;
    len = sprintf(str+total, "SrcCopyProtection=%s\n", GetSpdifCopyInfoName(g_AudioStatusInfo.curSpdifCopyInfo));
    total += len;
    len = sprintf(str+total, "OutputCopyProtection=%s\n", GetSpdifCopyInfoName(g_AudioStatusInfo.curSpdifCopyInfo));
    total += len;
    len = sprintf(str+total, "Gain=0x%x\n", g_AudioStatusInfo.curSPDIFOutGain);
    total += len;
    len = sprintf(str+total, "Mute=%d\n", g_AudioStatusInfo.curSPDIFMuteStatus);
    total += len;
    len = sprintf(str+total, "Delay=%d\n", g_AudioStatusInfo.curSPDIFOutDelay);
    total += len;
    len = sprintf(str+total, "ProcessingDelay=%d\n", adec_status[Aud_mainAudioIndex].Delay +
            get_ProcessingDelay(Aud_mainAudioIndex, COMMON_OPTIC) + g_AudioStatusInfo.curSPDIFOutDelay);
    total += len;

    /* LG OPTICAL */
    len = sprintf(str+total, "\n[OPTIC_LG]\n");
    total += len;
    len = sprintf(str+total, "Open=%d\n", (ADEC_SB_SPDIF_CheckOpen() || ADEC_SB_SPDIF_CheckConnect()));
    total += len;
    len = sprintf(str+total, "Connect=%d\n", ADEC_SB_SPDIF_CheckConnect());
    total += len;
    len = sprintf(str+total, "Light=%d\n", g_AudioStatusInfo.curSpdifLightOnOff);
    total += len;
    len = sprintf(str+total, "OpticMode=%s\n", GetSpdifModeName(g_AudioStatusInfo.curAudioSpdifMode));
    total += len;
    len = sprintf(str+total, "OutputCodec=%s\n", GetSRCTypeName(g_AudioStatusInfo.curAudioSpdifMode));
    total += len;
    len = sprintf(str+total, "OutputChannel=%d\n", 2);
    total += len;
    len = sprintf(str+total, "SrcCopyProtection=%s\n", GetSpdifCopyInfoName(g_AudioStatusInfo.curSpdifCopyInfo));
    total += len;
    len = sprintf(str+total, "OutputCopyProtection=%s\n", GetSpdifCopyInfoName(g_AudioStatusInfo.curSpdifCopyInfo));
    total += len;
    len = sprintf(str+total, "SoundBarID=0x%x\n", g_AudioStatusInfo.curSoundBarInfo.barId);
    total += len;
    len = sprintf(str+total, "SoundBarVolume=%d\n", g_AudioStatusInfo.curSoundBarInfo.volume);
    total += len;
    len = sprintf(str+total, "SoundBarMute=%d\n", g_AudioStatusInfo.curSoundBarInfo.bMuteOnOff);
    total += len;
    len = sprintf(str+total, "SoundBarPower=%d\n", g_AudioStatusInfo.curSoundBarInfo.bPowerOnOff);
    total += len;
    len = sprintf(str+total, "Gain=0x%x\n", g_AudioStatusInfo.curSPDIFOutGain);
    total += len;
    len = sprintf(str+total, "Mute=%d\n", g_AudioStatusInfo.curSPDIF_LG_MuteStatus);
    total += len;
    len = sprintf(str+total, "Delay=%d\n", g_AudioStatusInfo.curSPDIFSBOutDelay);
    total += len;
    len = sprintf(str+total, "ProcessingDelay=%d\n", adec_status[Aud_mainAudioIndex].Delay +
            get_ProcessingDelay(Aud_mainAudioIndex, COMMON_OPTIC_LG) + g_AudioStatusInfo.curSPDIFSBOutDelay);
    total += len;

    /* ARC */
    len = sprintf(str+total, "\n[ARC]\n");
    total += len;
    len = sprintf(str+total, "Open=%d\n", (ADEC_ARC_CheckOpen() || ADEC_ARC_CheckConnect()));
    total += len;
    len = sprintf(str+total, "Connect=%d\n", ADEC_ARC_CheckConnect());
    total += len;
    len = sprintf(str+total, "ArcOnOff=%d\n", HDMI_arcOnOff);
    total += len;
    len = sprintf(str+total, "ArcMode=%s\n", GetARCModeName(g_ARCStatusInfo.curAudioArcMode));
    total += len;
    len = sprintf(str+total, "OutputCodec=%s\n", GetSRCTypeName(arc_out_type));
    total += len;
    len = sprintf(str+total, "OutputChannel=%s\n", ChannelName[arc_out_ch]);
    total += len;
    len = sprintf(str+total, "SrcCopyProtection=%s\n", GetSpdifCopyInfoName(g_ARCStatusInfo.curARCCopyInfo));
    total += len;
    len = sprintf(str+total, "OutputCopyProtection=%s\n", GetSpdifCopyInfoName(g_ARCStatusInfo.curARCCopyInfo));
    total += len;
    len = sprintf(str+total, "Gain=0x%x\n", g_ARCStatusInfo.curARCOutGain);
    total += len;
    len = sprintf(str+total, "Mute=%d\n", g_ARCStatusInfo.curARCMuteStatus);
    total += len;
    len = sprintf(str+total, "Delay=%d\n", g_ARCStatusInfo.curARCOutDelay);
    total += len;
    len = sprintf(str+total, "ProcessingDelay=%d\n", adec_status[Aud_mainAudioIndex].Delay +
            get_ProcessingDelay(Aud_mainAudioIndex, COMMON_ARC) + g_ARCStatusInfo.curARCOutDelay);
    total += len;

    /* BT */
    len = sprintf(str+total, "\n[BLUETOOTH]\n");
    total += len;
    len = sprintf(str+total, "Open=%d\n", ADEC_BT_CheckOpen(COMMON_BLUETOOTH));
    total += len;
    len = sprintf(str+total, "Connect=%d\n", IS_BT_Connected());
    total += len;
    len = sprintf(str+total, "Gain=0x%x\n", g_AudioStatusInfo.curBTOutGain);
    total += len;
    len = sprintf(str+total, "Mute=%d\n", g_AudioStatusInfo.curBTMuteStatus);
    total += len;
    len = sprintf(str+total, "Delay=NA\n");
    total += len;
    len = sprintf(str+total, "ProcessingDelay=%d\n", adec_status[Aud_mainAudioIndex].Delay +
            get_ProcessingDelay(Aud_mainAudioIndex, COMMON_BLUETOOTH) + g_AudioStatusInfo.curBTOutDelay);
    total += len;

    /* HP */
    len = sprintf(str+total, "\n[HP]\n");
    total += len;
    len = sprintf(str+total, "Open=%d\n", (ADEC_HP_CheckOpen() || ADEC_HP_CheckConnect()));
    total += len;
    len = sprintf(str+total, "Connect=%d\n", ADEC_HP_CheckConnect());
    total += len;
    len = sprintf(str+total, "Gain=0x%x\n", g_AudioStatusInfo.curHPOutGain);
    total += len;
    len = sprintf(str+total, "Mute=%d\n", g_AudioStatusInfo.curHPMuteStatus);
    total += len;
    len = sprintf(str+total, "Delay=%d\n", g_AudioStatusInfo.curHPOutDelay);
    total += len;
    len = sprintf(str+total, "ProcessingDelay=%d\n", adec_status[Aud_mainAudioIndex].Delay +
            get_ProcessingDelay(Aud_mainAudioIndex, COMMON_HP) + g_AudioStatusInfo.curHPOutDelay);
    total += len;

    /* WISA */
    len = sprintf(str+total, "\n[WISA]\n");
    total += len;
    len = sprintf(str+total, "Open=%d\n", ADEC_BT_CheckOpen(COMMON_WISA));
    total += len;
    len = sprintf(str+total, "Connect=%d\n", IS_WISA_Connected());
    total += len;
    len = sprintf(str+total, "Gain=0x%x\n", g_AudioStatusInfo.curWISAOutGain);
    total += len;
    len = sprintf(str+total, "Mute=%d\n", g_AudioStatusInfo.curWISAMuteStatus);
    total += len;
    len = sprintf(str+total, "Delay=NA\n");
    total += len;
    len = sprintf(str+total, "ProcessingDelay=%d\n", adec_status[Aud_mainAudioIndex].Delay +
            get_ProcessingDelay(Aud_mainAudioIndex, COMMON_WISA) + g_AudioStatusInfo.curWISAOutDelay);
    total += len;

    /* SE_BT */
    len = sprintf(str+total, "\n[SE_BT]\n");
    total += len;
    len = sprintf(str+total, "Open=%d\n", ADEC_BT_CheckOpen(COMMON_SE_BT));
    total += len;
    len = sprintf(str+total, "Connect=%d\n", IS_SE_BT_Connected());
    total += len;
    len = sprintf(str+total, "Gain=0x%x\n", g_AudioStatusInfo.curSEBTOutGain);
    total += len;
    len = sprintf(str+total, "Mute=%d\n", g_AudioStatusInfo.curSEBTMuteStatus);
    total += len;
    len = sprintf(str+total, "Delay=NA\n");
    total += len;
    len = sprintf(str+total, "ProcessingDelay=%d\n", adec_status[Aud_mainAudioIndex].Delay +
            get_ProcessingDelay(Aud_mainAudioIndex, COMMON_SE_BT) + g_AudioStatusInfo.curWISAOutDelay);
    total += len;

    /* Info */
    len = sprintf(str+total, "\n[Misc]\n");
    total += len;
    len = sprintf(str+total, "ConnectedAdec=");
    total += len;
    for(i = 0; i < 2 ; i++)
    {
        len = 0;
        if(ADEC_Statue_CheckConnect(i))
        {
            len = sprintf(str+total, "%d,", i);
            total += len;
        }
    }
    if(len != 0)
        len = sprintf(str+total-1, "\n");
    else
        len = sprintf(str+total, "\n");
    total += len;
    len = sprintf(str+total, "ConnectedAmixer=");
    total += len;
    for(i = (int)HAL_AUDIO_RESOURCE_MIXER0; i <= HAL_AUDIO_RESOURCE_MIXER7 ; i++)
    {
        len = 0;
        if(ADEC_Statue_CheckConnect(i))
        if(AMIXER_Status_CheckConnect(i))
        {
            len = sprintf(str+total, "%d,", i-(int)HAL_AUDIO_RESOURCE_MIXER0+1);
            total += len;
        }
    }
    if(len != 0)
        len = sprintf(str+total-1, "\n");
    else
        len = sprintf(str+total, "\n");
    total += len;
    len = sprintf(str+total, "MainAudioOutput=%s\n", GetHalIndexName(Aud_mainAudioIndex));
    total += len;

    return total;
}

int32_t get_amixer_status(char* str)
{
    int i, len, total = 0;

    len = sprintf(str+total, "openedPort ");
    total += len;

    for(i = (int)HAL_AUDIO_RESOURCE_MIXER0; i <= HAL_AUDIO_RESOURCE_MIXER7 ; i++)
    {
        if(AMIXER_Status_CheckConnect(i))
        {
            len = sprintf(str+total, "%d,", i-(int)HAL_AUDIO_RESOURCE_MIXER0+1);
            total += len;
        }
    }

    len = sprintf(str+total-1, "\n");
    total += len;
    return total;
}

int32_t get_hdmi_AudioData(void)
{
    switch(prevHDMIMode)
    {
    case AHDMI_UNKNOWN:
    case AHDMI_DVI:
    case AHDMI_NO_AUDIO:
        return 0;
    default:
        return 1;
    }
}

int32_t get_hdmi_status(char* str)
{
    int len, total = 0, sr = 0;
    HAL_AUDIO_SRC_TYPE_T SRCType = HAL_AUDIO_SRC_TYPE_UNKNOWN;
    AUDIO_RPC_AIN_FORMAT_INFO ain_fomat;
    UINT32 retStatus;

    retStatus = ((AIN*)Aud_MainAin->pDerivedObj)->GetAudioFormatInfo(Aud_MainAin, &ain_fomat);
    if(retStatus == S_OK)
    {
        SRCType = ADECType_to_SRCType(ain_fomat.type, ain_fomat.reserved[1]);
        sr = (ain_fomat.nSamplesPerSec / 1000);
    }

    len = sprintf(str+total, "HDMIConnected=%d\n", (GetCurrentHDMIConnectPin()>=0)?1:0);
    total += len;
    len = sprintf(str+total, "AudioData=%d\n", get_hdmi_AudioData());
    total += len;
    len = sprintf(str+total, "Codec=%s\n", GetSRCTypeName(SRCType));
    total += len;
    len = sprintf(str+total, "SamplingRate=%d\n", sr);
    total += len;

    return total;
}

char* GetBandCountryName(sif_country_ext_type_t index)
{
    switch ((int)index){
        case SIF_TYPE_NONE:
            return "NONE";
        case SIF_ATSC_SELECT:
            return "ATSC";
        case SIF_KOREA_A2_SELECT:
            return "KOREA_A2";
        case SIF_BTSC_SELECT:
            return "BTSC";
        case SIF_BTSC_BR_SELECT:
            return "BTSC_BR";
        case SIF_BTSC_US_SELECT:
            return "BTSC_US";
        case SIF_DVB_SELECT:
            return "DVB";
        case SIF_DVB_ID_SELECT:
            return "DVB_ID";
        case SIF_DVB_IN_SELECT:
            return "DVB_IN";
        case SIF_DVB_CN_SELECT:
            return "DVB_CN";
        case SIF_DVB_AJJA_SELECT:
            return "DVB_AJJA";
        case SIF_TYPE_MAX:
            return "TYPE_MAX";
        default:
            return "";
	}
}

char* GetSoundSystemName(sif_soundsystem_ext_type_t index)
{
    switch ((int)index){
        case SIF_SYSTEM_UNKNOWN:
            return "UNKNOWN";
        case SIF_SYSTEM_BG:
            return "BG";
        case SIF_SYSTEM_I:
            return "I";
        case SIF_SYSTEM_DK:
            return "DK";
        case SIF_SYSTEM_L:
            return "L";
        case SIF_SYSTEM_MN:
            return "MN";
        default:
            return "";
	}
}

char* StandardString[]=
{
    "DETECT",
    "BG_NICAM",
    "BG_FM",
    "BG_A2",
    "I_NICAM",
    "I_FM",
    "DK_NICAM",
    "DK_FM",
    "DK1_A2",
    "DK2_A2",
    "DK3_A2",
    "L_NICAM",
    "L_AM",
    "MN_A2",
    "MN_BTSC",
    "MN_EIAJ",
    "NUM_SOUND_STD",
    "",
};

char* GetStandardName(sif_standard_ext_type_t index)
{
   int id;
   id = (int)index;
   if((int)index >= (sizeof(StandardString)/sizeof(StandardString[0])))
   {
       AUDIO_ERROR("unknown standard index %d \n", index);
       id = (sizeof(StandardString)/sizeof(StandardString[0])) - 1;// unknown
   }
   return StandardString[id];
}

char* CurAnalogModeString[]=
{
    "PAL_UNKNOWN",
    "PAL_MONO",
    "PAL_STEREO",
    "PAL_DUAL",
    "PAL_NICAM_MONO",
    "PAL_NICAM_STEREO",
    "PAL_NICAM_DUAL",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "NTSC_A2_UNKNOWN",	//0x10
    "NTSC_A2_MONO",
    "NTSC_A2_STEREO",
    "NTSC_A2_SAP",
    "NTSC_BTSC_UNKNOWN",
    "NTSC_BTSC_MONO",
    "NTSC_BTSC_STEREO",
    "NTSC_BTSC_SAP_MONO",
    "NTSC_BTSC_SAP_STEREO",
    "",
};

char* GetCurAnalogModeName(sif_mode_ext_type_t index)
{
   int id;
   id = (int)index;
   if((int)index >= (sizeof(CurAnalogModeString)/sizeof(CurAnalogModeString[0])))
   {
       AUDIO_ERROR("unknown CurAnalogModeString index %d \n", index);
       id = (sizeof(CurAnalogModeString)/sizeof(CurAnalogModeString[0])) - 1;// unknown
   }
   return CurAnalogModeString[id];
}

char* UserAnalogModeString[]=
{
    "PAL_UNKNOWN",
    "PAL_MONO",
    "PAL_MONO_FORCED",
    "PAL_STEREO",
    "PAL_STEREO_FORCED",
    "PAL_DUALI",
    "PAL_DUALII",
    "PAL_DUALI_II",
    "PAL_NICAM_MONO",
    "PAL_NICAM_MONO_FORCED",
    "PAL_NICAM_STEREO",
    "PAL_NICAM_STEREO_FORCED",
    "PAL_NICAM_DUALI",
    "PAL_NICAM_DUALII",
    "PAL_NICAM_DUALI_II",
    "PAL_NICAM_DUAL_FORCED",
    "NTSC_A2_UNKNOWN",	//0x10
    "NTSC_A2_MONO",
    "NTSC_A2_STEREO",
    "NTSC_A2_SAP",
    "NTSC_BTSC_UNKNOWN",
    "NTSC_BTSC_MONO",
    "NTSC_BTSC_STEREO",
    "NTSC_BTSC_SAP_MONO",
    "NTSC_BTSC_SAP_STEREO",
    "",
};

char* GetUserAnalogModeName(sif_mode_user_ext_type_t index)
{
   int id;
   id = (int)index;
   if((int)index >= (sizeof(UserAnalogModeString)/sizeof(UserAnalogModeString[0])))
   {
       AUDIO_ERROR("unknown UserAnalogModeString index %d \n", index);
       id = (sizeof(UserAnalogModeString)/sizeof(UserAnalogModeString[0])) - 1;// unknown
   }
   return UserAnalogModeString[id];
}

char* SifExistString[]=
{
    "ABSENT",
    "PRESENT",
    "DETECTION_EXSISTANCE",
    "",
};

char* GetSifExistName(sif_existence_info_ext_type_t index)
{
   int id;
   id = (int)index;
   if((int)index >= (sizeof(SifExistString)/sizeof(SifExistString[0])))
   {
       AUDIO_ERROR("unknown SifExistString index %d \n", index);
       id = (sizeof(SifExistString)/sizeof(SifExistString[0])) - 1;// unknown
   }
   return SifExistString[id];
}

int32_t get_atv_status(char* str)
{
    int len, total = 0, sr = 0;
    HAL_AUDIO_SRC_TYPE_T SRCType = HAL_AUDIO_SRC_TYPE_UNKNOWN;
    AUDIO_RPC_AIN_FORMAT_INFO ain_fomat;
    UINT32 retStatus;
    sif_country_ext_type_t Country;
    sif_soundsystem_ext_type_t SoundSystem;
    sif_standard_ext_type_t Standard;
    sif_mode_ext_type_t CurAnalogMode;
    sif_mode_user_ext_type_t UserAnalogMode;
    sif_existence_info_ext_type_t SifExist;
    uint32_t HDev;

    retStatus = ((AIN*)Aud_MainAin->pDerivedObj)->GetAudioFormatInfo(Aud_MainAin, &ain_fomat);
    if(retStatus == S_OK)
    {
        SRCType = ADECType_to_SRCType(ain_fomat.type, ain_fomat.reserved[1]);
        sr = (ain_fomat.nSamplesPerSec / 1000);
    }

    len = sprintf(str+total, "Open=%d\n", ATV_open);
    total += len;
    len = sprintf(str+total, "Connect=%d\n", ATV_connect);
    total += len;
    Audio_SIF_Get_BandCountry(&Country);
    len = sprintf(str+total, "BandCountry=%s\n", GetBandCountryName(Country));
    total += len;
    Audio_SIF_Get_BandSoundSystem(&SoundSystem);
    len = sprintf(str+total, "BandSoundSystem=%s\n", GetSoundSystemName(SoundSystem));
    total += len;
    Audio_SIF_Get_Standard(&Standard);
    len = sprintf(str+total, "Standard=%s\n", GetStandardName(Standard));
    total += len;
    Audio_SIF_Get_CurAnalogMode(&CurAnalogMode);
    len = sprintf(str+total, "CurAnalogMode=%s\n", GetCurAnalogModeName(CurAnalogMode));
    total += len;
    Audio_SIF_Get_UserAnalogMode(&UserAnalogMode);
    len = sprintf(str+total, "UserAnalogMode=%s\n", GetUserAnalogModeName(UserAnalogMode));
    total += len;
    Audio_SIF_Get_SifExiste(&SifExist);
    len = sprintf(str+total, "SifExist=%s\n", GetSifExistName(SifExist));
    total += len;
    Audio_SIF_Get_HDev(&HDev);
    len = sprintf(str+total, "HDev=%d\n", HDev);
    total += len;

    return total;
}

char* AencFormatString[]=
{
    "MP3",
    "AAC",
    "PCM",
    "Unknown",
};

char* GetAencFormatName(HAL_AUDIO_AENC_ENCODING_FORMAT_T aenc_format)
{
   int id;
   id = (int)aenc_format;
   if((int)aenc_format >= (sizeof(AencFormatString)/sizeof(AencFormatString[0])))
   {
       AUDIO_ERROR("unknown AencFormatString index %d \n", aenc_format);
       id = (sizeof(AencFormatString)/sizeof(AencFormatString[0])) - 1;// unknown
   }
   return AencFormatString[id];
}

char* AencBitrateString[]=
{
    "48K",
    "56K",
    "64K",
    "80K",
    "112K",
    "128K",
    "160K",
    "192K",
    "224K",
    "256K",
    "320K",
    "Unknown",
};

char* GetAencBitrateName(HAL_AUDIO_AENC_BITRATE_T aenc_bitrate)
{
   int id;
   id = (int)aenc_bitrate;
   if((int)aenc_bitrate >= (sizeof(AencBitrateString)/sizeof(AencBitrateString[0])))
   {
       AUDIO_ERROR("unknown AencBitrateString index %d \n", aenc_bitrate);
       id = (sizeof(AencBitrateString)/sizeof(AencBitrateString[0])) - 1;// unknown
   }
   return AencBitrateString[id];
}

int32_t get_aenc_status(char* str)
{
    int len, total = 0;

    len = sprintf(str+total, "status: %s\n", (Aud_aenc[HAL_AUDIO_AENC0]->info.status==HAL_AUDIO_AENC_STATUS_PLAY)?"playing":"idle");
    total += len;
    len = sprintf(str+total, "pts: 0x%llx\n", Aud_aenc[HAL_AUDIO_AENC0]->info.pts);
    total += len;
    len = sprintf(str+total, "decoder index: %d\n", Aud_aenc[HAL_AUDIO_AENC0]->info.dec_port);
    total += len;
    len = sprintf(str+total, "volume gain: 0x%x\n", Aud_aenc[HAL_AUDIO_AENC0]->info.gain);
    total += len;
    len = sprintf(str+total, "format: %s\n", GetAencFormatName(Aud_aenc[HAL_AUDIO_AENC0]->info.codec));
    total += len;
    len = sprintf(str+total, "bitrate: %s\n", GetAencBitrateName(Aud_aenc[HAL_AUDIO_AENC0]->info.bitrate));
    total += len;

    return total;
}

int32_t get_ProcessingDelay(HAL_AUDIO_INDEX_T main_adecIndex, common_output_ext_type_t device)
{
    int32_t dly_time = 0;
    /*char hdmi_status[128];*/
    HAL_AUDIO_RESOURCE_T decIptResId;
    HAL_AUDIO_RESOURCE_T main_adec_res;
    lgse_se_mode_ext_type_t SEmode = LGSE_MODE_UNKNOWN;
    adec_src_codec_ext_type_t CurCodec = adec_status[main_adecIndex].CurCodec;
    BOOLEAN b_atmos = FALSE;

    if(main_adecIndex == HAL_AUDIO_INDEX0)
        main_adec_res = HAL_AUDIO_RESOURCE_ADEC0;
    else if(main_adecIndex == HAL_AUDIO_INDEX1)
        main_adec_res = HAL_AUDIO_RESOURCE_ADEC1;
    decIptResId = GetNonOutputModuleSingleInputResource(ResourceStatus[main_adec_res]);

    HAL_AUDIO_LGSE_GetSoundEngineMode(&SEmode);
    if(SEmode == LGSE_MODE_LGSE_ATMOS)
        b_atmos = TRUE;
    else
        b_atmos = FALSE;

    if(decIptResId == HAL_AUDIO_RESOURCE_ADC || decIptResId == HAL_AUDIO_RESOURCE_AAD){
        switch(device)
        {
        case COMMON_SPK:
            if(b_atmos == FALSE) dly_time = 51;
            else dly_time = 69;
            break;
        case COMMON_OPTIC:
        case COMMON_OPTIC_LG:
        case COMMON_ARC:
            dly_time = 51; break;
        case COMMON_HP:
        case COMMON_BLUETOOTH:
        case COMMON_WISA:
            dly_time = 50; break;
        case COMMON_SE_BT:
            dly_time = 69; break;
        default:
            dly_time = 0; break;
        }
    }else if(decIptResId == HAL_AUDIO_RESOURCE_HDMI || (decIptResId >= HAL_AUDIO_RESOURCE_HDMI0 && decIptResId <= HAL_AUDIO_RESOURCE_HDMI3)){
        if(device == COMMON_SPK){
            if(CurCodec == ADEC_SRC_CODEC_PCM){
                if(b_atmos == TRUE) dly_time = 69;
                else dly_time = 51;
            }else{
                switch(CurCodec)
                {
                case ADEC_SRC_CODEC_AC3:
                case ADEC_SRC_CODEC_EAC3:
                    dly_time = 131; break;
                case ADEC_SRC_CODEC_EAC3_ATMOS:
                    dly_time = 168; break;
                case ADEC_SRC_CODEC_AC4:
                case ADEC_SRC_CODEC_AC4_ATMOS:
                    dly_time = 157; break;
                case ADEC_SRC_CODEC_MAT:
                case ADEC_SRC_CODEC_MAT_ATMOS:
                case ADEC_SRC_CODEC_TRUEHD:
                case ADEC_SRC_CODEC_TRUEHD_ATMOS:
                    dly_time = 84; break;
                default:
                    break;
                }
                if(b_atmos == TRUE) dly_time += 12;

                if(CurCodec == ADEC_SRC_CODEC_HEAAC || CurCodec == ADEC_SRC_CODEC_AAC) dly_time = 108;
                else if(CurCodec == ADEC_SRC_CODEC_MPEG ) dly_time = 98;
            }
#if 0 // resample delay
            get_hdmi_status(hdmi_status);
            if(strstr(hdmi_status, "SamplingRate=48") == NULL)  // non 48k
                dly_time += 4;
#endif
        }else if((device == COMMON_ARC)){
            if(CurCodec == ADEC_SRC_CODEC_PCM){
                dly_time = 38;
            }else if(CurCodec == ADEC_SRC_CODEC_AC3 || CurCodec == ADEC_SRC_CODEC_EAC3){
                switch(_AudioARCMode)
                {
                case ENABLE_DOWNMIX:
                    dly_time = 103; break;
                case NON_PCM_OUT_EN_AUTO_BYPASS:
                case NON_PCM_OUT_EN_AUTO_BYPASS_DDP:
                    dly_time = 85; break;
                case NON_PCM_OUT_EN_AUTO:
                case NON_PCM_OUT_EN_AUTO_DDP:
                case NON_PCM_OUT_EN_AUTO_FORCED_DDP:
                    dly_time = 152; break;
                default:
                    break;
                }
                if((HDMI_earcOnOff == TRUE) && (_AudioEARCMode == NON_PCM_OUT_EN_AUTO_MAT || _AudioEARCMode == NON_PCM_OUT_EN_AUTO_FORCED_MAT))
                    dly_time = 165;
            }else if(CurCodec == ADEC_SRC_CODEC_EAC3_ATMOS){
                switch(_AudioARCMode)
                {
                case ENABLE_DOWNMIX:
                    dly_time = 127; break;
                case NON_PCM_OUT_EN_AUTO_BYPASS:
                case NON_PCM_OUT_EN_AUTO_BYPASS_DDP:
                    dly_time = 85; break;
                case NON_PCM_OUT_EN_AUTO:
                case NON_PCM_OUT_EN_AUTO_DDP:
                case NON_PCM_OUT_EN_AUTO_FORCED_DDP:
                    dly_time = 177; break;
                default:
                    break;
                }
                if((HDMI_earcOnOff == TRUE) && (_AudioEARCMode == NON_PCM_OUT_EN_AUTO_MAT || _AudioEARCMode == NON_PCM_OUT_EN_AUTO_FORCED_MAT))
                    dly_time = 165;
            }else if(CurCodec == ADEC_SRC_CODEC_TRUEHD || CurCodec == ADEC_SRC_CODEC_TRUEHD_ATMOS){
                switch(_AudioARCMode)
                {
                case ENABLE_DOWNMIX:
                    dly_time = 59; break;
                case NON_PCM_OUT_EN_AUTO:
                case NON_PCM_OUT_EN_AUTO_DDP:
                case NON_PCM_OUT_EN_AUTO_FORCED_DDP:
                    dly_time = 109; break;
                default:
                    break;
                }
                if((HDMI_earcOnOff == TRUE) && (_AudioEARCMode == NON_PCM_OUT_EN_AUTO_BYPASS_MAT))
                    dly_time = 69;
                else if((HDMI_earcOnOff == TRUE) && (_AudioEARCMode == NON_PCM_OUT_EN_AUTO_MAT || _AudioEARCMode == NON_PCM_OUT_EN_AUTO_FORCED_MAT))
                    dly_time = 97;
            }else if(CurCodec == ADEC_SRC_CODEC_MAT || CurCodec == ADEC_SRC_CODEC_MAT_ATMOS){
                switch(_AudioARCMode)
                {
                case ENABLE_DOWNMIX:
                    dly_time = 56; break;
                case NON_PCM_OUT_EN_AUTO:
                case NON_PCM_OUT_EN_AUTO_DDP:
                case NON_PCM_OUT_EN_AUTO_FORCED_DDP:
                    dly_time = 105; break;
                default:
                    break;
                }
                if((HDMI_earcOnOff == TRUE) && (_AudioEARCMode == NON_PCM_OUT_EN_AUTO_BYPASS_MAT))
                    dly_time = 66;
                else if((HDMI_earcOnOff == TRUE) && (_AudioEARCMode == NON_PCM_OUT_EN_AUTO_MAT || _AudioEARCMode == NON_PCM_OUT_EN_AUTO_FORCED_MAT))
                    dly_time = 93;
            }else if(CurCodec == ADEC_SRC_CODEC_AC4 || CurCodec == ADEC_SRC_CODEC_AC4_ATMOS){
                switch(_AudioARCMode)
                {
                case ENABLE_DOWNMIX:
                    dly_time = 131; break;
                case NON_PCM_OUT_EN_AUTO:
                case NON_PCM_OUT_EN_AUTO_DDP:
                case NON_PCM_OUT_EN_AUTO_FORCED_DDP:
                    dly_time = 180; break;
                default:
                    break;
                }
                if((HDMI_earcOnOff == TRUE) && (_AudioEARCMode == NON_PCM_OUT_EN_AUTO_MAT || _AudioEARCMode == NON_PCM_OUT_EN_AUTO_FORCED_MAT))
                    dly_time = 168;
            }else if(CurCodec == ADEC_SRC_CODEC_AAC || CurCodec == ADEC_SRC_CODEC_HEAAC){
                switch(_AudioARCMode)
                {
                case ENABLE_DOWNMIX:
                    dly_time = 98; break;
                case NON_PCM_OUT_EN_AUTO:
                case NON_PCM_OUT_EN_AUTO_DDP:
                case NON_PCM_OUT_EN_AUTO_FORCED_DDP:
                    dly_time = 129; break;
                default:
                    break;
                }
            }else if(CurCodec == ADEC_SRC_CODEC_MPEG){
                switch(_AudioARCMode)
                {
                case ENABLE_DOWNMIX:
                    dly_time = 94; break;
                case NON_PCM_OUT_EN_AUTO:
                case NON_PCM_OUT_EN_AUTO_DDP:
                case NON_PCM_OUT_EN_AUTO_FORCED_DDP:
                    dly_time = 125; break;
                default:
                    break;
                }
            }
        }else if((device == COMMON_OPTIC) || (device == COMMON_OPTIC_LG)){
            if(CurCodec == ADEC_SRC_CODEC_PCM){
                dly_time = 38;
            }else if(CurCodec == ADEC_SRC_CODEC_AC3 || CurCodec == ADEC_SRC_CODEC_EAC3){
                switch(_AudioSPDIFMode)
                {
                case ENABLE_DOWNMIX:
                    dly_time = 103; break;
                case NON_PCM_OUT_EN_AUTO_BYPASS:
                    dly_time = 85; break;
                case NON_PCM_OUT_EN_AUTO:
                case NON_PCM_OUT_EN_AUTO_FORCED_AC3:
                    dly_time = 152; break;
                default:
                    break;
                }
            }else if(CurCodec == ADEC_SRC_CODEC_EAC3_ATMOS){
                switch(_AudioSPDIFMode)
                {
                case ENABLE_DOWNMIX:
                    dly_time = 127; break;
                case NON_PCM_OUT_EN_AUTO_BYPASS:
                    dly_time = 85; break;
                case NON_PCM_OUT_EN_AUTO:
                case NON_PCM_OUT_EN_AUTO_FORCED_AC3:
                    dly_time = 177; break;
                default:
                    break;
                }
            }else if(CurCodec == ADEC_SRC_CODEC_TRUEHD || CurCodec == ADEC_SRC_CODEC_TRUEHD_ATMOS){
                switch(_AudioSPDIFMode)
                {
                case ENABLE_DOWNMIX:
                    dly_time = 59; break;
                case NON_PCM_OUT_EN_AUTO:
                case NON_PCM_OUT_EN_AUTO_FORCED_AC3:
                    dly_time = 109; break;
                default:
                    break;
                }
            }else if(CurCodec == ADEC_SRC_CODEC_MAT || CurCodec == ADEC_SRC_CODEC_MAT_ATMOS){
                switch(_AudioSPDIFMode)
                {
                case ENABLE_DOWNMIX:
                    dly_time = 56; break;
                case NON_PCM_OUT_EN_AUTO:
                case NON_PCM_OUT_EN_AUTO_FORCED_AC3:
                    dly_time = 105; break;
                default:
                    break;
                }
            }else if(CurCodec == ADEC_SRC_CODEC_AC4 || CurCodec == ADEC_SRC_CODEC_AC4_ATMOS){
                switch(_AudioSPDIFMode)
                {
                case ENABLE_DOWNMIX:
                    dly_time = 131; break;
                case NON_PCM_OUT_EN_AUTO:
                case NON_PCM_OUT_EN_AUTO_FORCED_AC3:
                    dly_time = 180; break;
                default:
                    break;
                }
            }else if(CurCodec == ADEC_SRC_CODEC_AAC || CurCodec == ADEC_SRC_CODEC_HEAAC){
                switch(_AudioSPDIFMode)
                {
                case ENABLE_DOWNMIX:
                    dly_time = 98; break;
                case NON_PCM_OUT_EN_AUTO:
                case NON_PCM_OUT_EN_AUTO_FORCED_AC3:
                    dly_time = 129; break;
                default:
                    break;
                }
            }else if(CurCodec == ADEC_SRC_CODEC_MPEG){
                switch(_AudioSPDIFMode)
                {
                case ENABLE_DOWNMIX:
                    dly_time = 94; break;
                case NON_PCM_OUT_EN_AUTO:
                case NON_PCM_OUT_EN_AUTO_FORCED_AC3:
                    dly_time = 125; break;
                default:
                    break;
                }
            }
        }else if((device == COMMON_HP) || (device == COMMON_BLUETOOTH) || (device == COMMON_WISA) || (device == COMMON_SE_BT)){
            switch(CurCodec)
            {
                case ADEC_SRC_CODEC_PCM:
                    dly_time = 38; break;
                case ADEC_SRC_CODEC_AC3:
                case ADEC_SRC_CODEC_EAC3:
                    dly_time = 118; break;
                case ADEC_SRC_CODEC_EAC3_ATMOS:
                    dly_time = 155; break;
                case ADEC_SRC_CODEC_AC4:
                case ADEC_SRC_CODEC_AC4_ATMOS:
                    dly_time = 104; break;
                case ADEC_SRC_CODEC_MAT:
                case ADEC_SRC_CODEC_MAT_ATMOS:
                    dly_time = 72; break;
                case ADEC_SRC_CODEC_TRUEHD:
                case ADEC_SRC_CODEC_TRUEHD_ATMOS:
                    dly_time = 75; break;
                case ADEC_SRC_CODEC_HEAAC:
                case ADEC_SRC_CODEC_AAC:
                    dly_time = 96; break;
                case ADEC_SRC_CODEC_MPEG:
                    dly_time = 86; break;
                default:
                    break;
            }
            if((device == COMMON_SE_BT)) dly_time += 12;
        }
    }else if((decIptResId == HAL_AUDIO_RESOURCE_ATP0) || (decIptResId == HAL_AUDIO_RESOURCE_ATP1)){
        if(device == COMMON_SPK){
            if((CurCodec == ADEC_SRC_CODEC_HEAAC) || (CurCodec == ADEC_SRC_CODEC_AAC) ||
                    (CurCodec >= ADEC_SRC_CODEC_AC3 && CurCodec <= ADEC_SRC_CODEC_AC4_ATMOS))
                dly_time = 19;
            if(CurCodec != ADEC_SRC_CODEC_HEAAC && CurCodec != ADEC_SRC_CODEC_AAC && b_atmos == TRUE)
                dly_time += 12;
            if(CurCodec == ADEC_SRC_CODEC_MPEG)
                dly_time = 13;
        }else if((device == COMMON_OPTIC) || (device == COMMON_OPTIC_LG) || (device == COMMON_ARC)){
            dly_time = 40;
        }else if((device == COMMON_HP) || (device == COMMON_BLUETOOTH) || (device == COMMON_WISA) || (device == COMMON_SE_BT)){
            if((CurCodec == ADEC_SRC_CODEC_HEAAC) || (CurCodec == ADEC_SRC_CODEC_AAC) ||
                    (CurCodec >= ADEC_SRC_CODEC_AC3 && CurCodec <= ADEC_SRC_CODEC_AC4_ATMOS))
                dly_time = 7;
            else if (CurCodec == ADEC_SRC_CODEC_MPEG)
                dly_time = 1;
            if((device == COMMON_SE_BT)) dly_time += 12;
        }
    }

    return dly_time;
}
