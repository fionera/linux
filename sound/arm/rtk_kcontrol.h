#ifndef _RTK_KCONTROL_
#define _RTK_KCONTROL_

#include <linux/wait.h>
#include <sound/control.h>
#include <sound/asound.h>
#include <linux/string.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include "rtk_kdriver/hal_inc/hal_audio.h"

//#define UINT32 unsigned int

#define ADEC_MAX_CAP   2
#define AMIXER_MAX_CAP 8
#define DLY_OUTPUT_BASE 24

#define ADEC_TABLE_MAX (4)
#define AMIXER_TABLE_MAX (8)
#define GAIN_INPUT_TABLE_MAX (24)
#define DATA_MAX_NUM (128)
#define GAIN_MAX_VALUE (0xFFFFFFF) //for socts:268435455; 30dB = 265271077

#define ALSA_DEBUG_INFO pr_debug
#define ALSA_INFO(format, ...) pr_info("[ADSP] " format, ##__VA_ARGS__)
#define ALSA_WARN pr_warn

int snd_mars_ctl_adec_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo);
int snd_mars_ctl_adec_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int snd_mars_ctl_adec_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);

#define MARS_CTL_ADEC(xname, xindex, addr)\
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,\
	.name = xname,\
	.index = xindex,\
	.info = snd_mars_ctl_adec_info,\
	.get = snd_mars_ctl_adec_get,\
	.put = snd_mars_ctl_adec_put,\
	.private_value = addr }

int snd_mars_ctl_sndout_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo);
int snd_mars_ctl_sndout_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int snd_mars_ctl_sndout_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);

#define MARS_CTL_SNDOUT(xname, xindex, addr)\
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,\
	.name = xname,\
	.index = xindex,\
	.info = snd_mars_ctl_sndout_info,\
	.get = snd_mars_ctl_sndout_get,\
	.put = snd_mars_ctl_sndout_put,\
	.private_value = addr }

int snd_mars_ctl_mute_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo);
int snd_mars_ctl_mute_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int snd_mars_ctl_mute_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);

#define MARS_CTL_MUTE(xname, xindex, addr)\
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,\
	.name = xname,\
	.index = xindex,\
	.info = snd_mars_ctl_mute_info,\
	.get = snd_mars_ctl_mute_get,\
	.put = snd_mars_ctl_mute_put,\
	.private_value = addr }

int snd_mars_ctl_gain_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo);
int snd_mars_ctl_gain_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int snd_mars_ctl_gain_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);

#define MARS_CTL_GAIN(xname, xindex, addr)\
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,\
	.name = xname,\
	.index = xindex,\
	.info = snd_mars_ctl_gain_info,\
	.get = snd_mars_ctl_gain_get,\
	.put = snd_mars_ctl_gain_put,\
	.private_value = addr }

int snd_mars_ctl_delay_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo);
int snd_mars_ctl_delay_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int snd_mars_ctl_delay_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);

#define MARS_CTL_DELAY(xname, xindex, addr)\
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,\
	.name = xname,\
	.index = xindex,\
	.info = snd_mars_ctl_delay_info,\
	.get = snd_mars_ctl_delay_get,\
	.put = snd_mars_ctl_delay_put,\
	.private_value = addr }

int snd_mars_ctl_lgse_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo);
int snd_mars_ctl_lgse_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int snd_mars_ctl_lgse_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);

#define MARS_CTL_LGSE(xname, xindex, addr)\
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,\
	.name = xname,\
	.index = xindex,\
	.info = snd_mars_ctl_lgse_info,\
	.get = snd_mars_ctl_lgse_get,\
	.put = snd_mars_ctl_lgse_put,\
	.private_value = addr }

int snd_mars_ctl_hdmi_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo);
int snd_mars_ctl_hdmi_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int snd_mars_ctl_hdmi_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);

#define MARS_CTL_HDMI(xname, xindex, addr)\
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,\
	.name = xname,\
	.index = xindex,\
	.info = snd_mars_ctl_hdmi_info,\
	.get = snd_mars_ctl_hdmi_get,\
	.put = snd_mars_ctl_hdmi_put,\
	.private_value = addr }

int snd_mars_ctl_direct_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo);
int snd_mars_ctl_direct_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int snd_mars_ctl_direct_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);

#define MARS_CTL_DIRECT(xname, xindex, addr)\
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,\
	.name = xname,\
	.index = xindex,\
	.info = snd_mars_ctl_direct_info,\
	.get = snd_mars_ctl_direct_get,\
	.put = snd_mars_ctl_direct_put,\
	.private_value = addr }

int snd_mars_ctl_sif_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo);
int snd_mars_ctl_sif_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int snd_mars_ctl_sif_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);

#define MARS_CTL_SIF(xname, xindex, addr)\
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,\
	.name = xname,\
	.index = xindex,\
	.info = snd_mars_ctl_sif_info,\
	.get = snd_mars_ctl_sif_get,\
	.put = snd_mars_ctl_sif_put,\
	.private_value = addr }

#define MARS_VOLUME(xname, xindex, addr)\
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,\
	.name = xname,\
	.index = xindex,\
	.info = snd_mars_volume_info,\
	.get = snd_mars_volume_get,\
	.put = snd_mars_volume_put,\
	.private_value = addr }

int snd_mars_ctl_aenc_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo);
int snd_mars_ctl_aenc_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int snd_mars_ctl_aenc_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);

#define MARS_CTL_AENC(xname, xindex, addr)\
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,\
	.name = xname,\
	.index = xindex,\
	.info = snd_mars_ctl_aenc_info,\
	.get = snd_mars_ctl_aenc_get,\
	.put = snd_mars_ctl_aenc_put,\
	.private_value = addr }

int snd_mars_ctl_adc_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo);
int snd_mars_ctl_adc_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int snd_mars_ctl_adc_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);

#define MARS_CTL_ADC(xname, xindex, addr)\
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,\
	.name = xname,\
	.index = xindex,\
	.info = snd_mars_ctl_adc_info,\
	.get = snd_mars_ctl_adc_get,\
	.put = snd_mars_ctl_adc_put,\
	.private_value = addr }

typedef struct {
    unsigned int open_status[ADEC_TABLE_MAX];
    unsigned int close_status[ADEC_TABLE_MAX];
    unsigned int start_status[ADEC_TABLE_MAX];
    unsigned int stop_status[ADEC_TABLE_MAX];
    unsigned int src_type[ADEC_TABLE_MAX];
    unsigned int codec_type[ADEC_TABLE_MAX];
    unsigned int eTrickMode[ADEC_TABLE_MAX];
    unsigned int drcMode[ADEC_TABLE_MAX];
    unsigned int downmixMode[ADEC_TABLE_MAX];
    unsigned int firstLang[ADEC_TABLE_MAX];
    unsigned int secondLang[ADEC_TABLE_MAX];
    unsigned int dialEnhanceGain[ADEC_TABLE_MAX];
    unsigned int bOnOff_sync_mode[ADEC_TABLE_MAX];
    unsigned int bIsEnable[ADEC_TABLE_MAX];
    adec_ac4_ad_ext_type_t ac4_ad_type[ADEC_TABLE_MAX];
    adec_ac4_lang_code_ext_type_t ac4_lang_code_type[ADEC_TABLE_MAX];
    adec_dualmono_mode_ext_type_t dualmono_mode[ADEC_TABLE_MAX];
    unsigned int adec_ac4_pres_group_idx[ADEC_TABLE_MAX];
    unsigned int bOnOff_AC4_ADMixing[ADEC_TABLE_MAX];
    unsigned int user_adec_max_cap;
    unsigned int user_amixer_max_cap;
    unsigned int bOnOff_AD;
    unsigned int AD_adecIndex;
    unsigned int bIsOTTEnable;
    unsigned int bIsATMOSLockingEnable;
    unsigned int b_set_codec;
} ALSA_ADEC_CMD_INFO;

typedef struct {
    unsigned int output_src;
    unsigned int opened_device;
    unsigned int input_type;
    unsigned int input_port;
    unsigned int i2sNumber;
    unsigned int bOnOff_optic;
    unsigned int bOnOff_arc;
    unsigned int num_of_connected;
    unsigned int connected_all_outputs;
    unsigned int connected_device[9];
    int optic_category_code;
    int arc_category_code;
    unsigned int checksum;
    unsigned int main_audio_port;
    sndout_optic_mode_ext_type_t           optic_mode;
    sndout_arc_mode_ext_type_t             arc_mode;
    sndout_optic_copyprotection_ext_type_t optic_copyprotection;
    sndout_arc_copyprotection_ext_type_t   arc_copyprotection;
    HAL_AUDIO_SB_SET_INFO_T                soundbar_info;//soundbar_info;
    int capture_disable;
    unsigned int bOnOff_earc;
    ALSA_SNDOUT_EARC_INFO earc_info;
    unsigned int bOnOff_eac3_atmos_enc;
} ALSA_SNDOUT_CMD_INFO;

typedef struct {
    unsigned int dly_status[GAIN_INPUT_TABLE_MAX+INDEX_SNDOUT_MAX];
} ALSA_DELAY_CMD_INFO;

typedef struct {
    unsigned int input_gain[GAIN_INPUT_TABLE_MAX];
    unsigned int output_gain[INDEX_SNDOUT_MAX];
    unsigned int ad_gain;
} ALSA_GAIN_CMD_INFO;

typedef struct {
    unsigned int flag;
    lgse_se_mode_ext_type_t mode;
    unsigned int se_init;
    unsigned int bOnOff_vx_st;
    int procOutGain;
    int bEnable_CertParam;
    lgse_postproc_mode_ext_type_t          postgain;
    lgse_dap_surround_virtualizer_mode_t   virtualizer_mode;
    DAP_PARAM_DIALOGUE_ENHANCER            DialogueEnhancer;
    DAP_PARAM_VOLUME_LEVELER               VolumeLeveler;
    DAP_PARAM_VOLUME_MODELER               VolumeModeler;
    int                                    volmax_boost;
    DAP_PARAM_AUDIO_OPTIMIZER              Optimizer;
    HAL_AUDIO_LGSE_DAP_PROCESS_OPTIMIZER_T Process_Optimizer;
    int                                    surround_decoder_enable;
    int                                    surround_boost;
    DAP_PARAM_INTELLIGENT_EQUALIZER        Intelligence_eq;
    DAP_PARAM_MEDIA_INTELLIGENCE           MediaIntelligence;
    DAP_PARAM_GRAPHICAL_EQUALIZER          GraphicalEQ;
    DAP_PARAM_BASS_ENHANCER                BassEnhancer;
    HAL_AUDIO_LGSE_DAP_BASS_EXTRACTION_T   BassExtraction;
    DAP_PARAM_VIRTUAL_BASS                 VirtualBass;
    DAP_PARAM_AUDIO_REGULATOR              Regulator;
    DAP_PARAM_VIRTUALIZER_SPEAKER_ANGLE    Speaker_angle;
    lgse_dap_perceptual_height_filter_mode_ext_type_t height_filter_mode;
    AUDIO_RPC_LGSE00861 lgse00861; // init main
    AUDIO_RPC_LGSE00876 lgse00876; // var  main
    AUDIO_RPC_LGSE00855 lgse00855; // init fn000
    AUDIO_RPC_LGSE00870 lgse00870; // var  fn000
    AUDIO_RPC_LGSE00854 lgse00854; // init fn001
    AUDIO_RPC_LGSE00869 lgse00869; // var  fn001
    AUDIO_RPC_LGSE_SETFN004 fn004;
    AUDIO_RPC_LGSE_SETFN005 fn005;
    AUDIO_RPC_LGSE00858 lgse00858; // init fn008
    AUDIO_RPC_LGSE00873 lgse00873; // var  fn008
    AUDIO_RPC_LGSE00857 lgse00857; // init fn009
    AUDIO_RPC_LGSE00872 lgse00872; // var  fn009
    AUDIO_RPC_LGSE00859 lgse00859; // init fn010
    AUDIO_RPC_LGSE00874 lgse00874; // var  fn010
    AUDIO_RPC_LGSE00853 lgse00853; // init fn014
    AUDIO_RPC_LGSE00868 lgse00868; // var  fn014
    AUDIO_RPC_LGSE03520 lgse03520; // init fn016
    AUDIO_RPC_LGSE03521 lgse03521; // var  fn016
    AUDIO_RPC_LGSE00879 lgse00879; // var  fn017
    AUDIO_RPC_LGSE00856 lgse00856; // init fn019
    AUDIO_RPC_LGSE00871 lgse00871; // var  fn019
    AUDIO_RPC_LGSE00863 lgse00863; // init fn022
    AUDIO_RPC_LGSE00878 lgse00878; // var  fn022
    AUDIO_RPC_LGSE02624 lgse02624; // init fn024
    AUDIO_RPC_LGSE_DATA026  fn026; // var  fn026
    AUDIO_RPC_LGSE02871 lgse02871; // var  fn028
    AUDIO_RPC_LGSE02869 lgse02869; // var  fn029
    int fn023[188]; // init fn023, not use, only for socts
} ALSA_LGSE_CMD_INFO;

typedef struct {
    sif_soundsystem_ext_type_t sound_system;
    sif_soundsystem_ext_type_t get_sound_system;
    sif_country_ext_type_t     sif_country;
    sif_country_ext_type_t     get_sif_country;
    sif_standard_ext_type_t    sif_standard;
    sif_standard_ext_type_t    get_sif_standard;
    sif_mode_user_ext_type_t   user_analog_mode;
    sif_mode_user_ext_type_t   get_user_analog_mode;
    int hdev_onoff;
    int get_hdev_onoff;
    int a2thresholdlevel;
    int get_a2thresholdlevel;
    int open_status;
    int close_status;
    int connect_status;
    int disconnect_status;
    unsigned int SignalQuality;
} ALSA_SIF_CMD_INFO;

typedef struct {
    int decoder_idx;
    int codec;
    int bitrate;
    HAL_AUDIO_VOLUME_T volume;
    unsigned int gain;
} ALSA_AENC_CMD_INFO;

typedef struct {
	int adecport;
	int codecType;
	int samplingFreq;
	int numbOfChannel;
	int bitPerSample;
} DIRECT_CODEC_INFO;

typedef struct {
    int open_status;
    int close_status;
    int connected_port;
    int disconnect_status;
} ALSA_ADC_CMD_INFO;

enum MIXER_ADDR {
	MIXER_ADDR_0,
	MIXER_ADDR_1,
	MIXER_ADDR_2,
	MIXER_ADDR_3,
	MIXER_ADDR_4,
	MIXER_ADDR_5,
	MIXER_ADDR_6,
	MIXER_ADDR_7,
	MIXER_ADDR_MAX
};

enum MIXERC_ADDR_ADEC {
	MIXER_ADDR_ADEC_0,
	MIXER_ADDR_ADEC_1,
	MIXER_ADDR_ADEC_2,
	MIXER_ADDR_ADEC_3,
	MIXER_ADDR_ADEC_4,
	MIXER_ADDR_ADEC_5,
	MIXER_ADDR_ADEC_6,
	MIXER_ADDR_ADEC_7,
	MIXER_ADDR_ADEC_8,
	MIXER_ADDR_ADEC_9,
	MIXER_ADDR_ADEC_10,
	MIXER_ADDR_ADEC_11,
	MIXER_ADDR_ADEC_12,
	MIXER_ADDR_ADEC_13,
	MIXER_ADDR_ADEC_14,
	MIXER_ADDR_ADEC_15,
	MIXER_ADDR_ADEC_16,
	MIXER_ADDR_ADEC_17,
	MIXER_ADDR_ADEC_18,
	MIXER_ADDR_ADEC_19,
	MIXER_ADDR_ADEC_20,
	MIXER_ADDR_ADEC_21,
	MIXER_ADDR_ADEC_22,
	MIXER_ADDR_ADEC_23,
	MIXER_ADDR_ADEC_24,
	MIXER_ADDR_ADEC_25,
	MIXER_ADDR_ADEC_26,
	MIXER_ADDR_ADEC_27,
	MIXER_ADDR_ADEC_28,
	MIXER_ADDR_ADEC_29,
	MIXER_ADDR_ADEC_30,
	MIXER_ADDR_ADEC_31,
	MIXER_ADDR_ADEC_32,
	MIXER_ADDR_ADEC_33,
	MIXER_ADDR_ADEC_34,
	MIXER_ADDR_ADEC_MAX
};

enum MIXER_ADDR_SNDOUT {
	MIXER_ADDR_SNDOUT_0,
	MIXER_ADDR_SNDOUT_1,
	MIXER_ADDR_SNDOUT_2,
	MIXER_ADDR_SNDOUT_3,
	MIXER_ADDR_SNDOUT_4,
	MIXER_ADDR_SNDOUT_5,
	MIXER_ADDR_SNDOUT_6,
	MIXER_ADDR_SNDOUT_7,
	MIXER_ADDR_SNDOUT_8,
	MIXER_ADDR_SNDOUT_9,
    MIXER_ADDR_SNDOUT_10,
    MIXER_ADDR_SNDOUT_11,
    MIXER_ADDR_SNDOUT_12,
    MIXER_ADDR_SNDOUT_13,
    MIXER_ADDR_SNDOUT_14,
    MIXER_ADDR_SNDOUT_15,
    MIXER_ADDR_SNDOUT_16,
    MIXER_ADDR_SNDOUT_17,
    MIXER_ADDR_SNDOUT_MAX
};

enum MIXER_ADDR_MUTE {
    MIXER_ADDR_MUTE_0,
    MIXER_ADDR_MUTE_1,
    MIXER_ADDR_MUTE_MAX
};

enum MIXER_ADDR_GAIN {
    MIXER_ADDR_GAIN_0,
    MIXER_ADDR_GAIN_1,
    MIXER_ADDR_GAIN_2,
    MIXER_ADDR_GAIN_MAX
};

enum MIXER_ADDR_DELAY {
    MIXER_ADDR_DELAY_0,
    MIXER_ADDR_DELAY_MAX
};

enum MIXER_ADDR_LGSE_0 {
	MIXER_ADDR_LGSE_0_0,
	MIXER_ADDR_LGSE_0_1,
	MIXER_ADDR_LGSE_0_2,
	MIXER_ADDR_LGSE_0_3,
	MIXER_ADDR_LGSE_0_4,
	MIXER_ADDR_LGSE_0_5,
	MIXER_ADDR_LGSE_0_6,
	MIXER_ADDR_LGSE_0_7,
	MIXER_ADDR_LGSE_0_8,
	MIXER_ADDR_LGSE_0_9,
	MIXER_ADDR_LGSE_0_10,
	MIXER_ADDR_LGSE_0_11,
	MIXER_ADDR_LGSE_0_12,
	MIXER_ADDR_LGSE_0_13,
	MIXER_ADDR_LGSE_0_14,
	MIXER_ADDR_LGSE_0_15,
	MIXER_ADDR_LGSE_0_16,
	MIXER_ADDR_LGSE_0_17,
	MIXER_ADDR_LGSE_0_18,
	MIXER_ADDR_LGSE_0_19,
	MIXER_ADDR_LGSE_0_20,
	MIXER_ADDR_LGSE_0_21,
	MIXER_ADDR_LGSE_0_22,
	MIXER_ADDR_LGSE_0_23,
	MIXER_ADDR_LGSE_0_24,
	MIXER_ADDR_LGSE_0_25,
	MIXER_ADDR_LGSE_0_26,
	MIXER_ADDR_LGSE_0_27,
	MIXER_ADDR_LGSE_0_28,
	MIXER_ADDR_LGSE_0_29,
	MIXER_ADDR_LGSE_0_30,
	MIXER_ADDR_LGSE_0_31,
	MIXER_ADDR_LGSE_0_32,
	MIXER_ADDR_LGSE_0_33,
	MIXER_ADDR_LGSE_0_34,
	MIXER_ADDR_LGSE_0_35,
	MIXER_ADDR_LGSE_0_36,
	MIXER_ADDR_LGSE_0_37,
	MIXER_ADDR_LGSE_0_38,
	MIXER_ADDR_LGSE_0_39,
	MIXER_ADDR_LGSE_0_40,
	MIXER_ADDR_LGSE_0_41,
	MIXER_ADDR_LGSE_0_42,
	MIXER_ADDR_LGSE_0_43,
	MIXER_ADDR_LGSE_0_44,
	MIXER_ADDR_LGSE_0_45,
	MIXER_ADDR_LGSE_0_46,
	MIXER_ADDR_LGSE_0_47,
	MIXER_ADDR_LGSE_0_48,
	MIXER_ADDR_LGSE_0_49,
	MIXER_ADDR_LGSE_0_50,
	MIXER_ADDR_LGSE_0_51,
	MIXER_ADDR_LGSE_0_52,
	MIXER_ADDR_LGSE_0_53,
	MIXER_ADDR_LGSE_0_54,
	MIXER_ADDR_LGSE_0_55,
	MIXER_ADDR_LGSE_0_56,
	MIXER_ADDR_LGSE_0_57,
	MIXER_ADDR_LGSE_0_58,
	MIXER_ADDR_LGSE_0_59,
	MIXER_ADDR_LGSE_0_60,
	MIXER_ADDR_LGSE_0_61,
	MIXER_ADDR_LGSE_0_62,
	MIXER_ADDR_LGSE_0_63,
	MIXER_ADDR_LGSE_0_64,
	MIXER_ADDR_LGSE_0_65,
	MIXER_ADDR_LGSE_0_66,
	MIXER_ADDR_LGSE_0_67,
	MIXER_ADDR_LGSE_0_68,
	MIXER_ADDR_LGSE_0_69,
	MIXER_ADDR_LGSE_0_70,
	MIXER_ADDR_LGSE_0_71,
	MIXER_ADDR_LGSE_0_72,
	MIXER_ADDR_LGSE_0_73,
	MIXER_ADDR_LGSE_0_74,
	MIXER_ADDR_LGSE_0_75,
	MIXER_ADDR_LGSE_0_76,
	MIXER_ADDR_LGSE_0_77,
	MIXER_ADDR_LGSE_0_78,
	MIXER_ADDR_LGSE_0_79,
	MIXER_ADDR_LGSE_0_80,
	MIXER_ADDR_LGSE_0_81,
	MIXER_ADDR_LGSE_0_82,
	MIXER_ADDR_LGSE_0_83,
	MIXER_ADDR_LGSE_0_84,
	MIXER_ADDR_LGSE_0_85,
	MIXER_ADDR_LGSE_0_86,
	MIXER_ADDR_LGSE_0_87,
	MIXER_ADDR_LGSE_0_88,
	MIXER_ADDR_LGSE_0_89,
	MIXER_ADDR_LGSE_0_90,
    MIXER_ADDR_LGSE_0_MAX
};

enum MIXER_ADDR_LGSE_1 {
	MIXER_ADDR_LGSE_1_0,
	MIXER_ADDR_LGSE_1_1,
	MIXER_ADDR_LGSE_1_2,
	MIXER_ADDR_LGSE_1_3,
	MIXER_ADDR_LGSE_1_4,
	MIXER_ADDR_LGSE_1_5,
	MIXER_ADDR_LGSE_1_6,
	MIXER_ADDR_LGSE_1_7,
	MIXER_ADDR_LGSE_1_8,
	MIXER_ADDR_LGSE_1_9,
	MIXER_ADDR_LGSE_1_10,
	MIXER_ADDR_LGSE_1_11,
	MIXER_ADDR_LGSE_1_12,
	MIXER_ADDR_LGSE_1_13,
	MIXER_ADDR_LGSE_1_14,
	MIXER_ADDR_LGSE_1_15,
	MIXER_ADDR_LGSE_1_16,
	MIXER_ADDR_LGSE_1_17,
	MIXER_ADDR_LGSE_1_18,
	MIXER_ADDR_LGSE_1_19,
	MIXER_ADDR_LGSE_1_20,
	MIXER_ADDR_LGSE_1_21,
	MIXER_ADDR_LGSE_1_22,
	MIXER_ADDR_LGSE_1_23,
	MIXER_ADDR_LGSE_1_24,
	MIXER_ADDR_LGSE_1_25,
	MIXER_ADDR_LGSE_1_26,
	MIXER_ADDR_LGSE_1_27,
	MIXER_ADDR_LGSE_1_28,
	MIXER_ADDR_LGSE_1_29,
	MIXER_ADDR_LGSE_1_30,
	MIXER_ADDR_LGSE_1_31,
	MIXER_ADDR_LGSE_1_32,
	MIXER_ADDR_LGSE_1_33,
	MIXER_ADDR_LGSE_1_34,
	MIXER_ADDR_LGSE_1_35,
	MIXER_ADDR_LGSE_1_36,
	MIXER_ADDR_LGSE_1_37,
	MIXER_ADDR_LGSE_1_38,
	MIXER_ADDR_LGSE_1_39,
	MIXER_ADDR_LGSE_1_40,
	MIXER_ADDR_LGSE_1_41,
	MIXER_ADDR_LGSE_1_42,
	MIXER_ADDR_LGSE_1_43,
	MIXER_ADDR_LGSE_1_44,
	MIXER_ADDR_LGSE_1_45,
	MIXER_ADDR_LGSE_1_46,
	MIXER_ADDR_LGSE_1_47,
	MIXER_ADDR_LGSE_1_48,
	MIXER_ADDR_LGSE_1_49,
	MIXER_ADDR_LGSE_1_50,
	MIXER_ADDR_LGSE_1_51,
	MIXER_ADDR_LGSE_1_52,
	MIXER_ADDR_LGSE_1_53,
	MIXER_ADDR_LGSE_1_54,
	MIXER_ADDR_LGSE_1_55,
	MIXER_ADDR_LGSE_1_56,
	MIXER_ADDR_LGSE_1_57,
	MIXER_ADDR_LGSE_1_58,
	MIXER_ADDR_LGSE_1_59,
	MIXER_ADDR_LGSE_1_60,
	MIXER_ADDR_LGSE_1_61,
	MIXER_ADDR_LGSE_1_62,
	MIXER_ADDR_LGSE_1_63,
	MIXER_ADDR_LGSE_1_64,
	MIXER_ADDR_LGSE_1_65,
	MIXER_ADDR_LGSE_1_66,
	MIXER_ADDR_LGSE_1_67,
	MIXER_ADDR_LGSE_1_68,
	MIXER_ADDR_LGSE_1_69,
	MIXER_ADDR_LGSE_1_70,
	MIXER_ADDR_LGSE_1_71,
	MIXER_ADDR_LGSE_1_72,
	MIXER_ADDR_LGSE_1_73,
	MIXER_ADDR_LGSE_1_74,
	MIXER_ADDR_LGSE_1_75,
	MIXER_ADDR_LGSE_1_76,
	MIXER_ADDR_LGSE_1_77,
	MIXER_ADDR_LGSE_1_78,
	MIXER_ADDR_LGSE_1_79,
	MIXER_ADDR_LGSE_1_80,
	MIXER_ADDR_LGSE_1_81,
	MIXER_ADDR_LGSE_1_82,
	MIXER_ADDR_LGSE_1_83,
	MIXER_ADDR_LGSE_1_84,
	MIXER_ADDR_LGSE_1_85,
	MIXER_ADDR_LGSE_1_86,
	MIXER_ADDR_LGSE_1_87,
	MIXER_ADDR_LGSE_1_88,
	MIXER_ADDR_LGSE_1_89,
	MIXER_ADDR_LGSE_1_90,
    MIXER_ADDR_LGSE_1_MAX
};

enum MIXER_ADDR_LGSE_2 {
	MIXER_ADDR_LGSE_2_0,
	MIXER_ADDR_LGSE_2_1,
	MIXER_ADDR_LGSE_2_2,
	MIXER_ADDR_LGSE_2_3,
	MIXER_ADDR_LGSE_2_4,
	MIXER_ADDR_LGSE_2_5,
	MIXER_ADDR_LGSE_2_6,
	MIXER_ADDR_LGSE_2_7,
	MIXER_ADDR_LGSE_2_8,
	MIXER_ADDR_LGSE_2_9,
	MIXER_ADDR_LGSE_2_10,
	MIXER_ADDR_LGSE_2_11,
	MIXER_ADDR_LGSE_2_12,
	MIXER_ADDR_LGSE_2_13,
	MIXER_ADDR_LGSE_2_14,
	MIXER_ADDR_LGSE_2_15,
	MIXER_ADDR_LGSE_2_16,
	MIXER_ADDR_LGSE_2_17,
	MIXER_ADDR_LGSE_2_18,
	MIXER_ADDR_LGSE_2_19,
	MIXER_ADDR_LGSE_2_20,
	MIXER_ADDR_LGSE_2_21,
	MIXER_ADDR_LGSE_2_22,
	MIXER_ADDR_LGSE_2_23,
	MIXER_ADDR_LGSE_2_24,
	MIXER_ADDR_LGSE_2_25,
	MIXER_ADDR_LGSE_2_26,
	MIXER_ADDR_LGSE_2_27,
	MIXER_ADDR_LGSE_2_28,
	MIXER_ADDR_LGSE_2_29,
	MIXER_ADDR_LGSE_2_30,
	MIXER_ADDR_LGSE_2_31,
	MIXER_ADDR_LGSE_2_32,
	MIXER_ADDR_LGSE_2_33,
	MIXER_ADDR_LGSE_2_34,
	MIXER_ADDR_LGSE_2_35,
	MIXER_ADDR_LGSE_2_36,
	MIXER_ADDR_LGSE_2_37,
	MIXER_ADDR_LGSE_2_38,
	MIXER_ADDR_LGSE_2_39,
	MIXER_ADDR_LGSE_2_40,
	MIXER_ADDR_LGSE_2_41,
	MIXER_ADDR_LGSE_2_42,
	MIXER_ADDR_LGSE_2_43,
	MIXER_ADDR_LGSE_2_44,
	MIXER_ADDR_LGSE_2_45,
	MIXER_ADDR_LGSE_2_46,
	MIXER_ADDR_LGSE_2_47,
	MIXER_ADDR_LGSE_2_48,
	MIXER_ADDR_LGSE_2_49,
	MIXER_ADDR_LGSE_2_50,
	MIXER_ADDR_LGSE_2_51,
	MIXER_ADDR_LGSE_2_52,
	MIXER_ADDR_LGSE_2_53,
	MIXER_ADDR_LGSE_2_54,
	MIXER_ADDR_LGSE_2_55,
	MIXER_ADDR_LGSE_2_56,
	MIXER_ADDR_LGSE_2_57,
	MIXER_ADDR_LGSE_2_58,
	MIXER_ADDR_LGSE_2_59,
	MIXER_ADDR_LGSE_2_60,
	MIXER_ADDR_LGSE_2_61,
	MIXER_ADDR_LGSE_2_62,
	MIXER_ADDR_LGSE_2_63,
	MIXER_ADDR_LGSE_2_64,
	MIXER_ADDR_LGSE_2_65,
	MIXER_ADDR_LGSE_2_66,
	MIXER_ADDR_LGSE_2_67,
	MIXER_ADDR_LGSE_2_68,
	MIXER_ADDR_LGSE_2_69,
	MIXER_ADDR_LGSE_2_70,
	MIXER_ADDR_LGSE_2_71,
	MIXER_ADDR_LGSE_2_72,
	MIXER_ADDR_LGSE_2_73,
	MIXER_ADDR_LGSE_2_74,
	MIXER_ADDR_LGSE_2_75,
	MIXER_ADDR_LGSE_2_76,
	MIXER_ADDR_LGSE_2_77,
	MIXER_ADDR_LGSE_2_78,
	MIXER_ADDR_LGSE_2_79,
	MIXER_ADDR_LGSE_2_80,
	MIXER_ADDR_LGSE_2_81,
	MIXER_ADDR_LGSE_2_82,
	MIXER_ADDR_LGSE_2_83,
	MIXER_ADDR_LGSE_2_84,
	MIXER_ADDR_LGSE_2_85,
	MIXER_ADDR_LGSE_2_86,
	MIXER_ADDR_LGSE_2_87,
	MIXER_ADDR_LGSE_2_88,
	MIXER_ADDR_LGSE_2_89,
	MIXER_ADDR_LGSE_2_90,
    MIXER_ADDR_LGSE_2_MAX
};

enum MIXER_ADDR_HDMI {
    MIXER_ADDR_HDMI_0,
    MIXER_ADDR_HDMI_1,
    MIXER_ADDR_HDMI_2,
    MIXER_ADDR_HDMI_3,
    MIXER_ADDR_HDMI_4,
    MIXER_ADDR_HDMI_5,
    MIXER_ADDR_HDMI_6,
    MIXER_ADDR_HDMI_7,
    MIXER_ADDR_HDMI_MAX
};

enum MIXER_ADDR_DIRECT {
	MIXER_ADDR_DIRECT_0,
	MIXER_ADDR_DIRECT_MAX
};

enum MIXER_ADDR_SIF {
    MIXER_ADDR_SIF_0,
    MIXER_ADDR_SIF_1,
    MIXER_ADDR_SIF_2,
    MIXER_ADDR_SIF_3,
    MIXER_ADDR_SIF_4,
    MIXER_ADDR_SIF_5,
    MIXER_ADDR_SIF_6,
    MIXER_ADDR_SIF_7,
    MIXER_ADDR_SIF_8,
    MIXER_ADDR_SIF_9,
    MIXER_ADDR_SIF_10,
    MIXER_ADDR_SIF_11,
    MIXER_ADDR_SIF_12,	
    MIXER_ADDR_SIF_MAX
};

enum MIXER_ADDR_AENC {
	MIXER_ADDR_AENC_0,
	MIXER_ADDR_AENC_1,
	MIXER_ADDR_AENC_2,
	MIXER_ADDR_AENC_MAX
};

enum MIXER_ADDR_ADC {
	MIXER_ADDR_ADC_0,
	MIXER_ADDR_ADC_1,
	MIXER_ADDR_ADC_2,
	MIXER_ADDR_ADC_3,
	MIXER_ADDR_ADC_MAX
};

struct snd_card_mars {
	struct snd_card *card;
	spinlock_t mixer_lock;
	struct work_struct work_volume;
	struct mutex rpc_lock;
	int ao_pin_id[MIXER_ADDR_MAX];
	int ao_flash_volume[MIXER_ADDR_MAX];
	int ao_flash_change[MIXER_ADDR_MAX];
};

static struct snd_kcontrol_new snd_mars_adec_controls[] = {
	MARS_CTL_ADEC(ADEC_OPEN,                       0, MIXER_ADDR_ADEC_0),
	MARS_CTL_ADEC(ADEC_CLOSE,                      0, MIXER_ADDR_ADEC_1),
	MARS_CTL_ADEC(ADEC_CONNECT,                    0, MIXER_ADDR_ADEC_2),
	MARS_CTL_ADEC(ADEC_DISCONNECT,                 0, MIXER_ADDR_ADEC_3),
	MARS_CTL_ADEC(ADEC_CODEC,                      0, MIXER_ADDR_ADEC_4),
	MARS_CTL_ADEC(ADEC_START,                      0, MIXER_ADDR_ADEC_5),
	MARS_CTL_ADEC(ADEC_STOP,                       0, MIXER_ADDR_ADEC_6),
	MARS_CTL_ADEC(ADEC_MAXCAPACITY,                0, MIXER_ADDR_ADEC_7),
	MARS_CTL_ADEC(ADEC_USERMAXCAPACITY,            0, MIXER_ADDR_ADEC_8),
	MARS_CTL_ADEC(ADEC_TP_DECODER_OUTPUTMODE,      0, MIXER_ADDR_ADEC_9),
	MARS_CTL_ADEC(ADEC_SYNCMODE,                   0, MIXER_ADDR_ADEC_10),
	MARS_CTL_ADEC(ADEC_DOLBYDRCMODE,               0, MIXER_ADDR_ADEC_11),
	MARS_CTL_ADEC(ADEC_DOWNMIXMODE,                0, MIXER_ADDR_ADEC_12),
	MARS_CTL_ADEC(ADEC_TRICKMODE,                  0, MIXER_ADDR_ADEC_13),
	MARS_CTL_ADEC(ADEC0_INFO,                      0, MIXER_ADDR_ADEC_14),
	MARS_CTL_ADEC(ADEC1_INFO,                      0, MIXER_ADDR_ADEC_15),
	MARS_CTL_ADEC(ADEC2_INFO,                      0, MIXER_ADDR_ADEC_16),
	MARS_CTL_ADEC(ADEC3_INFO,                      0, MIXER_ADDR_ADEC_17),
	MARS_CTL_ADEC(ADEC_TP_AUDIODESCRIPTION,        0, MIXER_ADDR_ADEC_18),
	MARS_CTL_ADEC(ADEC0_TP_PTS,                    0, MIXER_ADDR_ADEC_19),
	MARS_CTL_ADEC(ADEC1_TP_PTS,                    0, MIXER_ADDR_ADEC_20),
	MARS_CTL_ADEC(ADEC2_TP_PTS,                    0, MIXER_ADDR_ADEC_21),
	MARS_CTL_ADEC(ADEC3_TP_PTS,                    0, MIXER_ADDR_ADEC_22),
	MARS_CTL_ADEC(ADEC_AC4_AUTO1STLANG,            0, MIXER_ADDR_ADEC_23),
	MARS_CTL_ADEC(ADEC_AC4_AUTO2NDLANG,            0, MIXER_ADDR_ADEC_24),
	MARS_CTL_ADEC(ADEC_AC4_AUTO_ADTYPE,            0, MIXER_ADDR_ADEC_25),
	MARS_CTL_ADEC(ADEC_AC4_AUTO_PRIORITIZE_ADTYPE, 0, MIXER_ADDR_ADEC_26),
	MARS_CTL_ADEC(ADEC_AC4_DIALOGENHANCEGAIN,      0, MIXER_ADDR_ADEC_27),
	MARS_CTL_ADEC(ADEC_DOLBY_OTTMODE,              0, MIXER_ADDR_ADEC_28),
	MARS_CTL_ADEC(ADEC0_TP_BUFFERSTATUS,           0, MIXER_ADDR_ADEC_29),
	MARS_CTL_ADEC(ADEC1_TP_BUFFERSTATUS,           0, MIXER_ADDR_ADEC_30),
	MARS_CTL_ADEC(ADEC2_TP_BUFFERSTATUS,           0, MIXER_ADDR_ADEC_31),
	MARS_CTL_ADEC(ADEC3_TP_BUFFERSTATUS,           0, MIXER_ADDR_ADEC_32),
	MARS_CTL_ADEC(ADEC_AC4_AUTO_ADMIXING,          0, MIXER_ADDR_ADEC_33),
	MARS_CTL_ADEC(ADEC_AC4_PRES_GROUP_IDX,         0, MIXER_ADDR_ADEC_34),
};

static struct snd_kcontrol_new snd_mars_sndout_controls[] = {
	MARS_CTL_SNDOUT(SNDOUT_OPEN,                     0, MIXER_ADDR_SNDOUT_0),
	MARS_CTL_SNDOUT(SNDOUT_CLOSE,                    0, MIXER_ADDR_SNDOUT_1),
	MARS_CTL_SNDOUT(SNDOUT_CONNECT,                  0, MIXER_ADDR_SNDOUT_2),
	MARS_CTL_SNDOUT(SNDOUT_DISCONNECT,               0, MIXER_ADDR_SNDOUT_3),
	MARS_CTL_SNDOUT(SNDOUT_MAINAUDIO_OUTPUT,         0, MIXER_ADDR_SNDOUT_4),
	MARS_CTL_SNDOUT(SNDOUT_SPDIF_OUTPUTTYPE,         0, MIXER_ADDR_SNDOUT_5),
	MARS_CTL_SNDOUT(SNDOUT_SPDIF_CATERGORYCODE,      0, MIXER_ADDR_SNDOUT_6),
	MARS_CTL_SNDOUT(SNDOUT_SPDIF_COPYPROTECTIONINFO, 0, MIXER_ADDR_SNDOUT_7),
	MARS_CTL_SNDOUT(SNDOUT_OPTIC_LIGHTONOFF,         0, MIXER_ADDR_SNDOUT_8),
	MARS_CTL_SNDOUT(SNDOUT_ARC_ONOFF,                0, MIXER_ADDR_SNDOUT_9),
	MARS_CTL_SNDOUT(SNDOUT_OPTIC_LG,                 0, MIXER_ADDR_SNDOUT_10),
	MARS_CTL_SNDOUT(SNDOUT_SPK_OUTPUT,               0, MIXER_ADDR_SNDOUT_11),
	MARS_CTL_SNDOUT(SNDOUT_CAPTURE_DISABLE,          0, MIXER_ADDR_SNDOUT_12),
	MARS_CTL_SNDOUT(SNDOUT_EARC_ONOFF,               0, MIXER_ADDR_SNDOUT_13),
	MARS_CTL_SNDOUT(SNDOUT_EARC_OUTPUT_TYPE,         0, MIXER_ADDR_SNDOUT_14),
	MARS_CTL_SNDOUT(SNDOUT_CAPTURE_OUTPUT_RESAMPLING,0, MIXER_ADDR_SNDOUT_15),
	MARS_CTL_SNDOUT(SNDOUT_CAPTURE_INPUT_TIMECLOCK,  0, MIXER_ADDR_SNDOUT_16),
	MARS_CTL_SNDOUT(SNDOUT_EAC3_ATMOS_ENCODE_ONOFF,  0, MIXER_ADDR_SNDOUT_17),
};

static struct snd_kcontrol_new snd_mars_mute_controls[] = {
	MARS_CTL_MUTE(MUTE_INPUT,            0, MIXER_ADDR_MUTE_0),
	MARS_CTL_MUTE(MUTE_OUTPUT,           0, MIXER_ADDR_MUTE_1),
};

static struct snd_kcontrol_new snd_mars_gain_controls[] = {
	MARS_CTL_GAIN(GAIN_INPUT,            0, MIXER_ADDR_GAIN_0),
	MARS_CTL_GAIN(GAIN_OUTPUT,           0, MIXER_ADDR_GAIN_1),
	MARS_CTL_GAIN(GAIN_AUDIODESCRIPTION, 0, MIXER_ADDR_GAIN_2),
};

static struct snd_kcontrol_new snd_mars_delay_controls[] = {
	MARS_CTL_DELAY(DELAY_INPUTOUTPUT,    0, MIXER_ADDR_DELAY_0),
};

static struct snd_kcontrol_new snd_mars_hdmi_controls[] = {
	MARS_CTL_HDMI(AHDMI_PORT0_AUDIOMODE,          0, MIXER_ADDR_HDMI_0),
	MARS_CTL_HDMI(AHDMI_PORT1_AUDIOMODE,          0, MIXER_ADDR_HDMI_1),
	MARS_CTL_HDMI(AHDMI_PORT2_AUDIOMODE,          0, MIXER_ADDR_HDMI_2),
	MARS_CTL_HDMI(AHDMI_PORT3_AUDIOMODE,          0, MIXER_ADDR_HDMI_3),
	MARS_CTL_HDMI(AHDMI_PORT0_COPYPROTECTIONINFO, 0, MIXER_ADDR_HDMI_4),
	MARS_CTL_HDMI(AHDMI_PORT1_COPYPROTECTIONINFO, 0, MIXER_ADDR_HDMI_5),
	MARS_CTL_HDMI(AHDMI_PORT2_COPYPROTECTIONINFO, 0, MIXER_ADDR_HDMI_6),
	MARS_CTL_HDMI(AHDMI_PORT3_COPYPROTECTIONINFO, 0, MIXER_ADDR_HDMI_7),
};

static struct snd_kcontrol_new snd_mars_direct_controls[] = {
	MARS_CTL_DIRECT(DIRECT_CODEC, 0, MIXER_ADDR_DIRECT_0),
};

/* LGSE_SPK */
static struct snd_kcontrol_new snd_mars_lgse_SPK_controls[] = {
	MARS_CTL_LGSE(LGSE_MODE,         0, MIXER_ADDR_LGSE_0_0),
	MARS_CTL_LGSE(LGSE_INIT_FN000,   0, MIXER_ADDR_LGSE_0_1),
	MARS_CTL_LGSE(LGSE_INIT_FN000,   1, MIXER_ADDR_LGSE_0_2),
	MARS_CTL_LGSE(LGSE_INIT_FN000,   2, MIXER_ADDR_LGSE_0_3),
	MARS_CTL_LGSE(LGSE_VAR_FN000,    0, MIXER_ADDR_LGSE_0_4),
#if (ALSA_LGSE_VERSION == 70)
	MARS_CTL_LGSE(LGSE_VAR_FN000_1,  0, MIXER_ADDR_LGSE_0_5),
#endif
	MARS_CTL_LGSE(LGSE_INIT_FN001,   0, MIXER_ADDR_LGSE_0_6),
	MARS_CTL_LGSE(LGSE_VAR_FN001,    0, MIXER_ADDR_LGSE_0_7),
	MARS_CTL_LGSE(LGSE_VAR_FN001,    1, MIXER_ADDR_LGSE_0_8),
#if (ALSA_LGSE_VERSION == 70)
	MARS_CTL_LGSE(LGSE_INIT_FN002,   0, MIXER_ADDR_LGSE_0_9),
	MARS_CTL_LGSE(LGSE_VAR_FN002,    0, MIXER_ADDR_LGSE_0_10),
#endif
	MARS_CTL_LGSE(LGSE_VAR_FN004,    0, MIXER_ADDR_LGSE_0_11),
#if (ALSA_LGSE_VERSION == 70)
	MARS_CTL_LGSE(LGSE_VAR_FN004_1,  0, MIXER_ADDR_LGSE_0_12),
#endif
	MARS_CTL_LGSE(LGSE_VAR_FN005,    0, MIXER_ADDR_LGSE_0_13),
	MARS_CTL_LGSE(LGSE_INIT_FN008,   0, MIXER_ADDR_LGSE_0_14),
	MARS_CTL_LGSE(LGSE_VAR_FN008,    0, MIXER_ADDR_LGSE_0_15),
#if (ALSA_LGSE_VERSION == 70)
	MARS_CTL_LGSE(LGSE_VAR_FN008_1,  0, MIXER_ADDR_LGSE_0_16),
#endif
	MARS_CTL_LGSE(LGSE_INIT_FN009,   0, MIXER_ADDR_LGSE_0_17),
	MARS_CTL_LGSE(LGSE_VAR_FN009,    0, MIXER_ADDR_LGSE_0_18),
	MARS_CTL_LGSE(LGSE_INIT_FN010,   0, MIXER_ADDR_LGSE_0_19),
	MARS_CTL_LGSE(LGSE_VAR_FN010,    0, MIXER_ADDR_LGSE_0_20),
	MARS_CTL_LGSE(LGSE_OUT_FN010,    0, MIXER_ADDR_LGSE_0_21),
	MARS_CTL_LGSE(LGSE_INIT_FN014,   0, MIXER_ADDR_LGSE_0_22),
	MARS_CTL_LGSE(LGSE_INIT_FN014,   1, MIXER_ADDR_LGSE_0_23),
	MARS_CTL_LGSE(LGSE_VAR_FN014,    0, MIXER_ADDR_LGSE_0_24),
	MARS_CTL_LGSE(LGSE_OUT_FN014,    0, MIXER_ADDR_LGSE_0_25),
	MARS_CTL_LGSE(LGSE_INIT_FN016,   0, MIXER_ADDR_LGSE_0_26),
	MARS_CTL_LGSE(LGSE_VAR_FN016,    0, MIXER_ADDR_LGSE_0_27),
#if (ALSA_LGSE_VERSION == 63)
	MARS_CTL_LGSE(LGSE_VAR_FN017,    0, MIXER_ADDR_LGSE_0_28),
	MARS_CTL_LGSE(LGSE_VAR_FN017,    1, MIXER_ADDR_LGSE_0_29),
	MARS_CTL_LGSE(LGSE_INIT_FN019,   0, MIXER_ADDR_LGSE_0_30),
	MARS_CTL_LGSE(LGSE_VAR_FN019,    0, MIXER_ADDR_LGSE_0_31),
	MARS_CTL_LGSE(LGSE_INIT_FN022,   0, MIXER_ADDR_LGSE_0_32),
	MARS_CTL_LGSE(LGSE_VAR_FN022,    0, MIXER_ADDR_LGSE_0_33),
	MARS_CTL_LGSE(LGSE_VAR_FN022,    1, MIXER_ADDR_LGSE_0_34),
	MARS_CTL_LGSE(LGSE_VAR_FN022,    2, MIXER_ADDR_LGSE_0_35),
	MARS_CTL_LGSE(LGSE_VAR_FN022,    3, MIXER_ADDR_LGSE_0_36),
#elif (ALSA_LGSE_VERSION == 70)
	MARS_CTL_LGSE(LGSE_VAR_FN017_2,  0, MIXER_ADDR_LGSE_0_28),
	MARS_CTL_LGSE(LGSE_VAR_FN017_2,  1, MIXER_ADDR_LGSE_0_29),
	MARS_CTL_LGSE(LGSE_VAR_FN017_3,  0, MIXER_ADDR_LGSE_0_30),
	MARS_CTL_LGSE(LGSE_VAR_FN017_3,  1, MIXER_ADDR_LGSE_0_31),
	MARS_CTL_LGSE(LGSE_VAR_FN019_1,  0, MIXER_ADDR_LGSE_0_32),
	MARS_CTL_LGSE(LGSE_VAR_FN019_1,  1, MIXER_ADDR_LGSE_0_33),
	MARS_CTL_LGSE(LGSE_VAR_FN022_1,  0, MIXER_ADDR_LGSE_0_34),
	MARS_CTL_LGSE(LGSE_VAR_FN022_1,  1, MIXER_ADDR_LGSE_0_35),
	MARS_CTL_LGSE(LGSE_VAR_FN022_1,  2, MIXER_ADDR_LGSE_0_36),
	MARS_CTL_LGSE(LGSE_VAR_FN022_1,  3, MIXER_ADDR_LGSE_0_37),
	MARS_CTL_LGSE(LGSE_VAR_FN022_2,  0, MIXER_ADDR_LGSE_0_38),
	MARS_CTL_LGSE(LGSE_VAR_FN022_2,  1, MIXER_ADDR_LGSE_0_39),
	MARS_CTL_LGSE(LGSE_VAR_FN022_2,  2, MIXER_ADDR_LGSE_0_40),
	MARS_CTL_LGSE(LGSE_VAR_FN022_2,  3, MIXER_ADDR_LGSE_0_41),
#endif
	MARS_CTL_LGSE(LGSE_INIT_FN023,   0, MIXER_ADDR_LGSE_0_42),
	MARS_CTL_LGSE(LGSE_INIT_FN023,   1, MIXER_ADDR_LGSE_0_43),
	MARS_CTL_LGSE(LGSE_INIT_FN024,   0, MIXER_ADDR_LGSE_0_44),
	MARS_CTL_LGSE(LGSE_VAR_FN026,    0, MIXER_ADDR_LGSE_0_45),
	MARS_CTL_LGSE(LGSE_VAR_FN026,    1, MIXER_ADDR_LGSE_0_46),
#if (ALSA_LGSE_VERSION == 63)
	MARS_CTL_LGSE(LGSE_VAR_FN028,    0, MIXER_ADDR_LGSE_0_47),
	MARS_CTL_LGSE(LGSE_VAR_FN029,    0, MIXER_ADDR_LGSE_0_48),
	MARS_CTL_LGSE(LGSE_VAR_FN029,    1, MIXER_ADDR_LGSE_0_49),
#elif (ALSA_LGSE_VERSION == 70)
	MARS_CTL_LGSE(LGSE_INIT_FN030,   0, MIXER_ADDR_LGSE_0_47),
	MARS_CTL_LGSE(LGSE_VAR_FN030,    0, MIXER_ADDR_LGSE_0_48),
	MARS_CTL_LGSE(LGSE_OUT_FN030,    0, MIXER_ADDR_LGSE_0_49),
	MARS_CTL_LGSE(LGSE_INIT_FN030_1, 0, MIXER_ADDR_LGSE_0_50),
	MARS_CTL_LGSE(LGSE_VAR_FN030_1,  0, MIXER_ADDR_LGSE_0_51),
#endif
	MARS_CTL_LGSE(LGSE_VX_STATUS,                         0, MIXER_ADDR_LGSE_0_52),
	MARS_CTL_LGSE(LGSE_VX_PROCOUTPUTGAIN,                 0, MIXER_ADDR_LGSE_0_53),
	MARS_CTL_LGSE(LGSE_VX_TSX,                            0, MIXER_ADDR_LGSE_0_54),
	MARS_CTL_LGSE(LGSE_VX_TSXPASSIVEMATRIXUPMIX,          0, MIXER_ADDR_LGSE_0_55),
	MARS_CTL_LGSE(LGSE_VX_TSXHEIGHTUPMIX,                 0, MIXER_ADDR_LGSE_0_56),
	MARS_CTL_LGSE(LGSE_VX_TSXLPRGAIN,                     0, MIXER_ADDR_LGSE_0_57),
	MARS_CTL_LGSE(LGSE_VX_TSXLPRGAINASINPUTCHANNEL,       0, MIXER_ADDR_LGSE_0_58),
	MARS_CTL_LGSE(LGSE_VX_TSXHORIZVIREFF,                 0, MIXER_ADDR_LGSE_0_59),
	MARS_CTL_LGSE(LGSE_VX_TSXHORIZVIREFFASINPUTCHANNEL,   0, MIXER_ADDR_LGSE_0_60),
	MARS_CTL_LGSE(LGSE_VX_TSXHEIGHTMIXCOEF,               0, MIXER_ADDR_LGSE_0_61),
	MARS_CTL_LGSE(LGSE_VX_TSXHEIGHTMIXCOEFASINPUTCHANNEL, 0, MIXER_ADDR_LGSE_0_62),
	MARS_CTL_LGSE(LGSE_VX_CERTPARAM,                      0, MIXER_ADDR_LGSE_0_63),
	MARS_CTL_LGSE(LGSE_VX_CENTERGAIN,                     0, MIXER_ADDR_LGSE_0_64),
	MARS_CTL_LGSE(LGSE_POSTPROCESS,                 0, MIXER_ADDR_LGSE_0_65),
	MARS_CTL_LGSE(LGSE_DAP_SURROUNDVIRTUALIZERMODE, 0, MIXER_ADDR_LGSE_0_66),
	MARS_CTL_LGSE(LGSE_DAP_DIALOGUEENHANCER,        0, MIXER_ADDR_LGSE_0_67),
	MARS_CTL_LGSE(LGSE_DAP_VOLUMELEVELER,           0, MIXER_ADDR_LGSE_0_68),
	MARS_CTL_LGSE(LGSE_DAP_VOLUMEMODELER,           0, MIXER_ADDR_LGSE_0_69),
	MARS_CTL_LGSE(LGSE_DAP_VOLUMEMAXIMIZER,         0, MIXER_ADDR_LGSE_0_70),
	MARS_CTL_LGSE(LGSE_DAP_OPTIMIZER,               0, MIXER_ADDR_LGSE_0_71),
	MARS_CTL_LGSE(LGSE_DAP_OPTIMIZER,               1, MIXER_ADDR_LGSE_0_72),
	MARS_CTL_LGSE(LGSE_DAP_PROCESSOPTIMIZER,        0, MIXER_ADDR_LGSE_0_73),
	MARS_CTL_LGSE(LGSE_DAP_PROCESSOPTIMIZER,        1, MIXER_ADDR_LGSE_0_74),
	MARS_CTL_LGSE(LGSE_DAP_SURROUNDDECODER,         0, MIXER_ADDR_LGSE_0_75),
	MARS_CTL_LGSE(LGSE_DAP_SURROUNDCOMPRESSOR,      0, MIXER_ADDR_LGSE_0_76),
	MARS_CTL_LGSE(LGSE_DAP_VIRTUALIZERSPEAKERANGLE, 0, MIXER_ADDR_LGSE_0_77),
	MARS_CTL_LGSE(LGSE_DAP_INTELLIGENCEEQ,          0, MIXER_ADDR_LGSE_0_78),
	MARS_CTL_LGSE(LGSE_DAP_MEIDAINTELLIGENCE,       0, MIXER_ADDR_LGSE_0_79),
	MARS_CTL_LGSE(LGSE_DAP_GRAPHICALEQ,             0, MIXER_ADDR_LGSE_0_80),
	MARS_CTL_LGSE(LGSE_DAP_PERCEPTUALHEIGHTFILTER,  0, MIXER_ADDR_LGSE_0_81),
	MARS_CTL_LGSE(LGSE_DAP_BASSENHANCER,            0, MIXER_ADDR_LGSE_0_82),
	MARS_CTL_LGSE(LGSE_DAP_BASSEXTRACTION,          0, MIXER_ADDR_LGSE_0_83),
	MARS_CTL_LGSE(LGSE_DAP_VIRTUALBASS,             0, MIXER_ADDR_LGSE_0_84),
	MARS_CTL_LGSE(LGSE_DAP_REGULATOR,               0, MIXER_ADDR_LGSE_0_85),
	MARS_CTL_LGSE(LGSE_SOUNDENGINE_MODE,            0, MIXER_ADDR_LGSE_0_86),
	MARS_CTL_LGSE(LGSE_VERSION,                     0, MIXER_ADDR_LGSE_0_87),
	MARS_CTL_LGSE(LGSE_SOUNDENGINE_INIT,            0, MIXER_ADDR_LGSE_0_88),
	MARS_CTL_LGSE(LGSE_INIT_MAIN,                   0, MIXER_ADDR_LGSE_0_89),
	MARS_CTL_LGSE(LGSE_VAR_MAIN,                    0, MIXER_ADDR_LGSE_0_90),
};

/* LGSE_SPK_BTSUR */
static struct snd_kcontrol_new snd_mars_lgse_SPK_BTSUR_controls[] = {
	MARS_CTL_LGSE(LGSE_MODE,         10, MIXER_ADDR_LGSE_1_0),
	MARS_CTL_LGSE(LGSE_INIT_FN000,   10, MIXER_ADDR_LGSE_1_1),
	MARS_CTL_LGSE(LGSE_INIT_FN000,   11, MIXER_ADDR_LGSE_1_2),
	MARS_CTL_LGSE(LGSE_INIT_FN000,   12, MIXER_ADDR_LGSE_1_3),
	MARS_CTL_LGSE(LGSE_VAR_FN000,    10, MIXER_ADDR_LGSE_1_4),
#if (ALSA_LGSE_VERSION == 70)
	MARS_CTL_LGSE(LGSE_VAR_FN000_1,  10, MIXER_ADDR_LGSE_1_5),
#endif
	MARS_CTL_LGSE(LGSE_INIT_FN001,   10, MIXER_ADDR_LGSE_1_6),
	MARS_CTL_LGSE(LGSE_VAR_FN001,    10, MIXER_ADDR_LGSE_1_7),
	MARS_CTL_LGSE(LGSE_VAR_FN001,    11, MIXER_ADDR_LGSE_1_8),
#if (ALSA_LGSE_VERSION == 70)
	MARS_CTL_LGSE(LGSE_INIT_FN002,   10, MIXER_ADDR_LGSE_1_9),
	MARS_CTL_LGSE(LGSE_VAR_FN002,    10, MIXER_ADDR_LGSE_1_10),
#endif
	MARS_CTL_LGSE(LGSE_VAR_FN004,    10, MIXER_ADDR_LGSE_1_11),
#if (ALSA_LGSE_VERSION == 70)
	MARS_CTL_LGSE(LGSE_VAR_FN004_1,  10, MIXER_ADDR_LGSE_1_12),
#endif
	MARS_CTL_LGSE(LGSE_VAR_FN005,    10, MIXER_ADDR_LGSE_1_13),
	MARS_CTL_LGSE(LGSE_INIT_FN008,   10, MIXER_ADDR_LGSE_1_14),
	MARS_CTL_LGSE(LGSE_VAR_FN008,    10, MIXER_ADDR_LGSE_1_15),
#if (ALSA_LGSE_VERSION == 70)
	MARS_CTL_LGSE(LGSE_VAR_FN008_1,  10, MIXER_ADDR_LGSE_1_16),
#endif
	MARS_CTL_LGSE(LGSE_INIT_FN009,   10, MIXER_ADDR_LGSE_1_17),
	MARS_CTL_LGSE(LGSE_VAR_FN009,    10, MIXER_ADDR_LGSE_1_18),
	MARS_CTL_LGSE(LGSE_INIT_FN010,   10, MIXER_ADDR_LGSE_1_19),
	MARS_CTL_LGSE(LGSE_VAR_FN010,    10, MIXER_ADDR_LGSE_1_20),
	MARS_CTL_LGSE(LGSE_OUT_FN010,    10, MIXER_ADDR_LGSE_1_21),
	MARS_CTL_LGSE(LGSE_INIT_FN014,   10, MIXER_ADDR_LGSE_1_22),
	MARS_CTL_LGSE(LGSE_INIT_FN014,   11, MIXER_ADDR_LGSE_1_23),
	MARS_CTL_LGSE(LGSE_VAR_FN014,    10, MIXER_ADDR_LGSE_1_24),
	MARS_CTL_LGSE(LGSE_OUT_FN014,    10, MIXER_ADDR_LGSE_1_25),
	MARS_CTL_LGSE(LGSE_INIT_FN016,   10, MIXER_ADDR_LGSE_1_26),
	MARS_CTL_LGSE(LGSE_VAR_FN016,    10, MIXER_ADDR_LGSE_1_27),
#if (ALSA_LGSE_VERSION == 63)
	MARS_CTL_LGSE(LGSE_VAR_FN017,    10, MIXER_ADDR_LGSE_1_28),
	MARS_CTL_LGSE(LGSE_VAR_FN017,    11, MIXER_ADDR_LGSE_1_29),
	MARS_CTL_LGSE(LGSE_INIT_FN019,   10, MIXER_ADDR_LGSE_1_30),
	MARS_CTL_LGSE(LGSE_VAR_FN019,    10, MIXER_ADDR_LGSE_1_31),
	MARS_CTL_LGSE(LGSE_INIT_FN022,   10, MIXER_ADDR_LGSE_1_32),
	MARS_CTL_LGSE(LGSE_VAR_FN022,    10, MIXER_ADDR_LGSE_1_33),
	MARS_CTL_LGSE(LGSE_VAR_FN022,    11, MIXER_ADDR_LGSE_1_34),
	MARS_CTL_LGSE(LGSE_VAR_FN022,    12, MIXER_ADDR_LGSE_1_35),
	MARS_CTL_LGSE(LGSE_VAR_FN022,    13, MIXER_ADDR_LGSE_1_36),
#elif (ALSA_LGSE_VERSION == 70)
	MARS_CTL_LGSE(LGSE_VAR_FN017_2,  10, MIXER_ADDR_LGSE_1_28),
	MARS_CTL_LGSE(LGSE_VAR_FN017_2,  11, MIXER_ADDR_LGSE_1_29),
	MARS_CTL_LGSE(LGSE_VAR_FN017_3,  10, MIXER_ADDR_LGSE_1_30),
	MARS_CTL_LGSE(LGSE_VAR_FN017_3,  11, MIXER_ADDR_LGSE_1_31),
	MARS_CTL_LGSE(LGSE_VAR_FN019_1,  10, MIXER_ADDR_LGSE_1_32),
	MARS_CTL_LGSE(LGSE_VAR_FN019_1,  11, MIXER_ADDR_LGSE_1_33),
	MARS_CTL_LGSE(LGSE_VAR_FN022_1,  10, MIXER_ADDR_LGSE_1_34),
	MARS_CTL_LGSE(LGSE_VAR_FN022_1,  11, MIXER_ADDR_LGSE_1_35),
	MARS_CTL_LGSE(LGSE_VAR_FN022_1,  12, MIXER_ADDR_LGSE_1_36),
	MARS_CTL_LGSE(LGSE_VAR_FN022_1,  13, MIXER_ADDR_LGSE_1_37),
	MARS_CTL_LGSE(LGSE_VAR_FN022_2,  10, MIXER_ADDR_LGSE_1_38),
	MARS_CTL_LGSE(LGSE_VAR_FN022_2,  11, MIXER_ADDR_LGSE_1_39),
	MARS_CTL_LGSE(LGSE_VAR_FN022_2,  12, MIXER_ADDR_LGSE_1_40),
	MARS_CTL_LGSE(LGSE_VAR_FN022_2,  13, MIXER_ADDR_LGSE_1_41),
#endif
	MARS_CTL_LGSE(LGSE_INIT_FN023,   10, MIXER_ADDR_LGSE_1_42),
	MARS_CTL_LGSE(LGSE_INIT_FN023,   11, MIXER_ADDR_LGSE_1_43),
	MARS_CTL_LGSE(LGSE_INIT_FN024,   10, MIXER_ADDR_LGSE_1_44),
	MARS_CTL_LGSE(LGSE_VAR_FN026,    10, MIXER_ADDR_LGSE_1_45),
	MARS_CTL_LGSE(LGSE_VAR_FN026,    11, MIXER_ADDR_LGSE_1_46),
#if (ALSA_LGSE_VERSION == 63)
	MARS_CTL_LGSE(LGSE_VAR_FN028,    10, MIXER_ADDR_LGSE_1_47),
	MARS_CTL_LGSE(LGSE_VAR_FN029,    10, MIXER_ADDR_LGSE_1_48),
	MARS_CTL_LGSE(LGSE_VAR_FN029,    11, MIXER_ADDR_LGSE_1_49),
#elif (ALSA_LGSE_VERSION == 70)
	MARS_CTL_LGSE(LGSE_INIT_FN030,   10, MIXER_ADDR_LGSE_1_47),
	MARS_CTL_LGSE(LGSE_VAR_FN030,    10, MIXER_ADDR_LGSE_1_48),
	MARS_CTL_LGSE(LGSE_OUT_FN030,    10, MIXER_ADDR_LGSE_1_49),
	MARS_CTL_LGSE(LGSE_INIT_FN030_1, 10, MIXER_ADDR_LGSE_1_50),
	MARS_CTL_LGSE(LGSE_VAR_FN030_1,  10, MIXER_ADDR_LGSE_1_51),
#endif
	MARS_CTL_LGSE(LGSE_VX_STATUS,                         10, MIXER_ADDR_LGSE_1_52),
	MARS_CTL_LGSE(LGSE_VX_PROCOUTPUTGAIN,                 10, MIXER_ADDR_LGSE_1_53),
	MARS_CTL_LGSE(LGSE_VX_TSX,                            10, MIXER_ADDR_LGSE_1_54),
	MARS_CTL_LGSE(LGSE_VX_TSXPASSIVEMATRIXUPMIX,          10, MIXER_ADDR_LGSE_1_55),
	MARS_CTL_LGSE(LGSE_VX_TSXHEIGHTUPMIX,                 10, MIXER_ADDR_LGSE_1_56),
	MARS_CTL_LGSE(LGSE_VX_TSXLPRGAIN,                     10, MIXER_ADDR_LGSE_1_57),
	MARS_CTL_LGSE(LGSE_VX_TSXLPRGAINASINPUTCHANNEL,       10, MIXER_ADDR_LGSE_1_58),
	MARS_CTL_LGSE(LGSE_VX_TSXHORIZVIREFF,                 10, MIXER_ADDR_LGSE_1_59),
	MARS_CTL_LGSE(LGSE_VX_TSXHORIZVIREFFASINPUTCHANNEL,   10, MIXER_ADDR_LGSE_1_60),
	MARS_CTL_LGSE(LGSE_VX_TSXHEIGHTMIXCOEF,               10, MIXER_ADDR_LGSE_1_61),
	MARS_CTL_LGSE(LGSE_VX_TSXHEIGHTMIXCOEFASINPUTCHANNEL, 10, MIXER_ADDR_LGSE_1_62),
	MARS_CTL_LGSE(LGSE_VX_CERTPARAM,                      10, MIXER_ADDR_LGSE_1_63),
	MARS_CTL_LGSE(LGSE_VX_CENTERGAIN,                     10, MIXER_ADDR_LGSE_1_64),
	MARS_CTL_LGSE(LGSE_POSTPROCESS,                 10, MIXER_ADDR_LGSE_1_65),
	MARS_CTL_LGSE(LGSE_DAP_SURROUNDVIRTUALIZERMODE, 10, MIXER_ADDR_LGSE_1_66),
	MARS_CTL_LGSE(LGSE_DAP_DIALOGUEENHANCER,        10, MIXER_ADDR_LGSE_1_67),
	MARS_CTL_LGSE(LGSE_DAP_VOLUMELEVELER,           10, MIXER_ADDR_LGSE_1_68),
	MARS_CTL_LGSE(LGSE_DAP_VOLUMEMODELER,           10, MIXER_ADDR_LGSE_1_69),
	MARS_CTL_LGSE(LGSE_DAP_VOLUMEMAXIMIZER,         10, MIXER_ADDR_LGSE_1_70),
	MARS_CTL_LGSE(LGSE_DAP_OPTIMIZER,               10, MIXER_ADDR_LGSE_1_71),
	MARS_CTL_LGSE(LGSE_DAP_OPTIMIZER,               11, MIXER_ADDR_LGSE_1_72),
	MARS_CTL_LGSE(LGSE_DAP_PROCESSOPTIMIZER,        10, MIXER_ADDR_LGSE_1_73),
	MARS_CTL_LGSE(LGSE_DAP_PROCESSOPTIMIZER,        11, MIXER_ADDR_LGSE_1_74),
	MARS_CTL_LGSE(LGSE_DAP_SURROUNDDECODER,         10, MIXER_ADDR_LGSE_1_75),
	MARS_CTL_LGSE(LGSE_DAP_SURROUNDCOMPRESSOR,      10, MIXER_ADDR_LGSE_1_76),
	MARS_CTL_LGSE(LGSE_DAP_VIRTUALIZERSPEAKERANGLE, 10, MIXER_ADDR_LGSE_1_77),
	MARS_CTL_LGSE(LGSE_DAP_INTELLIGENCEEQ,          10, MIXER_ADDR_LGSE_1_78),
	MARS_CTL_LGSE(LGSE_DAP_MEIDAINTELLIGENCE,       10, MIXER_ADDR_LGSE_1_79),
	MARS_CTL_LGSE(LGSE_DAP_GRAPHICALEQ,             10, MIXER_ADDR_LGSE_1_80),
	MARS_CTL_LGSE(LGSE_DAP_PERCEPTUALHEIGHTFILTER,  10, MIXER_ADDR_LGSE_1_81),
	MARS_CTL_LGSE(LGSE_DAP_BASSENHANCER,            10, MIXER_ADDR_LGSE_1_82),
	MARS_CTL_LGSE(LGSE_DAP_BASSEXTRACTION,          10, MIXER_ADDR_LGSE_1_83),
	MARS_CTL_LGSE(LGSE_DAP_VIRTUALBASS,             10, MIXER_ADDR_LGSE_1_84),
	MARS_CTL_LGSE(LGSE_DAP_REGULATOR,               10, MIXER_ADDR_LGSE_1_85),
	MARS_CTL_LGSE(LGSE_SOUNDENGINE_MODE,            10, MIXER_ADDR_LGSE_1_86),
	MARS_CTL_LGSE(LGSE_VERSION,                     10, MIXER_ADDR_LGSE_1_87),
	MARS_CTL_LGSE(LGSE_SOUNDENGINE_INIT,            10, MIXER_ADDR_LGSE_1_88),
	MARS_CTL_LGSE(LGSE_INIT_MAIN,                   10, MIXER_ADDR_LGSE_1_89),
	MARS_CTL_LGSE(LGSE_VAR_MAIN,                    10, MIXER_ADDR_LGSE_1_90),
};

/* LGSE_BT_BTSUR */
static struct snd_kcontrol_new snd_mars_lgse_BT_BTSUR_controls[] = {
	MARS_CTL_LGSE(LGSE_MODE,         20, MIXER_ADDR_LGSE_2_0),
	MARS_CTL_LGSE(LGSE_INIT_FN000,   20, MIXER_ADDR_LGSE_2_1),
	MARS_CTL_LGSE(LGSE_INIT_FN000,   21, MIXER_ADDR_LGSE_2_2),
	MARS_CTL_LGSE(LGSE_INIT_FN000,   22, MIXER_ADDR_LGSE_2_3),
	MARS_CTL_LGSE(LGSE_VAR_FN000,    20, MIXER_ADDR_LGSE_2_4),
#if (ALSA_LGSE_VERSION == 70)
	MARS_CTL_LGSE(LGSE_VAR_FN000_1,  20, MIXER_ADDR_LGSE_2_5),
#endif
	MARS_CTL_LGSE(LGSE_INIT_FN001,   20, MIXER_ADDR_LGSE_2_6),
	MARS_CTL_LGSE(LGSE_VAR_FN001,    20, MIXER_ADDR_LGSE_2_7),
	MARS_CTL_LGSE(LGSE_VAR_FN001,    21, MIXER_ADDR_LGSE_2_8),
#if (ALSA_LGSE_VERSION == 70)
	MARS_CTL_LGSE(LGSE_INIT_FN002,   20, MIXER_ADDR_LGSE_2_9),
	MARS_CTL_LGSE(LGSE_VAR_FN002,    20, MIXER_ADDR_LGSE_2_10),
#endif
	MARS_CTL_LGSE(LGSE_VAR_FN004,    20, MIXER_ADDR_LGSE_2_11),
#if (ALSA_LGSE_VERSION == 70)
	MARS_CTL_LGSE(LGSE_VAR_FN004_1,  20, MIXER_ADDR_LGSE_2_12),
#endif
	MARS_CTL_LGSE(LGSE_VAR_FN005,    20, MIXER_ADDR_LGSE_2_13),
	MARS_CTL_LGSE(LGSE_INIT_FN008,   20, MIXER_ADDR_LGSE_2_14),
	MARS_CTL_LGSE(LGSE_VAR_FN008,    20, MIXER_ADDR_LGSE_2_15),
#if (ALSA_LGSE_VERSION == 70)
	MARS_CTL_LGSE(LGSE_VAR_FN008_1,  20, MIXER_ADDR_LGSE_2_16),
#endif
	MARS_CTL_LGSE(LGSE_INIT_FN009,   20, MIXER_ADDR_LGSE_2_17),
	MARS_CTL_LGSE(LGSE_VAR_FN009,    20, MIXER_ADDR_LGSE_2_18),
	MARS_CTL_LGSE(LGSE_INIT_FN010,   20, MIXER_ADDR_LGSE_2_19),
	MARS_CTL_LGSE(LGSE_VAR_FN010,    20, MIXER_ADDR_LGSE_2_20),
	MARS_CTL_LGSE(LGSE_OUT_FN010,    20, MIXER_ADDR_LGSE_2_21),
	MARS_CTL_LGSE(LGSE_INIT_FN014,   20, MIXER_ADDR_LGSE_2_22),
	MARS_CTL_LGSE(LGSE_INIT_FN014,   21, MIXER_ADDR_LGSE_2_23),
	MARS_CTL_LGSE(LGSE_VAR_FN014,    20, MIXER_ADDR_LGSE_2_24),
	MARS_CTL_LGSE(LGSE_OUT_FN014,    20, MIXER_ADDR_LGSE_2_25),
	MARS_CTL_LGSE(LGSE_INIT_FN016,   20, MIXER_ADDR_LGSE_2_26),
	MARS_CTL_LGSE(LGSE_VAR_FN016,    20, MIXER_ADDR_LGSE_2_27),
#if (ALSA_LGSE_VERSION == 63)
	MARS_CTL_LGSE(LGSE_VAR_FN017,    20, MIXER_ADDR_LGSE_2_28),
	MARS_CTL_LGSE(LGSE_VAR_FN017,    21, MIXER_ADDR_LGSE_2_29),
	MARS_CTL_LGSE(LGSE_INIT_FN019,   20, MIXER_ADDR_LGSE_2_30),
	MARS_CTL_LGSE(LGSE_VAR_FN019,    20, MIXER_ADDR_LGSE_2_31),
	MARS_CTL_LGSE(LGSE_INIT_FN022,   20, MIXER_ADDR_LGSE_2_32),
	MARS_CTL_LGSE(LGSE_VAR_FN022,    20, MIXER_ADDR_LGSE_2_33),
	MARS_CTL_LGSE(LGSE_VAR_FN022,    21, MIXER_ADDR_LGSE_2_34),
	MARS_CTL_LGSE(LGSE_VAR_FN022,    22, MIXER_ADDR_LGSE_2_35),
	MARS_CTL_LGSE(LGSE_VAR_FN022,    23, MIXER_ADDR_LGSE_2_36),
#elif (ALSA_LGSE_VERSION == 70)
	MARS_CTL_LGSE(LGSE_VAR_FN017_2,  20, MIXER_ADDR_LGSE_2_28),
	MARS_CTL_LGSE(LGSE_VAR_FN017_2,  21, MIXER_ADDR_LGSE_2_29),
	MARS_CTL_LGSE(LGSE_VAR_FN017_3,  20, MIXER_ADDR_LGSE_2_30),
	MARS_CTL_LGSE(LGSE_VAR_FN017_3,  21, MIXER_ADDR_LGSE_2_31),
	MARS_CTL_LGSE(LGSE_VAR_FN019_1,  20, MIXER_ADDR_LGSE_2_32),
	MARS_CTL_LGSE(LGSE_VAR_FN019_1,  21, MIXER_ADDR_LGSE_2_33),
	MARS_CTL_LGSE(LGSE_VAR_FN022_1,  20, MIXER_ADDR_LGSE_2_34),
	MARS_CTL_LGSE(LGSE_VAR_FN022_1,  21, MIXER_ADDR_LGSE_2_35),
	MARS_CTL_LGSE(LGSE_VAR_FN022_1,  22, MIXER_ADDR_LGSE_2_36),
	MARS_CTL_LGSE(LGSE_VAR_FN022_1,  23, MIXER_ADDR_LGSE_2_37),
	MARS_CTL_LGSE(LGSE_VAR_FN022_2,  20, MIXER_ADDR_LGSE_2_38),
	MARS_CTL_LGSE(LGSE_VAR_FN022_2,  21, MIXER_ADDR_LGSE_2_39),
	MARS_CTL_LGSE(LGSE_VAR_FN022_2,  22, MIXER_ADDR_LGSE_2_40),
	MARS_CTL_LGSE(LGSE_VAR_FN022_2,  23, MIXER_ADDR_LGSE_2_41),
#endif
	MARS_CTL_LGSE(LGSE_INIT_FN023,   20, MIXER_ADDR_LGSE_2_42),
	MARS_CTL_LGSE(LGSE_INIT_FN023,   21, MIXER_ADDR_LGSE_2_43),
	MARS_CTL_LGSE(LGSE_INIT_FN024,   20, MIXER_ADDR_LGSE_2_44),
	MARS_CTL_LGSE(LGSE_VAR_FN026,    20, MIXER_ADDR_LGSE_2_45),
	MARS_CTL_LGSE(LGSE_VAR_FN026,    21, MIXER_ADDR_LGSE_2_46),
#if (ALSA_LGSE_VERSION == 63)
	MARS_CTL_LGSE(LGSE_VAR_FN028,    20, MIXER_ADDR_LGSE_2_47),
	MARS_CTL_LGSE(LGSE_VAR_FN029,    20, MIXER_ADDR_LGSE_2_48),
	MARS_CTL_LGSE(LGSE_VAR_FN029,    21, MIXER_ADDR_LGSE_2_49),
#elif (ALSA_LGSE_VERSION == 70)
	MARS_CTL_LGSE(LGSE_INIT_FN030,   20, MIXER_ADDR_LGSE_2_47),
	MARS_CTL_LGSE(LGSE_VAR_FN030,    20, MIXER_ADDR_LGSE_2_48),
	MARS_CTL_LGSE(LGSE_OUT_FN030,    20, MIXER_ADDR_LGSE_2_49),
	MARS_CTL_LGSE(LGSE_INIT_FN030_1, 20, MIXER_ADDR_LGSE_2_50),
	MARS_CTL_LGSE(LGSE_VAR_FN030_1,  20, MIXER_ADDR_LGSE_2_51),
#endif
	MARS_CTL_LGSE(LGSE_VX_STATUS,                         20, MIXER_ADDR_LGSE_2_52),
	MARS_CTL_LGSE(LGSE_VX_PROCOUTPUTGAIN,                 20, MIXER_ADDR_LGSE_2_53),
	MARS_CTL_LGSE(LGSE_VX_TSX,                            20, MIXER_ADDR_LGSE_2_54),
	MARS_CTL_LGSE(LGSE_VX_TSXPASSIVEMATRIXUPMIX,          20, MIXER_ADDR_LGSE_2_55),
	MARS_CTL_LGSE(LGSE_VX_TSXHEIGHTUPMIX,                 20, MIXER_ADDR_LGSE_2_56),
	MARS_CTL_LGSE(LGSE_VX_TSXLPRGAIN,                     20, MIXER_ADDR_LGSE_2_57),
	MARS_CTL_LGSE(LGSE_VX_TSXLPRGAINASINPUTCHANNEL,       20, MIXER_ADDR_LGSE_2_58),
	MARS_CTL_LGSE(LGSE_VX_TSXHORIZVIREFF,                 20, MIXER_ADDR_LGSE_2_59),
	MARS_CTL_LGSE(LGSE_VX_TSXHORIZVIREFFASINPUTCHANNEL,   20, MIXER_ADDR_LGSE_2_60),
	MARS_CTL_LGSE(LGSE_VX_TSXHEIGHTMIXCOEF,               20, MIXER_ADDR_LGSE_2_61),
	MARS_CTL_LGSE(LGSE_VX_TSXHEIGHTMIXCOEFASINPUTCHANNEL, 20, MIXER_ADDR_LGSE_2_62),
	MARS_CTL_LGSE(LGSE_VX_CERTPARAM,                      20, MIXER_ADDR_LGSE_2_63),
	MARS_CTL_LGSE(LGSE_VX_CENTERGAIN,                     20, MIXER_ADDR_LGSE_2_64),
	MARS_CTL_LGSE(LGSE_POSTPROCESS,                 20, MIXER_ADDR_LGSE_2_65),
	MARS_CTL_LGSE(LGSE_DAP_SURROUNDVIRTUALIZERMODE, 20, MIXER_ADDR_LGSE_2_66),
	MARS_CTL_LGSE(LGSE_DAP_DIALOGUEENHANCER,        20, MIXER_ADDR_LGSE_2_67),
	MARS_CTL_LGSE(LGSE_DAP_VOLUMELEVELER,           20, MIXER_ADDR_LGSE_2_68),
	MARS_CTL_LGSE(LGSE_DAP_VOLUMEMODELER,           20, MIXER_ADDR_LGSE_2_69),
	MARS_CTL_LGSE(LGSE_DAP_VOLUMEMAXIMIZER,         20, MIXER_ADDR_LGSE_2_70),
	MARS_CTL_LGSE(LGSE_DAP_OPTIMIZER,               20, MIXER_ADDR_LGSE_2_71),
	MARS_CTL_LGSE(LGSE_DAP_OPTIMIZER,               21, MIXER_ADDR_LGSE_2_72),
	MARS_CTL_LGSE(LGSE_DAP_PROCESSOPTIMIZER,        20, MIXER_ADDR_LGSE_2_73),
	MARS_CTL_LGSE(LGSE_DAP_PROCESSOPTIMIZER,        21, MIXER_ADDR_LGSE_2_74),
	MARS_CTL_LGSE(LGSE_DAP_SURROUNDDECODER,         20, MIXER_ADDR_LGSE_2_75),
	MARS_CTL_LGSE(LGSE_DAP_SURROUNDCOMPRESSOR,      20, MIXER_ADDR_LGSE_2_76),
	MARS_CTL_LGSE(LGSE_DAP_VIRTUALIZERSPEAKERANGLE, 20, MIXER_ADDR_LGSE_2_77),
	MARS_CTL_LGSE(LGSE_DAP_INTELLIGENCEEQ,          20, MIXER_ADDR_LGSE_2_78),
	MARS_CTL_LGSE(LGSE_DAP_MEIDAINTELLIGENCE,       20, MIXER_ADDR_LGSE_2_79),
	MARS_CTL_LGSE(LGSE_DAP_GRAPHICALEQ,             20, MIXER_ADDR_LGSE_2_80),
	MARS_CTL_LGSE(LGSE_DAP_PERCEPTUALHEIGHTFILTER,  20, MIXER_ADDR_LGSE_2_81),
	MARS_CTL_LGSE(LGSE_DAP_BASSENHANCER,            20, MIXER_ADDR_LGSE_2_82),
	MARS_CTL_LGSE(LGSE_DAP_BASSEXTRACTION,          20, MIXER_ADDR_LGSE_2_83),
	MARS_CTL_LGSE(LGSE_DAP_VIRTUALBASS,             20, MIXER_ADDR_LGSE_2_84),
	MARS_CTL_LGSE(LGSE_DAP_REGULATOR,               20, MIXER_ADDR_LGSE_2_85),
	MARS_CTL_LGSE(LGSE_SOUNDENGINE_MODE,            20, MIXER_ADDR_LGSE_2_86),
	MARS_CTL_LGSE(LGSE_VERSION,                     20, MIXER_ADDR_LGSE_2_87),
	MARS_CTL_LGSE(LGSE_SOUNDENGINE_INIT,            20, MIXER_ADDR_LGSE_2_88),
	MARS_CTL_LGSE(LGSE_INIT_MAIN,                   20, MIXER_ADDR_LGSE_2_89),
	MARS_CTL_LGSE(LGSE_VAR_MAIN,                    20, MIXER_ADDR_LGSE_2_90),
};

static struct snd_kcontrol_new snd_mars_sif_controls[] = {
	MARS_CTL_SIF(SIF_OPEN,              0, MIXER_ADDR_SIF_0),
	MARS_CTL_SIF(SIF_CLOSE,             0, MIXER_ADDR_SIF_1),
	MARS_CTL_SIF(SIF_CONNECT,           0, MIXER_ADDR_SIF_2),
	MARS_CTL_SIF(SIF_DISCONNECT,        0, MIXER_ADDR_SIF_3),
	MARS_CTL_SIF(SIF_DETECTSOUNDSYSTEM, 0, MIXER_ADDR_SIF_4),
	MARS_CTL_SIF(SIF_BANDSETUP,         0, MIXER_ADDR_SIF_5),
	MARS_CTL_SIF(SIF_STANDARDSETUP,     0, MIXER_ADDR_SIF_6),
	MARS_CTL_SIF(SIF_CUR_ANALOGMODE,    0, MIXER_ADDR_SIF_7),
	MARS_CTL_SIF(SIF_USER_ANALOGMODE,   0, MIXER_ADDR_SIF_8),
	MARS_CTL_SIF(SIF_SIFEXIST,          0, MIXER_ADDR_SIF_9),
	MARS_CTL_SIF(SIF_HDEV,              0, MIXER_ADDR_SIF_10),
	MARS_CTL_SIF(SIF_A2THERESHOLDLEVEL, 0, MIXER_ADDR_SIF_11),
	MARS_CTL_SIF("Sif SoundSystemStrength", 0, MIXER_ADDR_SIF_12),	
};

static struct snd_kcontrol_new snd_mars_aenc_controls[] = {
	MARS_CTL_AENC(AENC_INFO,   0, MIXER_ADDR_AENC_0),
	MARS_CTL_AENC(AENC_VOLUME, 0, MIXER_ADDR_AENC_1),
	MARS_CTL_AENC(AENC_PTS,    0, MIXER_ADDR_AENC_2),
};

static struct snd_kcontrol_new snd_mars_adc_controls[] = {
	MARS_CTL_ADC(ADC_OPEN,       0, MIXER_ADDR_ADC_0),
	MARS_CTL_ADC(ADC_CLOSE,      0, MIXER_ADDR_ADC_1),
	MARS_CTL_ADC(ADC_CONNECT,    0, MIXER_ADDR_ADC_2),
	MARS_CTL_ADC(ADC_DISCONNECT, 0, MIXER_ADDR_ADC_3),
};
#endif /*_RTK_KCONTROL_*/
