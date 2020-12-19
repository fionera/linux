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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <pthread.h>
#include <errno.h>
#include <termios.h>

#include <linux/videodev2.h>

#pragma scalar_storage_order big-endian
#include <rpc_common.h>
#include <InbandAPI.h>
#include <VideoRPC_System.h>
#pragma scalar_storage_order default

#include "v4l2_vdec_ext.h"

#include "syswrap.h"
#include "ringbuf.h"
#include "options_vdec.h"

#define IF_ERR_EXIT(errno) \
	if (errno) { \
		printf("%s:%d ret %d\n", __func__, __LINE__, errno); \
		exit(-1); \
	}

typedef struct cap_buf_t {
	struct v4l2_buffer buffer;
	struct v4l2_plane planes[VIDEO_MAX_PLANES];
	int expbuf_fd[VIDEO_MAX_PLANES];
	int impbuf_fd[VIDEO_MAX_PLANES];
} cap_buf_t;

typedef struct priv_t {
	int fd_dec;
	unsigned int bExit:1;
	unsigned int bEOS:1;
	unsigned int nResolutionChange;
	unsigned int nFrmInfo;
	unsigned int nPicInfo;
	unsigned int nUserData;
	struct v4l2_ext_src_ringbufs src;
	cap_buf_t cap_bufs[64];
	pthread_mutex_t lock;
	int (*sink)(struct priv_t *priv, const char *file_path);
} priv_t;

typedef struct lookup_entry_t {
	char name[32];
	int value;
} lookup_entry_t;

typedef struct test_entry_t {
	int id;
	int (*func)(priv_t*);
	char desc[128];
} test_entry_t;

static int initTerm(void)
{
	struct termios tp;

	/* return error if not in the foreground */
	if (tcgetpgrp(STDIN_FILENO) != getpgrp())
		return -1;

	tcgetattr(STDIN_FILENO, &tp);
	tp.c_lflag &= ~(ICANON|ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &tp);

	return 0;
}

static inline uint64_t getTimeStamp64(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * (uint64_t)1000000 + tv.tv_usec;
}

static lookup_entry_t lookups[] = {
	{"mpeg2", V4L2_PIX_FMT_MPEG2},
	{"avc", V4L2_PIX_FMT_H264},
	{"h264", V4L2_PIX_FMT_H264},
	{"hevc", V4L2_PIX_FMT_HEVC},
	{"h265", V4L2_PIX_FMT_HEVC},
	{"avs",	V4L2_PIX_FMT_AVS},
	{"avs2", V4L2_PIX_FMT_AVS2},
};

static int lookup(const char *name)
{
	int i, cnt;

	cnt = sizeof(lookups)/sizeof(lookup_entry_t);

	for (i=0; i<cnt; i++)
		if (!strcasecmp(lookups[i].name, name))
			return lookups[i].value;

	printf("Error! lookup %s fails\n", name);
	exit(1);
	return -1;
}

static int s_ctrl(int fd, unsigned int id, int val)
{
	struct v4l2_control control_arg;

	control_arg.id = id;
	control_arg.value = val;

	return ioctl(fd, VIDIOC_S_CTRL, &control_arg);
}

static int g_ctrl(int fd, unsigned int id, int *val)
{
	int ret;
	struct v4l2_control control_arg;

	control_arg.id = id;
	control_arg.value = 0;

	ret = ioctl(fd, VIDIOC_G_CTRL, &control_arg);
	*val = control_arg.value;

	return ret;
}

static int s_ext_ctrls(int fd, struct v4l2_ext_control *ext_ctrl, int cnt)
{
	int ret;
	struct v4l2_ext_controls ext_ctrls = {0};

	ext_ctrls.controls = ext_ctrl;
	ext_ctrls.count = cnt;

	ret = ioctl(fd, VIDIOC_S_EXT_CTRLS, &ext_ctrls);
	if (ret) {
		if (errno != EAGAIN)
			printf("%s: errno %d %s\n",  __func__, errno, strerror(errno));
		return ret;
	}

	return 0;
}

#define s_ext_ctrls_va(__fd, ...) \
({ \
	struct v4l2_ext_control __payload = {__VA_ARGS__}; \
	s_ext_ctrls(__fd, &__payload, 1); \
})

#define s_ext_ctrls_val(__fd, __id, __value) \
	s_ext_ctrls_va(__fd, .id = __id, .value = __value)
#define s_ext_ctrls_ptr(__fd, __id, __ptr) \
	s_ext_ctrls_va(__fd, .id = __id, .ptr = __ptr)
#define s_ext_ctrls_ptr_size(__fd, __id, __ptr, __size) \
	s_ext_ctrls_va(__fd, .id = __id, .ptr = __ptr, .size = __size)


static int g_ext_ctrls(int fd, struct v4l2_ext_control *ext_ctrl, int cnt)
{
	int ret;
	struct v4l2_ext_controls ext_ctrls = {0};

	ext_ctrls.controls = ext_ctrl;
	ext_ctrls.count = cnt;

	ret = ioctl(fd, VIDIOC_G_EXT_CTRLS, &ext_ctrls);
	if (ret) {
		printf("%s: errno %d %s\n",  __func__, errno, strerror(errno));
		return ret;
	}

	return 0;
}

#define g_ext_ctrls_va(__fd, ...) \
({ \
	struct v4l2_ext_control __payload = {__VA_ARGS__}; \
	g_ext_ctrls(__fd, &__payload, 1); \
})

#define g_ext_ctrls_ptr(__fd, __id, __ptr) \
	g_ext_ctrls_va(__fd, .id = __id, .ptr = __ptr)
#define g_ext_ctrls_ptr_size(__fd, __id, __ptr, __size) \
	g_ext_ctrls_va(__fd, .id = __id, .ptr = __ptr, .size = __size)

static int s_input(int fd, int index)
{
	return ioctl(fd, VIDIOC_S_INPUT, &index);
}

static int s_output(int fd, int index)
{
	return ioctl(fd, VIDIOC_S_OUTPUT, &index);
}

static int streamon(int fd, int type)
{
	return ioctl(fd, VIDIOC_STREAMON, &type);
}

static int subscribe(int fd, unsigned int type, unsigned int id)
{
	struct v4l2_event_subscription subscription_arg;

	subscription_arg.type = type;
	subscription_arg.id = id;

	return ioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &subscription_arg);
}

static int decoder_cmd(int fd, __u32 cmd)
{
	int ret;

	struct v4l2_decoder_cmd decoder_cmd = {
		.cmd = cmd
	};

	ret = ioctl(fd, VIDIOC_DECODER_CMD, &decoder_cmd);
	IF_ERR_EXIT(ret);

	printf("%s: cmd %d\n", __func__, cmd);

	return 0;
}

static int recvFrmInfo(__u8 *data)
{
	int i;
	__u8 type;
	long long pts = 0;

	//In the u.data[0], picture type data should be stored.(SeqHeader : 0, I Frame : 1, P Frame : 2, B Frame : 3).
	//In the u.data[1] ~ u.data[5], PTS data should be stored.

	type = data[0];

	for (i=1; i<6; i++)
		pts = pts << 8 | data[i];

	if (opts.verbose)
		printf("%s: type %d pts %lld\n", __func__, type, pts);

	return 0;
}

static int recvPicInfo(int fd)
{
	int ret;
	struct v4l2_ext_picinfo_msg data;

	ret = g_ext_ctrls_ptr_size(fd, V4L2_CID_EXT_VDEC_PICINFO_EVENT_DATA, &data, sizeof(data));
	IF_ERR_EXIT(ret);

	if (opts.verbose)
		printf("%s: channel %d frame_rate %d h_size %d v_size %d\n", __func__,
			data.channel, data.frame_rate,
			data.h_size, data.v_size);

	return 0;
}

static int recvUserData(int fd, int size)
{
	int ret;
	static unsigned char userdata_buf[4096];

	if (size > sizeof(userdata_buf)) {
		printf("%s: size %d is too big\n", __func__, size);
		return -1;
	}

	ret = g_ext_ctrls_ptr_size(fd, V4L2_CID_EXT_VDEC_USER_EVENT_DATA, userdata_buf, size);
	IF_ERR_EXIT(ret)

	if (opts.verbose)
		printf("%s: size %d\n", __func__, size);

	return 0;
}

static int recvMediaInfo(struct v4l2_ext_vdec_media_info *info)
{
	printf("media-info: %dx%d\n", info->width, info->height);
	return 0;
}

static int getVideoInfo(int fd)
{
	int i, ret;
	struct v4l2_control ctrl[5] = {0};
	struct v4l2_ext_control ext_ctrl[5] = {0};

	for (i=0; i<5; i++) {
		ext_ctrl[i].id = V4L2_CID_EXT_VDEC_VIDEO_INFO;
		ext_ctrl[i].ptr = &ctrl[i];
		ext_ctrl[i].size = sizeof(sizeof(struct v4l2_control));
	}

	ctrl[0].id = VDEC_V_SIZE;
	ctrl[1].id = VDEC_H_SIZE;
	ctrl[2].id = VDEC_FRAME_RATE;
	ctrl[3].id = VDEC_ASPECT_RATIO;
	ctrl[4].id = VDEC_PROGRESSIVE_SEQUENCE;

	ret = g_ext_ctrls(fd, ext_ctrl, 5);
	IF_ERR_EXIT(ret);

	printf("VDEC_V_SIZE value = %d\n", ctrl[0].value);
	printf("VDEC_H_SIZE value = %d\n", ctrl[1].value);
	printf("VDEC_FRAME_RATE value = %d\n", ctrl[2].value);
	printf("VDEC_ASPECT_RATIO value = %d\n", ctrl[3].value);
	printf("VDEC_PROGRESSIVE_SEQUENCE value = %d\n", ctrl[4].value);

	return 0;
}

static int getStreamInfo(int fd)
{
	int ret;
	struct v4l2_ext_stream_info data;

	ret = g_ext_ctrls_ptr_size(fd, V4L2_CID_EXT_VDEC_STREAM_INFO, &data, sizeof(data));
	IF_ERR_EXIT(ret);

	printf("%s: v_size %d h_size %d video_format %d frame_rate %d aspect_ratio %d\n", __func__,
		data.v_size, data.h_size,
		data.video_format, data.frame_rate,
		data.aspect_ratio);

	return 0;
}

static int getDecoderStatus(int fd)
{
	int ret;
	struct v4l2_ext_decoder_status data;

	ret = g_ext_ctrls_ptr_size(fd, V4L2_CID_EXT_VDEC_DECODER_STATUS, &data, sizeof(data));
	IF_ERR_EXIT(ret);

	printf("%s: vdec_state %d codec_type %d\n", __func__,
		data.vdec_state, data.codec_type);

	return 0;
}

static int procExtEvent(priv_t *priv, int fd, struct v4l2_event *event)
{
	switch (event->id) {

		case V4L2_SUB_EXT_VDEC_FRAME:
			recvFrmInfo(event->u.data);
			priv->nFrmInfo++;
			break;

		case V4L2_SUB_EXT_VDEC_PICINFO:
			recvPicInfo(fd);
			priv->nPicInfo++;
			if (priv->nPicInfo == 1) {
				getStreamInfo(fd);
				getDecoderStatus(fd);
				getVideoInfo(fd);
			}
			break;

		case V4L2_SUB_EXT_VDEC_USERDATA:
			recvUserData(fd, *(int*)event->u.data);
			priv->nUserData++;
			break;

		case V4L2_SUB_EXT_VDEC_MEDIAINFO:
			recvMediaInfo((struct v4l2_ext_vdec_media_info*)event->u.data);
			break;

		default:
			printf("%s: unknown event %d\n", __func__, event->id);
			break;
	}

	return 0;
}

static int dqevent(priv_t *priv, int fd)
{
	int ret;
	struct pollfd poll_fd;
	struct v4l2_event event_arg = {0};

	poll_fd.fd = fd;
	poll_fd.events = POLLPRI;

	printf("%s: start\n", __func__);

	while (!priv->bEOS && !priv->bExit) {

		ret = poll(&poll_fd, 1, 50);
		if (ret < 0) return errno;
		if (ret == 0) continue;

		do {

			ret = ioctl(fd, VIDIOC_DQEVENT, &event_arg);
			if (ret) {
				printf("%s: VIDIOC_DQEVENT ret %d\n", __func__, ret);
				break;
			}

			switch (event_arg.type) {
				case V4L2_EVENT_EOS:
					priv->bEOS = 1;
					printf("V4L2_EVENT_EOS\n");
					break;
				case V4L2_EVENT_SOURCE_CHANGE:
					priv->nResolutionChange++;
					printf("V4L2_EVENT_SOURCE_CHANGE\n");
					break;
				case V4L2_EVENT_PRIVATE_EXT_VDEC_EVENT:
					procExtEvent(priv, fd, &event_arg);
					break;
				default:
					printf("%s: unknown event %d\n", __func__, event_arg.type);
					break;
			}

		} while (event_arg.pending);

	}

	printf("%s: end\n", __func__);

	return 0;
}

static int dqbuf(int fd)
{
	int ret, data_offset_Y, data_offset_C, cnt, picW, picH, bEOS;
	uint64_t ts, ts_start;
	double fps;
	struct v4l2_buffer buf = {0};
	struct v4l2_plane planes[VIDEO_MAX_PLANES] = {0};
	struct v4l2_ext_vcap_picobj *picobj;

	picobj = malloc(sizeof(*picobj));
	IF_ERR_EXIT(!picobj);

	ts_start = getTimeStamp64();
	cnt = picW = picH = bEOS = 0;

	while (!bEOS) {

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		buf.memory = opts.import_dmabuf? V4L2_MEMORY_DMABUF : V4L2_MEMORY_MMAP;
		buf.length = VIDEO_MAX_PLANES;
		buf.m.planes = planes;

		ioctl(fd, VIDIOC_DQBUF, &buf);
		IF_ERR_EXIT(errno);

		if (buf.flags & V4L2_BUF_FLAG_LAST) {
			printf("ooo: V4L2_BUF_FLAG_LAST\n");
			bEOS = 1;
		}

		if (!buf.m.planes[0].bytesused) {
			printf("ooo: empty buf\n");
			ioctl(fd, VIDIOC_QBUF, &buf);
			IF_ERR_EXIT(errno);
			continue;
		}

		ret = g_ext_ctrls_ptr(fd, V4L2_CID_EXT_VCAP_PICOBJ_IDX0 + buf.index, picobj);
		IF_ERR_EXIT(ret);

		data_offset_Y = buf.m.planes[0].data_offset;
		data_offset_C = buf.m.planes[0].data_offset + picobj->addr[1] -  picobj->addr[0];
		ts = getTimeStamp64();
		cnt++;

		fps = (double)(cnt) * 1000000 / (ts - ts_start);

		if (picW != picobj->picW || picH != picobj->picH) {
			printf("media-info: pos %d,%d disp %dx%d sample %dx%d pitch %d,%d\n",
				picobj->picX, picobj->picY,
				picobj->picW, picobj->picH,
				picobj->sampleW, picobj->sampleH,
				picobj->pitchY, picobj->pitchC);
			picW = picobj->picW;
			picH = picobj->picH;
		}

		if (opts.verbose) {
			printf("ooo: index %d data_offset %d,%d pts %ld.%06ld fps %lf\n",
				buf.index, data_offset_Y, data_offset_C,
				buf.timestamp.tv_sec, buf.timestamp.tv_usec, fps);
		}

		/* re-enqueue */
		ioctl(fd, VIDIOC_QBUF, &buf);
		IF_ERR_EXIT(errno);
	}

	free(picobj);

	return 0;
}

static unsigned int getCurWr(unsigned int rbh_addr)
{
	int ret;
	unsigned int wr;
	void *handle;

	ret = ringbuf_open(rbh_addr, &handle);
	IF_ERR_EXIT(ret);

	ret = ringbuf_getWr(handle, &wr);
	IF_ERR_EXIT(ret);

	ringbuf_close(handle);

	return wr;
}

static int sendEOS(struct v4l2_ext_src_ringbufs *src)
{
	int ret;
	EOS eos;
	void *handle;

	ret = ringbuf_open(src->ib_hdr_addr, &handle);
	IF_ERR_EXIT(ret);

	eos.header.type = INBAND_CMD_TYPE_EOS;
	eos.header.size = sizeof(eos);
	eos.eventID = 0;
	eos.wPtr = getCurWr(src->bs_hdr_addr);

	ret = ringbuf_write(handle, &eos, sizeof(eos));
	IF_ERR_EXIT(ret);

	ringbuf_close(handle);

	printf("%s: wptr 0x%08x\n", __func__, eos.wPtr);

	return 0;
}

static int sendNewSeg(struct v4l2_ext_src_ringbufs *src, __u32 wptr)
{
	int ret;
	void *handle;
	NEW_SEG new_seg;

	ret = ringbuf_open(src->ib_hdr_addr, &handle);
	IF_ERR_EXIT(ret);

	new_seg.header.type = INBAND_CMD_TYPE_NEW_SEG;
	new_seg.header.size = sizeof(new_seg);
	new_seg.wPtr = wptr;

	ret = ringbuf_write(handle, &new_seg, sizeof(NEW_SEG));
	IF_ERR_EXIT(ret);

	ringbuf_close(handle);

	printf("%s: wptr 0x%08x\n", __func__, wptr);

	return 0;
}

static int sendDecode(struct v4l2_ext_src_ringbufs *src, int mode)
{
	int ret;
	void *handle;
	DECODE decode;

	ret = ringbuf_open(src->ib_hdr_addr, &handle);
	IF_ERR_EXIT(ret);

	decode.header.type = INBAND_CMD_TYPE_DECODE;
	decode.header.size = sizeof(decode);
	decode.RelativePTSH = 0;
	decode.RelativePTSL = 0;
	decode.PTSDurationH = (-1LL) >> 32;
	decode.PTSDurationL = (-1LL);
	decode.skip_GOP = 0;
	decode.mode = mode;
	decode.isHM91 = 0;

	ret = ringbuf_write(handle, &decode, sizeof(DECODE));
	IF_ERR_EXIT(ret);

	ringbuf_close(handle);

	printf("%s: mode %d\n", __func__, mode);

	return 0;
}

static int sinkFile(priv_t *priv, const char *file_path)
{
	int ret, fd = -1;
	void *handle = NULL, *buf = NULL;
	int bufsize, size;

	if (!file_path)
		return -1;

	fd = open(file_path, O_RDONLY);
	if (fd < 0) return -ENOENT;

	bufsize = 0x40000;
	buf = malloc(bufsize);;
	if (!buf) return -ENOMEM;

	ringbuf_open(priv->src.bs_hdr_addr, &handle);

	printf("%s: start\n", __func__);

	if (handle) {
		while (!priv->bExit) {
			size = read(fd, buf, bufsize);
			if (size <= 0) break;
			do {
				pthread_mutex_lock(&priv->lock);
				ret = ringbuf_write(handle, buf, size);
				pthread_mutex_unlock(&priv->lock);
				usleep(1000);
			} while (ret < 0 && !priv->bExit);
		}
	}

	free(buf);
	close(fd);

	printf("%s: end\n", __func__);

	return 0;
}

static int sinkFile_DirectMode(priv_t *priv, const char *file_path)
{
	int ret, fd = -1;
	void *buf = NULL;
	int bufsize, size;

	if (!file_path)
		return -1;

	fd = open(file_path, O_RDONLY);
	if (fd < 0) return -ENOENT;

	bufsize = 0x40000;
	buf = malloc(bufsize);;
	if (!buf) return -ENOMEM;

	printf("%s: start\n", __func__);

	while (!priv->bExit) {
		size = read(fd, buf, bufsize);
		if (size <= 0) break;
		do {
			pthread_mutex_lock(&priv->lock);
			ret = s_ext_ctrls_ptr_size(priv->fd_dec, V4L2_CID_EXT_VDEC_DIRECT_ESDATA, buf, size);
			pthread_mutex_unlock(&priv->lock);
			usleep(1000);
		} while (ret < 0 && !priv->bExit);
	}

	free(buf);
	close(fd);

	printf("%s: end\n", __func__);

	return 0;
}

static int sinkFile_DripMode(priv_t *priv, const char *file_path)
{
	int ret, fd = -1;
	void *buf = NULL;
	int bufsize, size;

	if (!file_path)
		return -1;

	fd = open(file_path, O_RDONLY);
	if (fd < 0) return -ENOENT;

	bufsize = 0x40000;
	buf = malloc(bufsize);;
	if (!buf) return -ENOMEM;

	printf("%s: start\n", __func__);

	size = read(fd, buf, bufsize);
	pthread_mutex_lock(&priv->lock);
	ret = s_ext_ctrls_ptr_size(priv->fd_dec, V4L2_CID_EXT_VDEC_DRIPDEC_PICTURE, buf, size);
	IF_ERR_EXIT(ret);
	pthread_mutex_unlock(&priv->lock);

	free(buf);
	close(fd);

	printf("%s: end\n", __func__);

	return 0;
}

static void* thread_sinkFile(void *arg)
{
	int ret;
	priv_t *priv = arg;

	if (priv->sink)
		ret = priv->sink(priv, opts.input);
	else
		ret = sinkFile(priv, opts.input);

	IF_ERR_EXIT(ret);

	if (!priv->bExit && priv->src.ib_hdr_addr)
		sendEOS(&priv->src);

	return NULL;
}

static void* thread_readKey(void *arg)
{
	int ret;
	char c;
	priv_t *priv = arg;

	if (initTerm())
		return NULL;

	while ((c = getchar()) != 'q') {
		if (c == 'f') {
			pthread_mutex_lock(&priv->lock);
			ret = decoder_cmd(priv->fd_dec, V4L2_DEC_CMD_FLUSH);
			IF_ERR_EXIT(ret);
			ret = sendNewSeg(&priv->src, getCurWr(priv->src.bs_hdr_addr));
			IF_ERR_EXIT(ret);
			ret = sendDecode(&priv->src, NORMAL_DECODE);
			IF_ERR_EXIT(ret);
			pthread_mutex_unlock(&priv->lock);
		}
	}

	priv->bExit = 1;

	return NULL;
}

static int initRefClk(struct v4l2_ext_src_ringbufs *src)
{
	REFCLOCK *pRefClk;

	pRefClk = (REFCLOCK*)syswrap_mmap(src->refclk_addr, sizeof(REFCLOCK));
	if (!pRefClk) return -ENOMEM;

	memset(pRefClk, 0, sizeof(REFCLOCK));
	pRefClk->mastership.videoMode = AVSYNC_FORCED_MASTER;
	pRefClk->videoFreeRunThreshold = 2700000;

	syswrap_munmap(pRefClk, sizeof(REFCLOCK));

	return 0;
}

static int initRingBufHdr(struct v4l2_ext_src_ringbufs *src)
{
	int ret;

	ret = ringbuf_initHdr(src->bs_hdr_addr, RINGBUFFER_STREAM, src->bs_buf_addr, src->bs_buf_size);
	IF_ERR_EXIT(ret);

	ret = ringbuf_initHdr(src->ib_hdr_addr, RINGBUFFER_COMMAND, src->ib_buf_addr, src->ib_buf_size);
	IF_ERR_EXIT(ret);

	ret = sendNewSeg(src, getCurWr(src->bs_hdr_addr));
	IF_ERR_EXIT(ret);

	ret = sendDecode(src, NORMAL_DECODE);
	IF_ERR_EXIT(ret);

	return 0;
}

static int init_cap_bufs(priv_t *priv, int fd, int picW, int picH, int num_planes, int bufcnt)
{
	int i, j, ret;
	struct v4l2_requestbuffers reqbuf = {0};
	struct v4l2_exportbuffer expbuf = {0};

	reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	reqbuf.memory = V4L2_MEMORY_MMAP;
	reqbuf.count = bufcnt;
	ret = ioctl(fd, VIDIOC_REQBUFS, &reqbuf);
	IF_ERR_EXIT(errno);
	IF_ERR_EXIT(reqbuf.count != bufcnt);

	for (i=0; i<bufcnt; i++) {
		for (j=0; j<num_planes; j++) {
			expbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
			expbuf.index = i;
			expbuf.plane = j;
			ret = ioctl(fd, VIDIOC_EXPBUF, &expbuf);
			IF_ERR_EXIT(errno);
			priv->cap_bufs[i].expbuf_fd[j] = expbuf.fd;
			printf("expbuf: index %d plane %d fd %d\n", expbuf.index, expbuf.plane, expbuf.fd);
		}
	}

	for (i=0; i<bufcnt; i++) {
		priv->cap_bufs[i].buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		priv->cap_bufs[i].buffer.memory = V4L2_MEMORY_MMAP;
		priv->cap_bufs[i].buffer.index = i;
		priv->cap_bufs[i].buffer.length = VIDEO_MAX_PLANES;
		priv->cap_bufs[i].buffer.m.planes = priv->cap_bufs[i].planes;
		ret = ioctl(fd, VIDIOC_QUERYBUF, &priv->cap_bufs[i].buffer);
		IF_ERR_EXIT(errno);
		for (j=0; j<num_planes; j++)
			printf("querybuf: index %d plane %d\n", i, j);
	}

	for (i=0; i<bufcnt; i++) {
		ret = ioctl(fd, VIDIOC_QBUF, &priv->cap_bufs[i].buffer);
		IF_ERR_EXIT(errno);
	}

	return ret;
}

static int import_dmabuf(priv_t *priv, int fd, int num_planes, int bufcnt)
{
	int i, j, ret;
	struct v4l2_requestbuffers reqbuf = {0};
	struct v4l2_buffer *buf;

#if 1
	IF_ERR_EXIT(ENOTSUP);
#else
	struct v4l2_format fmt;
	struct v4l2_exportbuffer expbuf = {0};
	int fd_exp = open(V4L2_EXT_DEV_PATH_GPSCALER, O_RDWR);
	IF_ERR_EXIT(errno);

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	ret = ioctl(fd_exp, VIDIOC_G_FMT, &fmt);
	IF_ERR_EXIT(ret);

	fmt.fmt.pix_mp.width = opts.width;
	fmt.fmt.pix_mp.height = opts.height;
	fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12;
	ret = ioctl(fd_exp, VIDIOC_S_FMT, &fmt);
	IF_ERR_EXIT(ret);

	reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	reqbuf.memory = V4L2_MEMORY_MMAP;
	reqbuf.count = bufcnt;
	ret = ioctl(fd_exp, VIDIOC_REQBUFS, &reqbuf);
	IF_ERR_EXIT(errno);
	IF_ERR_EXIT(reqbuf.count != bufcnt);

	for (i=0; i<bufcnt; i++) {
		for (j=0; j<num_planes; j++) {
			expbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
			expbuf.index = i;
			expbuf.plane = j;
			ret = ioctl(fd_exp, VIDIOC_EXPBUF, &expbuf);
			IF_ERR_EXIT(errno);
			priv->cap_bufs[i].impbuf_fd[j] = expbuf.fd;
		}
	}
#endif

	reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	reqbuf.memory = V4L2_MEMORY_DMABUF;
	reqbuf.count = bufcnt;
	ret = ioctl(fd, VIDIOC_REQBUFS, &reqbuf);
	IF_ERR_EXIT(errno);
	IF_ERR_EXIT(reqbuf.count != bufcnt);

	for (i=0; i<bufcnt; i++) {
		buf = &priv->cap_bufs[i].buffer;
		buf->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		buf->memory = V4L2_MEMORY_DMABUF;
		buf->index = i;
		buf->length = num_planes;
		buf->m.planes = priv->cap_bufs[i].planes;
		for (j=0; j<num_planes; j++) {
			buf->m.planes[j].m.fd = priv->cap_bufs[i].impbuf_fd[j];
			IF_ERR_EXIT(errno);
			printf("import_dmabuf: index %d fd %d\n", i, buf->m.planes[j].m.fd);
		}
		ret = ioctl(fd, VIDIOC_QBUF, buf);
		IF_ERR_EXIT(errno);
	}

	return ret;
}

static int allocMemory(int fd, int type, int size, int flags, unsigned int *pAddr)
{
	int ret;
	struct v4l2_ext_vdec_mem_alloc args = {0};

	args.type = type;
	args.size = size;
	args.flags = flags;

	ret = s_ext_ctrls_ptr(fd, V4L2_CID_EXT_VDEC_MEM_ALLOC, &args);
	IF_ERR_EXIT(ret);

	*pAddr = args.phyaddr;

	printf("%s: type %d size %d flags %d addr 0x%08x\n", __func__,
		type, size, flags, args.phyaddr);

	return 0;
}

static int init_src_ringbufs(int fd, struct v4l2_ext_src_ringbufs *src, char bRefclk)
{
	int ret;

	src->bs_buf_addr = opts.bs_buf_addr;
	src->bs_buf_size = opts.bs_buf_size;
	src->ib_buf_addr = opts.ib_buf_addr;
	src->ib_buf_size = opts.ib_buf_size;

	ret = allocMemory(fd, V4L2_EXT_VDEC_MEMTYPE_GENERIC, sizeof(RINGBUFFER_HEADER), 0, &src->bs_hdr_addr);
	IF_ERR_EXIT(ret);

	ret = allocMemory(fd, V4L2_EXT_VDEC_MEMTYPE_GENERIC, sizeof(RINGBUFFER_HEADER), 0, &src->ib_hdr_addr);
	IF_ERR_EXIT(ret);

	if (!src->bs_buf_addr) {
		ret = allocMemory(fd, V4L2_EXT_VDEC_MEMTYPE_GENERIC, src->bs_buf_size, 0, &src->bs_buf_addr);
		IF_ERR_EXIT(ret);
	}

	if (!src->ib_buf_addr) {
		ret = allocMemory(fd, V4L2_EXT_VDEC_MEMTYPE_GENERIC, src->ib_buf_size, 0, &src->ib_buf_addr);
		IF_ERR_EXIT(ret);
	}

	ret = initRingBufHdr(src);
	IF_ERR_EXIT(ret);

	if (bRefclk) {
		ret = allocMemory(fd, V4L2_EXT_VDEC_MEMTYPE_GENERIC, sizeof(REFCLOCK), 0, &src->refclk_addr);
		IF_ERR_EXIT(ret);
		ret = initRefClk(src);
		IF_ERR_EXIT(ret);
	}

	return 0;
}

static int test_emulate_dtv(priv_t *priv)
{
	int ret, fd_dec, fd_vdo, val;
	struct v4l2_format fmt;
	pthread_t th_sinkFile, th_readKey;

	if (!opts.input) {
		printf("%s: no input\n", __func__);
		return -1;
	}

	fd_dec = open(V4L2_EXT_DEV_PATH_VDEC, O_RDWR);
	IF_ERR_EXIT(errno);

	ret = init_src_ringbufs(fd_dec, &priv->src, 1);
	IF_ERR_EXIT(ret);

	fd_vdo = open(V4L2_EXT_DEV_PATH_VDO0, O_RDWR);
	IF_ERR_EXIT(errno);

	ret = g_ctrl(fd_dec, V4L2_CID_EXT_VDEC_DECODER_VERSION, &val);
	IF_ERR_EXIT(ret);

	printf("decoder version: %d\n", val);

	ret = s_ctrl(fd_dec, V4L2_CID_EXT_VDEC_CHANNEL, 0);
	IF_ERR_EXIT(ret);

	/* connect vdo0 to vdec channel 0 */
	ret = s_input(fd_vdo, 0);
	IF_ERR_EXIT(ret);

	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	ret = ioctl(fd_dec, VIDIOC_G_FMT, &fmt);
	IF_ERR_EXIT(ret);

	fmt.fmt.pix_mp.width = opts.width;
	fmt.fmt.pix_mp.height = opts.height;
	fmt.fmt.pix_mp.pixelformat = lookup(opts.source);
	ret = ioctl(fd_dec, VIDIOC_S_FMT, &fmt);
	IF_ERR_EXIT(ret);

	s_ext_ctrls_val(fd_dec, V4L2_CID_EXT_VDEC_USERTYPE, V4L2_EXT_VDEC_USERTYPE_DTV);
	s_ext_ctrls_ptr(fd_dec, V4L2_CID_EXT_VDEC_SRC_RINGBUFS, &priv->src);

	ret = subscribe(fd_dec, V4L2_EVENT_PRIVATE_EXT_VDEC_EVENT, V4L2_SUB_EXT_VDEC_FRAME);
	IF_ERR_EXIT(ret);

	ret = subscribe(fd_dec, V4L2_EVENT_PRIVATE_EXT_VDEC_EVENT, V4L2_SUB_EXT_VDEC_PICINFO);
	IF_ERR_EXIT(ret);

	ret = subscribe(fd_dec, V4L2_EVENT_PRIVATE_EXT_VDEC_EVENT, V4L2_SUB_EXT_VDEC_USERDATA);
	IF_ERR_EXIT(ret);

	priv->fd_dec = fd_dec;
	ret = pthread_create(&th_readKey, NULL, thread_readKey, priv);
	IF_ERR_EXIT(ret);

	ret = pthread_create(&th_sinkFile, NULL, thread_sinkFile, priv);
	IF_ERR_EXIT(ret);

	ret = streamon(fd_dec, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	IF_ERR_EXIT(ret);

	ret = dqevent(priv, fd_dec);
	IF_ERR_EXIT(ret);

	pthread_join(th_sinkFile, NULL);
	close(fd_dec);
	close(fd_vdo);

	return 0;
}

static int test_emulate_media_tunnel(priv_t *priv)
{
	int ret, fd_dec, fd_vdo;
	struct v4l2_format fmt;
	pthread_t th_sinkFile, th_readKey;

	if (!opts.input) {
		printf("%s: no input\n", __func__);
		return -1;
	}

	fd_dec = open(V4L2_EXT_DEV_PATH_VDEC, O_RDWR);
	IF_ERR_EXIT(errno);

	ret = init_src_ringbufs(fd_dec, &priv->src, 1);
	IF_ERR_EXIT(ret);

	fd_vdo = open(V4L2_EXT_DEV_PATH_VDO0, O_RDWR);
	IF_ERR_EXIT(errno);

	ret = s_ctrl(fd_dec, V4L2_CID_EXT_VDEC_CHANNEL, 0);
	IF_ERR_EXIT(ret);

	/* connect vdo0 to vdec channel 0 */
	ret = s_input(fd_vdo, 0);
	IF_ERR_EXIT(ret);

	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	ret = ioctl(fd_dec, VIDIOC_G_FMT, &fmt);
	IF_ERR_EXIT(ret);

	fmt.fmt.pix_mp.width = opts.width;
	fmt.fmt.pix_mp.height = opts.height;
	fmt.fmt.pix_mp.pixelformat = lookup(opts.source);
	ret = ioctl(fd_dec, VIDIOC_S_FMT, &fmt);
	IF_ERR_EXIT(ret);

	s_ext_ctrls_val(fd_dec, V4L2_CID_EXT_VDEC_USERTYPE, V4L2_EXT_VDEC_USERTYPE_MEDIA_TUNNEL);
	s_ext_ctrls_ptr(fd_dec, V4L2_CID_EXT_VDEC_SRC_RINGBUFS, &priv->src);

	priv->fd_dec = fd_dec;
	ret = pthread_create(&th_readKey, NULL, thread_readKey, priv);
	IF_ERR_EXIT(ret);

	ret = pthread_create(&th_sinkFile, NULL, thread_sinkFile, priv);
	IF_ERR_EXIT(ret);

	ret = subscribe(fd_dec, V4L2_EVENT_EOS, 0);
	IF_ERR_EXIT(ret);

	ret = subscribe(fd_dec, V4L2_EVENT_SOURCE_CHANGE, 0);
	IF_ERR_EXIT(ret);

	ret = subscribe(fd_dec, V4L2_EVENT_PRIVATE_EXT_VDEC_EVENT, V4L2_SUB_EXT_VDEC_MEDIAINFO);
	IF_ERR_EXIT(ret);

	ret = streamon(fd_dec, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	IF_ERR_EXIT(ret);

	ret = dqevent(priv, fd_dec);
	IF_ERR_EXIT(ret);

	pthread_join(th_sinkFile, NULL);
	close(fd_dec);
	close(fd_vdo);

	return 0;
}

static int test_emulate_media_capture(priv_t *priv)
{
	int ret, fd_dec, fd_cap, picW, picH, bufcnt;
	struct v4l2_format fmt;
	pthread_t th_sinkFile;

	if (!opts.input) {
		printf("%s: no input\n", __func__);
		return -1;
	}

	picW = opts.width;
	picH = opts.height;
	bufcnt = opts.bufcnt;

	fd_dec = open(V4L2_EXT_DEV_PATH_VDEC, O_RDWR);
	IF_ERR_EXIT(errno);

	ret = init_src_ringbufs(fd_dec, &priv->src, 0);
	IF_ERR_EXIT(ret);

	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	ret = ioctl(fd_dec, VIDIOC_G_FMT, &fmt);
	IF_ERR_EXIT(ret);

	fmt.fmt.pix_mp.width = picW;
	fmt.fmt.pix_mp.height = picH;
	fmt.fmt.pix_mp.pixelformat = lookup(opts.source);
	ret = ioctl(fd_dec, VIDIOC_S_FMT, &fmt);
	IF_ERR_EXIT(ret);

	s_ext_ctrls_val(fd_dec, V4L2_CID_EXT_VDEC_USERTYPE, V4L2_EXT_VDEC_USERTYPE_MEDIA_CAPTURE);
	s_ext_ctrls_ptr(fd_dec, V4L2_CID_EXT_VDEC_SRC_RINGBUFS, &priv->src);

	ret = pthread_create(&th_sinkFile, NULL, thread_sinkFile, priv);
	IF_ERR_EXIT(ret);

	ret = s_output(fd_dec, opts.vcap_port);
	IF_ERR_EXIT(ret);

	ret = streamon(fd_dec, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	IF_ERR_EXIT(ret);

	fd_cap = open(V4L2_EXT_DEV_PATH_GPSCALER, O_RDWR);
	IF_ERR_EXIT(errno);

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	ret = ioctl(fd_cap, VIDIOC_G_FMT, &fmt);
	IF_ERR_EXIT(ret);

	fmt.fmt.pix_mp.width = picW;
	fmt.fmt.pix_mp.height = picH;
	fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12;
	ret = ioctl(fd_cap, VIDIOC_S_FMT, &fmt);
	IF_ERR_EXIT(ret);

	if (opts.import_dmabuf) {
		ret = import_dmabuf(priv, fd_cap, fmt.fmt.pix_mp.num_planes, bufcnt);
		IF_ERR_EXIT(ret);
	}

	else {
		ret = init_cap_bufs(priv, fd_cap, picW, picH, fmt.fmt.pix_mp.num_planes, bufcnt);
		IF_ERR_EXIT(ret);
	}

	ret = s_input(fd_cap, opts.vcap_port);
	IF_ERR_EXIT(ret);

	ret = streamon(fd_cap, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	IF_ERR_EXIT(ret);

	ret = dqbuf(fd_cap);
	IF_ERR_EXIT(ret);

	pthread_join(th_sinkFile, NULL);
	close(fd_dec);

	return 0;
}

static int test_emulate_media_capture_m2m(priv_t *priv)
{
	int ret, fd_dec, picW, picH, bufcnt;
	struct v4l2_format fmt;
	pthread_t th_sinkFile;

	if (!opts.input) {
		printf("%s: no input\n", __func__);
		return -1;
	}

	picW = opts.width;
	picH = opts.height;
	bufcnt = opts.bufcnt;

	fd_dec = open(V4L2_EXT_DEV_PATH_VDEC, O_RDWR);
	IF_ERR_EXIT(errno);

	ret = init_src_ringbufs(fd_dec, &priv->src, 0);
	IF_ERR_EXIT(ret);

	/* out */
	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	ret = ioctl(fd_dec, VIDIOC_G_FMT, &fmt);
	IF_ERR_EXIT(ret);

	fmt.fmt.pix_mp.width = picW;
	fmt.fmt.pix_mp.height = picH;
	fmt.fmt.pix_mp.pixelformat = lookup(opts.source);
	ret = ioctl(fd_dec, VIDIOC_S_FMT, &fmt);
	IF_ERR_EXIT(ret);

	/* cap */
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	ret = ioctl(fd_dec, VIDIOC_G_FMT, &fmt);
	IF_ERR_EXIT(ret);

	fmt.fmt.pix_mp.width = picW;
	fmt.fmt.pix_mp.height = picH;
	fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12;
	ret = ioctl(fd_dec, VIDIOC_S_FMT, &fmt);
	IF_ERR_EXIT(ret);

	ret = init_cap_bufs(priv, fd_dec, picW, picH, fmt.fmt.pix_mp.num_planes, bufcnt);
	IF_ERR_EXIT(ret);

	s_ext_ctrls_val(fd_dec, V4L2_CID_EXT_VDEC_USERTYPE, V4L2_EXT_VDEC_USERTYPE_MEDIA_CAPTURE_M2M);
	s_ext_ctrls_ptr(fd_dec, V4L2_CID_EXT_VDEC_SRC_RINGBUFS, &priv->src);

	ret = pthread_create(&th_sinkFile, NULL, thread_sinkFile, priv);
	IF_ERR_EXIT(ret);

	//ret = streamon(fd_dec, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	//IF_ERR_EXIT(ret);

	ret = streamon(fd_dec, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	IF_ERR_EXIT(ret);

	ret = dqbuf(fd_dec);
	IF_ERR_EXIT(ret);

	pthread_join(th_sinkFile, NULL);
	close(fd_dec);

	return 0;
}

static int test_emulate_media_vdec_part(priv_t *priv)
{
	int ret, fd_dec, picW, picH;
	struct v4l2_format fmt;
	pthread_t th_sinkFile;

	if (!opts.input) {
		printf("%s: no input\n", __func__);
		return -1;
	}

	picW = opts.width;
	picH = opts.height;

	fd_dec = open(V4L2_EXT_DEV_PATH_VDEC, O_RDWR);
	IF_ERR_EXIT(errno);

	ret = init_src_ringbufs(fd_dec, &priv->src, 0);
	IF_ERR_EXIT(ret);

	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	ret = ioctl(fd_dec, VIDIOC_G_FMT, &fmt);
	IF_ERR_EXIT(ret);

	fmt.fmt.pix_mp.width = picW;
	fmt.fmt.pix_mp.height = picH;
	fmt.fmt.pix_mp.pixelformat = lookup(opts.source);
	ret = ioctl(fd_dec, VIDIOC_S_FMT, &fmt);
	IF_ERR_EXIT(ret);

	s_ext_ctrls_val(fd_dec, V4L2_CID_EXT_VDEC_USERTYPE, V4L2_EXT_VDEC_USERTYPE_MEDIA_CAPTURE);
	s_ext_ctrls_ptr(fd_dec, V4L2_CID_EXT_VDEC_SRC_RINGBUFS, &priv->src);

	ret = pthread_create(&th_sinkFile, NULL, thread_sinkFile, priv);
	IF_ERR_EXIT(ret);

	ret = s_output(fd_dec, opts.vcap_port);
	IF_ERR_EXIT(ret);

	ret = subscribe(fd_dec, V4L2_EVENT_EOS, 0);
	IF_ERR_EXIT(ret);

	ret = subscribe(fd_dec, V4L2_EVENT_SOURCE_CHANGE, 0);
	IF_ERR_EXIT(ret);

	ret = streamon(fd_dec, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	IF_ERR_EXIT(ret);

	ret = dqevent(priv, fd_dec);
	IF_ERR_EXIT(ret);

	pthread_join(th_sinkFile, NULL);
	close(fd_dec);

	return 0;
}

static int test_emulate_media_vcap_part(priv_t *priv)
{
	int ret, fd_cap, picW, picH, bufcnt;
	struct v4l2_format fmt;

	picW = opts.width;
	picH = opts.height;
	bufcnt = opts.bufcnt;

	fd_cap = open(V4L2_EXT_DEV_PATH_GPSCALER, O_RDWR);
	IF_ERR_EXIT(errno);

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	ret = ioctl(fd_cap, VIDIOC_G_FMT, &fmt);
	IF_ERR_EXIT(ret);

	fmt.fmt.pix_mp.width = picW;
	fmt.fmt.pix_mp.height = picH;
	fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12;
	ret = ioctl(fd_cap, VIDIOC_S_FMT, &fmt);
	IF_ERR_EXIT(ret);

	if (opts.import_dmabuf) {
		ret = import_dmabuf(priv, fd_cap, fmt.fmt.pix_mp.num_planes, bufcnt);
		IF_ERR_EXIT(ret);
	}

	else {
		ret = init_cap_bufs(priv, fd_cap, picW, picH, fmt.fmt.pix_mp.num_planes, bufcnt);
		IF_ERR_EXIT(ret);
	}

	ret = s_input(fd_cap, opts.vcap_port);
	IF_ERR_EXIT(ret);

	ret = streamon(fd_cap, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	IF_ERR_EXIT(ret);

	ret = dqbuf(fd_cap);
	IF_ERR_EXIT(ret);

	close(fd_cap);

	return 0;
}

static int test_emulate_dtv_direct_mode(priv_t *priv)
{
	int ret, fd_dec, fd_vdo;
	struct v4l2_format fmt;
	pthread_t thread;

	if (!opts.input) {
		printf("%s: no input\n", __func__);
		return -1;
	}

	fd_dec = open(V4L2_EXT_DEV_PATH_VDEC, O_RDWR);
	IF_ERR_EXIT(errno);

	fd_vdo = open(V4L2_EXT_DEV_PATH_VDO0, O_RDWR);
	IF_ERR_EXIT(errno);

	ret = s_ctrl(fd_dec, V4L2_CID_EXT_VDEC_CHANNEL, 0);
	IF_ERR_EXIT(ret);

	/* connect vdo0 to vdec channel 0 */
	ret = s_input(fd_vdo, 0);
	IF_ERR_EXIT(ret);

	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	ret = ioctl(fd_dec, VIDIOC_G_FMT, &fmt);
	IF_ERR_EXIT(ret);

	fmt.fmt.pix_mp.width = opts.width;
	fmt.fmt.pix_mp.height = opts.height;
	fmt.fmt.pix_mp.pixelformat = lookup(opts.source);
	ret = ioctl(fd_dec, VIDIOC_S_FMT, &fmt);
	IF_ERR_EXIT(ret);

	s_ext_ctrls_val(fd_dec, V4L2_CID_EXT_VDEC_USERTYPE, V4L2_EXT_VDEC_USERTYPE_DTV);
	s_ctrl(fd_dec, V4L2_CID_EXT_VDEC_DIRECT_MODE, 1);

	ret = streamon(fd_dec, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	IF_ERR_EXIT(ret);

	priv->fd_dec = fd_dec;
	priv->sink = sinkFile_DirectMode;
	ret = pthread_create(&thread, NULL, thread_sinkFile, priv);
	IF_ERR_EXIT(ret);

	ret = subscribe(fd_dec, V4L2_EVENT_PRIVATE_EXT_VDEC_EVENT, V4L2_SUB_EXT_VDEC_FRAME);
	IF_ERR_EXIT(ret);

	ret = subscribe(fd_dec, V4L2_EVENT_PRIVATE_EXT_VDEC_EVENT, V4L2_SUB_EXT_VDEC_PICINFO);
	IF_ERR_EXIT(ret);

	ret = subscribe(fd_dec, V4L2_EVENT_PRIVATE_EXT_VDEC_EVENT, V4L2_SUB_EXT_VDEC_USERDATA);
	IF_ERR_EXIT(ret);

	ret = dqevent(priv, fd_dec);
	IF_ERR_EXIT(ret);

	pthread_join(thread, NULL);
	close(fd_dec);
	close(fd_vdo);

	return 0;
}

static int test_emulate_dtv_drip_mode(priv_t *priv)
{
	int ret, fd_dec, fd_vdo;
	struct v4l2_format fmt;
	pthread_t thread;

	if (!opts.input) {
		printf("%s: no input\n", __func__);
		return -1;
	}

	fd_dec = open(V4L2_EXT_DEV_PATH_VDEC, O_RDWR);
	IF_ERR_EXIT(errno);

	fd_vdo = open(V4L2_EXT_DEV_PATH_VDO0, O_RDWR);
	IF_ERR_EXIT(errno);

	ret = s_ctrl(fd_dec, V4L2_CID_EXT_VDEC_CHANNEL, 0);
	IF_ERR_EXIT(ret);

	/* connect vdo0 to vdec channel 0 */
	ret = s_input(fd_vdo, 0);
	IF_ERR_EXIT(ret);

	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	ret = ioctl(fd_dec, VIDIOC_G_FMT, &fmt);
	IF_ERR_EXIT(ret);

	fmt.fmt.pix_mp.width = opts.width;
	fmt.fmt.pix_mp.height = opts.height;
	fmt.fmt.pix_mp.pixelformat = lookup(opts.source);
	ret = ioctl(fd_dec, VIDIOC_S_FMT, &fmt);
	IF_ERR_EXIT(ret);

	s_ext_ctrls_val(fd_dec, V4L2_CID_EXT_VDEC_USERTYPE, V4L2_EXT_VDEC_USERTYPE_DTV);
	s_ctrl(fd_dec, V4L2_CID_EXT_VDEC_DRIPDEC_MODE, 1);

	ret = streamon(fd_dec, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	IF_ERR_EXIT(ret);

	priv->fd_dec = fd_dec;
	priv->sink = sinkFile_DripMode;
	ret = pthread_create(&thread, NULL, thread_sinkFile, priv);
	IF_ERR_EXIT(ret);

	ret = subscribe(fd_dec, V4L2_EVENT_PRIVATE_EXT_VDEC_EVENT, V4L2_SUB_EXT_VDEC_FRAME);
	IF_ERR_EXIT(ret);

	ret = subscribe(fd_dec, V4L2_EVENT_PRIVATE_EXT_VDEC_EVENT, V4L2_SUB_EXT_VDEC_PICINFO);
	IF_ERR_EXIT(ret);

	ret = subscribe(fd_dec, V4L2_EVENT_PRIVATE_EXT_VDEC_EVENT, V4L2_SUB_EXT_VDEC_USERDATA);
	IF_ERR_EXIT(ret);

	ret = dqevent(priv, fd_dec);
	IF_ERR_EXIT(ret);

	pthread_join(thread, NULL);
	close(fd_dec);
	close(fd_vdo);

	return 0;
}

static test_entry_t tests[] = {
	{1, test_emulate_dtv, "emulate dtv source"},
	{2, test_emulate_media_tunnel, "emulate media source with tunnel mode"},
	{3, test_emulate_media_capture, "emulate media source with video capture"},
	{4, test_emulate_media_capture_m2m, "emulate media source with video capture-m2m"},
	{5, test_emulate_media_vdec_part, "emulate the vdec part of media-capture model"},
	{6, test_emulate_media_vcap_part, "emulate the vcap part of media-capture model"},
	{7, test_emulate_dtv_direct_mode, "emulate dtv source with direct mode"},
	{8, test_emulate_dtv_drip_mode, "emulate dtv source with drip mode"},
};

static void getRange(char *str, int *start, int *end)
{
	char *p;

	*start = strtoll(str, NULL, 0);
	p = strstr(str, "-");
	*end = p? strtoll(p+1, NULL, 0) : *start;

	return;
}

static void runTest(priv_t *priv, int start, int end)
{
	int ret = 0, id, x, cnt, step;

	cnt = sizeof(tests) / sizeof(test_entry_t);
	step = (start > end)? -1 : 1;
	id = start;

	while (1) {

		for (x=0; x<cnt; x++) {
			if (tests[x].id == id) {
				printf("\n------------------------------------------------\n");
				printf("test_%02d running ...\n", id);
				ret = tests[x].func(priv);
				printf("test_%02d ret %d\n", id, ret);
				printf("------------------------------------------------\n\n");
				break;
			}
		}

		if (x == cnt)
			printf("no test_%02d\n", id);

		if (id == end) break;
		id += step;
	}

	return;
}

int main(int argc, char *argv[])
{
	char *p;
	int i, start, end, cnt;
	priv_t *priv;

	parseArgs(&opts, argc, argv);

	priv = malloc(sizeof(priv_t));
	memset(priv, 0, sizeof(priv_t));
	pthread_mutex_init(&priv->lock, NULL);

	if (opts.list_tests) {
		cnt = sizeof(tests) / sizeof(test_entry_t);
		for (i=0; i<cnt; i++)
			printf("test_%02d: %s\n", tests[i].id, tests[i].desc);
		return 0;
	}

	p = strtok(opts.test, " ");
	while (p) {
		getRange(p, &start, &end);
		runTest(priv, start, end);
		p = strtok(NULL, " ");
	}

	return 0;
}

