#ifndef _AUDIO_PROCESS_NODE_H_
#define _AUDIO_PROCESS_NODE_H_

#include "audio_pin.h"
#include "AudioRPC_System.h"
#include "audio_process.h"
#ifdef AC4DEC
#ifdef __cplusplus
extern "C"
{
#endif
#include <tee_client_api.h>
#ifdef __cplusplus
}
#endif
#endif
#include "node_handle.h"

//#define AC4_CH_DEBUG
//#define DEBUG_DURATION

#define DECODER_INFO_CH             0
#define DECODER_INFO_CH_CNT         1
#define DECODER_INPIN_PAYLOAD_SIZE  32768
#define DECODER_INDATA_CH_CNT       1
#define ENCODER_INDATA_CH_CNT       8
#define DECODER_OUTPIN_PAYLOAD_SIZE 16384
#define DECODER_OUTDATA_CH_CNT      8
#define ENCODER_OUTDATA_CH_CNT      1
#define DECODER_INTERNAL_MEM        1048576   // 1024*1024
#define DECODER_OUTTEMP_BUF_SIZE    1536      // samples
#define DECODER_PCM_DATA_SIZE       8192

#define CLOCK_RATE                  1099// 1099MHZ by cpu

void bs_cache_flushed(Pin* pPinObj, UINT32 readptr, UINT32 size);
void UninitialNode(node_handle h_node);
void calculate_kcps(node_handle h_node, UINT32 dur);
void FwFlushCmd(node_handle h_node);
int NodeInputTask(node_handle h_node);
int NodeOutputTask(node_handle h_node);

node_handle CreateNode(SYSTEM_MESSAGE_INFO *info);
int udc_node_create(node_handle h_node, long data[16]);
int ac4_node_create(node_handle h_node, long data[16]);
int heaacdec_node_create(node_handle h_node, long data[16]);
int matdec_node_create(node_handle h_node, long data[16]);
int opusdec_node_create(node_handle h_node, long data[16]);
int file_writer_create(node_handle h_node, long data[29]);
int audio_manager_node_create(node_handle h_node, long data[29]);
void ddpenc_node_create(node_handle h_node, long data[29]);
void aacenc_node_create(node_handle h_node, long data[29]);
void fdkaacdec_node_create(node_handle h_node, long data[29]);
int dtsdec_node_create(node_handle h_node, long data[16]);

long RegisterNodeAndGetNodeID(node_handle h_node, struct list_head *head);
long UnRegisterNode(node_handle h_node, struct list_head *head);
long GetPacketPinBufHeader(Pin* pPin);
node_handle GetNodeByNodeID(struct list_head *head, long nodeID);
#endif
