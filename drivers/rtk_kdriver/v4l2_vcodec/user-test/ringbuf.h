#ifndef __RINGBUF_H__
#define __RINGBUF_H__

#ifdef __cplusplus
extern "C" {
#endif

int ringbuf_initHdr(unsigned int rbh, int type, unsigned int base, int size);
int ringbuf_open(unsigned int rbh, void **ppHandle);
int ringbuf_close(void *pHandle);
int ringbuf_write(void *pHandle, void *data, int size);
int ringbuf_read(void *pHandle, void *data, int size);

int ringbuf_getWr(void *pHandle, unsigned int *res);
int ringbuf_getRd(void *pHandle, unsigned int *res);

#ifdef __cplusplus
}
#endif

#endif

