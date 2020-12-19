#ifndef _CLOCK_H
#define _CLOCK_H

#include <linux/kernel.h>
#include <linux/types.h>

#if !defined(DEMUX_CLOCKS_PER_SEC) && !defined(DEMUX_CLOCKS_PER_MS)
	#define DEMUX_CLOCKS_PER_SEC (90000)
	#define DEMUX_CLOCKS_PER_MS  (90)
#endif

enum clock_type {
	CLOCK_MISC,
	CLOCK_PCR,
	CLOCK_PCRA  =  CLOCK_PCR,
#ifdef CONFIG_RTK_KDRV_MULTI_TP_CLOCK
	CLOCK_PCRB,
#endif
	CLOCK_NUM,
};
int64_t CLOCK_GetPTS(void);
int64_t CLOCK_GetTPPTS(enum clock_type clock_type);
int64_t CLOCK_GetAVSyncPTS(enum clock_type clock_type);

#endif /* _CLOCK_H */
