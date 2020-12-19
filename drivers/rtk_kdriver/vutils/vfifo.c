/*
 * Copyright (c) 2018 Jias Huang <jias_huang@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/pageremap.h>
#include <linux/io.h>
#include <linux/uaccess.h>

#pragma scalar_storage_order big-endian
#include <rpc_common.h>
#pragma scalar_storage_order default

#include "vfifo.h"

#define GETRBH(x) ((RINGBUFFER_HEADER*)(x->pRBH))

static inline phys_addr_t __read(vfifo_t *vfifo, phys_addr_t rd, void *dst, int size)
{
	void *pRd;
	int len;

	pRd = vfifo->pBase + (rd - vfifo->base);
	len = vfifo->pLimit - pRd;
	if (len >= size) len = size;

	memcpy(dst, pRd, len);

	if (len < size) memcpy(dst + len, vfifo->pBase, size - len);

	rd = vfifo_wrap(vfifo, rd + size);

	return rd;
}

static inline phys_addr_t __read_to_user(vfifo_t *vfifo, phys_addr_t rd, void __user *dst, int size)
{
	void *pRd;
	int len;

	pRd = vfifo->pBase + (rd - vfifo->base);
	len = vfifo->pLimit - pRd;
	if (len >= size) len = size;

	if (copy_to_user(dst, pRd, len))
		return -EFAULT;

	if (len < size) {
		if (copy_to_user(dst + len, vfifo->pBase, size - len))
			return -EFAULT;
	}

	rd = vfifo_wrap(vfifo, rd + size);

	return rd;
}

static inline phys_addr_t __write(vfifo_t *vfifo, phys_addr_t wr, void *src, int size)
{
	void *pWr;
	int len;

	pWr = vfifo->pBase + (wr - vfifo->base);
	len = vfifo->pLimit - pWr;
	if (len >= size) len = size;

	memcpy(pWr, src, len);

	if (len < size) memcpy(vfifo->pBase, src + len, size - len);

	wr = vfifo_wrap(vfifo, wr + size);

	return wr;
}

static inline phys_addr_t __write_from_user(vfifo_t *vfifo, phys_addr_t wr, void __user *src, int size)
{
	void *pWr;
	int len;

	pWr = vfifo->pBase + (wr - vfifo->base);
	len = vfifo->pLimit - pWr;
	if (len >= size) len = size;

	if (copy_from_user(pWr, src, len))
		return -EFAULT;

	if (len < size) {
		if (copy_from_user(vfifo->pBase, src + len, size - len))
			return -EFAULT;
	}

	wr = vfifo_wrap(vfifo, wr + size);

	return wr;
}

#ifndef HAVE_MULTI_READ_FULL_SUPPORT
static void __updateNextRd(vfifo_t *vfifo)
{
	phys_addr_t rd;
	int i, cnt_min, cnt, idx;

	if (vfifo->nRd == 1) {
		GETRBH(vfifo)->readPtr[0] = vfifo->rd[0];
		return;
	}

	rd = GETRBH(vfifo)->readPtr[0];

	for (i=0; i<vfifo->nRd; i++) {
		if (rd == vfifo->rd[i])
			return;
	}

	idx = 0;
	cnt_min = INT_MAX;

	for (i=0; i<vfifo->nRd; i++) {
		cnt = vfifo->rd[i] - rd;
		if (cnt < 0) cnt += vfifo->size;
		if (cnt < cnt_min) {
			idx = i;
			cnt_min = cnt;
		}
	}

	GETRBH(vfifo)->readPtr[0] = vfifo->rd[idx];
}
#endif

static int allocBuf(vfifo_t *vfifo, int size)
{
	void *hdr_cached, *hdr_uncached;
	void *buf_cached, *buf_uncached;

	if (!vfifo || !size)
		return -EINVAL;

	if (vfifo->hdr.addr || vfifo->buf.addr)
		return -EFAULT;

	hdr_cached = hdr_uncached =
	buf_cached = buf_uncached = NULL;

	hdr_cached = dvr_malloc_uncached_specific(sizeof(RINGBUFFER_HEADER), GFP_DCU1, &hdr_uncached);
	if (!hdr_cached) goto err;

	buf_cached = dvr_malloc_uncached_specific(size, GFP_DCU1, &buf_uncached);
	if (!buf_cached) goto err;

	/* finalize */

	vfifo->hdr.cached   = hdr_cached;
	vfifo->hdr.uncached = hdr_uncached;
	vfifo->hdr.addr	 = dvr_to_phys(hdr_cached);
	vfifo->hdr.size	 = sizeof(RINGBUFFER_HEADER);
	vfifo->buf.cached   = buf_cached;
	vfifo->buf.uncached = buf_uncached;
	vfifo->buf.addr	 = dvr_to_phys(buf_cached);
	vfifo->buf.size	 = size;

	return 0;

err:

	if (hdr_cached) dvr_free(hdr_cached);
	if (buf_cached) dvr_free(buf_cached);

	return -ENOMEM;
}

static int useBuf(vfifo_t *vfifo, phys_addr_t rbh, phys_addr_t buf_addr, int buf_size)
{
	void *hdr_uncached, *buf_uncached;
	RINGBUFFER_HEADER *pRBH;

	hdr_uncached = buf_uncached = NULL;

	hdr_uncached = ioremap_nocache(rbh, sizeof(RINGBUFFER_HEADER));
	if (!hdr_uncached) goto err;

	if (vfifo->flags & VFIFO_F_IMPORTHDR) {
		pRBH = (RINGBUFFER_HEADER*)hdr_uncached;
		buf_addr = pRBH->beginAddr;
		buf_size = pRBH->size;
	}

	if ((vfifo->flags & VFIFO_F_RDONLY) || (vfifo->flags & VFIFO_F_WRONLY)) {
		if (buf_addr && buf_size)
			buf_uncached = ioremap_nocache(buf_addr, buf_size);
		if (!buf_uncached)
			goto err;
	}

	/* finalize */

	vfifo->hdr.uncached = hdr_uncached;
	vfifo->hdr.addr	 = rbh;
	vfifo->hdr.size	 = sizeof(RINGBUFFER_HEADER);
	vfifo->buf.uncached = buf_uncached;
	vfifo->buf.addr	 = buf_addr;
	vfifo->buf.size	 = buf_size;

	return 0;

err:

	if (hdr_uncached) iounmap(hdr_uncached);
	if (buf_uncached) iounmap(buf_uncached);

	return -ENOMEM;
}

static void initHdr(vfifo_t *vfifo, int type, int nRd)
{
	phys_addr_t addr;
	int size;
	RINGBUFFER_HEADER *pRBH;

	addr = vfifo->buf.addr;
	size = vfifo->buf.size;
	pRBH = (RINGBUFFER_HEADER*)vfifo->hdr.uncached;

	memset(pRBH, 0, sizeof(RINGBUFFER_HEADER));

	pRBH->bufferID	  = type;
	pRBH->size		  = size;
	pRBH->numOfReadPtr  = nRd;
	pRBH->beginAddr	 =
	pRBH->writePtr	  =
	pRBH->readPtr[0]	=
	pRBH->readPtr[1]	=
	pRBH->readPtr[2]	=
	pRBH->readPtr[3]	= addr;
}

static void initCache(vfifo_t *vfifo)
{
	phys_addr_t addr;
	int size;
	RINGBUFFER_HEADER *pRBH;

	pRBH = (RINGBUFFER_HEADER*)vfifo->hdr.uncached;
	addr = pRBH->beginAddr;
	size = pRBH->size;

	vfifo->rd[0]	= pRBH->readPtr[0];
	vfifo->rd[1]	= pRBH->readPtr[1];
	vfifo->rd[2]	= pRBH->readPtr[2];
	vfifo->rd[3]	= pRBH->readPtr[3];
	vfifo->wr	   = pRBH->writePtr;
	vfifo->base	 = addr;
	vfifo->limit	= addr + size;
	vfifo->size	 = size;
	vfifo->pBase	= vfifo->buf.uncached;
	vfifo->pLimit   = vfifo->buf.uncached + size;
	vfifo->pRBH	 = vfifo->hdr.uncached;
	vfifo->nRd	  = pRBH->numOfReadPtr;
}

int vfifo_reset(vfifo_t *vfifo, int type, int nRd)
{
	if (!vfifo || !nRd)
		return -EINVAL;

	initHdr(vfifo, type, nRd);
	initCache(vfifo);

	return 0;
}
EXPORT_SYMBOL(vfifo_reset);

int vfifo_init(vfifo_t *vfifo, int type, int size, int nRd, int flags)
{
	int ret;

	if (!vfifo || !size || !nRd)
		return -EINVAL;

	memset(vfifo, 0, sizeof(vfifo_t));
	vfifo->flags = flags | VFIFO_F_MALLOC;

	ret = allocBuf(vfifo, size);
	if (ret) return ret;

	initHdr(vfifo, type, nRd);
	initCache(vfifo);

	return 0;
}
EXPORT_SYMBOL(vfifo_init);

int vfifo_init_importHdr(vfifo_t *vfifo, phys_addr_t rbh, int flags)
{
	int ret;

	if (!vfifo || !rbh)
		return -EINVAL;

	memset(vfifo, 0, sizeof(vfifo_t));
	vfifo->flags = flags | VFIFO_F_IMPORTHDR;

	ret = useBuf(vfifo, rbh, 0, 0);
	if (ret) return ret;

	initCache(vfifo);

	return 0;
}
EXPORT_SYMBOL(vfifo_init_importHdr);

int vfifo_init_NoAlloc(vfifo_t *vfifo, phys_addr_t rbh,
		phys_addr_t buf_addr, int buf_size, int type, int nRd, int flags)
{
	int ret;

	if (!vfifo || !rbh || !buf_addr || !buf_size || !nRd)
		return -EINVAL;

	memset(vfifo, 0, sizeof(vfifo_t));
	vfifo->flags = flags;

	ret = useBuf(vfifo, rbh, buf_addr, buf_size);
	if (ret) return ret;

	initHdr(vfifo, type, nRd);
	initCache(vfifo);

	return 0;
}
EXPORT_SYMBOL(vfifo_init_NoAlloc);

int vfifo_exit(vfifo_t *vfifo)
{
	if (!vfifo)
		return -EINVAL;

	if (vfifo->flags & VFIFO_F_MALLOC) {
		if (vfifo->hdr.cached) dvr_free(vfifo->hdr.cached);
		if (vfifo->buf.cached) dvr_free(vfifo->buf.cached);
	}

	else {
		if (vfifo->hdr.uncached) iounmap(vfifo->hdr.uncached);
		if (vfifo->buf.uncached) iounmap(vfifo->buf.uncached);
	}


	memset(vfifo, 0, sizeof(vfifo_t));

	return 0;
}
EXPORT_SYMBOL(vfifo_exit);

int vfifo_read(vfifo_t *vfifo, int idx, void *data, int size)
{
	phys_addr_t rd;

	if (!vfifo || !vfifo->pRBH || !data || !size)
		return -EINVAL;

	if (idx < 0 || idx >= vfifo->nRd)
		return -EINVAL;

	rd = vfifo->rd[idx];
	rd = __read(vfifo, rd, data, size);

	vfifo->rd[idx] = rd;

	#ifdef HAVE_MULTI_READ_FULL_SUPPORT
	GETRBH(vfifo)->readPtr[idx] = rd;
	#else
	__updateNextRd(vfifo);
	#endif

	return 0;
}
EXPORT_SYMBOL(vfifo_read);

int vfifo_read_to_user(vfifo_t *vfifo, int idx, void __user *userptr, int size)
{
	phys_addr_t rd;

	if (!vfifo || !vfifo->pRBH || !userptr || !size)
		return -EINVAL;

	if (idx < 0 || idx >= vfifo->nRd)
		return -EINVAL;

	rd = vfifo->rd[idx];
	rd = __read_to_user(vfifo, rd, userptr, size);

	vfifo->rd[idx] = rd;

	#ifdef HAVE_MULTI_READ_FULL_SUPPORT
	GETRBH(vfifo)->readPtr[idx] = rd;
	#else
	__updateNextRd(vfifo);
	#endif

	return 0;
}
EXPORT_SYMBOL(vfifo_read_to_user);

int vfifo_write(vfifo_t *vfifo, void *data, int size)
{
	phys_addr_t wr;

	if (!vfifo || !vfifo->pRBH || !data || !size)
		return -EINVAL;

	wr = vfifo->wr;
	wr = __write(vfifo, wr, data, size);

	vfifo->wr = wr;
	GETRBH(vfifo)->writePtr = wr;

	return 0;
}
EXPORT_SYMBOL(vfifo_write);

int vfifo_write_from_user(vfifo_t *vfifo, void __user *userptr, int size)
{
	phys_addr_t wr;

	if (!vfifo || !vfifo->pRBH || !userptr || !size)
		return -EINVAL;

	wr = vfifo->wr;
	wr = __write_from_user(vfifo, wr, userptr, size);

	vfifo->wr = wr;
	GETRBH(vfifo)->writePtr = wr;

	return 0;
}
EXPORT_SYMBOL(vfifo_write_from_user);

int vfifo_syncWr(vfifo_t *vfifo, phys_addr_t next)
{
	if (!vfifo || !vfifo->pRBH)
		return -EINVAL;

	if (next)
		vfifo->wr = GETRBH(vfifo)->writePtr = next;
	else
		vfifo->wr = GETRBH(vfifo)->writePtr;

	return 0;
}
EXPORT_SYMBOL(vfifo_syncWr);

int vfifo_syncRd(vfifo_t *vfifo, int idx, phys_addr_t next)
{
	if (!vfifo || !vfifo->pRBH)
		return -EINVAL;

	if (idx < 0 || idx >= vfifo->nRd)
		return -EINVAL;

	#ifdef HAVE_MULTI_READ_FULL_SUPPORT
	if (next)
		vfifo->rd[idx] = GETRBH(vfifo)->readPtr[idx] = next;
	else
		vfifo->rd[idx] = GETRBH(vfifo)->readPtr[idx];
	#else
	if (next) {
		vfifo->rd[idx] = next;
		__updateNextRd(vfifo);
	}
	#endif

	return 0;
}
EXPORT_SYMBOL(vfifo_syncRd);

int vfifo_cnt(vfifo_t *vfifo, int idx)
{
	int ret;

	if (!vfifo || !vfifo->pRBH)
		return -EINVAL;

	if (idx < 0 || idx >= vfifo->nRd)
		return -EINVAL;

	if (!(vfifo->flags & VFIFO_F_RDONLY))
		vfifo->rd[idx] = GETRBH(vfifo)->readPtr[idx];

	if (!(vfifo->flags & VFIFO_F_WRONLY))
		vfifo->wr = GETRBH(vfifo)->writePtr;

	ret = vfifo->wr - vfifo->rd[idx];
	if (ret < 0) ret += vfifo->size;

	return ret;
}
EXPORT_SYMBOL(vfifo_cnt);

int vfifo_space(vfifo_t *vfifo)
{
	int ret, i, len;

	if (!vfifo || !vfifo->pRBH)
		return -EINVAL;

	ret = vfifo->size;

	if (!(vfifo->flags & VFIFO_F_RDONLY)) {
		for (i=0; i<vfifo->nRd; i++)
			vfifo->rd[i] = GETRBH(vfifo)->readPtr[i];
	}

	if (!(vfifo->flags & VFIFO_F_WRONLY))
		vfifo->wr = GETRBH(vfifo)->writePtr;

	for (i=0; i<vfifo->nRd; i++) {
		len = vfifo->rd[i] - vfifo->wr - 1;
		if (len < 0) len += vfifo->size;
		if (len < ret) ret = len;
	}

	return ret;
}
EXPORT_SYMBOL(vfifo_space);

int vfifo_peek(vfifo_t *vfifo, int idx, void *bufOnWrap, int size, void **ppNext)
{
	phys_addr_t rd;
	void *ret;

	if (!vfifo || !size || !bufOnWrap || !ppNext)
		return -EINVAL;

	if (idx < 0 || idx >= vfifo->nRd)
		return -EINVAL;

	rd = vfifo->rd[idx];

	if (rd + size > vfifo->limit) {
		__read(vfifo, rd, bufOnWrap, size);
		ret = bufOnWrap;
	}

	else {
		ret = vfifo->pBase + (rd - vfifo->base);
	}

	*ppNext = ret;

	return 0;
}
EXPORT_SYMBOL(vfifo_peek);

int vfifo_skip(vfifo_t *vfifo, int idx, int size)
{
	phys_addr_t rd;

	if (!vfifo || !size)
		return -EINVAL;

	if (idx < 0 || idx >= vfifo->nRd)
		return -EINVAL;

	rd = vfifo_wrap(vfifo, vfifo->rd[idx] + size);

	vfifo->rd[idx] = rd;

	#ifdef HAVE_MULTI_READ_FULL_SUPPORT
	GETRBH(vfifo)->readPtr[idx] = rd;
	#else
	__updateNextRd(vfifo);
	#endif

	return 0;
}
EXPORT_SYMBOL(vfifo_skip);
