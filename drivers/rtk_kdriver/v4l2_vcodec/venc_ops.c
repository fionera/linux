#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <linux/mm.h>

#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-vmalloc.h>

#include "venc_conf.h"
#include "venc_struct.h"
#include "venc_core.h"
#include "vcodec_struct.h"
#include "vcodec_utils.h"
#include "vcodec_def.h"
#include <rpc_common.h>
#include "vutils/vrpc.h"


static int venc_queue_setup(struct vb2_queue *q, const void *parg,
			   unsigned int *num_buffers, unsigned int *num_planes,
			   unsigned int sizes[], void *alloc_ctxs[])
{
	int nPlanes;
	venc_handle_t *handle;

	handle = vb2_get_drv_priv(q);

	pr_info("%s: type %d nBufs %d\n", __FUNCTION__, q->type, *num_buffers);

	if (q->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		nPlanes = handle->pixfmt.num_planes;
		*num_planes = nPlanes;
	}

	return 0;
}

#if 0
static int venc_buf_init(struct vb2_buffer *vb)
{
	pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct vb2_queue *vq = vb->vb2_queue;
	//struct s5p_mfc_ctx *ctx = fh_to_ctx(vq->drv_priv);
	unsigned int i;
	int ret;

	if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		vb2_dma_contig_plane_dma_addr(vb, 0);
	} else if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		
	} else {
		pr_err("invalid queue type: %d\n", vq->type);
		return -EINVAL;
	}
	return 0;
}

static int venc_buf_prepare(struct vb2_buffer *vb)
{
	pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);
	struct vb2_queue *vq = vb->vb2_queue;
    struct v4l2_fh *vfh;
    venc_handle_t *handle;
	struct v4l2_pix_format_mplane *pix_fmt_mp;

    vfh = vq->drv_priv;
    handle = container_of(vfh, venc_handle_t, fh);

	pix_fmt_mp = &handle->pixfmt;
	int ret;

	if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {

		if (vb2_plane_size(vb, 0) < 0x200000) {
			pr_err("plane size is too small for capture\n");
			return -EINVAL;
		}
	} else if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
	} else {
		pr_err("invalid queue type: %d\n", vq->type);
		return -EINVAL;
	}
	return 0;
}
#endif
static int venc_start_streaming(struct vb2_queue *q, unsigned int count)
{
	venc_handle_t *handle;
	pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);

	handle = vb2_get_drv_priv(q);

	return venc_streamon(handle);
}

static void venc_stop_streaming(struct vb2_queue *q)
{
	venc_handle_t *handle;
	pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);

	handle = vb2_get_drv_priv(q);

	venc_streamoff(handle);
}

int venc_qbuf(venc_handle_t *handle, struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	venc_buffer_t *buf = container_of(vbuf, venc_buffer_t, vb);
	//unsigned char tmpbuf[3]={0xf7,0xf8,0xf9};

	/* TODO: queue buf into video-firmwares capture */

	//pr_info("%s:[%d] usptr:%x \n", __FUNCTION__,__LINE__,(int)vb->planes[0].m.userptr);
	/*if(copy_to_user((void __user *) vb->planes[0].m.userptr, tmpbuf, sizeof(unsigned char)))
	{
		pr_info("%s:[%d]copy_to_user_fail!! usptr:%x \n", __FUNCTION__,__LINE__,(int)vb->planes[0].m.userptr);
		return -EFAULT;
	}*/
	list_add_tail(&buf->list, &handle->buf_list);

	return 0;
}


static void venc_buf_queue(struct vb2_buffer *vb)
{

struct vb2_v4l2_buffer *vbuf;
venc_handle_t *handle;
vcodec_buffer_t *buf;

//pr_info("%s: type %d index %d\n", __FUNCTION__, vb->type, vb->index);

handle = vb2_get_drv_priv(vb->vb2_queue);
vbuf = to_vb2_v4l2_buffer(vb);
buf = container_of(vbuf, vcodec_buffer_t, vb);

mutex_lock(&handle->thread_lock);

if (vb->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
	venc_qbuf(handle, vb);

mutex_unlock(&handle->thread_lock);

}


static int venc_allocMemory(venc_handle_t *handle, struct v4l2_ext_vdec_mem_alloc *userPtr)
{
	int ret;
	struct v4l2_ext_vdec_mem_alloc args;
	vrpc_mresult_t mres;

	if (copy_from_user(&args, userPtr, sizeof(args)))
		return -EFAULT;

	if (!args.size)
		return -EFAULT;

	ret = vrpc_malloc(handle->userID, args.type, args.size, args.flags, &mres);
	if (ret) return ret;

	args.phyaddr = mres.phyaddr;
	if (copy_to_user(userPtr, &args, sizeof(args)))
		return -EFAULT;

	return 0;
}

static int venc_releaseMemory(venc_handle_t *handle, struct v4l2_ext_vdec_mem_alloc *userPtr)
{
	int ret;
	struct v4l2_ext_vdec_mem_alloc args;

	if (copy_from_user(&args, userPtr, sizeof(args)))
		return -EFAULT;

	if (!args.phyaddr || !args.size)
		return -EFAULT;

	ret = vrpc_free(handle->userID, args.phyaddr);
	if (ret) return ret;

	return 0;
}


static struct vb2_ops venc_qops = {
	.queue_setup		= venc_queue_setup,
	//.wait_prepare		= vb2_ops_wait_prepare,
	//.wait_finish		= vb2_ops_wait_finish,
	//.buf_init		    = venc_buf_init,
	//.buf_prepare		= venc_buf_prepare,
	.start_streaming	= venc_start_streaming,
	.stop_streaming		= venc_stop_streaming,
	.buf_queue		    = venc_buf_queue,
};

struct vb2_ops *get_enc_queue_ops(void)
{
	return &venc_qops;
}

static int fops_open(struct file *filp)
{
    int ret = 0;
    vcodec_device_t *device;
    venc_handle_t *handle;
	//struct vb2_queue *q;

    device = video_drvdata(filp);

    if (mutex_lock_interruptible(&device->lock))
        return -ERESTARTSYS;

    handle = vzalloc(sizeof(venc_handle_t));
    if (!handle) {
        ret = -ENOMEM;
        goto exit;
    }

    filp->private_data = &handle->fh;
    handle->device = device;
    v4l2_fh_init(&handle->fh, &device->vdev);
    v4l2_fh_add(&handle->fh);
    list_add(&handle->list, &device->handle_list);
    venc_initHandle(handle);

	handle->vq_dst.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	handle->vq_dst.io_modes = VB2_MMAP | VB2_DMABUF | VB2_USERPTR;
	handle->vq_dst.timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	handle->vq_dst.ops = &venc_qops;
	handle->vq_dst.mem_ops = &vb2_vmalloc_memops;//&vb2_dma_contig_memops;
	handle->vq_dst.drv_priv = handle;
	//handle->vq_dst.buf_struct_size = sizeof(vcodec_buffer_t);
	handle->vq_dst.min_buffers_needed = 0;

    device->vdev.queue = &handle->vq_dst; /* for vb2 */

   ret = vb2_queue_init(&handle->vq_dst);

/* One way to indicate end-of-stream for MFC is to set the
 * bytesused == 0. However by default videobuf2 handles bytesused
 * equal to 0 as a special case and changes its value to the size
 * of the buffer. Set the allow_zero_bytesused flag so that videobuf2
 * will keep the value of bytesused intact.
 */
//q->allow_zero_bytesused = 1;

INIT_LIST_HEAD(&handle->buf_list);



exit:
    mutex_unlock(&device->lock);
    return ret;
}

static int fops_release(struct file *filp)
{
    struct v4l2_fh *fh;
    vcodec_device_t *device;
    venc_handle_t *handle;

    fh = filp->private_data;
    device = video_drvdata(filp);
    handle = container_of(fh, venc_handle_t, fh);

    mutex_lock(&device->lock);
    venc_exitHandle(handle);
    list_del(&handle->list);
    v4l2_fh_del(fh);
    v4l2_fh_exit(fh);
    vfree(handle);
    mutex_unlock(&device->lock);

    return 0;
}

static ssize_t fops_write(struct file *filp, const char *buf, size_t cnt, loff_t *offp)
{
    char *pCmd, *pVal, cmdBuf[80], *pCmdBuf;

    if (cnt > sizeof(cmdBuf))
        return -E2BIG;

    if (copy_from_user(&cmdBuf, buf, cnt))
        return -EFAULT;

    cmdBuf[cnt-1] = 0;
    pCmdBuf = cmdBuf;

    pCmd = strsep(&pCmdBuf, "=");
    pVal = pCmdBuf;

    #define DISPATCH(__cmd, __fun) \
        if (!strcmp(pCmd, __cmd)) {__fun;} else

    //DISPATCH("status", vdec_showStatus(NULL))
    pr_err("unknown cmd <%s> val <%s>\n", pCmd, pVal);

    return cnt;
}

int fops_mmap(struct file *filp, struct vm_area_struct *vma)
{
    phys_addr_t addr;
    int ret, size;
    venc_handle_t *handle;
    struct v4l2_fh *fh;

    addr = vma->vm_pgoff << PAGE_SHIFT;
    size = vma->vm_end - vma->vm_start;

    fh = filp->private_data;
    handle = container_of(fh, venc_handle_t, fh);

    ret = venc_getPermission(handle, addr, size);
    if (ret) return ret;

    ret = remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, size, vma->vm_page_prot);
    if (ret) return ret;

    return 0;
}

static int vidioc_g_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
    struct v4l2_fh *vfh;
    struct v4l2_pix_format_mplane *mp;
    venc_handle_t *handle;
	struct v4l2_pix_format_mplane *pix_fmt_mp;

    vfh = file->private_data;
    handle = container_of(vfh, venc_handle_t, fh);

	pix_fmt_mp = &f->fmt.pix_mp;

	pr_info("%s f->type = %d \n",__FUNCTION__, f->type);
	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		/* This is run on output (encoder dest) */
		//pix_fmt_mp->width = 720;
		//pix_fmt_mp->height = 480;
		//pix_fmt_mp->field = V4L2_FIELD_NONE;
		//pix_fmt_mp->pixelformat =V4L2_PIX_FMT_YUV420 ;
		//pix_fmt_mp->num_planes = ;

		//pix_fmt_mp->plane_fmt[0].sizeimage = 0x200000;

        memcpy(pix_fmt_mp, &handle->pixfmt, sizeof(*mp));

		
	} else {
		pr_info("invalid buf type\n");
		return -EINVAL;
	}
	return 0;
}

static int vidioc_try_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct v4l2_pix_format_mplane *pix_fmt_mp = &f->fmt.pix_mp;

	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {

		pix_fmt_mp->plane_fmt[0].bytesperline =
			pix_fmt_mp->plane_fmt[0].sizeimage;
	} else {
		pr_info("invalid buf type\n");
		return -EINVAL;
	}
	return 0;
}

static int vidioc_s_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	int ret;
	struct v4l2_fh *vfh;
	struct v4l2_pix_format_mplane *mp;
	venc_handle_t *handle;

	vfh = file->private_data;
	handle = container_of(vfh, venc_handle_t, fh);
	mp = &f->fmt.pix_mp;

	ret = venc_setFormat(mp, mp->pixelformat, mp->width, mp->height);
	if (ret) return ret;

	memcpy(&handle->pixfmt, mp, sizeof(*mp));

	return 0;

}

#if 0
static int ioctl_dqbuf(struct file *file, void *fh, struct v4l2_buffer *buf)
{
    struct v4l2_fh *vfh;
    venc_handle_t *handle;
    vfifo_t *EncBsQ;
    RINGBUFFER_HEADER *pRBH;
	pr_info("%s:\n", __FUNCTION__);

    vfh = file->private_data;
    handle = container_of(vfh, venc_handle_t, fh);

    EncBsQ = &handle->enc_bs;
    pRBH = (RINGBUFFER_HEADER*)EncBsQ->hdr.uncached;

	buf->m.offset=pRBH->writePtr;
	
	buf->bytesused=pRBH->writePtr-pRBH->readPtr[0];
		
	pr_info("%s:[w:%x,r:%x]\n", __FUNCTION__,(int)pRBH->writePtr,(int)pRBH->readPtr[0]);

 
    return 0;
}

static int ioctl_qbuf(struct file *file, void *fh, struct v4l2_buffer *buf)
{
    struct v4l2_fh *vfh;
    venc_handle_t *handle;

    vfh = file->private_data;
    handle = container_of(vfh, venc_handle_t, fh);

 
 pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);
    return 0;
}

static int ioctl_querybuf(struct file *file, void *fh, struct v4l2_buffer *buf)
{
    struct v4l2_fh *vfh;
    venc_handle_t *handle;

    vfh = file->private_data;
    handle = container_of(vfh, venc_handle_t, fh);

 
 pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);
    return 0;
}

static int ioctl_reqbufs(struct file *file, void *fh, struct v4l2_requestbuffers *reqbufs)
{
	pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);
    struct v4l2_fh *vfh;
    venc_handle_t *handle;
    int ret;
    vfh = file->private_data;
    handle = container_of(vfh, venc_handle_t, fh);

	pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);

	/* if memory is not mmp or userptr return error */
	if ((reqbufs->memory != V4L2_MEMORY_MMAP) &&
		(reqbufs->memory != V4L2_MEMORY_USERPTR))
		return -EINVAL;

	pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);

	if (reqbufs->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		if (reqbufs->count == 0) {
			pr_info("Freeing buffers\n");
			ret = vb2_reqbufs(&handle->vq_dst, reqbufs);
			return ret;
		}
		
		pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);
		ret = vb2_reqbufs(&handle->vq_dst, reqbufs);
		if (ret != 0) {
			pr_info("error in vb2_reqbufs() for E(D)\n");
			return ret;
		}
	}

 
 pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);
    return 0;
}
#endif
static int ioctl_g_ctrl(struct file *file, void *fh, struct v4l2_control *ctrl)
{
    struct v4l2_fh *vfh;
    venc_handle_t *handle;

    vfh = file->private_data;
    handle = container_of(vfh, venc_handle_t, fh);

    switch (ctrl->id) {

        case V4L2_CID_EXT_VDEC_DECODER_VERSION:
            ctrl->value = VDEC_VERSION;
            break;

        case V4L2_CID_EXT_VDEC_DISPLAY_DELAY:
            //ctrl->value = handle->display_delay;
            break;

        default:
            pr_err("%s: unknown id 0x%x\n",
                __FUNCTION__, ctrl->id);
            return -EINVAL;
    }

    return 0;
}

static int ioctl_s_ctrl(struct file *file, void *fh, struct v4l2_control *ctrl)
{
    int ret;
    struct v4l2_fh *vfh;
    venc_handle_t *handle;

    vfh = file->private_data;
    handle = container_of(vfh, venc_handle_t, fh);

    switch (ctrl->id) {

        case V4L2_CID_EXT_VDEC_CHANNEL:
            //ret = vdec_setChannel(handle, ctrl->value);
            break;

        case V4L2_CID_EXT_VDEC_VTP_PORT:
            ret = venc_setSource(handle, ctrl->value);
            break;

        case V4L2_CID_EXT_VDEC_RESETTING:
            //ret = vdec_reset(handle);
            break;

        case V4L2_CID_EXT_VDEC_VDO_PORT:
            //ret = vdec_connectDisplay(ctrl->value, handle->device->port);
            break;

        case V4L2_CID_EXT_VDEC_AV_SYNC:
            //ret = vdec_setAVSync(handle, ctrl->value);
            break;

        case V4L2_CID_EXT_VDEC_FRAME_ADVANCE:
            //ret = vdec_setFrameAdvance(handle, ctrl->value);
            break;

        case V4L2_CID_EXT_VDEC_DECODING_SPEED:
            //ret = vdec_setSpeed(handle, ctrl->value);
            break;

        case V4L2_CID_EXT_VDEC_DECODE_MODE:
            //ret = vdec_setDecMode(handle, ctrl->value);
            break;

        case V4L2_CID_EXT_VDEC_DISPLAY_DELAY:
            //ret = vdec_setDisplayDelay(handle, ctrl->value);
            break;

        case V4L2_CID_EXT_VDEC_KR_DUAL_3D_MODE:
            //ret = vdec_setDual3D(handle, ctrl->value);
            break;

        case V4L2_CID_EXT_VDEC_VSYNC_THRESHOLD:
            //ret = vdec_setVSyncThreshold(handle, ctrl->value);
            break;

        case V4L2_CID_EXT_VDEC_HFR_TYPE:
            //ret = vdec_setHighFrameRateType(handle, ctrl->value);
            break;

        case V4L2_CID_EXT_VDEC_TEMPORAL_ID_MAX:
            //ret = vdec_setTemporalIDMax(handle, ctrl->value);
            break;

        /* XXX: no implementation on HAL_VDEC side ?! */
        case V4L2_CID_EXT_VDEC_STC_MODE:
        case V4L2_CID_EXT_VDEC_LIPSYNC_MASTER:
        case V4L2_CID_EXT_VDEC_AUDIO_CHANNEL:
        case V4L2_CID_EXT_VDEC_PVR_MODE:
        case V4L2_CID_EXT_VDEC_FAST_IFRAME_MODE:
            ret = 0;
            pr_warning("%s: ignore id 0x%x val %d\n",
                __FUNCTION__, ctrl->id, ctrl->value);
            break;

        default:
            pr_err("%s: unknown id 0x%x val %d\n",
                __FUNCTION__, ctrl->id, ctrl->value);
            return -EINVAL;
    }

    return ret;
}

static int ioctl_streamon(struct file *file, void *fh, enum v4l2_buf_type type)
{
    int ret;
    struct v4l2_fh *vfh;
    venc_handle_t *handle;

    vfh = file->private_data;
    handle = container_of(vfh, venc_handle_t, fh);

    //if (!handle->bChannel || !handle->bSource)
        //return -EPERM;

    //if (!handle->bFilter) {
        //ret = venc_flt_init(handle);
        //if (ret) return ret;
    //}
    pr_info("%s:[%d]\n", __FUNCTION__,__LINE__);

    ret = venc_flt_run(handle);
    //if (ret) return ret;
    pr_info("%s:[%d] ret=%d\n", __FUNCTION__,__LINE__,ret);
	vb2_ioctl_streamon(file, fh, type);

    return 0;
}

static int ioctl_streamoff(struct file *file, void *fh, enum v4l2_buf_type type)
{
    int ret;
    struct v4l2_fh *vfh;
    venc_handle_t *handle;

    vfh = file->private_data;
    handle = container_of(vfh, venc_handle_t, fh);

    if (handle->bFilter) {
        ret = venc_flt_exit(handle);
        if (ret) return ret;
    }
	vb2_ioctl_streamoff(file, fh, type);

    /* reset some settings */
    //handle->HFR_type = 0;

    return 0;
}

static int ioctl_g_ext_ctrls(struct file *file, void *fh, struct v4l2_ext_controls *ctrls)
{
    int ret, i;
    struct v4l2_fh *vfh;
    venc_handle_t *handle;
    struct v4l2_ext_control *ctrl;

    vfh = file->private_data;
    handle = container_of(vfh, venc_handle_t, fh);
    ret = 0;

    for (i=0; !ret && i<ctrls->count; i++) {
        ctrl = &ctrls->controls[i];
        switch (ctrl->id) {
            /*case V4L2_CID_EXT_VDEC_PICINFO_EVENT_DATA:
                mutex_lock(&handle->task_lock);
                //ret = vdec_getPicInfo(handle, ctrl->ptr, ctrl->size);
                mutex_unlock(&handle->task_lock);
                break;
            case V4L2_CID_EXT_VDEC_USER_EVENT_DATA:
                mutex_lock(&handle->task_lock);
                //ret = vdec_getUserData(handle, ctrl->ptr, ctrl->size);
                mutex_unlock(&handle->task_lock);
                break;*/
            case V4L2_CID_EXT_VDEC_STREAM_INFO:
                ret = venc_getStream(handle, ctrl->ptr, ctrl->size);
                break;
            /*case V4L2_CID_EXT_VDEC_DECODER_STATUS:
                //ret = vdec_getDecoderStatus(handle, ctrl->ptr, ctrl->size);
                break;*/
            default:
                ret = -1;
                pr_err("%s: unknown id 0x%x\n", __FUNCTION__, ctrl->id);
                break;
        }
    }

    return ret;
}

static int ioctl_s_ext_ctrls(struct file *file, void *fh, struct v4l2_ext_controls *ctrls)
{

    int ret, i;
    struct v4l2_fh *vfh;
    venc_handle_t *handle;
    struct v4l2_ext_control *ctrl;

    vfh = file->private_data;
    handle = container_of(vfh, venc_handle_t, fh);
    ret = 0;

    for (i=0; !ret && i<ctrls->count; i++) {
        ctrl = &ctrls->controls[i];
        switch (ctrl->id) {
            case V4L2_CID_MPEG_VIDEO_ENCODING:
				ctrl->value=V4L2_MPEG_VIDEO_ENCODING_MPEG_4_AVC;
                break;
				
            case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
				ctrl->value=V4L2_MPEG_VIDEO_H264_PROFILE_MAIN;			
                break;
				
            case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
				ctrl->value=V4L2_MPEG_VIDEO_H264_LEVEL_4_0;
                break;
				
            case V4L2_CID_MPEG_VIDEO_BITRATE:
				ctrl->value=3000;
                break;
				
			case V4L2_CID_MPEG_VIDEO_H264_VUI_EXT_SAR_WIDTH:
				ctrl->value=720;
				break;
				
			case V4L2_CID_MPEG_VIDEO_H264_VUI_EXT_SAR_HEIGHT:
				ctrl->value=480;
				break;

			case V4L2_CID_EXT_VDEC_MEM_ALLOC:
					ret = venc_allocMemory(handle, ctrl->ptr);
					break;
			case V4L2_CID_EXT_VDEC_MEM_FREE:
					ret = venc_releaseMemory(handle, ctrl->ptr);
					break;


            default:
                ret = -1;
                pr_err("%s: unknown id 0x%x\n", __FUNCTION__, ctrl->id);
                break;
        }
    }

    return ret;

    return 0;
}

static int ioctl_subscribe_event(struct v4l2_fh *fh, const struct v4l2_event_subscription *sub)
{
    venc_handle_t *handle;

    handle = container_of(fh, venc_handle_t, fh);
    //if (sub->type == V4L2_EVENT_PRIVATE_EXT_VDEC_EVENT)
        //vdec_setEventFlag(handle, sub->id, 1);

    v4l2_event_subscribe(fh, sub, 0, NULL);

    return 0;
}

static int ioctl_unsubscribe_event(struct v4l2_fh *fh, const struct v4l2_event_subscription *sub)
{
    venc_handle_t *handle;

    handle = container_of(fh, venc_handle_t, fh);
    //if (sub->type == V4L2_EVENT_PRIVATE_EXT_VDEC_EVENT)
      //  vdec_setEventFlag(handle, sub->id, 0);

    v4l2_event_unsubscribe(fh, sub);

    return 0;
}


const struct v4l2_file_operations venc_file_ops = {
    .owner          = THIS_MODULE,
    .open           = fops_open,
    .release        = fops_release,
	.poll			= vb2_fop_poll,
    .unlocked_ioctl = video_ioctl2,
    .write          = fops_write,
    .mmap           = vb2_fop_mmap,
};


const struct v4l2_ioctl_ops venc_ioctl_ops = {
    .vidioc_g_ctrl                  = ioctl_g_ctrl,
    .vidioc_s_ctrl                  = ioctl_s_ctrl,
    .vidioc_streamon                = ioctl_streamon,
    .vidioc_streamoff               = ioctl_streamoff,
    .vidioc_g_ext_ctrls             = ioctl_g_ext_ctrls,
    .vidioc_s_ext_ctrls             = ioctl_s_ext_ctrls,
    .vidioc_subscribe_event         = ioctl_subscribe_event,
    .vidioc_unsubscribe_event       = ioctl_unsubscribe_event,
	//.vidioc_reqbufs 				= ioctl_reqbufs,
	//.vidioc_querybuf				= ioctl_querybuf,
    //.vidioc_dqbuf                   = ioctl_dqbuf,
	//.vidioc_qbuf					= ioctl_qbuf,
	.vidioc_g_fmt_vid_cap = vidioc_g_fmt,
	.vidioc_try_fmt_vid_cap = vidioc_try_fmt,
	.vidioc_s_fmt_vid_cap = vidioc_s_fmt,


	.vidioc_reqbufs	= vb2_ioctl_reqbufs,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_expbuf = vb2_ioctl_expbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
};


