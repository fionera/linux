#include "kadp_audio.h"
#include "audio_rpc.h"
#include <linux/string.h>

/************************************************************************
 *  Definitions
 ************************************************************************/

#define IOCTL_RET_ERROR  (-1)
#define IOCTL_FD_NULL    (-2)
#define IOCTL_ADDR_NULL  (-3)
#define IOCTL_TYPE_ERROR (-4)

char Err_Status[4][16] = {
    "DRIVER_ERROR",
    "FD_NULL",
    "ADDR_NULL",
    "TYPE_ERROR",
};

typedef struct
{
    char name[12];
    int connectStatus[10];
    int inputConnectResourceId[10]; //HAL_AUDIO_RESOURCE_T
    int outputConnectResourceID[10];//HAL_AUDIO_RESOURCE_T
    int numOptconnect;
    int numIptconnect;
    int maxVaildIptNum; // output moduel used

} AUDIO_MODULE_STATUS;

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
    unsigned char IsSystemOutput;
    unsigned char IsDTV0SourceRead; // ATP
    unsigned char IsDTV1SourceRead;
    unsigned char IsMainPPAOExist;
    unsigned char IsSubPPAOExist;
} AUDIO_FLOW_STATUS;

typedef struct
{
    int ainPinStatus[9];
} AUDIO_AIN_MODULE_STATUS;

typedef struct
{
    int SEPinStatus;
} AUDIO_SE_MODULE_STATUS;

typedef struct
{
    UINT32 decMute;
    UINT32 decDelay;
    UINT32 decVol[8];
} ADSP_DEC_INFO;

 /************************************************************************
 *  Static variables
 ************************************************************************/

static KADP_AUDIO_SPDIF_CHANNEL_STATUS_SOUNDBAR Aud_SB_ChannelStatus = {0};

static char Aud_trickPlayState[2] = {0};
extern long rtkaudio_send_audio_config(AUDIO_CONFIG_COMMAND * cmd);

 /************************************************************************
 *  Function body
 ************************************************************************/

/* read from kernel driver with ioctl */
int rtkaudio_ioctl_get(int cntl_type, void *addr, int size)
{
    if(cntl_type >= MAX_CONTROL_NUM) return IOCTL_TYPE_ERROR;

    switch(cntl_type)
    {
    case VIRTUALX_CMD_INFO:
        return RTKAUDIO_RPC_TOAGENT_VX_PRIVATEINFO_SVC((virtualx_cmd_info*)addr);
        break;
    case MAIN_DECODER_STATUS:
        break;
    case ALSA_DEV_INFO:
        break;
    case GST_DEBUG_INFO:
        break;
    default:
        break;
    }
    return 0;
}

/* write to kernel driver with ioctl */
int rtkaudio_ioctl_set(int cntl_type, void *addr, int size)
{
    if(cntl_type >= MAX_CONTROL_NUM) return IOCTL_TYPE_ERROR;

    switch(cntl_type)
    {
    case VIRTUALX_CMD_INFO:
        return RTKAUDIO_RPC_TOAGENT_VX_PRIVATEINFO_SVC((virtualx_cmd_info*)addr);
        break;
    case MAIN_DECODER_STATUS:
        break;
    case ALSA_DEV_INFO:
        break;
    case GST_DEBUG_INFO:
        break;
    default:
        break;
    }
    return 0;
}

KADP_STATUS_T KADP_AUDIO_SB_SetChannelStatus(KADP_AUDIO_SPDIF_CHANNEL_STATUS_SOUNDBAR pCS)
{
    struct AUDIO_CONFIG_COMMAND audioConfig;
    int i;
    UINT32 res;
    unsigned int cs[6] = {0};

    //keep a cs status, but maybe it should get from GetAudioSpdifChannelStatusFull
    memcpy(&Aud_SB_ChannelStatus,&pCS,sizeof(KADP_AUDIO_SPDIF_CHANNEL_STATUS_SOUNDBAR));

    cs[0] = Aud_SB_ChannelStatus.res_0;

    //vol->63:60, id->59:36
    cs[1] = ((Aud_SB_ChannelStatus.OSD_Vol.bits & 0xF) << 28) | (Aud_SB_ChannelStatus.barId << 4);
    cs[1] = cs[1] | (Aud_SB_ChannelStatus.res_1 << 4);

    cs[2] = ((Aud_SB_ChannelStatus.reserved_data & 0x0FFF) << 20) | (Aud_SB_ChannelStatus.woofer_Vol.bits << 12);
    cs[2] = cs[2] | (Aud_SB_ChannelStatus.command << 4);
    cs[2] = cs[2] | ((Aud_SB_ChannelStatus.OSD_Vol.bits >> 4) & 0xF);

    cs[3] = (Aud_SB_ChannelStatus.res_2 & 0xFFFFF000 ) | (Aud_SB_ChannelStatus.checksum << 4);
    cs[3] = cs[3] | (Aud_SB_ChannelStatus.reserved_data >> 12);

    cs[4] = Aud_SB_ChannelStatus.res_3;
    cs[5] = Aud_SB_ChannelStatus.res_4;

    memset(&audioConfig, 0, sizeof(struct AUDIO_CONFIG_COMMAND));

    audioConfig.msgID = AUDIO_CONFIG_CMD_SET_SPDIF_CS_ALL;

    i=0;

    /* value [0] ~ [5] 192 bit:L */
    audioConfig.value[i++] = cs[0];
    audioConfig.value[i++] = cs[1];
    audioConfig.value[i++] = cs[2];
    audioConfig.value[i++] = cs[3];
    audioConfig.value[i++] = cs[4];
    audioConfig.value[i++] = cs[5];
    /* value [0] ~ [5] 192 bit:R */
    audioConfig.value[i++] = cs[0];
    audioConfig.value[i++] = cs[1];
    audioConfig.value[i++] = cs[2];
    audioConfig.value[i++] = cs[3];
    audioConfig.value[i++] = cs[4];
    audioConfig.value[i++] = cs[5];

    res = (UINT32)rtkaudio_send_audio_config(&audioConfig);
    if(res != S_OK) ERROR("[%s:%d] RPC return != S_OK\n",__FUNCTION__,__LINE__);
    return res;
}

KADP_STATUS_T KADP_AUDIO_SB_GetChannelStatus(KADP_AUDIO_SPDIF_CHANNEL_STATUS_SOUNDBAR *pCS)
{
    memcpy(pCS, &Aud_SB_ChannelStatus, sizeof(KADP_AUDIO_SPDIF_CHANNEL_STATUS_SOUNDBAR));
    return KADP_OK;
}

#define AUDIO_RESOURCE_ATP0 2
#define AUDIO_RESOURCE_ATP1 3

KADP_STATUS_T KADP_AUDIO_GetTrickState(UINT32 atp_index, char* ret)
{
    if(atp_index < AUDIO_RESOURCE_ATP0 || atp_index > AUDIO_RESOURCE_ATP1)
    {
        AUDIO_ERROR("[%s] atp_index[%d] is out of range.\n", __FUNCTION__, atp_index);
        return KADP_ERROR;
    }
    if (ret == NULL)
    {
        AUDIO_ERROR("[%s] ret is NULL\n", __FUNCTION__);
        return KADP_ERROR;
    }

    *ret = Aud_trickPlayState[atp_index-AUDIO_RESOURCE_ATP0];
    return KADP_OK;
}

KADP_STATUS_T KADP_AUDIO_ResetTrickState(UINT32 atp_index)
{
    if(atp_index < AUDIO_RESOURCE_ATP0 || atp_index > AUDIO_RESOURCE_ATP1)
    {
        AUDIO_DEBUG("[%s] atp_index[%d] is out of range.\n", __FUNCTION__, atp_index);
        return KADP_ERROR;
    }

    Aud_trickPlayState[atp_index-AUDIO_RESOURCE_ATP0] = 0;
    return KADP_OK;
}

KADP_STATUS_T KADP_AUDIO_PrivateInfo(struct AUDIO_RPC_PRIVATEINFO_PARAMETERS *parameter, AUDIO_RPC_PRIVATEINFO_RETURNVAL *ret)
{
    AUDIO_RPC_PRIVATEINFO_RETURNVAL result;
    UINT32 res;

    if ((ret == NULL) || (parameter == NULL))
    {
        AUDIO_ERROR("[%s] pointer is NULL\n", __FUNCTION__);
        return KADP_ERROR;
    }

    res = RTKAUDIO_RPC_TOAGENT_PRIVATEINFO_SVC(parameter, &result);
    if(res != S_OK){
        ERROR("[%s:%d] RPC return != S_OK\n",__func__,__LINE__);
        return KADP_ERROR;
    }
    else
    {
        memcpy(ret, &result, sizeof(AUDIO_RPC_PRIVATEINFO_RETURNVAL));
        return KADP_OK;
    }
}

KADP_STATUS_T KADP_AUDIO_GetAudioSpdifChannelStatus(KADP_AO_SPDIF_CHANNEL_STATUS_BASIC *cs, AUDIO_MODULE_TYPE type)
{
    UINT32 res;
    AUDIO_SPDIF_CS spdif_cs = {0};

    if (cs == NULL)
    {
        AUDIO_ERROR("[%s] pointer is NULL\n", __FUNCTION__);
        return KADP_ERROR;
    }

    if((type != AUDIO_OUT) && (type != AUDIO_IN))
    {
        AUDIO_ERROR("[%s,%d] error module type\n", __FUNCTION__, __LINE__);
        memset(cs, 0, sizeof(KADP_AO_SPDIF_CHANNEL_STATUS_BASIC));
        return KADP_ERROR;
    }
    spdif_cs.module_type = type;

    res = RTKAUDIO_RPC_TOAGENT_GET_SPDIF_CS_SVC(&spdif_cs);
    if(res != S_OK) ERROR("[%s:%d] RPC return != S_OK\n",__FUNCTION__,__LINE__);

    if(res == S_OK)
    {
        cs->category_code          = spdif_cs.category_code;
        cs->professional           = spdif_cs.professional;
        cs->copyright              = spdif_cs.copyright;
        cs->channel_number         = spdif_cs.channel_number;
        cs->source_number          = spdif_cs.source_number;
        cs->sampling_freq          = spdif_cs.sampling_freq;
        cs->clock_accuracy         = spdif_cs.clock_accuracy;
        cs->word_length            = spdif_cs.word_length;
        cs->original_sampling_freq = spdif_cs.original_sampling_freq;
        cs->cgms_a                 = spdif_cs.cgms_a;
        cs->pre_emphasis           = spdif_cs.pre_emphasis;
        cs->data_type              = spdif_cs.data_type;
        cs->mode                   = spdif_cs.mode;
    } else {
        return KADP_ERROR;
    }

    return KADP_OK;
}

KADP_STATUS_T KADP_AUDIO_SetSpdifOutPinSrc(KADP_AUDIO_SPDIFO_SOURCE src)
{
    UINT32 res;
    AUDIO_RPC_SPDIFO_SOURCE aSpdifSrc;

    aSpdifSrc.instanceID = -1;// do not care
    switch(src)
    {
        case KADP_AUDIO_SPDIFO_SRC_FIFO :
            aSpdifSrc.source = SPDIFO_SRC_FIFO;
            break;
        case KADP_AUDIO_SPDIFO_SRC_IN :
            aSpdifSrc.source = SPDIFO_SRC_IN;
            break;
        case KADP_AUDIO_SPDIFO_SRC_HDMI :
            aSpdifSrc.source = SPDIFO_SRC_HDMI;
            break;
        case   KADP_AUDIO_SPDIFO_SRC_DISABLE :
        default :
            aSpdifSrc.source = SPDIFO_SRC_DISABLE;
            break;
    }

    res = RTKAUDIO_RPC_AO_TOAGENT_SPDIFO_SOURCE_CONFIG_SVC(&aSpdifSrc);
    if(res != S_OK) ERROR("[%s:%d] RPC return != S_OK\n",__FUNCTION__,__LINE__);
    return res;
}

static KADP_STATUS_T KADP_AUDIO_SetVirtualXParam(virtualx_cmd_info *info)
{
    int ret = rtkaudio_ioctl_set(VIRTUALX_CMD_INFO, (void*)info, sizeof(*info));

    if(ret < 0) {
        AUDIO_ERROR("[%s] failed with %s\n",__FUNCTION__,Err_Status[-(ret+1)]);

        return KADP_ERROR;
    }

    return KADP_OK;
}

static KADP_STATUS_T KADP_AUDIO_GetVirtualXParam(virtualx_cmd_info *info)
{
    int ret = rtkaudio_ioctl_get(VIRTUALX_CMD_INFO, (void*)info, sizeof(*info));

    if(ret < 0) {
        AUDIO_ERROR("[%s] failed with %s\n",__FUNCTION__,Err_Status[-(ret+1)]);

        return KADP_ERROR;
    }

    return KADP_OK;
}

KADP_STATUS_T KADP_AUDIO_SetTruSrndXParam(AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO *param, int index)
{
    KADP_STATUS_T ret;

    virtualx_cmd_info virx_cmd;

    virx_cmd.index = index;
    virx_cmd.type= ENUM_VIRTUALX_CMD_TYPE_TRUSRNDX_SETPARAM;
    virx_cmd.size= sizeof(*param);
    memcpy(&virx_cmd.data, param, sizeof(*param));

    ret = KADP_AUDIO_SetVirtualXParam(&virx_cmd);
    if (ret == KADP_OK)
    {
        if(0 == virx_cmd.result)
            memcpy(param, &virx_cmd.data, sizeof(*param));
        else
            ret = KADP_ERROR;
    }
    return ret;
}

KADP_STATUS_T KADP_AUDIO_GetTruSrndXParam(AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO *param, int index)
{
    KADP_STATUS_T ret;

    virtualx_cmd_info virx_cmd;

    virx_cmd.index = index;
    virx_cmd.type= ENUM_VIRTUALX_CMD_TYPE_TRUSRNDX_GETPARAM;
    virx_cmd.size= sizeof(*param);
    memcpy(&virx_cmd.data, param, sizeof(*param));

    ret = KADP_AUDIO_GetVirtualXParam(&virx_cmd);
    if (ret == KADP_OK)
    {
        if(0 == virx_cmd.result)
            memcpy(param, &virx_cmd.data, sizeof(*param));
        else
            ret = KADP_ERROR;
    }
    return ret;
}

KADP_STATUS_T KADP_AUDIO_SetTbhdXParam(AUDIO_VIRTUALX_TBHDX_PARAM_INFO *param)
{
    KADP_STATUS_T ret;

    virtualx_cmd_info virx_cmd;

    virx_cmd.type= ENUM_VIRTUALX_CMD_TYPE_TBHDX_SETPARAM;
    virx_cmd.size= sizeof(*param);
    memcpy(&virx_cmd.data, param, sizeof(*param));

    ret = KADP_AUDIO_SetVirtualXParam(&virx_cmd);
    if (ret == KADP_OK)
    {
        if(0 == virx_cmd.result)
            memcpy(param, &virx_cmd.data, sizeof(*param));
        else
            ret = KADP_ERROR;
    }
    return ret;
}

KADP_STATUS_T KADP_AUDIO_GetTbhdXParam(AUDIO_VIRTUALX_TBHDX_PARAM_INFO *param)
{
    KADP_STATUS_T ret;

    virtualx_cmd_info virx_cmd;

    virx_cmd.type= ENUM_VIRTUALX_CMD_TYPE_TBHDX_GETPARAM;
    virx_cmd.size= sizeof(*param);
    memcpy(&virx_cmd.data, param, sizeof(*param));

    ret = KADP_AUDIO_GetVirtualXParam(&virx_cmd);
    if (ret == KADP_OK)
    {
        if(0 == virx_cmd.result)
            memcpy(param, &virx_cmd.data, sizeof(*param));
        else
            ret = KADP_ERROR;
    }
    return ret;
}

KADP_STATUS_T KADP_AUDIO_SetMbhlParam(AUDIO_VIRTUALX_MBHL_PARAM_INFO *param)
{
    KADP_STATUS_T ret;

    virtualx_cmd_info virx_cmd;

    virx_cmd.type= ENUM_VIRTUALX_CMD_TYPE_MBHL_SETPARAM;
    virx_cmd.size= sizeof(*param);
    memcpy(&virx_cmd.data, param, sizeof(*param));

    ret = KADP_AUDIO_SetVirtualXParam(&virx_cmd);
    if (ret == KADP_OK)
    {
        if(0 == virx_cmd.result)
            memcpy(param, &virx_cmd.data, sizeof(*param));
        else
            ret = KADP_ERROR;
    }
    return ret;
}

KADP_STATUS_T KADP_AUDIO_GetMbhlParam(AUDIO_VIRTUALX_MBHL_PARAM_INFO *param)
{
    KADP_STATUS_T ret;

    virtualx_cmd_info virx_cmd;

    virx_cmd.type= ENUM_VIRTUALX_CMD_TYPE_MBHL_GETPARAM;
    virx_cmd.size= sizeof(*param);
    memcpy(&virx_cmd.data, param, sizeof(*param));

    ret = KADP_AUDIO_GetVirtualXParam(&virx_cmd);
    if (ret == KADP_OK)
    {
        if(0 == virx_cmd.result)
            memcpy(param, &virx_cmd.data, sizeof(*param));
        else
            ret = KADP_ERROR;
    }
    return ret;
}
