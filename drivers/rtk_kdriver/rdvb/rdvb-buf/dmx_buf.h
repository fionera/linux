#ifndef __DMX_BUF__
#define __DMX_BUF__
#include "dmx_buf_misc.h"


int dmx_buf_poll_setup(struct BUF_INFO **pBuf,unsigned int size,
		unsigned int nr_buf_header);
int dmx_buf_poll_alloc(struct BUF_INFO *pbuf, unsigned int size);

int dmx_buf_alloc_tp_main(unsigned char index, struct BUF_INFO* buf);
int dmx_buf_alloc_tp_sub(unsigned char index, struct BUF_INFO* buf);

int dmx_buf_vtp_bs_alloc(struct BUF_INFO_T * pBuf);
int dmx_buf_vtp_ib_alloc(struct BUF_INFO_T * pBuf);
int dmx_buf_vtp_bs_free(struct BUF_INFO_T * pBuf);
int dmx_buf_vtp_ib_free(struct BUF_INFO_T * pBuf);


int dmx_buf_atp_bs_alloc(struct BUF_INFO_T * pBuf);
int dmx_buf_atp_ib_alloc(struct BUF_INFO_T * pBuf);
int dmx_buf_atp_bs_free(struct BUF_INFO_T * pBuf);
int dmx_buf_atp_ib_free(struct BUF_INFO_T * pBuf);


int dmx_buf_pes_bs_alloc(struct BUF_INFO_T *pBuf);
int dmx_buf_pes_bs_free(struct BUF_INFO_T *pBuf);


unsigned int dmx_buf_get_availableSize(struct BUF_INFO_T *pBuf);
int dmx_buf_write(struct BUF_INFO_T * pBuf, void *data, unsigned int size);
int dmx_buf_writeIB(struct BUF_INFO_T * pBuf, void *pData, unsigned int dataSize);
int dmx_buf_sync(struct BUF_INFO_T * pBuf);

//for pes

int dmx_buf_write_rollback(struct BUF_INFO_T * pBuf);
int dmx_buf_transpes(struct BUF_INFO_T *pBuf, ts_callback callback, void * filter);

//ecp

int dmx_buf_ta_map(unsigned char idx, unsigned char type,
				   struct BUF_INFO_T *es, struct BUF_INFO_T *ib);
int dmx_buf_ta_unmap(unsigned char idx, unsigned char type,
					struct BUF_INFO_T *es, struct BUF_INFO_T *ib);


int dmx_buf_ta_invalid(struct BUF_INFO_T *buf);
#endif 
