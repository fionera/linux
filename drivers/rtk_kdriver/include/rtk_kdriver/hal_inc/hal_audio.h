/******************************************************************************
 *   DTV LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 *   Copyright(c) 1999 by LG Electronics Inc.
 *
 *   All rights reserved. No part of this work may be reproduced, stored in a
 *   retrieval system, or transmitted by any means without prior written
 *   permission of LG Electronics Inc.
 *****************************************************************************/

/** @file hal_audio.h
 *
 *  HAL 함수 header 파일.
 *
 *
 *  @author		yong kwang kim(ykwang.kim@lge.com)
 *  @version	4.10
 *  @date		2018.09.03
 *  @note
 *  @see
 */

/******************************************************************************
 	Header File Guarder
******************************************************************************/
#ifndef _HAL_AUDIO_H_
#define _HAL_AUDIO_H_

/******************************************************************************
 #include 파일들 (File Inclusions)
******************************************************************************/
#include "hal_common.h"

#include "linux/alsa-ext/alsa-ext-common.h"
#include "linux/alsa-ext/alsa-ext-adec.h"
#include "linux/alsa-ext/alsa-ext-adc.h"
#include "linux/alsa-ext/alsa-ext-aenc.h"
#include "linux/alsa-ext/alsa-ext-direct.h"
#include "linux/alsa-ext/alsa-ext-sndout.h"
#include "linux/alsa-ext/alsa-ext-hdmi.h"
#include "linux/alsa-ext/alsa-ext-mute.h"
#include "linux/alsa-ext/alsa-ext-delay.h"
#include "linux/alsa-ext/alsa-ext-gain.h"
#include "linux/alsa-ext/alsa-ext-lgse.h"
#include "linux/alsa-ext/alsa-ext-atv.h"
#include "linux/linuxtv-ext-ver.h"

#define ALSA_GAIN_0dB (0x800000)

#include "AudioInbandAPI.h"
/******************************************************************************
 	상수 정의(Constant Definitions)
******************************************************************************/
/**
 * HAL AUDIO LGSE SMARTSOUND CALLBACK DATA Length(UINT32).
 *
 */
#define HAL_AUDIO_LGSE_SMARTSOUND_CB_LENGTH		7

/**
 * HAL AUDIO DAP count of bands (UINT8).
 *
 */
#define HAL_AUDIO_LGSE_DAP_MAX_BAND_COUNT		20
#define HAL_AUDIO_LGSE_DAP_MAX_CHANNEL_COUNT	8

#define ALSA_AMIXER_BASE (8)

/******************************************************************************
    매크로 함수 정의 (Macro Definitions)
******************************************************************************/

#define AUDIO_CHECK_ISBOOLEAN(value) \
        ((value == 1) || (value == 0))
#define AUDIO_CHECK_ISVALID(target,min,max) \
        ((target >= min) && (target <= max))

/******************************************************************************
	형 정의 (Type Definitions)
******************************************************************************/
/**
 * HAL AUDIO MS12 Version.
 *
 */
typedef  enum
{
	HAL_AUDIO_MS12_1_3					= 0, /*MS12 v1.3 Dose not support TVATMOS, AC4*//*Default value*/
	HAL_AUDIO_MS12_2_0					= 1, /*MS12 v2.0 Support TVATMOS, AC4*/
	HAL_AUDIO_MS12_2_0_ATMOS_FLAVOR		= 2, /*MS12 v2.0 is Supported without TVATMOS*/
	HAL_AUDIO_MS12_MAX	= HAL_AUDIO_MS12_2_0_ATMOS_FLAVOR,
} HAL_AUDIO_MS12_VERSION_T;


typedef struct {
	unsigned int  msg_id;
	unsigned int value[15];
} AUDIO_CONFIG_COMMAND_RTKAUDIO;
/**
 * HAL AUDIO Decoder Index.
 *
 */
typedef  enum
{
	HAL_AUDIO_ADEC0		= 0,
	HAL_AUDIO_ADEC1		= 1,
	HAL_AUDIO_ADEC2		= 2,
	HAL_AUDIO_ADEC3		= 3,
	HAL_AUDIO_ADEC_MAX	= HAL_AUDIO_ADEC3,
} HAL_AUDIO_ADEC_INDEX_T;

/**
 * HAL AUDIO Index.
 *
 */
typedef  enum
{
	HAL_AUDIO_INDEX0	= 0, /*Audio Decoder Input 0*/
	HAL_AUDIO_INDEX1	= 1, /*Audio Decoder Input 1*/
	HAL_AUDIO_INDEX2	= 2, /*Audio Decoder Input 2*/
	HAL_AUDIO_INDEX3	= 3, /*Audio Decoder Input 3*/
	HAL_AUDIO_INDEX4	= 4, /*Audio Mixer Input 0*/
	HAL_AUDIO_INDEX5	= 5, /*Audio Mixer Input 1*/
	HAL_AUDIO_INDEX6	= 6, /*Audio Mixer Input 2*/
	HAL_AUDIO_INDEX7	= 7, /*Audio Mixer Input 3*/
	HAL_AUDIO_INDEX8	= 8, /*Audio Mixer Input 4*/
	HAL_AUDIO_INDEX9	= 9, /*Audio Mixer Input 5*/
	HAL_AUDIO_INDEX10	= 10, /*Audio Mixer Input 6*/
	HAL_AUDIO_INDEX11	= 11, /*Audio Mixer Input 7*/
	HAL_AUDIO_INDEX_MAX	= HAL_AUDIO_INDEX11,
} HAL_AUDIO_INDEX_T;


/**
 * HAL AUDIO Mixer Index.
 *
 */
typedef  enum
{
	HAL_AUDIO_MIXER0	= 0,	/* G-Streamer Mixer Input 0 */
	HAL_AUDIO_MIXER1	= 1,	/* G-Streamer Mixer Input 1 */
	HAL_AUDIO_MIXER2	= 2,	/* G-Streamer Mixer Input 2 */
	HAL_AUDIO_MIXER3	= 3,	/* G-Streamer Mixer Input 3 */
	HAL_AUDIO_MIXER4	= 4,	/* G-Streamer Mixer Input 4 */
	HAL_AUDIO_MIXER5	= 5,	/* HAL  AUDIO Mixer Input 5 */
	HAL_AUDIO_MIXER6	= 6,	/* ALSA AUDIO Mixer Input 6 */
	HAL_AUDIO_MIXER7	= 7,	/* ALSA AUDIO Mixer Input 7 */
	HAL_AUDIO_MIXER_MAX	= HAL_AUDIO_MIXER7,
} HAL_AUDIO_MIXER_INDEX_T;

/**
 * HAL AUDIO TP Index.
 *
 */
typedef  enum
{
	HAL_AUDIO_TP0		= 0,
	HAL_AUDIO_TP1		= 1,
	HAL_AUDIO_TP_MAX	= HAL_AUDIO_TP1,
} HAL_AUDIO_TP_INDEX_T;

/**
 * HAL AUDIO HDMI Port Index.
 * HAL_AUDIO_HDMI_SWITCH is defined for HDMI Switch Model and 1 Port Only in SoC.
 *
 */
typedef  enum
{
	HAL_AUDIO_HDMI0			= 0,	/* H15  : HDMI2.0 Port 0. */
	HAL_AUDIO_HDMI1			= 1,	/* H15  : HDMI2.0 Port 1. */
	HAL_AUDIO_HDMI2 		= 2,	/* H15  : HDMI1.4 Port 0 with ARC. */
	HAL_AUDIO_HDMI3 		= 3,	/* H15  : HDMI1.4 Port 1 with MHL. */
	HAL_AUDIO_HDMI_SWITCH 	= 4,	/* M14+ : HDMI1.4 Port with HDMI Switch Model. */
	HAL_AUDIO_HDMI_MAX		= HAL_AUDIO_HDMI_SWITCH,
} HAL_AUDIO_HDMI_INDEX_T;

/**
 * HAL AUDIO Sound Output Mode.
 *
 */
typedef  enum
{
	HAL_AUDIO_NO_OUTPUT		= 0x00,
	HAL_AUDIO_SPK			= 0x01,
	HAL_AUDIO_SPDIF			= 0x02,
	HAL_AUDIO_SB_SPDIF		= 0x04,
	HAL_AUDIO_SB_PCM		= 0x08,
	HAL_AUDIO_HP			= 0x10,
	HAL_AUDIO_ARC			= 0x20,
	HAL_AUDIO_WISA			= 0x40,
	HAL_AUDIO_SE_BT 		= 0x80,
	HAL_AUDIO_MAX_OUTPUT	= 0x100,
} HAL_AUDIO_SNDOUT_T;

typedef  enum
{
    INDEX_SPK         = 0,
    INDEX_OPTIC       = 1,
    INDEX_OPTIC_LG    = 2,
    INDEX_BLUETOOTH   = 3,
    INDEX_HP          = 4,
    INDEX_ARC         = 5,
    INDEX_WISA        = 6,
    INDEX_SE_BT       = 7,
    INDEX_SNDOUT_MAX  = 8,
} AUDIO_SNDOUT_INDEX;

/**
 * HAL AUDIO Sound Output LR Mode.
 *
 */
typedef enum
{
	HAL_AUDIO_SNDOUT_LRMODE_LR 		= 0,
	HAL_AUDIO_SNDOUT_LRMODE_LL 		= 1,
	HAL_AUDIO_SNDOUT_LRMODE_RR 		= 2,
	HAL_AUDIO_SNDOUT_LRMODE_MIX		= 3,
} HAL_AUDIO_SNDOUT_LRMODE_T;

/**
 * HAL AUDIO Resource Type for Inter Connection.
 *
 */
typedef  enum
{
	HAL_AUDIO_RESOURCE_SDEC0			=  0,
	HAL_AUDIO_RESOURCE_SDEC1			=  1,
	HAL_AUDIO_RESOURCE_ATP0				=  2,
	HAL_AUDIO_RESOURCE_ATP1				=  3,
	HAL_AUDIO_RESOURCE_ADC				=  4,
	HAL_AUDIO_RESOURCE_HDMI				=  5,
	HAL_AUDIO_RESOURCE_AAD				=  6,
	HAL_AUDIO_RESOURCE_SYSTEM			=  7,	/* Clip or LMF Play */
	HAL_AUDIO_RESOURCE_ADEC0			=  8,
	HAL_AUDIO_RESOURCE_ADEC1			=  9,
	HAL_AUDIO_RESOURCE_AENC0			= 10,
	HAL_AUDIO_RESOURCE_AENC1			= 11,
	HAL_AUDIO_RESOURCE_SE				= 12,
	HAL_AUDIO_RESOURCE_OUT_SPK			= 13,	/* Speaker */
	HAL_AUDIO_RESOURCE_OUT_SPDIF		= 14,	/* SPDIF Ouput */
	HAL_AUDIO_RESOURCE_OUT_SB_SPDIF		= 15,	/* Sound Bar(SPDIF) : Mixer Output */
	HAL_AUDIO_RESOURCE_OUT_SB_PCM		= 16,	/* Sound Bar(PCM)   : Mixer Output(Wireless) */
	HAL_AUDIO_RESOURCE_OUT_SB_CANVAS	= 17,	/* Sound Bar(CANVAS): Sound Engine Output */
	HAL_AUDIO_RESOURCE_OUT_HP			= 18,	/* Must be controlled by audio decoder.*/
	HAL_AUDIO_RESOURCE_OUT_ARC			= 19,	/* ARC */

	HAL_AUDIO_RESOURCE_MIXER0			= 20,	/* Audio Mixer Input 0. */
	HAL_AUDIO_RESOURCE_MIXER1			= 21,	/* Audio Mixer Input 1. */
	HAL_AUDIO_RESOURCE_MIXER2			= 22,	/* Audio Mixer Input 2. */
	HAL_AUDIO_RESOURCE_MIXER3			= 23,	/* Audio Mixer Input 3. */
	HAL_AUDIO_RESOURCE_MIXER4			= 24,	/* Audio Mixer Input 4. */
	HAL_AUDIO_RESOURCE_MIXER5			= 25,	/* Audio Mixer Input 5. */
	HAL_AUDIO_RESOURCE_MIXER6			= 26,	/* Audio Mixer Input 6. */
	HAL_AUDIO_RESOURCE_MIXER7			= 27,	/* Audio Mixer Input 7. */
	HAL_AUDIO_RESOURCE_OUT_SPDIF_ES		= 28,	/* SPDIF ES Ouput Only */
	HAL_AUDIO_RESOURCE_HDMI0			= 29,	/* Audio HDMI Input 0. */
	HAL_AUDIO_RESOURCE_HDMI1			= 30,	/* Audio HDMI Input 1. */
	HAL_AUDIO_RESOURCE_HDMI2			= 31,	/* Audio HDMI Input 2. */
	HAL_AUDIO_RESOURCE_HDMI3			= 32,	/* Audio HDMI Input 3. */
	HAL_AUDIO_RESOURCE_SWITCH			= 33,	/* Audio HDMI Input with switch. */
	HAL_AUDIO_RESOURCE_ADEC2			= 34,
	HAL_AUDIO_RESOURCE_ADEC3			= 35,
	HAL_AUDIO_RESOURCE_OUT_WISA			= 36,
	HAL_AUDIO_RESOURCE_NO_CONNECTION 	= 0XFF,
} HAL_AUDIO_RESOURCE_T;

typedef enum {
  DEMUX_CH_A = 0,
  DEMUX_CH_B,
  DEMUX_CH_C,
  DEMUX_CH_D,
  DEMUX_CH_NUM

} DEMUX_CHANNEL_T ;

typedef enum
{
  RSDEC_ENUM_TYPE_DEST_VDEC0,
  RSDEC_ENUM_TYPE_DEST_VDEC1,
  RSDEC_ENUM_TYPE_DEST_ADEC0,
  RSDEC_ENUM_TYPE_DEST_ADEC1,
  RSDEC_ENUM_TYPE_DEST_TSO,    // for Dump TS

} RSDEC_ENUM_TYPE_DEST_T ;

/**
 * HAL AUDIO Source Format Type.
 *
 */
typedef  enum
{
	HAL_AUDIO_SRC_TYPE_UNKNOWN      = 0,
	HAL_AUDIO_SRC_TYPE_PCM          = 1,
	HAL_AUDIO_SRC_TYPE_AC3          = 2,
	HAL_AUDIO_SRC_TYPE_EAC3         = 3,
	HAL_AUDIO_SRC_TYPE_MPEG         = 4,
	HAL_AUDIO_SRC_TYPE_AAC          = 5,
	HAL_AUDIO_SRC_TYPE_HEAAC        = 6,
	HAL_AUDIO_SRC_TYPE_DRA			= 7,
	HAL_AUDIO_SRC_TYPE_MP3			= 8,
	HAL_AUDIO_SRC_TYPE_DTS			= 9,
	HAL_AUDIO_SRC_TYPE_SIF			= 10,
	HAL_AUDIO_SRC_TYPE_SIF_BTSC		= 11,
	HAL_AUDIO_SRC_TYPE_SIF_A2		= 12,
	HAL_AUDIO_SRC_TYPE_DEFAULT		= 13,
	HAL_AUDIO_SRC_TYPE_NONE			= 14,
	HAL_AUDIO_SRC_TYPE_DTS_HD_MA 	= 15,
	HAL_AUDIO_SRC_TYPE_DTS_EXPRESS 	= 16,
	HAL_AUDIO_SRC_TYPE_DTS_CD		= 17,
	HAL_AUDIO_SRC_TYPE_EAC3_ATMOS	= 18,
	HAL_AUDIO_SRC_TYPE_AC4          = 19,
	HAL_AUDIO_SRC_TYPE_AC4_ATMOS	= 20,
	HAL_AUDIO_SRC_TYPE_MPEG_H		= 21,
	HAL_AUDIO_SRC_TYPE_MAT		    = 22,
	HAL_AUDIO_SRC_TYPE_MAT_ATMOS    = 23,
	HAL_AUDIO_SRC_TYPE_TRUEHD       = 24,
	HAL_AUDIO_SRC_TYPE_TRUEHD_ATMOS = 25,
	HAL_AUDIO_SRC_TYPE_WMA_PRO      = 26,
	HAL_AUDIO_SRC_TYPE_VORBIS       = 27,
	HAL_AUDIO_SRC_TYPE_AMR_WB       = 28,
	HAL_AUDIO_SRC_TYPE_AMR_NB       = 29,
	HAL_AUDIO_SRC_TYPE_ADPCM        = 30,
	HAL_AUDIO_SRC_TYPE_RA8          = 31,
	HAL_AUDIO_SRC_TYPE_FLAC         = 32,
} HAL_AUDIO_SRC_TYPE_T;

/**
 * HAL AUDIO Source Input Type.
 * TP, PCM, SPDIF
 *
 */
typedef  enum
{
	HAL_AUDIO_IN_PORT_NONE		=  0,
	HAL_AUDIO_IN_PORT_TP		=  1,	// From TPA Stream Input
	HAL_AUDIO_IN_PORT_SPDIF 	=  2,	// From SERIAL INTERFACE 0
	HAL_AUDIO_IN_PORT_SIF		=  3,	// From Analog Front End (SIF)
	HAL_AUDIO_IN_PORT_ADC		=  4,	// Fron ADC Input
	HAL_AUDIO_IN_PORT_HDMI		=  5,	// From HDMI
	HAL_AUDIO_IN_PORT_I2S		=  6,	// From I2S
	HAL_AUDIO_IN_PORT_SYSTEM	=  7,	// From System
} HAL_AUDIO_IN_PORT_T;

/**
 * HAL AUDIO Dual-Mono Output Mode Type.
 *
 */
typedef enum
{
	HAL_AUDIO_DUALMONO_MODE_LR		= 0,
	HAL_AUDIO_DUALMONO_MODE_LL		= 1,
	HAL_AUDIO_DUALMONO_MODE_RR		= 2,
	HAL_AUDIO_DUALMONO_MODE_MIX		= 3,
} HAL_AUDIO_DUALMONO_MODE_T;

/**
 * HAL AUDIO SPDIF Type.
 *
 */
typedef  enum
{
	HAL_AUDIO_SPDIF_NONE		= 0,
	HAL_AUDIO_SPDIF_PCM		= 1,
	HAL_AUDIO_SPDIF_AUTO		= 2,
	HAL_AUDIO_SPDIF_AUTO_AAC	= 3,
	HAL_AUDIO_SPDIF_HALF_AUTO	= 4,
	HAL_AUDIO_SPDIF_HALF_AUTO_AAC	= 5,
	HAL_AUDIO_SPDIF_FORCED_AC3      = 6,
	HAL_AUDIO_SPDIF_BYPASS		= 7,
	HAL_AUDIO_SPDIF_BYPASS_AAC	= 8,
} HAL_AUDIO_SPDIF_MODE_T;

/**
 * HAL AUDIO ARC Type. 2016/06/04 by ykwang.kim
 *
 */
typedef  enum
{
	HAL_AUDIO_ARC_NONE			= 0,
	HAL_AUDIO_ARC_PCM 			= 1,
	HAL_AUDIO_ARC_AUTO			= 2,
	HAL_AUDIO_ARC_AUTO_AAC			= 3,
	HAL_AUDIO_ARC_AUTO_EAC3			= 4,
	HAL_AUDIO_ARC_AUTO_EAC3_AAC 		= 5,
	HAL_AUDIO_ARC_HALF_AUTO			= 6,
	HAL_AUDIO_ARC_HALF_AUTO_AAC		= 7,
	HAL_AUDIO_ARC_HALF_AUTO_EAC3		= 8,
	HAL_AUDIO_ARC_HALF_AUTO_EAC3_AAC	= 9,
	HAL_AUDIO_ARC_FORCED_AC3            	= 10,
	HAL_AUDIO_ARC_FORCED_EAC3           	= 11,
	HAL_AUDIO_ARC_BYPASS			= 12,
	HAL_AUDIO_ARC_BYPASS_AAC		= 13,
	HAL_AUDIO_ARC_BYPASS_EAC3		= 14,
	HAL_AUDIO_ARC_BYPASS_EAC3_AAC		= 15,
} HAL_AUDIO_ARC_MODE_T;

/**
 * HAL AUDIO HDMI Format Type.
 *
 */
typedef enum
{
	HAL_AUDIO_HDMI_DVI			= 0,
	HAL_AUDIO_HDMI_NO_AUDIO		= 1,
	HAL_AUDIO_HDMI_PCM			= 2,
	HAL_AUDIO_HDMI_AC3			= 3,
	HAL_AUDIO_HDMI_DTS			= 4,
	HAL_AUDIO_HDMI_AAC			= 5,
	HAL_AUDIO_HDMI_DEFAULT		= 6,
	HAL_AUDIO_HDMI_MPEG			= 10,
	HAL_AUDIO_HDMI_DTS_HD_MA	= 11,
	HAL_AUDIO_HDMI_DTS_EXPRESS	= 12,
	HAL_AUDIO_HDMI_DTS_CD		= 13,
	HAL_AUDIO_HDMI_EAC3			= 14,
	HAL_AUDIO_HDMI_EAC3_ATMOS	= 15,
	HAL_AUDIO_HDMI_MAT		    = 16,
	HAL_AUDIO_HDMI_MAT_ATMOS    = 17,
	HAL_AUDIO_HDMI_TRUEHD       = 18,
	HAL_AUDIO_HDMI_TRUEHD_ATMOS = 19,
} HAL_AUDIO_HDMI_TYPE_T;

/**
 * AUDIO Sampling Frequency Index.
 */
typedef enum
{
	HAL_AUDIO_SAMPLING_FREQ_NONE		=	     0,
	HAL_AUDIO_SAMPLING_FREQ_4_KHZ		=	  4000,
	HAL_AUDIO_SAMPLING_FREQ_8_KHZ		=	  8000,
	HAL_AUDIO_SAMPLING_FREQ_11_025KHZ	=	 11025,
	HAL_AUDIO_SAMPLING_FREQ_12_KHZ		=	 12000,
	HAL_AUDIO_SAMPLING_FREQ_16_KHZ		=	 16000,
	HAL_AUDIO_SAMPLING_FREQ_22_05KHZ	=	 22050,
	HAL_AUDIO_SAMPLING_FREQ_24_KHZ		=	 24000,
	HAL_AUDIO_SAMPLING_FREQ_32_KHZ		=	 32000,
	HAL_AUDIO_SAMPLING_FREQ_44_1KHZ		=	 44100,
	HAL_AUDIO_SAMPLING_FREQ_48_KHZ		=	 48000,
	HAL_AUDIO_SAMPLING_FREQ_64_KHZ		=	 64000,
	HAL_AUDIO_SAMPLING_FREQ_88_2KHZ		=	 88200,
	HAL_AUDIO_SAMPLING_FREQ_96_KHZ		=	 96000,
	HAL_AUDIO_SAMPLING_FREQ_128_KHZ		=	128000,
	HAL_AUDIO_SAMPLING_FREQ_176_4KHZ	=	176400,
	HAL_AUDIO_SAMPLING_FREQ_192_KHZ		=	192000,
	HAL_AUDIO_SAMPLING_FREQ_768_KHZ		=	768000,
	HAL_AUDIO_SAMPLING_FREQ_DEFAULT		=	999000,
} HAL_AUDIO_SAMPLING_FREQ_T;

/**
 * HAL AUDIO Country Type.
 *
 */
typedef  enum
{
	HAL_AUDIO_SIF_TYPE_NONE				= 0x0000,		///< INIT TYPE : NONE
	HAL_AUDIO_SIF_ATSC_SELECT			= 0x0001,		///< INIT TYPE : TV Systems for A2 enabled in default ATSC system
	HAL_AUDIO_SIF_KOREA_A2_SELECT		= 0x0002,		///< INIT TYPE : TV Systems for A2 enabled in Korea A2 system
	HAL_AUDIO_SIF_BTSC_SELECT			= 0x0004,		///< INIT TYPE : TV Systems for BTSC enabled in ATSC(CO, CF) or DVB(Taiwan) system
	HAL_AUDIO_SIF_BTSC_BR_SELECT		= 0x0008,		///< INIT TYPE : TV Systems for BTSC enabled in ATSC(Brazil) system
	HAL_AUDIO_SIF_BTSC_US_SELECT		= 0x0010,		///< INIT TYPE : TV Systems for BTSC enabled in ATSC(US) system
	HAL_AUDIO_SIF_DVB_SELECT 			= 0x0020,		///< INIT TYPE : TV Systems for EU in default DVB system
	HAL_AUDIO_SIF_DVB_ID_SELECT			= 0x0040,		///< INIT TYPE : TV Systems for ID(Indonesia) in DVB(PAL B/G) system
	HAL_AUDIO_SIF_DVB_IN_SELECT			= 0x0080,		///< INIT TYPE : TV Systems for IN(India) in DVB(PAL B) system
	HAL_AUDIO_SIF_DVB_CN_SELECT			= 0x0100,		///< INIT TYPE : TV Systems for CN(China, Hong Kone) in DVB system
	HAL_AUDIO_SIF_DVB_AJJA_SELECT		= 0x0200,		///< INIT TYPE : TV Systems for AJ(Asia JooDong), JA(JooAang Asia) in DVB system
	HAL_AUDIO_SIF_TYPE_MAX				= 0x0FFF,		///< INIT TYPE : MAX
} HAL_AUDIO_SIF_TYPE_T;

/**
 * HAL AUDIO SIF Input Source Type.
 *
 */
typedef enum
{
	HAL_AUDIO_SIF_INPUT_EXTERNAL	= 0,
	HAL_AUDIO_SIF_INPUT_INTERNAL	= 1,
} HAL_AUDIO_SIF_INPUT_T;

/**
 * HAL AUDIO SIF Sound System Type.
 *
 */
typedef enum
{
	HAL_AUDIO_SIF_SYSTEM_BG			= 0x00,
	HAL_AUDIO_SIF_SYSTEM_I			= 0x01,
	HAL_AUDIO_SIF_SYSTEM_DK			= 0x02,
	HAL_AUDIO_SIF_SYSTEM_L			= 0x03,
	HAL_AUDIO_SIF_SYSTEM_MN			= 0x04,
	HAL_AUDIO_SIF_SYSTEM_UNKNOWN	= 0xF0,
} HAL_AUDIO_SIF_SOUNDSYSTEM_T;

/**
 * HAL AUDIO SIF Sound Standard Mode Type.
 *
 */
typedef enum
{
	HAL_AUDIO_SIF_MODE_DETECT	= 0,
	HAL_AUDIO_SIF_BG_NICAM		= 1,
	HAL_AUDIO_SIF_BG_FM			= 2,
	HAL_AUDIO_SIF_BG_A2			= 3,
	HAL_AUDIO_SIF_I_NICAM		= 4,
	HAL_AUDIO_SIF_I_FM			= 5,
	HAL_AUDIO_SIF_DK_NICAM		= 6,
	HAL_AUDIO_SIF_DK_FM			= 7,
	HAL_AUDIO_SIF_DK1_A2		= 8,
	HAL_AUDIO_SIF_DK2_A2		= 9,
	HAL_AUDIO_SIF_DK3_A2		= 10,
	HAL_AUDIO_SIF_L_NICAM		= 11,
	HAL_AUDIO_SIF_L_AM			= 12,
	HAL_AUDIO_SIF_MN_A2			= 13,
	HAL_AUDIO_SIF_MN_BTSC		= 14,
	HAL_AUDIO_SIF_MN_EIAJ		= 15,
	HAL_AUDIO_SIF_NUM_SOUND_STD	= 16,
} HAL_AUDIO_SIF_STANDARD_T;

/**
 * HAL AUDIO SIF Analog Audio Setting Parameter.
 *
 */
typedef enum
{
	HAL_AUDIO_SIF_SET_PAL_MONO					=	0x00,	// PAL Mono
	HAL_AUDIO_SIF_SET_PAL_MONO_FORCED			=	0x01,	// PAL Mono Force Mono
	HAL_AUDIO_SIF_SET_PAL_STEREO				=	0x02,	// PAL Stereo
	HAL_AUDIO_SIF_SET_PAL_STEREO_FORCED			=	0x03,	// PAL Stereo Force Mono
	HAL_AUDIO_SIF_SET_PAL_DUALI					=	0x04,	// PAL Dual I
	HAL_AUDIO_SIF_SET_PAL_DUALII				=	0x05,	// PAL Dual II
	HAL_AUDIO_SIF_SET_PAL_DUALI_II				=	0x06,	// PAL Dual I+II
	HAL_AUDIO_SIF_SET_PAL_NICAM_MONO			=	0x07,	// PAL NICAM Mono
	HAL_AUDIO_SIF_SET_PAL_NICAM_MONO_FORCED		=	0x08,	// PAL NICAM Mono Force Mono
	HAL_AUDIO_SIF_SET_PAL_NICAM_STEREO			=	0x09,	// PAL NICAM Stereo
	HAL_AUDIO_SIF_SET_PAL_NICAM_STEREO_FORCED	=	0x0A,	// PAL NICAM Stereo Force Mono
	HAL_AUDIO_SIF_SET_PAL_NICAM_DUALI			=	0x0B,	// PAL NICAM Dual I
	HAL_AUDIO_SIF_SET_PAL_NICAM_DUALII			=	0x0C,	// PAL NICAM Dual II
	HAL_AUDIO_SIF_SET_PAL_NICAM_DUALI_II		=	0x0D,	// PAL NICAM Dual I+II
	HAL_AUDIO_SIF_SET_PAL_NICAM_DUAL_FORCED		=	0x0E,	// PAL NICAM Dual Forced Mono(Not Supported)
	HAL_AUDIO_SIF_SET_PAL_UNKNOWN				=	0x0F,	// PAL Unkown State
	HAL_AUDIO_SIF_SET_NTSC_A2_MONO				=	0x10,	// NTSC(A2) Mono
	HAL_AUDIO_SIF_SET_NTSC_A2_STEREO			=	0x11,	// NTSC(A2) Stereo
	HAL_AUDIO_SIF_SET_NTSC_A2_SAP				=	0x12,	// NTSC(A2) SAP
	HAL_AUDIO_SIF_SET_NTSC_A2_UNKNOWN			=	0x13,	// NTSC(A2) Unkown State
	HAL_AUDIO_SIF_SET_NTSC_BTSC_MONO			=	0x14,	// NTSC(BTSC) Mono
	HAL_AUDIO_SIF_SET_NTSC_BTSC_STEREO			=	0x15,	// NTSC(BTSC) Stereo
	HAL_AUDIO_SIF_SET_NTSC_BTSC_SAP_MONO		=	0x16,	// NTSC(BTSC) SAP Mono
	HAL_AUDIO_SIF_SET_NTSC_BTSC_SAP_STEREO		=	0x17,	// NTSC(BTSC) SAP Stereo
	HAL_AUDIO_SIF_SET_NTSC_BTSC_UNKNOWN			=	0x18,	// NTSC(BTSC) Unkown State
} HAL_AUDIO_SIF_MODE_SET_T;

/**
 * HAL AUDIO SIF Standard Type.
 *
 */
typedef enum
{
	HAL_AUDIO_SIF_NICAM					= 0,
	HAL_AUDIO_SIF_A2					= 1,
	HAL_AUDIO_SIF_FM					= 2,
	HAL_AUDIO_SIF_DETECTING_AVALIBILITY = 3,
} HAL_AUDIO_SIF_AVAILE_STANDARD_T;

/**
 * HAL AUDIO SIF Exist Type.
 *
 */
typedef enum
{
	HAL_AUDIO_SIF_ABSENT	 			= 0,
	HAL_AUDIO_SIF_PRESENT 		 		= 1,
	HAL_AUDIO_SIF_DETECTING_EXSISTANCE	= 2,
} HAL_AUDIO_SIF_EXISTENCE_INFO_T;

/**
 * HAL AUDIO SIF Analog Audio Getting Parameter.
 *
 */
typedef enum
{
	HAL_AUDIO_SIF_GET_PAL_MONO				=	0x00,	// PAL Mono
	HAL_AUDIO_SIF_GET_PAL_STEREO			=	0x01,	// PAL Stereo
	HAL_AUDIO_SIF_GET_PAL_DUAL				=	0x02,	// PAL Dual
	HAL_AUDIO_SIF_GET_PAL_NICAM_MONO		=	0x03,	// PAL NICAM Mono
	HAL_AUDIO_SIF_GET_PAL_NICAM_STEREO		=	0x04,	// PAL NICAM Stereo
	HAL_AUDIO_SIF_GET_PAL_NICAM_DUAL		=	0x05,	// PAL NICAM Dual
	HAL_AUDIO_SIF_GET_PAL_UNKNOWN			=	0x06,	// PAL Unkown State
	HAL_AUDIO_SIF_GET_NTSC_A2_MONO			=	0x10,	// NTSC(A2) Mono
	HAL_AUDIO_SIF_GET_NTSC_A2_STEREO		=	0x11,	// NTSC(A2) Stereo
	HAL_AUDIO_SIF_GET_NTSC_A2_SAP			=	0x12,	// NTSC(A2) SAP
	HAL_AUDIO_SIF_GET_NTSC_A2_UNKNOWN		=	0x13,	// NTSC(A2) Unkown State
	HAL_AUDIO_SIF_GET_NTSC_BTSC_MONO		=	0x14,	// NTSC(BTSC) Mono
	HAL_AUDIO_SIF_GET_NTSC_BTSC_STEREO		=	0x15,	// NTSC(BTSC) Stereo
	HAL_AUDIO_SIF_GET_NTSC_BTSC_SAP_MONO	=	0x16,	// NTSC(BTSC) SAP Mono
	HAL_AUDIO_SIF_GET_NTSC_BTSC_SAP_STEREO	=	0x17,	// NTSC(BTSC) SAP Stereo
	HAL_AUDIO_SIF_GET_NTSC_BTSC_UNKNOWN		=	0x18,	// NTSC(BTSC) Unkown State
} HAL_AUDIO_SIF_MODE_GET_T;

/**
 * HAL AUDIO Copy Protection Type for SPDIF
 *
 */
typedef  enum
{
	HAL_AUDIO_SPDIF_COPY_FREE		= 0,	/* cp-bit : 1, L-bit : 0 */
	HAL_AUDIO_SPDIF_COPY_NO_MORE	= 1,	/* cp-bit : 0, L-bit : 1 */
	HAL_AUDIO_SPDIF_COPY_ONCE		= 2,	/* cp-bit : 0, L-bit : 0 */
	HAL_AUDIO_SPDIF_COPY_NEVER		= 3,	/* cp-bit : 0, L-bit : 1 */
} HAL_AUDIO_SPDIF_COPYRIGHT_T;


/**
	* HAL AUDIO Copy Protection Type for ARC
	*
	*/
typedef  enum{
	HAL_AUDIO_ARC_COPY_FREE			= 0,	/* cp-bit : 1, L-bit : 0 */
	HAL_AUDIO_ARC_COPY_NO_MORE		= 1,	/* cp-bit : 0, L-bit : 1 */
	HAL_AUDIO_ARC_COPY_ONCE			= 2,	/* cp-bit : 0, L-bit : 0 */
	HAL_AUDIO_ARC_COPY_NEVER		= 3,	/* cp-bit : 0, L-bit : 1 */
} HAL_AUDIO_ARC_COPYRIGHT_T;

/**
 * HAL AUDIO Volume Structure
 * dB Scale		:     main,	fine
 *   -127dB Min 	: 0x00(-127dB)	 0x00(0 dB)
 * - 125.9325 dB 	: 0x01(-126 dB), 0x01(1/16 dB)
 * - 121.8125 dB 	: 0x05(-122 dB), 0x03(3/16 dB)
 * -0.9375dB        : 0x7E(-1dB),    0x01(1/16dB)
 *    0 dB          : 0x7F(0 dB),    0x00(0 dB)
 *    30dB          : 0x9D(30dB),    0x00(0 dB)
 */
typedef struct HAL_AUDIO_VOLUME
{
	UINT8	mainVol;	// 1 dB step, -127 ~ +30 dB.
	UINT8	fineVol;  	// 1/16 dB step, 0dB ~ 15/16dB
} HAL_AUDIO_VOLUME_T ;

/**
 * HAL AUDIO DOLBY DRC Mode.
 *
 * @see
*/
typedef  enum
{
	HAL_AUDIO_DOLBY_LINE_MODE	= 0,
	HAL_AUDIO_DOLBY_RF_MODE		= 1,
	HAL_AUDIO_DOLBY_DRC_OFF		= 2,
} HAL_AUDIO_DOLBY_DRC_MODE_T;

/**
 * HAL AUDIO DOLBY Downmix Mode.
 *
*/
typedef  enum
{
	HAL_AUDIO_LORO_MODE		= 0,
	HAL_AUDIO_LTRT_MODE		= 1,
} HAL_AUDIO_DOWNMIX_MODE_T;

/**
 * HAL AUDIO Channel Mode.
 *
 * @see
*/
typedef enum HAL_AUDIO_MODE
{
	HAL_AUDIO_MODE_MONO	 			= 0,
	HAL_AUDIO_MODE_JOINT_STEREO 	= 1,
	HAL_AUDIO_MODE_STEREO		 	= 2,
	HAL_AUDIO_MODE_DUAL_MONO 		= 3,
	HAL_AUDIO_MODE_MULTI			= 4,
	HAL_AUDIO_MODE_UNKNOWN			= 5,
} HAL_AUDIO_MODE_T;

/**
 * HAL AUDIO TP Channel Mode.
 *
 * @see
*/
typedef enum HAL_AUDIO_CHANNEL_MODE
{
	HAL_AUDIO_CH_MODE_MONO	 					= 0,
	HAL_AUDIO_CH_MODE_JOINT_STEREO 				= 1,
	HAL_AUDIO_CH_MODE_STEREO					= 2,
	HAL_AUDIO_CH_MODE_DUAL_MONO					= 3,
	HAL_AUDIO_CH_MODE_MULTI						= 4,
	HAL_AUDIO_CH_MODE_UNKNOWN					= 5,
	HAL_AUDIO_CH_MODE_2_1_FL_FR_LFE 			= 6,
	HAL_AUDIO_CH_MODE_3_0_FL_FR_RC	 			= 7,
	HAL_AUDIO_CH_MODE_3_1_FL_FR_RC_LFE			= 8,
	HAL_AUDIO_CH_MODE_4_0_FL_FR_RL_RR			= 9,
	HAL_AUDIO_CH_MODE_4_1_FL_FR_RL_RR_LFE		= 10,
	HAL_AUDIO_CH_MODE_5_0_FL_FR_FC_RL_RR		= 11,
	HAL_AUDIO_CH_MODE_5_1_FL_FR_FC_RL_RR_LFE	= 12,
} HAL_AUDIO_CHANNEL_MODE_T;

/**
 * HAL Audio Endian of PCM..
 *
 */
typedef enum
{
	HAL_AUDIO_PCM_LITTLE_ENDIAN	= 0,
	HAL_AUDIO_PCM_BIG_ENDIAN	= 1,
} HAL_AUDIO_PCM_ENDIAN_T;

/**
 * HAL Audio Signed of PCM..
 *
 */
typedef enum
{
	HAL_AUDIO_PCM_SIGNED	= 0,
	HAL_AUDIO_PCM_UNSIGNED	= 1,
} HAL_AUDIO_PCM_SIGNED_T;

/**
 * HAL Audio Clip Play Status Type.
 *
 */
typedef enum
{
	HAL_AUDIO_CLIP_NONE		= 0,
	HAL_AUDIO_CLIP_PLAY		= 1,
	HAL_AUDIO_CLIP_STOP		= 2,
	HAL_AUDIO_CLIP_RESUME	= 3,
	HAL_AUDIO_CLIP_PAUSE	= 4,
	HAL_AUDIO_CLIP_DONE		= 5,
} HAL_AUDIO_CLIP_STATUS_T;

/**
 * HAL AUDIO Trick Mode Type.
 *
 */
typedef  enum
{
	HAL_AUDIO_TRICK_NONE				= 0, 	///<  rate : None, TP Live Play
	HAL_AUDIO_TRICK_PAUSE				= 1, 	///<  rate : Pause, DVR Play
	HAL_AUDIO_TRICK_NORMAL_PLAY			= 2, 	///<  rate : Normal Play, DVR Play
	HAL_AUDIO_TRICK_SLOW_MOTION_0P25X 	= 3,	///<  rate : 0.25 Play
	HAL_AUDIO_TRICK_SLOW_MOTION_0P50X 	= 4,	///<  rate : 0.50 Play
	HAL_AUDIO_TRICK_SLOW_MOTION_0P80X 	= 5,	///<  rate : 0.80 Play
	HAL_AUDIO_TRICK_FAST_FORWARD_1P20X 	= 6, 	///<  rate : 1.20 Play
	HAL_AUDIO_TRICK_FAST_FORWARD_1P50X 	= 7, 	///<  rate : 1.50 Play
	HAL_AUDIO_TRICK_FAST_FORWARD_2P00X	= 8,	///<  rate : 2.00 Play
	HAL_AUDIO_TRICK_ONE_FRAME_DECODE	= 9,    ///<  rate : one frame decode
} HAL_AUDIO_TRICK_MODE_T;

/**
 *  HAL AUDIO LGSE PARAMETER Setting TYPE
 *
 */
typedef enum
{
	HAL_AUDIO_LGSE_INIT_ONLY = 0, // "init only" parameter will be written
	HAL_AUDIO_LGSE_VARIABLES = 1, // "variables" will be writen
	HAL_AUDIO_LGSE_ALL		 = 2, // "init only" and "variables" will be written simultaneously
} HAL_AUDIO_LGSE_DATA_MODE_T;

/**
 *  HAL AUDIO LGSE PARAMETER ACCESS TYPE
 *
 */
typedef enum
{
	HAL_AUDIO_LGSE_WRITE	= 0, // "pParams" data will be written to DSP
	HAL_AUDIO_LGSE_READ		= 1, // "pParams" data will be read from DSP to CPU
} HAL_AUDIO_LGSE_DATA_ACCESS_T;

/**
 *  HAL AUDIO LGSE FN004 MODE VARIABLE TYPE
 *
 */
typedef enum
{
	HAL_AUDIO_LGSE_MODE_VARIABLES0	= 0, // "VARIABLES_00"will be written
	HAL_AUDIO_LGSE_MODE_VARIABLES1	= 1, // "VARIABLES_01"will be written
	HAL_AUDIO_LGSE_MODE_VARIABLES2	= 2, // "VARIABLES_02"will be written
	HAL_AUDIO_LGSE_MODE_VARIABLES3	= 3, // "VARIABLES_03"will be written
	HAL_AUDIO_LGSE_MODE_VARIABLES4	= 4, // "VARIABLES_04"will be written
	HAL_AUDIO_LGSE_MODE_VARIABLESALL= 5  // All "VARIABLES" will be written simultaneously. Data will be arranged from 0 to 4.
} HAL_AUDIO_LGSE_VARIABLE_MODE_T;

/**
 *  HAL AUDIO LGSE FUNCTION MODE TYPE
 *
 */
typedef enum
{
    HAL_AUDIO_LGSE_LGSEFN000        = 0,
    HAL_AUDIO_LGSE_LGSEFN001        = 1,
    HAL_AUDIO_LGSE_LGSEFN002        = 2,    // new webOS4.5 (multi channel process)
    HAL_AUDIO_LGSE_LGSEFN003        = 3,    // deprecated
    HAL_AUDIO_LGSE_LGSEFN004        = 4,    // new webOS2.0 - deprecated
    HAL_AUDIO_LGSE_LGSEFN005        = 5,    // new webOS1.0 - deprecated
    HAL_AUDIO_LGSE_LGSEFN006        = 6,    // deprecated
    HAL_AUDIO_LGSE_LGSEFN007        = 7,    // old
    HAL_AUDIO_LGSE_LGSEFN008        = 8,
    HAL_AUDIO_LGSE_LGSEFN009        = 9,
    HAL_AUDIO_LGSE_LGSEFN010        = 10,
    HAL_AUDIO_LGSE_LGSEFN011        = 11,   // deprecated
    HAL_AUDIO_LGSE_LGSEFN012        = 12,   // deprecated
    HAL_AUDIO_LGSE_LGSEFN013        = 13,   // deprecated
    HAL_AUDIO_LGSE_LGSEFN014        = 14,   // new webOS1.0
    HAL_AUDIO_LGSE_LGSEFN016        = 16,   // new webOS1.0 - deprecated
    HAL_AUDIO_LGSE_LGSEFN017        = 17,   // new webOS2.0
    HAL_AUDIO_LGSE_LGSEFN018        = 18,   // new webOS3.0 - deprecated
    HAL_AUDIO_LGSE_LGSEFN019        = 19,   // new webOS2.0
    HAL_AUDIO_LGSE_LGSEFN020        = 20,   // new webOS2.0
    HAL_AUDIO_LGSE_LGSEFN022        = 22,   // new webOS2.0
    HAL_AUDIO_LGSE_LGSEFN023        = 23,   // new webOS2.0
    HAL_AUDIO_LGSE_LGSEFN024        = 24,   // new webOS2.0
    HAL_AUDIO_LGSE_LGSEFN025        = 25,   // new webOS3.0 - deprecated
    HAL_AUDIO_LGSE_LGSEFN026        = 26,   // new webOS3.0
    HAL_AUDIO_LGSE_LGSEFN027        = 27,   // new webOS3.0 - deprecated
    HAL_AUDIO_LGSE_LGSEFN028        = 28,   // new webOS3.0
    HAL_AUDIO_LGSE_LGSEFN029        = 29,   // new webOS3.0
    HAL_AUDIO_LGSE_LGSEFN030        = 30,   // new webOS4.5 (AI sound, feature extraction)
    HAL_AUDIO_LGSE_LGSEFN004MODE1   = 41,
    HAL_AUDIO_LGSE_LGSEFN004MODE2   = 42,
    HAL_AUDIO_LGSE_LGSEFN004MODE3   = 43,
    HAL_AUDIO_LGSE_LGSEFN000_1      = 1001, // new webOS4.5 (clear voice for multi channel)
    HAL_AUDIO_LGSE_LGSEFN002_1      = 201,  // new webOS4.5 (multi channel process for 2ch)
    HAL_AUDIO_LGSE_LGSEFN004_1      = 401,  // new webOS4.5 (ssm for multi channel)
    HAL_AUDIO_LGSE_LGSEFN008_1      = 801,  // new webOS4.5 (height channel for multi channel)
    HAL_AUDIO_LGSE_LGSEFN017_2      = 1702, // new webOS4.5 (SXP for multi channel for 2ch)
    HAL_AUDIO_LGSE_LGSEFN017_3      = 1703, // new webOS4.5 (SXP for multi channel)
    HAL_AUDIO_LGSE_LGSEFN022_1      = 2201, // new webOS4.5 (SSC for multi channel for 2ch)
    HAL_AUDIO_LGSE_LGSEFN022_2      = 2202, // new webOS4.5 (SSC for multi channel)
    HAL_AUDIO_LGSE_LGSEFN030_1      = 3001, // new webOS4.5 (AI sound, synthesizer)
    HAL_AUDIO_LGSE_MODE             = 100,
    HAL_AUDIO_LGSE_MAIN             = 101,  // new webOS 1.0
} HAL_AUDIO_LGSE_FUNCLIST_T;

typedef enum
{
    HAL_AUDIO_LGSE_MODE_NONE          = 0,
    HAL_AUDIO_LGSE_MODE_LGSE          = 1,
    HAL_AUDIO_LGSE_MODE_DAP           = 2,
    HAL_AUDIO_LGSE_MODE_DTSVX         = 3,
    HAL_AUDIO_LGSE_MODE_LGSE_AISOUND  = 4,
} HAL_AUDIO_LGSE_MODE_T;

/**
 * HAL AUDIO Encoder Index.
 *
 */
typedef  enum
{
	HAL_AUDIO_AENC0		= 0,
	HAL_AUDIO_AENC1		= 1,
	HAL_AUDIO_AENC_MAX	= HAL_AUDIO_AENC1,
} HAL_AUDIO_AENC_INDEX_T ;

/**
 * HAL AUDIO AENC encode format
 */
typedef enum
{
	HAL_AUDIO_AENC_ENCODE_MP3 	= 0,	/* Encode MP3 format */
	HAL_AUDIO_AENC_ENCODE_AAC	= 1,	/* Encode AAC format */
	HAL_AUDIO_AENC_ENCODE_PCM	= 2,	/* Encode PCM format */
} HAL_AUDIO_AENC_ENCODING_FORMAT_T;

/**
 * HAL AUDIO AENC encode Status
 */
typedef enum
{
	HAL_AUDIO_AENC_STATUS_STOP	 	= 0,
	HAL_AUDIO_AENC_STATUS_PLAY	 	= 1,
	HAL_AUDIO_AENC_STATUS_ABNORMAL 	= 2,
} HAL_AUDIO_AENC_STATUS_T;

/**
 * HAL AUDIO AENC encode # of channel
 */
typedef enum
{
	HAL_AUDIO_AENC_MONO 	= 0,
	HAL_AUDIO_AENC_STEREO 	= 1,
} HAL_AUDIO_AENC_CHANNEL_T;

/**
 * HAL AUDIO AENC encode bitrate
 */
typedef enum
{
	HAL_AUDIO_AENC_BIT_48K		= 0,
	HAL_AUDIO_AENC_BIT_56K		= 1,
	HAL_AUDIO_AENC_BIT_64K		= 2,
	HAL_AUDIO_AENC_BIT_80K		= 3,
	HAL_AUDIO_AENC_BIT_112K		= 4,
	HAL_AUDIO_AENC_BIT_128K		= 5,
	HAL_AUDIO_AENC_BIT_160K		= 6,
	HAL_AUDIO_AENC_BIT_192K		= 7,
	HAL_AUDIO_AENC_BIT_224K		= 8,
	HAL_AUDIO_AENC_BIT_256K		= 9,
	HAL_AUDIO_AENC_BIT_320K		= 10,
} HAL_AUDIO_AENC_BITRATE_T;

/**
 * HAL AUDIO PCM Input Mode.
 *
 */
typedef  enum
{
	HAL_AUDIO_PCM_SB_PCM		= 0,	/* Sound Bar Output : No SE Output */
	HAL_AUDIO_PCM_SB_CANVAS		= 1,	/* Sound Bar Output : SE Output */
	HAL_AUDIO_PCM_I2S			= 2,	/* I2S(L/R) Input for AEC(Audio Echo Cancellation) */
	HAL_AUDIO_PCM_WISA			= 3,    /* WISA Speaker Output : No SE Output */
	HAL_AUDIO_PCM_INPUT_MAX		= 4,	/* PCM Input Max */
} HAL_AUDIO_PCM_INPUT_T;

/**
 * HAL AUDIO Input/Output I2S Port.
 *
 */
typedef  enum
{
	HAL_AUDIO_INOUT_I2S0		= 0x01,	/* I2S0(L/R) Input/Output for AEC(Audio Echo Cancellation) */
	HAL_AUDIO_INOUT_I2S1		= 0x02, /* I2S1(Lh/Rh) Input/Output for AEC(Audio Echo Cancellation) */
	HAL_AUDIO_INOUT_I2S2		= 0x04, /* I2S2(Lr/Rr) Input/Output for AEC(Audio Echo Cancellation) */
	HAL_AUDIO_INOUT_I2S3		= 0x08, /* I2S3(C/Lf) Input/Output for AEC(Audio Echo Cancellation) */
} HAL_AUDIO_INOUT_I2S_T;

/**
 * HAL AUDIO PostProcess Mode Definition
 *
*/
typedef enum HAL_AUDIO_POSTPROC_MODE
{
	HAL_AUDIO_POSTPROC_LGSE_MODE 		= 0, 	/* Use LGSE for Audio post processing */
	HAL_AUDIO_POSTPROC_DAP_LGSE_MODE	= 1, 	/* Use both LGSE and DAP for Audio post processing */
	HAL_AUDIO_POSTPROC_DAP_MODE			= 2, 	/* Use DAP for audio post processing and volume and upsampling function with LGSE processing. */
}HAL_AUDIO_POSTPROC_MODE_T;

/**
 * HAL AUDIO DAP Surround Virtualizer Mode Definition
 *
*/
typedef enum HAL_AUDIO_LGSE_DAP_SURROUND_VIRTUALIZER_MODE
{
	HAL_AUDIO_LGSE_DAP_SURROUND_VIRTUALIZER_OFF			= 0,     /* Virtualizer off(defalut) : process mode-DAP_CPDP_PROCESS_2 and mix_matrix-NULL*/
	HAL_AUDIO_LGSE_DAP_SURROUND_VIRTUALIZER_ON			= 1,     /* Virtualizer on: When input content is stereo,   process mode-DAP_CPDP_PROCESS_5_1_SPEAKER and mix_matrix-speaker_5_1_2_to_2_0_mix_matrix
																	When input content is 5.1 or 5.1.2(ATMOS), 		process mode-DAP_CPDP_PROCESS_5_1_2_SPEAKER and mix_matrix-speaker_5_1_2_to_2_0_mix_matrix */
	HAL_AUDIO_LGSE_DAP_SURROUND_VIRTUALIZER_AUTO		= 2,     /* Virtualizer Auto: When input content is 5.1.2(ATMOS),  process mode-DAP_CPDP_PROCESS_5_1_2_SPEAKER and mix_matrix-speaker_5_1_2_to_2_0_mix_matrix
																	When input content is Non ATMOS, process mode-DAP_CPDP_PROCESS_2 and mix_matrix-NULL */
	HAL_AUDIO_LGSE_DAP_SURROUND_VIRTUALIZER_ON_2_0_2	= 3,     /* Virtualizer on: When input content is stereo,	process mode-DAP_CPDP_PROCESS_5_1_SPEAKER and mix_matrix-speaker_5_1_2_to_2_0_2_mix_matrix
																	When input content is 5.1 or 5.1.2(ATMOS), 		process mode-DAP_CPDP_PROCESS_5_1_2_SPEAKER and mix_matrix-speaker_5_1_2_to_2_0_2_mix_matrix */
}HAL_AUDIO_LGSE_DAP_SURROUND_VIRTUALIZER_MODE_T;

/**
 * HAL AUDIO DAP Audio Regulator System Parameter Definition
 *
*/
typedef enum HAL_AUDIO_LGSE_DAP_REGULATOR_MODE
{
	HAL_AUDIO_LGSE_DAP_VIRTUALBASS_MODE_PEAK_PROTECTION	 			= 0, 	/* 0 : Peak Protection mode: The Audio Regulator has the same operating characteristics for each bank */
	HAL_AUDIO_LGSE_DAP_SYSPARAM_VIRTUALBASS_MODE_SPEAKER_DISTOTION	= 1, 	/* !0: Speaker Distortion mode: The tuning configuration supplied bydap_cpdp_regulator_tuning_configure and dap_cpdp_regulator_overdrive_set is used to give the Audio Regulator per-band operating characteristics */
} HAL_AUDIO_LGSE_DAP_REGULATOR_MODE_T;

/**
 * HAL AUDIO DAP Height Filter Mode Definition
 *
*/
typedef enum HAL_AUDIO_LGSE_DAP_PERCEPTUAL_HEIGHT_FILTER_MODE
{
	HAL_AUDIO_LGSE_DAP_HEIGHT_FILTER_DISABLED 		= 0, 	/* Height filter is disabled */
	HAL_AUDIO_LGSE_DAP_HEIGHT_FILTER_FRONT_FIRING	= 1, 	/* Height filter configured for front firing speakers */
	HAL_AUDIO_LGSE_DAP_HEIGHT_FILTER_UP_FIRING 		= 2, 	/* Height filter configured for up firing speakers */
}HAL_AUDIO_LGSE_DAP_PERCEPTUAL_HEIGHT_FILTER_MODE_T;

/**
 * HAL AUDIO DAP Virtual Bass Mode.
 *
*/
typedef enum HAL_AUDIO_LGSE_DAP_VIRTUAL_BASS_MODE
{
	HAL_AUDIO_LGSE_DAP_VIRTUAL_BASS_MODE_DELAYONLY	 				= 0, 	/* 0: Delay only */
	HAL_AUDIO_LGSE_DAP_SYS_PARAM_VIRTUAL_BASS_MODE_2ND_ORDER 		= 1, 	/* Second-order harmonics */
	HAL_AUDIO_LGSE_DAP_SYS_PARAM_VIRTUAL_BASS_MODE_3RD_ORDER		= 2, 	/* Second- and third-order harmonics */
	HAL_AUDIO_LGSE_DAP_SYS_PARAM_VIRTUAL_BASS_MODE_4TH_ORDER		= 3, 	/* Second-, third-, and fourth-order  harmonics */
} HAL_AUDIO_LGSE_DAP_VIRTUAL_BASS_MODE_T;

/**
* HAL AUDIO Language Code Type.
*
*/
typedef  enum
{
	HAL_AUDIO_LANG_CODE_ISO639_1	= 0, 	/* 2bytes example : 'e''n'00   */
	HAL_AUDIO_LANG_CODE_ISO639_2	= 1, 	/* 3bytes example : 'e''n''g'0 */
} HAL_AUDIO_LANG_CODE_TYPE_T;

/**
* HAL AUDIO AC4 AD Type.
*
*/
typedef enum HAL_AUDIO_AC4_AD_TYPE
{
	HAL_AUDIO_AC4_AD_TYPE_NONE	= 0, 	/* None */
	HAL_AUDIO_AC4_AD_TYPE_VI	= 1, 	/* Visually Impaired (VI) - Default */
	HAL_AUDIO_AC4_AD_TYPE_HI	= 2, 	/* Hearing Impaired (HI) */
	HAL_AUDIO_AC4_AD_TYPE_C		= 3, 	/* Commentary (C) */
	HAL_AUDIO_AC4_AD_TYPE_E		= 4, 	/* Emergency (E) */
	HAL_AUDIO_AC4_AD_TYPE_VO	= 5, 	/* Voice Over (VO) */
} HAL_AUDIO_AC4_AD_TYPE_T;

/**
* HAL AUDIO Input Delay Param
*
* @see
*/
typedef struct HAL_AUDIO_INPUT_DELAY_PARAM{
	HAL_AUDIO_ADEC_INDEX_T adecIndex;
	SINT32 delayTime;
}HAL_AUDIO_INPUT_DELAY_PARAM_T;

/**
* HAL AUDIO Output Delay Param
*
* @see
*/
typedef struct HAL_AUDIO_OUTPUT_DELAY_PARAM{
	HAL_AUDIO_SNDOUT_T soundOutType;
	SINT32 delayTime;
}HAL_AUDIO_OUTPUT_DELAY_PARAM_T;

/**
 * HAL AUDIO AC3 ES Info
 *
 * @see
*/
typedef struct HAL_AUDIO_AC3_ES_INFO{
    UINT8 bitRate;
    UINT8 sampleRate;
    UINT8 channelNum;
    UINT8 EAC3;       	/* AC3 0x0, EAC3 0x1*/
} HAL_AUDIO_AC3_ES_INFO_T;

/**
 * HAL AUDIO MPEG ES Info
 *
 * @see
*/
typedef struct HAL_AUDIO_MPEG_ES_INFO{
    UINT8 bitRate;
    UINT8 sampleRate;
    UINT8 layer;
    UINT8 channelNum;
} HAL_AUDIO_MPEG_ES_INFO_T;

/**
 * HAL AUDIO HE-AAC ES Info
 *
 * @see
*/
typedef struct HAL_AUDIO_HEAAC_ES_INFO{
    UINT8 version;    		  	/* AAC = 0x0, HE-AACv1 = 0x1, HE-AACv2 = 0x2 */
    UINT8 transmissionformat;   /* LOAS/LATM = 0x0, ADTS = 0x1*/
    UINT8 channelNum;
} HAL_AUDIO_HEAAC_ES_INFO_T;

/**
 * HAL AUDIO Clip Decoder Play Info
 *
*/
typedef struct HAL_AUDIO_CLIP_DEC_PARAM{
	HAL_AUDIO_SRC_TYPE_T 		clipType;
	UINT32 						repeatNumber;		/* The play number of audio clip. */
} HAL_AUDIO_CLIP_DEC_PARAM_T;

/**
 * HAL AUDIO Clip Mixer Play Info
 *
*/
typedef struct HAL_AUDIO_CLIP_MIX_PARAM{
	UINT32 						numOfChannel;		/* 2  : stereo, 1 : mono,  8 : 8 channel */
	UINT32 						bitPerSample;		/* 16 : 16 bit, 8 : 8 bit 24 : 24bit */
	HAL_AUDIO_SAMPLING_FREQ_T	samplingFreq;		/* 48000 : 48Khz, 44100 : 44.1Khz */
	HAL_AUDIO_PCM_ENDIAN_T		endianType;			/* 0  : little endian, 1 : big endian */
	HAL_AUDIO_PCM_SIGNED_T		signedType;			/* 0  : signed PCM, 1 : unsigned PCM */
	UINT32 						repeatNumber;		/* The play number of audio clip. */
} HAL_AUDIO_CLIP_MIX_PARAM_T;

/**
 * HAL AUDIO Sound Bar Set Info
 *
*/
typedef struct HAL_AUDIO_SB_SET_INFO{
    UINT32 			barId;
    UINT32 			volume;
	BOOLEAN 		bMuteOnOff;
	BOOLEAN 		bPowerOnOff;
} HAL_AUDIO_SB_SET_INFO_T;

/**
 * HAL AUDIO Sound Bar(Canvas) Command Set Info
 *
*/
typedef struct HAL_AUDIO_SB_SET_CMD{
	BOOLEAN 		bAutoVolume;
	UINT8	 		wooferLevel;
    UINT32 			reservedField;
} HAL_AUDIO_SB_SET_CMD_T;

/**
 * HAL AUDIO Sound Bar Get Info
 *
*/
typedef struct HAL_AUDIO_SB_GET_INFO{
    UINT32 			subFrameID;
    UINT8 			subFrameData;
	UINT8 			subFramePowerData;
	UINT8 			subframeChecksum;
} HAL_AUDIO_SB_GET_INFO_T;

/**
 * HAL AUDIO Sound Bar(Canvas) Command Get Info
 *
*/
typedef struct HAL_AUDIO_SB_GET_CMD{
    UINT32 			subFrameID;
    UINT32 			subFrameData;
	UINT8 			subframeChecksum;
} HAL_AUDIO_SB_GET_CMD_T;

/**
 * HAL AUDIO AENC Info
 *
*/
typedef struct HAL_AUDIO_AENC_INFO
{
	//Get Info for debugging on DDI
	HAL_AUDIO_AENC_STATUS_T				status;	// current ENC Status
	HAL_AUDIO_AENC_ENCODING_FORMAT_T	codec;	// current ENC Codec

	UINT32	errorCount;							// current ENC error counter
	UINT32	inputCount;							// current ENC input counter - we distinguish whether or not enter data from input.
	UINT32	underflowCount;						// current ENC underflowCnt in kernel space - we distinguish which module have problem between muxer and encdoer
	UINT32	overflowCount;						// current ENC overflowCnt - we distinguish

	//Set Info - it is applied realtime, no matter stop&start
	HAL_AUDIO_AENC_CHANNEL_T	channel;		// number of channel
	HAL_AUDIO_AENC_BITRATE_T	bitrate;		// bitrate
	HAL_AUDIO_ADEC_INDEX_T		dec_port;
	UINT64						pts;
	UINT32						gain;
} HAL_AUDIO_AENC_INFO_T;

/**
 *  HAL AUDIO PCM DATA TYPE
 *
 */
typedef struct HAL_AUDIO_PCM_DATA
{
    UINT8	index;		// PCM index
	UINT32	pts;		// PTS(unit : 90Khz clock base, max value : 0xFFFFFFFF)
	UINT8	*pData;		// pointer to Audio Data
	UINT32	dataLen;	// Audio Data Length
} HAL_AUDIO_PCM_DATA_T;

/**
 * HAL AUDIO PCM Capture Info
 * The PCM other parameter is pre-defined as follows.(little endian, singed)
 *
*/
typedef struct HAL_AUDIO_PCM_INFO{
	HAL_AUDIO_SAMPLING_FREQ_T	samplingFreq;		/* 48000 : 48Khz, 44100 : 44.1Khz */
	UINT32 						numOfChannel;		/* 2  : stereo, 1 : mono */
	UINT32 						bitPerSample;		/* 16 : 16 bit, 8 : 8 bit */
	HAL_AUDIO_INOUT_I2S_T		inOutI2SPort;		/* I2S Input/Output Port Bit Mask.(I2S0 & I2S1 => 0x03) */
} HAL_AUDIO_PCM_INFO_T;

/**
 *  HAL AUDIO CALLBACK TYPE
 *
 */
typedef enum
{
	HAL_AUDIO_WARNING_DEFAULT 	  		= 0,
	HAL_AUDIO_WARNING_PIP_OVERRUN 		= 1, //AAC5.1 ch X 2 decoding인 경우, performance이슈로 callback notify on H13
	HAL_AUDIO_WARNING_1KHZ_TONE_ON 		= 2, //ERP Power Requirement : 1KHz Tone Detect On(Woofer Amp PWM OFF)
	HAL_AUDIO_WARNING_1KHZ_TONE_OFF		= 3, //ERP Power Requirement : 1KHz Tone Detect Off(Woofer Amp PWM ON)
} HAL_AUDIO_WARNING_IDX;

/**
 *  HAL AUDIO AENC DATA MESSAGE TYPE
 *
 */
typedef struct HAL_AUDIO_AENC_DATA
{
	UINT32	index;		// Encoder index
	UINT64	pts;		// PTS
	UINT8	*pData; 	// pointer to Audio Data
	UINT32	dataLen;	// Audio Data Length
	UINT8	*pRStart;	// start pointer of buffer
	UINT8	*pREnd; 	// end pointer of buffer
}HAL_AUDIO_AENC_DATA_T;

/**
 *  HAL AUDIO CALLBACK FUNCTION
 *
 */
typedef DTV_STATUS_T (*pfnAdecoderClipDone)(HAL_AUDIO_ADEC_INDEX_T adecIndex);
typedef DTV_STATUS_T (*pfnAmixerClipDone)(HAL_AUDIO_MIXER_INDEX_T amixIndex);
typedef DTV_STATUS_T (*pfnAENCDataHandling)(HAL_AUDIO_AENC_DATA_T *pMsg);
typedef DTV_STATUS_T (*pfnPCMDataHandling)(HAL_AUDIO_PCM_DATA_T *pMsg);
typedef DTV_STATUS_T (*pfnPCMSending)(UINT8 *pBuf, UINT16 length);
typedef DTV_STATUS_T (*pfnADECWarning)(HAL_AUDIO_WARNING_IDX index);
typedef DTV_STATUS_T (*pfnLGSESmartSound)(UINT32 *pData, UINT16 length);

/**
 * HAL AUDIO Decoder Debug Information Definition
 *
 */
typedef struct HAL_AUDIO_ADEC_INFO
{
	int32_t                         adec_port_number;        // value.integer.value[0]
	int32_t                         bAdecStart;              // value.integer.value[1]
	HAL_AUDIO_SRC_TYPE_T            userAdecFormat;          // value.integer.value[2]
	HAL_AUDIO_SRC_TYPE_T            curAdecFormat;           // value.integer.value[3]
	HAL_AUDIO_SRC_TYPE_T            sourceAdecFormat;        // value.integer.value[4]
	adec_src_port_index_ext_type_t  curAdecInputPort;        // value.integer.value[5]
	adec_src_port_index_ext_type_t  prevAdecInputPort;       // value.integer.value[6]
	adec_dualmono_mode_ext_type_t   curAdecDualMonoMode;     // value.integer.value[7]
	int                             IsEsExist;               // value.integer.value[8]
	adec_tp_mode_ext_type_t         audioMode;               // value.integer.value[9]
	int32_t                         heaac_version;           // value.integer.value[10]
	int32_t                         heaac_trasmissionformat; // value.integer.value[11]
	int32_t                         heaac_channelNum;        // value.integer.value[12]
	int32_t                         mpeg_bitrate;            // value.integer.value[13]
	int32_t                         mpeg_sampleRate;         // value.integer.value[14]
	int32_t                         mpeg_layer;              // value.integer.value[15]
	int32_t                         mpeg_channelNum;         // value.integer.value[16]
	int32_t                         ac3_bitrate;             // value.integer.value[17]
	int32_t                         ac3_sampleRate;          // value.integer.value[18]
	int32_t                         ac3_channelnum;          // value.integer.value[19]
	int32_t                         ac3_EAC3;                // value.integer.value[20]
} HAL_AUDIO_ADEC_INFO_T;

/**
 * HAL AUDIO Decoder Status Information Definition
 *
 */
typedef struct HAL_AUDIO_ADEC_STATUS
{
	int32_t                         Open;
	adec_src_port_index_ext_type_t  Connect;
	int32_t                         Start;
	adec_src_codec_ext_type_t       UserCodec;
	adec_src_codec_ext_type_t       CurCodec;
	adec_src_codec_ext_type_t       SrcCodec;
	int32_t                         SrcBitrates;
	int32_t                         SrcSamplingRate;
	int32_t                         SrcChannel;
	adec_src_codec_ext_type_t       OutputCodec;
	int32_t                         OutputChannel;
	uint64_t                        PTS;
	int32_t                         Language;
	adec_dolbydrc_mode_ext_type_t   DolbyDRCMode;
	adec_downmix_mode_ext_type_t    Downmix;
	int32_t                         SyncMode;
	adec_trick_mode_ext_type_t      TrickMode;
	int32_t                         Gain;
	int32_t                         Mute;
	int32_t                         Delay;
	int32_t                         AudioDescription;
	int32_t                         UsingByGst;
} HAL_AUDIO_ADEC_STATUS_T;

/**
 * HAL AUDIO HAL Driver Information Definition
 *
 */
typedef struct HAL_AUDIO_COMMON_INFO
{
	HAL_AUDIO_SPDIF_MODE_T 		curAudioSpdifMode;			/* Current Audio SPDIF Output Mode. */
	BOOLEAN						bAudioSpdifOutPCM;			/* Current Audio SPDIF Output is PCM. */
	BOOLEAN						bCurAudioSpdifOutPCM;		/* Current UI Setting SPDIF Output is PCM. */

	sndout_optic_copyprotection_ext_type_t	curSpdifCopyInfo;			/* SPDIF Output SCMS(Serail Copy Management System) => Copy Info. : FREE/ONCE/NEVER */
	UINT8						curSpdifCategoryCode;		/* Current Audio SPDIF Category Code */
	BOOLEAN						curSpdifLightOnOff;			/* Current Audio SPDIF Light Output ON/OFF */

	UINT32						curSPKOutDelay;
	UINT32						curSPDIFOutDelay;
	UINT32						curSPDIFSBOutDelay;
	UINT32						curHPOutDelay;
	UINT32						curBTOutDelay;
	UINT32						curWISAOutDelay;
	UINT32						curSEBTOutDelay;

	HAL_AUDIO_VOLUME_T			curSPKOutVolume;
	HAL_AUDIO_VOLUME_T			curSPDIFOutVolume;
	HAL_AUDIO_VOLUME_T			curHPOutVolume;

	UINT32						curSPKOutGain;
	UINT32						curSPDIFOutGain;
	UINT32						curHPOutGain;
	UINT32						curBTOutGain;
	UINT32						curWISAOutGain;
	UINT32						curSEBTOutGain;

	BOOLEAN						curSPKMuteStatus;
	BOOLEAN						curSPDIFMuteStatus;
	BOOLEAN						curSPDIF_LG_MuteStatus;
	BOOLEAN						curHPMuteStatus;
	BOOLEAN						curBTMuteStatus;
	BOOLEAN						curWISAMuteStatus;
	BOOLEAN						curSEBTMuteStatus;

	BOOLEAN						curBTConnectStatus[HAL_AUDIO_INDEX_MAX+1];
	BOOLEAN						curWISAConnectStatus[HAL_AUDIO_INDEX_MAX+1];
	BOOLEAN						curSEBTConnectStatus[HAL_AUDIO_INDEX_MAX+1];

	HAL_AUDIO_SB_SET_INFO_T 	curSoundBarInfo;
	HAL_AUDIO_SB_SET_CMD_T		curSoundBarCommand;
	UINT32						i2sNumber;
} HAL_AUDIO_COMMON_INFO_T;

/**
 * HAL AUDIO DAP suppported auido type Definition
 *
*/
typedef struct HAL_AUDIO_LGSE_DAP_SUPPORTED_AUDIO_TYPE
{
	BOOLEAN		bEnableAll;			/* All audiotype should be processed by DAP */
	BOOLEAN		bEnableATMOS; 		/* ATMOS should be processed by DAP, regardless of AC4, eAC3, AC3 and AAC */
	BOOLEAN		bEnableAC4; 		/* AC4 should be processed by DAP */
	BOOLEAN		bEnableEAC3; 		/* EAC3 should be processed by DAP */
	BOOLEAN		bEnableAC3; 		/* AC3 should be processed by DAP */
	BOOLEAN		bEnableAAC;			/* AAC should be processed by DAP */
	BOOLEAN		bEnableMPEGH;		/* MPEGH should be processed by DAP */
	BOOLEAN		bEnableMPEG; 		/* MPEG should be processed by DAP */
	BOOLEAN		bEnableDTS;			/* DTS should be processed by DAP */
} HAL_AUDIO_LGSE_DAP_SUPPORTED_AUDIO_TYPE_T;

/*******************************************Dolby ATMOS DAP User Parameter**********************************************/
/**
 * HAL AUDIO DAP Dialogue Enhancer Definition
 *
 */
typedef struct HAL_AUDIO_LGSE_DAP_DIALOGUE_ENHANCER
{
	BOOLEAN 	bIsDialogueEnhancerEnable;	/* Enables or disables the Dialogue Enhancer feature 0(disabled), !0(enabled) */
	UINT32 		dialogueEnhancerAmount;		/* Determines the strength of the Dialogue Enhancer effect for inputs other than Dolby AC-4	0 to 16 */
	UINT32		ducking;					/* When dialog is detected, Dialog Enhancer also supports attenuating channels which are not the source of the dialog 0 to 16*/
} HAL_AUDIO_LGSE_DAP_DIALOGUE_ENHANCER_T;

/**
 * HAL AUDIO DAP Volume Leveler Definition
 *
 */
typedef struct HAL_AUDIO_LGSE_DAP_VOLUME_LEVELER
{
	BOOLEAN 	bIsVolumeLevelerEnable;		/* Enables or disables the Volume Leveler 0(off), 1(on) */
	UINT32 		volumeLevelerAmount;		/* Specifies how aggressive the leveler is in attempting to reach the output target level	0 to 10 */
	SINT32		inputTargetLevel;			/* Specifies the average loudness level of the incoming audio specified according to a K loudness weighting -640 to 0*/
	SINT32		outputTargetLevel;			/* Specifies the average loudness level which the audio should be moved to. -640 to 0*/
} HAL_AUDIO_LGSE_DAP_VOLUME_LEVELER_T;

/*******************************************Dolby ATMOS DAP System Parameter**********************************************/
/**
 * HAL AUDIO DAP Bands Definition
 *
*/
typedef struct HAL_AUDIO_LGSE_DAP_BANDS
{
	UINT32 		bandCount;	/* Band count: 1-20 */
	UINT32 		freqs[HAL_AUDIO_LGSE_DAP_MAX_BAND_COUNT];
	SINT32		gains[HAL_AUDIO_LGSE_DAP_MAX_BAND_COUNT];
} HAL_AUDIO_LGSE_DAP_BANDS_T;

/**
 * HAL AUDIO OPTIMIZER DAP Bands Definition
 *
*/
typedef struct HAL_AUDIO_LGSE_DAP_OPTIMIZER_BANDS
{
	UINT32 		bandCount;	/* Band count: 1-20 */
	UINT32 		freqs[HAL_AUDIO_LGSE_DAP_MAX_BAND_COUNT];
	SINT32		gains[HAL_AUDIO_LGSE_DAP_MAX_CHANNEL_COUNT][HAL_AUDIO_LGSE_DAP_MAX_BAND_COUNT];
} HAL_AUDIO_LGSE_DAP_OPTIMIZER_BANDS_T;

/**
 * HAL AUDIO DAP Volume Modeler Definition
 *
 */
typedef struct HAL_AUDIO_LGSE_DAP_VOLUME_MODELER
{
	BOOLEAN		bEnable;					/* Enables or disables the Volume Modeler 0(off), 1(on) */
	SINT32		calibration; 				/* Used to fine-tune the manufacturer calibrated reference level to the listening environment -320 to 320 */
} HAL_AUDIO_LGSE_DAP_VOLUME_MODELER_T;

/**
 * HAL AUDIO DAP Volume Maximizer Definition
 *
 */
typedef struct HAL_AUDIO_LGSE_DAP_VOLUME_MAXIMIZER
{
	UINT32 		boost;						/* Controls the amount of gain applied by the Volume Maximizer while the Volume Leveler is enabled. 0 to 192 */
} HAL_AUDIO_LGSE_DAP_VOLUME_MAXIMIZER_T;

/**
 * HAL AUDIO DAP Audio Optimizer System Parameter Definition
 *
*/
typedef struct HAL_AUDIO_LGSE_DAP_OPTIMIZER
{
	BOOLEAN 								bIsEnable;		/* Enables or disables the Audio Optimizer 0 (disabled), !0 (enabled) */
	HAL_AUDIO_LGSE_DAP_OPTIMIZER_BANDS_T 	stBands;		/* Sets the parameters for the Audio Optimizer bands, including number of bands,
															   frequencies for each band supplied as integral values in Hz, and corresponding gains for each band and each channel
															   Band Count: 1-20
															   Band Frequencies: 20-20,000 Hz
															   Band Gains: -30 to +30 dB */
} HAL_AUDIO_LGSE_DAP_OPTIMIZER_T;

/**
 * HAL AUDIO DAP Process Optimizer System Parameter Definition
 *
*/
typedef struct HAL_AUDIO_LGSE_DAP_PROCESS_OPTIMIZER
{
	BOOLEAN 								bIsEnableProcess;	/* Enables or disables the Audio Optimizer 0 (disabled), !0 (enabled) */
	HAL_AUDIO_LGSE_DAP_OPTIMIZER_BANDS_T 	stBandsProcess;		/* Sets the parameters for the Process Optimizer bands, including number of bands,
																   frequencies for each band supplied as integral values in Hz, and corresponding gains for each band and each channel
																   Band Count: 1-20
																   Band Frequencies: 20-20,000 Hz
																   Band Gains: -30 to +30 dB */
} HAL_AUDIO_LGSE_DAP_PROCESS_OPTIMIZER_T;

/**
 * HAL AUDIO DAP Surround Decoder Definition
 *
 */
typedef struct HAL_AUDIO_LGSE_DAP_SURROUND_DECODER
{
	BOOLEAN 	bEnable;					/* Enables or disables the Surround Decoder 0(off), 1(on) */
} HAL_AUDIO_LGSE_DAP_SURROUND_DECODER_T;

/**
 * HAL AUDIO DAP Surround Compressor Definition
 *
 */
typedef struct HAL_AUDIO_LGSE_DAP_SURROUND_COMPRESSOR
{
	UINT32 		boost;						/* The maximum amount of gain which can be applied to a surround channel by the Surround Compressor feature 0 to 96*/
} HAL_AUDIO_LGSE_DAP_SURROUND_COMPRESSOR_T;

/**
 * HAL AUDIO DAP Virtualizer Speaker Angle System Parameter Definition
 *
*/
typedef struct HAL_AUDIO_LGSE_DAP_VIRTUALIZER_SPEAKER_ANGLE
{
	UINT32		frontSpeakerAngle;			/* Virtualization parameters for the left and right channels 1 to 30 */
	UINT32		surroundSpeakerAngle;		/* Virtualization parameters for the left and right surround channels 1 to 30*/
	UINT32		heightSpeakerAngle;			/* Virtualization parameters for the left and right height channels 1 to 30 */
} HAL_AUDIO_LGSE_DAP_VIRTUALIZER_SPEAKER_ANGLE_T;

/**
 * HAL AUDIO DAP Audio Intelligence EQ Parameter Definition
 *
*/
typedef struct HAL_AUDIO_LGSE_DAP_INTELLIGENCE_EQ
{
	BOOLEAN 					bIsEnable;					/* Enables or disables the Audio Optimizer 0 (disabled), !0 (enabled) */
	HAL_AUDIO_LGSE_DAP_BANDS_T 	stIntelligentEQBands; 		/* Sets parameters for the Intelligent Equalizer bands, including number of bands,
															   frequencies for each band supplied as integral values in Hz, and corresponding target gains for each band
															   Band count: 1-20
															   Band frequencies: 20-20,000 Hz
															   Band gains: -30 to +30 dB */
	UINT32						strength;					/* Determines how aggressive the feature is at attempting to match the target timbre curve 0 to 16*/
} HAL_AUDIO_LGSE_DAP_INTELLIGENCE_EQ_T;

/**
 * HAL AUDIO DAP Intelligence System Parameter Definition
 *
*/
typedef struct HAL_AUDIO_LGSE_DAP_MEDIA_INTELLIGENCE
{
	BOOLEAN 	bIsEnable; 									/* Enables or disables the Media Intelligence settings 0(disabled), !0(enabled) */
	BOOLEAN 	bIsEqualizerEnable; 						/* If this parameter is enabled, Intelligent Equalizer uses information from Media Intelligence to improve the quality of the processing. 0(disabled), !0 (enabled) */
	BOOLEAN 	bIsVolumeLevelerEnable;						/* If this parameter is enabled, Volume Leveler uses information from Media Intelligence to improve the quality of the processing. 0(disabled), !0 (enabled) */
	BOOLEAN 	bIsDialogueEnhancerEnable; 					/* If this parameter is enabled, Dialogue Enhancer uses information from Media Intelligence to improve the quality of the processing. 0(disabled), !0(enabled) */
	BOOLEAN 	bIsSurroundCompressorEnable;				/* The Surround Compressor is used only when the headphone or speaker virtualizer is enabled. If this parameter is enabled, Surround Compressor will use information from Media Intelligence to improve the quality of the processing. 0(disabled), !0(enabled) */
} HAL_AUDIO_LGSE_DAP_MEDIA_INTELLIGENCE_T;

/**
 * HAL AUDIO DAP Graphic Equalizer System Parameter Definition
 *
*/
typedef struct HAL_AUDIO_LGSE_DAP_GRAPHICAL_EQ
{
	BOOLEAN 					bIsEnable;			/* Enables or disables the Graphical Equalizer 0 (disabled), !0 (enabled)*/
	HAL_AUDIO_LGSE_DAP_BANDS_T 	stBands;			/* Sets parameters for the Graphical Equalizer bands, including number of bands, frequencies for each band supplied as integral values in Hz, and corresponding target gains for each band
													   Band count: 1-20
													   Band frequencies: 20-20,000 Hz
													   Band gains: -36 to 36 dB */
} HAL_AUDIO_LGSE_DAP_GRAPHICAL_EQ_T;

/**
 * HAL AUDIO DAP Perceptual Height Filter Parameter Definition
 *
*/
typedef struct HAL_AUDIO_LGSE_DAP_PERCEPTUAL_HEIGHT_FILTER
{
	HAL_AUDIO_LGSE_DAP_PERCEPTUAL_HEIGHT_FILTER_MODE_T	mode;	/* Set the mode of operation of the perceptual height filter */

} HAL_AUDIO_LGSE_DAP_PERCEPTUAL_HEIGHT_FILTER_T;

/**
 * HAL AUDIO DAP Bass Enhancer System Parameter Definition
 *
*/
typedef struct HAL_AUDIO_LGSE_DAP_BASS_ENHANCER
{
	BOOLEAN 	bIsEnable;					/* Enables or disables the Bass Enhancer 0(disabled), !0(enabled) */
	UINT32 		boost; 						/* Sets the amount of bass boost applied by the Bass Enhancer	0-24 dB */
	UINT32 		cutoffFreq;					/* Sets the cutoff frequency used by the Bass Enhancer 20-2,000 Hz */
	UINT32 		width; 						/* Sets the width of the bass enhancement boost curve used by the Bass Enhancer, in units of octaves below the cutoff frequency	0.125-4 octaves*/
} HAL_AUDIO_LGSE_DAP_BASS_ENHANCER_T;

/**
 * HAL AUDIO DAP Bass Extraction System Parameter Definition
 *
*/
typedef struct HAL_AUDIO_LGSE_DAP_BASS_EXTRACTION
{
	BOOLEAN 	bIsEnable;					/* Enables or disables the Bass Extraction 0(disabled), !0(enabled) */
	UINT32 		cutoffFreq;					/* Specifies the filter cutoff frequency used for Bass Extraction 45(u)-200(u) Hz */
} HAL_AUDIO_LGSE_DAP_BASS_EXTRACTION_T;

/**
 * HAL AUDIO Virtual Bass Bands Definition
 *
*/
typedef struct HAL_AUDIO_LGSE_VIRTUAL_BASS_BANDS
{
	UINT32 		num;											/* default: 3 */
	SINT32		subgains[HAL_AUDIO_LGSE_DAP_MAX_BAND_COUNT]; 	/* -480 to 0 */
} HAL_AUDIO_LGSE_VIRTUAL_BASS_BANDS_T;

/**
 * HAL AUDIO DAP Virtual Bass System Parameter Definition
 *
*/
typedef struct HAL_AUDIO_LGSE_DAP_VIRTUAL_BASS
{
	HAL_AUDIO_LGSE_DAP_VIRTUAL_BASS_MODE_T 	enMode;						/* Determines the harmonics for Virtual Bass processing
																		   0 : Delay only
																		   1 : Second-order harmonics
																		   2 : Second- and third-order harmonics
																		   3 : Second-, third-, and fourth-order  harmonics */
	UINT32 									lowSrcFreqRange; 			/* Defines the lowest frequency of the source to be transposed 30-90 Hz (default 35 Hz) */
	UINT32 									highSrcFreqRange;			/* Defines the highest frequency of the source to be transposed	90-270 Hz (default 160 Hz) */
	SINT32 									overallGain;				/* Defines the overall gain applied to the output of the Virtual Bass transposer -480 to 0 (-30 to 0 dB) (default 0) */
	SINT32 									slopeGain;					/* used to adjust the envelope of the transposer output -3 to 0 (-3 to 0 dB) (default 0) */
	HAL_AUDIO_LGSE_VIRTUAL_BASS_BANDS_T		stSubGains;					/* Defines the gains applied to individual harmonics at the output of the Virtual Bass transposer -480 to 0 (-30 to 0 dB) (default 0) for the 2nd, 3rd and 4th harmonic respectively */
	UINT32 									lowMixedFreqBoundaries; 	/* Defines the lower boundary of the frequency range in which transposed harmonics are mixed 0-375 Hz */
	UINT32 									highMixedFreqBoundaries;	/* Defines the upper boundary of the frequency range in which transposed harmonics are mixed 281-938 Hz */
} HAL_AUDIO_LGSE_DAP_VIRTUAL_BASS_T;

/**
 * HAL AUDIO DAP Regulator Tuning Thresholds Definition
 *
*/
typedef struct HAL_AUDIO_LGSE_DAP_REGULATOR_TUNING_THRESHOLDS
{
	UINT32		bands;													/* nb_bands: 1-20 */
	UINT32 		bandCenters[HAL_AUDIO_LGSE_DAP_MAX_BAND_COUNT];			/* p_band_centers: 20-20,000 Hz */
	SINT32		lowThresholds[HAL_AUDIO_LGSE_DAP_MAX_BAND_COUNT];		/* p_low_thresholds: -130 to 0 dB */
	SINT32		highThresholds[HAL_AUDIO_LGSE_DAP_MAX_BAND_COUNT]; 		/* p_high_thresholds: -130 to 0 */
	BOOLEAN		bBandIsolateFlag[HAL_AUDIO_LGSE_DAP_MAX_BAND_COUNT];	/* Band isolation flag: 0(band is not isolated), 1(band is isolated) */
} HAL_AUDIO_LGSE_DAP_REGULATOR_TUNING_THRESHOLDS_T;

/**
 * HAL AUDIO DAP Regulator Definition
 *
*/
typedef struct HAL_AUDIO_LGSE_DAP_REGULATOR
{
	BOOLEAN 											bIsEnable;			/* Enables or disables the Audio Regulator 0 (disabled), !0 (enabled)*/
	HAL_AUDIO_LGSE_DAP_REGULATOR_TUNING_THRESHOLDS_T 	stTuningthresholds;	/* Provides the Audio Regulator with tuning coefficients. These coefficients are only used when the Audio Regulator is operating in Speaker Distortion mode.
																			   nb_bands : 1-20
																			   p_band_centers : 20-20,000 Hz
																			   p_low_thresholds : -130 to 0 dB
																			   p_high_thresholds : -130 to 0 dB*/
	UINT32 												overdrive;						/* Sets the boost to be applied to all of the tuned low and high thresholds (as set by dap_cpdp_regulator_tuning_configure) when the Audio Regulator is operating in speaker distortion mode 0-12 dB */
	UINT32 												timbrePreservationSetter;		/* Sets the timbre preservation amount for the Audio Regulator, in both operating modes Values close to zero maximize loudness; values close to one maximize the preservation of signal tonality. 0.0-1.0 */
	UINT32 												distortionRelaxationAmount; 	/* Sets the Audio Regulator distortion relaxation amount 0-9 dB*/
	HAL_AUDIO_LGSE_DAP_REGULATOR_MODE_T 				enOperatingMode;	/* Sets the operating mode of the Audio Regulator:
																			 - Peak Protection mode: The Audio Regulator has the same operating characteristics for each band.
																			 - Speaker Distortion mode: The tuning configuration supplied by
																			   dap_cpdp_regulator_tuning_configure and dap_cpdp_regulator_overdrive_set is used to
																			   give the Audio Regulator per-band operating characteristics.
																			   0  : Peak Protection mode
																			   !0 : Speaker Distortion mode*/
} HAL_AUDIO_LGSE_DAP_REGULATOR_T;

/**
 * HAL AUDIO DAP System parameter Definition
 *
 */
typedef struct HAL_AUDIO_LGSE_DAP_SYS_PARAM								/* Set All DAP system param*/
{
	HAL_AUDIO_LGSE_DAP_DIALOGUE_ENHANCER_T			stDialogueEnhancer;			/* Set Dialogue Enhancer parameters */
	HAL_AUDIO_LGSE_DAP_VOLUME_LEVELER_T				stVolumeLeveler;			/* Set Volume Leveler */
	HAL_AUDIO_LGSE_DAP_VOLUME_MODELER_T				stVolumeModeler;			/* Set Volume Modeler */
	HAL_AUDIO_LGSE_DAP_VOLUME_MAXIMIZER_T			stVolumeMaximizer;			/* Set Volume Maximizer */
	HAL_AUDIO_LGSE_DAP_OPTIMIZER_T 					stOptimizer;				/* Set Optimizer parameters */
	HAL_AUDIO_LGSE_DAP_PROCESS_OPTIMIZER_T 			stProcessOptimizer;			/* Set Process Optimizer parameters */
	HAL_AUDIO_LGSE_DAP_SURROUND_DECODER_T			stSurroundDecoder;			/* Set Surround Decoder */
	HAL_AUDIO_LGSE_DAP_SURROUND_COMPRESSOR_T		stSurroundCompressor;		/* Set Surround Compressor */
	HAL_AUDIO_LGSE_DAP_VIRTUALIZER_SPEAKER_ANGLE_T	stVirtualizerSpeakerAngle;	/* Set Virtualizer Speaker Angle parameters */
	HAL_AUDIO_LGSE_DAP_INTELLIGENCE_EQ_T			stIntelligenceEQ;			/* Set Intelligence EQ parameters */
	HAL_AUDIO_LGSE_DAP_MEDIA_INTELLIGENCE_T			stMediaIntelligence;		/* Set Media Intelligence parameters */
	HAL_AUDIO_LGSE_DAP_GRAPHICAL_EQ_T 				stGraphicalEQ;				/* Set Graphical EQ parameters */
	HAL_AUDIO_LGSE_DAP_PERCEPTUAL_HEIGHT_FILTER_T	stPerceptualHeightFilter;	/* Set Perceptual Height Filter */
	HAL_AUDIO_LGSE_DAP_BASS_ENHANCER_T 				stBassEnhancer;				/* Set Bass Enhanser parameters */
	HAL_AUDIO_LGSE_DAP_BASS_EXTRACTION_T 			stBassExtraction;			/* Set Bass Extraction parameters */
	HAL_AUDIO_LGSE_DAP_REGULATOR_T 					stRegulator;				/* Set Regulator parameters */
	HAL_AUDIO_LGSE_DAP_VIRTUAL_BASS_T				stVirtualBass;				/* Set Virtual Bass parameters */
} HAL_AUDIO_LGSE_DAP_SYS_PARAM_T;

/**
 * HAL AUDIO DTS Virtual:X (VX) input channel information Definition
 *
 */

typedef enum
{
    HAL_AUDIO_LGSE_VX_INPUT_CHANNEL_20 = 0,
    HAL_AUDIO_LGSE_VX_INPUT_CHANNEL_51 = 1,
} HAL_AUDIO_LGSE_VX_INPUT_CHANNEL_T;

/**
 * JAPAN 4K ES Push mode type
 *
 */
typedef enum {
	HAL_AUDIO_ES_NORMAL        = 0,
	HAL_AUDIO_ES_ARIB_UHD      = 1,
	HAL_AUDIO_ES_PLAY_ALSA     = 2,
	HAL_AUDIO_ES_MAX
} HAL_AUDIO_ES_MODE_T;


typedef struct {
    sndout_earc_output_set_type_t    set_type;
    sndout_earc_output_codec_type_t  codec_type;
    sndout_earc_output_channel_num_t channel_num;
    sndout_earc_output_sample_rate_t sample_rate;
    sndout_earc_output_mix_option_t  mix_option;
} ALSA_SNDOUT_EARC_INFO;

/******************************************************************************
	함수 선언 (Function Declaration)
******************************************************************************/
DTV_STATUS_T HAL_AUDIO_InitializeModule(HAL_AUDIO_SIF_TYPE_T eSifType);
DTV_STATUS_T HAL_AUDIO_SetSPKOutput(UINT8 i2sNumber, HAL_AUDIO_SAMPLING_FREQ_T samplingFreq);

/* Open, Close */
DTV_STATUS_T HAL_AUDIO_TP_Open(HAL_AUDIO_TP_INDEX_T tpIndex);
DTV_STATUS_T HAL_AUDIO_TP_Close(HAL_AUDIO_TP_INDEX_T tpIndex);
DTV_STATUS_T HAL_AUDIO_ADC_Open(void);
DTV_STATUS_T HAL_AUDIO_ADC_Close(void);
DTV_STATUS_T HAL_AUDIO_HDMI_Open(void);
DTV_STATUS_T HAL_AUDIO_HDMI_Close(void);
DTV_STATUS_T HAL_AUDIO_HDMI_OpenPort(HAL_AUDIO_HDMI_INDEX_T hdmiIndex);
DTV_STATUS_T HAL_AUDIO_HDMI_ClosePort(HAL_AUDIO_HDMI_INDEX_T hdmiIndex);
DTV_STATUS_T HAL_AUDIO_AAD_Open(void);
DTV_STATUS_T HAL_AUDIO_AAD_Close(void);
DTV_STATUS_T HAL_AUDIO_DIRECT_SYSTEM_Open(void);
DTV_STATUS_T HAL_AUDIO_DIRECT_SYSTEM_Close(void);
DTV_STATUS_T HAL_AUDIO_ADEC_Open(HAL_AUDIO_ADEC_INDEX_T adecIndex);
DTV_STATUS_T HAL_AUDIO_ADEC_Close(HAL_AUDIO_ADEC_INDEX_T adecIndex);
DTV_STATUS_T HAL_AUDIO_AENC_Open(HAL_AUDIO_AENC_INDEX_T aencIndex);
DTV_STATUS_T HAL_AUDIO_AENC_Close(HAL_AUDIO_AENC_INDEX_T aencIndex);
DTV_STATUS_T HAL_AUDIO_SE_Open(void);
DTV_STATUS_T HAL_AUDIO_SE_Close(void);
DTV_STATUS_T HAL_AUDIO_SNDOUT_Open(common_output_ext_type_t soundOutType);
DTV_STATUS_T HAL_AUDIO_SNDOUT_Close(common_output_ext_type_t soundOutType);

/* Connect & Disconnect */
DTV_STATUS_T HAL_AUDIO_TP_Connect(HAL_AUDIO_RESOURCE_T currentConnect, HAL_AUDIO_RESOURCE_T inputConnect, HAL_AUDIO_ADEC_INDEX_T adecIndex);
DTV_STATUS_T HAL_AUDIO_TP_Disconnect(HAL_AUDIO_RESOURCE_T currentConnect, HAL_AUDIO_RESOURCE_T inputConnect, HAL_AUDIO_ADEC_INDEX_T adecIndex);
DTV_STATUS_T HAL_AUDIO_ADC_Connect(UINT8 portNum);
DTV_STATUS_T HAL_AUDIO_ADC_Disconnect(UINT8 portNum);
DTV_STATUS_T HAL_AUDIO_HDMI_Connect(void);
DTV_STATUS_T HAL_AUDIO_HDMI_Disconnect(void);
DTV_STATUS_T HAL_AUDIO_HDMI_ConnectPort(HAL_AUDIO_HDMI_INDEX_T hdmiIndex);
DTV_STATUS_T HAL_AUDIO_HDMI_DisconnectPort(HAL_AUDIO_HDMI_INDEX_T hdmiIndex);
DTV_STATUS_T HAL_AUDIO_AAD_Connect(void);
DTV_STATUS_T HAL_AUDIO_AAD_Disconnect(void);
DTV_STATUS_T HAL_AUDIO_DIRECT_SYSTEM_Connect(void);
DTV_STATUS_T HAL_AUDIO_DIRECT_SYSTEM_Disconnect(void);
DTV_STATUS_T HAL_AUDIO_DIRECT_ADEC_Connect(int adec_port);
DTV_STATUS_T HAL_AUDIO_DIRECT_ADEC_Disconnect(int adec_port);
DTV_STATUS_T HAL_AUDIO_ADEC_Connect(int adec_port, adec_src_port_index_ext_type_t inputConnect);
DTV_STATUS_T HAL_AUDIO_ADEC_Disconnect(int adec_port, adec_src_port_index_ext_type_t inputConnect);
DTV_STATUS_T HAL_AUDIO_AMIX_Connect(HAL_AUDIO_RESOURCE_T currentConnect, HAL_AUDIO_RESOURCE_T inputConnect);
DTV_STATUS_T HAL_AUDIO_AMIX_Disconnect(HAL_AUDIO_RESOURCE_T currentConnect, HAL_AUDIO_RESOURCE_T inputConnect);
DTV_STATUS_T HAL_AUDIO_AENC_Connect(HAL_AUDIO_RESOURCE_T currentConnect, HAL_AUDIO_RESOURCE_T inputConnect);
DTV_STATUS_T HAL_AUDIO_AENC_Disconnect(HAL_AUDIO_RESOURCE_T currentConnect, HAL_AUDIO_RESOURCE_T inputConnect);
DTV_STATUS_T HAL_AUDIO_SE_Connect(HAL_AUDIO_RESOURCE_T inputConnect);
DTV_STATUS_T HAL_AUDIO_SE_Disconnect(HAL_AUDIO_RESOURCE_T inputConnect);
DTV_STATUS_T HAL_AUDIO_SNDOUT_Connect(common_output_ext_type_t output_type, int res_input);
DTV_STATUS_T HAL_AUDIO_SNDOUT_Disconnect(common_output_ext_type_t output_type, int res_input);
DTV_STATUS_T KHAL_AUDIO_ALSA_SNDOUT_Connect(int opened_device, int mixer_pin);
DTV_STATUS_T KHAL_AUDIO_ALSA_SNDOUT_Disconnect(int opened_device, int mixer_pin);
DTV_STATUS_T KHAL_AUDIO_SNDOUT_Connect_Set_Status_to_ALSA(common_output_ext_type_t output_type, unsigned int* connected_device);
DTV_STATUS_T KHAL_AUDIO_SNDOUT_eARC_OutputType(ALSA_SNDOUT_EARC_INFO earc_info);
DTV_STATUS_T HAL_AUDIO_BT_Connect(int input_port);
DTV_STATUS_T HAL_AUDIO_BT_Disconnect(int input_port);
BOOLEAN IS_BT_Connected(void);
DTV_STATUS_T HAL_AUDIO_WISA_Connect(int input_port);
DTV_STATUS_T HAL_AUDIO_WISA_Disconnect(int input_port);
BOOLEAN IS_WISA_Connected(void);
DTV_STATUS_T HAL_AUDIO_SE_BT_Connect(int input_port);
DTV_STATUS_T HAL_AUDIO_SE_BT_Disconnect(int input_port);
BOOLEAN IS_SE_BT_Connected(void);

/* Start & Stop */
DTV_STATUS_T HAL_AUDIO_StartDecoding(HAL_AUDIO_ADEC_INDEX_T adecIndex, adec_src_codec_ext_type_t audioType);
DTV_STATUS_T HAL_AUDIO_StopDecoding(HAL_AUDIO_ADEC_INDEX_T adecIndex);
DTV_STATUS_T HAL_AUDIO_SetMainDecoderOutput(HAL_AUDIO_ADEC_INDEX_T adecIndex);
DTV_STATUS_T HAL_AUDIO_SetMainAudioOutput(HAL_AUDIO_INDEX_T audioIndex);
DTV_STATUS_T HAL_AUDIO_DIRECT_StartDecoding(HAL_AUDIO_ADEC_INDEX_T adecIndex, HAL_AUDIO_SRC_TYPE_T audioType);

DTV_STATUS_T HAL_AUDIO_SETUP_ATP(HAL_AUDIO_ADEC_INDEX_T adecIndex);
DTV_STATUS_T HAL_AUDIO_ADEC_CODEC(int adec_port, adec_src_codec_ext_type_t codec_type);

/* HDMI */
DTV_STATUS_T HAL_AUDIO_HDMI_GetPortAudioMode(HAL_AUDIO_HDMI_INDEX_T hdmiIndex, ahdmi_type_ext_type_t *pHDMIMode);
DTV_STATUS_T HAL_AUDIO_HDMI_SetPortAudioReturnChannel(HAL_AUDIO_HDMI_INDEX_T hdmiIndex, BOOLEAN bOnOff);
DTV_STATUS_T HAL_AUDIO_HDMI_GetPortCopyInfo(HAL_AUDIO_HDMI_INDEX_T hdmiIndex, ahdmi_copyprotection_ext_type_t *pCopyInfo);
DTV_STATUS_T RHAL_AUDIO_HDMI_SetEnhancedAudioReturnChannel(BOOLEAN bOnOff);

/* Decoder */
DTV_STATUS_T HAL_AUDIO_SetSyncMode(HAL_AUDIO_ADEC_INDEX_T adecIndex, BOOLEAN bOnOff);
DTV_STATUS_T HAL_AUDIO_SetDolbyDRCMode(HAL_AUDIO_ADEC_INDEX_T adecIndex, adec_dolbydrc_mode_ext_type_t drcMode);
DTV_STATUS_T HAL_AUDIO_SetDownMixMode(HAL_AUDIO_ADEC_INDEX_T adecIndex, adec_downmix_mode_ext_type_t downmixMode);
DTV_STATUS_T HAL_AUDIO_GetDecodingType(HAL_AUDIO_ADEC_INDEX_T adecIndex, HAL_AUDIO_SRC_TYPE_T *pAudioType);
DTV_STATUS_T HAL_AUDIO_GetAdecStatus(HAL_AUDIO_ADEC_INDEX_T adecIndex, HAL_AUDIO_ADEC_INFO_T *pAudioAdecInfo);
DTV_STATUS_T HAL_AUDIO_SetDualMonoOutMode(HAL_AUDIO_ADEC_INDEX_T adecIndex, adec_dualmono_mode_ext_type_t outputMode);
DTV_STATUS_T HAL_AUDIO_TP_GetAudioPTS(HAL_AUDIO_ADEC_INDEX_T adecIndex, UINT32 *pPts);
DTV_STATUS_T HAL_AUDIO_TP_SetAudioDescriptionMain(HAL_AUDIO_ADEC_INDEX_T main_adecIndex, BOOLEAN bOnOff);
DTV_STATUS_T HAL_AUDIO_SetTrickMode(HAL_AUDIO_ADEC_INDEX_T adecIndex, adec_trick_mode_ext_type_t eTrickMode);
DTV_STATUS_T HAL_AUDIO_TP_GetBufferStatus(HAL_AUDIO_ADEC_INDEX_T adecIndex, UINT32 *pMaxSize, UINT32 *pFreeSize);

/* Encoder */
long RHAL_AUDIO_Encoder_DataDeliver_loop(int aenc_index, HAL_AUDIO_AENC_DATA_T *Msg);
void HAL_AUDIO_MEMOUT_ReleaseData(long data_size);
BOOLEAN aenc_flow_start(HAL_AUDIO_ADEC_INDEX_T adecIndex, aenc_encoding_format_t format, aenc_bitrate_t bitrate, adec_src_port_index_ext_type_t inputConnect, HAL_AUDIO_VOLUME_T volume, UINT32 gain);
void aenc_flow_stop(HAL_AUDIO_ADEC_INDEX_T adecIndex, adec_src_port_index_ext_type_t inputConnect);
DTV_STATUS_T KHAL_AUDIO_SetEAC3_ATMOS_ENCODE(BOOLEAN bOnoff);

/* AC-4 decoder */
/* AC-4 Auto Presenetation Selection. Refer to "Selection using system-level preferences" of "Dolby MS12 Multistream Decoder Implementation integration manual" */
/* When Auto Presentation selection is set during decoding,
   if setting value is identical with the value already set, audio should be decoded without disconnecting and noise.
   if setting value is different with the value already set, audio can be disconnected withtout noise. */
/* ISO 639-1 or ISO 639-2 can be used as Language Code type. */
/* Language Code type is decided by enCodetype(HAL_AUDIO_LANG_CODE_TYPE_T) */
/* ISO 639-1 code is defined upper 2byte of UINT32 and rest 2byte includes 0 example : 'e''n'00 */
/* ISO 639-2 code is defined upper 3byte of UINT32 and rest 1byte includes 0 example : 'e''n''g'0 */
DTV_STATUS_T HAL_AUDIO_AC4_SetAutoPresentationFirstLanguage(HAL_AUDIO_ADEC_INDEX_T adecIndex, HAL_AUDIO_LANG_CODE_TYPE_T enCodeType, UINT32 firstLang); 	// default : none
DTV_STATUS_T HAL_AUDIO_AC4_SetAutoPresentationSecondLanguage(HAL_AUDIO_ADEC_INDEX_T adecIndex, HAL_AUDIO_LANG_CODE_TYPE_T enCodeType, UINT32 secondLang); 	// default : none
DTV_STATUS_T HAL_AUDIO_AC4_SetAutoPresentationADMixing(HAL_AUDIO_ADEC_INDEX_T adecIndex, BOOLEAN bIsEnable);				// default: FALSE
DTV_STATUS_T HAL_AUDIO_AC4_SetAutoPresentationADType(HAL_AUDIO_ADEC_INDEX_T adecIndex, adec_ac4_ad_ext_type_t enADType); 	// default : 'VI'
DTV_STATUS_T HAL_AUDIO_AC4_SetAutoPresentationPrioritizeADType(HAL_AUDIO_ADEC_INDEX_T adecIndex, BOOLEAN bIsEnable); 		// default : FALSE

/* AC-4 Dialogue Enhancement */
/* This Enhancement Gain just affect to AC-4 codec and DAP Enhancement amount does not affect AC-4 codec.
   When AC-4 Dialog Enhancement Gain is set during decoding,
   if setting value is identical with the value already set, audio should be decoded without disconnecting and noise.
   if setting value is different with the value already set, audio should be decoded without disconnecting and noise. */
DTV_STATUS_T HAL_AUDIO_AC4_SetDialogueEnhancementGain(HAL_AUDIO_ADEC_INDEX_T adecIndex, UINT32 dialEnhanceGain);	//Gain should be 0~12 in dB, default : 0

DTV_STATUS_T HAL_AUDIO_AC4_SetPresentationGroupIndex(HAL_AUDIO_ADEC_INDEX_T adecIndex, UINT32 pres_group_idx);

/* Volume, Mute & Delay */
DTV_STATUS_T HAL_AUDIO_SetDecoderInputGain(HAL_AUDIO_ADEC_INDEX_T adecIndex, HAL_AUDIO_VOLUME_T volume, UINT32 gain);
DTV_STATUS_T HAL_AUDIO_SetDecoderDelayTime(HAL_AUDIO_ADEC_INDEX_T adecIndex, UINT32 delayTime);
DTV_STATUS_T HAL_AUDIO_SetMixerInputGain(HAL_AUDIO_MIXER_INDEX_T mixerIndex, HAL_AUDIO_VOLUME_T volume);

DTV_STATUS_T HAL_AUDIO_SetSPKOutVolume(HAL_AUDIO_VOLUME_T volume, UINT32 gain);
DTV_STATUS_T HAL_AUDIO_SetSPDIFOutVolume(HAL_AUDIO_VOLUME_T volume, UINT32 gain);
DTV_STATUS_T HAL_AUDIO_SetHPOutVolume(HAL_AUDIO_VOLUME_T volume, BOOLEAN bForced, UINT32 gain);
DTV_STATUS_T HAL_AUDIO_SetARCOutVolume(HAL_AUDIO_VOLUME_T volume, UINT32 gain);
DTV_STATUS_T HAL_AUDIO_SetBTOutVolume(UINT32 gain);
DTV_STATUS_T HAL_AUDIO_SetWISAOutVolume(UINT32 gain);
DTV_STATUS_T HAL_AUDIO_SetSEBTOutVolume(UINT32 gain);
DTV_STATUS_T HAL_AUDIO_SetAudioDescriptionVolume(HAL_AUDIO_ADEC_INDEX_T adecIndex, HAL_AUDIO_VOLUME_T volume);
DTV_STATUS_T HAL_AUDIO_SetDecoderInputMute(HAL_AUDIO_ADEC_INDEX_T adecIndex, BOOLEAN bOnOff);
DTV_STATUS_T HAL_AUDIO_SetMixerInputMute(HAL_AUDIO_MIXER_INDEX_T mixerIndex, BOOLEAN bOnOff);
DTV_STATUS_T HAL_AUDIO_SetSPKOutMute(BOOLEAN bOnOff);
DTV_STATUS_T HAL_AUDIO_SetSPDIFOutMute(BOOLEAN bOnOff, UINT32 output_device);
DTV_STATUS_T HAL_AUDIO_SetHPOutMute(BOOLEAN bOnOff);
DTV_STATUS_T HAL_AUDIO_SetARCOutMute(BOOLEAN bOnOff);
DTV_STATUS_T HAL_AUDIO_SetBTOutMute(BOOLEAN bOnOff);
DTV_STATUS_T HAL_AUDIO_SetWISAOutMute(BOOLEAN bOnOff);
DTV_STATUS_T HAL_AUDIO_SetSEBTOutMute(BOOLEAN bOnOff);

DTV_STATUS_T HAL_AUDIO_GetSPKOutMuteStatus(BOOLEAN *pOnOff);
DTV_STATUS_T HAL_AUDIO_GetSPDIFOutMuteStatus(BOOLEAN *pOnOff);
DTV_STATUS_T HAL_AUDIO_GetHPOutMuteStatus(BOOLEAN *pOnOff);
DTV_STATUS_T HAL_AUDIO_GetARCOutMuteStatus(BOOLEAN *pOnOff);

DTV_STATUS_T HAL_AUDIO_SetSPKOutDelayTime(UINT32 delayTime, BOOLEAN bForced);
DTV_STATUS_T HAL_AUDIO_SetSPDIFOutDelayTime(UINT32 delayTime, BOOLEAN bForced);
DTV_STATUS_T HAL_AUDIO_SetSPDIFSBOutDelayTime(UINT32 delayTime, BOOLEAN bForced);
DTV_STATUS_T HAL_AUDIO_SetHPOutDelayTime(UINT32 delayTime, BOOLEAN bForced);
DTV_STATUS_T HAL_AUDIO_SetARCOutDelayTime(UINT32 delayTime, BOOLEAN bForced);
DTV_STATUS_T HAL_AUDIO_SetBTOutDelayTime(UINT32 delayTime);
DTV_STATUS_T HAL_AUDIO_SetWISAOutDelayTime(UINT32 delayTime);
DTV_STATUS_T HAL_AUDIO_SetSEBTOutDelayTime(UINT32 delayTime);
DTV_STATUS_T HAL_AUDIO_GetStatusInfo(HAL_AUDIO_COMMON_INFO_T *pAudioStatusInfo);
DTV_STATUS_T HAL_AUDIO_SetInputOutputDelay(HAL_AUDIO_INPUT_DELAY_PARAM_T inputParam, HAL_AUDIO_OUTPUT_DELAY_PARAM_T outputParam);

DTV_STATUS_T KHAL_AUDIO_Get_OUTPUT_MuteStatus(UINT32 *status);
DTV_STATUS_T KHAL_AUDIO_Get_INPUT_MuteStatus(UINT32 *status);

/* SPDIF(Sound Bar) */
DTV_STATUS_T HAL_AUDIO_SPDIF_SetOutputType(sndout_optic_mode_ext_type_t eSPDIFMode, BOOLEAN bForced);
DTV_STATUS_T HAL_AUDIO_SPDIF_SetCopyInfo(sndout_optic_copyprotection_ext_type_t copyInfo);
DTV_STATUS_T HAL_AUDIO_SPDIF_SetCategoryCode(UINT8 categoryCode);
DTV_STATUS_T HAL_AUDIO_SPDIF_SetLightOnOff(BOOLEAN bOnOff);
DTV_STATUS_T HAL_AUDIO_SB_SetOpticalIDData(HAL_AUDIO_SB_SET_INFO_T info, unsigned int *ret_checksum);
DTV_STATUS_T HAL_AUDIO_SB_GetOpticalStatus(HAL_AUDIO_SB_GET_INFO_T *pInfo);
DTV_STATUS_T HAL_AUDIO_SB_SetCommand(HAL_AUDIO_SB_SET_CMD_T info);
DTV_STATUS_T HAL_AUDIO_SB_GetCommandStatus(HAL_AUDIO_SB_GET_CMD_T *pInfo);
DTV_STATUS_T HAL_AUDIO_ARC_SetOutputType(HAL_AUDIO_ARC_MODE_T eARCMode, BOOLEAN bForced); // for TB24 ARC DD+ output 2016/06/4 by ykwang.kim
DTV_STATUS_T HAL_AUDIO_ARC_SetCopyInfo(sndout_arc_copyprotection_ext_type_t copyInfo);
DTV_STATUS_T HAL_AUDIO_ARC_SetCategoryCode(UINT8 categoryCode);

/* SE */
//DTV_STATUS_T HAL_AUDIO_LGSE_SetMode(UINT32 *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption);
//DTV_STATUS_T HAL_AUDIO_LGSE_SetMain(UINT32 *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption);
//DTV_STATUS_T HAL_AUDIO_LGSE_SetFN000(UINT32 *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption);
//DTV_STATUS_T HAL_AUDIO_LGSE_SetFN001(UINT32 *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption);
/*DTV_STATUS_T HAL_AUDIO_LGSE_SetFN004(UINT32 *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption,		\*/
//DTV_STATUS_T HAL_AUDIO_LGSE_SetFN005(UINT32 *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption);
//DTV_STATUS_T HAL_AUDIO_LGSE_SetFN008(UINT32 *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption);
//DTV_STATUS_T HAL_AUDIO_LGSE_SetFN009(UINT32 *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption);
//
//DTV_STATUS_T HAL_AUDIO_LGSE_SetFN014(UINT32 *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption);
//DTV_STATUS_T HAL_AUDIO_LGSE_SetFN016(UINT32 *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption);
//DTV_STATUS_T HAL_AUDIO_LGSE_SetFN017(UINT32 *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption);
//DTV_STATUS_T HAL_AUDIO_LGSE_SetFN019(UINT32 *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption);
//DTV_STATUS_T HAL_AUDIO_LGSE_SetFN022(UINT32 *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption);
//DTV_STATUS_T HAL_AUDIO_LGSE_SetFN024(UINT32 *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption);
//DTV_STATUS_T HAL_AUDIO_LGSE_SetFN026(UINT32 *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption);
//DTV_STATUS_T HAL_AUDIO_LGSE_SetFN028(UINT32 *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption);
//DTV_STATUS_T HAL_AUDIO_LGSE_SetFN029(UINT32 *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption);

DTV_STATUS_T HAL_AUDIO_LGSE_Init(lgse_se_init_ext_type_t se_init);
DTV_STATUS_T HAL_AUDIO_LGSE_SetMode(UINT32 pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption ,int index);
DTV_STATUS_T HAL_AUDIO_LGSE_SetMain(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption ,int index);
DTV_STATUS_T HAL_AUDIO_LGSE_SetFN000(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption ,int index);
DTV_STATUS_T HAL_AUDIO_LGSE_SetFN001(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption ,int index);
DTV_STATUS_T HAL_AUDIO_LGSE_SetFN004(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption,        \
									 HAL_AUDIO_LGSE_VARIABLE_MODE_T varOption ,int index);
DTV_STATUS_T HAL_AUDIO_LGSE_SetFN005(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption ,int index);
DTV_STATUS_T HAL_AUDIO_LGSE_SetFN008(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption ,int index);
DTV_STATUS_T HAL_AUDIO_LGSE_SetFN009(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption ,int index);
DTV_STATUS_T HAL_AUDIO_LGSE_SetFN010(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_ACCESS_T dataOption ,int index);
DTV_STATUS_T HAL_AUDIO_LGSE_SetFN014(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption ,int index);
DTV_STATUS_T HAL_AUDIO_LGSE_SetFN016(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption ,int index);
DTV_STATUS_T HAL_AUDIO_LGSE_SetFN017(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption ,int index);
DTV_STATUS_T HAL_AUDIO_LGSE_SetFN019(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption ,int index);
DTV_STATUS_T HAL_AUDIO_LGSE_SetFN022(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption ,int index);
DTV_STATUS_T HAL_AUDIO_LGSE_SetFN023(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption ,int index);
DTV_STATUS_T HAL_AUDIO_LGSE_SetFN024(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption ,int index);
DTV_STATUS_T HAL_AUDIO_LGSE_SetFN026(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption ,int index);
DTV_STATUS_T HAL_AUDIO_LGSE_SetFN028(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption ,int index);
DTV_STATUS_T HAL_AUDIO_LGSE_SetFN029(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption ,int index);
DTV_STATUS_T HAL_AUDIO_LGSE_SetFN000_1(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption ,int index);
DTV_STATUS_T HAL_AUDIO_LGSE_SetFN002(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption ,int index);
DTV_STATUS_T HAL_AUDIO_LGSE_SetFN002_1(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption ,int index);
DTV_STATUS_T HAL_AUDIO_LGSE_SetFN004_1(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption,		\
									 HAL_AUDIO_LGSE_VARIABLE_MODE_T varOption ,int index);
DTV_STATUS_T HAL_AUDIO_LGSE_SetFN008_1(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption ,int index);
DTV_STATUS_T HAL_AUDIO_LGSE_SetFN017_2(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption ,int index);
DTV_STATUS_T HAL_AUDIO_LGSE_SetFN017_3(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption ,int index);
DTV_STATUS_T HAL_AUDIO_LGSE_SetFN022_1(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption ,int index);
DTV_STATUS_T HAL_AUDIO_LGSE_SetFN022_2(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption ,int index);
DTV_STATUS_T HAL_AUDIO_LGSE_SetFN030(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption ,int index);
DTV_STATUS_T HAL_AUDIO_LGSE_SetFN030_1(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption ,int index);
DTV_STATUS_T HAL_AUDIO_LGSE_GetData(HAL_AUDIO_LGSE_FUNCLIST_T funcList, HAL_AUDIO_LGSE_DATA_ACCESS_T rw, 			\
									UINT32 *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption ,int index);
DTV_STATUS_T HAL_AUDIO_LGSE_RegSmartSoundCallback(pfnLGSESmartSound pfnCallBack, UINT32 callbackPeriod, int index);
DTV_STATUS_T HAL_AUDIO_LGSE_SetAISoundMode(BOOLEAN bEnable, int index);
DTV_STATUS_T HAL_AUDIO_LGSE_SetSoundEngineMode(lgse_se_init_ext_type_t se_init, lgse_se_mode_ext_type_t seMode, int index);
DTV_STATUS_T HAL_AUDIO_LGSE_GetSoundEngineMode(lgse_se_mode_ext_type_t *pSEMode);
DTV_STATUS_T HAL_AUDIO_LGSE_SetLGSEDownMix(BOOLEAN bEnable, int index);

/**
 * HAL AUDIO DAP API define
 *
 * 20160702 sanghyun.han(MS12 V1.30), 20160909 kwangshik.kim(MS12 V2.00)
 */
DTV_STATUS_T HAL_AUDIO_LGSE_SetPostProcess(lgse_postproc_mode_ext_type_t ePostProcMode, int index);		/* Select Audio Post Processing mode */
DTV_STATUS_T HAL_AUDIO_LGSE_GetPostProcess(lgse_postproc_mode_ext_type_t *pePostProcMode, int index);	/* Get current Audio Post Processing mode */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetSurroundVirtualizerMode(lgse_dap_surround_virtualizer_mode_t stSurroundVirtualizerMode, int index);						/* Set DAP Surround Virtualizer mode */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetSurroundVirtualizerMode(lgse_dap_surround_virtualizer_mode_t *pstSurroundVirtualizerMode, int index);				    	/* Get current DAP Surround Virtualizer mode */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetDialogueEnhancer(DAP_PARAM_DIALOGUE_ENHANCER stDialogueEnhancer, int index);											/* Set DAP Dialogue Enhancer */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetDialogueEnhancer(DAP_PARAM_DIALOGUE_ENHANCER *pstDialogueEnhancer, int index);											/* Get current DAP Dialogue Enhancer */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetVolumeLeveler(DAP_PARAM_VOLUME_LEVELER stVolumeLeveler, int index);													/* Set DAP Volume Leveler */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetVolumeLeveler(DAP_PARAM_VOLUME_LEVELER *pstVolumeLeveler, int index);													/* Get current DAP Volume Leveler status */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetVolumeModeler(DAP_PARAM_VOLUME_MODELER stVolumeModeler, int index);													/* Set DAP Volume Modeler */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetVolumeModeler(DAP_PARAM_VOLUME_MODELER *pstVolumeModeler, int index);													/* Get current DAP Volume Modeler status */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetVolumeMaximizer(int stVolumeMaximizer, int index);												/* Set DAP Volume Maximizer */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetVolumeMaximizer(int *pstVolumeMaximizer, int index);												/* Get current DAP Volume Maximizer status */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetOptimizer(DAP_PARAM_AUDIO_OPTIMIZER stOptimizer, int index);																	/* Set DAP Optimizer */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetOptimizer(DAP_PARAM_AUDIO_OPTIMIZER *pstOptimizer, int index);																	/* Set current DAP Optimizer status */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetProcessOptimizer(HAL_AUDIO_LGSE_DAP_PROCESS_OPTIMIZER_T stProcessOptimizer, int index);											/* Set DAP Process Optimizer */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetProcessOptimizer(HAL_AUDIO_LGSE_DAP_PROCESS_OPTIMIZER_T *pstProcessOptimizer, int index);											/* Set current DAP Process Optimizer status */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetSurroundDecoder(int stSurroundDecoder, int index);												/* Set DAP Surround Decoder */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetSurroundDecoder(int *pstSurroundDecoder, int index);												/* Set current DAP Surround Decoder status */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetSurroundCompressor(int stSurroundCompressor, int index);										/* Set DAP Surround Compressor */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetSurroundCompressor(int *pstSurroundCompressor, int index);										/* Get current DAP Surround Compressor status */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetVirtualizerSpeakerAngle(DAP_PARAM_VIRTUALIZER_SPEAKER_ANGLE stSpeaker_angle, int index);						/* Set DAP Virtualizer Speaker Angle */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetVirtualizerSpeakerAngle(DAP_PARAM_VIRTUALIZER_SPEAKER_ANGLE *pstSpeaker_angle, int index);						/* Set current DAP Virtualizer Speaker Angle status */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetIntelligenceEQ(DAP_PARAM_INTELLIGENT_EQUALIZER stIntelligenceEQ, int index);													/* Set DAP Intelligence EQ */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetIntelligenceEQ(DAP_PARAM_INTELLIGENT_EQUALIZER *pstIntelligenceEQ, int index);													/* Get current DAP Intelligence EQ */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetMediaIntelligence(DAP_PARAM_MEDIA_INTELLIGENCE stMediaIntelligence, int index);										/* Set DAP Media Intelligence */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetMediaIntelligence(DAP_PARAM_MEDIA_INTELLIGENCE *pstMediaIntelligence, int index);										/* Get current DAP Media Intelligence */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetGraphicalEQ(DAP_PARAM_GRAPHICAL_EQUALIZER stGraphicalEQ, int index);															/* Set DAP Graphical Equalizer */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetGraphicalEQ(DAP_PARAM_GRAPHICAL_EQUALIZER *pstGraphicalEQ, int index);															/* Get current DAP Graphical Equalizer status */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetPerceptualHeightFilter(lgse_dap_perceptual_height_filter_mode_ext_type_t stPerceptualHeightFilter, int index);						/* Set DAP Perceptual Height Filter */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetPerceptualHeightFilter(lgse_dap_perceptual_height_filter_mode_ext_type_t *pstPerceptualHeightFilter, int index);						/* Get current DAP Perceptual Height Filter status */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetBassEnhancer(DAP_PARAM_BASS_ENHANCER stBassEnhancer, int index);														/* Set DAP Bass Enhancer */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetBassEnhancer(DAP_PARAM_BASS_ENHANCER *pstBassEnhancer, int index);														/* Get current DAP Bass Enhancer status */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetBassExtraction(HAL_AUDIO_LGSE_DAP_BASS_EXTRACTION_T stBassExtraction, int index);													/* Set DAP Bass Extraction */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetBassExtraction(HAL_AUDIO_LGSE_DAP_BASS_EXTRACTION_T *pstBassExtraction, int index);													/* Get current DAP Bass Extraction status */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetVirtualBass(DAP_PARAM_VIRTUAL_BASS stVirtualBass, int index);															/* Set DAP Surround Virtual Bass */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetVirtualBass(DAP_PARAM_VIRTUAL_BASS *pstVirtualBass, int index);															/* Get current DAP Surround Virtual Bass Status */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetRegulator(DAP_PARAM_AUDIO_REGULATOR stRegulator, int index);																	/* Set DAP Regulator */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetRegulator(DAP_PARAM_AUDIO_REGULATOR *pstRegulator, int index);																	/* Get current DAP Regulator stauts */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetSysParam(HAL_AUDIO_LGSE_DAP_SYS_PARAM_T stDapSysParam, int index);																/* Set DAP System Param */
DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetSysParam(HAL_AUDIO_LGSE_DAP_SYS_PARAM_T *pstDapSysParam, int index);																/* Get current DAP System Param stauts */

/**
 * HAL AUDIO DTS Virtual:X (VX) API define
 *
 * 20170531 kwangshik.kim
 */
DTV_STATUS_T HAL_AUDIO_LGSE_VX_EnableVx(BOOLEAN bEnable, int index);                               /* Enable or disable Virtual:X Lib1 processing */
DTV_STATUS_T HAL_AUDIO_LGSE_VX_SetVxStatus(BOOLEAN bEnable, int index);                           /* Get status if Virtual:X is enabled or not  */
DTV_STATUS_T HAL_AUDIO_LGSE_VX_GetVxStatus(BOOLEAN *bEnable, int index);                           /* Get status if Virtual:X is enabled or not  */
DTV_STATUS_T HAL_AUDIO_LGSE_VX_SetProcOutputGain(SINT32 procOutGain, int index);                   /*  Processing output gain */
DTV_STATUS_T HAL_AUDIO_LGSE_VX_GetProcOutputGain(SINT32 *pProcOutGain, int index);                 /* Get Processing output gain */
DTV_STATUS_T HAL_AUDIO_LGSE_VX_EnableTsx(BOOLEAN bEnable, int index);                              /* Enable or disable TruSurround:X processing */
DTV_STATUS_T HAL_AUDIO_LGSE_VX_GetTsxStatus(BOOLEAN *pEnable, int index);                          /* Get status if TruSurround:X processing is enabled or not */
DTV_STATUS_T HAL_AUDIO_LGSE_VX_SetTsxPassiveMatrixUpmix(BOOLEAN bEnable, int index);               /* Enable or disable TruSurround:X Passive Matrix upmix for stereo input signals. */
DTV_STATUS_T HAL_AUDIO_LGSE_VX_GetTsxPassiveMatrixUpmix(BOOLEAN *pEnable, int index);              /* Get status if TruSurround:X Passive Matrix upmix is enabled or not */
DTV_STATUS_T HAL_AUDIO_LGSE_VX_SetTsxHeightUpmix(BOOLEAN bEnable, int index);                      /* Enable or disable TruSurround:X Height upmix for non-immersive channel layout (= inputs without discrete height channels) */
DTV_STATUS_T HAL_AUDIO_LGSE_VX_GetTsxHeightUpmix(BOOLEAN *pEnable, int index);                     /* Get status if TruSurround:X Height upmix is enabled or not */
DTV_STATUS_T HAL_AUDIO_LGSE_VX_SetTsxLPRGain(SINT32 lprGain, int index);                           /* Controls phantom center mix level to the center channel */
DTV_STATUS_T HAL_AUDIO_LGSE_VX_GetTsxLPRGain(SINT32 *pLprGain, int index);                         /* Get phantom center mix level to the center channel */
DTV_STATUS_T HAL_AUDIO_LGSE_VX_SetTsxLPRGainAsInputChannel(HAL_AUDIO_LGSE_VX_INPUT_CHANNEL_T inputChannel, SINT32 lprGain, int index);                           /* Controls phantom center mix level to the center channel for selected input channel */
DTV_STATUS_T HAL_AUDIO_LGSE_VX_GetTsxLPRGainAsInputChannel(HAL_AUDIO_LGSE_VX_INPUT_CHANNEL_T inputChannel, SINT32 *pLprGain, int index);                         /* Get phantom center mix level to the center channel for selected input channel */
DTV_STATUS_T HAL_AUDIO_LGSE_VX_SetTsxHorizVirEff(BOOLEAN bEnable, int index);                      /* Controls virtualization effect strengths for horizontal plane sources */
DTV_STATUS_T HAL_AUDIO_LGSE_VX_GetTsxHorizVirEff(BOOLEAN *pEnable, int index);                     /* Get virtualization effect strengths for horizontal plane sources */
DTV_STATUS_T HAL_AUDIO_LGSE_VX_SetTsxHorizVirEffAsInputChannel(HAL_AUDIO_LGSE_VX_INPUT_CHANNEL_T inputChannel, BOOLEAN bEnable, int index);                      /* Controls virtualization effect strengths for horizontal plane sources for selected input channel */
DTV_STATUS_T HAL_AUDIO_LGSE_VX_GetTsxHorizVirEffAsInputChannel(HAL_AUDIO_LGSE_VX_INPUT_CHANNEL_T inputChannel, BOOLEAN *pEnable, int index);                     /* Get virtualization effect strengths for horizontal plane sources for selected input channel */
DTV_STATUS_T HAL_AUDIO_LGSE_VX_SetTsxHeightMixCoeff(SINT32 heightCoeff, int index);                /* Controls the level of height signal in downmix to horizontal plane channels */
DTV_STATUS_T HAL_AUDIO_LGSE_VX_GetTsxHeightMixCoeff(SINT32 *pHeightCoeff, int index);              /* Get the level of height signal in downmix to horizontal plane channels */
DTV_STATUS_T HAL_AUDIO_LGSE_VX_SetTsxHeightMixCoeffAsInputChannel(HAL_AUDIO_LGSE_VX_INPUT_CHANNEL_T inputChannel, SINT32 heightCoeff, int index);                /* Controls the level of height signal in downmix to horizontal plane channels for selected input channel */
DTV_STATUS_T HAL_AUDIO_LGSE_VX_GetTsxHeightMixCoeffAsInputChannel(HAL_AUDIO_LGSE_VX_INPUT_CHANNEL_T inputChannel, SINT32 *pHeightCoeff, int index);              /* Get the level of height signal in downmix to horizontal plane channels for selected input channel */
DTV_STATUS_T HAL_AUDIO_LGSE_VX_SetCenterGain(SINT32 centerGain, int index);                        /* Control center singal gain */
DTV_STATUS_T HAL_AUDIO_LGSE_VX_GetCenterGain(SINT32 *pCenterGain, int index);                      /* Get center signal gain */
DTV_STATUS_T HAL_AUDIO_LGSE_VX_SetCertParam(BOOLEAN bEnable, int index);                           /* Set Parameters for DTS VX certification */

/* AAD */
DTV_STATUS_T HAL_AUDIO_SIF_SetInputSource(HAL_AUDIO_SIF_INPUT_T sifSource);
DTV_STATUS_T HAL_AUDIO_SIF_SetHighDevMode(BOOLEAN bOnOff);
DTV_STATUS_T HAL_AUDIO_SIF_SetBandSetup(HAL_AUDIO_SIF_TYPE_T eSifType, HAL_AUDIO_SIF_SOUNDSYSTEM_T sifBand);
DTV_STATUS_T HAL_AUDIO_SIF_SetModeSetup(HAL_AUDIO_SIF_STANDARD_T sifStandard);
DTV_STATUS_T HAL_AUDIO_SIF_SetUserAnalogMode(HAL_AUDIO_SIF_MODE_SET_T sifAudioMode);
DTV_STATUS_T HAL_AUDIO_SIF_SetA2ThresholdLevel(UINT16 thrLevel);
DTV_STATUS_T HAL_AUDIO_SIF_SetNicamThresholdLevel(UINT16 thrLevel);
DTV_STATUS_T HAL_AUDIO_SIF_GetBandDetect(HAL_AUDIO_SIF_SOUNDSYSTEM_T soundSystem, UINT32 *pBandStrength);
DTV_STATUS_T HAL_AUDIO_SIF_DetectSoundSystem(HAL_AUDIO_SIF_SOUNDSYSTEM_T setSoundSystem, BOOLEAN bManualMode, HAL_AUDIO_SIF_SOUNDSYSTEM_T *pDetectSoundSystem, UINT32 *pSignalQuality);
DTV_STATUS_T HAL_AUDIO_SIF_CheckNicamDigital(HAL_AUDIO_SIF_EXISTENCE_INFO_T *pIsNicamDetect);
DTV_STATUS_T HAL_AUDIO_SIF_CheckAvailableSystem(HAL_AUDIO_SIF_AVAILE_STANDARD_T standard, HAL_AUDIO_SIF_EXISTENCE_INFO_T *pAvailability);
DTV_STATUS_T HAL_AUDIO_SIF_CheckA2DK(HAL_AUDIO_SIF_STANDARD_T standard, HAL_AUDIO_SIF_EXISTENCE_INFO_T *pAvailability);
DTV_STATUS_T HAL_AUDIO_SIF_GetA2StereoLevel(UINT16 *pLevel);
DTV_STATUS_T HAL_AUDIO_SIF_GetNicamThresholdLevel(UINT16 *pLevel);
DTV_STATUS_T HAL_AUDIO_SIF_GetCurAnalogMode(HAL_AUDIO_SIF_MODE_GET_T *pSifAudioMode);
BOOLEAN HAL_AUDIO_SIF_IsSIFExist(void);
DTV_STATUS_T HAL_AUDIO_SIF_SetAudioEQMode(BOOLEAN bOnOff);

/* AENC */
DTV_STATUS_T HAL_AUDIO_AENC_Start(HAL_AUDIO_AENC_INDEX_T aencIndex, HAL_AUDIO_AENC_ENCODING_FORMAT_T audioType);
DTV_STATUS_T HAL_AUDIO_AENC_Stop(HAL_AUDIO_AENC_INDEX_T aencIndex);
DTV_STATUS_T HAL_AUDIO_AENC_RegCallback(HAL_AUDIO_AENC_INDEX_T aencIndex, pfnAENCDataHandling pfnCallBack);
DTV_STATUS_T HAL_AUDIO_AENC_SetCodec(HAL_AUDIO_AENC_INDEX_T aencIndex, HAL_AUDIO_AENC_ENCODING_FORMAT_T codec);
DTV_STATUS_T HAL_AUDIO_AENC_SetVolume(HAL_AUDIO_AENC_INDEX_T aencIndex, HAL_AUDIO_VOLUME_T volume, UINT32 gain);
DTV_STATUS_T HAL_AUDIO_AENC_ReleaseData(HAL_AUDIO_AENC_INDEX_T aencIndex, UINT8 *pBufAddr, UINT32 datasize);

/* PCM(Sound Bar Buletooth, PCM Capture) */
DTV_STATUS_T HAL_AUDIO_PCM_StartUpload(HAL_AUDIO_PCM_INPUT_T apcmIndex);
DTV_STATUS_T HAL_AUDIO_PCM_StopUpload(HAL_AUDIO_PCM_INPUT_T apcmIndex);
DTV_STATUS_T HAL_AUDIO_PCM_RegSendPCMCallback(HAL_AUDIO_PCM_INPUT_T apcmIndex, pfnPCMSending pfnCallBack);
DTV_STATUS_T HAL_AUDIO_PCM_RegUploadCallBack(HAL_AUDIO_PCM_INPUT_T apcmIndex, pfnPCMDataHandling pfnCallBack);
DTV_STATUS_T HAL_AUDIO_PCM_SetVolume(HAL_AUDIO_PCM_INPUT_T apcmIndex, HAL_AUDIO_VOLUME_T volume);
DTV_STATUS_T HAL_AUDIO_PCM_SetDataCount(HAL_AUDIO_PCM_INPUT_T apcmIndex, UINT8 count);	//for bluetooth, 1 count takes 5.3ms.
DTV_STATUS_T HAL_AUDIO_PCM_SetInfo(HAL_AUDIO_PCM_INPUT_T apcmIndex, HAL_AUDIO_PCM_INFO_T info);
DTV_STATUS_T HAL_AUDIO_PCM_GetInfo(HAL_AUDIO_PCM_INPUT_T apcmIndex, HAL_AUDIO_PCM_INFO_T *pInfo);

/* Debug */
void HAL_AUDIO_DebugMenu(void);
void show_resource_st(HAL_AUDIO_RESOURCE_T res);

/* In below function WebOS3.0 not use function, so will be deleted or some function can use. */
DTV_STATUS_T HAL_AUDIO_FinalizeModule(void);      /* function body is empty */
DTV_STATUS_T HAL_AUDIO_AENC_Initialize(void); // function body is empty
DTV_STATUS_T HAL_AUDIO_AENC_Destroy(void); // function body is empty

/**
JAPAN 4K ADEC
1.API_ADEC_SetEsPushMode will be called before adec start
  Default setting should be FALSE.
  If App call this function with esMode = 1, It means app will push ES data to ADEC.
2.After App call ADEC Stop, ES Push mode should change to FALSE.
@param adecIndex [IN] specifies an audio decoder index on which an audio decoder
@param esMode [IN] specifies an ES Push Mode
**/
DTV_STATUS_T HAL_AUDIO_SetEsPushMode(HAL_AUDIO_ADEC_INDEX_T adecIndex, HAL_AUDIO_ES_MODE_T esMode, UINT32 BSHeader);
DTV_STATUS_T HAL_AUDIO_SetEsPushWptr(HAL_AUDIO_ADEC_INDEX_T adecIndex, UINT32 wptr);
DTV_STATUS_T HAL_AUDIO_SetEsPush_PCMFormat(HAL_AUDIO_ADEC_INDEX_T adecindex, int fs, int nch, int bitPerSample);

/**
JAPAN 4K ADEC
@param adecIndex [IN] specifies an audio decoder index on which an audio decoder
@param pESDATA [IN] ptr in system memory containing ES data
@param dataSize [IN] byte size of ES data
@param pts [IN] pts value in system memory of UINT64 PTS Value for JP 4K
**/
DTV_STATUS_T HAL_AUDIO_PushEsData(HAL_AUDIO_ADEC_INDEX_T adecIndex, UINT8 *pESDATA, UINT32 dataSize, UINT64 pts);

/**
Set OTT Mode
Netflix wants OTT mode and ATMOS Locking.  Refer to IIDK MS12 1.3.2(2.2.1) or later.
ATMOS Locking can work after OTT mode is enabled.
Default is bIsOTTEnable=FALSE and bIsATMOSLockingEnable=FALSE.
If LGE MW call with bIsOTTEnable=FALSE and bIsATMOSLockingEnable=TRUE, the API should  returen FALSE without any action.
This API can called in duplicate and BSP set and remain state as bIsOTTEnable and bIsATMOSLockingEnable.
This API must support from Y19 Model (webOS4.5).
This API should be supported on also Y18 model(webOS4.0) supporing TV ATMOS but ATMOS Locking will be not used for Y18 model.
@param bIsOTTEnable [IN] enable/disable OTT Mode
@param bIsATMOSLockingEnable [IN] enable/disable ATMOS Locking
**/
DTV_STATUS HAL_AUDIO_SetDolbyOTTMode(BOOLEAN bIsOTTEnable, BOOLEAN bIsATMOSLockingEnable);

/* HAL decoder for GST path */
UINT32 HAL_AUDIO_GetDecoderInstanceID(HAL_AUDIO_ADEC_INDEX_T adecIndex, int proceddID, long *instanceID);
UINT32 HAL_AUDIO_ReleaseDecoderInstanceID(HAL_AUDIO_ADEC_INDEX_T adecIndex);
UINT32 HAL_AUDIO_ReleaseDecoderInstanceByPID(int proceddID);
UINT32 HAL_AUDIO_GetDecoderOutBuffer(HAL_AUDIO_ADEC_INDEX_T adecIndex, UINT32 pin_index, UINT32 *bufferHeaderPhyAddr, UINT32 *bufferPhyAddr, UINT32 *size);
UINT32 HAL_AUDIO_GetDecoderIcqBuffer(HAL_AUDIO_ADEC_INDEX_T adecIndex, UINT32 *bufferHeaderPhyAddr, UINT32 *bufferPhyAddr, UINT32 *size);
UINT32 HAL_AUDIO_GetDecoderDsqBuffer(HAL_AUDIO_ADEC_INDEX_T adecIndex, UINT32 *bufferHeaderPhyAddr, UINT32 *bufferPhyAddr, UINT32 *size);

/* khal internal usage */
void init_lgse_gloal_var(void);
void Switch_OnOff_MuteProtect(int duration);

/* lgtv-driver status */
int32_t get_adec_status(HAL_AUDIO_ADEC_INDEX_T adecIndex, char* str);
int32_t get_sndout_status(char* str);
int32_t get_lgse_status(char* str);
int32_t get_amixer_status(char* str);
int32_t get_hdmi_status(char* str);
int32_t get_atv_status(char* str);
int32_t get_aenc_status(char* str);
int32_t get_ProcessingDelay(HAL_AUDIO_INDEX_T main_adecIndex, common_output_ext_type_t device);
#endif /* _HAL_AUDIO_H_ */
