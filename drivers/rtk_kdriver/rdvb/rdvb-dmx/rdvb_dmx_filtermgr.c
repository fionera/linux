#include <linux/kref.h>
#include <linux/errno.h>
#include <linux/vmalloc.h>
#include "rdvb_dmx_filtermgr.h"
#include <tp/tp_def.h>
#include "tp/tp_drv_api.h"
#include "dmx_filter.h"

#define err_log(fmt, ...) \
	dmx_err(rdvb_dmx->index, fmt, ##__VA_ARGS__)
#define filter_log(fmt, ...) \
	dmx_mask_print(DEMUX_FILTER_DEBUG, rdvb_dmx->index, fmt, ##__VA_ARGS__)

extern struct rdvb_dmxdev* get_rdvb_dmxdev(void);

static void audiofilter_release(struct kref *kref)
{
	af_ref_t * af_ref = container_of(kref, af_ref_t, kref);
	struct dmxfilter * df = container_of(af_ref, struct dmxfilter, af_ref);

	struct rdvb_dmx  *rdvb_dmx = (struct rdvb_dmx  *)df->priv.priv;
	SDEC_CHANNEL_T * sdec = (SDEC_CHANNEL_T*) rdvb_dmx->dmxdev.demux;

	dmx_info(NOCH, "++%s_%d\n", __func__, __LINE__);
	af_ref->is_enable = false;

	dmx_filter_removeAV(sdec, df);
	df->type = DEMUX_PID_TYPE_NUM;
	df->dest = DEMUX_DEST_NONE;
	dmx_info(NOCH, "--%s_%d\n", __func__, __LINE__);
}

static void audiofilter_get(af_ref_t * af_ref)
{
	dmx_info(NOCH, "%s_%d: :is_enable(%d) \n", __func__, __LINE__, af_ref->is_enable);
	if (af_ref->is_enable){
		kref_get(&af_ref->kref);
	} else {
		af_ref->is_enable = true;
		kref_init(&af_ref->kref);
	}
}

static void audiofilter_put(af_ref_t * af_ref)
{
	dmx_info(NOCH, "%s_%d\n", __func__, __LINE__);
	kref_put(&af_ref->kref, audiofilter_release);
}

static void dmxfilter_release(struct kref * kref)
{
	struct dmxfilter *df = container_of(kref, struct dmxfilter, priv.kref);
	struct rdvb_dmx  *rdvb_dmx = (struct rdvb_dmx  *)df->priv.priv;
	SDEC_CHANNEL_T * sdec = (SDEC_CHANNEL_T*) rdvb_dmx->dmxdev.demux;

	if (RHAL_RemovePIDFilter(rdvb_dmx->index, df->pid, (void*)0x00110011) <0){
		err_log("dmx: %s_%d TP remove filter fail\n",
			__func__, __LINE__);
	}

	//vfree(df);
	dmx_filter_release(sdec, df);

	//dynamic release
	list_del(&df->priv.filter_list);
	spin_lock_irq(&rdvb_dmx->lock);
	df->priv.state_initialized = 0;
	spin_unlock_irq(&rdvb_dmx->lock);
	filter_log("dmx(%d):filter(%p)  releaseed\n", __LINE__, df);
}

static void dmxfilter_get(struct dmxfilter *df)
{
	if (df) {
		kref_get(&df->priv.kref);
	}

}

static void dmxfilter_put(struct dmxfilter *df)
{
	if (df) {
		kref_put(&df->priv.kref, dmxfilter_release);
	}
}

static int dmxfilter_audio_pause_on(struct dmxfilter *df)
{
	dmx_info(NOCH, "%s_%d\n", __func__, __LINE__);
	if (df->is_ap) {
		dmx_info(NOCH, "%s_%d: audio pause alrady on\n", __func__, __LINE__);
		return 0;
	}
	dmxfilter_get(df);
	audiofilter_get(&df->af_ref);
	df->is_ap = true;
	return 0;
}
static int dmxfilter_audio_pause_off(struct dmxfilter *df)
{
	dmx_info(NOCH, "%s_%d\n", __func__, __LINE__);
	if (df->is_ap == 0) {
		dmx_info(NOCH, "%s_%d: audio pause alrady off\n", __func__, __LINE__);
		return 0;
	}
	df->is_ap = false;
	audiofilter_put(&df->af_ref);
	dmxfilter_put(df);
	return 0;
}

static void dmxfilter_init_internal(struct dmxfilter *df)
{
	struct rdvb_dmx *rdvb_dmx = NULL;;
	if (!df) {
		printk(KERN_ERR "dmx: %s_%d: invalid param!!\n", __func__, __LINE__);
		return ;
	}
	rdvb_dmx = (struct rdvb_dmx*)df->priv.priv;
	INIT_LIST_HEAD(&df->priv.filter_list);
	kref_init(&df->priv.kref);
	df->pid = 0x1FFF;
	df->type = DEMUX_PID_TYPE_NUM;
	df->dest = DEMUX_DEST_NONE;

	df->feed.ts = NULL;
	df->cb.ts = NULL;
	df->is_pcr = false;
	df->is_descrmb_en = false;
	df->is_scrambleCheck = false;
	df->bypass.is_enable = false;
	df->psi.is_enable = false;
	df->is_temi = false;
	df->is_ap = false;
	df->af_ref.is_enable = false;

	spin_lock_irq(&rdvb_dmx->lock);
	df->priv.state_initialized = true;
	spin_unlock_irq(&rdvb_dmx->lock);
	df->priv.tpc = NULL;
	INIT_LIST_HEAD(&df->section_list);
	mutex_init(&df->priv.mutex);
}

static struct dmxfilter *
dmxfilter_alloc(struct rdvb_dmx * rdvb_dmx, unsigned short pid)
{
	struct dmxfilter *df = NULL;
	int i = 0;
	list_for_each_entry(df, &rdvb_dmx->filter_list, priv.filter_list){
		if (df->pid == pid){
			break;
		}
	}
	if (&df->priv.filter_list == &rdvb_dmx->filter_list) {
		TPK_PID_FILTER_PARAM_T pidEntry = {
			.valid=1,
			.PID = pid,
			.DescrambleEn = 0,
			.KeySel = rdvb_dmx->index
			};
		
		spin_lock_irq(&rdvb_dmx->lock);
		for(i=0; i<rdvb_dmx->filter_num; i++){
			if (rdvb_dmx->dmx_filters[i].priv.state_initialized == 0){
				df = &rdvb_dmx->dmx_filters[i];
				break;
			}
		}
		spin_unlock_irq(&rdvb_dmx->lock);
		if (i == 32) {
			err_log("ERROR: alloc filter fail , no filter avail\n");
			return NULL;
		}
		dmxfilter_init_internal(df);
		list_add_tail(&df->priv.filter_list, &rdvb_dmx->filter_list);
		df->pid = pid;
		df->type = DEMUX_PID_BYPASS;
		df->dest = DEMUX_DEST_NONE;
		if (RHAL_AddPIDFilter(rdvb_dmx->index, pidEntry, (void*)0x00110011) < 0) {
			dmxfilter_put(df);
			err_log("dmx: %s_%d TP fail to add pid filter (0x%x)\n",
				__func__, __LINE__, pid);
			return NULL;
		}

		filter_log("dmx(%d):filter(%p)  created\n", __LINE__, df);
	} else {
		dmxfilter_get(df);
	}
	return df;
}

static void dmxfilter_mulit_filter_set(multi_func_t *mf)
{
	if (mf->is_enable){
		kref_get(&mf->kref);
	} else {
		mf->is_enable = true;
		kref_init(&mf->kref);
	}
}

static void dmxfilter_mulit_filter_release(struct kref *kref)
{
	multi_func_t *mf = container_of(kref, multi_func_t, kref);
	mf->is_enable = false;
}

static void dmxfilter_mulit_filter_remove(multi_func_t *mf)
{
	kref_put(&mf->kref, dmxfilter_mulit_filter_release);
}

static struct dmxsec * dmxfilter_section_alloc(struct dmxfilter *df)
{
	unsigned int i;
	struct dmxsec * ds = NULL;
	struct rdvb_dmx *rdvb_dmx = (struct rdvb_dmx*)df->priv.priv;
	struct rdvb_dmxdev * rdvb_dmxdev = get_rdvb_dmxdev();
	//ds = vmalloc(sizeof(struct dmxsec));
	
	spin_lock_irq(&rdvb_dmxdev->lock);
	for(i=0; i<rdvb_dmxdev->sdecnum; i++){
		if (rdvb_dmxdev->dmxsecs[i].priv.state_initialized == false){
			ds = &rdvb_dmxdev->dmxsecs[i];
			break;
		}
	}
	if (ds == NULL || i == rdvb_dmxdev->sdecnum) {
		err_log("fail to alloc section fitler ds:%p, i:%d\n", ds, i);
		spin_unlock_irq(&rdvb_dmxdev->lock);
		return NULL;
	}

	list_add_tail(&ds->next, &df->section_list);
	ds->parent = df;
	ds->cb_error = 0;
	ds->priv.state_initialized = true;
	spin_unlock_irq(&rdvb_dmxdev->lock);
	return ds;

}

static int dmxfilter_section_del(struct dmxsec * ds)
{
	struct rdvb_dmxdev * rdvb_dmxdev = get_rdvb_dmxdev();
	list_del(&ds->next);
	//vfree(ds);
	spin_lock_irq(&rdvb_dmxdev->lock);
	ds->priv.state_initialized = false;
	spin_unlock_irq(&rdvb_dmxdev->lock);
	return 0;
}

static char * dmxfilter_dumpfilter(struct dmxfilter *df, char *buf)
{
	struct dmxsec *ds= NULL;
	char * pbuf = buf;
	if (!df){
		printk(KERN_ERR "dmx: %s_%d: invalid param!!\n",
			__func__, __LINE__);
		return pbuf;
	}
	pbuf = print_to_buf(pbuf, "\tfilter(%p): pid[0x%04x] type[0x%02x] dest[0x%02x]"
		" [pcr:%d_scrm:%d_bp:%d_dc:%d_temi:%d_si:%d]\n",
		df, df->pid, df->type, df->dest,
		df->is_pcr, df->is_scrambleCheck, df->bypass.is_enable,
		df->is_descrmb_en, df->is_temi, df->psi.is_enable);
	if (list_empty(&df->section_list))
		return pbuf;
	list_for_each_entry(ds, &df->section_list, next) {
		pbuf = print_to_buf(pbuf,"\t\tsection(%p):[%*ph]:[%d]\n",
			ds, 4, ds->dsf->filter_value, (ds->cb_error == 0));
	}
	return pbuf;
}

//==============================================================================

static int rdvbdmx_ts_callback(const u8 *buffer1, size_t buffer1_len,
				    const u8 *buffer2, size_t buffer2_len,
				    void *source)
{
	int ret = -1;
	struct dmxfilter *df = (struct dmxfilter *)source;
	struct rdvb_dmx* rdvb_dmx = NULL;
	if (df == NULL) {
		dmx_err(NOCH, "ts filter call back fail!\n");
		return -1;
	}
	rdvb_dmx = (struct rdvb_dmx*)df->priv.priv;
	spin_lock_irq(&rdvb_dmx->lock);
	if (df->priv.state_initialized == false) goto END;
	if (df->cb.ts == NULL) 	goto END;
	ret = df->cb.ts(buffer1, buffer1_len, buffer2, buffer2_len, df->feed.ts);
	if (ret < 0) goto END;
END:
	spin_unlock_irq(&rdvb_dmx->lock);
	dmx_mask_print(DEMUX_FILTER_CB, rdvb_dmx->index,
			"ts filter callback(%p), buff1:%d, buff2:%d ret:%d\n",
			df, buffer1_len, buffer2_len, ret);
	ret = 0;
	return ret;
}

static int rdvbdmx_temi_callback(const u8 *buffer1, size_t buffer1_len,
				    const u8 *buffer2, size_t buffer2_len,
				    void *source)
{
	int ret = -1;
	struct dmxfilter *df = (struct dmxfilter *)source;
	struct rdvb_dmx* rdvb_dmx = NULL;
	if (df == NULL) {
		dmx_err(NOCH, "temi filter call back fail !!!\n");
		return -1;
	}
	rdvb_dmx = (struct rdvb_dmx*)df->priv.priv;

	spin_lock_irq(&rdvb_dmx->lock);
	if (df->priv.state_initialized == false) goto END;
	if (df->cb.temi == NULL || df->is_temi == false) goto END;
	ret = df->cb.temi(buffer1, buffer1_len, buffer2, buffer2_len, df->feed.temi);
	if (ret < 0) goto END;
END:
	spin_unlock_irq(&rdvb_dmx->lock);

	dmx_mask_print(DEMUX_FILTER_CB, rdvb_dmx->index,
		"temi filter callback(%p),ret:%d , buff1:%d, buff2:%d PTS:[%*ph] data[%*ph]\n",
		df, ret, buffer1_len, buffer2_len, 8, buffer1, 17, buffer2);

	ret = 0;
	return ret;
}

struct dmxfilter *
rdvbdmx_filter_bypass_add (struct rdvb_dmx* rdvb_dmx, unsigned short pid)
{
	struct dmxfilter *df = NULL;
	
	if (pid >= 0x1FFF) {
		err_log("ERROR: PID=0X1FFF\n");
		return NULL;
	}

	LOCK(&rdvb_dmx->mutex);
	df = dmxfilter_alloc(rdvb_dmx, pid);
	if (df == NULL) {
		err_log("ERROR:alloc filter fail (pid:0x%x)\n", pid);
		goto out;
	}
	dmxfilter_mulit_filter_set(&df->bypass);
out:
	UNLOCK(&rdvb_dmx->mutex);
	return df;
}


int rdvbdmx_filter_bypass_del(struct dmxfilter * df)
{
	SDEC_CHANNEL_T * sdec = NULL;
	struct rdvb_dmx* rdvb_dmx = NULL;
	int ret = 0;
	if (df  == NULL){
		dmx_err(NOCH, "dmx ERROR: filters invalid\n");
		return -ENOMEM;
	}
	rdvb_dmx = (struct rdvb_dmx*)df->priv.priv;
	sdec =(SDEC_CHANNEL_T*) rdvb_dmx->dmxdev.demux;
	filter_log("filter(%p): pid:0x%04x, type:%d, dest:%d\n",
		df, df->pid, df->type, df->dest);
	LOCK(&rdvb_dmx->mutex);
	LOCK(&df->priv.mutex);
	if (df->bypass.is_enable){
		dmxfilter_mulit_filter_remove(&df->bypass);
		dmxfilter_put(df);
	} else {
		err_log("CURRENT filters is not bybass filter \n");
		dmxfilter_dumpfilter(df, NULL);
		ret = -1;
	}
	UNLOCK(&df->priv.mutex);
	UNLOCK(&rdvb_dmx->mutex);
	return ret;
}

struct dmxfilter *
rdvbdmx_filter_add (struct rdvb_dmx* rdvb_dmx, struct dvb_demux_feed *feed)
{
	struct dmxfilter *df = NULL;
	SDEC_CHANNEL_T * sdec = (SDEC_CHANNEL_T*) rdvb_dmx->dmxdev.demux;

	if (feed->	pid >= 0x1FFF) {
		err_log("ERROR: PID=0X1FFF\n");
		return NULL;
	}
	//1. get dmxfilter
	filter_log("filter: pid:0x%04x, feed_type:%d, pes_type:%d\n",
				feed->pid, feed->type, feed->pes_type);

	LOCK(&rdvb_dmx->mutex);
	df = dmxfilter_alloc(rdvb_dmx, feed->pid);
	if (df == NULL) {
		err_log("ERROR:alloc filter fail (pid:0x%x)\n", feed->pid);
		goto out;
	}
	if (feed->type == DMX_TYPE_TEMI) {
		dmxfilter_mulit_filter_set(&df->bypass);
		goto out;
	}
	if (feed->type == DMX_TYPE_SEC) {
		dmxfilter_mulit_filter_set(&df->psi);
		goto out;
	}
	switch (feed->pes_type) {
	case DMX_PES_AUDIO0:
	case DMX_PES_AUDIO1:
		audiofilter_get(&df->af_ref);
		if (df->is_ap)
			break;
		df->type = DEMUX_PID_AUDIO;
		df->dest = feed->pes_type == DMX_PES_AUDIO0?
							DEMUX_DEST_ADEC0:DEMUX_DEST_ADEC1;
		if (dmx_filter_setpAV(sdec, df) < 0){
			audiofilter_put(&df->af_ref);
			dmxfilter_put(df);
			df = NULL;
			goto out;
		}
		if (rdvb_dmx->is_audio_paused)
			dmxfilter_audio_pause_on(df);
		break;
	case DMX_PES_VIDEO0:
	case DMX_PES_VIDEO1:
		df->type = DEMUX_PID_VIDEO;
		df->dest = feed->pes_type == DMX_PES_VIDEO0?
							DEMUX_DEST_VDEC0:DEMUX_DEST_VDEC1;
		if (dmx_filter_setpAV(sdec, df) < 0){
			dmxfilter_put(df);
			df = NULL;
			goto out;
		}
		break;
	case DMX_PES_TELETEXT0:
	case DMX_PES_TELETEXT1:
	case DMX_PES_SUBTITLE0:
	case DMX_PES_SUBTITLE1:
		df->type = DEMUX_PID_PES;
		df->dest = DEMUX_DEST_BUFFER;
		if ((feed->ts_type & TS_PAYLOAD_ONLY) != TS_PAYLOAD_ONLY)
			break;
		df->feed.ts = &feed->feed.ts;
		df->cb.ts =  feed->cb.ts;
		if (dmx_filter_setPES(sdec, df, rdvbdmx_ts_callback) < 0){
			dmxfilter_put(df);
			df->feed.ts = NULL;
			df->cb.ts = NULL;
			df = NULL;
			goto out;
		}
		break;
	case DMX_PES_PCR0:
	case DMX_PES_PCR1:
		df->is_pcr = true;
		if (dmx_filter_setPcr(sdec, df) <0){
			dmxfilter_put(df);
			df = NULL;
			goto out;
		}
		break;
	case DMX_PES_OTHER:
		dmxfilter_mulit_filter_set(&df->bypass);
		break;
	default:
		err_log("dmx: ERROR: unKnow type (%d)\n", feed->pes_type);
		dmxfilter_put(df);
		df = NULL;
		goto out;
		break;
	}

out:
	UNLOCK(&rdvb_dmx->mutex);

	if (df && df->type != DEMUX_PID_BYPASS && df->psi.is_enable) {
		err_log("filter(%p)si filter already exist\n", df);
		dmxfilter_dumpfilter(df, NULL);
	}
	return df;
}

int rdvbdmx_filter_del(struct dmxfilter * df, struct dvb_demux_feed *feed)
{
	SDEC_CHANNEL_T * sdec = NULL;
	struct rdvb_dmx* rdvb_dmx = NULL;
	int ret = 0;
	if (df  == NULL){
		dmx_err(NOCH, "dmx ERROR: filters invalid\n");
		return -ENOMEM;
	}
	rdvb_dmx = (struct rdvb_dmx*)df->priv.priv;
	sdec =(SDEC_CHANNEL_T*) rdvb_dmx->dmxdev.demux;
	filter_log("filter(%p): pid:0x%04x, type:%d, dest:%d\n",
					df, df->pid, df->type, df->dest);
	LOCK(&rdvb_dmx->mutex);
	LOCK(&df->priv.mutex);
	if (feed->type == DMX_TYPE_TEMI) {
		if (df->bypass.is_enable){
			dmxfilter_mulit_filter_remove(&df->bypass);
			dmxfilter_put(df);
		} else {
			err_log("ERROR: CURRENT filter is not bybass filter \n");
			dmxfilter_dumpfilter(df, NULL);
			ret = -1;
		}
		goto out;
	}
	if (feed->type == DMX_TYPE_SEC) {
		if (df->type != DEMUX_PID_BYPASS) {
			err_log("filter(%p), abnoam\n", df);
			dmxfilter_dumpfilter(df, NULL);
		}
		if (df->psi.is_enable) {
			dmxfilter_mulit_filter_remove(&df->psi);
			dmxfilter_put(df);
		} else {
			err_log(" filters is not match (pid:0x%x, type:%d, dest:%d)\n",
				df->pid, df->type, df->dest);
			dmxfilter_dumpfilter(df, NULL);
			ret = -1;
		}
		goto out;
	}

	switch (feed->pes_type) {
	case DMX_PES_AUDIO0:
	case DMX_PES_AUDIO1:
		audiofilter_put(&df->af_ref);
		dmxfilter_put(df);
		break;
	case DMX_PES_VIDEO0:
	case DMX_PES_VIDEO1:
		if (df->type != DEMUX_PID_VIDEO || 
			df->dest != (feed->pes_type==DMX_PES_VIDEO0?DEMUX_DEST_VDEC0:DEMUX_DEST_VDEC1)) {
			err_log("filters is not match (pid:0x%x, type:%d, dest:%d)\n",
				df->pid, df->type, df->dest);
			dmxfilter_dumpfilter(df, NULL);
			ret = -1;
			break;
		}
		dmx_filter_removeAV(sdec, df);
		df->type = DEMUX_PID_BYPASS;
		df->dest = DEMUX_DEST_NONE;
		dmxfilter_put(df);
		break;
	case DMX_PES_TELETEXT0:
	case DMX_PES_TELETEXT1:
	case DMX_PES_SUBTITLE0:
	case DMX_PES_SUBTITLE1:
		if (df->type != DEMUX_PID_PES) {
			err_log("ERROR:filters is not match (pid:0x%x, type:%d, dest:%d)\n",
				df->pid, df->type, df->dest);
			dmxfilter_dumpfilter(df, NULL);
			ret = -1;
			break;
		}
		dmx_filter_removePES(sdec, df);
		df->type = DEMUX_PID_BYPASS;
		df->dest = DEMUX_DEST_NONE;
		df->feed.ts = NULL;
		df->cb.ts =  NULL;
		dmxfilter_put(df);
		break;
	case DMX_PES_PCR0:
	case DMX_PES_PCR1:
		if (df->is_pcr){
			dmx_filter_removePcr(sdec, df);
			df->is_pcr = false;
			dmxfilter_put(df);
		} else {
			err_log("ERROR:CURRENT filters is not PCR \n");
			dmxfilter_dumpfilter(df, NULL);
		}
		break;
	case DMX_PES_OTHER:
		if (df->bypass.is_enable){
			dmxfilter_mulit_filter_remove(&df->bypass);
			dmxfilter_put(df);
		} else {
			err_log("ERROR:CURRENT filters is not bybass filter \n");
			dmxfilter_dumpfilter(df, NULL);
			ret = -1;
		}
		break;
	default:
		err_log("ERROR:filters is not match (pid:0x%x, type:%d, dest:%d)\n",
			df->pid, df->type, df->dest);
		dmxfilter_dumpfilter(df, NULL);
		ret = -1;
		break;
	}
out:
	UNLOCK(&df->priv.mutex);
	UNLOCK(&rdvb_dmx->mutex);
	return ret;
}

int rdvbdmx_filter_start_scrambcheck(struct dmxfilter *df)
{
	SDEC_CHANNEL_T * sdec = NULL;
	struct rdvb_dmx* rdvb_dmx = NULL;
	if (df == NULL) {
		dmx_err(NOCH, "filter: ERROR \n");
		return -1;
	}
	rdvb_dmx = (struct rdvb_dmx*)df->priv.priv;
	sdec =(SDEC_CHANNEL_T*) rdvb_dmx->dmxdev.demux;

	if (sdec == NULL) {
		err_log("ERROR: sdec NULL \n");
		return -1;
	}
	printk("dmx:%d, pid:0x%x, start scrambcheck\n", sdec->idx, df->pid);

	LOCK(&df->priv.mutex);
	df->is_scrambleCheck = true;
	if (dmx_filter_enable_scramcheck(sdec, df) < 0){
		UNLOCK(&df->priv.mutex);
		err_log("ERROR: fail to enable scramble check\n");
		return -1;
	}
	UNLOCK(&df->priv.mutex);
	return 0;
}

int rdvbdmx_filter_stop_scrambcheck(struct dmxfilter *df)
{
	SDEC_CHANNEL_T * sdec = NULL;
	struct rdvb_dmx* rdvb_dmx = NULL;
	if (df == NULL) {
		dmx_err(NOCH, "filter: ERROR \n");
		return -1;
	}
	rdvb_dmx = (struct rdvb_dmx*)df->priv.priv;

	sdec =(SDEC_CHANNEL_T*) rdvb_dmx->dmxdev.demux;

	err_log("pid:0x%x, stop scrambcheck\n",  df->pid);
	LOCK(&df->priv.mutex);
	if (df->is_scrambleCheck){
		dmx_filter_disable_scramcheck(sdec, df);
		df->is_scrambleCheck = false;
	} else {
		err_log("ERROR: CURRENT filters is not check scramble \n");
		dmxfilter_dumpfilter(df, NULL);
	}
	UNLOCK(&df->priv.mutex);
	return 0;
}

int rdvbdmx_filter_get_scramstatus(struct dmxfilter * df, int * scramble_status)
{
	SDEC_CHANNEL_T * sdec = NULL;
	struct rdvb_dmx* rdvb_dmx = NULL;
	if (df == NULL) {
		dmx_err(NOCH, "filter: ERROR \n");
		return -1;
	}
	rdvb_dmx = (struct rdvb_dmx*)df->priv.priv;

	sdec =(SDEC_CHANNEL_T*) rdvb_dmx->dmxdev.demux;
	LOCK(&df->priv.mutex);
	if (!df->is_scrambleCheck) {
		err_log("dmx: ERROR: df scramble not enabled \n");
		UNLOCK(&df->priv.mutex);
		return -1;
	}
	err_log("pid:0x%x, scrambcheck\n",df->pid);
	if (dmx_filter_get_scramstatus(sdec, df, scramble_status) < 0) {
		UNLOCK(&df->priv.mutex);
		err_log("dmx: ERROR: fail to get scramble status \n");
		return -1;
	}
	UNLOCK(&df->priv.mutex);
	return 0;
}

int rdvbdmx_filter_descrmb_ctrl(struct dmxfilter *df, u8 is_enable)
{
	SDEC_CHANNEL_T * sdec = NULL;
	TPK_PID_FILTER_PARAM_T pidEntry;
	struct rdvb_dmx* rdvb_dmx = NULL;
	if (df == NULL) {
		dmx_err(NOCH, "filter: ERROR \n");
		return -1;
	}
	rdvb_dmx = (struct rdvb_dmx*)df->priv.priv;
	sdec =(SDEC_CHANNEL_T*) rdvb_dmx->dmxdev.demux; 

	err_log("pid:0x%x, descramble enable :%d\n",df->pid, is_enable);
	LOCK(&df->priv.mutex);
	if (is_enable == df->is_descrmb_en) {
		UNLOCK(&df->priv.mutex);
		return 0;
	}

	pidEntry.valid = 1;
	pidEntry.PID = df->pid;
	pidEntry.DescrambleEn = is_enable;
	pidEntry.KeySel = rdvb_dmx->index;
	if (RHAL_AddPIDFilter(rdvb_dmx->index, pidEntry, (void*)0x00110011) < 0) {
		err_log("dmx: TP fail to add pid filter (0x%x)\n", df->pid);
		UNLOCK(&df->priv.mutex);
		return -1;
	}
	df->is_descrmb_en =  is_enable;
	UNLOCK(&df->priv.mutex);
	return 0;
}

int rdvbdmx_filter_temi_enable(struct dmxfilter *df, struct dvb_demux_feed *feed)
{
	SDEC_CHANNEL_T * sdec = NULL;
	struct rdvb_dmx* rdvb_dmx = NULL;
	if (df == NULL) {
		dmx_err(NOCH, "filter: ERROR \n");
		return -1;
	}
	rdvb_dmx = (struct rdvb_dmx*)df->priv.priv;
	filter_log("pid:0x%x, start temi\n",  df->pid);
	sdec =(SDEC_CHANNEL_T*) rdvb_dmx->dmxdev.demux;
	LOCK(&df->priv.mutex);

	if (dmx_filter_temi_enable(sdec, df, feed->feed.temi.timeline_id,
							rdvbdmx_temi_callback) < 0){
		UNLOCK(&df->priv.mutex);
		err_log("ERROR: fail to enable temi\n");
		return -1;
	}
	spin_lock_irq(&rdvb_dmx->lock);
	df->is_temi = true;
	df->feed.temi = &feed->feed.temi;
	df->cb.temi = feed->cb.temi;
	spin_unlock_irq(&rdvb_dmx->lock);
	UNLOCK(&df->priv.mutex);
	return 0;
}

int rdvbdmx_filter_temi_disable(struct dmxfilter *df)
{
	SDEC_CHANNEL_T * sdec = NULL;
	struct rdvb_dmx* rdvb_dmx = NULL;
	if (df == NULL) {
		dmx_err(NOCH, "filter: ERROR \n");
		return -1;
	}
	rdvb_dmx = (struct rdvb_dmx*)df->priv.priv;

	sdec =(SDEC_CHANNEL_T*) rdvb_dmx->dmxdev.demux;

	filter_log("pid:0x%x, stop temi\n",  df->pid);
	LOCK(&df->priv.mutex);
	if (df->is_temi){
		dmx_filter_temi_disable(sdec, df);
		df->is_temi = false;
		df->feed.temi = NULL;
		df->cb.temi = NULL;
	} else {
		err_log("ERROR: CURRENT filters is not check temi \n");
		dmxfilter_dumpfilter(df, NULL);
	}
	UNLOCK(&df->priv.mutex);
	return 0;

}

/*
* @request
* @	LOCK  rdvb_dmx
*/
void rdvbdmx_filter_audio_pause_on(struct list_head *filters)
{
	struct dmxfilter *df = NULL;
	dmx_info(NOCH, "%s_%d\n", __func__, __LINE__);
	list_for_each_entry(df, filters, priv.filter_list){
		if (df->type != DEMUX_PID_AUDIO)
			continue;
		dmxfilter_audio_pause_on (df);
	}
}

void rdvbdmx_filter_audio_pause_off(struct list_head *filters)
{
	struct dmxfilter *df = NULL, *df_t= NULL;
	dmx_info(NOCH, "%s_%d\n", __func__, __LINE__);
	list_for_each_entry_safe(df, df_t, filters, priv.filter_list){
		if (df->type != DEMUX_PID_AUDIO)
			continue;
		dmxfilter_audio_pause_off (df);
	}
}

typedef int (*rdvb_section_cb)(const u8 *buffer1,
			      size_t buffer1_len,
			      const u8 *buffer2,
			      size_t buffer2_len,
			      void *source);
static int rdvbdmx_section_callback(const u8 *buffer1, size_t buffer1_len,
				    const u8 *buffer2, size_t buffer2_len,
				    void *source)
{
	int ret = -1;
	bool _1st_err = false;
	struct dmxsec *ds = (struct dmxsec *)source;
	struct dmxfilter * df = NULL;
	struct rdvb_dmx * rdvb_dmx = NULL;
	struct rdvb_dmxdev * rdvb_dmxdev = get_rdvb_dmxdev();
	if (ds == NULL) {
		//dmx_err(NOCH, "section filter call back fail (%d)\n", __LINE__);
		return -1;
	}
	spin_lock_irq(&rdvb_dmxdev->lock);
	if (ds->priv.state_initialized == false) goto END;
	df = ds->parent;
	rdvb_dmx = (struct rdvb_dmx*)df->priv.priv ;
	if (rdvb_dmx== NULL || ds->dsc == NULL) goto END;
	ret = ds->dsc(buffer1, buffer1_len, buffer2, buffer2_len, ds->dsf);
	if (ret < 0) {
		if (ds->cb_error == 0)
			_1st_err = true;
		ds->cb_error = ret;
	}

END:
	spin_unlock_irq(&rdvb_dmxdev->lock);
	if (_1st_err)
		err_log("filter(%p) section(%p) callback fail \n", df, ds);
	dmx_mask_print(DEMUX_SECTION_CB, (rdvb_dmx==NULL?NOCH:rdvb_dmx->index),
			"section filter callback(%p), buff1:%d, buff2:%d ret:%d [%*ph]\n",
			ds, buffer1_len, buffer2_len, ret, 4, buffer1);
	return 0;
}

struct dmxsec * rdvbdmx_filter_section_add(struct dmxfilter *df,
			struct dmx_section_filter * dsf, dmx_section_cb dsc)
{
	int  i = 0;
	struct dmxsec * ds = NULL;
	TPK_SEC_FILTER_PARAM_T psec = {};
	struct rdvb_dmx * rdvb_dmx = (struct rdvb_dmx*)df->priv.priv ;

	LOCK(&df->priv.mutex);
	ds = dmxfilter_section_alloc(df);
	if (ds == NULL) {
		err_log("section filter alloc fail(pid:0x%x))\n", df->pid);
		UNLOCK(&df->priv.mutex);
		return NULL;
	}

	ds->dsf = dsf;
	ds->dsc = dsc;
	psec.PID = df->pid;
	memcpy(&psec.PosVal, &dsf->filter_value, 10);
	memcpy(&psec.NegVal, &dsf->filter_value, 10);
	for (i=0;i<10;i++) {
		psec.PosMsk[i] = dsf->filter_mask[i] & (dsf->filter_mode[i]);
		psec.NegMsk[i] = dsf->filter_mask[i] & (~dsf->filter_mode[i]);
	}

	psec.crc_en = 1;
	psec.one_shoot = dsf->one_shot;
	psec.pContext2 = ds;
	psec.dsc = rdvbdmx_section_callback;
	dmx_mask_print(DEMUX_SECTION_DEBUG, rdvb_dmx->index,
		"filter:%p sec:%p\n\tvalue:%*ph \n\tmaske:%*ph \n\tmode:%*ph\n",
		df, ds,
		DMX_MAX_FILTER_SIZE, dsf->filter_value,
		DMX_MAX_FILTER_SIZE, dsf->filter_mask,
		DMX_MAX_FILTER_SIZE, dsf->filter_mode);
	if (RHAL_AddSectionFilter(rdvb_dmx->index, psec, NULL,
		 &ds->sec_handle) != TPK_SUCCESS)  {

		dmxfilter_section_del(ds);
		dmxfilter_put(df);
		err_log("ERROR:section filter alloc fail(pid:0x%x))\n", df->pid);
		ds = NULL;
	}
	//RHAL_TPStreamControl(0, TP_STREAM_START);
	UNLOCK(&df->priv.mutex);
	return ds;

}

int rdvbdmx_filter_section_del(struct dmxsec * ds)
{
	struct dmxfilter * df = NULL;
	struct rdvb_dmx * rdvb_dmx = NULL;
	df = ds->parent;
	rdvb_dmx = (struct rdvb_dmx*)df->priv.priv ;

	
	dmx_mask_print(DEMUX_SECTION_DEBUG, rdvb_dmx->index,
		"filter:%p sec:%p\n", df, ds);
	if (RHAL_RemoveSectionFilter(rdvb_dmx->index, ds->sec_handle) != TPK_SUCCESS) {
		err_log("dmx: ERROR :RemoveSection  (Pid:0x%x)\n", df->pid);
		return -1;
	}

	dmxfilter_section_del(ds);
	return 0;
}
int rdvbdmx_filter_reset(struct list_head *df)
{
	return 0;
}

char * rdvbdmx_filter_dump(struct list_head *filters, char *buf)
{
	struct dmxfilter *df = NULL;
	char * pbuf = buf;
	list_for_each_entry(df, filters, priv.filter_list){
		pbuf = dmxfilter_dumpfilter(df, pbuf);
	}
	return pbuf;
}

static char * pes_dest_str [] = {
	"NONE",
	"ADEC0",
	"ADEC1",
	"VDEC0",
	"VDEC1",
	"PCR0",
	NULL,
};
inline char * dest2str(DEMUX_PES_DEST_T dest)
{
	if (dest >= DEMUX_DEST_NUM)
		return NULL;
	return pes_dest_str[dest];
}
char *  rdvbdmx_filter_dump_pes(struct list_head *filters, char * buf)
{
	struct dmxfilter *df = NULL;
	struct rdvb_dmx * rdvb_dmx =  container_of(filters, struct rdvb_dmx, filter_list);
	int idx = 0;
	list_for_each_entry(df, filters, priv.filter_list){
		if (df->type <= DEMUX_PID_PCR){
			buf = print_to_buf(buf, "[%3x] CH[%x] PID[0x%4x] [EN] [PES] [%s]\n",
				idx, rdvb_dmx->index, df->pid, dest2str(df->dest));

			idx++;
		}
	}
	return buf;
}

char * rdvbdmx_filter_dump_sec(struct list_head *filters, char * buf)
{
	struct dmxfilter *df = NULL;
	struct dmxsec * ds = NULL;
	struct rdvb_dmx * rdvb_dmx =  container_of(filters, struct rdvb_dmx, filter_list);
	int idx = 0;
	list_for_each_entry(df, filters, priv.filter_list){
		if (list_empty(&df->section_list))
			continue;

		list_for_each_entry(ds, &df->section_list, next) {
			buf = print_to_buf(buf, "[%3x] CH[%x] PID[0x%4x] REG[%*phN]  "
				"[en] [SEC] [GPB] [00]\n",
				idx, rdvb_dmx->index, df->pid, 4, ds->dsf->filter_value);

			idx++;
		}
	}
	return buf;
	
}

char * rdvbdmx_filter_dump_scrmbck(struct list_head *filters, char * buf)
{
	struct dmxfilter *df = NULL;
	struct rdvb_dmx * rdvb_dmx =  container_of(filters, struct rdvb_dmx, filter_list);
	int idx = 0;
	list_for_each_entry(df, filters, priv.filter_list){
		if (df->is_scrambleCheck){
			buf = print_to_buf(buf, "[%4x]  ch[%d] PID[%4x\n",
				idx, rdvb_dmx->index, df->pid);
			idx++;
		}
	}
	return buf;
}
