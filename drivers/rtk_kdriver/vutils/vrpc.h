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

#ifndef __VRPC_H__
#define __VRPC_H__

#define VRPC_F_IP2		1
#define VRPC_F_COPY_FROM_USER	2
#define VRPC_F_COPY_TO_USER	4

typedef int (*vrpc_serv_t)(int portID, int cmd, void *data, int size, void *priv);

typedef struct vrpc_mresult_t {
	void *cached;
	void *uncached;
	phys_addr_t phyaddr;
} vrpc_mresult_t;

typedef struct vrpc_stuff_t {
	int cmdID;
	int type;
	u32 param1;
	u32 param2;
	void *payloadPtr;
	int payloadSize;
	void *resultPtr;
	int resultSize;
} vrpc_stuff_t;

#define vrpc_sendRPC_param(__userID, __service, __param1, __param2) \
({ \
	vrpc_stuff_t __stuff = { \
		.type = __service, \
		.param1 = __param1, \
		.param2 = __param2, \
	}; \
	vrpc_sendRPC(__userID, &__stuff, 0); \
})

#define vrpc_sendRPC_payload(__userID, __service, __payload) \
({ \
	vrpc_stuff_t __stuff = { \
		.type = __service, \
		.payloadPtr = __payload, \
		.payloadSize = sizeof(*__payload), \
	}; \
	vrpc_sendRPC(__userID, &__stuff, 0); \
})

#define vrpc_sendRPC_payload_va(__userID, __service, __structname, ...) \
({ \
	__structname __payload = {__VA_ARGS__}; \
	vrpc_stuff_t __stuff = { \
		.type = __service, \
		.payloadPtr = &__payload, \
		.payloadSize = sizeof(__payload), \
	}; \
	vrpc_sendRPC(__userID, &__stuff, 0); \
})

#define vrpc_sendRPC_cmdID_payload_va(__userID, __cmdID, __service, __structname, ...) \
({ \
	__structname __payload = {__VA_ARGS__}; \
	vrpc_stuff_t __stuff = { \
		.cmdID = __cmdID, \
		.type = __service, \
		.payloadPtr = &__payload, \
		.payloadSize = sizeof(__payload), \
	}; \
	vrpc_sendRPC(__userID, &__stuff, 0); \
})

#define vrpc_sendRPC_result_param(__userID, __service, __result, __param1, __param2) \
({ \
	vrpc_stuff_t __stuff = { \
		.type = __service, \
		.param1 = __param1, \
		.param2 = __param2, \
		.resultPtr = __result, \
		.resultSize = sizeof(*__result), \
	}; \
	vrpc_sendRPC(__userID, &__stuff, 0); \
})

#define vrpc_sendRPC_result_payload(__userID, __service, __result, __payload) \
({ \
	vrpc_stuff_t __stuff = { \
		.type = __service, \
		.payloadPtr = __payload, \
		.payloadSize = sizeof(*__payload), \
		.resultPtr = __result, \
		.resultSize = sizeof(*__result), \
	}; \
	vrpc_sendRPC(__userID, &__stuff, 0); \
})

#define vrpc_sendRPC_result_payload_va(__userID, __service, __result, __structname, ...) \
({ \
	__structname __payload = {__VA_ARGS__}; \
	vrpc_stuff_t __stuff = { \
		.type = __service, \
		.payloadPtr = &__payload, \
		.payloadSize = sizeof(__payload), \
		.resultPtr = __result, \
		.resultSize = sizeof(*__result), \
	}; \
	vrpc_sendRPC(__userID, &__stuff, 0); \
})

#define vrpc_createFilter(__userID, __type) \
({ \
	VIDEO_RPC_INSTANCE __payload = { \
		.type = __type, \
	}; \
	RPCRES_LONG __result = {0}; \
	vrpc_stuff_t __stuff = { \
		.type = VIDEO_RPC_ToAgent_Create, \
		.payloadPtr = &__payload, \
		.payloadSize = sizeof(__payload), \
		.resultPtr = &__result, \
		.resultSize = sizeof(__result), \
	}; \
	vrpc_sendRPC(__userID, &__stuff, 0); \
	__result.data; \
})

#define vrpc_postRingBuf(__userID, __fltID, __rbhAddr) \
({ \
	RPC_RINGBUFFER __payload = { \
		.instanceID = __fltID, \
		.pRINGBUFF_HEADER = __rbhAddr, \
	}; \
	vrpc_stuff_t __stuff = { \
		.type = VIDEO_RPC_ToAgent_InitRingBuffer, \
		.payloadPtr = &__payload, \
		.payloadSize = sizeof(__payload), \
	}; \
	vrpc_sendRPC(__userID, &__stuff, 0); \
})

#define vrpc_postRingBuf_pinID(__userID, __fltID, __rbhAddr, __pinID) \
({ \
	RPC_RINGBUFFER __payload = { \
		.instanceID = __fltID, \
		.pinID = __pinID, \
		.pRINGBUFF_HEADER = __rbhAddr, \
	}; \
	vrpc_stuff_t __stuff = { \
		.type = VIDEO_RPC_ToAgent_InitRingBuffer, \
		.payloadPtr = &__payload, \
		.payloadSize = sizeof(__payload), \
	}; \
	vrpc_sendRPC(__userID, &__stuff, 0); \
})

static inline int vrpc_generateUserID(int pid, int fd)
{
	/* NOTE: PID_MAX_DEFAULT 0x8000 */
	if (pid <= 0 || pid >= 0x8000 || fd < 0 || fd >= 0x10000)
		return -1;
	return (pid << 16) | fd;
}

int vrpc_init(void);
int vrpc_exit(void);
int vrpc_sendRPC(int userID, vrpc_stuff_t *stuff, int flags);
int vrpc_sendRPC_multi_flts_param(int nPortStart, int nPortEnd,
		int *cmds, int nCmds, int *fltTypeInOrder, int nFltTypeInOrder);
int vrpc_releaseMemory(int userID);
int vrpc_releaseFilter(int userID);
int vrpc_sendRecoveryCmds(int nPortStart, int nPortEnd);
int vrpc_registerService(int nPortStart, int nPortEnd, vrpc_serv_t serv, void *priv);
int vrpc_unregisterService(int nPortStart, int nPortEnd);
int vrpc_registerDefService(vrpc_serv_t serv);

int vrpc_malloc(int userID, int type, int size, int flags, vrpc_mresult_t *res);
int vrpc_free(int userID, phys_addr_t phyaddr);

/* debug helper */
void vrpc_showStatus(int userID);

#endif
