#include <linux/math64.h>
#include <linux/string.h>
#include <linux/kernel.h>

#include "rdvb-common/clock.h"
#include "debug.h"
#include "refclk.h"
#include "tp/tp_drv_api.h"
#include "pcrsync.h"
#define DTV_DELAY_TIME        (200*DEMUX_CLOCKS_PER_MS )
#define TIMEOUT_TO_WAIT_AUDIO (200*DEMUX_CLOCKS_PER_MS )
#define PCR_STABLE_PREIOD     (  3*DEMUX_CLOCKS_PER_SEC)
#define UNSTABLE_RCD_THRESHOLD	(500)

/*
https://harmony.lge.com:8443/issue/browse/KTASKWBS-3734
==> ALEX-TV channel in 140630_1plp_3services_ h265_h264_ac3+_tele_hbbtv_2.ts
https://harmony.lge.com:8443/issue/browse/WOSQRTK-6553
==> Eurosport channel in INDONESIA_EWS_long_message_Lv213f.ts
*/
#define AV_AUTO_FREERUN_BUFFERING_TIMEOUT (3*DEMUX_CLOCKS_PER_SEC)



int64_t PCRSYNC_GetCloser(PCRSYNC_T *pPcrProcesser, int64_t *getPts, int64_t *getPcr);
void PCRSYNC_UpdateRCDImpl(PCRSYNC_T *pPcrProcesser, NAVDTVPCRINFO pcrinfo);
void PCRSYNC_AverageRCD(PCRSYNC_T *pPcrProcesser, int64_t prevPTS, int64_t pts, NAVHACKRCD *pRCD);
bool PCRSYNC_GetHackRCD(PCRSYNC_T *pPcrProcesser, bool bFirst, NAVDTVPCRINFO *pcrinfo);
int PCRSYNC_SmoothPCR(PCRSYNC_T *pPcrProcesser , NAVDTVPCRINFO *pPcrinfo);
void PCRSYNC_HandlePCR(PCRSYNC_T *pPcrProcesser, NAVDTVPCRINFO pcrinfo);
void PCRSYNC_ResetRCDCounter(NAVHACKRCD *pRCD);
void PCRSYNC_AdjustPCR(PCRSYNC_T *pPcrProcesser, NAVDTVPCRINFO pcrinfo);
void PCRSYNC_UpdatePTSStableStatus(PCRSYNC_T *pPcrProcesser, int64_t audioPTS, int64_t videoPTS);
void PCRSYNC_DEBUG_SaveAvSyncInfo(PCRSYNC_T *pPcrProcesser, NAVDTVPCRINFO *pcrinfo);
bool PCRSYNC_ShouldEnterFreeRunMode(const PCRSYNC_T *processor);
void PCRSYNC_CheckAVFreerun( PCRSYNC_T *pPcrProcesser);

static inline int64_t pcrsync_clock_get(PCRSYNC_T *pPcrProcesser) {
	return CLOCK_GetAVSyncPTS(REFCLK_GetClockMode(pPcrProcesser->pRefClock));
}
void SWDEMUXPCR_Reset(SWDEMUXPCR* pSwPCRInfo)
{
	memset(pSwPCRInfo, 0, sizeof(SWDEMUXPCR));
	pSwPCRInfo->aPCR = -1;
	pSwPCRInfo->vPCR = -1;
	pSwPCRInfo->aPcrRef = -1;
	pSwPCRInfo->vPcrRef = -1;
	pSwPCRInfo->aPrevDemuxPTS = -1;
	pSwPCRInfo->vPrevDemuxPTS = -1;
}

void PCREXISTDETECT_Reset(PCREXISTDETECT* pPcrExistDetect)
{
	memset(pPcrExistDetect, 0, sizeof(PCREXISTDETECT));
	pPcrExistDetect->checkPcrExitEnd = false;
	pPcrExistDetect->checkPacketReady = false;
	pPcrExistDetect->oldPCR = -1;
	pPcrExistDetect->pcrCheckTimeout = -1;
	pPcrExistDetect->newPCR = -1;
}
void PCRSYNC_Init(PCRSYNC_T *pPcrProcesser, REFCLOCK *pRefClock, unsigned char tunerHandle)
{
        pPcrProcesser->pRefClock = pRefClock;
        pPcrProcesser->tunerHandle = tunerHandle;
        pPcrProcesser->pid = 0x1fff;
        PCRSYNC_Reset(pPcrProcesser);
}
 void PCRSYNC_Reset(PCRSYNC_T *pPcrProcesser)
{

	SWDEMUXPCR_Reset(&pPcrProcesser->swPCRInfo);
	PCREXISTDETECT_Reset(&pPcrProcesser->pcrExistDetect);
	//pPcrProcesser->audioType = AUDIO_UNKNOWN_TYPE;

	pPcrProcesser->pcrDefaultOffset = DTV_DELAY_TIME;
	PCRSYNC_SetFreeRunMode(pPcrProcesser, false);

	pPcrProcesser->rcd = -1;
	pPcrProcesser->stc = -1;
	pPcrProcesser->pcr = -1;
	pPcrProcesser->nextPCR = -1;
	pPcrProcesser->nextSTC = -1;
	pPcrProcesser->lastVideoSystemPTS = -1;
	pPcrProcesser->lastAudioSystemPTS = -1;
	pPcrProcesser->lastDemuxAudioPTS = -1;
	pPcrProcesser->lastDemuxVideoPTS = -1;
	pPcrProcesser->prevVideoDemuxPTS = -1;
	pPcrProcesser->prevAudioDemuxPTS = -1;
	pPcrProcesser->prevRcd = -1;

	PCRSYNC_ResetRcdStabilityCheckStatus(pPcrProcesser);

	pPcrProcesser->audioFullnessLowCount = 0;
	pPcrProcesser->audioFullnessLowCheckNum = 0;
	pPcrProcesser->lastMPTS = -1;
	pPcrProcesser->masterPassPTSFreerunEstimated = -1;

	pPcrProcesser->bCheckPcrOffset = true;
	pPcrProcesser->pcrOffset = pPcrProcesser->pcrTargetOffset = pPcrProcesser->pcrDefaultOffset;
	pPcrProcesser->pcrOffsetApproachPTS = 0;
	pPcrProcesser->waitForPcrOffsetChanged = false;
	pPcrProcesser->wrapAroundRecheckPcrOffset = false;
	pPcrProcesser->audioWrapAroundRecheckPcrOffset = false;
	pPcrProcesser->videoWrapAroundRecheckPcrOffset = false;
	pPcrProcesser->freerun_checkTimeout = -1;
	pPcrProcesser->bPcrBackTrack = false;
	pPcrProcesser->bWrapAround = false;
	LRegress_Reset(&pPcrProcesser->LRegress);

	pPcrProcesser->timeToWaitForBothAVPkt = -1;
	pPcrProcesser->startAudioPts = -1;
	pPcrProcesser->startVideoPts = -1;
	pPcrProcesser->bUseHackPCR = false;
	pPcrProcesser->bForceHackPCR = false;
	pPcrProcesser->bUseAudioPTS = false;
	PCRSYNC_ResetRCDCounter(&pPcrProcesser->audioRCD);
	PCRSYNC_ResetRCDCounter(&pPcrProcesser->videoRCD);

	PCRDEBUG_Reset(&pPcrProcesser->pcrDebugger);
	pPcrProcesser->pcrPrintPTS = -1;
	
	pPcrProcesser->audioPtsUnstableCounter = 0;
	pPcrProcesser->audioPtsStableCounter = 0;
	pPcrProcesser->videoPtsUnstableCounter = 0;
	pPcrProcesser->videoPtsStableCounter = 0;
	pPcrProcesser->avPtsUnstableCounter = 0;
	pPcrProcesser->avPtsStableCounter = 0;
	pPcrProcesser->bVideoPTSStable = true;
	pPcrProcesser->bAudioPTSStable = true;

	pPcrProcesser->noAudioPtsCounter = 0;
	pPcrProcesser->audioPtsCounter = 0;
	pPcrProcesser->bNoAudioPts = false;
	pPcrProcesser->noVideoPtsCounter = 0;
	pPcrProcesser->videoPtsCounter = 0;
	pPcrProcesser->bNoVideoPts = false;
	pPcrProcesser->bVideoFreerun=false;
	pPcrProcesser->bAudioFreerun=false;

        DEBUG_FlushSyncInfo(pPcrProcesser->tunerHandle);
        pPcrProcesser->resetPTS = CLOCK_GetPTS();
}
void PCRSYNC_UpdateRCD(PCRSYNC_T *pPcrProcesser, NAVDTVPCRINFO pcrinfo)
{
	/*For hack mode, calling sequence should be PCRSYNC_AveragePTS -> PCRSYNC_UpdateRCDImpl*/
	if (pPcrProcesser->bUseHackPCR)
		return;
	if (pPcrProcesser->bUseFixedRCD){
		return;
	}
	PCRSYNC_UpdateRCDImpl(pPcrProcesser, pcrinfo);
	PCRSYNC_CheckAVFreerun(pPcrProcesser);
}

/*
*PCRSYNC_FREERUNTimeoutCheck:
*	at beginning of play check , a/v auto freerun timeout, to buffer data
*
*	return:
*		true:  timeout, stop buffer, start update RCD
*		false:  still need to wait GPTS TIMEOUT. DO NOT UPDATE RCD
*/
bool PCRSYNC_FREERUNTimeoutCheck(PCRSYNC_T *pPcrProcesser, NAVDTVPCRINFO pcrinfo)
{
	static  int64_t prePCR = -1;
	int64_t RCD = 0 , currPTS = 0;
	bool 	bVCheck = false, bACheck = false;

	RCD = REFCLK_GetRCD(pPcrProcesser->pRefClock);
	if( (pPcrProcesser->lastDemuxVideoPTS == -1)||(pPcrProcesser->lastDemuxVideoPTS - pcrinfo.pcr > pPcrProcesser->v_fr_thrhd))
		bVCheck = true;
	if( (pPcrProcesser->lastDemuxAudioPTS == -1) ||(pPcrProcesser->lastDemuxAudioPTS - pcrinfo.pcr > DEFAULT_FREE_RUN_AUDIO_THRESHOLD))
		bACheck = true;
	//donot updatepcr,in freerunmod, utill, gptstimeout
	if ( (RCD == -1) )
	{
		/*
		audio	video																				result
		v		v																						v
		x		v		AUDIO_ONLY																		x
						NOT_AUDIO_ONLY	videopts==-1		pPcrProcesser->startAudioPts < pcrinfo.pcr	x
															pPcrProcesser->startAudioPts > pcrinfo.pcr	v
										videopts!=-1													v
		v		x		VIDEO_ONLY																		x
						NOT_VIDEO_ONLY	Audio_pts==-1		pPcrProcesser->startVideoPts < pcrinfo.pcr	x
															pPcrProcesser->startVideoPts > pcrinfo.pcr	v
										Audio_pts!=-1													x
		x		x																						x
		*/
		if (pPcrProcesser->freerun_checkTimeout == -1){
			pPcrProcesser->freerun_checkTimeout = AV_AUTO_FREERUN_BUFFERING_TIMEOUT +pcrsync_clock_get(pPcrProcesser);
			prePCR = pcrinfo .pcr;
			dmx_notice(pPcrProcesser->tunerHandle,"freerun startcheck timeout: %llx\n", pPcrProcesser->freerun_checkTimeout);
		}
		currPTS = pcrsync_clock_get(pPcrProcesser);

		

		if (prePCR  == pcrinfo.pcr /*in case of  error pcr, (pcr not update ,pcr belong to previous channel)*/
			||(bVCheck && bACheck)
			|| ((bVCheck && !bACheck) && ( ( (pPcrProcesser->lastDemuxVideoPTS == -1) && (pPcrProcesser->startAudioPts > pcrinfo.pcr/*(pPcrProcesser->lastDemuxAudioPTS - pPcrProcesser->startAudioPts)<200*90*/))
											||(pPcrProcesser->lastDemuxVideoPTS != -1)))
			|| ((!bVCheck && bACheck) && (( (pPcrProcesser->lastDemuxAudioPTS == -1) && (pPcrProcesser->startVideoPts > pcrinfo.pcr/*pPcrProcesser->lastDemuxVideoPTS - pPcrProcesser->startVideoPts)<200*90 */)))
				)
			)
		{
			if (currPTS < pPcrProcesser->freerun_checkTimeout  ) {
			/*	dmx_emerg("RCD not deliver:check-A/V:%d/%d pcr:%llx pts-A/V: %llx/%llx ,thresthold-A/V:%x/%x,cur/timeout: %llx/%llx\n",
				bACheck, bVCheck,
				pcrinfo.pcr, pPcrProcesser->lastDemuxAudioPTS,pPcrProcesser->lastDemuxVideoPTS,
				DEFAULT_FREE_RUN_AUDIO_THRESHOLD, pPcrProcesser->v_fr_thrhd ,
				currPTS, pPcrProcesser->freerun_checkTimeout); */
				return false;
			}
			else
				dmx_notice(pPcrProcesser->tunerHandle,"RCD  deliver(timeout):check-A/V:%d/%d, pcr:%lld, pts-A/V: %lld/%lld, thresthold-A/V:%d/%ld, cur/timeout:%lld/%lld\n",
					bACheck, bVCheck,
					pcrinfo.pcr, pPcrProcesser->lastDemuxAudioPTS,pPcrProcesser->lastDemuxVideoPTS,
					DEFAULT_FREE_RUN_AUDIO_THRESHOLD, pPcrProcesser->v_fr_thrhd ,
					currPTS, pPcrProcesser->freerun_checkTimeout);
		}
		else
			dmx_notice(pPcrProcesser->tunerHandle,"ch[%d] RCD  deliver:check-A/V:%d/%d, pcr:%lld, pts-A/V: %lld/%lld, thresthold-A/V:%d/%ld, cur/timeout: %lld/%lld\n",
					(int)pPcrProcesser->tunerHandle, bACheck, bVCheck,
					pcrinfo.pcr, pPcrProcesser->lastDemuxAudioPTS,pPcrProcesser->lastDemuxVideoPTS,
					DEFAULT_FREE_RUN_AUDIO_THRESHOLD, pPcrProcesser->v_fr_thrhd ,
					currPTS, pPcrProcesser->freerun_checkTimeout);

		pPcrProcesser->freerun_checkTimeout = -1;
	}
	return true;
}
void PCRSYNC_UpdateRCDImpl(PCRSYNC_T *pPcrProcesser, NAVDTVPCRINFO pcrinfo)
{
	enum {
		RET_INIT = 0,
		NO_PCR_DURING_ONE_SEC_AFTER_BUFFERING_END,
		TIME_S_OUT_PCR_COMES_FROM_AVERAGED_PTS,
		PCR_AND_STC_WAS_THE_SAME,
		FAILED_TO_INSERT_PCR_TO_HISTORY_WHICH_MAY_CAUSED_FROM_STC_STC_BACKTRACK,
		FREERUN_TIMEOUT,
		CLK_CHANGED_DO_NOT_GET_OLD_STC_PCR,
	};
	int iRet;
	int64_t tmpPCR, tmpSTC, currPTS;
	PRESENTATION_POSITIONS position;
	bool wrapAroundRecheckPcrOffset = pPcrProcesser->wrapAroundRecheckPcrOffset;
	tmpPCR = pPcrProcesser->nextPCR;
	pPcrProcesser->nextPCR = pcrinfo.pcr;
	pcrinfo.pcr = tmpPCR;

	tmpSTC = pPcrProcesser->nextSTC;
	pPcrProcesser->nextSTC = pcrinfo.stc;
	pcrinfo.stc = tmpSTC;

	iRet = RET_INIT;

	if (pcrinfo.stc == 0 || pcrinfo.stc == -1 || pcrinfo.pcr == -1 || pPcrProcesser->bForceHackPCR) {
		/*pcr is not available now. wait a minute.*/

		int64_t GPTS = REFCLK_GetGPTSTimeout(pPcrProcesser->pRefClock);
		int64_t RCD = REFCLK_GetRCD(pPcrProcesser->pRefClock);
		if (pcrsync_clock_get(pPcrProcesser) < (GPTS) /*|| RCD != -1*/) { /*no pcr during one sec after buffering end*/
			iRet = NO_PCR_DURING_ONE_SEC_AFTER_BUFFERING_END;
			goto RET_PCRSYNC_UPDATE_RCD_IMPL;
		}

		/*case 1: time's out. pcr comes from averaged PTS*/
		if (!PCRSYNC_GetHackRCD(pPcrProcesser, RCD == -1, &pcrinfo)) {
			iRet = TIME_S_OUT_PCR_COMES_FROM_AVERAGED_PTS;
			goto RET_PCRSYNC_UPDATE_RCD_IMPL;
		}

	} else {
		/*case 2: pcr comes from TP*/
		if (pPcrProcesser->bUseHackPCR && !pPcrProcesser->bForceHackPCR) {
			/*pcr is available now, so disable hack mode.*/

			pPcrProcesser->bUseHackPCR = false;
			PCRSYNC_ResetRCDCounter(&pPcrProcesser->audioRCD);
			PCRSYNC_ResetRCDCounter(&pPcrProcesser->videoRCD);
			pPcrProcesser->bUseAudioPTS = false;
			PCRDEBUG_Reset(&pPcrProcesser->pcrDebugger);
			dmx_notice(pPcrProcesser->tunerHandle,"get pcr info from TP\n");
		}
		
		if (PCRSYNC_FREERUNTimeoutCheck(pPcrProcesser, pcrinfo) == false) {
			iRet = FREERUN_TIMEOUT;
			goto RET_PCRSYNC_UPDATE_RCD_IMPL;
		}
	}

	if (pPcrProcesser->stc == pcrinfo.stc || pPcrProcesser->pcr == pcrinfo.pcr) {
		iRet = PCR_AND_STC_WAS_THE_SAME;
		goto RET_PCRSYNC_UPDATE_RCD_IMPL;
	}
	/*Fail to insert pcr to history which may caused from stc backtrack.*/
	if (PCRDEBUG_Insert(&pPcrProcesser->pcrDebugger, pcrinfo.pcr, pcrinfo.stc, pPcrProcesser->lastDemuxAudioPTS, pPcrProcesser->lastDemuxVideoPTS) == 0) {
		iRet = FAILED_TO_INSERT_PCR_TO_HISTORY_WHICH_MAY_CAUSED_FROM_STC_STC_BACKTRACK;
		goto RET_PCRSYNC_UPDATE_RCD_IMPL;
	}


	/* Might be predict next pcr.  */
	if (PCRSYNC_SmoothPCR(pPcrProcesser,&pcrinfo)<0){
		iRet = PCR_AND_STC_WAS_THE_SAME;
		goto RET_PCRSYNC_UPDATE_RCD_IMPL;
	}

	REFCLK_GetPresentationPositions(pPcrProcesser->pRefClock, &position);

	pPcrProcesser->lastAudioSystemPTS = position.audioSystemPTS;
	pPcrProcesser->lastVideoSystemPTS = position.videoSystemPTS;

	currPTS = CLOCK_GetPTS();
	if((currPTS - pPcrProcesser->resetPTS) > 2*DEMUX_CLOCKS_PER_SEC)
	{
		long videoUnderflow = 0, audioUnderflow = 0;
		REFCLK_CheckUnderflow(pPcrProcesser->pRefClock, &videoUnderflow, &audioUnderflow);

		if(pPcrProcesser->lastDemuxAudioPTS != -1 && pPcrProcesser->lastAudioSystemPTS != -1 && pPcrProcesser->bNoAudioPts == false)
		{
			if(audioUnderflow != 0)
				pPcrProcesser->audioFullnessLowCount++;
			if(pPcrProcesser->audioFullnessLowCount)
				pPcrProcesser->audioFullnessLowCheckNum++;

			if(pPcrProcesser->audioFullnessLowCount > 100)
			{
				pPcrProcesser->bCheckPcrOffset = true;
				dmx_notice(pPcrProcesser->tunerHandle,"audioFullnessLowCheck: count=%lld, CheckNum=%lld\n",
				pPcrProcesser->audioFullnessLowCount, pPcrProcesser->audioFullnessLowCheckNum);
			}

			if(pPcrProcesser->bCheckPcrOffset == true || pPcrProcesser->audioFullnessLowCheckNum > 300)
			{
				pPcrProcesser->audioFullnessLowCount = 0;
				pPcrProcesser->audioFullnessLowCheckNum = 0;
			}
		}

		PCRSYNC_EvaluateRcdStability(
			pPcrProcesser,
			pcrinfo.pcr,
			pcrinfo.stc
		);

		if (PCRSYNC_ShouldEnterFreeRunMode(pPcrProcesser))
		{
			int64_t unstableCount, unstableCheck;
			dmx_crit(pPcrProcesser->tunerHandle, "RCD unstable Enter freerun Mode \n");
			PCRSYNC_EnterFreeRunMode(pPcrProcesser);

			PCRSYNC_GetRCDUnstableStatus(
				pPcrProcesser,
				&unstableCount,
				&unstableCheck
			);

			dmx_notice(pPcrProcesser->tunerHandle,
				"rcdUnstableCheck: count=%lld, CheckNum=%lld\n",
				unstableCount, unstableCheck
			);
		}

		if (PCRSYNC_ShouldResetRcdStabilityCheckStatus(pPcrProcesser))
		{
			PCRSYNC_ResetRcdStabilityCheckStatus(pPcrProcesser);
		}
	}
	else if((currPTS - pPcrProcesser->resetPTS) < 0)
		pPcrProcesser->resetPTS = CLOCK_GetPTS();

	if((currPTS - pPcrProcesser->resetPTS) > DEMUX_CLOCKS_PER_SEC)
		PCRSYNC_AdjustPCR(pPcrProcesser, pcrinfo);

	if((wrapAroundRecheckPcrOffset == true) && (pPcrProcesser->wrapAroundRecheckPcrOffset == false))
	{
		pPcrProcesser->audioFullnessLowCount = 0;
		pPcrProcesser->audioFullnessLowCheckNum = 0;
		PCRSYNC_ResetRcdStabilityCheckStatus(pPcrProcesser);

		pPcrProcesser->resetPTS = CLOCK_GetPTS();
	}

	PCRSYNC_DEBUG_SaveAvSyncInfo(pPcrProcesser, &pcrinfo);

	PCRSYNC_HandlePCR(pPcrProcesser, pcrinfo);

 RET_PCRSYNC_UPDATE_RCD_IMPL:
	if( iRet > RET_INIT ) {
		dmx_mask_print(DEMUX_PCRSYC_DEBUG_EX1, pPcrProcesser->tunerHandle, "\033[1;36m iRet=%d\033[m\n",iRet );
	}
}
void PCRSYNC_HandlePCR(PCRSYNC_T *pPcrProcesser, NAVDTVPCRINFO pcrinfo)
{
	enum {
		RET_INIT = 0,
		STC_WAS_THE_SAME,
		PCR_JUMP_OR_DROPS_TOO_MUCH,
	};

	int64_t RCD, pts, mpts;
	int64_t currentTime  , passedTime ,  raisedPcr, raisedStc;
	bool    isNotPrintYet, isPcrIncreaseTooQuickly; 
	int iRet = RET_INIT;

	if (pPcrProcesser->stc == pcrinfo.stc) {
		iRet = STC_WAS_THE_SAME;
		goto RET_PCRSYNC_HANDLE_PCR;
	}

	pts  = pcrsync_clock_get(pPcrProcesser);
	RCD  = pcrinfo.pcr - pcrinfo.stc - pPcrProcesser->pcrOffset;
	mpts = RCD+pts;

	currentTime   = pts;
	passedTime    = abs64(currentTime - pPcrProcesser->pcrPrintPTS);
	isNotPrintYet = (pPcrProcesser->pcrPrintPTS == -1);

	/* https://realtektv.realtek.com/view.php?id=97246*/
	/* pcr jumps*/
	raisedPcr = abs64(pcrinfo.pcr - pPcrProcesser->pcr);
	raisedStc = abs64(pcrinfo.stc - pPcrProcesser->stc);
	isPcrIncreaseTooQuickly = (30000 < raisedPcr && raisedStc < 6000);

	if (isPcrIncreaseTooQuickly) {
		if (isNotPrintYet || DEMUX_CLOCKS_PER_SEC <= passedTime) {
			dmx_mask_print(DEMUX_PCRSYC_DEBUG|DEMUX_NOTICE_PRINT, pPcrProcesser->tunerHandle, "PCR jumps/drops too much (1):pcr=%lld, prev_pcr=%lld, stc=%lld, prev_stc=%lld\n", pcrinfo.pcr,pPcrProcesser->pcr,pcrinfo.stc,pPcrProcesser->stc);
			pPcrProcesser->pcrPrintPTS = currentTime;
		}
		LRegress_PopDataPair(&pPcrProcesser->LRegress);
		iRet = PCR_JUMP_OR_DROPS_TOO_MUCH;
		goto RET_PCRSYNC_HANDLE_PCR;
	}

	REFCLK_SetRCD(pPcrProcesser->pRefClock, RCD);

	if (isNotPrintYet || DEMUX_CLOCKS_PER_SEC <= passedTime) {
		int64_t unstableCount, unstableCheck;

		const long clk_src = REFCLK_GetClockMode(pPcrProcesser->pRefClock);

		dmx_mask_print(DEMUX_PCRSYC_DEBUG, pPcrProcesser->tunerHandle, "[dtv(%d)]set pcr=%lld,stc=%lld,RCD=%lld,MasterPTS=%lld,aDemuxPTS=%lld,vDexmuxPTS=%lld,offset=%lld (%lld),pll=%lld,clk_src=%ld\n",
			pPcrProcesser->tunerHandle, pcrinfo.pcr, pcrinfo.stc, RCD, mpts, pPcrProcesser->lastDemuxAudioPTS, pPcrProcesser->lastDemuxVideoPTS, pPcrProcesser->pcrOffset, pPcrProcesser->pcrDefaultOffset, pts, clk_src);

		dmx_mask_print(DEMUX_PCRSYC_DEBUG_EX1, pPcrProcesser->tunerHandle, "audioFullnessLowCheck: count=%lld, CheckNum=%lld\n", 			pPcrProcesser->audioFullnessLowCount, pPcrProcesser->audioFullnessLowCheckNum);

		dmx_mask_print(DEMUX_PCRSYC_DEBUG_EX1, pPcrProcesser->tunerHandle, "NoAudioPtsCheck=%d (%d, %d), NoVideoPtsCheck=%d (%d, %d)\n", 			pPcrProcesser->bNoAudioPts, pPcrProcesser->noAudioPtsCounter, pPcrProcesser->audioPtsCounter,
			pPcrProcesser->bNoVideoPts, pPcrProcesser->noVideoPtsCounter, pPcrProcesser->videoPtsCounter);

		PCRSYNC_GetRCDUnstableStatus(
			pPcrProcesser,
			&unstableCount,
			&unstableCheck
		);

		dmx_mask_print(DEMUX_PCRSYC_DEBUG_EX1, pPcrProcesser->tunerHandle, 
			"rcdUnstableCheck: count=%lld, CheckNum=%lld\n",
			unstableCount, unstableCheck
		);

		pPcrProcesser->pcrPrintPTS = currentTime;
	}

	pPcrProcesser->prevRcd = pPcrProcesser->rcd;
	pPcrProcesser->rcd = RCD;
	pPcrProcesser->lastMPTS = mpts;

	pPcrProcesser->stc = pcrinfo.stc;
	pPcrProcesser->pcr = pcrinfo.pcr;
	pPcrProcesser->prevVideoDemuxPTS = pPcrProcesser->lastDemuxVideoPTS;
	pPcrProcesser->prevAudioDemuxPTS = pPcrProcesser->lastDemuxAudioPTS;

RET_PCRSYNC_HANDLE_PCR:
	if( iRet > RET_INIT && iRet != STC_WAS_THE_SAME) {
		dmx_mask_print(DEMUX_PCRSYC_DEBUG_EX1, pPcrProcesser->tunerHandle, "\033[1;36m Ret=%d\033[m\n", iRet );
	}


}

int  PCRSYNC_SmoothPCR(PCRSYNC_T *pPcrProcesser , NAVDTVPCRINFO *pPcrinfo)
{
	static int count =0;
    if (pPcrProcesser->stc != pPcrinfo->stc && pPcrProcesser->pcr != pPcrinfo->pcr)
    {
		int64_t tmp_pcr;
		/* Reset when got abnormal Pcr.  */
        if( (pPcrinfo->pcr > 0 && pPcrProcesser->pcr > 0 && abs64(pPcrinfo->pcr - pPcrProcesser->pcr) > 180000) ||
			(pPcrinfo->stc > 0 && pPcrProcesser->stc > 0 && pPcrinfo->stc < pPcrProcesser->stc)) {
			dmx_mask_print(DEMUX_PCRSYC_DEBUG|DEMUX_NOTICE_PRINT, pPcrProcesser->tunerHandle, "\033[1;36m  LRegress_Reset() pcr:%lld, pre_pcr:%lld\033[m\n", pPcrinfo->pcr, pPcrProcesser->pcr);
			LRegress_Reset(&pPcrProcesser->LRegress);
			count  = 0;
        }
        if (count<10)
		{
			dmx_mask_print(DEMUX_LR_DEBUG, pPcrProcesser->tunerHandle, " [LRegress] count: %d, pcr:%lld, pre_pcr:%lld, stc:%lld, pre_stc:%lld\n",count,pPcrinfo->pcr, pPcrProcesser->pcr, pPcrinfo->stc, pPcrProcesser->stc);
			count ++;
		}
		tmp_pcr = LRegress_GetPredictY(&pPcrProcesser->LRegress,pPcrinfo->stc);

		/*dmx_dbg("\033[1;36m [%s %d]URCD[%lld, %lld, %lld](%lld) \033[m\n",__func__ , __LINE__ , pPcrinfo->pcr , pPcrinfo->stc, tmp_pcr , pPcrinfo->pcr-tmp_pcr); */

		if(pPcrinfo->stc > 0 && pPcrProcesser->stc > 0 && pPcrinfo->stc != pPcrProcesser->stc)
		{
			LINEARPAIR pair;
			pair.x = pPcrinfo->stc;
			pair.y = pPcrinfo->pcr;
			if (LRegress_InsertDataPair(&pPcrProcesser->LRegress,pair) <0)
				return -1;
		}

		if(pPcrinfo->pcr > 0 && tmp_pcr > 0 && abs64(pPcrinfo->pcr - tmp_pcr) > 100) {
			pPcrinfo->pcr = tmp_pcr;
		}
	}
	return 0;

}

void PCRSYNC_AdjustPCR(PCRSYNC_T *pPcrProcesser, NAVDTVPCRINFO pcrinfo)
{
	//SWDEMUXPCR* pSwPCRInfo = &pPcrProcesser->swPCRInfo;

	if (pPcrProcesser->bCheckPcrOffset)
	{
		if (pcrinfo.pcr > 0 && pPcrProcesser->pcr > 0 && (pPcrProcesser->lastDemuxAudioPTS != -1 || pPcrProcesser->lastDemuxVideoPTS != -1))
		{
			pPcrProcesser->bCheckPcrOffset = false;
			pPcrProcesser->wrapAroundRecheckPcrOffset = true;
			pPcrProcesser->audioWrapAroundRecheckPcrOffset = true;
			pPcrProcesser->videoWrapAroundRecheckPcrOffset = true;
		}
	}
	else
	{
		if (pcrinfo.pcr > 0 && pPcrProcesser->pcr > 0 && (pPcrProcesser->lastDemuxAudioPTS != -1 || pPcrProcesser->lastDemuxVideoPTS != -1))
		{
			if(((pPcrProcesser->pcr - pcrinfo.pcr) > 2*DEMUX_CLOCKS_PER_SEC)
			   || (pPcrProcesser->bNoAudioPts == false && pPcrProcesser->lastDemuxAudioPTS != -1 && pPcrProcesser->prevAudioDemuxPTS != -1 && (pPcrProcesser->prevAudioDemuxPTS- pPcrProcesser->lastDemuxAudioPTS ) > 2*DEMUX_CLOCKS_PER_SEC )
			   || (pPcrProcesser->bNoVideoPts == false && pPcrProcesser->lastDemuxVideoPTS != -1 && pPcrProcesser->prevVideoDemuxPTS != -1 && (pPcrProcesser->prevVideoDemuxPTS- pPcrProcesser->lastDemuxVideoPTS ) > 2*DEMUX_CLOCKS_PER_SEC ) )
			{
				pPcrProcesser->bWrapAround = false;
				if((pPcrProcesser->bNoAudioPts == false)
					&& (pPcrProcesser->lastAudioSystemPTS > pPcrProcesser->lastDemuxAudioPTS)
					&& (pPcrProcesser->lastAudioSystemPTS - pPcrProcesser->lastDemuxAudioPTS) < DEFAULT_FREE_RUN_THRESHOLD)
						REFCLK_SetAudioFreeRunThreshold(pPcrProcesser->pRefClock, 0, 0);
				if((pPcrProcesser->bNoVideoPts == false)
					&& (pPcrProcesser->lastVideoSystemPTS > pPcrProcesser->lastDemuxVideoPTS)
					&& (pPcrProcesser->lastVideoSystemPTS - pPcrProcesser->lastDemuxVideoPTS) < pPcrProcesser->v_fr_thrhd)
						REFCLK_SetVideoFreeRunThreshold(pPcrProcesser->pRefClock, 0);
				pPcrProcesser->wrapAroundRecheckPcrOffset = true;
				pPcrProcesser->audioWrapAroundRecheckPcrOffset = true;
				pPcrProcesser->videoWrapAroundRecheckPcrOffset = true;
				pPcrProcesser->pcrPrintPTS = -1;
				dmx_crit(pPcrProcesser->tunerHandle,"stream warp around, pcr:%lld/%lld, vpts:%lld/%lld, apts:%lld/%lld\n", 
					pcrinfo.pcr, pcrinfo.pcr, pPcrProcesser->lastDemuxVideoPTS, pPcrProcesser->prevVideoDemuxPTS, pPcrProcesser->lastDemuxAudioPTS, pPcrProcesser->prevAudioDemuxPTS);
			}
			else if(pPcrProcesser->wrapAroundRecheckPcrOffset == true)
			{
				int64_t pts, pcr;
				if (((pPcrProcesser->bNoAudioPts == true) || ((pPcrProcesser->lastDemuxAudioPTS != -1) && (pPcrProcesser->lastAudioSystemPTS != -1) && (pPcrProcesser->lastDemuxAudioPTS >= pPcrProcesser->lastAudioSystemPTS)))
				&& ((pPcrProcesser->bNoVideoPts == true)  || ((pPcrProcesser->lastDemuxVideoPTS != -1) && (pPcrProcesser->lastVideoSystemPTS != -1) && (pPcrProcesser->lastDemuxVideoPTS >= pPcrProcesser->lastVideoSystemPTS))))
				{
					REFCLK_SetAudioFreeRunThreshold(pPcrProcesser->pRefClock, DEFAULT_FREE_RUN_AUDIO_THRESHOLD, DEFAULT_FREE_RUN_AUDIO_THRESHOLD);
					REFCLK_SetVideoFreeRunThreshold(pPcrProcesser->pRefClock, pPcrProcesser->v_fr_thrhd);
				}

				pts = -1, pcr = -1;
				PCRSYNC_GetCloser(pPcrProcesser, &pts, &pcr);
				if(pts != -1 && pcr != -1)
				{
					//int64_t lastDemuxAudioPTS = pSwPCRInfo->aPrevDemuxPTS;
					//int64_t lastDemuxVideoPTS = pSwPCRInfo->vPrevDemuxPTS;

					if (((pPcrProcesser->bNoAudioPts == true) || (pPcrProcesser->lastDemuxAudioPTS >= pPcrProcesser->lastAudioSystemPTS))
					&& ((pPcrProcesser->bNoVideoPts == true) || (pPcrProcesser->lastDemuxVideoPTS >= pPcrProcesser->lastVideoSystemPTS)))
					{
						pPcrProcesser->wrapAroundRecheckPcrOffset = false;
						pPcrProcesser->audioWrapAroundRecheckPcrOffset = false;
						pPcrProcesser->videoWrapAroundRecheckPcrOffset = false;
					}

					if(pPcrProcesser->audioWrapAroundRecheckPcrOffset == false || pPcrProcesser->videoWrapAroundRecheckPcrOffset == false)
					{
						if ((pcr - pts) <= 10*DEMUX_CLOCKS_PER_SEC)
						{
							pPcrProcesser->pcrDefaultOffset = DTV_DELAY_TIME;
							pPcrProcesser->pcrTargetOffset  = pPcrProcesser->pcrDefaultOffset;
							pPcrProcesser->pcrOffset        = pPcrProcesser->pcrTargetOffset;
							//dmx_notice(pPcrProcesser->tunerHandle,"\033[1;33mreset pcrTargetOffset = %lld, pts = %lld, pcr = %lld, apts = %lld, vpts = %lld\033[m\n", pPcrProcesser->pcrTargetOffset, pts, pcr, lastDemuxAudioPTS, lastDemuxVideoPTS);
						}
						else
						{
							pPcrProcesser->pcrDefaultOffset = 2*DTV_DELAY_TIME;
							pPcrProcesser->pcrTargetOffset = pcr - pts + pPcrProcesser->pcrDefaultOffset;
							pPcrProcesser->pcrOffset = pPcrProcesser->pcrTargetOffset;
							//dmx_notice(pPcrProcesser->tunerHandle,"\033[1;33m reset pcrTargetOffset = %lld, pts = %lld, pcr = %lld, apts = %lld, vpts = %lld\033[m\n",  pPcrProcesser->pcrTargetOffset, pts, pcr, lastDemuxAudioPTS, lastDemuxVideoPTS);
						}
					}
				}
			}
		}
	}
}
void PCRSYNC_ResetRCDCounter(NAVHACKRCD *pRCD)
{
	pRCD->lastPCR = -1;
	pRCD->lastSTC = -1;
	pRCD->PCR = 0;
	pRCD->STC = 0;
	pRCD->counter = 0;
	pRCD->firstSamplePTS = -1;
}


void PCRSYNC_AverageRCD(PCRSYNC_T *pPcrProcesser, int64_t prevPTS, int64_t pts, NAVHACKRCD *pRCD)
{
	NAVDTVPCRINFO pcrinfo;
	bool bPTSBackTrack = (prevPTS != -1 && (prevPTS-pts) >= 10*DEMUX_CLOCKS_PER_SEC);

	bool bUpdate = false;

	int64_t RCD = REFCLK_GetRCD(pPcrProcesser->pRefClock);
	int64_t masterPTS = RCD + pcrsync_clock_get(pPcrProcesser);
	if (pPcrProcesser->bUseAudioPTS) {
		if (RCD != -1 && pPcrProcesser->lastDemuxAudioPTS != -1) {
			bUpdate = ((pPcrProcesser->lastDemuxAudioPTS - masterPTS) < pPcrProcesser->pcrOffset);
		}
	} else {
		if (RCD != -1 && pPcrProcesser->lastDemuxVideoPTS != -1) {
			bUpdate = ((pPcrProcesser->lastDemuxVideoPTS - masterPTS) < pPcrProcesser->pcrOffset);
		}
	}

	/*UpdateRCD whenever
	//1. pts back track or
	//2. every 128 pts or
	//3. every 100 ms or
	//4. bUpdate = true which is just a way to make RCD be updated frequently*/
	if ((pRCD->counter > 0) && (bPTSBackTrack || bUpdate || pRCD->counter == 128 || (CLOCK_GetPTS()-pRCD->firstSamplePTS) >= 100*DEMUX_CLOCKS_PER_MS)) {
		pRCD->PCR = div_s64(pRCD->PCR, pRCD->counter);
		pRCD->STC = div_s64(pRCD->STC, pRCD->counter);
		/*printf("avg pcr = %lld, stc= %lld, RCD = %lld, time = %lld, m_bUseAudioPTS = %d\n", pRCD->PCR, pRCD->STC, pRCD->PCR-pRCD->STC-m_delayTime, CLOCK_GetPTS(), m_bUseAudioPTS);*/
		pRCD->lastPCR = pRCD->PCR;

		/*to fix http://realtektv.realtek.com/view.php?id=64706*/
		/*It seems audio pts comes every 300ms*/
		pRCD->lastSTC = ((pcrsync_clock_get(pPcrProcesser) - pRCD->STC) > 100*DEMUX_CLOCKS_PER_MS) ? pcrsync_clock_get(pPcrProcesser) : pRCD->STC;
		pRCD->PCR = 0;
		pRCD->STC = 0;
		pRCD->counter = 0;
		pRCD->firstSamplePTS = CLOCK_GetPTS();

		memset(&pcrinfo, 0, sizeof(NAVDTVPCRINFO));
		PCRSYNC_UpdateRCDImpl(pPcrProcesser, pcrinfo);

	}

	pRCD->PCR += pts;
	pRCD->STC += pcrsync_clock_get(pPcrProcesser);
	pRCD->counter++;

	if (bPTSBackTrack) {
		pRCD->lastPCR = pts;
		pRCD->lastSTC = pcrsync_clock_get(pPcrProcesser);

		memset(&pcrinfo, 0, sizeof(NAVDTVPCRINFO));
		PCRSYNC_UpdateRCDImpl(pPcrProcesser, pcrinfo);
	}
}
/*No PCR sample : \\172.21.1.219\Public\TVQC_ADB\DTV_Stream\ISDB\2013¦~«?\Field Trial\20130507-0527_TPV_Field Trial_Brazil_Eric\20130507-0511_SaoPaulo\Air DTV singal\ErrorStream\20130508_Barzil_SaoPola_CH33.2_TOP TV SD_587MHz.mpg*/
bool PCRSYNC_GetHackRCD(PCRSYNC_T *pPcrProcesser, bool bFirst, NAVDTVPCRINFO *pcrinfo)
{
	memset(pcrinfo, 0, sizeof(NAVDTVPCRINFO));

	if (bFirst) {
		/*It is better to use audio averaged pts because video pts may cause RCD jump.*/
		/*If audio packet comes late, wait for him.*/
		if (pPcrProcesser->startAudioPts == -1) {
			if (pPcrProcesser->timeToWaitForBothAVPkt == -1) {
				pPcrProcesser->timeToWaitForBothAVPkt = CLOCK_GetPTS();
				return false;
			} else if ((CLOCK_GetPTS() - pPcrProcesser->timeToWaitForBothAVPkt) <= TIMEOUT_TO_WAIT_AUDIO)
				return false;
		}
		if (pPcrProcesser->startAudioPts != -1 || pPcrProcesser->startVideoPts != -1) {
			pPcrProcesser->videoRCD.lastPCR = pPcrProcesser->startVideoPts;
			pPcrProcesser->audioRCD.lastPCR = pPcrProcesser->startAudioPts;

			if (pPcrProcesser->startAudioPts == -1) {
				pcrinfo->pcr = pPcrProcesser->startVideoPts; pPcrProcesser->bUseAudioPTS = false;
			} else if (pPcrProcesser->startVideoPts == -1) {
				pcrinfo->pcr = pPcrProcesser->startAudioPts; pPcrProcesser->bUseAudioPTS = true;
			} else if (pPcrProcesser->startAudioPts != -1) {
				pcrinfo->pcr = pPcrProcesser->startAudioPts;
				pPcrProcesser->bUseAudioPTS = true;
			} else {
				pcrinfo->pcr = pPcrProcesser->startVideoPts;
				pPcrProcesser->bUseAudioPTS = false;
			}

			pcrinfo->stc = pcrsync_clock_get(pPcrProcesser);
			pPcrProcesser->bUseHackPCR = true;
			pPcrProcesser->audioRCD.firstSamplePTS = pcrinfo->stc;
			pPcrProcesser->videoRCD.firstSamplePTS = pcrinfo->stc;
			return true;

		}
	} else {/*if((pcrsync_clock_get(pPcrProcesser) - stc) >= DEMUX_CLOCKS_PER_SEC)*/

		if (pPcrProcesser->bUseAudioPTS) {
			pcrinfo->pcr = pPcrProcesser->audioRCD.lastPCR;
			pcrinfo->stc = pPcrProcesser->audioRCD.lastSTC;
		} else {
			pcrinfo->pcr = pPcrProcesser->videoRCD.lastPCR;
			pcrinfo->stc = pPcrProcesser->videoRCD.lastSTC;
		}

		/* no need to do further modified to pcr*/
		return true;
	}

	return false;
}

void PCRSYNC_UpdatePTSStableStatus(PCRSYNC_T *pPcrProcesser, int64_t audioPTS, int64_t videoPTS)
{
	if (pPcrProcesser->lastDemuxAudioPTS != -1 && audioPTS != -1) {
		int64_t curr_adiff = audioPTS - pPcrProcesser->lastDemuxAudioPTS;

		pPcrProcesser->bAudioPTSStable = abs64(curr_adiff) < 3*DEMUX_CLOCKS_PER_SEC;
		if (pPcrProcesser->bAudioPTSStable) {
			pPcrProcesser->audioPtsStableCounter++;
			/*m_audioPtsStableCheckStart = -1;*/
		} else
			pPcrProcesser->audioPtsUnstableCounter++;

		if (pPcrProcesser->lastDemuxVideoPTS != -1) {
			bool bAVPTSStable =  (abs64(audioPTS-pPcrProcesser->lastDemuxVideoPTS) < 10*DEMUX_CLOCKS_PER_SEC);
			if (bAVPTSStable)
				pPcrProcesser->avPtsStableCounter++;
			else
				pPcrProcesser->avPtsUnstableCounter++;
		}
	}

	if (pPcrProcesser->lastDemuxVideoPTS != -1 && videoPTS != -1) {
		int64_t curr_vdiff = videoPTS - pPcrProcesser->lastDemuxVideoPTS;


		/*video pts is sent at decode order, so we have use absolute value*/
		pPcrProcesser->bVideoPTSStable = abs64(curr_vdiff) < 3*DEMUX_CLOCKS_PER_SEC;
		if (pPcrProcesser->bVideoPTSStable) {
			pPcrProcesser->videoPtsStableCounter++;
			/*m_videoPtsStableCheckStart = -1;*/
		} else
			pPcrProcesser->videoPtsUnstableCounter++;

		if (pPcrProcesser->lastDemuxAudioPTS != -1) {
			bool bAVPTSStable =  (abs64(videoPTS-pPcrProcesser->lastDemuxAudioPTS) < 10*DEMUX_CLOCKS_PER_SEC);
			if (bAVPTSStable)
				pPcrProcesser->avPtsStableCounter++;
			else
				pPcrProcesser->avPtsUnstableCounter++;
		}
	}
}

bool PCRSYNC_GetRCDUnstableStatus(const PCRSYNC_T *pPcrProcesser, int64_t *rcdUnstableCount, int64_t *rcdUnstableCheckCount)
{
	if (pPcrProcesser->rcd == -1)
		return false;

	if (rcdUnstableCount)
		*rcdUnstableCount = pPcrProcesser->rcdUnstableCount;

	if (rcdUnstableCheckCount)
		*rcdUnstableCheckCount = pPcrProcesser->rcdUnstableCheckCount;

	return true;
}

int64_t PCRSYNC_GetCloser(PCRSYNC_T *pPcrProcesser, int64_t *getPts, int64_t *getPcr)
{
	SWDEMUXPCR* pSwPCRInfo = NULL;
	int pick = 0;
	int64_t pts = -1;
	int64_t pcr = -1, lastDemuxAudioPTS = -1, lastDemuxVideoPTS = -1;
	if ((pPcrProcesser == NULL) || (getPts == NULL) || (getPcr == NULL))
		return -1;

	pSwPCRInfo = &pPcrProcesser->swPCRInfo;
	if((pSwPCRInfo->aPcrRef <= 0 || pSwPCRInfo->vPcrRef <= 0)||
			((!pPcrProcesser->bNoAudioPts && !pPcrProcesser->bNoVideoPts) && (pSwPCRInfo->aPcrRef != pSwPCRInfo->vPcrRef)))
	{
		*getPcr = pcr;
		*getPts = pts;
		return 0;
	}

	lastDemuxAudioPTS = pSwPCRInfo->aPrevDemuxPTS;
	lastDemuxVideoPTS = pSwPCRInfo->vPrevDemuxPTS;
	if(pPcrProcesser->bNoAudioPts)
		pcr = pSwPCRInfo->vPcrRef;
	else
		pcr = pSwPCRInfo->aPcrRef;


	if(pcr <= 0 || (lastDemuxAudioPTS <= 0 && lastDemuxVideoPTS <= 0))
	{
		pick = 0;
	}
	else if(pPcrProcesser->bNoAudioPts == true || lastDemuxAudioPTS <= 0)
	{
		pick = 2;
	}
	else if(pPcrProcesser->bNoVideoPts == true || lastDemuxVideoPTS <= 0)
	{
		pick = 1;
	}
	else
	{
		int64_t adiff = abs64(lastDemuxAudioPTS - pcr);
		int64_t vdiff = abs64(lastDemuxVideoPTS - pcr);
		int64_t avdiff = abs64(lastDemuxAudioPTS - lastDemuxVideoPTS);

		if(avdiff > 13*DEMUX_CLOCKS_PER_SEC)
		{
			if(adiff < vdiff)
				pick = 1;
			else
				pick = 2;
		}
		else
		{
			if(lastDemuxAudioPTS < lastDemuxVideoPTS)
				pick = 1;
			else
				pick = 2;
		}
	}

	if(pick == 1)
	{
		pts = lastDemuxAudioPTS;
	}
	else if(pick == 2)
	{
		if(lastDemuxVideoPTS > 0)
			pts = lastDemuxVideoPTS;
		else
			pts = -1;
	}
	else
	{
		pts = -1;
	}
	*getPcr = pcr;
	*getPts = pts;
	return 1;
}
void PCRSYNC_UpdateDemuxPTS(PCRSYNC_T *pPcrProcesser, bool bVideo, int64_t pts)
{
	if (bVideo) {
		PCRSYNC_UpdatePTSStableStatus(pPcrProcesser, -1, pts);
		if (pPcrProcesser->bUseHackPCR && !pPcrProcesser->bUseAudioPTS&&!pPcrProcesser->bUseFixedRCD)
			PCRSYNC_AverageRCD(pPcrProcesser, pPcrProcesser->lastDemuxVideoPTS, pts, &pPcrProcesser->videoRCD);

		if (pPcrProcesser->startVideoPts == -1) {
			dmx_crit(pPcrProcesser->tunerHandle, "[dtv] ch[%d] deliver first video pts = %lld, time = %lld\n", (int)pPcrProcesser->tunerHandle, pts, pcrsync_clock_get(pPcrProcesser));
			pPcrProcesser->startVideoPts = pts;
		}

		pPcrProcesser->lastDemuxVideoPTS = pts;

	} else { /*Audio*/
		PCRSYNC_UpdatePTSStableStatus(pPcrProcesser, pts, -1);
		if (pPcrProcesser->bUseHackPCR && pPcrProcesser->bUseAudioPTS&&!pPcrProcesser->bUseFixedRCD)
			PCRSYNC_AverageRCD(pPcrProcesser, pPcrProcesser->lastDemuxAudioPTS, pts, &pPcrProcesser->audioRCD);

		if (pPcrProcesser->startAudioPts == -1) {
			dmx_crit(pPcrProcesser->tunerHandle, "[dtv] ch[%d] deliver first audio pts = %lld, time = %lld\n", (int)pPcrProcesser->tunerHandle, pts, pcrsync_clock_get(pPcrProcesser));
			pPcrProcesser->startAudioPts = pts;
		}

		pPcrProcesser->lastDemuxAudioPTS = pts;
		REFCLOCK_SetDemuxAudioPTS(pPcrProcesser->pRefClock, pts);

	}
}

void PCRSYNC_DEBUG_SaveAvSyncInfo(PCRSYNC_T *pPcrProcesser, NAVDTVPCRINFO *pcrinfo)
{
	PRESENTATION_POSITIONS position;
	int64_t pts;
	RTKDEMUX_AVSYNC_INFO_T info;

	if (g_dbgSetting[pPcrProcesser->tunerHandle].dump_sync_info.enable == 0)
		return;
    
	REFCLK_GetPresentationPositions(pPcrProcesser->pRefClock, &position);
	pts = pcrsync_clock_get(pPcrProcesser);
    
    
	info.Pcr = pcrinfo->pcr;
	info.Stc = pcrinfo->stc;
	info.Rcd = pPcrProcesser->rcd;
	info.AudioDemuxPts = pPcrProcesser->lastDemuxAudioPTS;
	info.VideoDemuxPts = pPcrProcesser->lastDemuxVideoPTS;
	info.AudioSystemPts = position.audioSystemPTS;
	info.VideoSystemPts = position.videoSystemPTS;
	info.PcrOffset = pPcrProcesser->pcrOffset;
	info.MasterPTS = pPcrProcesser->rcd + pts;
	info.SystemPts = pts;
    
	DEBUG_DumpSyncInfo(pPcrProcesser->tunerHandle, &info);
	
}

void PCRSYNC_ResetRcdStabilityCheckStatus(PCRSYNC_T *processor)
{
	processor->rcdStableCount        = 0;
	processor->rcdStableCheckCount   = 0;
	processor->rcdUnstableCount      = 0;
	processor->rcdUnstableCheckCount = 0;
}

bool PCRSYNC_EvaluateRcdStability(
	PCRSYNC_T *processor,
	int64_t newPcr,
	int64_t newStc
)
{
	int64_t oldRcd,	newRcd,	rcdDiff;
	bool isReasonableRcdDiff, hasOldRcd, isRcdStable;


	if (processor->pcr == newPcr || processor->stc == newStc)
		return false;  
		 
	oldRcd  = processor->rcd;
	newRcd  = newPcr - newStc - processor->pcrOffset;
	rcdDiff = abs64(newRcd - oldRcd);

	isReasonableRcdDiff = rcdDiff < 10*DEMUX_CLOCKS_PER_SEC;
	hasOldRcd           = (oldRcd != -1);
	isRcdStable         = rcdDiff <= UNSTABLE_RCD_THRESHOLD;

	if (processor->pcr == newPcr || processor->stc == newStc)
		return false;
	// check for stable
	if (isReasonableRcdDiff && (hasOldRcd && isRcdStable))
		processor->rcdStableCount++;

	if (processor->rcdStableCount)
		processor->rcdStableCheckCount++;

	// check for unstable
	if (isReasonableRcdDiff && (hasOldRcd && !isRcdStable))
		processor->rcdUnstableCount++;

	if (processor->rcdUnstableCount)
		processor->rcdUnstableCheckCount++;

	return true;
}

bool PCRSYNC_ShouldEnterFreeRunMode(const PCRSYNC_T *processor)
{
	return (30 < processor->rcdUnstableCount);
}

bool PCRSYNC_ShouldReturnToPCRMaster(const PCRSYNC_T *processor)
{
	return (200 <= processor->rcdStableCheckCount
		&& 30 > processor->rcdUnstableCount);
}

bool PCRSYNC_ShouldResetRcdStabilityCheckStatus(
	const PCRSYNC_T *processor
)
{
	// under free run mode
	const bool shouldResetUnderFreeRunMode =
		(PCRSYNC_IsUnderFreeRunMode(processor)
		&& 200 < processor->rcdStableCheckCount);

	// under other modes
	const bool shouldResetUnderOtherModes =
		(!PCRSYNC_IsUnderFreeRunMode(processor)
		&& (processor->bCheckPcrOffset == true
			|| 100 < processor->rcdUnstableCheckCount));

	return (shouldResetUnderFreeRunMode
		|| shouldResetUnderOtherModes);
}

void PCRSYNC_SetFreeRunMode(
	PCRSYNC_T *processor,
	bool isUnderFreeRunMode
)
{
	processor->isUnderFreeRunMode = isUnderFreeRunMode;
}

bool PCRSYNC_IsUnderFreeRunMode(const PCRSYNC_T *processor)
{
	return processor->isUnderFreeRunMode;
}

#ifndef DEBUG_MSG_CONTROL_CODE
	#define DEBUG_MSG_CONTROL_CODE
	#define CONTROL_CODE_YELLOW    "\033[1;33m"
	#define CONTROL_CODE_END       "\033[m"
#endif

void PCRSYNC_EnterFreeRunMode(PCRSYNC_T *pPcrProcesser)
{
	dmx_crit(pPcrProcesser->tunerHandle,
		CONTROL_CODE_YELLOW"(CH_%d)Enter free run mode"CONTROL_CODE_END"\n" , pPcrProcesser->tunerHandle
	);


	LRegress_dumpDatabase(&pPcrProcesser->LRegress,__func__, __LINE__);
	PCRSYNC_SetFreeRunMode(pPcrProcesser, true);
	pPcrProcesser->bCheckPcrOffset    = true;

	PCRSYNC_ResetRcdStabilityCheckStatus(pPcrProcesser);
}

void PCRSYNC_ReturnToPCRMaster(PCRSYNC_T *pPcrProcesser)
{
	dmx_crit(pPcrProcesser->tunerHandle,
		CONTROL_CODE_YELLOW"(CH_%d)Return to PCR Master"CONTROL_CODE_END"\n", pPcrProcesser->tunerHandle
	);

	PCRSYNC_SetFreeRunMode(pPcrProcesser, false);
	pPcrProcesser->bCheckPcrOffset    = false;

	PCRSYNC_ResetRcdStabilityCheckStatus(pPcrProcesser);
}
bool PCRSYNC_EvaluateToReturnPCRMaster(PCRSYNC_T *pPcrProcesser)
{
	int64_t newPcr, newStc, ignore;


	if (!PCRSYNC_IsUnderFreeRunMode(pPcrProcesser))
		return false;

	if (RHAL_GetPCRTrackingStatus(pPcrProcesser->tunerHandle, &newPcr, &newStc, &ignore, &ignore ) != 0)
		return false;

	if (REFCLK_GetRCD(pPcrProcesser->pRefClock) == -1)
		return false;

	//const bool shouldUpdateProcessorRcd = PCRSYNC_EvaluateRcdStability(processor, newPcr, newStc);

	if (PCRSYNC_EvaluateRcdStability(pPcrProcesser, newPcr, newStc))
		PCRSYNC_UpdateProcessorRcd(pPcrProcesser, newPcr, newStc);


	//DMX_DO_ACTION_ONCE_EVERY(DEMUX_CLOCKS_PER_SEC,
	//	dmx_notice(pCh->tunerHandle,"[%s:%d] RCD stability: stable check = %lld, unstable count = %lld\n",
	//		processor->rcdStableCheckCount,	processor->rcdUnstableCount);
	//);

	if (PCRSYNC_ShouldReturnToPCRMaster(pPcrProcesser))
	{
		PCRSYNC_ReturnToPCRMaster(pPcrProcesser);
		return true;
	}

	if (PCRSYNC_ShouldResetRcdStabilityCheckStatus(pPcrProcesser))
		PCRSYNC_ResetRcdStabilityCheckStatus(pPcrProcesser);

	return false;
}

#ifdef DEBUG_MSG_CONTROL_CODE
	#undef DEBUG_MSG_CONTROL_CODE
	#undef CONTROL_CODE_YELLOW
	#undef CONTROL_CODE_END
#endif

void PCRSYNC_UpdateProcessorRcd(PCRSYNC_T *processor, int64_t newPcr, int64_t newStc)
{
	// update processor info for later use if
	// switch to pcr master mode
	const int64_t newRcd = (newPcr - newStc - processor->pcrOffset);
	processor->pcr = newPcr;
	processor->stc = newStc;
	processor->rcd = newRcd;

	processor->prevVideoDemuxPTS = processor->lastDemuxVideoPTS;
	processor->prevAudioDemuxPTS = processor->lastDemuxAudioPTS;
}
int PCRSYNC_CheckPTSChange(PCRSYNC_T *pPcrProcesser)
{

	if (pPcrProcesser->lastDemuxAudioPTS == -1 || pPcrProcesser->prevAudioDemuxPTS == -1 || (pPcrProcesser->lastDemuxAudioPTS == pPcrProcesser->prevAudioDemuxPTS))
		pPcrProcesser->noAudioPtsCounter++;
	else
		pPcrProcesser->audioPtsCounter++;

	if (pPcrProcesser->lastDemuxVideoPTS == -1 || pPcrProcesser->prevVideoDemuxPTS == -1 || (pPcrProcesser->lastDemuxVideoPTS == pPcrProcesser->prevVideoDemuxPTS))
		pPcrProcesser->noVideoPtsCounter++;
	else
		pPcrProcesser->videoPtsCounter++;

	return 1;
}
void PCRSYNC_CheckPTSDataStable(PCRSYNC_T *pPcrProcesser)
{
	if((pPcrProcesser->noAudioPtsCounter + pPcrProcesser->audioPtsCounter) > 100)
	{
		if(pPcrProcesser->audioPtsCounter > 10)
			pPcrProcesser->bNoAudioPts = false;
		else if(pPcrProcesser->noAudioPtsCounter > 200)
			pPcrProcesser->bNoAudioPts = true;

		if((pPcrProcesser->noAudioPtsCounter + pPcrProcesser->audioPtsCounter) > 300)
		{
			pPcrProcesser->noAudioPtsCounter = 0;
			pPcrProcesser->audioPtsCounter = 0;
		}
	}

	if((pPcrProcesser->noVideoPtsCounter + pPcrProcesser->videoPtsCounter) > 100)
	{
		if(pPcrProcesser->videoPtsCounter > 10)
			pPcrProcesser->bNoVideoPts = false;
		else if(pPcrProcesser->noVideoPtsCounter > 200)
			pPcrProcesser->bNoVideoPts = true;

		if((pPcrProcesser->noVideoPtsCounter + pPcrProcesser->videoPtsCounter) > 300)
		{
			pPcrProcesser->noVideoPtsCounter = 0;
			pPcrProcesser->videoPtsCounter = 0;
		}
	}
}

void PCRSYNC_CheckAVFreerun(PCRSYNC_T *pPcrProcesser)
{
	if (pPcrProcesser->rcd != -1 )
	{
		int64_t mpts = pPcrProcesser->rcd+pcrsync_clock_get(pPcrProcesser);
		pPcrProcesser -> bVideoFreerun = (abs64(pPcrProcesser->lastDemuxVideoPTS - mpts ) > pPcrProcesser->v_fr_thrhd);
		pPcrProcesser -> bAudioFreerun = (abs64(pPcrProcesser->lastDemuxAudioPTS - mpts ) > DEFAULT_FREE_RUN_AUDIO_THRESHOLD);
	}
}

PCR_EXIST_STATUS PCRSYNC_CheckPcrExist(PCRSYNC_T *pPcrProcesser)
{
	#define PCR_CNT_IN_SECOND (1)

	PCREXISTDETECT* pPcrExistDetect = &pPcrProcesser->pcrExistDetect;
	PCR_EXIST_STATUS status = PCR_EXIST_STATUS_STILL_CHECK;

	if(pPcrExistDetect->checkPcrExitEnd)
		return PCR_EXIST_STATUS_ALREADY_GOT;
	else if (!pPcrExistDetect->checkPacketReady)
		return PCR_EXIST_STATUS_STILL_CHECK;


	if(pPcrExistDetect->pcrCheckTimeout == -1) {
		pPcrExistDetect->pcrCheckTimeout = (90000+45000) + CLOCK_GetPTS();
		dmx_notice(pPcrProcesser->tunerHandle,"Start check PCR(%d,%d)\n",
			pPcrExistDetect->detectPcrCnt, DEMUX_CLOCKS_PER_SEC);
	}

	if(pPcrExistDetect->oldPCR < 0 && pPcrExistDetect->newPCR > 0) {
		pPcrExistDetect->detectPcrCnt++;
		pPcrExistDetect->oldPCR = pPcrExistDetect->newPCR;
	}
	else if ((pPcrExistDetect->newPCR != pPcrExistDetect->oldPCR) && 
		(abs64(pPcrExistDetect->newPCR - pPcrExistDetect->oldPCR) < 180000)) {
		pPcrExistDetect->detectPcrCnt++;
		pPcrExistDetect->oldPCR = pPcrExistDetect->newPCR;
	}

	if(pPcrExistDetect->detectPcrCnt >= PCR_CNT_IN_SECOND)
	{
		status = PCR_EXIST_STATUS_ALREADY_GOT;
		pPcrExistDetect->checkPcrExitEnd = true;
		dmx_notice(pPcrProcesser->tunerHandle,"PCR ready (cnt %d)\n", pPcrExistDetect->detectPcrCnt);
		dmx_notice(pPcrProcesser->tunerHandle,"1st pcr packet is arrived\n");
	}
	else if ((CLOCK_GetPTS() > pPcrExistDetect->pcrCheckTimeout) && 
		(pPcrExistDetect->detectPcrCnt < PCR_CNT_IN_SECOND))
	{
		status = PCR_EXIST_STATUS_TIMEOUT;
		pPcrExistDetect->checkPcrExitEnd = true;
		dmx_crit(pPcrProcesser->tunerHandle,
			"PCR timeout (cnt %d, oldPcr %lld, newPcr %lld, a %lld, v %lld) (%d,%d)\n",
		  pPcrExistDetect->detectPcrCnt, pPcrExistDetect->oldPCR,
		  pPcrExistDetect->newPCR, pPcrProcesser->lastDemuxAudioPTS,
		  pPcrProcesser->lastDemuxVideoPTS,
		  (pPcrExistDetect->oldPCR < 0 && pPcrExistDetect->newPCR > 0),
	    ((pPcrProcesser->lastDemuxAudioPTS > 0 && (abs64(pPcrExistDetect->newPCR - pPcrProcesser->lastDemuxAudioPTS) < DEFAULT_FREE_RUN_AUDIO_THRESHOLD)) ||
		(pPcrProcesser->lastDemuxVideoPTS > 0 && (abs64(pPcrExistDetect->newPCR - pPcrProcesser->lastDemuxVideoPTS) < pPcrProcesser->v_fr_thrhd))));
	}

	return status;
}

char * pcrsync_status_dump(PCRSYNC_T *pPcrProcesser, char * pbuf)
{
	bool enable = (pPcrProcesser->pid != 0x1fff);
	return print_to_buf(pbuf, "%2d %2d %4d 0x%04x %08llx %08llx %5d %08llx\n",
		pPcrProcesser->tunerHandle, pPcrProcesser->bUseFixedRCD == false, 1,
		pPcrProcesser->pid, 
		(enable ? pPcrProcesser->rcd+ pcrsync_clock_get(pPcrProcesser): 0),
		(enable ? pPcrProcesser->pcr : 0), 90000,  CLOCK_GetPTS());
}