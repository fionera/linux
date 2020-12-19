#include <linux/string.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include <rdvb-common/misc.h>
#include <rdvb-common/dmx_re_ta.h>
#include "dmx_buf.h"
#include "dmx_buf_dbg.h"

#include "rdvb-dmx/dmx_filter.h"
#include "dmx_buf_mgr.h"
static struct rdvb_dmxbuff_mgr dbm;

static int dmxbuf_setup_tpbuf(struct TP_INFO_T * tp)
{
	switch (tp->tp_buf_type) {
	case TP_BUF_LEVEL_A:
		if (dmx_buf_alloc_tp_main(tp->index, &tp->buf) < 0) {
			dmx_err(tp->index, "ERROR : FAIL alloc tp  buf\n");
			return -1;
		}
		break;
	case TP_BUF_LEVEL_B:
		if (dmx_buf_alloc_tp_sub(tp->index, &tp->buf) < 0) {
			dmx_err(tp->index, "ERROR : FAIL alloc tp  buf\n");
			return -1;
			
		}
		break;
	default:
		break;
	}
	return  0;
}

static int dmxbuf_setup_buffpoll(unsigned int pollsize, unsigned int  nr_buf_header)
{
	return dmx_buf_poll_setup(&dbm.buffPoll, pollsize, nr_buf_header);
}
int dmxbuf_init(unsigned int pollsize)
{

	int i = 0;

	memset(&dbm, 0, sizeof(dbm));
	for (i=0;i<PIN_MAX_NUM;i++) {
		dbm.pinInfo[i].src = -1;
		dbm.pinInfo[i].dest = -1;
		dbm.pinInfo[i].priv = NULL;

		dbm.pinInfo[i].index = i;
		if (i < PIN_VTP_END){
			dbm.pinInfo[i].pin_type = PIN_TYPE_VTP;
			dbm.pinInfo[i].pin_index =  i - PIN_VTP_START;
		} else if (i < PIN_ATP_END){
			dbm.pinInfo[i].pin_type = PIN_TYPE_ATP;
			dbm.pinInfo[i].pin_index =  i - PIN_ATP_START;
		} else if  (i < PIN_PES_END){
			dbm.pinInfo[i].pin_type = PIN_TYPE_PES;
			dbm.pinInfo[i].pin_index =  i - PIN_PES_START;
		}
		dmx_buf_dbg_init(&dbm.pinInfo[i].debug);
	}

	for (i=0;i<TP_BUF_MAX_NUM;i++) {
		if (i < TP_BUF_MAIN_MAX_NUM)
			dbm.tpinfo[i]. tp_buf_type = TP_BUF_LEVEL_A;
		else
			dbm.tpinfo[i]. tp_buf_type = TP_BUF_LEVEL_B;
		dbm.tpinfo[i].index = i;
		if (dmxbuf_setup_tpbuf(&dbm.tpinfo[i])<0)
			return -1;
		dbm.tpinfo[i].owner = TP_CON_NUM;
		dbm.tpinfo[i].used = false;
	}

	if (dmxbuf_setup_buffpoll(pollsize, PIN_MAX_NUM*2) < 0) {
		dmx_err(NOCH, "ERROR : FAIL alloc buff  poll\n");
		return -1;
	}
	mutex_init(&dbm.mutex);
	dbm.bInit = 1;
	return 0;
}

void dmxbuf_uninit(void)
{

	int i = 0;
	if (!dbm.bInit)
		return;

	dbm.bInit = 0;
	for (i=0;i<PIN_MAX_NUM;i++) {
	}
	dbm.buffPoll = NULL;
	mutex_destroy(&dbm.mutex);
}

int dmxbuf_ecp_enter(void)
{
	return 0;
}
int dmxbuf_ecp_exit(void)
{
	return 0;
}
extern unsigned int g_buffer_poll_allocated_size;
char * rdvb_dmxbuf_status(char * buf)
{
	int i  = 0;
	printk("dmx buffer init:%d\n", dbm.bInit);
	while(i<PIN_VTP_END) {
		struct PIN_INFO_T *pinInfo = &dbm.pinInfo[i];
		RINGBUFFER_HEADER* es_phd =
			((RINGBUFFER_HEADER*)pinInfo->es.bufHeader.viraddr);
		RINGBUFFER_HEADER* ib_phd =
			((RINGBUFFER_HEADER*)pinInfo->ib.bufHeader.viraddr);
		buf = print_to_buf(buf,
			"dmx VTP(%d)_use:%d SDEC(%d), VDEC(%d)\n"
			"\tes(b:0x%x,u:0x%x,w:0x%x,r:0x%x)\n"
			"\tib(b:0x%x,u:0x%x,w:0x%x,r:0x%x))\n",
			i, pinInfo->used, pinInfo->src, pinInfo->dest,
			pinInfo->es.buf.phyaddr,
			pinInfo->es.buf.phyaddr+pinInfo->es.buf.bufSize,
			reverse_int32(es_phd?es_phd->writePtr:0),
			reverse_int32(es_phd?es_phd->readPtr[0]:0),

			pinInfo->ib.buf.phyaddr,
			pinInfo->ib.buf.phyaddr+pinInfo->ib.buf.bufSize,
			reverse_int32(ib_phd?ib_phd->writePtr:0),
			reverse_int32(ib_phd?ib_phd->readPtr[0]:0)
			);
		buf = print_to_buf(buf, "\t Total data:%d "
			"deliver_size:%d, drop_count:%d\n",
			pinInfo->debug.total_size,
			pinInfo->debug.deliver_size, pinInfo->debug.drop_count);
		i++;
	}
	while(i<PIN_ATP_END) {
		struct PIN_INFO_T *pinInfo = &dbm.pinInfo[i];
		RINGBUFFER_HEADER* es_phd =
			((RINGBUFFER_HEADER*)pinInfo->es.bufHeader.viraddr);
		RINGBUFFER_HEADER* ib_phd =
			((RINGBUFFER_HEADER*)pinInfo->ib.bufHeader.viraddr);
		buf = print_to_buf(buf,
			"dmx ATP(%d)_use:%d SDEC(%d), ADEC(%d)\n"
			"\tes(b:0x%x,u:0x%x,w:0x%x,r:0x%x)\n"
			"\tib(b:0x%x,u:0x%x,w:0x%x,r:0x%x))\n",
			i, pinInfo->used, pinInfo->src, pinInfo->dest,
			pinInfo->es.buf.phyaddr,
			pinInfo->es.buf.phyaddr+pinInfo->es.buf.bufSize,
			reverse_int32(es_phd?es_phd->writePtr:0),
			reverse_int32(es_phd?es_phd->readPtr[0]:0),

			pinInfo->ib.buf.phyaddr,
			pinInfo->ib.buf.phyaddr+pinInfo->ib.buf.bufSize,
			reverse_int32(ib_phd?ib_phd->writePtr:0),
			reverse_int32(ib_phd?ib_phd->readPtr[0]:0)
			);
		buf = print_to_buf(buf, "\t Total data:%d "
			"deliver_size:%d, drop_count:%d\n",
			pinInfo->debug.total_size,
			pinInfo->debug.deliver_size, pinInfo->debug.drop_count);
		i++;
	}
	while(i<PIN_PES_END) {
		struct PIN_INFO_T *pinInfo = &dbm.pinInfo[i];
		RINGBUFFER_HEADER* es_phd =
			((RINGBUFFER_HEADER*)pinInfo->es.bufHeader.viraddr);
		RINGBUFFER_HEADER* ib_phd =
			((RINGBUFFER_HEADER*)pinInfo->ib.bufHeader.viraddr);
		buf = print_to_buf(buf,
			"dmx PES(%d)_use:%d SDEC(%d),  PES(%d)"
			"\tes(b:0x%x,u:0x%x,w:0x%x,r:0x%x)"
			"\tib(b:0x%x,u:0x%x,w:0x%x,r:0x%x))\n",
			i, pinInfo->used, pinInfo->src, pinInfo->dest,
			pinInfo->es.buf.phyaddr,
			pinInfo->es.buf.phyaddr+pinInfo->es.buf.bufSize,
			reverse_int32(es_phd?es_phd->writePtr:0),
			reverse_int32(es_phd?es_phd->readPtr[0]:0),

			pinInfo->ib.buf.phyaddr,
			pinInfo->ib.buf.phyaddr+pinInfo->ib.buf.bufSize,
			reverse_int32(ib_phd?ib_phd->writePtr:0),
			reverse_int32(ib_phd?ib_phd->readPtr[0]:0)
			);
		buf = print_to_buf(buf, "\t Total data:%d "
			"deliver_size:%d, drop_count:%d\n",
			pinInfo->debug.total_size,
			pinInfo->debug.deliver_size, pinInfo->debug.drop_count);
		i++;
	}
	buf = print_to_buf(buf, "dmx poll: b:0x%x, u:0x%x, size:0x%x: used:0x%x\n", 
		dbm.buffPoll->phyaddr, dbm.buffPoll->phyaddr+dbm.buffPoll->bufSize,
		dbm.buffPoll->bufSize, g_buffer_poll_allocated_size);

	i = 0;
	while (i< TP_BUF_MAX_NUM) {
		struct TP_INFO_T *tpinfo = &dbm.tpinfo[i];
		buf = print_to_buf(buf, "tp_buf:%d, used:%d src:%d, level:%d"
			"\tbase:0x%x, limit:0x%x\n",
			i, tpinfo->used, tpinfo->owner, tpinfo->tp_buf_type,
			tpinfo->buf.phyaddr, tpinfo->buf.phyaddr+tpinfo->buf.bufSize);
		i++;
	}
	return  buf;
}

int dmxbuf_poll_alloc(struct BUF_INFO *pbuf, unsigned int size)
{
	return dmx_buf_poll_alloc(pbuf, size);
}

int dmxbuf_setup_vtpbuffer (struct PIN_INFO_T * pin)
{
	//printk("@@@@####\n");
	LOCK(&dbm.mutex);
	if (dmx_buf_vtp_bs_alloc(&pin->es)<0)
	{
		UNLOCK(&dbm.mutex);
		dmx_err(pin->src, "ERROR : FAIL alloc vtp_%d bs\n", pin->pin_index);
		return -1;
	}
	if (dmx_buf_vtp_ib_alloc(&pin->ib)<0)
	{
		dmx_buf_vtp_bs_free(&pin->es);
		UNLOCK(&dbm.mutex);
		dmx_err(pin->src, "ERROR : FAIL alloc vtp_%d ib\n", pin->pin_index);
		return -1;
	}
	UNLOCK(&dbm.mutex);
	dmx_err(pin->src, "VBS:(0x%x/0x%x),VIB:(0x%x/0x%x)\n",
		pin->es.buf.phyaddr, pin->es.bufHeader.phyaddr,
		pin->ib.buf.phyaddr, pin->ib.bufHeader.phyaddr);
	return 0;
}

static int dmxbuf_release_vtpbuffer (struct PIN_INFO_T * pin)
{
	if (dmx_buf_vtp_bs_free(&pin->es)<0)
	{
		dmx_err(pin->src, "ERROR : FAIL free vtp_%d bs\n", pin->pin_index);
		return -1;
	}
	if (dmx_buf_vtp_ib_free(&pin->ib)<0) {
		dmx_err(pin->src, "ERROR : FAIL free vtp_%d ib\n", pin->pin_index);
		return -1;
	}
	return 0;
}
int dmxbuf_setup_atpbuffer(struct PIN_INFO_T * pin)
{
	//dmx_err(pin->src, "@@@@####\n");
	LOCK(&dbm.mutex);
	if (dmx_buf_atp_bs_alloc(&pin->es)<0)
	{
		UNLOCK(&dbm.mutex);
		dmx_err(pin->src, "ERROR : FAIL alloc atp_%d bs\n", pin->pin_index);
		return -1;
	}
	if (dmx_buf_atp_ib_alloc(&pin->ib)<0)
	{
		dmx_buf_atp_bs_free(&pin->es);
		dmx_err(pin->src, "ERROR : FAIL alloc atp_%d ib\n", pin->pin_index);
		UNLOCK(&dbm.mutex);
		return -1;
	}
	UNLOCK(&dbm.mutex);
	dmx_err(pin->src, "ABS:(0x%x/0x%x),AIB:(0x%x/0x%x)\n",
		pin->es.buf.phyaddr, pin->es.bufHeader.phyaddr,
		pin->ib.buf.phyaddr, pin->ib.bufHeader.phyaddr);
	return 0;
}

static int dmxbuf_release_atpbuffer (struct PIN_INFO_T * pin)
{
	if (dmx_buf_atp_bs_free(&pin->es)<0)
	{
		dmx_err(pin->src, "ERROR : FAIL free atp_%d bs\n", pin->pin_index);
		return -1;
	}
	if (dmx_buf_atp_ib_free(&pin->ib)<0) {
		dmx_err(pin->src, "ERROR : FAIL free atp_%d ib\n", pin->pin_index);
		return -1;

	}
	return 0;
}

int dmxbuf_setup_pesbuffe(struct PIN_INFO_T * pin)
{
	LOCK(&dbm.mutex);
	if (dmx_buf_pes_bs_alloc(&pin->es)<0)
	{
		UNLOCK(&dbm.mutex);
		dmx_err(pin->src, "ERROR : FAIL alloc pes_%d bs\n", pin->pin_index);
		return -1;
	}
	UNLOCK(&dbm.mutex);
	return 0;
}
static int dmxbuf_release_pesbuffer (struct PIN_INFO_T * pin)
{
	if (dmx_buf_pes_bs_free(&pin->es)<0)
	{
		dmx_err(pin->src, "ERROR : FAIL free pes_%d bs\n", pin->pin_index);
		return -1;
	}
	return 0;
}

static int dmxbuf_pin_get(struct PIN_INFO_T *pin)
{

	if (pin == NULL ) {
		return -1;
	}
	kref_get(&pin->kref);
	return 0;
}
static void dmxbuf_pin_cleanup(struct PIN_INFO_T * pin)
{
	dmx_err(pin->src, "type:%d, port:%d \n",pin->pin_type, pin->pin_index);

	switch(pin->pin_type) {
	case PIN_TYPE_VTP:
		dmxbuf_release_vtpbuffer(pin);
	break;
	case PIN_TYPE_ATP:
		dmxbuf_release_atpbuffer(pin);
	break;
	case PIN_TYPE_PES:
		dmxbuf_release_pesbuffer(pin);
	break;
	default:
	break;
	}
}
static void dmxbuf_pin_release(struct kref *kref)
{
	struct PIN_INFO_T *pin = container_of(kref, struct PIN_INFO_T, kref);
	pin->used = false;
	dmxbuf_pin_cleanup(pin);
}
static int dmxbuf_pin_put(struct PIN_INFO_T *pin)
{
	if (pin == NULL || !pin->used) {
		return -1;
	}
	kref_put(&pin->kref, dmxbuf_pin_release);
	return 0;

}

struct PIN_INFO_T* dmxbuf_get_pinInfo(enum  pin_type pin_type, int pin_index)
{
	int start, end;
	int i;
	switch(pin_type){
	case PIN_TYPE_VTP:
		start = PIN_VTP_START;
		end   = PIN_VTP_END;
		break;
	case PIN_TYPE_ATP:
		start = PIN_ATP_START;
		end   = PIN_ATP_END;
		break;
	case PIN_TYPE_PES:
		start = PIN_PES_START;
		end   = PIN_PES_END;
		if (pin_index != -1)\
			break;
		for (i=start;i<end;i++){
			if (!dbm.pinInfo[i].used){
				pin_index = i-start;
				break;
			}
		}
		break;
	default:
		return NULL;
		break;
	}
	if (pin_index >= end-start)
		return NULL;
	return &dbm.pinInfo[start+ pin_index];

}
struct PIN_INFO_T*
dmxbuf_pin_connect(struct PIN_INFO_T * pin, enum host_type host_type, int host_port)
{
	if (pin == NULL)
		return NULL;
	LOCK(&dbm.mutex);
	switch(host_type){
	case HOST_TYPE_SDEC:
		if (pin->used && pin->src != -1){
			dmx_err(pin->src, "dmxbuf: pin already connect src(%d-%d)\n", 
				pin->src, host_port);
			UNLOCK(&dbm.mutex);
			return NULL; 
		}
		pin->src = host_port;
		dmx_buf_dbg_reset(&pin->debug);
	break;
	case HOST_TYPE_VDEC:
	case HOST_TYPE_ADEC:
		if (pin->used && pin->dest != -1){
			dmx_err(pin->src, "dmxbuf: pin already connect dest(%d-%d)\n", 
				pin->dest, host_port);
			UNLOCK(&dbm.mutex);
			return NULL; 
		}
		pin->dest = host_port;
	break;
	default:
		UNLOCK(&dbm.mutex);
		return NULL;
	break;
	}
	if (pin->used == false){
		pin->used = true;
		pin->debug.total_size = 0;
		pin->debug.drop_count = 0;
		pin->debug.deliver_size = 0;
		kref_init(&pin->kref);
	} else{
		dmxbuf_pin_get(pin);
	}
	UNLOCK(&dbm.mutex);
	return pin;
}
struct PIN_INFO_T*
dmxbuf_pin_disconnect(struct PIN_INFO_T * pin, enum host_type host_type, int host_port)
{
	if (pin == NULL)
		return NULL;
	LOCK(&dbm.mutex);
	switch(host_type){
	case HOST_TYPE_SDEC:
		if (!pin->used || pin->src==-1){
			dmx_err(pin->src, "dmxbuf: pin already disconnect"
				"src(used:%d port:%d-%d)\n", 
				pin->used, pin->src, host_port);
			UNLOCK(&dbm.mutex);
			return NULL;
		}
		pin->src = -1;
	break;
	case HOST_TYPE_VDEC:
	case HOST_TYPE_ADEC:
		if (!pin->used || pin->dest==-1){
			dmx_err(pin->src, "dmxbuf: pin already disconnect dest"
				"(used:%d port:%d-%d)\n", 
				pin->used, pin->dest, host_port);
			UNLOCK(&dbm.mutex);
			return NULL;
		}
		pin->dest = -1;
	break;
	default:
		return NULL;
	break;
	}

	dmxbuf_pin_put(pin);
	UNLOCK(&dbm.mutex);
	return pin;
}

int dmxbuf_get_decoder_port(struct PIN_INFO_T * pin)
{
	if (pin == NULL)
		return -1;
	LOCK(&dbm.mutex);
	if (pin->used==false || pin->src == -1)
	{
		dmx_err(pin->src, "ERROR pintype:%d, pinnum:%d\n",
			pin->pin_type, pin->pin_index);
		UNLOCK(&dbm.mutex);
		return -1;
	}
	UNLOCK(&dbm.mutex);
	return pin->dest;

}

unsigned int dmxbuf_get_avail_size(struct PIN_INFO_T * pin)
{
	unsigned int avail_size = 0;
	LOCK(&dbm.mutex);
	avail_size = dmx_buf_get_availableSize(&pin->es);
	UNLOCK(&dbm.mutex);
	return avail_size;

}

int dmxbuf_write(struct PIN_INFO_T * pin, void *data, unsigned int size)
{
	if (pin == NULL)
		return -1;
	LOCK(&dbm.mutex);
	if (pin->used==false || pin->src == -1)
	{
		dmx_err(pin->src, "ERROR pintype:%d, pinnum:%d\n",
			pin->pin_type, pin->pin_index);
		UNLOCK(&dbm.mutex);
		return -1;
	}
	dmx_buf_dbg_dump(&pin->debug, data, size);
	pin->debug.total_size += size;
	if (pin->dest == -1) {
		pin->debug.drop_count += size;
		UNLOCK(&dbm.mutex);
		return 0;
	}
	if (dmx_buf_write(&pin->es, data, size) < 0){
		pin->debug.drop_count += size;
		if ((pin->debug.drop_count & 0xffff) == 0) {
			dmx_err(pin->src, "@@@@ pin:%d, port:%d src:%d buffer full !!!\n",
		 	pin->pin_type, pin->pin_index, pin->src);
		}
		//TODO: DROP HANDLE
		UNLOCK(&dbm.mutex);
		return -1;
	}
	pin->debug.deliver_size += size;
	UNLOCK(&dbm.mutex);
	return 0;
}
int dmxbuf_sync(struct PIN_INFO_T * pin)
{
	LOCK(&dbm.mutex);
	if (dmx_buf_sync(&pin->es) < 0) {
		dmx_err(pin->src, "ERROR \n");
		UNLOCK(&dbm.mutex);
		BUG();
		return -1;
	}
	UNLOCK(&dbm.mutex);
	return 0;

}
int dmxbuf_writeIB(struct PIN_INFO_T * pin, 
	void *data, unsigned int size, unsigned int *pW, bool is_ecp)
{
	LOCK(&dbm.mutex);
	if (pW){
		RINGBUFFER_HEADER * pHeader =
			(RINGBUFFER_HEADER *)pin->es.bufHeader.viraddr;
		*pW = reverse_int32(pHeader->writePtr);
	}
	if (is_ecp) {
		unsigned char buftype;
		buftype = pin->pin_type * 2 + pin->pin_index;
		dmx_ta_ib_write(pin->src, buftype, data, size);
		UNLOCK(&dbm.mutex);
		return 0;
	}
	if (dmx_buf_writeIB(&pin->ib, data, size)< 0){
		UNLOCK(&dbm.mutex);
		return -1;
	}
	if (dmx_buf_sync(&pin->ib)<0) {
		dmx_err(pin->src, "ERROR \n");
		UNLOCK(&dbm.mutex);
		return -1;
	}
	UNLOCK(&dbm.mutex);
	return 0;


}

int dmxbuf_write_rollback(struct PIN_INFO_T * pin)
{

	if (pin == NULL)
		return -1;
	LOCK(&dbm.mutex);
	if (pin->used==false || pin->src == -1)
	{
		dmx_err(pin->src, "ERROR pintype:%d, pinnum:%d\n",
			pin->pin_type, pin->pin_index);
		UNLOCK(&dbm.mutex);
		return -1;
	}
	if (dmx_buf_write_rollback(&pin->es) < 0) {
		dmx_err(pin->src, "@@@@####\n");
		UNLOCK(&dbm.mutex);
		return -1;

	}
	UNLOCK(&dbm.mutex);
	return 0;
}
int dmxbuf_trans_pesdata(struct PIN_INFO_T * pin, ts_callback callback, void * filter)
{
	
	if (pin == NULL)
		return -1;
	LOCK(&dbm.mutex);
	if (dmx_buf_transpes(&pin->es, callback, filter) < 0) {
		dmx_err(pin->src, "@@@@####\n");
		UNLOCK(&dbm.mutex);
		return -1;

	}
	UNLOCK(&dbm.mutex);
	return 0;
}

static struct TP_INFO_T * get_aval_tpbuf(enum tp_buf_type tp_buf_type)
{
	int i = 0;
	for (i=0; i<TP_BUF_MAX_NUM; i++) {
		if (dbm.tpinfo[i].tp_buf_type == tp_buf_type 
			&& dbm.tpinfo[i].used == false)
			return &dbm.tpinfo[i];
	}
	return NULL;
}

static struct TP_INFO_T * get_tpbuf_byowner(enum tp_con_type tct)
{

	int i = 0;
	for (i=0; i<TP_BUF_MAX_NUM; i++) {
		if (dbm.tpinfo[i].owner == tct 
			&& dbm.tpinfo[i].used == true)
			return &dbm.tpinfo[i];
	}
	return NULL;
}
int dmxbuf_request_tpbuf(enum tp_con_type tct, enum tp_buf_type tp_buf_type, 
		u32 *phyaddr, u8 ** viraddr, unsigned int *size, bool iscache)
{
	struct TP_INFO_T   * buf = NULL;
	LOCK(&dbm.mutex);
	buf =  get_aval_tpbuf(tp_buf_type);
	if (buf == NULL) {
		dmx_err(tct, "@@@@#### no avaliable tp buffer (buf_level:%d)\n",
			tp_buf_type);
		UNLOCK(&dbm.mutex);
		return -1;

	}
	if (!iscache && buf->buf.iscache) {
		dmx_err(tct, "@@@@#### can not get non cache memory\n");
		UNLOCK(&dbm.mutex);
		return -1;
	}

	*phyaddr = buf->buf.phyaddr;
	*viraddr = iscache? buf->buf.cacheaddr: buf->buf.viraddr;
	*size = buf->buf.bufSize;
	buf->owner = tct;
	buf->used = true;


	UNLOCK(&dbm.mutex);
	return 0;
}


int dmxbuf_release_tpbuf(enum tp_con_type tct)
{
	struct TP_INFO_T   * buf = NULL;
	LOCK(&dbm.mutex);
	buf =  get_tpbuf_byowner(tct);
	if (buf == NULL){
		dmx_err(tct, "@@@@#### no tp buffer \n");
		UNLOCK(&dbm.mutex);
		return 0;

	}
	buf->used = false;
	buf->owner = TP_CON_NUM;
	UNLOCK(&dbm.mutex);
	return 0;
}

int dmxbuf_ta_map(struct PIN_INFO_T *pin)
{
	unsigned char buftype;
	buftype = pin->pin_type * 2 + pin->pin_index;
	dmx_notice(pin->src, "[ECP] buf map:pintype:%d, pinport:%d\n",
			   pin->pin_type, pin->pin_index);
	LOCK(&dbm.mutex);
	dmx_buf_ta_map(pin->src, buftype, &pin->es, &pin->ib);
	UNLOCK(&dbm.mutex);
	return 0;
}

int dmxbuf_ta_unmap(struct PIN_INFO_T *pin)
{
	unsigned char buftype;
	buftype = pin->pin_type * 2 + pin->pin_index;
	dmx_notice(pin->src, "[ECP] buf unmap:pintype:%d, pinport:%d\n",
			   pin->pin_type, pin->pin_index);
	LOCK(&dbm.mutex);
	dmx_buf_ta_unmap(pin->src, buftype, &pin->es, &pin->ib);
	UNLOCK(&dbm.mutex);
	return 0;
}

int dmxbuf_ta_invalid(struct PIN_INFO_T * pin)
{
	//only support es ,for now
	LOCK(&dbm.mutex);
	dmx_buf_ta_invalid(&pin->es);
	UNLOCK(&dbm.mutex);
	return 0;
}
int dmxbuf_ta_map_poll(void)
{
	dmx_notice(NOCH, "[ECP] poll buf map:\n");
	LOCK(&dbm.mutex);
	dmx_buf_ta_map_ex(-1, buf_type_poll, dbm.buffPoll);
	UNLOCK(&dbm.mutex);
	return 0;
}