#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/file.h>
#include <linux/types.h>
#include <linux/unistd.h>
#include <rdvb-common/misc.h>
#include "sdec.h"
#include "debug.h"
#include "rdvb_dmx_ctrl_internal.h"


#define RDVB_PROC_ENTRY "dbg"
#define RDVB_MAX_CMD_LENGTH 256

enum {
	DBG_CMD_LOG_LEVEL = 0,
	DBG_CMD_SET_PCR_OFFSET = 1,
	DBG_CMD_DONT_DELIVER_AUDIO = 2,
	DBG_CMD_DONT_DELIVER_VIDEO = 3,
	DBG_CMD_DTV_FORCE_MASTERSHIP = 4,
	DBG_CMD_PRINT_IO = 5,
	DBG_CMD_DUMP_TS = 6,
	DBG_CMD_DUMP_ES = 7,
	DBG_CMD_DUMP_AVSYNC_INFO = 8,
	DBG_CMD_DUMP_PTS = 9,
	DBG_CMD_PRINT_DEMUX_TARGET = 10,
	DBG_CMD_PRINT_FILTER_LIST = 11,
	DBG_CMD_PRINT_SECTION_FILTER_LIST = 12,
	DBG_CMD_PCR_TRACK = 13,
	DBG_CMD_DUMP_PES = 14,
	DBG_CMD_ES_MONITOR_OFF = 15,
	DBG_CMD_ECP_MODE = 16,
	DBG_CMD_NUMBER
} DBG_CMD_INDEX_T;

static const char *g_cmd_str[] = {
	"level=",			/* DBG_CMD_LOG_LEVEL */
	"pcroffset=",		/* DBG_CMD_SET_PCR_OFFSET */
	"nodemuxaudio=",    /* DBG_CMD_DONT_DELIVER_AUDIO */
	"nodemuxvideo=",    /* DBG_CMD_DONT_DELIVER_VIDEO */
	"mastership=",		/* DBG_CMD_DTV_FORCE_MASTERSHIP */
	"printio=",			/* DBG_CMD_PRINT_IO */
	"dumpts=",			/* DBG_CMD_DUMP_TS */		/* dump_ts=1,/tmp/dump.ts */
	"dumpes=",			/* DBG_CMD_DUMP_ES */  /* dump_es=1,audio,/tmp/dump.ac3 */
	"dumpavsyncinfo=",  /* DBG_CMD_DUMP_AVSYNC_INFO */
	"dumppts=",			/* DBG_CMD_DUMP_PTS */
	"demux_target", 	/* DBG_CMD_PRINT_DEMUX_TARGET */
	"filter_list",		/* DBG_CMD_PRINT_FILTER_LIST */
	"section_filter",	/* DBG_CMD_PRINT_SECTION_FILTER_LIST */
	"pcrtrack=",		/* DBG_CMD_PCR_TRACK */
	"dumppes=",			/* DEMUX_DBG_CMD_DUMP_PES */
	"esmonitor_off=",	/* DBG_CMD_ES_MONITOR_OFF */
	"ecp=",				/* DEMUX_DBG_CMD_ECP_MODE */

};

struct DBG_SETTING g_dbgSetting[DEMUX_TP_NUM];

void DEBUG_Execute(unsigned char chid, const int cmd_index, char **cmd_pointer);
extern SDEC_CHANNEL_T* rdvb_dmx_getSdec(unsigned char chid);


extern struct list_head filter_list;
extern void rdvbdmx_filter_dump(struct list_head *filters);

static int demux_proc_open0(struct inode *inode, struct file *file)
{
	file->private_data = (void*)0;
	return 0;
}
static int demux_proc_open1(struct inode *inode, struct file *file)
{
	file->private_data = (void*)1;
	return 0;
}
static int demux_proc_open2(struct inode *inode, struct file *file)
{
	file->private_data = (void*)2;
	return 0;
}
static int demux_proc_open3(struct inode *inode, struct file *file)
{
	file->private_data = (void*)3;
	return 0;
}
static ssize_t demux_proc_write(
	struct file *file,
	const char __user *buf,
	size_t count,
	loff_t *ppos)
{
	int cmd_index;
	char *cmd_pointer = NULL;
	char str[RDVB_MAX_CMD_LENGTH] = {0};
	unsigned char chid = (unsigned char)(unsigned long)file->private_data;;

	if (buf == NULL) {
		return -EFAULT;
	}
	if (count > RDVB_MAX_CMD_LENGTH - 1)
		count = RDVB_MAX_CMD_LENGTH - 1;
	if (copy_from_user(str, buf, RDVB_MAX_CMD_LENGTH - 1)) {
		return -EFAULT;
	}

	if (count > 0)
		str[count-1] = '\0';
	printk("demux_debug:%d demux:%d >>%s\n", __LINE__,chid, str);


	for (cmd_index = 0; cmd_index < DBG_CMD_NUMBER; cmd_index++) {
		if (strncmp(str, g_cmd_str[cmd_index], strlen(g_cmd_str[cmd_index])) == 0)
			break;
	}


	if (cmd_index >= DBG_CMD_NUMBER)
		return count;

	cmd_pointer = &str[strlen(g_cmd_str[cmd_index])];


	DEBUG_Execute(chid, cmd_index, &cmd_pointer);

	return count;
}

static const struct file_operations g_demux_proc_fops[4] = {
	{
	.owner = THIS_MODULE,
	.open = demux_proc_open0,
	.write = demux_proc_write,
	},
	{
	.owner = THIS_MODULE,
	.open = demux_proc_open1,
	.write = demux_proc_write,
	},
	{
	.owner = THIS_MODULE,
	.open = demux_proc_open2,
	.write = demux_proc_write,
	},
	{
	.owner = THIS_MODULE,
	.open = demux_proc_open3,
	.write = demux_proc_write,
	}
	
};

bool DEBUG_proc_init(unsigned char chid)
{
	static const char* demux_dir[DEMUX_TP_NUM] = {"demux0", "demux1", "demux2", "demux3"};

	if (chid >= DEMUX_TP_NUM)
		return false;

	if (g_dbgSetting[chid].proc_dir != NULL || g_dbgSetting[chid].proc_entry != NULL)
		return true;

	g_dbgSetting[chid].proc_dir = proc_mkdir(demux_dir[chid] , NULL);

	if (g_dbgSetting[chid].proc_dir == NULL) {
		dmx_crit(chid, "create proc entry (%s) failed\n", demux_dir[chid]);
		return false;
	}

	g_dbgSetting[chid].proc_entry = proc_create(RDVB_PROC_ENTRY, 0666,
					g_dbgSetting[chid].proc_dir, &g_demux_proc_fops[chid]);

	if (g_dbgSetting[chid].proc_entry == NULL) {
		proc_remove(g_dbgSetting[chid].proc_dir);
		g_dbgSetting[chid].proc_dir = NULL;
		dmx_crit(chid, "failed to get proc entry for %s/%s\n",
					demux_dir[chid], RDVB_PROC_ENTRY);
		return false;
	}

	return true;
}

void DEBUG_proc_uninit(unsigned char chid)
{
	if (chid >= DEMUX_TP_NUM)
		return;

	if (g_dbgSetting[chid].proc_entry) {
		proc_remove(g_dbgSetting[chid].proc_entry);
		g_dbgSetting[chid].proc_entry = NULL;
	}


	if (g_dbgSetting[chid].proc_dir) {
		proc_remove(g_dbgSetting[chid].proc_dir);
		g_dbgSetting[chid].proc_dir = NULL;
	}
}
static void DEBUG_PrintDemuxTarget(SDEC_CHANNEL_T *pCh)
{
	/*static const char *targetNames[DEMUX_NUM_OF_TARGETS] = {"VIDEO0", "AUDIO0", "VIDEO1", "AUDIO1"};
	int i = 0;*/
	if (pCh == 0)
		return;
/*
	pr_emerg("===============================================================\n");
	for (i = 0; i < DEMUX_NUM_OF_TARGETS; i++) {
		if (pCh->activeTarget[i].pid < 0 || pCh->activeTarget[i].pid >= 0x1fff)
			continue;
		pr_emerg("pid = %d, target = %s\n", pCh->activeTarget[i].pid, targetNames[i]);
	}
	pr_emerg("===============================================================\n");*/
}
////////////////////////////////////////////////////////////////////////////////////////
// print io

void DEBUG_CumulateDeliverSize(unsigned char chid, int pin_idx, unsigned long deliver_size)
{
	if (chid < DEMUX_TP_NUM && pin_idx >= 0 && pin_idx < NUMBER_OF_PINS) {
		g_dbgSetting[chid].printio_deliver_count[pin_idx] += deliver_size;
	}
}
void DEBUG_CumulateReadSize(unsigned char chid, unsigned long read_size)
{
	if (chid < DEMUX_TP_NUM)
		g_dbgSetting[chid].printio_read_count += read_size;
}
void DEBUG_Printio(unsigned char chid)
{
	static int64_t clock[DEMUX_TP_NUM] = {0};


	if (chid >= DEMUX_TP_NUM)
		return;
	if (CLOCK_GetPTS()-clock[chid] >= DEMUX_CLOCKS_PER_SEC) {

		printk(
			DGB_COLOR_GREEN"CH[%d]: READ:%lu"
			", VIDEO:%lu, AUDIO:%lu, VIDEO2:%lu, AUDIO2:%lu\n"DGB_COLOR_NONE,
			chid,
			g_dbgSetting[chid].printio_read_count,
			g_dbgSetting[chid].printio_deliver_count[VIDEO_PIN],
			g_dbgSetting[chid].printio_deliver_count[AUDIO_PIN],
			g_dbgSetting[chid].printio_deliver_count[VIDEO2_PIN],
			g_dbgSetting[chid].printio_deliver_count[AUDIO2_PIN]
		);

		g_dbgSetting[chid].printio_read_count = 0;

		memset(g_dbgSetting[chid].printio_deliver_count, 0, sizeof(g_dbgSetting[chid].printio_deliver_count));

		clock[chid] = CLOCK_GetPTS();
	}
}
//////////////////////////////////////////////////////////////////////////////////
//dump ts


/* use IS_ERR(x) to check the (struct file *) */
void DEBUG_CreateFile(const char *filename, struct file **handle)
{
	*handle = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, 0777);
}

void DEBUG_CloseFile(struct file *handle)
{
	filp_close(handle, NULL);
}

int DEBUG_WriteFile(struct file *handle, char *buffer, unsigned int size, loff_t *pOffset)
{
	int ret = 0;

	if (handle && pOffset) {
		mm_segment_t fs;
		fs = get_fs();
		set_fs(get_ds());    /* set to KERNEL_DS */
		vfs_write(handle, buffer, size, pOffset);
		set_fs(fs);
	}

	return ret;
}
static void DEBUG_StopDumpTs(unsigned char chid)
{
	if (g_dbgSetting[chid].dump_ts.enable) {
		g_dbgSetting[chid].dump_ts.enable = false;
		if (g_dbgSetting[chid].dump_ts.fp != NULL && !IS_ERR(g_dbgSetting[chid].dump_ts.fp))
			DEBUG_CloseFile(g_dbgSetting[chid].dump_ts.fp);
	}
	memset(&g_dbgSetting[chid].dump_ts, 0, sizeof(g_dbgSetting[chid].dump_ts));
}
static void DEBUG_StartDumpTs(unsigned char chid, const char* filename)
{
	DEBUG_CreateFile(filename, &g_dbgSetting[chid].dump_ts.fp);
	if (g_dbgSetting[chid].dump_ts.fp == NULL || IS_ERR(g_dbgSetting[chid].dump_ts.fp)) {
		printk(DGB_COLOR_RED"debug:%d dump ts start FAILED...\n"DGB_COLOR_NONE, __LINE__);
		g_dbgSetting[chid].dump_ts.fp = NULL;
		return;
	}

	g_dbgSetting[chid].dump_ts.enable = true;
}
void DEBUG_DumpTsEnableFlush(unsigned char chid)
{
	if (g_dbgSetting[chid].dump_ts.enable == 0)
		return;

	if (!g_dbgSetting[chid].dump_ts.flush_en)
		return;

	g_dbgSetting[chid].dump_ts.do_flush = 1;
}
void DEBUG_DumpTs(unsigned char chid, char *buffer, unsigned int size)
{
	int64_t clock;
	static int64_t notify_clock[DEMUX_TP_NUM] = {0};
	if (g_dbgSetting[chid].dump_ts.enable == 0)
		return;


	if (g_dbgSetting[chid].dump_ts.flush_en && g_dbgSetting[chid].dump_ts.do_flush) {
		g_dbgSetting[chid].dump_ts.enable = false;
		if (g_dbgSetting[chid].dump_ts.fp != NULL && !IS_ERR(g_dbgSetting[chid].dump_ts.fp))
		DEBUG_CloseFile(g_dbgSetting[chid].dump_ts.fp);
		g_dbgSetting[chid].dump_ts.fp = NULL;
		g_dbgSetting[chid].dump_ts.pos = 0;
		DEBUG_StartDumpTs(chid, g_dbgSetting[chid].dump_ts.filename);
		g_dbgSetting[chid].dump_ts.do_flush = 0;
	}
	DEBUG_WriteFile(g_dbgSetting[chid].dump_ts.fp, buffer, size, &g_dbgSetting[chid].dump_ts.pos);

	clock = CLOCK_GetPTS();
	if (clock-notify_clock[chid] > 5*DEMUX_CLOCKS_PER_SEC) {

		printk(DGB_COLOR_CYAN"Dump TP%d Processing...\n"DGB_COLOR_NONE, chid);
		notify_clock[chid] = clock;
	}

}
/////////////////////////////////////////////////////////////////////////////
// dump es
static const char *g_dbg_pin_name[NUMBER_OF_PINS] = { "VIDEO0", "AUDIO0", "VIDEO1", "AUDIO1" };
static void DEBUG_StopDumpEs(unsigned char chid, int pin_index)
{
	if (g_dbgSetting[chid].dump_es[pin_index].enable) {
		g_dbgSetting[chid].dump_es[pin_index].enable = false;
		if (g_dbgSetting[chid].dump_es[pin_index].fp != NULL && !IS_ERR(g_dbgSetting[chid].dump_es[pin_index].fp))
			DEBUG_CloseFile(g_dbgSetting[chid].dump_es[pin_index].fp);
	}
	memset(&g_dbgSetting[chid].dump_es[pin_index], 0, sizeof(g_dbgSetting[chid].dump_es[pin_index]));
}
static void DEBUG_StartDumpEs(unsigned char chid, int pin_index, const char* filename)
{
	DEBUG_CreateFile(filename, &(g_dbgSetting[chid].dump_es[pin_index].fp));
	if (g_dbgSetting[chid].dump_es[pin_index].fp == NULL || IS_ERR(g_dbgSetting[chid].dump_es[pin_index].fp)) {
		printk(DGB_COLOR_RED"debug:%d dump bitstream start FAILED...\n"DGB_COLOR_NONE, __LINE__);
		g_dbgSetting[chid].dump_es[pin_index].fp = NULL;
		return;
	}

	g_dbgSetting[chid].dump_es[pin_index].enable = true;
}
void DEBUG_DumpEsFlush(unsigned char chid, int pin_index)
{
	if(g_dbgSetting[chid].dump_es[pin_index].flush_en == 0 || g_dbgSetting[chid].dump_es[pin_index].filename[0] == '\0')
		return;

	if (g_dbgSetting[chid].dump_es[pin_index].enable) {
		g_dbgSetting[chid].dump_es[pin_index].enable = false;
		if (g_dbgSetting[chid].dump_es[pin_index].fp != NULL && !IS_ERR(g_dbgSetting[chid].dump_es[pin_index].fp))
			DEBUG_CloseFile(g_dbgSetting[chid].dump_es[pin_index].fp);
	}
	g_dbgSetting[chid].dump_es[pin_index].fp = NULL;
	g_dbgSetting[chid].dump_es[pin_index].pos = 0;
	DEBUG_StartDumpEs(chid, pin_index, g_dbgSetting[chid].dump_es[pin_index].filename);

}
void DEBUG_DumpEs(unsigned char chid, int pin_index, char *buffer, unsigned int size)
{
	int64_t clock;
	static int64_t notify_clock[DEMUX_TP_NUM][NUMBER_OF_PINS] = {{0}};
	if (g_dbgSetting[chid].dump_es[pin_index].enable == 0)
		return;

	DEBUG_WriteFile(g_dbgSetting[chid].dump_es[pin_index].fp, buffer, size, &g_dbgSetting[chid].dump_es[pin_index].pos);

	clock = CLOCK_GetPTS();
	if (clock-notify_clock[chid][pin_index] > 5*DEMUX_CLOCKS_PER_SEC) {

		printk(DGB_COLOR_CYAN"Dump TP%d(%s) Processing...\n"DGB_COLOR_NONE, chid, g_dbg_pin_name[pin_index]);
		notify_clock[chid][pin_index] = clock;
	}

}
////////////////////////////////////////////////////////////////////////////////////
// dump pts

static void DEBUG_StopDumpPts(unsigned char chid)
{
	if (g_dbgSetting[chid].dump_pts.enable) {
		g_dbgSetting[chid].dump_pts.enable = false;
		if (g_dbgSetting[chid].dump_pts.fp != NULL && !IS_ERR(g_dbgSetting[chid].dump_pts.fp))
			DEBUG_CloseFile(g_dbgSetting[chid].dump_pts.fp);
	}
	memset(&g_dbgSetting[chid].dump_pts, 0, sizeof(g_dbgSetting[chid].dump_pts));
}
static void DEBUG_StartDumpPts(unsigned char chid, const char* filename)
{
	DEBUG_CreateFile(filename, &g_dbgSetting[chid].dump_pts.fp);
	if (g_dbgSetting[chid].dump_pts.fp == NULL || IS_ERR(g_dbgSetting[chid].dump_pts.fp)) {
		printk(DGB_COLOR_RED"debug:%d print pts start FAILED...\n"DGB_COLOR_NONE, __LINE__);
		g_dbgSetting[chid].dump_pts.fp = NULL;
		return;
	}

	g_dbgSetting[chid].dump_pts.enable = true;
}
void DEBUG_DumpPts(unsigned char chid, int pin_index, int64_t pts, unsigned char* WP)
{
	char buffer[128];
	if (g_dbgSetting[chid].dump_pts.enable == 0)
		return;


	memset(buffer, 0, sizeof(buffer));
	snprintf(buffer, 128,
			"[Ch%d][%s] PTS = 0x%llx, WP = 0x%lx\n",
			chid, g_dbg_pin_name[pin_index], pts, (unsigned long)(uintptr_t)WP
		);

	DEBUG_WriteFile(g_dbgSetting[chid].dump_pts.fp, buffer, strlen(buffer), &g_dbgSetting[chid].dump_pts.pos);

}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// dump sync info

static void DEBUG_StopDumpSyncInfo(unsigned char chid)
{
	if (g_dbgSetting[chid].dump_sync_info.enable) {
		g_dbgSetting[chid].dump_sync_info.enable = false;
		if (g_dbgSetting[chid].dump_sync_info.fp != NULL && !IS_ERR(g_dbgSetting[chid].dump_sync_info.fp))
			DEBUG_CloseFile(g_dbgSetting[chid].dump_sync_info.fp);
	}
	memset(&g_dbgSetting[chid].dump_sync_info, 0, sizeof(g_dbgSetting[chid].dump_sync_info));
}
static void DEBUG_StartDumpSyncInfo(unsigned char chid, const char* filename)
{
	char buffer[128];
	size_t null_terminated_index;
	DEBUG_StopDumpSyncInfo(chid);
	DEBUG_CreateFile(filename, &g_dbgSetting[chid].dump_sync_info.fp);
	if (g_dbgSetting[chid].dump_sync_info.fp == NULL || IS_ERR(g_dbgSetting[chid].dump_sync_info.fp)) {
		printk(DGB_COLOR_RED"debug:%d dump avsync info start FAILED...\n"DGB_COLOR_NONE, __LINE__);
		g_dbgSetting[chid].dump_sync_info.fp = NULL;
		return;
	}

	null_terminated_index = strnlen(filename, RTKDEMUX_DBG_DUMP_AVSYNC_INFO_FILENAME_MAX_LENGTH-1);
	strncpy(g_dbgSetting[chid].dump_sync_info.filename, filename, RTKDEMUX_DBG_DUMP_AVSYNC_INFO_FILENAME_MAX_LENGTH-1);
	g_dbgSetting[chid].dump_sync_info.filename[null_terminated_index] = '\0';

	/* first line */
	snprintf(buffer, 128, "PCR,STC,RCD,AudioDemuxPTS,VideoDemuxPTS,AudioSystemPTS,VideoSystemPTS,PcrOffset,MasterPTS,SystemPTS\n");
	DEBUG_WriteFile(g_dbgSetting[chid].dump_sync_info.fp, buffer, strlen(buffer), &g_dbgSetting[chid].dump_sync_info.pos);

	g_dbgSetting[chid].dump_sync_info.enable = true;
}
void DEBUG_FlushSyncInfo(unsigned char chid)
{
	if (g_dbgSetting[chid].dump_sync_info.enable)
		DEBUG_StartDumpSyncInfo(chid, g_dbgSetting[chid].dump_sync_info.filename);
}
void DEBUG_DumpSyncInfo(unsigned char chid, const RTKDEMUX_AVSYNC_INFO_T *info)
{
	char buffer[256];
	int64_t clock;

	static int64_t notify_clock[DEMUX_TP_NUM] = {0};
	if (g_dbgSetting[chid].dump_sync_info.enable == 0)
		return;

	memset(buffer, 0, sizeof(buffer));
	snprintf(buffer, 256, "%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld\n",
		info->Pcr, info->Stc, info->Rcd, info->AudioDemuxPts, info->VideoDemuxPts,
		info->AudioSystemPts, info->VideoSystemPts, info->PcrOffset, info->MasterPTS, info->SystemPts);
	DEBUG_WriteFile(g_dbgSetting[chid].dump_sync_info.fp, buffer, strlen(buffer), &g_dbgSetting[chid].dump_sync_info.pos);

	clock = CLOCK_GetPTS();
	if (clock-notify_clock[chid] > 5*DEMUX_CLOCKS_PER_SEC) {

		printk(DGB_COLOR_CYAN"Dump TP%d AvsyncInfo...\n"DGB_COLOR_NONE, chid);
		notify_clock[chid] = clock;
	}

}
static inline bool DEBUG_ParseValue(char *cmd_pointer, long long *parsed_data)
{
	if (kstrtoll(cmd_pointer, 0, parsed_data) == 0) {
		return true;
	} else {
		printk("debug:%d parsing data failed. pCmd=%s\n",
			__LINE__, cmd_pointer);
		return false;
	}
}
void DEBUG_Execute(unsigned char chid, const int cmd_index, char **cmd_pointer)
{
	static const char *delim = ", ";
	long long parsed_data = 0;
	char *token = NULL;

	SDEC_CHANNEL_T *pCh = rdvb_dmx_getSdec(chid);
	switch (cmd_index) {
	case DBG_CMD_LOG_LEVEL:
		DEBUG_ParseValue(*cmd_pointer, &parsed_data);
		g_dbgSetting[chid].log_mask = (unsigned long)parsed_data;
		set_log_level(parsed_data);
		printk("log_mask[%d]=0x%8lx\n",chid, g_dbgSetting[chid].log_mask);
		break;
	case DBG_CMD_DONT_DELIVER_AUDIO:
		DEBUG_ParseValue(*cmd_pointer, &parsed_data);
		g_dbgSetting[chid].nodemux_audio = (unsigned char)parsed_data;
		printk("nodemux_audio[%d]=%d\n", chid, g_dbgSetting[chid].nodemux_audio);
		return;
	case DBG_CMD_DONT_DELIVER_VIDEO:
		DEBUG_ParseValue(*cmd_pointer, &parsed_data);
		g_dbgSetting[chid].nodemux_video = (unsigned char)parsed_data;
		printk("nodemux_video[%d]=%d\n", chid, g_dbgSetting[chid].nodemux_video);
		return;
	case DBG_CMD_PRINT_DEMUX_TARGET:
		return DEBUG_PrintDemuxTarget(pCh);
	case DBG_CMD_ES_MONITOR_OFF:
		DEBUG_ParseValue(*cmd_pointer, &parsed_data);
		g_dbgSetting[chid].esbufmonitor_off = (unsigned char)parsed_data;
		printk("esbufmonitor_off[%d]=%d\n", chid, g_dbgSetting[chid].esbufmonitor_off);
		return;
	case DBG_CMD_PRINT_IO:
		DEBUG_ParseValue(*cmd_pointer, &parsed_data);
		g_dbgSetting[chid].printio = (unsigned char)parsed_data;
		printk("printio[%d]=%d\n", chid, g_dbgSetting[chid].printio);
		if (g_dbgSetting[chid].printio == 0) {
			g_dbgSetting[chid].printio_read_count = 0;
			memset(g_dbgSetting[chid].printio_deliver_count, 0, sizeof(g_dbgSetting[chid].printio_deliver_count));
		}
		return;
	case DBG_CMD_DUMP_TS:
	{
		int dump_en = -1, flush_en = 0;
		if (*cmd_pointer == NULL) {
			printk(DGB_COLOR_RED"debug:%d dump ts command is incorrect.\n"DGB_COLOR_NONE, __LINE__);
			return;
		}

		token = strsep(cmd_pointer, delim);
		if (token == NULL || kstrtoint(token, 0, &dump_en) != 0) {
			printk(DGB_COLOR_RED"debug:%d failed to parse the dump ts enable flag.\n"DGB_COLOR_NONE, __LINE__);
			return;
		}


		if (dump_en == 1) {
			char *pFileName = strsep(cmd_pointer, delim);
			int filelength = 0;
			if (pFileName == NULL) {
				printk(DGB_COLOR_RED"debug:%d failed to parse filename.\n"DGB_COLOR_NONE, __LINE__);
				return;
			}

			DEBUG_StopDumpTs(chid);
			DEBUG_StartDumpTs(chid, pFileName);
			token = strsep(cmd_pointer, delim);
			if (token == NULL || kstrtoint(token, 0, &flush_en) != 0) g_dbgSetting[chid].dump_ts.flush_en = 0;
			else               g_dbgSetting[chid].dump_ts.flush_en = flush_en;

			filelength = strlen(pFileName);
			if ((filelength+1) <= sizeof(g_dbgSetting[chid].dump_ts.filename)) {
				strncpy(g_dbgSetting[chid].dump_ts.filename, pFileName, filelength);
				g_dbgSetting[chid].dump_ts.filename[filelength] = '\0';
			}
			printk(DGB_COLOR_GREEN"debug:%d start to dump ts: dump_ts_en=%d, tp=%d, path=%s, flush = %d\n"DGB_COLOR_NONE, __LINE__, dump_en, chid, pFileName, g_dbgSetting[chid].dump_ts.flush_en);
		} else {
			DEBUG_StopDumpTs(chid);
			printk(DGB_COLOR_GREEN"debug:%d stop to dump ts: dump_ts_en=%d, tp=%d\n"DGB_COLOR_NONE, __LINE__, dump_en, chid);
		}

		return;
	}
	case DBG_CMD_DUMP_ES:
	{
		int dump_en = -1, flush_en = 0;
		int pin_index;
		if (*cmd_pointer == NULL) {
			printk(DGB_COLOR_RED"debug:%d dump es command is incorrect.\n"DGB_COLOR_NONE, __LINE__);
			return;
		}

		token = strsep(cmd_pointer, delim);
		if (token == NULL || kstrtoint(token, 0, &dump_en) != 0) {
			printk(DGB_COLOR_RED"debug:%d failed to parse the dump es enable flag.\n"DGB_COLOR_NONE, __LINE__);
			return;
		}

		token = strsep(cmd_pointer, delim);
		if (token == NULL) {
			printk(DGB_COLOR_RED"debug:%d failed to parse pin index.\n"DGB_COLOR_NONE, __LINE__);
			return;
		} else {
			int i = 0;
			for (i = 0 ; i < NUMBER_OF_PINS ; i++) {
				if (g_dbg_pin_name[i] != NULL && strncasecmp(g_dbg_pin_name[i], token, strlen(g_dbg_pin_name[i])) == 0) {
					pin_index = i;
					return;
				}
			}

			if (i == NUMBER_OF_PINS) {
				printk(DGB_COLOR_RED"debug:%d failed to parse pin index.\n"DGB_COLOR_NONE, __LINE__);
				return;
			}
		}

		if (dump_en == 1) {
			char *pFileName = strsep(cmd_pointer, delim);
			int filelength = 0;
			if (pFileName == NULL) {
				printk(DGB_COLOR_RED"rtkdemux_debug:%d failed to parse filename.\n"DGB_COLOR_NONE, __LINE__);
				return;
			}

			DEBUG_StopDumpEs(chid, pin_index);
			DEBUG_StartDumpEs(chid, pin_index, pFileName);
			token = strsep(cmd_pointer, delim);
			if (token == NULL || kstrtoint(token, 0, &flush_en) != 0) g_dbgSetting[chid].dump_es[pin_index].flush_en = 0;
			else               g_dbgSetting[chid].dump_es[pin_index].flush_en = flush_en;

			filelength = strlen(pFileName);
			if((filelength+1) <= sizeof(g_dbgSetting[chid].dump_es[pin_index].filename))
			{
				strncpy(g_dbgSetting[chid].dump_es[pin_index].filename, pFileName, filelength);
				g_dbgSetting[chid].dump_es[pin_index].filename[filelength] = '\0';
			}
			printk(DGB_COLOR_GREEN"debug:%d start to dump bitstream: dump_ts_en=%d, tp=%d, pin=%s, path=%s, flush=%d\n"DGB_COLOR_NONE,
				__LINE__, dump_en, chid, g_dbg_pin_name[pin_index], pFileName, g_dbgSetting[chid].dump_es[pin_index].flush_en);
		} else if (dump_en == 0) {
			DEBUG_StopDumpEs(chid, pin_index);
			printk(DGB_COLOR_GREEN"debug:%d stop to dump bitstream: dump_ts_en=%d, tp=%d, pin=%s\n"DGB_COLOR_NONE,
				__LINE__, dump_en, chid, g_dbg_pin_name[pin_index]);
		}

		return;
	}
	case DBG_CMD_PCR_TRACK:
	{
		int pcrtrack_en;
		if (*cmd_pointer == NULL) {
			printk(DGB_COLOR_RED"rtkdemux_debug:%d pcr track command is incorrect.\n"DGB_COLOR_NONE, __LINE__);
			return;
		}

		token = strsep(cmd_pointer, delim);
		if (token == NULL || kstrtoint(token, 0, &pcrtrack_en) != 0) {
			printk(DGB_COLOR_RED"debug:%d failed to parse the pcr track enable flag.\n"DGB_COLOR_NONE, __LINE__);
			return;
		}
		if (pcrtrack_en) {
			pCh->bPcrTrackEnable = 1;
			SDEC_EnablePCRTracking(pCh);
		} else {
			pCh->bPcrTrackEnable = 0;
			SDEC_DisablePCRTracking(pCh);
		}
		return;
	}
	case DBG_CMD_DUMP_PTS:
	{
		int dump_en;
		if (*cmd_pointer == NULL) {
			printk(DGB_COLOR_RED"rtkdemux_debug:%d dump pts command is incorrect.\n"DGB_COLOR_NONE, __LINE__);
			return;
		}

		token = strsep(cmd_pointer, delim);
		if (token == NULL || kstrtoint(token, 0, &dump_en) != 0) {
			printk(DGB_COLOR_RED"rtkdemux_debug:%d failed to parse the dump pts enable flag.\n"DGB_COLOR_NONE, __LINE__);
			return;
		}

		if (dump_en == 1) {
			token = strsep(cmd_pointer, delim);
			if (token == NULL) {
				printk(DGB_COLOR_RED"rtkdemux_debug:%d failed to parse filename.\n"DGB_COLOR_NONE, __LINE__);
				return;
			}

			DEBUG_StopDumpPts(chid);
			DEBUG_StartDumpPts(chid, token);
			printk(DGB_COLOR_GREEN"debug:%d start to dump pts: dump_ts_en=%d, tp=%d, path=%s\n"DGB_COLOR_NONE, __LINE__, dump_en, chid, token);
		} else {
			DEBUG_StopDumpPts(chid);
			printk(DGB_COLOR_GREEN"debug:%d stop to dump pts: dump_ts_en=%d, tp=%d\n"DGB_COLOR_NONE, __LINE__, dump_en, chid);
		}
		return;
	}
	case DBG_CMD_DUMP_AVSYNC_INFO:
	{
		int dump_en = 0;
		if (*cmd_pointer == NULL) {
			printk(DGB_COLOR_RED"debug:%d dump avsync info command is incorrect.\n"DGB_COLOR_NONE, __LINE__);
			return;
		}

		token = strsep(cmd_pointer, delim);
		if (token == NULL || kstrtoint(token, 0, &dump_en) != 0) {
			printk(DGB_COLOR_RED"debug:%d failed to parse the dump avsync info enable flag.\n"DGB_COLOR_NONE, __LINE__);
			return;
		}


		if (dump_en == 1) {
			token = strsep(cmd_pointer, delim);
			if (token == NULL) {
				printk(DGB_COLOR_RED"debug:%d failed to parse filename.\n"DGB_COLOR_NONE, __LINE__);
				return;
			}

			DEBUG_StartDumpSyncInfo(chid, token);
			printk(DGB_COLOR_GREEN"rtkdemux_debug:%d start to dump avsync info: dump_ts_en=%d, tp=%d, path=%s\n"DGB_COLOR_NONE, __LINE__, dump_en, chid, token);
		} else {
			DEBUG_StopDumpSyncInfo(chid);
			printk(DGB_COLOR_GREEN"rtkdemux_debug:%d stop to dump avsync info: dump_ts_en=%d, tp=%d\n"DGB_COLOR_NONE, __LINE__, dump_en, chid);
		}

		return;
	}
	//FIXME
	//case DBG_CMD_PRINT_FILTER_LIST:
	//	return rdvbdmx_filter_dump(&filter_list);
	
	default:
		return;
	}
}