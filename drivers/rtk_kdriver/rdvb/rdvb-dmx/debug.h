#ifndef RDVB_DEBUG_H
#define RDVB_DEBUG_H

#include <linux/types.h>
#include <rdvb-common/misc.h>
#include "common.h"
#include "refclk.h"

/* FOR DEBUG*/
#define DGB_COLOR_RED_WHITE      "\033[1;31;47m"
#define DGB_COLOR_RED            "\033[1;31m"
#define DGB_COLOR_GREEN          "\033[1;32m"
#define DGB_COLOR_YELLOW         "\033[1;33m"
#define DGB_COLOR_BLUE           "\033[1;34m"
#define DGB_COLOR_PURPLE         "\033[1;35m"
#define DGB_COLOR_CYAN           "\033[1;36m"
#define DGB_COLOR_NONE           "\033[m"
#define DGB_COLOR_NONE_N         "\033[m \n"
#define RTKDEMUX_DBG_DUMP_AVSYNC_INFO_FILENAME_MAX_LENGTH 64
struct DBG_SETTING {
	unsigned long log_mask;

	unsigned char nodemux_audio;
	unsigned char nodemux_video;

	//print io
	unsigned char printio;
	unsigned long printio_read_count;
	unsigned long printio_deliver_count[NUMBER_OF_PINS];

	unsigned char esbufmonitor_off;

	//dump ts
	struct {
		unsigned char enable;
		unsigned char flush_en;
		unsigned char do_flush;
		struct file *fp;
		loff_t pos;
		unsigned char filename[1024];
	} dump_ts;

	//dump es
	struct {
		unsigned char enable;
		unsigned char flush_en;
		struct file *fp;
		loff_t pos;
		unsigned char filename[1024];
	} dump_es[NUMBER_OF_PINS];

	//dump pts
	struct {
		unsigned char enable;
		struct file *fp;
		loff_t pos;
	} dump_pts;

	//dump sync info
	struct {
		unsigned char enable;
		struct file *fp;
		loff_t pos;
		unsigned char filename[RTKDEMUX_DBG_DUMP_AVSYNC_INFO_FILENAME_MAX_LENGTH];
	} dump_sync_info;

	struct proc_dir_entry *proc_dir;
	struct proc_dir_entry *proc_entry;
};
extern struct DBG_SETTING g_dbgSetting[DEMUX_TP_NUM];

/* Print IO */
void DEBUG_CumulateReadSize(unsigned char chid, unsigned long read_size);
void DEBUG_CumulateDeliverSize(unsigned char chid, int pin_idx, unsigned long deliver_size);
void DEBUG_Printio(unsigned char chid);

/* Dump AvSync Info */
typedef struct {
	long long Pcr;
	long long Stc;
	long long Rcd;
	long long AudioDemuxPts;
	long long VideoDemuxPts;
	long long AudioSystemPts;
	long long VideoSystemPts;
	long long PcrOffset;
	long long MasterPTS;
	long long SystemPts;
} RTKDEMUX_AVSYNC_INFO_T;

bool DEBUG_proc_init(unsigned char chid);
void DEBUG_proc_uninit(unsigned char chid);

//dump ts
void DEBUG_DumpTsEnableFlush(unsigned char chid);
void DEBUG_DumpTs(unsigned char chid, char *buffer, unsigned int size);

//dump es
void DEBUG_DumpEs(unsigned char chid, int pin_index, char *buffer, unsigned int size);
void DEBUG_DumpEsFlush(unsigned char chid, int pin_index);

void rtkdemux_dbg_dump_pes(int tp_index, char *buffer, unsigned int size, unsigned int dumppid);
void rtkdemux_dbg_dump_pes_flush(int tp_index);

//dump sync info
void DEBUG_DumpSyncInfo(unsigned char chid, const RTKDEMUX_AVSYNC_INFO_T *info);
void DEBUG_FlushSyncInfo(unsigned char chid);

//dump pts
void DEBUG_DumpPts(unsigned char chid, int pin_index, int64_t pts, unsigned char* WP);

void rtkdemux_dbg_force_mastership_check(int tp_index, REFCLOCK *refClock, AV_SYNC_MODE *avSyncMode);


#endif /* RDVB_DEBUG_H */
