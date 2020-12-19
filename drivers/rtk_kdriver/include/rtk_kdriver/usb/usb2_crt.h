#ifndef _USB2_CRT_H_
#define _USB2_CRT_H_

#if defined(CONFIG_RTK_KDRV_EHCI_HCD_PLATFORM) || defined(CONFIG_RTK_KDRV_OHCI_HCD_PLATFORM)
extern void usb2_crt_on(void);
extern void usb2_crt_off(void);
#else
static inline void usb2_crt_on(void) {}
static inline void usb2_crt_off(void) {}
#endif

#endif
