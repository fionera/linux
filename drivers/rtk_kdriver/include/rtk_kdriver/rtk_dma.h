///////////////////////////////////////////////////////////////////////////////
// Copyright 2017 Realtek Semiconductor Corp.
///////////////////////////////////////////////////////////////////////////////

#ifndef __REALTEK_3413F315_02CC_4EE3_9F39_E68DF4D75C6F_H__
#define __REALTEK_3413F315_02CC_4EE3_9F39_E68DF4D75C6F_H__

#include <linux/dma-buf.h>

typedef enum _rtk_dmadev_cmd_t {
  RTK_DMADEV_CMD_NONE,
  RTK_DMADEV_CMD_DMABUF_ACQUIRE,
  RTK_DMADEV_CMD_DMABUF_RELEASE,
} rtk_dmadev_cmd_t;

typedef enum _rtk_dmadev_status_t {
  RTK_DMADEV_STATUS_OK,
  RTK_DMADEV_STATUS_ERROR,
  RTK_DMADEV_STATUS_INVALID_CMD,
} rtk_dmadev_status_t;

typedef struct _rtk_dmadev_acquire_t {
  unsigned int group;
  unsigned long phyaddr;
  size_t size;
  int flags;
  int fd;
} rtk_dmadev_acquire_t;

typedef union _rtk_dmadev_data_t {
  rtk_dmadev_acquire_t dma_acquire;
} rtk_dmadev_data_t;

typedef struct _rtk_dmadev_request_t {
  rtk_dmadev_cmd_t cmd;
  rtk_dmadev_status_t status;
  rtk_dmadev_data_t data;
} rtk_dmadev_request_t;

/* extra references appended to driver object for registered */
typedef struct _rtk_dmadev_dmabuf_ref {
     struct dma_buf *dmabuf;
     struct dma_buf_attachment *attach;
     struct sg_table *sgt;
} rtk_dmadev_dmabuf_ref_t;

#define RTK_DMADEC_PROC_DIR "rtkdma"
#define RTK_DMADEC_DEVICE_PATH "/dev/rtkdma"
#define RTK_DMADEC_IOC_MAGIC 'o'
#define RTK_DMADEC_IOC_REQUEST \
  _IOWR(RTK_DMADEC_IOC_MAGIC, 1, rtk_dmadev_request_t)

#endif /* __REALTEK_3413F315_02CC_4EE3_9F39_E68DF4D75C6F_H__ */
