#ifndef __PVR_H__
#define __PVR_H__

#include <linux/mutex.h>
#include <demux.h>

/*
 * APVR ES with field picture encoded
 * 0 : Encoded ES is Frame Picture
 * 1 : Encoded ES is Field Picture
*/
#define APVR_PROP_ES_TYPE	1

/*
 * APVR Bandwidth capability limitation
 * 0 : No BW limitation APVR and 4K play
 * 1 : BW limitation APVR and 4K play
 */
#define APVR_PROP_BW_CAPA	1

/******************************************************************************
 * Recording
 *****************************************************************************/
//pvr thread state
typedef enum {
	PVR_STATE_STOP = 0,
	PVR_STATE_PAUSE,
	PVR_STATE_RUNNING,
} PVR_STATE_T;

typedef enum {
	PVR_AUTH_VERIFY_ERROR = 0,
	PVR_AUTH_VERIFY_SUCCESS
} PVR_AUTH_VERIFY_T;

typedef struct {

	int tunerHandle;
	struct mutex mutex ;
	struct task_struct *pvr_thread_task;
	PVR_STATE_T pvrThreadState;
	PVR_AUTH_VERIFY_T pvrAuthVerifyResult;

	//rp/wp
	unsigned int pPvrReadPtr;
	unsigned int pPvrWritePtr;

	unsigned int rec_base;
	unsigned int rec_limit;
	long         rec_vir_offset;

	//callback
	dvr_rec_cb rec_callback;
	void * client;

} PVR_REC_SETTING;

void pvr_rec_init(PVR_REC_SETTING* pvr, unsigned int index);
int pvr_rec_open(PVR_REC_SETTING* pvr, dvr_rec_cb dvr_callback, void *source);
int pvr_rec_close(PVR_REC_SETTING *pvr);

void pvr_rec_set_bufinfo(PVR_REC_SETTING* pvr, unsigned int base, unsigned int limit,
							long offset);

/******************************************************************************
 * Playback
 *****************************************************************************/
 typedef struct {
	int   tunerHandle;
	PVR_STATE_T pbState;
	UINT8 *pMtpBufUpper;
	UINT8 *pMtpBufLower;
	long  mtpPhyOffset;

	//For ES buffer monitor scheme
	UINT8 byPassPids[1024];
	UINT8 filterCnt;
	void *filterAry[16]; //store bypass dmxfilter pointer

	//For status debug
	UINT32 totalWriteSize;//Mbytes

} PVR_PB_SETTING;

void pvr_pb_init(PVR_PB_SETTING* pvr, unsigned int index);
void pvr_pb_set_mtp(PVR_PB_SETTING* pvr, UINT8* base, UINT8* limit, long offset);
int pvr_pb_write_mtp(PVR_PB_SETTING *pvr, const char __user *buf, size_t count);
int pvr_pb_close(PVR_PB_SETTING* pvr);

#endif //__PVR_H__