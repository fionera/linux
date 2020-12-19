/******************************************************************************
 *
 *   Copyright(c) 2014 Realtek Semiconductor Corp. All rights reserved.
 *
 *   @author yungjui.lee@realtek.com
 *
 *****************************************************************************/

#ifndef _RTK_FLOW_H_
#define _RTK_FLOW_H_

#include <linux/fs.h>
#include "audio_inc.h"
#include "audio_util.h"

typedef void (*pfnCallBack)(SINT32 Data);

typedef struct _FlowManager FlowManager;

typedef struct _FlowManager{
    void* pDerivedFlow;
    Base* pBaseObj;
    ReferenceClock *pRefClock;
    UINT32 state;
    UINT32 extRefClock_phy;
    Queue* pEventQueue;
    Queue* pUserQueue;
    struct semaphore* m_eos_sem;
    pfnCallBack eos_callback_func;
    SINT32 eos_callback_data;
    UINT32 SkipDECFlush;

    UINT32 (*Connect)(FlowManager*, Base*, Base*);
    UINT32 (*Remove)(FlowManager*, Base*);
    UINT32 (*Run)(FlowManager*);
    UINT32 (*Stop)(FlowManager*);
    UINT32 (*Pause)(FlowManager*);
    UINT32 (*Flush)(FlowManager*);
    UINT32 (*SetRate)(FlowManager*, SINT32);
    UINT32 (*GetState)(FlowManager*);
    UINT32 (*SetExtRefClock)(FlowManager*, UINT32);
    UINT32 (*SetEOSCallback)(FlowManager*, pfnCallBack, SINT32);
    UINT32 (*Search)(FlowManager*, Base*);
    void   (*Delete)(FlowManager*);
    UINT32 (*ShowCurrentExitModule)(FlowManager*, int forcePrint);
    UINT32 (*CheckExistModule)(FlowManager*, Base*);
    UINT32 (*DirectRun)(FlowManager*);
    UINT32 (*SetSkipDECFlush)(FlowManager*, UINT32);
}FlowManager;
FlowManager* new_flow(void);

#endif
