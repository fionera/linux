#ifndef __TA_RE_CORE__
#define __TA_RE_CORE__
#include <linux/types.h>
#define CFG_RTK_CMA_MAP 0
#include <linux/tee_drv.h>


#define TEE_NUM_PARAM 4
struct optee_ta
{
	struct tee_context *ctx;
	__u32 session;
};
int ta_re_init(struct teec_uuid *uuid, struct optee_ta * ta);
int ta_re_invoke(struct optee_ta *ta, int func_id, struct tee_param *param);
struct tee_shm *ta_re_shm_alloc(struct optee_ta *ta, unsigned int size, void *data);
void ta_re_shm_copy_to_re(struct tee_shm *shm_buf, void *data, unsigned int size);
void ta_re_shm_free(struct tee_shm *shm_buf);
#endif