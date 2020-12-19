#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/pageremap.h>

#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>

#pragma scalar_storage_order big-endian
#include <rpc_common.h>
#include <InbandAPI.h>
#include <VideoRPC_System.h>
#include <VideoRPC_Agent.h>
#include <video_userdata.h>
#pragma scalar_storage_order default

#include "venc_conf.h"
#include "venc_struct.h"
#include "venc_core.h"
#include "venc_test.h"

#include "vutils/vrpc.h"
#include "vcodec_struct.h"




static const venc_pixfmt_t pixfmt_cap[] = {
	{
		.pixelformat = V4L2_PIX_FMT_H264,
		.num_planes = 1,
	},
};


#define IF_ERR_GOTO(err_code, label) \
    if (err_code) { \
        pr_err("%s:%d ret %d\n", __FUNCTION__, __LINE__, err_code); \
        goto label; \
    }

#define IF_ERR_RET(err_code) \
    if (err_code) { \
        pr_err("%s:%d ret %d\n", __FUNCTION__, __LINE__, err_code); \
        return err_code; \
    }


static int readVencMsgInfo(venc_handle_t *handle)
{
    RINGBUFFER_HEADER *pRBH;
    vfifo_t *msgQ,*EncBsQ;
    unsigned int rd, wr ;
    //struct v4l2_event event;
    int type;
    //VENC_MSG_TYPE_T   enc_msg ;
    VIDEO_RPC_ENC_ELEM_GEN_INFO     videoGen,*msg = NULL  ;
    VIDEO_RPC_ENC_ELEM_FRAME_INFO   videoFrame;
    //unsigned long videoPTS     = 0;
    unsigned int frameNumber;
	vcodec_buffer_t *buf;

	EncBsQ = &handle->enc_bs;

    msgQ = &handle->enc_msg;
    pRBH = (RINGBUFFER_HEADER*)msgQ->hdr.uncached;

    rd = msgQ->rd[0];
    wr = msgQ->wr = pRBH->writePtr;


    /* do nothing if empty */
    if (rd == wr)
        return 0;

    while (rd != wr) 
	{
		vfifo_peek(msgQ, 0, &videoGen, sizeof(videoGen), (void**)&msg);
		type = (ENUM_DVD_VIDEO_ENCODER_OUTPUT_INFO_TYPE)msg->infoType;

		//pr_info("%s: msg type %x rd 0x%x wr 0x%x\n",__FUNCTION__, type, rd, wr);

      if (type == VIDEOENCODER_VideoGEN)
	  {
          handle->nPicInfo++;
          if (handle->nPicInfo == 1) 
		  {
          //pr_info("%s: get 1st PicInfo\n", __FUNCTION__);
          }

	      vfifo_read(msgQ,0,(void*)&videoGen,sizeof(videoGen));

          videoGen.encoderType			  = VF_TYPE_VIDEO_ENCODER;

       #if 0
           pr_info("VENC: Deliver, got GenInfo:		  \n\
				   GenInfo->encoderType = %d		  \n\
			       GenInfo->videoInputSource = %d	  \n\
				   GenInfo->hResolution = %d				  \n\
				   GenInfo->videoRCMode = %d		  \n\
				   GenInfo->video_STD_buf_size= %d	  \n\
				   GenInfo->init_STD_buf_fullness= %d \n\
				   GenInfo->videoBitRate= %d		  \n\
				   GenInfo->M = %d					  \n\
				   GenInfo->N = %d					  \n\
				   GenInfo->numOfGopsPerVOBU = %d \n\n\n", videoGen.encoderType    , videoGen.videoInputSource
														 , videoGen.horizontalResolusion, videoGen.videoRCMode
														 , videoGen.video_STD_buffer_size, videoGen.init_STD_buffer_fullness
														 , videoGen.videoBitRate, videoGen.gop_M, videoGen.gop_N
														 , videoGen.numOfGOPsPerVOBU);
        #endif

          vfifo_skip(msgQ, 0, 128-sizeof(videoGen));

          rd = msgQ->rd[0];

      }
      else if (type == VIDEOENCODER_VideoFrameInfo) 
	  {
          handle->nFrmInfo++;
          if (handle->nFrmInfo == 1) 
		  {
            pr_info("%s: get 1st FrmInfo,msgQ->pBase=%x\n", __FUNCTION__,(int)msgQ->pBase);
          }
		  vfifo_read(msgQ,0,(void*)&videoFrame,sizeof(videoFrame));
          /*
		  pr_info("Got FrameInfo:			  \n\
				   FrameInfo->pictureNumber= %d   \n\
				   FrameInfo->pictureType  = %d   \n\
				   FrameInfo->frameSize 	= %d  \n", videoFrame.pictureNumber, videoFrame.pictureType, videoFrame.frameSize);
          */

			//videoPTS  = (((unsigned long)videoFrame.PTShigh)<<32) | (videoFrame.PTSlow);

			if (videoFrame.pictureNumber != frameNumber)
			{
			 pr_info("[VENC]: pictureNumber not continuous! correct: %d, now: %d\n",
											  frameNumber, videoFrame.pictureNumber);
				frameNumber = videoFrame.pictureNumber;
		    }

            if (!list_empty(&handle->buf_list))
            {
				buf = list_first_entry(&handle->buf_list, struct vcodec_buffer_t,list);
				
                //pr_info("[VENC]userptr:%x,EncBsQ->pBase=%x\n",(int)buf->vb.vb2_buf.planes[0].m.userptr,(int)EncBsQ->pBase);
                if(EncBsQ->pBase!=0)
                {
                void *vaddr = vb2_plane_vaddr(&buf->vb.vb2_buf, 0);
                //pr_info("%s:[%d] vaddr:%x \n", __FUNCTION__,__LINE__,(int)vaddr);
                vfifo_read(EncBsQ,0, vaddr,videoFrame.frameSize);
                //vfifo_skip(EncBsQ,0,videoFrame.frameSize);
                buf->vb.vb2_buf.planes[0].bytesused=videoFrame.frameSize;
                list_del(&buf->list);
                vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_DONE);
                }
            }

			if(frameNumber%10==0) pr_info("[VENC]: frame_num %d size %d\n",videoFrame.pictureNumber,videoFrame.frameSize);
			frameNumber++;
			//g_stVencData.outframeNumber = frameNumber;

            vfifo_skip(msgQ, 0, 128-sizeof(videoFrame));
            rd = msgQ->rd[0];
        }
        else 
		{
            pr_err("%s: unknown type %x rd 0x%x wr 0x%x\n",
                __FUNCTION__, type, rd, wr);
            vfifo_syncRd(msgQ, 0, wr);
            break;
        }
    }

    return 0;
}

static int onTask(void *arg)
{
    venc_handle_t *handle;
    //pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);

    if (!arg)
        return -1;

    handle = (venc_handle_t*)arg;

    while (!kthread_should_stop()) {
        mutex_lock(&handle->thread_lock);
        readVencMsgInfo(handle);
        mutex_unlock(&handle->thread_lock);
        msleep_interruptible(handle->thread_msecs);
    }
    //pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);

    return 0;
}

static int initTask(venc_handle_t *handle)
{
    struct task_struct *thread;
    pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);

    if (!handle)
        return -EINVAL;

    if (handle->thread)
        return 0;

    thread = kthread_create(onTask, handle, "venc_task");
    if (IS_ERR(thread)) {
        pr_err("%s: kthread_create fail\n", __FUNCTION__);
        return -1;
    }

    handle->thread = thread;
    wake_up_process(thread);
    pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);

    return 0;
}

static int exitTask(venc_handle_t *handle)
{
    if (!handle)
        return -EINVAL;

    if (!handle->thread)
       return 0;

    kthread_stop(handle->thread);
    handle->thread = NULL;

    return 0;
}

static int onService(int portID, int cmd, void *data, int size)
{

    pr_info("%s: portID %d cmd %d data %p size %d\n",
        __FUNCTION__, portID, cmd, data, size);

    return 0;
}


const venc_pixfmt_t* venc_getPixelFormat(u32 pixelformat)
{
	int i;

	for (i=0; i<ARRAY_SIZE(pixfmt_cap); i++) {
		if (pixfmt_cap[i].pixelformat == pixelformat)
			return &pixfmt_cap[i];
	}

	return NULL;
}

const venc_pixfmt_t* venc_getPixelFormatByIndex(int index)
{
	if (index >=0 && index < ARRAY_SIZE(pixfmt_cap))
		return &pixfmt_cap[index];

	return NULL;
}

int venc_setFormat(struct v4l2_pix_format_mplane *mp, u32 pixfmt, int picW, int picH)
{
	int pitchY, pitchC;
	const venc_pixfmt_t *inst;

	inst = venc_getPixelFormat(pixfmt);
	if (!inst) {
		pr_err("%s: invalid pixfmt 0x%08x\n", __FUNCTION__, pixfmt);
		return -EINVAL;
	}

	/* load instance */
	mp->pixelformat = inst->pixelformat;
	mp->num_planes = inst->num_planes;

	/* calculate bytesperline and sizeimage */
	if (pixfmt == V4L2_PIX_FMT_H264) {
		pitchY = picW; /* TODO: 10 bits and align */
		pitchC = picW; /* TODO: 10 bits and align */
		mp->width = picW;
		mp->height = picH;
		mp->plane_fmt[0].bytesperline = 0;//pitchY;
		mp->plane_fmt[0].sizeimage = 0x2000;//pitchY * mp->height;
		//mp->plane_fmt[1].bytesperline = pitchC;
		//mp->plane_fmt[1].sizeimage = pitchC * (mp->height >> 1);
	}

	return 0;
}


int venc_setSource(venc_handle_t *handle, int port)
{
int ret;

    //if (!handle->bChannel || !handle->bSource)
        //return -EPERM;

    if (!handle)
        return -1;

    if (port < 0) {
    }

     venc_emulateSource(handle);
	 //pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);

    if (!handle->bFilter) {
        ret = venc_flt_init(handle);
        if (ret) return ret;
    }
    //pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);

    //if (port > 1)
        //return -1;

    /* TODO */


    return 0;
}

int venc_flt_init(venc_handle_t *handle)
{
    int ret, userID ,size;
    unsigned int flt_enc;
    char bServ, bMsgQ, bUserDataQ, bBitstreamQ, bTask ,bDinQ;

    if (!handle || !handle->userID) {
        pr_err("%s: no handle or userID\n", __FUNCTION__);
        return -EINVAL;
    }

    if (handle->bFilter) {
        pr_err("%s: already created\n", __FUNCTION__);
        return -EPERM;
    }

    userID = handle->userID;
    flt_enc = 0;
    bServ = bMsgQ = bUserDataQ = bBitstreamQ = bDinQ = bTask = 0;

    //ret = subscribe(flt_enc, V4L2_EVENT_PRIVATE_EXT_VDEC_EVENT, V4L2_SUB_EXT_VDEC_FRAME);
    //IF_ERR_EXIT(ret);

    /* register service at the beginning */
    bServ = !(ret = vrpc_registerService(userID, userID, (vrpc_serv_t)onService,handle));
    IF_ERR_GOTO(ret, err);

    /* alloc msgQ ringbuf */
#if 1
    size = VENC_ENC_RBSIZE_MESSAGE;//sizeof(VIDEO_DEC_FRM_MSG) * VDEC_CONF_FRM_MSG_CNT;
    bMsgQ = !(ret = vfifo_init(&handle->enc_msg, RINGBUFFER_MESSAGE, size, 1, VFIFO_F_RDONLY));
    IF_ERR_GOTO(ret, err);

    //size = VENC_DIN_RBSIZE_COMMAND;//sizeof(VIDEO_DEC_FRM_MSG) * VDEC_CONF_FRM_MSG_CNT;
    //bMsgQ = !(ret = vfifo_init(&handle->din_cmd, RINGBUFFER_COMMAND, size, 1, VFIFO_F_RDONLY));
    //IF_ERR_GOTO(ret, err);

    /* alloc VENC BS ringbuf */
    //size = VENC_ENC_RBSIZE_STREAM;
    //bUserDataQ = !(ret = vfifo_init(&handle->enc_bs, RINGBUFFER_STREAM, size, 2, VFIFO_F_RDONLY));
    //IF_ERR_GOTO(ret, err);

    //size = VENC_DIN_RBSIZE_STREAM;
    //bUserDataQ = !(ret = vfifo_init(&handle->din_bs, RINGBUFFER_STREAM, size, 2, VFIFO_F_RDONLY));
    //IF_ERR_GOTO(ret, err);
#else
    /* import bitstream ringbuf */
    bMsgQ = !(ret = vfifo_init_importHdr(&handle->enc_msg, handle->src.enc_msg_hdr_addr, 0));
    IF_ERR_GOTO(ret, err);
#endif
    bMsgQ = !(ret = vfifo_init_importHdr(&handle->din_cmd, handle->src.din_ib_hdr_addr, VFIFO_F_WRONLY));
    IF_ERR_GOTO(ret, err);

    bBitstreamQ = !(ret = vfifo_init_importHdr(&handle->enc_bs, handle->src.enc_bs_hdr_addr, VFIFO_F_RDONLY));
    IF_ERR_GOTO(ret, err);

	
	pr_info("%s:enc_bs->pBase=%x\n", __FUNCTION__,(int)handle->enc_bs.pBase);

    bBitstreamQ = !(ret = vfifo_init_importHdr(&handle->din_bs, handle->src.din_bs_hdr_addr, VFIFO_F_WRONLY));
    IF_ERR_GOTO(ret, err);

    //pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);

    /* create filters */
    ret = !(flt_enc = vrpc_createFilter(userID, VF_TYPE_VIDEO_ENCODER));
    IF_ERR_GOTO(ret, err);
    //pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);

    /* init encoder */
    ret = vrpc_sendRPC_payload_va(userID,
            VIDEO_RPC_ENC_ToAgent_Init,
            VIDEO_RPC_ENC_INIT,
            .instanceID = flt_enc,
            .type = handle->src.enc_type,
		    .videoFormat = (VIDEO_FORMAT) VIDEO_FORMAT_NTSC,
		    .videoSource = VIDEO_SOURCE_RAWIN,
		    .yuvFormat	 = (YUV_FMT)FMT_YUV420P,
		    .mixerWinID  = 0,
            );
    IF_ERR_GOTO(ret, err);
    //pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);

    /* set enc filter's ringbufs */
    ret = vrpc_postRingBuf_pinID(userID, flt_enc, handle->enc_bs.hdr.addr, VENC_Enc_RB_PIN);
    IF_ERR_GOTO(ret, err);
    //pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);

    ret = vrpc_postRingBuf_pinID(userID, flt_enc, handle->enc_msg.hdr.addr, VENC_Enc_RB_PIN);
    IF_ERR_GOTO(ret, err);
    //pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);

    /* set din filter's ringbufs */
    ret = vrpc_postRingBuf_pinID(userID, flt_enc, handle->din_bs.hdr.addr, VENC_DIN_RB_PIN);
    IF_ERR_GOTO(ret, err);
    //pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);

    ret = vrpc_postRingBuf_pinID(userID, flt_enc, handle->din_cmd.hdr.addr, VENC_DIN_RB_PIN);
    IF_ERR_GOTO(ret, err);
    //pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);


    ret = vrpc_sendRPC_payload_va(userID,
            VIDEO_RPC_ENC_ToAgent_SetNR,
            VIDEO_RPC_ENC_SET_NR,
                .instanceID = flt_enc,
                .Hstrength = 0,
                .Vstrength = 1,
            );
    IF_ERR_GOTO(ret, err);
    //pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);


    /* set enc format */
    ret = vrpc_sendRPC_payload_va(userID,
            VIDEO_RPC_ENC_ToAgent_SetEncodeFormat,
            VIDEO_RPC_ENC_SET_ENCFORMAT,
                .instanceID = flt_enc,
                .streamType = VIDEO_STREAM_H264,
            );
    IF_ERR_GOTO(ret, err);
    //pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);

    /* set resolution  */
    ret = vrpc_sendRPC_payload_va(userID,
            VIDEO_RPC_ENC_ToAgent_SetNewResolution,
            VIDEO_RPC_ENC_SET_NEW_RESOLUTION,
                .instanceID = flt_enc,
		        .in_width = 720,
		        .in_height = 480,
                .out_width = 720,
                .out_height = 480,
            );
    IF_ERR_GOTO(ret, err);
    //pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);

    /* set frame rate */
        ret = vrpc_sendRPC_payload_va(userID,
                VIDEO_RPC_ENC_ToAgent_SetFrameRate,
                VIDEO_RPC_ENC_SET_FRAME_RATE,
                    .instanceID = flt_enc,
                    .frame_rate = 30,
                );
        IF_ERR_GOTO(ret, err);
		//pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);

    /* set bit rate */
    ret = vrpc_sendRPC_payload_va(userID,
            VIDEO_RPC_ENC_ToAgent_SetBitRate,
            VIDEO_RPC_ENC_SET_BITRATE,
                .instanceID = flt_enc,
		        .rateControlMode	 = VIDEO_RATE_CBG,//CBG:enable RC; CBR:disable RC. 
		        .peakBitRate		 = 0,
		        .aveBitRate		 = 3*1024*1024,//aveBitRate; if use CBR, set this value for QP (ex:30) 
		        .bitBufferSize 	 = 0,//2*1024*1024 ,
		        .initBufferFullness = 0,//1*1024*1024 ,
		        .time=0,
            );
    IF_ERR_GOTO(ret, err);
    //pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);

    /* set GOP structure */
    ret = vrpc_sendRPC_payload_va(userID,
            VIDEO_RPC_ENC_ToAgent_SetGOPStructure,
            VIDEO_RPC_ENC_SET_GOPSTRUCTURE,
                .instanceID = flt_enc,
                .M = 1,
                .N = 20,
            );
    IF_ERR_GOTO(ret, err);
    //pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);

    /* set profile */
    ret = vrpc_sendRPC_payload_va(userID,
            VIDEO_RPC_ENC_ToAgent_SetProfile,
            VIDEO_RPC_ENC_SET_PROFILE,
                .instanceID = flt_enc,
                .profile = VIDEO_PROFILE_HIGH,
            );
    IF_ERR_GOTO(ret, err);
    //pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);


    /* start record */
    ret = vrpc_sendRPC_payload_va(userID,
            VIDEO_RPC_ENC_ToAgent_StartRecord,
            VIDEO_RPC_ENC_START_ENC,
                .instanceID = flt_enc,
                .startMode = 0,
            );
    IF_ERR_GOTO(ret, err);
    //pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);


    /* create reader task */
    bTask = !(ret = initTask(handle));
    IF_ERR_GOTO(ret, err);
    //pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);

    /* finalize */
    handle->flt_enc = flt_enc;
    handle->bFilter = 1;

    return 0;

err:
    pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);
    if (bTask) exitTask(handle);
    if (flt_enc) vrpc_releaseFilter(handle->userID);
    if (bMsgQ) vfifo_exit(&handle->msgQ);
    if (bUserDataQ) vfifo_exit(&handle->userDataQ);
    if (bBitstreamQ) vfifo_exit(&handle->bitstreamQ);
    if (bServ) vrpc_unregisterService(userID, userID);

    return ret;
}

////////
int venc_flt_sendCmd(venc_handle_t *handle, int cmd)
{
    int ret;

    if (!handle || !handle->bFilter)
        return -1;

    ret = vrpc_sendRPC_param(handle->userID, cmd, handle->flt_enc, 0);
    IF_ERR_RET(ret);

    return 0;
}

int venc_flt_run(venc_handle_t *handle)
{
    return venc_flt_sendCmd(handle, VIDEO_RPC_ToAgent_Run);
}

int venc_flt_pause(venc_handle_t *handle)
{
    return venc_flt_sendCmd(handle, VIDEO_RPC_ToAgent_Pause);
}

int venc_flt_flush(venc_handle_t *handle)
{
    return venc_flt_sendCmd(handle, VIDEO_RPC_ToAgent_Flush);
}

int venc_flt_stop(venc_handle_t *handle)
{
    return venc_flt_sendCmd(handle, VIDEO_RPC_ToAgent_Stop);
}

int venc_flt_destroy(venc_handle_t *handle)
{
    return venc_flt_sendCmd(handle, VIDEO_RPC_ToAgent_Destroy);
}

int venc_flt_exit(venc_handle_t *handle)
{
    int ret;

    if (!handle)
        return -EINVAL;

    if (!handle->bFilter)
        return -EPERM;

    ret = exitTask(handle);
    IF_ERR_RET(ret);

    venc_flt_pause(handle);
    venc_flt_flush(handle);
    venc_flt_stop(handle);
    venc_flt_destroy(handle);

    vfifo_exit(&handle->msgQ);
    vfifo_exit(&handle->userDataQ);
    vfifo_exit(&handle->bitstreamQ);

    ret = vrpc_unregisterService(handle->userID, handle->userID);
    IF_ERR_RET(ret);

    #if 0
    /* flush pending events */
    struct v4l2_event event;
    while (v4l2_event_dequeue(&handle->fh, &event, 1) == 0) {}
    #endif

    handle->bFilter = 0;

    return 0;
}

int venc_initHandle(venc_handle_t *handle)
{
    int userPID, userSockfd;

    userPID = current->pid;
    userSockfd = current->files->next_fd - 1; /* peek current fd */
    handle->userID = vrpc_generateUserID(userPID, userSockfd);
    mutex_init(&handle->thread_lock);
	
    handle->thread_msecs = VDEC_CONF_TASK_MSECS;

	INIT_LIST_HEAD(&handle->buf_list);

    pr_info("%s: userID 0x%08x\n",
        __FUNCTION__, handle->userID);

    return 0;
}


int venc_exitHandle(venc_handle_t *handle)
{
    if (!handle)
        return -1;

    if (handle->bFilter)
        venc_flt_exit(handle);

    #ifdef DEBUG_ALL
    DEBUG_ALL();
    #endif

    pr_info("%s: userID 0x%08x\n",
        __FUNCTION__, handle->userID);

    return 0;
}

int venc_getPermission(venc_handle_t *handle, phys_addr_t addr, int size)
{
    phys_addr_t addrEnd, start, end;

    if (!handle || !addr || !size)
        return -EINVAL;

    addrEnd = addr + size;
    start = end = 0;

    /* TODO */
    return 0;

    pr_warning("%s: userID 0x%08x addr %pa size %d forbidden\n",
        __FUNCTION__, handle->userID, &addr, size);

    return -EPERM;
}

int venc_getStream(venc_handle_t *handle, void *userPtr, int size)
{
    int ret=0;
#if 0

    vfifo_t *enc_bsQ;
    enc_bsQ = &handle->enc_bs;
    int rd, wr;
	int bs_size=0;
	void *buf_cached, *buf_uncached;

    rd = (int)enc_bsQ->rd[0];
    wr = (int)enc_bsQ->wr;
    ret = 0;


    if (rd == wr) {
        pr_err("%s: no userData\n", __FUNCTION__);
        return -1;
    }
    bs_size=wr-rd;
    buf_cached = dvr_malloc_uncached_specific(bs_size, GFP_DCU1, &buf_uncached);

    vfifo_read(enc_bsQ,0,buf_cached,bs_size);

    ret = copy_to_user(userPtr,buf_cached,bs_size );
    if (ret) {
        pr_err("%s: copy_to_user fail\n", __FUNCTION__);
        ret = -EFAULT;
    }
#endif

    return ret;
}

int venc_streamon(venc_handle_t *handle)
{
	//vcodec_buffer_t *buf;

	//if (!handle->bBufQ && initBufQ(handle) < 0)
		//return -1;

	/* deliver all pending buffers */
	//list_for_each_entry (buf, &handle->buf_list, list) {
		//if (!buf->nIn)
			//sendQBuf(handle, buf);
	//}

	//initThread(handle, pollCap, "vcap_pollCap", 10);
	//initTask(handle);
	return 0;
}

int venc_streamoff(venc_handle_t *handle)
{
	//exitThread(handle);
	//returnAllBuf(handle);
	return 0;
}


