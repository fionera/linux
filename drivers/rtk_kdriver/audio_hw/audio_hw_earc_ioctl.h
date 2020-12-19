#ifndef AUDIO_HW_EARC_IOCTL_H
#define AUDIO_HW_EARC_IOCTL_H

#define AUDIO_EARC_DEVICE_NAME	"earc0"

/* 
 * NOTICE: kernel space routines. Should not be exported to the user space api
 */
#ifdef __KERNEL__

struct file;

long audio_hw_earc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

#endif /* __KERNEL__ */

#endif
