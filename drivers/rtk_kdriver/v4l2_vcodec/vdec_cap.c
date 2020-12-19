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

#include <media/v4l2-device.h>
#include <media/v4l2-event.h>

#include "v4l2_vdec_ext.h"

#include "vcodec_conf.h"
#include "vcodec_def.h"
#include "vcodec_struct.h"
#include "vdec_cap.h"
#include "vcap_core.h"

const vcodec_pixfmt_t* vdec_getCapPixelFormat(u32 pixelformat)
{
	return vcap_getPixelFormat(pixelformat);
}

const vcodec_pixfmt_t* vdec_getCapPixelFormatByIndex(int index)
{
	return vcap_getPixelFormatByIndex(index);
}

int vdec_cap_setFormat(struct v4l2_pix_format_mplane *mp, u32 pixfmt, int picW, int picH)
{
	return vcap_setFormat(mp, pixfmt, picW, picH);
}

int vdec_cap_qbuf(vdec_handle_t *handle, vcodec_buffer_t *buf)
{
	if (handle->elem.bufQ_in)
		vcap_fifo_qbuf(&handle->bufQ_in, buf);

	list_add_tail(&buf->list, &handle->buf_list);

	return 0;
}

int vdec_cap_dqbuf(vdec_handle_t *handle)
{
	int idx;

	if (!handle->elem.bufQ_out)
		return -1;

	idx = vcap_fifo_dqbuf(&handle->bufQ_out, &handle->buf_list);

	if (idx == VCAP_IDX_EOS) {
		struct v4l2_event event;

		event.type = V4L2_EVENT_EOS;
		event.id = 0;
		v4l2_event_queue_fh(&handle->fh, &event);

		dev_info(handle->dev, "EOS\n");
	}

	return idx;
}
