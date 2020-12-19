/*
 * include/linux/serial.h
 *
 * Copyright (C) 1992 by Theodore Ts'o.
 * 
 * Redistribution of this file is permitted under the terms of the GNU 
 * Public License (GPL)
 */
#ifndef _LINUX_SERIAL_H
#define _LINUX_SERIAL_H

#include <asm/page.h>
#include <uapi/linux/serial.h>

extern void usb_rtice_console_write(const char *buf, unsigned count);
#if defined(CONFIG_REALTEK_RTICE)// || defined(CONFIG_RTK_KDRV_RTICE)
extern int rtice_uart_handler(unsigned char data, unsigned int dir);
extern int rtice_usb2serial_handler(unsigned char *data, unsigned int len, unsigned int dir);
#endif
/*
 * Counters of the input lines (CTS, DSR, RI, CD) interrupts
 */

enum {
	RTICE_UART,
	RTICE_USB2SERIAL,
};

struct async_icount {
	__u32	cts, dsr, rng, dcd, tx, rx;
	__u32	frame, parity, overrun, brk;
	__u32	buf_overrun;
};

/*
 * The size of the serial xmit buffer is 1 page, or 4096 bytes
 */
#define SERIAL_XMIT_SIZE PAGE_SIZE

#include <linux/compiler.h>

#endif /* _LINUX_SERIAL_H */
