
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/dma-mapping.h>
#include <asm/outercache.h>

#include <rdvb-common/misc.h>
#include "rtk_vdec.h"
#include "dmx_buf_misc.h"
#include "dmx_buf.h"
#include <rdvb-common/dmx_re_ta.h>
extern void rtk_flush_range (const void *, const void *);
extern void rtk_inv_range (const void *, const void *);

#define WITH_SVP
#define VTP_ES_BUFFER_SIZE (8*1024*1024)
#define VTP_IB_BUFFER_SIZE (256*1024)
#define ATP_ES_BUFFER_SIZE (256*1024)
#define ATP_IB_BUFFER_SIZE (256*1024)

#define PES_ES_BUFFER_SIZE (64*1024)

#define DCU1 1
#define DCU2 0

static struct BUF_INFO g_buffer_poll;
unsigned int g_buffer_poll_allocated_size = 0;

int dmx_buf_poll_setup(struct BUF_INFO **pBuf,
		unsigned int size, unsigned int nr_buf_header)
{
	unsigned int poll_size = 0;
	poll_size = size + nr_buf_header*((sizeof(RINGBUFFER_HEADER)+0xf) & ~0xf);
	dmx_notice(NOCH, "poll: p1:0x%x, p2:%d(buf_head), size:0x%x\n",
			size, nr_buf_header, poll_size);
	if (poll_size  == 0) 
		return 0;
	if (dmx_buf_AllocBuffer(&g_buffer_poll, poll_size,
			NON_CACHED_BUFF, DCU1) < 0) {
		dmx_err(NOCH,"BUF: ERROR: fail to allocate buf poll\n");
		return -1;
	}
	g_buffer_poll_allocated_size = 0;
	*pBuf = &g_buffer_poll;
	return 0;
}

int dmx_buf_poll_alloc(struct BUF_INFO *pBuf, unsigned int size)
{
	if (pBuf == NULL ||
		g_buffer_poll_allocated_size+size > g_buffer_poll.bufSize) {
		dmx_err(NOCH,"ERROR buf:%p, %d+%d==>%d \n",
			 pBuf, g_buffer_poll_allocated_size,
			size, g_buffer_poll.bufSize);
		return -1;
	}
	memcpy(pBuf, &g_buffer_poll, sizeof(struct BUF_INFO));

	pBuf->phyaddr        += g_buffer_poll_allocated_size;
	pBuf->bufSize         = size;
	pBuf->viraddr        += g_buffer_poll_allocated_size;
	pBuf->cacheaddr      += g_buffer_poll_allocated_size;
	pBuf->buftype         = POLL_BUF;

	g_buffer_poll_allocated_size += ((size+0xf) & ~0xf);
	memset(pBuf->viraddr, 0, size);
	dmx_notice(NOCH, "dmx poll usage:(0x%x)0x%x\n", size, g_buffer_poll_allocated_size);
	return 0;
}

static int dmx_buf_cma_alloc(struct BUF_INFO_T *pBuf, unsigned int size,
	RINGBUFFER_TYPE  buff_type, bool is_cache)
{
	if (pBuf == NULL) {
		dmx_err(NOCH,"BUF: invalid param\n");
		return -1;

	}
	if (pBuf->buf.phyaddr){
		return 0;
	}
	if (pBuf->buf.phyaddr == 0) {
		//startTime = CLOCK_GetPTS();
		if (dmx_buf_AllocBuffer(&pBuf->buf, size, 
			is_cache?CACHED_BUFF:NON_CACHED_BUFF, DCU1) < 0) {
			dmx_err(NOCH,"BUF: FATAL ERROR: allocate buffer\n");
			BUG();
			return -1;
		}
		//dmx_err(NOCH,"BUF: alloc ES buffer(0x%x)\n",
		//	  pBuf->buf.phyaddr);
	}
	if (pBuf->bufHeader.viraddr == NULL) {
		if (dmx_buf_poll_alloc(&pBuf->bufHeader,
				sizeof(RINGBUFFER_HEADER)) < 0) {
			dmx_err(NOCH,"BUF: FATAL ERROR: alloc buffer header\n");
			BUG();
			return -1;
		}
	}
	dmx_buf_ResetBufHeader(pBuf, buff_type,  1);
	//dmx_err(NOCH,"[BUF]: BsBufferHeader:0x%x\n", 
	//	 pBuf->bufHeader.phyaddr);
	return 0;
}
static int dmx_buf_cma_free(struct BUF_INFO_T *pBuf, RINGBUFFER_TYPE  buff_type)
{

	if (pBuf==NULL){
		dmx_err(NOCH,"BUF: invalid param\n");
		return -1;
	}
	if (pBuf->buf.phyaddr == 0){
		dmx_err(NOCH,"BUF: invalid param\n");
		return 0;
	}

	
	if (dmx_buf_FreeBuffer(&pBuf->buf) < 0 )
		return -1;
	dmx_buf_ResetBufHeader(pBuf, buff_type,  1);
	return 0;
}

int dmx_buf_alloc_tp_main(unsigned char index, struct BUF_INFO* buf)
{
	//#define _4k_6k_align(x)  (((x+2*6*1024 -1)/(2*6*1024))*(2*6*1024))
	//#define _4k_6k_align_remain(x) ((x)%(2*6*1024)) 
	unsigned int caved_buf_size = 0;
	unsigned int caved_buf_phy = 0;
	unsigned int buf_1=0, buf_2=0, buf_1_sz=0, buf_2_sz=0;
	if (index >=2 ){
		dmx_err(NOCH,"BUF ERROR wrong main tp index [%d]\n", index);
		return -1;
	}
	caved_buf_size = carvedout_buf_query(CARVEDOUT_TP,(void**)&caved_buf_phy);
	if (caved_buf_size <= 0){
		dmx_err(NOCH,"BUF ERROR tpbuf:%d [%d]\n", index, caved_buf_size);
		return -1;
	}
	buf_1 = __align_down_4k_6k(caved_buf_phy);
	buf_1_sz = (caved_buf_size - __align_down_extend_4k_6k(caved_buf_phy)) >> 1;

	buf_2 = buf_1+ buf_1_sz;
	buf_2 = __align_down_4k_6k(buf_2);
	buf_1_sz =  buf_2 - buf_1;
	buf_2_sz = (caved_buf_phy+ caved_buf_size - buf_2) &(~0xfff);

	dmx_err(NOCH, "TP Caved (0x%x/0x%x) allocate: 0x%x/0x%x, 0x%x/0x%x\n",
				caved_buf_phy,caved_buf_size, buf_1, buf_1_sz, buf_2, buf_2_sz);
	/*
	buf->phyaddr = caved_buf_phy+ index* TP_MAIN_BUFF_SIZE;
	if (buf->phyaddr+ TP_MAIN_BUFF_SIZE > caved_buf_phy+ caved_buf_size){
		dmx_err(NOCH,"BUF ERROR tpbuf:%d [0x%x/0x%x, 0x%x/0x%x ]\n",
			index, buf->phyaddr,TP_MAIN_BUFF_SIZE,
			caved_buf_phy, caved_buf_size);
		return -1;
	}
	*/
	if (index == 0) {
		buf->phyaddr = buf_1;
		buf->bufSize = buf_1_sz;
	} else {
		buf->phyaddr = buf_2;
		buf->bufSize = buf_2_sz;
	}
	buf->iscache = false;
	buf->cacheaddr = 0;
	buf->viraddr =  dvr_remap_uncached_memory(buf->phyaddr, buf->bufSize ,
					 __builtin_return_address(0));
	if(buf->viraddr == NULL) {
		dmx_err(NOCH, "ch:%d: map tp buffer(0x%x/0x%x) fail\n",
			index,buf->phyaddr, buf->bufSize);
		return -1;
	}
	buf->offsetToViraddr = (long)buf->viraddr - buf->phyaddr;
	return 0;
}

int dmx_buf_alloc_tp_sub(unsigned char index, struct BUF_INFO* buf)
{
	if (dmx_buf_AllocBuffer(buf, TP_SUB_BUFF_SIZE,
			NON_CACHED_BUFF, DCU2) < 0) {
		dmx_err(NOCH,"BUF: FATAL ERROR: fail to alloc TP(%d) buffer \n", index);
		BUG();
		return -1;
	}
	return 0;
}
int dmx_buf_vtp_bs_alloc(struct BUF_INFO_T * pBuf)
{
	struct BUF_INFO *buf=NULL;
	struct BUF_INFO *bufheader=NULL;
	if (pBuf == NULL) {
		dmx_err(NOCH,"BUF: invalid param\n");
		return -1;

	}
	if (pBuf->buf.phyaddr){
		//dmx_err(NOCH,"BUF: invalid param\n");
		return 0;
	}
	buf = &pBuf->buf;
	bufheader = &pBuf->bufHeader;
	#ifdef WITH_SVP
	buf->phyaddr = rtkvdec_cobuffer_alloc(VDEC_SVP_BUF, 0xff);
	//dmx_err(NOCH,"BUF: borrow SVPbuffer(0x%x)\n", buf->phyaddr);
	#endif
	if (buf->phyaddr == 0) {
		dmx_err(NOCH, "BUF: notice : fail to borrow SVP buffer\n");
		if (dmx_buf_AllocBuffer(buf, VTP_ES_BUFFER_SIZE,
			 CACHED_BUFF, DCU1) < 0) {
			dmx_err(NOCH,"BUF: FATAL ERROR: allocate video ES buffer\n");
			BUG();
			return -1;
		}
		dmx_err(NOCH,"BUF: alloc ES buffer(0x%x)\n",buf->phyaddr);
	} else {
		buf->bufSize = RTKVDEC_SVPMEM_SIZE_8M;
		//buf->allocSize = 0; //it means memory is from SVP.
		buf->viraddr = dvr_remap_cached_memory(buf->phyaddr,
			buf->bufSize, __builtin_return_address(0));
		buf->offsetToViraddr =(long)buf->viraddr - buf->phyaddr;
		buf->pSyncWritePtr = buf->viraddr;
		buf->iscache = true;
		buf->buftype = SVP_BUF;
	}
	if (bufheader->viraddr == NULL) {
		if (dmx_buf_poll_alloc(bufheader, sizeof(RINGBUFFER_HEADER)) < 0) {
			dmx_err(NOCH,"BUF: FATAL ERROR: allocate video ES buffer\n");
			BUG();
			return -1;
		}
	}
	dmx_buf_ResetBufHeader(pBuf, RINGBUFFER_STREAM,  1);
	//dmx_err(NOCH,"[BUF]: videoBsBufferHeader:0x%x\n", bufheader->phyaddr);
	return 0;
}

int dmx_buf_vtp_ib_alloc(struct BUF_INFO_T * pBuf)
{
	struct BUF_INFO *buf=NULL;
	struct BUF_INFO *bufheader=NULL;
	if (pBuf == NULL) {
		dmx_err(NOCH,"BUF: invalid param\n");
		return -1;

	}
	if (pBuf->buf.phyaddr){
		//dmx_err(NOCH,"BUF: invalid param\n");
		return 0;
	}
	buf = &pBuf->buf;
	bufheader = &pBuf->bufHeader;
	#ifdef WITH_SVP
	buf->phyaddr = rtkvdec_cobuffer_alloc(VDEC_INBAND_BUF, 0xff);
	//dmx_err(NOCH,"BUF: borrow SVPbuffer(0x%x)\n",
	//	 buf->phyaddr);
	#endif
	if (buf->phyaddr == 0) {
		if (dmx_buf_AllocBuffer(buf, VTP_IB_BUFFER_SIZE,
			 CACHED_BUFF, DCU1) < 0) {
			dmx_err(NOCH,"BUF: FATAL ERROR: allocate video ES buffer\n");
			BUG();
			return -1;
		}
	} else {
		buf->bufSize = RTKVDEC_IBBUF_SIZE;
		//buf->allocSize = 0; //it means memory is from SVP.
		buf->viraddr = dvr_remap_cached_memory(buf->phyaddr,
			 buf->bufSize, __builtin_return_address(0));
		buf->offsetToViraddr = (long)buf->viraddr - buf->phyaddr;
		buf->pSyncWritePtr = buf->viraddr;
		buf->iscache = true;
		buf->buftype = SVP_BUF;
	}
	if (bufheader->viraddr == NULL) {
		if (dmx_buf_poll_alloc(bufheader, sizeof(RINGBUFFER_HEADER)) < 0) {
			dmx_err(NOCH,"BUF: FATAL ERROR: allocate video ES buffer\n");
			BUG();
			return -1;
		}
	}
	dmx_buf_ResetBufHeader(pBuf, RINGBUFFER_COMMAND,  1);
	//dmx_err(NOCH,"[BUF]: videoibBufferHeader:0x%x\n", 
	//	 bufheader->phyaddr);
	return 0;
}

int dmx_buf_vtp_bs_free(struct BUF_INFO_T * pBuf)
{
	if (pBuf==NULL)
		return -1;
	if (pBuf->buf.phyaddr == 0)
		return 0;
	if (pBuf->buf.buftype == SVP_BUF) {
		rtkvdec_cobuffer_free(VDEC_SVP_BUF, pBuf->buf.phyaddr);
		dvr_unmap_memory(pBuf->buf.viraddr, pBuf->buf.bufSize);
	} else {
		if (pBuf->buf.viraddr)
			dvr_free(pBuf->buf.cacheaddr);
		else
			BUG();
	}
	memset(&pBuf->buf, 0, sizeof(struct BUF_INFO));
	dmx_buf_ResetBufHeader(pBuf, RINGBUFFER_STREAM,  1);
	return 0;
}

int dmx_buf_vtp_ib_free(struct BUF_INFO_T * pBuf)
{
	if (pBuf==NULL)
		return -1;
	if (pBuf->buf.phyaddr == 0)
		return 0;
	if (pBuf->buf.buftype == SVP_BUF) {
		rtkvdec_cobuffer_free(VDEC_INBAND_BUF, pBuf->buf.phyaddr);
		dvr_unmap_memory(pBuf->buf.viraddr, pBuf->buf.bufSize);
	} else {
		if (pBuf->buf.viraddr)
			dvr_free(pBuf->buf.cacheaddr);
		else
			BUG();
	}
	memset(&pBuf->buf, 0, sizeof(struct BUF_INFO));
	dmx_buf_ResetBufHeader(pBuf, RINGBUFFER_COMMAND,  1);
	return 0;
}

int dmx_buf_atp_bs_alloc(struct BUF_INFO_T * pBuf)
{
	return dmx_buf_cma_alloc(pBuf, ATP_ES_BUFFER_SIZE, RINGBUFFER_STREAM, true);
}

int dmx_buf_atp_ib_alloc(struct BUF_INFO_T * pBuf)
{
	return dmx_buf_cma_alloc(pBuf, ATP_IB_BUFFER_SIZE, RINGBUFFER_COMMAND, true);
}

int dmx_buf_atp_bs_free(struct BUF_INFO_T * pBuf)
{
	return dmx_buf_cma_free(pBuf, RINGBUFFER_STREAM);
}

int dmx_buf_atp_ib_free(struct BUF_INFO_T * pBuf)
{
	return dmx_buf_cma_free(pBuf, RINGBUFFER_COMMAND);
}

int dmx_buf_pes_bs_alloc(struct BUF_INFO_T *pBuf)
{
	return dmx_buf_cma_alloc(pBuf, PES_ES_BUFFER_SIZE, RINGBUFFER_STREAM, true);
}

int dmx_buf_pes_bs_free(struct BUF_INFO_T *pBuf)
{
	return dmx_buf_cma_free(pBuf, RINGBUFFER_STREAM);	
}

static unsigned int dmx_buf_get_syncsize(struct BUF_INFO_T* pBuf)
{
	int size;
	u8 *pWP;
	RINGBUFFER_HEADER * pHeader;
	if (pBuf == 0)
		return 0;

	pHeader = (RINGBUFFER_HEADER *)pBuf->bufHeader.viraddr;
	if (pHeader == NULL)
		return 0;
	pWP = (u8*)reverse_int32(pHeader->writePtr) + pBuf->buf.offsetToViraddr;
	size = pBuf->buf.pSyncWritePtr - pWP;
	if (size < 0)
		size += pBuf->buf.bufSize;
	/*
	dmx_err(NOCH,"BUF: syncsize(:%d (%p, %p)\n", 
		size, pWP, pBuf->buf.pSyncWritePtr);*/
	return size;
}

/*NOT include non-sync zone*/
static unsigned int dmx_buf_get_fullness_ex(struct BUF_INFO_T *pBuf)
{
	int fullness;
	RINGBUFFER_HEADER * pHeader;
	if (pBuf == NULL)
		return 0;
	pHeader = (RINGBUFFER_HEADER *)pBuf->bufHeader.viraddr;
	fullness = reverse_int32(pHeader->writePtr) 
			- reverse_int32(pHeader->readPtr[0]);
	if (fullness < 0)
		fullness += pBuf->buf.bufSize;

	return fullness;
}
/*include non-sync zone*/
static unsigned int dmx_buf_get_fullness(struct BUF_INFO_T *pBuf)
{
	int fullness;
	fullness = dmx_buf_get_fullness_ex(pBuf);

	fullness += dmx_buf_get_syncsize(pBuf);
	//dmx_err(NOCH,"BUF: fullness:%d \n", fullness);
	return fullness;
}


/* real availablesize, */
unsigned int dmx_buf_get_availableSize(struct BUF_INFO_T *pBuf)
{
	if (pBuf == 0)
		return 0;

	return pBuf->buf.bufSize - dmx_buf_get_fullness(pBuf) - 1;
}

static void dmx_buf_flush_cache(u8 *pSrc, phys_addr_t srcPhy, int bytes)
{
	rtk_flush_range(pSrc, pSrc + bytes) ;
	outer_flush_range(srcPhy, srcPhy + bytes) ;
}

static void dmx_buf_cache_invalid(u8 *pSrc, phys_addr_t srcPhy, int bytes)
{
	rtk_inv_range(pSrc, pSrc + bytes);
	outer_inv_range(srcPhy, srcPhy + bytes);
}

int dmx_buf_write(struct BUF_INFO_T * pBuf, void *pData, unsigned int dataSize)
{
	u32 pUpper;
	u32 pLower;
	u8* pWP_v;
	phys_addr_t phyWP;
	RINGBUFFER_HEADER * pHeader;

	if (dmx_buf_get_availableSize(pBuf)  < dataSize){
		return -1;
	}

	pHeader = (RINGBUFFER_HEADER *)pBuf->bufHeader.viraddr;
	pLower  = reverse_int32(pHeader->beginAddr);
	pUpper = pLower + reverse_int32(pHeader->size);
	//phyWP = TP_MGR_reverseInteger(pBufInfo->pHeader->writePtr);
	pWP_v = pBuf->buf.pSyncWritePtr;//(u8*)phyWP + pBuf->offsetToCached;
	phyWP = (u32)pWP_v - pBuf->buf.offsetToViraddr;
	/*
	dmx_err(NOCH,"BUF: write:%p, %x, size:%d\n",
		pWP_v, phyWP, dataSize);
	*/
	if (phyWP + dataSize > pUpper)
	{
		int size = pUpper - phyWP;
		memcpy(pWP_v, pData, size);

		phyWP = pLower;
		pWP_v = (u8*)pLower + pBuf->buf.offsetToViraddr;
		dataSize -= size;
		pData += size;
	}

	memcpy(pWP_v, pData, dataSize);

	pWP_v += dataSize;
	phyWP += dataSize;
	if (phyWP >= pUpper)
		pWP_v -= pBuf->buf.bufSize;

	pBuf->buf.pSyncWritePtr = pWP_v;
	/*dmx_err(NOCH,"BUF: write:%p, %x, size:%d\n",
		pWP_v, phyWP, dataSize);*/
	return 0;
}

static void dmx_buf_IB_data_write(u8* des, u8* src, unsigned long size)
{
	unsigned long i ;
	u32 *psrc, *pdes ;

	if ((((long)src & 0x3) != 0) ||
		(((long)des & 0x3) != 0) ||
		((size & 0x3) != 0))
		dmx_err(NOCH,"error in dmx_buf_IB_data_write()...\n") ;

	for (i = 0 ; i < size ; i += 4) {
		psrc = (u32 *)&src[i];
		pdes = (u32 *)&des[i];
		*pdes = reverse_int32(*psrc) ;
		/*        dmx_err(NOCH,"%x, %x...\n", src[i], des[i]);*/
	}

}

int dmx_buf_writeIB(struct BUF_INFO_T * pBuf, void *pData, unsigned int dataSize)
{
	u32 pUpper;
	u32 pLower;
	u8* pWP_v;
	phys_addr_t phyWP;
	RINGBUFFER_HEADER * pHeader;

	if (dmx_buf_get_availableSize(pBuf)  < dataSize){
		//dmx_err(NOCH,"BUF: dataDrop--->>>>>>> \n", );
		return -1;
	}

	pHeader = (RINGBUFFER_HEADER *)pBuf->bufHeader.viraddr;
	pLower  = reverse_int32(pHeader->beginAddr);
	pUpper = pLower + reverse_int32(pHeader->size);
	//phyWP = TP_MGR_reverseInteger(pBufInfo->pHeader->writePtr);
	pWP_v = pBuf->buf.pSyncWritePtr;//(u8*)phyWP + pBuf->offsetToCached;
	phyWP = (u32)pWP_v - pBuf->buf.offsetToViraddr;
	/*
	dmx_err(NOCH,"BUF: write:%p, %x, size:%d\n",
		pWP_v, phyWP, dataSize);
	*/
	if (phyWP + dataSize > pUpper)
	{
		int size = pUpper - phyWP;
		//memcpy(pWP_v, pData, size);
		dmx_buf_IB_data_write(pWP_v, pData, size);
		phyWP = pLower;
		pWP_v = (u8*)pLower + pBuf->buf.offsetToViraddr;
		dataSize -= size;
		pData += size;
	}

	//memcpy(pWP_v, pData, dataSize);
	dmx_buf_IB_data_write(pWP_v, pData, dataSize);

	pWP_v += dataSize;
	phyWP += dataSize;
	if (phyWP >= pUpper)
		pWP_v -= pBuf->buf.bufSize;

	pBuf->buf.pSyncWritePtr = pWP_v;
	/*
	dmx_err(NOCH,"BUF: write:%p, %x, size:%d\n",
		pWP_v, phyWP, dataSize);
	*/
	return 0;
}

int dmx_buf_sync(struct BUF_INFO_T * pBuf)
{
	u32 pUpper;
	u32 pLower;
	u32 pWP;
	phys_addr_t phyWP;
	RINGBUFFER_HEADER * pHeader;

	int syncSize = dmx_buf_get_syncsize(pBuf);
	if (syncSize == 0)
		return 0;

	pHeader = (RINGBUFFER_HEADER *)pBuf->bufHeader.viraddr;
	pLower  = reverse_int32(pHeader->beginAddr);
	pUpper  = pLower + reverse_int32(pHeader->size);
	phyWP   = reverse_int32(pHeader->writePtr);
	pWP     = phyWP ;
	/*
	dmx_err(NOCH,"BUF: start sync: write:%d,  size:%d\n",
		pWP, syncSize);
	*/

	if (phyWP + syncSize > pUpper) {
		int size = pUpper - phyWP;
		dmx_buf_flush_cache((u8*)pWP+pBuf->buf.offsetToViraddr, pWP , size);

		pWP = pLower;
		syncSize -= size;
	}
	dmx_buf_flush_cache((u8*)pWP+pBuf->buf.offsetToViraddr, pWP, syncSize);

	__sync_synchronize();
	pHeader->writePtr = reverse_int32
		((u32)pBuf->buf.pSyncWritePtr - pBuf->buf.offsetToViraddr);
	/*
	dmx_err(NOCH,"BUF:after sync : write:%d, \n",
		reverse_int32(pHeader->writePtr));
		*/
	return 0;
}
//only for pes 
int dmx_buf_write_rollback(struct BUF_INFO_T * pBuf)
{
	RINGBUFFER_HEADER * pHeader;
	if (pBuf == NULL) {
		dmx_err(NOCH,"BUF: ERROR: invalid param\n");
		return -1;
	}
	pBuf->buf.pSyncWritePtr = pBuf->buf.viraddr;
	pHeader = (RINGBUFFER_HEADER *)pBuf->bufHeader.viraddr;
	pHeader->writePtr = pHeader->readPtr[0] = pHeader->beginAddr;
	return 0;
}

int dmx_buf_transpes(struct BUF_INFO_T *pBuf, ts_callback callback, void * filter)
{
	unsigned int size, size0, size1;
	u32 pUpper;
	u32 pLower;
	phys_addr_t phyWP, phyRP;
	RINGBUFFER_HEADER * pHeader;
	void * buf0, *buf1;
	if (pBuf == NULL || callback == NULL || filter == NULL){
		
		return -1;
	}
	size = dmx_buf_get_fullness_ex(pBuf);
	if (size <= 0) {
		dmx_err(NOCH,"BUF ERROR: @@@@#### size:%d \n", size);
		return -1;
	}


	pHeader = (RINGBUFFER_HEADER *)pBuf->bufHeader.viraddr;
	pLower  = reverse_int32(pHeader->beginAddr);
	pUpper  = pLower + reverse_int32(pHeader->size);
	phyWP   = reverse_int32(pHeader->writePtr);
	phyRP   = reverse_int32(pHeader->readPtr[0]);

	buf0 = (void*)phyRP + pBuf->buf.offsetToViraddr;
	buf1 = NULL;
	size0 = size;
	size1 = 0;
	if (phyRP >phyWP) {
		size0 =  pUpper - phyRP;
		buf1 = (void*)pLower + pBuf->buf.offsetToViraddr;
		size1 = phyWP - pLower;
	}
	if (callback(buf0, size0, buf1, size1,filter) < 0) {
		dmx_err(NOCH,"BUF: callback fail\n");
		pHeader->readPtr[0] = pHeader->writePtr;
		return -1;
	}
	//dmx_err(NOCH,"BUF: pes callback finish:size:%d\n", size);
	pHeader->readPtr[0] = pHeader->writePtr;
	return 0;
}

int dmx_buf_ta_map(unsigned char idx, unsigned char type,
					struct BUF_INFO_T *es, struct BUF_INFO_T *ib)
{
	
	dmx_mask_print(DMX_ECP1_DBG, idx,
			"bs:phyaddr:0x%x, viraddr:%p, syncWp:%p, wp:0x%x\n"
			"ib:phyaddr:0x%x, viraddr:%p, syncWp:%p, wp:0x%x\n",
			es->buf.phyaddr, es->buf.viraddr, es->buf.pSyncWritePtr,
			reverse_int32(((RINGBUFFER_HEADER *)es->bufHeader.viraddr)->writePtr),
			ib->buf.phyaddr, ib->buf.viraddr, ib->buf.pSyncWritePtr,
			reverse_int32(((RINGBUFFER_HEADER *)ib->bufHeader.viraddr)->writePtr));
	return dmx_ta_buf_map(idx, type,
						  es->buf.phyaddr, es->buf.bufSize, 
						  es->bufHeader.phyaddr, es->bufHeader.bufSize,
						  ib->buf.phyaddr, ib->buf.bufSize, 
						  ib->bufHeader.phyaddr, ib->bufHeader.bufSize);
}
static void dmx_buf_ta_unmap_sync_wp(struct BUF_INFO_T *buf)
{
	phys_addr_t phyWP;
	RINGBUFFER_HEADER * pHeader;

	if (buf->buf.phyaddr == 0)
		return;
	if (buf->bufHeader.phyaddr == 0)
		return;
	pHeader = (RINGBUFFER_HEADER *)buf->bufHeader.viraddr;
	phyWP   = reverse_int32(pHeader->writePtr);
	buf->buf.pSyncWritePtr = (u8*)(phyWP + buf->buf.offsetToViraddr);

	dmx_mask_print(DMX_ECP1_DBG, NOCH,
			"phyaddr:0x%x, viraddr:%p, wp:0x%x, syncWp:%p\n",
			buf->buf.phyaddr, buf->buf.viraddr, phyWP, buf->buf.pSyncWritePtr);
}

int dmx_buf_ta_unmap(unsigned char idx, unsigned char type,
					struct BUF_INFO_T *es, struct BUF_INFO_T *ib)
{
	dmx_ta_buf_unmap(idx, type);
	dmx_buf_ta_unmap_sync_wp(es);
	dmx_buf_ta_unmap_sync_wp(ib);
	return 0;
}

int dmx_buf_ta_invalid(struct BUF_INFO_T *buf)
{
	u32 pUpper;
	u32 pLower;
	u32 pRP;
	phys_addr_t phyRP, phyWP;
	RINGBUFFER_HEADER * pHeader;

	pHeader = (RINGBUFFER_HEADER *)buf->bufHeader.viraddr;
	pLower  = reverse_int32(pHeader->beginAddr);
	pUpper  = pLower + reverse_int32(pHeader->size);
	phyRP   = reverse_int32(pHeader->readPtr[0]);
	phyWP   = reverse_int32(pHeader->writePtr);
	pRP     = phyRP ;

	if (pRP > phyWP) {
		pRP = pLower;
		dmx_buf_cache_invalid((u8*)pRP+buf->buf.offsetToViraddr,pRP, pUpper - pRP);
	}

	dmx_buf_cache_invalid((u8*)pRP+buf->buf.offsetToViraddr,pRP, phyWP - pRP);
	return 0;
}