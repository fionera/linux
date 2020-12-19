/******************************************************************************
 *
 *   Copyright(c) 2014 Realtek Semiconductor Corp. All rights reserved.
 *
 *   @author yungjui.lee@realtek.com
 *
 *****************************************************************************/

#include "audio_base.h"
#include <linux/slab.h>

typedef struct {
    UINT32 bufBase;       // ring buffer base address
    UINT32 bufLimit;      // ring buffer end address
    SINT32 bufSize;       // ring buffer size in byte
    UINT8* pCurrReadPtr;
    SINT32 readSize;
    SINT32 readFrame;     // how many frames can be read
    UINT8* pCurrWritePtr;
    SINT32 writeSize;
    UINT8* pReserved;     // It is used in the case of ring buf wrap-around.
    SINT32 reservedSize;
    SINT64 firstPTSinBuf;
    SINT64 lastPTSinBuf;
    UINT32 readBufferingSize;
    UINT32 readBufferingFrameNum;
} RING_BUFFER_INFO;

UINT32 receive(Base* pBaseObj, UINT8* pWritePointer, SINT32 WriteBufferSize)
{
    MemOut* pMemOutObj = (MemOut*)pBaseObj->pDerivedObj;
    RING_BUFFER_INFO* m_ringBufInfo = (RING_BUFFER_INFO*)pMemOutObj->m_ringBufInfo;

    if(m_ringBufInfo->bufBase == 0)
    {
        /*ERROR("[%s][Error] MemOut is not Connected\n",pBaseObj->name);*/
        return S_OK;
    }

    if(!((UINT32)pWritePointer >= m_ringBufInfo->bufBase && (UINT32)pWritePointer < m_ringBufInfo->bufLimit))
    {
        ERROR("[%s][Error]pointer is out of ring buffer boundary\n",pBaseObj->name);
        return S_OK;
    }
    if(m_ringBufInfo->pCurrReadPtr == 0)
    {
        /*assert(m_ringBufInfo->readSize == 0);*/
        if(WriteBufferSize <= 0)
        {
            ERROR("[%s][ERROR]WriteBufferSize[%d]\n",pBaseObj->name,WriteBufferSize);
            return S_OK;
        }
        m_ringBufInfo->pCurrReadPtr = pWritePointer;
        m_ringBufInfo->readSize = WriteBufferSize;
    }
    else
    {
#if 0
        assert(m_ringBufInfo->readSize > 0);

        if(((UINT32)m_ringBufInfo->pCurrReadPtr + m_ringBufInfo->readSize) < m_ringBufInfo->bufLimit)
            assert((m_ringBufInfo->pCurrReadPtr + m_ringBufInfo->readSize) == pWritePointer);
        else
            assert((m_ringBufInfo->pCurrReadPtr + m_ringBufInfo->readSize - m_ringBufInfo->bufSize) == pWritePointer);
#endif

        m_ringBufInfo->readSize += WriteBufferSize;
    }

    m_ringBufInfo->readFrame++;
    pMemOutObj->m_frameSize = WriteBufferSize;
    DEBUG("[%s:%d] readFrame %d\n",__FUNCTION__,__LINE__,m_ringBufInfo->readFrame);
    return S_OK;
}
UINT32 ReadFrame(Base* pBaseObj, UINT8** pData, UINT32* pSize, UINT64* pPts, UINT32* frameNum)
{
    MemOut* pMemOutObj = (MemOut*)pBaseObj->pDerivedObj;
    RING_BUFFER_INFO* m_ringBufInfo = (RING_BUFFER_INFO*)pMemOutObj->m_ringBufInfo;
    UINT32 readPtr;

    if(m_ringBufInfo->bufBase == 0)
    {
        /*ERROR("[%s][Error] MemOut is not Connected\n",pBaseObj->name);*/
        *frameNum = 0;
        return S_OK;
    }

    readPtr = (UINT32)(m_ringBufInfo->pCurrReadPtr) + m_ringBufInfo->readBufferingSize;
    if(readPtr >= m_ringBufInfo->bufLimit)
        readPtr -= m_ringBufInfo->bufSize;

    *pData    = (UINT8*)readPtr;
    *pSize    = m_ringBufInfo->readSize - m_ringBufInfo->readBufferingSize;
    *pPts     = m_ringBufInfo->firstPTSinBuf;
    DEBUG("[%s:%d] readFrame %d readBufferingFrameNum %d\n",__FUNCTION__,__LINE__,m_ringBufInfo->readFrame,m_ringBufInfo->readBufferingFrameNum);
    *frameNum = m_ringBufInfo->readFrame - m_ringBufInfo->readBufferingFrameNum;

    m_ringBufInfo->firstPTSinBuf = -1;
    m_ringBufInfo->lastPTSinBuf = -1;
    m_ringBufInfo->readBufferingSize     += *pSize;
    m_ringBufInfo->readBufferingFrameNum += *frameNum;
    return S_OK;
}
UINT32 GetRingBuffer(Base* pBaseObj, UINT32* base, UINT32* size)
{
    MemOut* pMemOutObj = (MemOut*)pBaseObj->pDerivedObj;
    RING_BUFFER_INFO* m_ringBufInfo = (RING_BUFFER_INFO*)pMemOutObj->m_ringBufInfo;

    *base = m_ringBufInfo->bufBase;
    *size = m_ringBufInfo->bufSize;
    return S_OK;
}
UINT32 ReleaseData(Base* pBaseObj, UINT32 dataSize)
{
    MemOut* pMemOutObj = (MemOut*)pBaseObj->pDerivedObj;
    RING_BUFFER_INFO* m_ringBufInfo = (RING_BUFFER_INFO*)pMemOutObj->m_ringBufInfo;
    UINT32 pNewReadPtr;

    if(dataSize < (UINT32)m_ringBufInfo->readSize)
    {
        /* release partial data */
        pNewReadPtr = (UINT32)(m_ringBufInfo->pCurrReadPtr) + dataSize;

        m_ringBufInfo->pCurrReadPtr  = (BYTE*)pNewReadPtr;
        m_ringBufInfo->readSize     -= dataSize;
        m_ringBufInfo->readFrame    -= (dataSize/pMemOutObj->m_frameSize);
        m_ringBufInfo->readBufferingSize     -= dataSize;
        m_ringBufInfo->readBufferingFrameNum -= (dataSize/pMemOutObj->m_frameSize);
    }
    else
    {
        /* release all data */
        pNewReadPtr = (UINT32)(m_ringBufInfo->pCurrReadPtr) + m_ringBufInfo->readSize;

        m_ringBufInfo->pCurrReadPtr = 0;
        m_ringBufInfo->readSize = 0;
        m_ringBufInfo->readFrame = 0;
        m_ringBufInfo->readBufferingSize = 0;
        m_ringBufInfo->readBufferingFrameNum = 0;
    }
    /* update rp */
    if(pNewReadPtr >= m_ringBufInfo->bufLimit)
        pNewReadPtr -= m_ringBufInfo->bufSize;
    DEBUG("[%s:%d] rp %x\n",__FUNCTION__,__LINE__,pNewReadPtr);
    pBaseObj->inPin->SetReadPtr(pBaseObj->inPin, (BYTE*)pNewReadPtr, 0);

    return S_OK;
}
UINT32 MemOut_PrivateInfo(Base* pBaseObj, UINT32 infoId, UINT8* pData, UINT32 length)
{
    MemOut* pMemOutObj = (MemOut*)pBaseObj->pDerivedObj;
    RING_BUFFER_INFO* m_ringBufInfo = (RING_BUFFER_INFO*)pMemOutObj->m_ringBufInfo;

    if(infoId == INFO_DELIVER_INFO && length == sizeof(DELIVERINFO))
    {
        DELIVERINFO* pDInfo = (DELIVERINFO*)pData;
        DEBUG("[%s][%s] DeliverInfo\n", pBaseObj->name, __FUNCTION__);
        receive(pBaseObj, pDInfo->pWritePointer, pDInfo->writeBufferSize);
    }
    else if(infoId == AUDIOENCODER_AudioFrameInfo && length == sizeof(AUDIO_RPC_ENC_ELEM_FRAME_INFO))
    {
        //DEBUG("[%s][%s] AudioFrameInfo\n", pBaseObj->name, __FUNCTION__);
        AUDIO_RPC_ENC_ELEM_FRAME_INFO* pAInfo = (AUDIO_RPC_ENC_ELEM_FRAME_INFO*)pData;
        m_ringBufInfo->lastPTSinBuf = ((SINT64)(pAInfo->PTSH) << 32) | (pAInfo->PTSL);
        if(m_ringBufInfo->firstPTSinBuf == -1)
            m_ringBufInfo->firstPTSinBuf = m_ringBufInfo->lastPTSinBuf;
        /*DEBUG("got FrameInfo\n");*/
        /*DEBUG("      infoType: %d\n", pAInfo->infoType);*/
        /*DEBUG("      frameNumber: %d\n", pAInfo->frameNumber);*/
        /*DEBUG("      PTSH: %lu\n", pAInfo->PTSH);*/
        /*DEBUG("      PTSL: %lu\n", pAInfo->PTSL);*/
        /*DEBUG("      firstPTSinBuf: %lld\n", m_ringBufInfo->firstPTSinBuf);*/
        /*DEBUG("      lastPTSinBuf: %lld\n", m_ringBufInfo->lastPTSinBuf);*/
        /*DEBUG("      frameSize: %ld\n", pAInfo->frameSize);*/
        /*DEBUG("      NumberOfFrame: %ld\n", pAInfo->NumberOfFrame);*/
    }
    else if(infoId == AUDIOENCODER_AudioGEN && length == sizeof(AUDIO_RPC_ENC_ELEM_GENERAL_INFO))
    {
        /*AUDIO_RPC_ENC_ELEM_GENERAL_INFO* pAGen = (AUDIO_RPC_ENC_ELEM_GENERAL_INFO*)pData;*/
        DEBUG("[%s][%s] AudioGEN\n", pBaseObj->name, __FUNCTION__);
        //DEBUG("got GenInfo\n");
        //DEBUG("         GenInfo->infoType = %d\n", pAGen->infoType);
        //DEBUG("         GenInfo->audioEncoderType = %d\n", pAGen->audioEncoderType);
        //DEBUG("         GenInfo->bitRate = %ld\n",pAGen->bitRate);
        //DEBUG("         GenInfo->samplingRate = %ld\n", pAGen->samplingRate);
        //DEBUG("         GenInfo->mode = %d\n", pAGen->mode);
    }
    return S_OK;
}
UINT32 MemOut_Connect(Base* pForeBaseObj, Base* pBaseObj)
{
    UINT8* pBase = 0;
    HRESULT hr = S_OK;
    MemOut* pMemOutObj = (MemOut*)pBaseObj->pDerivedObj;
    RING_BUFFER_INFO* m_ringBufInfo = (RING_BUFFER_INFO*)pMemOutObj->m_ringBufInfo;

    if(pForeBaseObj->GetOutPin(pForeBaseObj) == NULL) return S_FALSE;

    pBaseObj->inPin = pForeBaseObj->GetOutPin(pForeBaseObj);

    //update ring buffer info
    m_ringBufInfo->bufSize = 0;
    pBaseObj->inPin->GetBuffer(pBaseObj->inPin, &pBase, (UINT32*)&m_ringBufInfo->bufSize);
    m_ringBufInfo->bufBase = (UINT32)pBase;
    m_ringBufInfo->bufLimit = m_ringBufInfo->bufBase + m_ringBufInfo->bufSize;
    m_ringBufInfo->pCurrReadPtr = 0;
    m_ringBufInfo->readSize = 0;
    m_ringBufInfo->readFrame = 0;
    m_ringBufInfo->pReserved = 0;
    m_ringBufInfo->reservedSize = 0;
    m_ringBufInfo->firstPTSinBuf = -1;
    m_ringBufInfo->lastPTSinBuf = -1;
    m_ringBufInfo->readBufferingSize = 0;
    m_ringBufInfo->readBufferingFrameNum = 0;

    DEBUG("[%s]m_ringBufInfo.bufBase :%p\n", pBaseObj->name, (void*)m_ringBufInfo->bufBase);
    DEBUG("[%s]m_ringBufInfo.bufLimit :%p\n", pBaseObj->name, (void*)m_ringBufInfo->bufLimit);

    list_add(&pBaseObj->flowList, &pForeBaseObj->flowList);
    return hr;
}



UINT32 MemOut_Remove(Base* pBaseObj)
{
    MemOut* pMemOutObj = (MemOut*)pBaseObj->pDerivedObj;
    RING_BUFFER_INFO* m_ringBufInfo = (RING_BUFFER_INFO*)pMemOutObj->m_ringBufInfo;
    UINT8* pBase = 0;

    m_ringBufInfo->bufSize = 0;
    m_ringBufInfo->bufBase = (UINT32)pBase;
    m_ringBufInfo->bufLimit = m_ringBufInfo->bufBase + m_ringBufInfo->bufSize;
    m_ringBufInfo->pCurrReadPtr = 0;
    m_ringBufInfo->readSize = 0;
    m_ringBufInfo->readFrame = 0;
    m_ringBufInfo->pReserved = 0;
    m_ringBufInfo->reservedSize = 0;
    m_ringBufInfo->firstPTSinBuf = -1;
    m_ringBufInfo->lastPTSinBuf = -1;
    m_ringBufInfo->readBufferingSize = 0;
    m_ringBufInfo->readBufferingFrameNum = 0;
    return Remove( pBaseObj);
}

void delete_MemOut(Base* pBaseObj)
{
    MemOut* pMemOutObj = NULL;
    if(pBaseObj == NULL) return;
    pMemOutObj = (MemOut*)pBaseObj->pDerivedObj;

    MemOut_Remove(pBaseObj);
    pBaseObj->Remove = NULL;

    kfree(pMemOutObj->m_ringBufInfo);
    kfree(pMemOutObj);
    return delete_base(pBaseObj);
}

Base* new_MemOut()
{
    /* object init */
    char name[] = "MEMOUT";
    MemOut* pMemOutObj = NULL;
    Base* pObj = new_base();

    if(pObj == NULL)
    {
        return NULL;
    }

    pMemOutObj = (MemOut*)kmalloc(sizeof(MemOut), GFP_KERNEL);
    if(pMemOutObj == NULL)
    {
        pObj->Delete(pObj);
        return NULL;
    }
    memcpy(pObj->name, name, sizeof(name));
    pMemOutObj->m_ringBufInfo = (RING_BUFFER_INFO*)kmalloc(sizeof(RING_BUFFER_INFO), GFP_KERNEL);
    if(pMemOutObj->m_ringBufInfo == NULL)
    {
        pObj->Delete(pObj);
        kfree(pMemOutObj);
        return NULL;
    }
    memset(pMemOutObj->m_ringBufInfo, 0, sizeof(RING_BUFFER_INFO));
    pObj->pDerivedObj = pMemOutObj;

    /* setup member functions */
    pObj->Delete              = delete_MemOut;
    pObj->PrivateInfo         = MemOut_PrivateInfo;
    pObj->Connect             = MemOut_Connect;
    pObj->Remove              = MemOut_Remove;
    pMemOutObj->GetRingBuffer = GetRingBuffer;
    pMemOutObj->ReadFrame     = ReadFrame;
    pMemOutObj->ReleaseData   = ReleaseData;

    /* agent init */
    pObj->instanceID = UNDEFINED_AGENT_ID;  // no agent
    pObj->inPinID = UNINIT_PINID;           // no in pin

    pObj->outPin = NULL;
    pObj->outPinID = UNINIT_PINID;

    return pObj;
}
