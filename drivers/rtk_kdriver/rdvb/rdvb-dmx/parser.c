#include "sdec.h"
#include "debug.h"
#include "dmx_parse_context.h"
#include "rdvb-buf/rdvb_dmx_buf.h"
#ifndef abs64
#define abs64 abs
#endif


#define PES_HEADER_MIN_SIZE  (9)
#define PRELOAD_STEP         (4)
#define DEMUX_IS_VIDEO_TARGET(_target) \
	((_target) == DEMUX_TARGET_VIDEO || (_target) == DEMUX_TARGET_VIDEO_2ND)
#define DEMUX_IS_AUDIO_TARGET(_target) \
	((_target) == DEMUX_TARGET_AUDIO || (_target) == DEMUX_TARGET_AUDIO_2ND)

#define DMX_PRELOAD(ptr__, size__)       do { } while (false)

#define IS_OFF_SYNC(p__)         ( (p__)[0] != 0x47      )
#define PID(p__)                 (((((int)(p__)[1]) & 0x1F) << 8) | (p__)[2])
#define IS_TRANSPORT_ERROR(p__)  (((p__)[1] >> 7) & 0x01 )  // transport error
#define IS_START_UNIT(p__)       (((p__)[1] &  0x40) != 0)
#define HAS_PAYLOAD(p__)         (((p__)[3] &  0x10) != 0)
#define HAS_AF(p__)              (((p__)[3] &  0x20) != 0)
#define CONTI_COUNT(p__)         ( (p__)[3] &  0x0F      )
#define AF_LENGTH(p__)           ( (p__)[4]              )
#define IS_DISCONTI(p__)         (((p__)[5] &  0x80) != 0)
#define HAS_PCR(p__)             (((p__)[5] &  0x10) != 0)
#define PCR(p__)                 ((((int64_t)(p__)[6]) << 25) \
				| (((int)(p__)[7]) << 17) \
				| (((int)(p__)[8]) << 9) \
				| (((int)(p__)[9]) << 1) \
				| (((p__)[10]>>7) & 0x1))
#define HAS_PACKET_START_CODE(p__) \
	( (p__)[0] == 0x00 && (p__)[1] == 0x00 && (p__)[2] == 0x01)

#define AD_version_text_tag(p__)    ((p__)[ 6])
#define AD_fade_byte(p__)           ((p__)[ 7])
#define AD_pan_byte(p__)            ((p__)[ 8])
#define AD_gain_byte_center(p__)    ((p__)[ 9])
#define AD_gain_byte_front(p__)     ((p__)[10])
#define AD_gain_byte_surround(p__)  ((p__)[11])

#define SCRAMBLE_CONTROL(_p) (((_p) & 0xC0) >> 6)

enum {
	EVENT_UPDATE_AUDIO_FORMAT      = 1 << 0,
	EVENT_DISCONTINUITY            = 1 << 1,
	EVENT_DROP_PACKET              = 1 << 2,
	EVENT_AUDIO_CHECK_PES_PRIORITY = 1 << 3,
	EVENT_AUDIO_DATA_DISCONTINUITY = 1 << 4,
	EVENT_UPDATE_AD_INFO           = 1 << 6,
};

static int PARSER_ResyncTSStream(const unsigned char *pData, unsigned int bytes)
{
#define NUM_SYNC_WORDS_TO_LOCK  10
#define UNSEARCHED_LAST_BYTES   512

	static const int packetSizeTable[] = { 188, 192, 204 };
	unsigned int byteIdx = 0;
	const unsigned char *pLimit = pData + bytes;
	while (byteIdx + UNSEARCHED_LAST_BYTES < bytes) {
		if (pData[byteIdx] == 0x47) {
			unsigned int sizeIdx;
			for (sizeIdx = 0; sizeIdx < sizeof(packetSizeTable)/sizeof(packetSizeTable[0]); sizeIdx++) {
				const unsigned char *pNextSyncWord = pData + byteIdx;
				int i;
				for (i = 1; i < NUM_SYNC_WORDS_TO_LOCK; i++) {
					pNextSyncWord += packetSizeTable[sizeIdx];
					if (pNextSyncWord >= pLimit)
						break;
					if (pNextSyncWord[0] != 0x47)
						break;
				}
				if (i == NUM_SYNC_WORDS_TO_LOCK) {
					return byteIdx;
				}
			}
		}
		byteIdx++;
	}
	return -1;
}
static void PARSER_CheckScrambling(struct ts_parse_context *tpc, unsigned char *pData)
{
	//dmx_err(NOCH,"%s_%d \n", __FUNCTION__, __LINE__);
	if (IS_START_UNIT(pData) && HAS_PAYLOAD(pData)) {
		tpc->scram_status = SCRAMBLE_CONTROL(pData[3]) == 0?
			SCRAM_STATUS_NOT_SCRAMBLE : SCRAM_STATUS_SCRAMBLE;
	}
}
static void PARSER_ParsePCR(SDEC_CHANNEL_T *pCh, unsigned char *pTsPacket)
{
	int64_t PCRBase, PCR;
	SWDEMUXPCR* pSwPCRInfo = &pCh->pcrProcesser.swPCRInfo;
	int afLen = pTsPacket[4];
	int PCRFlag = (pTsPacket[5] & 0x10);
	if(!PCRFlag || afLen < 7) return;
	PCRBase = (((long long)pTsPacket[6] << 25)
		|  ((long long)pTsPacket[7] << 17)
		|  ((long long)pTsPacket[8] << 9)
		|  ((long long)pTsPacket[9] < 1)
		|  (((long long)pTsPacket[10] & 0x80) >> 7));
	PCR = PCRBase;/*PCRBase * 300 + PCRExt*/;
	pCh->pcrProcesser.pcrExistDetect.newPCR = PCR;
	pSwPCRInfo->aPCR = PCR;
	pSwPCRInfo->vPCR = PCR;
	pSwPCRInfo->aPcrOffset = 0;
	pSwPCRInfo->vPcrOffset = 0;
	//dmx_crit(0, "[%s]PCR = %lld\n", __func__, PCR);
}
static void PARSER_ParseTEMI(struct ts_parse_context *tpc, unsigned char *pTsPacket)
{
	unsigned char *pData = pTsPacket + 4;
	unsigned char af_flag = 0;
	unsigned char af_ex_length = 0;
	unsigned char af_ex_flag = 0;
	unsigned char af_descr_length = 0;
	unsigned char af_descr_flag = 0;

	if (pData[0] == 0)
		return;

	pData ++;
	af_flag = *pData;
	pData ++;
	if (af_flag & 0x10) //pcr_flag
		pData += 6;
	if (af_flag & 0x08) //opcr_flag
		pData += 6;
	if (af_flag & 0x04) //splicing_point_flag
		pData += 1;
	if (af_flag & 0x02) {//transport_private_data_flag
		pData +=1;
		pData += (*pData);
	}
	if ((af_flag & 0x01) == 0)
		return;

	af_ex_length = (*pData);
	pData ++;
	af_ex_flag = *(pData);
	pData ++;
	af_ex_length--;
	if (af_ex_flag & 0x80) {
		pData += 2;
		af_ex_length -=2;
	}
	if (af_ex_flag & 0x40) {
		pData += 3;
		af_ex_length -=3;
	}
	if (af_ex_flag & 0x20) {
		pData += 5;
		af_ex_length -=5;
	}
	if ((af_ex_flag & 0x10) == 0x10)
		return ;
	while (af_ex_length >= 2) {
		if (pData[0] == 0x04) {//time_Line_Descriptor
			unsigned int timeline_id = 0;
			af_descr_length = pData[1];
			af_ex_length -=2;
			af_ex_length -= af_descr_length;
			af_descr_flag = pData[2];
			timeline_id = pData[4];
			dmx_mask_print(DMX_TEMI_DBG,NOCH,"TEMI: u32TimelineID:0x%08x\n",
							timeline_id);
			if (timeline_id != tpc->temi_timeline_id)
				continue;
			if (tpc->dfc((u8*)&tpc->pts, sizeof(int64_t), pData,
							af_descr_length+2, tpc->filter)<0)
				dmx_err(NOCH, "deliver pes fail\n");
			break;
		} else {
			af_descr_length = pData[1];
			af_ex_length -= 2;
			pData += 2;
			if (af_ex_length >= af_descr_length){
				af_ex_length -= af_descr_length;
				pData += af_descr_length;
				dmx_mask_print(DMX_TEMI_DBG, NOCH, "TEMI:af_tag:0x%02x, len:0x%02x , af_ex_len:0x%x [%*ph]\n",
						pData[0],  af_descr_length, af_ex_length, 64, (void*)pData);
			} else {
				dmx_err(NOCH, "TEMI:af_tag:0x%02x, len:0x%02x , af_ex_len:0x%x [%*ph]\n",
						pData[0],  af_descr_length, af_ex_length, 64, (void*)pData);
				dmx_err(NOCH, "[%*ph]\n", 64, pTsPacket);
				break;
			}
		}
	}

}

static void PARSER_UpdateADInfo(struct ts_parse_context *tpc, unsigned char AD_fade_byte, 
	unsigned char AD_pan_byte, char AD_gain_byte_center, char AD_gain_byte_front,
	char AD_gain_byte_surround)
{
	if(tpc->adi.AD_fade_byte != AD_fade_byte 
		|| tpc->adi.AD_pan_byte != AD_pan_byte
		|| tpc->adi.AD_gain_byte_center != AD_gain_byte_center 
		|| tpc->adi.AD_gain_byte_front != AD_gain_byte_front
		|| tpc->adi.AD_gain_byte_surround != AD_gain_byte_surround) {
		tpc->adi.AD_fade_byte = AD_fade_byte;
		tpc->adi.AD_pan_byte = AD_pan_byte;
		tpc->adi.AD_gain_byte_center = AD_gain_byte_center;
		tpc->adi.AD_gain_byte_front = AD_gain_byte_front;
		tpc->adi.AD_gain_byte_surround = AD_gain_byte_surround;

		dmx_notice(CHANNEL_UNKNOWN,"[ad] %s :AD_fade_byte %d "
			"AD_pan_byte %d AD_gain_byte_center %d "
			"AD_gain_byte_front %d AD_gain_byte_surround %d\n",
			__FUNCTION__,tpc->adi.AD_fade_byte,
			tpc->adi.AD_pan_byte, tpc->adi.AD_gain_byte_center,
			tpc->adi.AD_gain_byte_front,tpc->adi.AD_gain_byte_surround);
		SET_FLAG(tpc->events, EVENT_UPDATE_AD_INFO);
	}
}
static void PARSER_ParseADInfo(struct ts_parse_context *tpc, 
			const unsigned char *pCurr, unsigned int headerLen,
			 unsigned int pesHdrOffset)
{
	const unsigned char *pPesExten = NULL;
	const unsigned char *pAD_descriptor  = NULL;
	unsigned char version_text_tag = 0;
	unsigned char fade_byte = 0;
	unsigned char pan_byte = 0;
	unsigned char gain_byte_center = 0;
	unsigned char gain_byte_front = 0;
	unsigned char gain_byte_surround = 0;
	 /* has DTS */
	if (headerLen >= PES_HEADER_MIN_SIZE && (pCurr[7] & 0x40))
		pesHdrOffset += 5;
 	/* has ESCR */
	if (headerLen >= PES_HEADER_MIN_SIZE && (pCurr[7] & 0x20))
		pesHdrOffset += 6;
	/* has ES rate */
	if (headerLen >= PES_HEADER_MIN_SIZE && (pCurr[7] & 0x10))
		pesHdrOffset += 3;
	/* has DSM_trick_mode */
	if (headerLen >= PES_HEADER_MIN_SIZE && (pCurr[7] & 0x08))
		pesHdrOffset += 1;
	/* has additional_copy_info */
	if (headerLen >= PES_HEADER_MIN_SIZE && (pCurr[7] & 0x04))
		pesHdrOffset += 1;
	/* has PES_CRC */
	if (headerLen >= PES_HEADER_MIN_SIZE && (pCurr[7] & 0x02))
		pesHdrOffset += 2;

	/*pes extension*/
	if ((headerLen < PES_HEADER_MIN_SIZE) || ((pCurr[7] & 0x01) == 0)
		|| (pesHdrOffset >= headerLen))
		return;

	/*pes private data*/
	pPesExten = pCurr + pesHdrOffset;
	if (((pPesExten[0]&0x80) == 0) || ((pesHdrOffset + 1 + 16) > headerLen))
		return;
	
	pAD_descriptor = pPesExten + 1;
	if (((pAD_descriptor[0] & 0x0f) < 8)
		|| (pAD_descriptor[1] != 0x44)
		|| (pAD_descriptor[2] != 0x54)
		|| (pAD_descriptor[3] != 0x47)
		|| (pAD_descriptor[4] != 0x41)
		|| (pAD_descriptor[5] != 0x44))
		return;

	tpc ->is_ad = true;
	version_text_tag = AD_version_text_tag(pAD_descriptor);
	fade_byte = AD_fade_byte(pAD_descriptor);
	pan_byte = AD_pan_byte(pAD_descriptor);

	//dmx_err(NOCH,"AD_fade_byte: 0x%x, AD_pan_byte: 0x%x. tag:0x%x\n", fade_byte, pan_byte, version_text_tag);

	if (version_text_tag == 0x32) {
		gain_byte_center = AD_gain_byte_center(pAD_descriptor);
		gain_byte_front = AD_gain_byte_front(pAD_descriptor);
		gain_byte_surround = AD_gain_byte_surround(pAD_descriptor);
	/*
		dmx_err(NOCH,"AD_gain_byte_center: 0x%x, AD_gain_byte_front: 0x%x ,"
			" AD_gain_byte_surround: 0x%x.\n",
			gain_byte_center, gain_byte_front, gain_byte_surround);
	*/
		PARSER_UpdateADInfo(tpc, fade_byte, pan_byte, gain_byte_center,
			gain_byte_front, gain_byte_surround);
	} else {
		PARSER_UpdateADInfo(tpc, fade_byte,pan_byte, 0, 0, 0);
	}
}

static void PARSER_ParsePESHeader(struct ts_parse_context *tpc,
			const unsigned char *pCurr, unsigned int len)
{
	unsigned int headerLen;
	unsigned int pesHdrBufSize = PES_HEADER_BUF_SIZE;

	if (tpc ->bufferedBytes + len < pesHdrBufSize) {
		memcpy(tpc ->pesHeaderBuf + tpc ->bufferedBytes, pCurr, len);

		if (tpc ->bufferedBytes + len < PES_HEADER_MIN_SIZE) {
			tpc ->remainingHeaderBytes = PES_HEADER_MIN_SIZE - tpc ->bufferedBytes;
			tpc ->bufferedBytes += len;
		} else {
			headerLen = tpc ->pesHeaderBuf[8] + PES_HEADER_MIN_SIZE;

			tpc ->remainingHeaderBytes = headerLen - tpc ->bufferedBytes;
			tpc ->bufferedBytes += len;

			if (headerLen < pesHdrBufSize)
				tpc ->bufferedBytes = 0;
		}
	} else {
		unsigned int pesHdrOffset;
		if (tpc ->bufferedBytes > 0) {
			memcpy(tpc ->pesHeaderBuf + tpc ->bufferedBytes, pCurr,
					pesHdrBufSize - tpc ->bufferedBytes);

			pCurr = tpc ->pesHeaderBuf;
		}

		headerLen = pCurr[8] + PES_HEADER_MIN_SIZE;

		tpc ->remainingHeaderBytes = headerLen - tpc ->bufferedBytes;
		tpc ->bufferedBytes = 0;
		pesHdrOffset = PES_HEADER_MIN_SIZE;

		if (headerLen >= PES_HEADER_BUF_SIZE && (pCurr[7] & 0x80)) { /* has PTS */
			tpc ->pts = (((int64_t)pCurr[9]  & 0x0E) << 29) |
				(pCurr[10]         << 22) |
				((pCurr[11] & 0xFE) << 14) |
				(pCurr[12]         <<  7) |
				((pCurr[13] & 0xFE) >>  1);
			pesHdrOffset += 5;	/* PTS data size */
			//dmx_err(NOCH,"tpc:%p , pts: %llx\n", tpc, tpc->pts);
#ifndef DONTFIXBUG13648
			if (tpc ->bResyncPTS)
				tpc ->bResyncPTS = false;
#endif

			if ( tpc->data_context_type == DATA_CONTEXT_TYPE_AUDIO && tpc->is_ad == false) {
				SWDEMUXPCR *pSwPCRInfo = &((SDEC_CHANNEL_T *)tpc->priv)->pcrProcesser.swPCRInfo;
				if (pSwPCRInfo->aPCR != -1) {
					pSwPCRInfo->aPcrOffset = tpc ->pts - pSwPCRInfo->aPCR;
					pSwPCRInfo->aPcrRef = pSwPCRInfo->aPCR;
					pSwPCRInfo->aPCR = -1;
					if (pSwPCRInfo->aPrevDemuxPTS != -1) {
						pSwPCRInfo->aByteRate = 0; // (tpc ->pts > pSwPCRInfo->aPrevDemuxPTS) ? div_s64(pSwPCRInfo->aReadSize * DEMUX_CLOCKS_PER_SEC, tpc ->pts - pSwPCRInfo->aPrevDemuxPTS) : 0;
						pSwPCRInfo->aReadSize = 0;
					}
					pSwPCRInfo->aPrevDemuxPTS = tpc ->pts;
					//dmx_crit(pCh->tunerHandle, "[%s]aPcrOffset = %lld, aByteRate = %d\n", __func__, pSwPCRInfo->aPcrOffset, pSwPCRInfo->aByteRate);
				}
			} else if (tpc->data_context_type == DATA_CONTEXT_TYPE_VIDEO ) {
				SWDEMUXPCR *pSwPCRInfo = &((SDEC_CHANNEL_T *)tpc->priv)->pcrProcesser.swPCRInfo;
				if (pSwPCRInfo->vPCR != -1) {
					pSwPCRInfo->vPcrOffset = tpc ->pts - pSwPCRInfo->vPCR;
					pSwPCRInfo->vPcrRef = pSwPCRInfo->vPCR;
					pSwPCRInfo->vPCR = -1;
					if (pSwPCRInfo->vPrevDemuxPTS != -1) {
						pSwPCRInfo->vByteRate = 0; // (tpc ->pts > pSwPCRInfo->vPrevDemuxPTS) ? div_s64(pSwPCRInfo->vReadSize * DEMUX_CLOCKS_PER_SEC, tpc ->pts - pSwPCRInfo->vPrevDemuxPTS) : 0;
						pSwPCRInfo->vReadSize = 0;
					}
					pSwPCRInfo->vPrevDemuxPTS = tpc ->pts;
					//dmx_crit(pCh->tunerHandle, "[%s]vPcrOffset = %lld, vByteRate = %d\n", __func__, pSwPCRInfo->vPcrOffset, pSwPCRInfo->vByteRate);
				}
			}
		}
		if (tpc->is_temi == false)
			PARSER_ParseADInfo(tpc, pCurr, headerLen, pesHdrOffset);
	}
}

static void PARSE_ParsePTS(struct ts_parse_context *tpc, unsigned char *pPacketStart)
{
	unsigned char * payloadData  = pPacketStart + 4;
	size_t          payloadSize  = 184;
	if (!HAS_PAYLOAD(pPacketStart))
		return ;
	if (HAS_AF(pPacketStart)) {
		int len = AF_LENGTH(pPacketStart) + 1;

		if (len < 184) {
			payloadData += len;
			payloadSize -= len;
		} else {
			payloadSize = 0;
		}
	}

	if (IS_START_UNIT(pPacketStart)) {
		if (HAS_PACKET_START_CODE(payloadData)) {
			tpc->bufferedBytes = 0;
			PARSER_ParsePESHeader(tpc, payloadData, payloadSize);
		} else {
			return ;
		}
	} else if (tpc->bufferedBytes > 0) {
		PARSER_ParsePESHeader(tpc, payloadData, payloadSize);
	}

}

static void PARSER_PackPrivateInfo(unsigned int infoId, void* infoData,
					 unsigned int infoSize, NAVBUF* pBuffer)
{
	pBuffer->type             = NAVBUF_DATA;
	pBuffer->data.payloadSize = 0;
	pBuffer->data.infoId      = infoId;
	pBuffer->data.infoData    = (unsigned char*)infoData;
	pBuffer->data.infoSize    = infoSize;
}
static void PARSER_DemuxOutPacket(struct ts_parse_context *tpc, 
				NAVDEMUXOUT *pDemuxOut, int phyAddrOffset)
{
	//SWDEMUXPCR* pSwPCRInfo;
	NAVBUF * demuxout_entry = &pDemuxOut->pNavBuffers[pDemuxOut->numBuffers];
	// fixbug 18043; DONTFIXBUG13912
	if (HAS_FLAG(tpc->events, EVENT_DROP_PACKET)) {
		demuxout_entry->data.payloadSize = 0;
		return;
	}

#ifndef DONTFIXBUG13648
	if ((DEMUX_IS_AUDIO_TARGET(target) || DEMUX_IS_VIDEO_TARGET(target)) && tpc->bResyncPTS)
		demuxout_entry->data.payloadSize = 0;
	else
#endif
	{
		demuxout_entry->type              = (phyAddrOffset == 0) ? NAVBUF_DATA : NAVBUF_DATA_EXT;
		demuxout_entry->data.tpc          = tpc;
		demuxout_entry->data.pts          = tpc->pts;
		/*        demuxout_entry->data.channelIndex = GetChannelIndex(target);*/

		tpc->pts = -1;

		/*#ifdef IS_ARCH_ARM_COMMON */
		if (phyAddrOffset != 0) {
			demuxout_entry->type =  NAVBUF_DATA_EXT;
			demuxout_entry->data.phyAddrOffset = phyAddrOffset;
		}
/*    #endif */
		//put in last check
		if (/*target == TARGET_AUDIO_2ND && */HAS_FLAG(tpc->events, EVENT_UPDATE_AD_INFO)
			&& pDemuxOut->numBuffers < (NUM_OF_NAV_BUFFERS-1)) {

			pDemuxOut->numBuffers ++;
			RESET_FLAG(tpc->events, EVENT_UPDATE_AD_INFO);
			dmx_notice(CHANNEL_UNKNOWN,"[ad] EVENT_UPDATE_AD_INFO AD_fade_byte = 0x%x\n",tpc->adi.AD_fade_byte);

			dmx_err(NOCH,"[ad] EVENT_UPDATE_AD_INFO fade:%d pan:%d center:%d front:%d surround:%d\n",
				tpc->adi.AD_fade_byte,tpc->adi.AD_pan_byte,
				tpc->adi.AD_gain_byte_center,tpc->adi.AD_gain_byte_front,
				tpc->adi.AD_gain_byte_surround);
			demuxout_entry = &pDemuxOut->pNavBuffers[pDemuxOut->numBuffers];
			PARSER_PackPrivateInfo(PRIVATEINFO_AUDIO_AD_INFO, &tpc->adi, sizeof(AUDIO_AD_INFO), demuxout_entry);
			demuxout_entry->data.tpc     = tpc;
		}

		//pSwPCRInfo = &pCh->pcrProcesser.swPCRInfo;
		//if (DEMUX_IS_AUDIO_TARGET(target)) pSwPCRInfo->aReadSize += demuxout_entry->data.payloadSize;
		//if (DEMUX_IS_VIDEO_TARGET(target)) pSwPCRInfo->vReadSize += demuxout_entry->data.payloadSize;

		pDemuxOut->numBuffers++;
		tpc->demuxedCount++;
	}
}
static bool PARSER_ParsePacket(struct ts_parse_context *tpc, unsigned char *pPacketStart, int phyAddrOffset, /*out*/NAVDEMUXOUT *pDemuxOut)
{
	int count ;

	NAVBUF * demuxout_entry = &pDemuxOut->pNavBuffers[pDemuxOut->numBuffers];
	if (tpc->events != 0) {
		demuxout_entry->data.tpc = tpc;

		if (HAS_FLAG(tpc->events, EVENT_DISCONTINUITY)) {
			RESET_FLAG(tpc->events, EVENT_DISCONTINUITY);

			tpc->contiCounter         = -1;
			tpc->pts                  = -1;
			tpc->bufferedBytes        = 0;
			tpc->remainingHeaderBytes = 0;
			tpc->bResyncStartUnit     = true;
#ifndef DONTFIXBUG13648
			/*if (target == DEMUX_TARGET_AUDIO || target == DEMUX_TARGET_VIDEO)*/
			if (tpc->bSyncPTSEnable) {
				tpc->bResyncPTS = true;
				for (unsigned int i = 0; i < pDemuxOut->numBuffers ; i++) {
					if (pDemuxOut->pNavBuffers[i].data.tpc == tpc) {
						pDemuxOut->pNavBuffers[i].data.payloadSize = 0;
					}
				}
			}
#endif
		}
	}

	demuxout_entry->data.payloadData = pPacketStart + 4;
	demuxout_entry->data.payloadSize = 184;
	demuxout_entry->data.infoId = PRIVATEINFO_NONE;
	/*    dmx_err(NOCH,"payloadSize %d\n", demuxout_entry->data.payloadSize) ;*/
	if (HAS_AF(pPacketStart)) {
		int len = AF_LENGTH(pPacketStart) + 1;

		if (len < 184) {
			demuxout_entry->data.payloadData += len;
			demuxout_entry->data.payloadSize -= len;
		} else {
			demuxout_entry->data.payloadSize = 0;
		}

		if (len > 1 && IS_DISCONTI(pPacketStart))
			tpc->contiCounter = -1;
	}

	if (!HAS_PAYLOAD(pPacketStart))
		return true;

	/******************* Check CC start **************************/
	/*
	//for bug 12007
	//if (demuxTargetInfo[target].bCheckContiCounter) {
	 */
	count = CONTI_COUNT(pPacketStart);

	if (tpc->contiCounter == -1)
		tpc->contiCounter = count;
	else {
		if (tpc->contiCounter == count) { /* duplicated packet*/
			if (tpc->bCheckContiCounter || IS_TRANSPORT_ERROR(pPacketStart))
				demuxout_entry->data.payloadSize = 0;
		} else {
			tpc->contiCounter = (tpc->contiCounter + 1) & 0xF;

			if (tpc->contiCounter != count) { /* continuity_counter is not continuous */
				tpc_packet_dcc_report(tpc, count);
				if (tpc->bCheckContiCounter || IS_TRANSPORT_ERROR(pPacketStart)) {
					SET_FLAG(tpc->events, EVENT_DISCONTINUITY);
					return true;
				} else {
					tpc->contiCounter = -1;
				}
			}
		}
	}
	/*}*/
	/******************* Check CC end **************************/
	if (tpc->bResyncStartUnit) {
		if (IS_START_UNIT(pPacketStart) || tpc->bKeepPES)
			tpc->bResyncStartUnit = false;
		else
			demuxout_entry->data.payloadSize = 0;
	}

	if ((int)demuxout_entry->data.payloadSize <= 0){
		return true;
	}

	if ((!IS_START_UNIT(pPacketStart) && tpc->remainingHeaderBytes == 0) ||	tpc->bKeepPES) {
		PARSER_DemuxOutPacket(tpc, pDemuxOut, phyAddrOffset);
	} else {
		const unsigned char * const payload     = demuxout_entry->data.payloadData;
		const size_t                payloadSize = demuxout_entry->data.payloadSize;
		if (IS_START_UNIT(pPacketStart)) {
			if (HAS_PACKET_START_CODE(payload)) {
				tpc->bufferedBytes = 0;

				PARSER_ParsePESHeader(tpc, payload, payloadSize);
			} else {
				return false;
			}
		} else if (tpc->bufferedBytes > 0) {
			PARSER_ParsePESHeader(tpc, payload, payloadSize);
		}

		if (tpc->is_temi && HAS_AF(pPacketStart))
			PARSER_ParseTEMI(tpc, pPacketStart);

		if (tpc->remainingHeaderBytes < demuxout_entry->data.payloadSize) {
			demuxout_entry->data.payloadData += tpc->remainingHeaderBytes;
			demuxout_entry->data.payloadSize -= tpc->remainingHeaderBytes;
			tpc->remainingHeaderBytes = 0;

			PARSER_DemuxOutPacket(tpc, pDemuxOut, phyAddrOffset);
		} else {
			tpc->remainingHeaderBytes -= demuxout_entry->data.payloadSize;
			demuxout_entry->data.payloadSize = 0;
			/*
			//            assert(tpc->remainingHeaderBytes > 0 || tpc->bufferedBytes == 0);

			//        #ifndef DONTFIXBUG21129
			//            CheckEventAudioDatatDiscontinuity(DEMUX_TARGET_AUDIO, pDemuxOut);
			//            #if defined(ENABLE_DTV_DUAL_AUDIO)
			//                CheckEventAudioDatatDiscontinuity(DEMUX_TARGET_AUDIO_2ND, pDemuxOut);
			//            #endif
			//        #endif //DONTFIXBUG21129
			 */
		}
	}
	return true;
}
static 
bool PARSER_ParsePacketCB(SDEC_CHANNEL_T *pCh, struct ts_parse_context *tpc, unsigned char *pPacketStart,  int phyAddrOffset)
{
	unsigned char *pData = pPacketStart + 4;
	int size = 184;
	int count;
	int isStartUnit;

	if (HAS_AF(pPacketStart)) {
		int len = AF_LENGTH(pPacketStart) + 1;

		if (len < 184) {
			pData += len;
			size  -= len;
		} else {
			size = 0;
		}

		if (len > 1 && IS_DISCONTI(pPacketStart))
			tpc->contiCounter = -1 ;
	}

	if (!HAS_PAYLOAD(pPacketStart))
		return true;

	/******************* Check CC start **************************/
	count = CONTI_COUNT(pPacketStart);

	if (tpc->contiCounter == -1)
		tpc->contiCounter = count;
	else {
		if (tpc->contiCounter == count) {/* duplicated packet */
			if (/*tpc->bCheckContiCounter || */IS_TRANSPORT_ERROR(pPacketStart))
				size = 0;
		} else {
			tpc->contiCounter = (tpc->contiCounter + 1) & 0xF;

			if (tpc->contiCounter != count) { /* continuity_counter is not continuous */
				tpc_packet_dcc_report(tpc, count);
				if (/*tpc->bCheckContiCounter || */IS_TRANSPORT_ERROR(pPacketStart)) {
					tpc->contiCounter = -1;
					return true;
				}
				else
				{
					tpc->contiCounter = -1;
				}
			}
		}
	}
	/******************* Check CC end **************************/

	if (size <= 0)
		return true;


	isStartUnit = IS_START_UNIT(pPacketStart);
	if (!tpc->size && !isStartUnit){
		dmx_mask_print(DMX_ERROR_2, NOCH, "DMX: PES: ERROR: size:%d, isStartUnit:%d \n",
			tpc->size, isStartUnit);
		return false ;
	}
	if (isStartUnit && tpc->size != 0){
		//last data not finish, and jump to new packet.so drop last data start new one
		//WOSQRTK-3068
		rdvb_dmxbuf_write_rollback(PIN_TYPE_PES, tpc->data_port_num);
		tpc->size = 0;
	}


	if (isStartUnit) { /* reset buffer */
		tpc->unitSize = ((pData[4] << 8) | pData[5]) + 6;      /* PES_packet_length */
	}
	if ((tpc->unitSize - tpc->size) < size )
		size = tpc->unitSize - tpc->size;

	if (tpc->size + size > tpc->unitSize) { /* skip this pes */
		dmx_err(NOCH, " PES size is unmatch (accumulated %d,unit %d)!!\n",
			tpc->size, tpc->unitSize);
		tpc->size = 0;  /* re-sync StartUnit */
		rdvb_dmxbuf_write_rollback(PIN_TYPE_PES, tpc->data_port_num);
		return false;
	}

	//memcpy(tpc->tpcBuf+tpc->size, pData, size);
	if (rdvb_dmxbuf_write(PIN_TYPE_PES, tpc->data_port_num, pData, size) < 0){
		tpc->size = 0;  /* re-sync StartUnit */
		rdvb_dmxbuf_write_rollback(PIN_TYPE_PES, tpc->data_port_num);
		return false;
	}
	tpc->size += size;
	if (tpc->size == tpc->unitSize) {
		rdvb_dmxbuf_sync(PIN_TYPE_PES, tpc->data_port_num, pCh->idx);
		if (rdvb_dmxbuf_trans_pesdata(PIN_TYPE_PES, tpc->data_port_num,
										tpc->dfc, tpc->filter)<0){
			dmx_err(NOCH,"DMX:PES deliver pes fail\n");
		}

		tpc->size = 0;

	}


	return true;
}

int PARSER_Parse(SDEC_CHANNEL_T *pCh, NAVDEMUXIN *pDemuxIn, NAVDEMUXOUT *pDemuxOut)
{
	unsigned char *pNextCachePreloadPos;
	//int i;
	
	pDemuxOut->numBuffers = 0;
	pDemuxOut->pNavBuffers = pCh->navBuffers;

	/* at the very beginning of multiplex buffer, preload the 4 bytes long header of the first PRELOAD_STEP TS packets */
	
	if (pDemuxIn->pCurr == pDemuxIn->pBegin) {
		if(pCh->tsPacketSize == 192) {
			pDemuxIn->pCurr += 4;
		}
	}
	pNextCachePreloadPos = pDemuxIn->pCurr + PRELOAD_STEP * pCh->tsPacketSize ;

	/* other un-demuxed packets comes, reset demux counter */

	while (pDemuxIn->pCurr+188 <= pDemuxIn->pEnd) {
		struct ts_parse_context * tpc = NULL;
		int pid;

		/* preload the 4 bytes long TS packet header that is PRELOAD_STEP ahead */
		if (pNextCachePreloadPos+4 <= pDemuxIn->pEnd) {
			DMX_PRELOAD(pNextCachePreloadPos, 4);
		}

		if (IS_OFF_SYNC(pDemuxIn->pCurr)) {
			int offset = 0;
			offset = PARSER_ResyncTSStream(pDemuxIn->pCurr, pDemuxIn->pEnd - pDemuxIn->pCurr);

			if (offset == -1){
				break;
			}
			pDemuxIn->pCurr += offset;
			continue;
		}
		pid  = PID(pDemuxIn->pCurr);


			//if ((target == DEMUX_TARGET_AUDIO && pCh->audio_newformat_info.Audio.IsSendNewFormat == false) || (target == DEMUX_TARGET_AUDIO_2ND && pCh->audio_newformat_info.Audio_2ND.IsSendNewFormat == false)) {
            //    //DBG_Warning(DGB_COLOR_RED_WHITE "ch:%d, pin:%d, Audio format is not set.\n", pCh->tunerHandle, target);
			//	target = DEMUX_TARGET_DROP;
			//}

		tpc =ts_parse_context_find(&pCh->tpc_list_head, pid);
		if (tpc == NULL)
			goto packet_parse_end;
		if (tpc->is_pcr && HAS_AF(pDemuxIn->pCurr))
			PARSER_ParsePCR(pCh, pDemuxIn->pCurr);
		//check scramble

		if (tpc->is_checkscram)
			PARSER_CheckScrambling(tpc, pDemuxIn->pCurr);

		if (tpc->data_context_type == DATA_CONTEXT_TYPE_AUDIO && tpc->start == false &&
			pCh->tp_src != MTP)
			goto packet_parse_end;
		//assemble pes 
		if (tpc->data_context_type == DATA_CONTEXT_TYPE_PES)
			PARSER_ParsePacketCB(pCh, tpc, pDemuxIn->pCurr, pDemuxIn->phyAddrOffset);
		//demux es
		if (tpc->data_context_type < DATA_CONTEXT_TYPE_PES) {
			PARSER_ParsePacket(tpc, pDemuxIn->pCurr, pDemuxIn->phyAddrOffset, pDemuxOut);
			if (pDemuxOut->numBuffers == NUM_OF_NAV_BUFFERS) {
				pDemuxIn->pCurr += pCh->tsPacketSize;
				pNextCachePreloadPos += pCh->tsPacketSize;
				return 0;
			}
		} else if (tpc->is_temi) {
			PARSE_ParsePTS(tpc, pDemuxIn->pCurr);
			if ( HAS_AF(pDemuxIn->pCurr))
				PARSER_ParseTEMI(tpc, pDemuxIn->pCurr);
		}
packet_parse_end:
		pDemuxIn->pCurr += pCh->tsPacketSize;
		pNextCachePreloadPos += pCh->tsPacketSize;
	}

	/*    FeekbackDemuxStatistics(pDemuxOut); */

	pDemuxIn->pCurr = pDemuxIn->pEnd;

	return (pDemuxOut->numBuffers > 0) ? 0 : -1 ;
}
