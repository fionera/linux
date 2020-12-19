#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/file.h>
#include <asm/io.h>

#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>

#pragma scalar_storage_order big-endian
#include <rpc_common.h>
#include <InbandAPI.h>
#include <VideoRPC_System.h>
#pragma scalar_storage_order default

#include "venc_conf.h"
//#include "venc_def.h"
#include "venc_struct.h"
#include "venc_test.h"

int venc_emulateSource(venc_handle_t *handle)
{
    venc_source_t *src;

    if (!handle )
        return -1;

    src = &handle->src;

    src->enc_bs_hdr_addr = ENC_BS_HDR_ADDR;
    src->enc_bs_buf_addr = ENC_BS_BUF_ADDR;
    src->enc_bs_buf_size = ENC_BS_BUF_SIZE;

    src->enc_msg_hdr_addr = ENC_MSG_HDR_ADDR;
    src->enc_msg_buf_addr = ENC_MSG_BUF_ADDR;
    src->enc_msg_buf_size = ENC_MSG_BUF_SIZE;

    src->din_bs_hdr_addr = DIN_BS_HDR_ADDR;
    src->din_bs_buf_addr = DIN_BS_BUF_ADDR;
    src->din_bs_buf_size = DIN_BS_BUF_SIZE;

    src->din_ib_hdr_addr = DIN_IB_HDR_ADDR;
    src->din_ib_buf_addr = DIN_IB_BUF_ADDR;
    src->din_ib_buf_size = DIN_IB_BUF_SIZE;

    //handle->bSource = 1;

    //pr_info("%s:\n", __FUNCTION__);

    return 0;
}

