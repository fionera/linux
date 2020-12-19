#ifndef NODE_HANDLE_H_
#define NODE_HANDLE_H_

typedef struct node_s* node_handle;
typedef int (*node_activate_func_ptr)(node_handle h_node);
typedef int (*node_destroy_func_ptr)(node_handle h_node);
typedef int (*node_initial_func_ptr)(node_handle h_node);
typedef int (*node_setting_func_ptr)(node_handle h_node);
typedef int (*node_drc_setting_func_ptr)(node_handle h_node, int32_t drc_mode);

typedef struct node_s
{
#if defined(ANDROID) //Ronald debug
    struct list_head list;
#else
    struct list_head list;
#endif
    long nodeID;
    char name[64];
    int  nodeType;
    int  bInpinPayloadCached;
    node_activate_func_ptr activate;
    node_destroy_func_ptr destroy;
    node_initial_func_ptr initial;
    node_setting_func_ptr setting;
    node_drc_setting_func_ptr drc_setting;
    SYS_PROCESS_PAYLOAD_HEADER plh_out;
    SYS_PROCESS_PAYLOAD_HEADER plh_in;
    Pin *inPktPin;
    Pin *outPktPin;
    Pin *inPktPin_debug;
    unsigned char* PrevDebugCheckRp;
    UINT32 inBufSize;
    UINT32 outBufSize;
    UINT32 inPinCnt;
    UINT32 outPinCnt;
    int eos_cnt;
    void *memory;
    void *reserveBuf[8];
    UINT32 flushToNewPtr;
    int32_t subStreamId;
    int32_t initialData;
    void* cmdMetadata;
    int   cmdMetadataSize;
    void* pcmdata[32];
    UINT32 avg_mcps;
    int activate_ex_ms;   // activate excution time in ms
    int threadSleepMs;
#if defined(AC4DEC)
    AUDIO_AC4_UI_SETTING ui_set;
    TEEC_Session ta_sess;
    TEEC_Context ta_ctx;
    int ta_ignore;        // if 1, ignore ac4 ta check
#ifdef AC4DEC_TA
    UINT32       ta_info_shmva;
    UINT32       ta_pin_shmsz;
    UINT32       ta_pin_shmva;
    UINT32       *ta_pin_shmva_noncache;
    UINT32       ta_pin_shmpa;
    UINT32       ta_info[16];
    //FILE*        fp_bs;
#endif
#endif

#ifdef ARM_MPEGHDEC_ENABLE
    AUDIO_MPEGH_UI_INFO mpegh_ui_info;
#endif

} Node_Struct;

#endif
