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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>

#pragma scalar_storage_order big-endian

#include "rpc_common.h"

#pragma scalar_storage_order default

#include "syswrap.h"
#include "ringbuf.h"

typedef struct ringbuf_handle_t {
	RINGBUFFER_HEADER *pRBH;
	void *pBase;
	unsigned int base;
	unsigned int wr;
	unsigned int limit;
	int size;
} ringbuf_handle_t;

int ringbuf_initHdr(unsigned int rbh, int type, unsigned int base, int size)
{
	RINGBUFFER_HEADER *pRBH;

	if (!rbh || !base || !size)
		return -1;

	pRBH = (RINGBUFFER_HEADER*)syswrap_mmap(rbh, sizeof(RINGBUFFER_HEADER));
	if (!pRBH) return -1;

	pRBH->bufferID = type;
	pRBH->size = size;
	pRBH->numOfReadPtr = 1;
	pRBH->beginAddr =
	pRBH->writePtr =
	pRBH->readPtr[0] =
	pRBH->readPtr[1] =
	pRBH->readPtr[2] =
	pRBH->readPtr[3] = base;

	syswrap_munmap(pRBH, sizeof(RINGBUFFER_HEADER));

	return 0;
}

int ringbuf_open(unsigned int rbh, void **ppHandle)
{
	ringbuf_handle_t *handle = NULL;
	RINGBUFFER_HEADER *pRBH = NULL;
	void *pBase = NULL;

	if (!rbh || !ppHandle)
		return -1;

	do {

		handle = (ringbuf_handle_t*)malloc(sizeof(ringbuf_handle_t));
		if (!handle) break;

		pRBH = (RINGBUFFER_HEADER*)syswrap_mmap(rbh, sizeof(RINGBUFFER_HEADER));
		if (!pRBH) break;

		pBase = syswrap_mmap(pRBH->beginAddr, pRBH->size);
		if (!pBase) break;

		handle->pRBH = pRBH;
		handle->pBase = pBase;
		handle->size = pRBH->size;
		handle->base = pRBH->beginAddr;
		handle->wr = pRBH->writePtr;
		handle->limit = handle->base + handle->size;
		*ppHandle = handle;

		return 0;

	} while (0);

	if (pBase) syswrap_munmap(pBase, pRBH->size);
	if (pRBH) syswrap_munmap(pRBH, sizeof(RINGBUFFER_HEADER));
	if (handle) free(handle);

	return -1;
}

int ringbuf_close(void *pHandle)
{
	ringbuf_handle_t *handle;

	if (!pHandle)
		return -1;

	handle = (ringbuf_handle_t*)pHandle;

	if (handle) {
		if (handle->pBase) syswrap_munmap(handle->pBase, handle->size);
		if (handle->pRBH) syswrap_munmap(handle->pRBH, sizeof(RINGBUFFER_HEADER));
		free(handle);
	}

	return 0;
}

int ringbuf_write(void *pHandle, void *data, int size)
{
	int space, len;
	ringbuf_handle_t *handle;
	RINGBUFFER_HEADER *pRBH;
	unsigned int rd, wr;

	if (!pHandle || !data || !size)
		return -1;

	handle = (ringbuf_handle_t*)pHandle;
	pRBH = handle->pRBH;
	rd = pRBH->readPtr[0];
	wr = handle->wr;

	space = (rd + handle->size - wr - 1) % handle->size;
	if (space < size)
		return -EAGAIN;

	while (size) {
		len = handle->limit - wr;
		if (len >= size) len = size;
		memcpy(handle->pBase + (wr - handle->base), data, len);
		wr += len;
		data += len;
		size -= len;
		if (wr >= handle->limit) wr = handle->base;
	}

	pRBH->writePtr = handle->wr = wr;

	return 0;
}

int ringbuf_getWr(void *pHandle, unsigned int *res)
{
	ringbuf_handle_t *handle;

	if (!pHandle || !res)
		return -1;

	handle = (ringbuf_handle_t*)pHandle;
	*res = handle->wr;

	return 0;
}

int ringbuf_getRd(void *pHandle, unsigned int *res)
{
	ringbuf_handle_t *handle;

	if (!pHandle || !res)
		return -1;

	handle = (ringbuf_handle_t*)pHandle;
	*res = handle->pRBH->readPtr[0];

	return 0;
}

