/*
 *  include/linux/fault_notifier.h
 *
 *  Copyright (C) 2016 Sangseok Lee <sangseok.lee@lge.com>
 *
 */

#ifndef _LINUX_FAULTNOTIFIER_H
#define _LINUX_FAULTNOTIFIER_H

#define	FN_TYPE_EXT4_CRASH		0x1
#define	FN_TYPE_ALLOC_FAIL		0x10
#define	FN_TYPE_FD_LEAK		0x20
#define	FN_TYPE_MMAP_LEAK		0x30
#define	FN_TYPE_TEST			0x80000000
#define	FN_TYPE_ALL			0xffffffff

/**
 * fn_kernel_notify - notify kernel fault to Fault Manager
 * @type: fault type
 * @fmt: format string as printk
 * @...: variable args as printk
 * Returns: if success 0, else
 *		-EINVAL : reason is NULL
 *		-ENOMEM : memory access fail
 *		-ENDEV  : Fault Manager not registered
 *
 * Before Fault Manager register path, Only one fault reason can be saved.
 * The saved fault reason fire as soon as Fault Manager register path.
 * If there are more than two fault before Fault Manager registere path,
 * return -ENODEV
 */

enum fn_type {
	FN_KERNEL,
	FN_DBGFRWK,
	FN_SOCKDRV,
	MAX_FN,
};

#ifdef CONFIG_FAULT_NOTIFIER
extern int _fn_kernel_notify(int type, int fn_type, const char *fmt, ...);

#define fn_kernel_notify(type, fmt, ...) \
     _fn_kernel_notify(type, FN_KERNEL, fmt, ##__VA_ARGS__)
#define fn_kdriver_notify(fmt, ...) \
     _fn_kernel_notify(FN_TYPE_ALL, FN_SOCKDRV, fmt, ##__VA_ARGS__)
#else
static inline int _fn_kernel_notify(int type, int fn_type, const char *fmt, ...) { return 0; };

#define fn_kernel_notify(type, fmt, ...) \
    _fn_kernel_notify(0, 0, fmt, ##__VA_ARGS__)
#define fn_kdriver_notify(fmt, ...) \
    _fn_kernel_notify(0, 0, fmt, ##__VA_ARGS__)
#endif

#endif /* _LINUX_FAULTNOTIFIER_H */
