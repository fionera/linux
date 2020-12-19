#ifndef __ACASC_ATR_H__
#define __ACASC_ATR_H__

#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/spinlock.h>

#include <core/rtk_scd.h>
#include <core/rtk_scd_atr.h>
#include <adapter/rtk_scd_priv.h>
#define SC_ATR_INFO scd_atr_info

extern int SC_ParseATR(unsigned char* pATR, unsigned int AtrLen, SC_ATR_INFO* pAtrInfo);
extern int SC_CheckATR(unsigned char* pATR, unsigned int AtrLen);

extern int SC_GetDi(unsigned char di);
extern int SC_GetFi(unsigned char fi);
extern int SC_GetT14ETU(unsigned char TAx);
extern int SC_GetFmax(unsigned char fi);

#endif //__ACASC_ATR_H__