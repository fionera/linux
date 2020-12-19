#include <linux/kconfig.h>
#include <linux/compiler.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/stat.h>			/* permission */
#include <linux/fs.h>			/* fs */
#include <linux/errno.h>		/* error codes */
#include <linux/types.h>		/* size_t */
#include <linux/fcntl.h>		/* O_ACCMODE */
#include <linux/uaccess.h>		/* copy_*_user */
#include <linux/semaphore.h>		/* semaphore */
#include <linux/kthread.h>		/* kernel thread */
#include <linux/freezer.h>		/* set freezable */
#include <linux/wait.h>			/* wait event */
#include <linux/jiffies.h>		/* jiffies */
#include <linux/delay.h>

/*
 * rbus registers
 */
#include <rtk_kdriver/RPCDriver.h>

#include "audio_hw_port.h"
#include "audio_hw_driver.h"
#include "audio_hw_atv.h"
#include "audio_hw_aio.h"
#include "audio_hw_app.h"
#include "audio_hw_rpc.h"
#include "audio_hw_khal.h"

#define __ALOG_SUBTAG "khal"
#define SIF_STD_THRESHOLD 0x1500

#ifdef ALSA_BASE_EN
static RHAL_AUDIO_SIF_INFO_T g_AudioSIFInfo = {
	RHAL_AUDIO_SIF_INPUT_EXTERNAL,			// sifSource
	SIF_TYPE_NONE,							// curSifType
	0,									// bHighDevOnOff
	SIF_SYSTEM_UNKNOWN,					// curSifBand
	SIF_NUM_SOUND_STD,						// curSifStandard
	SIF_DETECTION_EXSISTANCE,				// curSifIsA2
	SIF_USER_PAL_UNKNOWN,					// curSifModeSet
	SIF_PAL_UNKNOWN,						// curSifModeGet
	SIF_DETECTION_EXSISTANCE				// curSifExist
	};

extern int Aud_initial;
static unsigned int g_ATV_CONNECT = 0;
static unsigned int g_ATV_DETECT = 0;
static int g_AudioHw_init = 0;
static uint32_t g_HDev_notfinish = 0;	 // 1: need to set, 0: no need to set
static uint32_t g_HDev_ONOFF = 0;	 // 

static void Audio_SoundStd_To_Hal(sif_standard_ext_type_t *HalSound, ATV_SOUND_STD DriverSound);
static void Audio_Set_SoundStd(ATV_SOUND_STD sound_std);
static uint32_t AtvIsSifMode(sif_mode_user_ext_type_t sif_mode);
static uint32_t AtvIsSifCountry(sif_country_ext_type_t sif_country);
static uint32_t AtvIsSoundSystem(sif_soundsystem_ext_type_t std);
#endif

int32_t KHAL_SIF_CONNECT(void)
{
	#ifdef ALSA_BASE_EN
	alog_info("%s, %d", __FUNCTION__, __LINE__);
		#ifdef FUNCTION_EN
		Audio_AtvEnterAtvSource();
		#endif
	g_AudioSIFInfo.bHighDevOnOff  = FALSE;
	g_AudioSIFInfo.curSifBand = SIF_SYSTEM_UNKNOWN;
	g_AudioSIFInfo.curSifStandard = SIF_NUM_SOUND_STD;
	g_AudioSIFInfo.curSifIsA2 = SIF_DETECTION_EXSISTANCE;
	g_AudioSIFInfo.curSifModeSet = SIF_PAL_UNKNOWN;
	g_AudioSIFInfo.curSifModeGet = SIF_PAL_UNKNOWN;
	g_AudioSIFInfo.sifSource = RHAL_AUDIO_SIF_INPUT_EXTERNAL;
	g_ATV_CONNECT = 1;
	return AIO_OK;
	#else
	return AIO_ERROR;
	#endif
}

int32_t KHAL_SIF_DISCONNECT(void) //need to modify by ryanlan
{
	#ifdef ALSA_BASE_EN
		#ifdef FUNCTION_EN
		AUDIO_SIF_STOP_DETECT(true);
		if (AUDIO_SIF_CHECK_INIT() != true)
		{
			AUDIO_SIF_STOP_DETECT(false);
			return AIO_ERROR;
		}

		AUDIO_SIF_RESET_DATA();
		AUDIO_SIF_STOP_DETECT(false);
		#endif
	return AIO_OK;
	#else
	return AIO_ERROR;
	#endif
}



int32_t KHAL_SIF_DetectSoundSystem(sif_soundsystem_ext_type_t SoundSystem, 
	sif_soundsystem_ext_type_t *GetSoundSystem, uint32_t *SignalQuality)
{
	#ifdef ALSA_BASE_EN
	ATV_SOUND_STD_MAIN_SYSTEM atv_sound_std_main_system = ATV_SOUND_UNKNOWN_SYSTEM;
	#ifdef FUNCTION_EN
	ATV_SOUND_STD_MAIN_SYSTEM HWDetectSoundSystem = ATV_SOUND_UNKNOWN_SYSTEM;
	ATV_SOUND_STD HWDetectSoundStd = ATV_SOUND_STD_UNKNOWN;
	int32_t pToneSNR = 0;
	#endif
	SIF_INPUT_SOURCE  rtk_sif_input_source = SIF_FROM_IFDEMOD;
	uint32_t Next_thresh=SIF_STD_THRESHOLD;
	sif_soundsystem_ext_type_t Next_Std=SIF_SYSTEM_UNKNOWN;

	alog_info("%s, SoundSystem=%x, GetSoundSystem=%x\n", __FUNCTION__, SoundSystem,*GetSoundSystem);

	if (g_ATV_CONNECT == 0)
		return AIO_ERROR;

	//if (AtvIsSoundSystem(SoundSystem)==0)
	//	return AIO_ERROR;
	if(SoundSystem<SIF_SYSTEM_BG || SoundSystem> (SIF_SYSTEM_MN | SIF_SYSTEM_BG |SIF_SYSTEM_I |SIF_SYSTEM_DK |SIF_SYSTEM_L))
			return AIO_ERROR;
	
	if (GetSoundSystem == NULL)
		return AIO_ERROR;

	if (SignalQuality == NULL)
		return AIO_ERROR;

	//*GetSoundSystem = SoundSystem;
	*SignalQuality = 0;
	if(AUDIO_SIF_CHECK_INIT() != TRUE)
		return AIO_ERROR;

	//set band internal/external
	switch(g_AudioSIFInfo.sifSource)
	{
		case RHAL_AUDIO_SIF_INPUT_EXTERNAL:
		{
			rtk_sif_input_source = SIF_FROM_SIF_ADC;
			break;
		}
		case RHAL_AUDIO_SIF_INPUT_INTERNAL:
		{
			rtk_sif_input_source = SIF_FROM_IFDEMOD;
			break;
		}
		default:
		{
			alog_err("[%s] sif input:%d\n", __FUNCTION__, g_AudioSIFInfo.sifSource);
			return AIO_ERROR;
		}
	}

	//detect sound system
	switch(SoundSystem)
	{
		case  SIF_SYSTEM_BG:
		{
			atv_sound_std_main_system = ATV_SOUND_BG_SYSTEM;
			break;
		}
		case  SIF_SYSTEM_I:
		{
			atv_sound_std_main_system = ATV_SOUND_I_SYSTEM;
			break;
		}
		case  SIF_SYSTEM_DK:
		{
			atv_sound_std_main_system = ATV_SOUND_DK_SYSTEM;
			break;
		}
		case  SIF_SYSTEM_L:
		{
			atv_sound_std_main_system = ATV_SOUND_L_SYSTEM;
			break;
		}
		case  SIF_SYSTEM_MN:
		{
			atv_sound_std_main_system = ATV_SOUND_MN_SYSTEM;
			break;
		}
		default:
		{
			atv_sound_std_main_system = ATV_SOUND_AUTO_SYSTEM;
			break;
		}
		#if 0
		default:
		{
			alog_err("[%s] not in case1:%d\n",__FUNCTION__, SoundSystem);
			return AIO_ERROR;
		}
		#endif
	}
	
		#ifdef FUNCTION_EN
		Audio_HwpSetAtvAudioBand(rtk_sif_input_source, atv_sound_std_main_system);
		Audio_HwpSIFGetMainToneSNR(atv_sound_std_main_system, &HWDetectSoundSystem, &HWDetectSoundStd, &pToneSNR);
		switch(HWDetectSoundSystem) //auto mode reuten HWDetectSoundSystem
		{
			case  ATV_SOUND_BG_SYSTEM:
			{
				*GetSoundSystem = SIF_SYSTEM_BG;
				break;
			}
			case  ATV_SOUND_I_SYSTEM:
			{
				*GetSoundSystem = SIF_SYSTEM_I;
				break;
			}
			case  ATV_SOUND_DK_SYSTEM:
			{
				*GetSoundSystem = SIF_SYSTEM_DK;
				break;
			}
			case  ATV_SOUND_L_SYSTEM:
			{
				*GetSoundSystem = SIF_SYSTEM_L;
				break;
			}
			case  ATV_SOUND_MN_SYSTEM:
			{
				*GetSoundSystem = SIF_SYSTEM_MN;
				break;
			}
			case  ATV_SOUND_UNKNOWN_SYSTEM:
			{
				*GetSoundSystem = SIF_SYSTEM_UNKNOWN;
				break;
			}
			default:
			{
				alog_err("[%s] not in case2:%d\n",__FUNCTION__,HWDetectSoundSystem);
				return AIO_ERROR;
			}
		}

		*SignalQuality = pToneSNR;
		g_ATV_DETECT = 1;
		alog_info("%s, GetSoundSystem=%x\n", __FUNCTION__, *GetSoundSystem);


//1213
//for soCTS
		if((SoundSystem & *GetSoundSystem )==SIF_SYSTEM_UNKNOWN)
		{

			if ((SoundSystem & SIF_SYSTEM_BG) !=0)
			{
				Audio_HwpSIFGetMainToneMag(ATV_SOUND_BG_SYSTEM,&pToneSNR);			
				if(pToneSNR>Next_thresh)
				{
					Next_Std=SIF_SYSTEM_BG;
					Next_thresh=pToneSNR;
				}
			}
			if((SoundSystem & SIF_SYSTEM_I) !=0)
			{
				Audio_HwpSIFGetMainToneMag(ATV_SOUND_I_SYSTEM,&pToneSNR);				
				if(pToneSNR>Next_thresh)
				{
					Next_Std=SIF_SYSTEM_I;
					Next_thresh=pToneSNR;
				}			
			}
			//DK main tone is the same with L
			if((SoundSystem & SIF_SYSTEM_DK) !=0)
			{
				Audio_HwpSIFGetMainToneMag(ATV_SOUND_DK_SYSTEM,&pToneSNR);				
				if(pToneSNR>Next_thresh)
				{
					Next_Std=SIF_SYSTEM_DK;
					Next_thresh=pToneSNR;
				}			
			}else if((SoundSystem & SIF_SYSTEM_L) !=0)
			{
				
				Audio_HwpSIFGetMainToneMag(ATV_SOUND_L_SYSTEM,&pToneSNR);				
				if(pToneSNR>Next_thresh)
				{
					Next_Std=SIF_SYSTEM_L;
					Next_thresh=pToneSNR;
				}			
			}			
			if((SoundSystem & SIF_SYSTEM_MN) !=0)
			{
				
				Audio_HwpSIFGetMainToneMag(ATV_SOUND_MN_SYSTEM,&pToneSNR);				
				if(pToneSNR>Next_thresh)
				{
					Next_Std=SIF_SYSTEM_MN;
					Next_thresh=pToneSNR;
				}			
			}
			*GetSoundSystem=Next_Std;
			if(*GetSoundSystem==SIF_SYSTEM_UNKNOWN)
				*SignalQuality =0;				
			else
				*SignalQuality =Next_thresh;
			
		}		
		
		alog_info("%s, Return soundsystem=%x, SignalQuality=%x", __FUNCTION__, *GetSoundSystem, *SignalQuality);
		#else
		*SignalQuality = 0x1500;
		*GetSoundSystem = SoundSystem;
		#endif
	return AIO_OK;
	#else
	return AIO_ERROR;
	#endif
}

int32_t KHAL_SIF_SoundSystemStrength(sif_soundsystem_ext_type_t SoundSystem, 
	sif_soundsystem_ext_type_t *GetSoundSystem, uint32_t *SignalQuality)
{
	int32_t status;
	status = KHAL_SIF_DetectSoundSystem(SoundSystem, GetSoundSystem,SignalQuality);

	if(*GetSoundSystem==SIF_SYSTEM_UNKNOWN)
		*GetSoundSystem=SoundSystem;

	return status;
		
}

int32_t KHAL_SIF_BandSetup(sif_country_ext_type_t SifCountry, sif_soundsystem_ext_type_t SoundSystem, 
	sif_country_ext_type_t *GetSifCountry, sif_soundsystem_ext_type_t *GetSoundSystem)
{
	#ifdef ALSA_BASE_EN
	ATV_SOUND_STD_MAIN_SYSTEM atv_sound_std_main_system = ATV_SOUND_UNKNOWN_SYSTEM;
	ATV_SOUND_STD SoundStd = ATV_SOUND_STD_UNKNOWN;
	uint32_t getHDev_ONOFF = 0;	 //
	Audio_AtvEnableAutoChangeStdFlag(0); // Clayton: add disable auto std change when Bandsetup

	if (GetSifCountry == NULL || GetSoundSystem == NULL)
		return AIO_ERROR;
//	
	if(AtvIsSifCountry(SifCountry)==0)
		return AIO_ERROR;
	if (AtvIsSoundSystem(SoundSystem)==0)
		return AIO_ERROR;		

//	*GetSifCountry = SIF_TYPE_NONE;
//	*GetSoundSystem = SIF_SYSTEM_UNKNOWN;
	if (g_user_config != AUDIO_USER_CONFIG_BRING_UP)
	{
		if(AUDIO_SIF_CHECK_INIT() != TRUE)
		{
			alog_err("\n KHAL_SIF_BandSetup, init ng\n");
			return AIO_ERROR;
		}
	}

	if (SifCountry < SIF_ATSC_SELECT || SifCountry > SIF_DVB_AJJA_SELECT)
	{
		alog_err("[%s] error SifCountry:0x%d\n", __FUNCTION__, SifCountry);
		return AIO_ERROR;
	}
	
	alog_info("[%s] SoundSystem=0x%x, SifCountry=0x%x\n", __FUNCTION__, SoundSystem, SifCountry);
	g_AudioSIFInfo.curSifType = SifCountry;
	#ifndef FUNCTION_EN
	if (g_ATV_CONNECT == 0)
	{
		alog_err("\n g_ATV_connect ng\n");
		return AIO_ERROR;
	}

	*GetSifCountry = SifCountry;
	*GetSoundSystem = SoundSystem;
	#if 0
	alog_err("\n%s, %d, %x, %x\n", __FUNCTION__, __LINE__, SifCountry, SoundSystem);
	if(SoundSystem < SIF_SYSTEM_BG || SoundSystem > SIF_SYSTEM_MN || SifCountry < SIF_ATSC_SELECT || SifCountry > SIF_DVB_AJJA_SELECT)
	{
		alog_err("[%s] error sifBand:0x%d\n", __FUNCTION__, SoundSystem);
		return AIO_ERROR;
	}
	else
	{
		//return AIO_OK;
	}
	#endif
	#endif

	if (g_ATV_CONNECT == 0)
	{
		//*GetSoundSystem = SIF_SYSTEM_UNKNOWN;
		alog_err("\n g_ATV_connect ng\n");
		return AIO_ERROR;
	}

	//alog_info("mute start\n");
	AtvSetMute(ATV_ENABLE); //nio_liao 20191015, avoid channel switch pop noise


	if(SoundSystem == SIF_SYSTEM_L)
	{
		alog_info("[%s] Set priority as L-system, SoundSystem=0x%x, SifCountry=0x%x\n", __FUNCTION__, SoundSystem, SifCountry);
		Audio_AtvSetMtsPriority(ATV_MTS_PRIO_L);
	}
	else if(SoundSystem == SIF_SYSTEM_DK)
	{
		alog_info("[%s] Set priority as DK-system, SoundSystem=0x%x, SifCountry=0x%x\n", __FUNCTION__, SoundSystem, SifCountry);
		Audio_AtvSetMtsPriority(ATV_MTS_PRIO_DK);
	}
	// Clayton 2019/8/19: It already reset on KHAL_SIF_Connect
	//g_AudioSIFInfo.curSifBand = SIF_SYSTEM_UNKNOWN;
	//g_AudioSIFInfo.curSifStandard = ATV_SOUND_STD_UNKNOWN;
	//g_AudioSIFInfo.curSifIsA2 = SIF_DETECTION_EXSISTANCE;
	//g_AudioSIFInfo.curSifModeSet = SIF_PAL_UNKNOWN;
	//g_AudioSIFInfo.curSifModeGet = SIF_PAL_UNKNOWN;
	//g_AudioSIFInfo.curSifExist = SIF_DETECTION_EXSISTANCE;
	//*GetSifCountry = SifCountry;
	//*GetSoundSystem = SoundSystem;
	Audio_HwpSetChannelChange();
	Audio_HwpSetBandDelay();		
	if(SoundSystem < SIF_SYSTEM_UNKNOWN || SoundSystem > 0x1f)
	{
		alog_err("[%s] error sifBand:0x%d\n", __FUNCTION__, SoundSystem);
		return AIO_ERROR;
	}
	else
	{
		SIF_INPUT_SOURCE  rtk_sif_input_source;

		switch(g_AudioSIFInfo.sifSource)
		{
			case RHAL_AUDIO_SIF_INPUT_EXTERNAL:
			{
				rtk_sif_input_source = SIF_FROM_SIF_ADC;
				break;
			}
			case RHAL_AUDIO_SIF_INPUT_INTERNAL:
			{
				rtk_sif_input_source = SIF_FROM_IFDEMOD;
				break;
			}
			default:
			{
				alog_err("[%s] sif input:%d\n",__FUNCTION__, g_AudioSIFInfo.sifSource);
				return AIO_ERROR;
			}
		}
		
		switch(SoundSystem)
		{
			case  SIF_SYSTEM_BG:
			{
				atv_sound_std_main_system = ATV_SOUND_BG_SYSTEM;
				SoundStd = ATV_SOUND_STD_BG_MONO;
				break;
			}
			case  SIF_SYSTEM_I:
			{
				atv_sound_std_main_system = ATV_SOUND_I_SYSTEM;
				SoundStd = ATV_SOUND_STD_FM_MONO_NO_I;
				break;
			}
			case  SIF_SYSTEM_DK:
			{
				atv_sound_std_main_system = ATV_SOUND_DK_SYSTEM;
				SoundStd = ATV_SOUND_STD_DK_MONO;
				break;
			}
			case  SIF_SYSTEM_L:
			{
				atv_sound_std_main_system = ATV_SOUND_L_SYSTEM;
				SoundStd = ATV_SOUND_STD_AM_MONO;
				break;
			}
			case  SIF_SYSTEM_MN:
			{
				atv_sound_std_main_system = ATV_SOUND_MN_SYSTEM;
				SoundStd = ATV_SOUND_STD_BTSC;
				break;
			}
			default:
			{
				atv_sound_std_main_system = ATV_SOUND_AUTO_SYSTEM;
				SoundStd = ATV_SOUND_STD_BG_MONO;
				break;
			}
			#if 0
			case  SIF_SYSTEM_UNKNOWN:
			{
				atv_sound_std_main_system = ATV_SOUND_UNKNOWN_SYSTEM;
				break;
			}
			default:
			{
				alog_err("[%s] not in case1:0x%d\n", __FUNCTION__, SoundSystem);
				*GetSifCountry = SifCountry;
				*GetSoundSystem = SoundSystem;
				return AIO_ERROR;
			}
			#endif
		}

		switch (SifCountry)
		{
			 //A2
			case  SIF_ATSC_SELECT:
			case  SIF_KOREA_A2_SELECT:
			{
				alog_info("A2 func\n");
				break;
			}
			case  SIF_BTSC_SELECT:
			case  SIF_BTSC_BR_SELECT:
			case  SIF_BTSC_US_SELECT:
			{
				alog_info("BTSC func\n");
				break;
			}
			case  SIF_DVB_SELECT:
			case  SIF_DVB_ID_SELECT:
			case  SIF_DVB_IN_SELECT:
			case  SIF_DVB_CN_SELECT:
			case  SIF_DVB_AJJA_SELECT:
			{
				alog_info("DVB ASD func\n");
				break;
			}
			default:
			{
				alog_err("[%s] not in case2:0x%d\n", __FUNCTION__, SifCountry);
				*GetSifCountry = SifCountry;
				*GetSoundSystem = SoundSystem;
				return AIO_ERROR;
			}
		}

		if(Audio_HwpSetAtvAudioBand(rtk_sif_input_source, atv_sound_std_main_system)== -1)
		{
			alog_err("[%s] Audio_HwpSetAtvAudioBand not success\n",__FUNCTION__);
		}

		Audio_HwpCurSifType(SifCountry);
		g_AudioSIFInfo.curSifBand = SoundSystem;
		*GetSifCountry = SifCountry;
		*GetSoundSystem = SoundSystem;
		#if 0
		if (g_AudioSIFInfo.curSifType != SIF_ATSC_SELECT && g_AudioSIFInfo.curSifType != SIF_KOREA_A2_SELECT &&
			g_AudioSIFInfo.curSifType != SIF_BTSC_SELECT && g_AudioSIFInfo.curSifType != SIF_BTSC_BR_SELECT &&
			g_AudioSIFInfo.curSifType != SIF_BTSC_US_SELECT)
		#else
		if (1)
		#endif
		{
			Audio_Set_SoundStd(SoundStd);
		}

		if ((g_HDev_notfinish == 1) && (g_AudioSIFInfo.curSifType != SIF_TYPE_NONE) && (g_AudioSIFInfo.curSifType != SIF_TYPE_MAX))
		{
			KHAL_SIF_HDev(g_HDev_ONOFF, &getHDev_ONOFF);
			alog_info("[%s] Re-set HDEV, ONOFF= %d\n",__FUNCTION__,g_HDev_ONOFF);
		}

		
		alog_info("[%s] end\n",__FUNCTION__);
		return AIO_OK;
	}
	#else
	return AIO_ERROR;
	#endif
}

int32_t KHAL_SIF_StandardSetup(sif_standard_ext_type_t SifStandard, sif_standard_ext_type_t *GetSifStandard)
{
	#if 1
	#ifdef ALSA_BASE_EN
	#define DEBOUNCE_TIME 2
	
	ATV_SOUND_STD sound_std = ATV_SOUND_STD_UNKNOWN;
	ATV_SOUND_STD HwDetectStd = ATV_SOUND_STD_UNKNOWN;
	ATV_SOUND_STD NewHwDetectStd = ATV_SOUND_STD_UNKNOWN, PreHwDetectStd = ATV_SOUND_STD_UNKNOWN;
	ATV_SOUND_STD_MAIN_SYSTEM main_system = ATV_SOUND_UNKNOWN_SYSTEM;
	unsigned int SoundStdDebounce = 0, Timeout = 0;

	// To make sure ATV thread cannot invoke RHAL_ATVSetDecoderXMute callback and cause deadlock by AUDIO_FUNC_CALL()
//	Audio_AtvPauseTVStdDetection(true);
	
	if (GetSifStandard == NULL)
	{
		alog_err("%s, %d, ERR", __FUNCTION__, __LINE__);
		//Audio_AtvPauseTVStdDetection(false);
		return AIO_ERROR;
	}

	//alog_info("%s, SifStandard=%d, start\n", __FUNCTION__, SifStandard);
	*GetSifStandard = SIF_STANDARD_DETECT;
	if (g_user_config != AUDIO_USER_CONFIG_BRING_UP)
	{
		if(AUDIO_SIF_CHECK_INIT() != TRUE || SifStandard < SIF_STANDARD_DETECT || SifStandard > SIF_NUM_SOUND_STD)
		{
//			Audio_AtvPauseTVStdDetection(false);
			alog_err("%s, %d, ERR", __FUNCTION__, __LINE__);
			return AIO_ERROR;
		}
	}

	switch(SifStandard)
	{
		case SIF_NUM_SOUND_STD:
		case SIF_STANDARD_DETECT:
		{
			if (g_AudioSIFInfo.curSifBand == SIF_SYSTEM_BG)
				sound_std = ATV_SOUND_STD_BG_MONO;
			else	if (g_AudioSIFInfo.curSifBand == SIF_SYSTEM_I)
				sound_std = ATV_SOUND_STD_FM_MONO_NO_I;
			else	if (g_AudioSIFInfo.curSifBand == SIF_SYSTEM_DK)
				sound_std = ATV_SOUND_STD_DK_MONO;
			else	if (g_AudioSIFInfo.curSifBand == SIF_SYSTEM_L)
				sound_std = SIF_L_AM;
			else sound_std = SIF_SYSTEM_MN;
			break;
		}
		//BG
		case  SIF_BG_NICAM:
		{
			sound_std = ATV_SOUND_STD_NICAM_BG;
			break;
		}
		case  SIF_BG_FM:
		{
			sound_std = ATV_SOUND_STD_BG_MONO;
			break;
		}
		case  SIF_BG_A2:
		{
			sound_std = ATV_SOUND_STD_A2_BG;
			break;
		}
		//I
		case  SIF_I_NICAM:
		{
			sound_std = ATV_SOUND_STD_NICAM_I;
			break;
		}
		case  SIF_I_FM:
		{
			sound_std = ATV_SOUND_STD_FM_MONO_NO_I;
			break;
		}
		//DK
		case  SIF_DK_NICAM:
		{
			sound_std = ATV_SOUND_STD_NICAM_DK;
			break;
		}
		case  SIF_DK_FM:
		{
			sound_std = ATV_SOUND_STD_DK_MONO;
			break;
		}
		case  SIF_DK1_A2:
		{
			sound_std = ATV_SOUND_STD_A2_DK1;
			break;
		}
		case  SIF_DK2_A2:
		{
			sound_std = ATV_SOUND_STD_A2_DK2;
			break;
		}
		case  SIF_DK3_A2:
		{
			sound_std = ATV_SOUND_STD_A2_DK3;
			break;
		}
		//L
		case  SIF_L_NICAM:
		{
			sound_std = ATV_SOUND_STD_NICAM_L;
			break;
		}
		case  SIF_L_AM:
		{
			sound_std = ATV_SOUND_STD_AM_MONO;
			break;
		}
		//MN
		case  SIF_MN_A2:
		{
			sound_std = ATV_SOUND_STD_A2_M;
			break;
		}
		case  SIF_MN_BTSC:
		{
			sound_std = ATV_SOUND_STD_BTSC;
			break;
		}
		case  SIF_MN_EIAJ:
		{
			alog_err("SIF_MN_EIAJ not support\n");
			#ifdef FUNCTION_EN
			//Release sem
//			Audio_AtvPauseTVStdDetection(false);
			#endif
			return AIO_ERROR;
		}
		default:
		{
			alog_err("%s, %d, ERR", __FUNCTION__, __LINE__);
			#ifdef FUNCTION_EN
			//Release sem
//			Audio_AtvPauseTVStdDetection(false);
			#endif
			return AIO_ERROR;
		}
	}

// Clayton: No need to return when curSifStandard = ATV_SOUND_STD_UNKNOWN
/*
	if (g_AudioSIFInfo.curSifStandard != ATV_SOUND_STD_UNKNOWN)
	{
		//Audio_AtvPauseTVStdDetection(false);
		Audio_SoundStd_To_Hal(GetSifStandard, g_AudioSIFInfo.curSifStandard);
		return AIO_OK;
	}
*/
	if (g_AudioSIFInfo.curSifType == SIF_BTSC_US_SELECT)
	{
		sound_std = ATV_SOUND_STD_BTSC;
	}

	//Audio_AtvPauseTVStdDetection(true);
	//Audio_AtvSetSoundStd(main_system, sound_std);
	//Audio_AtvPauseTVStdDetection(false);
	if (g_AudioSIFInfo.curSifType != SIF_ATSC_SELECT && g_AudioSIFInfo.curSifType != SIF_KOREA_A2_SELECT &&
			g_AudioSIFInfo.curSifType != SIF_BTSC_SELECT && g_AudioSIFInfo.curSifType != SIF_BTSC_BR_SELECT &&
			g_AudioSIFInfo.curSifType != SIF_BTSC_US_SELECT)
	{
		while (1)
		{
			audio_hw_usleep(25*1000);
			//HwDetectStd = (ATV_SOUND_STD)Audio_GetHWDectectStd();
			
			//alog_info("%s, [Clayton1]HwDetectStd=%x\n", __FUNCTION__, HwDetectStd);
			// Clayton: Originally "Audio_HwpSIFDetectedSoundStandard" can return the same Hwdetectstd, no need to add Audio_GetHWDectectStd()
			Audio_HwpSIFDetectedSoundStandard(&HwDetectStd);
			//alog_info("%s, [Clayton2]HwDetectStd=%x\n", __FUNCTION__, HwDetectStd);
			//HwDetectStd = (ATV_SOUND_STD)AtvGetHWStd();
			//alog_err("%s, HwDetectStd=%x\n", __FUNCTION__, HwDetectStd);
			if (PreHwDetectStd != HwDetectStd)
			{
				PreHwDetectStd = HwDetectStd;
				SoundStdDebounce = 0;
			}
			else
			{
				if (HwDetectStd != ATV_SOUND_STD_UNKNOWN)
				{
					++ SoundStdDebounce;
				}
				else
				{
					SoundStdDebounce = 0;
				}
			}

			if (SoundStdDebounce >= DEBOUNCE_TIME || Timeout > 45)
			{
				NewHwDetectStd = HwDetectStd;
				break;
			}

			++Timeout;
		}
		Audio_AtvEnableAutoChangeStdFlag(1); // Enable driver auto std change for all model except ATSC model
	}
	else
	{
		NewHwDetectStd = sound_std;
		Audio_AtvEnableAutoChangeStdFlag(0); //Disable driver auto std change for ATSC model
	}
	// Clayton: Remove "Audio_AtvPauseTVStdDetection" calling when set sound std
	//alog_info("%s, [Clayton3]HwDetectStd=0x%x NewHwDetectStd= 0x%x\n", __FUNCTION__, HwDetectStd,NewHwDetectStd);

	if (g_AudioSIFInfo.curSifStandard == NewHwDetectStd)
	{
		//Audio_AtvPauseTVStdDetection(false);
		Audio_SoundStd_To_Hal(GetSifStandard, g_AudioSIFInfo.curSifStandard);
		return AIO_OK;
	}

	Audio_AtvPauseTVStdDetection(true);
	Audio_AtvSetSoundStd(main_system, NewHwDetectStd);
	Audio_AtvPauseTVStdDetection(false);
	//audio_hw_usleep(100*1000);
	g_AudioSIFInfo.curSifStandard = NewHwDetectStd;
	Audio_SoundStd_To_Hal(GetSifStandard, NewHwDetectStd);
	alog_info("%s, GetSifStandard=%x, NewHwDetectStd=%x, end\n", __FUNCTION__, *GetSifStandard, NewHwDetectStd);
	return AIO_OK;
	#else
	return AIO_ERROR;
	#endif
	#else
	ATV_SOUND_STD sound_std = ATV_SOUND_STD_UNKNOWN;
	ATV_SOUND_STD_MAIN_SYSTEM main_system = ATV_SOUND_UNKNOWN_SYSTEM;
	
	if (GetSifStandard == NULL)
	{
		alog_err("%s, %d, ERR", __FUNCTION__, __LINE__);
		return AIO_ERROR;
	}

	*GetSifStandard = SIF_STANDARD_DETECT;
	if (g_user_config != AUDIO_USER_CONFIG_BRING_UP)
	{
		if(AUDIO_SIF_CHECK_INIT() != TRUE || SifStandard < SIF_STANDARD_DETECT || SifStandard > SIF_NUM_SOUND_STD)
		{
			alog_err("%s, %d, ERR", __FUNCTION__, __LINE__);
			return AIO_ERROR;
		}
	}

	if (g_AudioSIFInfo.curSifType != SIF_ATSC_SELECT && g_AudioSIFInfo.curSifType != SIF_KOREA_A2_SELECT &&
		g_AudioSIFInfo.curSifType != SIF_BTSC_SELECT && g_AudioSIFInfo.curSifType != SIF_BTSC_BR_SELECT &&
		g_AudioSIFInfo.curSifType != SIF_BTSC_US_SELECT)
	{
		Audio_SoundStd_To_Hal(GetSifStandard, g_AudioSIFInfo.curSifStandard);
		return AIO_OK;
	}

	// To make sure ATV thread cannot invoke RHAL_ATVSetDecoderXMute callback and cause deadlock by AUDIO_FUNC_CALL()
	Audio_AtvPauseTVStdDetection(true);
	switch(SifStandard)
	{
		case SIF_NUM_SOUND_STD:
		case SIF_STANDARD_DETECT:
		{
			if (g_AudioSIFInfo.curSifBand == SIF_SYSTEM_BG)
				sound_std = ATV_SOUND_STD_BG_MONO;
			else	if (g_AudioSIFInfo.curSifBand == SIF_SYSTEM_I)
				sound_std = ATV_SOUND_STD_FM_MONO_NO_I;
			else	if (g_AudioSIFInfo.curSifBand == SIF_SYSTEM_DK)
				sound_std = ATV_SOUND_STD_DK_MONO;
			else	if (g_AudioSIFInfo.curSifBand == SIF_SYSTEM_L)
				sound_std = SIF_L_AM;
			else sound_std = SIF_SYSTEM_MN;
			break;
		}
		//BG
		case  SIF_BG_NICAM:
		{
			sound_std = ATV_SOUND_STD_NICAM_BG;
			break;
		}
		case  SIF_BG_FM:
		{
			sound_std = ATV_SOUND_STD_BG_MONO;
			break;
		}
		case  SIF_BG_A2:
		{
			sound_std = ATV_SOUND_STD_A2_BG;
			break;
		}
		//I
		case  SIF_I_NICAM:
		{
			sound_std = ATV_SOUND_STD_NICAM_I;
			break;
		}
		case  SIF_I_FM:
		{
			sound_std = ATV_SOUND_STD_FM_MONO_NO_I;
			break;
		}
		//DK
		case  SIF_DK_NICAM:
		{
			sound_std = ATV_SOUND_STD_NICAM_DK;
			break;
		}
		case  SIF_DK_FM:
		{
			sound_std = ATV_SOUND_STD_DK_MONO;
			break;
		}
		case  SIF_DK1_A2:
		{
			sound_std = ATV_SOUND_STD_A2_DK1;
			break;
		}
		case  SIF_DK2_A2:
		{
			sound_std = ATV_SOUND_STD_A2_DK2;
			break;
		}
		case  SIF_DK3_A2:
		{
			sound_std = ATV_SOUND_STD_A2_DK3;
			break;
		}
		//L
		case  SIF_L_NICAM:
		{
			sound_std = ATV_SOUND_STD_NICAM_L;
			break;
		}
		case  SIF_L_AM:
		{
			sound_std = ATV_SOUND_STD_AM_MONO;
			break;
		}
		//MN
		case  SIF_MN_A2:
		{
			sound_std = ATV_SOUND_STD_A2_M;
			break;
		}
		case  SIF_MN_BTSC:
		{
			sound_std = ATV_SOUND_STD_BTSC;
			break;
		}
		case  SIF_MN_EIAJ:
		{
			alog_err("SIF_MN_EIAJ not support\n");
			#ifdef FUNCTION_EN
			//Release sem
			Audio_AtvPauseTVStdDetection(false);
			#endif
			return AIO_ERROR;
		}
		default:
		{
			alog_err("%s, %d, ERR", __FUNCTION__, __LINE__);
			#ifdef FUNCTION_EN
			//Release sem
			Audio_AtvPauseTVStdDetection(false);
			#endif
			return AIO_ERROR;
		}
	}

	if (g_AudioSIFInfo.curSifType == SIF_BTSC_US_SELECT)
	{
		sound_std = ATV_SOUND_STD_BTSC;
	}

	if (g_AudioSIFInfo.curSifStandard == sound_std)
	{
		//alog_info("%s, WebOS set the same sound std, %x\n", __FUNCTION__, sound_std);
		Audio_AtvPauseTVStdDetection(false);
		*GetSifStandard = SifStandard;
		return AIO_OK;
	}		

	Audio_AtvSetSoundStd(main_system, sound_std);
	g_AudioSIFInfo.curSifStandard = sound_std;
	Audio_AtvPauseTVStdDetection(false);
	*GetSifStandard = SifStandard;
	alog_info("%s, SifStandard=%d, end\n", __FUNCTION__, SifStandard);
	return AIO_OK;
	#endif
}

int32_t KHAL_SIF_CurAnalogMode(sif_mode_ext_type_t *GetAnalogMode)
{
	#ifdef ALSA_BASE_EN
	ATV_SOUND_INFO sound_info = {0};
	uint8_t isNTSC = 0;
	int nicam_stable = 0;
	ATV_SOUND_STD HwDetectStd = ATV_SOUND_STD_UNKNOWN;
	ATV_SOUND_STD RealHwDetectStd = ATV_SOUND_STD_UNKNOWN;

	if (g_user_config != AUDIO_USER_CONFIG_BRING_UP)
	{
		if(AUDIO_SIF_CHECK_INIT() != TRUE || GetAnalogMode == NULL)
			return AIO_ERROR;
	}

	if (g_ATV_CONNECT == 0)
	{
		//*GetAnalogMode = SIF_PAL_UNKNOWN;
		return AIO_ERROR;
	}

	*GetAnalogMode = SIF_PAL_UNKNOWN; //need add for PAL NTSC BTSC..........
	Audio_AtvGetSoundStd(&sound_info);
	if (sound_info.std_type == ATV_MAIN_STD_NICAM)
	{
		*GetAnalogMode = SIF_PAL_NICAM_MONO;
	}
	else if (sound_info.std_type == ATV_MAIN_STD_A2)
	{
		*GetAnalogMode = SIF_NTSC_A2_MONO;
	}
	else if (sound_info.std_type == ATV_MAIN_STD_BTSC)
	{
		*GetAnalogMode = SIF_NTSC_BTSC_MONO;
	}
	else if (sound_info.std_type == ATV_MAIN_STD_MONO)
	{
		*GetAnalogMode = SIF_PAL_MONO;
	}

	#if 0
	switch(sound_info.std_type)
	{
		case ATV_MAIN_STD_MONO:
		{
			*GetAnalogMode = SIF_PAL_MONO;
			break;
		}
		case ATV_MAIN_STD_NICAM:
		{
			if(sound_info.dig_soundmode  == ATV_SOUND_MODE_MONO)
			{
				nicam_stable = Audio_AtvGetNicamSignalStable();
				if(nicam_stable == 1)
				{
					*GetAnalogMode = SIF_PAL_NICAM_MONO;
				}
				else
				{
					*GetAnalogMode = SIF_PAL_MONO;
				}
			}
			else if(sound_info.dig_soundmode  == ATV_SOUND_MODE_STEREO)
			{
				*GetAnalogMode = SIF_PAL_NICAM_STEREO;
			}
			else if(sound_info.dig_soundmode  == ATV_SOUND_MODE_DUAL)
			{
				*GetAnalogMode = SIF_PAL_NICAM_DUAL;
			}
			
			break;
		}
		case ATV_MAIN_STD_A2:
		{
			if((sound_info.sound_std == ATV_SOUND_STD_MN_MONO || sound_info.sound_std == ATV_SOUND_STD_BTSC || sound_info.sound_std == ATV_SOUND_STD_A2_M))
			{
				isNTSC = 1;
			}
			
			if(sound_info.ana_soundmode == ATV_SOUND_MODE_STEREO)
			{
				if(isNTSC == 1)
				{
					*GetAnalogMode = SIF_NTSC_A2_STEREO;
				}
				else
				{
					*GetAnalogMode = SIF_PAL_STEREO;
				}
			}
			else if(sound_info.ana_soundmode == ATV_SOUND_MODE_DUAL)
			{
				if(isNTSC == 1)
				{
					*GetAnalogMode = SIF_NTSC_A2_SAP;
				}
				else
				{
					*GetAnalogMode = SIF_PAL_DUAL;
				}
			}
			else if(sound_info.ana_soundmode == ATV_SOUND_MODE_MONO)
			{
				if(isNTSC == 1)
				{
					*GetAnalogMode = SIF_NTSC_A2_MONO;
				}
				else
				{
					*GetAnalogMode = SIF_PAL_MONO;
				}
			}
			
			break;
		}
		case ATV_MAIN_STD_BTSC:
		{
			if(sound_info.ana_soundmode == ATV_SOUND_MODE_MONO)
			{
				*GetAnalogMode =  SIF_NTSC_BTSC_MONO;
			}
			else if(sound_info.ana_soundmode == ATV_SOUND_MODE_STEREO)
			{
				*GetAnalogMode = SIF_NTSC_BTSC_STEREO;
			}
			else if(sound_info.ana_soundmode == ATV_SOUND_MODE_SAP_MONO)
			{
				*GetAnalogMode = SIF_NTSC_BTSC_SAP_MONO;
			}
			else if(sound_info.ana_soundmode == ATV_SOUND_MODE_SAP_STEREO)
			{
				*GetAnalogMode =  SIF_NTSC_BTSC_SAP_STEREO;
			}

			break;
		}
		case ATV_MAIN_STD_UNKNOW:
		default:
		{
			break;
		}
	}
	#else
	RealHwDetectStd = (ATV_SOUND_STD)Audio_GetHWDectectStd();
	Audio_HwpSIFDetectedSoundStandard(&HwDetectStd);
	//g_AudioSIFInfo.curSifStandard = sound_info.sound_std;	//Clayton: sound_std needs to refreshed by sound_info.sound_std
	//alog_info("[%s] HwDetectStd=0x%x, curSifStandard=0x%x, sound_info.sound_std=0x%x, RealHwDetectStd=0x%x\n", __FUNCTION__, HwDetectStd,g_AudioSIFInfo.curSifStandard,sound_info.sound_std,RealHwDetectStd);
	//switch(g_AudioSIFInfo.curSifStandard)
	switch(sound_info.sound_std)
	{
		case ATV_SOUND_STD_MN_MONO:
			isNTSC = 1;
			*GetAnalogMode = SIF_NTSC_A2_MONO;
			break;
			
		case ATV_SOUND_STD_BG_MONO:
		case ATV_SOUND_STD_DK_MONO:
		case ATV_SOUND_STD_AM_MONO:
		case ATV_SOUND_STD_FM_MONO_NO_I:
		{
			*GetAnalogMode = SIF_PAL_MONO;
			break;
		}
		case ATV_SOUND_STD_NICAM_BG:
		case ATV_SOUND_STD_NICAM_DK:
		case ATV_SOUND_STD_NICAM_I:
		case ATV_SOUND_STD_NICAM_L:
		{
			if(sound_info.dig_soundmode  == ATV_SOUND_MODE_MONO)
			{
				nicam_stable = Audio_AtvGetNicamSignalStable();
				if(nicam_stable == 1)
				{
					*GetAnalogMode = SIF_PAL_NICAM_MONO;
				}
				else
				{
					*GetAnalogMode = SIF_PAL_MONO;
				}
			}
			else if(sound_info.dig_soundmode  == ATV_SOUND_MODE_STEREO)
			{
				*GetAnalogMode = SIF_PAL_NICAM_STEREO;
			}
			else if(sound_info.dig_soundmode  == ATV_SOUND_MODE_DUAL)
			{
				*GetAnalogMode = SIF_PAL_NICAM_DUAL;
			}
			
			break;
		}
		case ATV_SOUND_STD_A2_M:
		case ATV_SOUND_STD_A2_BG:
		case ATV_SOUND_STD_A2_DK1:
		case ATV_SOUND_STD_A2_DK2:
		case ATV_SOUND_STD_A2_DK3:
		{
			if((sound_info.sound_std == ATV_SOUND_STD_MN_MONO || sound_info.sound_std == ATV_SOUND_STD_BTSC || sound_info.sound_std == ATV_SOUND_STD_A2_M))
			{
				isNTSC = 1;
			}
			
			if(sound_info.ana_soundmode == ATV_SOUND_MODE_STEREO)
			{
				if(isNTSC == 1)
				{
					*GetAnalogMode = SIF_NTSC_A2_STEREO;
				}
				else
				{
					*GetAnalogMode = SIF_PAL_STEREO;
				}
			}
			else if(sound_info.ana_soundmode == ATV_SOUND_MODE_DUAL)
			{
				if(isNTSC == 1)
				{
					*GetAnalogMode = SIF_NTSC_A2_SAP;
				}
				else
				{
					*GetAnalogMode = SIF_PAL_DUAL;
				}
			}
			else if(sound_info.ana_soundmode == ATV_SOUND_MODE_MONO)
			{
				if(isNTSC == 1)
				{
					*GetAnalogMode = SIF_NTSC_A2_MONO;
				}
				else
				{
					*GetAnalogMode = SIF_PAL_MONO;
				}
			}
			
			break;
		}
		case ATV_SOUND_STD_BTSC:
		{
			if(sound_info.ana_soundmode == ATV_SOUND_MODE_MONO)
			{
				*GetAnalogMode =  SIF_NTSC_BTSC_MONO;
			}
			else if(sound_info.ana_soundmode == ATV_SOUND_MODE_STEREO)
			{
				*GetAnalogMode = SIF_NTSC_BTSC_STEREO;
			}
			else if(sound_info.ana_soundmode == ATV_SOUND_MODE_SAP_MONO)
			{
				*GetAnalogMode = SIF_NTSC_BTSC_SAP_MONO;
			}
			else if(sound_info.ana_soundmode == ATV_SOUND_MODE_SAP_STEREO)
			{
				*GetAnalogMode =  SIF_NTSC_BTSC_SAP_STEREO;
			}

			break;
		}
		default:
		{
			break;
		}
	}
	#endif

	g_AudioSIFInfo.curSifStandard = sound_info.sound_std;
	g_AudioSIFInfo.curSifModeGet = *GetAnalogMode;
	//alog_info("GetAnalogMode=%x, std=%x\n", *GetAnalogMode, g_AudioSIFInfo.curSifStandard);
	//alog_info("[%s] GetAnalogMode=0x%x, std=0x%x\n", __FUNCTION__, *GetAnalogMode, g_AudioSIFInfo.curSifStandard);

	if(RealHwDetectStd == ATV_SOUND_STD_UNKNOWN)
	{
		if((*GetAnalogMode ==SIF_PAL_MONO) || (*GetAnalogMode ==SIF_PAL_STEREO) || (*GetAnalogMode ==SIF_PAL_DUAL) || (*GetAnalogMode ==SIF_PAL_NICAM_MONO) || (*GetAnalogMode ==SIF_PAL_NICAM_STEREO) || (*GetAnalogMode ==SIF_PAL_NICAM_DUAL))
			*GetAnalogMode = SIF_PAL_UNKNOWN;
		else if((*GetAnalogMode ==SIF_NTSC_A2_MONO) || (*GetAnalogMode ==SIF_NTSC_A2_STEREO) || (*GetAnalogMode ==SIF_NTSC_A2_SAP))
			*GetAnalogMode = SIF_NTSC_A2_UNKNOWN;
		else if((*GetAnalogMode ==SIF_NTSC_BTSC_MONO) || (*GetAnalogMode ==SIF_NTSC_BTSC_STEREO) || (*GetAnalogMode ==SIF_NTSC_BTSC_SAP_MONO) || (*GetAnalogMode ==SIF_NTSC_BTSC_SAP_STEREO))
			*GetAnalogMode = SIF_NTSC_BTSC_UNKNOWN;

		//alog_info("[%s] RealHwDetectStd=0x%x, *GetAnalogMode=0x%x\n", __FUNCTION__, RealHwDetectStd, *GetAnalogMode);
	}


	return AIO_OK;
	#else
	return AIO_ERROR;
	#endif
}

int32_t KHAL_SIF_UserAnalogMode(sif_mode_user_ext_type_t UserAnalogMode, sif_mode_user_ext_type_t *GetUserAnalogMode)
{
	#ifdef ALSA_BASE_EN
	uint32_t force_analogmode =0;

	if (GetUserAnalogMode == NULL)
		return AIO_ERROR;
//	
	if (AtvIsSifMode(UserAnalogMode)==0)
		return AIO_ERROR;

	//*GetUserAnalogMode = UserAnalogMode;
	if (AUDIO_SIF_CHECK_INIT() != TRUE)
		return AIO_ERROR;

	//if (UserAnalogMode < SIF_USER_PAL_UNKNOWN || UserAnalogMode > SIF_USER_NTSC_BTSC_SAP_STEREO)
		//return AIO_ERROR;

	if (g_ATV_CONNECT == 0)
	{
		//*GetUserAnalogMode = SIF_USER_PAL_UNKNOWN;
		return AIO_ERROR;
	}

	switch(UserAnalogMode)
	{
		//A2
		case  SIF_USER_PAL_MONO:
		{
			Audio_AtvSetA2SoundSelect(0, 0);
			Audio_AtvSetNICAMSoundSelect(0, 0);
			break;
		}
		case  SIF_USER_PAL_STEREO:
		{
			Audio_AtvSetA2SoundSelect(1, 0);
			break;
		}
		case  SIF_USER_PAL_DUALI:
		{
			Audio_AtvSetA2SoundSelect(0, 0);
			break;
		}
		case  SIF_USER_PAL_DUALII:
		{
			Audio_AtvSetA2SoundSelect(0, 1);
			break;
		}
		case  SIF_USER_PAL_DUALI_II:
		{
			Audio_AtvSetA2SoundSelect(0, 2);
			break;
		}
		//NICAM
		case  SIF_USER_PAL_NICAM_MONO:
		{
			Audio_AtvSetNICAMSoundSelect(1, 0);
			break;
		}
		case  SIF_USER_PAL_NICAM_STEREO:
		{
			Audio_AtvSetNICAMSoundSelect(1, 0);
			break;
		}
		case  SIF_USER_PAL_NICAM_DUALI:
		{
			Audio_AtvSetNICAMSoundSelect(1, 0);
			break;
		}
		case  SIF_USER_PAL_NICAM_DUALII:
		{
			Audio_AtvSetNICAMSoundSelect(1, 1);
			break;
		}
		case  SIF_USER_PAL_NICAM_DUALI_II:
		{
			Audio_AtvSetNICAMSoundSelect(1, 2);
			break;
		}
		//A2
		case  SIF_USER_NTSC_A2_MONO:
		{
			Audio_AtvSetA2SoundSelect(0, 0);
			break;
		}
		case  SIF_USER_NTSC_A2_SAP:
		{
			Audio_AtvSetA2SoundSelect(0, 1);//Lang B
			break;
		}
		case  SIF_USER_NTSC_A2_STEREO:
		{
			Audio_AtvSetA2SoundSelect(1, 0);
			break;
		}
		//BTSC
		case  SIF_USER_NTSC_BTSC_MONO:
		{
			Audio_AtvSetBTSCSoundSelect(0,0);
			break;
		}
		case  SIF_USER_NTSC_BTSC_STEREO:
		{
			Audio_AtvSetBTSCSoundSelect(1,0);
			break;
		}
		case  SIF_USER_NTSC_BTSC_SAP_MONO:
		{
			//assume SAP_1
			Audio_AtvSetBTSCSoundSelect(1,1);//
			break;
		}
		case  SIF_USER_NTSC_BTSC_SAP_STEREO:
		{
			//assume SAP_2
			Audio_AtvSetBTSCSoundSelect(1,1);
			break;
		}
		case SIF_USER_PAL_MONO_FORCED:
		{
			Audio_AtvSetA2SoundSelect(0, 0);
			Audio_AtvSetNICAMSoundSelect(0, 0);
			break;
		}
		case SIF_USER_PAL_STEREO_FORCED:
		{
			Audio_AtvSetA2SoundSelect(0, 0);
			break;
		}
		case SIF_USER_PAL_NICAM_MONO_FORCED:
		{
			Audio_AtvSetNICAMSoundSelect(0, 0);
			force_analogmode = 1;
			break;
		}
		case SIF_USER_PAL_NICAM_STEREO_FORCED:
		{
			Audio_AtvSetNICAMSoundSelect(0, 0);
			force_analogmode = 1;
			break;
		}
		case SIF_USER_PAL_NICAM_DUAL_FORCED:
		{
			Audio_AtvSetNICAMSoundSelect(0, 0);
			force_analogmode = 1;
			break;
		}
		//-------------
		case SIF_USER_PAL_UNKNOWN:
		case SIF_USER_NTSC_A2_UNKNOWN:
		case SIF_USER_NTSC_BTSC_UNKNOWN:
		{
			*GetUserAnalogMode = UserAnalogMode;
			return AIO_OK;
		}
		default:
		{
			alog_err("[%s]Not Ready case or expected value=%d\n", __FUNCTION__, UserAnalogMode);
			return AIO_ERROR;
		}
	}
		
	alog_info("[%s] Sound Sel=%d\n", __FUNCTION__, *GetUserAnalogMode);
	g_AudioSIFInfo.curSifModeSet = UserAnalogMode;
	if(force_analogmode == 1)
	{
		Audio_AtvForceSoundSel(1);
	}
	else
	{
		Audio_AtvForceSoundSel(0);
	}
	*GetUserAnalogMode = UserAnalogMode;
	return AIO_OK;
	#else
	return AIO_ERROR;
	#endif
}

int32_t KHAL_SIF_SifExist(sif_existence_info_ext_type_t *GetSifExist)
{
	#ifdef ALSA_BASE_EN
	ATV_SOUND_STD_MAIN_SYSTEM atv_sound_std_main_system = ATV_SOUND_UNKNOWN_SYSTEM;
	ATV_SOUND_STD_MAIN_SYSTEM HWDetectSoundSystem = ATV_SOUND_UNKNOWN_SYSTEM;
	ATV_SOUND_STD HWDetectSoundStd = ATV_SOUND_STD_UNKNOWN;
	uint32_t ToneSNR = 0;
	uint32_t passSNR = 0x1500; //dB

	if(AUDIO_SIF_CHECK_INIT() != TRUE || GetSifExist == NULL)
		return AIO_ERROR;
	
	#ifndef FUNCTION_EN
	*GetSifExist = SIF_PRESENT;
	return AIO_OK;
	#endif
	
	for(atv_sound_std_main_system=ATV_SOUND_DK_SYSTEM; atv_sound_std_main_system<=ATV_SOUND_L_SYSTEM;)
	{
		Audio_HwpSIFGetMainToneSNR(atv_sound_std_main_system, &HWDetectSoundSystem, &HWDetectSoundStd, &ToneSNR);
		atv_sound_std_main_system =  (ATV_SOUND_STD_MAIN_SYSTEM)(atv_sound_std_main_system+1);
		if(ToneSNR > passSNR)
		{
			alog_info("[%s] tone %d pass %d\n", __FUNCTION__, ToneSNR, passSNR);
			*GetSifExist = SIF_PRESENT;
			g_AudioSIFInfo.curSifExist = *GetSifExist;
			return AIO_OK;
		}
	}
	
	alog_info("[%s] tone %d pass %d\n", __FUNCTION__, ToneSNR, passSNR);
	*GetSifExist = SIF_ABSENT;
	g_AudioSIFInfo.curSifExist = *GetSifExist;
	return AIO_OK;
	#else
	return AIO_ERROR;
	#endif
}

int32_t KHAL_SIF_HDev(uint32_t OnOff, uint32_t *GetOnOff)
{
	#ifdef ALSA_BASE_EN
	int am_wider_bw = 0;
	A2_BW_SEL_T  deviation_bw;
	A2_BW_SEL_T  deviation_bw_sub;

	deviation_bw_sub = BANDWIDTH_HDV0_120KHZ;
	if (g_user_config != AUDIO_USER_CONFIG_BRING_UP)
	{
		if (GetOnOff == NULL)
		{
			return AIO_ERROR;
		}
		
		if(AUDIO_SIF_CHECK_INIT() != TRUE)
		{
			*GetOnOff = 0;
			return AIO_ERROR;
		}
	}

	if(OnOff > 1)
	{
		alog_err("[%s] HighDevMode %d\n", __FUNCTION__, OnOff);
		return AIO_ERROR;
	}

	#ifndef FUNCTION_EN
	*GetOnOff = OnOff;
	return AIO_OK;
	#endif

	alog_info("[%s]New OnOff=%d, Stype=%d\n", __FUNCTION__, OnOff, g_AudioSIFInfo.curSifType);

	g_HDev_notfinish = 0;	//default set 0, if g_AudioSIFInfo.curSifType is not ready, set 1
	if (OnOff == 1)
	{
		Audio_AtvSetDevOnOff(TRUE);
		if (g_AudioSIFInfo.curSifType == SIF_KOREA_A2_SELECT || g_AudioSIFInfo.curSifType == SIF_ATSC_SELECT)
		{
			deviation_bw = BANDWIDTH_HDV0_240KHZ;    //200Khz
		}
		else if (g_AudioSIFInfo.curSifType == SIF_DVB_AJJA_SELECT)
		{
			deviation_bw = BANDWIDTH_HDV1_740KHZ;    //540Khz
			am_wider_bw = 0;
		}
		else if (g_AudioSIFInfo.curSifType == SIF_DVB_IN_SELECT)
		{
			deviation_bw = BANDWIDTH_HDV1_740KHZ;    //540Khz
			am_wider_bw = 0;
		}
		else if (g_AudioSIFInfo.curSifType == SIF_DVB_SELECT || g_AudioSIFInfo.curSifType == SIF_DVB_CN_SELECT)
		{
			deviation_bw = BANDWIDTH_HDV1_480KHZ;    //384Khz
			am_wider_bw = 0;
		}
		else if (g_AudioSIFInfo.curSifType == SIF_DVB_ID_SELECT)
		{
			deviation_bw = BANDWIDTH_HDV1_480KHZ;    //384Khz
			am_wider_bw = 0;
		}
		else if ( g_AudioSIFInfo.curSifType == SIF_BTSC_SELECT || g_AudioSIFInfo.curSifType == SIF_BTSC_BR_SELECT
			|| g_AudioSIFInfo.curSifType == SIF_BTSC_US_SELECT)
		{
			deviation_bw = BANDWIDTH_HDV0_370KHZ;
		}
		else
		{
			deviation_bw = BANDWIDTH_HDV0_120KHZ;    //100Khz
			g_HDev_notfinish = 1;
			g_HDev_ONOFF = OnOff;
			
		}
	}
	else
	{
		Audio_AtvSetDevOnOff(FALSE);
		if (g_AudioSIFInfo.curSifType == SIF_KOREA_A2_SELECT || g_AudioSIFInfo.curSifType == SIF_ATSC_SELECT)
		{
			deviation_bw = BANDWIDTH_HDV0_240KHZ;    //200Khz
		}
		else if (g_AudioSIFInfo.curSifType == SIF_DVB_AJJA_SELECT)
		{
			deviation_bw = BANDWIDTH_HDV0_370KHZ;    //100kHz
			am_wider_bw = 0;
		}
		else if (g_AudioSIFInfo.curSifType == SIF_DVB_IN_SELECT)
		{
			deviation_bw = BANDWIDTH_HDV0_370KHZ;    //100kHz
			am_wider_bw = 0;
		}
		else if (g_AudioSIFInfo.curSifType == SIF_DVB_SELECT || g_AudioSIFInfo.curSifType == SIF_DVB_CN_SELECT)
		{
			deviation_bw = BANDWIDTH_HDV0_370KHZ;    //100kHz
			am_wider_bw = 0;
		}
		else if (g_AudioSIFInfo.curSifType == SIF_DVB_ID_SELECT)
		{
			deviation_bw = BANDWIDTH_HDV0_370KHZ;    //100kHz
			am_wider_bw = 0;
		}
		else if ( g_AudioSIFInfo.curSifType == SIF_BTSC_SELECT || g_AudioSIFInfo.curSifType == SIF_BTSC_BR_SELECT
			|| g_AudioSIFInfo.curSifType == SIF_BTSC_US_SELECT)
		{
			deviation_bw = BANDWIDTH_HDV0_370KHZ;
		}
		else
		{
			deviation_bw = BANDWIDTH_HDV0_120KHZ;  //50Khz
			g_HDev_notfinish = 1;
			g_HDev_ONOFF = OnOff;			
		}
	}
	
	Audio_AtvSetDevBandWidth(deviation_bw, deviation_bw_sub);
	g_AudioSIFInfo.bHighDevOnOff = OnOff;
	*GetOnOff = g_AudioSIFInfo.bHighDevOnOff;
	return AIO_OK;
	#else
	return AIO_ERROR;
	#endif
}

int32_t KHAL_SIF_A2ThresholdLevel(uint32_t Threshold, uint32_t *GetThreshold)
{
	#ifdef ALSA_BASE_EN
	if(AUDIO_SIF_CHECK_INIT() != TRUE || GetThreshold == NULL)
	{
		return AIO_ERROR;
	}

	if(Threshold > 100)
	{
		alog_err("[%s]A2 TH Not expected value %d\n", __FUNCTION__, Threshold);
		return AIO_ERROR;
	}

	#ifndef FUNCTION_EN
	*GetThreshold = Threshold;
	return AIO_OK;
	#endif

	Audio_HwpSIFSetA2StereoDualTH(Threshold);
	*GetThreshold = Threshold;
	return AIO_OK;
	#else
	return AIO_ERROR;
	#endif
}

//Internal Functions
int32_t AUDIO_SIF_CHECK_INIT(void)
{
	#ifdef ALSA_BASE_EN
	#if 0 //need to enable by ryanlan
	if (Aud_initial != 1)
	{
		alog_err("audiohw, check Aud_initial fail\n");
	}
	#endif

	if (g_AudioHw_init != 1)
	{
		alog_err("audiohw, check g_AudioHw_init fail\n");
	}
	else if (Aud_initial != 1)
	{
		alog_err("audiohw, check Aud_initial sw fail\n");
	}
	
	return (Aud_initial & g_AudioHw_init); //need to enable by ryanlan
	return TRUE;
	#else
	return TRUE;
	#endif
}

int32_t AUDIO_SIF_STOP_DETECT(bool en)
{
	#ifdef ALSA_BASE_EN
		#ifdef FUNCTION_EN
		alog_err("%s, %d", __FUNCTION__, __LINE__);
		Audio_AtvPauseTVStdDetection(en);
		#endif
	return AIO_OK;
	#else
	return AIO_ERROR;
	#endif
}

int32_t AUDIO_SIF_RESET_DATA(void)
{
	#ifdef ALSA_BASE_EN
		alog_err("\n%s\n", __FUNCTION__);
		g_ATV_CONNECT = 0;
		g_ATV_DETECT = 0;
		#ifdef FUNCTION_EN
		Audio_AtvCleanTVSourceData();
		#endif
	return AIO_OK;
	#else
	return AIO_ERROR;
	#endif
}

int32_t AUDIO_SIF_INIT(void)
{
	ATV_CFG atv_cfg = {0};

	alog_info("audiohw init s\n");
	if (g_AudioHw_init == 1)
	{
		alog_err("audiohw init ready bypass\n");
		return AIO_OK;
	}

	g_AudioHw_init = 1;
	Audio_SetAudioHWConfig(AUDIO_USER_CONFIG_TV001);
	#ifdef TV001_BOARD
	Audio_HwpSetSIFDataSource(SIF_FROM_SIF_ADC);
	g_AudioSIFInfo.sifSource = RHAL_AUDIO_SIF_INPUT_EXTERNAL;
	Audio_AioSemInit();
	Audio_AppInit();
	#endif
	Audio_AtvInit(&atv_cfg);
	Audio_AtvSetDeviationMethod(ATV_DEV_CHANGE_BY_USER);
	Audio_AtvEnableAutoChangeSoundModeFlag(1);
	alog_info("audiohw init end\n");
	return AIO_OK;
}

static void Audio_SoundStd_To_Hal(sif_standard_ext_type_t *HalSound, ATV_SOUND_STD DriverSound)
{
	switch(DriverSound)
	{
		case ATV_SOUND_STD_MN_MONO:
		{
			break;
		}
		case ATV_SOUND_STD_BTSC:
		{
			*HalSound = SIF_MN_BTSC;
			break;
		}
		case ATV_SOUND_STD_A2_M:
		{
			*HalSound = SIF_MN_A2;
			break;
		}
		case ATV_SOUND_STD_BG_MONO:
		{
			*HalSound = SIF_BG_FM;
			break;
		}
		case ATV_SOUND_STD_A2_BG:
		{
			*HalSound = SIF_BG_A2;
			break;
		}
		case ATV_SOUND_STD_NICAM_BG:
		{
			*HalSound = SIF_BG_NICAM;
			break;
		}
		case ATV_SOUND_STD_DK_MONO:
		{
			*HalSound = SIF_DK_FM;
			break;
		}
		case ATV_SOUND_STD_A2_DK1:
		{
			*HalSound = SIF_DK1_A2;
			break;
		}
		case ATV_SOUND_STD_A2_DK2:
		{
			*HalSound = SIF_DK2_A2;
			break;
		}
		case ATV_SOUND_STD_A2_DK3:
		{
			*HalSound = SIF_DK3_A2;
			break;
		}
		case ATV_SOUND_STD_NICAM_DK:
		{
			*HalSound = SIF_DK_NICAM;
			break;
		}
		case ATV_SOUND_STD_AM_MONO:
		{
			*HalSound = SIF_L_AM;
			break;
		}
		case ATV_SOUND_STD_NICAM_L:
		{
			*HalSound = SIF_L_NICAM;
			break;
		}
		case ATV_SOUND_STD_FM_MONO_NO_I:
		{
			*HalSound = SIF_I_FM;
			break;
		}
		case ATV_SOUND_STD_NICAM_I:
		{
			*HalSound = SIF_I_NICAM;
			break;
		}
		default:
		{
			*HalSound = SIF_STANDARD_DETECT;
			break;
		}
	}
}

void Audio_SIF_SetAudioInHandle(long instanceID)
{
	Audio_AtvSetAudioInHandle(instanceID);
}

void Audio_SIF_SetSubAudioInHandle(long instanceID)
{
	Audio_AtvSetSubAudioInHandle(instanceID);
}

void Audio_SIF_Get_BandCountry(sif_country_ext_type_t *Country)
{
	*Country = g_AudioSIFInfo.curSifType;
}

void Audio_SIF_Get_BandSoundSystem(sif_soundsystem_ext_type_t *SoundSystem)
{
	*SoundSystem = g_AudioSIFInfo.curSifBand;
}

void Audio_SIF_Get_Standard(sif_standard_ext_type_t *Standard)
{
	sif_standard_ext_type_t GetSoundSystem;
	ATV_SOUND_STD HwDetectStd = ATV_SOUND_STD_UNKNOWN;

	Audio_HwpSIFDetectedSoundStandard(&HwDetectStd);
	
	Audio_SoundStd_To_Hal(&GetSoundSystem, HwDetectStd);
	*Standard = GetSoundSystem;
	//alog_info("%s, GetSifStandard=%x \n", __FUNCTION__, *Standard);
	
}

void Audio_SIF_Get_CurAnalogMode(sif_mode_ext_type_t *CurAnalogMode)
{
	*CurAnalogMode = g_AudioSIFInfo.curSifModeGet;
}

void Audio_SIF_Get_UserAnalogMode(sif_mode_user_ext_type_t *UserAnalogMode)
{
	*UserAnalogMode = g_AudioSIFInfo.curSifModeSet;
}

void Audio_SIF_Get_SifExiste(sif_existence_info_ext_type_t *SifExist)
{
	*SifExist = g_AudioSIFInfo.curSifExist;
}

void Audio_SIF_Get_HDev(uint32_t *HDev)
{
	*HDev = g_AudioSIFInfo.bHighDevOnOff;
}

void Audio_Set_SoundStd(ATV_SOUND_STD sound_std)
{
	//#define DEBOUNCE_TIME 2
	
	//ATV_SOUND_STD HwDetectStd = ATV_SOUND_STD_UNKNOWN;
	//ATV_SOUND_STD NewHwDetectStd = ATV_SOUND_STD_UNKNOWN, PreHwDetectStd = ATV_SOUND_STD_UNKNOWN;
	ATV_SOUND_STD_MAIN_SYSTEM main_system = ATV_SOUND_UNKNOWN_SYSTEM;
	//unsigned int SoundStdDebounce = 0, Timeout = 0;

	if (g_AudioSIFInfo.curSifStandard == ATV_SOUND_STD_UNKNOWN)
	{
		Audio_AtvPauseTVStdDetection(true);
		Audio_AtvSetSoundStd(main_system, sound_std);
		g_AudioSIFInfo.curSifStandard = sound_std;
		Audio_AtvPauseTVStdDetection(false);
	}
	#if 0
	while (1)
	{
		audio_hw_usleep(25*1000);
		HwDetectStd = (ATV_SOUND_STD)Audio_GetHWDectectStd();
		//HwDetectStd = (ATV_SOUND_STD)AtvGetHWStd();
		//alog_err("%s, HwDetectStd=%x\n", __FUNCTION__, HwDetectStd);
		if (PreHwDetectStd != HwDetectStd)
		{
			PreHwDetectStd = HwDetectStd;
			SoundStdDebounce = 0;
		}
		else
		{
			if (HwDetectStd != ATV_SOUND_STD_UNKNOWN)
			{
				++ SoundStdDebounce;
			}
			else
			{
				SoundStdDebounce = 0;
			}
		}

		if (SoundStdDebounce >= DEBOUNCE_TIME || Timeout > 45)
		{
			NewHwDetectStd = HwDetectStd;
			break;
		}

		++Timeout;
	}

	if (g_AudioSIFInfo.curSifStandard != NewHwDetectStd)
	{
		Audio_AtvPauseTVStdDetection(true);
		Audio_AtvSetSoundStd(main_system, NewHwDetectStd);
		Audio_AtvPauseTVStdDetection(false);
	}
	
	//audio_hw_usleep(100*1000);
	g_AudioSIFInfo.curSifStandard = NewHwDetectStd;
	//Audio_SoundStd_To_Hal(GetSifStandard, NewHwDetectStd);
	alog_info("%s, NewHwDetectStd=%x, end\n", __FUNCTION__, NewHwDetectStd);
	//return AIO_OK;
	#endif
}

void Audio_SIF_RegMuteCallback(int decindex, pfnMuteHandling pfnCallBack)
{
	Audio_AtvRegMuteCallback(decindex, pfnCallBack);
}

//parameter check
uint32_t AtvIsSoundSystem(sif_soundsystem_ext_type_t std)
{
	uint32_t isStd;

	switch (std) {
	case SIF_SYSTEM_BG:
	case SIF_SYSTEM_I:
	case SIF_SYSTEM_DK:
	case SIF_SYSTEM_L:
	case SIF_SYSTEM_MN:		
		isStd = 1;
		break;
	default :
		isStd = 0;
		break;
	}
	return isStd;
}

uint32_t AtvIsSifCountry(sif_country_ext_type_t sif_country)
{
	uint32_t isStd;

	switch (sif_country) {
	case SIF_ATSC_SELECT:
	case SIF_KOREA_A2_SELECT:
	case SIF_BTSC_SELECT:
	case SIF_BTSC_BR_SELECT:
	case SIF_BTSC_US_SELECT:
	case SIF_DVB_SELECT:		
	case SIF_DVB_ID_SELECT:
	case SIF_DVB_IN_SELECT:
	case SIF_DVB_CN_SELECT:
	case SIF_DVB_AJJA_SELECT:
		isStd = 1;
		break;
	default :
		isStd = 0;
		break;
	}
	return isStd;
}

uint32_t AtvIsSifMode(sif_mode_user_ext_type_t sif_mode)
{
	uint32_t isStd;

	switch (sif_mode) {
	case SIF_USER_PAL_UNKNOWN:
	case SIF_USER_PAL_MONO:
	case SIF_USER_PAL_MONO_FORCED:
	case SIF_USER_PAL_STEREO:
	case SIF_USER_PAL_STEREO_FORCED:
	case SIF_USER_PAL_DUALI:
	case SIF_USER_PAL_DUALII:		
	case SIF_USER_PAL_DUALI_II:
	case SIF_USER_PAL_NICAM_MONO:
	case SIF_USER_PAL_NICAM_MONO_FORCED:
	case SIF_USER_PAL_NICAM_STEREO:
	case SIF_USER_PAL_NICAM_STEREO_FORCED:
	case SIF_USER_PAL_NICAM_DUALI:
	case SIF_USER_PAL_NICAM_DUALII:
	case SIF_USER_PAL_NICAM_DUALI_II:
	case SIF_USER_PAL_NICAM_DUAL_FORCED:
	case SIF_USER_NTSC_A2_UNKNOWN:		
	case SIF_USER_NTSC_A2_MONO:
	case SIF_USER_NTSC_A2_STEREO:
	case SIF_USER_NTSC_A2_SAP:
	case SIF_USER_NTSC_BTSC_UNKNOWN:
	case SIF_USER_NTSC_BTSC_MONO:		
	case SIF_USER_NTSC_BTSC_STEREO:
	case SIF_USER_NTSC_BTSC_SAP_MONO:
	case SIF_USER_NTSC_BTSC_SAP_STEREO:	
		isStd = 1;
		break;
	default :
		isStd = 0;
		break;
	}
	return isStd;
}
