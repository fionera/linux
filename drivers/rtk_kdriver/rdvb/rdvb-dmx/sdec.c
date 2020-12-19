#include <linux/freezer.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include "AudioInbandAPI.h"
#include "sdec.h"
#include "debug.h"
#include <tp/tp_drv_global.h>
#include "rdvb_dmx_ctrl.h"
#include "rdvb-buf/rdvb_dmx_buf.h"
#include <VideoRPC_System.h>
#include <rtk_kdriver/RPCDriver.h>
#include "dmx_inbandcmd.h"
#include "rdvb_dmx.h"
#include "rdvb_dmx_filtermgr.h"
#include "dmx_inbandcmd.h"
#define IS_AUDIO_PIN(x) ((x) == AUDIO_PIN || (x) == AUDIO2_PIN)
#define IS_VIDEO_PIN(x) ((x) == VIDEO_PIN || (x) == VIDEO2_PIN)

#define IS_VIDEO_TARGET(_target) \
	((_target) == DEMUX_TARGET_VIDEO || (_target) == DEMUX_TARGET_VIDEO_2ND)

#define IS_AUDIO_TARGET(_target) \
	((_target) == DEMUX_TARGET_AUDIO || (_target) == DEMUX_TARGET_AUDIO_2ND)

#define NAV_SLEEP_TIME_ON_IDLE      100 /* milliseconds*/
#define TUNER_DATA_READ_WAIT_TIME  (20)

#define RELEASE_TP_BUFFER()                      \
do {                                             \
    if (pCh->pTPRelase && pCh->TPReleaseSize) {  \
        RHAL_TPReleaseData(                      \
            pCh->idx,                    \
            pCh->pTPRelase + pCh->tpBuffer.phyAddrOffset, \
            pCh->TPReleaseSize                   \
        );                                       \
        pCh->pTPRelase = NULL;                   \
        pCh->TPReleaseSize = 0;                  \
    }                                            \
} while (false)

#define DMX_DO_ACTION_ONCE_EVERY(duration__, statements__) \
do {                                                       \
    static int64_t last__    = 0;                          \
    const  int64_t current__ = CLOCK_GetPTS();             \
    const  int64_t passed__  = abs64(current__ - last__);  \
    if ((duration__) <= passed__) {                        \
        { statements__ }                                   \
        last__ = current__;                                \
    }                                                      \
} while (false)

static PCRTRK_MANAGER_T gClockRecovery[3];
/*const char *rtkdemux_outputNames[NUMBER_OF_PINS] = {
    "VIDEO0",
    "AUDIO0",
    "VIDEO1",
    "AUDIO1",
};*/
static void SDEC_FlushTPBuffer(SDEC_CHANNEL_T *pCh);

/******************************************************************************
 * SDEC playback APIs (ex: pvr playback)
 *****************************************************************************/
 static bool sdec_pb_isAudioOnlyFile(SDEC_CHANNEL_T * pCh)
{
	struct ts_parse_context * tpc;
	int vCnt = 0, aCnt = 0;
	bool ret = false;

	list_for_each_entry(tpc, &pCh->tpc_list_head, list_head) {
		if (tpc->data_context_type == DATA_CONTEXT_TYPE_VIDEO)
			vCnt++;
		if (tpc->data_context_type == DATA_CONTEXT_TYPE_AUDIO)
			aCnt++;
	}

	if (vCnt == 0 && aCnt > 0) {
		ret = true;
	}

	return ret;
}

static bool sdec_pb_isVideoOnlyFile(SDEC_CHANNEL_T * pCh)
{
	struct ts_parse_context * tpc;
	int vCnt = 0, aCnt = 0;
	bool ret = false;

	list_for_each_entry(tpc, &pCh->tpc_list_head, list_head) {
		if (tpc->data_context_type == DATA_CONTEXT_TYPE_VIDEO)
			vCnt++;
		if (tpc->data_context_type == DATA_CONTEXT_TYPE_AUDIO)
			aCnt++;
	}

	if (aCnt == 0 && vCnt > 0 ) {
		ret = true;
	}

	return ret;
}

static void sdec_pb_startToPlay(SDEC_CHANNEL_T *pCh)
{
	REFCLOCK * const refClock = pCh->refClock.prefclk;

	/* stop to do buffering */
	pCh->bPendingOnPTSCheck = false;
	pCh->bPendingOnFullness = false;

	/* let master starts to play */
	REFCLK_SetTimeout(refClock, 0);
}

/* Buffering by checking A/V PTS difference to start playback
 * Conditions to start playback: Case 1 or Case 2 is true
 * Case 1:
 *   a. 1st videoPts > 1st audioPts
 *   b. Diff of video and audio < 3 sec (free run threshold is 3sec)
 *   c. last audioPts must larger than 1st videoPts over 500ms
 * Case 2:
 *   d. 1st audioPts > 1st videoPts
 *   f. last videoPts must larger than 1st audioPts over 500ms
*/
static void sdec_pb_checkPtsDiff(SDEC_CHANNEL_T *pCh)
{
	PCRSYNC_T *pcrsync;
	REFCLOCK * const refClock = pCh->refClock.prefclk;
	bool isAudioOnly = sdec_pb_isAudioOnlyFile(pCh);
	bool isVideoOnly = sdec_pb_isVideoOnlyFile(pCh);

	pcrsync = &pCh->pcrProcesser;
	if (isAudioOnly) {
		SDEC_ResetAVSync(pCh, NAV_AVSYNC_AUDIO_ONLY);
		sdec_pb_startToPlay(pCh);
		dmx_crit(pCh->idx, "Start to play: Audio only(%lld, %lld)\n",
			pcrsync->startAudioPts,
			pcrsync->lastDemuxAudioPTS);
		return;
	}
	if (((pcrsync->startVideoPts > pcrsync->startAudioPts)
		&& (pcrsync->startVideoPts -pcrsync->startAudioPts) < 270000
		&& (pcrsync->lastDemuxAudioPTS - pcrsync->startVideoPts) >= pCh->avSyncStartupPTS)
		||(pcrsync->startVideoPts <= pcrsync->startAudioPts
		&& (pcrsync->lastDemuxVideoPTS-pcrsync->startAudioPts) >= pCh->avSyncStartupPTS)) {

		sdec_pb_startToPlay(pCh);
		dmx_crit(pCh->idx, "Start to play: APTS = %lld, VPTS = %lld\n",
			pcrsync->lastDemuxAudioPTS, pcrsync->lastDemuxVideoPTS);
		return;
	}

	// Check if Stream is wrapped around? => If yes, Check video PTS fullness only
	if (pcrsync->lastDemuxVideoPTS < pcrsync->startVideoPts &&
		(pcrsync->startVideoPts - pcrsync->lastDemuxVideoPTS) >= 2*90000) {

		pcrsync->startVideoPts = pcrsync->lastDemuxVideoPTS;
		pCh->bPendingOnFullness = true;
		pCh->avSyncStartupPTS = 0;
		pCh->bPendingOnPTSCheck = false;
		dmx_crit(pCh->idx, "[pvr] Stream warp-around, reset first video pts = %lld \n",
			pcrsync->startVideoPts);
		return;
	}

	//Check PTS diff timeout(3sec)
	if (pCh->bPendingOnPTSCheck && REFCLK_GetRCD(refClock) != -1) {
		sdec_pb_startToPlay(pCh);
		dmx_crit(pCh->idx, "Timeout: APTS = %lld, VPTS = %lld\n",
			pcrsync->lastDemuxAudioPTS,
			pcrsync->lastDemuxVideoPTS);
		return;
	}

	//Check video PTS fullness only
	if (REFCLK_GetRCD(refClock) == -1 && ((isVideoOnly)
		|| (pcrsync->startVideoPts != -1 && pcrsync->startAudioPts != -1
		&& abs(pcrsync->startVideoPts -pcrsync->startAudioPts) >= 270000))) {
		pCh->bPendingOnFullness = true;
		pCh->avSyncStartupPTS = 0;
		pCh->bPendingOnPTSCheck = false;
		dmx_crit(pCh->idx,"isVideoOnly=%d, A/V startPTS diff = %d"
			", check videoPts fullness start ...\n", isVideoOnly,
			(int)abs(pcrsync->startVideoPts -pcrsync->startAudioPts));
	}
}

/* Buffering by checking video PTS fullness(>500ms) to start playback
 * Case 1: When diff of startVideoPts/startAudioPts over 3 sec (> free run threshold)
 *   a. In order to speed up video display time, we check video PTS fullness instead
 *   b. In timeShift delay play case: pause->play within 2sec
 *      Although A/V pts diff is satisfied, but audio have no data,
 *      thus we change to check video PTS fullness instead
 * Case 2: video only program
*/
static void sdec_pb_checkPtsFullness(SDEC_CHANNEL_T *pCh)
{
	PCRSYNC_T *pcrsync = &pCh->pcrProcesser;

	if (pcrsync->startVideoPts != -1 && pcrsync->lastDemuxVideoPTS !=-1
		&& (pcrsync->lastDemuxVideoPTS - pcrsync->startVideoPts) > 45000) {
		sdec_pb_startToPlay(pCh);
		dmx_crit(pCh->idx, "Start to play: Video only(%lld, %lld)!\n",
			pcrsync->startVideoPts,
			pcrsync->lastDemuxVideoPTS);
	}

	if (pCh->bPendingOnFullness && REFCLK_GetRCD(pCh->refClock.prefclk) != -1) {
		sdec_pb_startToPlay(pCh);
		dmx_crit(pCh->idx, "Timeout: vPTS(%lld, %lld)!\n",
			pcrsync->startVideoPts,
			pcrsync->lastDemuxVideoPTS);
	}
}

static void sdec_pb_buffering(SDEC_CHANNEL_T *pCh)
{
	if (!pCh->bPendingOnPTSCheck && !pCh->bPendingOnFullness)
		return;

	if (pCh->bPendingOnPTSCheck) {
		sdec_pb_checkPtsDiff(pCh);
	} else if(pCh->bPendingOnFullness) {
		sdec_pb_checkPtsFullness(pCh);
	}
}

static inline bool sdec_pb_EsMonitor_checkPTS(const int64_t aSysPTS,
				const int64_t vSysPTS, const int64_t lastDemuxAudioPTS,
				const int64_t lastDemuxVideoPTS)
{
	if ((aSysPTS !=-1 || vSysPTS != -1) &&
		(lastDemuxAudioPTS !=-1 || lastDemuxVideoPTS != -1) )
		return true;
	else
		return false;
}

static inline bool sdec_pb_EsMonitor_checkAudio(const int64_t aSysPTS,
				const int64_t vSysPTS, const int64_t lastDemuxAudioPTS,
				const int64_t lastDemuxVideoPTS)
{
	if (lastDemuxAudioPTS == -1) {
		// 1.When switch audio language;
		// 2.Video only program
		return true;
	}

	if (aSysPTS ==-1) {
		/* audio not start to play */
		if (vSysPTS == -1)
			return false;

		if ((int)(lastDemuxAudioPTS-vSysPTS)/90 > 200)
			return true;
		else
			return false;
	} else {
		//if (lastDemuxAudioPTS < aSysPTS)/*stream wrap around*/
		//	return true;
		if ((int)(lastDemuxAudioPTS-aSysPTS)/90 > 200)
			return true;
		else
			return false;
	}
}

static inline bool sdec_pb_EsMonitor_checkVideio(const int64_t aSysPTS,
						const int64_t vSysPTS, const int64_t lastDemuxAudioPTS,
						const int64_t lastDemuxVideoPTS)
{
	if (lastDemuxVideoPTS ==-1) //audio only program
		return true;
	if (vSysPTS == -1)
		return false;
	//if (lastDemuxVideoPTS < vSysPTS)/*stream wrap around*/
	//	return true;
	if ((int)(lastDemuxVideoPTS - vSysPTS)/90 >= 500)
		return true;
	else
		return false;
}

/* Check PTS difference to determine ES buffer stable or not
*    return:
*        TRUE: stop feed data to a/v
*        FALSE: allowed feed data to a/v
*    for detail :https://wiki.realtek.com/display/MLPKB/PVR+ES+monitor
*/
static bool sdec_pb_checkESBufStable(SDEC_CHANNEL_T *pCh)
{
	bool ret = false;
	static unsigned int actionMonitor= 0;
	static unsigned int actionMonitor_subCount =0;
	bool print_info= false;
	static int64_t 	_last	 = 0;
	const int64_t _current = CLOCK_GetPTS();
	const int64_t _passed  = abs(_current - _last);
	unsigned int conditionMonitor = 0;
	REFCLOCK * const pRefClock = pCh->refClock.prefclk ;
	int64_t vSysPTS = reverse_int64(pRefClock->videoSystemPTS);
	int64_t aSysPTS = reverse_int64(pRefClock->audioSystemPTS);

	PCRSYNC_T *pcrsync = &pCh->pcrProcesser;

	/*Paused*/
	if (pCh->isAudioPaused == true) {
		conditionMonitor = (1<<4) | 0x1;// 0x11
		ret =  true;
		goto END;
	}
	/*Radio channel(Audio only) program*/
	if (pCh->avSyncMode == NAV_AVSYNC_AUDIO_ONLY) {
		ret =  false;
		goto END;
	}
	/* During pvr reset flow */
	if (HAS_FLAG(pCh->status, STATUS_FLUSH_MTP)) {
		conditionMonitor = 0x01;// 0x01
		ret = false;
		goto END;
	}
	/* Normal Play */
	if ((pCh->avSyncMode != NAV_AVSYNC_VIDEO_ONLY) && (pCh->pvrPbSpeed != 0) ) {
		if (sdec_pb_EsMonitor_checkPTS(aSysPTS, vSysPTS, 
					pcrsync->lastDemuxAudioPTS,pcrsync->lastDemuxVideoPTS)
			&& sdec_pb_EsMonitor_checkAudio(aSysPTS, vSysPTS, 
					pcrsync->lastDemuxAudioPTS,pcrsync->lastDemuxVideoPTS)
			&& sdec_pb_EsMonitor_checkVideio(aSysPTS, vSysPTS,
				pcrsync->lastDemuxAudioPTS,pcrsync->lastDemuxVideoPTS)) {
			/* Handle VO/AO underflow case */
			if (/*((pcrsync->lastDemuxAudioPTS != -1) &&
					(aSysPTS != -1) &&
					 (DEMUX_reverseInteger(pRefClock->AO_Underflow))) ||*/
				((pcrsync->lastDemuxVideoPTS != -1) &&
				  (vSysPTS != -1) &&
				  (reverse_int32(pRefClock->VO_Underflow)))) {
				ret = false;
				conditionMonitor = 0x02;// 0x02
				goto END;
			}

			ret =  true;
			conditionMonitor =( (1<<4) | 0x2);// 0x12
			goto END;
		} else {
			conditionMonitor = 0x03;// 0x03
			ret = false;
			goto END;
		}
	}

	/* step/0.5x/2x play */
	if ((pCh->pvrPbSpeed == 0) || ((pCh->avSyncMode == NAV_AVSYNC_VIDEO_ONLY) &&
						((pCh->pvrPbSpeed == 128)||(pCh->pvrPbSpeed == 512)))) {
		//only check video status
		if ( (vSysPTS != -1  )
			&&(pcrsync->lastDemuxVideoPTS != -1)
			&& (((int)(pcrsync->lastDemuxVideoPTS - vSysPTS)/90 >= 500) ? true : false)) {

			if (reverse_int32(pRefClock->VO_Underflow)) {
				ret = false;
				conditionMonitor = 0x04;// 0x04
			} else {
				ret =  true;
				conditionMonitor = ( (1<<4) | 0x3);// 0x13
			}
			goto END;
		} else {
			conditionMonitor = 0x05; //0x05
			ret =  false;
			goto END;
		}
	}
	conditionMonitor = 0x06;//0x06


END:


	if (_passed >= 45000) {
		print_info = true;
	}

	if (ret) {
		if (actionMonitor%2 == 0) {// true : odd
			actionMonitor ++;
			print_info = true;
		} else {
			actionMonitor_subCount++;
		}
	} else {

		if (actionMonitor%2 == 1) { // false: even
			actionMonitor ++;
			print_info = true;
		} else {
			actionMonitor_subCount++;
		}
	}

	if (print_info) {
		if (ret) {
			dmx_mask_print(DEMUX_PVR_ES_MONITOR_DEBUG,  pCh->idx,
				"\033[1;33m""ch:%d, a:%d ms, v:%d ms, synMode:%d, speed:%d,"
				"isPause:%d, pts:(%lld, %lld), sysPts:(%lld, %lld), underflow(%d,%d)"
				" ===>ret = %d (%d--%d): (0x%02x) \033[m\n",
				pCh->idx,(int)(pcrsync->lastDemuxAudioPTS-aSysPTS)/90,
				(int)(pcrsync->lastDemuxVideoPTS-vSysPTS)/90, pCh->avSyncMode,
				pCh->pvrPbSpeed , pCh->isAudioPaused, pcrsync->lastDemuxAudioPTS,
				pcrsync->lastDemuxVideoPTS,aSysPTS,vSysPTS,
				reverse_int32(pRefClock->AO_Underflow),
				reverse_int32(pRefClock->VO_Underflow),
				ret, actionMonitor ,actionMonitor_subCount, conditionMonitor);
		} else {
			dmx_mask_print(DEMUX_PVR_ES_MONITOR_DEBUG,  pCh->idx,
				"\033[1;33;46m""ch:%d, a:%d ms, v:%d ms, synMode:%d, speed:%d,"
				"isPause:%d, pts:(%lld, %lld), sysPts:(%lld, %lld), underflow(%d,%d)"
				"===>ret = %d (%d--%d): (0x%02x) \033[m\n",
				pCh->idx,(int)(pcrsync->lastDemuxAudioPTS-aSysPTS)/90,
				(int)(pcrsync->lastDemuxVideoPTS-vSysPTS)/90,pCh->avSyncMode,
				pCh->pvrPbSpeed , pCh->isAudioPaused, pcrsync->lastDemuxAudioPTS,
				pcrsync->lastDemuxVideoPTS,aSysPTS,vSysPTS,
				reverse_int32(pRefClock->AO_Underflow),
				reverse_int32(pRefClock->VO_Underflow),
				ret, actionMonitor ,actionMonitor_subCount, conditionMonitor);
		}
		actionMonitor_subCount=1;
		_last = _current;
	}

	return ret;

}

int SDEC_PB_AudioPause(SDEC_CHANNEL_T * pCh, bool isOn)
{
       struct rdvb_dmx * rdvb_dmx = pCh->dvb_demux.priv;

       if (isOn == pCh->isAudioPaused)
               return 0;

       dmx_crit(pCh->idx, "Audio paused: %d ! \n", isOn);

       if (isOn) {
               rdvbdmx_filter_audio_pause_on(&rdvb_dmx->filter_list);
               LOCK(&pCh->mutex);
               rdvb_dmx->is_audio_paused = true;
               pCh->isAudioPaused = true;
               UNLOCK(&pCh->mutex);
       } else {
               rdvbdmx_filter_audio_pause_off(&rdvb_dmx->filter_list);
               LOCK(&pCh->mutex);
               rdvb_dmx->is_audio_paused = false;
               pCh->isAudioPaused = false;
               UNLOCK(&pCh->mutex);
       }
       return 0;
}

int SDEC_PB_SetSpeed(SDEC_CHANNEL_T * pCh, int speed)
{
	if (pCh->pvrPbSpeed == 0 && speed == 128) {
		// 1/2x play, stop audio
		dmx_err(pCh->idx, "[%s, %d]  1/2x play \n", __func__, __LINE__);
		tpc_stop_audio_decoding(&pCh->tpc_list_head);
	}
	LOCK(&pCh->mutex);
	pCh->pvrPbSpeed = speed;
	if (speed == 0) {
		UNLOCK(&pCh->mutex);
		return 0;
	}

	//Set speed info to VO
	tpc_set_speed_to_vo(&pCh->tpc_list_head, speed);

	if (speed != 256) {
		if (pCh->avSyncMode != NAV_AVSYNC_VIDEO_ONLY) {
			SDEC_ResetAVSync(pCh, NAV_AVSYNC_VIDEO_ONLY);
		}
		UNLOCK(&pCh->mutex);
		return 0;
	} else {
		if (pCh->avSyncMode == MTP_AVSYNC_MODE) {
			UNLOCK(&pCh->mutex);
			return 0;
		}
		SDEC_ResetAVSync(pCh, MTP_AVSYNC_MODE);
	}

	UNLOCK(&pCh->mutex);
	return 0;

}

int SDEC_PB_Reset(SDEC_CHANNEL_T *pCh)
{
	// 1. reset mtp buffer
	if (RHAL_MTPFlushBuffer(pCh->idx, MTP_BUFF_FLUSH_TO_BASE) != TPK_SUCCESS) {
		dmx_err(pCh->idx, "RHAL_MTPFlushBuffer failed!! \n");
		return -1;
	}
	// 2. reset tp and demux buffer
	SDEC_FlushTPBuffer(pCh);

	return 0;
}

/******************************************************************************
 * SDEC DTV APIs
 *****************************************************************************/
static int SDEC_DeliverPrivateInfo( enum pin_type pin_type, int pin_port,
	 int infoId, char *pInfo)
{
	int res = -1;
	/*pack info and send to inband buffer*/
	if (infoId == PRIVATEINFO_PTS) {
		PTS_INFO cmd;
		cmd.header.type = INBAND_CMD_TYPE_PTS;
		cmd.header.size = sizeof(PTS_INFO);
		cmd.PTSH = (*(int64_t *)pInfo) >> 32;
		cmd.PTSL = *(int64_t *)pInfo;

		res = rdvb_dmxbuf_writeIB(pin_type, pin_port, (void*)&cmd,
					sizeof(PTS_INFO), &cmd.wPtr, false);
	} else if (infoId == PRIVATEINFO_VIDEO_NEW_SEG) {
		NEW_SEG cmd;
		cmd.header.type = INBAND_CMD_TYPE_NEW_SEG;
		cmd.header.size = sizeof(NEW_SEG);
		//cmd.wPtr = SDEC_reverseInteger(pEsBuf->pHeader->writePtr);
		res = rdvb_dmxbuf_writeIB(pin_type, pin_port, (void*)&cmd,
					sizeof(NEW_SEG), &cmd.wPtr, false);
		dmx_err(NOCH, "NEW_SEG: res:%d, size:%d\n", res, sizeof(NEW_SEG));
	} else if (infoId == PRIVATEINFO_AUDIO_NEW_SEG) {
		AUDIO_DEC_NEW_SEG cmd;
		cmd.header.type = AUDIO_DEC_INBAND_CMD_TYPE_NEW_SEG;
		cmd.header.size = sizeof(AUDIO_DEC_NEW_SEG);
		//cmd.wPtr = SDEC_reverseInteger(pEsBuf->pHeader->writePtr);
		res = rdvb_dmxbuf_writeIB(pin_type, pin_port, (void *)&cmd,
			sizeof(AUDIO_DEC_NEW_SEG), (unsigned int *)&cmd.wPtr, false);

	} else if (infoId == PRIVATEINFO_VIDEO_DECODE_COMMAND) {
		DECODE cmd;
		int64_t duration = -1;
		cmd.header.type = INBAND_CMD_TYPE_DECODE;
		cmd.header.size = sizeof(DECODE);
		cmd.RelativePTSH = 0;
		cmd.RelativePTSL = 0;
		cmd.PTSDurationH = duration >> 32;
		cmd.PTSDurationL = duration;
		cmd.skip_GOP     = 0;
		cmd.mode         = *((DECODE_MODE*)pInfo);

		res = rdvb_dmxbuf_writeIB(pin_type, pin_port, (void *)&cmd,
			sizeof(DECODE),NULL, false);
		dmx_err(NOCH, "DECODE_MODE: res:%d, size:%d\n",
								res, sizeof(VIDEO_DECODE_MODE));

	} else if (infoId == PRIVATEINFO_AUDIO_DECODE_COMMAND) {
		AUDIO_DEC_DECODE cmd;
		int64_t duration = -1;
		cmd.header.type = AUDIO_DEC_INBAND_CMD_TYPE_DECODE;
		cmd.header.size = sizeof(AUDIO_DEC_DECODE);
		cmd.RelativePTSH = 0;
		cmd.RelativePTSL = 0;
		cmd.PTSDurationH = duration >> 32;
		cmd.PTSDurationL = duration;
		cmd.decodeMode   = AUDIO_DECODE;
		//cmd.wPtr = SDEC_reverseInteger(pEsBuf->pHeader->writePtr);
		res = rdvb_dmxbuf_writeIB(pin_type, pin_port, (void *)&cmd,
			sizeof(AUDIO_DEC_DECODE), (unsigned int *)&cmd.wPtr, false);

	} else if (infoId == PRIVATEINFO_AUDIO_FORMAT) {
		res = rdvb_dmxbuf_writeIB(pin_type, pin_port, pInfo,
			sizeof(AUDIO_DEC_NEW_FORMAT), 
			(unsigned int *)&((AUDIO_DEC_NEW_FORMAT *)pInfo)->wPtr, false);

	} else if (infoId == PRIVATEINFO_VIDEO_NEW_DECODE_MODE) {
		VIDEO_DECODE_MODE cmd;
		cmd.header.type = VIDEO_DEC_INBAND_CMD_TYPE_NEW_DECODE_MODE;
		cmd.header.size = sizeof(VIDEO_DECODE_MODE);
		//cmd.wPtr = SDEC_reverseInteger(pEsBuf->pHeader->writePtr);
		cmd.mode = *(DECODE_MODE *)pInfo;
		res = rdvb_dmxbuf_writeIB(pin_type, pin_port, (void *)&cmd,
			sizeof(VIDEO_DECODE_MODE), &cmd.wPtr, false);
		dmx_err(NOCH, "NEW_DECODE_MODE: res:%d, size:%d\n",
							res, sizeof(VIDEO_DECODE_MODE));

	} else if (infoId == PRIVATEINFO_VIDEO_INBAND_CMD_TYPE_SOURCE_DTV) {
		INBAND_SOURCE_DTV cmd;
		cmd.header.type = VIDEO_INBAND_CMD_TYPE_SOURCE_DTV;
		cmd.header.size = sizeof(INBAND_SOURCE_DTV);
		//cmd.wPtr = SDEC_reverseInteger(pEsBuf->pHeader->writePtr);
		res = rdvb_dmxbuf_writeIB(pin_type, pin_port, (void *)&cmd,
			sizeof(INBAND_SOURCE_DTV), &cmd.wPtr, false);
		dmx_err(NOCH, "SOURCE_DTV: res:%d, size:%d\n",
								res, sizeof(INBAND_SOURCE_DTV));

	} else if (infoId == PRIVATEINFO_AUDIO_AD_INFO) {
		AUDIO_DEC_AD_DESCRIPTOR cmd;
		memset(&cmd,0, sizeof(AUDIO_DEC_AD_DESCRIPTOR));
		cmd.header.type =  AUDIO_DEC_INBAND_CMD_TYPE_AD_DESCRIPTOR;
		cmd.header.size = sizeof(AUDIO_DEC_AD_DESCRIPTOR);
		//bs-ptr
		//cmd.wPtr = SDEC_reverseInteger(pEsBuf->pHeader->writePtr);
		//pts
		cmd. PTSH = 0;
		cmd. PTSL = 0;
		//adinfo
		cmd.AD_fade_byte       = (((AUDIO_AD_INFO *)pInfo)->AD_fade_byte);
		cmd.AD_pan_byte        = (((AUDIO_AD_INFO *)pInfo)->AD_pan_byte);
		cmd.gain_byte_center   = (((AUDIO_AD_INFO *)pInfo)->AD_gain_byte_center);
		cmd.gain_byte_front    = (((AUDIO_AD_INFO *)pInfo)->AD_gain_byte_front);
		cmd.gain_byte_surround = (((AUDIO_AD_INFO *)pInfo)->AD_gain_byte_surround);
		res = rdvb_dmxbuf_writeIB(pin_type, pin_port, (void *)&cmd,
			sizeof(AUDIO_DEC_AD_DESCRIPTOR), (unsigned int *)&cmd.wPtr, false);
	}

	return res;
}
static void SDEC_check_freerun(SDEC_CHANNEL_T *pCh, uint64_t pts, bool is_video)
{
	int64_t masterPTS = -1, RCD = -1;
	if (pCh->avSyncMode != NAV_AVSYNC_PCR_MASTER)
		return;
	if (is_video == false)
		return;
	RCD = REFCLK_GetRCD(pCh->pcrProcesser.pRefClock);
	masterPTS = RCD + CLOCK_GetAVSyncPTS(REFCLK_GetClockMode(pCh->pcrProcesser.pRefClock));

	if (RCD == -1)
		return;
	if (pts - masterPTS > pCh->pcrProcesser.v_fr_thrhd || pts < masterPTS) {
		if (pCh->video_status.freerun_count < 5)
		pCh->video_status.freerun_count++;
		if (pCh->video_status.bfreerun == false)
			dmx_err(pCh->idx, "{VFREERUN}[%d]RCD:%llx, masterPTS:%llx, pts:%llx,[%lld] [%d]\n",
				pCh->video_status.bfreerun, RCD, masterPTS, pts,
				pts- masterPTS,pCh->video_status.freerun_count);
	} else {
		if (pCh->video_status.freerun_count > -5)
			pCh->video_status.freerun_count--;
		if (pCh->video_status.bfreerun == true)
			dmx_err(pCh->idx, "{VFREERUN}[%d]RCD:%llx, masterPTS:%llx, pts:%llx,[%lld] [%d]\n",
				pCh->video_status.bfreerun, RCD, masterPTS, pts,
				pts- masterPTS,pCh->video_status.freerun_count);
	}
	if ((pCh->video_status.freerun_count == 5) && (pCh->video_status.bfreerun == false)) {
		REFCLK_SetVideoFreeRunThreshold(pCh->pcrProcesser.pRefClock, 0);
		pCh->video_status.bfreerun = true;
		dmx_err(pCh->idx, "{VFREERUN} video enter freerun,RCD:%llx, masterPTS:%llx, pts:%llx\n",
			RCD, masterPTS, pts);
	} else if ((pCh->video_status.freerun_count == -5)&& (pCh->video_status.bfreerun == true)){
		REFCLK_SetVideoFreeRunThreshold(pCh->pcrProcesser.pRefClock, pCh->pcrProcesser.v_fr_thrhd);
		pCh->video_status.bfreerun = false;
		dmx_err(pCh->idx, "{VFREERUN} video Exit freerun,RCD:%llx, masterPTS:%llx, pts:%llx\n",
			RCD, masterPTS, pts);
	}
}

static void SDEC_UpdateAFMode(SDEC_CHANNEL_T *pCh)
{
	REFCLOCK * const pRefClock = pCh->refClock.prefclk;
	MASTERSHIP * mastership = &pRefClock->mastership;
	PCRSYNC_T *pPcrProcesser = &pCh->pcrProcesser;

	if(pCh->avSyncMode != NAV_AVSYNC_AUDIO_MASTER_AF || !pCh->bCheckAFState)
		return;

	if(REFCLK_GetRCD(pRefClock) != -1) {
		pCh->bCheckAFState = false;
		mastership->videoMode    = (unsigned char)AVSYNC_AUTO_SLAVE;
		mastership->audioMode    = (unsigned char)AVSYNC_AUTO_MASTER_NO_SKIP;
		mastership->masterState  = (unsigned char)AUTOMASTER_IS_MASTER;
		dmx_crit(pCh->idx, "audio is master\n");
	} else if (CLOCK_GetPTS() >= pCh->timeToUpdteAF) {
		pCh->bCheckAFState = false;
		mastership->videoMode    = (unsigned char)AVSYNC_AUTO_SLAVE;
		mastership->audioMode    = (unsigned char)AVSYNC_AUTO_MASTER_NO_SKIP;
		if (pPcrProcesser->startAudioPts != -1 &&
			pPcrProcesser->startAudioPts <= pPcrProcesser->startVideoPts)
			mastership->masterState = (unsigned char)AUTOMASTER_IS_MASTER;
		else
			mastership->masterState = (unsigned char)AUTOMASTER_NOT_MASTER;
		dmx_crit(pCh->idx, "masterState = %d\n", mastership->masterState);
	}
	//pCh->preMasterState = pRefClock->mastership.masterState;

}
static int SDEC_DeliverNavBufCommands(SDEC_CHANNEL_T *pCh, NAVBUF *pNavBuffer)
{
	switch (pNavBuffer->type) {
	case NAVBUF_NONE:
		break;

	case NAVBUF_WAIT:
		msleep(pNavBuffer->wait.waitTime);
		break;

	case NAVBUF_SHORT_WAIT:
		msleep(pNavBuffer->wait.waitTime);
		break;

	default:
		dmx_err(pCh->idx, "Unknown bufferType : %d\n", pNavBuffer->type);
	}

	return 0;
}
static inline void buf_sync(unsigned char chid)
{
	unsigned int i;
	for (i =0; i < NUMBER_OF_PINS;i++){
		enum pin_type pin_type = PIN_TYPE_MAX;
		int pin_port = VTP_PORT_MAX;
		if (i == VIDEO_PIN) {
			pin_type = PIN_TYPE_VTP;
			pin_port = VTP_PORT_0;
		} else if (i == VIDEO2_PIN) {
			pin_type = PIN_TYPE_VTP;
			pin_port = VTP_PORT_1;
		} else if (i == AUDIO_PIN) {
			pin_type = PIN_TYPE_ATP;
			pin_port = ATP_PORT_0;
		} else if (i == AUDIO2_PIN) {
			pin_type = PIN_TYPE_ATP;
			pin_port = ATP_PORT_1;
		}

		rdvb_dmxbuf_sync(pin_type, pin_port, chid);

	}
}

static void demux_calc_data_size(NAVDEMUXOUT * pDemuxOut)
{
	unsigned int i;
	NAVBUF *pNavBuffer;

	for (i = 0; i < pDemuxOut->numBuffers ; i++) {
		struct ts_parse_context * tpc;
		pNavBuffer = &pDemuxOut->pNavBuffers[i];
		tpc = pNavBuffer->data.tpc;
		if (tpc == NULL) {
			continue;
		}
		tpc_set_deliver_size(tpc, pNavBuffer->data.payloadSize);
	}

}

static int demux_varify_writable(struct list_head* tpc_head, 
		NAVDEMUXOUT *pDemuxOut, bool no_drop)
{
	tpc_reset_deliver_size(tpc_head);
	demux_calc_data_size(pDemuxOut);
	//tpc_check_vo_pic_level(tpc_head);
	return tpc_varify_writable(tpc_head, no_drop);

}

static int SDEC_DeliverData(SDEC_CHANNEL_T *pCh, NAVDEMUXOUT *pDemuxOut)
{
	unsigned int i;
	NAVBUF *pNavBuffer;

	if (demux_varify_writable(&pCh->tpc_list_head, pDemuxOut,
		 pCh->is_playback) < 0){
		return -1;
	}

	if (pCh->is_playback == false)
		tpc_flush_vo(&pCh->tpc_list_head);

	for (i = 0; i < pDemuxOut->numBuffers ; i++) {
		enum pin_type pin_type;
		int pin_port;
		struct ts_parse_context * tpc;
		pNavBuffer = &pDemuxOut->pNavBuffers[i];
		tpc = pNavBuffer->data.tpc;

		if (pNavBuffer->type != NAVBUF_DATA
			&& pNavBuffer->type != NAVBUF_DATA_EXT) {
			buf_sync(pCh->idx);
			SDEC_DeliverNavBufCommands(pCh, pNavBuffer);
			continue ;
		}

		if (tpc == NULL) {
			continue;
		}
		pin_type = tpc_get_pin_type(tpc->data_context_type);
		pin_port = tpc->data_port_num;
		if ((int)pin_type < 0 || pin_port < 0) {
			continue;
		}
		if (pNavBuffer->data.infoId != PRIVATEINFO_NONE) {
			rdvb_dmxbuf_sync(pin_type, pin_port, pCh->idx);

			if (dmx_ib_send_ad_info(pin_type, pin_port,
				pNavBuffer->data.infoData, false) < 0) {
				pDemuxOut->pNavBuffers = &pDemuxOut->pNavBuffers[i];
				pDemuxOut->numBuffers -= i;
				return -1 ;
			}
		}
		/* no payload, move on to the next navbuf */
		if (pNavBuffer->data.payloadSize <= 0){
			continue;
		}

		if (pNavBuffer->data.pts >= 0) {

			if (pCh->tp_src == MTP &&
				g_dbgSetting[pCh->idx].esbufmonitor_off == 0) {
				if (pin_type == PIN_TYPE_ATP ||
					pCh->isAudioPaused == true ||
					pCh->avSyncMode == NAV_AVSYNC_VIDEO_ONLY ||
					pCh->pcrProcesser.lastDemuxAudioPTS == -1) {
					if (sdec_pb_checkESBufStable(pCh)) {
						rdvb_dmxbuf_sync(pin_type, pin_port, pCh->idx);
						pDemuxOut->pNavBuffers = &pDemuxOut->pNavBuffers[i];
						pDemuxOut->numBuffers -= i;
						return -1;
					} else if (pCh->pvrPbSpeed ==0) {
						dmx_mask_print(DEMUX_PVR_ES_MONITOR_DEBUG, pCh->idx,
							"ch%d, deliverdata in stepMode: deliverNum:%d \n",
							pCh->idx, pDemuxOut->numBuffers );
					}
				}
			}

			if(tpc->is_ad == false) {
				PCRSYNC_UpdateDemuxPTS(&pCh->pcrProcesser,
					tpc->data_context_type == DATA_CONTEXT_TYPE_VIDEO,
					pNavBuffer->data.pts);
				SDEC_check_freerun(pCh, pNavBuffer->data.pts,
					tpc->data_context_type == DATA_CONTEXT_TYPE_VIDEO);
			}

			PCRSYNC_CheckPTSChange(&pCh->pcrProcesser);
			PCRSYNC_CheckPTSDataStable(&pCh->pcrProcesser);

			sdec_pb_buffering(pCh);
			rdvb_dmxbuf_sync(pin_type, pin_port, pCh->idx);


			if (dmx_ib_send_pts(pin_type, pin_port,
							(char *)&pNavBuffer->data.pts, pCh->is_ecp) < 0) {
				pDemuxOut->pNavBuffers = &pDemuxOut->pNavBuffers[i];
				pDemuxOut->numBuffers -= i;
				dmx_err(pCh->idx,
						"ERROR: deliver PTS fail :pin:%d, port:%d\n",
						pin_type, pin_port);
				return -1;
			}

		}

		rdvb_dmxbuf_write(pin_type, pin_port, pNavBuffer->data.payloadData,
			pNavBuffer->data.payloadSize);
	}

	buf_sync(pCh->idx);
	pDemuxOut->numBuffers = 0;

	return 0;
}
static void SDEC_Flush(SDEC_CHANNEL_T *pCh, int target)
{
	int  i = 0;
	if (pCh == NULL || target < 0 || target >= DEMUX_NUM_OF_TARGETS) {
		dmx_err(CHANNEL_UNKNOWN, "invalid param target:%d\n", target);
		return;
	}
	if ((pCh->AdChannelTarget != -1) &&(pCh->AdChannelTarget == target)) {
		pCh->AdChannelTarget = -1;
		SET_FLAG(pCh->events, EVENT_FLUSH_AD_INFO);
	}

	//flush demuxout data
	for (i = 0; i < pCh->demuxOutReserved.numBuffers; i++) {
		if (pCh->demuxOutReserved.pNavBuffers[i].data.pinIndex == target) {
			dmx_mask_print(DEMUX_AUDIO_STABLE_DEBUG,  pCh->idx,  
				"flush dirty data of pin %d ,pts:%lld\n",
				target, pCh->demuxOutReserved.pNavBuffers[i].data.pts);
			pCh->demuxOutReserved.pNavBuffers[i].type = NAVBUF_NONE;
			pCh->demuxOutReserved.pNavBuffers[i].data.pinIndex = DEMUX_TARGET_DROP;
		}
	}
	if (pCh->tp_src == MTP) {
		REFCLOCK * const pRefClock = pCh->refClock.prefclk;
		if (IS_AUDIO_TARGET(target)) {
			dmx_mask_print(DEMUX_AUDIO_STABLE_DEBUG,  pCh->idx,  
				"reset pin:%d,  \n", target);

			pCh->pcrProcesser.lastDemuxAudioPTS = -1;
			pCh->pcrProcesser.startAudioPts = -1;
			pRefClock->audioSystemPTS = reverse_int64(-1);

		} else if (IS_VIDEO_TARGET(target)) {
			dmx_mask_print(DEMUX_AUDIO_STABLE_DEBUG,  pCh->idx,  
				"reset pin:%d,  \n", target);

			pCh->pcrProcesser.lastDemuxVideoPTS = -1;
			pRefClock->videoSystemPTS = reverse_int64(-1);

		}
	}
}
static int SDEC_FlushVideo(SDEC_CHANNEL_T *pCh,  int pin)
{
	if (pCh == NULL) {
		dmx_err(CHANNEL_UNKNOWN, "ERROR!!!, invalid param\n");
		return -1;
	}
	SDEC_Flush(pCh, (pin == VIDEO_PIN) ?
							DEMUX_TARGET_VIDEO : DEMUX_TARGET_VIDEO_2ND);
	REFCLK_Reset(pCh->refClock.prefclk, pCh->avSyncMode, pCh->v_fr_thrhd);
	PCRSYNC_Reset(&pCh->pcrProcesser);
	FULLCHECK_Reset(&pCh->fullCheck, 0, 1);

	if (pin == VIDEO_PIN)
		SET_FLAG(pCh->events, EVENT_SET_VIDEO_START);
	else
		SET_FLAG(pCh->events, EVENT_SET_VIDEO2_START);

	return 0;
}
static int SDEC_FlushAudio(SDEC_CHANNEL_T *pCh, int pin)
{
	if (pCh == NULL) {
		dmx_err(CHANNEL_UNKNOWN, "ERROR!!!, invalid param\n");
		return -1;
	}
	SDEC_Flush(pCh, (pin == AUDIO_PIN) ?
						DEMUX_TARGET_AUDIO : DEMUX_TARGET_AUDIO_2ND);
	FULLCHECK_Reset(&pCh->fullCheck, pin, 0);

	if (pin == AUDIO_PIN)
		SET_FLAG(pCh->events, EVENT_SET_AUDIO_START);
	else
		SET_FLAG(pCh->events, EVENT_SET_AUDIO2_START);


	return 0;
}
static void SDEC_FlushTPBuffer(SDEC_CHANNEL_T *pCh)
{
	if (pCh->AdChannelTarget != -1)
		SET_FLAG(pCh->events, EVENT_FLUSH_AD_INFO);


	//flush demuxIn & demuxInReserved
	pCh->demuxIn.pBegin  = pCh->demuxIn.pEnd = pCh->demuxIn.pCurr = 0;
	pCh->demuxInReserved.pBegin  =
	pCh->demuxInReserved.pEnd    =
	pCh->demuxInReserved.pCurr   = 0;

	//flush demuxout && demuxoutReserved
	pCh->demuxOutReserved.numBuffers = 0;
	pCh->demuxOut.numBuffers = 0;

	// release tp buffer
	RELEASE_TP_BUFFER();
	RHAL_TPFlushBuffer(pCh->idx);
}
int SDEC_EnablePCRTracking(SDEC_CHANNEL_T *pCh)
{
	TPK_PCR_CLK_SRC_T clk_src;
	INT32 iClkRet;
	REFCLOCK *pRefClock = 0;
	if (pCh== NULL)
		return -1;

	pRefClock = pCh->refClock.prefclk;
	/* Don't start pcr recovery while in MTP status */
	if (pCh->tp_src == MTP) return 0;


	if (!pCh->pClockRecovery) {
		dmx_err(pCh->idx, 
			"fail to enable pcrtracking, no support pcrtracking\n");
		return -1;
	}
	
	pCh->pcrProcesser.pid = pCh->PCRPID;
	if ( 1 == pCh->bPcrTrackEnable) {

		/* Change clock mode */
		clk_src = PCRTRACK_ChoosePCRClock(pCh->idx);
		dmx_mask_print(DEMUX_PCRTRACK_DEBUG,  pCh->idx, 
			"\033[1;36m (dtv%d)Set tp&ref clk=%d\033[m\n",pCh->idx,clk_src );
		iClkRet = RHAL_PCRTrackingEnable(pCh->idx, true, pCh->PCRPID, clk_src);
		if( TPK_SUCCESS != iClkRet ) {
			dmx_notice(pCh->idx,
				"\033[1;31m (dtv%d)Set tp clk=%d Failed,ret=%d,PID=0x%x\033[m\n",
				pCh->idx , clk_src , iClkRet , pCh->PCRPID);
			return -1;
		}

		REFCLK_SetClockMode(pRefClock, clk_src);

		/* Reset AV Sync variables */
		/* Start PcrTracking State Machine */
		if (PCRTRACK_Start(pCh->pClockRecovery, pCh->idx) ==false ) {
			dmx_mask_print(DEMUX_PCRTRACK_DEBUG,  pCh->idx, 
				"\031[1;36m  fail to start PcrTracking \033[m\n");
			return -1;
		}

	} else {
		clk_src = MISC_90KHz;
		dmx_mask_print(DEMUX_PCRTRACK_DEBUG,  pCh->idx,
			 "\033[1;36m (dtv%d)Set tp&ref clk=%d\033[m\n",

			 pCh->idx,clk_src );
		iClkRet = RHAL_PCRTrackingEnable(pCh->idx, true, pCh->PCRPID, clk_src);
		if ( TPK_SUCCESS == iClkRet ) {
			REFCLK_SetClockMode(pRefClock, clk_src);
		} else {
			dmx_notice(pCh->idx,
				"\033[1;31m (dtv%d)Set tp clk=%d Failed,ret=%d,PID=0x%x\033[m\n",
				pCh->idx , clk_src , iClkRet , pCh->PCRPID);
		}
	}


	return 0;
}
int SDEC_DisablePCRTracking(SDEC_CHANNEL_T *pCh)
{
	enum clock_type clk_src;
	INT32 iClkRet;
	REFCLOCK *pRefClock = 0;
	if (pCh== NULL)
		return -1;
	if (pCh->tp_src == MTP) return 0;
	if (pCh->pClockRecovery) {
		/* Stop PcrTracking State Machine */
		PCRTRACK_Stop(pCh->pClockRecovery);
	}

	pRefClock = pCh->refClock.prefclk;
	/* change clock mode */
	clk_src = MISC_90KHz;
	pCh->pcrProcesser.pid = 0x1fff;
	if (pCh->pcrProcesser.bUseFixedRCD) {
		clk_src = REFCLK_GetClockMode(pRefClock);
		//dmx_must_print("\033[1;36m bUseFixedRCD =true RCD=%lld\033[m\n",
	}
	dmx_mask_print(DEMUX_PCRTRACK_DEBUG,  pCh->idx, "\033[1;36m Ch =%d\033[m\n",
	pCh->idx );
	if (0 < pCh->PCRPID && pCh->PCRPID < 0x1FFF)
		iClkRet = RHAL_PCRTrackingEnable(pCh->idx, true , pCh->PCRPID, clk_src);
	else
		iClkRet = RHAL_PCRTrackingEnable(pCh->idx, false , pCh->PCRPID, clk_src);

	if( TPK_SUCCESS == iClkRet ) {

		REFCLK_SetClockMode(pRefClock, clk_src);

		/* Reset AV Sync variables */
		PCRSYNC_Reset(&pCh->pcrProcesser);
	} else {
		dmx_notice(pCh->idx,
			"\033[1;31m (dtv%d)Set tp clk=%d Failed,ret=%d,PID=0x%x\033[m\n",
			pCh->idx , clk_src , iClkRet , pCh->PCRPID);
	}
	return 0;
}
//event no need  finish handle. befor go process data.
//may be try more than one times.
static void SDEC_HandleUnpedingEvents(SDEC_CHANNEL_T *pCh)
{
	if (HAS_FLAG(pCh->status, STATUS_PCRTAK_STOP)) {
		SDEC_DisablePCRTracking(pCh);
		RESET_FLAG(pCh->status, STATUS_PCRTAK_STOP);
	}

	if (HAS_FLAG(pCh->status, STATUS_PCRTAK_START)) {
		if (SDEC_EnablePCRTracking(pCh)<0) {
			dmx_mask_print(DEMUX_PCRTRACK_DEBUG,  pCh->idx,
				"\033[1;36mfail to start PcrTracking \033[m\n");
		} else {
			RESET_FLAG(pCh->status, STATUS_PCRTAK_START);
			dmx_mask_print(DEMUX_PCRTRACK_DEBUG|DEMUX_NOTICE_PRINT, pCh->idx,
			 "start PcrTracking....\n");
		}
	}


	if (HAS_FLAG(pCh->status, STATUS_FLUSH_VIDEO)) {
		SDEC_FlushVideo(pCh, VIDEO_PIN);
		RESET_FLAG(pCh->status, STATUS_FLUSH_VIDEO);
	}

	if (HAS_FLAG(pCh->status, STATUS_FLUSH_VIDEO2)) {
		SDEC_FlushVideo(pCh, VIDEO2_PIN);
		RESET_FLAG(pCh->status, STATUS_FLUSH_VIDEO2);
	}
	if (HAS_FLAG(pCh->status, STATUS_FLUSH_AUDIO)) {
		SDEC_FlushAudio(pCh, VIDEO_PIN);
		RESET_FLAG(pCh->status, STATUS_FLUSH_AUDIO);
	}

	if (HAS_FLAG(pCh->status, STATUS_FLUSH_AUDIO2)) {
		SDEC_FlushAudio(pCh, VIDEO2_PIN);
		RESET_FLAG(pCh->status, STATUS_FLUSH_AUDIO2);
	}
	if (HAS_FLAG(pCh->status, STATUS_FLUSH_TP)) {
		SDEC_FlushTPBuffer(pCh);
		RESET_FLAG(pCh->status, STATUS_FLUSH_TP);
	}

}
static int SDEC_StartVideo(SDEC_CHANNEL_T *pCh, DECODE_MODE decMode, int pin)
{
	//BUF_INFO_T *pEsBuf, *pIbBuf;
	int pin_port = -1;
	if (pCh == NULL) {
		dmx_err(CHANNEL_UNKNOWN, "ERROR!!!, invalid param\n");
		return -1;
	}
	if (pin == VIDEO_PIN) pin_port = VTP_PORT_0;
	else if (pin == VIDEO2_PIN) pin_port = VTP_PORT_1;
	else
		return -1;

	if (dmx_ib_send_video_new_seg(PIN_TYPE_VTP, pin_port, pCh->is_ecp) < 0){
		dmx_mask_print(DEMUX_NOMAL_DEBUG,  pCh->idx,
			"Deliver PRIVATEINFO_VIDEO_NEW_SEG FAIL \n");
		return -1;
	}
	/* ANDROIDTV-227 issue */
	if (dmx_ib_send_video_dtv_src(PIN_TYPE_VTP, pin_port, pCh->is_ecp) < 0) {
		dmx_mask_print(DEMUX_NOMAL_DEBUG,  pCh->idx, 
			"Deliver PRIVATEINFO_VIDEO_INBAND_CMD_TYPE_SOURCE_DTV FAIL \n");
		return -1;
	}
	if (dmx_ib_send_video_decode_cmd(PIN_TYPE_VTP, pin_port, (void *)&decMode, pCh->is_ecp)<0){
		dmx_mask_print(DEMUX_NOMAL_DEBUG,  pCh->idx, 
			"Deliver PRIVATEINFO_VIDEO_DECODE_COMMAND FAIL \n");
		return -1;
	}

	if (pCh->tp_src != MTP)
		SDEC_ResetAVSync(pCh, NAV_AVSYNC_PCR_MASTER);
	else if(pCh->avSyncMode == MTP_AVSYNC_MODE && pCh->tp_src == MTP)
	    SDEC_ResetAVSync(pCh, MTP_AVSYNC_MODE);

	DEBUG_DumpEsFlush(pCh->idx, pin);
	return 0;
}
int SDEC_StartAudio(SDEC_CHANNEL_T *pCh,  int pin)
{
	//BUF_INFO_T *pEsBuf, *pIbBuf;
	int pin_port = -1;

	if (pCh == NULL) {
		dmx_err(CHANNEL_UNKNOWN, "ERROR!!!, invalid param\n");
		return -1;
	}

	if (pin == AUDIO_PIN) pin_port = ATP_PORT_0;
	else if (pin == AUDIO2_PIN) pin_port = ATP_PORT_1;
	else
		return -1;

	//pEsBuf = &pCh->pinBuf[pin].es;
	//pIbBuf = &pCh->pinBuf[pin].ib;
	if (SDEC_DeliverPrivateInfo(PIN_TYPE_ATP, pin_port,
									PRIVATEINFO_AUDIO_NEW_SEG, NULL) < 0) {
		dmx_mask_print(DEMUX_NOMAL_DEBUG,  pCh->idx,
			"ERROR: Deliver PRIVATEINFO_AUDIO_NEW_SEG FAIL \n");
		return -1;
	}
	if (SDEC_DeliverPrivateInfo(PIN_TYPE_ATP, pin_port,
							PRIVATEINFO_AUDIO_DECODE_COMMAND, NULL) < 0) {
		dmx_mask_print(DEMUX_NOMAL_DEBUG,  pCh->idx,
			"Deliver PRIVATEINFO_AUDIO_DECODE_COMMAND FAIL \n");
		return -1;
	}

	DEBUG_DumpEsFlush(pCh->idx, pin);
	return 0;

}
static int SDEC_AudioNewFormat(SDEC_CHANNEL_T *pCh, int pin,
								AUDIO_DEC_TYPE audioFormat, s32 privateInfo[8])
{
	//int pid;
	//BUF_INFO_T *pEsBuf, *pIbBuf;
	int target;
	AUDIO_DEC_NEW_FORMAT cmd;
	cmd.header.type = AUDIO_DEC_INBAMD_CMD_TYPE_NEW_FORMAT;
	cmd.header.size = sizeof(AUDIO_DEC_NEW_FORMAT);
	cmd.audioType   = audioFormat;
	cmd.wPtr        = 0;
	memcpy(cmd.privateInfo, privateInfo, sizeof (cmd.privateInfo));

	target = (pin == AUDIO_PIN) ? DEMUX_TARGET_AUDIO : DEMUX_TARGET_AUDIO_2ND;
	SDEC_Flush(pCh, target);

	if (dmx_ib_send_audio_new_fmt(PIN_TYPE_ATP,(pin == AUDIO_PIN)?ATP_PORT_0:ATP_PORT_1,
									audioFormat, privateInfo, pCh->is_ecp) < 0) {
		dmx_mask_print(DEMUX_NOMAL_DEBUG,  pCh->idx,
			"Deliver PRIVATEINFO_AUDIO_FORMAT FAIL !!!!\n");
		return -1;
	}
	dmx_mask_print(DEMUX_AUDIO_STABLE_DEBUG|DEMUX_PVR_ES_MONITOR_DEBUG,
			pCh->idx, "pin:%d, audioFormat:%d\n", pin, audioFormat);
	return 0;
}
static int SDEC_SendAudioDesc(AUDIO_AD_INFO *pAdInfo)
{
	struct AUDIO_CONFIG_COMMAND {
		s32 msgID;
		u32 value[15];
	};

	struct BUF_INFO adBuf;
	unsigned long ret;
	struct AUDIO_CONFIG_COMMAND *pConfig;

	if (dmx_buf_AllocBuffer(&adBuf, sizeof(struct AUDIO_CONFIG_COMMAND),
													NON_CACHED_BUFF, 1) < 0) {
		dmx_err(CHANNEL_UNKNOWN, "buff poll  out of memory\n");
		return -1;
	}

	pConfig = (struct AUDIO_CONFIG_COMMAND *)adBuf.viraddr;
	pConfig->msgID = reverse_int32(74); /* AUDIO_CONFIG_CMD_AD_DESCRIPTOR */
	pConfig->value[0] = reverse_int32(pAdInfo->AD_fade_byte);
	pConfig->value[1] = reverse_int32(pAdInfo->AD_pan_byte);
	pConfig->value[2] = reverse_int32(pAdInfo->AD_gain_byte_center);
	pConfig->value[3] = reverse_int32(pAdInfo->AD_gain_byte_front);
	pConfig->value[4] = reverse_int32(pAdInfo->AD_gain_byte_surround);

	dmx_dbg(NOCH, "send_rpc_command start\n");
	dmx_dbg(NOCH, "msgID = 0x%x, value[0] = 0x%x, value[1] = 0x%x, "
		"value[2] = 0x%x, value[3] = 0x%x, value[4] = 0x%x\n",
		 pConfig->msgID, pConfig->value[0], pConfig->value[1],
		 pConfig->value[2], pConfig->value[3], pConfig->value[4]);
	if (send_rpc_command(RPC_AUDIO, 0x204, adBuf.phyaddr,
											adBuf.bufSize, &ret) == RPC_FAIL)
		dmx_err(CHANNEL_UNKNOWN, "SEND audio descriptor RPC fail\n");

	dmx_dbg(NOCH, "send_rpc_command end\n");
	dmx_buf_FreeBuffer(&adBuf);
	return 0;
}
static void SDEC_HandleEvents(SDEC_CHANNEL_T *pCh, NAVBUF *pBuffer)
{
	if (HAS_FLAG(pCh->events, EVENT_VIDEO0_NEW_DECODE_MODE)) {
		if (SDEC_DeliverPrivateInfo(PIN_TYPE_VTP, VTP_PORT_0,
							PRIVATEINFO_VIDEO_NEW_DECODE_MODE,
							(char *)&pCh->videoDecodeMode[0]) < 0)
			dmx_err(NOCH, "deliver video0 new decoder mode fail \n");
		RESET_FLAG(pCh->events, EVENT_VIDEO0_NEW_DECODE_MODE);
	}

	if (HAS_FLAG(pCh->events, EVENT_VIDEO1_NEW_DECODE_MODE)) {
		if (SDEC_DeliverPrivateInfo(PIN_TYPE_VTP, VTP_PORT_1,
								PRIVATEINFO_VIDEO_NEW_DECODE_MODE,
								(char *)&pCh->videoDecodeMode[1]) < 0)
			dmx_err(NOCH, "deliver video1 new decoder mode fail \n");
		RESET_FLAG(pCh->events, EVENT_VIDEO1_NEW_DECODE_MODE);
	}


    //---video start--
	if (HAS_FLAG(pCh->events, EVENT_SET_VIDEO_START)) {
		if (SDEC_StartVideo(pCh, pCh->videoDecodeMode[0], VIDEO_PIN) < 0) {
			dmx_mask_print(DEMUX_NOMAL_DEBUG,  pCh->idx,
				"FlushVideo (%d)FAIL, try latter  \n", VIDEO_PIN);
		} else
			RESET_FLAG(pCh->events, EVENT_SET_VIDEO_START);
	}

	if (HAS_FLAG(pCh->events, EVENT_SET_VIDEO2_START)) {
		if (SDEC_StartVideo(pCh, pCh->videoDecodeMode[1], VIDEO2_PIN) < 0) {
			dmx_mask_print(DEMUX_NOMAL_DEBUG,  pCh->idx,
					"FlushVideo (%d)FAIL, try latter  \n", VIDEO2_PIN);
		} else
			RESET_FLAG(pCh->events, EVENT_SET_VIDEO2_START);
	}
	//------audio START----
	if (HAS_FLAG(pCh->events, EVENT_SET_AUDIO_START)) {
		if (SDEC_StartAudio(pCh, AUDIO_PIN) < 0) {
			dmx_mask_print(DEMUX_NOMAL_DEBUG,  pCh->idx,
				"FlushVideo (%d)FAIL, try latter  \n", AUDIO_PIN);
		} else
			RESET_FLAG(pCh->events, EVENT_SET_AUDIO_START);
	}

	if (HAS_FLAG(pCh->events, EVENT_SET_AUDIO2_START)) {
		if (SDEC_StartAudio(pCh, AUDIO2_PIN) < 0) {
			dmx_mask_print(DEMUX_NOMAL_DEBUG,  pCh->idx,
				"FlushVideo (%d)FAIL, try latter \n", AUDIO2_PIN);
		} else
			RESET_FLAG(pCh->events, EVENT_SET_AUDIO2_START);
	}
	//-----audio--new format--
	if (HAS_FLAG(pCh->events, EVENT_SET_AUDIO_FORMAT)) {
		if (SDEC_AudioNewFormat(pCh, AUDIO_PIN, pCh->audioFormat,
												pCh->audioPrivInfo) < 0) {
			dmx_mask_print(DEMUX_NOMAL_DEBUG,  pCh->idx, 
				"SetAudioFormat (%d)FAIL, try latter\n", AUDIO_PIN);
		} else
			RESET_FLAG(pCh->events, EVENT_SET_AUDIO_FORMAT);

	}
	//--------AUDIO 2--NEW FORMATE--
	if (HAS_FLAG(pCh->events, EVENT_SET_AUDIO2_FORMAT)) {
		if (SDEC_AudioNewFormat(pCh, AUDIO2_PIN, pCh->audioFormat2,
											pCh->audioPrivInfo2) < 0) {
			dmx_mask_print(DEMUX_NOMAL_DEBUG,  pCh->idx,
				"SetAudioFormat (%d)FAIL, try latter\n", AUDIO2_PIN);
		} else 
			RESET_FLAG(pCh->events, EVENT_SET_AUDIO2_FORMAT);
	}
	//---- FLUSH AD_INFO--
	if (HAS_FLAG(pCh->events, EVENT_FLUSH_AD_INFO)) {
		dmx_notice(CHANNEL_UNKNOWN, "handle EVENT_FLUSH_AD_INFO\n");
		pCh->adInfo.AD_fade_byte = 0x0;
		pCh->adInfo.AD_pan_byte = 0x0;
		pCh->adInfo.AD_gain_byte_center = 0x0;
		pCh->adInfo.AD_gain_byte_front = 0x0;
		pCh->adInfo.AD_gain_byte_surround = 0x0;
		pCh->AdChannelTarget = -1;
		SDEC_SendAudioDesc(&pCh->adInfo);
		RESET_FLAG(pCh->events, EVENT_FLUSH_AD_INFO);
	}
}
static bool SDEC_CheckStatus(SDEC_CHANNEL_T *pCh, NAVBUF *pBuffer, bool isProhibited)
{

	if (pCh == NULL || pBuffer == NULL)
		return false;

	if (pCh->status)
		SDEC_HandleUnpedingEvents(pCh);

	if (isProhibited) {
		pBuffer->type = NAVBUF_WAIT;
		pBuffer->wait.waitTime = 10;
		return true;
	}

	if (pCh->events != 0) {
		pBuffer->type = NAVBUF_NONE;
		SDEC_HandleEvents(pCh, pBuffer);
		return true;
	}

	return false;
}
static int SDEC_ReadData(SDEC_CHANNEL_T *pCh, NAVBUF *pBuffer)
{
	int ret;
	unsigned char *pLastReadAddr = 0;

	if (pCh->pTPRelase) {
		pLastReadAddr = pCh->pTPRelase + pCh->TPReleaseSize;
		if (pLastReadAddr >= (pCh->tpBuffer.nonCachedaddr + pCh->tpBuffer.size))
			pLastReadAddr -= pCh->tpBuffer.size;
	}
	RELEASE_TP_BUFFER();
	pBuffer->data.payloadData = 0;
	pBuffer->data.payloadSize = 0;

	ret = RHAL_TPReadData(pCh->idx, &pBuffer->data.payloadData,
					(UINT32 *)(&pBuffer->data.payloadSize), pCh->dataThreshold) ;
	if (pBuffer->data.payloadSize) {
		/* convert to cached address */
		pBuffer->data.payloadData -= pCh->tpBuffer.phyAddrOffset ;
		  /* align size to DTV_LEAST_DELIVER_SIZE before demuxing */
		pBuffer->data.payloadSize -= (pBuffer->data.payloadSize % pCh->tsPacketSize);

		if(pCh->pcrProcesser.pcrExistDetect.checkPacketReady == false)
			pCh->pcrProcesser.pcrExistDetect.checkPacketReady = true;
	}

	if (pLastReadAddr &&
		pBuffer->data.payloadSize > 0 &&
		pLastReadAddr != pBuffer->data.payloadData) {
		dmx_crit(pCh->idx, "ERROR: read ptr shift : "
			"last = 0x%p, now = 0x%p, tp = 0x%p/0x%lx\n", 
			pLastReadAddr, pBuffer->data.payloadData,
			pCh->tpBuffer.nonCachedaddr, (unsigned long)pCh->tpBuffer.phyAddr);
	}
	if (pCh->tp_src == MTP) { /* don't care buffer full on MTP mode */
		if (ret == TPK_BUFFER_FULL) {
			ret = TPK_SUCCESS;
			dmx_warn(pCh->idx, DGB_COLOR_RED_WHITE 
				" buffer full, addr %p, phyAddr %p, size %d, ret %d\n"
				DGB_COLOR_NONE,
				pBuffer->data.payloadData,
				pBuffer->data.payloadData + pCh->tpBuffer.phyAddrOffset,
				pBuffer->data.payloadSize, ret);
		}
		if (pBuffer->data.payloadSize > pCh->tsPacketSize*1024*2)
			pBuffer->data.payloadSize = pCh->tsPacketSize*1024*2;
	}

	if (ret == TPK_BUFFER_FULL) {
		dmx_warn(pCh->idx,DGB_COLOR_RED_WHITE
			"buffer full, tp %d, addr %p, phyAddr %p, size %d\n"
			DGB_COLOR_NONE,  pCh->idx, pBuffer->data.payloadData,
			pBuffer->data.payloadData+pCh->tpBuffer.phyAddrOffset,
			pBuffer->data.payloadSize);
		pBuffer->type = NAVBUF_SHORT_WAIT;
		pBuffer->wait.waitTime = TUNER_DATA_READ_WAIT_TIME;

		pCh->pTPRelase     = pBuffer->data.payloadData;
		pCh->TPReleaseSize = pBuffer->data.payloadSize;
		RELEASE_TP_BUFFER();
		//pCh->lastStampedPTS = -1;
		return 0;
	} else if (ret == TPK_SUCCESS &&
				pBuffer->data.payloadSize >= pCh->tsPacketSize) {

		pBuffer->type = NAVBUF_DATA;
		pBuffer->data.startAddress = 0xFFFFFFFF;
		pBuffer->data.infoId = PRIVATEINFO_NONE;
		/*  #ifdef IS_ARCH_ARM_COMMON */
		if (pCh->tpBuffer.phyAddrOffset != 0) {
			pBuffer->type =  NAVBUF_DATA_EXT;
			pBuffer->data.phyAddrOffset = pCh->tpBuffer.phyAddrOffset;
		}
		/*  #endif */

		pCh->pTPRelase     = pBuffer->data.payloadData;
		pCh->TPReleaseSize = pBuffer->data.payloadSize;
		pCh->dataThreshold = pCh->tsPacketSize * 1024;

		DEBUG_DumpTs(pCh->idx, pBuffer->data.payloadData,
											pBuffer->data.payloadSize);
	} else {
		pBuffer->type = NAVBUF_SHORT_WAIT;
		pBuffer->wait.waitTime = TUNER_DATA_READ_WAIT_TIME;//DEMUX_GetWaitTime(pCh);
		/* next time, no matter how much data in buffer, I will read anyway */
		pCh->dataThreshold = 0;
	}

	return 0;
}
static void SDEC_Read(SDEC_CHANNEL_T *pCh)
{
	unsigned int receivedBytes = 0;


	SDEC_UpdateAFMode(pCh);

	while (1) {
		NAVDTVPCRINFO pcrinfo;
		int isProhibited = 0;
		PCR_EXIST_STATUS status;
		NAVDEMUXIN *pDemuxIn  = &pCh->demuxIn;
		NAVDEMUXOUT *pDemuxOut = &pCh->demuxOut;

		if (pCh->demuxOutReserved.numBuffers > 0) {
			isProhibited = 1;
		} else if (pCh->demuxInReserved.pCurr < pCh->demuxInReserved.pEnd) {
			/* process reserved demux-in data when it exists,
			* and there is no reserved data to deliver
			*/
			pDemuxIn  = &pCh->demuxInReserved;
			pDemuxOut = &pCh->demuxOutReserved;
		}


		if (!pCh->bIsSDT && pCh->avSyncMode == NAV_AVSYNC_PCR_MASTER) {
			INT32 iRet = 1;
			memset(&pcrinfo, 0, sizeof(pcrinfo));

			if (pCh->PCRPID > 0) {
				iRet = RHAL_GetPCRTrackingStatus(pCh->idx, &pcrinfo.pcr,
							 &pcrinfo.stc, &pcrinfo.pcrBase, &pcrinfo.stcBase) ;
				if (iRet != TPK_SUCCESS ) {
					memset(&pcrinfo, 0, sizeof(pcrinfo));

					//dmx_info(0, "%s_%d_monitor , iRet:%d \n", iRet);
					//DMX_DO_ACTION_ONCE_EVERY(DEMUX_CLOCKS_PER_SEC,
					//	dmx_err(pCh->idx,
					//		"[%s:%d] can not get pcr/stc from TP%d\n",
					//		pCh->idx
					//	);
					//);
				}
			}

			if (pCh->avSyncMode == NAV_AVSYNC_PCR_MASTER) {
				PCRSYNC_T * const processor = &pCh->pcrProcesser;
				processor->v_fr_thrhd = pCh->v_fr_thrhd;
				status = PCRSYNC_CheckPcrExist(&pCh->pcrProcesser);
				if (status != PCR_EXIST_STATUS_ALREADY_GOT) {
					if(status == PCR_EXIST_STATUS_TIMEOUT)
						PCRSYNC_EnterFreeRunMode(&pCh->pcrProcesser);
				} else {
					if( !(TPK_NOT_ENOUGH_RESOURCE == iRet) ){
						PCRSYNC_UpdateRCD(processor, pcrinfo);
					}
				}

				if (PCRSYNC_IsUnderFreeRunMode(processor))
					SDEC_ResetAVSync(pCh, NAV_AVSYNC_FREE_RUN);
			}
		}
		// under other a/v sync modes
		else if (!pCh->bIsSDT) {
			if (0 < pCh->PCRPID &&
					PCRSYNC_EvaluateToReturnPCRMaster(&pCh->pcrProcesser))
				SDEC_ResetAVSync(pCh, NAV_AVSYNC_PCR_MASTER);
		}
		PCRTRACK_UpdatePosition(pCh->pClockRecovery,
									&pCh->pcrProcesser, pCh->avSyncMode);

		if (receivedBytes >= 1024*1024) {
			UNLOCK(&pCh->mutex);
			msleep(NAV_SLEEP_TIME_ON_IDLE);
			LOCK(&pCh->mutex);
			return;
		}

		if (pDemuxIn->pCurr >= pDemuxIn->pEnd) {
			NAVBUF buf ;
			//isProhibited |= !pCh->startStreaming;

			buf.type = NAVBUF_NONE;
			if (SDEC_CheckStatus(pCh, &buf, isProhibited) == false)
				SDEC_ReadData(pCh, &buf);

			if (buf.type == NAVBUF_DATA || buf.type == NAVBUF_DATA_EXT) {
				pDemuxIn->pts           = buf.data.pts;
				pDemuxIn->startAddress  = buf.data.startAddress;
				pDemuxIn->phyAddrOffset = (buf.type == NAVBUF_DATA)?
													0 : buf.data.phyAddrOffset;

				/* fill it up with new raw data */
				if (buf.data.payloadSize > 0) {
					receivedBytes  += buf.data.payloadSize;
					pDemuxIn->pCurr = pDemuxIn->pBegin = buf.data.payloadData;
					pDemuxIn->pEnd  = pDemuxIn->pBegin + buf.data.payloadSize;
					if (g_dbgSetting[pCh->idx].printio) {
						DEBUG_CumulateReadSize(pCh->idx, buf.data.payloadSize);
					}
				}

			} else {
				UNLOCK(&pCh->mutex);
				SDEC_DeliverNavBufCommands(pCh, &buf);
				LOCK(&pCh->mutex);
				return ;
			}
		}
		if (PARSER_Parse(pCh, pDemuxIn, pDemuxOut) == 0) {
			return;
		}
		pDemuxOut->numBuffers = 0;
	}
}
int SDEC_ThreadProc(SDEC_CHANNEL_T *pCh)
{
	//DEMUX_CheckBeforeThreadRun(pCh);
	pCh->threadState = SDEC_STATE_RUNNING;
	while (!kthread_should_stop()) {

		set_freezable();
		LOCK(&pCh->mutex);
		/* try to re-deliver reserved demux-out */
		if (pCh->demuxOutReserved.numBuffers > 0)
			SDEC_DeliverData(pCh, &pCh->demuxOutReserved);

		SDEC_Read(pCh);

		/* deliver data from main demux-out */
		if (pCh->demuxOut.numBuffers > 0 &&
			SDEC_DeliverData(pCh, &pCh->demuxOut) < 0) {
			 /* reserve data if delivery is not successful */
			pCh->demuxInReserved = pCh->demuxIn;
			pCh->demuxIn.pBegin  = pCh->demuxIn.pEnd = pCh->demuxIn.pCurr = 0;
			pCh->demuxOutReserved     = pCh->demuxOut;
			pCh->demuxOut.numBuffers  = 0;
		}

		UNLOCK(&pCh->mutex);

		/* for DEBUG */
		if (pCh->tp_src == MTP && !pCh->bIsSDT && pCh->refClock.prefclk) {
			DMX_DO_ACTION_ONCE_EVERY(1*DEMUX_CLOCKS_PER_SEC,
				REFCLOCK *pRefClock = pCh->refClock.prefclk;
				dmx_mask_print(DEMUX_AUDIO_STABLE_DEBUG, pCh->idx,
					"[pvr]: mastership mode %d, s %d, v %d, a %d, st %d,"
					"vPTS %lld, aPTS %lld\n"
					, pCh->avSyncMode
					, pRefClock->mastership.systemMode
					, pRefClock->mastership.videoMode
					, pRefClock->mastership.audioMode
					, pRefClock->mastership.masterState
					, reverse_int64(pRefClock->videoSystemPTS)
					, reverse_int64(pRefClock->audioSystemPTS));
			);
		}

		if (g_dbgSetting[pCh->idx].printio)
			DEBUG_Printio(pCh->idx);
	}
	pCh->threadState = SDEC_STATE_STOP;
	return 0;
}
void SDEC_ResetAVSync(SDEC_CHANNEL_T *pCh, AV_SYNC_MODE avSyncMode)
{
	// NOTICE: caller should lock pCh->mutex first.
	REFCLOCK * const refClock = pCh->refClock.prefclk;
	if (avSyncMode == NAV_AVSYNC_AUDIO_MASTER_AF) {

		if (pCh->tp_src == MTP) {
			pCh->avSyncStartupFullness = 0;
			pCh->bPendingOnFullness = false;
			pCh->avSyncStartupPTS = 45000;
			pCh->bPendingOnPTSCheck = true;

			REFCLK_SetFreeRunThreshold(refClock, FREE_RUN_THRESHOLD_FOR_MTP,
					FREE_RUN_THRESHOLD_FOR_MTP, AUDIO_FREE_RUN_THRESHOLD_TO_WAIT);
		} else {
			pCh->avSyncStartupFullness = 512 * 1024;
			pCh->bPendingOnFullness = true;
			pCh->avSyncStartupPTS = 0;
			pCh->bPendingOnPTSCheck = false;
		}
		pCh->avSyncMode = NAV_AVSYNC_AUDIO_MASTER_AF;
		pCh->bCheckAFState = true;
		pCh->timeToUpdteAF = CLOCK_GetPTS() + 1500*90;

		REFCLK_SetMasterShip(refClock, pCh->avSyncMode);
		REFCLK_SetTimeout(refClock, 3*90000);

	} else if (avSyncMode == NAV_AVSYNC_AUDIO_MASTER_AUTO) {

		pCh->avSyncStartupFullness = 0;
		pCh->bPendingOnFullness = false;
		pCh->avSyncStartupPTS = 45000;
		pCh->bPendingOnPTSCheck = true;
		pCh->avSyncMode = NAV_AVSYNC_AUDIO_MASTER_AUTO;

		REFCLK_SetMasterShip(refClock, pCh->avSyncMode);
		REFCLK_SetFreeRunThreshold(refClock, FREE_RUN_THRESHOLD_FOR_MTP,
					FREE_RUN_THRESHOLD_FOR_MTP, AUDIO_FREE_RUN_THRESHOLD_TO_WAIT);
		if(REFCLK_GetRCD(refClock) == -1)
			REFCLK_SetTimeout(refClock, 3*90000);

	} else if (avSyncMode == NAV_AVSYNC_PCR_MASTER) {

		pCh->avSyncStartupFullness = 0;
		pCh->bPendingOnFullness = false;
		pCh->avSyncStartupPTS = 0;
		pCh->bPendingOnPTSCheck = false;
		pCh->avSyncMode = NAV_AVSYNC_PCR_MASTER;

		REFCLK_SetMasterShip(refClock, pCh->avSyncMode);
		REFCLK_SetTimeout(refClock, 3*90000);

	} else if (avSyncMode == NAV_AVSYNC_AUDIO_ONLY) {

		pCh->avSyncStartupFullness = 0;
		pCh->bPendingOnFullness = false;
		pCh->avSyncStartupPTS = 0;
		pCh->bPendingOnPTSCheck = false;
		pCh->avSyncMode = NAV_AVSYNC_AUDIO_ONLY;

		REFCLK_SetTimeout(refClock, 0);
		REFCLK_SetMasterShip(refClock, pCh->avSyncMode);
		REFCLK_SetFreeRunThreshold(refClock, FREE_RUN_THRESHOLD_FOR_MTP,
					FREE_RUN_THRESHOLD_FOR_MTP, AUDIO_FREE_RUN_THRESHOLD_TO_WAIT);

	} else if (avSyncMode == NAV_AVSYNC_VIDEO_ONLY) {

		pCh->avSyncStartupFullness = 0;
		pCh->bPendingOnFullness = false;
		pCh->avSyncStartupPTS = 0;
		pCh->bPendingOnPTSCheck = false;
		pCh->avSyncMode = NAV_AVSYNC_VIDEO_ONLY;

		REFCLK_SetTimeout(refClock, 0);
		REFCLK_SetMasterShip(refClock, pCh->avSyncMode);
	} else if (avSyncMode == NAV_AVSYNC_FREE_RUN) {
		pCh->avSyncStartupFullness = 0;
		pCh->bPendingOnFullness = false;
		pCh->avSyncStartupPTS = 0;
		pCh->bPendingOnPTSCheck = false;
		pCh->avSyncMode = NAV_AVSYNC_FREE_RUN;

		REFCLK_SetTimeout(refClock, 0);
		REFCLK_SetMasterShip(refClock, pCh->avSyncMode);
    }
	//pCh->preMasterState = refClock->mastership.masterState;
	//pCh->preAudioSystemPTS = DEMUX_reverseLongInteger(refClock->audioSystemPTS);
	//pCh->preVideoSystemPTS = DEMUX_reverseLongInteger(refClock->videoSystemPTS);
	//pCh->preRCD = DEMUX_reverseLongInteger(refClock->RCD);

	dmx_crit(pCh->idx, "change avsync mode = %d\n", pCh->avSyncMode);
}
/*
int SDEC_ThreadProcEntry(void *pParam)
{
	struct cpumask demux_cpumask;
	SDEC_CHANNEL_T * const pCh = (SDEC_CHANNEL_T *)pParam;

	struct sched_param param = { .sched_priority = 1 };
	sched_setscheduler(current, SCHED_RR, &param);
	current->flags &= ~PF_NOFREEZE;


	cpumask_clear(&demux_cpumask);
	cpumask_set_cpu(2, &demux_cpumask); // run task in core 3
	//sched_setaffinity(0, &demux_cpumask);
	SDEC_ThreadProc(pCh);
	return 0 ;
}
void SDEC_StartThread(SDEC_CHANNEL_T *pCh)
{
	char pstr[10] = {0};
	snprintf(pstr,9,"RDVB_%d", pCh->idx);
	pCh->thread_task = kthread_create(SDEC_ThreadProcEntry, (void *)pCh, "%s",pstr);
	if (!(pCh->thread_task))
		return;

	wake_up_process(pCh->thread_task);
	dmx_notice(pCh->idx, "\n");
}
*/

int SDEC_InitChannel(SDEC_CHANNEL_T *pCh, char idx)
{
	int j;
	//int tpBufSize, tpDataSize;
	TPK_HARDWARE_INFO_T tpInfo = {0};

	bool bHasSDT = false;
	struct dvb_demux dvb_demux = pCh->dvb_demux;

	create_tp_controller();
	RHAL_GetHardwareInfo(&tpInfo);
	bHasSDT = ((tpInfo.tp_num < 2) ||
				(tpInfo.chip_version < RTD294X_TP_PLUS)) ? 0 : 1;


	memset(pCh, 0, sizeof(SDEC_CHANNEL_T));
	pCh->dvb_demux = dvb_demux;

	pCh->idx = idx;
	pCh->bIsSDT = idx > 1 && bHasSDT;
	pCh->threadState = SDEC_STATE_STOP;
	pCh->avSyncMode = NAV_AVSYNC_PCR_MASTER;
	pCh->v_fr_thrhd = DEFAULT_FREE_RUN_VIDEO_THRESHOLD;
	pCh->PCRPID = -1;
	pCh->bPcrTrackEnable = 1;
	pCh->tsPacketSize  = 188;
	pCh->dataThreshold = pCh->tsPacketSize * 1024;
	pCh->AdChannelTarget = -1;

	//PVR
	pvr_rec_init(&pCh->pvrRecSetting, pCh->idx);
	pvr_pb_init(&pCh->pvrPbSetting, pCh->idx);
	pCh->pvrPbSpeed = 256;
	pCh->isAudioPaused = false;

	INIT_LIST_HEAD(&pCh->tpc_list_head);
	pCh->tpc_num = 10;
	pCh->tpcs =  kmalloc(sizeof(struct ts_parse_context)*pCh->tpc_num, GFP_KERNEL);
	for (j=0; j< pCh->tpc_num; j++) {
		ts_parse_context_init(&pCh->tpcs[j]);
		pCh->tpcs[j].priv = pCh;
	}

	mutex_init(&pCh->mutex);

	RHAL_TPInit(pCh->idx);
	if (pCh->bIsSDT)
		return 1;

	if (rdvb_dmxbuf_poll_alloc(&pCh->refClock.refClock, sizeof(REFCLOCK)) < 0)
		goto SDEC_INIT_FAIL;

	pCh->refClock.prefclk = (REFCLOCK *)pCh->refClock.refClock.viraddr;
	REFCLK_Reset(pCh->refClock.prefclk, pCh->avSyncMode, pCh->v_fr_thrhd);
	REFCLK_SetClockMode(pCh->refClock.prefclk, MISC_90KHz);
	PCRSYNC_Init(&pCh->pcrProcesser, pCh->refClock.prefclk, pCh->idx);


#ifdef CONFIG_RTK_KDRV_MULTI_TP_CLOCK
	if (pCh->idx < DEMUX_PCRTRACKING_MANAGER_NUM)
		pCh->pClockRecovery = &gClockRecovery[pCh->idx];
	else
		pCh->pClockRecovery = NULL;
#else
	pCh->pClockRecovery = &gClockRecovery[0];
#endif
	if (pCh->pClockRecovery)
		PCRTRACK_Initialize(pCh->pClockRecovery, pCh->idx);

	return 1;

SDEC_INIT_FAIL:
	SDEC_ReleaseChannel(pCh);
	return -1;
}
int SDEC_ReleaseChannel(SDEC_CHANNEL_T *pCh)
{

	if (pCh->threadState != SDEC_STATE_STOP) {
		if (pCh->thread_task)
			kthread_stop(pCh->thread_task);
		pCh->thread_task = 0;
	}


	if (pCh->pClockRecovery)
		PCRTRACK_Release(pCh->pClockRecovery);
	RHAL_TPStreamControl(pCh->idx, TP_STREAM_STOP);
	RHAL_TPUninit(pCh->idx);
	mutex_destroy(&pCh->mutex);

	if (pCh->tpcs)
		kfree(pCh->tpcs);
	
	//CMA_FreeBuffer(&pCh->refClock);
	//CMA_FreeTPBuffer(pCh);
	return 1;
}
struct ts_parse_context *
get_free_ts_parse_context(struct list_head *tpc_list_head)
{
	SDEC_CHANNEL_T * pCh = NULL;
	int i  = 0;
	if (tpc_list_head == NULL)
		return NULL;
	pCh = container_of(tpc_list_head,SDEC_CHANNEL_T, tpc_list_head);

	for (i = 0; i < pCh->tpc_num; i++) {
		if (pCh->tpcs[i].is_avalible)
			break;
	}
	if (i >=  pCh->tpc_num) {
		dmx_err(pCh->idx, "ts_parse_context out of used(%d)\n", pCh->tpc_num);
		return NULL;
	}
	return &pCh->tpcs[i];
}

void sdec_flush_by_tpc(struct ts_parse_context *tpc)
{
	int  i = 0;
	SDEC_CHANNEL_T *sdec = tpc->priv;

	if (tpc ->is_ad == true) {
		tpc->is_ad = false;
		SET_FLAG(tpc->events, EVENT_FLUSH_AD_INFO);
	}
	dmx_notice(sdec->idx, "tpc:%p, type:%d, portnum:%d\n",
			tpc, tpc->data_context_type, tpc->data_port_num);

	//flush demuxout data
	for (i = 0; i < sdec->demuxOutReserved.numBuffers; i++) {
		NAVBUF *pBuf = &sdec->demuxOutReserved.pNavBuffers[i];
		if (pBuf->data.tpc == tpc) {
			pBuf->type = NAVBUF_NONE;
			pBuf->data.pinIndex = DEMUX_TARGET_DROP;
			pBuf->data.tpc = NULL;
		}
	}
	for (i = 0; i < sdec->demuxOut.numBuffers; i++) {
		NAVBUF *pBuf = &sdec->demuxOut.pNavBuffers[i];
		if (pBuf->data.tpc == tpc) {
			pBuf->type = NAVBUF_NONE;
			pBuf->data.pinIndex = DEMUX_TARGET_DROP;
			pBuf->data.tpc = NULL;
		}
	}

}

void sdec_reset_pts_by_tpc(struct ts_parse_context *tpc)
{
	SDEC_CHANNEL_T *sdec = tpc->priv;

	dmx_notice(sdec->idx, "tpc:%p, type:%d, portnum:%d\n",
					tpc, tpc->data_context_type, tpc->data_port_num);

	if (sdec->is_playback == true) {
		REFCLOCK * const pRefClock = sdec->refClock.prefclk;
		if (tpc->data_context_type == DATA_CONTEXT_TYPE_AUDIO) {
			dmx_mask_print(DEMUX_AUDIO_STABLE_DEBUG,  sdec->idx,
				"reset pin(%d/%d)\n", tpc->data_context_type, tpc->data_port_num);

			sdec->pcrProcesser.lastDemuxAudioPTS = -1;
			sdec->pcrProcesser.startAudioPts = -1;
			pRefClock->audioSystemPTS = reverse_int64(-1);

		} else if (tpc->data_context_type == DATA_CONTEXT_TYPE_VIDEO) {
			dmx_mask_print(DEMUX_AUDIO_STABLE_DEBUG,  sdec->idx,
				"reset pin:(%d/%d)\n", tpc->data_context_type, tpc->data_port_num);

			sdec->pcrProcesser.lastDemuxVideoPTS = -1;
			sdec->pcrProcesser.startVideoPts = -1;
			pRefClock->videoSystemPTS = reverse_int64(-1);

		}
	}
}
