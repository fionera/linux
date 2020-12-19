/******************************************************************************
 *
 *   Copyright(c) 2014 Realtek Semiconductor Corp. All rights reserved.
 *
 *   @author yungjui.lee@realtek.com
 *
 *****************************************************************************/

#include "audio_base.h"
#include "audio_rpc.h"
#include <linux/slab.h>
#include <linux/delay.h>
#include "hal_common.h"
#include "audio_inc.h"

#define INST_MASK       0xFFFFF000

UINT32 Voice_SwitchFocus(Base* pBaseObj, void* data)
{
    UINT32 res;
    Voice* pVoiceObj = (Voice*)pBaseObj->pDerivedObj;
    AUDIO_RPC_IPT_SRC pSource;
    AUDIO_RPC_IPT_SRC* m_pIptSrc = &(pVoiceObj->m_pIptSrc);
    DEBUG("[%s][%s]\n", __FUNCTION__, pBaseObj->name);

    pSource.instanceID  = pBaseObj->GetAgentID(pBaseObj);
    pSource.focus_in[0] = AUDIO_IPT_SRC_DMIC0;
    pSource.mux_in      = 0;
    res = RTKAUDIO_RPC_TOAGENT_CHG_IPT_SRC_SVC(&pSource);

    if(res == S_OK)
    {
        m_pIptSrc->instanceID  = pBaseObj->GetAgentID(pBaseObj);
    }
    else
        ERROR("[%s:%d] RPC return != S_OK\n",__FUNCTION__,__LINE__);

    return res;
}

UINT32 Voice_GetOutRingBufHeader(Base* pBaseObj)
{
    RINGBUFFER_HEADER *vAddr;
    UINT32 pAddr;

    pBaseObj->outPin->GetBufHeader(pBaseObj->outPin, (void**)&vAddr, &pAddr);
    return (UINT32)pAddr;
}

void delete_Voice(Base* pBaseObj)
{
    Voice* pVoiceObj;;
    if(pBaseObj == NULL) return;

    pVoiceObj = (Voice*)pBaseObj->pDerivedObj;

    del_id_from_map(pBaseObj->GetAgentID(pBaseObj));
    pBaseObj->ResetOutRingBuf(pBaseObj);
    pBaseObj->Remove(pBaseObj);

    if(pBaseObj->outPin != NULL)
	{
        pBaseObj->outPin->Delete(pBaseObj->outPin);
		pBaseObj->outPin = NULL;
	}

    kfree(pVoiceObj);
    delete_base(pBaseObj);
    return;
}

Base* new_Voice()
{
    /* object init */
    char name[] = "Voice";
    Voice* pVoiceObj = NULL;
    Base* pObj = new_base();
    if(pObj == NULL)
    {
        return NULL;
    }
    pVoiceObj = (Voice*)kmalloc(sizeof(Voice), GFP_KERNEL);
    if(pVoiceObj == NULL)
    {
        pObj->Delete(pObj);
        return NULL;
    }
    pObj->pDerivedObj = pVoiceObj;

    DEBUG("[%s] \n", __FUNCTION__);

    /* setup member functions */
    pObj->Delete      = delete_Voice;
    pObj->SwitchFocus = Voice_SwitchFocus;
    pVoiceObj->GetOutRingBufHeader = Voice_GetOutRingBufHeader;

    /* agent init */
    pObj->instanceID = CreateAgent(AUDIO_IN);
    if(pObj->instanceID == UNDEFINED_AGENT_ID)
    {
        pObj->Delete(pObj);
        return NULL;
    }
    pObj->outPinID   = PCM_OUT1;
    pObj->instanceID = (pObj->instanceID & INST_MASK) | pObj->outPinID;
    memcpy(pObj->name, name, sizeof(name));

    pObj->outPin = new_pin(AUDIO_VOICE_BUF_SIZE);

    msleep(100);

    pObj->InitOutRingBuf(pObj);

    Voice_SwitchFocus(pObj, NULL);

    add_id_to_map(pObj, pObj->GetAgentID(pObj));
    DEBUG("[%s] init OK\n", pObj->name);
    return pObj;
}
