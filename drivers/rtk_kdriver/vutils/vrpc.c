/*
 * Copyright (c) 2018 Jias Huang <jias_huang@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>
#include <linux/pageremap.h>
#include <linux/workqueue.h>

#pragma scalar_storage_order big-endian
#include <RPCBaseDS_data.h>
#include <VideoRPC_System.h>
#include <video_krpc_interface.h>
#pragma scalar_storage_order default

#include <rtk_kdriver/RPCDriver.h>
#include <hresult.h>

#include "vrpc.h"

#define WEAKREF(__ret, __name, ...) \
	static __ret __weakref_##__name (__VA_ARGS__) __attribute__((weakref(#__name), long_call)); \

WEAKREF(void, rpc_setVRPCService, int (*funptr)(int portID, int cmd, void *data, int size));

typedef struct vrpc_serv_work_t {
	struct work_struct work;
	int portID;
	int cmd;
	int size;
	char data[0];
} vrpc_serv_work_t;

typedef struct entry_mem_t {
	struct list_head list;
	int type;
	int size;
	int flags;
	void *cached;
	void *uncached;
	phys_addr_t phyaddr;
	int userID;
} entry_mem_t;

typedef struct entry_flt_t {
	struct list_head list;
	int vcpuID;
	u32 fltID;
	int fltType;
	int userID;
} entry_flt_t;

typedef struct entry_serv_t {
	struct list_head list;
	int nPortStart;
	int nPortEnd;
	vrpc_serv_t serv;
	void *priv;
} entry_serv_t;

static DEFINE_MUTEX(lock_mem);
static DEFINE_MUTEX(lock_flt);
static DEFINE_MUTEX(lock_serv);
static LIST_HEAD(entry_mem_head);
static LIST_HEAD(entry_flt_head);
static LIST_HEAD(entry_serv_head);
static vrpc_serv_t def_serv = NULL;

static int entry_addFilter(int userID, int vcpuID, u32 fltID, int fltType)
{
	entry_flt_t *entry_new;

	entry_new = (entry_flt_t*)vmalloc(sizeof(entry_flt_t));
	if (!entry_new) {
		pr_err("vrpc: fail to alloc entry_flt_t\n");
		return -1;
	}

	entry_new->userID = userID;
	entry_new->vcpuID = vcpuID;
	entry_new->fltID = fltID;
	entry_new->fltType = fltType;

	mutex_lock(&lock_flt);
	list_add(&entry_new->list, &entry_flt_head);
	mutex_unlock(&lock_flt);

	return 0;
}

static int entry_delFilter(u32 fltID)
{
	entry_flt_t *entry, *target = NULL;

	mutex_lock(&lock_flt);
	list_for_each_entry (entry, &entry_flt_head, list) {
		if (entry->fltID == fltID) {
			list_del(&entry->list);
			target = entry;
			break;
		}
	}
	mutex_unlock(&lock_flt);

	if (!target) {
		pr_err("vrpc: fail to del flt 0x%x\n", fltID);
		return -1;
	}

	vfree(target);

	return 0;
}

/* TODO: allocate specific memory by flags */
static int entry_allocateMemory(entry_mem_t *entry)
{
	void *cached, *uncached;

	if (!(cached = dvr_malloc_uncached_specific(entry->size, GFP_DCU1, &uncached))) {
		pr_err("vrpc: dvr_malloc_uncached_specific fail\n");
		return -1;
	}

	entry->cached = cached;
	entry->uncached = uncached;
	entry->phyaddr = dvr_to_phys(cached);

	return 0;
}

/* TODO: free specific memory by flags */
static int entry_freeMemory(entry_mem_t *entry)
{
	dvr_free(entry->cached);
	return 0;
}

int vrpc_malloc (int userID, int type, int size, int flags, vrpc_mresult_t *res)
{
	entry_mem_t *entry_new;

	if (!userID || !size || !res)
		return -EINVAL;

	entry_new = (entry_mem_t*)vmalloc(sizeof(entry_mem_t));
	if (!entry_new) {
		pr_err("vrpc: fail to alloc entry_mem_t\n");
		return -1;
	}

	entry_new->type = type;
	entry_new->size = size;
	entry_new->flags = flags;
	entry_new->userID = userID;

	if (entry_allocateMemory(entry_new) < 0) {
		vfree(entry_new);
		return -1;
	}

	mutex_lock(&lock_mem);
	list_add(&entry_new->list, &entry_mem_head);
	mutex_unlock(&lock_mem);

	res->cached = entry_new->cached;
	res->uncached = entry_new->uncached;
	res->phyaddr = entry_new->phyaddr;

	pr_info("vrpc: alloc addr %pa size %d\n", &entry_new->phyaddr, size);

	return 0;
}
EXPORT_SYMBOL(vrpc_malloc);

int vrpc_free (int userID, phys_addr_t phyaddr)
{
	entry_mem_t *entry, *target = NULL;

	if (!userID || !phyaddr)
		return -EINVAL;

	mutex_lock(&lock_mem);
	list_for_each_entry (entry, &entry_mem_head, list) {
		if (entry->phyaddr == phyaddr) {
			list_del(&entry->list);
			target = entry;
			break;
		}
	}
	mutex_unlock(&lock_mem);

	if (!target) {
		pr_err("vrpc: fail to free addr %pa\n", &phyaddr);
		return -1;
	}

	entry_freeMemory(target);
	vfree(target);

	pr_info("vrpc: free addr %pa\n", &phyaddr);

	return 0;
}
EXPORT_SYMBOL(vrpc_free);

static void vrpc_serv_work(struct work_struct *arg)
{
	entry_serv_t *entry;
	vrpc_serv_t serv = def_serv;
	void *priv = NULL;
	vrpc_serv_work_t *work = (vrpc_serv_work_t*)arg;

	mutex_lock(&lock_serv);
	list_for_each_entry (entry, &entry_serv_head, list) {
		if (work->portID >= entry->nPortStart && work->portID <= entry->nPortEnd) {
			serv = entry->serv;
			priv = entry->priv;
			break;
		}
	}
	if (serv)
		serv(work->portID, work->cmd, work->data, work->size, priv);
	mutex_unlock(&lock_serv);

	kfree(arg);
}

static int vrpc_redirectVRPC (int portID, int cmd, void *pData, int size)
{
	vrpc_serv_work_t *work;

	work = kmalloc(sizeof(vrpc_serv_work_t) + size, GFP_ATOMIC);

	if (!work) {
		pr_err("vrpc: fail to alloc work\n");
		return -1;
	}

	INIT_WORK((struct work_struct*)work, vrpc_serv_work);
	work->portID = portID;
	work->cmd = cmd;
	work->size = size;
	if (size)
		memcpy(work->data, pData, size);
	schedule_work((struct work_struct*)work);

	return 0;
}

int vrpc_sendRPC(int userID, vrpc_stuff_t *stuff, int flags)
{
	long rpc_result = 0;
	int ret, vcpuID, cmdID, offset_payload, offset_result;
	video_krpc_generic_hdr_t *krpc;
	entry_mem_t mem;

	if (!userID || !stuff)
		return -1;

	#ifdef CONFIG_REALTEK_RPC_VCPU2
	vcpuID = (flags & VRPC_F_IP2)? RPC_VIDEO2 : RPC_VIDEO;
	#else
	vcpuID = RPC_VIDEO;
	#endif

	cmdID = stuff->cmdID? stuff->cmdID : RPC_VCPU_ID_0x150_SYSTEM_TO_VIDEO_GENERIC;

	mem.cached = NULL;
	mem.size = sizeof(video_krpc_generic_hdr_t) + stuff->payloadSize + stuff->resultSize;
	if (entry_allocateMemory(&mem) < 0)
		return -1;

	offset_payload = sizeof(video_krpc_generic_hdr_t);
	offset_result = offset_payload + stuff->payloadSize;

	krpc = (video_krpc_generic_hdr_t*)mem.uncached;
	memset(krpc, 0, sizeof(video_krpc_generic_hdr_t));

	krpc->type = stuff->type;
	krpc->portID = userID;
	krpc->param1 = stuff->param1;
	krpc->param2 = stuff->param2;

	if (stuff->payloadSize) {
		krpc->payloadAddr = mem.phyaddr + offset_payload;
		krpc->payloadSize = stuff->payloadSize;
		if (stuff->payloadPtr && (flags & VRPC_F_COPY_FROM_USER)) {
			ret = copy_from_user(mem.uncached + offset_payload,
				stuff->payloadPtr, stuff->payloadSize);
			if (ret) goto exit;
		} else if (stuff->payloadPtr) {
			memcpy(mem.uncached + offset_payload,
				stuff->payloadPtr, stuff->payloadSize);
		}
	}

	if (stuff->resultSize) {
		krpc->resultAddr = mem.phyaddr + offset_result;
		krpc->resultSize = stuff->resultSize;
	}

	ret = send_rpc_command(vcpuID, cmdID, mem.phyaddr, 0, &rpc_result);
	if (ret < 0 || rpc_result != S_OK) {
		if (!ret) ret = -EFAULT;
		pr_err("vrpc: rpc fail: type %d ret %d rpc_result %ld\n", krpc->type, ret, rpc_result);
		goto exit;
	}

	/* write back result */
	if (stuff->resultPtr && stuff->resultSize) {
		if ((flags & VRPC_F_COPY_TO_USER)) {
			ret = copy_to_user(stuff->resultPtr, mem.uncached + offset_result, stuff->resultSize);
			if (ret) goto exit;
		} else {
			memcpy(stuff->resultPtr, mem.uncached + offset_result, stuff->resultSize);
		}
	}

	if (stuff->type == VIDEO_RPC_ToAgent_Create && stuff->resultSize) {
		RPCRES_LONG *rpcres = (RPCRES_LONG*)(mem.uncached + offset_result);
		VIDEO_RPC_INSTANCE *payload = (VIDEO_RPC_INSTANCE*)(mem.uncached + offset_payload);
		entry_addFilter(userID, vcpuID, rpcres->data, payload->type);
	}

	if (stuff->type == VIDEO_RPC_ToAgent_Destroy)
		entry_delFilter(stuff->param1);

exit:

	if (mem.cached)
		entry_freeMemory(&mem);

	return ret;
}
EXPORT_SYMBOL(vrpc_sendRPC);

int vrpc_sendRPC_multi_flts_param(int nPortStart, int nPortEnd,
		int *cmds, int nCmds, int *fltTypeInOrder, int nFltTypeInOrder)
{
	int i, ret, fltTypeIndex;
	long rpc_result = 0;
	entry_flt_t *entry, *entry_tmp;
	video_krpc_generic_hdr_t *krpc;
	entry_mem_t mem;

	mem.size = sizeof(video_krpc_generic_hdr_t);
	if (entry_allocateMemory(&mem) < 0)
		return -1;

	krpc = (video_krpc_generic_hdr_t*)mem.uncached;
	memset(krpc, 0, sizeof(video_krpc_generic_hdr_t));

	mutex_lock(&lock_flt);

	/*
	 * send cmd0 to fltTypeA, fltTypeB in given order
	 * send cmd1 to fltTypeA, fltTypeB in given order
	 */
	for (i=0; i<nCmds; i++) {
		fltTypeIndex = 0;
		do {
			list_for_each_entry_safe (entry, entry_tmp, &entry_flt_head, list) {
				if (nFltTypeInOrder && entry->fltType != fltTypeInOrder[fltTypeIndex])
					continue;
				if (entry->userID >= nPortStart && entry->userID <= nPortEnd) {
					krpc->portID = entry->userID;
					krpc->type = cmds[i];
					krpc->param1 = entry->fltID;
					ret = send_rpc_command(
						entry->vcpuID, RPC_VCPU_ID_0x150_SYSTEM_TO_VIDEO_GENERIC,
						mem.phyaddr, 0, &rpc_result);
					if (ret || rpc_result != S_OK) {
						pr_err("vrpc: rpc fail: flt 0x%x cmd %d ret %d result %ld\n",
							entry->fltID, cmds[i], ret, rpc_result);
					}
					if (cmds[i] == VIDEO_RPC_ToAgent_Destroy) {
						pr_info("vrpc: release flt 0x%x\n", entry->fltID);
						list_del(&entry->list);
						vfree(entry);
					}
				}
			}
			fltTypeIndex++;
		} while (fltTypeIndex < nFltTypeInOrder);
	}

	mutex_unlock(&lock_flt);

	entry_freeMemory(&mem);

	return 0;
}
EXPORT_SYMBOL(vrpc_sendRPC_multi_flts_param);

int vrpc_releaseMemory(int userID)
{
	entry_mem_t *entry, *entry_tmp;

	pr_info("vrpc: release memory with userID 0x%08x\n", userID);

	mutex_lock(&lock_mem);
	list_for_each_entry_safe (entry, entry_tmp, &entry_mem_head, list) {
		if (entry->userID == userID) {
			pr_info("vrpc: release addr %pa\n", &entry->phyaddr);
			list_del(&entry->list);
			entry_freeMemory(entry);
			vfree(entry);
		}
	}
	mutex_unlock(&lock_mem);

	return 0;
}
EXPORT_SYMBOL(vrpc_releaseMemory);

int vrpc_releaseFilter(int userID)
{
	int cmds[] = {VIDEO_RPC_ToAgent_Pause, VIDEO_RPC_ToAgent_Stop, VIDEO_RPC_ToAgent_Destroy};

	pr_info("vrpc: release filters with userID 0x%08x\n", userID);

	return vrpc_sendRPC_multi_flts_param(userID, userID, cmds, ARRAY_SIZE(cmds), NULL, 0);
}
EXPORT_SYMBOL(vrpc_releaseFilter);

int vrpc_sendRecoveryCmds(int nPortStart, int nPortEnd)
{
	unsigned long ret;
	entry_flt_t *entry, *entry_tmp;

	pr_info("vrpc: send recovery cmds port 0x%08x ~ 0x%08x\n", nPortStart, nPortEnd);

	send_rpc_command(RPC_VIDEO, RPC_VCPU_ID_0x126_Resource_Recovery_Pause, nPortStart, nPortEnd, &ret);
	send_rpc_command(RPC_VIDEO, RPC_VCPU_ID_0x125_Resource_Recovery_Flush, nPortStart, nPortEnd, &ret);
	send_rpc_command(RPC_VIDEO, RPC_VCPU_ID_0x123_Resource_Recovery_Stop, nPortStart, nPortEnd, &ret);
	send_rpc_command(RPC_VIDEO, RPC_VCPU_ID_0x124_Resource_Recovery_Destory, nPortStart, nPortEnd, &ret);

	mutex_lock(&lock_flt);
	list_for_each_entry_safe (entry, entry_tmp, &entry_flt_head, list) {
		if (entry->userID >= nPortStart && entry->userID <= nPortEnd) {
			list_del(&entry->list);
			vfree(entry);
		}
	}
	mutex_unlock(&lock_flt);

	return 0;
}
EXPORT_SYMBOL(vrpc_sendRecoveryCmds);

int vrpc_registerService(int nPortStart, int nPortEnd, vrpc_serv_t serv, void *priv)
{
	entry_serv_t *entry, *entry_new;

	if (nPortStart < 0 || nPortEnd < 0 || nPortStart > nPortEnd || !serv)
		return -EINVAL;

	entry_new = (entry_serv_t*)vmalloc(sizeof(entry_serv_t));
	if (!entry_new) {
		pr_err("vrpc: fail to alloc entry_serv_t\n");
		return -1;
	}

	entry_new->nPortStart = nPortStart;
	entry_new->nPortEnd = nPortEnd;
	entry_new->serv = serv;
	entry_new->priv = priv;

	mutex_lock(&lock_serv);
	list_for_each_entry (entry, &entry_serv_head, list) {
		if (!(nPortEnd < entry->nPortStart || nPortStart > entry->nPortEnd)) {
			pr_err("vrpc: invalid port range\n");
			vfree(entry_new);
			mutex_unlock(&lock_serv);
			return -1;
		}
	}
	list_add(&entry_new->list, &entry_serv_head);
	mutex_unlock(&lock_serv);

	pr_info("vrpc: register serv port 0x%08x ~ 0x%08x\n", nPortStart, nPortEnd);

	return 0;
}
EXPORT_SYMBOL(vrpc_registerService);

int vrpc_unregisterService(int nPortStart, int nPortEnd)
{
	entry_serv_t *entry, *entry_tmp;

	mutex_lock(&lock_serv);
	list_for_each_entry_safe (entry, entry_tmp, &entry_serv_head, list) {
		if (entry->nPortStart == nPortStart && entry->nPortEnd == nPortEnd) {
			list_del(&entry->list);
			vfree(entry);
			break;
		}
	}
	mutex_unlock(&lock_serv);

	pr_info("vrpc: unregister serv port 0x%08x ~ 0x%08x\n", nPortStart, nPortEnd);

	return 0;
}
EXPORT_SYMBOL(vrpc_unregisterService);

int vrpc_registerDefService(vrpc_serv_t serv)
{
	pr_info("vrpc: register default serv 0x%p\n", serv);

	mutex_lock(&lock_serv);
	def_serv = serv;
	mutex_unlock(&lock_serv);

	return 0;
}
EXPORT_SYMBOL(vrpc_registerDefService);

int vrpc_init (void)
{
	if (__weakref_rpc_setVRPCService)
		__weakref_rpc_setVRPCService(vrpc_redirectVRPC);
	return 0;
}

int vrpc_exit (void)
{
	if (__weakref_rpc_setVRPCService)
		__weakref_rpc_setVRPCService(NULL);
	return 0;
}

void vrpc_showStatus(int userID)
{
	entry_flt_t *flt;
	entry_mem_t *mem;
	entry_serv_t *serv;

	pr_info("vprc: show status of userID 0x%08x\n", userID);

	list_for_each_entry (serv, &entry_serv_head, list) {
		if (!userID || (userID >= serv->nPortStart && userID <= serv->nPortEnd)) {
			pr_info("\tnPortStart 0x%x nPortEnd 0x%x serv 0x%p\n",
				serv->nPortStart, serv->nPortEnd, serv->serv);
		}
	}

	list_for_each_entry (flt, &entry_flt_head, list) {
		if (!userID || userID == flt->userID) {
			pr_info("\tvcpuID %d fltID 0x%x\n",
				flt->vcpuID, flt->fltID);
		}
	}

	list_for_each_entry (mem, &entry_mem_head, list) {
		if (!userID || userID == mem->userID) {
			pr_info("\tphyaddr %pa size 0x%x\n",
				&mem->phyaddr, mem->size);
		}
	}

}
