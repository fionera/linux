#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/err.h>
#include "misc.h"
#include "ta_re_core.h"
#include "dmx_re_ta.h"

#define DEMUX_UUID  {                                      \
		0x9bdca48d, 0x0244, 0x4cb4,                        \
		{                                                  \
			0xb0, 0x53, 0xf2, 0xd5, 0x0f, 0x98, 0x43, 0xd8 \
		}                                                  \
	}

static struct optee_ta dmx_ta = {};
int dmx_ta_init(void)
{
	if (IS_ERR_OR_NULL(dmx_ta.ctx)) {
		struct teec_uuid uuid = DEMUX_UUID;
		if (ta_re_init(&uuid, &dmx_ta) < 0) {
			pr_err("%s_%d: fail ta init\n", __func__, __LINE__);
			return -1;
		}
	}
	return 0;
}
int dmx_ta_dmx_init(unsigned char ch, unsigned char pksize, struct ta_tpc_info *tpcs)
{
	struct tee_param *param = NULL;
	struct tee_shm *shm_tpc_buf = NULL;
	unsigned char * data = NULL;
	int ret = 0;
	int tpc_size = 0;
	param = kcalloc(TEE_NUM_PARAM, sizeof(struct tee_param), GFP_KERNEL);
	
	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[0].u.value.a = ch;
	param[0].u.value.b = pksize;
	
	param[1].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT;
	tpc_size = sizeof(struct tpc_info) *tpcs->num + 4/* tpc numbers*/;
	
	data = vmalloc(tpc_size);
	memcpy(data, &tpcs->num, sizeof(uint16_t));
	data[2] = data[3] = 0xff;
	if (tpcs->num != 0) {
		memcpy(data+4, tpcs->tpc_infos, sizeof(struct tpc_info) *tpcs->num);
	}
	shm_tpc_buf = ta_re_shm_alloc(&dmx_ta, tpc_size, data);
	if (shm_tpc_buf == NULL){
		vfree(data);
		kfree(param);
		return -1;
	}
	param[1].u.memref.shm = shm_tpc_buf;
	param[1].u.memref.size = tpc_size;

	param[2].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	param[3].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;

	if (ta_re_invoke(&dmx_ta, DMX_TA_CMD_DMX_INIT, param) < 0) {
		dmx_err(ch, "Ta data process fail\n");
		ret = -1;
	}
	ta_re_shm_free(shm_tpc_buf);
	vfree(data);
	kfree(param);
	return ret;
}

unsigned int dmx_ta_buf_map(unsigned char ch, unsigned char type,
							unsigned int bs_pa, unsigned int bs_size,
							unsigned int bs_hdr_pa, unsigned int bs_hdr_size,
							unsigned int ib_pa, unsigned int ib_size,
							unsigned int ib_hdr_pa, unsigned int ib_hdr_size)
{
	struct tee_param *param = NULL;
	struct ta_buf_info buf_info;
	struct tee_shm *shm_buf_buf = NULL;
	int ret = -1;
	param = kcalloc(TEE_NUM_PARAM, sizeof(struct tee_param), GFP_KERNEL);
	
	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[0].u.value.a = ch;
	param[0].u.value.b = type;
	
	param[1].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT;
	buf_info.pa = bs_pa;
	buf_info.pa_size = bs_size;
	buf_info.hdr_pa = bs_hdr_pa;
	buf_info.hdr_pa_size = bs_hdr_size;
	buf_info.i_pa = ib_pa;
	buf_info.i_pa_size = ib_size;
	buf_info.i_hdr_pa = ib_hdr_pa;
	buf_info.i_hdr_pa_size = ib_hdr_size;
	shm_buf_buf = ta_re_shm_alloc(&dmx_ta, sizeof(struct ta_buf_info), &buf_info);
	if (shm_buf_buf == NULL) {
		kfree(param);
		dmx_err(ch, "Ta buf map fail\n");
		return -1;
	}
	param[1].u.memref.shm = shm_buf_buf;
	param[1].u.memref.size = sizeof(struct ta_buf_info);
	
	//return curr
	param[2].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_OUTPUT;
	param[3].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;

	if (ta_re_invoke(&dmx_ta, DMX_TA_CMD_BUF_MAP, param) < 0) {
		dmx_err(ch, "Ta buf map fai\n");
		ret =  -1;
	} else {
		ret = param[3].u.value.a;
	}
	ta_re_shm_free(shm_buf_buf);
	kfree(param);
	return ret;
}
void dmx_ta_buf_unmap(u8 idx, u8 type)
{
	struct tee_param *param = NULL;
	param = kcalloc(TEE_NUM_PARAM, sizeof(struct tee_param), GFP_KERNEL);

	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[0].u.value.a = idx;
	param[0].u.value.b = type;
	
	param[1].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	param[2].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	param[3].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;

	if (ta_re_invoke(&dmx_ta, DMX_TA_CMD_BUF_UNMAP, param) < 0) {
		dmx_err(idx, "Ta BUF UNMAP fail\n");
	}
	kfree(param);
}
unsigned int dmx_ta_data_process(unsigned char ch, unsigned int cur,
			unsigned int end, int64_t* vpts, int64_t *apts, uint16_t *pes_pid0, uint16_t *pes_pid1)
{
	struct tee_param *param = NULL;
	struct tee_shm *shm_buf_dmx = NULL;
	struct dmx_result dmx_result;
	unsigned int ret = 0;
	param = kcalloc(TEE_NUM_PARAM, sizeof(struct tee_param), GFP_KERNEL);

	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[0].u.value.a = ch;
	param[0].u.value.b = 0;
	param[1].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[1].u.value.a = cur;
	param[1].u.value.b = end;
	//return curr
	param[2].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_OUTPUT;
	shm_buf_dmx = ta_re_shm_alloc(&dmx_ta, sizeof(struct dmx_result), NULL);
	if (shm_buf_dmx == NULL) {
		kfree(param);
		dmx_err(ch, "Ta shm alloc  fail\n");
		return -1;
	}
	param[2].u.memref.shm = shm_buf_dmx;
	param[2].u.memref.size = sizeof(struct dmx_result);

	param[3].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;

	if (ta_re_invoke(&dmx_ta, DMX_TA_CMD_DATA_PROCESS, param) < 0) {
		dmx_err(ch, "Ta data process fail\n");
		ret =  cur;
	} else {
		ta_re_shm_copy_to_re(shm_buf_dmx, &dmx_result, sizeof(struct dmx_result));
		ret = dmx_result.cur_phy;
		*vpts = dmx_result.vpts;
		*apts = dmx_result.apts;
		*pes_pid0 = dmx_result.pes_pid0;
		*pes_pid1 = dmx_result.pes_pid1;
	}

	ta_re_shm_free(shm_buf_dmx);
	kfree(param);
	return ret;
}

void dmx_ta_tpc_create(u8 idx, unsigned short pid, unsigned char type, unsigned char port)
{
	struct tee_param *param = NULL;
	param = kcalloc(TEE_NUM_PARAM, sizeof(struct tee_param), GFP_KERNEL);

	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[0].u.value.a = idx;
	param[0].u.value.b = pid;
	param[1].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[1].u.value.a = type;
	param[1].u.value.b = port;
	param[2].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;

	param[3].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;

	if (ta_re_invoke(&dmx_ta, DMX_TA_CMD_TPC_CREATE, param) < 0) {
		dmx_err(idx, "Ta TPC CREATE fail\n");
	}

	kfree(param);
}

void dmx_ta_tpc_release(u8 idx, unsigned short pid)
{
	struct tee_param *param = NULL;
	param = kcalloc(TEE_NUM_PARAM, sizeof(struct tee_param), GFP_KERNEL);

	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[0].u.value.a = idx;
	param[0].u.value.b = pid;

	param[1].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	param[2].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	param[3].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;

	if (ta_re_invoke(&dmx_ta, DMX_TA_CMD_TPC_RELEASE, param) < 0) {
		dmx_err(idx, "Ta TPC RELEASE fail\n");
	}

	kfree(param);
}

int dmx_ta_ib_write(u8 idx, u8 type, void * data, u32 size)
{
	struct tee_param *param = NULL;
	struct tee_shm *shm_ib_buf = NULL;
	int ret = 0;
	param = kcalloc(TEE_NUM_PARAM, sizeof(struct tee_param), GFP_KERNEL);

	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[0].u.value.a = idx;
	param[0].u.value.b = type;

	param[1].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT;
	
	shm_ib_buf = ta_re_shm_alloc(&dmx_ta, size, data);
	if (shm_ib_buf == NULL) {
		kfree(param);
		dmx_err(idx, "Ta TPC RELEASE fail\n");
		return -1;
	}
	param[1].u.memref.shm = shm_ib_buf;
	param[1].u.memref.size = size;

	param[2].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	param[3].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;

	if (ta_re_invoke(&dmx_ta, DMX_TA_CMD_IB_WRITE, param) < 0) {
		dmx_err(idx, "Ta TPC RELEASE fail\n");
		ret = -1;
	}

	ta_re_shm_free(shm_ib_buf);
	kfree(param);
	return ret;
}

int dmx_ta_pvr_auth_verify(u8 key[32])
{
	struct tee_param *param = NULL;
	struct tee_shm *shm_key_buf = NULL;
	int ret = 0;
	if (dmx_ta_init() < 0){
		dmx_err(NOCH, "ta init fail\n");
		return ret;
	}
	param = kcalloc(TEE_NUM_PARAM, sizeof(struct tee_param), GFP_KERNEL);

	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT;	
	shm_key_buf = ta_re_shm_alloc(&dmx_ta, 32, key);
	if (shm_key_buf == NULL) {
		kfree(param);
		dmx_err(NOCH, "DMX_TA_CMD_PVR_AUTH_VERIFY fail\n");
		return ret;
	}
	param[0].u.memref.shm = shm_key_buf;
	param[0].u.memref.size = 32;
	param[1].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_OUTPUT;
	param[2].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	param[3].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;

	if (ta_re_invoke(&dmx_ta, DMX_TA_CMD_PVR_AUTH_VERIFY, param) < 0) {
		dmx_err(NOCH, "DMX_TA_CMD_PVR_AUTH_VERIFY fail\n");
	} else {
		ret = param[1].u.value.a;
	}
	ta_re_shm_free(shm_key_buf);
	kfree(param);
	return ret;
}