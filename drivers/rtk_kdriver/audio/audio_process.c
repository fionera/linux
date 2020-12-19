/******************************************************************************
 *
 *   Copyright(c) 2016 Realtek Semiconductor Corp. All rights reserved.
 *
 *   @author yungjui.lee@realtek.com
 *
 *****************************************************************************/

#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/freezer.h>

#include "audio_process.h"
#include "audio_process_node.h"
#include "AudioInbandAPI.h"
#include "AudioRPC_System.h"
#include "audio_inc.h"

#include "audio_base.h"

#include "audio_rpc.h"

#define MULTI_THREAD

#if defined(USE_INBAND_SYSPROC_COMMUNICATION)
node_handle g_Audio_Manager_node;
#endif

static struct task_struct *rtkaudio_manager_tsk;
static struct task_struct *rtkaudio_process_tsk;
static struct task_struct *rtkaudio_audio_process_tsk;
static DEFINE_SEMAPHORE(process_list_sem);

static struct list_head process_list = LIST_HEAD_INIT(process_list);
/*static osal_sem_t process_list_sem;*/

#if 0
static void thread_sched_attr(int policy, struct sched_param *param)
{
    INFO("policy=%s, priority=%d\n",
        (policy == SCHED_FIFO)  ? "SCHED_FIFO" :
        (policy == SCHED_RR)    ? "SCHED_RR" :
        (policy == SCHED_OTHER) ? "SCHED_OTHER" :
        "???",
        param->sched_priority);
}

static void show_audio_process_thread_sched_attr(int priority_offset)
{
    int policy, s;
    struct sched_param param;

    /*
    s = pthread_getschedparam(pthread_self(), &policy, &param);
    if (s != 0)
    {
        MY_LIBK_INFO("%s get thread policy fail\n", __FUNCTION__);
        return;
    }
    */

    //thread_sched_attr(policy, &param);

    //if(policy != SCHED_RR) {
       int max_prio = sched_get_priority_max(SCHED_RR);
       if (priority_offset > max_prio)
       {
           MY_LIBK_INFO("%s user set thread priority error. max:%d, user:%d", __FUNCTION__, max_prio, priority_offset);
           return;
       }
       param.sched_priority = max_prio - priority_offset;
       s = pthread_setschedparam(pthread_self(), (int)SCHED_RR, &param);
       if (s != 0)
       {
           MY_LIBK_INFO("%s set thread policy fail\n", __FUNCTION__);
           return;
       }
    s = pthread_getschedparam(pthread_self(), &policy, &param);
    if (s != 0)
    {
        MY_LIBK_INFO("%s get thread policy fail\n", __FUNCTION__);
    }
    thread_sched_attr(policy, &param);
    //}
    //if(policy != SCHED_FIFO) {
    //    int max_prio = sched_get_priority_max(SCHED_FIFO);
    //    param.sched_priority = max_prio;
    //    pthread_setschedparam(pthread_self(), (int)SCHED_FIFO, &param);
    //}
}
#endif

#ifdef MULTI_THREAD

int AudioNodeThread(void *data)
{
    node_handle h_node = (node_handle)data;
    int res;
    int64_t start_pts = 0, duration_pts = 0;
#if 0
    int64_t start_ms = 0;
    char th_name[50];
    sprintf(th_name, "AudioNode %s", h_node->name);
    pli_setThreadName(th_name);

    show_audio_process_thread_sched_attr(0);
    nice(-20);
#endif

    /*while (h_node->pThread->IsAskToExit(h_node->pThread) == FALSE)*/
	for (;;) {
		set_freezable();

		if (kthread_should_stop())
			break;

        if(h_node->activate != NULL) {
#ifdef AC4DEC_TA
            if (h_node->nodeType != ENUM_NODE_AC4_DEC)
#endif
                if (!NodeInputTask(h_node)) {
                    msleep(h_node->threadSleepMs);
                    continue;
                }

#if 0
            if (start_ms > 0)
            {
                if ((pli_getMilliseconds() - start_ms) > h_node->activate_ex_ms*2)
                    h_node->plh_out.b_act_polling_delay = 1;
                h_node->plh_out.polling_act_ms = pli_getMilliseconds() - start_ms;
            }
            start_ms = pli_getMilliseconds();
#endif
            start_pts = pli_getPTS();

            res = h_node->activate(h_node);

            duration_pts = pli_getPTS() - start_pts;
            if (res == NO_ERR
#ifdef AC4DEC_TA
                    && h_node->nodeType != ENUM_NODE_AC4_DEC
#endif
                )
            {
#if 0
                h_node->activate_ex_ms = (int)(pli_getMilliseconds() - start_ms);
#endif
                calculate_kcps(h_node, duration_pts);
                NodeOutputTask(h_node);
            }
        }
#if 0
        if(pli_getMilliseconds() - start_ms > 100) {
            DEBUG("[AudioProcess] Node(%s) takes %d ms\n", h_node->name, (int)(pli_getMilliseconds() - start_ms));
        }
#endif
        msleep(h_node->threadSleepMs);
    }
    return S_OK;
}
#endif

int CreateProcess(SYSTEM_MESSAGE_INFO *info, SYSTEM_MESSAGE_RES *res)
{
    node_handle h_node;
    INFO("=============[LIBK]CreateProscess=====================\n");
    /*INFO("=============[LIBK]sema addr %x (pid:%d, threadid:%x) \n", &process_list_sem, getpid(), syscall(SYS_gettid));*/
    // KWarning: checked ok by chengyao.tseng@realtek.com
    h_node = CreateNode(info);
#ifdef MULTI_THREAD
    if(h_node) {
        INFO("[LIBK]CreateProscess Node succes\n");
        down(&process_list_sem);
        /*osal_SemWait(&process_list_sem, TIME_INFINITY);*/
        res->instanceID = RegisterNodeAndGetNodeID(h_node, &process_list);
        /*osal_SemGive(&process_list_sem);*/
        up(&process_list_sem);

        h_node->nodeID = res->instanceID;
        memset(res->data, 0, sizeof(long) * 29);
        res->packetBufHeader[0] = GetPacketPinBufHeader(h_node->inPktPin);
        res->packetBufHeader[1] = GetPacketPinBufHeader(h_node->outPktPin);
        INFO("Create Finished Header:%x/%x\n",(UINT32)res->packetBufHeader[0],(UINT32)res->packetBufHeader[1]);

        INFO("Node %s with nodeID %d\n", h_node->name, (UINT32)h_node->nodeID);
#if 0
        h_node->pThread = new_thread(AudioNodeThread, /*(node_handle*)*/h_node);

        if(h_node->pThread->IsRun(h_node->pThread) == FALSE) {
            h_node->pThread->Run(h_node->pThread);
        }
#else
    }
    rtkaudio_process_tsk = kthread_create(AudioNodeThread, NULL,
            "rtkaudio_process_tsk");
    if (IS_ERR(rtkaudio_process_tsk)) {
        rtkaudio_process_tsk = NULL;
        return S_FALSE;
    }
    wake_up_process(rtkaudio_audio_process_tsk);
#endif
#else
    if(h_node) {
        INFO("[LIBK]CreateProscess Node succes\n");
        /*osal_SemWait(&process_list_sem, TIME_INFINITY);*/
        down(&process_list_sem);
        res->instanceID = RegisterNodeAndGetNodeID(h_node, &process_list);
        /*osal_SemGive(&process_list_sem);*/
        up(&process_list_sem);

        h_node->nodeID = res->instanceID;
        memset(res->data, 0, sizeof(long) * 29);
        res->packetBufHeader[0] = GetPacketPinBufHeader(h_node->inPktPin);
        res->packetBufHeader[1] = GetPacketPinBufHeader(h_node->outPktPin);
        INFO("Node(%s) Create Finished\n",h_node->name);
    }
#endif
    return S_OK;
}

int DeleteProcess(SYSTEM_MESSAGE_INFO *info)
{
    node_handle h_node;
    long instanceID = 0xdeaddead;
    INFO("[AUDL]%s\n", __FUNCTION__);
    if(info != NULL)
      instanceID = info->instanceID;
    else
        return S_FALSE;
    INFO("=============[LIBK]CloseProscess========%x %x \n", (UINT32)info, (UINT32)instanceID);
    /*INFO("=============[LIBK]Close sema addr %x (pid:%d, threadid:%x) \n", &process_list_sem, getpid(), syscall(SYS_gettid));*/
    /*osal_SemWait(&process_list_sem, TIME_INFINITY);*/
    down(&process_list_sem);
    h_node = GetNodeByNodeID(&process_list, info->instanceID);
    if(h_node) {
        INFO("[AUDL]%s(%s) delete:%d\n", __FUNCTION__, h_node->name,(int)h_node->nodeID);
#ifdef MULTI_THREAD
#if 0
        if(h_node->pThread) {
            if(h_node->pThread->IsRun(h_node->pThread))
                h_node->pThread->Exit(h_node->pThread, TRUE);
            h_node->pThread->Delete(h_node->pThread);
        }
        h_node->pThread = NULL;
#else
        if (rtkaudio_process_tsk) {
            int ret;
            ret = kthread_stop(rtkaudio_process_tsk);
            if (!ret)
                ERROR("rtkaudio thread stopped\n");
        }
#endif
#endif
        UnRegisterNode(h_node, &process_list);
        if (h_node->destroy)
            h_node->destroy(h_node);

        UninitialNode(h_node);
    }
    else
    {
        INFO("h_node is NULL");
    }
    /*osal_SemGive(&process_list_sem);*/
    up(&process_list_sem);
    INFO("[AUDL]%s done\n", __FUNCTION__);
    return S_OK;
}

#if !defined(USE_INBAND_SYSPROC_COMMUNICATION)
static void RegisterPIDToACPU(int valid)
{
    /*CLNT_STRUCT clnt;*/
    UINT32 res;
    AUDIO_RPC_PRIVATEINFO_RETURNVAL result;
    struct AUDIO_RPC_PRIVATEINFO_PARAMETERS parameter;

    /*clnt = prepareCLNT(BLOCK_MODE | USE_INTR_BUF | SEND_AUDIO_CPU, AUDIO_SYSTEM, VERSION);*/
    parameter.type = ENUM_PRIVATEINFO_AUDIO_SET_SYSTEM_PROCESS_PID;
    parameter.instanceID = valid;
    res = RTKAUDIO_RPC_TOAGENT_PRIVATEINFO_SVC(&parameter, &result);
    if(res != S_OK) ERROR("[%s:%d] RPC return != S_OK\n",__FUNCTION__,__LINE__);


    /*if( (hr == (AUDIO_RPC_PRIVATEINFO_RETURNVAL *)-1 ) ||  (hr== 0))*/
            /*MY_AUDIO_RPC_ERROR("return value = %x\n", (unsigned int) hr);*/
        /*MY_AUDIO_ASSERT((hr != ((AUDIO_RPC_PRIVATEINFO_RETURNVAL *)-1 )));*/

     /*if (((int)hr == 0) || ((int)hr== -1))*/
     /*{*/
        /*return;//  S_FALSE;*/
     /*}*/
     /*else*/
     /*{*/
        /*free(hr);*/
     /*}*/

}
#else
static void RegisterPIDToACPU(int valid, long readInfo, long writeInfo)
{
    /*CLNT_STRUCT clnt;*/
    UINT32 res;
    AUDIO_RPC_PRIVATEINFO_RETURNVAL result;
    struct AUDIO_RPC_PRIVATEINFO_PARAMETERS parameter;

    /*clnt = prepareCLNT(BLOCK_MODE | USE_INTR_BUF | SEND_AUDIO_CPU, AUDIO_SYSTEM, VERSION);*/
    parameter.type = ENUM_PRIVATEINFO_AUDIO_SET_SYSTEM_PROCESS_PID;
    parameter.instanceID = valid;
    parameter.privateInfo[0] = readInfo;
    parameter.privateInfo[1] = writeInfo;
    parameter.privateInfo[2] = 0x97322379;
    res = RTKAUDIO_RPC_TOAGENT_PRIVATEINFO_SVC(&parameter, &result);
    if(res != S_OK) ERROR("[%s:%d] RPC return != S_OK\n",__FUNCTION__,__LINE__);

    /*if((hr == (AUDIO_RPC_PRIVATEINFO_RETURNVAL *) -1) || (hr== 0))*/
        /*MY_AUDIO_RPC_ERROR("return value = %x\n", (unsigned int) hr);*/
    /*MY_AUDIO_ASSERT((hr != ((AUDIO_RPC_PRIVATEINFO_RETURNVAL *)-1 )));*/

    /*if (((int)hr == 0) || ((int)hr== -1))*/
    /*{*/
        /*return;//  S_FALSE;*/
    /*}*/
    /*else*/
    /*{*/
        /*free(hr);*/
    /*}*/

}
#endif

#if !defined(USE_INBAND_SYSPROC_COMMUNICATION)
int AudioProcessThread(void* data)
{
    /*AUDIO_PROCESS *pAProcess = (AUDIO_PROCESS*)data;*/
    /*pli_setThreadName("AudioProcessThread");*/
    /*int64_t start_ms = 0;*/
#if !defined(MULTI_THREAD)
    node_handle h_node;
    int res;
    int64_t start_pts = 0, duration_pts = 0;
    int loop = 0;
#endif
#if 0
    cpu_set_t get;
    int num = 0;

    cpu_set_t mask;
    #define RUNONCPU (2)
    #define RUNONCPU2 (3) //prepare to set the other using cpu

    num = sysconf(_SC_NPROCESSORS_CONF);
    MY_LIBK_INFO("[AudioProcess]PASS:%s system has %d processor(s)\n", __FUNCTION__ , num);

    CPU_ZERO(&mask);
    CPU_SET(RUNONCPU, &mask);
    //CPU_SET(RUNONCPU2, &mask);//prepare to set the other using cpu
    int rtn = pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask);
    //if(sched_setaffinity(0, sizeof(mask), &mask) < 0)
    if(rtn < 0)
    {
        MY_LIBK_INFO("[AudioProcess] failed to run on CPU %d,rtn=%d\n", RUNONCPU,rtn);
    }

    CPU_ZERO(&get);

    if (pthread_getaffinity_np(pthread_self(), sizeof(get), &get) < 0) {
        MY_LIBK_INFO("[AudioProcess] %s get thread affinity failed\n", __FUNCTION__);
    }

    for (int j = 0; j < num; j++) {
        if (CPU_ISSET(j, &get)) {
            MY_LIBK_INFO("[AudioProcess] %s thread 0x%x is running in processor %d\n", __FUNCTION__, pthread_self(), j);
        }
    }

    //show_audio_process_thread_sched_attr();
    nice(-20);
#endif

#if !defined(MULTI_THREAD)
    /*while (pAProcess->pThread->IsAskToExit(pAProcess->pThread) == FALSE)*/
	for (;;) {

		set_freezable();

		if (kthread_should_stop())
			break;

        /*osal_SemWait(&process_list_sem, TIME_INFINITY);*/
        down(&process_list_sem);
        list_for_each_entry(h_node, &process_list, list) {
            if(h_node->activate != NULL) {

                if (h_node->nodeType != ENUM_NODE_AC4_DEC)
                    if (!NodeInputTask(h_node)) {
                        msleep(h_node->threadSleepMs);
                        continue;
                    }

#if 0
                if (start_ms > 0)
                {
                    if ((pli_getMilliseconds() - start_ms) > h_node->activate_ex_ms*2)
                        h_node->plh_out.b_act_polling_delay = 1;
                    h_node->plh_out.polling_act_ms = pli_getMilliseconds() - start_ms;
                }
                start_ms = pli_getMilliseconds();
#endif
                start_pts = pli_getPTS();

                res = h_node->activate(h_node);

                duration_pts = pli_getPTS() - start_pts;
                if (res == NO_ERR && h_node->nodeType != ENUM_NODE_AC4_DEC)
                {
                    /*h_node->activate_ex_ms = (int)(pli_getMilliseconds() - start_ms);*/
                    calculate_kcps(h_node, duration_pts);
                    NodeOutputTask(h_node);
                }
            }

#if 0
            if(pli_getMilliseconds() - start_ms > 100) {
                DEBUG("[AudioProcess] Node(%s) takes %d ms\n", h_node->name, (int)(pli_getMilliseconds() - start_ms));
#endif
            }
        }
        /*osal_SemGive(&process_list_sem);*/
        up(&process_list_sem);
        msleep(1);
        loop++;
        if( (loop % 50000) == 0)
           INFO("[AudioProcess]sema address %x \n", (UINT32)&process_list_sem);
           /*INFO("[AudioProcess]sema address %x (pid:%d, threadid:%x) \n", &process_list_sem, getpid(), syscall(SYS_gettid));*/
    }
#endif
    return S_OK;
}

int DeleteAudioProcessThread(void)
{

#if 0
    if(pAProcess) {
        if(pAProcess->pThread) {
            if(pAProcess->pThread->IsRun(pAProcess->pThread))
                pAProcess->pThread->Exit(pAProcess->pThread, TRUE);
            pAProcess->pThread->Delete(pAProcess->pThread);
        }
        pAProcess->pThread = NULL;

        RTK_Free(pAProcess);
        osal_SemDestroy(&process_list_sem);
        //FreePreCreatePkePin();
    }
#else
	if (rtkaudio_audio_process_tsk) {
        int ret;
		ret = kthread_stop(rtkaudio_audio_process_tsk);
		if (!ret)
			ERROR("rtkaudio thread stopped\n");
	}
#endif

#if !defined(USE_INBAND_SYSPROC_COMMUNICATION)
    RegisterPIDToACPU(FALSE);
#endif

    return 0;
}

int CreateAudioProcessThread(void)
{
#if 0
    AUDIO_PROCESS *pAProcess = (AUDIO_PROCESS*)RTK_Malloc(sizeof(AUDIO_PROCESS));
    if(pAProcess == NULL)
        return NULL;
#endif

#if !defined(MULTI_THREAD)
    /*osal_SemCreate(1, 0, &process_list_sem);*/
    /*osal_SemGive(&process_list_sem);*/
    process_list = LIST_HEAD_INIT(process_list);
#endif

#if 0
    pAProcess->pThread = new_thread(AudioProcessThread, pAProcess);
    if(pAProcess->pThread != NULL)
    {
        if(pAProcess->pThread->IsRun(pAProcess->pThread) == FALSE) {
            pAProcess->pThread->Run(pAProcess->pThread);
        }
    }
#else
	rtkaudio_audio_process_tsk = kthread_create(AudioProcessThread, NULL,
			"rtkaudio_audio_process_tsk");
	if (IS_ERR(rtkaudio_audio_process_tsk)) {
		rtkaudio_audio_process_tsk = NULL;
		return S_FALSE;
	}
	wake_up_process(rtkaudio_audio_process_tsk);
#endif

#if !defined(USE_INBAND_SYSPROC_COMMUNICATION)
    RegisterPIDToACPU(TRUE);
#endif
    DEBUG("CreateAudioProcessThread Finished\n");

    return S_OK;
}
#endif

#if defined(USE_INBAND_SYSPROC_COMMUNICATION)
int AudioManagerProcessThread(void* data)
{
    /*AUDIO_PROCESS *pAProcess = (AUDIO_PROCESS*)data;*/
    /*pli_setThreadName("AudioManager");*/
    /*int64_t start_ms;*/
    int loop = 0;
    long read_hdr, write_hdr;
    int64_t curr_pts = 0;

#if defined(MULTI_THREAD)
    /*osal_SemCreate(1, 0, &process_list_sem);*/
    /*osal_SemGive(&process_list_sem);*/
    INIT_LIST_HEAD(&process_list);
#endif

#if 0
    show_audio_process_thread_sched_attr(1);
    nice(-20);
#endif

    /*while (pAProcess->pThread->IsAskToExit(pAProcess->pThread) == FALSE)*/
	for (;;) {
        int64_t take_pts = (int64_t)((pli_getPTS() - curr_pts));

		set_freezable();

		if (kthread_should_stop())
			break;

        if (curr_pts != 0  && (take_pts > 1000*90))
            ERROR("[AudioManager] Node(%s) scheduled over than 1s. takse %d ms, expect %dms\n", g_Audio_Manager_node->name, (unsigned int)take_pts,g_Audio_Manager_node->threadSleepMs);

        curr_pts = pli_getPTS();
#if 0
        start_ms = pli_getMilliseconds();
#endif
        if(g_Audio_Manager_node->activate != NULL) {
            g_Audio_Manager_node->activate(g_Audio_Manager_node);
        }

#if 0
        if(pli_getMilliseconds() - start_ms > 100) {
            ERROR("[AudioManager] Node(%s) takes %d ms\n", g_Audio_Manager_node->name, (int)(pli_getMilliseconds() - start_ms));
        }
#endif

        read_hdr = GetPacketPinBufHeader(g_Audio_Manager_node->inPktPin);
        write_hdr = GetPacketPinBufHeader(g_Audio_Manager_node->outPktPin);

        msleep(g_Audio_Manager_node->threadSleepMs);
        loop++;
        if((loop % 10000) == 0)
           INFO("[AudioManager] InfoQ address (%x,%x)\n", (UINT32)read_hdr, (UINT32)write_hdr);
           /*MY_LIBK_INFO("[AudioManager] InfoQ address (%x,%x) (pid:%d, threadid:%x) \n", read_hdr, write_hdr, getpid(), syscall(SYS_gettid));*/
    }

    return S_OK;
}

int DeleteAudioManagerProcessThread(void)
{
#if 0
    if(pAProcess) {
        if(pAProcess->pThread) {
            if(pAProcess->pThread->IsRun(pAProcess->pThread))
                pAProcess->pThread->Exit(pAProcess->pThread, TRUE);
            pAProcess->pThread->Delete(pAProcess->pThread);
        }
        pAProcess->pThread = NULL;

        RTK_Free(pAProcess);
    }
#else
	if (rtkaudio_manager_tsk) {
        int ret;
		ret = kthread_stop(rtkaudio_manager_tsk);
		if (!ret)
			ERROR("rtkaudio thread stopped\n");
	}
#endif

    RegisterPIDToACPU(FALSE, 0, 0);
    if (g_Audio_Manager_node) {
        g_Audio_Manager_node->destroy(g_Audio_Manager_node);
        UninitialNode(g_Audio_Manager_node);
    }

    return 0;
}

int CreateAudioManagerProcessThread(void)
{
    SYSTEM_MESSAGE_INFO info;
    long read_hdr, write_hdr;

    info.type = ENUM_NODE_MESSAGE_MANAGER;
    info.instanceID = -1;

    g_Audio_Manager_node = CreateNode(&info);
    if (g_Audio_Manager_node == NULL)
        return S_FALSE;

#if 0
    AUDIO_PROCESS *pAProcess = (AUDIO_PROCESS*)kmalloc(sizeof(AUDIO_PROCESS), GFP_KERNEL);
    if(pAProcess == NULL)
        return NULL;

    pAProcess->pThread = new_thread(AudioManagerProcessThread, pAProcess);
    if(pAProcess->pThread != NULL)
    {
        if(pAProcess->pThread->IsRun(pAProcess->pThread) == FALSE) {
            pAProcess->pThread->Run(pAProcess->pThread);
        }
    }
#else
	rtkaudio_manager_tsk = kthread_create(AudioManagerProcessThread, NULL,
			"rtkaudio_manager_tsk");
	if (IS_ERR(rtkaudio_manager_tsk)) {
		rtkaudio_manager_tsk = NULL;
		return S_FALSE;
	}
	wake_up_process(rtkaudio_manager_tsk);
#endif

    read_hdr = GetPacketPinBufHeader(g_Audio_Manager_node->inPktPin);
    write_hdr = GetPacketPinBufHeader(g_Audio_Manager_node->outPktPin);
    RegisterPIDToACPU(TRUE, read_hdr, write_hdr);

    DEBUG("CreateAudioManagerProcessThread Finished\n");

    return S_OK;
}
#endif
