/*!
*********************************************************************************************************
* This product contains one or more programs protected under international and U.S. copyright laws
* as unpublished works.  They are confidential and proprietary to Dolby Laboratories.
* Their reproduction or disclosure, in whole or in part, or the production of derivative works therefrom
* without the express permission of Dolby Laboratories is prohibited.


* Copyright 2011 - 2013 by Dolby Laboratories.  All rights reserved.
*********************************************************************************************************
*/


#ifndef _TYPEDEF_H_
#define _TYPEDEF_H_

//RTK MARK
#ifdef RTK_EDBEC_CONTROL
#include <linux/types.h>
#include <rbus/timer_reg.h>
#define EXIT_SUCCESS    0
#define EXIT_FAILURE    1

#else
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
typedef int bool;
#define false 0
#define true 1
#define inline __inline
#endif


#define DM_FIXED_POINT// added by smfan

//#define DV_DEBUG
//#define DOLBYPQ_DEBUG

#define printf printk	// added by smfan
#define DV_EMERG      0      /* DV is unusable */
#define DV_ALERT      1    /* action must be taken immediately */
#define DV_CRIT       2    /* critical conditions */
#define DV_ERR        3    /* error conditions */
#define DV_WARNING    4    /* warning conditions */
#define DV_NOTICE     5    /* normal but significant condition */
#define DV_INFO       6    /* informational */
#define DV_DEBUG_INFO     7    /* debug-level messages */
#define DV_DEBUG_KERNEL     8    /* debug-level messages */

#define DV_DUMP_RPU          0x00000001
#define DV_DUMP_COMPOSE      0x00000002
#define DV_DUMP_DM           0x00000004
#define DV_DUMP_DmExecFxp    0x00000008
#define DV_DUMP_DM_FILE      0x00000010
#define DV_DUMP_COMPOSE_FILE 0x00000020
#define DV_DUMP_RPU_FILE     0x00000040
#define DV_DUMP_TCLUT_FILE   0x00000080
#define DV_DUMP_LUT3D_FILE   0x00000100

int DV_printk(int lvl,const char *fmt, ...);

#define DBG_PRINT_L(lvl,fmt, args...) DV_printk(lvl,fmt, ## args)
int DBG_PRINT_RESET(void);



#ifdef DV_DEBUG
#ifdef WIN32
#define DBG_PRINT(args, ...) printk(args, __VA_ARGS__)
#else
#define DBG_PRINT(s, args...) printk(s, ## args)
#endif
#else
#ifdef WIN32
#define DBG_PRINT(s, args, ...)
#else
/*#define DBG_PRINT(s, args...)*/
#define DBG_PRINT(fmt, args...) DV_printk(DV_DEBUG_KERNEL,fmt, ## args)
#endif
#endif


/* show LUT info */
//#define TC_LUT_SHOW	1			/* for B02 */
//#define FIXED_3DLUT_SHOW	1		/* for B05 */

/* disable WARN_ON */
#undef WARN_ON
#define WARN_ON(x)

#include <linux/kern_levels.h>
#ifndef KERN_EMERG
#define KERN_EMERG
#endif


#define Warning(format, ...)	  \
{\
  static unsigned int  LastPrintPTS;			\
  static unsigned int  WarningCount;			\
  unsigned int		 CurrentPTS = (rtd_inl(TIMER_SCPU_CLK90K_LO_reg));	\
  if (CurrentPTS - LastPrintPTS > 90090)	{ \
	printk("[%d]:", WarningCount+1);	\
	printk(format, ##__VA_ARGS__);	\
	LastPrintPTS = CurrentPTS;	\
	WarningCount = 0;		\
  } \
  else	\
	WarningCount++; \
}





typedef uint64_t 	    U64;
typedef uint64_t 	    u64;
typedef int64_t     	S64;
typedef int64_t     	s64;
typedef uint32_t 		U32;
typedef uint32_t 		u32;
typedef int32_t 	    S32;
typedef uint16_t		U16;
typedef int16_t 		S16;
typedef uint8_t 		U8;
typedef int8_t			S8;

// added by smfan
typedef long int                intptr_t;

#define INT32_MAX INT_MAX
#define INT32_MIN  INT_MIN

#define UINT32_MAX ((u32)~0U)

#define U64_MAX         ((u64)~0ULL)
#define S64_MAX         ((s64)(U64_MAX>>1))
#define S64_MIN         ((s64)(-S64_MAX - 1))


#define INT64_MIN S64_MIN
#define INT64_MAX S64_MAX






#endif // _TYPEDEF_H_
