/******************************************************************************
 *
 *   Copyright(c) 2016 Realtek Semiconductor Corp. All rights reserved.
 *
 *   @author yungjui.lee@realtek.com
 *
 *****************************************************************************/

#include <linux/slab.h>
#include "audio_process_node.h"
#ifdef RESAMPLE
#include "resample_node.h"
#endif
#include "audio_inc.h"

#if defined(OAR_ENABLE)
#include "oar_middleware.h"
#endif

#if defined(ARM_MPEGHDEC_ENABLE)
#include "mpeghdec_middleware.h"
#endif

#ifdef BS_INBUF_CACHED
extern inline void DEMUX_flushMemory(unsigned char *pSrc, unsigned char *pSrcPhy, int bytes)
void bs_cache_flushed(Pin* pPinObj, UINT32 readptr, UINT32 size)
{
    UINT8 *base, *limit;
    UINT32 bufferSize;
    pPinObj->GetBuffer(pPinObj, &base, &bufferSize);
    limit = base + bufferSize;

    if (readptr + size > (UINT32)limit)
    {
        //TODO  need vir, phy addr
        /*RTKAudio_flush_memory((void*)readptr, (UINT32)limit - readptr);*/
        DEMUX_flushMemory((void*)readptr, (UINT32)limit - readptr);
        DEMUX_flushMemory((void*)base, readptr + size - (UINT32)limit);
    }
    else
    {
        DEMUX_flushMemory((void*)readptr, size);
    }
}
#endif

static void InitialNode(node_handle h_node)
{
    if (h_node->bInpinPayloadCached)
        h_node->inPktPin = new_packetPin_Cached(h_node->inBufSize, h_node->inPinCnt);
    else
        h_node->inPktPin = new_packetPin(h_node->inBufSize, h_node->inPinCnt);
    h_node->outPktPin = new_packetPin(h_node->outBufSize, h_node->outPinCnt);
}

void UninitialNode(node_handle h_node)
{
#ifdef AC4DEC_TA
    if (h_node->nodeType == ENUM_NODE_AC4_DEC) return;
#endif

    if(h_node->inPktPin != NULL)
        h_node->inPktPin->Delete(h_node->inPktPin);
    if(h_node->outPktPin!= NULL)
        h_node->outPktPin->Delete(h_node->outPktPin);
    kfree(h_node);
}

void calculate_kcps(node_handle h_node, UINT32 dur)
{
    //CLOCK*duration*samplerate/samples/90
    UINT32 tmp = CLOCK_RATE * h_node->plh_out.pcm_samplrate;
    UINT32 tmp1;
    if (h_node->nodeType == ENUM_NODE_DDP_ENC || h_node->nodeType == ENUM_NODE_AAC_ENC)
        tmp1 = (h_node->plh_in.payload_size >> 2) * 90;
    else
        tmp1 = (h_node->plh_out.payload_size >> 2) * 90;

    if(tmp1)
        h_node->plh_out.frame_mcps = (tmp/tmp1)*dur;
    //ERROR("%s %s id:%ld frame_kcps:%ld, duration:%upts, sample_rate:%u, samples:%u\n", __FUNCTION__, h_node->name, h_node->plh_out.frameid, h_node->plh_out.frame_mcps,duration, (UINT32)h_node->plh_out.pcm_samplrate, (UINT32)h_node->plh_out.payload_size/4);
}

void FwFlushCmd(node_handle h_node)
{
    SYS_PROCESS_COMMAND_INFO cmd_info;
    Pin* pOutPktPin;
    pOutPktPin = h_node->outPktPin;

    memset(&(cmd_info), 0, sizeof(cmd_info));
    cmd_info.header.type = AUDIO_SYS_PROCESS_CMD_COMMAND_INFO;
    cmd_info.header.size = sizeof(cmd_info);
    cmd_info.cmd_Type = SYS_FLUSH;
    pOutPktPin->MemCopyToWritePtr(pOutPktPin, (UINT8*)&(cmd_info), sizeof(cmd_info));
    pOutPktPin->AddWritePtr(pOutPktPin, sizeof(cmd_info));
    INFO("%s send flush command\n", h_node->name);
}

static int FlushNode(node_handle h_node)
{
    Pin *pInPktPin, *pOutPktPin, *pInDataPinObj;
    int inputsize = 0;
    UINT8 *readPtr, *base;
    UINT32 bufferSize;
    SYS_PROCESS_COMMAND_INFO cmd_info;

    pInPktPin = h_node->inPktPin;
    pInDataPinObj = ((PacketPin*)pInPktPin->pDerivedObj)->pDataPinObj;
    pOutPktPin = h_node->outPktPin;

    if(IsPtrValid(h_node->flushToNewPtr, pInDataPinObj))
    {
        pInDataPinObj->GetBuffer(pInDataPinObj, &base, &bufferSize);
        readPtr = pInDataPinObj->GetReadPtr(pInDataPinObj, 0);
        inputsize = (int)h_node->flushToNewPtr - (int)readPtr;
        if(inputsize < 0)
            inputsize += bufferSize;
        INFO("[LIBK]%s want flush %x to %x sz (%d)\n", h_node->name, (UINT32)pInDataPinObj->GetReadPtr(pInDataPinObj, 0), h_node->flushToNewPtr, inputsize);
    }
    else
    {
        inputsize = pInDataPinObj->GetReadSize(pInDataPinObj, 0); // flush all
    }

    pInDataPinObj->AddReadPtr(pInDataPinObj, inputsize); // shift a frame size
    INFO("%s flush to %x sz (%d)\n", h_node->name, (UINT32)pInDataPinObj->GetReadPtr(pInDataPinObj, 0), inputsize);

    if(h_node->nodeType != ENUM_NODE_DDP_ENC)
    {
        memset(&(cmd_info), 0, sizeof(cmd_info) );

        cmd_info.header.type = AUDIO_SYS_PROCESS_CMD_COMMAND_INFO;
        cmd_info.header.size = sizeof(cmd_info);
        cmd_info.cmd_Type = SYS_FLUSH_FINISH;
        cmd_info.cmd_Data = (long) pInDataPinObj->GetReadPtr(pInDataPinObj, 0);
        cmd_info.cmd_Data  = (long)ConvertVirtual2Phy( pInDataPinObj, cmd_info.cmd_Data );

        INFO("%s flush cmd_Data(%x),size:%ld\n", h_node->name, (UINT32)cmd_info.cmd_Data, cmd_info.header.size);
        pOutPktPin->MemCopyToWritePtr(pOutPktPin, (UINT8*)&(cmd_info), sizeof(cmd_info));
        pOutPktPin->AddWritePtr(pOutPktPin, sizeof(cmd_info));
    }

    return TRUE;
}

int NodeInputTask(node_handle h_node)
{
    int res = TRUE;
    Pin *pInPktPin, *pInDataPinObj;
    int numPayload = 0;
    AUDIO_INBAND_CMD_PKT_HEADER header;
    SYS_PROCESS_COMMAND_INFO cmd_info;
#ifdef DEBUG_DURATION
    int64_t getcmd_ms = 0;
#endif

    pInPktPin = h_node->inPktPin;
    h_node->subStreamId = -1;

#ifdef DEBUG_DURATION
    getcmd_ms = pli_getPTS();
#endif
    while(pInPktPin->GetReadSize(pInPktPin, 0) >= sizeof(AUDIO_INBAND_CMD_PKT_HEADER) && numPayload <= 1) {
#ifdef DEBUG_DURATION
        if (((pli_getPTS() - getcmd_ms)) / 90000 > 10){
            ERROR("[%s] main get header timeout\n", __FUNCTION__);
            return TRUE;
        }
#endif
        memset(&header, 0, sizeof(AUDIO_INBAND_CMD_PKT_HEADER));
        pInPktPin->MemCopyFromReadPtr(pInPktPin, (UINT8*)&header, sizeof(AUDIO_INBAND_CMD_PKT_HEADER), AUDIO_NONE);
        //INFO("%s, %s header.type(%d), size(%d)\n", __FUNCTION__, h_node->name, header.type, header.size);
        switch(header.type) {
            case AUDIO_SYS_PROCESS_CMD_PAYLOAD_HEADER:
                if(numPayload == 0) {
                    pInPktPin->MemCopyFromReadPtr(pInPktPin, (UINT8*)&(h_node->plh_in), sizeof(SYS_PROCESS_PAYLOAD_HEADER), AUDIO_NONE);
                    pInPktPin->AddReadPtr(pInPktPin, h_node->plh_in.header.size);
                    //ShowPinInfo(pInPktPin);
                } else {
                    //INFO("%s %s Next payload\n", __FUNCTION__, h_node->name);
                }
                numPayload++;
                res = TRUE;
                break;
            case AUDIO_SYS_PROCESS_CMD_COMMAND_INFO:
                pInPktPin->MemCopyFromReadPtr(pInPktPin, (UINT8*)&cmd_info, sizeof(SYS_PROCESS_COMMAND_INFO), AUDIO_NONE);
                INFO("%s %s cmd_info.header.size:%ld type:%ld\n", __FUNCTION__, h_node->name, cmd_info.header.size, cmd_info.cmd_Type);
                pInPktPin->AddReadPtr(pInPktPin, cmd_info.header.size);
                if(cmd_info.cmd_Type == SYS_FLUSH) {
                    h_node->flushToNewPtr = cmd_info.cmd_Data;
                    // check input
                    pInDataPinObj = ((PacketPin*)pInPktPin->pDerivedObj)->pDataPinObj;
                    h_node->flushToNewPtr = (int)ConvertPhy2Virtual(pInDataPinObj, h_node->flushToNewPtr);
                    INFO("%s %s Flush command to %x \n", __FUNCTION__, h_node->name,  h_node->flushToNewPtr);
                    FlushNode(h_node);
                    if (h_node->initial)
                        h_node->initial(h_node);
                    res = FALSE;
                } else if(cmd_info.cmd_Type == SYS_SELECT_SUBSTREAM) {
                    h_node->subStreamId = cmd_info.cmd_Data;
                    res = TRUE;
                } else if (cmd_info.cmd_Type == SYS_INIT) {
                    if (h_node->initial)
                    {
                        h_node->initialData = cmd_info.cmd_Data;
                        h_node->initial(h_node);
                    }
                    res = FALSE;
                } else if (cmd_info.cmd_Type == SYS_DLB_DRC) {
                    if (h_node->drc_setting)
                    {
                        h_node->drc_setting(h_node, cmd_info.cmd_Data);
                    }
                    res = FALSE;
                }
                break;
            case AUDIO_SYS_PROCESS_CMD_INIT_RING_HEADER:
                // should not receive this header,
                // if received, it means FW initialize not complete,
                // herefore drop it.
                //ERROR("%s: FW is not ready\n", h_node->name);
                res = FALSE;
                break;
            case AUDIO_SYS_PROCESS_CMD_COMMAND_EOS:
                pInPktPin->MemCopyFromReadPtr(pInPktPin, (UINT8*)&(h_node->plh_in), sizeof(SYS_PROCESS_PAYLOAD_HEADER), AUDIO_NONE);
                INFO("%s %s EOS cmd_info.header.size:%ld\n", __FUNCTION__, h_node->name, h_node->plh_in.header.size);
                pInPktPin->AddReadPtr(pInPktPin, h_node->plh_in.header.size);
                h_node->eos_cnt = 5;
                ERROR("%s get eos. count:%d \n", h_node->name, h_node->eos_cnt); // cert will get 3 times
                res = TRUE;
                break;
            case AUDIO_DEC_INBAND_CMD_TYPE_DDPENC_METADATA:
                //INFO("%s %s case AUDIO_DEC_INBAND_CMD_TYPE_DDPENC_METADATA\n", __FUNCTION__, h_node->name);
                return TRUE;//exit NodeInput When header type is AUDIO_DEC_INBAND_CMD_TYPE_DDPENC_METADATA to do encode active ddp
                break;
            case AUDIO_SYS_PROCESS_CMD_COMMAND_AC4_UI_SETTING:
#if defined(AC4DEC)
                pInPktPin->MemCopyFromReadPtr(pInPktPin, (UINT8*)&(h_node->ui_set), sizeof(AUDIO_AC4_UI_SETTING), AUDIO_NONE);
                pInPktPin->AddReadPtr(pInPktPin, h_node->ui_set.header.size);
                if (h_node->setting)
                    h_node->setting(h_node);
                res = TRUE;
#endif
                break;
            case AUDIO_SYS_PROCESS_CMD_COMMAND_MPEGH_UI:
#if defined(AC4DEC)
                pInPktPin->MemCopyFromReadPtr(pInPktPin, (UINT8*)&(h_node->mpegh_ui_info) , sizeof((h_node->mpegh_ui_info)), AUDIO_NONE);
                pInPktPin->AddReadPtr(pInPktPin, h_node->mpegh_ui_info.header.size);
                if (h_node->setting)
                    h_node->setting(h_node);
                res = TRUE;
#endif
                break;
            default:
                INFO("%s %s Unknown type:%d, size:%ld, numPayload:%d, readPtr:%x\n", __FUNCTION__, h_node->name, header.type, header.size, numPayload, (UINT32)pInPktPin->GetReadPtr(pInPktPin, 0));
                if( (header.type >= 0) && (header.type < AUDIO_SYS_PROCESS_CMD_COMMAND_NUM) && (pInPktPin->CheckReadableSize(pInPktPin, header.size) ==NO_ERR ) )
                {
                    pInPktPin->AddReadPtr(pInPktPin, header.size);
                }
                else
                {
                    ERROR("%s %s SKIP this invalid command\n", __FUNCTION__, h_node->name);
                    ShowPinInfo(pInPktPin);
                }
                res = FALSE;
                break;
        }
    }
    return res;
}

int NodeOutputTask(node_handle h_node)
{
    int ch;
    Pin *pOutPktPin, *pOutDataPinObj, *pInnerPin;

    //INFO("[LIBK][%s] node type(%d), h_node->cmdMetadata(%p), (%d, %d)\n", __FUNCTION__, h_node->nodeType, h_node->cmdMetadata, h_node->plh_out.pcm_channels, h_node->plh_out.payload_size);
    //INFO("cmdMetadataSize:%x,pcmdata[0]:%x,pcmdata[1]:%x\n",h_node->cmdMetadataSize,h_node->pcmdata[0],h_node->pcmdata[1]);
    //if (!h_node->cmdMetadata) return TRUE;

    pOutPktPin = h_node->outPktPin;
    pOutDataPinObj = ((PacketPin*)pOutPktPin->pDerivedObj)->pDataPinObj;

    for (ch=0; ch<h_node->plh_out.pcm_channels; ch++)
    {
        if (ch == 0)
            pInnerPin = pOutDataPinObj;
        else
            pInnerPin = ((MultiPin*)pOutDataPinObj->pDerivedObj)->pPinObjArr[ch-1];

        if(h_node->nodeType == ENUM_NODE_DDP_ENC || h_node->nodeType == ENUM_NODE_AAC_ENC)
        {
            //used for encoded bs data
            pInnerPin->MemCopyToWritePtrNoInverse(pInnerPin, (UINT8*)h_node->pcmdata[ch], h_node->plh_out.payload_size);
            //INFO("[LIBK]%s (%s), (%d, %d)\n", __FUNCTION__, h_node->name, h_node->plh_out.pcm_channels, h_node->plh_out.payload_size);
        }
        else
        {
            pInnerPin->MemCopyToWritePtr(pInnerPin, (UINT8*)h_node->pcmdata[ch], h_node->plh_out.payload_size);
        }

        pInnerPin->AddWritePtr(pInnerPin, h_node->plh_out.payload_size);
    }

    if((h_node->cmdMetadata != NULL) && (h_node->cmdMetadataSize != 0))
    {
        h_node->avg_mcps += h_node->plh_out.frame_mcps;
        if (!(h_node->plh_out.frameid % 500) && h_node->plh_out.frameid != 0)
        {
            h_node->avg_mcps /= 500;
            ((SYS_PROCESS_PAYLOAD_HEADER*)h_node->cmdMetadata)->frame_mcps = h_node->avg_mcps;
            h_node->avg_mcps = 0;
            INFO("%s %s frameid:%ld, avg kcps:%ld n_bytes_buffering:%ld\n", __FUNCTION__, h_node->name, h_node->plh_out.frameid, ((SYS_PROCESS_PAYLOAD_HEADER*)h_node->cmdMetadata)->frame_mcps, h_node->plh_out.n_bytes_buffering);
        }
        if (h_node->plh_out.b_act_polling_delay)
            ERROR("%s %s frame_id:%ld, activate_ex_ms:%d, polling_act_ms:%ld, n_bytes_buffering:%ld\n", __FUNCTION__, h_node->name, h_node->plh_out.frameid, h_node->activate_ex_ms, h_node->plh_out.polling_act_ms, h_node->plh_out.n_bytes_buffering);

        ((SYS_PROCESS_PAYLOAD_HEADER*)h_node->cmdMetadata)->b_act_polling_delay = h_node->plh_out.b_act_polling_delay;
        ((SYS_PROCESS_PAYLOAD_HEADER*)h_node->cmdMetadata)->polling_act_ms = h_node->plh_out.polling_act_ms;
        pOutPktPin->MemCopyToWritePtr(pOutPktPin, (UINT8*)h_node->cmdMetadata, h_node->cmdMetadataSize);
        pOutPktPin->AddWritePtr(pOutPktPin, h_node->cmdMetadataSize);
        memset(&h_node->plh_out, 0, sizeof(SYS_PROCESS_PAYLOAD_HEADER));
        h_node->cmdMetadata = NULL;
        h_node->cmdMetadataSize = 0;
    }

    return TRUE;
}

node_handle CreateNode(SYSTEM_MESSAGE_INFO *info)
{
    node_handle h_node = (node_handle)kmalloc(sizeof(Node_Struct), GFP_KERNEL);
    if(h_node == NULL)
        return NULL;
    memset(h_node, 0, sizeof(Node_Struct));
    INFO("[LIBK]CreateNode type(%d), ENUM_NODE_AC3_DEC(%d)\n", info->type, ENUM_NODE_AC3_DEC);
    switch(info->type)
    {
#if 0
    case ENUM_NODE_DTS_DEC:
#if defined(DTS_M6)
        h_node->nodeType = ENUM_NODE_DTS_DEC;
        h_node->inPinCnt = DECODER_INDATA_CH_CNT;
        h_node->outPinCnt = DECODER_OUTDATA_CH_CNT;
        h_node->inBufSize = 0x40000;
        h_node->outBufSize = 16384;
        h_node->bInpinPayloadCached = 1;
        h_node->threadSleepMs = 1;
        InitialNode(h_node);
        dtsdec_node_create(h_node, info->data);
#endif
        break;
    case ENUM_NODE_SRC:
#ifdef RESAMPLE
        h_node = resample_node_create(info->data);
#endif
        break;
    case ENUM_NODE_DUMP:
        h_node->nodeType = ENUM_NODE_DUMP;
        h_node->inPinCnt = 8;
        h_node->outPinCnt = 1;
        h_node->inBufSize = 0x14000;
        h_node->outBufSize = 1024;
        h_node->bInpinPayloadCached = 0;
        h_node->threadSleepMs = 10;
        InitialNode(h_node);
        file_writer_create(h_node, info->data);
        break;
    case ENUM_NODE_AC3_DEC:
#ifdef UDC
        h_node->nodeType = ENUM_NODE_AC3_DEC;
        h_node->inPinCnt = DECODER_INDATA_CH_CNT;
        h_node->outPinCnt = DECODER_OUTDATA_CH_CNT;
        h_node->inBufSize = 16384;
        h_node->outBufSize = 16384;
        h_node->bInpinPayloadCached = 1;
        h_node->threadSleepMs = 5;
        h_node->eos_cnt = -1;
        InitialNode(h_node);
        udc_node_create(h_node, info->data);
#endif
        break;
    case ENUM_NODE_AC4_DEC:
#ifdef AC4DEC
#ifdef AC4DEC_TA
        h_node->nodeType = ENUM_NODE_AC4_DEC;
#ifndef AC4_CH_DEBUG
        h_node->inPinCnt = DECODER_INDATA_CH_CNT + 1; // add info pin
#else
        h_node->inPinCnt = DECODER_INDATA_CH_CNT + 1 + 1; // add info pin, debug bs pin
#endif
        h_node->outPinCnt = DECODER_OUTDATA_CH_CNT + 1; // add info pin
        h_node->inBufSize = 16384;
        h_node->outBufSize = 16384;
        h_node->bInpinPayloadCached = 0;
        h_node->threadSleepMs = 1;
        if (!ac4_node_create(h_node, info->data)) return NULL;
#else
        h_node->nodeType = ENUM_NODE_AC4_DEC;
        h_node->inPinCnt = DECODER_INDATA_CH_CNT;
        h_node->outPinCnt = DECODER_OUTDATA_CH_CNT;
        h_node->inBufSize = 16384;
        h_node->outBufSize = 16384;
        h_node->bInpinPayloadCached = 1;
        h_node->threadSleepMs = 5;
        InitialNode(h_node);
        if(!ac4_node_create(h_node, info->data))
        {
            UninitialNode(h_node);
            MY_LIBK_ERROR("%s create ac4 node fail\n", __FUNCTION__);
            return NULL;
        }
#endif // end of AC4DEC_TA
#endif
        break;
    case ENUM_NODE_AAC_DEC:
#ifdef HEAAC
        h_node->nodeType = ENUM_NODE_AAC_DEC;
        h_node->inPinCnt = DECODER_INDATA_CH_CNT;
        h_node->outPinCnt = DECODER_OUTDATA_CH_CNT;
        h_node->inBufSize = 12288;
        h_node->outBufSize = 16384;
        h_node->bInpinPayloadCached = 1;
        h_node->threadSleepMs = 5;
        InitialNode(h_node);
        heaacdec_node_create(h_node, info->data);
#elif defined(AACELD)
        h_node->nodeType = ENUM_NODE_AAC_DEC;
        h_node->inPinCnt = DECODER_INDATA_CH_CNT;
        h_node->outPinCnt = DECODER_OUTDATA_CH_CNT;
        h_node->inBufSize = 5376;
        h_node->outBufSize = 16384;
        h_node->bInpinPayloadCached = 1;
        h_node->threadSleepMs = 5;
        InitialNode(h_node);
        fdkaacdec_node_create(h_node, info->data);
#endif
        break;
    case ENUM_NODE_OPUS_DEC:
#ifdef OPUSDEC
        h_node->nodeType = ENUM_NODE_OPUS_DEC;
        h_node->inPinCnt =  DECODER_INDATA_CH_CNT;
        h_node->outPinCnt =  DECODER_OUTDATA_CH_CNT;
        h_node->inBufSize = 16384;
        h_node->outBufSize = 32768;
        h_node->bInpinPayloadCached = 1;
        h_node->threadSleepMs = 5;
        InitialNode(h_node);
        opusdec_node_create(h_node, info->data);
#endif
        break;
    case ENUM_NODE_MAT_DEC:
#ifdef MATDEC
        h_node->nodeType = ENUM_NODE_MAT_DEC;
        h_node->inPinCnt = DECODER_INDATA_CH_CNT;
        h_node->outPinCnt = DECODER_OUTDATA_CH_CNT; //TODO: tmp output 8 ch
        h_node->inBufSize = 16384*8;
        h_node->outBufSize = 16384*8;
        h_node->bInpinPayloadCached = 1;
        h_node->threadSleepMs = 1;
        InitialNode(h_node);
        matdec_node_create(h_node, info->data);
#endif
        break;
    case ENUM_NODE_DDP_ENC:
#if defined(ARM_DDPENC_ENABLE)
        h_node->nodeType = ENUM_NODE_DDP_ENC;
        h_node->inPinCnt = ENCODER_INDATA_CH_CNT;
        h_node->outPinCnt = ENCODER_OUTDATA_CH_CNT;
        h_node->inBufSize = 16384*2;
        h_node->outBufSize = 16384;
        h_node->bInpinPayloadCached = 0;
        h_node->threadSleepMs = 5;
        InitialNode(h_node);
        ddpenc_node_create(h_node, info->data);
        //INFO("[LIBK]%s nodeType(%d), (%d, %d)\n", __FUNCTION__, h_node->nodeType, h_node->plh_out.pcm_channels, h_node->plh_out.payload_size);
#endif
        break;
    case ENUM_NODE_MPEGH_DEC:
#if defined(ARM_MPEGHDEC_ENABLE)
        h_node->nodeType = ENUM_NODE_MPEGH_DEC;
        h_node->inPinCnt = DECODER_INDATA_CH_CNT;
        h_node->outPinCnt = DECODER_OUTDATA_CH_CNT;
        h_node->inBufSize = 16384;
        h_node->outBufSize = 16384;
        h_node->bInpinPayloadCached = 1;
        h_node->threadSleepMs = 5;
        InitialNode(h_node);
        mpeghdec_node_create(h_node, info->data);
        INFO("[%s] mpegh node create\n", __FUNCTION__);
#endif
        break;
    case ENUM_NODE_AAC_ENC:
#if defined(ARM_AACENC_ENABLE)
        h_node->nodeType = ENUM_NODE_AAC_ENC;
        h_node->inPinCnt = ENCODER_INDATA_CH_CNT;
        h_node->outPinCnt = ENCODER_OUTDATA_CH_CNT;
        h_node->inBufSize = 16384*2;
        h_node->outBufSize = 1024*4*4;
        h_node->bInpinPayloadCached = 0;
        h_node->threadSleepMs = 5;
        InitialNode(h_node);
        aacenc_node_create(h_node, info->data);
#endif
        break;
#endif
    case ENUM_NODE_MESSAGE_MANAGER:
#if defined(USE_INBAND_SYSPROC_COMMUNICATION)
        h_node->nodeType = ENUM_NODE_MESSAGE_MANAGER;
        h_node->inPinCnt = 1;
        h_node->outPinCnt = 1;
        h_node->inBufSize = 4096;
        h_node->outBufSize = 4096;
        h_node->bInpinPayloadCached = 0;
        h_node->threadSleepMs = 10;
        InitialNode(h_node);
        audio_manager_node_create(h_node, info->data);
#endif
        break;
    default:
        kfree(h_node);
        return NULL;
    }

    if (info->type != ENUM_NODE_MESSAGE_MANAGER) {
    ((PacketPin*)h_node->inPktPin->pDerivedObj)->InitPacketPin(h_node->inPktPin, 0);
    ((PacketPin*)h_node->outPktPin->pDerivedObj)->InitPacketPin(h_node->outPktPin, 0);
    }

    INFO("[%s] inPkt infoQ rp:%x , wp:%x\n", __FUNCTION__, (UINT32)((Pin*)h_node->inPktPin)->GetReadPtr(((Pin*)h_node->inPktPin), 0), (UINT32)((Pin*)h_node->inPktPin)->GetWritePtr(((Pin*)h_node->inPktPin)));
    INFO("[%s] inPkt dataQ rp:%x , wp:%x\n", __FUNCTION__, (UINT32)((PacketPin*)h_node->inPktPin->pDerivedObj)->pDataPinObj->GetReadPtr(((PacketPin*)h_node->inPktPin->pDerivedObj)->pDataPinObj, 0), (UINT32)((PacketPin*)h_node->inPktPin->pDerivedObj)->pDataPinObj->GetWritePtr(((PacketPin*)h_node->inPktPin->pDerivedObj)->pDataPinObj));
    INFO("[%s] outPkt infoQ rp:%x , wp:%x\n", __FUNCTION__, (UINT32)((Pin*)h_node->outPktPin)->GetReadPtr(((Pin*)h_node->outPktPin), 0), (UINT32)((Pin*)h_node->outPktPin)->GetWritePtr(((Pin*)h_node->outPktPin)));
    INFO("[%s] outPkt dataQ rp:%x , wp:%x\n", __FUNCTION__, (UINT32)((PacketPin*)h_node->outPktPin->pDerivedObj)->pDataPinObj->GetReadPtr(((PacketPin*)h_node->outPktPin->pDerivedObj)->pDataPinObj, 0), (UINT32)((PacketPin*)h_node->outPktPin->pDerivedObj)->pDataPinObj->GetWritePtr(((PacketPin*)h_node->outPktPin->pDerivedObj)->pDataPinObj));
    INFO("[AUDL]CreateNode switch done=========\n");
    return h_node;
}

long RegisterNodeAndGetNodeID(node_handle h_node, struct list_head *head)
{
    long id = 0;
    node_handle tmp_node;

    /* Check for un-registered node ID */
    list_for_each_entry(tmp_node, head, list) {
        if(tmp_node->nodeID >= id)
            id = (tmp_node->nodeID+1);
    }

    h_node->nodeID = id;
    list_add_tail(&h_node->list, head);
    return id;
}

long UnRegisterNode(node_handle h_node, struct list_head *head)
{
    list_del_init(&h_node->list);
    return 0;
}

long GetPacketPinBufHeader(Pin *pPin)
{
    UINT32 pAddr;
    RINGBUFFER_HEADER *vAddr;
    pPin->GetBufHeader(pPin, (void**)&vAddr, &pAddr);
    return pAddr;
}

node_handle GetNodeByNodeID(struct list_head *head, long nodeID)
{
    node_handle h_node;
    list_for_each_entry(h_node, head, list) {
        if(h_node->nodeID == nodeID) return h_node;
    }
    return NULL;
}
