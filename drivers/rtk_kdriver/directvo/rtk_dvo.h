#ifndef _DVO_H_
#define _DVO_H_

#include "base_types.h"
#include <rbus/sys_reg_reg.h>
#include <rbus/pll27x_reg_reg.h>
#include <rbus/pll_reg_reg.h>
#include <rbus/vodma_reg.h>
#include <rbus/vodma2_reg.h>
//#include <rbus/sensio_reg.h>
#include <rbus/dc_sys_reg.h>
//#include <rbus/dc2_sys_reg.h>
#include <rbus/osdovl_reg.h>
#include <rbus/ppoverlay_reg.h>

#define RTK_DVO_NAME		"rtkdvo"

#ifndef RTK_DVO_MAJOR
#define RTK_DVO_MAJOR 0 /* dynamic major by default */
#endif

typedef enum _DVO_LOG_LEVEL {
    DVO_LOG_EMESG = 0,
    DVO_LOG_ALERT,
    DVO_LOG_CRIT,
    DVO_LOG_ERROR,
    DVO_LOG_WARN,
    DVO_LOG_NOTICE,
    DVO_LOG_INFO,
    DVO_LOG_DEBUG,
    DVO_LOG_TRACE,
    DVO_LOG_MAX
} DVO_LOG_LEVEL;

typedef enum _DVO_BUF_TYPE {
	DVO_TYPE_NONE = 0,
	DVO_TYPE_PR2_POLLING,
	DVO_TYPE_PR2_RPC,
	DVO_TYPE_PR3_POLLING,
	DVO_TYPE_PR3_RPC,
	DVO_TYPE_WIDEVINE,
	DVO_TYPE_DRM,
	DVO_TYPE_SSTORE,
	DVO_TYPE_SKB,
	DVO_TYPE_SHARE_MEMORY,//need not to notify KCPU in kill func
	DVO_TYPE_MAX
} DVO_BUF_TYPE;

/* Use 't' as magic number */
#define DVO_IOC_MAGIC  't'
#define DVO_IOC_INIT       _IO  (DVO_IOC_MAGIC, 1)
#define DVO_IOC_DEINIT     _IO  (DVO_IOC_MAGIC, 2)
#define DVO_IOC_MAXNR            3

#define ENABLE_DUMP_FRAME_INFO

#define S_OK		0x10000000
#define S_FAIL		0x10000001

//==============================================================================


typedef enum {
	DVO_STATE_INIT = 0x00,
	DVO_STATE_OPEN,
	DVO_STATE_DISPLAY
} DVO_STATE_T;

#ifdef ENABLE_DUMP_FRAME_INFO
  /* dump VO CRC info to file */
  #ifndef FILE_NAME_SIZE
  #define FILE_NAME_SIZE 128
  #define DUMP_ES_SIZE 2*1024*1024
  #endif

  typedef struct {
        unsigned long   phy_addr[32];
        unsigned int    buffer_size[32];
  } DVO_DATA;

  typedef struct DVO_MALLOC_STRUCT {
        unsigned long Memory;
        unsigned long PhyAddr;
        unsigned long VirtAddr;
  } DVO_MALLOC_STRUCT;

  typedef struct DVO_DUMP_STRUCT {
        unsigned char file_name[FILE_NAME_SIZE];    /* debug log file place & name */
        unsigned int  mem_size; 	/* debug memory size */
        unsigned char enable;
  } DVO_DUMP_STRUCT;

  struct DVO_RPC_DEBUG_MEMORY {
        int videoFirmwareVersion;
  };
  typedef struct DVO_RPC_DEBUG_MEMORY DVO_RPC_DEBUG_MEMORY;

  typedef struct DVO_DUMP_BUFFER_HEADER {
        unsigned int magic;
        unsigned int size;
        unsigned int rd;
        unsigned int wr;
  } DVO_DUMP_BUFFER_HEADER;
  // -------------------------------
#endif

unsigned char get_voinfo_flag(void);

#endif
