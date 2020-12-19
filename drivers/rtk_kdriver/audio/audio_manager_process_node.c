/******************************************************************************
 *
 *   Copyright(c) 2017 Realtek Semiconductor Corp. All rights reserved.
 *
 *   @author deni10135@realtek.com
 *
 *****************************************************************************/
#if defined(USE_INBAND_SYSPROC_COMMUNICATION)
#include "audio_process_node.h"
#include "audio_base.h"
#include "AudioInbandAPI.h"
#include "audio_process.h"

int audio_manager_activate(node_handle h_node)
{
    Pin *inPktPin, *outPktPin;
    SYSTEM_MESSAGE_RES res;
    SYSTEM_MESSAGE_INFO info;
    UINT8 *in_rp, *in_wp, *in_base;
    UINT8 *out_rp, *out_wp, *out_base;
    UINT32 bufferSize;
    UINT32 check_addr;

    inPktPin = h_node->inPktPin;
    outPktPin = h_node->outPktPin;

    inPktPin->GetBuffer(inPktPin, &in_base, &bufferSize);;
    in_rp = inPktPin->GetReadPtr(inPktPin, 0);
    in_wp = inPktPin->GetWritePtr(inPktPin);

    check_addr = (in_rp - in_base) % sizeof(SYSTEM_MESSAGE_INFO);
    if (check_addr != 0)
    {
        ERROR("[AMM_THD] rp addr is not alignment %d\n", check_addr);
        inPktPin->SetWritePtr(inPktPin, in_base);
        inPktPin->SetReadPtr(inPktPin, in_base, 0);
        ERROR("[AMM_THD] reset wp/rp addr to base address\n");
    }

    check_addr = (in_wp - in_base) % sizeof(SYSTEM_MESSAGE_INFO);
    if (check_addr != 0)
    {
        ERROR("[AMM_THD] wp addr is not alignment %d\n", check_addr);
        inPktPin->SetWritePtr(inPktPin, in_base);
        inPktPin->SetReadPtr(inPktPin, in_base, 0);
        ERROR("[AMM_THD] reset wp/rp addr to base address\n");
    }

    outPktPin->GetBuffer(outPktPin, &out_base, &bufferSize);
    out_rp = outPktPin->GetReadPtr(outPktPin, 0);
    out_wp = outPktPin->GetWritePtr(outPktPin);

    check_addr = (out_rp - out_base) % sizeof(SYSTEM_MESSAGE_INFO);
    if (check_addr != 0)
    {
        ERROR("[AMM_THD] rp addr is not alignment %d\n", check_addr);
        outPktPin->SetReadPtr(outPktPin, out_base, 0);
        outPktPin->SetWritePtr(outPktPin, out_base);
        ERROR("[AMM_THD] reset wp/rp addr to base address\n");
    }

    check_addr = (out_wp - out_base) % sizeof(SYSTEM_MESSAGE_INFO);
    if (check_addr != 0)
    {
        ERROR("[AMM_THD] wp addr is not alignment %d\n", check_addr);
        outPktPin->SetReadPtr(outPktPin, out_base, 0);
        outPktPin->SetWritePtr(outPktPin, out_base);
        ERROR("[AMM_THD] reset wp/rp addr to base address\n");
    }

    memset(&res, 0, sizeof(SYSTEM_MESSAGE_RES));
    if (inPktPin->GetReadSize(inPktPin, 0) >= sizeof(SYSTEM_MESSAGE_INFO)) {
        inPktPin->MemCopyFromReadPtr(inPktPin, (UINT8*)&info, sizeof(SYSTEM_MESSAGE_INFO), AUDIO_NONE);
        inPktPin->AddReadPtr(inPktPin, sizeof(SYSTEM_MESSAGE_INFO));
        INFO("[AMM_THD] info: type %x, instanceID %x\n", info.type, (UINT32)info.instanceID);

        switch (info.m_type)
        {
            case ENUM_NODE_CLOSE_MESSAGE:
            {
                INFO("[AMM_THD] SysProc_Delete Info\n");

                    //TODO
                /*DeleteProcess(&info);*/

                res.instanceID = -1;
                res.packetBufHeader[0] = 0x2379;
                res.packetBufHeader[1] = 0x2379;

                outPktPin->MemCopyToWritePtr(outPktPin, (UINT8 *)&res, sizeof(SYSTEM_MESSAGE_RES));
                outPktPin->AddWritePtr(outPktPin, sizeof(SYSTEM_MESSAGE_RES));

                INFO("[AMM_THD] SysProc_Delete Info End\n");
            }
            break;
            case ENUM_NODE_OPEN_MESSAGE:
            {
                if((info.type >= ENUM_NODE_SRC) && (info.type <= ENUM_NODE_AAC_ENC))
                {
                    INFO("[AMM_THD] Sys_Create Info\n");

                    //TODO
                    /*CreateProcess(&info, &res);*/

                    INFO("[AMM_THD] res.instanceID %x\n", (UINT32)res.instanceID);
                    INFO("[AMM_THD] res.header[0] = %x\n", (UINT32)res.packetBufHeader[0]);
                    INFO("[AMM_THD] res.header[1] = %x\n", (UINT32)res.packetBufHeader[1]);

                    outPktPin->MemCopyToWritePtr(outPktPin, (UINT8 *)&res, sizeof(SYSTEM_MESSAGE_RES));
                    outPktPin->AddWritePtr(outPktPin, sizeof(SYSTEM_MESSAGE_RES));

                    INFO("[AMM_THD] Sys_Create Info End\n");
                }
                else
                {
                    ERROR("[AMM_THD] Sys_Create Wrong node type %d\n", info.type);
                    res.instanceID = -1;
                    res.packetBufHeader[0] = 0x2379;
                    res.packetBufHeader[1] = 0x2379;
                    outPktPin->MemCopyToWritePtr(outPktPin, (UINT8 *)&res, sizeof(SYSTEM_MESSAGE_RES));
                    outPktPin->AddWritePtr(outPktPin, sizeof(SYSTEM_MESSAGE_RES));
                }
            }
            break;
            case ENUM_AOUT_EOS_MESSAGE:
            {
                Base* pBase;
                INFO("[AMM_THD] Sys_Proc AOUT EOS\n");
                pBase = map_to_module(info.instanceID);

                if (pBase == NULL)
                {
                    INFO("[AMM_THD] NO EOS Handler in RHAL\n");
                }
                else
                {
                    UINT32 event = info.data[1];
                    pBase->PrivateInfo(pBase, INFO_EOS, (UINT8*)&event, sizeof(UINT32));
                }

                if(!info.is_non_blocking)
                {
                    res.data[3] = S_OK;
                    outPktPin->MemCopyToWritePtr(outPktPin, (UINT8 *)&res, sizeof(SYSTEM_MESSAGE_RES));
                    outPktPin->AddWritePtr(outPktPin, sizeof(SYSTEM_MESSAGE_RES));
                }

                INFO("[AMM_THD] Sys_Proc AOUT EOS End\n");
            }
            break;
            case ENUM_UNKNOWN_MESSAGE:
            case ENUM_MESSAGE_MAX:
            default:
            {
                ERROR("[AMM_THD] Unknown Message From ADSP %x\n", info.m_type);
                res.instanceID = -1;
                res.packetBufHeader[0] = 0x2379;
                res.packetBufHeader[1] = 0x2379;
                outPktPin->MemCopyToWritePtr(outPktPin, (UINT8 *)&res, sizeof(SYSTEM_MESSAGE_RES));
                outPktPin->AddWritePtr(outPktPin, sizeof(SYSTEM_MESSAGE_RES));
            }
            break;
        }
    }

    return NO_ERR;
}

int audio_manager_destroy(node_handle h_node)
{
    /*
    if(h_node == NULL)
        return S_OK;

    if(h_node->inPktPin != NULL)
        h_node->inPktPin->Delete(h_node->inPktPin);
    if(h_node->outPktPin!= NULL)
        h_node->outPktPin->Delete(h_node->outPktPin);
    RTK_Free(h_node);
    */

    return S_OK;
}

int audio_manager_node_create(node_handle h_node, long data[29])
{
    /*
    node_handle h_node     = NULL;
    Pin *inPacketPin       = NULL;
    Pin *outPacketPin      = NULL;

    h_node = (node_handle)RTK_Malloc(sizeof(Node_Struct));
    if(h_node == NULL)
    {
        return NULL;
    }

    inPacketPin = new_packetPin(4096, 1);
    outPacketPin = new_packetPin(4096, 1);

    */
    //h_node->memory   = (void*)h_audio_manager;
    h_node->activate = audio_manager_activate;
    h_node->destroy  = audio_manager_destroy;
    /*
    h_node->inPktPin = inPacketPin;
    h_node->outPktPin = outPacketPin;
    */
    memcpy(h_node->name, "Audio_Message_Manager_Node", 27);
    INFO("[AMM_THD] audio_manager_node_create %x, %x\n", (UINT32)h_node->inPktPin, (UINT32)h_node->outPktPin);
    return TRUE;
}
#endif
