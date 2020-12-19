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

#ifndef __VCAP_CORE_H__
#define __VCAP_CORE_H__

#define VCAP_IDX_EOS 1000
#define GPSC_ALONE

const vcodec_pixfmt_t* vcap_getPixelFormat(u32 pixelformat);
const vcodec_pixfmt_t* vcap_getPixelFormatByIndex(int index);

int vcap_initHandle(vcap_handle_t *handle);
int vcap_exitHandle(vcap_handle_t *handle);
int vcap_setFormat(struct v4l2_pix_format_mplane *mp, u32 pixfmt, int picW, int picH);
int vcap_qbuf(vcap_handle_t *handle, vcodec_buffer_t *buf);
int vcap_dqbuf(vcap_handle_t *handle);
int vcap_streamon(vcap_handle_t *handle);
int vcap_streamoff(vcap_handle_t *handle);

int vcap_fifo_qbuf(vfifo_t *q, vcodec_buffer_t *buf);
int vcap_fifo_dqbuf(vfifo_t *q, struct list_head *buf_list);

#endif
