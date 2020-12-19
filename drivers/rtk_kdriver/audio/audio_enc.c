/******************************************************************************
 *
 *   Copyright(c) 2014 Realtek Semiconductor Corp. All rights reserved.
 *
 *   @author yungjui.lee@realtek.com
 *
 *****************************************************************************/

#include "audio_base.h"
#include <audio_rpc.h>
#include <linux/slab.h>

#define AUDIO_ENC_MSG_BUF_SIZE      (8*1024)
static UINT32 InitOutPinRing(Base* pBaseObj)
{
    ENC* pENCObj = (ENC*)pBaseObj->pDerivedObj;
    Pin* pStreamPin = pBaseObj->GetOutPin(pBaseObj);
    UINT32 pAddr, MsgBufferSize;
    UINT32 res;
    AUDIO_RPC_RINGBUFFER_HEADER header;

    /* stream buffer */
    pBaseObj->InitOutRingBuf(pBaseObj);
    pStreamPin->GetBuffer(pStreamPin, &pENCObj->m_pRingLower, &pENCObj->m_BufferSize);
    pENCObj->m_pRingUpper = pENCObj->m_pRingLower + pENCObj->m_BufferSize;
    pENCObj->m_RingBufferWrPtr = pENCObj->m_pRingLower;

    /* message buffer */
    pENCObj->pMsgPin->GetBufHeader(pENCObj->pMsgPin, (void**)&pENCObj->m_pMsgRingBufferHeader, &pAddr);
    pENCObj->pMsgPin->GetBuffer(pENCObj->pMsgPin, &pENCObj->m_pMsgRingLower, &MsgBufferSize);
    pENCObj->m_pMsgRingUpper = pENCObj->m_pMsgRingLower + MsgBufferSize;
    IPC_WriteU32((BYTE*)&(pENCObj->m_pMsgRingBufferHeader->bufferID), RINGBUFFER_MESSAGE);

    header.instanceID = pBaseObj->GetAgentID(pBaseObj);
    header.pinID      = MESSAGE_QUEUE;
    header.readIdx    = 0;
    header.listSize   = 1;
    header.pRingBufferHeaderList[0] = (unsigned long)pAddr;
    res = RTKAUDIO_RPC_TOAGENT_INITRINGBUFFER_HEADER_SVC(&header);

    if(res != S_OK) AUDIO_ERROR("[%s:%d] RPC return != S_OK\n",__FUNCTION__,__LINE__);

    return res;
}

SINT64 DeliverAudioFrame(Base* pBaseObj)
{
    ENC* pENCObj = (ENC*)pBaseObj->pDerivedObj;
    Pin* pMsgPin = pENCObj->pMsgPin;
    AUDIO_RPC_ENC_ELEM_GENERAL_INFO audioGen;
    AUDIO_RPC_ENC_ELEM_FRAME_INFO audioFrame;
    UINT32 readSize;
    ENUM_DVD_AUDIO_ENCODER_OUTPUT_INFO_TYPE infoType;

    BYTE *pNewWritePointer = NULL;

    readSize = pMsgPin->GetReadSize(pMsgPin, 0);
    pENCObj->m_MsgRingBufferRdPtr = pMsgPin->GetReadPtr(pMsgPin, 0);
    while(readSize != 0)
    {
        infoType = (ENUM_DVD_AUDIO_ENCODER_OUTPUT_INFO_TYPE)IPC_ReadU32(pENCObj->m_MsgRingBufferRdPtr);
        if (infoType == AUDIOENCODER_AudioGEN)
        {
            if (pENCObj->m_sendGenInfoFlag == true)
            {
                Base* pNextObj;
                AUDIO_RPC_ENC_ELEM_GENERAL_INFO *pAGen;
                audioGen.infoType = infoType;
                audioGen.audioEncoderType = (enum AUDIO_MODULE_TYPE)IPC_ReadU32(pENCObj->m_MsgRingBufferRdPtr+
                                             sizeof(ENUM_DVD_AUDIO_ENCODER_OUTPUT_INFO_TYPE));
                audioGen.bitRate = IPC_ReadU32(pENCObj->m_MsgRingBufferRdPtr + 8);
                audioGen.samplingRate = IPC_ReadU32(pENCObj->m_MsgRingBufferRdPtr + 12);
                audioGen.mode = (enum ENCODE_MODE)IPC_ReadU32(pENCObj->m_MsgRingBufferRdPtr + 16);

                pAGen = (AUDIO_RPC_ENC_ELEM_GENERAL_INFO *)&audioGen;

                if (pENCObj->m_EncoderType == AUDIO_AC3_ENCODER)
                    pAGen->audioEncoderType = (enum AUDIO_MODULE_TYPE)RHALTYPE_DOLBY_AC3;
                else if (pENCObj->m_EncoderType == AUDIO_MPEG_ENCODER)
                    pAGen->audioEncoderType = (enum AUDIO_MODULE_TYPE)RHALTYPE_MPEG1ES;
                else if (pENCObj->m_EncoderType == AUDIO_MP3_ENCODER)
                    pAGen->audioEncoderType = (enum AUDIO_MODULE_TYPE)RHALTYPE_MP3;
                else if(pENCObj->m_EncoderType == AUDIO_LPCM_ENCODER)
                    pAGen->audioEncoderType = (enum AUDIO_MODULE_TYPE)RHALTYPE_PCM_LITTLE_ENDIAN;
                else if(pENCObj->m_EncoderType == AUDIO_AAC_ENCODER)
                    pAGen->audioEncoderType = (enum AUDIO_MODULE_TYPE)RHALTYPE_AAC;
                else
                    pAGen->audioEncoderType = (enum AUDIO_MODULE_TYPE)RHALTYPE_NONE;

                DEBUG("[AE] got GenInfo\n");
                DEBUG("         GenInfo->infoType = %d\n", pAGen->infoType);
                DEBUG("         GenInfo->audioEncoderType = %d\n", pAGen->audioEncoderType);
                DEBUG("         GenInfo->bitRate = %ld\n",pAGen->bitRate);
                DEBUG("         GenInfo->samplingRate = %ld\n", pAGen->samplingRate);
                DEBUG("         GenInfo->mode = %d\n", pAGen->mode);

                // only deliver privateInfo
                if(list_empty(&pBaseObj->flowList))
                {
                    ERROR("[%s] Out Pin is not connect\n",pBaseObj->name);
                    break;
                }
                pNextObj = container_of(pBaseObj->flowList.next, Base, flowList);
                if(pNextObj->PrivateInfo(pNextObj, (UINT32)pAGen->infoType, (UINT8*)pAGen,
                            sizeof(AUDIO_RPC_ENC_ELEM_FRAME_INFO)) != S_OK)
                {
                    ERROR("[%s] The Muxer input audio queue is full!!!\n",pBaseObj->name);
                    break;
                }
            }
            pENCObj->m_startEncodeFlag = true;
        }
        else if (infoType == AUDIOENCODER_AudioFrameInfo)
        {
            AUDIO_RPC_ENC_ELEM_FRAME_INFO *pInfo;
            Base* pNextObj;
            BYTE *pWritePointer;
            DELIVERINFO deliverInfo;
            audioFrame.infoType      = infoType;
            audioFrame.frameNumber   = IPC_ReadU32(pENCObj->m_MsgRingBufferRdPtr+ sizeof(ENUM_DVD_AUDIO_ENCODER_OUTPUT_INFO_TYPE));
            audioFrame.PTSH          = IPC_ReadU32(pENCObj->m_MsgRingBufferRdPtr+ 8);
            audioFrame.PTSL          = IPC_ReadU32(pENCObj->m_MsgRingBufferRdPtr+ 12);
            audioFrame.frameSize     = IPC_ReadU32(pENCObj->m_MsgRingBufferRdPtr+ 16);
            audioFrame.NumberOfFrame = IPC_ReadU32(pENCObj->m_MsgRingBufferRdPtr+ 20);

            pInfo = (AUDIO_RPC_ENC_ELEM_FRAME_INFO *)&audioFrame;

            // 1. Check if receive Frame Info after EOS
            if (!pENCObj->m_startEncodeFlag)
            {
                DEBUG("[%s] Receive Audio Frame Info after EOS!!!!\n",pBaseObj->name);

                //Update write pointer even in this case.
                pENCObj->m_RingBufferWrPtr += pInfo->frameSize;
                if (pENCObj->m_RingBufferWrPtr >= pENCObj->m_pRingUpper)
                    pENCObj->m_RingBufferWrPtr -= pENCObj->m_BufferSize;

                // maybe get the next item from msg queue.
                readSize -= 128;
                pENCObj->m_MsgRingBufferRdPtr += 128;
                if (pENCObj->m_MsgRingBufferRdPtr >= pENCObj->m_pMsgRingUpper)
                    pENCObj->m_MsgRingBufferRdPtr = pENCObj->m_pMsgRingLower;

                pMsgPin->SetReadPtr(pMsgPin, pENCObj->m_MsgRingBufferRdPtr, 0);
                continue;
            }

            if (pInfo->frameNumber != pENCObj->m_frameNumber) {
                DEBUG("[AE] pictureNumber not continuous!!!!! correct: %d, now: %d, offset: %d\n",
                                                                           pENCObj->m_frameNumber,
                                                                           pInfo->frameNumber,
                                                                           (readSize-128)/128);
                DEBUG("[AE] got FrameInfo\n");
                DEBUG("      infoType: %d\n", pInfo->infoType);
                DEBUG("      frameNumber: %d\n", pInfo->frameNumber);
                DEBUG("      PTSH: %lu\n", pInfo->PTSH);
                DEBUG("      PTSL: %lu\n", pInfo->PTSL);
                DEBUG("      frameSize: %ld\n", pInfo->frameSize);
                DEBUG("      NumberOfFrame: %ld\n", pInfo->NumberOfFrame);
                pENCObj->m_frameNumber = pInfo->frameNumber;
            }

#ifdef PRINT_DEBUG
            if ((pENCObj->m_frameNumber % 150) == 1) {
                int64_t aPTS;

                aPTS = pli_getPTS();

                DEBUG("[AE] %d(%d) (PTS: %lu(%lld)), off: %d, r: 0x%x(%d), w: 0x%x(%d)\n"
                        ,pInfo->frameNumber
                        ,pENCObj->m_frameNumber
                        ,pInfo->PTSL
                        ,aPTS
                        ,(readSize-128)/128
                        ,(unsigned int)pMsgPin->GetReadPtr(pMsgPin, 0)
                        ,pMsgPin->GetReadSize(pMsgPin, 0)
                        ,(unsigned int)pMsgPin->GetWritePtr(pMsgPin)
                        ,pMsgPin->GetWriteSize(pMsgPin));
            }
#endif
            pENCObj->m_audioPTS = (((int64_t)pInfo->PTSH) << 32) | pInfo->PTSL;

            // 2. Deliver FrameInfo
            if(list_empty(&pBaseObj->flowList))
            {
                ERROR("[%s] Out Pin is not connect\n",pBaseObj->name);
                break;
            }
            pNextObj = container_of(pBaseObj->flowList.next, Base, flowList);
            if(pNextObj->PrivateInfo(pNextObj, (UINT32)pInfo->infoType, (UINT8*)pInfo,
                        sizeof(AUDIO_RPC_ENC_ELEM_FRAME_INFO)) != S_OK)
            {
                ERROR("[%s] The Muxer input audio queue is full!!!\n",pBaseObj->name);
                break;
            }

            // 3. Deliver Frame
            pWritePointer = pENCObj->m_RingBufferWrPtr;

            // New WritePointer decision
            pNewWritePointer = pWritePointer + pInfo->frameSize;
            if (pNewWritePointer >= pENCObj->m_pRingUpper)
                pNewWritePointer = pNewWritePointer - pENCObj->m_BufferSize;

            // call deliver
            deliverInfo.pWritePointer = pWritePointer;
            deliverInfo.writeBufferSize = pInfo->frameSize;
            pNextObj->PrivateInfo(pNextObj, INFO_DELIVER_INFO, (UINT8*)&deliverInfo, sizeof(DELIVERINFO));

            pENCObj->m_RingBufferWrPtr = pNewWritePointer;
            pENCObj->m_frameNumber++;
        }

        // Get next element from queue !
        readSize -= 128;
        pENCObj->m_MsgRingBufferRdPtr += 128;
        if (pENCObj->m_MsgRingBufferRdPtr >= pENCObj->m_pMsgRingUpper)
            pENCObj->m_MsgRingBufferRdPtr = pENCObj->m_pMsgRingLower;

        pMsgPin->SetReadPtr(pMsgPin, pENCObj->m_MsgRingBufferRdPtr, 0);
    } /* end of while */

    return pENCObj->m_audioPTS;
}

UINT32 StartEncode(Base* pBaseObj)
{
    ENC* pENCObj = (ENC*)pBaseObj->pDerivedObj;
    Pin* pStreamPin = pBaseObj->GetOutPin(pBaseObj);
    Pin* pMsgPin = pENCObj->pMsgPin;
    UINT32 res;
    AUDIO_RPC_SEND_LONG mLong;
    BYTE *pWritePointer;
    /*long  writeBufferSize;*/

    DEBUG("[%s][%s]\n",pBaseObj->name,__FUNCTION__);
    //Reset write pointers
    pWritePointer = pStreamPin->GetWritePtr(pStreamPin);
    if (pENCObj->m_RingBufferWrPtr != pWritePointer) {
        BYTE *bufferRdPtr;
        DEBUG("[AE] stream buffer wrong for the previous record session streams. system:0x%x audio:0x%x\n",
               (int)pENCObj->m_RingBufferWrPtr, (int)pWritePointer);

        // Set WritePointer to System's WritePointer.
        // Since when emergency Pause/Resume happens, we can't set buffer to empty...
        bufferRdPtr = pStreamPin->GetReadPtr(pStreamPin, 0);
        if (bufferRdPtr != pENCObj->m_RingBufferWrPtr) {
            ERROR("WARNING: AE's write ptr 0x%x != Muxer's read ptr 0x%x\n",
                                                          (int)pENCObj->m_RingBufferWrPtr, (int)bufferRdPtr);
        }
        // reset ReadPointer(used by Muxer) & WritePointer(used by System side's AE)
        // since firmware's AE won't update its write pointer during StartEncode
        pStreamPin->SetReadPtr(pStreamPin, pWritePointer, 0);
        pENCObj->m_RingBufferWrPtr = pWritePointer;
        DEBUG("[AE] reset stream buffer's read pointer to: 0x%x\n", (int)pWritePointer);
    }


    //Reset Msg Ring
    pWritePointer = pMsgPin->GetWritePtr(pMsgPin);
    if (pENCObj->m_MsgRingBufferRdPtr != pWritePointer) {
        ERROR("[AE] MSG buffer wrong for the previous record session streams. system:0x%x audio:0x%x\n",
               (int)pENCObj->m_MsgRingBufferRdPtr, (int)pWritePointer);

        pMsgPin->SetReadPtr(pMsgPin, pWritePointer, 0);
    }

    pENCObj->m_sendGenInfoFlag = true;

    mLong.instanceID = (long)pBaseObj->GetAgentID(pBaseObj);
    mLong.data = 0;
    res = RTKAUDIO_RPC_ENC_TOAGENT_STARTENCODER_SVC(&mLong);

    if(res != S_OK) AUDIO_ERROR("[%s:%d] RPC return != S_OK\n",__FUNCTION__,__LINE__);
    else DEBUG("[%s][%s] RPC return OK\n",pBaseObj->name,__FUNCTION__);

    pENCObj->m_EncodeState = Enc_State_Running;
    pENCObj->m_audioPTS = 0;
    return S_OK;
}

UINT32 StopEncode(Base* pBaseObj)
{
    ENC* pENCObj = (ENC*)pBaseObj->pDerivedObj;
    UINT32 res;
    AUDIO_RPC_SEND_LONG mLong;

    if (pENCObj->m_EncodeState == Enc_State_Paused) {
        DEBUG("[%s] Already in Paused mode. call EndOfStream()\n",pBaseObj->name);
        // TODO(ray): add this if necessary
        //EndOfStream(0, 0);
        pENCObj->m_EncodeState = Enc_State_Stopped;

        pENCObj->m_frameNumber = 1;
        pENCObj->m_startEncodeFlag = false;
        return S_OK;
    }

    if(pENCObj->m_EncodeState == Enc_State_Stopped)
        return S_OK;

    mLong.instanceID = (long)pBaseObj->GetAgentID(pBaseObj);
    mLong.data = 0;
    res = RTKAUDIO_RPC_ENC_TOAGENT_STOPENCODER_SVC(&mLong);
    if(res != S_OK) AUDIO_ERROR("[%s:%d] RPC return != S_OK\n",__FUNCTION__,__LINE__);

    pENCObj->m_EncodeState = Enc_State_Stopped;

    return S_OK;
}

UINT32 EncoderConfig(Base* pBaseObj, void* data)
{
    AUDIO_ENC_CFG* ecfg = (AUDIO_ENC_CFG*)data;
    UINT32 res;
    AUDIO_RPC_ENC_COMMAND pCommand;

    pCommand.instanceID = pBaseObj->GetAgentID(pBaseObj);
    pCommand.enc_config = *ecfg;
    res = RTKAUDIO_RPC_ENC_TOAGENT_COMMAND_SVC(&pCommand);
    if(res != S_OK) AUDIO_ERROR("[%s:%d] RPC return != S_OK\n",__FUNCTION__,__LINE__);

    return S_OK;
}

UINT32 ENC_SetRefClock(Base* pBaseObj, UINT32 refClockPhyAddr)
{
    return S_OK;
}

UINT32 SetEncodeType(Base* pBaseObj, AUDIO_MODULE_TYPE type)
{
    AUDIO_RPC_ENC_INFO info;
    UINT32 res;

    info.instanceID = pBaseObj->GetAgentID(pBaseObj);
    info.encode_type = type;
    res = RTKAUDIO_RPC_ENC_SETENCODER_SVC(&info);
    if(res != S_OK) AUDIO_ERROR("[%s:%d] RPC return != S_OK\n",__FUNCTION__,__LINE__);

    return S_OK;
}

void delete_ENC(Base* pBaseObj)
{
    ENC* pENCObj = NULL;
    if(pBaseObj == NULL) return;
    pENCObj = (ENC*)pBaseObj->pDerivedObj;

    del_id_from_map(pBaseObj->GetAgentID(pBaseObj));

    kfree(pENCObj);
    return delete_base(pBaseObj);
}

Base* new_ENC(AUDIO_MODULE_TYPE type)
{
    /* object init */
    char name[] = "ENC";
    ENC* pENCObj = NULL;
    Base* pObj = new_base();
    if(pObj == NULL)
    {
        return NULL;
    }
    pENCObj = (ENC*)kmalloc(sizeof(ENC), GFP_KERNEL);
    if(pENCObj == NULL)
    {
        pObj->Delete(pObj);
        return NULL;
    }
    memcpy(pObj->name, name, sizeof(name));
    pObj->pDerivedObj = pENCObj;

    DEBUG("[%s] \n", __FUNCTION__);

    /* setup member functions */
    pObj->Delete               = delete_ENC;
    pObj->SetRefClock          = ENC_SetRefClock;
    pENCObj->DeliverAudioFrame = DeliverAudioFrame;
    pENCObj->StartEncode       = StartEncode;
    pENCObj->StopEncode        = StopEncode;
    pENCObj->EncoderConfig     = EncoderConfig;
    pENCObj->SetEncodeType     = SetEncodeType;

    /* agent init */
    pObj->instanceID = CreateAgent(AUDIO_ENCODER);
    if(pObj->instanceID == UNDEFINED_AGENT_ID)
    {
        pObj->Delete(pObj);
        return NULL;
    }
    DEBUG("[%s] CreateAgent: agentID: %d \n", pObj->name, pObj->instanceID);

    pObj->inPinID = PCM_IN_RTK;
    pObj->outPin = new_pin(AUDIO_ENC_OUTPUT_BUF_SIZE);
    pObj->outPinID = BASE_BS_OUT;

    pENCObj->pMsgPin = new_pin(AUDIO_ENC_MSG_BUF_SIZE);
    if(pENCObj->pMsgPin == NULL)
    {
        kfree(pENCObj);
        pObj->Delete(pObj);
        return NULL;
    }
    pENCObj->m_MsgRingBufferRdPtr = pENCObj->pMsgPin->GetWritePtr(pENCObj->pMsgPin);
    pENCObj->m_startEncodeFlag = FALSE;
    pENCObj->m_sendGenInfoFlag = FALSE;
    pENCObj->m_frameNumber = 1;
    pENCObj->m_audioPTS = 0;
    pENCObj->m_RingBufferWrPtr = NULL;
    pENCObj->m_pMsgRingBufferHeader = NULL;
    pENCObj->m_EncodeState = Enc_State_Stopped;

    InitOutPinRing(pObj);

    //pENCObj->SetEncodeType(pObj, type);

    add_id_to_map(pObj, pObj->GetAgentID(pObj));
    return pObj;
}
