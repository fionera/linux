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

#ifndef __VFIFO_H__
#define __VFIFO_H__

#define VFIFO_F_RDONLY		0x01
#define VFIFO_F_WRONLY		0x02
#define VFIFO_F_IMPORTHDR	0x04
#define VFIFO_F_MALLOC		0x08

typedef struct vfifo_mem_t {
	void *cached;
	void *uncached;
	phys_addr_t addr;
	int size;
} vfifo_mem_t;

typedef struct vfifo_t {

	vfifo_mem_t hdr;
	vfifo_mem_t buf;

	/* local variables for caching */
	phys_addr_t base;
	phys_addr_t limit;
	phys_addr_t rd[4];
	phys_addr_t wr;
	int size;
	int nRd;
	int flags;
	void *pBase;
	void *pLimit;
	void *pRBH;

} vfifo_t;

#define vfifo_wrap(vfifo, next) \
({ \
	typeof((vfifo) + 1) __q = (vfifo); \
	u32 __ret = (next); \
	(__ret >= __q->limit) ? (__ret - __q->size) : __ret; \
})

int vfifo_init(vfifo_t *vfifo, int type, int size, int nRd, int flags);
int vfifo_reset(vfifo_t *vfifo, int type, int nRd);
int vfifo_init_importHdr(vfifo_t *vfifo, phys_addr_t rbh, int flags);
int vfifo_init_NoAlloc(vfifo_t *vfifo, phys_addr_t rbh,
	phys_addr_t buf_addr, int buf_size, int type, int nRd, int flags);
int vfifo_exit(vfifo_t *vfifo);
int vfifo_read(vfifo_t *vfifo, int idx, void *data, int size);
int vfifo_read_to_user(vfifo_t *vfifo, int idx, void __user *userptr, int size);
int vfifo_peek(vfifo_t *vfifo, int idx, void *bufOnWrap, int size, void **ppNext);
int vfifo_skip(vfifo_t *vfifo, int idx, int size);
int vfifo_write(vfifo_t *vfifo, void *data, int size);
int vfifo_write_from_user(vfifo_t *vfifo, void __user *userptr, int size);
int vfifo_syncRd(vfifo_t *vfifo, int idx, u32 next);
int vfifo_syncWr(vfifo_t *vfifo, u32 next);
int vfifo_cnt(vfifo_t *vfifo, int idx);
int vfifo_space(vfifo_t *vfifo);

#endif
