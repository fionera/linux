/*
 * Copyright (c) 2018 VictorYang <victor_yang@realtek.com>
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
#include <sys/mman.h>
#include <poll.h>
#include <pthread.h>
#include <errno.h>

#include <linux/videodev2.h>

#pragma scalar_storage_order big-endian
#include <rpc_common.h>
#include <InbandAPI.h>
#include <VideoRPC_System.h>
#pragma scalar_storage_order default

#include "syswrap.h"
#include "ringbuf.h"
#include "rtkdma.h"

#define IF_ERR_EXIT(err_code) \
	if (err_code) { \
		printf("%s:%d ret %d\n", __FUNCTION__, __LINE__, err_code); \
		exit(-1); \
	}

#define RTK_DMA_DEV_PATH "/dev/rtkdma"

        // O_APPEND:   0x400
        // O_SYNC:     0x101000
        // O_CLOEXEC:  0x80000
        // O_RDWR:     0x2
        // O_ACCMODE:  0x3
        // O_NONBLOCK: 0x800

static unsigned char userdata_buf[4096];

typedef struct cap_buf_t {
	struct v4l2_buffer buffer;
	struct v4l2_plane planes[VIDEO_MAX_PLANES];
	int expbuf_fd[VIDEO_MAX_PLANES];
} cap_buf_t;
static cap_buf_t cap_bufs[64];

typedef struct lookup_entry_t {
	char name[32];
	int value;
} lookup_entry_t;

typedef struct test_entry_t {
	int id;
	int (*fun)(void);
	char desc[128];
} test_entry_t;

static int test_dma_1 (void)
{
        int ret = 0, fd_dma, fd_vdec;
        int clock_fd = 0;

        printf ("[+] dmaUserTest_1 \n");

        rtk_dmadev_request_t req;
        req.cmd = RTK_DMADEV_CMD_DMABUF_ACQUIRE;
        req.data.dma_acquire.size = 4096;
        req.data.dma_acquire.flags = O_SYNC;
        req.data.dma_acquire.group = getpid();

        rtk_dma_request_t vreq;
        vreq.size = 4096;
        vreq.attach = 1;

        // open rtkdma driver
        printf ("  [AP] open rtk_dma driver\n");
	fd_dma = open(RTK_DMA_DEV_PATH, O_RDWR);
	IF_ERR_EXIT(errno);
        printf ("  [AP] fd: %d\n", fd_dma);

        printf ("  [AP] ioctl rtk_dma to request fd\n");
	ret = ioctl(fd_dma, RTK_DMADEC_IOC_REQUEST, &req);
        clock_fd = req.data.dma_acquire.fd;
        printf ("  [AP] fd: %d size: %d group: %d\n", req.data.dma_acquire.fd,
                                                      req.data.dma_acquire.size,
                                                      req.data.dma_acquire.group);
        printf ("starting sleep 1 s....\n");
        sleep (1);

        req.cmd = RTK_DMADEV_CMD_DMABUF_RELEASE;
        printf ("  [AP] ioctl rtk_dma to close fd:%d \n", req.data.dma_acquire.fd);
	ret = ioctl(fd_dma, RTK_DMADEC_IOC_REQUEST, &req);

        printf ("  [AP] req.fd %d\n", req.data.dma_acquire.fd);
        sleep (1);

        printf ("  [AP] close fd:%d \n", clock_fd);
        close(clock_fd);

        printf ("  [AP] close fd:%d \n", fd_dma);
        close(fd_dma);

        printf ("[-] dmaUserTest_1 %d \n", ret);

        return ret;
}

/********************************************************
  Test Case 2
  (1) AP   : alloc dmabuf -> get fd -> pass to VDEC driver
  (2) VDEC : get dmabuf via fd -> attach dmabuf
  (3) ADEC : get dmabuf via fd -> attach dmabuf
  (4) AP   : close fd
  (5) VDEC : detach dmabuf -> put dmabuf
  (6) ADEC : detach dmabuf -> put dmabuf
 ********************************************************/
static int test_dma_2 (void)
{
        int ret = 0, fd_dma, fd_vdec, fd_adec;
        int clock_fd = 0;

        printf ("[+] dmaUserTest_2 \n");

        rtk_dmadev_request_t req;
        req.cmd = RTK_DMADEV_CMD_DMABUF_ACQUIRE;
        req.data.dma_acquire.size = 4096;
        req.data.dma_acquire.flags = O_SYNC;
        req.data.dma_acquire.group = getpid();

        rtk_dma_request_t vreq;
        vreq.size = 4096;
        vreq.attach = 1;

        rtk_dma_request_t areq;
        areq.size = 4096;
        areq.attach = 1;

        // open rtkdma driver
	fd_dma = open(RTK_DMA_DEV_PATH, O_RDWR);
	IF_ERR_EXIT(errno);
        printf ("  [AP] fd: %d\n", fd_dma);

        printf ("  [AP] ioctl dmadev request\n");
	ret = ioctl(fd_dma, RTK_DMADEC_IOC_REQUEST, &req);
        clock_fd = req.data.dma_acquire.fd;
        printf ("  [AP] fd: %d size: %d group: %d\n", req.data.dma_acquire.fd,
                                                      req.data.dma_acquire.size,
                                                      req.data.dma_acquire.group);
        // open vdec driver
        printf ("  [AP] open vdec driver\n");
	fd_vdec = open(RTK_VDEC_DEVICE_PATH, O_RDWR);
	IF_ERR_EXIT(errno);

        printf ("  [AP] passing fd %d to vdec driver by ioctl\n", clock_fd);
        vreq.fd = clock_fd;
	ret = ioctl(fd_vdec, VDEC_IOC_DMA_OPEN, &vreq);

        printf ("... sleep 2 s ...\n");
        sleep (2);

        // open adec driver
        printf ("  [AP] open adec driver\n");
	fd_adec = open(RTK_ADEC_DEVICE_PATH, O_RDWR);
	IF_ERR_EXIT(errno);

        printf ("  [AP] passing fd %d to adec driver by ioctl\n", clock_fd);
        areq.fd = clock_fd;
	ret = ioctl(fd_adec, RTKAUDIO_IOC_DMA_OPEN, &areq);

        printf ("... sleep 2 s ...\n");
        sleep (2);

        // user close fd first
        printf ("  [AP] close fd: %d\n", clock_fd);
        close(clock_fd);

        printf ("... sleep 5 s ....\n");
        sleep (5);

        // audio close fd
        printf ("  [AP] adec close fd: %d \n", clock_fd);
	ret = ioctl(fd_adec, RTKAUDIO_IOC_DMA_CLOSE, &areq);

        printf ("... sleep 2 s ....\n");
        sleep (2);

        // vdec close fd
        printf ("  [AP] vdec close fd: %d \n", clock_fd);
	ret = ioctl(fd_vdec, VDEC_IOC_DMA_CLOSE, &vreq);

        printf ("... sleep 1 s ....\n");
        sleep (1);

        printf ("  [AP] user close dma_fd: %d \n", fd_dma);
        close(fd_dma);

        printf ("[-] dmaUserTest_2 %d \n", ret);

        return ret;
}

/********************************************************
  Test Case 3
  (1) AP   : alloc dmabuf -> get fd
  (2) AP   : mmap fd
  (3) AP   : close fd
 ********************************************************/
static int test_dma_3 (void)
{
        int ret = 0, fd_dma;
        int clock_fd = 0;

        printf ("[+] dmaUserTest_3 \n");

        printf ("size of REFCLOCK :%d\n ", sizeof (REFCLOCK));
        rtk_dmadev_request_t req;
        req.cmd = RTK_DMADEV_CMD_DMABUF_ACQUIRE;
        req.data.dma_acquire.size = 4096;
        req.data.dma_acquire.flags = O_SYNC;
        req.data.dma_acquire.group = getpid();

        // open rtkdma driver
        printf ("  [AP] open rtk_dma driver\n");
	fd_dma = open(RTK_DMA_DEV_PATH, O_RDWR);
	IF_ERR_EXIT(errno);
        printf ("  [AP] fd: %d\n", fd_dma);

        printf ("  [AP] ioctl rtk_dma driver\n");
	ret = ioctl(fd_dma, RTK_DMADEC_IOC_REQUEST, &req);
        clock_fd = req.data.dma_acquire.fd;
        printf ("  [AP] fd: %d size: %d group: %d\n", req.data.dma_acquire.fd,
                                                      req.data.dma_acquire.size,
                                                      req.data.dma_acquire.group);

        char* p = (char *) mmap (0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, clock_fd, 0L);
        printf ("  [AP] mmap fd:%d %p %d \n", clock_fd, p, errno);

        printf ("  [AP] close fd:%d %p %d \n", clock_fd, p, errno);
        close(clock_fd);

        printf ("  [AP] close dma_fd:%d \n", fd_dma);
        close(fd_dma);

        printf ("[-] dmaUserTest_3 %d \n", ret);

        return ret;
}
/********************************************************
  Test Case 4
  (1) AP   : alloc dmabuf -> get fd
  (2) VDEC : get dmabuf via fd -> attach dmabuf
  (3) ADEC : get dmabuf via fd -> attach dmabuf
  (4) AP   : kill -9 getpid()
 ********************************************************/
static int test_dma_4 (void)
{
        int ret = 0, fd_dma, fd_vdec, fd_adec;
        int clock_fd = 0;

        printf ("[+] dmaUserTest_4 \n");

        rtk_dmadev_request_t req;
        req.cmd = RTK_DMADEV_CMD_DMABUF_ACQUIRE;
        req.data.dma_acquire.size = 4096;
        req.data.dma_acquire.flags = O_SYNC;
        req.data.dma_acquire.group = getpid();

        rtk_dma_request_t vreq;
        vreq.size = 4096;
        vreq.attach = 1;

        rtk_dma_request_t areq;
        areq.size = 4096;
        areq.attach = 1;

        // open rtkdma driver
        printf ("  [AP] open rtk_dma driver\n");
	fd_dma = open(RTK_DMA_DEV_PATH, O_RDWR);
	IF_ERR_EXIT(errno);
        printf ("  [AP] fd: %d\n", fd_dma);

        printf ("  [AP] ioctl rtk_dma driver\n");
	ret = ioctl(fd_dma, RTK_DMADEC_IOC_REQUEST, &req);
        clock_fd = req.data.dma_acquire.fd;
        printf ("  [AP] fd: %d size: %d group: %d\n", req.data.dma_acquire.fd,
                                                      req.data.dma_acquire.size,
                                                      req.data.dma_acquire.group);
        // open vdec driver
        printf ("  [AP] open vdec driver\n");
	fd_vdec = open(RTK_VDEC_DEVICE_PATH, O_RDWR);
	IF_ERR_EXIT(errno);

        printf ("  [AP] passing fd %d to vdec driver by ioctl\n", clock_fd);
        vreq.fd = clock_fd;
	ret = ioctl(fd_vdec, VDEC_IOC_DMA_OPEN, &vreq);

        printf ("... sleep 2 s ...\n");
        sleep (2);

        // open adec driver
        printf ("  [AP] open adec driver\n");
	fd_adec = open(RTK_ADEC_DEVICE_PATH, O_RDWR);
	IF_ERR_EXIT(errno);

        printf ("  [AP] passing fd %d to adec driver by ioctl\n", clock_fd);
        areq.fd = clock_fd;
	ret = ioctl(fd_adec, RTKAUDIO_IOC_DMA_OPEN, &areq);

        printf ("... sleep 100 s, kill %d process please ....\n", req.data.dma_acquire.group);
        sleep (100);

        printf ("[-] dmaUserTest_4 %d \n", ret);

        return ret;
}

static test_entry_t tests[] = {
	{1, test_dma_1, "emulate dma ioctl"},
	{2, test_dma_2, "emulate dma/vdec/adec ioctl"},
	{3, test_dma_3, "emulate dma mmap"},
	{4, test_dma_4, "emulate kill ap process"},
};

static void getRange(char *str, int *start, int *end)
{
	char *p;

	*start = strtoll(str, NULL, 0);
	p = strstr(str, "-");
	*end = p? strtoll(p+1, NULL, 0) : *start;

	return;
}

static void runTest(int start, int end)
{
	int ret = 0, id, x, cnt, step;

	cnt = sizeof(tests) / sizeof(test_entry_t);
        printf ("cnt:%d sizeof(tests):%d \n", cnt, sizeof(tests));
	step = (start > end)? -1 : 1;
	id = start;

	while (1) {

		for (x=0; x<cnt; x++) {
			if (tests[x].id == id) {
				printf("\n------------------------------------------------\n");
				printf("test_%02d running ...\n", id);
				ret = tests[x].fun();
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

	parseArgs(&opts, argc, argv);

	if (opts.list_tests) {
		cnt = sizeof(tests) / sizeof(test_entry_t);
		for (i=0; i<cnt; i++)
			printf("test_%02d: %s\n", tests[i].id, tests[i].desc);
		return 0;
	}

	p = strtok(opts.test, " ");
	while (p) {
		getRange(p, &start, &end);
		runTest(start, end);
		p = strtok(NULL, " ");
	}

	return 0;
}
