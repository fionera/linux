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

#ifndef __VDMA_CARVEOUT_H__
#define __VDMA_CARVEOUT_H__

int vdma_carveout_init(dma_addr_t *chunk_addr, int *chunk_size, int chunk_cnt, int blk_size);
int vdma_carveout_alloc(int userID, int size, dma_addr_t *addr);
int vdma_carveout_free(dma_addr_t addr);
int vdma_carveout_exit(void);
int vdma_carveout_show_status(char *buf);

#endif
