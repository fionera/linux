/* ------------------------------------------------------------------------
   scd_rtd299x.c  scd driver for Realtek Neptune/rtd299x
   -------------------------------------------------------------------------
    Copyright (C) 2019 nico

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
----------------------------------------------------------------------------
Update List :
----------------------------------------------------------------------------
    1.0     |   20190312    |   nico  | 1) create phase
----------------------------------------------------------------------------*/
#include <asm/delay.h>

#include "rdvb_acas_session_t1.h"
#include "rdvb_acas.h"
#include "rdvb_acas_debug.h"

unsigned char   m_ifsd_rdy;
unsigned int    m_SequenceCounter;    
unsigned int    m_cwt;
unsigned int    m_bwt;    
unsigned char   m_ifsc;     
unsigned char   m_ifsd;   
unsigned char   m_use_crc; 
unsigned long   m_flags;

extern unsigned char SC_LRC8(
    unsigned char*          pData, 
    unsigned char           Len, 
    unsigned char           InitLRC
    );
extern unsigned short SC_CRC16(
    unsigned char*          pData, 
    unsigned char           Len, 
    unsigned short          InitCRC
    );


const char* acas_error_str(int ret)
{
    switch(ret) {
    case SC_SUCCESS:                 	return "Success";
    case SC_FAIL:                    	return "Fail";
    case SC_ERR_TIMEOUT:             	return "Timeout";
    case SC_ERR_NO_ICC:                 return "No ICC";
    case SC_ERR_ICC_DEACTIVATE: 	    return "ICC Deactivated";
    case SC_ERR_IFD_DISABLED:   	    return "IFD disabled";
    case SC_ERR_ATR_TIMEOUT:    	    return "Wait ATR Timeout";
    case SC_ERR_EDC_OR_PARITY_ERROR: 	return "EDC and/or Parity Error";
    }

    return "unknown error";
}

void rdvb_acas_io_init_session_t1(void)
{
	m_SequenceCounter = 0;
	m_cwt	   = 200;	     // character wait time
	m_bwt	   = 2000;	     // block wait time
	m_ifsc	   = 32;	     // maximum information field size of Card
	m_ifsd	   = 252;	     // maximum information field size of IFD
	m_ifsd_rdy = 0;
	m_use_crc  = 0; 	     // use crc or lrc
	m_flags    = 0;
}

/*---------------------------------------------------------------------- 
 * Func : rdvb_acas_session_t1_set_param 
 * Desc : SetParam
 * Parm : Id  : Parameter ID
 *        Val : Parameter Value
 * Retn : SC_OK
 *----------------------------------------------------------------------*/
int rdvb_acas_session_t1_set_param(SC_PROTOCOL_PARAM_ID Id, unsigned long Val)
{       
    switch(Id)
    {
        case SC_PROTOCOL_PARAM_T1_SEQUENCE_COUNTER_ENABLE:
            if (Val){
                ACAS_INFO("T1 Sequence Counter Enabled\n");
                m_flags &= ~DISABLE_SEQUENCE_COUNTER;
            }else{
                ACAS_INFO("T1 Sequence Counter Disabled\n");
                m_flags |= DISABLE_SEQUENCE_COUNTER;
                m_SequenceCounter = 0;
            }
            break;

        case SC_PROTOCOL_PARAM_T1_IFSD:
            ACAS_INFO("T1 IFSD=%d\n", m_ifsd);    
            break;

        case SC_PROTOCOL_PARAM_T1_IFSC:
            if (Val > 254)
                Val = 254;

            m_ifsc  = Val;
            ACAS_INFO("T1 IFSC=%d\n", m_ifsc);    
            break;

        case SC_PROTOCOL_PARAM_T1_EDC_ALGO:
            m_use_crc = (Val) ? 1 : 0;
            ACAS_INFO("T1 EDC Mode=(%s)\n", (m_use_crc) ? "CRC" : "LRC");
            break;

        default:
            return SC_FAIL;
    }
    return SC_SUCCESS;
}

/*---------------------------------------------------------------------- 
 * Func : rdvb_acas_session_t1_set_param 
 * Desc : SetParam
 * Parm : Id  : Parameter ID
 *        Val : Parameter Value
 * Retn : SC_OK
 *----------------------------------------------------------------------*/
int rdvb_acas_session_t1_get_param(SC_PROTOCOL_PARAM_ID Id,unsigned long* pVal)
{
    switch(Id)
    {
        case SC_PROTOCOL_PARAM_T1_SEQUENCE_COUNTER_ENABLE:
            *pVal = (m_flags & DISABLE_SEQUENCE_COUNTER) ? 0 : 1;
            break;
  
        case SC_PROTOCOL_PARAM_T1_IFSC:
            *pVal = m_ifsc;
            break;

        case SC_PROTOCOL_PARAM_T1_IFSD:
            *pVal = 254;
            break;

        case SC_PROTOCOL_PARAM_T1_EDC_ALGO:
            *pVal = m_use_crc;
            break;
        default:
            return SC_FAIL;
    }
    return SC_SUCCESS;
}


int rdvb_acas_session_t1_send_rblock(scd *pthis, unsigned int Seq, unsigned char Status)
{
    SESSION1_BLOCK Block;
    ACAS_INFO("SC_SESSION_T1:: Send R(%d, %d) to Card\n", (Seq &0x1), Status);
   
    Block.NAD = 0;
    Block.PCB = 0x80 | (Seq & 0x1)<<5 | (Status & 0x2F);  // keep sequence counter ...
    Block.Len = 0;
    return rdvb_acas_session_t1_send_block(pthis, &Block);                
}

/*---------------------------------------------------------------------- 
 * Func : rdvb_acas_session_t1_do_abort 
 * Desc : Do Abort 
 * Parm : N/A
 * Retn : SC_OK / SC_FAIL
 *----------------------------------------------------------------------*/
int rdvb_acas_session_t1_do_abort(scd *pthis)
{
    SESSION1_BLOCK Block;
    int ret = SC_SUCCESS;

    Block.NAD = 0;        
    Block.PCB = 0xC2;
    Block.Len = 0;
    ret = rdvb_acas_session_t1_block_transfer(pthis, &Block, &Block);  
    if (ret==SC_SUCCESS && Block.PCB==0xE2){
        ACAS_INFO("Do Abort successed\n");
        return SC_SUCCESS;
    }

    ACAS_WARNING("Do Abort failed\n");
    return SC_FAIL;
}

/*---------------------------------------------------------------------- 
 * Func : rdvb_acas_session_t1_do_resync 
 * Desc : Do Resync 
 * Parm : N/A
 * Retn : SC_OK / SC_FAIL
 *----------------------------------------------------------------------*/
int rdvb_acas_session_t1_do_resync(scd *pthis)
{
    SESSION1_BLOCK Block;
    int ret = SC_SUCCESS;

    Block.NAD = 0;
    Block.PCB = 0xC0;
    Block.Len = 0; 
    ret = rdvb_acas_session_t1_block_transfer(pthis, &Block, &Block); 
    if (ret==SC_SUCCESS && Block.PCB==0xE0){        
        ACAS_INFO("Do Resync successed\n");
        return SC_SUCCESS;
    }

    ACAS_WARNING("Do Resync failed\n");
    return SC_FAIL;
}

/*---------------------------------------------------------------------- 
 * Func : rdvb_acas_session_t1_do_IFS_request 
 * Desc : Update Information Field Size of Device
 * Parm : pIFSD : new IFS of Device (I/O)
 * Retn : SC_OK / SC_FAIL
 *----------------------------------------------------------------------*/
int rdvb_acas_session_t1_do_IFS_request(scd *pthis,unsigned char* pIFSD)
{
    SESSION1_BLOCK Block;
    int ret = SC_SUCCESS;

    Block.NAD = 0;
    Block.PCB = 0xC1;
    Block.Len = 1;
    Block.Data[0] = *pIFSD;
    m_ifsd_rdy = 1;                     // set up ifsd 

    ret = rdvb_acas_session_t1_block_transfer(pthis, &Block, &Block);    
    if (ret==SC_SUCCESS && Block.PCB==0xE1){
        *pIFSD = Block.Data[0];
        ACAS_INFO("Do IFS Request successed, IFSD=%d\n", *pIFSD);
        return SC_SUCCESS;
    }
    ACAS_WARNING("Do IFS Request failed\n");
    return SC_FAIL;
}

int rdvb_acas_session_t1_send_WTX_response(scd *pthis)
{
    SESSION1_BLOCK Block;
    Block.NAD = 0;
    Block.PCB = 0xE3;
    Block.Len = 0;
    return rdvb_acas_session_t1_send_block(pthis, &Block);
}

int rdvb_acas_session_t1_send_IFS_response(scd *pthis, unsigned char IFS)
{
    SESSION1_BLOCK Block;
    Block.NAD = 0;
    Block.PCB = 0xE1;
    Block.Len = 1;
    Block.Data[0] = IFS;
    return rdvb_acas_session_t1_send_block(pthis, &Block);
}

/*---------------------------------------------------------------------- 
 * Func : rdvb_acas_session_t1_block_transfer 
 * Desc : Transaction
 * Parm : pBlock    : (IN) Block Data to be sent
 * Retn : SC_OK / SC_FAIL
 *----------------------------------------------------------------------*/
int rdvb_acas_session_t1_block_transfer(scd *pthis, SESSION1_BLOCK* pCBlock, SESSION1_BLOCK* pRBlock)
{    
    int ret = SC_SUCCESS;
    if((pCBlock==NULL)||(pRBlock==NULL)){
        ACAS_WARNING(" input error,L:%d",__LINE__);  
        return SC_FAIL;
    }
    if ((ret = rdvb_acas_session_t1_send_block(pthis, pCBlock))!=SC_SUCCESS)
        return ret;
    else
        return rdvb_acas_session_t1_receive_block(pthis, pRBlock);
}

/*---------------------------------------------------------------------- 
 * Func : rdvb_acas_session_t1_send_block 
 * Desc : Send block
 * Parm : pBlock    : (IN) Block Data to be sent
 * Retn : SC_OK / SC_FAIL
 *----------------------------------------------------------------------*/
int rdvb_acas_session_t1_send_block(scd *pthis, SESSION1_BLOCK* pBlock)
{
    if(pBlock==NULL){
        ACAS_WARNING(" input error,L:%d",__LINE__);	
        return SC_FAIL;
    }
    if(pBlock->Len > 254){
        ACAS_WARNING("pBlock->Data over flow, pBlock->Len=%d\n", pBlock->Len);
        return SC_FAIL;
    }
    if (m_use_crc){
        unsigned short crc = SC_CRC16((unsigned char*)pBlock, pBlock->Len + 3, 0);
        pBlock->Data[pBlock->Len]   = (crc >> 8) & 0xFF;
        pBlock->Data[pBlock->Len+1] = crc & 0xFF;
        return scd_xmit(pthis, (unsigned char*)pBlock, pBlock->Len + 5);
    }
    else
    {
        pBlock->Data[pBlock->Len] = SC_LRC8((unsigned char*)pBlock, pBlock->Len + 3, 0);    // add EDC code
        return scd_xmit(pthis, (unsigned char*)pBlock, pBlock->Len + 4);
    }
}

/*---------------------------------------------------------------------- 
 * Func : rdvb_acas_session_t1_receieve_characters 
 * Desc : Receive character with character timeout
 * Parm : pBlock    : (IN) Rx Block
 * Retn : SC_OK / SC_FAIL
 *----------------------------------------------------------------------*/
int rdvb_acas_session_t1_receieve_characters(scd *pthis, unsigned char* pBuff, unsigned int Len)
{
    int rx_len = 0;
    int ret;
    while(Len){
        ret = scd_read(pthis, pBuff, Len, m_cwt);
        if (ret <=0)
            return ret;

        Len    -= ret;
        pBuff  += ret;
        rx_len += ret;
    }
    return rx_len;
}

/*---------------------------------------------------------------------- 
 * Func : rdvb_acas_session_t1_receive_block 
 * Desc : Receive an block
 * Parm : pBlock    : (IN) Rx Block
 * Retn : SC_OK / SC_FAIL
 *----------------------------------------------------------------------*/
int rdvb_acas_session_t1_receive_block(scd *p_this, SESSION1_BLOCK* pBlock)
{    
    unsigned char* pBuff = (unsigned char*) pBlock;
    int rx_len = 0;
    int ret;

    if(pBlock==NULL){
        ACAS_WARNING("ReceiveBlock(): input error\n");	
        return SC_FAIL;
    }
    memset(pBlock, 0, sizeof(SESSION1_BLOCK));
    
    if ((ret = rdvb_acas_session_t1_receieve_characters(p_this, pBuff, 3))<3) {
        ACAS_WARNING("Receive Block header failed, ret=%d\n", ret);
        return SC_FAIL;
    }

    // Receiver APDU Data
    rx_len = (m_use_crc) ? pBuff[2] + 2 : pBuff[2] + 1;
    ret = rdvb_acas_session_t1_receieve_characters(p_this, &pBuff[3], rx_len);

    if (ret!=rx_len){
        ACAS_WARNING("Receive Block data failed, ret=%d/%d\n", ret, rx_len);
        ACAS_WARNING("Receive Block = ");
        acas_apdu_rx_dump(pBuff, (ret > 0) ? (3 + ret) : 3);   // dump received data...                    
        return SC_FAIL;
    }
    if(pBlock->Len>254){
        ACAS_WARNING("SendBlock failed: pBlock->Data over flow, pBlock->Len=%d\n", pBlock->Len);
        return SC_FAIL;
    }
    if (m_use_crc) {
        unsigned short crc = (pBlock->Data[pBlock->Len]<<8) + pBlock->Data[pBlock->Len+1];
        if (crc!=SC_CRC16((unsigned char*)pBlock, pBlock->Len +3, 0)){
            ACAS_WARNING("Receive Block length failed, check crc failed\n");
            acas_apdu_rx_dump(pBuff, pBlock->Len + 3);
            return SC_FAIL;
        }
    }else{
        if (SC_LRC8((unsigned char*)pBlock, pBlock->Len + 4, 0)){
            ACAS_WARNING("Receive Block length failed, check lrc failed\n");
            acas_apdu_rx_dump(pBuff, pBlock->Len + 4);
            return SC_FAIL;
        }
    }

    return SC_SUCCESS;                
}

/*---------------------------------------------------------------------- 
 * Func : rdvb_acas_session_t1_transfer 
 * Desc : do APDU Transfer
 * Parm : pCAPDU    : (IN) C-APDU to be sent
 *        APDULen   : (IN) size of C-APDU
 *        pRAPDU    : (IN) R-APDU buffer
 *        pRAPDULen : (IN) size of buffer to restire R-APDU
 *                    (OUT)size of received R-APDU
 *        Timeout   : (IN) maximum wait time of the APDU Transfer
 * Retn : SC_OK / SC_FAIL
 *----------------------------------------------------------------------*/
int rdvb_acas_session_t1_transfer(
    scd*                    pthis,
    unsigned char*          pCAPDU, 
    unsigned int            APDULen, 
    unsigned char*          pRAPDU, 
    unsigned int*           pRAPDULen, 
    unsigned int            Timeout
    )
{        
    SESSION1_BLOCK Block;
    SESSION1_BLOCK RBlock;
    unsigned int tx_error_cnt = 0;
    unsigned int rx_error_cnt = 0;
    unsigned int resend_capdu = 0;
    int ret = SC_SUCCESS;

    if (m_ifsd_rdy==0)    
        rdvb_acas_session_t1_do_IFS_request(pthis, &m_ifsd);        // send IFS request to SMC to inform the maxmium infomation field size of IFD    

    while(APDULen){
        // Send Block
        Block.NAD = 0;
        Block.PCB = SEQUENCE_NUM(m_SequenceCounter);

        if (APDULen > (m_ifsc-4)){            
            Block.Len = (m_ifsc -4);
            Block.PCB |= 0x20;
        }else{
            Block.Len = APDULen;
        }

        memcpy(Block.Data, pCAPDU, Block.Len);
        pCAPDU  += Block.Len;
        APDULen -= Block.Len;

send_capdu:
        if ((ret = rdvb_acas_session_t1_send_block(pthis, &Block))!=SC_SUCCESS){
            ACAS_WARNING(" Xmit CAPDU failed - %d (%s)\n", ret, acas_error_str(ret));
            switch(ret) {
                case SC_FAIL:
                case SC_ERR_TIMEOUT:
                    if (tx_error_cnt++ <= MAX_TX_FAIL_RETRY) {
                    msleep(1);   // sleep for 10 m
                    ACAS_WARNING("send block do retry (%d/%d)\n", tx_error_cnt, MAX_TX_FAIL_RETRY);
                    goto send_capdu;
                }
                ACAS_WARNING("Transfer APDU failed, TX Retry timeout\n");
            }
            return ret;
        }
        if (m_flags & DISABLE_SEQUENCE_COUNTER)
            m_SequenceCounter=0;        // for some companies that use T1 without sequence counter...
        else            
            m_SequenceCounter++;

receieve_rapdu:
        if ((ret = rdvb_acas_session_t1_receive_block(pthis, &RBlock))!=SC_SUCCESS) {
            /*------------------------------------------
             * case 1 : unable to receive correct response from ICC
             * Solution : Send RBlock to ICC to ask ICC to resend
             *            command again
             *-----------------------------------------*/
            ACAS_WARNING("Unable to Receive RAPDU from ICC - %d (%s) (cnt=%d)\n",
                ret, acas_error_str(ret), rx_error_cnt);

            if (rx_error_cnt++ > MAX_RX_FAIL_RETRY){
                ACAS_WARNING("Transfer APDU failed, Rx Retry timeout\n");
                rdvb_acas_session_t1_do_resync(pthis);
                m_SequenceCounter = 0;
                return SC_FAIL;
            }

            ACAS_WARNING("Send R(%d) to ICC (cnt=%d/%d)\n", 
                IBLOCK_SEQ(Block.PCB), rx_error_cnt, MAX_RX_FAIL_RETRY);
            msleep(1);
            rdvb_acas_session_t1_send_rblock(pthis, IBLOCK_SEQ(Block.PCB), (ret==SC_ERR_EDC_OR_PARITY_ERROR) ? RBLOCK_STATUS_EDC_PARITY_ERROR : RBLOCK_STATUS_OTHER_ERROR);                
            goto receieve_rapdu;
        }
        rx_error_cnt = 0;

        /*------------------------------------------
         * handle Response Block
         *-----------------------------------------*/   
      
        switch(RBlock.PCB & 0xC0){
           case 0x80:{
           /*------------------------------------------
            * R-Block         
            *-----------------------------------------*/                        
            unsigned char ResponseSequence = RBLOCK_SEQ(RBlock.PCB);
            unsigned char ResponseError = RBLOCK_ERR(RBlock.PCB);            

            ACAS_DEBUG("Got R-Block (Seq=%d, error=%d)\n", ResponseSequence, ResponseError);                                    

            if (IBLOCK_SEQ(Block.PCB)!=RBLOCK_SEQ(RBlock.PCB)){
                // Sequence toggled : ACK                                  
                if (RBLOCK_ERR(RBlock.PCB)){ 
                    // Humm... Actually, this case should not happen anyway                    
                    ACAS_WARNING("ICC reponse R-Block with different Sequence Number but with Error Status (%d), do nothing..\n",     
                            RBLOCK_ERR(RBlock.PCB));
                }

                if (APDULen==0){
                    // Humm... Actually, this case should not happen anyway 
                    ACAS_WARNING("Transfer APDU failed - ICC send R(%d) for next block, but no more data left\n", 
                                RBLOCK_ERR(RBlock.PCB)); 

                    rdvb_acas_session_t1_do_resync(pthis);
                    m_SequenceCounter = 0;                                
                    return SC_FAIL;
                } 
            }else{
                // Sequence not changed : NACK, should resend Command APDU                 
                ACAS_WARNING("Got R-Block with same Seq number %d (Seq=%d, error=%d)\n", 
                        RBLOCK_SEQ(RBlock.PCB), 
                        ResponseSequence, ResponseError);                                      

                if (resend_capdu++ < MAX_CAPDU_RESENT) {
                    m_SequenceCounter--;               
                    goto send_capdu;                    
                }

                ACAS_WARNING("Transfer APDU failed - Resend C-APDU timeout\n");                
                rdvb_acas_session_t1_do_resync(pthis);
                m_SequenceCounter = 0;      
                return SC_FAIL;
            }
            break;
        }
        case 0xC0:{
            switch(RBlock.PCB){
                    case 0xC0:
                        ACAS_WARNING("Got Resynchronisation Request, (Not Implemented Yet....)\n");
                        break;

                    case 0xE0:
                        ACAS_WARNING("Got Resynchronisation Response, (Not Implemented Yet....)\n");
                        break;
                      
                    case 0xC1:
                        ACAS_WARNING("Got IFS request from SMC, (IFSC=%d), Send IFS Response\n", RBlock.Data[0]);
                        m_ifsc = RBlock.Data[0];      
                        rdvb_acas_session_t1_send_IFS_response(pthis, m_ifsc);                
                        break;

                    case 0xE1:
                        ACAS_WARNING("Got IFS response, (do nothing....)\n");
                        break;

                    case 0xC2:
                        ACAS_WARNING("Got abort request, (Not Implemented Yet....)\n");
                        break;

                    case 0xE2:
                        ACAS_WARNING("Got abort response, (Not Implemented Yet....)\n");
                        break;                
                                     
                    case 0xC3:
                        ACAS_DEBUG("Got WTX Request, Response WTX reponse\n");
                        rdvb_acas_session_t1_send_WTX_response(pthis);
                        continue;

                    case 0xE3:
                        ACAS_DEBUG("Got WTX Response... do noting\n");
                        break;

                    default:
                        if ((RBlock.PCB >> 6) & 0x1){          
                            ACAS_WARNING("Got S-Block - unknown request id - %x\n",  RBlock.PCB & 0x1F);
                        }else{
                            ACAS_WARNING("Got S-Block - unknown response id - %x\n",  RBlock.PCB & 0x1F);
                        }
                        break;
                }
                break;
            }
        default:{
                // I-Block : so far we didn't handl the Data chaining function...            
                if (IBLOCK_CHAINING(RBlock.PCB)){
                    ACAS_WARNING("transfer command failed, ICC data chaining receive is not supported yet\n");
                    rdvb_acas_session_t1_do_resync(pthis);
                    m_SequenceCounter = 0;                
                    return SC_FAIL; 
                }

                if (*pRAPDULen < RBlock.Len){
                    ACAS_WARNING("transfer command failed, rx buffer over flow (size=%d, need %d)\n", *pRAPDULen, RBlock.PCB);
                    return SC_FAIL; 
                }
                memcpy(pRAPDU, RBlock.Data, RBlock.Len);
                *pRAPDULen = RBlock.Len;
                goto end_proc;
            }
        }
    }
end_proc:    

    //SC_INFO(PURPLE"Exit SC_SESSION_T1::Transfer(), CA out buf=%p, CA out len=%d\n"NONE,pCA_OUT_buf,CA_OUT_len);
    return SC_SUCCESS;    
}    

