#ifndef __V4L2_VENC_CORE_H__
#define __V4L2_VENC_CORE_H__


int venc_initHandle(venc_handle_t *handle);
int venc_exitHandle(venc_handle_t *handle);

int venc_flt_init(venc_handle_t *handle);
int venc_flt_sendCmd(venc_handle_t *handle, int cmd);
int venc_flt_run(venc_handle_t *handle);
int venc_flt_pause(venc_handle_t *handle);
int venc_flt_flush(venc_handle_t *handle);
int venc_flt_stop(venc_handle_t *handle);
int venc_flt_destroy(venc_handle_t *handle);
int venc_flt_exit(venc_handle_t *handle);
int venc_getPermission(venc_handle_t *handle, phys_addr_t addr, int size);
int venc_setSource(venc_handle_t *handle, int port);
int venc_getStream(venc_handle_t *handle, void *userPtr, int size);
int venc_setFormat(struct v4l2_pix_format_mplane *mp, u32 pixfmt, int picW, int picH);
int venc_streamon(venc_handle_t *handle);
int venc_streamoff(venc_handle_t *handle);


#endif
