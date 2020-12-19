#ifndef __SC_SESSION_T1_H__
#define __SC_SESSION_T1_H__


#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/spinlock.h>

#include <core/rtk_scd.h>
#include <core/rtk_scd_atr.h>
#include <adapter/rtk_scd_priv.h>
#include "rdvb_acas_atr.h"

typedef enum
{
    SC_PROTOCOL_PARAM_T1_IFSC = 0x100,                      // IFSC : Information Field Size of Card
    SC_PROTOCOL_PARAM_T1_IFSD = 0x102,                      // IFSD : Information Field Size of Device
    SC_PROTOCOL_PARAM_T1_EDC_ALGO = 0x101,                  // EDC mode : 0 : LRC, others: CRC
    SC_PROTOCOL_PARAM_T1_SEQUENCE_COUNTER_ENABLE = 0x110,   // Sequence counter mode : 0 : Disable, 1 : Enable
    SC_PROTOCOL_PARAM_T1_DATA_CHAINING = 0x111,             // Data Chaining mode : 0 : Disable , 1 : Enable
}SC_PROTOCOL_PARAM_ID;

typedef struct
{
    unsigned char  flags;
    unsigned char  ifsc;
}SC_PROTOCOL_PARAM_T1;

typedef struct
{
    unsigned char NAD;
    unsigned char PCB;
    unsigned char Len;
    unsigned char Data[256];
}SESSION1_BLOCK;

#define MAX_TX_FAIL_RETRY   3
#define MAX_RX_FAIL_RETRY   3
#define MAX_CAPDU_RESENT    3

#define RBLOCK_STATUS_NO_ERROR          0x0
#define RBLOCK_STATUS_EDC_PARITY_ERROR  0x1       
#define RBLOCK_STATUS_OTHER_ERROR       0x2

#define IBLOCK_SEQ(x)       ((x >> 6) & 0x1)
#define IBLOCK_CHAINING(x)  ((x >> 5) & 0x1)

#define RBLOCK_SEQ(x)       ((x >> 5) & 0x1)
#define RBLOCK_ERR(x)       (x & 0x2F)

#define SEQUENCE_NUM(x)     ((x & 0x1) << 6)
#define CHAINING(x)	    ((x) ? (1 << 5) : 0)

#define DISABLE_SEQUENCE_COUNTER	0x80
#define DISABLE_DATA_CHAINING		0x40


void rdvb_acas_io_init_session_t1(void);
int rdvb_acas_session_t1_send_rblock(scd *pthis, unsigned int Seq, unsigned char Status);
int rdvb_acas_session_t1_send_block(scd *pthis, SESSION1_BLOCK* pBlock);
int rdvb_acas_session_t1_do_abort(scd *pthis);
int rdvb_acas_session_t1_do_resync(scd *pthis);
int rdvb_acas_session_t1_do_IFS_request(scd *pthis, unsigned char* pIFSD);
int rdvb_acas_session_t1_receive_block(scd *p_this, SESSION1_BLOCK* pBlock);
int rdvb_acas_session_t1_send_WTX_response(scd *pthis);
int rdvb_acas_session_t1_block_transfer(scd *pthis, SESSION1_BLOCK* pCBlock, SESSION1_BLOCK* pRBlock);
int rdvb_acas_session_t1_set_param(SC_PROTOCOL_PARAM_ID Id, unsigned long Val);

int rdvb_acas_session_t1_transfer(
    scd                     *pthis,
    unsigned char*          pCAPDU,
    unsigned int            APDULen,
    unsigned char*          pRAPDU,
    unsigned int*           pRAPDULen,
    unsigned int            Timeout
    );


#endif

