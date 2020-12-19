#ifndef _RTK_VDEC_SVP_H_
#define _RTK_VDEC_SVP_H_

#define TYPE_SVP_PROTECT_CPB 0
#define TYPE_SVP_PROTECT_COMEM 1
#define TYPE_SVP_PROTECT_DISPLAY 2

int rtkvdec_svp_enable_protection (unsigned int addr, unsigned int size, unsigned int type);

int rtkvdec_svp_disable_protection (unsigned int addr, unsigned int size, unsigned int type);

#endif

