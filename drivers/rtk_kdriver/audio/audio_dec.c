/******************************************************************************
 *
 *   Copyright(c) 2014 Realtek Semiconductor Corp. All rights reserved.
 *
 *   @author yungjui.lee@realtek.com
 *
 *****************************************************************************/

#include "audio_base.h"

#include "audio_pin.h"
#include "AudioInbandAPI.h"
#include <linux/kthread.h>
#include <audio_rpc.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/freezer.h>

static DEFINE_SEMAPHORE(Dwn_strm_sem);

#define DEFAULT_DELAYRP_SETTING 1
#define AUDIO_DEC_MSG_BUF_SIZE      (0x2800)
#define AUDIO_DEC_INBAND_BUF_SIZE   (0x10000)
static UINT32 DeliverInBandCommand(Base* pBaseObj, BYTE* Data, UINT32 Size)
{
    BYTE*   pBufferCurr;
    BYTE*   pBufferNext;
    BYTE*   pICQBufferUpper;
    BYTE*   pICQBufferLower;
    UINT32  bufferSize;
    AUDIO_INBAND_CMD_TYPE cmdType;
    int64_t startPTS;
    DEC* pDECObj = (DEC*)pBaseObj->pDerivedObj;
    Pin* pICQPin = pDECObj->pICQPin;

    cmdType = *(AUDIO_INBAND_CMD_TYPE*)Data;

    pICQPin->GetBuffer(pICQPin, &pICQBufferLower, &bufferSize);
    pICQBufferUpper = pICQBufferLower + bufferSize;

    startPTS = pli_getPTS();
    while(pICQPin->GetWriteSize(pICQPin) < Size)
    {
        /*osal_Sleep(100);*/
        msleep(100);
        if((pli_getPTS() - startPTS) > 90000)
        {
            ERROR("[%s %d] cmd buffer full. cmdType = %d\n", __FILE__, __LINE__, cmdType);
            startPTS = pli_getPTS();
        }
    }

    pBufferCurr  = pICQPin->GetWritePtr(pICQPin);

    if(pBufferCurr + Size > pICQBufferUpper)
    {
        UINT32 space = pICQBufferUpper - pBufferCurr;
        MyMemWrite(pBufferCurr, Data, space);

        if(Size > space)
            MyMemWrite(pICQBufferLower, Data + space, Size - space);

        pBufferNext = pICQBufferLower + Size - space;
    }
    else
    {
        pBufferNext = pBufferCurr + Size;
        MyMemWrite(pBufferCurr, Data, Size);
    }
    pICQPin->SetWritePtr(pICQPin, pBufferNext);

    return S_OK;
}
static void GetAudioTypeAndPrivateInfo(AUDIO_FORMAT* a_format, AUDIO_DEC_TYPE* audioType, long* privateInfo)
{
    int HDMV_flag = 0;

    switch (a_format->type)
    {
        //////////////////////////////////////////////
        case RHALTYPE_HDMV_DOLBY_AC3:
            HDMV_flag = 1;  // on when receive HDMV AC3
        case RHALTYPE_DOLBY_AC3:
            *audioType = AUDIO_AC3_DECODER_TYPE;
            break;

#if defined(ARM_AC4DEC_ENABLE)
        case RHALTYPE_DOLBY_AC4_AUDIO:
            *audioType = AUDIO_AC4_DECODER_TYPE;
            privateInfo[6]  |= a_format->privateData[5];
            break;
#endif

#if defined(MATDEC)
        case RHALTYPE_DOLBY_MAT_AUDIO:
            *audioType = AUDIO_MAT_DECODER_TYPE;
            break;
#endif

#if defined(AACELD)
        case RHALTYPE_AAC_ELD:
            *audioType = AUDIO_AACELD_DECODER_TYPE;
            break;
#endif
        //////////////////////////////////////////////
        case RHALTYPE_HDMV_DDP:
            HDMV_flag = 1;  // on when receive HDMV DDP
        case RHALTYPE_DDP:
            *audioType = AUDIO_DDP_DECODER_TYPE;
            privateInfo[6]  |= a_format->privateData[5];
            break;
        //////////////////////////////////////////////

        case RHALTYPE_MPEG_AUDIO:
        case RHALTYPE_MPEG2_AUDIO:
        {
            AUDIO_EXT_BS*    extBS;
            extBS = (AUDIO_EXT_BS*)privateInfo;
            extBS->exist = 0;
            *audioType = AUDIO_MPEG_DECODER_TYPE;
            break;
        }
        case RHALTYPE_MPEGH_AUDIO:
        {
            *audioType = AUDIO_MPEGH_DECODER_TYPE;
            privateInfo[0]  = 0;//latm_header_present
            break;
        }
        case RHALTYPE_PCM:
        case RHALTYPE_PCM_LITTLE_ENDIAN:
            privateInfo[6]  |= a_format->privateData[5];
        case RHALTYPE_PCM_HDMV:
        {
            int signed_flag = 1;  // for backward compatible, if 8 bit, means unsigned, if 16,20,24,32 means signed
            int channel_mask = 0;
            int channel_assignment = 0;  // for hdmv lpcm channel assignment
            *audioType = AUDIO_LPCM_DECODER_TYPE;
            privateInfo[0] = a_format->numberOfChannels;
            privateInfo[1] = a_format->bitsPerSample;
            privateInfo[2] = a_format->samplingRate;
            privateInfo[3] = a_format->dynamicRange;

            if (a_format->bitsPerSample == 8)
            {
                signed_flag = 0;
            }

            // [0:3]  emphasis
            // [4]    floating
            // [5]    aiff 8 bit
            // [6:21] wave channel mask
            // [22:25] hdmv channel assignment
            privateInfo[4] = (a_format->emphasis&0xF)
                            | ((a_format->privateData[0]&0x1)<<4)  // (a_format->privateData[0]&0x1)<<4 : float or not
                            | ((signed_flag&0x1)<<5)
                            | ((channel_mask&0xFFFF)<<6)
                            | ((channel_assignment&0xF)<<22);


            privateInfo[7]  = AUDIO_BIG_ENDIAN;
            if (a_format->type == RHALTYPE_PCM_LITTLE_ENDIAN)
            {
                privateInfo[7]  = AUDIO_LITTLE_ENDIAN;
            }
            else if (a_format->type == RHALTYPE_PCM_HDMV)
            {
                privateInfo[7] = AUDIO_LPCM_HDMV_TYPE;
            }
            break;
        }
        case RHALTYPE_DV:
            *audioType = AUDIO_DV_DECODER_TYPE;
            break;

        //////////////////////////////////////////////
        case RHALTYPE_HDMV_DTS:
            HDMV_flag = 1;  // on when receive HDMV DTS
        case RHALTYPE_DTS:
            *audioType = AUDIO_DTS_DECODER_TYPE;
            break;
        //////////////////////////////////////////////

        //////////////////////////////////////////////
        case RHALTYPE_HDMV_DTS_HD:
            HDMV_flag = 1;  // on when receive HDMV DTS_HD
        case RHALTYPE_DTS_HD:
            *audioType = AUDIO_DTS_HD_DECODER_TYPE;
            break;
        //////////////////////////////////////////////

        case RHALTYPE_AAC:
        {
            *audioType = AUDIO_AAC_DECODER_TYPE;
            privateInfo[0]  = 0;//latm_header_present
            break;
        }
        case RHALTYPE_RAW_AAC:
        {
            *audioType = AUDIO_RAW_AAC_DECODER_TYPE;
            privateInfo[0]  = a_format->numberOfChannels;
            privateInfo[1]  = a_format->samplingRate;
            privateInfo[2]  = a_format->privateData[0]; //downSampledSBR
            privateInfo[3]  = a_format->privateData[1]; //sbr_present_flag
            privateInfo[4]  = a_format->privateData[2]; //pce_present_flag
            privateInfo[6]  |= a_format->privateData[5];
            break;
        }
        case RHALTYPE_LATM_AAC:
        {
            *audioType = AUDIO_AAC_DECODER_TYPE;
            privateInfo[0]  = 1;//latm_header_present
            privateInfo[6]  |= a_format->privateData[5];
            break;
        }
        case RHALTYPE_MP4:
        {
            *audioType = AUDIO_MP4AAC_DECODER_TYPE;
            privateInfo[0]  = a_format->numberOfChannels;
            privateInfo[1]  = a_format->samplingRate;
            privateInfo[2]  = a_format->privateData[0];//stsz_sample_count
            privateInfo[3]  = a_format->privateData[1];//object_type
            break;
        }
        case RHALTYPE_WMA:
            *audioType = AUDIO_WMA_DECODER_TYPE;
            break;

        case RHALTYPE_WMAPRO:
            *audioType = AUDIO_WMA_PRO_DECODER_TYPE;
            break;

        case RHALTYPE_OGG_AUDIO:
            *audioType = AUDIO_VORBIS_DECODER_TYPE;
            privateInfo[0]  = a_format->privateData[0];
            privateInfo[1]  = a_format->privateData[1];
            break;

        case RHALTYPE_OGG_OPUS:
            *audioType = AUDIO_OPUS_DECODER_TYPE;
            privateInfo[0]  = a_format->privateData[0];
            privateInfo[1]  = a_format->privateData[1];
            break;

        case RHALTYPE_RA_COOK:
            *audioType = AUDIO_COOK_DECODER_TYPE;
            break;

        case RHALTYPE_RA_LSD:
            *audioType = AUDIO_LSD_DECODER_TYPE;
            break;

        case RHALTYPE_ADPCM:
        {
/* WAVE form wFormatTag IDs */
//ref: http://wiki.multimedia.cx/index.php?title=TwoCC
#define  WAVE_FORMAT_DVI_ADPCM      0x0011  /*  Intel Corporation  */
#define  WAVE_FORMAT_IMA_ADPCM      (WAVE_FORMAT_DVI_ADPCM) /*  Intel Corporation  */
#define  WAVE_FORMAT_G726_ADPCM     0x0045  /*  ITU-T G.726 ADPCM  */

            *audioType = AUDIO_ADPCM_DECODER_TYPE;
            privateInfo[0]  = a_format->numberOfChannels;
            privateInfo[1]  = a_format->samplingRate;
            privateInfo[2]  = a_format->privateData[0];//block align
            privateInfo[3]  = a_format->privateData[1];//wFormatTag ID
            if (a_format->privateData[1] == WAVE_FORMAT_G726_ADPCM)
            {
                privateInfo[4]  = a_format->dynamicRange;//bit rate
            }
            else if (a_format->privateData[1] == WAVE_FORMAT_IMA_ADPCM)
            {
                privateInfo[4]  = a_format->bitsPerSample;
            }
            break;
        }
        case RHALTYPE_ULAW:
        {
            *audioType = AUDIO_ULAW_DECODER_TYPE;
            privateInfo[0]      = a_format->numberOfChannels;
            privateInfo[1]      = a_format->bitsPerSample;
            privateInfo[2]      = a_format->samplingRate;
            break;
        }
        case RHALTYPE_ALAW:
        {
            *audioType = AUDIO_ALAW_DECODER_TYPE;
            privateInfo[0]      = a_format->numberOfChannels;
            privateInfo[1]      = a_format->bitsPerSample;
            privateInfo[2]      = a_format->samplingRate;
            break;
        }
        case RHALTYPE_FLAC:
        {
            *audioType = AUDIO_FLAC_DECODER_TYPE;
            privateInfo[0]      = a_format->privateData[0]; // 1 to skip flac sync words
            break;
        }
        case RHALTYPE_MLP_AUDIO:
        {
            *audioType = AUDIO_MLP_DECODER_TYPE;
            break;
        }
        case RHALTYPE_AMRWB_AUDIO:
        {
            *audioType = AUDIO_AMRWB_DECODER_TYPE;
            privateInfo[0]      = a_format->privateData[0]; // 1 for 3gpp file, 0 for pure amr file
            break;
        }
        case RHALTYPE_AMRNB_AUDIO:
        {
            *audioType = AUDIO_AMRNB_DECODER_TYPE;
            privateInfo[0]      = a_format->privateData[0]; // 1 for 3gpp file, 0 for pure amr file
            break;
        }
#ifdef ENABLE_APE
        case RHALTYPE_APE_AUDIO:
        {
            *audioType = AUDIO_APE_DECODER_TYPE;
            break;
        }
#endif
        case RHALTYPE_DRA:
        {
            *audioType = AUDIO_DRA_DECODER_TYPE;
            break;
        }
        default:
            *audioType = AUDIO_UNKNOWN_TYPE;
            break;
    }
}

static UINT32 SetSubDecoderMode(Base* pBaseObj, int mode, int id)
{
    UINT32 res;
    AUDIO_RPC_DECCFG    info = {0};
    AUDIO_DEC_CFG       m_cfg = {0};

    if(((mode&0x00000001) != DEC_IS_MAIN) && ((mode&0x00000001) != DEC_IS_SUB))
    {
        ERROR("set sub decoder mode(%d) error!!\n",mode);
        return S_FALSE;
    }

    m_cfg.sub_dec_mode = mode;
    m_cfg.substream_id = id;
    m_cfg.LFEMode      = (mode&0x40)? LFE_ON : LFE_OFF;
    m_cfg.dualmono     = DUAL_MONO_STEREO;

    info.instanceID = pBaseObj->GetAgentID(pBaseObj);
    info.cfg        = m_cfg;
    INFO("[dual] SubDecoderMode=%x\n",mode);
    res = RTKAUDIO_RPC_TOAGENT_DECODERCONFIG_SVC(&info);

    return res;
}

UINT32 InitDwnStrmQueue(Base* pBaseObj)
{
    UINT32 res;
    DEC* pDECObj = (DEC*)pBaseObj->pDerivedObj;
    Pin* pDwnStrmQ = pDECObj->pDwnStrmQ;
    UINT32 pAddr;
    RINGBUFFER_HEADER *vAddr;
    AUDIO_RPC_RINGBUFFER_HEADER header;

    pDwnStrmQ->GetBufHeader(pDwnStrmQ, (void**)&vAddr, &pAddr);

    header.instanceID = pBaseObj->GetAgentID(pBaseObj);
    header.pinID      = DWNSTRM_INBAND_QUEUE;
    header.readIdx    = 0;
    header.listSize   = 1;
    header.pRingBufferHeaderList[0] = (unsigned long)pAddr;
    res = RTKAUDIO_RPC_TOAGENT_INITRINGBUFFER_HEADER_SVC(&header);
    if(res != S_OK) ERROR("[%s:%d] RPC return != S_OK\n",__FUNCTION__,__LINE__);

    return res;
}

typedef struct
{
    ENUM_AUDIO_INFO_TYPE    infoType;
    AUDIO_PCM_FORMAT        pcmFormat;
    long                    start_addr;
    int                     max_bit_rate;
} AUDIO_INFO_PCM_FORMAT;

typedef struct
{
    ENUM_AUDIO_INFO_TYPE    infoType;
    char channel_index[8];    // note: here has endian swap issue.
    long start_addr;
    char dual_mono_flag;
} AUDIO_INFO_CHANNEL_INDEX;

static UINT8* ring_add(UINT8* base, UINT8* limit, UINT8* rp, UINT32 size)
{
    if(rp + size >= limit)
        return ((rp + size) - (limit - base));
    else
        return (rp + size);
}

static void ring_read(UINT8* base, UINT8* limit, UINT8* rp, UINT8* dest, UINT32 size)
{
    if(rp + size > limit) {
        MyMemRead(rp, dest, (limit - rp));
        MyMemRead(base, dest + (limit - rp), size - (limit - rp));
    } else {
        MyMemRead(rp, dest, size);
    }
}

static void SwapData(UINT8* src, UINT8* des, UINT32 len)
{
    UINT32 i;
    UINT32 *psrc, *pdes;
    UINT32 b0, b1, b2, b3;

    if ((((SINT32)src & 0x3) != 0) || (((SINT32)des & 0x3) != 0) || ((len & 0x3) != 0))
        ERROR("error in SwapData()...\n");

    for (i = 0; i < len; i+=4) {
        psrc = (UINT32 *)&src[i];
        pdes = (UINT32 *)&des[i];
        b0 = *psrc & 0x000000ff;
        b1 = (*psrc & 0x0000ff00) >> 8;
        b2 = (*psrc & 0x00ff0000) >> 16;
        b3 = (*psrc & 0xff000000) >> 24;
        *pdes = (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
    }
}

static void ReadDwnStrmCommand(Base* pBaseObj)
{
    DEC* pDECObj = (DEC*)pBaseObj->pDerivedObj;
    Pin* pDwnStrmQ = pDECObj->pDwnStrmQ;
    UINT32 bufferSize;
    UINT8 *readPtr, *writePtr, *base, *limit;

    down(&Dwn_strm_sem);
    pDwnStrmQ->GetBuffer(pDwnStrmQ, &base, &bufferSize);
    limit    = base + bufferSize;
    readPtr  = pDwnStrmQ->GetReadPtr(pDwnStrmQ, 0);
    writePtr = pDwnStrmQ->GetWritePtr(pDwnStrmQ);

    while(readPtr != writePtr)
    {
        AUDIO_INBAND_CMD_TYPE infoType;
        SINT32 i;
        ring_read(base, limit, readPtr, (UINT8*)&infoType, sizeof(infoType));

        if(infoType == AUDIO_DEC_INBAND_CMD_TYPE_PRIVATE_INFO)
        {
            AUDIO_INBAND_PRIVATE_INFO cmd;
            ring_read(base, limit, readPtr, (UINT8*)&cmd, sizeof(cmd));
            readPtr = ring_add(base, limit, readPtr, sizeof(cmd));

            if(cmd.infoType == AUDIO_INBAND_CMD_PRIVATE_PCM_FMT)
            {
                AUDIO_INFO_PCM_FORMAT pcmfmt;
                ring_read(base, limit, readPtr, (UINT8*)&pcmfmt, sizeof(pcmfmt));
                readPtr = ring_add(base, limit, readPtr, cmd.infoSize);
                INFO("pcmfmt: chnum = %d\n", pcmfmt.pcmFormat.chnum);
                INFO("pcmfmt: samplerate = %d\n", pcmfmt.pcmFormat.samplerate);
                pDECObj->rate = pcmfmt.pcmFormat.samplerate;
                pDECObj->channel = pcmfmt.pcmFormat.chnum;
            }
            else if(cmd.infoType == AUDIO_INBAND_CMD_PRIVATE_CH_IDX)
            {
                AUDIO_INFO_CHANNEL_INDEX infoChIndex;
                ring_read(base, limit, readPtr, (UINT8*)&infoChIndex, sizeof(infoChIndex));
                readPtr = ring_add(base, limit, readPtr, cmd.infoSize);
                SwapData((UINT8*)&infoChIndex.channel_index, (UINT8*)&infoChIndex.channel_index, sizeof(char) * AUDIO_MAX_CHANNEL_NUM);
                for(i = 0; i < AUDIO_MAX_CHANNEL_NUM; i++)
                {
                    if(infoChIndex.channel_index[i])
                    {
                        pDECObj->chan_idx[i] = infoChIndex.channel_index[i];
                    } else {
                        pDECObj->chan_idx[i] = 0;
                    }
                }
            }
        }
        else if(infoType == AUDIO_DEC_INBAND_CMD_TYPE_PTS)
        {
            AUDIO_DEC_PTS_INFO ptsInfo;
            ring_read(base, limit, readPtr, (UINT8*)&ptsInfo, sizeof(ptsInfo));
            readPtr = ring_add(base, limit, readPtr, sizeof(ptsInfo));
            pDECObj->curr_PTS = (((int64_t)ptsInfo.PTSH) << 32) | ptsInfo.PTSL;
        }
        else if(infoType == AUDIO_DEC_INBAND_CMD_TYPE_EOS)
        {
            readPtr = ring_add(base, limit, readPtr, sizeof(AUDIO_DEC_EOS));
        }
        else if(infoType == AUDIO_DEC_INBAND_CMD_TYPE_DDP_METADATA)
        {
            AUDIO_DDP_METADATA ddp_md;
            ring_read(base, limit, readPtr, (UINT8*)&ddp_md, sizeof(ddp_md));

            readPtr = ring_add(base, limit, readPtr, sizeof(ddp_md));
        }
        else if(infoType == AUDIO_DEC_INBAND_CMD_TYPE_AAC_METADATA)
        {
            AUDIO_AAC_METADATA aac_md;
            ring_read(base, limit, readPtr, (UINT8*)&aac_md, sizeof(aac_md));

            readPtr = ring_add(base, limit, readPtr, sizeof(aac_md));
        }
        else
        {
            ERROR("[DEC] unknown info, force reset Q\n");
            readPtr = writePtr;
        }
    } /* while(readPtr != writePtr) */

    pDwnStrmQ->SetReadPtr(pDwnStrmQ, writePtr, 0);
    up(&Dwn_strm_sem);
    return;
}

static int DwnStrmQ_MonitorThread(void* data)
{
    Base* pBaseObj = (Base*)data;
    DEC* pDECObj = (DEC*)pBaseObj->pDerivedObj;
    short i;

	for (;;) {
		set_freezable();

        for(i=0; i<4; i++) {
            if (kthread_should_stop())
                break;
            msleep(50);
        }
        if (kthread_should_stop())
            break;

        pDECObj->prev_2nd_PTS = pDECObj->prev_PTS;
        pDECObj->prev_PTS = pDECObj->curr_PTS;
        ReadDwnStrmCommand(pBaseObj);
    }

    pr_debug("[%s][%s] break\n",pBaseObj->name,__FUNCTION__);

	return S_OK;
}

UINT32 StartDwnStrmQMonitorThread(Base* pBaseObj)
{
    DEC* pDECObj = (DEC*)pBaseObj->pDerivedObj;
    if (pDECObj->adec_tsk == NULL) {
        pDECObj->adec_tsk = kthread_create(DwnStrmQ_MonitorThread, (void*)pBaseObj, "adec_tsk");
        if (IS_ERR(pDECObj->adec_tsk)) {
            pDECObj->adec_tsk = NULL;
            ERROR("[%s][%s] fail to create kthread\n",pBaseObj->name,__FUNCTION__);
            return S_FALSE;
        }
        wake_up_process(pDECObj->adec_tsk);
    }
    // re-init out pin ring buffer to set rp number back to 1
    pBaseObj->InitOutRingBuf(pBaseObj);
    return S_OK;
}

UINT32 StopDwnStrmQMonitorThread(Base* pBaseObj)
{
    DEC* pDECObj = (DEC*)pBaseObj->pDerivedObj;
    if (pDECObj->adec_tsk) {
        int ret;
        ret = kthread_stop(pDECObj->adec_tsk);
        if (!ret) {
            ERROR("[%s][%s] rtkaudio thread stopped\n",pBaseObj->name,__FUNCTION__);
        }
        pDECObj->adec_tsk = NULL;
    }
    return S_OK;
}

UINT32 InitBSRingBuf(Base* pBaseObj, UINT32 headerPhyAddr)
{
    UINT32 res;
    AUDIO_RPC_RINGBUFFER_HEADER header;

    header.instanceID = pBaseObj->GetAgentID(pBaseObj);
    header.pinID      = BASE_BS_IN;
    header.readIdx    = 0;
    header.listSize   = 1;
    header.pRingBufferHeaderList[0] = (unsigned long)headerPhyAddr;

    res = RTKAUDIO_RPC_TOAGENT_INITRINGBUFFER_HEADER_SVC(&header);
    if(res != S_OK) ERROR("[%s:%d] RPC return != S_OK\n",__FUNCTION__,__LINE__);
    INFO("[%s][%s] physical address:%x\n", pBaseObj->name, __FUNCTION__, headerPhyAddr);
    return res;
}
UINT32 InitICQRingBuf(Base* pBaseObj, UINT32 headerPhyAddr)
{
    UINT32 res;
    AUDIO_RPC_RINGBUFFER_HEADER header;

    header.instanceID = pBaseObj->GetAgentID(pBaseObj);
    header.pinID      = INBAND_QUEUE;
    header.readIdx    = 0;
    header.listSize   = 1;
    header.pRingBufferHeaderList[0] = (unsigned long)headerPhyAddr;

    res = RTKAUDIO_RPC_TOAGENT_INITRINGBUFFER_HEADER_SVC(&header);
    if(res != S_OK) ERROR("[%s:%d] RPC return != S_OK\n",__FUNCTION__,__LINE__);
    INFO("[%s][%s] physical address:%x\n", pBaseObj->name, __FUNCTION__, headerPhyAddr);
    return res;
}
UINT32 GetOUTRingBufPhyAddr(Base* pBaseObj, UINT32 pin_index, UINT32* headerPhyAddr, UINT32* bufferPhyAddr, UINT32* size)
{
    RINGBUFFER_HEADER *vhAddr;
    UINT8             *vbAddr;

    UINT32 listSize = pBaseObj->outPin->GetListSize(pBaseObj->outPin);
    if(pin_index >= listSize) {
        ERROR("[%s:%d] pin_index(%d) >= listSize(%d)\n",__FUNCTION__,__LINE__,pin_index,listSize);
        return S_FALSE;
    }
    if(pin_index == 0) {
        pBaseObj->outPin->GetBufHeader(pBaseObj->outPin, (void**)&vhAddr, headerPhyAddr);
        pBaseObj->outPin->GetBufAddress(pBaseObj->outPin, &vbAddr, bufferPhyAddr);
        *size = pBaseObj->outPin->GetBufferSize(pBaseObj->outPin);
    } else {
        MultiPin* pMObj = (MultiPin*)pBaseObj->outPin->pDerivedObj;
        pMObj->pPinObjArr[pin_index-1]->GetBufHeader(pMObj->pPinObjArr[pin_index-1], (void**)&vhAddr, headerPhyAddr);
        pMObj->pPinObjArr[pin_index-1]->GetBufAddress(pMObj->pPinObjArr[pin_index-1], &vbAddr, bufferPhyAddr);
        *size = pMObj->pPinObjArr[pin_index-1]->GetBufferSize(pMObj->pPinObjArr[pin_index-1]);
    }
    return S_OK;
}
UINT32 GetICQRingBufPhyAddr(Base* pBaseObj, UINT32* headerPhyAddr, UINT32* bufferPhyAddr, UINT32* size)
{
    DEC* pDECObj = (DEC*)pBaseObj->pDerivedObj;
    RINGBUFFER_HEADER *vhAddr;
    UINT8             *vbAddr;
    pDECObj->pICQPin->GetBufHeader(pDECObj->pICQPin, (void**)&vhAddr, headerPhyAddr);
    pDECObj->pICQPin->GetBufAddress(pDECObj->pICQPin, &vbAddr, bufferPhyAddr);
    *size = pDECObj->pICQPin->GetBufferSize(pDECObj->pICQPin);
    return S_OK;
}
UINT32 GetDSQRingBufPhyAddr(Base* pBaseObj, UINT32* headerPhyAddr, UINT32* bufferPhyAddr, UINT32* size)
{
    DEC* pDECObj = (DEC*)pBaseObj->pDerivedObj;
    RINGBUFFER_HEADER *vhAddr;
    UINT8             *vbAddr;
    pDECObj->pDwnStrmQ->GetBufHeader(pDECObj->pDwnStrmQ, (void**)&vhAddr, headerPhyAddr);
    pDECObj->pDwnStrmQ->GetBufAddress(pDECObj->pDwnStrmQ, &vbAddr, bufferPhyAddr);
    *size = pDECObj->pDwnStrmQ->GetBufferSize(pDECObj->pDwnStrmQ);
    return S_OK;
}
UINT32 ResetRingBuf(Base* pBaseObj)
{
    DEC* pDECObj = (DEC*)pBaseObj->pDerivedObj;
    RINGBUFFER_HEADER *vAddr;
    UINT32             pAddr;
    UINT8             *writePtr;

    /* setup Inband Command Queue buffer */
    pDECObj->pICQPin->GetBufHeader(pDECObj->pICQPin, (void**)&vAddr, &pAddr);
    pDECObj->InitICQRingBuf(pBaseObj, pAddr);

    writePtr = pDECObj->pICQPin->GetWritePtr(pDECObj->pICQPin);
    pDECObj->pICQPin->SetReadPtr(pDECObj->pICQPin, writePtr, 0);

    /* setup Message Queue buffer (downstreamQ) */
    InitDwnStrmQueue(pBaseObj);

    writePtr = pDECObj->pDwnStrmQ->GetWritePtr(pDECObj->pDwnStrmQ);
    pDECObj->pDwnStrmQ->SetReadPtr(pDECObj->pDwnStrmQ, writePtr, 0);
    return S_OK;
}
UINT32 SetOutRingNumOfReadPtr(Base* pBaseObj, UINT32 nReadPtr)
{
    SINT32 i, listSize;

    listSize = pBaseObj->outPin->GetListSize(pBaseObj->outPin);
    pBaseObj->outPin->SetNumOfReadPtr(pBaseObj->outPin, nReadPtr);
    if(listSize > 1)
    {
        MultiPin* pMObj = (MultiPin*)pBaseObj->outPin->pDerivedObj;
        for(i = 1; i < listSize; i++)
        {
            pMObj->pPinObjArr[i-1]->SetNumOfReadPtr(pMObj->pPinObjArr[i-1], nReadPtr);
        }
    }
    return S_OK;
}
// if type is mpg
// reserved[0] has extra info:
// bit [0:7]  for mpegN
// bit [8:15]  for layerN
// bit [16:23] for mode (0x0:stereo,0x1:joint stereo,0x2:dual,0x3:mono)
// if type is ac3/ddp/mlp
// reserved[0] has extra info:
// bit [0:7]  for lfeon
// bit [8:15]  for acmod
// if type is DRA
// reserved[0] has extra info:
// bit [0:15]  for lfeon
// bit [16:31]  for main channel number
//if type is aac/ heaac
// reserved[0] has extra info:
// bit [0:7]  for lfeon
// bit [8:15]  for mode /*MODE_11 MODE_10  MODE_20 MODE_32*/
//-- reserved[1] = ((VERSION<<8) & 0xFF00) | (FORMAT & 0x00FF)
// bit [0:7]  for format  /* LOAS/LATM = 0x0, ADTS = 0x1 */
// bit [8:15]  for version   /* AAC = 0x0, HE-AACv1 = 0x1, HE-AACv2 = 0x2 */


UINT32 GetAudioFormatInfo(Base* pBaseObj, AUDIO_RPC_DEC_FORMAT_INFO* formatInfo)
{
    AUDIO_RPC_DEC_FORMAT_INFO pInfo = {0};
    UINT32 instanceID = pBaseObj->GetAgentID(pBaseObj);
    UINT32 res;

    res = RTKAUDIO_RPC_DEC_TOAGENT_GETAUDIOFORMATINFO_SVC(&instanceID, &pInfo);
    if(res != S_OK) ERROR("[%s:%d] RPC return != S_OK\n",__FUNCTION__,__LINE__);

    if(pInfo.nChannels == 0) {
        *formatInfo = pInfo;
        return S_FALSE;
    }

    *formatInfo = pInfo;
    return S_OK;
}
UINT32 SetChannelSwap(Base* pBaseObj, UINT32 sel)
{
    UINT32 res;

    AUDIO_RPC_CHANNEL_OUT_MODE info = {0};
    info.instanceID = pBaseObj->GetAgentID(pBaseObj);

    if(sel == AUDIO_CHANNEL_OUT_SWAP_STEREO)
        info.mode = AUDIO_CHANNEL_OUT_STEREO_RTK;
    else if(sel == AUDIO_CHANNEL_OUT_SWAP_L_TO_R)
        info.mode = AUDIO_CHANNEL_OUT_L_TO_R_RTK;
    else if(sel == AUDIO_CHANNEL_OUT_SWAP_R_TO_L)
        info.mode = AUDIO_CHANNEL_OUT_R_TO_L_RTK;
    else if(sel == AUDIO_CHANNEL_OUT_SWAP_LR_SWAP)
        info.mode = AUDIO_CHANNEL_OUT_LR_SWAP_RTK;
    else if(sel == AUDIO_CHANNEL_OUT_SWAP_LR_MIXED)
        info.mode = AUDIO_CHANNEL_OUT_LR_MIXED_RTK;

    res = RTKAUDIO_RPC_DEC_TOAGENT_SETDUALMONOOUTMODE_SVC(&info);
    if(res != S_OK) ERROR("[%s:%d] RPC return != S_OK\n",__FUNCTION__,__LINE__);

    return res;
}
UINT32 GetCurrentPTS(Base* pBaseObj, SINT64* pts)
{
    DEC* pDECObj = (DEC*)pBaseObj->pDerivedObj;
    ReadDwnStrmCommand(pBaseObj);
    *pts = pDECObj->curr_PTS;
    return S_OK;
}

bool IsESExist(Base* pBaseObj)
{
    DEC* pDECObj = (DEC*)pBaseObj->pDerivedObj;
    return (pDECObj->prev_2nd_PTS != pDECObj->curr_PTS);
}

/** Decoder-SRC setting
 *  ena          : to do SRC in decoder, 1:yes, 0:no
 *  force_out_fs : to set output sample-rate, 0: auto(default)
 */
UINT32 SetDecoderSRCMode(Base* pBaseObj, SINT32 ena, SINT32 force_out_fs)
{
    struct AUDIO_RPC_PRIVATEINFO_PARAMETERS parameter;
    AUDIO_RPC_PRIVATEINFO_RETURNVAL result;
    UINT32 res;

    parameter.instanceID     = pBaseObj->GetAgentID(pBaseObj);
    parameter.type           = ENUM_PRIVATEINFO_AUDIO_DEC_SRC_ENABLE;
    parameter.privateInfo[0] = ena;
    parameter.privateInfo[1] = force_out_fs;
    res = RTKAUDIO_RPC_TOAGENT_PRIVATEINFO_SVC(&parameter, &result);
    if(res != S_OK) ERROR("[%s:%d] RPC return != S_OK\n",__FUNCTION__,__LINE__);

    return S_OK;
}

UINT32 SetDelayRpMode(Base* pBaseObj, UINT8 ena)
{
    struct AUDIO_RPC_PRIVATEINFO_PARAMETERS parameter;
    DEC* pDECObj = (DEC*)pBaseObj->pDerivedObj;
    AUDIO_RPC_PRIVATEINFO_RETURNVAL result;
    UINT32 res;

    pDECObj->m_ena_delayRp = ena;

    parameter.instanceID     = pBaseObj->GetAgentID(pBaseObj);
    parameter.type           = ENUM_PRIVATEINFO_AUDIO_DEC_DELAY_RP;
    parameter.privateInfo[0] = pDECObj->m_ena_delayRp;
    res = RTKAUDIO_RPC_TOAGENT_PRIVATEINFO_SVC(&parameter, &result);
    if(res != S_OK) ERROR("[%s:%d] RPC return != S_OK\n",__FUNCTION__,__LINE__);

    return S_OK;
}

UINT32 SetSkipFlushMode(Base* pBaseObj, UINT8 ena)
{
    struct AUDIO_RPC_PRIVATEINFO_PARAMETERS parameter;
    DEC* pDECObj = (DEC*)pBaseObj->pDerivedObj;
    AUDIO_RPC_PRIVATEINFO_RETURNVAL result;
    UINT32 res;

    pDECObj->skipflush = ena;

    parameter.instanceID     = pBaseObj->GetAgentID(pBaseObj);
    parameter.type           = ENUM_PRIVATEINFO_AUDIO_SKIP_DEC_FLUSH;
    parameter.privateInfo[0] = pDECObj->skipflush;
    res = RTKAUDIO_RPC_TOAGENT_PRIVATEINFO_SVC(&parameter, &result);
    if(res != S_OK) ERROR("[%s:%d] RPC return != S_OK\n",__FUNCTION__,__LINE__);

    AUDIO_INFO("SetSkipFlushMode(%d)\n", parameter.privateInfo[0]);
    return S_OK;
}

UINT32 DEC_PrivateInfo(Base* pBaseObj, UINT32 infoId, UINT8* pData, UINT32 length)
{
    Pin* pInPin = (Pin*)pBaseObj->inPin;
    UINT32 paddr;
    RINGBUFFER_HEADER* vaddr = NULL;

    /* Be caution of DTV case: sdec -> audio dec, no input pin in this case */
    if(pInPin) pInPin->GetBufHeader(pInPin, (void**)&vaddr, &paddr);

    if(infoId == INFO_DELIVER_INFO && length == sizeof(DELIVERINFO))
    {
        //INFO("[%s] DeliverInfo\n",pBaseObj->name);
    }
    else if(infoId == INFO_PTS)
    {
        AUDIO_DEC_PTS_INFO cmd ;

        if(pInPin == NULL)
        {
            ERROR("[%s][%s] PTS shouldn't exist in DTV\n", pBaseObj->name, __FUNCTION__);
            return S_FALSE;
        }

        if(vaddr == NULL)
        {
            INFO("%s %d vaddr is NULL \n", __FUNCTION__, __LINE__);
            return S_FALSE;
        }

        cmd.header.type = AUDIO_DEC_INBAND_CMD_TYPE_PTS;
        cmd.header.size = sizeof(AUDIO_DEC_PTS_INFO);
        cmd.PTSH        = (*(SINT64*)pData) >> 32;
        cmd.PTSL        = (*(SINT64*)pData);
        IPC_WriteU32((BYTE*)&(cmd.wPtr), vaddr->writePtr);

        DeliverInBandCommand(pBaseObj, (BYTE*)&cmd, cmd.header.size);
    }
    else if(infoId == INFO_EOS)
    {
        AUDIO_DEC_EOS cmd;
        INFO("[%s] EOS\n",pBaseObj->name);
        if(pInPin == NULL)
        {
            ERROR("[%s][%s] EOS shouldn't exist in DTV\n", pBaseObj->name, __FUNCTION__);
            return S_FALSE;
        }

        if(vaddr == NULL)
        {
            INFO("%s %d vaddr is NULL \n", __FUNCTION__, __LINE__);
            return S_FALSE;
        }

        cmd.header.type = AUDIO_DEC_INBAND_CMD_TYPE_EOS;
        cmd.header.size = sizeof(AUDIO_DEC_EOS);
        cmd.EOSID       = (*(UINT32*)pData);
        IPC_WriteU32( (BYTE *) &(cmd.wPtr), vaddr->writePtr);

        DeliverInBandCommand(pBaseObj, (BYTE*)&cmd, cmd.header.size);
    }
    else if(infoId == INFO_DDP_METADATA)
    {
        AUDIO_DDP_METADATA *cmd = (AUDIO_DDP_METADATA*)pData;

        INFO("[%s] DDP Metadata\n",pBaseObj->name);

        if(vaddr == NULL)
        {
            INFO("%s %d vaddr is NULL \n", __FUNCTION__, __LINE__);
            return S_FALSE;
        }

        cmd->header.type = AUDIO_DEC_INBAND_CMD_TYPE_DDP_METADATA;
        cmd->header.size = sizeof(AUDIO_DDP_METADATA);
        IPC_WriteU32((BYTE*)&cmd->wPtr, vaddr->writePtr);

        DeliverInBandCommand(pBaseObj, (BYTE*)cmd, cmd->header.size);
    }
    else if(infoId == INFO_AAC_METADATA)
    {
        AUDIO_AAC_METADATA *cmd = (AUDIO_AAC_METADATA*)pData;

        INFO("[%s] AAC Metadata\n",pBaseObj->name);

        if(vaddr == NULL)
        {
            INFO("%s %d vaddr is NULL \n", __FUNCTION__, __LINE__);
            return S_FALSE;
        }

        cmd->header.type = AUDIO_DEC_INBAND_CMD_TYPE_AAC_METADATA;
        cmd->header.size = sizeof(AUDIO_AAC_METADATA);
        IPC_WriteU32((BYTE*)&cmd->wPtr, vaddr->writePtr);

        DeliverInBandCommand(pBaseObj, (BYTE*)cmd, cmd->header.size);
    }
    else if(infoId == INFO_DAP_PARAMS)
    {
        DAP_SELFTEST_PARAM *cmd = (DAP_SELFTEST_PARAM*)pData;

        INFO("[%s] DAP Params\n",pBaseObj->name);

        if(vaddr == NULL)
        {
            INFO("%s %d vaddr is NULL \n", __FUNCTION__, __LINE__);
            return S_FALSE;
        }

        cmd->header.type = AUDIO_DEC_INBAND_CMD_TYPE_DAP_PARAMS;
        cmd->header.size = sizeof(DAP_SELFTEST_PARAM);
        IPC_WriteU32((BYTE*)&cmd->wPtr, vaddr->writePtr);

        DeliverInBandCommand(pBaseObj, (BYTE*)cmd, cmd->header.size);
    }
    else if(infoId == INFO_DAP_OAMDI)
    {
        DAP_OAMDI_INFO *cmd = (DAP_OAMDI_INFO*)pData;

        INFO("[%s] DAP Oamdi\n",pBaseObj->name);
        if(pInPin == NULL || vaddr == NULL)
            return S_FALSE;

        cmd->header.type = AUDIO_INBAND_CMD_DAP_OAMDI_INFO;
        cmd->header.size = sizeof(DAP_OAMDI_INFO);
        IPC_WriteU32((BYTE*)&cmd->wPtr, vaddr->writePtr);

        DeliverInBandCommand(pBaseObj, (BYTE*)cmd, cmd->header.size);
    }
    else if(infoId == INFO_MIXERE_AD_DESCRIPTOR)
    {
        AUDIO_DEC_AD_DESCRIPTOR *cmd = (AUDIO_DEC_AD_DESCRIPTOR*)pData;

        INFO("[%s] MIXERE_AD_DESCRIPTOR\n",pBaseObj->name);
        if(vaddr == NULL)
        {
            INFO("%s %d vaddr is NULL \n", __FUNCTION__, __LINE__);
            return S_FALSE;
        }

        cmd->header.type = AUDIO_DEC_INBAND_CMD_TYPE_AD_DESCRIPTOR;
        cmd->header.size = sizeof(AUDIO_DEC_AD_DESCRIPTOR);
        IPC_WriteU32((BYTE*)&cmd->wPtr, vaddr->writePtr);

        DeliverInBandCommand(pBaseObj, (BYTE*)cmd, cmd->header.size);
    }
    else if(infoId == INFO_MS12_MIXER_CMD)
    {
        AUDIO_MS12_MIXER_CMD *cmd = (AUDIO_MS12_MIXER_CMD*)pData;

        INFO("[%s] MS12_DDP_METADATA \n",pBaseObj->name);
        if(vaddr == NULL)
        {
            INFO("%s %d vaddr is NULL \n", __FUNCTION__, __LINE__);
            return S_FALSE;
        }

        cmd->header.type = AUDIO_INBAND_CMD_MS12_MIXER_CMD;
        cmd->header.size = sizeof(AUDIO_MS12_MIXER_CMD);

        DeliverInBandCommand(pBaseObj, (BYTE*)cmd, cmd->header.size);
    }
    else if(infoId == INFO_MS12_IIDK_USE_CASE)
    {
        IIDK_USE_CASE_INFO *cmd = (IIDK_USE_CASE_INFO*)pData;

        INFO("[%s] MS12_IIDK_USE_CASE \n",pBaseObj->name);
        if(vaddr == NULL)
        {
            INFO("%s %d vaddr is NULL \n", __FUNCTION__, __LINE__);
            return S_FALSE;
        }
        cmd->header.type = AUDIO_INBAND_CMD_MS12_IIDK_USE_CASE;
        cmd->header.size = sizeof(IIDK_USE_CASE_INFO);
        IPC_WriteU32((BYTE*)&cmd->wPtr, vaddr->writePtr);

        DeliverInBandCommand(pBaseObj, (BYTE*)cmd, cmd->header.size);
    }
    else if(infoId == INFO_MS12_IIDK_INIT_PARAMS)
    {
        IIDK_INIT_PARAMS_INFO *cmd = (IIDK_INIT_PARAMS_INFO*)pData;

        INFO("[%s] INFO_MS12_IIDK_INIT_PARAMS \n",pBaseObj->name);
        if(vaddr == NULL)
        {
            INFO("%s %d vaddr is NULL \n", __FUNCTION__, __LINE__);
            return S_FALSE;
        }
        cmd->header.type = AUDIO_INBAND_CMD_MS12_IIDK_INIT_PARAMS;
        cmd->header.size = sizeof(IIDK_INIT_PARAMS_INFO);
        IPC_WriteU32((BYTE*)&cmd->wPtr, vaddr->writePtr);

        DeliverInBandCommand(pBaseObj, (BYTE*)cmd, cmd->header.size);
    }
    else if(infoId == INFO_MS12_IIDK_RUNTIME_PARAMS)
    {
        IIDK_RUNTIME_PARAMS_INFO *cmd = (IIDK_RUNTIME_PARAMS_INFO*)pData;

        INFO("[%s] INFO_MS12_IIDK_RUNTIME_PARAMS \n",pBaseObj->name);
        if(vaddr == NULL)
        {
            INFO("%s %d vaddr is NULL \n", __FUNCTION__, __LINE__);
            return S_FALSE;
        }
        cmd->header.type = AUDIO_INBAND_CMD_MS12_IIDK_RUNTIME_PARAMS;
        cmd->header.size = sizeof(IIDK_RUNTIME_PARAMS_INFO);
        IPC_WriteU32((BYTE*)&cmd->wPtr, vaddr->writePtr);

        DeliverInBandCommand(pBaseObj, (BYTE*)cmd, cmd->header.size);
    }
    else if(infoId == INFO_AUDIO_FORMAT)
    {
        AUDIO_DEC_NEW_FORMAT cmd;
        AUDIO_FORMAT *pAUDIOFORMAT = (AUDIO_FORMAT*)pData;

        if(vaddr == NULL && pAUDIOFORMAT->wptr == 0)
        {
            INFO("%s %d vaddr is NULL && wptr == %d \n", __FUNCTION__, __LINE__,pAUDIOFORMAT->wptr);
            return S_FALSE;
        }

        memset(&cmd,0,sizeof(cmd));

        cmd.header.type = AUDIO_DEC_INBAMD_CMD_TYPE_NEW_FORMAT;
        cmd.header.size = sizeof(AUDIO_DEC_NEW_FORMAT);
        GetAudioTypeAndPrivateInfo((AUDIO_FORMAT*)pData, &cmd.audioType, cmd.privateInfo);
		if(vaddr == NULL) {
			cmd.wPtr = pAUDIOFORMAT->wptr;
		} else {
			IPC_WriteU32( (BYTE *) &(cmd.wPtr), vaddr->writePtr);
		}

        DeliverInBandCommand(pBaseObj, (BYTE*)&cmd, cmd.header.size);
    }
    else if(infoId == INFO_AUDIO_MIX_INFO)
    {
        /*
        1.  1PID, substream(1/2/3)_flag = 0  => ao mix off

            Decoder->SetSubDecoderMode(0);
            Decoder->SetSubStreamId(0);
            Decoder2->SetSubDecoderMode(1);
            Decoder2->SetSubStreamID(0);


        2.  1PID, substream(1/2/3)_flag = 1 => ao mix on

            Decoder->SetSubDecoderMode(0|0x40);
            Decoder->SetSubStreamId(0);
            Decoder2->SetSubDecoderMode(1|0x40);
            Decoder2->SetSubStreamID(1);

        3.  2PID,                            => ao mix on

            Decoder->SetSubDecoderMode(0|0x40);
            Decoder->SetSubStreamId(0);
            Decoder2->SetSubDecoderMode(1|0x40);
            Decoder2->SetSubStreamID(0);
        */
        DUAL_DEC_INFO* info = (DUAL_DEC_INFO*)pData;
        if(info->bEnable)
        {
            SetSubDecoderMode(pBaseObj, info->subDecoderMode|0x40, info->subStreamId);
        }
        else
        {
            SetSubDecoderMode(pBaseObj, info->subDecoderMode, 0);
        }
        return S_OK;
    }
    else if(infoId == ENUM_AUDIO_RPC_PRIVATE_INFO)
    {
        AUDIO_RPC_PRIVATEINFO_TO_SYS* pInfo;

        INFO("[%s] RPC to System PrivateInfo\n",pBaseObj->name);
        pInfo = (AUDIO_RPC_PRIVATEINFO_TO_SYS*)pData;

        switch (pInfo->type)
        {
        case ENUM_PRIVATEINFO_AUDIO_HDMV_UNCERTAIN_TYPE:
            {
                ERROR("Agent Return HDMV UNCERTAIN_TYPE\n");
            }
            break;
        case ENUM_PRIVATEINFO_AUDIO_DECODRER_MASSAGE:
            {
                UINT32 msgID;
                msgID = pInfo->privateInfo[0];
                INFO("Audio Decoder Send FE_Playback_DecoderMessage %d\n",msgID);
                if(msgID == DECODER_MESSAGE_AUDIO_FIRST_FRAME_DECODED)
                {
                    INFO("Decode 1st frame\n");
                }
            }
            break;
        default:
            break;
        }
    }

    return S_OK;
}

UINT32 DEC_Remove(Base* pBaseObj)
{
    DEC* pDECObj = (DEC*)pBaseObj->pDerivedObj;
    UINT32 pAddr;
    RINGBUFFER_HEADER *vAddr;
    UINT8* pBuffer;

    /* Reset ICQ buffer */
    pDECObj->pICQPin->GetBuffer(pDECObj->pICQPin, &pBuffer, NULL);
    pDECObj->pICQPin->SetReadPtr(pDECObj->pICQPin, pBuffer, 0);// reset
    pDECObj->pICQPin->SetWritePtr(pDECObj->pICQPin, pBuffer);// reset

    pDECObj->pICQPin->GetBufHeader(pDECObj->pICQPin, (void**)&vAddr, &pAddr);
    pDECObj->InitICQRingBuf(pBaseObj, pAddr);

    return Remove( pBaseObj);

}
void delete_DEC(Base* pBaseObj)
{
    DEC* pDECObj = NULL;
    if(pBaseObj == NULL) return;
    pDECObj = (DEC*)pBaseObj->pDerivedObj;

    SetSubDecoderMode(pBaseObj, DEC_IS_MAIN, 0);

    pBaseObj->Stop(pBaseObj);

    DEC_Remove(pBaseObj);
    pBaseObj ->Remove = NULL;

    del_id_from_map(pBaseObj->GetAgentID(pBaseObj));

    StopDwnStrmQMonitorThread(pBaseObj);

    if(pDECObj->pICQPin != NULL)
    {
        pDECObj->pICQPin->Delete(pDECObj->pICQPin);
    }
    if(pDECObj->pDwnStrmQ != NULL)
    {
        pDECObj->pDwnStrmQ->Delete(pDECObj->pDwnStrmQ);
    }
    kfree(pDECObj);
    return delete_base(pBaseObj);
}

Base* new_DEC(int idx)
{
    /* object init */
    UINT32 pAddr;
    SINT32 i;
    RINGBUFFER_HEADER *vAddr;
    char name[] = "DEC";
    DEC* pDECObj = NULL;
    Base* pObj = new_base();
    if(pObj == NULL)
    {
        return NULL;
    }
    pDECObj = (DEC*)kmalloc(sizeof(DEC), GFP_KERNEL);
    if(pDECObj == NULL)
    {
        pObj->Delete(pObj);
        return NULL;
    }
    sprintf(pObj->name, "%s-%d", name, idx);
    pObj->pDerivedObj = pDECObj;

    INFO("[%s] \n", __FUNCTION__);

    /* setup member functions */
    pObj->Delete                    = delete_DEC;
    pObj->PrivateInfo               = DEC_PrivateInfo;
    pObj->Remove                    = DEC_Remove;
    pDECObj->InitBSRingBuf          = InitBSRingBuf;
    pDECObj->InitICQRingBuf         = InitICQRingBuf;
    pDECObj->InitDwnStrmQueue       = InitDwnStrmQueue;
    pDECObj->GetOUTRingBufPhyAddr   = GetOUTRingBufPhyAddr;
    pDECObj->GetICQRingBufPhyAddr   = GetICQRingBufPhyAddr;
    pDECObj->GetDSQRingBufPhyAddr   = GetDSQRingBufPhyAddr;
    pDECObj->ResetRingBuf           = ResetRingBuf;
    pDECObj->SetOutRingNumOfReadPtr = SetOutRingNumOfReadPtr;
    pDECObj->GetAudioFormatInfo     = GetAudioFormatInfo;
    pDECObj->SetChannelSwap         = SetChannelSwap;
    pDECObj->GetCurrentPTS          = GetCurrentPTS;
    pDECObj->SetDecoderSRCMode      = SetDecoderSRCMode;
    pDECObj->IsESExist              = IsESExist;
    pDECObj->SetSkipFlushMode       = SetSkipFlushMode;

    pDECObj->StartDwnStrmQMonitorThread = StartDwnStrmQMonitorThread;
    pDECObj->StopDwnStrmQMonitorThread  = StopDwnStrmQMonitorThread;

    /* agent init */
    pObj->instanceID = CreateAgent(AUDIO_DECODER);
    if(pObj->instanceID == UNDEFINED_AGENT_ID)
    {
        pObj->Delete(pObj);
        return NULL;
    }

    pObj->inPinID = BASE_BS_IN;
    pObj->outPin = new_multiPin(AUDIO_DEC_OUTPUT_BUF_SIZE, AUDIO_MAX_CHANNEL_NUM);
    pObj->outPinID = PCM_OUT_RTK;
    pObj->InitOutRingBuf(pObj);

    pDECObj->pICQPin = new_pin(AUDIO_DEC_INBAND_BUF_SIZE);
    pDECObj->pDwnStrmQ = new_pin(AUDIO_DEC_MSG_BUF_SIZE);

	pDECObj->adec_tsk = kthread_create(DwnStrmQ_MonitorThread, (void*)pObj, "adec_tsk");
	if (IS_ERR(pDECObj->adec_tsk)) {
		pDECObj->adec_tsk = NULL;
		return NULL;
	}

    /* setup Inband Command Queue buffer */
    pDECObj->pICQPin->GetBufHeader(pDECObj->pICQPin, (void**)&vAddr, &pAddr);
    pDECObj->InitICQRingBuf(pObj, pAddr);

    /* setup Message Queue buffer (downstreamQ) */
    pDECObj->InitDwnStrmQueue(pObj);

    pDECObj->dwn_strm_sem = &Dwn_strm_sem;

    pDECObj->m_ena_delayRp = DEFAULT_DELAYRP_SETTING; // default is also TRUE in Agent
    pDECObj->prev_PTS = pDECObj->curr_PTS = pDECObj->prev_2nd_PTS = 0;
    pDECObj->rate = 0;
    pDECObj->channel = 0;
    for(i = 0; i < AUDIO_MAX_CHANNEL_NUM; i++)
        pDECObj->chan_idx[i] = -1;

    pDECObj->skipflush = FALSE;
    pDECObj->SetDecoderSRCMode(pObj, true, 0);
	wake_up_process(pDECObj->adec_tsk);

    add_id_to_map(pObj, pObj->GetAgentID(pObj));
    return pObj;
}
