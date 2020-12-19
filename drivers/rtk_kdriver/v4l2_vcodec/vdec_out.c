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

#include "v4l2_vdec_ext.h"

#include "vcodec_conf.h"
#include "vcodec_struct.h"

static const vcodec_pixfmt_t pixfmt_out[] = {
	{
		.pixelformat = V4L2_PIX_FMT_H264,
		.num_planes = 1,
	},
	{
		.pixelformat = V4L2_PIX_FMT_H263,
		.num_planes = 1,
	},
	{
		.pixelformat = V4L2_PIX_FMT_MPEG1,
		.num_planes = 1,
	},
	{
		.pixelformat = V4L2_PIX_FMT_MPEG2,
		.num_planes = 1,
	},
	{
		.pixelformat = V4L2_PIX_FMT_MPEG4,
		.num_planes = 1,
	},
	{
		.pixelformat = V4L2_PIX_FMT_XVID,
		.num_planes = 1,
	},
	{
		.pixelformat = V4L2_PIX_FMT_VC1_ANNEX_G,
		.num_planes = 1,
	},
	{
		.pixelformat = V4L2_PIX_FMT_VC1_ANNEX_L,
		.num_planes = 1,
	},
	{
		.pixelformat = V4L2_PIX_FMT_VP8,
		.num_planes = 1,
	},
	{
		.pixelformat = V4L2_PIX_FMT_VP9,
		.num_planes = 1,
	},
	{
		.pixelformat = V4L2_PIX_FMT_HEVC,
		.num_planes = 1,
	},
	{
		.pixelformat = V4L2_PIX_FMT_AVS,
		.num_planes = 1,
	},
	{
		.pixelformat = V4L2_PIX_FMT_AVS2,
		.num_planes = 1,
	},
};

const vcodec_pixfmt_t* vdec_getOutPixelFormat(u32 pixelformat)
{
	int i;

	for (i=0; i<ARRAY_SIZE(pixfmt_out); i++) {
		if (pixfmt_out[i].pixelformat == pixelformat)
			return &pixfmt_out[i];
	}

	return NULL;
}

const vcodec_pixfmt_t* vdec_getOutPixelFormatByIndex(int index)
{
	if (index >=0 && index < ARRAY_SIZE(pixfmt_out))
		return &pixfmt_out[index];

	return NULL;
}

