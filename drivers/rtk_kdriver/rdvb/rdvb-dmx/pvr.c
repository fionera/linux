#include <linux/freezer.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include "tp/tp_drv_api.h"
#include "pvr.h"
#include "rdvb_dmx_filtermgr.h"
#include "rdvb_dmx_filtermgr.h"
#include "debug.h"
#include "dvb_ringbuffer.h"

/******************************************************************************
 * Recording
 *****************************************************************************/
extern struct rdvb_dmxdev* get_rdvb_dmxdev(void);
extern struct rdvb_dmx* get_rdvb_dmx(unsigned char idx);
int PvrThreadProcEntry(void *pParam)
{
	PVR_REC_SETTING * const pvr = (PVR_REC_SETTING *)pParam;
	// 1. get tp buffer base and limit

	if (pvr->rec_base) {
		pvr->pPvrReadPtr = pvr->pPvrWritePtr = pvr->rec_base;
	}
	// 2. enable RHAL_DumpTPRingBuffer
	if (RHAL_DumpTPRingBuffer(pvr->tunerHandle, 1,(unsigned char*)&pvr->pPvrReadPtr,
		(unsigned char*)&pvr->pPvrWritePtr) != TPK_SUCCESS) {
		pvr->pvr_thread_task = 0;
		dmx_err(pvr->tunerHandle, "[%s] RHAL_DumpTPRingBuffer fail !!\n",__func__);
		return -1;
	}
	if (!pvr->rec_callback)  {
		dmx_err(pvr->tunerHandle, "[%s] Error!! rec_callback is null !\n", __func__);
		return -1;
	}
	// 3. thread running
	pvr->pvrThreadState = PVR_STATE_RUNNING;

	while (!kthread_should_stop()) {

		int readableSize = 0;
		unsigned int tmpRp = 0;
		long RP =0;

		unsigned int bufLimit = pvr->rec_limit;

		set_freezable();
		mutex_lock(&pvr->mutex);
		if (pvr->rec_base == 0)
			goto NEXT;

		if (pvr->pPvrWritePtr >= pvr->pPvrReadPtr) 
			readableSize = pvr->pPvrWritePtr - pvr->pPvrReadPtr;
		else 
			readableSize = bufLimit - pvr->pPvrReadPtr;

		dmx_mask_print(DEMUX_BUFFER_DEBUG,	pvr->tunerHandle,
			"[PVR] readableSize(%d) pPvrReadPtr(0x%08x)/pPvrWritePtr(0x%08x)\n",
			readableSize, pvr->pPvrReadPtr, pvr->pPvrWritePtr);

		RP = pvr->pPvrReadPtr - (pvr->rec_vir_offset);
		if (pvr->pvrAuthVerifyResult == PVR_AUTH_VERIFY_SUCCESS) {
			if (pvr->rec_callback((UINT8 *)RP, readableSize, NULL, 0, pvr->client) < 0) {
				dmx_err(pvr->tunerHandle,
					"[PVR] Warning, rec_callback failed! rp/wp:(0x%08x, 0x%08x), size:%d\n",
					pvr->pPvrReadPtr, pvr->pPvrWritePtr, readableSize);
				goto NEXT;
			}
		}

		// update read pointer
		tmpRp = pvr->pPvrReadPtr + readableSize;
		if(tmpRp >= bufLimit)
			tmpRp = pvr->rec_base;
		pvr->pPvrReadPtr = tmpRp;

	NEXT:
		mutex_unlock(&pvr->mutex);
		msleep(100);
	}//end while

	//stop PVR and disable RHAL_DumpTPRingBuffer
	RHAL_DumpTPRingBuffer(pvr->tunerHandle, 0, (unsigned char*)&pvr->pPvrReadPtr,
		(unsigned char*)&pvr->pPvrWritePtr);
	pvr->pvrThreadState = PVR_STATE_STOP;
	pvr->pvr_thread_task = 0;

	return 0;
}

void PvrRecord_startPvrThread(PVR_REC_SETTING* pvr)
{
	char pstr[10] = {0};
	snprintf(pstr,9,"RPVR_%d", pvr->tunerHandle);
	pvr->pvr_thread_task = kthread_create(PvrThreadProcEntry, (void *)pvr, "%s",pstr);
	if (!(pvr->pvr_thread_task))
		return;

	wake_up_process(pvr->pvr_thread_task);
}

void PvrRecord_stopPvrThread(PVR_REC_SETTING* pvr)
{
	if (pvr->pvrThreadState == PVR_STATE_STOP)
		return;

	if (pvr->pvr_thread_task != NULL) {
		kthread_stop(pvr->pvr_thread_task);
		dmx_warn(pvr->tunerHandle, DGB_COLOR_RED
			"[PVR] stopPvrThread::pvr thread stoped !!\n"DGB_COLOR_NONE);
	}
	pvr->pvr_thread_task = 0;
}

void pvr_rec_init(PVR_REC_SETTING* pvr, unsigned int index)
{

	pvr->tunerHandle = index;
	pvr->pvrThreadState = PVR_STATE_STOP;
	pvr->pPvrWritePtr = 0;
	pvr->pPvrReadPtr = 0;
	pvr->pvr_thread_task = NULL;
	pvr->rec_base = 0;
	pvr->rec_limit = 0;
	pvr->pvrAuthVerifyResult = PVR_AUTH_VERIFY_ERROR;
	mutex_init(&pvr->mutex);

}

void pvr_rec_set_bufinfo(PVR_REC_SETTING* pvr, unsigned int base,
				unsigned int limit, long offset)
{
	mutex_lock(&pvr->mutex);
	pvr->pPvrReadPtr = pvr->pPvrWritePtr = base;
	pvr->rec_base = base;
	pvr->rec_limit = limit;
	pvr->rec_vir_offset = offset;
	mutex_unlock(&pvr->mutex);
}

static void pvr_rec_set_callback(PVR_REC_SETTING* pvr, dvr_rec_cb dvr_callback,
								void *source)
{
	mutex_lock(&pvr->mutex);
	pvr->rec_callback = dvr_callback;
	pvr->client = source;
	mutex_unlock(&pvr->mutex);
}

int pvr_rec_open(PVR_REC_SETTING* pvr, dvr_rec_cb rec_callback, void *source)
{
	pvr_rec_set_callback(pvr, rec_callback, source);
	PvrRecord_startPvrThread(pvr);
	return 0;
}

int pvr_rec_close(PVR_REC_SETTING *pvr)
{
	pvr->pvrAuthVerifyResult = PVR_AUTH_VERIFY_ERROR;
	PvrRecord_stopPvrThread(pvr);
	pvr_rec_set_callback(pvr, NULL, NULL);
	return 0;
}

/******************************************************************************
 * Playback
 *****************************************************************************/
#define PVR_PID(p__)                 (((((int)(p__)[1]) & 0x1F) << 8) | (p__)[2])
#define PVR_HAS_PID(table, bit)    ((table[(bit >> 3)]) & (1 << (bit & 0x7)))
#define PVR_SET_PID(table, bit)    ((table[(bit >> 3)]) |= (1 << (bit & 0x7)))

static int addByPassFilter(PVR_PB_SETTING *pvr, UINT8* pUploadBuffer, int uploadSize)
{
	int packetSize = 192;
	int size = uploadSize;
	UINT8* pPacket = pUploadBuffer;
	struct rdvb_dmx     * rdvb_dmx= NULL;

	rdvb_dmx = get_rdvb_dmx(pvr->tunerHandle);

	while(size > 0)
	{
		struct dmxfilter *df = NULL;
		int pid = PVR_PID(pPacket+4);
		if ((pid < 0x1FFF && pid > 0 ) && (PVR_HAS_PID(pvr->byPassPids, pid) == 0)) {
			if (pvr->filterCnt >= 16) {
				dmx_err(pvr->tunerHandle, "Add pid(%d) failed! filter full!\n",pid);
				return -1;
			}
			df = rdvbdmx_filter_bypass_add(rdvb_dmx, pid);
			if (df) {
				pvr->filterAry[pvr->filterCnt++] = df;
				dmx_crit(pvr->tunerHandle, "pvr bypass pid:%d added, cnt=%d\n",
					pid, pvr->filterCnt);
			} else {
				dmx_err(pvr->tunerHandle, "rdvbdmx_filter_add pid:%d failed!!\n", pid);
			}

			PVR_SET_PID(pvr->byPassPids, pid);
		}
		size -= packetSize;
		pPacket += packetSize;
	}

	return 0;
}

static int clearByPassFilter(PVR_PB_SETTING *pvr)
{
	int i=0;
	struct dmxfilter *df = NULL;

	for (i = 0; i < pvr->filterCnt; i++) {
		df = (struct dmxfilter *)pvr->filterAry[i];
		rdvbdmx_filter_bypass_del(df);
	}
	pvr->filterCnt = 0;
	memset(pvr->filterAry, 0, sizeof(void *)*16);
	memset(pvr->byPassPids, 0, sizeof(pvr->byPassPids));

	dmx_crit(pvr->tunerHandle, "[%s, %d] clearByPassFilter success!\n",
		__func__, __LINE__);
	return 0;
}

void pvr_pb_init(PVR_PB_SETTING* pvr, unsigned int index)
{
	pvr->tunerHandle = index;
	pvr->pbState = PVR_STATE_STOP;
	pvr->pMtpBufUpper = NULL;
	pvr->pMtpBufLower = NULL;
	pvr->mtpPhyOffset = 0;
	memset(pvr->byPassPids, 0, sizeof(pvr->byPassPids));
	pvr->filterCnt = 0;
	memset(pvr->filterAry, 0, sizeof(void *)*16);
	pvr->totalWriteSize = 0;
}

void pvr_pb_set_mtp(PVR_PB_SETTING* pvr, UINT8* base, UINT8* limit, long offset)
{
	pvr->pMtpBufUpper = limit;
	pvr->pMtpBufLower = base;
	pvr->mtpPhyOffset = offset;
}

int pvr_pb_write_mtp(PVR_PB_SETTING *pvr, const char __user *buf, size_t count)
{
	UINT8 *pWritePhyAddr = 0;
	UINT32 contiguousWritableSize = 0;
	UINT32 writableSize = 0;
	UINT32 requireWriteSize = count;
	UINT8 mtp_id;

	if (!pvr || !buf || count <=0) {
		dmx_err(CHANNEL_UNKNOWN, "[%s, %d]:Invalid parameters,pvr=%p,buf=%p,count=%d!!\n",
		__func__, __LINE__, pvr, buf, count);
		return -EFAULT;
	}
	if (!pvr->pMtpBufUpper || !pvr->pMtpBufLower) {
		dmx_err(pvr->tunerHandle, "[%s, %d]:mtp buf not set!!\n", __func__, __LINE__);
		return -EFAULT;
	}
	if (pvr->pbState != PVR_STATE_RUNNING)
		pvr->pbState = PVR_STATE_RUNNING;
	mtp_id = pvr->tunerHandle;
	if (RHAL_GetMTPBufferStatus(mtp_id, &pWritePhyAddr, &contiguousWritableSize,
		&writableSize) != TPK_SUCCESS)
		return -EFAULT;

	if (writableSize < count) {
		//overflow
		dmx_err(pvr->tunerHandle,"[%s, %d]:Mtp buffer overflow require/free (%d/ %d) !! \n",
			__func__, __LINE__, count, writableSize);
		return 0;
	}
	if ((pWritePhyAddr + count) > pvr->pMtpBufUpper) {
		//handle mtp buffer wrap-around
		if (copy_from_user((void *)pWritePhyAddr-pvr->mtpPhyOffset, buf,
			pvr->pMtpBufUpper -pWritePhyAddr)) {
			dmx_err(pvr->tunerHandle, "[%s, %d]:Copy data to mtp buffer failed !! \n",
				__func__, __LINE__);
			return -EFAULT;
		}
		addByPassFilter(pvr, pWritePhyAddr-pvr->mtpPhyOffset, pvr->pMtpBufUpper -pWritePhyAddr);
		if (RHAL_WriteMTPBuffer(mtp_id, pWritePhyAddr, pvr->pMtpBufUpper -pWritePhyAddr)
			!= TPK_SUCCESS) {
			dmx_err(pvr->tunerHandle, "[%s, %d]: RHAL_WriteMTPBuffer failed!!\n",
				__func__, __LINE__);
			return -EFAULT;
		}
		buf = buf + (pvr->pMtpBufUpper -pWritePhyAddr);
		count = (pWritePhyAddr + count) - pvr->pMtpBufUpper;
		pWritePhyAddr = pvr->pMtpBufLower;
		dmx_crit(pvr->tunerHandle, "[%s, %d]: wrap-around new Wp/Cnt(%p/%d) !\n",
			__func__, __LINE__, pWritePhyAddr, count);
	}

	if (copy_from_user((void *)pWritePhyAddr-pvr->mtpPhyOffset, buf, count)) {
		dmx_err(pvr->tunerHandle, "[%s, %d]: Copy data to mtp buffer failed !! \n",
		__func__, __LINE__);
		return -EFAULT;
	}
	addByPassFilter(pvr, pWritePhyAddr-pvr->mtpPhyOffset, count);
	if (RHAL_WriteMTPBuffer(mtp_id, pWritePhyAddr, count) != TPK_SUCCESS) {
		dmx_err(pvr->tunerHandle, "[%s, %d]: RHAL_WriteMTPBuffer failed!! \n",
			__func__, __LINE__);
		return -EFAULT;
	}
	pvr->totalWriteSize += requireWriteSize;

	return requireWriteSize;
}

int pvr_pb_close(PVR_PB_SETTING* pvr)
{
	dmx_info(pvr->tunerHandle, "[%s, %d] \n", __func__, __LINE__);
	if (pvr->filterCnt > 0)
		clearByPassFilter(pvr);
	pvr->pbState = PVR_STATE_STOP;
	pvr->totalWriteSize = 0;
	return 0;
}

//=============================================================================
/*
 * Driver Status Path: /proc/lgtv-driver/dvb_dvr/status
 * 1. download device
 * status : idle / playing / paused
 * buffer status : total download buffer size , occupied buffer size ( kbytes )
 * read size total : total bytes for read function (mbytes)
 * 2. upload device
 * status : idle / playing / paused
 * buffer status : total download buffer size , occupied buffer size ( kbytes )
 * av sync : sync / free run
 * play rate : -1600 / -800 / -400 / -200 / 0 / 50 / 100 / 200 / 400 / 800 / 1600
 * write size total : total bytes for write function (mbytes)
*/
static ssize_t pvr_status_dump(char *buf)
{
	struct rdvb_dmxdev *rdvb_dmxdev = get_rdvb_dmxdev();
	char *pbuf = buf;
	int i = 0;

	//Record status
	pbuf = print_to_buf(pbuf, "--------------------\n");
	for (i=0; i< rdvb_dmxdev->dmxnum; i++)
	{
		SDEC_CHANNEL_T *sdec = NULL;
		sdec = (SDEC_CHANNEL_T *) rdvb_dmxdev->rdvb_dmx[i].dmxdev.demux;

		pbuf = print_to_buf(pbuf, "down-ch%d\n", i);
		if (sdec->pvrRecSetting.pvrThreadState == PVR_STATE_RUNNING)
		{
			struct dmxdev *dmxdev = &rdvb_dmxdev->rdvb_dmx[i].dmxdev;
			unsigned int occupySize = dvb_ringbuffer_avail(&dmxdev->dvr_buffer);

			pbuf = print_to_buf(pbuf, "status : running\n");
			pbuf = print_to_buf(pbuf, "buffer status : total %dkb, occupy %dkb\n",
				DVR_BUFFER_SIZE/1024, occupySize/1024);
			pbuf = print_to_buf(pbuf, "read size total : %dmb\n\n",
				(dmxdev->dvr_buffer_total_readSize)/(1024*1024));
		} else {
			pbuf = print_to_buf(pbuf, "status : idle\n\n");
		}
		if (i == 1) break;
	}

	//Playback status
	pbuf = print_to_buf(pbuf, "--------------------\n");
	for (i=0; i< rdvb_dmxdev->dmxnum; i++)
	{
		SDEC_CHANNEL_T *sdec = (SDEC_CHANNEL_T *) rdvb_dmxdev->rdvb_dmx[i].dmxdev.demux;
		PVR_PB_SETTING* pvr = &sdec->pvrPbSetting;

		pbuf = print_to_buf(pbuf, "up-ch%d\n", i);
		if (pvr->pbState == PVR_STATE_RUNNING)
		{
			REFCLOCK *pRefClock = sdec->refClock.prefclk;
			UINT8 *pWritePhyAddr = 0;
			UINT32 contiguousWritableSize = 0;
			UINT32 writableSize = 0;
			UINT32 totalBuSize = pvr->pMtpBufUpper - pvr->pMtpBufLower;

			RHAL_GetMTPBufferStatus(i, &pWritePhyAddr, &contiguousWritableSize,
				&writableSize);
			pbuf = print_to_buf(pbuf, "status : %s\n",
				(sdec->pvrPbSpeed == 0) ? "paused":"playing");
			pbuf = print_to_buf(pbuf, "buffer status : total %dkb, occupy %dkb\n",
				totalBuSize/1024, (totalBuSize-writableSize)/1024);
			pbuf = print_to_buf(pbuf, "av sync : %s\n",
				pRefClock->mastership.masterState ? "sync":"free run");
			pbuf = print_to_buf(pbuf, "play rate : %d\n", (sdec->pvrPbSpeed*100)/256);
			pbuf = print_to_buf(pbuf, "write size total : %dmb\n\n",
				pvr->totalWriteSize/(1024*1024));
		} else {
			pbuf = print_to_buf(pbuf, "status : idle\n\n");
		}

		if (i == 1)
			break;
	}
	pbuf = print_to_buf(pbuf, "--------------------\n");

	return pbuf - buf;
}

static int pvr_status_proc_open(struct inode * inode, struct file * pfile)
{

	char *status  = NULL;
	status = vmalloc(2048);
	if (status == NULL)
		return -ENOENT;
	pfile->private_data = status;
	pvr_status_dump(status);
	return 0;
}
static int pvr_status_proc_release(struct inode * inode, struct file * pfile)
{
	char * status = pfile->private_data;

	if (status)
		vfree(status);
	pfile->private_data = NULL;

	return 0;
}

static ssize_t pvr_status_proc_read (struct file * pfile, char __user *buf,
										size_t size, loff_t * offset)
{
	char *status  = NULL;
	ssize_t da_size = 0;
	ssize_t read_size = 0;
	int ret = 0;
	if (pfile->private_data == NULL)
		return -EIO;

	status = pfile->private_data;
	da_size = strlen(status);
	if (*offset >= da_size)
		return 0;

	ret = copy_to_user(buf, status+*offset, da_size- *offset);
	read_size = da_size- *offset - ret;

	return read_size;
}

const struct file_operations dvr_status_ops = {
	.owner = THIS_MODULE,
	.open = pvr_status_proc_open,
	.read = pvr_status_proc_read,
	.release = pvr_status_proc_release,
};

/*
 * Miscellaneous PVR Properties
 * Spec: ES with field picture encoded
 * File Path: /proc/lgtv-driver/dvb_dvr/prop_es_type
 * Value:
 * 0 : Encoded ES is Frame Picture
 * 1 : Encoded ES is Field Picture
*/
static ssize_t pvr_esType_proc_read (struct file * pfile, char __user *buf,
										size_t size, loff_t * offset)
{
	char esType = 0;
	ssize_t dataSize = 0;

	print_to_buf(&esType, "%d", APVR_PROP_ES_TYPE);
	dataSize = sizeof(char);

	if (*offset >= dataSize) {
		return 0;
	}

	if (copy_to_user(buf, &esType, dataSize)) {
		dmx_err(NOCH, "[%s, %d]: copy_to_user failed! \n", __func__, __LINE__);
		return -EFAULT;
	}
	*offset += dataSize;

	return dataSize;
}


const struct file_operations dvr_esType_ops = {
	.owner = THIS_MODULE,
	.read = pvr_esType_proc_read,
};

/*
 * Miscellaneous PVR Properties
 * Spec: Bandwidth capability limitation
 * File Path: /proc/lgtv-driver/dvb_dvr/prop_bw_capa
 * Value:
 * 0 : No BW limitation APVR and 4K play
 * 1 : BW limitation APVR and 4K play
*/
static ssize_t pvr_bwCapa_proc_read (struct file * pfile, char __user *buf,
										size_t size, loff_t * offset)
{
	char bwCapa = 0;
	ssize_t dataSize = 0;

	print_to_buf(&bwCapa, "%d", APVR_PROP_BW_CAPA);
	dataSize = sizeof(char);

	if (*offset >= dataSize) {
		return 0;
	}

	if (copy_to_user(buf, &bwCapa, dataSize)) {
		dmx_err(NOCH, "[%s, %d]: copy_to_user failed! \n", __func__, __LINE__);
		return -EFAULT;
	}
	*offset += dataSize;

	return dataSize;
}

const struct file_operations dvr_bwCapa_ops = {
	.owner = THIS_MODULE,
	.read = pvr_bwCapa_proc_read,
};
