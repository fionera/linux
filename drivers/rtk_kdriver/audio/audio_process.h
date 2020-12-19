/******************************************************************************
 *
 *   Copyright(c) 2016 Realtek Semiconductor Corp. All rights reserved.
 *
 *   @author yungjui.lee@realtek.com
 *
 *****************************************************************************/
#ifndef _RTK_AUDIO_PROCESS_H_
#define _RTK_AUDIO_PROCESS_H_
#include "audio_inc.h"
#include "audio_pin.h"
#include "audio_util.h"
#include "AudioRPC_System.h"

int CreateAudioProcessThread(void);
int DeleteAudioProcessThread(void);

int CreateAudioManagerProcessThread(void);
int DeleteAudioManagerProcessThread(void);

int CreateProcess(SYSTEM_MESSAGE_INFO*, SYSTEM_MESSAGE_RES*);
int DeleteProcess(SYSTEM_MESSAGE_INFO*);

#endif
