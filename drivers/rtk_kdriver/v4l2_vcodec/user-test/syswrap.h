#ifndef __SYSWRAP_H__
#define __SYSWRAP_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void* syswrap_mmap (uint32_t phyaddr, uint32_t size);
int syswrap_munmap (void *ptr, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif

