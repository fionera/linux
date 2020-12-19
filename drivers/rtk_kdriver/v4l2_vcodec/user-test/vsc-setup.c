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
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "options_vsc.h"

#include "include/ioctrl/scaler/vsc_cmd_id.h"

#define IF_ERR_EXIT(err_code) \
	if (err_code) { \
		printf("%s:%d ret %d\n", __func__, __LINE__, err_code); \
		exit(-1); \
	}

int openVSC(int fd, int winID)
{
	int ret;

	ret = ioctl(fd, VSC_IOC_OPEN, &winID);
	IF_ERR_EXIT(ret);

	return 0;
}

int connectVSC(int fd, int winID, int input_type, int input_attr,
	int input_resourceIndex, int output_mode)
{
	int ret;
	VSC_CONNECT_PARA_T args;

	args.connectwid = winID;
	args.vsc_input.type = input_type;
	args.vsc_input.attr = input_attr;
	args.vsc_input.resourceIndex = input_resourceIndex;
	args.outputmode = output_mode;

	ret = ioctl(fd, VSC_IOC_CONNECT, &args);
	IF_ERR_EXIT(ret);

	return 0;
}

int setInputRegion(int fd, int winID, int picW, int picH)
{
	int ret;
	VSC_SET_FRAME_REGION_T args;;

	args.setframewid = winID;
	args.inregion.x = 0;
	args.inregion.y = 0;
	args.inregion.w = picW;
	args.inregion.h = picH;

	ret = ioctl(fd, VSC_IOC_SET_INPUTREGION, &args);
	IF_ERR_EXIT(ret);

	return 0;
}

int setOutputRegion(int fd, int winID, int outW, int outH)
{
	int ret;
	VSC_SET_FRAME_REGION_T args;;

	args.setframewid = winID;
	args.inregion.x = 0;
	args.inregion.y = 0;
	args.inregion.w = outW;
	args.inregion.h = outH;

	ret = ioctl(fd, VSC_IOC_SET_OUTPUTREGION, &args);
	IF_ERR_EXIT(ret);

	return 0;
}

int setWinBlank(int fd, int winID, int onoff, int color)
{
	int ret;
	VSC_WINBLANK_PARA_T args;

	args.winblankwid = winID;
	args.winblankbonoff = onoff;
	args.winblankcolor = color;

	ret = ioctl(fd, VSC_IOC_SET_WINBLANK, &args);
	IF_ERR_EXIT(ret);

	return 0;
}

int setup()
{
	int fd;

	fd = open("/dev/"VSC_DEVICE_NAME, O_RDWR);
	if (fd < 0) return -ENOENT;

	openVSC(fd, opts.winID);
	connectVSC(fd, opts.winID, opts.input_type, opts.input_attr,
		opts.input_resourceIndex, opts.output_mode);
	setInputRegion(fd, opts.winID, opts.picW, opts.picH);
	setOutputRegion(fd, opts.winID, opts.outW, opts.outH);
	setWinBlank(fd, opts.winID, false, KADP_VIDEO_DDI_WIN_COLOR_BLACK);

	return 0;
}

int main(int argc, char *argv[])
{
	parseArgs(&opts, argc, argv);
	setup();
	return 0;
}

