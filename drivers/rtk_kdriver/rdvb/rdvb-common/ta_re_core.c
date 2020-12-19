#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/slab.h>

#include "ta_re_core.h"

static int TA_match(struct tee_ioctl_version_data *data, const void *vers)
{
	return 1;
}

int ta_re_init(struct teec_uuid *uuid, struct optee_ta * ta)
{
	int ret = 0, rc = 0;
	struct tee_ioctl_open_session_arg secion_open_arg;
	struct tee_ioctl_version_data vers = {
		.impl_id = TEE_IMPL_ID_OPTEE,
		.impl_caps = TEE_OPTEE_CAP_TZ,
		.gen_caps = TEE_GEN_CAP_GP,
		};
		

	memset(ta, 0, sizeof(struct optee_ta));
	ta->ctx = tee_client_open_context(NULL, TA_match, NULL, &vers);
	if (IS_ERR(ta->ctx)) {
		pr_err("optee_demux_tp: no ta context\n");
		ret = -EINVAL;
		goto err;
	}

	memset(&secion_open_arg, 0, sizeof(secion_open_arg));
	memcpy(&secion_open_arg.uuid, uuid, sizeof(struct teec_uuid));
	secion_open_arg.clnt_login = TEE_IOCTL_LOGIN_PUBLIC;
	pr_debug("secion_open_arg uuid %pUl \n", secion_open_arg.uuid);

	rc = tee_client_open_session(ta->ctx, &secion_open_arg, NULL);
	if(rc){
		pr_err("optee_demux_tp: open_session failed ret %x reg_ret: %x",
			 rc, secion_open_arg.ret);
		ret = -EINVAL;
		goto err;
	}
	if (secion_open_arg.ret) {
		ret = -EINVAL;
		goto err;
	}
	
	ta->session = secion_open_arg.session;

	pr_debug("open_session ok\n");
	return 0;
err:
	
	if (ta->session) {
		tee_client_close_session(ta->ctx, ta->session);
		pr_err("optee_demux_tp: open failed close session \n");
		ta->session = 0;
	}
	if (!IS_ERR_OR_NULL(ta->ctx)) {
		tee_client_close_context(ta->ctx);
		pr_err("optee_demux_tp: open failed close context\n");
		ta->ctx = NULL;
	}
	pr_err("open_session fail\n");

	return ret;
}

int ta_re_invoke(struct optee_ta *ta, int func_id, struct tee_param *param)
{
	int ret = 0, rc = 0;
	struct tee_ioctl_invoke_arg invoke_arg;

	if (IS_ERR_OR_NULL(ta->ctx)) {
		pr_err("optee_meminfo: no ta context\n");
		ret = -EINVAL;
		goto END;
	}
	//CALL PREPARE
	memset(&invoke_arg, 0, sizeof(invoke_arg));
	invoke_arg.func = func_id;
	invoke_arg.session = ta->session;
	invoke_arg.num_params = TEE_NUM_PARAM;

	rc = tee_client_invoke_func(ta->ctx, &invoke_arg, param);
	if (rc || invoke_arg.ret) {
		pr_err("optee_meminfo: invoke failed ret %x invoke_arg.ret %x\n",
			rc, invoke_arg.ret);
		ret = -EINVAL;
		goto END;
	}
	ret = 0;
END:

	return ret;
}
struct tee_shm * ta_re_shm_alloc(struct optee_ta *ta, unsigned int size, void *data)
{
	struct tee_shm *shm_buf = NULL;
	void * shm_va = NULL;
	shm_buf = tee_shm_alloc(ta->ctx, size, TEE_SHM_MAPPED);
	if (shm_buf == NULL){
		return NULL;
	}
	if (data == NULL)
		return shm_buf;
	shm_va = tee_shm_get_va(shm_buf, 0);
	if (shm_va == NULL) {
		ta_re_shm_free(shm_buf);
		return NULL;
	}
	memcpy(shm_va, data, size);
	return shm_buf;
}
void ta_re_shm_copy_to_re(struct tee_shm * shm_buf, void *data, unsigned int size)
{
	void *shm_va = NULL;
	if (shm_buf == NULL || data == NULL)
		return;
	shm_va = tee_shm_get_va(shm_buf, 0);
	if (shm_va == NULL) 
		return;
	memcpy(data, shm_va, size);
}
void ta_re_shm_free(struct tee_shm * shm_buf)
{
	tee_shm_free(shm_buf);
}