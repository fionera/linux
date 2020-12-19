#include <linux/freezer.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <rdvb-common/dmx_re_ta.h>
#include <rdvb-common/clock.h>
#include "sdec.h"
#include "pcrsync.h"
#define NAV_SLEEP_TIME_ON_IDLE 100 /* milliseconds*/
#define TUNER_DATA_READ_WAIT_TIME (20)

#define RELEASE_TP_BUFFER()                                                  \
	do {                                                                 \
		if (pCh->pTPRelase && pCh->TPReleaseSize) {                  \
			RHAL_TPReleaseData(pCh->idx, pCh->pTPRelase, \
			    pCh->TPReleaseSize);                             \
			pCh->pTPRelase = NULL;                               \
			pCh->TPReleaseSize = 0;                              \
		}                                                            \
	} while (false)
/*
static int sdec_ecp_DeliverNavBufCommands(NAVBUF *pNavBuffer)
{
	switch (pNavBuffer->type)
	{
	case NAVBUF_NONE:
		break;

	case NAVBUF_WAIT:
		msleep(pNavBuffer->wait.waitTime);
		break;

	case NAVBUF_SHORT_WAIT:
		msleep(pNavBuffer->wait.waitTime);
		break;

	default:
		dmx_err(CHANNEL_UNKNOWN, "Unknown bufferType : %d\n", pNavBuffer->type);
	}
	return 0;
}*/
static int sdec_ecp_readdata(SDEC_CHANNEL_T *pCh, NAVBUF *pBuffer)
{
	int ret;
	unsigned char * pLastReadAddr = 0;

	if (pCh->pTPRelase) {
		pLastReadAddr = pCh->pTPRelase + pCh->TPReleaseSize;
		if (pLastReadAddr >=
				(unsigned char *)(pCh->tpBuffer.phyAddr + pCh->tpBuffer.size))
			pLastReadAddr = (unsigned char *)pCh->tpBuffer.phyAddr;
	}
	RELEASE_TP_BUFFER();
	pBuffer->data.payloadData = 0;
	pBuffer->data.payloadSize = 0;

	ret = RHAL_TPReadData(pCh->idx, &pBuffer->data.payloadData, 
		(UINT32 *)(&pBuffer->data.payloadSize), pCh->dataThreshold);
	if (pBuffer->data.payloadSize) {
		pBuffer->data.payloadSize -= 
			(pBuffer->data.payloadSize % pCh->tsPacketSize);
		if (pCh->pcrProcesser.pcrExistDetect.checkPacketReady == false)
			pCh->pcrProcesser.pcrExistDetect.checkPacketReady = true;
	}
	if (pLastReadAddr && pBuffer->data.payloadSize > 0 
		&& pLastReadAddr != pBuffer->data.payloadData)
	{
		dmx_crit(pCh->idx, "ERROR: read ptr shift : "
			"last = 0x%p, now = 0x%p, tp = 0x%lx\n",
			pLastReadAddr, pBuffer->data.payloadData,
			(unsigned long)pCh->tpBuffer.phyAddr);
	}
	

	if (ret == TPK_BUFFER_FULL) {
		dmx_warn(pCh->idx, "buffer full,  addr %p, size %d\n" ,
			pBuffer->data.payloadData, pBuffer->data.payloadSize);
		pBuffer->type = NAVBUF_SHORT_WAIT;
		pBuffer->wait.waitTime = TUNER_DATA_READ_WAIT_TIME;

		pCh->pTPRelase = pBuffer->data.payloadData;
		pCh->TPReleaseSize = pBuffer->data.payloadSize;
		RELEASE_TP_BUFFER();
		return 0;
	}
	else if (ret == TPK_SUCCESS && pBuffer->data.payloadSize >= pCh->tsPacketSize)
	{

		pBuffer->type = NAVBUF_DATA;
		pBuffer->data.startAddress = 0xFFFFFFFF;
		pBuffer->data.infoId = PRIVATEINFO_NONE;

		pCh->pTPRelase = pBuffer->data.payloadData;
		pCh->TPReleaseSize = pBuffer->data.payloadSize;
		pCh->dataThreshold = pCh->tsPacketSize * 1024;

		
	} else {
		pBuffer->type = NAVBUF_SHORT_WAIT;
		pBuffer->wait.waitTime = TUNER_DATA_READ_WAIT_TIME;
		pCh->dataThreshold = 0;
	}

	return 0;
}

static void sdec_ecp_pes_handle(SDEC_CHANNEL_T *pCh, uint16_t pid )
{
	struct ts_parse_context * tpc = NULL;

	LOCK(&pCh->mutex);
	tpc = ts_parse_context_find(&pCh->tpc_list_head, pid);

	if ((tpc == NULL)||
		(rdvb_dmxbuf_ta_invalid(PIN_TYPE_PES, tpc->data_port_num)< 0) ||
		(rdvb_dmxbuf_trans_pesdata(PIN_TYPE_PES, tpc->data_port_num,
												tpc->dfc, tpc->filter) < 0)) {
		dmx_err(pCh->idx, " pes(pid:0x%x) tpc:%p,dlv fail\n", pid, tpc);
		UNLOCK(&pCh->mutex);
		return;
	}

	UNLOCK(&pCh->mutex);
}
static void sdec_ecp_process(SDEC_CHANNEL_T *pCh, NAVDEMUXIN *pDemuxIn)
{
	UINT64  start = CLOCK_GetPTS();
	int64_t vpts = -1, apts = -1;
	uint16_t pes_pid0=0x1fff, pes_pid1=0x1fff;
	dmx_mask_print(DMX_ECP0_DBG, pCh->idx,
				   "processdata: start:%p, end:%p cur:%p = %d packet\n",
				   pDemuxIn->pBegin, pDemuxIn->pEnd, pDemuxIn->pCurr,
				   (pDemuxIn->pEnd - pDemuxIn->pBegin) / 192);
	if (pDemuxIn->pCurr == pDemuxIn->pBegin && pCh->tsPacketSize == 192)
		pDemuxIn->pCurr +=4;
	pDemuxIn->pCurr =  (unsigned char *)dmx_ta_data_process(pCh->idx,
				(unsigned int)pDemuxIn->pCurr,  (unsigned int)pDemuxIn->pEnd,
				&vpts, &apts, &pes_pid0, &pes_pid1);

	dmx_mask_print(DMX_ECP0_DBG, pCh->idx, 
		"vpts:0x%llx, apts:0x%llx, pes_pid0:0x%x, pes_pid1:0x%x\n",
		 vpts, apts, pes_pid0, pes_pid1);
	if (vpts != -1)
		PCRSYNC_UpdateDemuxPTS(&pCh->pcrProcesser, true, vpts);
	if (apts != -1)
		PCRSYNC_UpdateDemuxPTS(&pCh->pcrProcesser, false, apts);
	if (pes_pid0 < 0x1fff) {
		sdec_ecp_pes_handle(pCh, pes_pid0);
	}
	if (pes_pid1 < 0x1fff) {
		sdec_ecp_pes_handle(pCh, pes_pid1);
	}
	dmx_mask_print(DMX_ECP0_DBG, pCh->idx,
			"processdata: start:%p, end:%p = cur:%p %d packet ==> (%lld) tick\n",
			pDemuxIn->pBegin, pDemuxIn->pEnd, pDemuxIn->pCurr,
			(pDemuxIn->pCurr - pDemuxIn->pBegin) / 192, CLOCK_GetPTS() - start);
}

static void sdec_ecp_avsync_handle(SDEC_CHANNEL_T *pCh)
{
	if (pCh->is_playback)
		return;
	//sdec_ecp_pcr_handle(&pCh->pcrProcesser);
	if (!pCh->bIsSDT && pCh->avSyncMode == NAV_AVSYNC_PCR_MASTER)
	{
		INT32 iRet = 1;
		NAVDTVPCRINFO pcrinfo;
		PCR_EXIST_STATUS status;
		memset(&pcrinfo, 0, sizeof(pcrinfo));

		if (pCh->PCRPID > 0) {
			iRet = RHAL_GetPCRTrackingStatus(pCh->idx, &pcrinfo.pcr,
							 &pcrinfo.stc, &pcrinfo.pcrBase, &pcrinfo.stcBase);
			if (iRet != TPK_SUCCESS) {
				memset(&pcrinfo, 0, sizeof(pcrinfo));
			}
		}

		if (pCh->avSyncMode == NAV_AVSYNC_PCR_MASTER) {
			PCRSYNC_T *const processor = &pCh->pcrProcesser;
			processor->v_fr_thrhd = pCh->v_fr_thrhd;
			status = PCRSYNC_CheckPcrExist(&pCh->pcrProcesser);
			if (status != PCR_EXIST_STATUS_ALREADY_GOT) {
				if (status == PCR_EXIST_STATUS_TIMEOUT)
					PCRSYNC_EnterFreeRunMode(&pCh->pcrProcesser);
			} else {
				if (!(TPK_NOT_ENOUGH_RESOURCE == iRet)) {
					PCRSYNC_UpdateRCD(processor, pcrinfo);
				}
			}

			if (PCRSYNC_IsUnderFreeRunMode(processor))
				SDEC_ResetAVSync(pCh, NAV_AVSYNC_FREE_RUN);
		}
	} else if (!pCh->bIsSDT) {
		if (0 < pCh->PCRPID && PCRSYNC_EvaluateToReturnPCRMaster(&pCh->pcrProcesser))
			SDEC_ResetAVSync(pCh, NAV_AVSYNC_PCR_MASTER);
	}
	PCRTRACK_UpdatePosition(pCh->pClockRecovery,
							&pCh->pcrProcesser, pCh->avSyncMode);
}
static void sdec_ecp_read(SDEC_CHANNEL_T *pCh)
{
	unsigned int receivedBytes = 0;
	while (1) {
		NAVDEMUXIN *pDemuxIn = &pCh->demuxIn;
		if (receivedBytes >= 1024 * 1024)
			return;
		sdec_ecp_avsync_handle(pCh);
		if (pDemuxIn->pCurr >= pDemuxIn->pEnd) {
			NAVBUF buf;
			buf.type = NAVBUF_NONE;
			if (sdec_ecp_readdata(pCh, &buf) < 0)
				return;
			if (buf.type != NAVBUF_DATA && buf.type != NAVBUF_DATA_EXT)
				return;
			if (buf.data.payloadSize <= 0)
				return;

			/* fill it up with new raw data */

			receivedBytes += buf.data.payloadSize;
			pDemuxIn->pCurr = pDemuxIn->pBegin = buf.data.payloadData;
			pDemuxIn->pEnd = pDemuxIn->pBegin + buf.data.payloadSize;
			
		}
		sdec_ecp_process(pCh, pDemuxIn);
	}
}
void sdec_ecp_ThreadProc(SDEC_CHANNEL_T *pCh)
{
	//DEMUX_CheckBeforeThreadRun(pCh);
	dmx_crit(pCh->idx, " start\n");
	pCh->threadState = SDEC_STATE_RUNNING;
	while (!kthread_should_stop()) {

		set_freezable();

		sdec_ecp_read(pCh);

		msleep(NAV_SLEEP_TIME_ON_IDLE);
	}
	pCh->threadState = SDEC_STATE_STOP;
	dmx_crit(pCh->idx, " stop\n");
}

void sdec_ecp_init(SDEC_CHANNEL_T *sdec)
{

	sdec->is_ecp = true;
	memset(&sdec->demuxIn, 0, sizeof(NAVDEMUXIN));
}