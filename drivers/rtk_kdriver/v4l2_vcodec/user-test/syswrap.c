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
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define DEV_MEM	 "/dev/mem"
#define DEV_RTKMEM  "/dev/rtkmem"

void* syswrap_mmap(uint32_t phyaddr, uint32_t size)
{
	int offset, fd;
	void *ret;

	offset = phyaddr & 0xfff;

	if ((fd = open(DEV_MEM, O_RDWR|O_SYNC)) < 0 && (fd = open(DEV_RTKMEM, O_RDWR|O_SYNC)) < 0) {
		printf("open /dev/mem fail\n");
		return NULL;
	}

	ret = mmap(NULL, size + offset, PROT_READ | PROT_WRITE, MAP_SHARED, fd, phyaddr - offset);

	close(fd);

	if (ret == MAP_FAILED) {
		printf("mmap fail (addr=%x size=%x)\n", phyaddr, size);
		return NULL;
	}

	return (void*)((uint32_t)ret + offset);
}

int syswrap_munmap(void *ptr, uint32_t size)
{
	int offset = (unsigned int)ptr & 0xfff;
	return munmap((void*)((uint32_t)ptr-offset), offset+size);
}

