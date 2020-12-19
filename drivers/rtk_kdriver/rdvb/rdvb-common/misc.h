#ifndef __RDVB_COMMON_MISC__
#define __RDVB_COMMON_MISC__

#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/printk.h>
#include <mach/rtk_log.h>
#include <linux/types.h>
#define DEMUX_TAG_NAME "RDVB"

u32 reverse_int32(u32 value);
u64 reverse_int64(u64 value);
extern unsigned int rdvb_log_level;

enum DEMUX_LOG_MASK_BIT{
	DEMUX_LOG_MASK_BIT_0 = 1 << 0,
		DEMUX_FUNCTIN_CALLER=DEMUX_LOG_MASK_BIT_0,
	DEMUX_LOG_MASK_BIT_1 = 1 << 1,
		DEMUX_NOMAL_DEBUG 	= DEMUX_LOG_MASK_BIT_1,
	DEMUX_LOG_MASK_BIT_2 = 1 << 2,
		DEMUX_BUFFER_DEBUG	= DEMUX_LOG_MASK_BIT_2,
	DEMUX_LOG_MASK_BIT_3 = 1 << 3,
		DEMUX_NOTICE_PRINT = DEMUX_LOG_MASK_BIT_3,// use this level to print, nomal log and mask log  same time,
	//avsync-----------------------------
	DEMUX_LOG_MASK_BIT_4 = 1 << 4,
		DEMUX_PCRSYC_DEBUG	= DEMUX_LOG_MASK_BIT_4,
	DEMUX_LOG_MASK_BIT_5 = 1<< 5,
		DEMUX_PCRTRACK_DEBUG = DEMUX_LOG_MASK_BIT_5,
	DEMUX_LOG_MASK_BIT_6 = 1<< 6,
		DEMUX_PCRSYC_DEBUG_EX1	= DEMUX_LOG_MASK_BIT_6,
	DEMUX_LOG_MASK_BIT_7 = 1<< 7,
		DEMUX_LR_DEBUG = DEMUX_LOG_MASK_BIT_7,
	//pes/section------------------------------
	DEMUX_LOG_MASK_BIT_8 = 1 << 8,
		DEMUX_SECTION_DEBUG 	= DEMUX_LOG_MASK_BIT_8,
	DEMUX_LOG_MASK_BIT_9 = 1<< 9,
		DEMUX_SECTION_CB = DEMUX_LOG_MASK_BIT_9,
	DEMUX_LOG_MASK_BIT_10 = 1<< 10,
		DEMUX_FILTER_DEBUG = DEMUX_LOG_MASK_BIT_10,
	DEMUX_LOG_MASK_BIT_11 = 1<< 11,
		DEMUX_FILTER_CB = DEMUX_LOG_MASK_BIT_11,
	//pvr---------------------------:x000
	DEMUX_LOG_MASK_BIT_12 = 1<< 12,
		DEMUX_AUDIO_STABLE_DEBUG = DEMUX_LOG_MASK_BIT_12,
	DEMUX_LOG_MASK_BIT_13 = 1<< 13,
		DEMUX_PVR_ES_MONITOR_DEBUG = DEMUX_LOG_MASK_BIT_13,
	DEMUX_LOG_MASK_BIT_14 = 1<< 14,
		DEMUX_SCRAMBLE_CHECK_DEBUG = DEMUX_LOG_MASK_BIT_14,
	DEMUX_LOG_MASK_BIT_15 = 1<<15,

	//err 
	DEMUX_LOG_MASK_BIT_16 = 1<<16,
		DMX_TEMI_DBG = DEMUX_LOG_MASK_BIT_16,
	DEMUX_LOG_MASK_BIT_17 = 1<<17,
		DMX_ERROR_2 = DEMUX_LOG_MASK_BIT_17,
	DEMUX_LOG_MASK_BIT_18 = 1<<18
		,DMX_ERROR_3 = DEMUX_LOG_MASK_BIT_18,
	DEMUX_LOG_MASK_BIT_19 = 1<<19,
		DMX_ERROR_4 = DEMUX_LOG_MASK_BIT_19,
	//ecp
	DEMUX_LOG_MASK_BIT_20 = 1<<20,
		DMX_ECP0_DBG = DEMUX_LOG_MASK_BIT_20,
	DEMUX_LOG_MASK_BIT_21 = 1<<21,
		DMX_ECP1_DBG = DEMUX_LOG_MASK_BIT_21,
	DEMUX_LOG_MASK_BIT_22 = 1<<22
		,DMX_ECP2_DBG = DEMUX_LOG_MASK_BIT_22,
	DEMUX_LOG_MASK_BIT_23 = 1<<23,
		DMX_ECP3_DBG = DEMUX_LOG_MASK_BIT_23,
	//DESCRAMBLER
	DEMUX_LOG_MASK_BIT_24 = 1<<24,
		DMX_DESCRAMBLER_DBG = DEMUX_LOG_MASK_BIT_24,
	DEMUX_LOG_MASK_BIT_25 = 1<<25,
		DMX_PCRTRCK_DBG_01 = DEMUX_LOG_MASK_BIT_25,
	DEMUX_LOG_MASK_BIT_26 = 1<<26,
	DEMUX_LOG_MASK_BIT_27 = 1<<27,

	DEMUX_LOG_MASK_BIT_28 = 1<<28,
	DEMUX_LOG_MASK_BIT_29 = 1<<29,
	DEMUX_LOG_MASK_BIT_30 = 1<<30,
	DEMUX_LOG_MASK_BIT_31 = 1<<31,

};

#define DEMUX_TP_NUM 4
#define CHANNEL_UNKNOWN 0XFF
#define NOCH CHANNEL_UNKNOWN

#define dmx_print(level, ch, fmt, ...)  \
	do{ \
		if (ch<DEMUX_TP_NUM){ \
			rtd_printk(level, DEMUX_TAG_NAME"-%d", "[%s@%d]\t"fmt, \
				ch, __func__, __LINE__, ##__VA_ARGS__); \
		} else { \
			rtd_printk(level, DEMUX_TAG_NAME"  ", "[%s@%d]\t"fmt, \
				__func__, __LINE__, ##__VA_ARGS__); \
		} \
	}while(0);

#define dmx_emerg(ch, fmt, ...)   dmx_print(KERN_EMERG,   ch, fmt, ##__VA_ARGS__);
#define dmx_alert(ch, fmt, ...)   dmx_print(KERN_ALERT,   ch, fmt, ##__VA_ARGS__);
#define dmx_crit(ch, fmt, ...)    dmx_print(KERN_CRIT,    ch, fmt, ##__VA_ARGS__);
#define dmx_err(ch, fmt, ...)     dmx_print(KERN_ERR,     ch, fmt, ##__VA_ARGS__);
#define dmx_warn(ch, fmt, ...)    dmx_print(KERN_WARNING, ch, fmt, ##__VA_ARGS__);
#define dmx_notice(ch, fmt, ...)  dmx_print(KERN_NOTICE,  ch, fmt, ##__VA_ARGS__);
#define dmx_info(ch, fmt, ...)    dmx_print(KERN_INFO,    ch, fmt, ##__VA_ARGS__);
#define dmx_dbg(ch, fmt, ...)     dmx_print(KERN_DEBUG,   ch, fmt, ##__VA_ARGS__);


#define dmx_mask_print(BIT, ch, fmt, ...) \
	do { \
		if ((BIT) & rdvb_log_level) { \
			dmx_emerg(ch, fmt, ##__VA_ARGS__); \
		} else if ((BIT) & DEMUX_NOTICE_PRINT) { \
			dmx_notice(ch, fmt, ##__VA_ARGS__); \
		} \
	}while(0);

/*
#define dmx_mask_print(BIT, fmt, ...)	\
	{if ((BIT) & rdvb_log_level) {dmx_emerg(0xff, fmt, ##__VA_ARGS__);} \}*/

#define dmx_function_call(ch)  {dmx_mask_print(DEMUX_FUNCTIN_CALLER, ch, "func %s, line %d\n", __func__, __LINE__)}

extern char * print_to_buf(char *buf, const char *fmt,...);

extern void set_log_level(unsigned int level);
extern unsigned int get_log_level(void);


typedef int (* ts_callback)(const u8 *buffer1, size_t buffer1_len, 
				    const u8 *buffer2, size_t buffer2_len,
				    void *source);

#define NON_CACHED_BUFF 0X00
#define CACHED_BUFF     0X01

#define MAX_VTP_NUM 2
#define MAX_ATP_NUM 2
#define MAX_PES_NUM 4
#define TP_BUF_MAX_NUM 4

#define __alignment_up(x, y) (x -x%y) 
#define __align_up_4k_6k(x) __alignment_up(x, 12*1024)

#define __alignment_down(x, align) (((x+align -1)/(align))*(align))
#define __align_down_extend(x, align) (__alignment_down(x, align)-x)

#define __align_down_4k_6k(x) __alignment_down(x, 12*1024)
#define __align_down_extend_4k_6k(x)  __align_down_extend(x, 12*1024)
//=============================================================================
#define LOCK_WRANLING_INTERVAL  90000 *10
#define LOCK(x)                                                               \
	do {                                                                         \
		uint64_t start = CLOCK_GetPTS();                                         \
		do{                                                                      \
			if (mutex_trylock(x) == 1)                                           \
				break;                                                           \
			if (CLOCK_GetPTS()- start >= LOCK_WRANLING_INTERVAL) {               \
				start = CLOCK_GetPTS();                                          \
				dmx_err(NOCH, "mutex_deadlock:mutex:%p, request:%d, owner:%d\n", \
					x,task_pid_nr(current), task_pid_nr((x)->owner));            \
			}                                                                    \
			msleep(10);                                                          \
		}while(1);                                                               \
	}while(0);



#define UNLOCK(X) mutex_unlock(X);
#endif