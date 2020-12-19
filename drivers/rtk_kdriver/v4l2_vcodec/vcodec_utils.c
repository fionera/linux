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
#include <linux/list.h>
#include <linux/mutex.h>

#include <media/videobuf2-dma-contig.h>

#include "vcodec_conf.h"
#include "vcodec_struct.h"
#include "vcodec_utils.h"
#include "videobuf2-dma-carveout.h"

static DEFINE_MUTEX(entry_lock);
static LIST_HEAD(entry_head);

void vcodec_lockDecDevice(vcodec_device_t *device)
{
	mutex_lock(&device->share->vdec->lock);
}

void vcodec_unlockDecDevice(vcodec_device_t *device)
{
	mutex_unlock(&device->share->vdec->lock);
}

int vcodec_getFreeMsgQ(vcodec_device_t *device)
{
  int i;
  for (i = 0; i < VDEC_NUM_MSGQ; i++)
    if(!device->bQUsed[i])
    {
        device->bQUsed[i]=1;	
    	return i;
    }

  return -1;
}

void vcodec_putMsgQ(vcodec_device_t *device, int index)
{
  device->bQUsed[index]=0;
}


vdec_handle_t* vcodec_getDecByChannel(vcodec_device_t *device, int channel)
{
	struct list_head *handle_list;
	vdec_handle_t *entry, *target = NULL;

	handle_list = &device->share->vdec->handle_list;

	list_for_each_entry (entry, handle_list, list) {
		if (entry->bChannel && entry->channel == channel) {
			target = entry;
			break;
		}
	}

	return target;
}

vdec_handle_t* vcodec_getDecByDisplay(vcodec_device_t *device, int display)
{
	struct list_head *handle_list;
	vdec_handle_t *entry, *target = NULL;

	handle_list = &device->share->vdec->handle_list;

	list_for_each_entry (entry, handle_list, list) {
		if (entry->bDisplay && entry->display == display) {
			target = entry;
			break;
		}
	}

	return target;
}

vdec_handle_t* vcodec_getDecByOutputIdx(vcodec_device_t *device, int outputIdx)
{
	struct list_head *handle_list;
	vdec_handle_t *entry, *target = NULL;

	handle_list = &device->share->vdec->handle_list;

	list_for_each_entry (entry, handle_list, list) {
		if (entry->bOutputIdx && entry->outputIdx == outputIdx) {
			target = entry;
			break;
		}
	}

	return target;
}

int vcodec_initVB2AllocatorContext(vcodec_device_t *device, struct vb2_alloc_ctx *ctxs[], int cnt)
{
	int i;
	struct vb2_alloc_ctx *ctx;

	for (i=0; i<cnt; i++) {
		if (device->dma_carveout_type)
			ctx = vb2_dma_carveout_init_ctx(device->dev);
		else
			ctx = vb2_dma_contig_init_ctx(device->dev);
		if (IS_ERR(ctx)) {
			pr_err("vcodec: failed to alloc vb2 context\n");
			goto err;
		}
		ctxs[i] = ctx;
	}

	return 0;

err:
	vcodec_exitVB2AllocatorContext(device, ctxs, cnt);
	return -ENOMEM;
}

void vcodec_exitVB2AllocatorContext(vcodec_device_t *device, struct vb2_alloc_ctx *ctxs[], int cnt)
{
	int i;

	for (i=0; i<cnt; i++) {
		if (ctxs[i]) {
			if (device->dma_carveout_type)
				vb2_dma_carveout_cleanup_ctx(ctxs[i]);
			else
				vb2_dma_contig_cleanup_ctx(ctxs[i]);
		}
	}
}

