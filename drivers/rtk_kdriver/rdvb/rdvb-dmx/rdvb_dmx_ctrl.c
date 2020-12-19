#include <dmx-ext.h>
#include <rdvb-common/misc.h>
#include <rdvb-common/dmx_re_ta.h>
#include "rdvb_dmx.h"
#include "rdvb-buf/rdvb_dmx_buf.h"
#include "rdvb_dmx_ctrl.h"
#include "sdec.h"
#include "pvr.h"
#include "rdvb_dmx_filtermgr.h"
#include "sdec_ecp.h"
#include "dmx_inbandcmd.h"
#include "./tvscalercontrol/scaler_vtdev.h"

#define _6k_align(x)  (((x+6*1024 -1)/(6*1024))*(6*1024))
extern struct rdvb_dmxdev* get_rdvb_dmxdev(void);
extern struct rdvb_dmx* get_rdvb_dmx(unsigned char idx);

SDEC_CHANNEL_T* rdvb_dmx_getSdec(unsigned char chid)
{
	struct rdvb_dmx     * rdvb_dmx= NULL;
	struct dmx_demux    * dmx_demux= NULL;
	SDEC_CHANNEL_T      * sdec= NULL;

	rdvb_dmx   = get_rdvb_dmx(chid);
	dmx_demux  = rdvb_dmx->dmxdev.demux;
	sdec       = (SDEC_CHANNEL_T*) dmx_demux;

	return sdec;
}

static int dmx_ctrl_mtp_streaming(SDEC_CHANNEL_T *sdec, bool bEnable)
{
	int ret = 0;
	TPK_MTP_STREAM_CTRL_T ctrl = bEnable ? MTP_STREAM_START : MTP_STREAM_STOP;

	if (sdec->mtpBuffer.size == 0) {
		dmx_err(sdec->idx, " Error!! MTP buffer not ready !!\n");
		return -1;
	}

	ret = RHAL_MTPStreamControl(sdec->idx, ctrl);
	if (ret == 0) {
		dmx_notice(sdec->idx,
			"RHAL_MTPStreamControl bEnable=%d success !! \n", bEnable);
	} else {
		dmx_err(sdec->idx,
			"RHAL_MTPStreamControl bEnable=%d failed  !! \n", bEnable);
	}

	return ret;
}

static int dmx_ctrl_notify_flushed(SDEC_CHANNEL_T* sdec,enum pin_type pin_type,
		int pin_port, struct dmx_priv_data* priv_data)
{
	unsigned int status = 0;
	struct ts_parse_context * tpc = NULL;
	enum data_context_type type = DATA_CONTEXT_TYPE_MAX;
	if (priv_data == NULL || sdec == NULL){
		dmx_err(NOCH, "ERROR  param invalid\n");
		return -1;

	}
	if (pin_type == PIN_TYPE_VTP){
		type = DATA_CONTEXT_TYPE_VIDEO;
		if (pin_port == 0){
			status = STATUS_FLUSH_VIDEO;
			sdec->videoDecodeMode[0] = 
				priv_data->data.video_decode_mode;
		} else if (pin_port == 1){
			status = STATUS_FLUSH_VIDEO2;
			sdec->videoDecodeMode[1] = 
				priv_data->data.video_decode_mode;
		} else {
			dmx_err(sdec->idx, "ERROR: UNKNOW pin:%d_%d\n",pin_type, pin_port);
			return -1;
		}
	} else
		return -1;
		
	LOCK(&sdec->mutex);
	tpc = tpc_find_by_pin(&sdec->tpc_list_head, type, pin_port);
	if (tpc)  {
		sdec_flush_by_tpc(tpc);
		sdec_reset_pts_by_tpc(tpc);
	}
	if (sdec->tp_src == MTP) {
		REFCLK_Reset(sdec->refClock.prefclk, sdec->avSyncMode, sdec->v_fr_thrhd);
		PCRSYNC_Reset(&sdec->pcrProcesser);
	}
	if (dmx_ib_send_video_new_seg(PIN_TYPE_VTP, pin_port, sdec->is_ecp) < 0 ||
		dmx_ib_send_video_dtv_src(PIN_TYPE_VTP, pin_port, sdec->is_ecp) < 0 ||
		dmx_ib_send_video_decode_cmd(PIN_TYPE_VTP, pin_port,
				(void *)&priv_data->data.video_decode_mode, sdec->is_ecp) < 0){
		dmx_err( sdec->idx, "Deliver VIDEO START COMMAND FAIL:pin(%d_%d)\n",
			pin_type, pin_port);
		UNLOCK(&sdec->mutex);
		return -1;
	}
	if (tpc)
		tpc->start = true;

	if (sdec->tp_src == MTP) {
		// Reset AVSync to reset buffering start time
		if (sdec->avSyncMode == MTP_AVSYNC_MODE) {
			SDEC_ResetAVSync(sdec, MTP_AVSYNC_MODE);
		}
		//Re-start MTP streaming
		dmx_ctrl_mtp_streaming(sdec, true);
		RESET_FLAG(sdec->status, STATUS_FLUSH_MTP);
	}
	UNLOCK(&sdec->mutex);
	return 0;
}

static int dmx_ctl_audio_format(SDEC_CHANNEL_T* sdec,enum pin_type pin_type,
		int pin_port, struct dmx_priv_data* priv_data)
{
	unsigned int event = 0;
	struct ts_parse_context * tpc = NULL;
	enum data_context_type type = DATA_CONTEXT_TYPE_MAX;
	if (priv_data == NULL || sdec == NULL){
		dmx_err(NOCH, "ERROR  param invalid\n");
		return -1;
	}
	if (pin_type != PIN_TYPE_ATP) {
		dmx_err(sdec->idx, "ERROR :pin type:%d!! \n", pin_type);
		return -1;
	}
	
	type = DATA_CONTEXT_TYPE_AUDIO;
	if (pin_port == 0){
		sdec->audioFormat = priv_data->data.audio_format.audio_format;
		memset(sdec->audioPrivInfo, 0, sizeof(sdec->audioPrivInfo));
		memcpy(sdec->audioPrivInfo,
			priv_data->data.audio_format.private_data, 
			sizeof(priv_data->data.audio_format.private_data));
		event = EVENT_SET_AUDIO_FORMAT;
	} else if (pin_port == 1) {
		sdec->audioFormat2 = priv_data->data.audio_format.audio_format;
		memset(sdec->audioPrivInfo2, 0, sizeof(sdec->audioPrivInfo2));
		memcpy(sdec->audioPrivInfo2,
			priv_data->data.audio_format.private_data, 
			sizeof(priv_data->data.audio_format.private_data));
		event = EVENT_SET_AUDIO2_FORMAT;

	} else {
		dmx_err(sdec->idx, "ERROR :type:%d port:%d!\n",pin_type, pin_port);
		return -1;
	}
	dmx_info(sdec->idx, "Audio Format:%d!!\n", sdec->audioFormat);
	LOCK(&sdec->mutex);
	//SET_FLAG(sdec->events, event);

	if (dmx_ib_send_audio_new_fmt(pin_type, pin_port, 
				priv_data->data.audio_format.audio_format,
				priv_data->data.audio_format.private_data, sdec->is_ecp) < 0 ){
		dmx_err(sdec->idx, "ERROR: FAIL send audio format:pin(%d_%d)\n",
			pin_type, pin_port);
		UNLOCK(&sdec->mutex);
		return -1;
	}
	tpc = tpc_find_by_pin(&sdec->tpc_list_head, type, pin_port);
	if (tpc)
		tpc->start = true;
	UNLOCK(&sdec->mutex);
	return 0;
}
int rdvb_dmx_ctrl_privateInfo(enum dmx_priv_cmd dmx_priv_cmd, enum pin_type pin_type,
	int pin_port, struct dmx_priv_data* priv_data)
{
	int ret = 0;
	int sdec_port = rdvb_dmxbuf_get_dmxPort(pin_type, pin_port);
	//unsigned int status = 0;
	SDEC_CHANNEL_T* sdec =NULL;
	if (sdec_port < 0) 	{
		dmx_err(NOCH, "ERROR: fail get SDEC port: pin(%d/%d) \n",
			pin_type, pin_port);
		return -1;
	}
	sdec = rdvb_dmx_getSdec(sdec_port);
	if (sdec == NULL) {
		dmx_err(NOCH, "ERROR: FAIL to found SDEC, sdecport:%d \n", sdec_port);
		return -1;
	}
	dmx_err(sdec->idx, "cmd:%d, pin_type:%d, pin_port:%d \n",
		 dmx_priv_cmd, pin_type, pin_port);

	switch(dmx_priv_cmd) {
	case DMX_PRIV_CMD_VIDEO_FLUSH_BEGIN:
		if (sdec->tp_src == MTP) {
			LOCK(&sdec->mutex);
			dmx_ctrl_mtp_streaming(sdec, false);
			SDEC_PB_Reset(sdec);
			UNLOCK(&sdec->mutex);
		}
		break;
	case DMX_PRIV_CMD_NOTIFY_FLUSHED:
		if (dmx_ctrl_notify_flushed(sdec, pin_type, pin_port, priv_data) < 0) {
			return -1;
		}
		break;
	case DMX_PRIV_CMD_VIDEO_DECODE_MODE:
		if (pin_type != PIN_TYPE_VTP) {
			dmx_err(sdec->idx, "ERROR !!!!! \n");
			return -1;
		}
		if (pin_port == 0){
			sdec->videoDecodeMode[0] = priv_data->data.video_decode_mode;
		} else if (pin_port == 1){
			sdec->videoDecodeMode[1] = priv_data->data.video_decode_mode;
		} else {
			dmx_err(sdec->idx, "ERROR \n");
			return -1;
		}
		SET_FLAG(sdec->events, (pin_port == 0) ?
					EVENT_VIDEO0_NEW_DECODE_MODE :
					EVENT_VIDEO1_NEW_DECODE_MODE);
		dmx_err(sdec->idx, "VDEC%d decode mode = %d\n", 
			pin_port, sdec->videoDecodeMode[pin_port]);
		break;
	case DMX_PRIV_CMD_VIDEO_FREERUN_THRESHOLD:
	{
		u32 ulTemp = 0;
		if( (ulTemp - 1) != priv_data->data.video_freerun_threshlod) {
			sdec->v_fr_thrhd =
					priv_data->data.video_freerun_threshlod;
		} else {
			sdec->v_fr_thrhd = DEFAULT_FREE_RUN_VIDEO_THRESHOLD;
		}
		break;
	}
	case DMX_PRIV_CMD_AUDIO_FORMAT:
		if (dmx_ctl_audio_format(sdec, pin_type, pin_port, priv_data) < 0) {
			return -1;
		}
		break;
	case DMX_PRIV_CMD_AUDIO_PAUSE:
		/* Because audio trick play mode is wrong in some cases,
		 * for example, double PAUSE,
		 * we reset this status at PVR upload speed is change to play
		 */
		SDEC_PB_AudioPause(sdec, true);
		break;
	case DMX_PRIV_CMD_AUDIO_UNPAUSE:
		SDEC_PB_AudioPause(sdec, false);
		break;

	default:
		ret = -1;
		break;
	}
	if (ret < 0) {
		dmx_err(sdec->idx,  "unKnow Command: %d \n", dmx_priv_cmd);
	}
	return 0;
}
EXPORT_SYMBOL(rdvb_dmx_ctrl_privateInfo);

int rdvb_dmx_ctrl_getBufferInfo(enum pin_type pin_type, int pin_port,
	enum host_type host_type, int host_port, struct dmx_buff_info_t *dmx_buff_info)
{
	int sdec_port = rdvb_dmxbuf_get_dmxPort(pin_type, pin_port);
	SDEC_CHANNEL_T* sdec =NULL;
	struct buffer_header_info buffer_header_info;
	if (sdec_port < 0){
		dmx_err(NOCH, "ERROR: fail get SDEC port: pin(%d/%d) dest(%d/%d)\n",
			pin_type, pin_port, host_type, host_port);
		return -1;
	}
	sdec = rdvb_dmx_getSdec(sdec_port);
	if (sdec == NULL) {
		dmx_err(NOCH, "ERROR: FAIL to found SDEC, sdecport:%d \n", sdec_port);
		return -1;
	}
	dmx_notice(sdec->idx, "pin:(%d_%d), host:(%d,%d)\n",
				pin_type, pin_port, host_type, host_port);
	if (rdvb_dmxbuf_request(pin_type, pin_port, host_type, host_port,
				NULL, &buffer_header_info) < 0) {
		dmx_err(sdec->idx, "ERROR: fail get SDEC port: pin(%d/%d) dest(%d/%d)\n",
			pin_type, pin_port, host_type, host_port);
		return -1;
	}
	dmx_buff_info->bsPhyAddr              = buffer_header_info.bs_header_addr;
	dmx_buff_info->ibPhyAddr              = buffer_header_info.ib_header_addr;
	dmx_buff_info->refClockPhyAddr        = sdec->refClock.refClock.phyaddr;
	dmx_buff_info->bsHeaderSize           = buffer_header_info.bs_header_size;
	dmx_buff_info->ibHeaderSize           = buffer_header_info.ib_header_size;
	dmx_buff_info->refClockHeaderSize     = sdec->refClock.refClock.bufSize;
	return 0;
}
EXPORT_SYMBOL(rdvb_dmx_ctrl_getBufferInfo);

int rdvb_dmx_ctrl_releaseBuffer(enum pin_type pin_type, int pin_port,
	 enum host_type host_type, int host_port)
{
	return rdvb_dmxbuf_release(pin_type, pin_port, host_type, host_port);
}
EXPORT_SYMBOL(rdvb_dmx_ctrl_releaseBuffer);

int rdvb_dmx_ctrl_currentSrc_isMTP(enum pin_type pin_type, int pin_port,
	uint8_t *retValue)
{
	int sdec_port = rdvb_dmxbuf_get_dmxPort(pin_type, pin_port);
	SDEC_CHANNEL_T* sdec =NULL;

	if (sdec_port < 0) {
		dmx_err(NOCH, "ERROR: fail get SDEC port: pin(%d/%d) \n",
			pin_type, pin_port);
		return -1;
	}

	sdec = rdvb_dmx_getSdec(sdec_port);
	if (sdec == NULL) {
		dmx_err(NOCH, "ERROR: FAIL to found SDEC, sdecport:%d \n", sdec_port);
		return -1;
	}

	if (sdec->tp_src == MTP)
		*retValue = 1;

	return 0;
}
EXPORT_SYMBOL(rdvb_dmx_ctrl_currentSrc_isMTP);

static int dmx_ctrl_tp_config(int tp_id, bool is192, TPK_INPUT_PORT_T portType,
	enum dmx_ext_port_type input_port_type, unsigned char castype,
	unsigned char syncbyte)
{
	TPK_TP_STATUS_T tpStatus;
	TPK_TP_SOURCE_T tp_src;

	if (RHAL_GetTpStatus(tp_id, &tpStatus) != TPK_SUCCESS)
		return TPK_FAIL;

	tpStatus.tp_param.casType = castype;
	tpStatus.tp_param.sync_byte = syncbyte;
	tpStatus.tp_param.serial = (input_port_type == DMX_EXT_PORT_TYPE_SERIAL);

	tpStatus.tp_param.prefix_en          = is192;
	tpStatus.tp_param.ts_in_tsp_len      = is192 ? byte_192 : byte_188;
	tpStatus.tp_param.keep               = (is192 && portType==TPK_PORT_FROM_MEM)
						? true: false;
	tpStatus.tp_param.clr_tsp_scrmbl_bit = 0x1;
	tpStatus.tp_param.pid_filter_en      = 0x1;

	tpStatus.tp_param.frc_en = false;
	if (RHAL_SetTPMode(tp_id, tpStatus.tp_param) != TPK_SUCCESS){
		dmx_err(tp_id, "ERROR: SET DEMOD PATH FAIL \n");
		return -1;
	}

	tp_src = RHAL_GetTPSource(portType);
	if (RHAL_SetTPSource(tp_id,tp_src,tpStatus.tp_param.casType) != TPK_SUCCESS){
		dmx_err(tp_id, "ERROR: SET DEMOD PATH FAIL \n");
		return -1;
	}

	return tp_src;
}

static int dmx_ctrl_set_tpbuf(SDEC_CHANNEL_T *sdec, u32 phyaddr, 
		u8* viraddr, unsigned int *size, bool iscache)
{
	unsigned int tp_buf_align  = 0;

	sdec->tpBuffer.phyAddr = phyaddr;
	//sdec->tpBuffer.allocSize = size;
	sdec->tpBuffer.isCache = iscache;
	if (iscache)
		sdec->tpBuffer.cachedaddr = viraddr;
	else
		sdec->tpBuffer.nonCachedaddr = viraddr;
	sdec->tpBuffer.virAddr = viraddr;

	sdec->tpBuffer.fromPoll = 0;
	sdec->tpBuffer.phyAddrOffset = (long)phyaddr - (long)viraddr;
	if (sdec->tsPacketSize == 192)
		tp_buf_align = (6*1024); //alignment（6k, 192 ,8）
	else
		tp_buf_align = (282*1024); //alignment（6k, 188 ,8）

	sdec->tpBuffer.size = __alignment_up(*size, tp_buf_align);//*size - (*size % tp_buf_align);
	if (sdec->is_ecp) {
		unsigned int caved_buf_size = 0;
		unsigned int caved_buf_phy = 0;
		if (sdec->idx < 2)
			caved_buf_size = carvedout_buf_query(CARVEDOUT_TP,(void**)&caved_buf_phy);
		dmx_ta_buf_map(sdec->idx, buf_type_tp, phyaddr, sdec->tpBuffer.size,
									caved_buf_phy ,caved_buf_size, 0 ,0, 0 ,0);
	}
	RHAL_SetTPRingBuffer(sdec->idx, (UINT8 *)phyaddr, (UINT8 *)viraddr,
				sdec->tpBuffer.size);
	pvr_rec_set_bufinfo(&sdec->pvrRecSetting, phyaddr, phyaddr+sdec->tpBuffer.size,
						sdec->tpBuffer.phyAddrOffset);
	*size = sdec->tpBuffer.size;
	return 0;
}
static int dmx_ctrl_unset_tpbuf(SDEC_CHANNEL_T *sdec)
{
	if (sdec->is_ecp)
		dmx_ta_buf_unmap(sdec->idx, buf_type_tp);
	pvr_rec_set_bufinfo(&sdec->pvrRecSetting, 0, 0, 0);
	sdec->tpBuffer.phyAddr = 0;
	return 0;
}

static int dmx_ctrl_set_mtpbuf(SDEC_CHANNEL_T *sdec, u32 phyaddr, 
		u8* viraddr,unsigned int size, bool iscache)
{
	unsigned int tp_buf_align  = 0;

	sdec->mtpBuffer.phyAddr = phyaddr;
	sdec->mtpBuffer.isCache = iscache;
	//sdec->mtpBuffer.allocSize = size;
	if (iscache)
		sdec->mtpBuffer.nonCachedaddr = viraddr;
	else
		sdec->mtpBuffer.cachedaddr = viraddr;
	sdec->mtpBuffer.virAddr = viraddr;

	sdec->mtpBuffer.fromPoll = 0;
	sdec->mtpBuffer.phyAddrOffset = (long)phyaddr - (long)viraddr;
	if (sdec->tsPacketSize == 192)
		tp_buf_align = 192;//alignment(188, 8)
	else
		tp_buf_align = 376; //alignment(188, 8)

	sdec->mtpBuffer.size = __alignment_up(size, tp_buf_align);//size - (size % tp_buf_align);
	dmx_crit(sdec->idx, "mtpBuffer.phyAddr = 0x%lx\n",
		 (long)sdec->mtpBuffer.phyAddr);

	if (RHAL_SetMTPBuffer(sdec->idx, (UINT8 *)phyaddr, (UINT8 *)viraddr,
				sdec->mtpBuffer.size) != TPK_SUCCESS) {
		dmx_err(sdec->idx, "Error !!RHAL_SetMTPBuffer failed!\n");
		return -1;
	}

	pvr_pb_set_mtp(&sdec->pvrPbSetting, (UINT8 *)sdec->mtpBuffer.phyAddr,
		(UINT8 *)sdec->mtpBuffer.phyAddr+ sdec->mtpBuffer.size,
		sdec->mtpBuffer.phyAddrOffset);

	if (dmx_ctrl_mtp_streaming(sdec, true) < 0) {
		return -1;
	}

	sdec->is_playback = true;
	return 0;
}

static int dmx_ctrl_unset_mtpbuf(SDEC_CHANNEL_T *sdec)
{
	int ret = 0;

	if (sdec->is_playback) {
		ret = dmx_ctrl_mtp_streaming(sdec, false);
	}

	pvr_pb_set_mtp(&sdec->pvrPbSetting, NULL, NULL, 0);
	sdec->mtpBuffer.phyAddr = 0;
	//sdec->mtpBuffer.allocSize = 0;
	sdec->is_playback = false;
	/*Notify TP that mtp buffer was freed*/
	RHAL_SetMTPBuffer(sdec->idx, NULL, NULL, 0);

	return ret;
}
static int dmx_ctrl_tp_buf_config(SDEC_CHANNEL_T *sdec, bool is_playback)
{
	u32 phyaddr = 0;
	u8 *viraddr = 0;
	unsigned int size = 0;
	enum tp_con_type tct;
	enum tp_buf_type tbt;

	tbt = sdec->bIsSDT? TP_BUF_LEVEL_B : TP_BUF_LEVEL_A;
	tct = TP_CON_LIVE_A + sdec->idx;

	if (rdvb_dmxbuf_tpbuf_get(tct, tbt, &phyaddr, &viraddr, &size,
		NON_CACHED_BUFF) < 0) {
		dmx_err(sdec->idx,
			"ERROR: SET DEMOD PATH FAIL: fail get tp buffer \n");
		return -1;
	}

	if (dmx_ctrl_set_tpbuf(sdec, phyaddr, viraddr, &size, NON_CACHED_BUFF) < 0){
		rdvb_dmxbuf_tpbuf_put(tct);
		dmx_err(sdec->idx,
			"ERROR: SET DEMOD PATH FAIL: fail set tp buffer \n");
		return -1;
	}
	return 0;
}
static int dmx_ctrl_setup_DEMOD_path(SDEC_CHANNEL_T *sdec, bool is_inter_demod,
		int input_port_num, enum dmx_ext_port_type input_port_type )
{
	int castype = 0;
	unsigned char syncbyte = 0x00;
	TPK_INPUT_PORT_T portType;
	int tp_src = 0;

	//tp config
	if (is_inter_demod) /*internal demod*/ {
		if (input_port_num == 0)
			portType = TPK_PORT_IN_DEMOD0;
		else
			portType = TPK_PORT_IN_DEMOD1;
		dmx_err(sdec->idx, "HACK input type to PARALLEL(%d-->%d)!",
			input_port_type, DMX_EXT_PORT_TYPE_PARALLEL);
		input_port_type = DMX_EXT_PORT_TYPE_PARALLEL;
	} else {  /*ext-demod*/
		if (input_port_num == 0)
			portType = TPK_PORT_EXT_INPUT0;
		else
			portType = TPK_PORT_EXT_INPUT1;

	}
	syncbyte  = 0x47;
	sdec->tsPacketSize = 192;
	tp_src = dmx_ctrl_tp_config(sdec->idx, true, portType, input_port_type,
				castype, syncbyte);
	if (tp_src < 0) {
		dmx_err(sdec->idx,
			"ERROR: SET DEMOD PATH FAIL : fail config tp\n");
		return -1;
	}
	sdec->tp_src = (TPK_TP_SOURCE_T)tp_src;
	// buf
	if (dmx_ctrl_tp_buf_config(sdec, false) <  0) {
		dmx_err(sdec->idx, "ERROR: SET DEMOD PATH FAIL: fail set tp buffer \n");
		return -1;
	}
	sdec->is_playback = false;
	return 0;
}
static int dmx_ctrl_setup_CI_path(SDEC_CHANNEL_T *sdec)
{
	int castype = 0;
	unsigned char syncbyte = 0x00;
	TPK_INPUT_PORT_T portType;
	int tp_src = 0;

	portType = TPK_PORT_IN_DEMOD0;//ci case not care
	castype  = 1;
	//tp config
	syncbyte  = 0x47;
	sdec->tsPacketSize = 192;
	tp_src = dmx_ctrl_tp_config(sdec->idx, true, portType, TPK_INPUT_PARALLEL,
				castype, syncbyte);
	if (tp_src < 0) {
		dmx_err(sdec->idx,
			"ERROR: SET CI PATH FAIL : fail config tp\n");
		return -1;
	}
	sdec->tp_src = (TPK_TP_SOURCE_T)tp_src;
	// buf
	if (dmx_ctrl_tp_buf_config(sdec, false) <  0) {
		dmx_err(sdec->idx, "ERROR: SET CI PATH FAIL: fail set tp buffer \n");
		return -1;
	}
	sdec->is_playback = false;
	return 0;
}
static int dmx_ctrl_setup_CIP_path(SDEC_CHANNEL_T *sdec, UINT8 cpi_port)
{
	int castype = 0;
	unsigned char syncbyte = 0x00;
	TPK_INPUT_PORT_T portType;
	int tp_src = 0;

	portType = TPK_PORT_IN_DEMOD0;//ci case not care
	castype  = 1;
	//tp config
	if (cpi_port == 0)
		syncbyte  = 0x47;
	else if (cpi_port == 1)
		syncbyte  = 0x48;
	else if (cpi_port == 2)
		syncbyte  = 0x49;
	else {
		dmx_err(sdec->idx, "ERROR: SET CPI PATH FAIL : fail config tp\n");
		return -1;
	}
	sdec->tsPacketSize = 192;
	tp_src = dmx_ctrl_tp_config(sdec->idx, true, portType, TPK_INPUT_PARALLEL,
				castype, syncbyte);
	if (tp_src < 0) {
		dmx_err(sdec->idx, "ERROR: SET CPI PATH FAIL : fail config tp\n");
		return -1;
	}
	sdec->tp_src = (TPK_TP_SOURCE_T)tp_src;
	// buf
	if (dmx_ctrl_tp_buf_config(sdec, false) <  0) {
		dmx_err(sdec->idx, "ERROR: SET DEMOD PATH FAIL: fail set tp buffer \n");
		return -1;
	}
	sdec->is_playback = false;
	return 0;
}

static int dmx_ctrl_setup_MEM_path(SDEC_CHANNEL_T *sdec)
{
	u32 phyaddr = 0;
	u8 *viraddr = 0;
	int castype = 0;
	unsigned char syncbyte = 0x00;
	TPK_INPUT_PORT_T portType;
	unsigned int size = 0;
	unsigned int tp_buf_size;
	unsigned int mtp_buf_size;
	enum tp_con_type tct;
	enum tp_buf_type tbt;
	//tp config
	int tp_src = 0;

	syncbyte  = 0x47;
	sdec->tsPacketSize = 192;
	portType = TPK_PORT_FROM_MEM;
	tp_src = dmx_ctrl_tp_config(sdec->idx, true, portType,
			DMX_EXT_PORT_TYPE_SERIAL, castype, syncbyte);
	if (tp_src < 0) {
		dmx_err(sdec->idx, "ERROR: SET MEM PATH FAIL \n");
		return -1;
	}
	sdec->tp_src = (TPK_TP_SOURCE_T)tp_src;
	// buf
	tbt = sdec->bIsSDT? TP_BUF_LEVEL_B : TP_BUF_LEVEL_A; 
	tct = TP_COM_PLAYBACK_A + sdec->idx;

	if (rdvb_dmxbuf_tpbuf_get(tct, tbt, &phyaddr, &viraddr, &size,
		NON_CACHED_BUFF) < 0) {
		dmx_err(sdec->idx,	"ERROR: SET MEM PATH FAIL: fail get tp buffer \n");
		return -1;
	}
	tp_buf_size = size-MTP_BUFFER_SIZE;
	if (dmx_ctrl_set_tpbuf(sdec, phyaddr, viraddr, &tp_buf_size,
		NON_CACHED_BUFF) < 0) {
		rdvb_dmxbuf_tpbuf_put(tct);
		dmx_err(sdec->idx, "ERROR: SET MEM PATH FAIL: fail set tp buffer \n");
		return -1;
	}
	mtp_buf_size = size - tp_buf_size;
	//alignment mpt start address
	if (dmx_ctrl_set_mtpbuf(sdec, phyaddr+tp_buf_size, viraddr+ tp_buf_size,
		mtp_buf_size, NON_CACHED_BUFF) < 0) {
		dmx_ctrl_unset_tpbuf(sdec);
		rdvb_dmxbuf_tpbuf_put(tct);
		dmx_err(sdec->idx, "ERROR: SET MEM PATH FAIL: fail set mtp buffer \n");
		return -1;
	}
	REFCLK_SetClockMode(sdec->refClock.prefclk, MISC_90KHz);//for std

	return 0;
}

static int dmx_ctrl_unset(SDEC_CHANNEL_T *sdec)
{
	enum tp_con_type tct;
	tct = sdec->is_playback?
		TP_COM_PLAYBACK_A + sdec->idx:
		TP_CON_LIVE_A + sdec->idx;
	if (dmx_ctrl_unset_tpbuf(sdec) < 0) {
		return -1;
	}
	if (dmx_ctrl_unset_mtpbuf(sdec) < 0) {
		return -1;
	}
	if (rdvb_dmxbuf_tpbuf_put(tct) < 0) {
		return -1;
	}
	sdec->is_playback = false;
	sdec->pcrProcesser.bUseFixedRCD = false;
	sdec->tp_src = TS_UNMAPPING;
	return 0;
}

static int SDEC_ThreadProcEntry(void *pParam)
{
	struct cpumask demux_cpumask;
	SDEC_CHANNEL_T *const pCh = (SDEC_CHANNEL_T *)pParam;

	struct sched_param param = {.sched_priority = 1};
	sched_setscheduler(current, SCHED_RR, &param);
	current->flags &= ~PF_NOFREEZE;

	cpumask_clear(&demux_cpumask);
	cpumask_set_cpu(2, &demux_cpumask); // run task in core 3
	//sched_setaffinity(0, &demux_cpumask);
	if (pCh->is_ecp)
		sdec_ecp_ThreadProc(pCh);
	else
		SDEC_ThreadProc(pCh);
	return 0;
}
static int dmx_ctrl_stop_tp(SDEC_CHANNEL_T *sdec)
{	
	RHAL_TPStreamControl(sdec->idx, TP_STREAM_STOP);
	if (sdec->threadState != SDEC_STATE_STOP) {
		if (sdec->thread_task)
			kthread_stop(sdec->thread_task);
		sdec->thread_task = 0;
	}

	dmx_notice(sdec->idx, " STOP TP !!!!!\n");
	return 0;
}
static int dmx_ctrl_start_tp(SDEC_CHANNEL_T *sdec)
{
	//char pstr[10] = {0};
	/*
	snprintf(pstr,9,"RDVB_%d", sdec->idx);
	sdec->thread_task = kthread_create(SDEC_ThreadProcEntry, (void *)sdec, pstr);
	if (!(sdec->thread_task))
		return -1;

	wake_up_process(sdec->thread_task);
	*/
	sdec->thread_task = kthread_run(SDEC_ThreadProcEntry, (void*)sdec, 
		"RDVB_%d",sdec->idx);
	if (IS_ERR(sdec->thread_task))
		return -1;
	RHAL_TPStreamControl(sdec->idx, TP_STREAM_START);
	dmx_notice(sdec->idx, "START TP!!!!\n");
	return 0;

}

int rdvb_dmx_ctrl_config(SDEC_CHANNEL_T *sdec, enum dmx_src_type input_src_type,
	unsigned int input_port_num, enum dmx_ext_port_type input_port_type)
{
	int ret = -1;
	struct rdvb_dmxdev * rdvb_dmxdev = get_rdvb_dmxdev();
	dmx_notice(sdec->idx, "src_typ:%d, port_num:%d, portType:%d .\n",
		input_src_type, input_port_num, input_port_type);

	LOCK(&rdvb_dmxdev->mutex);
	dmx_ctrl_stop_tp(sdec);
	if (dmx_ctrl_unset(sdec) < 0) {
		dmx_err(sdec->idx,"ERROR: stop demux path FAIL \n");
		goto end;
	}

	switch(input_src_type) {
	case DMX_EXT_SRC_TYPE_IN_DEMOD:
	case DMX_EXT_SRC_TYPE_EXT_DEMOD:
		if (dmx_ctrl_setup_DEMOD_path(sdec,
			input_src_type==DMX_EXT_SRC_TYPE_IN_DEMOD,
			input_port_num, input_port_type) < 0) {
			dmx_err(sdec->idx, "ERROR: SET DEMOD path FAIL \n");
			goto end;
		}
	break;
	case DMX_EXT_SRC_TYPE_CI:
		if (dmx_ctrl_setup_CI_path(sdec) < 0) {
			dmx_err(sdec->idx, "ERROR: SET CI path FAIL \n");
			goto end;
		}
	break;
	case DMX_EXT_SRC_TYPE_CIP:
		if (dmx_ctrl_setup_CIP_path(sdec, input_port_num) < 0) {
			dmx_err(sdec->idx, "ERROR: SET CIP path FAIL \n");
			goto end;
		}
	break;
	case DMX_EXT_SRC_TYPE_MEM:
		if (dmx_ctrl_setup_MEM_path(sdec)) {
			dmx_err(sdec->idx, "ERROR: SET MEM path FAIL \n");
			goto end;
		}
	break;
	default:
		dmx_ctrl_stop_tp(sdec);
		ret = 0;
		goto end;
	break;
	}

	ret = dmx_ctrl_start_tp(sdec);
end:
	UNLOCK(&rdvb_dmxdev->mutex);
	return ret;
}

/*
*  this api use by cip 1.4 add tpp filter
*    *filter : NULL,     ADD     filter
*    *filter : != NULL,  REMOVER filter.
*  
*/
int rdvb_dmx_ctrl_ci_filter(unsigned char id, unsigned short pid , void **filter)
{
	struct rdvb_dmx     * rdvb_dmx= NULL;

	rdvb_dmx   = get_rdvb_dmx(id);

	if (filter == NULL) {
		return -1;
	}
	if (*filter == NULL) {
		*filter = rdvbdmx_filter_bypass_add(rdvb_dmx, pid);
		if (*filter == NULL)
			return -1;
	} else {
		rdvbdmx_filter_bypass_del(*filter);
	}
	return 0;
}
EXPORT_SYMBOL(rdvb_dmx_ctrl_ci_filter);

int rdvb_dmx_ctrl_get_stc(SDEC_CHANNEL_T *sdec, u64 *stc, unsigned int *base)
{
	long long rcd;
	enum  clock_type clock_type;
	REFCLOCK * const pRefClock = sdec->refClock.prefclk;
	*stc = 0;
	*base = 1;
	if (pRefClock == NULL){
		dmx_err(sdec->idx, "ERROR: REFCLOCK null FAIL \n");
		return -1;
	}
	rcd = REFCLK_GetRCD(pRefClock);
	if (rcd == -1)
		return 0;
	clock_type = REFCLK_GetClockMode(pRefClock);
	*stc = CLOCK_GetAVSyncPTS(clock_type) + rcd;

	return 0;
}


int rdvb_dmx_ClockRecovery(SDEC_CHANNEL_T *sdec, bool bOnOff)
{
	int ret = 0;
	if (sdec == 0)
		return -1;

	dmx_function_call(sdec->idx);
	dmx_notice(sdec->idx, "clock recovery auto=%d\n", bOnOff);
	if (bOnOff == 1) {
		if(sdec->PCRPID > 0 && sdec->PCRPID < 0x1fff)
			ret = SDEC_EnablePCRTracking(sdec);
		sdec->pcrProcesser.bUseFixedRCD = false;
	} else {
		sdec->pcrProcesser.bUseFixedRCD = true;
		return SDEC_DisablePCRTracking(sdec);
	}
	return ret;
}

int rdvb_dmx_ctrl_pvr_write(SDEC_CHANNEL_T *sdec, const char __user *buf, size_t count)
{
	PVR_PB_SETTING * pvr = &sdec->pvrPbSetting;
	return pvr_pb_write_mtp(pvr, buf, count);
}

int rdvb_dmx_ctrl_pvr_SetRate(SDEC_CHANNEL_T *sdec, int speed)
{
	int dmxSpeed = 0;
	dmxSpeed = (256 * speed) / 100 ;

	return SDEC_PB_SetSpeed(sdec, dmxSpeed);
}

int rdvb_dmx_ctrl_pvr_open(SDEC_CHANNEL_T *sdec, dvr_rec_cb dvr_rec_cb, void *source)
{
	PVR_REC_SETTING * pvr = &sdec->pvrRecSetting;
	return pvr_rec_open(pvr, dvr_rec_cb, source);
}

int rdvb_dmx_ctrl_pvr_close(struct dmx_demux *demux)
{
	SDEC_CHANNEL_T *sdec = (SDEC_CHANNEL_T *)demux;

	if ((demux->frontend) && (demux->frontend->source == DMX_MEMORY_FE)) {
		sdec->pvrPbSpeed = 256;
		SDEC_PB_AudioPause(sdec, false);
		SDEC_ResetAVSync(sdec, NAV_AVSYNC_FREE_RUN);
		RESET_FLAG(sdec->status, STATUS_FLUSH_MTP);
		return pvr_pb_close(&sdec->pvrPbSetting);
	}

	return pvr_rec_close(&sdec->pvrRecSetting);
}

int rdvb_dmx_ctrl_pvr_reset(SDEC_CHANNEL_T *sdec)
{
	LOCK(&sdec->mutex);
	if (sdec->avSyncMode == MTP_AVSYNC_MODE) {
		SET_FLAG(sdec->status, STATUS_FLUSH_MTP);
	} else if (sdec->avSyncMode == NAV_AVSYNC_AUDIO_ONLY) {
		dmx_ctrl_mtp_streaming(sdec, false);
		SDEC_PB_Reset(sdec);
		dmx_ctrl_mtp_streaming(sdec, true);
		RESET_FLAG(sdec->status, STATUS_FLUSH_MTP);
	}
	UNLOCK(&sdec->mutex);
	return 0;
}

int rdvb_dmx_ctrl_pvr_auth_verify(SDEC_CHANNEL_T *sdec, char *buf)
{
	PVR_REC_SETTING * pvr = &sdec->pvrRecSetting;
	LOCK(&sdec->mutex);
	pvr->pvrAuthVerifyResult = (dmx_ta_pvr_auth_verify(buf) == 0) ? PVR_AUTH_VERIFY_SUCCESS : PVR_AUTH_VERIFY_ERROR;
	UNLOCK(&sdec->mutex);
	return 0;
}

char * rdvb_dmx_ctrl_dump_tp_status (SDEC_CHANNEL_T *sdec, char *buf)
{
	unsigned int error = 0, receive = 0, drop = 0;
	RHAL_TrackErrorPacket(sdec->idx, &error, &receive, &drop);
	
	buf = print_to_buf(buf, "%08x %08x/%08x\n",
		receive, error, drop);
	return buf;
}

char * rdvb_dmx_ctrl_dump_pcr_status (SDEC_CHANNEL_T *sdec, char *buf)
{
	if (sdec->bIsSDT)
		return buf;
	buf = pcrsync_status_dump(&sdec->pcrProcesser, buf);
	return buf;
}

static void dmx_ctrl_ecp_on(SDEC_CHANNEL_T *sdec)
{
	bool need_start = false;
	struct ta_tpc_info tpcs = {};
	if (sdec->threadState != SDEC_STATE_STOP){
		dmx_ctrl_stop_tp(sdec);
		need_start = true;
	}
	LOCK(&sdec->mutex);
	if (sdec->tpBuffer.phyAddr){
		unsigned int caved_buf_size = 0;
		unsigned int caved_buf_phy = 0;
		if (sdec->idx < 2)
			caved_buf_size = carvedout_buf_query(CARVEDOUT_TP,(void**)&caved_buf_phy);
		dmx_err(sdec->idx, "tp map:0x%x/0x%x, 0x%x/0x%x", sdec->tpBuffer.phyAddr,
				sdec->tpBuffer.size, caved_buf_phy, caved_buf_size);
		dmx_ta_buf_map(sdec->idx, buf_type_tp, sdec->tpBuffer.phyAddr,
				sdec->tpBuffer.size, caved_buf_phy, caved_buf_size, 0 ,0, 0 ,0);
	}

	tpc_output_buf_ta_map(&sdec->tpc_list_head);
	tpc_pack(&sdec->tpc_list_head, &tpcs);
	dmx_ta_dmx_init(sdec->idx,sdec->tsPacketSize , &tpcs);
	//data transfor to ta
	//TODO:
		//tp set buffer protect;
	sdec_ecp_init(sdec);
	UNLOCK(&sdec->mutex);
	if (tpcs.tpc_infos)
		kfree(tpcs.tpc_infos);
	if (need_start)
		dmx_ctrl_start_tp(sdec);
}

static void dmx_ctrl_ecp_off(SDEC_CHANNEL_T *sdec)
{
	bool need_start = false;
	if (sdec->threadState != SDEC_STATE_STOP)
	{
		dmx_ctrl_stop_tp(sdec);
		need_start = true;
	}
	LOCK(&sdec->mutex);
	//release ta data
	//TODO:
	// need tp release buffer: waith thomas
	tpc_output_buf_ta_unmap(&sdec->tpc_list_head);

	dmx_ta_buf_unmap(sdec->idx, buf_type_tp);
	sdec->is_ecp = false;
	UNLOCK(&sdec->mutex);
	if (need_start)
		dmx_ctrl_start_tp(sdec);
}

void rdvb_dmx_ctrl_ecp_on(void)
{
	int i = 0;
	struct rdvb_dmxdev * rdvb_dmxdev = NULL;
	struct rdvb_dmx *rdvb_dmx = NULL;
	struct dmx_demux *dmx_demux = NULL;
	SDEC_CHANNEL_T *sdec = NULL;
	if (dmx_ta_init() < 0){
		dmx_err(NOCH, "ta init fail\n");
		return;
	}
	set_dtv_securestatus(TRUE);
	rdvb_dmxdev = get_rdvb_dmxdev();
	LOCK(&rdvb_dmxdev->mutex);
	rdvb_dmxbuf_ta_map_poll();
	for (i=0; i< rdvb_dmxdev->dmxnum; i++) {
		rdvb_dmx = &rdvb_dmxdev->rdvb_dmx[i];
		dmx_demux = rdvb_dmx->dmxdev.demux;
		sdec = (SDEC_CHANNEL_T *)dmx_demux;
		dmx_ctrl_ecp_on(sdec);
	}

	RHAL_TP_EnableProtect(TP_TP0);
	UNLOCK(&rdvb_dmxdev->mutex);
}
void rdvb_dmx_ctrl_ecp_off(void)
{

	int i = 0;
	struct rdvb_dmxdev *rdvb_dmxdev = NULL;
	struct rdvb_dmx *rdvb_dmx = NULL;
	struct dmx_demux *dmx_demux = NULL;
	SDEC_CHANNEL_T *sdec = NULL;
	rdvb_dmxdev = get_rdvb_dmxdev();
	LOCK(&rdvb_dmxdev->mutex);
	RHAL_TP_DisableProtect(TP_TP0);
	for (i = 0; i < rdvb_dmxdev->dmxnum; i++) {
		rdvb_dmx = &rdvb_dmxdev->rdvb_dmx[i];
		dmx_demux = rdvb_dmx->dmxdev.demux;
		sdec = (SDEC_CHANNEL_T *)dmx_demux;
		dmx_ctrl_ecp_off(sdec);
	}
	dmx_ta_buf_unmap(-1, buf_type_poll);
	set_dtv_securestatus(FALSE);
	UNLOCK(&rdvb_dmxdev->mutex);
}
