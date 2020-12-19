#include "dmx_filter.h"
#include "rdvb-buf/rdvb_dmx_buf.h"


void dmx_filter_release(SDEC_CHANNEL_T* sdec, struct dmxfilter * df)
{
	if (df == NULL) {
		return;
	}
	if (df->priv.tpc == NULL)
		return;
	mutex_lock(&sdec->mutex);
	sdec_flush_by_tpc(df->priv.tpc);
	put_ts_parse_context(df->priv.tpc);
	df->priv.tpc = NULL;
	mutex_unlock(&sdec->mutex);
}

int dmx_filter_setpAV(SDEC_CHANNEL_T* sdec, struct dmxfilter *df )
{
	enum pin_type pin_type;
	enum data_context_type dct;
	int pin_port;
	switch(df->type) {
	case DEMUX_PID_VIDEO:
		pin_type = PIN_TYPE_VTP;
		pin_port =df->dest - DEMUX_DEST_VDEC0;
		dct = DATA_CONTEXT_TYPE_VIDEO;
	break;
	case DEMUX_PID_AUDIO:
		pin_type = PIN_TYPE_ATP;
		pin_port =df->dest - DEMUX_DEST_ADEC0;;
		dct = DATA_CONTEXT_TYPE_AUDIO;
	break;
	default:
		dmx_err(sdec->idx, "ERROR \n");
		return -1;

	break;
	}

	dmx_notice(sdec->idx, "type:%d, port:%d\n", pin_type, pin_port);
	if (rdvb_dmxbuf_request(pin_type, pin_port, HOST_TYPE_SDEC, sdec->idx,
		NULL, NULL)<0) {
		dmx_err(sdec->idx, "ERROR \n");
		return -1;
	}
	LOCK(&sdec->mutex);
	if (df->priv.tpc == NULL)
		df->priv.tpc = get_ts_parse_context(&sdec->tpc_list_head, df->pid);
	if (df->priv.tpc == NULL) {
		dmx_err(sdec->idx, "ERROR \n");
		goto error;
	}
	if (ts_parse_context_set(df->priv.tpc, dct, pin_port, NULL, NULL) < 0 ){
		dmx_err(sdec->idx, "ERROR \n");
		goto error;
	}
	if (sdec->is_ecp)
		ts_parse_context_ta_create(sdec->idx, df->priv.tpc);
	if (df->type == DEMUX_PID_VIDEO)
		memset(&sdec->video_status, 0, sizeof(struct video_status));
	UNLOCK(&sdec->mutex);

	ts_parse_context_dump(&sdec->tpc_list_head);
	return 0;
error:
	UNLOCK(&sdec->mutex);
	dmx_err(sdec->idx, "ERROR type:%d, port:%d\n", pin_type, pin_port);
	if (rdvb_dmxbuf_release(pin_type, pin_port, HOST_TYPE_SDEC, sdec->idx)<0) {
		dmx_err(sdec->idx, "ERROR \n");
		return -1;
	}
	return -1;
}

int dmx_filter_removeAV(SDEC_CHANNEL_T* sdec, struct dmxfilter *df)
{

	enum pin_type pin_type;
	int pin_port;
	switch(df->type) {
	case DEMUX_PID_VIDEO:
		pin_type = PIN_TYPE_VTP;
		pin_port =df->dest - DEMUX_DEST_VDEC0;
	break;
	case DEMUX_PID_AUDIO:
		pin_type = PIN_TYPE_ATP;
		pin_port =df->dest - DEMUX_DEST_ADEC0;
	break;
	default:
		dmx_err(sdec->idx, "ERROR type:%d \n", df->type);
		return -1;

	break;
	}

	dmx_notice(sdec->idx, "type:%d, port:%d\n", pin_type, pin_port);
	LOCK(&sdec->mutex);
	if (df->priv.tpc) {
		sdec_reset_pts_by_tpc(df->priv.tpc);
		if (sdec->is_ecp)
			ts_parse_context_ta_release(sdec->idx, df->priv.tpc);
		if (ts_parse_context_set(df->priv.tpc, DATA_CONTEXT_TYPE_FUNC,
													-1, NULL, NULL) < 0){
			dmx_err(sdec->idx, "ERROR type:%d, port:%d\n", pin_type, pin_port);
			goto error;
		}
	}
	UNLOCK(&sdec->mutex);
	ts_parse_context_dump(&sdec->tpc_list_head);

	if (rdvb_dmxbuf_release(pin_type, pin_port, HOST_TYPE_SDEC,
													sdec->idx)<0) {
		dmx_err(sdec->idx, "ERROR type:%d, port:%d\n", pin_type, pin_port);
		return -1;
	}
	return 0;
error:
	UNLOCK(&sdec->mutex);
	dmx_err(sdec->idx, "ERROR type:%d, port:%d\n", pin_type, pin_port);
	return -1;

}

int dmx_filter_setPES(SDEC_CHANNEL_T* sdec, struct dmxfilter *df,
		 ts_callback dfc)
{
	enum pin_type pin_type;
	int pes_buf_port;

	if (df->type  != DEMUX_PID_PES)
		return -1;

	pin_type = PIN_TYPE_PES;
	if (rdvb_dmxbuf_request(pin_type, -1, 
		HOST_TYPE_SDEC, sdec->idx, &pes_buf_port, NULL)<0) {
		dmx_err(sdec->idx, "ERROR \n");
		return -1;
	}

	LOCK(&sdec->mutex);
	if (df->priv.tpc == NULL)
		df->priv.tpc = get_ts_parse_context(&sdec->tpc_list_head, df->pid);
	if (df->priv.tpc == NULL) {
		dmx_err(sdec->idx, "ERROR \n");
		goto error;
	}
	if (ts_parse_context_set(df->priv.tpc, DATA_CONTEXT_TYPE_PES,
		pes_buf_port, dfc, df) < 0 ){
		dmx_err(sdec->idx, "ERROR \n");
		goto error;
	}
	if (sdec->is_ecp)
		ts_parse_context_ta_create(sdec->idx, df->priv.tpc);
	UNLOCK(&sdec->mutex);
	ts_parse_context_dump(&sdec->tpc_list_head);
	/*if (sdec->thread_task == 0)
		SDEC_StartThread(sdec);*/
	return 0;

error:
	UNLOCK(&sdec->mutex);
	if (rdvb_dmxbuf_release(pin_type, pes_buf_port, 
		HOST_TYPE_SDEC, sdec->idx)<0) {
		return -1;
	}
	dmx_err(sdec->idx, "ERROR \n");
	return -1;

}


int dmx_filter_removePES(SDEC_CHANNEL_T* sdec, struct dmxfilter *df)
{
	enum pin_type pin_type;
	int pes_buf_port;

	if (df->type != DEMUX_PID_PES)
		return -1;

	pin_type = PIN_TYPE_PES;

	LOCK(&sdec->mutex);
	if (df->priv.tpc == NULL) {
		dmx_err(sdec->idx, "ERROR PES filter with no tpc\n");
		goto error;
	}
	if (sdec->is_ecp)
		ts_parse_context_ta_release(sdec->idx, df->priv.tpc);
	pes_buf_port = df->priv.tpc->data_port_num;
	if (ts_parse_context_set(df->priv.tpc, DATA_CONTEXT_TYPE_FUNC, -1,
			NULL, NULL) < 0){
		dmx_err(sdec->idx, "ERROR PES filter tpc unset fail\n");
		goto error;
	}

	if (rdvb_dmxbuf_release(pin_type, pes_buf_port, HOST_TYPE_SDEC,
			sdec->idx)<0) {
		dmx_err(sdec->idx, "ERROR release pes buffer fail\n");
		goto error;
	}
	UNLOCK(&sdec->mutex);
	return 0;

error:
	UNLOCK(&sdec->mutex);
	dmx_err(sdec->idx, "ERROR \n");
	return -1;
}
int dmx_filter_setPcr(SDEC_CHANNEL_T* sdec, struct dmxfilter *df)
{
	LOCK(&sdec->mutex);

	if (df->priv.tpc == NULL)
		df->priv.tpc = get_ts_parse_context(&sdec->tpc_list_head, df->pid);
	if (df->priv.tpc == NULL) {
		dmx_err(sdec->idx, "ERROR \n");
		goto error;
	}
	if (ts_parse_context_setPcr(df->priv.tpc) < 0 ){
		dmx_err(sdec->idx, "ERROR \n");
		goto error;
	}
	sdec->PCRPID = df->pid;
	if (sdec->tp_src != MTP) {
		PCRSYNC_Reset(&sdec->pcrProcesser);
		REFCLK_Reset(sdec->refClock.prefclk, sdec->avSyncMode, sdec->v_fr_thrhd);
		SDEC_ResetAVSync(sdec, NAV_AVSYNC_PCR_MASTER);

		if (SDEC_EnablePCRTracking(sdec) < 0) {
			ts_parse_context_removePcr (df->priv.tpc);
			goto error;
		}
	}
	UNLOCK(&sdec->mutex);

	ts_parse_context_dump(&sdec->tpc_list_head);
	return 0;
error:
	UNLOCK(&sdec->mutex);
	dmx_err(sdec->idx, "ERROR \n");
	return -1;
}
int dmx_filter_removePcr(SDEC_CHANNEL_T* sdec, struct dmxfilter *df)
{
	LOCK(&sdec->mutex);
	sdec->PCRPID = -1;
	if (SDEC_DisablePCRTracking(sdec) < 0) {
		dmx_err(sdec->idx, "ERROR \n");
		goto error;
	}
	if (sdec->tp_src != MTP)
		SDEC_ResetAVSync(sdec, NAV_AVSYNC_FREE_RUN);
	if (df->priv.tpc == NULL)
		df->priv.tpc = get_ts_parse_context(&sdec->tpc_list_head, df->pid);
	if (df->priv.tpc == NULL) {
		dmx_err(sdec->idx, "ERROR \n");
		goto error;
	}
	if (ts_parse_context_removePcr(df->priv.tpc) < 0 ){
		dmx_err(sdec->idx, "ERROR \n");
		goto error;
	}
	UNLOCK(&sdec->mutex);
	ts_parse_context_dump(&sdec->tpc_list_head);
	return 0;

error:
	UNLOCK(&sdec->mutex);
	dmx_err(sdec->idx, "ERROR \n");
	return -1;

}

int dmx_filter_enable_scramcheck(SDEC_CHANNEL_T* sdec, struct dmxfilter *df)
{
	LOCK(&sdec->mutex);
	if (df->priv.tpc == NULL)
		df->priv.tpc = get_ts_parse_context(&sdec->tpc_list_head, df->pid);
	if (df->priv.tpc == NULL) {
		dmx_err(sdec->idx, "ERROR \n");
		goto error;
	}
	if (ts_parse_context_enable_scramcheck(df->priv.tpc) < 0 ){
		dmx_err(sdec->idx, "ERROR \n");
		goto error;
	}

	UNLOCK(&sdec->mutex);
	return 0;
error:
	UNLOCK(&sdec->mutex);
	dmx_err(sdec->idx, "ERROR \n");
	return -1;

}

int dmx_filter_disable_scramcheck(SDEC_CHANNEL_T* sdec, struct dmxfilter *df)
{
	LOCK(&sdec->mutex);
	if (df->priv.tpc == NULL)
		df->priv.tpc = get_ts_parse_context(&sdec->tpc_list_head, df->pid);
	if (df->priv.tpc == NULL) {
		dmx_err(sdec->idx, "ERROR \n");
		goto error;
	}
	if (ts_parse_context_disable_scramcheck(df->priv.tpc) < 0 ){
		dmx_err(sdec->idx, "ERROR \n");
		goto error;
	}
	UNLOCK(&sdec->mutex);
	return 0;
error:
	UNLOCK(&sdec->mutex);
	dmx_err(sdec->idx, "ERROR \n");
	return -1;

}
int dmx_filter_get_scramstatus(SDEC_CHANNEL_T* sdec, struct dmxfilter *df,
				int *status)
{
	LOCK(&sdec->mutex);
	if (df->priv.tpc == NULL)
		df->priv.tpc = get_ts_parse_context(&sdec->tpc_list_head, df->pid);
	if (df->priv.tpc == NULL) {
		dmx_err(sdec->idx, "ERROR \n");
		goto error;
	}
	if (ts_parse_context_get_scramstatus(df->priv.tpc, status) < 0) {
		dmx_err(sdec->idx, "ERROR \n");
		goto error;
	}
	UNLOCK(&sdec->mutex);
	return 0;
error:
	UNLOCK(&sdec->mutex);
	dmx_err(sdec->idx, "ERROR \n");
	return -1;
}

int dmx_filter_temi_enable(SDEC_CHANNEL_T* sdec, struct dmxfilter *df,
						u8 timeline_id, ts_callback tc)
{

	LOCK(&sdec->mutex);
	if (df->priv.tpc == NULL)
		df->priv.tpc = get_ts_parse_context(&sdec->tpc_list_head, df->pid);
	if (df->priv.tpc == NULL) {
		dmx_err(sdec->idx, "ERROR \n");
		goto error;
	}
	if (ts_parse_context_temi_enable(df->priv.tpc, timeline_id, tc, df) < 0 ) {
		dmx_err(sdec->idx, "ERROR \n");
		goto error;
	}

	UNLOCK(&sdec->mutex);
	return 0;
error:
	UNLOCK(&sdec->mutex);
	dmx_err(sdec->idx, "ERROR \n");
	return -1;
}

int dmx_filter_temi_disable(SDEC_CHANNEL_T* sdec, struct dmxfilter *df)
{
	LOCK(&sdec->mutex);
	if (df->priv.tpc == NULL)
		df->priv.tpc = get_ts_parse_context(&sdec->tpc_list_head, df->pid);
	if (df->priv.tpc == NULL) {
		dmx_err(sdec->idx, "ERROR \n");
		goto error;
	}
	if (ts_parse_context_temi_disable(df->priv.tpc) < 0) {
		dmx_err(sdec->idx, "ERROR \n");
		goto error;
	}
	UNLOCK(&sdec->mutex);
	return 0;
error:
	UNLOCK(&sdec->mutex);
	dmx_err(sdec->idx, "ERROR \n");
	return -1;

}