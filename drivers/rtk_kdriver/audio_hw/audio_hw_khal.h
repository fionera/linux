#ifndef AUDIO_HW_KHAL_H
#define AUDIO_HW_KHAL_H

#include <linux/types.h>
#include <sound/core.h>
#include <uapi/sound/asound.h>
#include <linux/dma-mapping.h>
#include <../linux/alsa-ext/alsa-ext-atv.h>
#include "audio_hw_atv.h"

#define FUNCTION_EN

typedef enum
{
	RHAL_AUDIO_SIF_INPUT_EXTERNAL	= 0,
	RHAL_AUDIO_SIF_INPUT_INTERNAL	= 1,
} RHAL_AUDIO_SIF_INPUT_T;

typedef struct RHAL_AUDIO_SIF_INFO
{
	RHAL_AUDIO_SIF_INPUT_T			sifSource;			/* Currnet SIF Source Input Status */
	sif_country_ext_type_t				curSifType;			/* Currnet SIF Type Status */
	uint32_t                            			bHighDevOnOff;		/* Currnet High DEV ON/OFF Status */
	sif_soundsystem_ext_type_t			curSifBand;			/* Currnet SIF Sound Band(Sound System) */
	ATV_SOUND_STD					curSifStandard;		/* Currnet SIF Sound Standard */
	sif_existence_info_ext_type_t		curSifIsA2;			/* Currnet SIF A2 Exist Status */
	sif_mode_user_ext_type_t			curSifModeSet;		/* Currnet SIF Sound Mode Set Status */
	sif_mode_ext_type_t				curSifModeGet;		/* Currnet SIF Sound Mode Get Status */
	sif_existence_info_ext_type_t		curSifExist;
} RHAL_AUDIO_SIF_INFO_T;

int32_t KHAL_SIF_CONNECT(void);
int32_t KHAL_SIF_DISCONNECT(void);
int32_t KHAL_SIF_DetectSoundSystem(sif_soundsystem_ext_type_t SoundSystem, 
	sif_soundsystem_ext_type_t *GetSoundSystem, uint32_t *SignalQuality);
int32_t KHAL_SIF_SoundSystemStrength(sif_soundsystem_ext_type_t SoundSystem, 
	sif_soundsystem_ext_type_t *GetSoundSystem, uint32_t *SignalQuality);
int32_t KHAL_SIF_BandSetup(sif_country_ext_type_t SifCountry, sif_soundsystem_ext_type_t SoundSystem, 
	sif_country_ext_type_t *GetSifCountry, sif_soundsystem_ext_type_t *GetSoundSystem);
int32_t KHAL_SIF_StandardSetup(sif_standard_ext_type_t SifStandard, sif_standard_ext_type_t *GetSifStandard);
int32_t KHAL_SIF_CurAnalogMode(sif_mode_ext_type_t *GetAnalogMode);
int32_t KHAL_SIF_UserAnalogMode(sif_mode_user_ext_type_t UserAnalogMode, sif_mode_user_ext_type_t *GetUserAnalogMode);
int32_t KHAL_SIF_SifExist(sif_existence_info_ext_type_t *GetSifExist);
int32_t KHAL_SIF_HDev(uint32_t OnOff, uint32_t *GetOnOff);
int32_t KHAL_SIF_A2ThresholdLevel(uint32_t Threshold, uint32_t *GetThreshold);

int32_t AUDIO_SIF_CHECK_INIT(void);
int32_t AUDIO_SIF_STOP_DETECT(bool en);
int32_t AUDIO_SIF_RESET_DATA(void);
int32_t AUDIO_SIF_INIT(void);
void Audio_SIF_SetAudioInHandle(long instanceID);
void Audio_SIF_SetSubAudioInHandle(long instanceID);
void Audio_SIF_Get_BandCountry(sif_country_ext_type_t *Country);
void Audio_SIF_Get_BandSoundSystem(sif_soundsystem_ext_type_t *SoundSystem);
void Audio_SIF_Get_Standard(sif_standard_ext_type_t *Standard);
void Audio_SIF_Get_CurAnalogMode(sif_mode_ext_type_t *CurAnalogMode);
void Audio_SIF_Get_UserAnalogMode(sif_mode_user_ext_type_t *UserAnalogMode);
void Audio_SIF_Get_SifExiste(sif_existence_info_ext_type_t *SifExist);
void Audio_SIF_Get_HDev(uint32_t *HDev);
void Audio_SIF_RegMuteCallback(int decindex, pfnMuteHandling pfnCallBack);
#endif
