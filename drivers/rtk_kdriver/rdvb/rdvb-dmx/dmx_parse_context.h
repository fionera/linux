#ifndef __DMX_PARSE_CONTEXT__
#define __DMX_PARSE_CONTEXT__
//#include <linux/spinlock.h>
#include <rdvb-common/misc.h>
#include "rdvb_dmx_ctrl.h"
#include <rdvb-common/dmx_re_ta.h>

#define PES_HEADER_BUF_SIZE 14
enum data_context_type {
	DATA_CONTEXT_TYPE_VIDEO,
	DATA_CONTEXT_TYPE_AUDIO,
	DATA_CONTEXT_TYPE_PES,
	DATA_CONTEXT_TYPE_FUNC,
	DATA_CONTEXT_TYPE_MAX,
};
enum tpc_state {
	TPC_STATE_FREE,
	TPC_STATE_ALLOCATED,
	TPC_STATE_GO,
};

enum scram_status
{
	SCRAM_STATUS_UNKNOWN = -1,
	SCRAM_STATUS_NOT_SCRAMBLE = 0,
	SCRAM_STATUS_SCRAMBLE = 1,
};


struct audio_ad_info{
	unsigned char AD_fade_byte;
	unsigned char AD_pan_byte;
	char AD_gain_byte_center;
	char AD_gain_byte_front;
	char AD_gain_byte_surround;
};
struct tpc_debug {
	uint32_t dsc_count;
	uint64_t dsc_lastprint;
};
struct ts_parse_context {
	//struct mutex     mutex;
	struct list_head list_head;
	bool             is_avalible;
	int              state;
	void             *priv;

	unsigned short         pid;
	enum data_context_type data_context_type;
	int                    data_port_num;
	bool                   start;
	//SCRAMBLE CHECK
	bool            is_checkscram;
	unsigned short  scram_status;
	// temi
	bool          is_temi;
	u8            temi_timeline_id;
	//pcr
	bool          is_pcr;
	//pes
	int           events;
	int           contiCounter;
	bool          bKeepPES;
	bool          bCheckContiCounter;
	bool          bResyncStartUnit;
	bool          bSyncPTSEnable;
	bool          bResyncPTS;    /*for bug 13648*/
	int64_t       pts;
	unsigned char pesHeaderBuf[PES_HEADER_BUF_SIZE];
	unsigned int  bufferedBytes;
	unsigned int  remainingHeaderBytes;
	unsigned int  demuxedCount;
	unsigned int  deliver_data_size;
	//for audio description
	bool                  is_ad;
	struct audio_ad_info  adi;
	//for pes
	unsigned int   unitSize; /* PES_packet_length */
	unsigned int   size;     /* accumulated size */
	//int pes_buf_port;
	void *filter;
	ts_callback dfc;

	//for debug
	struct tpc_debug tpc_debug;
};

int ts_parse_context_init(struct ts_parse_context *tpc);

struct ts_parse_context * get_ts_parse_context(struct list_head *tpc_list_head,
	unsigned short pid);
int  put_ts_parse_context( struct ts_parse_context * tpc);
struct ts_parse_context *
ts_parse_context_find(struct list_head *tpc_list_head, unsigned short pid);
int ts_parse_context_set(struct ts_parse_context *tpc,
	enum data_context_type data_context_type, int pin_port, ts_callback dfc, void * source);
int ts_parse_context_setPcr(struct ts_parse_context *tpc);
int ts_parse_context_removePcr(struct ts_parse_context *tpc);
void tpc_reset_deliver_size(struct list_head * tpc_list_head);
static inline void tpc_set_deliver_size(struct ts_parse_context *tpc, unsigned int size)
{
	tpc->deliver_data_size += size;
}

static inline enum pin_type tpc_get_pin_type(enum data_context_type type)
{
	if (type == DATA_CONTEXT_TYPE_VIDEO)
		return PIN_TYPE_VTP;
	if (type == DATA_CONTEXT_TYPE_AUDIO)
		return PIN_TYPE_ATP;
	if (type == DATA_CONTEXT_TYPE_PES)
		return PIN_TYPE_PES;
	return -1;
}

struct ts_parse_context *
tpc_find_by_pin(struct list_head *tpc_list_head, enum data_context_type type, int port);
int tpc_get_decoder_port(struct ts_parse_context *tpc);
int tpc_varify_writable(struct list_head * tpc_list_head, bool no_drop);
int ts_parse_context_enable_scramcheck(struct ts_parse_context *tpc);
int ts_parse_context_disable_scramcheck(struct ts_parse_context *tpc);
int ts_parse_context_get_scramstatus(struct ts_parse_context *tpc, int *status);


int ts_parse_context_temi_enable(struct ts_parse_context *tpc, u8 timeline_id,
									ts_callback tc, void * source);
int ts_parse_context_temi_disable(struct ts_parse_context *tpc);


void tpc_flush_vo(struct list_head* tpc_head);
void tpc_set_speed_to_vo(struct list_head* tpc_list_head, int speed);
void tpc_stop_audio_decoding(struct list_head* tpc_list_head);
void tpc_packet_dcc_report (struct ts_parse_context *tpc, int count);

int  tpc_pack(struct list_head *tpc_list_head, struct ta_tpc_info *ta_tpcs);
void ts_parse_context_ta_create(u8 idx, struct ts_parse_context *tpc);
void ts_parse_context_ta_release(u8 idx, struct ts_parse_context *tpc);
int tpc_output_buf_ta_map(struct list_head *tpc_list_head);
int tpc_output_buf_ta_unmap(struct list_head *tpc_list_head);
//for debug
void ts_parse_context_dump(struct list_head * tpc_list_head);
#endif
