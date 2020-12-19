#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <dvbdev.h>
#include <dmxdev.h>
#include <dvb_demux.h>
#include <demux.h>
#include "dmx_common.h"
#include "rdvb_dmx_ctrl_internal.h"
#include "rdvb_dmx_filtermgr.h"
#include "rdvb-buf/rdvb_dmx_buf.h"
#include "rdvb-adapter/rdvb_adapter.h"
#include "sdec.h"
#include "debug.h"
#include "descrambler.h"
#include "dmx_service.h"

struct rdvb_dmx* get_rdvb_dmx(unsigned char idx)
{
	struct rdvb_adapter * rdvb_adap= NULL;

	rdvb_adap  = rdvb_get_adapter();
	if (idx >= rdvb_adap->rdvb_dmxdev.dmxnum) {
		dmx_err(CHANNEL_UNKNOWN, " dmx handle(%d) invalid !! \n", idx);
		return NULL;
	}
	return  &rdvb_adap->rdvb_dmxdev.rdvb_dmx[idx];
}
struct rdvb_dmxdev * get_rdvb_dmxdev(void)
{
	struct rdvb_adapter * rdvb_adap= NULL;
	rdvb_adap  = rdvb_get_adapter();
	return &rdvb_adap->rdvb_dmxdev;
}

int rdvb_dmx_startfeed(struct dvb_demux_feed *feed)
{
	struct dvb_demux * dvb_demux = feed->demux;
	struct dmxdev* dmxdev = dvb_demux->priv;
	struct rdvb_dmx * rdvb_dmx =  container_of(dmxdev, struct rdvb_dmx, dmxdev);

	//dmx_notice(rdvb_dmx->index, "FILTER: pid:0x%0x, (%d_%d) type:%d, dest:%d\n",
	//			feed->pid, feed->type, feed->pes_type, type, dest);
	feed->priv = rdvbdmx_filter_add(rdvb_dmx, feed);
	if (!feed->priv){

		dmx_err(rdvb_dmx->index, "FILTER: ERROR: add filter fail!\n");
		return -ENOMEM;
	}

	//section
	if (feed->type == DMX_TYPE_SEC){
		feed->feed.sec.priv = rdvbdmx_filter_section_add(feed->priv,
			(struct dmx_section_filter *)feed->filter, feed->cb.sec);
		if (!feed->feed.sec.priv){
			dmx_err(rdvb_dmx->index, "filter: ERROR:set section fail!!\n");
			return -ENOMEM;
		}
	} else if (feed->type == DMX_TYPE_TEMI){
		dmx_err(rdvb_dmx->index, "filter: enable temi Check pid:0x%x\n", feed->pid);
		rdvbdmx_filter_temi_enable(feed->priv, feed);
	}
	/*dmx_notice(rdvb_dmx_index, "dmx: %s_%d, \n", __func__, __LINE__);*/
	return 0;
}
int rdvb_dmx_stopfeed(struct dvb_demux_feed *feed)
{
	struct dvb_demux * dvb_demux = feed->demux;
	struct dmxdev* dmxdev = dvb_demux->priv;
	struct rdvb_dmx * rdvb_dmx =  container_of(dmxdev, struct rdvb_dmx, dmxdev);
	/*
	dmx_notice(CHANNEL_UNKNOWN, "dmx: %s_%d, type:%d\n",
		 __func__, __LINE__, feed->type);
	*/
	if  (feed->type == DMX_TYPE_TEMI) {
		dmx_err(rdvb_dmx->index, "filter: disable temi Check pid:0x%x\n", feed->pid);
		rdvbdmx_filter_temi_disable(feed->priv);
	} else if (feed->type == DMX_TYPE_SEC) {
		if (rdvbdmx_filter_section_del(feed->feed.sec.priv) < 0){
			dmx_err(rdvb_dmx->index, "dmx:  section remove fail!! \n");
			return -1;
		}
		feed->feed.sec.priv = NULL;
	}
	rdvbdmx_filter_del(feed->priv, feed);
	feed->priv = NULL;
	//rdvbdmx_filter_dump(&rdvb_dmx->filter_list);
	//dmx_notice(CHANNEL_UNKNOWN, "dmx: %s_%d, \n", __func__, __LINE__);
	return 0;
}
 
static int rdvb_dmx_filter_control(struct dvb_demux_feed *feed, __u32 id, void * parg)
{
	int ret = 0;
	switch (id) {
	case DMX_EXT_CID_REQUEST_SCRMB:
		ret = rdvbdmx_filter_start_scrambcheck(feed->priv);
		break;
	case DMX_EXT_CID_CANCEL_SCRMB:
		ret = rdvbdmx_filter_stop_scrambcheck(feed->priv);
		break;
	case DMX_EXT_CID_CHECK_SCRMB:
	{
		int scramble_status = -1;
		ret = rdvbdmx_filter_get_scramstatus(feed->priv, &scramble_status);

		if (scramble_status != 0 && scramble_status != 1)
			scramble_status = -1;
		*(int64_t*)parg = scramble_status;
		break;
	}
	case DMX_EXT_CID_DSCRMB_PID:
	{
		struct dmx_ext_dscrmb_pid *descrmb_con = parg;
		ret  = rdvbdmx_filter_descrmb_ctrl(feed->priv, descrmb_con->bEnable);
		break;
	}
	default:
		dmx_err(CHANNEL_UNKNOWN, "set cmd:%d not support\n", id);
		ret = -1;
		break;
	}
	return ret;
}

static int rdvb_dmx_prop_set(struct dmx_demux *demux, __u32 cmd, void *parg)
{
	struct dvb_demux *dvbdemux = (struct dvb_demux *)demux;
	SDEC_CHANNEL_T *pCh = (SDEC_CHANNEL_T *)dvbdemux;
	struct rdvb_dmx *rdvb_dmx = (struct rdvb_dmx *)dvbdemux->priv;
	switch(cmd) {
	case DMX_EXT_CID_PLATFORM:
		rdvb_dmx->dmx_platform = *(__s64*)parg;
		break;
	case DMX_EXT_CID_COUNTRY:
		rdvb_dmx->dmx_country = *(__s64*)parg;
		break;
	case DMX_EXT_CID_INPUTSOURCE:
	{
		struct dmx_ext_source *src = (struct dmx_ext_source*)parg;

		if (rdvb_dmx->input_src_type  == src->input_src_type &&
			rdvb_dmx->input_port_num  == src->input_port_num &&
			rdvb_dmx->input_port_type == src->input_port_type) 
			return 0;

		if (rdvb_dmx_ctrl_config(pCh, src->input_src_type, 
				src->input_port_num, src->input_port_type) < 0){
			dmx_err(rdvb_dmx->index,"ERROR: dmx control fail!! \n");
			return -1;
		}
		rdvb_dmx->input_src_type  = src->input_src_type;
		rdvb_dmx->input_port_num  = src->input_port_num;
		rdvb_dmx->input_port_type = src->input_port_type;
		break;
	}
	case DMX_EXT_CID_PCR_ONOFF:
		if (rdvb_dmx_ClockRecovery(pCh, *(__s64*)parg) <  0)
			return -1;
		rdvb_dmx->pcr_recov_on = *(__s64*)parg;
		break;
	case DMX_EXT_CID_DSCRMB_TYPE:
		if (rdvb_dmx_descram_type_set(&rdvb_dmx->dscmb, *(__s64*)parg) <0) {
			dmx_err(rdvb_dmx->index,"ERROR : set descrambler type \n" );
			return -1;
		}
		break;
	case DMX_EXT_CID_DSCRMB_KEY:
		if (rdvb_dmx_descram_key_set(&rdvb_dmx->dscmb,
				(struct dmx_ext_dscrmb_key *)parg) < 0) {
			dmx_err(rdvb_dmx->index,"ERROR: set descrambler key \n" );
			return -1;
		}
		break;
	case DMX_EXT_CID_ECP_INFO_NOTI:
		dmx_err(rdvb_dmx->index, "DMX_EXT_CID_ECP_INFO_NOTI is NOT implemented\n");
		break;
	default:
		dmx_err(rdvb_dmx->index, "ERROR: set cmd:%d not support\n", cmd);
		return -1;
	}
	return 0;

}
static int rdvb_dmx_prop_get(struct dmx_demux *demux, __u32 cmd, void *parg)
{
	struct dvb_demux *dvbdemux = (struct dvb_demux *)demux;
	struct rdvb_dmx *rdvb_dmx = (struct rdvb_dmx *)dvbdemux->priv;
	struct dmx_ext_source * des = NULL;

	switch(cmd) {
	case DMX_EXT_CID_PLATFORM:
		*(__s64*)parg = rdvb_dmx->dmx_platform;
		break;
	case DMX_EXT_CID_COUNTRY:
		*(__s64*)parg = rdvb_dmx->dmx_country;
		break;
	case DMX_EXT_CID_MODEL_NO:
		*(__s64*)parg = rdvb_dmx->dmx_model_id;
		break;
	case DMX_EXT_CID_CHIP_ID:
		*(__s64*)parg = rdvb_dmx->dmx_chip_id;
		break;
	case DMX_EXT_CID_DRV_VER:
		*(__s64*)parg = rdvb_dmx->dmx_ver_id;
		break;
	case DMX_EXT_CID_PORT_NUM:
		//not support
		*(__s64*)parg = rdvb_dmx->index;
		break;
	case DMX_EXT_CID_INPUTSOURCE:
		des = (struct dmx_ext_source *)parg;
		des->input_src_type  = rdvb_dmx->input_src_type;
		des->input_port_num  = rdvb_dmx->input_port_num;
		des->input_port_type = rdvb_dmx->input_port_type;
		break;
	case DMX_EXT_CID_PCR_ONOFF:
		*(__s64*)parg = rdvb_dmx->pcr_recov_on;
		break;

	case DMX_EXT_CID_DSCRMB_TYPE:
		rdvb_dmx_descram_type_get(&rdvb_dmx->dscmb, parg);
		break;
	case DMX_EXT_CID_ECP_INFO_NOTI:
		dmx_err(rdvb_dmx->index, "DMX_EXT_CID_ECP_INFO_NOTI is NOT implemented\n");
		break;
	default:
		dmx_err(rdvb_dmx->index, "DO NOT support get : %d \n", cmd);
		return -1;
	}

	return 0;
}

static int rdvb_dmx_get_stc(struct dmx_demux *demux, unsigned int num,
			u64 *stc, unsigned int *base) 
{
	SDEC_CHANNEL_T *pCh = (SDEC_CHANNEL_T *)demux;

	if (rdvb_dmx_ctrl_get_stc(pCh, stc, base) < 0) {
		dmx_err(pCh->idx, "dmx: ERROR get stc fail!!!!!\n");
		return -1;
	}
	return 0;
}

static int rdvb_dmx_pvr_open(struct dmx_demux *demux, dvr_rec_cb dvr_rec_cb)
{
	SDEC_CHANNEL_T *pCh = (SDEC_CHANNEL_T *)demux;
	struct dvb_demux *dvbdemux = (struct dvb_demux *)demux;
	struct rdvb_dmx *rdvb_dmx = (struct rdvb_dmx *)dvbdemux->priv;
	return rdvb_dmx_ctrl_pvr_open(pCh, dvr_rec_cb, &rdvb_dmx->dmxdev);
}
static int rdvb_dmx_pvr_close(struct dmx_demux *demux)
{
	return rdvb_dmx_ctrl_pvr_close(demux);
}
static int rdvb_pvr_prop_set(struct dmx_demux *demux, __u32 cmd, void *parg)
{
	struct dvb_demux *dvbdemux = (struct dvb_demux *)demux;
	SDEC_CHANNEL_T *pCh = (SDEC_CHANNEL_T *)dvbdemux;
	int ret = 0;

	switch(cmd) {
	case PVR_EXT_CID_SETRATE:
	{
		int speed = 0;
		speed = *(__s64*)parg;
		dmx_notice(pCh->idx, "PVR_EXT_CID_SETRATE (%d)\n",speed);
		ret = rdvb_dmx_ctrl_pvr_SetRate(pCh, speed);
		break;
	}
	case PVR_EXT_CID_RESET:
		dmx_notice(pCh->idx, "PVR_EXT_CID_RESET  !! \n");
		ret = rdvb_dmx_ctrl_pvr_reset(pCh);
		break;
	case PVR_EXT_CID_CERT:
		dmx_notice(pCh->idx, "PVR_EXT_CID_CERT  !! \n");
		ret = rdvb_dmx_ctrl_pvr_auth_verify(pCh, (char*)parg);
		break;

	default:
		dmx_err(pCh->idx, "set cmd:%d not support\n", cmd);
		ret = -1;
	}
	return ret;
}

static int rdvb_dmx_write(struct dmx_demux *demux, const char __user *buf, size_t count)
{

	SDEC_CHANNEL_T *pCh = (SDEC_CHANNEL_T *)demux;
	return rdvb_dmx_ctrl_pvr_write(pCh, buf, count);
}

extern const struct file_operations status_ops;
static struct proc_dir_entry * dvbdmx_proc = NULL;
static void dmx_proc_reg(void)
{
	dvbdmx_proc = rdvb_adap_register_proc("lgtv-driver/dvb_demux");
	if (dvbdmx_proc)
		proc_create("status", 0444, dvbdmx_proc, &status_ops);
}

static void dmx_proc_unreg(void)
{
	if (dvbdmx_proc) {
		proc_remove(dvbdmx_proc);
		dvbdmx_proc = NULL;
	}
}

extern const struct file_operations dvr_status_ops;
extern const struct file_operations dvr_esType_ops;
extern const struct file_operations dvr_bwCapa_ops;
static struct proc_dir_entry * dvbdvr_proc = NULL;
static void dvr_proc_reg(void)
{
	dvbdvr_proc = rdvb_adap_register_proc("lgtv-driver/dvb_dvr");
	if (dvbdvr_proc) {
		proc_create("status", 0444, dvbdvr_proc, &dvr_status_ops);
		proc_create("prop_es_type", 0444, dvbdvr_proc, &dvr_esType_ops);
		proc_create("prop_bw_capa", 0444, dvbdvr_proc, &dvr_bwCapa_ops);
	}
}

static void dvr_proc_unreg(void)
{
	if (dvbdvr_proc) {
		proc_remove(dvbdvr_proc);
		dvbdvr_proc = NULL;
	}
}

static int dmx_dev_register(struct rdvb_dmx * rdvb_dmx, struct dvb_adapter * dvb_adapter)
{
	SDEC_CHANNEL_T *pCh = kmalloc(sizeof(SDEC_CHANNEL_T), GFP_KERNEL);
	struct dmx_demux *dmx = NULL;
	unsigned int j;
	int ret = -1;
	rdvb_dmx->filter_num = DMX_MAX_FILTER_NUM;
	if (pCh == NULL) {
		dmx_err(rdvb_dmx->index, "ERROR: alloc CHANNEL fail!\n");
		return -ENOMEM;
	}
	INIT_LIST_HEAD(&rdvb_dmx->filter_list);
	rdvb_dmx->dmx_filters =
		vmalloc(sizeof(struct dmxfilter) * rdvb_dmx->filter_num);
	if (!rdvb_dmx->dmx_filters){
		dmx_err(rdvb_dmx->index, "ERROR: alloc filter(%d) fail!\n",DMX_MAX_FILTER_NUM);
		goto fail_channel_free;
	}
	for(j=0;j<rdvb_dmx->filter_num;j++){
		rdvb_dmx->dmx_filters[j].priv.state_initialized = 0;
		rdvb_dmx->dmx_filters[j].priv.index = j;
		rdvb_dmx->dmx_filters[j].priv.priv = rdvb_dmx;
	}

	rdvb_dmx->dmx_ver_id   = DMX_VERSION_ID;
	rdvb_dmx->dmx_model_id = DMX_MODEL_ID;
	rdvb_dmx->dmx_chip_id  = DMX_CHIP_ID;
	rdvb_dmx->dmx_country  = DMX_COUNTRY_KR ;
	rdvb_dmx->dmx_platform = DMX_PLATFORM_ATSC;

	rdvb_dmx->input_src_type  = DMX_EXT_SRC_TYPE_NULL;
	rdvb_dmx->input_port_num  = 0;
	rdvb_dmx->input_port_type = DMX_EXT_PORT_TYPE_SERIAL;
	rdvb_dmx->is_audio_paused = false;
	rdvb_dmx_descram_init(&rdvb_dmx->dscmb,rdvb_dmx->index);
	mutex_init(&rdvb_dmx->mutex);
	spin_lock_init(&rdvb_dmx->lock);

	if (SDEC_InitChannel(pCh, rdvb_dmx->index) < 0) {
		dmx_err(pCh->idx, "[%s %d] init SDEC%d fail!!!!!\n",
			__func__, __LINE__, rdvb_dmx->index);
		goto fail_filter_free;
	}

	rdvb_dmx->dmxdev.exit = 0;
	pCh->dvb_demux.filternum = rdvb_dmx->dmxdev.filternum = 64;
	pCh->dvb_demux.feednum = 64;
	dmx = rdvb_dmx->dmxdev.demux = &pCh->dvb_demux.dmx;

	pCh->dvb_demux.priv = rdvb_dmx;
	pCh->dvb_demux.start_feed = rdvb_dmx_startfeed;
	pCh->dvb_demux.stop_feed = rdvb_dmx_stopfeed;

	pCh->dvb_demux.ts_control = rdvb_dmx_filter_control;

	pCh->idx = rdvb_dmx->index;
	
	if (dvb_dmx_init(&pCh->dvb_demux) < 0) {
		dmx_err(pCh->idx, "dmx: %s_%d, fail!!!!!\n",
			__func__, __LINE__);
		goto fail_filter_free;
	}

	if (dvb_dmxdev_init(&rdvb_dmx->dmxdev, dvb_adapter) < 0) {
		dmx_err(pCh->idx, "dmx: %s_%d, fail!!!!!\n",
		 __func__, __LINE__);
		goto fail_filter_free;
	}
	dmx->get_stc = rdvb_dmx_get_stc;
	dmx->prop_set = rdvb_dmx_prop_set;
	dmx->prop_get = rdvb_dmx_prop_get;
	dmx->dvr_open = rdvb_dmx_pvr_open;
	dmx->dvr_close = rdvb_dmx_pvr_close;
	dmx->pvr_prop_set = rdvb_pvr_prop_set;
	dmx->write = rdvb_dmx_write;

	//Add frontends to meet dvr device open flow
	rdvb_dmx->fe_hw.source = DMX_FRONTEND_0;
	if (dmx->add_frontend(rdvb_dmx->dmxdev.demux, &rdvb_dmx->fe_hw) < 0) {
		dmx_err(pCh->idx, " add_frontend failed ()\n");
		goto fail_filter_free;
	}
	rdvb_dmx->fe_mem.source = DMX_MEMORY_FE;
	if (dmx->add_frontend(rdvb_dmx->dmxdev.demux, &rdvb_dmx->fe_mem) < 0) {
		dmx_err(pCh->idx, "[%s, %d]: add_frontend failed ()\n",
			__func__, __LINE__);
		goto fail_filter_free;
	}

	DEBUG_proc_init(rdvb_dmx->index);
	dmx_crit(pCh->idx, "dmx:%s_%d, rdvb_dmx:%p sdec(%p), dvb_demux.priv:%p\n",
		 __func__, __LINE__, rdvb_dmx, pCh, pCh->dvb_demux.priv);
	return 0;

fail_filter_free:
	vfree(rdvb_dmx->dmx_filters);
fail_channel_free:
	kfree(pCh);
	return ret;
}
static void dmx_dev_unregister(struct rdvb_dmx * rdvb_dmx)
{
	SDEC_CHANNEL_T * sdec = (SDEC_CHANNEL_T*)rdvb_dmx->dmxdev.demux;

	dvb_dmxdev_release(&rdvb_dmx->dmxdev);
	dvb_dmx_release((struct dvb_demux*)&rdvb_dmx->dmxdev.demux);
	SDEC_ReleaseChannel(sdec);
	DEBUG_proc_uninit(rdvb_dmx->index);
	kfree(sdec);
	if (rdvb_dmx->dmx_filters)
		vfree(rdvb_dmx->dmx_filters);
}

int dmx_init(void)
{
	int i = 0, j = 0;
	int pollsize = 0 ;
	struct rdvb_adapter * rdvb_adap = rdvb_get_adapter();
	struct rdvb_dmxdev *rdvb_dmxdev = get_rdvb_dmxdev();

	rdvb_dmxdev->dmxnum = DMX_MAX_CHANNEL_NUM;
	rdvb_dmxdev->rdvb_dmx =
			vmalloc(sizeof(struct rdvb_dmx) * rdvb_dmxdev->dmxnum);
	pollsize = ((sizeof(REFCLOCK)+ 0xf) & ~0xf) *rdvb_dmxdev->dmxnum;
	rdvb_dmxbuf_Init(pollsize);
	for (i=0; i< rdvb_dmxdev->dmxnum; i++){
		struct rdvb_dmx *rdvb_dmx = &rdvb_dmxdev->rdvb_dmx[i];
		rdvb_dmx->index = i;

		if (dmx_dev_register(rdvb_dmx, rdvb_adap->dvb_adapter) < 0) {
			dmx_err(i, "ERROR: dmx register fail!!!!!\n");
			vfree(rdvb_dmxdev->rdvb_dmx);
			return -ENOMEM;
		}
		dmx_crit(i, "dmx: INIT dmx %d success\n", i);
		
	}
	mutex_init(&rdvb_dmxdev->mutex);
	rdvb_dmxdev->sdecnum = DMX_MAX_SECTION_FILTER_NUM;
	rdvb_dmxdev->dmxsecs =
			vmalloc(sizeof(struct dmxsec) *rdvb_dmxdev->sdecnum);
	for (j=0; j<rdvb_dmxdev->sdecnum; j++) {
		rdvb_dmxdev->dmxsecs[j].priv.idx = j;
		rdvb_dmxdev->dmxsecs[j].priv.state_initialized = false;
		rdvb_dmxdev->dmxsecs[j].priv.priv = NULL;
	}
	spin_lock_init(&rdvb_dmxdev->lock);
	dmx_proc_reg();
	dvr_proc_reg();
	dmx_service_on();
	return 0;
}

void dmx_exit(void)
{
	int i = 0;
	struct rdvb_dmxdev * rdvb_dmxdev = get_rdvb_dmxdev();
	if (rdvb_dmxdev->dmxsecs)
		vfree(rdvb_dmxdev->dmxsecs);
	dmx_proc_unreg();
	dvr_proc_unreg();
	dmx_service_off();
	for (i=0;i< rdvb_dmxdev->dmxnum;i++){
		struct rdvb_dmx *rdvb_dmx = &rdvb_dmxdev->rdvb_dmx[i];
		dmx_dev_unregister(rdvb_dmx);
	}
	if (rdvb_dmxdev->rdvb_dmx) vfree(rdvb_dmxdev->rdvb_dmx);
	rdvb_dmxbuf_Destroy();
}
module_init(dmx_init);
module_exit(dmx_exit);


MODULE_AUTHOR("jason_wang <jason_wang@realsil.com.cn>");
MODULE_DESCRIPTION("RTK TV dvb dmx Driver");
MODULE_LICENSE("GPL");
