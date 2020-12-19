#include "rtk_kcontrol.h"
#include "AudioRPCBaseDS_data.h"
#include "audio_hw_khal.h"

static BOOLEAN b_se_init = 0;

static ALSA_SIF_CMD_INFO sif_cmd_info;

int snd_mars_ctl_sif_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	/*ALSA_DEBUG_INFO("[ALSA] info name %s\n",uinfo->id.name);*/
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 500;
	uinfo->count = 1;
	if(strcmp(uinfo->id.name, SIF_BANDSETUP)==0){
		uinfo->count = 2;
	}else if(strcmp(uinfo->id.name, "Sif SoundSystemStrength")==0){
		uinfo->count = 2;
	}
	return 0;
}

int snd_mars_ctl_sif_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_card_mars *mars = snd_kcontrol_chip(kcontrol);
	unsigned long flags;
	UINT32 *item = ucontrol->value.enumerated.item;

	/*ALSA_DEBUG_INFO("[ALSA] get name %s\n", ucontrol->id.name);*/
	memset(item, 0, sizeof(UINT32)*128);

	spin_lock_irqsave(&mars->mixer_lock, flags);
	if(strcmp(ucontrol->id.name, SIF_OPEN)==0){
		item[0] = sif_cmd_info.open_status;
	}else if(strcmp(ucontrol->id.name, SIF_CLOSE)==0) {
		item[0] = sif_cmd_info.close_status;
	}else if(strcmp(ucontrol->id.name, SIF_CONNECT)==0) {
		item[0] = sif_cmd_info.connect_status;
	}else if(strcmp(ucontrol->id.name, SIF_DISCONNECT)==0) {
		item[0] = sif_cmd_info.disconnect_status;
	}else if(strcmp(ucontrol->id.name, SIF_DETECTSOUNDSYSTEM)==0) {
		item[0] = sif_cmd_info.get_sound_system;
	}else if(strcmp(ucontrol->id.name, SIF_BANDSETUP)==0) {
		item[0] = sif_cmd_info.get_sif_country;
		item[1] = sif_cmd_info.get_sound_system;
	}else if(strcmp(ucontrol->id.name, SIF_STANDARDSETUP)==0) {
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		Audio_SIF_Get_Standard(&sif_cmd_info.get_sif_standard);
		item[0] = sif_cmd_info.get_sif_standard;
		spin_lock_irqsave(&mars->mixer_lock, flags);
	}else if(strcmp(ucontrol->id.name, SIF_CUR_ANALOGMODE)==0) {
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		KHAL_SIF_CurAnalogMode((sif_mode_ext_type_t*)&item[0]);
		spin_lock_irqsave(&mars->mixer_lock, flags);
	}else if(strcmp(ucontrol->id.name, SIF_USER_ANALOGMODE)==0) {
		item[0] = sif_cmd_info.get_user_analog_mode;
	}else if(strcmp(ucontrol->id.name, SIF_SIFEXIST)==0) {
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		KHAL_SIF_SifExist((sif_existence_info_ext_type_t*)&item[0]);
		spin_lock_irqsave(&mars->mixer_lock, flags);
	}else if(strcmp(ucontrol->id.name, SIF_HDEV)==0) {
		item[0] = sif_cmd_info.get_hdev_onoff;
	}else if(strcmp(ucontrol->id.name, SIF_A2THERESHOLDLEVEL)==0) {
		item[0] = sif_cmd_info.get_a2thresholdlevel;
	}else if(strcmp(ucontrol->id.name, "Sif SoundSystemStrength")==0) {
		item[0] = sif_cmd_info.get_sound_system;
		item[1] = sif_cmd_info.SignalQuality;		
	}
	spin_unlock_irqrestore(&mars->mixer_lock, flags);
	return 0;
}
int snd_mars_ctl_sif_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_card_mars *mars = snd_kcontrol_chip(kcontrol);
	unsigned long flags;
	int ret = -1;
	UINT32 *item = ucontrol->value.enumerated.item;

	ALSA_DEBUG_INFO("[ALSA] put name \033[31m%s\033[0m item0 %d item1 %d\n",
			ucontrol->id.name, item[0], item[1]);

	if(strcmp(ucontrol->id.name, SIF_OPEN)==0){
		ret = HAL_AUDIO_AAD_Open();
		if(ret == OK)
			sif_cmd_info.open_status = TRUE;
	}else if(strcmp(ucontrol->id.name, SIF_CLOSE)==0) {
		ret = HAL_AUDIO_AAD_Close();
		if(ret == OK)
			sif_cmd_info.close_status = TRUE;
	}else if(strcmp(ucontrol->id.name, SIF_CONNECT)==0) {
		ret = HAL_AUDIO_AAD_Connect();
		if(ret == OK)
			sif_cmd_info.connect_status = TRUE;
	}else if(strcmp(ucontrol->id.name, SIF_DISCONNECT)==0) {
		ret = HAL_AUDIO_AAD_Disconnect();
		if(ret == OK)
			sif_cmd_info.disconnect_status = TRUE;
	}else if(strcmp(ucontrol->id.name, SIF_DETECTSOUNDSYSTEM)==0) {
		spin_lock_irqsave(&mars->mixer_lock, flags);
		sif_cmd_info.sound_system  = item[0];
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		ret = KHAL_SIF_DetectSoundSystem(sif_cmd_info.sound_system,
				&sif_cmd_info.get_sound_system, &sif_cmd_info.SignalQuality);
	}else if(strcmp(ucontrol->id.name, SIF_BANDSETUP)==0) {
		spin_lock_irqsave(&mars->mixer_lock, flags);
		sif_cmd_info.sif_country  = item[0];
		sif_cmd_info.sound_system = item[1];
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		ret = KHAL_SIF_BandSetup(sif_cmd_info.sif_country, sif_cmd_info.sound_system,
				&sif_cmd_info.get_sif_country, &sif_cmd_info.get_sound_system);
	}else if(strcmp(ucontrol->id.name, SIF_STANDARDSETUP)==0) {
		spin_lock_irqsave(&mars->mixer_lock, flags);
		sif_cmd_info.sif_standard = item[0];
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		ret = KHAL_SIF_StandardSetup(sif_cmd_info.sif_standard, &sif_cmd_info.get_sif_standard);
	}else if(strcmp(ucontrol->id.name, SIF_CUR_ANALOGMODE)==0) {
	}else if(strcmp(ucontrol->id.name, SIF_USER_ANALOGMODE)==0) {
		spin_lock_irqsave(&mars->mixer_lock, flags);
		sif_cmd_info.user_analog_mode = item[0];
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		ret = KHAL_SIF_UserAnalogMode(sif_cmd_info.user_analog_mode, &sif_cmd_info.get_user_analog_mode);
	}else if(strcmp(ucontrol->id.name, SIF_SIFEXIST)==0) {
	}else if(strcmp(ucontrol->id.name, SIF_HDEV)==0) {
		spin_lock_irqsave(&mars->mixer_lock, flags);
		sif_cmd_info.hdev_onoff = item[0];
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		ret = KHAL_SIF_HDev(sif_cmd_info.hdev_onoff, &sif_cmd_info.get_hdev_onoff);
	}else if(strcmp(ucontrol->id.name, SIF_A2THERESHOLDLEVEL)==0) {
		spin_lock_irqsave(&mars->mixer_lock, flags);
		sif_cmd_info.a2thresholdlevel = item[0];
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		ret = KHAL_SIF_A2ThresholdLevel(sif_cmd_info.a2thresholdlevel, &sif_cmd_info.get_a2thresholdlevel);
	}else if(strcmp(ucontrol->id.name, "Sif SoundSystemStrength")==0) {
		spin_lock_irqsave(&mars->mixer_lock, flags);
		sif_cmd_info.sound_system  = item[0];
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		ret = KHAL_SIF_SoundSystemStrength(sif_cmd_info.sound_system,
				&sif_cmd_info.get_sound_system, &sif_cmd_info.SignalQuality);
	}

	return ret;
}

static ALSA_LGSE_CMD_INFO lgse_cmd_info[3]; // 0: LGSE_SPK, 1:LGSE_SPK_BTSUR, 2:LGSE_BT_BTSUR
int snd_mars_ctl_lgse_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	int buf_index = (uinfo->id.index%10)%4;
	/*ALSA_DEBUG_INFO("[ALSA] info name %s index %d\n",uinfo->id.name,uinfo->id.index);*/

	uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
	uinfo->count = 512;
	if(strcmp(uinfo->id.name, LGSE_MODE)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 0x7fffffff;
	}else if(strcmp(uinfo->id.name, LGSE_SOUNDENGINE_MODE)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 4;
	}else if(strcmp(uinfo->id.name, LGSE_VERSION)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 100;
	}else if(strcmp(uinfo->id.name, LGSE_SOUNDENGINE_INIT)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 5;
	}else if(strcmp(uinfo->id.name, LGSE_INIT_FN000)==0){
		if(buf_index == 0 || buf_index == 1)
			uinfo->count = 128 << 2;
		else if(buf_index == 2)
			uinfo->count = 39 << 2;
	}else if(strcmp(uinfo->id.name, LGSE_VAR_FN000)==0){
		uinfo->count = 1 << 2;
	}else if(strcmp(uinfo->id.name, LGSE_INIT_FN001)==0){
		uinfo->count = 21 << 2;
	}else if(strcmp(uinfo->id.name, LGSE_VAR_FN001)==0){
		if(buf_index == 0)
			uinfo->count = 128 << 2;
		else if(buf_index == 1)
			uinfo->count = 2 << 2;
	}else if(strcmp(uinfo->id.name, LGSE_VAR_FN004)==0){
		uinfo->count = 25 << 2;
	}else if(strcmp(uinfo->id.name, LGSE_VAR_FN005)==0){
		uinfo->count = 15 << 2;
	}else if(strcmp(uinfo->id.name, LGSE_INIT_FN008)==0){
		uinfo->count = 108 << 2;
	}else if(strcmp(uinfo->id.name, LGSE_VAR_FN008)==0){
		uinfo->count = 11 << 2;
	}else if(strcmp(uinfo->id.name, LGSE_INIT_FN009)==0){
		uinfo->count = 10 << 2;
	}else if(strcmp(uinfo->id.name, LGSE_VAR_FN009)==0){
		uinfo->count = 2 << 2;
	}else if(strcmp(uinfo->id.name, LGSE_INIT_FN010)==0){
		uinfo->count = 10 << 2;
	}else if(strcmp(uinfo->id.name, LGSE_VAR_FN010)==0){
		uinfo->count = 1 << 2;
	}else if(strcmp(uinfo->id.name, LGSE_INIT_FN014)==0){
		if(buf_index == 0)
			uinfo->count = 128 << 2;
		else if(buf_index == 1)
			uinfo->count = 8 << 2;
	}else if(strcmp(uinfo->id.name, LGSE_VAR_FN014)==0){
		uinfo->count = 1 << 2;
	}else if(strcmp(uinfo->id.name, LGSE_INIT_FN016)==0){
		uinfo->count = 50 << 2;
	}else if(strcmp(uinfo->id.name, LGSE_VAR_FN016)==0){
		uinfo->count = 100 << 2;
	}else if(strcmp(uinfo->id.name, LGSE_VAR_FN017)==0){
		if(buf_index == 0)
			uinfo->count = 128 << 2;
		else if(buf_index == 1)
			uinfo->count = 34 << 2;
	}else if(strcmp(uinfo->id.name, LGSE_INIT_FN019)==0){
		uinfo->count = 98 << 2;
	}else if(strcmp(uinfo->id.name, LGSE_VAR_FN019)==0){
		uinfo->count = 4 << 2;
	}else if(strcmp(uinfo->id.name, LGSE_INIT_FN022)==0){
		uinfo->count = 6 << 2;
	}else if(strcmp(uinfo->id.name, LGSE_VAR_FN022)==0){
		if(buf_index == 0 || buf_index == 1 || buf_index == 2)
			uinfo->count = 128 << 2;
		else if(buf_index == 3)
			uinfo->count = 96 << 2;
	}else if(strcmp(uinfo->id.name, LGSE_INIT_FN023)==0){
		if(buf_index == 0)
			uinfo->count = 128 << 2;
		else if(buf_index == 1)
			uinfo->count = 60 << 2;
	}else if(strcmp(uinfo->id.name, LGSE_INIT_FN024)==0){
		uinfo->count = 12 << 2;
	}else if(strcmp(uinfo->id.name, LGSE_VAR_FN026)==0){
		if(buf_index == 0)
			uinfo->count = 128 << 2;
		else if(buf_index == 1)
			uinfo->count = 72 << 2;
	}else if(strcmp(uinfo->id.name, LGSE_VAR_FN028)==0){
		uinfo->count = 2 << 2;
	}else if(strcmp(uinfo->id.name, LGSE_VAR_FN029)==0){
		if(buf_index == 0)
			uinfo->count = 128 << 2;
		else if(buf_index == 1)
			uinfo->count = 72 << 2;
	}else if(strcmp(uinfo->id.name, LGSE_VX_STATUS)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 1;
	}else if(strcmp(uinfo->id.name, LGSE_VX_PROCOUTPUTGAIN)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = 0x8000000;
		uinfo->value.integer.max = 0x40000000;
	}else if(strcmp(uinfo->id.name, LGSE_VX_TSX)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 1;
	}else if(strcmp(uinfo->id.name, LGSE_VX_TSXPASSIVEMATRIXUPMIX)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 1;
	}else if(strcmp(uinfo->id.name, LGSE_VX_TSXHEIGHTUPMIX)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 1;
	}else if(strcmp(uinfo->id.name, LGSE_VX_TSXLPRGAIN)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 0x40000000;
	}else if(strcmp(uinfo->id.name, LGSE_VX_TSXLPRGAINASINPUTCHANNEL)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 2;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 0x40000000;
	}else if(strcmp(uinfo->id.name, LGSE_VX_TSXHORIZVIREFF)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 1;
	}else if(strcmp(uinfo->id.name, LGSE_VX_TSXHORIZVIREFFASINPUTCHANNEL)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 2;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 1;
	}else if(strcmp(uinfo->id.name, LGSE_VX_TSXHEIGHTMIXCOEF)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = 0x10000000;
		uinfo->value.integer.max = 0x40000000;
	}else if(strcmp(uinfo->id.name, LGSE_VX_TSXHEIGHTMIXCOEFASINPUTCHANNEL)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 2;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 0x40000000;
	}else if(strcmp(uinfo->id.name, LGSE_VX_CERTPARAM)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 1;
	}else if(strcmp(uinfo->id.name, LGSE_VX_CENTERGAIN)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = 0x20000000;
		uinfo->value.integer.max = 0x40000000;
	}else if(strcmp(uinfo->id.name, LGSE_POSTPROCESS)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 2;
	}else if(strcmp(uinfo->id.name, LGSE_DAP_SURROUNDVIRTUALIZERMODE)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 3;
	}else if(strcmp(uinfo->id.name, LGSE_DAP_DIALOGUEENHANCER)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 3;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 16;
	}else if(strcmp(uinfo->id.name, LGSE_DAP_VOLUMELEVELER)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 4;
		uinfo->value.integer.min = -640;
		uinfo->value.integer.max = 10;
	}else if(strcmp(uinfo->id.name, LGSE_DAP_VOLUMEMODELER)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 2;
		uinfo->value.integer.min = -320;
		uinfo->value.integer.max = 320;
	}else if(strcmp(uinfo->id.name, LGSE_DAP_VOLUMEMAXIMIZER)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 192;
	}else if(strcmp(uinfo->id.name, LGSE_DAP_OPTIMIZER)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 128;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 20000;
	}else if(strcmp(uinfo->id.name, LGSE_DAP_PROCESSOPTIMIZER)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 128;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 20000;
	}else if(strcmp(uinfo->id.name, LGSE_DAP_SURROUNDDECODER)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 1;
	}else if(strcmp(uinfo->id.name, LGSE_DAP_SURROUNDCOMPRESSOR)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 96;
	}else if(strcmp(uinfo->id.name, LGSE_DAP_VIRTUALIZERSPEAKERANGLE)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 3;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 30;
	}else if(strcmp(uinfo->id.name, LGSE_DAP_INTELLIGENCEEQ)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 43;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 20000;
	}else if(strcmp(uinfo->id.name, LGSE_DAP_MEIDAINTELLIGENCE)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 4;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 1;
	}else if(strcmp(uinfo->id.name, LGSE_DAP_GRAPHICALEQ)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 42;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 20000;
	}else if(strcmp(uinfo->id.name, LGSE_DAP_PERCEPTUALHEIGHTFILTER)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 2;
	}else if(strcmp(uinfo->id.name, LGSE_DAP_BASSENHANCER)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 4;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 20000;
	}else if(strcmp(uinfo->id.name, LGSE_DAP_BASSEXTRACTION)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 2;
	}else if(strcmp(uinfo->id.name, LGSE_DAP_VIRTUALBASS)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 10;
		uinfo->value.integer.min = -480;
		uinfo->value.integer.max = 938;
	}else if(strcmp(uinfo->id.name, LGSE_DAP_REGULATOR)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 86;
		uinfo->value.integer.min = -2080;
		uinfo->value.integer.max = 20000;
	}

	return 0;
}

void lgse_move_parameter(UINT32 *des, UINT32 *src, int noParam)
{
	int i;
	for(i = 0; i < noParam; i++){
		des[i] = src[i];
	}
}
int snd_mars_ctl_lgse_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_card_mars *mars = snd_kcontrol_chip(kcontrol);
	unsigned long flags;
	int noParam = 0;
	int index = ucontrol->id.index;
	int buf_index = ((ucontrol->id.index)%10)%4;
	int mode_index = ((ucontrol->id.index)/10)%3;
	unsigned char *data = ucontrol->value.bytes.data;
	unsigned int *item  = ucontrol->value.enumerated.item;

	/*ALSA_DEBUG_INFO("[ALSA] get name %s index %d\n", ucontrol->id.name, index);*/
	if(strcmp(ucontrol->id.name, LGSE_SOUNDENGINE_MODE)==0){
		item[0] = lgse_cmd_info[mode_index].mode;
	}else if(strcmp(ucontrol->id.name, LGSE_MODE)==0){
		item[0] = lgse_cmd_info[mode_index].flag;
	}else if(strcmp(ucontrol->id.name, LGSE_VERSION)==0){
#if (ALSA_LGSE_VERSION == 63)
		item[0] = 63;
#elif (ALSA_LGSE_VERSION == 70)
		item[0] = 70;
#endif
	}else if(strcmp(ucontrol->id.name, LGSE_SOUNDENGINE_INIT)==0){
		item[0] = lgse_cmd_info[mode_index].se_init;
		ALSA_DEBUG_INFO("[ALSA] init yet? %s\n",b_se_init?"TRUE":"FALSE");
	}else if(strcmp(ucontrol->id.name, LGSE_INIT_MAIN)==0){
		noParam = sizeof(AUDIO_RPC_LGSE00861)/sizeof(UINT32);
		lgse_move_parameter((UINT32*)data, (UINT32*)&lgse_cmd_info[mode_index].lgse00861, noParam);
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_MAIN)==0){
		noParam = sizeof(AUDIO_RPC_LGSE00876)/sizeof(UINT32);
		lgse_move_parameter((UINT32*)data, (UINT32*)&lgse_cmd_info[mode_index].lgse00876, noParam);
	}else if(strcmp(ucontrol->id.name, LGSE_INIT_FN000)==0){
		if((buf_index == 0) || (buf_index == 1))
			lgse_move_parameter((UINT32*)data, ((UINT32*)&lgse_cmd_info[mode_index].lgse00855)+(DATA_MAX_NUM * buf_index), DATA_MAX_NUM);
		else if(buf_index == 2)
			lgse_move_parameter((UINT32*)data, ((UINT32*)&lgse_cmd_info[mode_index].lgse00855)+(DATA_MAX_NUM * buf_index), 39);
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN000)==0){
		noParam = sizeof(AUDIO_RPC_LGSE00870)/sizeof(UINT32);
		lgse_move_parameter((UINT32*)data, (UINT32*)&lgse_cmd_info[mode_index].lgse00870, noParam);
	}else if(strcmp(ucontrol->id.name, LGSE_INIT_FN001)==0){
		noParam = sizeof(AUDIO_RPC_LGSE00854)/sizeof(UINT32);
		lgse_move_parameter((UINT32*)data, (UINT32*)&lgse_cmd_info[mode_index].lgse00854, noParam);
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN001)==0){
		if(buf_index == 0)
			lgse_move_parameter((UINT32*)data, (UINT32*)&lgse_cmd_info[mode_index].lgse00869, DATA_MAX_NUM);
		else if(buf_index == 1)
			lgse_move_parameter((UINT32*)data, ((UINT32*)&lgse_cmd_info[mode_index].lgse00869)+DATA_MAX_NUM, 2);
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN004)==0){
		noParam = 25;
		lgse_move_parameter((UINT32*)data, (UINT32*)&lgse_cmd_info[mode_index].fn004.data, noParam);
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN005)==0){
		noParam = 15;
		lgse_move_parameter((UINT32*)data, (UINT32*)&lgse_cmd_info[mode_index].fn005.data, noParam);
	}else if(strcmp(ucontrol->id.name, LGSE_INIT_FN008)==0){
		noParam = sizeof(AUDIO_RPC_LGSE00858)/sizeof(UINT32);
		lgse_move_parameter((UINT32*)data, (UINT32*)&lgse_cmd_info[mode_index].lgse00858, noParam);
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN008)==0){
		noParam = sizeof(AUDIO_RPC_LGSE00873)/sizeof(UINT32);
		lgse_move_parameter((UINT32*)data, (UINT32*)&lgse_cmd_info[mode_index].lgse00873, noParam);
	}else if(strcmp(ucontrol->id.name, LGSE_INIT_FN009)==0){
		noParam = sizeof(AUDIO_RPC_LGSE00857)/sizeof(UINT32);
		lgse_move_parameter((UINT32*)data, (UINT32*)&lgse_cmd_info[mode_index].lgse00857, noParam);
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN009)==0){
		noParam = sizeof(AUDIO_RPC_LGSE00872)/sizeof(UINT32);
		lgse_move_parameter((UINT32*)data, (UINT32*)&lgse_cmd_info[mode_index].lgse00872, noParam);
	}else if(strcmp(ucontrol->id.name, LGSE_INIT_FN010)==0){
		noParam = sizeof(AUDIO_RPC_LGSE00859)/sizeof(UINT32);
		lgse_move_parameter((UINT32*)data, (UINT32*)&lgse_cmd_info[mode_index].lgse00859, noParam);
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN010)==0){
		noParam = sizeof(AUDIO_RPC_LGSE00874)/sizeof(UINT32);
		lgse_move_parameter((UINT32*)data, (UINT32*)&lgse_cmd_info[mode_index].lgse00874, noParam);
	}else if(strcmp(ucontrol->id.name, LGSE_INIT_FN014)==0){
		if(buf_index == 0)
			lgse_move_parameter((UINT32*)data, (UINT32*)&lgse_cmd_info[mode_index].lgse00853, DATA_MAX_NUM);
		else if(buf_index == 1)
			lgse_move_parameter((UINT32*)data, ((UINT32*)&lgse_cmd_info[mode_index].lgse00853)+DATA_MAX_NUM, 8);
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN014)==0){
		noParam = sizeof(AUDIO_RPC_LGSE00868)/sizeof(UINT32);
		lgse_move_parameter((UINT32*)data, (UINT32*)&lgse_cmd_info[mode_index].lgse00868, noParam);
	}else if(strcmp(ucontrol->id.name, LGSE_INIT_FN016)==0){
		noParam = sizeof(AUDIO_RPC_LGSE03520)/sizeof(UINT32);
		lgse_move_parameter((UINT32*)data, (UINT32*)&lgse_cmd_info[mode_index].lgse03520, noParam);
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN016)==0){
		noParam = sizeof(AUDIO_RPC_LGSE03521)/sizeof(UINT32);
		lgse_move_parameter((UINT32*)data, (UINT32*)&lgse_cmd_info[mode_index].lgse03521, noParam);
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN017)==0){
		if(buf_index == 0)
			lgse_move_parameter((UINT32*)data, (UINT32*)&lgse_cmd_info[mode_index].lgse00879, DATA_MAX_NUM);
		else if(buf_index == 1)
			lgse_move_parameter((UINT32*)data, ((UINT32*)&lgse_cmd_info[mode_index].lgse00879)+DATA_MAX_NUM, 34);
	}else if(strcmp(ucontrol->id.name, LGSE_INIT_FN019)==0){
		noParam = sizeof(AUDIO_RPC_LGSE00856)/sizeof(UINT32);
		lgse_move_parameter((UINT32*)data, (UINT32*)&lgse_cmd_info[mode_index].lgse00856, noParam);
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN019)==0){
		noParam = sizeof(AUDIO_RPC_LGSE00871)/sizeof(UINT32);
		lgse_move_parameter((UINT32*)data, (UINT32*)&lgse_cmd_info[mode_index].lgse00871, noParam);
	}else if(strcmp(ucontrol->id.name, LGSE_INIT_FN022)==0){
		noParam = sizeof(AUDIO_RPC_LGSE00863)/sizeof(UINT32);
		lgse_move_parameter((UINT32*)data, (UINT32*)&lgse_cmd_info[mode_index].lgse00863, noParam);
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN022)==0){
		if((buf_index == 0) || (buf_index == 1) || (buf_index == 2))
			lgse_move_parameter((UINT32*)data, ((UINT32*)&lgse_cmd_info[mode_index].lgse00878)+(DATA_MAX_NUM * buf_index), DATA_MAX_NUM);
		else if(buf_index == 3)
			lgse_move_parameter((UINT32*)data, ((UINT32*)&lgse_cmd_info[mode_index].lgse00878)+(DATA_MAX_NUM * buf_index), 96);
	}else if(strcmp(ucontrol->id.name, LGSE_INIT_FN023)==0){ // not implement
		if(buf_index == 0)
			lgse_move_parameter((UINT32*)data, (UINT32*)&lgse_cmd_info[mode_index].fn023, DATA_MAX_NUM);
		else if(buf_index == 1)
			lgse_move_parameter((UINT32*)data, ((UINT32*)&lgse_cmd_info[mode_index].fn023)+DATA_MAX_NUM, 60);
	}else if(strcmp(ucontrol->id.name, LGSE_INIT_FN024)==0){
		noParam = sizeof(AUDIO_RPC_LGSE02624)/sizeof(UINT32);
		lgse_move_parameter((UINT32*)data, (UINT32*)&lgse_cmd_info[mode_index].lgse02624, noParam);
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN026)==0){
		if(buf_index == 0)
			lgse_move_parameter((UINT32*)data, (UINT32*)&lgse_cmd_info[mode_index].fn026, DATA_MAX_NUM);
		else if(buf_index == 1)
			lgse_move_parameter((UINT32*)data, ((UINT32*)&lgse_cmd_info[mode_index].fn026)+DATA_MAX_NUM, 72);
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN028)==0){
		noParam = sizeof(AUDIO_RPC_LGSE02871)/sizeof(UINT32);
		lgse_move_parameter((UINT32*)data, (UINT32*)&lgse_cmd_info[mode_index].lgse02871, noParam);
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN029)==0){
		if(buf_index == 0)
			lgse_move_parameter((UINT32*)data, (UINT32*)&lgse_cmd_info[mode_index].lgse02869, DATA_MAX_NUM);
		else if(buf_index == 1)
			lgse_move_parameter((UINT32*)data, ((UINT32*)&lgse_cmd_info[mode_index].lgse02869)+DATA_MAX_NUM, 72);
	}else if(strcmp(ucontrol->id.name, LGSE_VX_STATUS)==0){
		HAL_AUDIO_LGSE_VX_GetVxStatus((int*)&item[0], index);
	}else if(strcmp(ucontrol->id.name, LGSE_VX_PROCOUTPUTGAIN)==0){
		HAL_AUDIO_LGSE_VX_GetProcOutputGain((int*)&item[0], index);
	}else if(strcmp(ucontrol->id.name, LGSE_VX_TSX)==0){
		HAL_AUDIO_LGSE_VX_GetTsxStatus((int*)&item[0], index);
	}else if(strcmp(ucontrol->id.name, LGSE_VX_TSXPASSIVEMATRIXUPMIX)==0){
		HAL_AUDIO_LGSE_VX_GetTsxPassiveMatrixUpmix((int*)&item[0], index);
	}else if(strcmp(ucontrol->id.name, LGSE_VX_TSXHEIGHTUPMIX)==0){
		HAL_AUDIO_LGSE_VX_GetTsxHeightUpmix((int*)&item[0], index);
	}else if(strcmp(ucontrol->id.name, LGSE_VX_TSXLPRGAIN)==0){
		HAL_AUDIO_LGSE_VX_GetTsxLPRGain((int*)&item[0], index);
	}else if(strcmp(ucontrol->id.name, LGSE_VX_TSXLPRGAINASINPUTCHANNEL)==0){
		HAL_AUDIO_LGSE_VX_GetTsxLPRGainAsInputChannel(0,(int*)&item[0], index);
		HAL_AUDIO_LGSE_VX_GetTsxLPRGainAsInputChannel(1,(int*)&item[1], index);
	}else if(strcmp(ucontrol->id.name, LGSE_VX_TSXHORIZVIREFF)==0){
		HAL_AUDIO_LGSE_VX_GetTsxHorizVirEff((int*)&item[0], index);
	}else if(strcmp(ucontrol->id.name, LGSE_VX_TSXHORIZVIREFFASINPUTCHANNEL)==0){
		HAL_AUDIO_LGSE_VX_GetTsxHorizVirEffAsInputChannel(0,(int*)&item[0], index);
		HAL_AUDIO_LGSE_VX_GetTsxHorizVirEffAsInputChannel(1,(int*)&item[1], index);
	}else if(strcmp(ucontrol->id.name, LGSE_VX_TSXHEIGHTMIXCOEF)==0){
		HAL_AUDIO_LGSE_VX_GetTsxHeightMixCoeff((int*)&item[0], index);
	}else if(strcmp(ucontrol->id.name, LGSE_VX_TSXHEIGHTMIXCOEFASINPUTCHANNEL)==0){
		HAL_AUDIO_LGSE_VX_GetTsxHeightMixCoeffAsInputChannel(0,(int*)&item[0], index);
		HAL_AUDIO_LGSE_VX_GetTsxHeightMixCoeffAsInputChannel(1,(int*)&item[1], index);
	}else if(strcmp(ucontrol->id.name, LGSE_VX_CERTPARAM)==0){
		item[0] = lgse_cmd_info[mode_index].bEnable_CertParam;
	}else if(strcmp(ucontrol->id.name, LGSE_VX_CENTERGAIN)==0){
		HAL_AUDIO_LGSE_VX_GetCenterGain((int*)&item[0], index);
	}else if(strcmp(ucontrol->id.name, LGSE_POSTPROCESS)==0){
		HAL_AUDIO_LGSE_GetPostProcess(&lgse_cmd_info[mode_index].postgain, index);
		item[0] = lgse_cmd_info[mode_index].postgain;
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_SURROUNDVIRTUALIZERMODE)==0){
		HAL_AUDIO_LGSE_DAP_GetSurroundVirtualizerMode(&lgse_cmd_info[mode_index].virtualizer_mode, index);
		item[0] = lgse_cmd_info[mode_index].virtualizer_mode;
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_DIALOGUEENHANCER)==0){
		HAL_AUDIO_LGSE_DAP_GetDialogueEnhancer(&lgse_cmd_info[mode_index].DialogueEnhancer, index);
		item[0] = lgse_cmd_info[mode_index].DialogueEnhancer.de_enable;
		item[1] = lgse_cmd_info[mode_index].DialogueEnhancer.de_amount;
		item[2] = lgse_cmd_info[mode_index].DialogueEnhancer.de_ducking;
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_VOLUMELEVELER)==0){
		HAL_AUDIO_LGSE_DAP_GetVolumeLeveler(&lgse_cmd_info[mode_index].VolumeLeveler, index);
		item[0] = lgse_cmd_info[mode_index].VolumeLeveler.leveler_setting;
		item[1] = lgse_cmd_info[mode_index].VolumeLeveler.leveler_amount;
		item[2] = lgse_cmd_info[mode_index].VolumeLeveler.leveler_input;
		item[3] = lgse_cmd_info[mode_index].VolumeLeveler.leveler_output;
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_VOLUMEMODELER)==0){
		HAL_AUDIO_LGSE_DAP_GetVolumeModeler(&lgse_cmd_info[mode_index].VolumeModeler, index);
		item[0] = lgse_cmd_info[mode_index].VolumeModeler.modeler_enable;
		item[1] = lgse_cmd_info[mode_index].VolumeModeler.modeler_calibration;
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_VOLUMEMAXIMIZER)==0){
		HAL_AUDIO_LGSE_DAP_GetVolumeMaximizer(&lgse_cmd_info[mode_index].volmax_boost, index);
		item[0] = lgse_cmd_info[mode_index].volmax_boost;
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_OPTIMIZER)==0){
		int i,j;
		HAL_AUDIO_LGSE_DAP_GetOptimizer(&lgse_cmd_info[mode_index].Optimizer, index);
		if((index%10) == 0){
			item[0] = lgse_cmd_info[mode_index].Optimizer.optimizer_enable;
			item[1] = lgse_cmd_info[mode_index].Optimizer.optimizer_nb_bands;
			for(i = 0; i < 20; i++){
				item[2+i] = lgse_cmd_info[mode_index].Optimizer.a_opt_band_center_freq[i];
				for(j = 0; j < 5; j++)
					item[22+i+(j*20)] = lgse_cmd_info[mode_index].Optimizer.a_opt_band_gain[j][i];
			}
			for(i = 0; i < 6; i++){
				item[122+i] = lgse_cmd_info[mode_index].Optimizer.a_opt_band_gain[5][i];
			}
		} else if((index%10) & 1){
			for(i = 6; i < 20; i++)
				item[i-6] = lgse_cmd_info[mode_index].Optimizer.a_opt_band_gain[5][i];
			for(j = 6; j < 8; j++)
				for(i = 0; i < 20; i++)
					item[i+((j-6)*20)+14] = lgse_cmd_info[mode_index].Optimizer.a_opt_band_gain[j][i];
		}
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_PROCESSOPTIMIZER)==0){
		int i,j;
		HAL_AUDIO_LGSE_DAP_GetOptimizer(&lgse_cmd_info[mode_index].Optimizer, index);
		if((index%10) == 0){
			item[0] = lgse_cmd_info[mode_index].Optimizer.optimizer_enable;
			item[1] = lgse_cmd_info[mode_index].Optimizer.optimizer_nb_bands;
			for(i = 0; i < 20; i++){
				item[2+i] = lgse_cmd_info[mode_index].Optimizer.a_opt_band_center_freq[i];
				for(j = 0; j < 5; j++)
					item[22+i+(j*20)] = lgse_cmd_info[mode_index].Optimizer.a_opt_band_gain[j][i];
			}
			for(i = 0; i < 6; i++){
				item[122+i] = lgse_cmd_info[mode_index].Optimizer.a_opt_band_gain[5][i];
			}
		} else if((index%10) & 1){
			for(i = 6; i < 20; i++)
				item[i-6] = lgse_cmd_info[mode_index].Optimizer.a_opt_band_gain[5][i];
			for(j = 6; j < 8; j++)
				for(i = 0; i < 20; i++)
					item[i+((j-6)*20)+14] = lgse_cmd_info[mode_index].Optimizer.a_opt_band_gain[j][i];
		}
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_SURROUNDDECODER)==0){
		HAL_AUDIO_LGSE_DAP_GetSurroundDecoder(&lgse_cmd_info[mode_index].surround_decoder_enable, index);
		item[0] = lgse_cmd_info[mode_index].surround_decoder_enable;
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_SURROUNDCOMPRESSOR)==0){
		HAL_AUDIO_LGSE_DAP_GetSurroundCompressor(&lgse_cmd_info[mode_index].surround_boost, index);
		item[0] = lgse_cmd_info[mode_index].surround_boost;
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_VIRTUALIZERSPEAKERANGLE)==0){
		HAL_AUDIO_LGSE_DAP_GetVirtualizerSpeakerAngle(&lgse_cmd_info[mode_index].Speaker_angle, index);
		item[0] = lgse_cmd_info[mode_index].Speaker_angle.virtualizer_front_speaker_angle;
		item[1] = lgse_cmd_info[mode_index].Speaker_angle.virtualizer_surround_speaker_angle;
		item[2] = lgse_cmd_info[mode_index].Speaker_angle.virtualizer_height_speaker_angle;
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_INTELLIGENCEEQ)==0){
		int i;
		HAL_AUDIO_LGSE_DAP_GetIntelligenceEQ(&lgse_cmd_info[mode_index].Intelligence_eq, index);
		item[0] = lgse_cmd_info[mode_index].Intelligence_eq.ieq_enable;
		item[1] = lgse_cmd_info[mode_index].Intelligence_eq.ieq_nb_bands;
		for(i = 0; i < 20; i++){
			item[2+i]    = lgse_cmd_info[mode_index].Intelligence_eq.a_ieq_band_center[i];
			item[22+i] = lgse_cmd_info[mode_index].Intelligence_eq.a_ieq_band_target[i];
		}
		item[42] = lgse_cmd_info[mode_index].Intelligence_eq.ieq_amount;
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_MEIDAINTELLIGENCE)==0){
		HAL_AUDIO_LGSE_DAP_GetMediaIntelligence(&lgse_cmd_info[mode_index].MediaIntelligence, index);
		item[0] = lgse_cmd_info[mode_index].MediaIntelligence.mi_steering_enable;
		item[1] = lgse_cmd_info[mode_index].MediaIntelligence.mi_dv_enable;
		item[2] = lgse_cmd_info[mode_index].MediaIntelligence.mi_de_enable;
		item[3] = lgse_cmd_info[mode_index].MediaIntelligence.mi_surround_enable;
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_GRAPHICALEQ)==0){
		int i;
		HAL_AUDIO_LGSE_DAP_GetGraphicalEQ(&lgse_cmd_info[mode_index].GraphicalEQ, index);
		item[0] = lgse_cmd_info[mode_index].GraphicalEQ.eq_enable;
		item[1] = lgse_cmd_info[mode_index].GraphicalEQ.eq_nb_bands;
		for(i = 0; i < 20; i++){
			item[2+i]  = lgse_cmd_info[mode_index].GraphicalEQ.a_geq_band_center[i];
			item[22+i] = lgse_cmd_info[mode_index].GraphicalEQ.a_geq_band_target[i];
		}
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_PERCEPTUALHEIGHTFILTER)==0){
		HAL_AUDIO_LGSE_DAP_GetPerceptualHeightFilter(&lgse_cmd_info[mode_index].height_filter_mode, index);
		item[0] = lgse_cmd_info[mode_index].height_filter_mode;
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_BASSENHANCER)==0){
		HAL_AUDIO_LGSE_DAP_GetBassEnhancer(&lgse_cmd_info[mode_index].BassEnhancer, index);
		item[0] = lgse_cmd_info[mode_index].BassEnhancer.bass_enable;
		item[1] = lgse_cmd_info[mode_index].BassEnhancer.bass_boost ;
		item[2] = lgse_cmd_info[mode_index].BassEnhancer.bass_cutoff;
		item[3] = lgse_cmd_info[mode_index].BassEnhancer.bass_width ;
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_BASSEXTRACTION)==0){
		item[0] = lgse_cmd_info[mode_index].BassExtraction.bIsEnable;
		item[1] = lgse_cmd_info[mode_index].BassExtraction.cutoffFreq;
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_VIRTUALBASS)==0){
		HAL_AUDIO_LGSE_DAP_GetVirtualBass(&lgse_cmd_info[mode_index].VirtualBass, index);
		item[0] = lgse_cmd_info[mode_index].VirtualBass.vb_mode;
		item[1] = lgse_cmd_info[mode_index].VirtualBass.vb_low_src_freq;
		item[2] = lgse_cmd_info[mode_index].VirtualBass.vb_high_src_freq;
		item[3] = lgse_cmd_info[mode_index].VirtualBass.vb_overall_gain;
		item[4] = lgse_cmd_info[mode_index].VirtualBass.vb_slope_gain;
		item[5] = lgse_cmd_info[mode_index].VirtualBass.vb_subgain[0];
		item[6] = lgse_cmd_info[mode_index].VirtualBass.vb_subgain[1];
		item[7] = lgse_cmd_info[mode_index].VirtualBass.vb_subgain[2];
		item[8] = lgse_cmd_info[mode_index].VirtualBass.vb_mix_low_freq;
		item[9] = lgse_cmd_info[mode_index].VirtualBass.vb_mix_high_freq;
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_REGULATOR)==0){
		int i;
		HAL_AUDIO_LGSE_DAP_GetRegulator(&lgse_cmd_info[mode_index].Regulator, index);
		item[0] = lgse_cmd_info[mode_index].Regulator.regulator_enable;
		item[1] = lgse_cmd_info[mode_index].Regulator.reg_nb_bands;
		for(i = 0; i < 20; i++){
			item[2+i]  = lgse_cmd_info[mode_index].Regulator.a_reg_band_center[i];
			item[22+i] = lgse_cmd_info[mode_index].Regulator.a_reg_low_thresholds[i];
			item[42+i] = lgse_cmd_info[mode_index].Regulator.a_reg_high_thresholds[i];
			item[62+i] = lgse_cmd_info[mode_index].Regulator.a_reg_isolated_bands[i];
		}
		item[82] = lgse_cmd_info[mode_index].Regulator.regulator_overdrive;
		item[83] = lgse_cmd_info[mode_index].Regulator.regulator_timbre;
		item[84] = lgse_cmd_info[mode_index].Regulator.regulator_distortion;
		item[85] = lgse_cmd_info[mode_index].Regulator.regulator_mode;
	}
	return 0;
}

int snd_mars_ctl_lgse_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_card_mars *mars = snd_kcontrol_chip(kcontrol);
	unsigned long flags;
	int ret = NOT_OK;
	int noParam, dataOption, varOption;
	int index = ucontrol->id.index;
	int buf_index = ((ucontrol->id.index)%10)%4;
	int mode_index = ((ucontrol->id.index)/10)%3;
	unsigned char *data = ucontrol->value.bytes.data;
	unsigned int *item  = ucontrol->value.enumerated.item;

	ALSA_DEBUG_INFO("[ALSA] put name \033[31m%s\033[0m index %d item0 %d\n", ucontrol->id.name, index, item[0]);
	if(strcmp(ucontrol->id.name, LGSE_SOUNDENGINE_MODE)==0){
		ret = HAL_AUDIO_LGSE_SetSoundEngineMode(lgse_cmd_info[mode_index].se_init, (lgse_se_mode_ext_type_t)item[0], index);
		if(ret == OK){
			lgse_cmd_info[mode_index].mode = item[0];
		}
	}else if(strcmp(ucontrol->id.name, LGSE_MODE)==0){
		if(lgse_cmd_info[mode_index].flag == 0x3 && item[0] != 0x3) // DAP on->off mute protect
			Switch_OnOff_MuteProtect(60);
		if(lgse_cmd_info[mode_index].flag != 0x3 && item[0] == 0x3) // DAP off->on mute protect
			Switch_OnOff_MuteProtect(60);
		spin_lock_irqsave(&mars->mixer_lock, flags);
		lgse_cmd_info[mode_index].flag = item[0];
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		ret = HAL_AUDIO_LGSE_SetMode(lgse_cmd_info[mode_index].flag, 1, HAL_AUDIO_LGSE_INIT_ONLY, index);
	}else if(strcmp(ucontrol->id.name, LGSE_VERSION)==0){
		UINT32 version = 0;
		version = item[0];
		ret = OK;
	}else if(strcmp(ucontrol->id.name, LGSE_SOUNDENGINE_INIT)==0){
		if(b_se_init == 1){
			pr_emerg("[ALSA] multi-call LGSE SoundEngine Init!!\n");
		} else if(b_se_init == 0){
			int i=0;
			ret = HAL_AUDIO_LGSE_Init(item[0]);
			if(ret == OK){
				for(i=0;i<3;i++) //set same init value for all lgse set
					lgse_cmd_info[i].se_init = item[0];
				b_se_init = 1;
			}else{
				ALSA_WARN("[ALSA] LGSE SoundEngine Init fail!!\n");
				for(i=0;i<3;i++) //set same init value for all lgse set
					lgse_cmd_info[i].se_init = 0;
			}
		}
	}else if(strcmp(ucontrol->id.name, LGSE_INIT_MAIN)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		noParam = sizeof(AUDIO_RPC_LGSE00861)/sizeof(UINT32);
		lgse_move_parameter((UINT32*)&lgse_cmd_info[mode_index].lgse00861, (UINT32*)data, noParam);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		ret = HAL_AUDIO_LGSE_SetMain((int*)&lgse_cmd_info[mode_index].lgse00861, noParam, HAL_AUDIO_LGSE_INIT_ONLY, index);
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_MAIN)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		noParam = sizeof(AUDIO_RPC_LGSE00876)/sizeof(UINT32);
		lgse_move_parameter((UINT32*)&lgse_cmd_info[mode_index].lgse00876, (UINT32*)data, noParam);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		ret = HAL_AUDIO_LGSE_SetMain((int*)&lgse_cmd_info[mode_index].lgse00876, noParam, HAL_AUDIO_LGSE_VARIABLES, index);
	}else if(strcmp(ucontrol->id.name, LGSE_INIT_FN000)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		noParam = sizeof(AUDIO_RPC_LGSE00855)/sizeof(UINT32);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		if((buf_index == 1) || (buf_index == 0)){
			lgse_move_parameter(((UINT32*)&lgse_cmd_info[mode_index].lgse00855)+(DATA_MAX_NUM * buf_index), (UINT32*)data, DATA_MAX_NUM);
			ret = OK;
		} else if(buf_index == 2){
			lgse_move_parameter(((UINT32*)&lgse_cmd_info[mode_index].lgse00855)+(DATA_MAX_NUM * buf_index), (UINT32*)data, 39);
			ret = HAL_AUDIO_LGSE_SetFN000((int*)&lgse_cmd_info[mode_index].lgse00855, noParam, HAL_AUDIO_LGSE_INIT_ONLY, index);
		}
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN000)==0){
		if((UINT32*)data[0] != lgse_cmd_info[mode_index].lgse00870._LGSE00754){
			Switch_OnOff_MuteProtect(150); // clear voice, need long mute
			spin_lock_irqsave(&mars->mixer_lock, flags);
			noParam = sizeof(AUDIO_RPC_LGSE00870)/sizeof(UINT32);
			lgse_move_parameter((UINT32*)&lgse_cmd_info[mode_index].lgse00870, (UINT32*)data, noParam);
			spin_unlock_irqrestore(&mars->mixer_lock, flags);
			ret = HAL_AUDIO_LGSE_SetFN000((int*)&lgse_cmd_info[mode_index].lgse00870, noParam, HAL_AUDIO_LGSE_VARIABLES, index);
		} else{
			ALSA_DEBUG_INFO("[ALSA] %s index %d data:%x, same setting. ignore\n", ucontrol->id.name, index, (UINT32*)data[0]);
			ret = OK;
		}
	}else if(strcmp(ucontrol->id.name, LGSE_INIT_FN001)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		noParam = sizeof(AUDIO_RPC_LGSE00854)/sizeof(UINT32);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		lgse_move_parameter((UINT32*)&lgse_cmd_info[mode_index].lgse00854, (UINT32*)data, noParam);
		ret = HAL_AUDIO_LGSE_SetFN001((int*)&lgse_cmd_info[mode_index].lgse00854, noParam, HAL_AUDIO_LGSE_INIT_ONLY, index);
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN001)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		noParam = sizeof(AUDIO_RPC_LGSE00869)/sizeof(UINT32);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		if(buf_index == 0){
			lgse_move_parameter(((UINT32*)&lgse_cmd_info[mode_index].lgse00869), (UINT32*)data, DATA_MAX_NUM);
			ret = OK;
		} else if(buf_index == 1){
			lgse_move_parameter(((UINT32*)&lgse_cmd_info[mode_index].lgse00869)+DATA_MAX_NUM, (UINT32*)data, 2);
			ret = HAL_AUDIO_LGSE_SetFN001((int*)&lgse_cmd_info[mode_index].lgse00869, noParam, HAL_AUDIO_LGSE_VARIABLES, index);
		}
#if (ALSA_LGSE_VERSION == 70)
	}else if(strcmp(ucontrol->id.name, LGSE_INIT_FN002)==0 || strcmp(ucontrol->id.name, LGSE_VAR_FN002)==0){
		ret = HAL_AUDIO_LGSE_SetFN002((char**)bytes,0,0);
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN002_1)==0){
		ret = HAL_AUDIO_LGSE_SetFN002_1((char**)bytes,0,0);
#endif
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN004)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		noParam	= 25;
		dataOption = HAL_AUDIO_LGSE_VARIABLES;
		varOption  = HAL_AUDIO_LGSE_MODE_VARIABLESALL;
		lgse_move_parameter((UINT32*)&lgse_cmd_info[mode_index].fn004.data, (UINT32*)data, noParam);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		ret = HAL_AUDIO_LGSE_SetFN004((int*)&lgse_cmd_info[mode_index].fn004.data, noParam, dataOption, varOption, index);
#if (ALSA_LGSE_VERSION == 70)
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN004_1)==0){
		ret = HAL_AUDIO_LGSE_SetFN004_1((int*)bytes,0,0,0, index);
#endif
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN005)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		noParam = 15;
		lgse_move_parameter((UINT32*)&lgse_cmd_info[mode_index].fn005.data, (UINT32*)data, noParam);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		ret = HAL_AUDIO_LGSE_SetFN005((int*)&lgse_cmd_info[mode_index].fn005.data, noParam, HAL_AUDIO_LGSE_VARIABLES, index);
	}else if(strcmp(ucontrol->id.name, LGSE_INIT_FN008)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		noParam = sizeof(AUDIO_RPC_LGSE00858)/sizeof(UINT32);
		lgse_move_parameter((UINT32*)&lgse_cmd_info[mode_index].lgse00858, (UINT32*)data, noParam);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		ret = HAL_AUDIO_LGSE_SetFN008((int*)&lgse_cmd_info[mode_index].lgse00858, noParam, HAL_AUDIO_LGSE_INIT_ONLY, index);
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN008)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		noParam = sizeof(AUDIO_RPC_LGSE00873)/sizeof(UINT32);
		lgse_move_parameter((UINT32*)&lgse_cmd_info[mode_index].lgse00873, (UINT32*)data, noParam);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		ret = HAL_AUDIO_LGSE_SetFN008((int*)&lgse_cmd_info[mode_index].lgse00873, noParam, HAL_AUDIO_LGSE_VARIABLES, index);
#if (ALSA_LGSE_VERSION == 70)
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN008_1)==0){
		ret = HAL_AUDIO_LGSE_SetFN008_1((int*)bytes,0,0);
#endif
	}else if(strcmp(ucontrol->id.name, LGSE_INIT_FN009)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		noParam = sizeof(AUDIO_RPC_LGSE00857)/sizeof(UINT32);
		lgse_move_parameter((UINT32*)&lgse_cmd_info[mode_index].lgse00857, (UINT32*)data, noParam);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		ret = HAL_AUDIO_LGSE_SetFN009((int*)&lgse_cmd_info[mode_index].lgse00857, noParam, HAL_AUDIO_LGSE_INIT_ONLY, index);
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN009)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		noParam = sizeof(AUDIO_RPC_LGSE00872)/sizeof(UINT32);
		lgse_move_parameter((UINT32*)&lgse_cmd_info[mode_index].lgse00872, (UINT32*)data, noParam);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		ret = HAL_AUDIO_LGSE_SetFN009((int*)&lgse_cmd_info[mode_index].lgse00872, noParam, HAL_AUDIO_LGSE_VARIABLES, index);
	}else if(strcmp(ucontrol->id.name, LGSE_INIT_FN010)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		noParam = sizeof(AUDIO_RPC_LGSE00859)/sizeof(UINT32);
		lgse_move_parameter((UINT32*)&lgse_cmd_info[mode_index].lgse00859, (UINT32*)data, noParam);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		ret = HAL_AUDIO_LGSE_SetFN010((int*)&lgse_cmd_info[mode_index].lgse00859, noParam, HAL_AUDIO_LGSE_WRITE, index);
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN010)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		noParam = sizeof(AUDIO_RPC_LGSE00874)/sizeof(UINT32);
		lgse_move_parameter((UINT32*)&lgse_cmd_info[mode_index].lgse00874, (UINT32*)data, noParam);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		ret = HAL_AUDIO_LGSE_SetFN010((int*)data, noParam, HAL_AUDIO_LGSE_READ, index);
	}else if(strcmp(ucontrol->id.name, LGSE_OUT_FN010)==0){
#if 0
		/*ret = HAL_AUDIO_LGSE_SetFN010((int*)bytes,,);*/
#else
		ret = OK;
#endif
	}else if(strcmp(ucontrol->id.name, LGSE_INIT_FN014)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		noParam = sizeof(AUDIO_RPC_LGSE00853)/sizeof(UINT32);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		if(buf_index == 0){
			lgse_move_parameter(((UINT32*)&lgse_cmd_info[mode_index].lgse00853), (UINT32*)data, DATA_MAX_NUM);
			ret = OK;
		} else if(buf_index == 1){
			lgse_move_parameter(((UINT32*)&lgse_cmd_info[mode_index].lgse00853)+DATA_MAX_NUM, (UINT32*)data, 8);
			ret = HAL_AUDIO_LGSE_SetFN014((int*)&lgse_cmd_info[mode_index].lgse00853, noParam, HAL_AUDIO_LGSE_INIT_ONLY, index);
		}
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN014)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		noParam = sizeof(AUDIO_RPC_LGSE00868)/sizeof(UINT32);
		lgse_move_parameter((UINT32*)&lgse_cmd_info[mode_index].lgse00868, (UINT32*)data, noParam);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		ret = HAL_AUDIO_LGSE_SetFN014((int*)&lgse_cmd_info[mode_index].lgse00868, noParam, HAL_AUDIO_LGSE_VARIABLES, index);
	}else if(strcmp(ucontrol->id.name, LGSE_OUT_FN014)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		/*memcpy(bytes[buf_index], data, 512);*/
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
#if 0
		ret = HAL_AUDIO_LGSE_SetFN014((int*)bytes,,);
#else
		ret = OK;
#endif
	}else if(strcmp(ucontrol->id.name, LGSE_INIT_FN016)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		noParam = sizeof(AUDIO_RPC_LGSE03520)/sizeof(UINT32);
		lgse_move_parameter((UINT32*)&lgse_cmd_info[mode_index].lgse03520, (UINT32*)data, noParam);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		ret = HAL_AUDIO_LGSE_SetFN016((int*)&lgse_cmd_info[mode_index].lgse03520, noParam, HAL_AUDIO_LGSE_INIT_ONLY, index);
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN016)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		noParam = sizeof(AUDIO_RPC_LGSE03521)/sizeof(UINT32);
		lgse_move_parameter((UINT32*)&lgse_cmd_info[mode_index].lgse03521, (UINT32*)data, noParam);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		ret = HAL_AUDIO_LGSE_SetFN016((int*)&lgse_cmd_info[mode_index].lgse03521, noParam, HAL_AUDIO_LGSE_VARIABLES, index);
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN017)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		noParam = sizeof(AUDIO_RPC_LGSE00879)/sizeof(UINT32);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		if(buf_index == 0){
			lgse_move_parameter(((UINT32*)&lgse_cmd_info[mode_index].lgse00879), (UINT32*)data, DATA_MAX_NUM);
			ret = OK;
		} else if(buf_index == 1){
			lgse_move_parameter(((UINT32*)&lgse_cmd_info[mode_index].lgse00879)+DATA_MAX_NUM, (UINT32*)data, 34);
			ret = HAL_AUDIO_LGSE_SetFN017((int*)&lgse_cmd_info[mode_index].lgse00879, noParam, HAL_AUDIO_LGSE_VARIABLES, index);
		}
#if (ALSA_LGSE_VERSION == 70)
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN017_2)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		memcpy(bytes[buf_index], data, 512);
		noParam = sizeof(AUDIO_RPC_LGSE00879)/sizeof(UINT32);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		if(buf_index == 1)
			ret = HAL_AUDIO_LGSE_SetFN017_2((int*)bytes, noParam, HAL_AUDIO_LGSE_VARIABLES, index);
		else
			ret = OK;
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN017_3)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		memcpy(bytes[buf_index], data, 512);
		noParam = sizeof(AUDIO_RPC_LGSE00879)/sizeof(UINT32);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		if(buf_index == 1)
			ret = HAL_AUDIO_LGSE_SetFN017_3((int*)bytes, noParam, HAL_AUDIO_LGSE_VARIABLES, index);
		else
			ret = OK;
#endif
	}else if(strcmp(ucontrol->id.name, LGSE_INIT_FN019)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		noParam = sizeof(AUDIO_RPC_LGSE00856)/sizeof(UINT32);
		lgse_move_parameter((UINT32*)&lgse_cmd_info[mode_index].lgse00856, (UINT32*)data, noParam);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		ret = HAL_AUDIO_LGSE_SetFN019((int*)&lgse_cmd_info[mode_index].lgse00856, noParam, HAL_AUDIO_LGSE_INIT_ONLY, index);
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN019)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		noParam = sizeof(AUDIO_RPC_LGSE00871)/sizeof(UINT32);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		lgse_move_parameter((UINT32*)&lgse_cmd_info[mode_index].lgse00871, (UINT32*)data, noParam);
		ret = HAL_AUDIO_LGSE_SetFN019((int*)&lgse_cmd_info[mode_index].lgse00871, noParam, HAL_AUDIO_LGSE_VARIABLES, index);
#if (ALSA_LGSE_VERSION == 70)
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN019_1)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		noParam = sizeof(AUDIO_RPC_LGSE00871)/sizeof(UINT32);
		memcpy(bytes[buf_index], data, 512);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		if(buf_index == 1)
			ret = HAL_AUDIO_LGSE_SetFN019((int*)bytes, noParam, HAL_AUDIO_LGSE_VARIABLES, index);
		else
			ret = OK;
#endif
	}else if(strcmp(ucontrol->id.name, LGSE_INIT_FN022)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		noParam = sizeof(AUDIO_RPC_LGSE00863)/sizeof(UINT32);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		lgse_move_parameter((UINT32*)&lgse_cmd_info[mode_index].lgse00863, (UINT32*)data, noParam);
		ret = HAL_AUDIO_LGSE_SetFN022((int*)&lgse_cmd_info[mode_index].lgse00863, noParam, HAL_AUDIO_LGSE_INIT_ONLY, index);
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN022)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		noParam = sizeof(AUDIO_RPC_LGSE00878)/sizeof(UINT32);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		if((buf_index == 0) || (buf_index == 1) || (buf_index == 2)){
			lgse_move_parameter(((UINT32*)&lgse_cmd_info[mode_index].lgse00878)+(DATA_MAX_NUM * buf_index), (UINT32*)data, DATA_MAX_NUM);
			ret = OK;
		} else if(buf_index == 3){
			lgse_move_parameter(((UINT32*)&lgse_cmd_info[mode_index].lgse00878)+(DATA_MAX_NUM * buf_index), (UINT32*)data, 96);
			ret = HAL_AUDIO_LGSE_SetFN022((int*)&lgse_cmd_info[mode_index].lgse00878, noParam, HAL_AUDIO_LGSE_VARIABLES, index);
		}
#if (ALSA_LGSE_VERSION == 70)
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN022_1)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		noParam = sizeof(AUDIO_RPC_LGSE00878)/sizeof(UINT32);
		memcpy(bytes[buf_index], data, 512);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		if(buf_index == 3)
			ret = HAL_AUDIO_LGSE_SetFN022((int*)bytes, noParam, HAL_AUDIO_LGSE_VARIABLES, index);
		else
			ret = OK;
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN022_2)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		noParam = sizeof(AUDIO_RPC_LGSE00878)/sizeof(UINT32);
		memcpy(bytes[buf_index], data, 512);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		if(buf_index == 3)
			ret = HAL_AUDIO_LGSE_SetFN022((int*)bytes, noParam, HAL_AUDIO_LGSE_VARIABLES, index);
		else
			ret = OK;
#endif
	}else if(strcmp(ucontrol->id.name, LGSE_INIT_FN023)==0){ // not implement
		spin_lock_irqsave(&mars->mixer_lock, flags);
		noParam = sizeof(AUDIO_RPC_LGSE02624)/sizeof(UINT32);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		if(buf_index == 0){
			lgse_move_parameter((UINT32*)&lgse_cmd_info[mode_index].fn023, (UINT32*)data, DATA_MAX_NUM);
			ret = OK;
		} else if(buf_index == 1){
			lgse_move_parameter(((UINT32*)&lgse_cmd_info[mode_index].fn023)+DATA_MAX_NUM, (UINT32*)data, 60);
			ret = HAL_AUDIO_LGSE_SetFN023((int*)data, 0, HAL_AUDIO_LGSE_INIT_ONLY, index);
		}
	}else if(strcmp(ucontrol->id.name, LGSE_INIT_FN024)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		noParam = sizeof(AUDIO_RPC_LGSE02624)/sizeof(UINT32);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		lgse_move_parameter((UINT32*)&lgse_cmd_info[mode_index].lgse02624, (UINT32*)data, noParam);
		ret = HAL_AUDIO_LGSE_SetFN024((int*)&lgse_cmd_info[mode_index].lgse02624, noParam, HAL_AUDIO_LGSE_INIT_ONLY, index);
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN026)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		noParam = sizeof(AUDIO_RPC_LGSE_DATA026)/sizeof(UINT32);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		if(buf_index == 0){
			lgse_move_parameter(((UINT32*)&lgse_cmd_info[mode_index].fn026), (UINT32*)data, DATA_MAX_NUM);
			ret = OK;
		} else if(buf_index == 1){
			lgse_move_parameter(((UINT32*)&lgse_cmd_info[mode_index].fn026)+DATA_MAX_NUM, (UINT32*)data, 72);
			ret = HAL_AUDIO_LGSE_SetFN026((int*)&lgse_cmd_info[mode_index].fn026, noParam, HAL_AUDIO_LGSE_VARIABLES, index);
		}
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN028)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		noParam = sizeof(AUDIO_RPC_LGSE02871)/sizeof(UINT32);
		lgse_move_parameter((UINT32*)&lgse_cmd_info[mode_index].lgse02871, (UINT32*)data, noParam);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		ret = HAL_AUDIO_LGSE_SetFN028((int*)&lgse_cmd_info[mode_index].lgse02871, noParam, HAL_AUDIO_LGSE_VARIABLES, index);
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN029)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		noParam = sizeof(AUDIO_RPC_LGSE02869)/sizeof(UINT32);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		if(buf_index == 0){
			lgse_move_parameter(((UINT32*)&lgse_cmd_info[mode_index].lgse02869), (UINT32*)data, DATA_MAX_NUM);
			ret = OK;
		} else if(buf_index == 1){
			lgse_move_parameter(((UINT32*)&lgse_cmd_info[mode_index].lgse02869)+DATA_MAX_NUM, (UINT32*)data, 72);
			ret = HAL_AUDIO_LGSE_SetFN029((int*)&lgse_cmd_info[mode_index].lgse02869, noParam, HAL_AUDIO_LGSE_VARIABLES, index);
		}
#if (ALSA_LGSE_VERSION == 70)
	}else if(strcmp(ucontrol->id.name, LGSE_INIT_FN030)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		memcpy(bytes[buf_index], data, 512);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
#if 0
		ret = HAL_AUDIO_LGSE_SetFN030((int*)bytes, noParam, HAL_AUDIO_LGSE_INIT_ONLY, index);
#else
		ret = OK;
#endif
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN030)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		memcpy(bytes[buf_index], data, 512);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
#if 0
		ret = HAL_AUDIO_LGSE_SetFN030((int*)bytes, noParam, HAL_AUDIO_LGSE_VARIABLES, index);
#else
		ret = OK;
#endif
	}else if(strcmp(ucontrol->id.name, LGSE_INIT_FN030_1)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		memcpy(bytes[buf_index], data, 512);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
#if 0
		ret = HAL_AUDIO_LGSE_SetFN030_1((int*)bytes, noParam, HAL_AUDIO_LGSE_INIT_ONLY, index);
#else
		ret = OK;
#endif
	}else if(strcmp(ucontrol->id.name, LGSE_VAR_FN030_1)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		memcpy(bytes[buf_index], data, 512);
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
#if 0
		ret = HAL_AUDIO_LGSE_SetFN030_1((int*)bytes, noParam, HAL_AUDIO_LGSE_VARIABLES, index);
#else
		ret = OK;
#endif
#endif //end (ALSA_LGSE_VERSION == 70)
	}else if(strcmp(ucontrol->id.name, LGSE_VX_STATUS)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		lgse_cmd_info[mode_index].bOnOff_vx_st = ucontrol->value.enumerated.item[0];
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		if(!AUDIO_CHECK_ISBOOLEAN(lgse_cmd_info[mode_index].bOnOff_vx_st)){
			ret = NOT_OK;
		}else
			ret = HAL_AUDIO_LGSE_VX_SetVxStatus(lgse_cmd_info[mode_index].bOnOff_vx_st, index);
	}else if(strcmp(ucontrol->id.name, LGSE_VX_PROCOUTPUTGAIN)==0){
		ret = HAL_AUDIO_LGSE_VX_SetProcOutputGain(item[0], index);
	}else if(strcmp(ucontrol->id.name, LGSE_VX_TSX)==0){
		ret = HAL_AUDIO_LGSE_VX_EnableTsx(item[0], index);
	}else if(strcmp(ucontrol->id.name, LGSE_VX_TSXPASSIVEMATRIXUPMIX)==0){
		ret = HAL_AUDIO_LGSE_VX_SetTsxPassiveMatrixUpmix(item[0], index);
	}else if(strcmp(ucontrol->id.name, LGSE_VX_TSXHEIGHTUPMIX)==0){
		ret = HAL_AUDIO_LGSE_VX_SetTsxHeightUpmix(item[0], index);
	}else if(strcmp(ucontrol->id.name, LGSE_VX_TSXLPRGAIN)==0){
		ret = HAL_AUDIO_LGSE_VX_SetTsxLPRGain(item[0], index);
	}else if(strcmp(ucontrol->id.name, LGSE_VX_TSXLPRGAINASINPUTCHANNEL)==0){
		if((!AUDIO_CHECK_ISVALID(item[0],0,1)) || (!AUDIO_CHECK_ISVALID(item[1], 0, 1073741824))){
			ret = NOT_OK;
		}else
			ret = HAL_AUDIO_LGSE_VX_SetTsxLPRGainAsInputChannel(item[0], item[1], index);
	}else if(strcmp(ucontrol->id.name, LGSE_VX_TSXHORIZVIREFF)==0){
		ret = HAL_AUDIO_LGSE_VX_SetTsxHorizVirEff(item[0], index);
	}else if(strcmp(ucontrol->id.name, LGSE_VX_TSXHORIZVIREFFASINPUTCHANNEL)==0){
		if(AUDIO_CHECK_ISVALID(item[0],0,1))
			ret = HAL_AUDIO_LGSE_VX_SetTsxHorizVirEffAsInputChannel(item[0], item[1], index);
	}else if(strcmp(ucontrol->id.name, LGSE_VX_TSXHEIGHTMIXCOEF)==0){
		ret = HAL_AUDIO_LGSE_VX_SetTsxHeightMixCoeff(item[0], index);
	}else if(strcmp(ucontrol->id.name, LGSE_VX_TSXHEIGHTMIXCOEFASINPUTCHANNEL)==0){
		ret = HAL_AUDIO_LGSE_VX_SetTsxHeightMixCoeffAsInputChannel(item[0], item[1], index);
	}else if(strcmp(ucontrol->id.name, LGSE_VX_CERTPARAM)==0){
		lgse_cmd_info[mode_index].bEnable_CertParam = item[0];
		if(!AUDIO_CHECK_ISBOOLEAN(lgse_cmd_info[mode_index].bEnable_CertParam)){
			ret = NOT_OK;
		}else
			ret = HAL_AUDIO_LGSE_VX_SetCertParam(lgse_cmd_info[mode_index].bEnable_CertParam, index);
	}else if(strcmp(ucontrol->id.name, LGSE_VX_CENTERGAIN)==0){
		ret = HAL_AUDIO_LGSE_VX_SetCenterGain(item[0], index);
	}else if(strcmp(ucontrol->id.name, LGSE_POSTPROCESS)==0){
		if(!AUDIO_CHECK_ISVALID(item[0],
					LGSE_POSTPROC_LGSE_MODE, LGSE_POSTPROC_DAP_MODE)){
			ret = NOT_OK;
		}else
			ret = HAL_AUDIO_LGSE_SetPostProcess(item[0], index);
		if(ret == OK)
			lgse_cmd_info[mode_index].postgain = item[0];
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_SURROUNDVIRTUALIZERMODE)==0){
		if(!AUDIO_CHECK_ISVALID(item[0],
					LGSE_DAP_SURROUND_VIRTUALIZER_OFF, LGSE_DAP_SURROUND_VIRTUALIZER_ON_2_0_2)){
			ret = NOT_OK;
		}else
			ret = HAL_AUDIO_LGSE_DAP_SetSurroundVirtualizerMode(item[0], index);
		if(ret == OK){
			lgse_cmd_info[mode_index].virtualizer_mode = item[0];
		}
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_DIALOGUEENHANCER)==0){
		DAP_PARAM_DIALOGUE_ENHANCER DialogueEnhancer;
		DialogueEnhancer.de_enable  = item[0];
		DialogueEnhancer.de_amount  = item[1];
		DialogueEnhancer.de_ducking = item[2];
		if(!AUDIO_CHECK_ISBOOLEAN(DialogueEnhancer.de_enable)){
			ret = NOT_OK;
		}else
			ret = HAL_AUDIO_LGSE_DAP_SetDialogueEnhancer(DialogueEnhancer, index);
		if(ret == OK){
			memcpy(&lgse_cmd_info[mode_index].DialogueEnhancer, &DialogueEnhancer, sizeof(DAP_PARAM_DIALOGUE_ENHANCER));
		}
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_VOLUMELEVELER)==0){
		DAP_PARAM_VOLUME_LEVELER VolumeLeveler;
		VolumeLeveler.leveler_setting = item[0];
		VolumeLeveler.leveler_amount  = item[1];
		VolumeLeveler.leveler_input   = item[2];
		VolumeLeveler.leveler_output  = item[3];
		if(!AUDIO_CHECK_ISBOOLEAN(VolumeLeveler.leveler_setting)){
			ret = NOT_OK;
		}else
			ret = HAL_AUDIO_LGSE_DAP_SetVolumeLeveler(VolumeLeveler, index);
		if(ret == OK){
			memcpy(&lgse_cmd_info[mode_index].VolumeLeveler, &VolumeLeveler, sizeof(DAP_PARAM_VOLUME_LEVELER));
		}
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_VOLUMEMODELER)==0){
		DAP_PARAM_VOLUME_MODELER VolumeModeler;
		VolumeModeler.modeler_enable      = item[0];
		VolumeModeler.modeler_calibration = item[1];
		if(!AUDIO_CHECK_ISBOOLEAN(VolumeModeler.modeler_enable)){
			ret = NOT_OK;
		}else
			ret = HAL_AUDIO_LGSE_DAP_SetVolumeModeler(VolumeModeler, index);
		if(ret == OK){
			memcpy(&lgse_cmd_info[mode_index].VolumeModeler, &VolumeModeler, sizeof(DAP_PARAM_VOLUME_MODELER));
		}
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_VOLUMEMAXIMIZER)==0){
		if(!AUDIO_CHECK_ISVALID(item[0], 0, 192)){
			ret = NOT_OK;
		}else
			ret = HAL_AUDIO_LGSE_DAP_SetVolumeMaximizer(item[0], index);
		if(ret == OK){
			lgse_cmd_info[mode_index].volmax_boost = item[0];
		}
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_OPTIMIZER)==0){
		int i,j;
		DAP_PARAM_AUDIO_OPTIMIZER Optimizer;
		if((index%10) == 0){
			Optimizer.optimizer_enable   = item[0];
			Optimizer.optimizer_nb_bands = item[1];
			for(i = 0; i < 20; i++){
				Optimizer.a_opt_band_center_freq[i] = item[2+i];
				for(j = 0; j < 5; j++)
					Optimizer.a_opt_band_gain[j][i] = item[22+i+(j*20)];
			}
			for(i = 0; i < 6; i++)
				Optimizer.a_opt_band_gain[5][i] = item[122+i];

			if(!AUDIO_CHECK_ISBOOLEAN(Optimizer.optimizer_enable)){
				ret = NOT_OK;
			}else if(!AUDIO_CHECK_ISVALID(Optimizer.optimizer_nb_bands, 1, 20)){
				ret = NOT_OK;
			}else{
				ret = HAL_AUDIO_LGSE_DAP_SetOptimizer(Optimizer, index);
			}
			if(ret == OK){
				memcpy(&lgse_cmd_info[mode_index].Optimizer, &Optimizer, sizeof(DAP_PARAM_AUDIO_OPTIMIZER));
			}
		} else if((index%10) & 1){
			for(i = 6; i < 20; i++)
				Optimizer.a_opt_band_gain[5][i] = item[i-6];
			for(j = 6; j < 8; j++)
				for(i = 0; i < 20; i++)
					Optimizer.a_opt_band_gain[j][i] = item[i+((j-6)*20)+14];
			ret = OK;
		}
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_PROCESSOPTIMIZER)==0){
		int i,j;
		DAP_PARAM_AUDIO_OPTIMIZER Optimizer;
		if((index%10) == 0){
			Optimizer.optimizer_enable   = item[0];
			Optimizer.optimizer_nb_bands = item[1];
			for(i = 0; i < 20; i++){
				Optimizer.a_opt_band_center_freq[i] = item[2+i];
				for(j = 0; j < 5; j++)
					Optimizer.a_opt_band_gain[j][i] = item[22+i+(j*20)];
			}
			for(i = 0; i < 6; i++)
				Optimizer.a_opt_band_gain[5][i] = item[122+i];

			if(!AUDIO_CHECK_ISBOOLEAN(Optimizer.optimizer_enable)){
				ret = NOT_OK;
			}else
				ret = HAL_AUDIO_LGSE_DAP_SetOptimizer(Optimizer, index);  /* same as LGSE DAP Optimizer*/
			if(!AUDIO_CHECK_ISVALID(Optimizer.optimizer_nb_bands, 1, 20)){
				// for socts, just warning
				ALSA_WARN("[%s] WARN optimizer_nb_bands = %x\n",ucontrol->id.name,
						lgse_cmd_info[mode_index].Optimizer.optimizer_nb_bands);
			}
			if(ret == OK){
				memcpy(&lgse_cmd_info[mode_index].Optimizer, &Optimizer, sizeof(DAP_PARAM_AUDIO_OPTIMIZER));
			}
		} else if((index%10) & 1){
			for(i = 6; i < 20; i++)
				Optimizer.a_opt_band_gain[5][i] = item[i-6];
			for(j = 6; j < 8; j++)
				for(i = 0; i < 20; i++)
					Optimizer.a_opt_band_gain[j][i] = item[i+((j-6)*20)+14];
			ret = OK;
		}
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_SURROUNDDECODER)==0){
		if(!AUDIO_CHECK_ISBOOLEAN(item[0])){
			ret = NOT_OK;
		}else
			ret = HAL_AUDIO_LGSE_DAP_SetSurroundDecoder(item[0], index);
		if(ret == OK){
			lgse_cmd_info[mode_index].surround_decoder_enable = item[0];
		}
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_SURROUNDCOMPRESSOR)==0){
		if(!AUDIO_CHECK_ISVALID(item[0], 0, 96)){
			ret = NOT_OK;
		}else
			ret = HAL_AUDIO_LGSE_DAP_SetSurroundCompressor(item[0], index);
		if(ret == OK){
			lgse_cmd_info[mode_index].surround_boost = item[0];
		}
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_VIRTUALIZERSPEAKERANGLE)==0){
		DAP_PARAM_VIRTUALIZER_SPEAKER_ANGLE Speaker_angle;
		Speaker_angle.virtualizer_front_speaker_angle    = item[0];
		Speaker_angle.virtualizer_surround_speaker_angle = item[1];
		Speaker_angle.virtualizer_height_speaker_angle   = item[2];
		if(!AUDIO_CHECK_ISVALID(Speaker_angle.virtualizer_front_speaker_angle, 0, 30) ||
				!AUDIO_CHECK_ISVALID(Speaker_angle.virtualizer_surround_speaker_angle, 0, 30) ||
				!AUDIO_CHECK_ISVALID(Speaker_angle.virtualizer_height_speaker_angle, 0, 30)){
			ret = NOT_OK;
		}else
			ret = HAL_AUDIO_LGSE_DAP_SetVirtualizerSpeakerAngle(Speaker_angle, index);
		if(ret == OK){
			memcpy(&lgse_cmd_info[mode_index].Speaker_angle, &Speaker_angle, sizeof(DAP_PARAM_VIRTUALIZER_SPEAKER_ANGLE));
		}
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_INTELLIGENCEEQ)==0){
		int i;
		ret = OK;
		DAP_PARAM_INTELLIGENT_EQUALIZER Intelligence_eq;
		Intelligence_eq.ieq_enable = item[0];
		Intelligence_eq.ieq_nb_bands = item[1];
		for(i = 0; i < 20; i++){
			Intelligence_eq.a_ieq_band_center[i] = item[2+i];
			Intelligence_eq.a_ieq_band_target[i] = item[22+i];
		}
		Intelligence_eq.ieq_amount = item[42];
		if(!AUDIO_CHECK_ISVALID(Intelligence_eq.ieq_nb_bands, 1, 20)){
			ret = NOT_OK;
		}
		for(i = 0; i < 20; i++){ //for socts, just warning
			if(!AUDIO_CHECK_ISVALID(Intelligence_eq.a_ieq_band_center[i], 20, 20000)){
				ALSA_WARN("[ALSA][%s] WARN a_ieq_band_center[%d] = %x\n",ucontrol->id.name,i,
						lgse_cmd_info[mode_index].Intelligence_eq.a_ieq_band_center[i]);
			}
		}
		if(ret == OK)
			ret = HAL_AUDIO_LGSE_DAP_SetIntelligenceEQ(Intelligence_eq, index);
		if(ret == OK){
			memcpy(&lgse_cmd_info[mode_index].Intelligence_eq, &Intelligence_eq, sizeof(DAP_PARAM_INTELLIGENT_EQUALIZER));
		}
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_MEIDAINTELLIGENCE)==0){
		DAP_PARAM_MEDIA_INTELLIGENCE MediaIntelligence;
		MediaIntelligence.mi_steering_enable = item[0];
		MediaIntelligence.mi_dv_enable       = item[1];
		MediaIntelligence.mi_de_enable       = item[2];
		MediaIntelligence.mi_surround_enable = item[3];
		if((!AUDIO_CHECK_ISBOOLEAN(MediaIntelligence.mi_steering_enable)) ||
				!AUDIO_CHECK_ISBOOLEAN(MediaIntelligence.mi_dv_enable) ||
				!AUDIO_CHECK_ISBOOLEAN(MediaIntelligence.mi_de_enable) ||
				!AUDIO_CHECK_ISBOOLEAN(MediaIntelligence.mi_surround_enable)){
			ret = NOT_OK;
		}else
			ret = HAL_AUDIO_LGSE_DAP_SetMediaIntelligence(MediaIntelligence, index);
		if(ret == OK){
			memcpy(&lgse_cmd_info[mode_index].MediaIntelligence, &MediaIntelligence, sizeof(DAP_PARAM_MEDIA_INTELLIGENCE));
		}
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_GRAPHICALEQ)==0){
		int i;
		DAP_PARAM_GRAPHICAL_EQUALIZER GraphicalEQ;
		GraphicalEQ.eq_enable   = item[0];
		GraphicalEQ.eq_nb_bands = item[1];
		for(i = 0; i < 20; i++){
			GraphicalEQ.a_geq_band_center[i] = item[2+i];
			GraphicalEQ.a_geq_band_target[i] = item[22+i];
		}
		if(!AUDIO_CHECK_ISBOOLEAN(GraphicalEQ.eq_enable)){
			ret = NOT_OK;
		}else if(!AUDIO_CHECK_ISVALID(GraphicalEQ.eq_nb_bands, 1, 20)){
			ret = NOT_OK;
		}else
			ret = HAL_AUDIO_LGSE_DAP_SetGraphicalEQ(GraphicalEQ, index);
		if(ret == OK){
			memcpy(&lgse_cmd_info[mode_index].GraphicalEQ, &GraphicalEQ, sizeof(DAP_PARAM_GRAPHICAL_EQUALIZER));
		}
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_PERCEPTUALHEIGHTFILTER)==0){
		if(!AUDIO_CHECK_ISVALID(item[0],
					LGSE_DAP_HEIGHT_FILTER_DISABLED, LGSE_DAP_HEIGHT_FILTER_UP_FIRING)){
			ret = NOT_OK;
		}else
			ret = HAL_AUDIO_LGSE_DAP_SetPerceptualHeightFilter(item[0], index);
		if(ret == OK){
			lgse_cmd_info[mode_index].height_filter_mode = item[0];
		}
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_BASSENHANCER)==0){
		DAP_PARAM_BASS_ENHANCER BassEnhancer;
		BassEnhancer.bass_enable = item[0];
		BassEnhancer.bass_boost  = item[1];
		BassEnhancer.bass_cutoff = item[2];
		BassEnhancer.bass_width  = item[3];
		if(!AUDIO_CHECK_ISBOOLEAN(BassEnhancer.bass_enable)){
			ret = NOT_OK;
		}else if(!AUDIO_CHECK_ISVALID(BassEnhancer.bass_boost, 0, 24)){
			ret = NOT_OK;
		}else
			ret = HAL_AUDIO_LGSE_DAP_SetBassEnhancer(BassEnhancer, index);
		if(ret == OK){
			memcpy(&lgse_cmd_info[mode_index].BassEnhancer, &BassEnhancer, sizeof(DAP_PARAM_BASS_ENHANCER));
		}
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_BASSEXTRACTION)==0){
		HAL_AUDIO_LGSE_DAP_BASS_EXTRACTION_T BassExtraction;
		BassExtraction.bIsEnable  = item[0];
		BassExtraction.cutoffFreq = item[1];
		if(!AUDIO_CHECK_ISBOOLEAN(BassExtraction.bIsEnable)){
			ret = NOT_OK;
		}else if(!AUDIO_CHECK_ISVALID(BassExtraction.cutoffFreq, 45, 200)){
			ret = NOT_OK;
		}else
			ret = HAL_AUDIO_LGSE_DAP_SetBassExtraction(BassExtraction, index);
		if(ret == OK){
			memcpy(&lgse_cmd_info[mode_index].BassExtraction, &BassExtraction, sizeof(HAL_AUDIO_LGSE_DAP_BASS_EXTRACTION_T));
		}
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_VIRTUALBASS)==0){
		DAP_PARAM_VIRTUAL_BASS VirtualBass;
		VirtualBass.b_virtual_bass_enabled = 1;
		VirtualBass.vb_mode          = item[0];
		VirtualBass.vb_low_src_freq  = item[1];
		VirtualBass.vb_high_src_freq = item[2];
		VirtualBass.vb_overall_gain  = item[3];
		VirtualBass.vb_slope_gain    = item[4];
		VirtualBass.vb_subgain[0]    = item[5];
		VirtualBass.vb_subgain[1]    = item[6];
		VirtualBass.vb_subgain[2]    = item[7];
		VirtualBass.vb_mix_low_freq  = item[8];
		VirtualBass.vb_mix_high_freq = item[9];
		if(!AUDIO_CHECK_ISVALID(VirtualBass.vb_mode,
					LGSE_DAP_VIRTUAL_BASS_MODE_DELAYONLY, LGSE_DAP_SYS_PARAM_VIRUTAL_BASS_MODE_4TH_ORDER)){
			ret = NOT_OK;
		}else
			ret = HAL_AUDIO_LGSE_DAP_SetVirtualBass(VirtualBass, index);
		if(ret == OK){
			memcpy(&lgse_cmd_info[mode_index].VirtualBass, &VirtualBass, sizeof(DAP_PARAM_VIRTUAL_BASS));
		}
	}else if(strcmp(ucontrol->id.name, LGSE_DAP_REGULATOR)==0){
		int i;
		DAP_PARAM_AUDIO_REGULATOR Regulator;
		Regulator.regulator_enable = item[0];
		Regulator.reg_nb_bands = item[1];
		for(i = 0; i < 20; i++){
			Regulator.a_reg_band_center[i]     = item[2+i];
			Regulator.a_reg_low_thresholds[i]  = item[22+i];
			Regulator.a_reg_high_thresholds[i] = item[42+i];
			Regulator.a_reg_isolated_bands[i]  = item[62+i];
		}
		Regulator.regulator_overdrive  = item[82];
		Regulator.regulator_timbre     = item[83];
		Regulator.regulator_distortion = item[84];
		Regulator.regulator_mode       = item[85];
		if(!AUDIO_CHECK_ISBOOLEAN(Regulator.regulator_enable)){
			ret = NOT_OK;
		}else if(!AUDIO_CHECK_ISVALID(Regulator.reg_nb_bands, 1, 20)){
			ret = NOT_OK;
		}else
			ret = HAL_AUDIO_LGSE_DAP_SetRegulator(Regulator, index);
		if(ret == OK){
			memcpy(&lgse_cmd_info[mode_index].Regulator, &Regulator, sizeof(DAP_PARAM_AUDIO_REGULATOR));
		}
	}

	return ret;
}

