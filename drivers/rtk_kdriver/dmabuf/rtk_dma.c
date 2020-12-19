///////////////////////////////////////////////////////////////////////////////
// Copyright 2019 Realtek Semiconductor Corp.
///////////////////////////////////////////////////////////////////////////////

#include <generated/autoconf.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pageremap.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/sort.h>
#include <linux/stat.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#include <rpc_common.h>

#include <rtk_kdriver/rtk_dma.h>
#include <linux/dma-mapping.h> /* dma_alloc_coherent */

//////////////////////////////////////////////////////////////////////////////
// internal structures
///////////////////////////////////////////////////////////////////////////////

#define MAX_CLOCK_COUNT (4)
#define INVALID_GROUP_ID (-1U)
#define REFCLOCK_SIZE 4096
#define ENABLE_DMA

#define SHARED_SIZE	PAGE_ALIGN(sizeof(rtk_dmadev_handle_t))

typedef struct _rtk_dmadev_client_t {
    unsigned int group;
} rtk_dmadev_client_t;

typedef struct _rtk_dmadev_handle_t {
    void *dma_addr;
    ulong phyaddr;
    int fd;
    unsigned int group;
    unsigned int num_client;
    uint64_t time;
} __attribute__((__aligned__(256))) rtk_dmadev_handle_t;

typedef struct _rtk_dmadev_context_t {
    rtk_dmadev_handle_t handles[MAX_CLOCK_COUNT];
} rtk_dmadev_context_t;

static DEFINE_MUTEX(_device_lock);
static rtk_dmadev_context_t *_device_context = NULL;

__attribute__((unused)) static uint64_t rtk_dmadev_get_monotonic_time(void) {
    struct timespec ts;
    getrawmonotonic(&ts);
    return (1000000ULL * ts.tv_sec) + (ts.tv_nsec / 1000);
}

__attribute__((unused)) static int rtk_dmadev_handle_compare(const void *lhs, const void *rhs) {
    rtk_dmadev_handle_t *l = *((rtk_dmadev_handle_t **)lhs);
    rtk_dmadev_handle_t *r = *((rtk_dmadev_handle_t **)rhs);

    if (l->time < r->time) {
        return -1;
    } else if (l->time > r->time) {
        return 1;
    } else {
        return 0;
    }
}

static struct sg_table *rtk_map_dma_buf(struct dma_buf_attachment *attachment,
                                        enum dma_data_direction direction)
{
    pr_info ("[dma:%d] %s\n", __LINE__, __func__);
    return NULL;
}

static void rtk_unmap_dma_buf(struct dma_buf_attachment *attachment,
                              struct sg_table *table,
                              enum dma_data_direction direction)
{
    pr_info ("[dma:%d] %s\n", __LINE__, __func__);
}

static int rtk_dma_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
    rtk_dmadev_handle_t *handle = dmabuf->priv;
    struct file *file = dmabuf->file;
    int ret = 0;

    if ((file->f_flags & O_SYNC)) {
        pr_info("[dma:%d] set noncached flag %x \n", __LINE__, file->f_flags);
        vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
    }

    mutex_lock(&dmabuf->lock);
    ret = dma_mmap_coherent(NULL, vma, handle->dma_addr, handle->phyaddr, dmabuf->size);
    mutex_unlock(&dmabuf->lock);

    if (ret)
        pr_err("%s: failure mapping buffer to userspace\n",
               __func__);

    return ret;
}

static int rtk_dma_buf_attach(struct dma_buf *dmabuf, struct device *dev,
                              struct dma_buf_attachment *attachment)
{
    pr_info ("[dma:%d] %s %p ref:%d\n", __LINE__, __func__,
             dmabuf, atomic_read(&dmabuf->file->f_count));

    return 0;
}

static void rtk_dma_buf_detach(struct dma_buf *dmabuf,
                               struct dma_buf_attachment *attachment)
{
    if (WARN_ON(!dmabuf || !attachment))
        return;

    pr_info ("[dma:%d] %s %p ref:%d\n", __LINE__, __func__,
                  dmabuf, atomic_read(&dmabuf->file->f_count));
}

static void rtk_dma_buf_release(struct dma_buf *dmabuf)
{
    rtk_dmadev_handle_t *handle;
    handle = (rtk_dmadev_handle_t*) dmabuf->priv;
    pr_info ("[dma:%d] %s %p ref:%d\n", __LINE__, __func__,
             dmabuf, atomic_read(&dmabuf->file->f_count));

    if (handle) {
        if (handle->dma_addr) {
            pr_warning("[dma:%d] dvr_free fd:%d, va:%p pa: 0x%08lx! \n",
                    __LINE__, handle->fd, handle->dma_addr, handle->phyaddr);
            dvr_free(handle->dma_addr);
            handle->dma_addr = NULL;
        }
        handle->fd = -1U;
        handle->group = INVALID_GROUP_ID;
        handle->num_client = 0U;
        handle->time = 0U;
    } else {
        pr_warning("[dma:%d] dmabuf priv is NULL \n", __LINE__);
    }
}

static int rtk_dma_buf_begin_cpu_access(struct dma_buf *dmabuf, size_t start,
                                        size_t len,
                                        enum dma_data_direction direction)
{
    pr_info ("[dma:%d] %s\n", __LINE__, __func__);
    return 0;
}

static void rtk_dma_buf_end_cpu_access(struct dma_buf *dmabuf, size_t start,
                                       size_t len,
                                       enum dma_data_direction direction)
{
    pr_info ("[dma:%d] %s\n", __LINE__, __func__);
}

static void *rtk_dma_buf_kmap(struct dma_buf *dmabuf, unsigned long offset)
{
    pr_info ("[dma:%d] %s\n", __LINE__, __func__);
    return dmabuf->priv + offset * PAGE_SIZE;
}

static void rtk_dma_buf_kunmap(struct dma_buf *dmabuf, unsigned long offset,
                               void *ptr)
{
    pr_info ("[dma:%d] %s\n", __LINE__, __func__);
}

static struct dma_buf_ops dma_buf_ops = {
        .map_dma_buf = rtk_map_dma_buf,
        .unmap_dma_buf = rtk_unmap_dma_buf,
        .mmap = rtk_dma_mmap,
        .attach = rtk_dma_buf_attach,
        .detach = rtk_dma_buf_detach,
        .release = rtk_dma_buf_release,
        .begin_cpu_access = rtk_dma_buf_begin_cpu_access,
        .end_cpu_access = rtk_dma_buf_end_cpu_access,
        .kmap_atomic = rtk_dma_buf_kmap,
        .kunmap_atomic = rtk_dma_buf_kunmap,
        .kmap = rtk_dma_buf_kmap,
        .kunmap = rtk_dma_buf_kunmap,
};

static long rtk_dmadev_dma_acquire_lock(rtk_dmadev_client_t *client,
                                        rtk_dmadev_request_t *req) {
    unsigned int i = 0;
    rtk_dmadev_handle_t *handle = NULL;
    rtk_dmadev_acquire_t *param = NULL;
    struct dma_buf *dmabuf;
    DEFINE_DMA_BUF_EXPORT_INFO(exp_info);

    if (WARN_ON(!_device_context || !req))
        return -ENXIO;

    param = &req->data.dma_acquire;

    for (i = 0; i < MAX_CLOCK_COUNT; i++) {
        // start allocate buffer
        if (_device_context->handles[i].dma_addr == NULL) {
            handle = &_device_context->handles[i];
            handle->dma_addr = (void *) dvr_malloc_specific(param->size, GFP_DCU1);
            if (!handle->dma_addr) {
                goto error_refdma_acquire;
            }
            memset (handle->dma_addr, 0, param->size);
            handle->phyaddr = dvr_to_phys(handle->dma_addr);
            if (!handle->phyaddr) {
                goto error_refdma_acquire;
            }
            handle->group = INVALID_GROUP_ID;
            handle->num_client = 0U;
            handle->time = 0U;
            break;
        }
        if (i == MAX_CLOCK_COUNT - 1) {
            goto error_refdma_acquire;
        }
    }

    exp_info.ops   = &dma_buf_ops;
    exp_info.size  = param->size;
    exp_info.flags = param->flags | O_RDWR;
    exp_info.priv  = handle;

    dmabuf = dma_buf_export(&exp_info);
    if (IS_ERR(dmabuf)) {
        pr_err ("[dma:%d] buf export err\n", __LINE__);
        goto error_refdma_acquire;
    }

    handle->fd = dma_buf_fd(dmabuf, O_CLOEXEC);
    if (handle->fd < 0) {
        dma_buf_put(dmabuf);
        req->status = RTK_DMADEV_STATUS_ERROR;
        pr_err("[dma:%d] dma_buf_fd error \n", __LINE__);
    } else {
        param->fd = handle->fd;
        dmabuf->file->f_flags |= param->flags;
        client->group  = param->group;
        handle->group  = param->group;
        handle->num_client++;
        param->phyaddr = handle->phyaddr;
        req->status = RTK_DMADEV_STATUS_OK;

        pr_info("[dma:%d] %p fd %d sz %d ref %d\n", __LINE__, dmabuf,
                handle->fd, dmabuf->size, atomic_read(&dmabuf->file->f_count));
        pr_info("[dma:%d] va: %p pa: 0x%08lx flags %x %x\n", __LINE__,
                 handle->dma_addr, handle->phyaddr, param->flags, dmabuf->file->f_flags);
    }

    return 0;

error_refdma_acquire:
    if (handle && handle->dma_addr) {
        dvr_free(handle->dma_addr);
    }
    pr_err ("[dma:%d] allocate fail\n", __LINE__);

    return -ENOMEM;
}

static void rtk_dmadev_dma_release_lock(rtk_dmadev_client_t *client,
                                        rtk_dmadev_request_t *req) {
    rtk_dmadev_acquire_t *param;
    pr_info ("[dma:%d] %s \n", __LINE__, __func__);

    if (WARN_ON(!_device_context || !req))
        return;

    param = &req->data.dma_acquire;
    param->fd = -1;
    req->status = RTK_DMADEV_STATUS_OK;
}

static bool rtk_dmadev_client_create(struct file *filp) {
    rtk_dmadev_client_t *client = NULL;
    client = kzalloc(sizeof(*client), GFP_KERNEL);
    if (client) {
        client->group = INVALID_GROUP_ID;
    }
    filp->private_data = client;

    return client ? true : false;
}


static void rtk_dmadev_client_release(struct file *filp) {
    rtk_dmadev_client_t *client = NULL;
    if (filp && filp->private_data) {
        client = (rtk_dmadev_client_t *)filp->private_data;
        pr_info ("[dma:%d] %s -> kfree (%d) \n", __LINE__, __func__, client->group);
        kfree(client);
        filp->private_data = NULL;
    }
}

#if 0
static void rtk_dmadev_context_finalize_l(void) {
  size_t i;
  if (_device_context) {
    for (i = 0; i < MAX_CLOCK_COUNT; i++) {
      rtk_dmadev_handle_t *handle = &_device_context->handles[i];
      if (handle->dma_addr) {
        dvr_free(handle->dma_addr);
        pr_info("[dma:%d] finalize dma_addr %u %08lx\n", __LINE__, i, handle->phyaddr);
        handle->dma_addr = NULL;
      }
    }
    pr_info("[dma:%d] finalize context %p\n", __LINE__, _device_context);
    kfree((void *)_device_context);
    _device_context = NULL;
  }
}
#endif

static int rtk_dmadev_context_create(void) {
    size_t i;

    mutex_lock(&_device_lock);
    _device_context = kzalloc(sizeof(*_device_context), GFP_KERNEL);
    if (!_device_context) {
        pr_err("[dma:%d] failure to create context", __LINE__);
        goto error_create_device_context;
    }
    for (i = 0; i < MAX_CLOCK_COUNT; i++) {
        rtk_dmadev_handle_t *handle = &_device_context->handles[i];
        handle->dma_addr = NULL;
        handle->phyaddr = 0;
        handle->group = INVALID_GROUP_ID;
        handle->num_client = 0U;
        handle->time = 0U;
    }
    mutex_unlock(&_device_lock);

    return 0;

error_create_device_context:

    mutex_unlock(&_device_lock);

    return -ENOMEM;
}

#if 0
static void rtk_dmadev_context_finalize(void) {
    mutex_lock(&_device_lock);
    rtk_dmadev_context_finalize_l();
    mutex_unlock(&_device_lock);
}
#endif

static long _rtk_dmadev_ioctl_dma_acquire(rtk_dmadev_client_t *client,
                                            rtk_dmadev_request_t *req) {
    long ret = 0;
    mutex_lock(&_device_lock);
    ret = rtk_dmadev_dma_acquire_lock(client, req);
    mutex_unlock(&_device_lock);
    return ret;
}

static long _rtk_dmadev_ioctl_dma_release(rtk_dmadev_client_t *client,
                                            rtk_dmadev_request_t *req) {
    mutex_lock(&_device_lock);
    rtk_dmadev_dma_release_lock(client, req);
    mutex_unlock(&_device_lock);
    return 0;
}

static long _rtk_dmadev_ioctl_unknown(rtk_dmadev_client_t *client,
                                      rtk_dmadev_request_t *req) {
    req->status= RTK_DMADEV_STATUS_INVALID_CMD;
    pr_err("[dma] ioctl    : invalid command %d\n", req->cmd);
    return 0;
}

static long _rtk_dmadev_unlocked_ioctl(struct file *filp, unsigned int cmd,
                                       unsigned long arg) {
    long ret = 0;
    rtk_dmadev_client_t *client = (rtk_dmadev_client_t *)filp->private_data;
    rtk_dmadev_request_t req;

    if (cmd == RTK_DMADEC_IOC_REQUEST) {
        if (copy_from_user(&req, (void __user *)arg, sizeof(req))) {
            return -EFAULT;
        }
        switch (req.cmd) {
            case RTK_DMADEV_CMD_DMABUF_ACQUIRE:
                ret = _rtk_dmadev_ioctl_dma_acquire(client, &req);
                break;
            case RTK_DMADEV_CMD_DMABUF_RELEASE:
                ret = _rtk_dmadev_ioctl_dma_release(client, &req);
                break;
            default:
                ret = _rtk_dmadev_ioctl_unknown(client, &req);
                break;
        }
        if (!ret) {
            if (copy_to_user((void __user *)arg, &req, sizeof(req))) {
                return -EFAULT;
            }
        } else {
            pr_warning("[dma] ioctl    : %p command %d, ret %ld\n", client, req.cmd,
                 ret);
        }
    }
    return ret;
}


static int rtk_dmadev_open(struct inode *inode, struct file *filp) {
    if (!rtk_dmadev_client_create(filp)) {
        return -ENOMEM;
    }
    return 0;
}

static int rtk_dmadev_release(struct inode *inode, struct file *filp) {
    rtk_dmadev_client_release(filp);
    return 0;
}

static const struct file_operations rtk_dmadev_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = _rtk_dmadev_unlocked_ioctl,
    .open = rtk_dmadev_open,
    .release = rtk_dmadev_release,
};

static struct miscdevice rtk_dmadev_miscdev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "rtkdma",
    .fops = &rtk_dmadev_fops,
    .mode = 0666,
};

static int __init rtk_dmadev_init(void) {
    int ret = 0;

    ret = rtk_dmadev_context_create();
    if (ret) {
        pr_err("[dma] failure to allocate context\n");
        return ret;
    }
    ret = misc_register(&rtk_dmadev_miscdev);
    if (ret) {
        pr_err("[dma] failure to misc_register\n");
        return ret;
    }

    pr_info("[dma] init\n");

    return ret;
}

static void __exit rtk_dmadev_exit(void) {

    misc_deregister(&rtk_dmadev_miscdev);
#if 0
    rtk_dmadev_context_finalize();
#endif
    pr_info("[dma] exit\n");
}

module_init(rtk_dmadev_init);
module_exit(rtk_dmadev_exit);
MODULE_LICENSE("GPL");
