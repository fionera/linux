#ifndef __V4L2_VENC_STRUCT_H__
#define __V4L2_VENC_STRUCT_H__

#include <media/videobuf2-v4l2.h>
#include "vutils/vfifo.h"
#include "vcodec_struct.h"
/*
typedef struct venc_device_t {
    struct platform_device *pdev;
    struct device *dev;
    struct v4l2_device v4l2_dev;
    struct video_device vdev;
    struct list_head list;
    struct list_head handle_list;
    struct mutex lock;
    int port;
} venc_device_t;
*/
typedef struct venc_pixfmt_t {
	u32 pixelformat;
	u32 num_planes;
} venc_pixfmt_t;


typedef struct venc_buffer_t {
	struct vb2_v4l2_buffer vb;
	struct list_head list;
} venc_buffer_t;


typedef struct venc_source_t {
    int enc_type;
    phys_addr_t enc_bs_hdr_addr;
    phys_addr_t enc_bs_buf_addr;
    int         enc_bs_buf_size;

    phys_addr_t enc_msg_hdr_addr;
    phys_addr_t enc_msg_buf_addr;
    int         enc_msg_buf_size;

    phys_addr_t din_bs_hdr_addr;
    phys_addr_t din_bs_buf_addr;
    int 		din_bs_buf_size;

    phys_addr_t din_ib_hdr_addr;
    phys_addr_t din_ib_buf_addr;
    int 		din_ib_buf_size;


} venc_source_t;



typedef struct venc_handle_t {

    struct v4l2_fh fh;
    struct list_head list;
    vcodec_device_t *device;
    int userID;

    struct v4l2_pix_format_mplane pixfmt;
    struct vb2_alloc_ctx *alloc_ctx[VIDEO_MAX_PLANES];


    /* debug related */
    int debug[16];

    /* NOTE: clear all data from __reset_start to __reset_end during reset */
    char __reset_start[0];

    unsigned int bFilter:1;

    /* source related */
    venc_source_t src;

    /* stat */
    unsigned int nPicInfo;
    unsigned int nPicInfoIn;
    unsigned int nPicInfoOut;
    unsigned int nFrmInfo;
    unsigned int nEncBsData;

    /* subscribed events */
    struct {
        unsigned int bFrmInfo:1;
        unsigned int bPicInfo:1;
        unsigned int bUserData:1;
    } event;


    int channel;


    /* filter IDs */
    unsigned int flt_enc;

    /* ringbuf related */
    vfifo_t msgQ;
    vfifo_t userDataQ;
    vfifo_t bitstreamQ;

    /* venc ringbuf related */
    vfifo_t enc_bs;
    vfifo_t enc_msg;
    vfifo_t din_bs;
    vfifo_t din_cmd;

	struct vb2_queue vq_dst;
	struct list_head buf_list;

	/* thread related */
	struct mutex thread_lock;
	struct task_struct *thread;
	int thread_msecs;


    /* NOTE: clear all data from __reset_start to __reset_end during reset */
    char __reset_end[0];

} venc_handle_t;


typedef struct VENC_MSG_TYPE
{
	unsigned int	channel;	/**< Port Id */
	unsigned char	pictype;	/**< Picture Type IDC:1 I:2 P:4 B:8 */
	unsigned long	pts;		/**< PTS */
	unsigned char	*pData;		/**< pointer to Video Data */
	unsigned int	dataLen;	/**< Video Data Length */
	unsigned char	*pRStart;	/**< start pointer of buffer */
	unsigned char	*pREnd;		/**< end pointer of buffer */
}VENC_MSG_TYPE_T;

#endif
