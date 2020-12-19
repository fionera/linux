#ifndef __DMX_BUF_MISC__
#define __DMX_BUF_MISC__

#include <linux/types.h>
#include <rdvb-common/misc.h>
#include "rpc_common.h"

#define TP_BUF_MAIN_MAX_NUM 2
#define TP_MAIN_BUFF_SIZE 0xfff000 //(16*1024*1024 - 6*1024 - 6*1024) // 16M and 6k alignment for protection
#define TP_SUB_BUFF_SIZE (256*1024)

struct BUF_INFO{
	dma_addr_t phyaddr;    /* physical address */
	u8         *viraddr; /* cached address */
	u8         *cacheaddr;

	u8         *pSyncWritePtr;
	long       offsetToViraddr;    /* offset = virtual address - physical address */
	u32        bufSize;
	bool       iscache;

#define CMA_BUF 0X00
#define SVP_BUF 0X01
#define POLL_BUF 0X02
	int        buftype;
};

struct BUF_INFO_T{
	struct BUF_INFO    buf;
	struct BUF_INFO    bufHeader;
};

void dmx_buf_ResetBufHeader(struct BUF_INFO_T * pBuf, RINGBUFFER_TYPE type,
	 unsigned char reverse);
int dmx_buf_AllocBuffer(struct BUF_INFO *pBuf, int size, int isCache, int useZone);
int dmx_buf_FreeBuffer(struct BUF_INFO *pBuf);

int dmx_buf_ta_map_ex(unsigned char idx, unsigned char type, struct BUF_INFO *pBuf);
#endif
