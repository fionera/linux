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

#ifndef __VDEC_CAP_H__
#define __VDEC_CAP_H__

const vcodec_pixfmt_t* vdec_getCapPixelFormat(u32 pixelformat);
const vcodec_pixfmt_t* vdec_getCapPixelFormatByIndex(int index);

/* for vdec that support m2m operations */
int vdec_cap_setFormat(struct v4l2_pix_format_mplane *mp, u32 pixfmt, int picW, int picH);
int vdec_cap_qbuf(vdec_handle_t *handle, vcodec_buffer_t *buf);
int vdec_cap_dqbuf(vdec_handle_t *handle);

#endif
