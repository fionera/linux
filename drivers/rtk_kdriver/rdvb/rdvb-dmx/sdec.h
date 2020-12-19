#ifndef __SDEC_H__
#define __SDEC_H__

#include <linux/types.h>
#include <dvb_demux.h>
#include <tp/tp_def.h>
#include <tp/tp_drv_api.h>
#include "InbandAPI.h"
#include "AudioInbandAPI.h"
#include "refclk.h"
#include "pcrsync.h"
#include "fullcheck.h"
#include "pcrtracking.h"
#include "dmx_common.h"

#include <dmx-ext.h>
#include <rdvb-buf/dmx_buf_misc.h>
#include <rdvb-buf/rdvb_dmx_buf.h>
#include "dmx_parse_context.h"
#include "pvr.h"

#define DONTFIXBUG13648

#define PES_HEADER_BUF_SIZE   14
#define NUM_OF_NAV_BUFFERS    64
#define MAX_PID_FILTER_CB_NUM 2
#define MTP_AVSYNC_MODE             NAV_AVSYNC_AUDIO_MASTER_AUTO //NAV_AVSYNC_AUDIO_MASTER_AF


typedef enum {
	SDEC_STATE_STOP = 0,
	SDEC_STATE_PAUSE,
	SDEC_STATE_RUNNING,

} SDEC_STATE_T;



enum {
	DEMUX_TARGET_VIDEO = 0,
	DEMUX_TARGET_AUDIO = 1,    /*active audio*/
	DEMUX_TARGET_VIDEO_2ND = 2,
	DEMUX_TARGET_AUDIO_2ND = 3,
	DEMUX_NUM_OF_TARGETS,
	DEMUX_TARGET_AUDIO_NO_FORMAT,    /* active audio but not deliver format yet */
	DEMUX_TARGET_AUDIO2_NO_FORMAT,
	DEMUX_TARGET_DROP     = 0x1F,

};
enum {
	//0
	EVENT_UPDATE_TS_LIST             = 1	<< 0,
	EVENT_UPDATE_PES_LIST            = 1	<< 1,
	EVENT_UPDATE_SCRAMBLE_LIST       = 1	<< 2,
	EVENT_VIDEO0_NEW_DECODE_MODE     = 1	<< 3,
	EVENT_VIDEO1_NEW_DECODE_MODE     = 1	<< 4,
	//5
	EVENT_CHANGE_CLK_SRC             = 1	<< 5,
	EVENT_SET_VIDEO_START			 = 1	<< 6,
	EVENT_SET_VIDEO2_START			 = 1	<< 7,
	EVENT_SET_AUDIO_START			 = 1	<< 8,
	EVENT_SET_AUDIO2_START			 = 1	<< 9,
	//10
	EVENT_SET_AUDIO_FORMAT			 = 1	<< 10,
	EVENT_SET_AUDIO2_FORMAT		 	 = 1	<< 11,
	EVENT_FLUSH_AD_INFO              = 1	<< 12,
	EVENT_ENTER_ECP_MODE             = 1    << 13,
	EVENT_EXIT_ECP_MODE             = 1     << 14,
	//15
};
enum {

	//0
	STATUS_AUDIO_PAUSED              = 1	<< 0,
	STATUS_UPDATE_DEMUX_TARGET       = 1	<< 1,
	//STATUS_AUDIO_FORMAT_SENT         = 1	<< 2,
	//STATUS_AUDIO2_FORMAT_SENT        = 1 	<< 3,
	STATUS_PCRTAK_START              = 1	<< 4,
	//5
	STATUS_PCRTAK_STOP               = 1	<< 5,
	STATUS_APVR_PLAYBACK_START       = 1	<< 6,
	STATUS_SET_AUDIO_PIN_INFO        = 1	<< 7,
	//STATUS_SET_VIDEO_PIN_INFO        = 1	<< 8,
	STATUS_FLUSH_TP                  = 1	<< 9,
	//10
	STATUS_FLUSH_MTP                 = 1	<< 10,
	STATUS_FLUSH_VIDEO 				 = 1	<< 11,
	STATUS_FLUSH_VIDEO2				 = 1	<< 12,
	STATUS_FLUSH_AUDIO 				 = 1	<< 13,
	STATUS_FLUSH_AUDIO2				 = 1	<< 14,

};

enum {
	PID_SCRAMBLE_CHECK_STOP,
	PID_SCRAMBLE_CHECK_START,
	PID_IS_SCRAMBLE,
	PID_IS_NOT_SCRAMBLE,
};

typedef enum {

	/* to indicate there is no private info  0 */
	PRIVATEINFO_NONE,

	/* sending PTS information (int64_t)  1*/
	PRIVATEINFO_PTS,

	PRIVATEINFO_VIDEO_NEW_SEG,
	PRIVATEINFO_AUDIO_NEW_SEG,


	/* sending audio format information (AUDIOFORMAT) 12*/
	PRIVATEINFO_AUDIO_FORMAT,

	/* sending audio context information for AOUT feedback (int) 13*/
	PRIVATEINFO_AUDIO_CONTEXT,

	/* audio decoding command (AUDIODECODECOMMAND) 14*/
	PRIVATEINFO_AUDIO_DECODE_COMMAND,

	/* to flush audio alone (no argument) 15*/
	PRIVATEINFO_AUDIO_FLUSH,

	/* add by stream : to mark the position that data is discontinuity 16*/
	PRIVATEINFO_AUDIO_DATA_DISCONTINUITY,

	/* sending video end-of-sequence (no argument) 17*/
	PRIVATEINFO_VIDEO_END_OF_SEQUENCE,


	/* sending video context information for VOUT feedback (int) 22*/
	PRIVATEINFO_VIDEO_CONTEXT,

	/* video decoding command (VIDEODECODECOMMAND) 23*/
	PRIVATEINFO_VIDEO_DECODE_COMMAND,



	/*deliver both video and audio context 53 */
	PRIVATEINFO_AV_CONTEXT,

	/* ask video decoder not to hack rp. 67*/
	PRIVATEINFO_VIDEO_DTV_SOURCE,


	/* sending audio ad information (AUDIO_AD_INFO) 88*/
	PRIVATEINFO_AUDIO_AD_INFO,

	PRIVATEINFO_VIDEO_NEW_DECODE_MODE,

	/* sending video codeecfto notify DTV source (ANDROIDTV-227 issue) */
	PRIVATEINFO_VIDEO_INBAND_CMD_TYPE_SOURCE_DTV,
} PRIVATEINFO_IDENTIFIER;

typedef enum {

	/* to indicate there is nothing in Nav Buffer */
	NAVBUF_NONE,

	/* to send data (bit-stream or private-info), use fields under "data" */
	NAVBUF_DATA,

	/* to ask nav thread to wait, use fields under "wait" */
	NAVBUF_WAIT,

	/* to ask nav thread to wait (fullness pending is kept), use fields under "wait" */
	NAVBUF_SHORT_WAIT,

	/* to set phyAddrOffset */
	NAVBUF_DATA_EXT,


} NAV_BUF_ID;
typedef struct _tagNAVBUF {

	/* identify nav buffer type */
	NAV_BUF_ID type;

	union {

		/* if type == NAVBUF_DATA */
		struct {
			unsigned int pinIndex; /* index (0~) of the pin to deliver data */
			struct ts_parse_context *tpc;
			/* NOTE: pinIndex is decided by demux */

			unsigned char *payloadData;      /* bit-stream buffer (NULL means no bit-stream) */
			unsigned int   payloadSize;      /* size of payloadData in bytes (0 means no data) */
			int64_t        pts;              /* presentation time (-1 means N/A) */
			unsigned int   infoId;           /* private-info id (can be PRIVATEINFO_NONE) */
			unsigned char *infoData;         /* private-info data buffer */
			unsigned int   infoSize;         /* private-info data size in bytes */
			unsigned long startAddress; /* start address where the input block was read */
			/* NOTE: for parser application only, use 0xFFFFFFFF otherwise */

			long           phyAddrOffset; /* physical address - (non) cached address */
		} data;

		/* if type == NAVBUF_WAIT */
		struct {
			long waitTime; /* in milliseconds */
		} wait;

	};
} NAVBUF;

typedef struct _tagNAVDEMUXIN {
	unsigned char *pBegin;           /* mark begin of input data for demux plugin */
	unsigned char *pEnd;             /* mark end of input data for demux plugin */
	unsigned char *pCurr;            /* point to the first unused input data byte */
	int64_t        pts;              /* presentation time (-1 means N/A) */
	unsigned long  startAddress;     /* start address where the input block was read */

	long            phyAddrOffset;
} NAVDEMUXIN;

typedef struct _tagNAVDEMUXOUT {

	NAVBUF       *pNavBuffers;  /* point to the start of NAVBUF array */
	unsigned int numBuffers;   /* number of NAVBUF items in array */

} NAVDEMUXOUT;

/* audio decoding command for one segment (PRIVATEINFO_AUDIO_DECODE_COMMAND) */
typedef struct _tagAUDIODECODECOMMAND {
	unsigned int mode;   /* refer to struct DEC_MODE in AudioInbandAPI.h */
	int64_t relativePTS; /* decode start relative PTS */
	int64_t duration;    /* PTS duration of decoded segment */
} AUDIODECODECOMMAND;

/* video decoding command for one segment (PRIVATEINFO_VIDEO_DECODE_COMMAND) */
typedef struct _tagVIDEODECODECOMMAND {
	unsigned int mode;   /* refer to struct DECODE_MODE in InbandAPI.h */
	int64_t relativePTS; /* decode start relative PTS */
	int64_t duration;    /* PTS duration of decoded segment */
	s32 skip_GOP;       /* skipped GOP before decode start */
} VIDEODECODECOMMAND;



typedef struct {
	dma_addr_t     phyAddr;
	unsigned char *virAddr;
	unsigned char *cachedaddr;     /* cached addresss for release*/
	unsigned char *nonCachedaddr;
	unsigned int   size;
	//unsigned int   allocSize; //should not used.
	unsigned char  isCache;
	unsigned char  fromPoll;
	long           phyAddrOffset;
} SDEC_BUF_T;


typedef struct {
	unsigned char  occupied;
	unsigned int   unitSize; /* PES_packet_length */
	unsigned int   size;     /* accumulated size */


	int pid;
	int contiCounter;

	int pes_buf_port;
	void *filter;
	ts_callback dfc;
} PES_FILTER_T;


struct  ref_clk {
	REFCLOCK * prefclk;
	struct BUF_INFO refClock;
};
struct video_status {
	bool bfreerun;
	int freerun_count;
};
typedef struct
{
	struct dvb_demux dvb_demux;

	int idx;
	struct mutex mutex;
	bool is_ecp;
	bool is_playback;

	unsigned int status;
	unsigned int events;
	NAVDEMUXIN demuxIn;
	NAVDEMUXIN demuxInReserved;
	NAVDEMUXOUT demuxOut;
	NAVDEMUXOUT demuxOutReserved;

	NAVBUF navBuffers[NUM_OF_NAV_BUFFERS];

	int                      tpc_num;
	struct list_head         tpc_list_head;
	spinlock_t               tpc_lock;
	struct ts_parse_context *tpcs;
	unsigned char    scrambleCheck[1 << 13];
	SDEC_STATE_T threadState;
	struct task_struct *thread_task;

	/* av sync */
	PCRSYNC_T pcrProcesser;
	AV_SYNC_MODE avSyncMode;
	int v_fr_thrhd;
	int avSyncStartupFullness;
	int avSyncStartupPTS;
	bool bPendingOnPTSCheck;
	bool bPendingOnFullness;
	int PCRPID;
	unsigned char    bPcrTrackEnable;
	PCRTRK_MANAGER_T *pClockRecovery;
	

	/* AF mode */
	int64_t timeToUpdteAF;
	bool bCheckAFState;

	/* TP read pointer */
	unsigned char *pTPRelase;
	unsigned int   TPReleaseSize;

	unsigned int  tsPacketSize;
	unsigned long dataThreshold;
	TPK_TP_SOURCE_T tp_src;

	FULL_CHECK_T fullCheck;

	/* status flags */
	bool bIsSDT;    /* SDT channel */

	struct ref_clk refClock;
	SDEC_BUF_T tpBuffer;

	/* private info */
	DECODE_MODE videoDecodeMode[MAX_VTP_NUM];
	AUDIO_AD_INFO adInfo;
	AUDIO_DEC_TYPE      audioFormat;
	//DEMUX_AUDIO_DEC_T   audioFormat;
	s32                 audioPrivInfo[8];
	AUDIO_DEC_TYPE      audioFormat2;
	//DEMUX_AUDIO_DEC_T   audioFormat2;
	s32                 audioPrivInfo2[8];

	/* Check AD channel target*/
	int AdChannelTarget;

	/* pvr */
	SDEC_BUF_T       mtpBuffer;
	PVR_REC_SETTING  pvrRecSetting;
	PVR_PB_SETTING   pvrPbSetting;
	bool             isAudioPaused;
	int              pvrPbSpeed;

	int lumpsumLoopCount;
	int lumpsumErrCount;
	const char *lumpsumMsg[256];

	struct video_status video_status;

} SDEC_CHANNEL_T;

int SDEC_InitChannel(SDEC_CHANNEL_T *pCh, char idx);
int SDEC_ReleaseChannel(SDEC_CHANNEL_T *pCh);
void SDEC_ResetAVSync(SDEC_CHANNEL_T *pCh, AV_SYNC_MODE avSyncMode);
int SDEC_EnablePCRTracking(SDEC_CHANNEL_T *pCh);
int SDEC_DisablePCRTracking(SDEC_CHANNEL_T *pCh);

int SDEC_ThreadProc(SDEC_CHANNEL_T *pCh);
//void SDEC_StartThread(SDEC_CHANNEL_T *pCh);
int SDEC_PB_AudioPause(SDEC_CHANNEL_T * pCh, bool isOn);
int SDEC_PB_SetSpeed(SDEC_CHANNEL_T *pCh, int speed);
int SDEC_PB_Reset(SDEC_CHANNEL_T *pCh);

int PARSER_Parse(SDEC_CHANNEL_T *pCh, NAVDEMUXIN *pDemuxIn, NAVDEMUXOUT *pDemuxOut);

int CMA_AllocBuffer(SDEC_BUF_T *pBuf, int size, int isCache, int useZone);
int CMA_FreeBuffer(SDEC_BUF_T *pBuf);
int CMA_AllocateTPBuffer(SDEC_CHANNEL_T *pCh);
int CMA_FreeTPBuffer(SDEC_CHANNEL_T *pCh);
int CMA_ConfigTPBuffer(SDEC_CHANNEL_T *pCh);
int CMA_MTPBuffer_Create(SDEC_CHANNEL_T *pCh);

int IOCTL_SetPID(SDEC_CHANNEL_T *pCh, int pid, DEMUX_PID_TYPE_T type,  DEMUX_PES_DEST_T dest, struct dvb_demux_feed *feed);
int IOCTL_RemovePID(SDEC_CHANNEL_T *pCh, int pid, DEMUX_PID_TYPE_T type,  DEMUX_PES_DEST_T dest);
int IOCTL_FlushVideo(SDEC_CHANNEL_T *pCh, int port);
int IOCTL_SetAudioFormat(SDEC_CHANNEL_T *pCh, int port, int format, u32 privateInfo[], int infoCnt);
int IOCTL_StartSCrambleCheck(SDEC_CHANNEL_T *pCh, int pid);
int IOCTL_StopSCrambleCheck(SDEC_CHANNEL_T *pCh, int pid);
int IOCTL_GetSCrambleStatus(SDEC_CHANNEL_T *pCh, int pid);
int IOCTL_SetInputConfig(SDEC_CHANNEL_T *pCh, struct dmx_ext_source *pInfo);
int IOCTL_AutomateClockRecovery(SDEC_CHANNEL_T *pCh, bool bOnOff);

void sdec_flush_by_tpc(struct ts_parse_context *tpc);//used when A/V filter was stopped and released
void sdec_reset_pts_by_tpc(struct ts_parse_context *tpc);//used when A/V filter was stopped


#endif //__SDEC_H__
