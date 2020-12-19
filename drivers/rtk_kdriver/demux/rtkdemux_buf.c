#include <generated/autoconf.h>
#include <linux/uaccess.h>
#include <asm/cacheflush.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pageremap.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/string.h>
#ifdef CONFIG_LG_SNAPSHOT_BOOT
    #include <linux/suspend.h>
#endif
#include "rtkdemux.h"
#include "rtkdemux_buf.h"
#include "rtkdemux_export.h"
#include "rtkdemux_debug.h"
#include "rtkdemux_specialhandling.h"
#include "rtkdemux_re.h"
#include <rtk_kdriver/rtk_vdec.h>

#ifdef USE_STATIC_RESERVED_BUF
#include <mach/rtk_platform.h>
#endif
void DEMUX_BUF_ResetRBHeader(DEMUX_BUF_T *pBufH, DEMUX_BUF_T *pBuf, RINGBUFFER_TYPE type, unsigned char reverse)
{
	RINGBUFFER_HEADER *pHeader = NULL;
	if (pBufH == NULL)
		return;
	pHeader = (RINGBUFFER_HEADER *)pBufH->virAddr;
	if (pBuf == NULL){
#ifdef CONFIG_ARM64
		memset_io(pHeader, 0 , sizeof(RINGBUFFER_HEADER));
#else
		memset(pHeader, 0 , sizeof(RINGBUFFER_HEADER));
#endif
		return;
	}
	if (reverse) {
		pHeader->beginAddr    = DEMUX_reverseInteger(pBuf->phyAddr);
		pHeader->size         = DEMUX_reverseInteger(pBuf->size);
		pHeader->bufferID     = DEMUX_reverseInteger(type);
		pHeader->writePtr     = DEMUX_reverseInteger(pBuf->phyAddr);
		pHeader->numOfReadPtr = DEMUX_reverseInteger(1);
		pHeader->readPtr[0]   = DEMUX_reverseInteger(pBuf->phyAddr);
		pHeader->readPtr[1]   = DEMUX_reverseInteger(pBuf->phyAddr);
	} else {
		pHeader->beginAddr    = pBuf->phyAddr;
		pHeader->size         = pBuf->size;
		pHeader->bufferID     = type;
		pHeader->writePtr     = pBuf->phyAddr;
		pHeader->numOfReadPtr = 1;
		pHeader->readPtr[0]   = pBuf->phyAddr;
		pHeader->readPtr[1]   = pBuf->phyAddr;
	}
}

int DEMUX_BUF_AllocBuffer(DEMUX_BUF_T *pBuf, int size, int isCache, int useZone)
{
	isCache = demux_device->disableCache ? 0 : isCache ;
	pBuf->allocSize = size < 0x1000 ? 0x1000 : size ;
	pBuf->nonCachedaddr = 0;
	if (isCache)
		pBuf->cachedaddr = (UINT8 *)dvr_malloc_specific(pBuf->allocSize, useZone ? GFP_DCU1 : GFP_DCU2_FIRST);
	else {
		void* uncached;

		pBuf->cachedaddr = (UINT8 *)dvr_malloc_uncached_specific(
			pBuf->allocSize,
			useZone ? GFP_DCU1 : GFP_DCU2_FIRST,
			&uncached
		);

		pBuf->nonCachedaddr = (unsigned char *)uncached;
	}

	if (!(pBuf->cachedaddr)) {
		dmx_err(CHANNEL_UNKNOWN, "RTKDEMUX:  cannot allocate memory by using dvr_malloc_uncached_specific. size = %x\n", size) ;
		return -EFAULT ;
	} else {
		pBuf->phyAddr = dvr_to_phys(pBuf->cachedaddr) ;
		pBuf->virAddr = isCache ? pBuf->cachedaddr : pBuf->nonCachedaddr ;
		pBuf->isCache = isCache ;
	}
	pBuf->size = size ;
	pBuf->fromPoll = 0 ;

#ifdef CONFIG_LG_SNAPSHOT_BOOT
	register_cma_forbidden_region(__phys_to_pfn(pBuf->phyAddr), size);
#endif

	dmx_mask_print(DEMUX_BUFFER_DEBUG,	CHANNEL_UNKNOWN,"useZone cachedAddr %p, uncachedAddr %p, phyAddr %lx, allocSize %x\n",
		pBuf->cachedaddr, pBuf->nonCachedaddr, (ULONG)pBuf->phyAddr, pBuf->allocSize);
	return 0 ;
}
int DEMUX_BUF_FreeBuffer(DEMUX_BUF_T *pBuf)
{
	if (pBuf->cachedaddr && !pBuf->fromPoll) {
		dvr_free(pBuf->cachedaddr) ;
		memset(pBuf, 0, sizeof(DEMUX_BUF_T));
	}
	return 0 ;
}

int DEMUX_BUF_AllocFromPoll(DEMUX_BUF_T *pBuf, int size, DEMUX_BUF_T *pSrcBuf, int isCache)
{
	/*  return DEMUX_BUF_AllocBuffer(pBuf, size, isCache, 0) ;*/

	pBuf->allocSize = (size + 0xf) & ~0xf ; /*size < 0x1000 ? 0x1000 : size ;*/

	if (pBuf->allocSize + demux_device->usedPollSize > pSrcBuf->allocSize) {
		dmx_err(CHANNEL_UNKNOWN,"RTKDEMUX: cannot allocate memory from pool, pool size = %x, used size = %x, alloc size = %x\n", pSrcBuf->allocSize, demux_device->usedPollSize, size) ;
		return -EFAULT ;
	}
	pBuf->phyAddr       = (dma_addr_t)((unsigned int)pSrcBuf->phyAddr + demux_device->usedPollSize) ;
	pBuf->cachedaddr    = pSrcBuf->cachedaddr    + demux_device->usedPollSize ;
	pBuf->nonCachedaddr = pSrcBuf->nonCachedaddr + demux_device->usedPollSize ;
	pBuf->virAddr       = isCache ? pBuf->cachedaddr : pBuf->nonCachedaddr ;
	pBuf->isCache       = isCache ;
	pBuf->fromPoll = 1 ;
	pBuf->size       = size ;

	demux_device->usedPollSize += pBuf->allocSize ;

	dmx_mask_print(DEMUX_BUFFER_DEBUG, CHANNEL_UNKNOWN,"Poll: cachedAddr %p, uncachedAddr %p, phyAddr %lx, allocSize %x\n",
		pBuf->cachedaddr, pBuf->nonCachedaddr,	(ULONG)pBuf->phyAddr, pBuf->allocSize);
	return 0 ;
}

void DEMUX_Buf_SetPinInfo(DEMUX_BUF_PIN_T *pPinInfo, RINGBUFFER_HEADER *pRBH, DEMUX_BUF_T *pBuf)
{

	pPinInfo->pWrPtr		= (u32 *)&pRBH->writePtr;
	pPinInfo->pRdPtr		= (u32 *)&pRBH->readPtr[0];

	pPinInfo->pBufferLower  = (unsigned char *)(unsigned long)DEMUX_reverseInteger(pRBH->beginAddr);
	pPinInfo->pBufferUpper  = pPinInfo->pBufferLower + DEMUX_reverseInteger(pRBH->size);
	pPinInfo->phyAddrOffset = (long) pBuf->phyAddr - (long) pBuf->virAddr;
	pPinInfo->pRBH		  =  pRBH;
}
//#########################################################################################################################################
/* state transition :
              Add PID (in IOCTL)              Remove PID (in IOCTL)                            UpdatePinInfo (in demux thread)
             ------------------->             ----------------------------------->             ------------------------------->
BUF_STAT_DONE                    BUF_STAT_NEW                                     BUF_STAT_FREE                                BUF_STAT_DONE
             <-------------------             <----------------------------------              <------------------------------
             UpdatePinInfo (in demux thread)   Add PID and buffer is not released yet           Remove PID (in IOCTL)
*/
int DEMUX_BUF_PrepareVideoBuffer(demux_channel *pCh, DEMUX_PES_DEST_T dest)
{
	int retryCnt = 0;
	int port;
	DEMUX_BUF_T *pBuf = 0;
	int64_t startTime = 0;
	DEMUX_VTP_T *p_vtp = NULL;

	if (dest == DEMUX_DEST_VDEC0) port = 0;
	else if (dest == DEMUX_DEST_VDEC1) port = 1;
	else return -1;

	p_vtp = &demux_device->VTP_Info[port];
	pBuf = &p_vtp->bsBuf;
	if (pBuf->phyAddr) {
		/*buffer is not released yet. reuse it.
		 * Only need to flush buffer(reset buffer header)
		*/
		if (pCh->tunerHandle != p_vtp->SrcCh)
		{
			if (p_vtp->vBufStatus != BUF_STAT_FREE)
			{
				dmx_crit(pCh->tunerHandle, "%s_%d:BUF, vTP:%d,is race in status:%d\n", __func__, __LINE__, port, p_vtp->vBufStatus);
				return -1;
			}
			dmx_crit(pCh->tunerHandle, "%s_%d:BUF, vTP:%d, is reused  by DIFF channel :%d \n", __func__, __LINE__, port, pCh->tunerHandle);
			p_vtp->vBufStatus = BUF_STAT_REUSED_II;
			p_vtp->SrcCh = pCh->tunerHandle;
			return 0;
		}
		dmx_crit(pCh->tunerHandle, "%s_%d:BUF, vTP:%d, is reuseD by channel :%d \n", __func__, __LINE__, port, pCh->tunerHandle);
		p_vtp->vBufStatus = BUF_STAT_NEW;
		return 0;
	}
#ifdef WITH_SVP
	startTime = CLOCK_GetPTS();
	/* In case fail to borrow SVP buffer, maybe GStreamer release it soon.
	 * Therefore, retry at most 10 times.
	 */
	while (pBuf->phyAddr == 0 && retryCnt < 10)  {

#ifdef CONFIG_RTK_KDRV_VDEC
		pBuf->phyAddr = rtkvdec_cobuffer_alloc(VDEC_SVP_BUF, port);
		retryCnt++;
#else
		dmx_err(pCh->tunerHandle, "%s_%d: fail to use rtkvdec_cobuffer_alloc\n", __func__, __LINE__);
#endif
	}
	dmx_crit(pCh->tunerHandle, "[%s %d]CH%d it takes %lld(ticks) to borrow SVP[%d] buffer(0x%lx)\n", __func__, __LINE__, pCh->tunerHandle, CLOCK_GetPTS()-startTime, port, (ULONG)pBuf->phyAddr);
#endif
	if (pBuf->phyAddr == 0) {
		dmx_crit(pCh->tunerHandle, "[%s %d] notice : fail to borrow SVP buffer\n", __func__, __LINE__);
		startTime = CLOCK_GetPTS();
		if (DEMUX_BUF_AllocBuffer(pBuf, RTKVDEC_SVPMEM_SIZE_8M, 1, 1) < 0) {
			dmx_crit(pCh->tunerHandle, "[%s %d]CH%d fatal error: fail to allocate video ES buffer\n", __func__, __LINE__, pCh->tunerHandle);
			BUG();
			return -1;
		}
		dmx_crit(pCh->tunerHandle, "[%s %d]CH%d it takes %lld(ticks) to alloc ES[%d] buffer(0x%lx)\n", __func__, __LINE__, pCh->tunerHandle, CLOCK_GetPTS()-startTime, port, (ULONG)pBuf->phyAddr);
	} else {
		pBuf->size = RTKVDEC_SVPMEM_SIZE_8M;
		pBuf->allocSize = 0; //it means memory is from SVP.
		pBuf->cachedaddr = dvr_remap_cached_memory(pBuf->phyAddr, pBuf->size, __builtin_return_address(0));
		pBuf->nonCachedaddr = 0;
		pBuf->isCache = 1;
		pBuf->virAddr = pBuf->cachedaddr;
		pBuf->fromPoll = 0;
	}
	if (p_vtp->bsBufH.virAddr == NULL){
		if (DEMUX_BUF_AllocFromPoll(&p_vtp->bsBufH, sizeof(RINGBUFFER_HEADER), &demux_device->bufPoll, 0) < 0) {
			dmx_crit(pCh->tunerHandle, "[%s %d]CH%d fatal error : fail to allocate bs buffer header\n", __func__, __LINE__, pCh->tunerHandle);
			DEMUX_BUF_ReturnVideoBuffer(pCh, port);
			return -1;
		}
	}
	dmx_crit(pCh->tunerHandle, "[%s_%d]: ch_%d, videoBsBufferHeader:0x%lx\n", __func__, __LINE__, pCh->tunerHandle, (ULONG)p_vtp->bsBufH.phyAddr);
	DEMUX_BUF_ResetRBHeader(&p_vtp->bsBufH, pBuf, RINGBUFFER_STREAM,  1);

	/* If we can borrow SVP buffer, inband buffer is, too.
	 */
	pBuf = &p_vtp->ibBuf;
#ifdef WITH_SVP
	if (pBuf->phyAddr == 0) {
		startTime = CLOCK_GetPTS();
#ifdef CONFIG_RTK_KDRV_VDEC
		pBuf->phyAddr = rtkvdec_cobuffer_alloc(VDEC_INBAND_BUF, port);
		dmx_crit(pCh->tunerHandle, "[%s %d]CH%d it takes %lld(ticks) to borrow IB[%d] buffer(0x%lx)\n", __func__, __LINE__, pCh->tunerHandle, CLOCK_GetPTS()-startTime, port, (ULONG)pBuf->phyAddr);
#else
		dmx_err(pCh->tunerHandle, "[%s %d] fail to allocate inband buf\n", __func__, __LINE__);

		return -1;
#endif
	}
#endif
	if (pBuf->phyAddr == 0) {
		dmx_crit(pCh->tunerHandle, "[%s %d]CH%d notice : fail to borrow inband buffer\n", __func__, __LINE__, pCh->tunerHandle);
		startTime = CLOCK_GetPTS();
		/* inband buffer is mapped to noncached area because we don't flush cache before update wp. */
		if (DEMUX_BUF_AllocBuffer(pBuf, RTKVDEC_IBBUF_SIZE, 0, 1) < 0) {
			dmx_crit(pCh->tunerHandle, "[%s %d]CH%d fatal error: fail to allocate video IB buffer\n", __func__, __LINE__, pCh->tunerHandle);
			BUG();
			return -1;
		}

		dmx_crit(pCh->tunerHandle, "[%s %d]CH%d it takes %lld(ticks) to alloc IB[%d] buffer(0x%lx)\n", __func__, __LINE__, pCh->tunerHandle, CLOCK_GetPTS()-startTime, port, (ULONG)pBuf->phyAddr);
	} else {
		pBuf->size = RTKVDEC_IBBUF_SIZE;
		pBuf->allocSize = 0; //it means memory is from SVP.
		pBuf->cachedaddr = 0;
		pBuf->nonCachedaddr = dvr_remap_uncached_memory(pBuf->phyAddr, pBuf->size, __builtin_return_address(0));
		pBuf->isCache = 0;
		pBuf->virAddr = pBuf->nonCachedaddr;
		pBuf->fromPoll = 0;
	}
	if (p_vtp->ibBufH.virAddr == NULL){
		if (DEMUX_BUF_AllocFromPoll(&p_vtp->ibBufH, sizeof(RINGBUFFER_HEADER), &demux_device->bufPoll, 0) < 0) {
			dmx_crit(pCh->tunerHandle, "[%s %d]CH%d fatal error : fail to allocate ib buffer header\n", __func__, __LINE__, pCh->tunerHandle);
			DEMUX_BUF_ReturnVideoBuffer(pCh, port);
			return -1;
		}
	}
	dmx_crit(pCh->tunerHandle, "[%s_%d]: ch_%d, videoIBBufferHeader:0x%lx\n", __func__, __LINE__, pCh->tunerHandle, (ULONG)p_vtp->ibBufH.phyAddr);
	DEMUX_BUF_ResetRBHeader(&p_vtp->ibBufH, pBuf, RINGBUFFER_COMMAND, 1);

	p_vtp->vBufStatus = BUF_STAT_NEW;
	p_vtp->SrcCh = pCh->tunerHandle;
	/*if (p_vtp->isECP != pCh->isECP)
	{
		if (p_vtp->isECP)
			SET_FLAG(pCh->events, EVENT_ENTER_ECP_MODE);
		else
			SET_FLAG(pCh->events, EVENT_EXIT_ECP_MODE);
	}*/
	return 0;
}

int DEMUX_BUF_ReturnVideoBuffer(demux_channel *pCh, int port)
{
	DEMUX_BUF_T *pBuf = 0;
	DEMUX_VTP_T *p_vtp  = NULL;

	if ((pCh==NULL) ||( port < 0) || (port >= 2)){
		dmx_err(pCh?pCh->tunerHandle:CHANNEL_UNKNOWN, "invalid param port:%d\n", port);
		return -1;
	}

	p_vtp = &demux_device->VTP_Info[port];
	p_vtp->SrcCh = 0xFF;
	pBuf = &p_vtp->bsBuf;
	if (pBuf->allocSize == 0) {
		dmx_crit(pCh->tunerHandle,"[%s %d]CH%d return SVP buf = 0x%lx\n", __func__, __LINE__, pCh->tunerHandle, (ULONG)pBuf->phyAddr);
		if (pBuf->phyAddr) {
#ifdef CONFIG_RTK_KDRV_VDEC
			rtkvdec_cobuffer_free(VDEC_SVP_BUF, pBuf->phyAddr);
			dvr_unmap_memory(pBuf->isCache ? (void *)pBuf->cachedaddr : (void *)pBuf->nonCachedaddr, pBuf->size);
#else
		dmx_err(pCh->tunerHandle, "[%s %d]CH%d fail to free buf\n", __func__, __LINE__, pCh->tunerHandle);
		return -1;
#endif
		}
	} else {
		dmx_crit(pCh->tunerHandle,"[%s %d]CH%d return ES buf = 0x%lx, virAddr = %p\n", __func__, __LINE__, pCh->tunerHandle, (ULONG)pBuf->phyAddr, pBuf->virAddr);
		if (pBuf->cachedaddr)
			dvr_free(pBuf->cachedaddr);
	}
	memset(pBuf, 0, sizeof(DEMUX_BUF_T));

	pBuf = &p_vtp->ibBuf;
	if (pBuf->allocSize == 0) {
		dmx_crit(pCh->tunerHandle,"[%s %d]CH%d return IB buf = 0x%lx\n", __func__, __LINE__, pCh->tunerHandle, (ULONG)pBuf->phyAddr);
		if (pBuf->phyAddr) {
#ifdef CONFIG_RTK_KDRV_VDEC
			rtkvdec_cobuffer_free(VDEC_INBAND_BUF, pBuf->phyAddr);
			dvr_unmap_memory(pBuf->isCache ? (void *)pBuf->cachedaddr : (void *)pBuf->nonCachedaddr, pBuf->size);
#else
		dmx_err(pCh->tunerHandle, "[%s %d]CH%d fail to free buf\n", __func__, __LINE__, pCh->tunerHandle);
		return -1;

#endif
		}
	} else {
		dmx_crit(pCh->tunerHandle,"[%s %d]CH%d return IB buf = 0x%lx, virAddr = %p\n", __func__, __LINE__, pCh->tunerHandle, (ULONG)pBuf->phyAddr, pBuf->virAddr);
		if (pBuf->cachedaddr)
			dvr_free(pBuf->cachedaddr);
	}
	memset(pBuf, 0, sizeof(DEMUX_BUF_T));

	return 0;
}
/*
int DEMUX_BUF_AllocateBuffer(demux_channel *pCh, int port)
{
	DEMUX_BUF_T *pBuf = 0;

	pBuf = &pCh->bsBuf[port];
	pBuf->allocSize = pBuf->size = VDEC_SVP_BUF;
	pBuf->isCache = 1;
	pBuf->cachedaddr = (UINT8 *)dvr_malloc_specific(pBuf->allocSize, GFP_DCU1);
	if (pBuf->cachedaddr == 0) {
		dmx_crit("[%s %d] fatal error: fail to allocate video ES buffer\n", __func__, __LINE__);
		BUG();
		return -1;
	}
	pBuf->nonCachedaddr = pBuf->cachedaddr;
	pBuf->phyAddr = dvr_to_phys(pBuf->cachedaddr);
	pBuf->virAddr = isCache ? pBuf->cachedaddr : pBuf->nonCachedaddr;
	pBuf->fromPoll = 0;
	dmx_crit("[%s %d]CH%d ES buf = 0x%x, vir = 0x%x\n", __func__, __LINE__, pCh->tunerHandle, pBuf->phyAddr, pBuf->virAddr);

	//prepare inband buffer
	pBuf = &pCh->ibBuf[port];
	pBuf->allocSize = pBuf->size = VDEC_INBAND_BUF;
	pBuf->isCache = 0;
	pBuf->cachedaddr = (UINT8 *)dvr_malloc_uncached_specific(pBuf->allocSize, GFP_DCU1, (void*)&pBuf->nonCachedaddr);
	if (pBuf->cachedaddr == 0) {
		dmx_crit("[%s %d] fatal error: fail to allocate video IB buffer\n", __func__, __LINE__);
		dvr_free(pCh->bsBuf[port].virAddr);
		memset(&pCh->bsBuf[port], 0, sizeof(DEMUX_BUF_T));
		BUG();
		return -1;
	}
	pBuf->phyAddr = dvr_to_phys(pBuf->cachedaddr);
	pBuf->virAddr = isCache ? pBuf->cachedaddr : pBuf->nonCachedaddr;
	pBuf->fromPoll = 0;
	dmx_crit("[%s %d]CH%d IB buf = 0x%x, vir = 0x%x\n", __func__, __LINE__, pCh->tunerHandle, pBuf->phyAddr, pBuf->virAddr);

	SET_FLAG(pCh->events, EVENT_SET_VIDEO_PIN_INFO);
	return 0;
}
*/
int DEMUX_BUF_UpdatePinInfo(demux_channel *pCh, char bVideoPin)
{
	int i, port;
	DEMUX_BUF_PIN_T *pPinInfo, *pPinIBInfo;
	DEMUX_BUF_T *pBufBS, *pBufIB, *pBufBSH, *pBufIBH;
	DEMUX_VTP_T *p_vtp = NULL;

	for (i = VIDEO_PIN; i < NUMBER_OF_PINS; i++) {
		if ((bVideoPin && (i == AUDIO_PIN || i== AUDIO2_PIN))
		|| (!bVideoPin && (i == VIDEO_PIN || i == VIDEO2_PIN)))
			continue;

		if(i == VIDEO_PIN || i == VIDEO2_PIN) {
			port = (i != VIDEO_PIN);
			pBufBSH  = &demux_device->VTP_Info[port].bsBufH;
			pBufIBH  = &demux_device->VTP_Info[port].ibBufH;
			pBufBS   = &demux_device->VTP_Info[port].bsBuf;
			pBufIB   = &demux_device->VTP_Info[port].ibBuf;

		} else {
			continue;

		}

		pPinInfo   = &pCh->pinInfo[i].bufInfo[DEMUX_STREAM_TYPE_BS];
		pPinIBInfo = &pCh->pinInfo[i].bufInfo[DEMUX_STREAM_TYPE_IB];
		mutex_lock(&demux_device->mutex);
		p_vtp = &demux_device->VTP_Info[port];
		if ((p_vtp->vBufStatus == BUF_STAT_FREE) && (pCh->tunerHandle == p_vtp->SrcCh)) {
			if (pCh->isECP){
				DEMUX_TA_ECP_UpdatePininfo(pCh, i, DEMUX_STREAM_TYPE_IB, 0, 0, 0);
				DEMUX_TA_ECP_UpdatePininfo(pCh, i, DEMUX_STREAM_TYPE_BS, 0, 0, 0);

				//TODO, CANCEL PROTECTION
				RHAL_ES_DisableProtect(pCh->tunerHandle);
				pBufBS->cachedaddr = dvr_remap_cached_memory(pBufBS->phyAddr, pBufBS->size, __builtin_return_address(0));
				pBufBS->virAddr = pBufBS->cachedaddr;
			}
			DEMUX_BUF_ReturnVideoBuffer(pCh, port);
			memset(pPinIBInfo, 0, sizeof(DEMUX_BUF_PIN_T));
			memset(pPinInfo, 0, sizeof(DEMUX_BUF_PIN_T));
			dmx_crit(pCh->tunerHandle, "[%s %d] CH%d relase  pin = %d, vtp = %d\n", __func__, __LINE__, pCh->tunerHandle, i, port);
			p_vtp->vBufStatus = BUF_STAT_DONE;
		} else if ((p_vtp->vBufStatus == BUF_STAT_NEW) &&  (pCh->tunerHandle == p_vtp->SrcCh)) {
			RINGBUFFER_HEADER* pHeader = 0;
			//don't reset inband buffer header
			DEMUX_BUF_ResetRBHeader(pBufBSH, pBufBS, RINGBUFFER_STREAM,  1);
			DEMUX_Buf_SetPinInfo(pPinIBInfo, (RINGBUFFER_HEADER *)pBufIBH->virAddr, pBufIB);
			DEMUX_Buf_SetPinInfo(pPinInfo, (RINGBUFFER_HEADER *)pBufBSH->virAddr, pBufBS);
			dmx_crit(pCh->tunerHandle, "[%s %d]set CH%d pin = %d\n", __func__, __LINE__, pCh->tunerHandle, i);
			if (pCh->isECP){
				DEMUX_TA_ECP_UpdatePininfo(pCh, i, DEMUX_STREAM_TYPE_IB, 0, (unsigned long)pBufIBH - (unsigned long)demux_device, (unsigned long)pBufIB - (unsigned long)demux_device);
				DEMUX_TA_ECP_UpdatePininfo(pCh, i, DEMUX_STREAM_TYPE_BS, 0, (unsigned long)pBufBSH - (unsigned long)demux_device, (unsigned long)pBufBS - (unsigned long)demux_device);

				dvr_unmap_memory(pBufBS->isCache ? (void *)pBufBS->cachedaddr : (void *)pBufBS->nonCachedaddr, pBufBS->size);
				pBufBS->virAddr = pBufBS->cachedaddr = NULL;
				//TO DO: SET PROTECTIO
				RHAL_ES_EnableProtect(pCh->tunerHandle, pBufBS->phyAddr,  pBufBS->phyAddr+pBufBS->size);
				
			}
			pHeader = (RINGBUFFER_HEADER *)pBufIBH->virAddr;
			dmx_crit(pCh->tunerHandle, "[%s %d]IB rp=0x%x, wp = 0x%x, base = 0x%x\n", __func__, __LINE__, (u32)pHeader->readPtr[0], (u32)pHeader->writePtr, (u32)pHeader->beginAddr);
			p_vtp->vBufStatus = BUF_STAT_DONE;
		} else if (p_vtp->vBufStatus == BUF_STAT_REUSED_II) {
			if (pCh->tunerHandle == p_vtp->SrcCh) {
				dmx_crit(pCh->tunerHandle, "%s_%d: DMX_%d, reuse vtp_%d wait release\n", __func__, __LINE__, pCh->tunerHandle, port)
				return -1;
			} else {
				memset(pPinIBInfo, 0, sizeof(DEMUX_BUF_PIN_T));
				memset(pPinInfo, 0, sizeof(DEMUX_BUF_PIN_T));
				p_vtp->vBufStatus = BUF_STAT_NEW;
				dmx_crit(pCh->tunerHandle, "%s_%d: DMX_%d, disconnect vtp_%d\n", __func__, __LINE__, pCh->tunerHandle, port)
			}
		}
		mutex_unlock(&demux_device->mutex);

	}
	return 0;
}
//###########################################################AUDIO BUFFER#################################################
#ifdef DEMUX_AUDIO_USE_GLOBAL_BUFFER
bool isPinOwnTheBuffer(demux_channel * pCh, int pin, DEMUX_PIN_BUFFER_T * pBuffer)
{
	return ((pCh->tunerHandle == pBuffer->ownerChannel) &&(pin == pBuffer->ownerPin));
}

int demuxFreeAudioBuffer(DEMUX_PIN_BUFFER_T * pBuffer)
{
	//DO NOT RETURE AUDIO BUFFER , JUST RESET RING BUFFER EADER
	if (pBuffer == NULL)
	{
		dmx_err(CHANNEL_UNKNOWN, "[%s,%d], param error, \n", __func__, __LINE__);
		return -1;
	}
	//return bs buffer
#ifdef RETUN_AUDIO_BUFFER
	if (pBuffer->bsBufferH.virAddr)
	{
		DEMUX_BUF_ResetRBHeader(&pBuffer->bsBufferH,NULL, 0,0);
	}
	if (pBuffer->bsBuffer.phyAddr)
	{
		dmx_mask_print(DEMUX_NOTICE_PRINT|DEMUX_BUFFER_DEBUG, CHANNEL_UNKNOWN,"[%s %d]CH%d return audio ES buf = 0x%x, virAddr = %p\n",
			__func__, __LINE__, pCh->tunerHandle, pBuffer->bsBuffer->phyAddr, pBuffer->bsBuffer->virAddr);
		DEMUX_BUF_FreeBuffer(&pBuffer->bsBuffer);
	}
	//retrun ib buffer
	if (pBuffer->ibBufferH.virAddr)
	{
		DEMUX_BUF_ResetRBHeader(&pBuffer->ibBufferH, NULL, 0,0);
	}
	if (pBuffer->ibBuffer.phyAddr)
	{
		dmx_mask_print(DEMUX_NOTICE_PRINT|DEMUX_BUFFER_DEBUG, CHANNEL_UNKNOWN, "[%s %d]CH%d return audio IB buf = 0x%x, virAddr = %p\n",
			__func__, __LINE__, pCh->tunerHandle, pBuffer->ibBuffer->phyAddr, pBuffer->ibBuffer->virAddr);
		DEMUX_BUF_FreeBuffer(&pBuffer->ibBuffer);
	}
#else
	if (pBuffer->bsBufferH.virAddr)
	{

		DEMUX_BUF_ResetRBHeader(&pBuffer->bsBufferH, &pBuffer->bsBuffer, RINGBUFFER_STREAM, 1);

	}
	if (pBuffer->ibBufferH.virAddr)
	{
		DEMUX_BUF_ResetRBHeader(&pBuffer->ibBufferH, &pBuffer->ibBuffer, RINGBUFFER_COMMAND,  1);

	}
#endif
	return 0;
}

int demuxPrepareAudioBuffer(DEMUX_PIN_BUFFER_T *pBuffer)
{
	if(pBuffer == NULL)
	{
		dmx_err(CHANNEL_UNKNOWN, "[%s,%d], param error!!\n", __func__, __LINE__);
		return -1;
	}

	if (pBuffer->bsBufferH.virAddr == 0)
	{
		if (DEMUX_BUF_AllocFromPoll(&pBuffer->bsBufferH, sizeof(RINGBUFFER_HEADER), &demux_device->bufPoll, 0) < 0) {
			dmx_err(CHANNEL_UNKNOWN, "[%s %d]out of memory: allocate audio bs buffer header fail\n", __func__, __LINE__);
			return -1;
		}
	}
	if (pBuffer->ibBufferH.virAddr == 0) {
		if (DEMUX_BUF_AllocFromPoll(&pBuffer->ibBufferH, sizeof(RINGBUFFER_HEADER), &demux_device->bufPoll, 0) < 0) {
			dmx_err(CHANNEL_UNKNOWN, "[%s %d] out of memory: allocate audio ib buffer header fail\n", __func__, __LINE__);
			return -1;
		}
	}
	if (pBuffer->bsBuffer.virAddr == 0 ||pBuffer->ibBuffer.virAddr == 0 )
	{
		if ((pBuffer->bsBuffer.virAddr == 0 && DEMUX_BUF_AllocBuffer(&pBuffer->bsBuffer, demux_device->sizeOfAudioBS, 1, 1) < 0)
			|| (pBuffer->ibBuffer.virAddr == 0 && DEMUX_BUF_AllocBuffer(&pBuffer->ibBuffer, demux_device->sizeOfAudioIB, 0, 1) < 0)) {
			dmx_err(CHANNEL_UNKNOWN, "[%s %d]out of memory: allocate audio bs buffer\n", __func__, __LINE__);
			DEMUX_BUF_FreeBuffer(&pBuffer->bsBuffer);
			DEMUX_BUF_FreeBuffer(&pBuffer->ibBuffer);
			return -1;
		}
		//only when alloc buffer ,reset HEADER
		DEMUX_BUF_ResetRBHeader(&pBuffer->bsBufferH, &pBuffer->bsBuffer, RINGBUFFER_COMMAND, 1);
		DEMUX_BUF_ResetRBHeader(&pBuffer->ibBufferH, &pBuffer->ibBuffer, RINGBUFFER_STREAM,  1);
	}
	return 0;
}

bool demuxAudio_getFreeBuffer(DEMUX_PIN_BUFFER_T **ppbuffer)
{
	int i, tWaitFree = -1;;
	if (ppbuffer == NULL){
		return false;
	}

	for (i = 0; i<MAX_ADEC_NUM; i++)
	{
		if ((demux_device->audioBuffer[i].status == BUF_STAT_FREE) && (tWaitFree<0))
		{
			tWaitFree = i;
			continue;
		}
		//prepare the buffer
		//demuxAudio_prepareBufer(&demux_device->audioBuffer[i]);
		if (demux_device->audioBuffer[i].status == BUF_STAT_OFFLINE)
			break;
	}

	if (i == MAX_ADEC_NUM )
	{
		if ( tWaitFree>=0)
		{
			dmx_mask_print(DEMUX_NOTICE_PRINT|DEMUX_BUFFER_DEBUG,CHANNEL_UNKNOWN,"[%s, %d]  no unUsedBuffer, try resuse Buffer:%d(ch:%d, pin:%d)\n",
				__func__, __LINE__,  tWaitFree,demux_device->audioBuffer[tWaitFree].ownerChannel,demux_device->audioBuffer[tWaitFree].ownerPin);
			demux_device->audioBuffer[tWaitFree].status 		= BUF_STAT_REUSED;
			i  = tWaitFree;
		}
		else
		{
			dmx_err(CHANNEL_UNKNOWN, "[%s,%d],  ERROR Not idle Buffer for audio \n", __func__, __LINE__);
			return false;
		}
	}
	else{
		//:p1
		demuxPrepareAudioBuffer(&demux_device->audioBuffer[i]);
		demux_device->audioBuffer[i].status 		= BUF_STAT_NEW;
		dmx_mask_print(DEMUX_NOTICE_PRINT|DEMUX_BUFFER_DEBUG,CHANNEL_UNKNOWN,"[%s,%d]  alloc buffer success\n", __func__, __LINE__);
	}
	//demux_device->audioBuffer[tWaitFree].ownerChannel	= pCh->tunerHandle;
	//demux_devide->audioBuffer[tWaitFree].ownerPin		= pin;
	*ppbuffer = &demux_device->audioBuffer[i];
	dmx_mask_print(DEMUX_NOTICE_PRINT|DEMUX_BUFFER_DEBUG,CHANNEL_UNKNOWN,"[%s,%d] select AudioBuffer %d\n",__func__, __LINE__, demux_device->audioBuffer[i].bufferIndex);
	return true;
}
bool demxAudio_handleHoldBuffer(demux_channel *pCh, int pin)
{
	if (pCh==NULL && pin != AUDIO_PIN && pin != AUDIO2_PIN)
	{
		dmx_err(CHANNEL_UNKNOWN, "[%s,%d], param error, pin:%d\n", __func__, __LINE__, pin);
		return false;
	}
	if (pCh->pinInfo[pin].pBuffer == NULL )
	{
		dmx_err(pCh->tunerHandle,"[%s, %d], ch:%d, pin:%d, do not hold buffer \n", __func__, __LINE__, pCh->tunerHandle, pin)
		return false;
	}

	if (pCh->pinInfo[pin].pBuffer->status == BUF_STAT_OFFLINE)
	{

		dmx_err(pCh->tunerHandle,"[%s,%d] ch:%d, pin:%d buffer status wrong, OFFLINE BUT BEEN  REFERED\n",__func__, __LINE__, pCh->tunerHandle, pin);
		BUG();
		pCh->pinInfo[pin].pBuffer = NULL;
		if(demuxAudio_getFreeBuffer(&pCh->pinInfo[pin].pBuffer))
		{
			pCh->pinInfo[pin].pBuffer->ownerChannel = pCh->tunerHandle;
			pCh->pinInfo[pin].pBuffer->ownerPin = pin;
			return true;
		}
		else
			return false;
	}
	if (pCh->pinInfo[pin].pBuffer->status == BUF_STAT_ONLINE ||pCh->pinInfo[pin].pBuffer->status == BUF_STAT_NEW){
		//check owner :should same
		if (isPinOwnTheBuffer(pCh,pin, pCh->pinInfo[pin].pBuffer))
		{
			dmx_mask_print(DEMUX_NOTICE_PRINT|DEMUX_BUFFER_DEBUG, pCh->tunerHandle, "[%s,%d] ch:%d, pin:%d, buffer status:%d  DO NOTHING\n",
				__func__, __LINE__, pCh->tunerHandle, pin, pCh->pinInfo[pin].pBuffer->status);
			return true;
		}
		else{
			dmx_err(pCh->tunerHandle,"[%s,%d] ch:%d, pin:%d,ERROR buffer status:%d  OWNER: CH:%d, pin:%d \n",
				__func__, __LINE__, pCh->tunerHandle, pin, pCh->pinInfo[pin].pBuffer->status, pCh->pinInfo[pin].pBuffer->ownerChannel, pCh->pinInfo[pin].pBuffer->ownerPin);
			BUG();
			return false;
		}
	}
	else if (pCh->pinInfo[pin].pBuffer->status == BUF_STAT_FREE)
	{
		//SAME OWNER
		//if (isPinOwnTheBuffer(pCh,pin, pCh->pinInfo[pin].pBuffer)){
			dmx_mask_print(DEMUX_NOTICE_PRINT|DEMUX_BUFFER_DEBUG, pCh->tunerHandle, ,"[%s,%d] ch:%d, pin:%d, buffer status:%d  still hold buffer REUSE it \n",
				__func__, __LINE__, pCh->tunerHandle, pin, pCh->pinInfo[pin].pBuffer->status);
			pCh->pinInfo[pin].pBuffer->status 		= BUF_STAT_NEW;
			pCh->pinInfo[pin].pBuffer->ownerChannel 	= pCh->tunerHandle;
			pCh->pinInfo[pin].pBuffer->ownerPin		= pin;
			return true;
		//}
		//else{//DIFF OWNER
		//	dmx_mask_print(DEMUX_NOTICE_PRINT|DEMUX_BUFFER_DEBUG, pCh->tunerHandle, "[%s,%d] ch:%d, pin:%d,ERROR buffer status:%d  owner: ch%d, pin:%d\n",
		//		__func__, __LINE__, pCh->tunerHandle, pin, pCh->pinInfo[pin].pBuffer->status, pCh->pinInfo[pin].pBuffer->ownerChannel, pCh->pinInfo[pin].pBuffer->ownerPin);
		//	BUG();
		//	return false;
		//}
	}
	else if(pCh->pinInfo[pin].pBuffer->status == BUF_STAT_REUSED)
	{
		if (isPinOwnTheBuffer(pCh,pin, pCh->pinInfo[pin].pBuffer))
		{
			dmx_mask_print(DEMUX_NOTICE_PRINT|DEMUX_BUFFER_DEBUG, pCh->tunerHandle, "[%s,%d] ch:%d, pin:%d, buffer status:%d  already got buffer with for previous release \n",
				__func__, __LINE__, pCh->tunerHandle, pin, pCh->pinInfo[pin].pBuffer->status);
			return true;
		}
		else
		{
			//got new buffer
			if(demuxAudio_getFreeBuffer(&pCh->pinInfo[pin].pBuffer_reserved))
			{
				dmx_mask_print(DEMUX_NOTICE_PRINT|DEMUX_BUFFER_DEBUG, pCh->tunerHandle, "[%s, %d], ch:%d, pin:%d, request buffer success, still hold a buffer to relase\n",
					__func__, __LINE__, pCh->tunerHandle, pin);
				pCh->pinInfo[pin].pBuffer_reserved->ownerChannel = pCh->tunerHandle;
				pCh->pinInfo[pin].pBuffer_reserved->ownerPin = pin;
				pCh->pinInfo[pin].pBuffer->status 		= BUF_STAT_REUSED_II;
				return true;
			}
			else
			{
				dmx_mask_print(DEMUX_NOTICE_PRINT|DEMUX_BUFFER_DEBUG, pCh->tunerHandle, "[%s, %d], ch:%d, pin:%d, request buffer fail, \n",
					__func__, __LINE__, pCh->tunerHandle, pin);
				return false;
			}

		}
	}
	else if (pCh->pinInfo[pin].pBuffer->status == BUF_STAT_REUSED_II)
	{
		dmx_mask_print(DEMUX_NOTICE_PRINT|DEMUX_BUFFER_DEBUG, pCh->tunerHandle, "[%s,%d] ch:%d, pin:%d, buffer status:%d  already got buffer with for previous release\n",
			__func__, __LINE__, pCh->tunerHandle, pin, pCh->pinInfo[pin].pBuffer->status);
		return true;
	}

	dmx_err(pCh->tunerHandle,"[%s, %d], ch:%d,pin:%d, buffer status not recognize status:%d\n",
		__func__, __LINE__, pCh->tunerHandle, pin, pCh->pinInfo[pin].pBuffer->status);
	return false;
}

bool demuxAudio_updatePinStatus(demux_channel *pCh, int pin)
{
	bool ret = false;
	DEMUX_BUF_PIN_T *pPinInfo=NULL, *pPinIBInfo=NULL;
	if (pCh == NULL || ((pin !=AUDIO_PIN) &&(pin != AUDIO2_PIN)))
	{
		dmx_err(CHANNEL_UNKNOWN,"[%s, %d] param wrong pch:%p, pin:%d \n", __func__, __LINE__, pCh, pin);
		return false;
	}
	if (pCh->pinInfo[pin].pBuffer == NULL)
	{
		dmx_mask_print(DEMUX_NOTICE_PRINT|DEMUX_BUFFER_DEBUG, pCh->tunerHandle,"[%s, %d] ch:%d, pin:%d, there is not buffer in this pin \n", __func__, __LINE__, pCh->tunerHandle, pin);
		return true;
	}
	if (pCh->pinInfo[pin].pBuffer->status == BUF_STAT_NEW)
	{//:P8
		if (isPinOwnTheBuffer(pCh,pin, pCh->pinInfo[pin].pBuffer))
		{
			dmx_mask_print(DEMUX_NOTICE_PRINT|DEMUX_BUFFER_DEBUG, pCh->tunerHandle, "[%s,%d] ch:%d, pin :%d, buffer[%d] online \n",
				__func__, __LINE__, pCh->tunerHandle, pin,pCh->pinInfo[pin].pBuffer->bufferIndex);

			DEMUX_Buf_SetPinInfo(&pCh->pinInfo[pin].bufInfo[DEMUX_STREAM_TYPE_IB], (RINGBUFFER_HEADER *)pCh->pinInfo[pin].pBuffer->ibBufferH.virAddr, &pCh->pinInfo[pin].pBuffer->ibBuffer);
			DEMUX_Buf_SetPinInfo(&pCh->pinInfo[pin].bufInfo[DEMUX_STREAM_TYPE_BS], (RINGBUFFER_HEADER *)pCh->pinInfo[pin].pBuffer->bsBufferH.virAddr, &pCh->pinInfo[pin].pBuffer->bsBuffer);
			pCh->pinInfo[pin].pBuffer->status = BUF_STAT_ONLINE;
			ret = true;
		}
		else{
			dmx_err(pCh->tunerHandle,"[%s,%d], ch:%d, pin:%d, buffer[%d]status not sync,bufferOwer(ch:%d,pin:%d)\n",
				 __func__, __LINE__, pCh->tunerHandle, pin,pCh->pinInfo[pin].pBuffer->bufferIndex, pCh->pinInfo[pin].pBuffer->ownerChannel,pCh->pinInfo[pin].pBuffer->ownerPin);
			BUG();
			ret = false;
		}
	}
	else if (pCh->pinInfo[pin].pBuffer->status == BUF_STAT_FREE)
	{
		dmx_mask_print(DEMUX_NOTICE_PRINT|DEMUX_BUFFER_DEBUG, pCh->tunerHandle,"[%s,%d] ch:%d, pin :%d, buffer[%d] (ch:%d, pin:%d)OFFLINE \n",
			__func__, __LINE__, pCh->tunerHandle, pin, pCh->pinInfo[pin].pBuffer->bufferIndex, pCh->pinInfo[pin].pBuffer->ownerChannel, pCh->pinInfo[pin].pBuffer->ownerPin);
		pPinInfo   = &pCh->pinInfo[pin].bufInfo[DEMUX_STREAM_TYPE_BS];
		pPinIBInfo = &pCh->pinInfo[pin].bufInfo[DEMUX_STREAM_TYPE_IB];
		memset(pPinIBInfo, 0, sizeof(DEMUX_BUF_PIN_T));
		memset(pPinInfo, 0, sizeof(DEMUX_BUF_PIN_T));
		//need to return buffer
		demuxFreeAudioBuffer(pCh->pinInfo[pin].pBuffer)	;
		//no need to return buffer.
		//TODO
		pCh->pinInfo[pin].pBuffer->status = BUF_STAT_OFFLINE;
		pCh->pinInfo[pin].pBuffer = NULL;
		ret = true;

	}
	else if (pCh->pinInfo[pin].pBuffer->status == BUF_STAT_REUSED)
	{//:P5
		if (!isPinOwnTheBuffer(pCh, pin, pCh->pinInfo[pin].pBuffer))
		{
			dmx_mask_print(DEMUX_NOTICE_PRINT|DEMUX_BUFFER_DEBUG, pCh->tunerHandle,"[%s,%d] ch:%d, pin :%d, release reused buffer[%d] \n",
				__func__, __LINE__, pCh->tunerHandle, pin, pCh->pinInfo[pin].pBuffer->bufferIndex);
			pPinInfo   = &pCh->pinInfo[pin].bufInfo[DEMUX_STREAM_TYPE_BS];
			pPinIBInfo = &pCh->pinInfo[pin].bufInfo[DEMUX_STREAM_TYPE_IB];
			memset(pPinIBInfo, 0, sizeof(DEMUX_BUF_PIN_T));
			memset(pPinInfo, 0, sizeof(DEMUX_BUF_PIN_T));
			pCh->pinInfo[pin].pBuffer->status = BUF_STAT_NEW;
			pCh->pinInfo[pin].pBuffer = NULL;
			ret = true;
		}
		else
		{
			dmx_mask_print(DEMUX_NOTICE_PRINT|DEMUX_BUFFER_DEBUG, pCh->tunerHandle,"[%s,%d] ch:%d, pin :%d, wait buffer [%d] release \n",
				__func__, __LINE__, pCh->tunerHandle, pin, pCh->pinInfo[pin].pBuffer->bufferIndex);
			ret = false;
		}
	}
	else if (pCh->pinInfo[pin].pBuffer->status == BUF_STAT_REUSED_II)
	{
		if (!isPinOwnTheBuffer(pCh, pin, pCh->pinInfo[pin].pBuffer))
		{
			dmx_mask_print(DEMUX_NOTICE_PRINT|DEMUX_BUFFER_DEBUG, pCh->tunerHandle,"[%s,%d] ch:%d, pin :%d, release reusedII buffer %d \n",
				__func__, __LINE__, pCh->tunerHandle, pin,pCh->pinInfo[pin].pBuffer->bufferIndex);
			pPinInfo   = &pCh->pinInfo[pin].bufInfo[DEMUX_STREAM_TYPE_BS];
			pPinIBInfo = &pCh->pinInfo[pin].bufInfo[DEMUX_STREAM_TYPE_IB];
			memset(pPinIBInfo, 0, sizeof(DEMUX_BUF_PIN_T));
			memset(pPinInfo, 0, sizeof(DEMUX_BUF_PIN_T));
			pCh->pinInfo[pin].pBuffer->status = BUF_STAT_NEW;
			pCh->pinInfo[pin].pBuffer = NULL;
			//reserver buffer
			if (pCh->pinInfo[pin].pBuffer_reserved){
				dmx_mask_print(DEMUX_NOTICE_PRINT|DEMUX_BUFFER_DEBUG, pCh->tunerHandle,"[%s,%d] ch:%d, pin :%d, handle reserved buffer [%d]\n",
					__func__, __LINE__, pCh->tunerHandle, pin,pCh->pinInfo[pin].pBuffer->bufferIndex);
				pCh->pinInfo[pin].pBuffer = pCh->pinInfo[pin].pBuffer_reserved;
				pCh->pinInfo[pin].pBuffer_reserved = NULL;
				if (isPinOwnTheBuffer(pCh, pin, pCh->pinInfo[pin].pBuffer))
				{
					if (demuxAudio_updatePinStatus(pCh, pin))
						ret = true;
					else
						ret = false;
				}
				else{
					dmx_err(pCh->tunerHandle,"[%s,%d] ch:%d, pin :%d,  reusedII buffer %d , owner ERROR (CH:%d, pin:%d)\n",
						__func__, __LINE__, pCh->tunerHandle, pin,pCh->pinInfo[pin].pBuffer->bufferIndex, pCh->pinInfo[pin].pBuffer->ownerChannel,pCh->pinInfo[pin].pBuffer->ownerPin);
					BUG();
					ret = false;
				}
			}
			else{
				dmx_err(pCh->tunerHandle,"[%s,%d] ch:%d, pin :%d, reusedII pin, miss reserved buffer \n",__func__, __LINE__, pCh->tunerHandle, pin );
				BUG();
				ret=false;
			}
		}
		else
		{
			dmx_mask_print(DEMUX_NOTICE_PRINT|DEMUX_BUFFER_DEBUG, pCh->tunerHandle,"[%s,%d] ch:%d, pin :%d,  reserved buffer [%d] wiat release\n",
					__func__, __LINE__, pCh->tunerHandle, pin,pCh->pinInfo[pin].pBuffer->bufferIndex);
			ret = false;
		}
	}
	else{
		dmx_mask_print(DEMUX_NOTICE_PRINT|DEMUX_BUFFER_DEBUG, pCh->tunerHandle,"[%s,%d] ch:%d, pin :%d, bufer[%d], status:%d, no nothing\n",
			__func__, __LINE__, pCh->tunerHandle, pin,pCh->pinInfo[pin].pBuffer->bufferIndex, pCh->pinInfo[pin].pBuffer->status);
		ret = true;
	}
	return ret;
}

int DEMUX_BUF_RequestAudioBuffer(demux_channel *pCh, int pin)
{//this function should already hold channel mutex.

	if (pCh==NULL && pin != AUDIO_PIN && pin != AUDIO2_PIN)
	{
		dmx_err(CHANNEL_UNKNOWN,"[%s,%d], param error, pin:%d\n", __func__, __LINE__, pin);
		return -1;
	}

	mutex_lock(&demux_device->mutex);
	if (pCh->pinInfo[pin].pBuffer != NULL )//HOLD BUFFER
	{
		if (demxAudio_handleHoldBuffer(pCh, pin))
		{
			dmx_mask_print(DEMUX_NOTICE_PRINT|DEMUX_BUFFER_DEBUG, pCh->tunerHandle,"[%s,%d] ch:%d, pin:%d, request buffer success \n",__func__, __LINE__, pCh->tunerHandle, pin);
			goto SUCCESS;
		}
		else
		{
			dmx_err(pCh->tunerHandle,"[%s,%d] ch:%d, pin:%d, request buffer fail \n",__func__, __LINE__, pCh->tunerHandle, pin);
			goto FAIL;
		}
	}
	else
	{
		if (demuxAudio_getFreeBuffer(&pCh->pinInfo[pin].pBuffer))
		{
			pCh->pinInfo[pin].pBuffer->ownerChannel = pCh->tunerHandle;
			pCh->pinInfo[pin].pBuffer->ownerPin = pin;
			dmx_mask_print(DEMUX_NOTICE_PRINT|DEMUX_BUFFER_DEBUG, pCh->tunerHandle,"[%s,%d] ch:%d, pin:%d, request buffer success \n",__func__, __LINE__, pCh->tunerHandle, pin);
			goto SUCCESS;
		}
		else
		{
			dmx_err(pCh->tunerHandle,"[%s,%d] ch:%d, pin:%d, request buffer fail \n",__func__, __LINE__, pCh->tunerHandle, pin);
			goto FAIL;

		}
	}

SUCCESS:
	mutex_unlock(&demux_device->mutex);
	return 0;
FAIL:
	mutex_unlock(&demux_device->mutex);
	return -1;

}
int DEMUX_BUF_ReleaseAudioBuffer(demux_channel *pCh, int pin)
{
	DEMUX_PIN_BUFFER_T *pBuffer=NULL;
	DEMUX_BUF_PIN_T *pPinInfo, *pPinIBInfo;
	if (pCh==NULL && pin != AUDIO_PIN && pin != AUDIO2_PIN)
	{
		dmx_err(CHANNEL_UNKNOWN,"[%s,%d], param error, pin:%d\n", __func__, __LINE__, pin);
		return -1;
	}
	pPinInfo   = &pCh->pinInfo[pin].bufInfo[DEMUX_STREAM_TYPE_BS];
	pPinIBInfo = &pCh->pinInfo[pin].bufInfo[DEMUX_STREAM_TYPE_IB];
	mutex_lock(&demux_device->mutex);
	pBuffer = pCh->pinInfo[pin].pBuffer;
	if (pBuffer != NULL)
	{

		if(pBuffer->status == BUF_STAT_ONLINE ){
			dmx_mask_print(DEMUX_BUFFER_DEBUG|DEMUX_NOTICE_PRINT, pCh->tunerHandle,"[%s, %d] ch:%d, pin:%d, release  buffer status:%d, \n",
				__func__, __LINE__, pCh->tunerHandle, pin, pBuffer->status);
			pBuffer->status = BUF_STAT_FREE;
		}
		else if (pBuffer->status == BUF_STAT_NEW)
		{
			//check use owner
			dmx_mask_print(DEMUX_BUFFER_DEBUG|DEMUX_NOTICE_PRINT,pCh->tunerHandle,"[%s, %d] ch:%d, pin:%d, release unused buffer status:%d, \n",
				__func__, __LINE__, pCh->tunerHandle, pin, pBuffer->status);
			memset(pPinIBInfo, 0, sizeof(DEMUX_BUF_PIN_T));
			memset(pPinInfo, 0, sizeof(DEMUX_BUF_PIN_T));
			demuxFreeAudioBuffer(pCh->pinInfo[pin].pBuffer)	;
			pCh->pinInfo[pin].pBuffer->status = BUF_STAT_OFFLINE;
			pCh->pinInfo[pin].pBuffer = NULL;
		}
		else if (pBuffer->status == BUF_STAT_REUSED )
		{
			if (isPinOwnTheBuffer(pCh,pin, pBuffer))
			{
				dmx_mask_print(DEMUX_BUFFER_DEBUG|DEMUX_NOTICE_PRINT,pCh->tunerHandle,"[%s, %d] ch:%d, pin:%d, status:%d, undo reuse \n",
					__func__, __LINE__, pCh->tunerHandle, pin, pBuffer->status);
				memset(pPinIBInfo, 0, sizeof(DEMUX_BUF_PIN_T));
				memset(pPinInfo, 0, sizeof(DEMUX_BUF_PIN_T));
				pCh->pinInfo[pin].pBuffer->ownerChannel = -1;
				pCh->pinInfo[pin].pBuffer->ownerPin = -1;
				pCh->pinInfo[pin].pBuffer->status = BUF_STAT_FREE;
				pCh->pinInfo[pin].pBuffer = NULL;
			}
			else
			{
				dmx_mask_print(DEMUX_BUFFER_DEBUG|DEMUX_NOTICE_PRINT,pCh->tunerHandle,"[%s, %d] ch:%d, pin:%d, status:%d, wait for release , do nothing\n",
					__func__, __LINE__, pCh->tunerHandle, pin, pBuffer->status);
			}
		}
		else if (pBuffer->status == BUF_STAT_REUSED_II)
		{

			if (isPinOwnTheBuffer(pCh,pin, pBuffer))
			{
				dmx_mask_print(DEMUX_BUFFER_DEBUG|DEMUX_NOTICE_PRINT,pCh->tunerHandle,"[%s, %d] ch:%d, pin:%d, status:%d, undo reuse \n",
					__func__, __LINE__, pCh->tunerHandle, pin, pBuffer->status);
				memset(pPinIBInfo, 0, sizeof(DEMUX_BUF_PIN_T));
				memset(pPinInfo, 0, sizeof(DEMUX_BUF_PIN_T));
				pCh->pinInfo[pin].pBuffer->ownerChannel = -1;
				pCh->pinInfo[pin].pBuffer->ownerPin = -1;
				pCh->pinInfo[pin].pBuffer->status = BUF_STAT_FREE;
				pCh->pinInfo[pin].pBuffer = NULL;
			}
			else
			{
				if (isPinOwnTheBuffer(pCh, pin, pCh->pinInfo[pin].pBuffer_reserved))
				{
					if (pCh->pinInfo[pin].pBuffer_reserved->status == BUF_STAT_NEW)
					{
						dmx_mask_print(DEMUX_BUFFER_DEBUG|DEMUX_NOTICE_PRINT,pCh->tunerHandle,"[%s, %d] ch:%d, pin:%d,reserved buffer Free status:%d,  \n",
							__func__, __LINE__, pCh->tunerHandle, pin, pCh->pinInfo[pin].pBuffer_reserved->status);
						memset(pPinIBInfo, 0, sizeof(DEMUX_BUF_PIN_T));
						memset(pPinInfo, 0, sizeof(DEMUX_BUF_PIN_T));
						demuxFreeAudioBuffer(pCh->pinInfo[pin].pBuffer_reserved)	;
						pCh->pinInfo[pin].pBuffer_reserved->status = BUF_STAT_OFFLINE;
						pCh->pinInfo[pin].pBuffer_reserved = NULL;
					}
					else if (pCh->pinInfo[pin].pBuffer_reserved->status == BUF_STAT_REUSED ||pCh->pinInfo[pin].pBuffer_reserved->status == BUF_STAT_REUSED_II)
					{
						dmx_mask_print(DEMUX_BUFFER_DEBUG|DEMUX_NOTICE_PRINT,pCh->tunerHandle,"[%s, %d] ch:%d, pin:%d, reserved buffer status:%d, undo reuse \n",
							__func__, __LINE__, pCh->tunerHandle, pin, pBuffer->status);
						memset(pPinIBInfo, 0, sizeof(DEMUX_BUF_PIN_T));
						memset(pPinInfo, 0, sizeof(DEMUX_BUF_PIN_T));
						pCh->pinInfo[pin].pBuffer_reserved->ownerChannel = -1;
						pCh->pinInfo[pin].pBuffer_reserved->ownerPin = -1;
						pCh->pinInfo[pin].pBuffer_reserved->status = BUF_STAT_FREE;
						pCh->pinInfo[pin].pBuffer_reserved = NULL;
					}
				}
				else{
					dmx_mask_print(DEMUX_BUFFER_DEBUG|DEMUX_NOTICE_PRINT,pCh->tunerHandle,"[%s, %d] ch:%d, pin:%d, reused_II pin, reserved buffer status not sync owner:ch:%d, pin:%d\n",
						__func__, __LINE__, pCh->tunerHandle, pin, pCh->pinInfo[pin].pBuffer_reserved->ownerChannel,pCh->pinInfo[pin].pBuffer_reserved->ownerPin);
					BUG();
					mutex_unlock(&demux_device->mutex);
					return -1;
				}
			}
		}


	}
	mutex_unlock(&demux_device->mutex);
	return 0;
}

int DEMUX_BUF_UpdateChannelAudioPinStatus(demux_channel *pCh)
{
	int pin = 0;
	int ret = 0;

	if (pCh == NULL){
		dmx_err(CHANNEL_UNKNOWN,"[%s,%d], param ERROR\n", __func__, __LINE__);
		return -1;
	}
	mutex_lock(&demux_device->mutex);
	for (pin = VIDEO_PIN; pin < NUMBER_OF_PINS; pin++)
	{
		if(pin == AUDIO_PIN || pin== AUDIO2_PIN)
		{
			if (pCh->pinInfo[pin].pBuffer)
			{
				if (!demuxAudio_updatePinStatus(pCh, pin)){
					dmx_mask_print(DEMUX_NOTICE_PRINT|DEMUX_BUFFER_DEBUG, pCh->tunerHandle,"[%s, %d] ch:%d, pin:%d, unfinish jobs \n", __func__, __LINE__, pCh->tunerHandle, pin);
					ret = -1;
				}

			}

		}
	}
	mutex_unlock(&demux_device->mutex);
	return ret;
}

void DEMUX_BUF_InitAudioBuffer()
{
	int i = 0;
	for (i=0; i<demux_device->numberOfAD; i++)
	{
		memset(&(demux_device->audioBuffer[i]), 0, sizeof(DEMUX_PIN_BUFFER_T));
		demux_device->audioBuffer[i].ownerPin=-1;
		demux_device->audioBuffer[i].ownerChannel = -1;
		demux_device->audioBuffer[i].status = BUF_STAT_OFFLINE;
		demux_device->audioBuffer[i].bufferIndex = i;
	}
}
#endif //DEMUX_AUDIO_USE_GLOBAL_BUFFER

//########################################################## PES BUFFER ##################################################
#ifdef DEMUX_PES_BUFFER_DYNAMIC_ALLOC
int DEMUX_PESBuffer_Alloc(DEMUX_PES_MAP_T *pMap)
{
	RINGBUFFER_HEADER *pRBH = NULL;
	if (pMap == NULL){
		dmx_err(CHANNEL_UNKNOWN, "%s-%d : invalid param \n", __func__, __LINE__);
		return -1;
	}

	if (((pMap->phyAddr) != 0) || (pMap->pesFilterBuffer.phyAddr != 0))
	{
		dmx_err(CHANNEL_UNKNOWN, "%s-%d : pes buffer is already allocated \n", __func__, __LINE__);
		return -1;
	}
	if (pMap->rbHeader.virAddr == NULL){
		if (DEMUX_BUF_AllocFromPoll(&pMap->rbHeader, sizeof(RINGBUFFER_HEADER), &demux_device->bufPoll, 0) < 0){
			dmx_err(CHANNEL_UNKNOWN, "%s_%d, pes allocate header fail!\n", __func__, __LINE__);
			return -1;
		}
	}
	if (DEMUX_BUF_AllocBuffer(&(pMap->pesFilterBuffer), PES_FILTER_RING_BUFFER_SIZE, 1, 0) < 0){
		dmx_err(CHANNEL_UNKNOWN, "%s-%d : pes Buffer Allocate fail!\n", __func__, __LINE__);
		return -1;
	}

	pRBH = (RINGBUFFER_HEADER *)pMap->rbHeader.virAddr;
	if (pRBH == NULL){
		dmx_err(CHANNEL_UNKNOWN, "%s-%d : pes header buffer not be allocate !\n", __func__, __LINE__);
		DEMUX_BUF_FreeBuffer(&(pMap->pesFilterBuffer));
		return -1;
	}

	pMap->virAddr = pMap->pesFilterBuffer.virAddr;
	pMap->phyAddr = (unsigned int)pMap->pesFilterBuffer.phyAddr;

	pRBH->beginAddr    = (unsigned long)pMap->phyAddr;
	pRBH->size         = PES_FILTER_RING_BUFFER_SIZE;
	pRBH->writePtr     = pRBH->beginAddr;
	pRBH->numOfReadPtr = 1;
	pRBH->readPtr[0]   = pRBH->beginAddr;
	pMap->pWrPtr        = (u32*)&pRBH->writePtr;
	pMap->pRdPtr        = (u32*)&pRBH->readPtr[0];
	pMap->pBufferLower  = pRBH->beginAddr;
	pMap->pBufferUpper  = (pMap->pBufferLower + PES_FILTER_RING_BUFFER_SIZE);
	pMap->phyAddrOffset = (long) pMap->phyAddr - (long) pMap->virAddr;
	dmx_mask_print(DEMUX_PES_DEBUG, CHANNEL_UNKNOWN,"DEMUX_PESBuffer_Alloc pes buffer :  phyAddr:%x , vir:%p, size:%x\n", (u32)pRBH->beginAddr, pMap->virAddr,PES_FILTER_RING_BUFFER_SIZE);
	pMap->bufferStatus = BUFFER_NEW;
	return 0;


}
int DEMUX_PESBuffer_Free(DEMUX_PES_MAP_T *pMap)
{
	if (pMap == NULL){
		dmx_err(CHANNEL_UNKNOWN,"%s-%d : invalid param \n", __func__, __LINE__);
		return -1;
	}
	if (((pMap->phyAddr) == 0) || (pMap->pesFilterBuffer.phyAddr == 0))
	{
		dmx_err(CHANNEL_UNKNOWN,"%s-%d : pes buffer is not allocated \n", __func__, __LINE__);
		return 0;
	}

	if (DEMUX_BUF_FreeBuffer(&(pMap->pesFilterBuffer))<0)
	{
		dmx_err(CHANNEL_UNKNOWN,"FAIL!!!!: fail to free pes buffer\n");
		return -1;
	}

	pMap->virAddr = NULL;
	pMap->phyAddr = 0;

	return 0;

}
void DEMUX_PESBuffer_UpdateBuferStatus(demux_channel *pCh)
{
	int index  = 0;
	if (pCh == NULL){
		dmx_err(CHANNEL_UNKNOWN,"%s-%d : invalid param \n", __func__, __LINE__);
		return ;
	}
	for (index =0; index < MAX_PID_FILTER_CB_NUM; index++){
		DEMUX_PES_MAP_T *pMap = &pCh->pesMap[index];
		if (pMap->bufferStatus == BUFFER_FREE){
			dmx_mask_print(DEMUX_PES_DEBUG, pCh->tunerHandle,"updatebufferstatus  free pes buffer : ch: %d, pesId:%d \n", pCh->tunerHandle, index);
			if (DEMUX_PESBuffer_Free(pMap) < 0){
				dmx_err(pCh->tunerHandle, DGB_COLOR_RED"FAIL!!!!: fail to free pes buffer : ch:%d, pesid:%d\n"DGB_COLOR_NONE, pCh->tunerHandle, index);
			}
			else{
				if (pCh->isECP){
					if (DEMUX_TA_ECP_UpdatePESBufferinfo(pCh, index, 0)<0)
						dmx_err(pCh->tunerHandle, "%s_%d ECP RELEASE BUFFER FAIL !!!!\n", __func__, __LINE__);
				}
				pMap->bufferStatus = BUFFER_OFFLINE;
			}
		}

		if (pMap->bufferStatus == BUFFER_NEW){
			RINGBUFFER_HEADER *pRBH = (RINGBUFFER_HEADER *)pMap->rbHeader.virAddr;
			if (pRBH->reserve2 == DEMUX_PESBUFFER_OP_PENDING_FOR_INIT){//this bit set by hal,to notify kernel do init
				pRBH->writePtr     = pRBH->beginAddr;
				pRBH->numOfReadPtr = 1;
				pRBH->readPtr[0]   = pRBH->beginAddr;
				pRBH->readPtr[1]   = pRBH->beginAddr;

				pMap->unitSize = 0 ;
				pMap->size = 0 ;
				pMap->curWr = (unsigned char *)(uintptr_t)pRBH->beginAddr;

				pRBH->reserve2 = DEMUX_PESBUFFER_OP_INITIALIZED;
				dmx_mask_print(DEMUX_PES_DEBUG, pCh->tunerHandle,"updatebufferstatus reset pes buffer :ch:%d, pesid:%d, phyAddr:%x \n", pCh->tunerHandle, index, (u32)pRBH->beginAddr);
			}
			if (pCh->isECP){
				if (DEMUX_TA_ECP_UpdatePESBufferinfo(pCh, index, 1)<0)
					dmx_err(pCh->tunerHandle, "%s_%d ECP SETUP  BUFFER FAIL !!!!\n", __func__, __LINE__);
			}
			pMap->bufferStatus = BUFFER_ONLINE;
		}
	}
}
#endif //DEMUX_PES_BUFFER_DYNAMIC_ALLOC
/*==========================================================
*			TP BUFFER
*=========================================================
*/

int DEMUX_BUF_AllocateTPBuffer(demux_channel *pCh)
{
	DEMUX_BUF_T *pBuf = NULL;
	unsigned int size = 0;
	if (pCh == NULL){
		dmx_err(CHANNEL_UNKNOWN,"%s-%d : invalid param \n", __func__, __LINE__);
		goto ALLOCATE_FAIL;
	}
	pBuf = &pCh->tpBuffer;
	if (pCh->bIsSDT) {
		size = SDT_CH_BUFF_SIZE;
	} else {
#ifdef USE_STATIC_RESERVED_BUF
		unsigned int tpBufferZoneSize = 0;
		unsigned int tpBufferZoneStartPhyAddr = 0;
		unsigned int tpBufferPhyAddr =0;
		size = DEMUX_TP_BUFF_SIZE;
		//unsigned long phyAddr = DEMUX_TP_BUFFER_START + chNum * size;
		tpBufferZoneSize = carvedout_buf_query(CARVEDOUT_TP,(void**)&tpBufferZoneStartPhyAddr);
		dmx_notice(pCh->tunerHandle,"tpBuffer=> startPhyAdd:0x%x, totalSize:%d, ch:%d, chTpSize:%d\n",
				tpBufferZoneStartPhyAddr, tpBufferZoneSize, pCh->tunerHandle, size);
		//if ((phyAddr+size) > (DEMUX_TP_BUFFER_START + DEMUX_TP_BUFFER_SIZE))
		//	return DEMUX_BUF_AllocBuffer(pBuf, size, 0, 0);
		if (tpBufferZoneSize > 0){
			tpBufferPhyAddr = tpBufferZoneStartPhyAddr + (pCh->tunerHandle)*size;
			if ((tpBufferPhyAddr+size) <= (tpBufferZoneStartPhyAddr+tpBufferZoneSize)){
				pBuf->phyAddr = tpBufferPhyAddr;
				//pBuf->allocSize = size < 0x1000 ? 0x1000 : size ;
				pBuf->allocSize  = size;
				pBuf->cachedaddr = dvr_remap_cached_memory(pBuf->phyAddr, pBuf->allocSize, __builtin_return_address(0));
				pBuf->nonCachedaddr = dvr_remap_uncached_memory(pBuf->phyAddr, pBuf->allocSize, __builtin_return_address(0));
				pBuf->isCache = 0;
				pBuf->virAddr = pBuf->isCache ? pBuf->cachedaddr : pBuf->nonCachedaddr ;
				pBuf->size = size ;
				pBuf->fromPoll = 0 ;
				dmx_notice(pCh->tunerHandle,"[%s]TP[%d] cachedAddr %p, uncachedAddr %p, phyAddr 0x%lx, allocSize 0x%x\n",
					__func__, pCh->tunerHandle, pBuf->cachedaddr, pBuf->nonCachedaddr,(ULONG)pBuf->phyAddr, pBuf->allocSize);
				goto ALLOCATE_SUCCESS;
			}else {
				dmx_warn(pCh->tunerHandle, "%s_%d:  carvedout TP buffer allocate FAIL \n!!!!!", __func__, __LINE__);
			}
		} else {
			dmx_warn(pCh->tunerHandle, "%s_%d:  carvedout TP buffer allocate FAIL \n!!!!!", __func__, __LINE__);
		}
#endif
	}
	if (DEMUX_BUF_AllocBuffer(pBuf, size, 0, 0) < 0){
		dmx_err(pCh->tunerHandle, "%s_%d:  DEMUX_BUF_AllocBuffer\n!!!!!", __func__, __LINE__);
		goto ALLOCATE_FAIL;
	}

ALLOCATE_SUCCESS:
	pCh->phyAddrOffset = (long)pCh->tpBuffer.phyAddr - (long)pCh->tpBuffer.virAddr;
	if (DEMUX_BUF_AllocFromPoll(&pCh->tpBufferH, sizeof(RINGBUFFER_HEADER), &demux_device->bufPoll, 0) < 0){
		dmx_err(pCh->tunerHandle, "%s_%d:  DEMUX_BUF_AllocFromPoll tpBufferH FAIL\n!!!!!", __func__, __LINE__ );
		goto ALLOCATE_FAIL;
	}
	return 0 ;
ALLOCATE_FAIL:
	return -1;
}

int DEMUX_BUF_FreeTPBuffer(demux_channel *pCh)
{
	DEMUX_BUF_T *pBuf = NULL;
	if (pCh == NULL){
		dmx_err(CHANNEL_UNKNOWN,"%s-%d : invalid param \n", __func__, __LINE__);
		goto FREE_FAIL;
	}
	pBuf = &pCh->tpBuffer;
#ifdef USE_STATIC_RESERVED_BUF
	if (pCh->bIsSDT == 0){
		unsigned int tp_buf_start = 0;
		unsigned int tp_buf_size =0;
		tp_buf_size = carvedout_buf_query(CARVEDOUT_TP, (void**) &tp_buf_start);
		if (pBuf->phyAddr >= tp_buf_start && (pBuf->phyAddr+pBuf->allocSize) <= (tp_buf_start + tp_buf_size)) {
			dvr_unmap_memory((void *)pBuf->cachedaddr, pBuf->allocSize);
			dvr_unmap_memory((void *)pBuf->nonCachedaddr, pBuf->allocSize);
			memset(pBuf, 0, sizeof(DEMUX_BUF_T));
			dmx_notice(pCh->tunerHandle,"[%s %d] return carved-out tpBuffer \n",__func__,__LINE__);
			goto FREE_OK;
		}
	}
#endif

	if (DEMUX_BUF_FreeBuffer(pBuf) <0){
		dmx_err(pCh->tunerHandle, "%s_%d:  DEMUX_BUF_FreeBuffer TP BUFFER FAIL\n!!!!!", __func__, __LINE__ );
		goto FREE_FAIL;
	}
FREE_OK:
	if (DEMUX_BUF_FreeBuffer(&pCh->tpBufferH) < 0){
		dmx_err(pCh->tunerHandle, "%s_%d:  DEMUX_BUF_FreeBuffer TP HEADER FAIL\n!!!!!", __func__, __LINE__ );
		goto FREE_FAIL;
	}
	return 0;
FREE_FAIL:
	return -1;
}

int DEMUX_BUF_ConfigTPBuffer(demux_channel *pCh)
{
	SINT32 tpAlignment, tpBufAlignment, mtpAlignment;
	TPK_TP_STATUS_T tpStatus;
	UINT8 mtp_id,tpp_mtp_id;
	int tpBufSize, tpDataSize;
	int32_t ret = -1;
	if (pCh == NULL) {
		dmx_err(CHANNEL_UNKNOWN, "%s_%d: invalid param !!!!\n", __func__, __LINE__);
		return -1;
	}

	if (RHAL_GetTpStatus(pCh->tunerHandle, &tpStatus) != TPK_SUCCESS)
		return -1;

	mtp_id = DEMUX_GET_MTP_ID(pCh->tunerHandle);
	tpp_mtp_id = DEMUX_GET_TPP_MTP_ID(pCh->tunerHandle);
	//WOSQRTK-12161: Record buffer need to meet LG MW threshold 8*96*1024 alignment
	tpBufAlignment = tpStatus.tp_param.prefix_en ? (MIN_UPLOAD_SIZE_FOR_192*4) : (188*1024*2);
	tpAlignment    = tpStatus.tp_param.prefix_en ? 192 : 188;
	mtpAlignment   = (tpStatus.tp_param.ts_in_tsp_len == byte_192) ? 192 : 188;
	pCh->tsPacketSize  = tpAlignment;

	ret = RHAL_GetTPBufferStatus(pCh->tunerHandle, &tpBufSize, &tpDataSize) ;
	if ((ret != TPK_SUCCESS)  && (ret != TPK_NOT_INIT)) {
		dmx_err(pCh->tunerHandle, "[%s %d] FAIL RHAL_GetTPBufferStatus!!!!! \n", __func__, __LINE__);
		return -1;
	}
	/* buffer is packet-aligned. */
	if (((pCh->tpBuffer.size % tpBufAlignment) == 0)  && (tpBufSize == pCh->tpBuffer.size)) {
		if ((tpStatus.tp_src != MTP)&&(pCh->tppMtpBuffer.allocSize==0))
			return 0;

	}
	/* if tp starts streaming already, stop it. */
	if (tpStatus.tp_stream_status == TP_STREAM_START)
		RHAL_TPStreamControl(pCh->tunerHandle, TP_STREAM_STOP);

	if ((pCh->tpBuffer.size % tpBufAlignment) || (pCh->tpBuffer.size != tpBufSize)) {
		RINGBUFFER_HEADER *pHeader    = (RINGBUFFER_HEADER *)pCh->tpBufferH.virAddr;
		DEMUX_BUF_T       *pBuf       = &pCh->tpBuffer;

		pBuf->size = pBuf->size - (pBuf->size % tpBufAlignment);
		RHAL_SetTPRingBuffer(pCh->tunerHandle, (UINT8 *)pBuf->phyAddr, (UINT8 *)pBuf->nonCachedaddr, pBuf->size);

		tpBufSize = 0;
		tpDataSize = 0;
		if (RHAL_GetTPBufferStatus(pCh->tunerHandle, &tpBufSize, &tpDataSize) == TPK_SUCCESS && tpBufSize != pBuf->size) {
			dmx_crit(pCh->tunerHandle, "[%s %d] buf size not matched: tpBufSize = %d, org = %d\n", __func__, __LINE__, tpBufSize,  pBuf->size);
			pBuf->size = tpBufSize;
		}

		//pHeader->size             = pBuf->size;
		//pHeader->beginAddr        = pBuf->phyAddr;
		//pHeader->writePtr         = pBuf->phyAddr;
		//pHeader->readPtr[0]       = pBuf->phyAddr;
		//pHeader->readPtr[1]       = pBuf->phyAddr;

		DEMUX_BUF_ResetRBHeader(&pCh->tpBufferH, &pCh->tpBuffer, RINGBUFFER_STREAM, 0);
		dmx_info(pCh->tunerHandle, "[%s %d] TP readPtr[0] = 0x%x, readPtr[1] = 0x%x, writePtr = 0x%x\n",
			__func__, __LINE__, (u32)pHeader->readPtr[0], (u32)pHeader->readPtr[1], (u32)pHeader->writePtr
		);
		if (RHAL_DumpTPRingBuffer(pCh->tunerHandle, pCh->bStartRec, (UINT8 *)&pHeader->readPtr[0], (UINT8 *)&pHeader->writePtr) != TPK_SUCCESS)
			return -1;


	}

	if (tpStatus.tp_src == MTP && pCh->mtpBuffer.allocSize > 0) {
		pCh->mtpBuffer.size = pCh->mtpBuffer.allocSize - (pCh->mtpBuffer.allocSize % (mtpAlignment*1024)) - mtpAlignment*1024; /*reserve one 192k to handle wrap-around*/
		dmx_crit(pCh->tunerHandle, "[%s %d] pCh->mtpBuffer.phyAddr = 0x%lx\n", __func__, __LINE__, (ULONG)pCh->mtpBuffer.phyAddr);
		RHAL_SetMTPBuffer(mtp_id, (UINT8 *)pCh->mtpBuffer.phyAddr, (UINT8 *)pCh->mtpBuffer.virAddr, pCh->mtpBuffer.size);
	}

	if (pCh->tppMtpBuffer.allocSize > 0) {
		pCh->tppMtpBuffer.size = pCh->tppMtpBuffer.allocSize;
		dmx_crit(pCh->tunerHandle, "[%s %d] pCh->mtpBuffer.phyAddr = 0x%lx\n", __func__, __LINE__, (ULONG)pCh->tppMtpBuffer.phyAddr);
		RHAL_SetMTPBuffer(tpp_mtp_id, (UINT8 *)pCh->tppMtpBuffer.phyAddr, (UINT8 *)pCh->tppMtpBuffer.virAddr, pCh->tppMtpBuffer.size);
	}

	/* re-start tp if necessary */
	if (tpStatus.tp_stream_status == TP_STREAM_START)
		RHAL_TPStreamControl(pCh->tunerHandle, TP_STREAM_START);

	return 0;
}


int  DEMUX_BUF_MTPBuffer_Create(demux_channel *pCh)
{

	DEMUX_BUF_T *pTpBuf = NULL;
	if (pCh == NULL)
	{
		dmx_err	(CHANNEL_UNKNOWN, "%s_%d: invalid param !!!!!!!!!\n", __func__, __LINE__);
		return -1;
	}
	if (pCh->mtpBuffer.allocSize != 0)
		return 0;
	pTpBuf = &pCh->tpBuffer;
#ifdef SHARE_TPBUFFER_WITH_MTPBUFFER
	if (pTpBuf->allocSize == 0)
	{
		dmx_err(pCh->tunerHandle, "%s_%d: tp buffer still not allocate yet!!!\n", __func__, __LINE__);
		return -1;
	}

	//reduce tp size
	pTpBuf->size = DEMUX_TP_BUFF_SIZE - MTP_BUFFER_SIZE;
	// config mtp buffer
	pCh->mtpBuffer.phyAddr = pTpBuf->phyAddr + pTpBuf->size;
	pCh->mtpBuffer.allocSize = MTP_BUFFER_SIZE;
	pCh->mtpBuffer.cachedaddr = pTpBuf->cachedaddr + pTpBuf->size;
	pCh->mtpBuffer.nonCachedaddr = pTpBuf->nonCachedaddr + pTpBuf->size;
	pCh->mtpBuffer.isCache = 0;
	pCh->mtpBuffer.virAddr = pTpBuf->virAddr + pTpBuf->size;
	pCh->mtpBuffer.size = MTP_BUFFER_SIZE;
	pCh->mtpBuffer.fromPoll = 0 ;
	dmx_crit(pCh->tunerHandle, "%s_%d: MTP share TP buffer: tp (phy:%lx, vir:%p, size:%d/%d), mtp (phy:%lx, vir:%p, size:%d/%d)\n",
		__func__, __LINE__, (ULONG)pTpBuf->phyAddr, pTpBuf->virAddr, pTpBuf->size, pTpBuf->allocSize,
		(ULONG)pCh->mtpBuffer.phyAddr, pCh->mtpBuffer.virAddr, pCh->mtpBuffer.size, pCh->mtpBuffer.allocSize);
	DEMUX_BUF_ResetRBHeader(&pCh->tpBufferH, &pCh->tpBuffer, RINGBUFFER_STREAM, 0);

#else

	if (DEMUX_BUF_AllocBuffer(&pCh->mtpBuffer, MTP_BUFFER_SIZE, 1, 0) < 0) {
		dmx_crit(pCh->tunerHandle, "[%s %d] fail to allocate MTP buffer\n", __func__, __LINE__);
		return -1;
	}
#endif
	return 0;
}

int DEMUX_BUF_MTPBuffer_Destroy(demux_channel *pCh)
{

#ifdef SHARE_TPBUFFER_WITH_MTPBUFFER
	DEMUX_BUF_T *pTpBuf = NULL;
#endif
	if (pCh == NULL)
	{
		dmx_err	(CHANNEL_UNKNOWN, "%s_%d: invalid param !!!!!!!!!\n", __func__, __LINE__);
		return -1;
	}
#ifdef SHARE_TPBUFFER_WITH_MTPBUFFER
	pTpBuf = &pCh->tpBuffer;
	if (pCh->mtpBuffer.allocSize == 0)
		return 0;

	pTpBuf->size = DEMUX_TP_BUFF_SIZE;

	memset(&pCh->mtpBuffer, 0 , sizeof(DEMUX_BUF_T));
	DEMUX_BUF_ConfigTPBuffer(pCh);
	dmx_crit(pCh->tunerHandle, "%s_%d: MTP share TP buffer: tp (phy:%lx, vir:%p, size:%d/%d), mtp (phy:%lx, vir:%p, size:%d/%d)\n",
		__func__, __LINE__, (ULONG)pTpBuf->phyAddr, pTpBuf->virAddr, pTpBuf->size, pTpBuf->allocSize,
		(ULONG)pCh->mtpBuffer.phyAddr, pCh->mtpBuffer.virAddr, pCh->mtpBuffer.size, pCh->mtpBuffer.allocSize);

#else

	DEMUX_BUF_FreeBuffer(&pCh->mtpBuffer);
#endif
	return 0;
}
void DEMUX_BUF_InitVTP(void)
{
	int idx =0;
	for (idx =0 ; idx< MAX_VDEC_NUM; idx++) {
		DEMUX_VTP_T *p_vtp = &demux_device->VTP_Info[idx];
		p_vtp->SrcCh = 0xFF;
		p_vtp->destPort = 0xFF;
		p_vtp->vBufStatus = BUF_STAT_DONE;
		memset(&p_vtp->bsBufH, 0, sizeof(DEMUX_BUF_T));
		memset(&p_vtp->ibBufH, 0, sizeof(DEMUX_BUF_T));
	}
}
