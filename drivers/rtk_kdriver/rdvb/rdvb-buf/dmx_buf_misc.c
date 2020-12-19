
#include <linux/types.h>
#include <linux/string.h>
#include <asm/io.h>
#include <rdvb-common/misc.h>
#include <rdvb-common/dmx_re_ta.h>
#include "dmx_buf_misc.h"


void dmx_buf_ResetBufHeader(struct BUF_INFO_T * pBuf, RINGBUFFER_TYPE type,
	unsigned char reverse)
{
	RINGBUFFER_HEADER *pHeader = NULL;
	if (pBuf == NULL)
		return;
	pHeader = (RINGBUFFER_HEADER *)pBuf->bufHeader.viraddr;
	if (pBuf->buf.phyaddr == 0){
#ifdef CONFIG_ARM64
		memset_io(pHeader, 0 , sizeof(RINGBUFFER_HEADER));
#else
		memset(pHeader, 0 , sizeof(RINGBUFFER_HEADER));
#endif
		return;
	}
	if (reverse) {
		pHeader->beginAddr    = reverse_int32(pBuf->buf.phyaddr);
		pHeader->size         = reverse_int32(pBuf->buf.bufSize);
		pHeader->bufferID     = reverse_int32(type);
		pHeader->writePtr     = reverse_int32(pBuf->buf.phyaddr);
		pHeader->numOfReadPtr = reverse_int32(1);
		pHeader->readPtr[0]   = reverse_int32(pBuf->buf.phyaddr);
		pHeader->readPtr[1]   = reverse_int32(pBuf->buf.phyaddr);
		pHeader->reserve3     = 0;
	} else {
		pHeader->beginAddr    = pBuf->buf.phyaddr;
		pHeader->size         = pBuf->buf.bufSize;
		pHeader->bufferID     = type;
		pHeader->writePtr     = pBuf->buf.phyaddr;
		pHeader->numOfReadPtr = 1;
		pHeader->readPtr[0]   = pBuf->buf.phyaddr;
		pHeader->readPtr[1]   = pBuf->buf.phyaddr;
		pHeader->reserve3     = 0;
	}
}


int dmx_buf_AllocBuffer(struct BUF_INFO *pBuf, int size, int isCache, int useZone)
{
	void *cached,*uncached, *virAddr;
	dma_addr_t phyAddr;

	if (pBuf == 0)
		return -1;

	if (isCache){
		cached = (u8 *)dvr_malloc_specific(size, 
			useZone ? GFP_DCU1 : GFP_DCU2_FIRST);
		virAddr = cached;
	} else {
		cached = dvr_malloc_uncached_specific(size, 
			useZone ? GFP_DCU1 : GFP_DCU2_FIRST, &uncached);
		virAddr = uncached;
	}
	if (cached == 0)
		return -1;

	phyAddr                   = dvr_to_phys(cached);
	pBuf->phyaddr             = phyAddr;
	pBuf->viraddr             = virAddr;
	pBuf->cacheaddr           = cached;
	pBuf->offsetToViraddr     = (u32)virAddr - phyAddr;
	pBuf->bufSize             = size;
	pBuf->pSyncWritePtr       = virAddr;
	pBuf->iscache             = isCache;
	pBuf->buftype             = CMA_BUF;

	return 0;
}

int dmx_buf_FreeBuffer(struct BUF_INFO *pBuf)
{
	if (pBuf && pBuf->cacheaddr)
		dvr_free(pBuf->cacheaddr);
	else
		BUG();
	memset(pBuf, 0, sizeof(struct BUF_INFO));
	return 0;	
}


int dmx_buf_ta_map_ex(unsigned char idx, unsigned char type, struct BUF_INFO *pBuf)
{
	return dmx_ta_buf_map(idx, type, pBuf->phyaddr, pBuf->bufSize, 0, 0, 0 ,0, 0 ,0);
}