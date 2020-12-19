#ifndef __RDVB_DMXBUF_MGR__
#define __RDVB_DMXBUF_MGR__
#include <linux/types.h>
#include <linux/kref.h>
#include <rdvb-dmx/rdvb_dmx_ctrl.h>
#include <rdvb-common/misc.h>
#include "dmx_buf_misc.h"
#include "rdvb_dmx_buf.h"
#include "dmx_buf_dbg.h"


enum {
	PIN_VTP_START = 0,
	PIN_VTP_END   = PIN_VTP_START+MAX_VTP_NUM,
	PIN_ATP_START = PIN_VTP_END,
	PIN_ATP_END   =  PIN_ATP_START+MAX_ATP_NUM,
	PIN_PES_START = PIN_ATP_END,
	PIN_PES_END   = PIN_PES_START+ MAX_PES_NUM,
	PIN_MAX_NUM   = PIN_PES_END
};

struct PIN_INFO_T{
	int                  index;
	struct kref          kref;
	enum pin_type        pin_type;
	int                  pin_index; /*for vtp/atp*/

	int                  src;
	int                  dest;
	void                 *priv; /* */

	struct BUF_INFO_T	 es;
	struct BUF_INFO_T	 ib;

	bool                  used;

	struct pin_debug debug;
};

struct TP_INFO_T {
	int                index;
	enum tp_buf_type   tp_buf_type;
	struct BUF_INFO    buf;

	bool               used;
	enum tp_con_type   owner;
};

struct rdvb_dmxbuff_mgr{
	bool                   bInit;
	struct mutex           mutex;
	//struct PIN_INFO_T      vtpBuf[MAX_VTP_NUM];
	//struct PIN_INFO_T      atpBuf[MAX_ATP_NUM];
	//struct PIN_INFO_T      pesBuf[MAX_PES_NUM];
	struct PIN_INFO_T      pinInfo[PIN_MAX_NUM];
	struct TP_INFO_T       tpinfo[TP_BUF_MAX_NUM];
	struct BUF_INFO        *buffPoll;
};



int dmxbuf_init(unsigned int pollsize);
void dmxbuf_uninit(void);
int dmxbuf_ecp_enter(void);
int dmxbuf_ecp_exit(void);


int dmxbuf_poll_alloc(struct BUF_INFO *pbuf, unsigned int size);
int dmxbuf_request_tpbuf( enum tp_con_type tct, enum tp_buf_type tp_buf_type, 
		u32 *phyaddr, u8 ** viraddr, unsigned int *size, bool iscache);
int dmxbuf_release_tpbuf(enum tp_con_type tct);

struct PIN_INFO_T* dmxbuf_get_pinInfo(enum  pin_type pin_type, int pin_port);
struct PIN_INFO_T*
	dmxbuf_pin_connect(struct PIN_INFO_T * pin, enum host_type host_type,
	 int host_port);
struct PIN_INFO_T*
	dmxbuf_pin_disconnect(struct PIN_INFO_T * pin, enum host_type host_type,
	int host_port);

int dmxbuf_setup_vtpbuffer (struct PIN_INFO_T * pin);
int dmxbuf_setup_atpbuffer(struct PIN_INFO_T * pin);
int dmxbuf_setup_pesbuffe(struct PIN_INFO_T * pin);


int dmxbuf_get_decoder_port(struct PIN_INFO_T * pin);
unsigned int dmxbuf_get_avail_size(struct PIN_INFO_T * pin);
int dmxbuf_write(struct PIN_INFO_T * pin, void *data, unsigned int size);
int dmxbuf_sync(struct PIN_INFO_T * pin);
int dmxbuf_writeIB(struct PIN_INFO_T * pin, void *data, unsigned int size,
				 unsigned int *pW, bool is_ecp);

int dmxbuf_write_rollback(struct PIN_INFO_T * pin);
int dmxbuf_trans_pesdata(struct PIN_INFO_T * pin, ts_callback callback, void * filter);
//ecp
int dmxbuf_ta_map(struct PIN_INFO_T *pin);
int dmxbuf_ta_unmap(struct PIN_INFO_T *pin);
int dmxbuf_ta_map_poll(void);

int dmxbuf_ta_invalid(struct PIN_INFO_T * pin);
#endif
