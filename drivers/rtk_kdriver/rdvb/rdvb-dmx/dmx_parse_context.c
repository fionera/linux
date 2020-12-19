#include <linux/errno.h>
#include <linux/string.h>
#include <linux/list.h>
#include <rdvb-buf/rdvb_dmx_buf.h>
#include "dmx_parse_context.h"

#include <VideoRPC_System.h>
#include <rtk_kdriver/RPCDriver.h>
#include <rdvb-common/clock.h>
#include <rdvb-common/dmx_re_ta.h>

const char * context_type_str[] = {
	"VIDEO",
	"AUDIO",
	"PES",
	"FUNC"
};
static inline void init_tpc_debug(struct tpc_debug * tpc_debug)
{
	tpc_debug->dsc_count = 0;
	tpc_debug->dsc_lastprint = 0;
}

int ts_parse_context_init(struct ts_parse_context *tpc)
{
	memset(tpc, 0, sizeof(struct ts_parse_context));

	tpc->pid = 0xffff;
	tpc->is_pcr = false;
	tpc->is_temi = false;
	tpc->is_checkscram  = false;

	tpc->is_avalible = true;
	tpc->contiCounter = -1;
	tpc->bResyncStartUnit  = true;
#ifndef DONTFIXBUG13648
	tpc->bResyncPTS = true;
#endif
	tpc->state = TPC_STATE_FREE;
	return 0;
}

extern struct ts_parse_context * 
get_free_ts_parse_context(struct list_head *tpc_list_head);
struct ts_parse_context * 
get_ts_parse_context(struct list_head *tpc_list_head, unsigned short pid)
{
	struct ts_parse_context *tpc = NULL;
	list_for_each_entry (tpc, tpc_list_head, list_head) {
		if (tpc->pid == pid) {
			dmx_err(NOCH, "monitor, tpc:%p,\n", tpc);
			return tpc;
		}
	}
/*
	for (i = 0; i < sdec->tpc_num; i++) {
		if (sdec->tpcs[i].is_avalible)
			break;
	}
	if (i >=  sdec->tpc_num) {
		printk("%s_%d: ts_parse_context out of used(%d)\n", 
			__func__, __LINE__, sdec->tpc_num);
		return NULL;
	}
	tpc = &sdec->tpcs[i]; */
	tpc = get_free_ts_parse_context(tpc_list_head);
	if (tpc == NULL) {
		dmx_err(NOCH, "ERROR ts_parse_context out of used\n");
		return NULL;
	}
	tpc-> is_avalible = false;
	tpc-> pid = pid;
	tpc-> data_context_type = DATA_CONTEXT_TYPE_FUNC;
	tpc-> data_port_num     = -1;
	tpc-> is_temi = false;
	tpc-> is_pcr = false;
	tpc->state = TPC_STATE_ALLOCATED;
	init_tpc_debug(&tpc->tpc_debug);

	list_add(&tpc->list_head, tpc_list_head);
	dmx_err(NOCH, "NEW, tpc:%p\n", tpc);
	return tpc;
}


int  put_ts_parse_context( struct ts_parse_context * tpc)
{
	if (tpc->is_avalible) {
		dmx_err(NOCH, "tpc is not allocated \n");
		return -EINVAL;
	}

	list_del( &tpc->list_head);

	tpc->is_avalible = true;
	tpc->pid = 0xffff;
	tpc->dfc         = NULL;
	tpc->filter      = NULL;

	tpc->state = TPC_STATE_FREE;

	dmx_err(NOCH, "destroy,, tpc:%p,\n", tpc);
	return 0;
}


struct ts_parse_context *
ts_parse_context_find(struct list_head *tpc_list_head, 	unsigned short pid)
{
	struct ts_parse_context *tpc = NULL;
	list_for_each_entry (tpc, tpc_list_head, list_head) {
		if (tpc->pid == pid) {
			return tpc;
		}
	}
	return NULL;

}
struct ts_parse_context *
tpc_find_by_pin(struct list_head *tpc_list_head, enum data_context_type type, int port)
{
	struct ts_parse_context *tpc = NULL;
	list_for_each_entry (tpc, tpc_list_head, list_head) {
		if (tpc->data_context_type == type && tpc->data_port_num == port) {
			return tpc;
		}
	}
	return NULL;

}
int ts_parse_context_reset(struct ts_parse_context *tpc)
{
	if (tpc->is_avalible) {
		dmx_err(NOCH, "tpc is not online \n");
		return -EINVAL;

	}
	//TODO: reset data

	return 0;
}

int ts_parse_context_set(struct ts_parse_context *tpc, 
	enum data_context_type data_context_type, int pin_port,
	ts_callback dfc, void * source)
{

	if (tpc->is_avalible) {
		dmx_err(NOCH, "tpc is not online \n");
		return -EINVAL;

	}
	dmx_err(NOCH, "tpc:%p, type:%d, port:%d\n", tpc, data_context_type, pin_port);
	tpc->data_context_type = data_context_type;
	tpc->data_port_num     = pin_port;
	if (DATA_CONTEXT_TYPE_PES == data_context_type) {
		tpc->dfc               = dfc;
		tpc->filter            = source;
	}
	tpc->state             = TPC_STATE_GO;

	tpc->events = 0;
	tpc->contiCounter         = -1;
	tpc->pts                  = -1;
	tpc->bufferedBytes        = 0;
	tpc->remainingHeaderBytes = 0;
	tpc->bResyncStartUnit     = true;
	tpc->bSyncPTSEnable = false;
	tpc->bResyncPTS =  false;
	tpc->deliver_data_size = 0;
	memset(&tpc->adi, 0, sizeof(struct audio_ad_info));
	tpc->is_ad = false;
	tpc->start = false;
	return 0;
}

int ts_parse_context_setPcr(struct ts_parse_context *tpc)
{
	dmx_err(NOCH, "PCR: set, tpc:%p\n", tpc);
	if (tpc->is_avalible) {
		dmx_err(NOCH, "tpc is not online \n");
		return -EINVAL;

	}

	tpc->is_pcr            = true;
	tpc->state             = TPC_STATE_GO;

	return 0;
}

int ts_parse_context_removePcr(struct ts_parse_context *tpc)
{

	dmx_err(NOCH, "PCR: remove, tpc:%p\n", tpc);
	if (tpc->is_avalible) {
		dmx_err(NOCH, "tpc is not online \n");
		return -EINVAL;

	}
	tpc->is_pcr            = false;
	return 0;
}


int ts_parse_context_enable_scramcheck(struct ts_parse_context *tpc)
{
	if (tpc->is_avalible) {
		dmx_err(NOCH, "tpc is not online \n");
		return -EINVAL;

	}
	tpc->is_checkscram            = true;
	tpc->scram_status             = SCRAM_STATUS_UNKNOWN;
	tpc->state                    = TPC_STATE_GO;
	return 0;
}
int ts_parse_context_disable_scramcheck(struct ts_parse_context *tpc)
{

	if (tpc->is_avalible) {
		dmx_err(NOCH, "tpc is not online \n");
		return -EINVAL;

	}
	tpc->is_checkscram            = false;
	return 0;
}

int ts_parse_context_get_scramstatus(struct ts_parse_context *tpc,
				     int *status)
{

	if (tpc->is_avalible) {
		dmx_err(NOCH, "tpc is not online \n");
		return -EINVAL;
	}
	if (!tpc->is_checkscram) {
		dmx_err(NOCH, "pid:%d not check scramble \n", tpc->pid);
		return -EINVAL;
	}
	*status           = tpc->scram_status;
	return 0;
}

int ts_parse_context_temi_enable(struct ts_parse_context *tpc, u8 timeline_id,
									ts_callback tc, void * source)
{
	if (tpc->is_avalible) {
		dmx_err(NOCH, "tpc is not online \n");
		return -EINVAL;
	}
	tpc->is_temi          = true;
	tpc->temi_timeline_id = timeline_id;
	tpc->dfc              = tc;
	tpc->filter           = source;
	tpc->state            = TPC_STATE_GO;
	return 0;
}

int ts_parse_context_temi_disable(struct ts_parse_context *tpc)
{

	if (tpc->is_avalible) {
		dmx_err(NOCH, "tpc is not online \n");
		return -EINVAL;
	}
	tpc->is_temi     = false;
	tpc->dfc         = NULL;
	tpc->filter      = NULL;
	return 0;
}

void tpc_reset_deliver_size(struct list_head * tpc_list_head)
{
	struct ts_parse_context *tpc = NULL;

	list_for_each_entry (tpc, tpc_list_head, list_head) {
		tpc->deliver_data_size = 0;
	}
}

int tpc_get_decoder_port(struct ts_parse_context *tpc)
{
	enum pin_type pin_type;
	pin_type = tpc_get_pin_type(tpc->data_context_type);
	return rdvb_dmxbuf_get_decoder_port(pin_type, tpc->data_port_num);
}

int tpc_varify_writable(struct list_head * tpc_list_head, bool no_drop)
{
	struct ts_parse_context *tpc = NULL;
	unsigned long buf_avail = 0;
	enum pin_type pin_type;
	int result = 0;
	list_for_each_entry (tpc, tpc_list_head, list_head) {
		pin_type = tpc_get_pin_type(tpc->data_context_type);
		if ((int)pin_type < 0)
			continue;
		buf_avail = rdvb_dmxbuf_get_avail_size(pin_type, tpc->data_port_num);
		if (buf_avail <= tpc->deliver_data_size) {
			/*
			printk("[%s%d] buffer full \n",
				context_type_str[tpc->data_context_type],
				tpc_get_decoder_port(tpc));
			*/
			result --;
		}
	}
	// DTV ALWAYS DROP WHEN BUFFER FULL;
	if (no_drop == false)
		result = 0;
	return result;
}

void tpc_flush_vo(struct list_head* tpc_list_head)
{
	int port = 0;
	enum pin_type pin_type;
	unsigned int buf_avail = 0;
	struct ts_parse_context *tpc = NULL;
	long ignore;

	list_for_each_entry (tpc, tpc_list_head, list_head) {
		if (tpc->data_context_type != DATA_CONTEXT_TYPE_VIDEO)
			continue;
		port = tpc_get_decoder_port(tpc);
		if (port <  0){
			//pr_warn("%s_%d: fail get video port!!\n", __func__, __LINE__);
			return;
		}
		pin_type = tpc_get_pin_type(tpc->data_context_type);
		buf_avail = rdvb_dmxbuf_get_avail_size(pin_type, tpc->data_port_num);
		if (buf_avail > 1*1024*1024 +512*1024)
			return;
		if (send_rpc_command(RPC_VIDEO, VIDEO_RPC_VOUT_ToAgent_VOCompFlush, port, 0, &ignore)) {
			pr_err("RPC fail!!\n");
		}
	}
}

void tpc_set_speed_to_vo(struct list_head* tpc_list_head, int speed)
{
	int port = 0;
	struct ts_parse_context *tpc = NULL;
	long ignore = 0;

	list_for_each_entry (tpc, tpc_list_head, list_head) {
		if (tpc->data_context_type != DATA_CONTEXT_TYPE_VIDEO)
			continue;
		port = tpc_get_decoder_port(tpc);
		if (port <  0){
			dmx_warn(NOCH, "[%s, %d]: fail get video port!!\n", __func__, __LINE__);
			return;
		}
		if (send_rpc_command(RPC_VIDEO, VIDEO_RPC_VOUT_ToAgent_SetDemuxSpeed, port, speed, &ignore)) {
			dmx_err(NOCH, "[%s, %d]: RPC fail!! speed=%d \n",
				__func__, __LINE__, speed);
		} else {
			dmx_info(NOCH, "[%s, %d]: Success! speed=%d \n",
				__func__, __LINE__, speed);
		}
		break;
	}
}

extern int KHAL_AUDIO_TrickStopDecoding(int index);
void tpc_stop_audio_decoding(struct list_head* tpc_list_head)
{
	int port = 0;
	struct ts_parse_context *tpc = NULL;

	list_for_each_entry (tpc, tpc_list_head, list_head) {
		if (tpc->data_context_type != DATA_CONTEXT_TYPE_AUDIO)
			continue;
		port = tpc_get_decoder_port(tpc);
		if (port <  0){
			dmx_err(NOCH, "%s_%d: fail get audio port!!\n", __func__, __LINE__);
			return;
		}
		if (KHAL_AUDIO_TrickStopDecoding(port) < 0) {
			dmx_err(NOCH, "[%s, %d]: Fail !! \n", __func__, __LINE__);
		} else {
			dmx_info(NOCH, "[%s, %d]: Success!\n", __func__, __LINE__);
		}
		break;
	}
}

void tpc_packet_dcc_report (struct ts_parse_context *tpc, int count)
{
	tpc->tpc_debug.dsc_count ++;
	if (CLOCK_GetPTS() -  tpc->tpc_debug.dsc_lastprint > 3*90000) {
		tpc->tpc_debug.dsc_lastprint = CLOCK_GetPTS();
		dmx_err(NOCH,"tpc_packet_dcc_report:[E][type:%d_%d] [pid:%d](%d)"
						"detect discontinuous continuity-counter (%d != %d)\n",
			tpc->data_context_type, tpc->data_port_num, tpc->pid,
			tpc->tpc_debug.dsc_count, tpc->contiCounter, count);
		tpc->tpc_debug.dsc_count = 0;
	} else if (tpc->tpc_debug.dsc_count < 6) {
		dmx_err(NOCH,"tpc_packet_dcc_report:[S][type:%d_%d] [pid:%d](%d)"
						"detect discontinuous continuity-counter (%d != %d)\n",
			tpc->data_context_type, tpc->data_port_num, tpc->pid,
			tpc->tpc_debug.dsc_count, tpc->contiCounter, count);
	}
}

void ts_parse_context_dump(struct list_head * tpc_list_head)
{
	struct ts_parse_context *tpc = NULL;

	list_for_each_entry (tpc, tpc_list_head, list_head) {
		printk("TPC:%p state:%d, pid:0x%x, type:%d, portNum:%d, scrmCheck:%d,"
		    "pcr:%d, cb:%p, temi:%d start:%d\n", tpc,
		    tpc->state, tpc->pid, tpc->data_context_type, tpc->data_port_num, 
		    tpc->is_checkscram, tpc->is_pcr, tpc->dfc, tpc->is_temi, tpc->start);
	}

}
/*
*	ECP FUNCTIONS.
*
*/
int  tpc_pack(struct list_head *tpc_list_head, struct ta_tpc_info *ta_tpcs)
{
	unsigned char nr = 0;
	unsigned char idx = 0;
	struct ts_parse_context *tpc = NULL;
	list_for_each_entry (tpc, tpc_list_head, list_head) {
		nr++;
	}
	if (nr == 0)
		return 0;
	ta_tpcs->tpc_infos = kmalloc(sizeof(struct tpc_info) * nr, GFP_KERNEL);
	ta_tpcs->num = nr;
	list_for_each_entry (tpc, tpc_list_head, list_head) {
		ta_tpcs->tpc_infos[idx].pid       = tpc->pid;
		ta_tpcs->tpc_infos[idx].type      = tpc->data_context_type;
		ta_tpcs->tpc_infos[idx].dest_port = tpc->data_port_num;
		idx ++;
	}
	return 0;
}

void ts_parse_context_ta_create(u8 idx, struct ts_parse_context *tpc)
{
	enum pin_type pin_type = tpc_get_pin_type(tpc->data_context_type);
	rdvb_dmxbuf_ta_map(pin_type, tpc->data_port_num);
	dmx_ta_tpc_create(idx, tpc->pid, tpc->data_context_type, tpc->data_port_num);

}

void ts_parse_context_ta_release(u8 idx, struct ts_parse_context *tpc)
{
	enum pin_type pin_type = tpc_get_pin_type(tpc->data_context_type);
	dmx_ta_tpc_release(idx, tpc->pid);
	rdvb_dmxbuf_ta_unmap(pin_type, tpc->data_port_num);
}

int tpc_output_buf_ta_map(struct list_head *tpc_list_head)
{
	struct ts_parse_context *tpc = NULL;
	list_for_each_entry(tpc, tpc_list_head, list_head) {
		enum pin_type pin_type = tpc_get_pin_type(tpc->data_context_type);
		if (pin_type < 0)
			continue;
		rdvb_dmxbuf_ta_map(pin_type, tpc->data_port_num);
	}
	return 0;
}

int tpc_output_buf_ta_unmap(struct list_head *tpc_list_head)
{
	struct ts_parse_context *tpc = NULL;
	list_for_each_entry(tpc, tpc_list_head, list_head) {
		enum pin_type pin_type = tpc_get_pin_type(tpc->data_context_type);
		if (pin_type < 0)
			continue;
		rdvb_dmxbuf_ta_unmap(pin_type, tpc->data_port_num);
	}
	return 0;
}

