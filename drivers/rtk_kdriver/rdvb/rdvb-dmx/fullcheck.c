#include <rdvb-common/clock.h>
#include "fullcheck.h"

void FULLCHECK_Reset(FULL_CHECK_T *pChecker, int pinIndex, int isAllPin)
{

	if (isAllPin) {
		int i;
		for (i = 0; i < NUMBER_OF_PINS; i++) {
			pChecker->bDropflag[i] = false;
			pChecker->checkTime[i] = 0;
		}

	} else {	
		if (pChecker->bDropflag[pinIndex]) {
			pChecker->bDropflag[pinIndex] = false;
			pChecker->checkTime[pinIndex] = 0;
		}
	}
}
void FULLCHECK_StartDrop(FULL_CHECK_T *pChecker, int pinIndex)
{
	if (!pChecker->bDropflag[pinIndex] && (pChecker->checkTime[pinIndex] == 0))
		pChecker->checkTime[pinIndex] = CLOCK_GetPTS();
	else if (!pChecker->bDropflag[pinIndex] && (CLOCK_GetPTS() - pChecker->checkTime[pinIndex]) > 4500)
		pChecker->bDropflag[pinIndex] = true;

}

bool FULLCHECK_IsDropping(FULL_CHECK_T *pChecker, int pinIndex)
{ 
	return pChecker->bDropflag[pinIndex]; 
}

void FULLCHECK_StopDrop(FULL_CHECK_T *pChecker, int pinIndex, long writableSize, long ringBufferSize)
{
	pChecker->checkTime[pinIndex] = 0;

	/* When ring buffer writableSize > 5%, reset Dropflag */
	if (pChecker->bDropflag[pinIndex] && (writableSize*100/ringBufferSize) > 5)
		pChecker->bDropflag[pinIndex] = false;

}