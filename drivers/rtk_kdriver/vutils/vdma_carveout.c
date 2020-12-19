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

#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>
#include <linux/errno.h>
#include <linux/mm.h>

#include "vdma_carveout.h"

#define MAX_BUFS_IN_ONE_BLK 16

typedef struct priv_buf_t {
	dma_addr_t addr;
	dma_addr_t limit;
	int size;
	int refcnt;
} priv_buf_t;

typedef struct priv_blk_t {
	struct list_head list;
	struct priv_user_t *user;
	dma_addr_t addr;
	dma_addr_t limit;
	int size;
	int refcnt;
	int buf_cnt;
	priv_buf_t buf[MAX_BUFS_IN_ONE_BLK];
} priv_blk_t;

typedef struct priv_user_t {
	struct list_head list;
	struct list_head blk_list;
	int userID;
} priv_user_t;

typedef struct priv_t {
	struct list_head user_list;
	int blk_cnt;
	priv_blk_t blk[0];
} priv_t;

static DEFINE_MUTEX(lock);
static priv_t *priv = NULL;

static int get_remaining_buf(priv_user_t *user, int size, dma_addr_t *addr)
{
	int i;
	priv_blk_t *blk;

	list_for_each_entry (blk, &user->blk_list, list) {
		if (blk->refcnt >= blk->buf_cnt) continue;
		for (i=0; i<blk->buf_cnt; i++) {
			if (!blk->buf[i].refcnt && blk->buf[i].size >= size) {
				blk->buf[i].refcnt++;
				blk->refcnt++;
				*addr = blk->buf[i].addr;
				return 0;
			}
		}
	}

	return -1;
}

static int alloc_new_buf(priv_user_t *user, int size, dma_addr_t *pAddr)
{
	int i, j;
	priv_blk_t *blk;
	priv_buf_t *buf;
	dma_addr_t addr;

	for (i=0; i<priv->blk_cnt; i++) {
		blk = &priv->blk[i];
		if (!blk->refcnt) {
			blk->user = user;
			blk->buf_cnt = priv->blk[i].size / size;
			blk->refcnt = 1;
			addr = blk->addr;
			for (j=0; j<blk->buf_cnt; j++) {
				buf = &blk->buf[j];
				buf->addr = addr;
				buf->limit = addr + size;
				buf->size = size;
				buf->refcnt = 0;
				addr += size;
			}
			list_add_tail(&blk->list, &user->blk_list);
			blk->buf[0].refcnt = 1;
			*pAddr = blk->buf[0].addr;
			return 0;
		}
	}

	return -1;
}

static int carveout_init(dma_addr_t *chunk_addr, int *chunk_size, int chunk_cnt, int blk_size)
{
	int i, j, blk_cnt;
	void *base;
	priv_blk_t *blk;
	dma_addr_t addr;

	if (priv)
		return 0;

	if (!chunk_addr || !chunk_size || !chunk_cnt || !blk_size)
		return -EINVAL;

	blk_cnt = 0;
	for (i=0; i<chunk_cnt; i++)
		blk_cnt += chunk_size[i] / blk_size;

	base = vzalloc(sizeof(priv_t) + sizeof(priv_blk_t) * blk_cnt);
	if (!base) {
		pr_err("vdmabuf: fail to alloc base\n");
		return -1;
	}

	priv = base;
	priv->blk_cnt = blk_cnt;
	INIT_LIST_HEAD(&priv->user_list);
	blk = priv->blk;

	for (i=0; i<chunk_cnt; i++) {
		blk_cnt = chunk_size[i] / blk_size;
		addr = chunk_addr[i];
		for (j=0; j<blk_cnt; j++) {
			blk[j].addr = addr;
			blk[j].limit = addr + blk_size;
			blk[j].size = blk_size;
			addr += blk_size;
		}
		blk += blk_cnt;
	}

	return 0;
}

static int carveout_alloc(int userID, int size, dma_addr_t *addr)
{
	priv_user_t *user;
	int found = 0;

	if (!priv || !userID || !size || !addr)
		return -EINVAL;

	list_for_each_entry (user, &priv->user_list, list) {
		if (user->userID == userID) {
			found = 1;
			break;
		}
	}

	if (found) {
		if (!get_remaining_buf(user, size, addr))
			return 0;
	}

	else {
		user = vzalloc(sizeof(priv_user_t));
		if (!user) {
			pr_err("vdmabuf: fail to alloc user\n");
			return -1;
		}

		user->userID = userID;
		INIT_LIST_HEAD(&user->blk_list);
		list_add_tail(&user->list, &priv->user_list);
	}

	if (!alloc_new_buf(user, size, addr))
		return 0;

	return -1;
}

static int carveout_free(dma_addr_t addr)
{
	int i, j;
	priv_blk_t *blk;
	priv_buf_t *buf;
	priv_user_t *user;

	if (!priv || !addr)
		return -EPERM;

	for (i=0; i<priv->blk_cnt; i++) {
		blk = &priv->blk[i];
		user = blk->user;
		if (!blk->refcnt) continue;
		if (addr < blk->addr || addr >= blk->limit) continue;
		for (j=0; j<blk->buf_cnt; j++) {
			buf = &blk->buf[j];
			if (addr < buf->addr || addr >= buf->limit) continue;
			buf->refcnt--;
			blk->refcnt--;

			if (!blk->refcnt) {
				list_del(&blk->list);
				if (list_empty(&user->blk_list)) {
					list_del(&user->list);
					vfree(user);
				}
			}

			return 0;
		}
	}

	return -1;
}

static int carveout_exit(void)
{
	priv_user_t *user, *user_tmp;

	if (!priv)
		return 0;

	list_for_each_entry_safe (user, user_tmp, &priv->user_list, list) {
		list_del(&user->list);
		vfree(user);
	}

	vfree(priv);
	priv = NULL;

	return 0;
}

#define SHOW(fmt, ...) { \
	len += snprintf(buf + len, PAGE_SIZE - len, fmt, ##__VA_ARGS__); \
}

static int carveout_show_status(char *buf)
{
	priv_user_t *user;
	priv_blk_t *blk;
	int len, used, total_used;

	if (!priv)
		return 0;

	len = total_used = 0;

	list_for_each_entry (user, &priv->user_list, list) {
		used = 0;
		list_for_each_entry (blk, &user->blk_list, list) {
			used++;
		}
		total_used += used;
		SHOW("user 0x%x alloc %d blks\n", user->userID, used);
	}

	SHOW("total: %d alloc: %d free: %d\n", priv->blk_cnt, total_used, priv->blk_cnt - total_used);

	return len;
}

int vdma_carveout_init(dma_addr_t *chunk_addr, int *chunk_size, int chunk_cnt, int blk_size)
{
	int ret;

	mutex_lock(&lock);
	ret = carveout_init(chunk_addr, chunk_size, chunk_cnt, blk_size);
	mutex_unlock(&lock);

	return ret;
}
EXPORT_SYMBOL(vdma_carveout_init);

int vdma_carveout_exit(void)
{
	int ret;

	mutex_lock(&lock);
	ret = carveout_exit();
	mutex_unlock(&lock);

	return ret;
}
EXPORT_SYMBOL(vdma_carveout_exit);

int vdma_carveout_alloc(int userID, int size, dma_addr_t *addr)
{
	int ret;

	mutex_lock(&lock);
	size = PAGE_ALIGN(size);
	ret = carveout_alloc(userID, size, addr);
	mutex_unlock(&lock);

	return ret;
}
EXPORT_SYMBOL(vdma_carveout_alloc);

int vdma_carveout_free(dma_addr_t addr)
{
	int ret;

	mutex_lock(&lock);
	ret = carveout_free(addr);
	mutex_unlock(&lock);

	return ret;
}
EXPORT_SYMBOL(vdma_carveout_free);

int vdma_carveout_show_status(char *buf)
{
	int ret;

	mutex_lock(&lock);
	ret = carveout_show_status(buf);
	mutex_unlock(&lock);

	return ret;
}
EXPORT_SYMBOL(vdma_carveout_show_status);

