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

#ifndef __VCODEC_UTIlS_H__
#define __VCODEC_UTILS_H__

void vcodec_lockDecDevice(vcodec_device_t *device);
void vcodec_unlockDecDevice(vcodec_device_t *device);

int            vcodec_getFreeMsgQ(vcodec_device_t *device);
void           vcodec_putMsgQ(vcodec_device_t *device, int index);
vdec_handle_t* vcodec_getDecByChannel(vcodec_device_t *device, int channel);
vdec_handle_t* vcodec_getDecByDisplay(vcodec_device_t *device, int display);
vdec_handle_t* vcodec_getDecByOutputIdx(vcodec_device_t *device, int outputIdx);

int vcodec_initVB2AllocatorContext(vcodec_device_t *device, struct vb2_alloc_ctx *ctxs[], int cnt);
void vcodec_exitVB2AllocatorContext(vcodec_device_t *device, struct vb2_alloc_ctx *ctxs[], int cnt);

#endif
