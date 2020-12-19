#ifndef _FULLCHECK_H
#define _FULLCHECK_H


#include <linux/types.h>
#include "common.h"

typedef struct
{
	int64_t checkTime[NUMBER_OF_PINS];
	bool    bDropflag[NUMBER_OF_PINS];
} FULL_CHECK_T;


void FULLCHECK_Reset(FULL_CHECK_T *pChecker, int pinIndex, int isAllPin);
void FULLCHECK_StartDrop(FULL_CHECK_T *pChecker, int pinIndex);
bool FULLCHECK_IsDropping(FULL_CHECK_T *pChecker, int pinIndex);
void FULLCHECK_StopDrop(FULL_CHECK_T *pChecker, int pinIndex, long writableSize, long ringBufferSize);

#endif /* _FULLCHECK_H */
