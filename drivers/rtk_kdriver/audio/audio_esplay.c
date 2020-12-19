/******************************************************************************
 *
 *   Copyright(c) 2014 Realtek Semiconductor Corp. All rights reserved.
 *
 *   @author yungjui.lee@realtek.com
 *
 *****************************************************************************/

#include "audio_base.h"
#include <linux/slab.h>

UINT32 SetBSHeaderPhy(Base* pBaseObj, UINT32 phyAddr)
{
    ESPlay* pESPlayObj = (ESPlay*)pBaseObj->pDerivedObj;
    pESPlayObj->BSRing_phy = phyAddr;
    return S_OK;
}

UINT32 GetBSHeaderPhy(Base* pBaseObj)
{
    ESPlay* pESPlayObj = (ESPlay*)pBaseObj->pDerivedObj;
    return pESPlayObj->BSRing_phy;
}

UINT32 SetBSWptr(Base* pBaseObj, UINT32 wptr)
{
    ESPlay* pESPlayObj = (ESPlay*)pBaseObj->pDerivedObj;
    pESPlayObj->BSWptr = wptr;
    return S_OK;
}

UINT32 GetBSWptr(Base* pBaseObj)
{
    ESPlay* pESPlayObj = (ESPlay*)pBaseObj->pDerivedObj;
    return pESPlayObj->BSWptr;
}

UINT32 SetPCMFormat(Base* pBaseObj, int fs, int nch, int bitPerSample)
{
    ESPlay* pESPlayObj = (ESPlay*)pBaseObj->pDerivedObj;
    pESPlayObj->samplingFreq  = fs;
    pESPlayObj->numbOfChannel = nch;
    pESPlayObj->bitPerSample  = bitPerSample;
    return S_OK;
}

UINT32 GetPCMFormat(Base* pBaseObj, int *fs, int *nch, int *bitPerSample)
{
    ESPlay* pESPlayObj = (ESPlay*)pBaseObj->pDerivedObj;
    if(fs == NULL || nch == NULL || bitPerSample == NULL)
        return S_FALSE;
    *fs = pESPlayObj->samplingFreq;
    *nch = pESPlayObj->numbOfChannel;
    *bitPerSample = pESPlayObj->bitPerSample;
    return S_OK;
}

void delete_ESPlay(Base* pBaseObj)
{
    ESPlay* pESPlayObj = NULL;
    if(pBaseObj == NULL) return;
    pESPlayObj = (ESPlay*)pBaseObj->pDerivedObj;

    kfree(pESPlayObj);
    return delete_base(pBaseObj);
}

Base* new_ESPlay()
{
    /* object init */
    char name[] = "ESPlay";
    ESPlay* pESPlayObj = NULL;
    Base* pObj = new_base();
    if(pObj == NULL)
    {
        return NULL;
    }
    pESPlayObj = (ESPlay*)kmalloc(sizeof(ESPlay), GFP_KERNEL);
    if(pESPlayObj == NULL)
    {
        pObj->Delete(pObj);
        return NULL;
    }
    memset(pESPlayObj, 0, sizeof(ESPlay));
    memcpy(pObj->name, name, sizeof(name));
    pObj->pDerivedObj = pESPlayObj;

    /* setup member functions */
    pObj->Delete               = delete_ESPlay;
    pESPlayObj->SetBSHeaderPhy = SetBSHeaderPhy;
    pESPlayObj->GetBSHeaderPhy = GetBSHeaderPhy;
    pESPlayObj->SetBSWptr      = SetBSWptr;
    pESPlayObj->GetBSWptr      = GetBSWptr;
    pESPlayObj->SetPCMFormat   = SetPCMFormat;
    pESPlayObj->GetPCMFormat   = GetPCMFormat;

    /* agent init */
    pObj->instanceID = UNDEFINED_AGENT_ID;  // no agent
    pObj->inPinID = UNINIT_PINID;           // no in pin

    pObj->outPin = NULL;
    pObj->outPinID = BASE_BS_OUT;

    return pObj;
}
