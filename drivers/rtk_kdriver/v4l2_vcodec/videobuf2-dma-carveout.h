/*
 * videobuf2-dma-carveout.h - DMA carveout memory allocator for videobuf2
 *
 * Copyright (C) 2010 Samsung Electronics
 *
 * Author: Pawel Osciak <pawel@osciak.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#ifndef _MEDIA_VIDEOBUF2_DMA_CARVEOUT_H
#define _MEDIA_VIDEOBUF2_DMA_CARVEOUT_H

#include <media/videobuf2-v4l2.h>
#include <linux/dma-mapping.h>

static inline dma_addr_t
vb2_dma_carveout_plane_dma_addr(struct vb2_buffer *vb, unsigned int plane_no)
{
	dma_addr_t *addr = vb2_plane_cookie(vb, plane_no);

	return *addr;
}

void *vb2_dma_carveout_init_ctx(struct device *dev);
void vb2_dma_carveout_cleanup_ctx(void *alloc_ctx);

extern const struct vb2_mem_ops vb2_dma_carveout_memops;

#endif
