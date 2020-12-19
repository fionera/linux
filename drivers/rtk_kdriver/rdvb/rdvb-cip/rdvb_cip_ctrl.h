#ifndef __RDVB_CIP_CTRL_PRIV__
#define __RDVB_CIP_CTRL_PRIV__

#include <tp/tp_drv_global.h>
#include <tp/tp_def.h>

#include "rdvb_cip_dev.h"

#define PID_MAX_NUM_TO_SET    	16
#define CIP_MTP_BUFFER_SIZE   	(6*1024*1024)


typedef struct {
	UINT8 	valid;
	UINT16 	pid;
	void  	*filter;
} RDVB_DMX_PID;

typedef struct {
	unsigned char   count ;
	RDVB_DMX_PID    pidList[PID_MAX_NUM_TO_SET] ;
} CIP_PID_TABLE_T ;

typedef struct {
	UINT8          inited;
	dma_addr_t     phyAddr;
	unsigned char *virAddr;
	unsigned char *cachedaddr;     /* cached addresss for release*/
	unsigned char *nonCachedaddr;
	unsigned int   size;
	unsigned int   allocSize;
	unsigned char  isCache;
	unsigned char  fromPoll;
	long           phyAddrOffset;
} CIP_BUF_T;

typedef struct _tpp_priv_data {
	struct rdvb_cip_tpp tpp;

	TPK_TPP_ENGINE_T tpp_id;
	TPK_TP_MTP_T mtp_id;

	TPK_TP_SOURCE_T tpp_src;
		
	TPK_TPP_STREAM_CTRL_T tpp_stream_ctrl;
	TPK_TPP_STREAM_CTRL_T tpp_mtp_stream_ctrl;
	
	struct ca_ext_source ca_src;
	
	CIP_PID_TABLE_T pidTable;

	CIP_BUF_T mtpBuffer;
	CIP_BUF_T bypassBuffer;

	UINT8 tpp_pid_en;
	UINT8 filter_num;
	UINT8 syncbyte;
	UINT8 tsPacketSize;
	
}tpp_priv_data;


extern tpp_priv_data* rdvb_get_cip(int index);

#endif
