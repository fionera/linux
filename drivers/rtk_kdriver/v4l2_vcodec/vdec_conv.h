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

#ifndef __V4L2_VDEC_CONV_H__
#define __V4L2_VDEC_CONV_H__

#define getDecType(val) \
({ \
	(val == V4L2_PIX_FMT_H264)? VIDEO_STREAM_H264: \
	(val == V4L2_PIX_FMT_H263)? VIDEO_STREAM_H263: \
	(val == V4L2_PIX_FMT_MPEG1)? VIDEO_STREAM_MPEG1: \
	(val == V4L2_PIX_FMT_MPEG2)? VIDEO_STREAM_MPEG2: \
	(val == V4L2_PIX_FMT_MPEG4)? VIDEO_STREAM_MPEG4: \
	(val == V4L2_PIX_FMT_XVID)? VIDEO_STREAM_MPEG4: \
	(val == V4L2_PIX_FMT_VC1_ANNEX_G)? VIDEO_STREAM_VC1: \
	(val == V4L2_PIX_FMT_VC1_ANNEX_L)? VIDEO_STREAM_VC1: \
	(val == V4L2_PIX_FMT_VP8)? VIDEO_STREAM_VP8: \
	(val == V4L2_PIX_FMT_VP9)? VIDEO_STREAM_VP9: \
	(val == V4L2_PIX_FMT_HEVC)? VIDEO_STREAM_H265: \
	(val == V4L2_PIX_FMT_AVS)? VIDEO_STREAM_AVS: \
	(val == V4L2_PIX_FMT_AVS2)? VIDEO_STREAM_AVS2: \
	VIDEO_STREAM_H264; \
})

#define getCodecType(val) \
({ \
	(val == V4L2_PIX_FMT_MPEG2)? 2: \
	(val == V4L2_PIX_FMT_H264)? 4: \
	(val == V4L2_PIX_FMT_AVS)? 5: \
	(val == V4L2_PIX_FMT_HEVC)? 8: \
	0; \
})

#define getDecMode(val) \
({ \
	(val == V4L2_EXT_VDEC_DECODE_I_FRAME)? NORMAL_I_ONLY_DECODE: \
	(val == V4L2_EXT_VDEC_DECODE_IP_FRAME)? FASTFR_DECODE: \
	(val == V4L2_EXT_VDEC_DECODE_ALL_FRAME)? NORMAL_DECODE: \
	NORMAL_DECODE; \
})

#define getPlane(val) \
({ \
	(val < 0)? VO_VIDEO_PLANE_NONE: \
	(val == 0)? VO_VIDEO_PLANE_V1: \
	(val == 1)? VO_VIDEO_PLANE_V2: \
	VO_VIDEO_PLANE_NONE; \
})

#define getSpeed(val) \
({ \
	val * 256 / 1000; \
})

#define getOutFltType(val) \
({ \
	(val == V4L2_EXT_VDEC_USERTYPE_MEDIA_TUNNEL)? VF_TYPE_VIDEO_OUT: \
	(val == V4L2_EXT_VDEC_USERTYPE_MEDIA_CAPTURE)? VF_TYPE_VIDEO_BUFFER_POOL: \
	(val == V4L2_EXT_VDEC_USERTYPE_MEDIA_CAPTURE_M2M)? VF_TYPE_VIDEO_BUFFER_POOL: \
	(val == V4L2_EXT_VDEC_USERTYPE_DTV)? VF_TYPE_VIDEO_OUT: \
	VF_TYPE_VIDEO_OUT; \
})

#define getFramerateBycode(val) \
({ \
	(val == 1)? 23976: \
	(val == 2)? 24000: \
	(val == 3)? 25000: \
	(val == 4)? 29970: \
	(val == 5)? 30000: \
	(val == 6)? 50000: \
	(val == 7)? 59940: \
	(val == 8)? 60000: \
	(val == 9)? 100000: \
	(val == 10)? 120000: \
	24000; \
})
#endif
