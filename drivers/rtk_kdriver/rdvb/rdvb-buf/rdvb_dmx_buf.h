#ifndef __RDVB_DMX_BUF__
#define __RDVB_DMX_BUF__
#include <linux/types.h>
#include <rdvb-dmx/rdvb_dmx_ctrl.h>
#include <rdvb-dmx/dmx_common.h>
#include <rdvb-common/misc.h>
#include "dmx_buf_misc.h"


/*
enum pin_type {
	PIN_TYPE_VTP,
	PIN_TYPE_ATP,
	PIN_TYPE_PES,
};

enum host_type {
	HOST_TYPE_SDEC,
	HOST_TYPE_VDEC,
	HOST_TYPE_ADEC,
};
*/

enum tp_con_type {
	TP_CON_LIVE_A,
	TP_CON_LIVE_B,
	TP_CON_LIVE_C,
	TP_CON_LIVE_D,
	TP_COM_PLAYBACK_A,
	TP_COM_PLAYBACK_B,
	TP_CON_ATSC3,
	TP_CON_JP,
	TP_CON_NUM,
};
enum tp_buf_type {
	TP_BUF_LEVEL_A,//16m
	TP_BUF_LEVEL_B,//256K .SDT
	TP_BUF_LEVEL_NUM,
};

struct buffer_header_info
{
	u32  bs_header_addr;
	u32  ib_header_addr;
	u32  bs_header_size;
	u32  ib_header_size;
};
int rdvb_dmxbuf_Init(unsigned int pollsize);
void rdvb_dmxbuf_Destroy(void);

int rdvb_dmxbuf_poll_alloc(struct BUF_INFO *pbuf, unsigned int size);

int rdvb_dmxbuf_tpbuf_get(enum tp_con_type tct, enum tp_buf_type tbt,
		u32 *phyaddr, u8** viraddr, unsigned int *size, bool iscache);
int rdvb_dmxbuf_tpbuf_put(enum tp_con_type tct);

int rdvb_dmxbuf_request(enum pin_type pin_type, int pin_port, 
	enum host_type host_type, int host_port,void* priv, struct buffer_header_info *data);

int rdvb_dmxbuf_release(enum pin_type pin_type, int pin_port, 
	enum host_type host_type, int host_port);

int rdvb_dmxbuf_get_decoder_port(enum pin_type pin_type, int pin_port);
unsigned int rdvb_dmxbuf_get_avail_size(enum pin_type pin_type, int pin_port);
int rdvb_dmxbuf_write(enum pin_type, int pin_port, void *data, unsigned int size);
int rdvb_dmxbuf_writeIB(enum pin_type, int pin_port,
			void *data, unsigned int size, unsigned int *pW, bool is_ecp);
int rdvb_dmxbuf_sync(enum pin_type pin_type, int pin_port, unsigned char owner);

//only for pes
int rdvb_dmxbuf_write_rollback(enum pin_type pin_type, int pin_port);
int rdvb_dmxbuf_trans_pesdata(enum pin_type pin_type, int pin_port, 
	ts_callback callback, void* filter);

int rdvb_dmxbuf_get_dmxPort(enum pin_type pin_type, int pin_port);

//for ecp

int rdvb_dmxbuf_ta_map(enum pin_type pin_type, int pin_port);
int rdvb_dmxbuf_ta_unmap(enum pin_type pin_type, int pin_port);
int rdvb_dmxbuf_ta_map_poll(void);
int rdvb_dmxbuf_ta_invalid(enum pin_type pin_type, int pin_port);
#endif