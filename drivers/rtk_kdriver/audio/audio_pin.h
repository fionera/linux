/******************************************************************************
 *
 *   Copyright(c) 2014 Realtek Semiconductor Corp. All rights reserved.
 *
 *   @author yungjui.lee@realtek.com
 *
 *****************************************************************************/

#ifndef _RTK_PIN_
#define _RTK_PIN_
#include "audio_inc.h"

typedef struct _MultiPin MultiPin;

typedef struct _MultiPin{
    Pin* pPinObj;
    Pin** pPinObjArr;
}MultiPin;
Pin* new_multiPin(UINT32 bufSize, UINT32 listSize);
Pin* new_multiPin_Cached(UINT32 bufSize, UINT32 listSize);

typedef struct _PacketPin PacketPin;
typedef struct _PacketPin{
    Pin* pPinObj;
    Pin* pDataPinObj;

    void (*InitPacketPin)(Pin*, int);
}PacketPin;

Pin* new_packetPin(UINT32 bufSize, UINT32 listSize);
Pin* new_packetPin_optee(UINT32 bufSize, UINT32 listSize, UINT32* shmva, UINT32* shmpa);
Pin* new_packetPin_Cached(UINT32 bufSize, UINT32 listSize);

UINT32 IsPtrValid(int ptr, Pin* pPinObj);
void ShowPinInfo(Pin* pPinObj);

UINT8*  ConvertPhy2Virtual(Pin* pPinObj, int phyAddress);

UINT8*  ConvertVirtual2Phy(Pin* pPinObj, int VirtualAddress);
void MyMemWrite(BYTE* pCurr, BYTE* Data, long Size);
void MyMemRead(BYTE* pCurr, BYTE* Data, long Size);


#endif /*_RTK_PIN_*/
