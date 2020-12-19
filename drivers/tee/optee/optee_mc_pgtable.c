#include <linux/kernel.h>
#include <linux/tee.h>
#include <linux/ioctl.h>
#include <linux/module.h>
#include <linux/tee_drv.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <mach/rtk_platform.h>
#include <linux/optee_mc_pgtable.h>
#include <linux/gfp.h>
#include <linux/mm.h>

#define MC_PGTABLE_UUID                                     \
		{ 0xfa704667, 0xb9bb, 0x4697,                           \
			{ 0x9a, 0x1c, 0x60, 0x77, 0x15, 0xd2, 0x86, 0xbd } }

#define MC_PGTABLE_ENTRY_UNIT_DEFAULT 0x180000 // 1.5G
#define MC_PGTABLE_CMD_GET_ENTRY	  100
#define MC_PGTABLE_CMD_SET_ENTRY		200
#define MC_PGTABLE_CMD_UNSET_ENTRY		300
#define MC_PGTABLE_CMD_GET_MODULE_INFO  10
#define MC_PGTABLE_CMD_DEBUG  1000

#define TEE_NUM_PARAM 4
#define TEE_ALLOCATOR_DESC_LENGTH 32
#define TEE_TA_COUNT_MAX 1

struct optee_ta {
    struct tee_context *ctx;
    __u32 session;
};

static struct optee_ta mc_pgtable_ta;

static int mc_pgtable_match(struct tee_ioctl_version_data *data, const void *vers)
{
	return 1;
}

struct mc_module_range *mc_modules = NULL;
int mc_modules_num = 0;
struct page *mc_modules_page = NULL;
unsigned long *mc_modules_page_va = NULL;
unsigned long mc_module_va_base = 0;
unsigned long mc_pgtable_entry_unit = MC_PGTABLE_ENTRY_UNIT_DEFAULT;

extern unsigned long cma_mc_start_addr;
extern unsigned long cma_mc_size;

#define DRAM_size 0x60000000
static int mc_pgtable_init_modules (void);

int mc_pgtable_init(void)
{
	int ret = 0, rc = 0;
	struct teec_uuid uuid = MC_PGTABLE_UUID;
	struct tee_param *param = NULL;
	struct tee_ioctl_open_session_arg arg;
	struct tee_ioctl_version_data vers = {
		.impl_id = TEE_IMPL_ID_OPTEE,
		.impl_caps = TEE_OPTEE_CAP_TZ,
		.gen_caps = TEE_GEN_CAP_GP,
	};
	unsigned long ddr_size_multiple = 1; // 1=> 512M; 2=> 1024M; 3=> 1536M; 4=> 2048M

	//BUG_ON(DRAM_size <= 0);
	ddr_size_multiple = 3;//DRAM_size / 0x20000;
	//mc_pgtable_entry_unit = 0x80000 * ddr_size_multiple; // 512KB * 3 = 1.5MB

	memset(&mc_pgtable_ta, 0, sizeof(mc_pgtable_ta));
	mc_pgtable_ta.ctx = tee_client_open_context(NULL, mc_pgtable_match, NULL, &vers);
	if(mc_pgtable_ta.ctx == NULL) {
		pr_err("mc_pgtable: no ta context\n");
		ret = -EINVAL;
		goto err;
	}

	memset(&arg, 0, sizeof(arg));
	memcpy(&arg.uuid, &uuid, sizeof(struct teec_uuid));
	arg.clnt_login = TEE_IOCTL_LOGIN_PUBLIC;
	arg.num_params = TEE_NUM_PARAM;
	pr_err("mc_pgtable arg uuid %pUl \n", arg.uuid);

	param = kcalloc(TEE_NUM_PARAM, sizeof(struct tee_param), GFP_KERNEL);
	if(!param)
	{
		pr_err("mc_pgtable no mem param\n");
		ret = -ENOMEM;
		goto err;
	}


	memset(param, 0, sizeof(struct tee_param) * TEE_NUM_PARAM);
	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[0].u.value.a = mc_pgtable_entry_unit;
	param[0].u.value.b = 0;
	param[1].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[1].u.value.a = cma_mc_start_addr;
	param[1].u.value.b = cma_mc_size;
	param[2].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_OUTPUT;
	param[2].u.value.a = 0;
	param[2].u.value.b = 0;
	param[3].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	rc = tee_client_open_session(mc_pgtable_ta.ctx, &arg, param);
	if(rc){
		pr_err("mc_pgtable: open_session failed ret %x arg %x", rc, arg.ret);
		ret = -EINVAL;
		goto err;
	}
	if (arg.ret) {
		ret = -EINVAL;
		goto err;
	}
	mc_modules_num = param[2].u.value.a;
	mc_module_va_base = param[2].u.value.b;
	mc_pgtable_ta.session = arg.session;
	mc_modules = kmalloc(sizeof(struct mc_module_range) *  mc_modules_num, GFP_KERNEL);

	pr_info("open_session ok mc_pgtable total_module=%d module_va_base=%lu pa_pool_addr=%lu\n", (int)mc_modules_num,(unsigned long) mc_module_va_base, (unsigned long)cma_mc_start_addr);
	ret = mc_pgtable_init_modules();
	if(ret != 0)
	{
		pr_err("optee init modules fail ret=%d\n",ret);
		BUG_ON(1);
	}
	if(param)
		kfree(param);
	return 0;
err:
	if (mc_pgtable_ta.session) {
		tee_client_close_session(mc_pgtable_ta.ctx, mc_pgtable_ta.session);
		pr_err("optee_mc_pgtable: open failed close session \n");
		mc_pgtable_ta.session = 0;
	}
	if (mc_pgtable_ta.ctx) {
		tee_client_close_context(mc_pgtable_ta.ctx);
		pr_err("optee_mc_pgtable: open failed close context\n");
		mc_pgtable_ta.ctx = NULL;
	}
	if(param)
		kfree(param);
	pr_err("mc_pgtable open_session fail\n");

	return ret;

}
void mc_pgtable_deinit(void)
{
	if (mc_pgtable_ta.session) {
		tee_client_close_session(mc_pgtable_ta.ctx, mc_pgtable_ta.session);
		mc_pgtable_ta.session = 0;
	}

	if (mc_pgtable_ta.ctx) {
		tee_client_close_context(mc_pgtable_ta.ctx);
		mc_pgtable_ta.ctx = NULL;
	}
	if(mc_modules) {
		kfree(mc_modules);
	}
}

int module_size = 0;
static int mc_pgtable_init_modules (void)
{
	int ret = 0, rc = 0;
	struct tee_ioctl_invoke_arg arg;
	struct tee_param *param = NULL;
	struct tee_shm *shm_buffer;
	int shm_len = 0;
	phys_addr_t shm_pa = 0;
	unsigned long *shm_va = 0;
	int i = 0;

	mc_modules_page = alloc_page(GFP_KERNEL);
	mc_modules_page_va =  page_address(mc_modules_page);

	if(mc_pgtable_ta.ctx == NULL) {
		pr_err("mc_pgtable: no ta context\n");
		ret = -EINVAL;
		goto out;
	}

	param = kcalloc(TEE_NUM_PARAM, sizeof(struct tee_param), GFP_KERNEL);
	if(!param)
	{
		pr_err("mc_pgtable no mem param\n");
		ret = -ENOMEM;
		goto out;
	}


	memset(&arg, 0, sizeof(arg));
	arg.func = MC_PGTABLE_CMD_GET_MODULE_INFO;
	arg.session = mc_pgtable_ta.session;
	arg.num_params = TEE_NUM_PARAM;

	// alloc share memory
	shm_len = (sizeof(unsigned long) * mc_modules_num * 2);
	shm_buffer = tee_shm_alloc(mc_pgtable_ta.ctx, shm_len, TEE_SHM_MAPPED);
	if (shm_buffer == NULL) {
        pr_err("mc_pgtable: no shm_buffer\n");
		ret = -ENOMEM;
		goto out;
	}

	// get share memory virtual addr for data accessing
	shm_va = tee_shm_get_va(shm_buffer, 0);
	if (shm_va == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	// get share memory physial addr for tee param
	rc = tee_shm_get_pa(shm_buffer, 0, &shm_pa);
	if (rc) {
		ret = -ENOMEM;
		goto out;
	}

	//pr_err("shm_buffer=%p, shm_len=%d, va=%p, pa=%x\n", (void *)shm_buffer, shm_len, shm_va, (unsigned int)shm_pa);

	memset(param, 0, sizeof(struct tee_param) * TEE_NUM_PARAM);
	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_OUTPUT;
	param[0].u.memref.shm = shm_buffer;
	param[0].u.memref.size = shm_len;
	param[1].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	param[2].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	param[3].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;

	rc = tee_client_invoke_func(mc_pgtable_ta.ctx, &arg, param);
	if (rc || arg.ret) {
		pr_err("mc_pgtable: invoke failed ret %x arg.ret %x\n", rc, arg.ret);
		ret = -EINVAL;
		goto out;
	}

	//mc_modules[i] (unsigned long *)shm_va, shm_len);

	for(i = 0; i < mc_modules_num ; i++) {
		mc_modules[i].id = i;
		mc_modules[i].entry_count = shm_va[i * 2];
		mc_modules[i].va = shm_va[i * 2 + 1];
		mc_modules[i].used = 0;
      //pr_err("sunny id %d addr %lu \n", i, mc_modules[i].entry_count);
	}

out:
	if (param)
		kfree(param);

	if (shm_buffer)
		tee_shm_free(shm_buffer);

	if (ret)
		return ret;
	else
		return 0;
}

int mc_pgtable_get_entries (enum remap_module_id module_id, unsigned long *pa_array)
{
	int ret = 0, rc = 0;
	struct tee_ioctl_invoke_arg arg;
	struct tee_param *param = NULL;
	struct tee_shm *shm_buffer = NULL;
	int shm_len = 0;
	phys_addr_t shm_pa = 0;
	unsigned long *shm_va = 0;
	//int module_size;
	int entry_count = 0;

	pr_debug("get from optee\n");
	if(mc_modules[module_id].used)
	{
		pr_err("%s module used %d\n", __func__, mc_modules[module_id].used);
		mc_pgtable_debug(0x1);
		ret = -EINVAL;
		goto out;
	}

	if(mc_pgtable_ta.ctx == NULL) {
		pr_err("mc_pgtable: no ta context\n");
		ret = -EINVAL;
		goto out;
	}

	param = kcalloc(TEE_NUM_PARAM, sizeof(struct tee_param), GFP_KERNEL);
	if(!param)
	{
		pr_err("mc_pgtable no mem param\n");
		ret = -ENOMEM;
		goto out;
	}


	memset(&arg, 0, sizeof(arg));
	arg.func = MC_PGTABLE_CMD_GET_ENTRY;
	arg.session = mc_pgtable_ta.session;
	arg.num_params = TEE_NUM_PARAM;
	entry_count = mc_modules[module_id].entry_count;
	pr_err("%s get entry_count %x\n",__func__, entry_count);
	// alloc share memory
	shm_len = (sizeof(unsigned long) * entry_count);
	shm_buffer = tee_shm_alloc(mc_pgtable_ta.ctx, shm_len, TEE_SHM_MAPPED);
	if (shm_buffer == NULL) {
        pr_err("mc_pgtable: no shm_buffer\n");
		ret = -ENOMEM;
		goto out;
	}

	// get share memory virtual addr for data accessing
	shm_va = tee_shm_get_va(shm_buffer, 0);
	if (shm_va == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	// get share memory physial addr for tee param
	rc = tee_shm_get_pa(shm_buffer, 0, &shm_pa);
	if (rc) {
		ret = -ENOMEM;
		goto out;
	}
	//pr_err("shm_buffer=%p, shm_len=%d, va=%p, pa=%x\n", (void *)shm_buffer, shm_len, shm_va, (unsigned int)shm_pa);

	memset(param, 0, sizeof(struct tee_param) * TEE_NUM_PARAM);
	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[0].u.value.a = module_id; // module id
	param[0].u.value.b = 0;
	param[1].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_OUTPUT;
	param[1].u.memref.shm = shm_buffer;
	param[1].u.memref.size = shm_len;
	param[2].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	param[3].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	rc = tee_client_invoke_func(mc_pgtable_ta.ctx, &arg, param);
	if (rc || arg.ret) {
		pr_err("mc_pgtable: invoke failed ret %x arg.ret %x\n", rc, arg.ret);
		ret = -EINVAL;
		goto out;
	}

	if(pa_array == NULL)
		pr_err(" no mem pa_array\n");

	memcpy(pa_array, (unsigned long *)shm_va, shm_len);


out:
	if (param)
		kfree(param);

	if (shm_buffer)
		tee_shm_free(shm_buffer);

	if (ret)
	{
		return ret;
	}
	else
	{
		return 0;
	}
}
int mc_pgtable_set_entries (enum remap_module_id module_id)
{
	int ret = 0, rc = 0;
	struct tee_ioctl_invoke_arg arg;
	struct tee_param *param = NULL;
	int entry_count = 0;

	if(mc_modules[module_id].used)
	{
		pr_err("%s module used %d\n", __func__, mc_modules[module_id].used);
		mc_pgtable_debug(0x1);
		ret = -EINVAL;
		goto out;
	}

	if(mc_pgtable_ta.ctx == NULL) {
		pr_err("mc_pgtable: no ta context\n");
		ret = -EINVAL;
		goto out;
	}

	param = kcalloc(TEE_NUM_PARAM, sizeof(struct tee_param), GFP_KERNEL);
	if(!param)
	{
		pr_err("mc_pgtable no mem param\n");
		ret = -ENOMEM;
		goto out;
	}

	memset(&arg, 0, sizeof(arg));
	arg.func = MC_PGTABLE_CMD_SET_ENTRY;
	arg.session = mc_pgtable_ta.session;
	arg.num_params = TEE_NUM_PARAM;

	entry_count = mc_modules[module_id].entry_count;
	memset(param, 0, sizeof(struct tee_param) * TEE_NUM_PARAM);
	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[0].u.value.a = module_id; // module id
	param[0].u.value.b = 0;
	param[1].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	param[2].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	param[3].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;

	rc = tee_client_invoke_func(mc_pgtable_ta.ctx, &arg, param);
	if (rc || arg.ret) {
		pr_err("mc_pgtable: invoke failed ret %x arg.ret %x\n", rc, arg.ret);
		ret = -EINVAL;
		goto out;
	}
	mc_modules[module_id].used = 1;

out:
	if (param)
		kfree(param);

	if (ret)
		return ret;
	else
		return 0;

}
int mc_pgtable_unset_entries (enum remap_module_id module_id, unsigned long *pa_array)
{
	int ret = 0, rc = 0;
	struct tee_ioctl_invoke_arg arg;
	struct tee_param *param = NULL;
	struct tee_shm *shm_buffer = NULL;
	int shm_len = 0;
	phys_addr_t shm_pa = 0;
	unsigned long *shm_va = 0;
	int entry_count = 0;

	if(!mc_modules[module_id].used)
	{
		pr_err("%s module id=%d used %d\n", __func__, mc_modules[module_id].used);
		mc_pgtable_debug(0x1);
		ret = -EINVAL;
		goto out;
	}

	if(mc_pgtable_ta.ctx == NULL) {
		pr_err("mc_pgtable: no ta context\n");
		ret = -EINVAL;
		goto out;
	}

	param = kcalloc(TEE_NUM_PARAM, sizeof(struct tee_param), GFP_KERNEL);
	if(!param)
	{
		pr_err("mc_pgtable no mem param\n");
		ret = -ENOMEM;
		goto out;
	}


	memset(&arg, 0, sizeof(arg));
	arg.func = MC_PGTABLE_CMD_UNSET_ENTRY;
	arg.session = mc_pgtable_ta.session;
	arg.num_params = TEE_NUM_PARAM;

	entry_count = mc_modules[module_id].entry_count;
	// alloc share memory
	shm_len = (sizeof(unsigned long) * entry_count);
	shm_buffer = tee_shm_alloc(mc_pgtable_ta.ctx, shm_len, TEE_SHM_MAPPED);
	if (shm_buffer == NULL) {
		pr_err("mc_pgtable: no shm_buffer\n");
		ret = -ENOMEM;
		goto out;
	}

	// get share memory virtual addr for data accessing
	shm_va = tee_shm_get_va(shm_buffer, 0);
	if (shm_va == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	// get share memory physial addr for tee param
	rc = tee_shm_get_pa(shm_buffer, 0, &shm_pa);
	if (rc) {
		ret = -ENOMEM;
		goto out;
	}

	// alloc share memory
	memset(param, 0, sizeof(struct tee_param) * TEE_NUM_PARAM);
	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[0].u.value.a = module_id; // module id
	param[0].u.value.b = 0;
	param[1].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_OUTPUT;
	param[1].u.memref.shm = shm_buffer;
	param[1].u.memref.size = shm_len;
	param[2].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	param[3].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;

	rc = tee_client_invoke_func(mc_pgtable_ta.ctx, &arg, param);
	if (rc || arg.ret) {
		pr_err("mc_pgtable: invoke failed ret %x arg.ret %x\n", rc, arg.ret);
		ret = -EINVAL;
		goto out;
	}
	mc_modules[module_id].used = 0;
	if(pa_array == NULL)
		pr_err("mc_pgtable no mem\n");

	memcpy(pa_array, (unsigned long *)shm_va, shm_len);

out:
	if (param)
		kfree(param);

	if (shm_buffer)
		tee_shm_free(shm_buffer);

	if (ret)
		return ret;
	else
		return 0;
}
int mc_pgtable_debug(int cmd)
{
	int ret = 0, rc = 0;
	struct tee_ioctl_invoke_arg arg;
	struct tee_param *param = NULL;

	if(mc_pgtable_ta.ctx == NULL) {
		pr_err("mc_pgtable: no ta context\n");
		ret = -EINVAL;
		goto out;
	}

	param = kcalloc(TEE_NUM_PARAM, sizeof(struct tee_param), GFP_KERNEL);
	if(!param)
	{
		pr_err("mc_pgtable no mem param\n");
		ret = -ENOMEM;
		goto out;
	}

	memset(&arg, 0, sizeof(arg));
	arg.func = MC_PGTABLE_CMD_DEBUG;
	arg.session = mc_pgtable_ta.session;
	arg.num_params = TEE_NUM_PARAM;

	memset(param, 0, sizeof(struct tee_param) * TEE_NUM_PARAM);
	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[0].u.value.a = cmd;
	param[0].u.value.b = 0;
	param[1].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	param[2].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	param[3].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;

	rc = tee_client_invoke_func(mc_pgtable_ta.ctx, &arg, param);
	if (rc || arg.ret) {
		pr_err("mc_pgtable: invoke failed ret %x arg.ret %x\n", rc, arg.ret);
		ret = -EINVAL;
		goto out;
	}

out:
	if (param)
		kfree(param);

	if (ret)
		return ret;
	else
		return 0;

}
