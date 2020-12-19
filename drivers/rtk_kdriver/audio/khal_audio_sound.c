#include <linux/string.h>
#include <linux/unistd.h>
#include <linux/delay.h>

/* khal audio */
#include <hal_common.h>
#include <hal_audio.h>

#include "hresult.h"
#include "audio_flow.h"
#include "audio_rpc.h"
#include "audio_inc.h"
#include "kadp_audio.h"
#include "rtk_kdriver/rtkaudio_debug.h"
#include "audio_base.h"

#define UNCACHEABLE_VADDR(paddr)    (typeof(paddr))(((int)paddr)|0xA0000000)

//#define DTS_VX_ENABLED
//
/*#define LGSE_PRINT*/
/*#define LGSE_DAP_DEBUG*/
//===============================================================================

#define MS12_v2_4

#ifdef MS12_v2_4
/* ============================ for DAP API ==================================================*/
#define HAL_MS_MIN_MAX_CHANNEL     6                 /**< Minimum value for max. number of channels        */
#define HAL_MS_MAX_MAX_CHANNEL     8                 /**< Maximum value for max. number of channels        */
#define HAL_DAP_MAX_BANDS          (20)
#define HAL_DAP_IEQ_MAX_BANDS      HAL_DAP_MAX_BANDS
#define HAL_DAP_GEQ_MAX_BANDS      HAL_DAP_MAX_BANDS
#define HAL_DAP_REG_MAX_BANDS      HAL_DAP_MAX_BANDS
#define HAL_DAP_OPT_MAX_BANDS      HAL_DAP_MAX_BANDS
#define HAL_DAP_MAX_CHANNELS       (HAL_MS_MAX_MAX_CHANNEL)

/* ===========================================================================================*/

/* DAP API */
DTV_STATUS_T RHAL_AUDIO_SetDAPParam(AUDIO_ENUM_PRIVAETINFO paramType, void *paramValue, int paramValueSize, int index);
DTV_STATUS_T RHAL_AUDIO_GetDAPParam(AUDIO_ENUM_PRIVAETINFO paramType, void *paramValue, int paramValueSize, int index);

#endif //MS12_v2_4

typedef struct HAL_LGSE_INFO{
	lgse_se_mode_ext_type_t se_mode;
	UINT32 spk_flag;
	UINT32 spk_btsur_flag;
	UINT32 bt_btsur_flag;
} HAL_LGSE_INFO_T;

unsigned int lgse_fn004_cnt = 0;
AUDIO_RPC_LGSE_SETFN004 lgse_config_fn004;
HAL_LGSE_INFO_T g_lgseInfo;

unsigned int g_lgse_fn_debug = 0;
unsigned int g_lgse_dap_debug = 0;
void enable_lgse_fn_debug(unsigned int function_bit)
{
	if(!(g_lgse_fn_debug & function_bit)){
		g_lgse_fn_debug |= function_bit;
		AUDIO_FATAL("[%s] enable LGSE log flag:0x%x\n",__FUNCTION__, g_lgse_fn_debug);
	}else{
		g_lgse_fn_debug &= g_lgse_fn_debug & (~function_bit);
		AUDIO_FATAL("[%s] disable LGSE log flag:0x%x\n",__FUNCTION__, g_lgse_fn_debug);
	}
}
void enable_lgse_dap_debug(unsigned int function_bit)
{
	if(!(g_lgse_dap_debug & function_bit)){
		g_lgse_dap_debug |= function_bit;
		AUDIO_FATAL("[%s] enable LGSE dap log flag:0x%x\n",__FUNCTION__, g_lgse_dap_debug);
	}else{
		g_lgse_dap_debug &= g_lgse_dap_debug & (~function_bit);
		AUDIO_FATAL("[%s] disable LGSE dap log flag:0x%x\n",__FUNCTION__, g_lgse_dap_debug);
	}
}

void init_lgse_gloal_var(void)
{
	// LGSE init for Fn004
	memset(&lgse_config_fn004, 0, sizeof(AUDIO_RPC_LGSE_SETFN004));
}
/* SE */

DTV_STATUS HAL_AUDIO_LGSE_Init(lgse_se_init_ext_type_t se_init)
{
	char para[20];
	UINT32 res = S_FALSE;
	UINT32 len = sizeof(para);
	AUDIO_INFO("[AUDH] %s  mode %d \n", __FUNCTION__, se_init);

	if(se_init == LGSE_INIT_LGSE_ONLY)
		return OK;

	switch (se_init) {
		case LGSE_INIT_LGSE_VX:
		case LGSE_INIT_LGSE_VX_AISOUND:
			sprintf(para,"lgse_mode dts");
			res = RTKAUDIO_RPC_TOAGENT_DEBUG_SVC(para, len);
			break;
		case LGSE_INIT_LGSE_ATMOS:
		case LGSE_INIT_LGSE_ATMOS_AISOUND:
			sprintf(para,"lgse_mode dap");
			res = RTKAUDIO_RPC_TOAGENT_DEBUG_SVC(para, len);
			break;
		default:
			AUDIO_INFO("Unknown init value %d\n", se_init);
			break;
	}
	if(res != S_OK){
		AUDIO_ERROR("[%s:%d] RPC return != S_OK\n",__FUNCTION__,__LINE__);
		return NOT_OK;
	}
	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_SetMode(UINT32 pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption, int index)
{
	UINT32 res;
	AUDIO_RPC_LGSE_SETMODE lgse_config;
#ifdef LGSE_PRINT
	AUDIO_INFO("[AUDH] %s argu0 = %x,argu1 = %d,argu2= %d \n", __FUNCTION__,pParams,noParam,dataOption );
#endif
	AUDIO_DEBUG("[%s] mode %x, index %d\n", __FUNCTION__, pParams, index);

	memset(&lgse_config, 0, sizeof(AUDIO_RPC_LGSE_SETMODE));
	lgse_config.instanceID = (long)index; //for kcontrol
	// LGSE parameter setting.
	/*lgse_config.flag = pParams[0];*/
	lgse_config.flag = pParams; // only for test

	//how to use : pParams[1] & pParams[2]???
	lgse_config.dataOption = dataOption;

	res = RTKAUDIO_RPC_TOAGENT_AO_LGSE_SETMODE_SVC(&lgse_config);
	if(res != S_OK){
		AUDIO_ERROR("[%s:%d] RPC return != S_OK\n",__func__,__LINE__);
		return NOT_OK;
	}

	if(index >= 10 && index < 20) {
		g_lgseInfo.spk_btsur_flag = pParams;
	} else if (index >= 20 && index < 30) {
		g_lgseInfo.bt_btsur_flag = pParams;
	} else {
		g_lgseInfo.spk_flag = pParams;
	}

	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_SetMain(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption, int index)
{
	UINT32 res;
	AUDIO_RPC_LGSE_SETMAIN lgse_config;
	if((g_lgse_fn_debug >> DEBUG_LGSE_MAIN) & 0x1){
		UINT32 i;
		UINT32 *pParam = (UINT32*)pParams;
		AUDIO_INFO("[AUDH] %s argu0 = %x,argu1 = %d,argu2= %d \n", __FUNCTION__,(UINT32)pParams,noParam,dataOption );
		for(i=0; i<noParam; i++) {
			AUDIO_INFO("LGSE: pParams[%d] = %x\n", i, pParam[i]);
		}
	}
	memset(&lgse_config, 0, sizeof(AUDIO_RPC_LGSE_SETMAIN));
	lgse_config.instanceID = (long)index; //for kcontrol
	// LGSE parameter setting.
	lgse_config.dataOption = (short)dataOption;
	// deliver Init & Var to lgse_config

	switch(dataOption){
	case HAL_AUDIO_LGSE_INIT_ONLY:
		if(noParam == (sizeof(AUDIO_RPC_LGSE00861)/sizeof(UINT32))){
			IPC_memcpy((long*)&(lgse_config.dataInit),(long*)pParams, noParam);
		}else{
			AUDIO_INFO("[AUDH] %s Init data size isn't correct(%d != %d)!\n\n", __FUNCTION__,noParam,sizeof(AUDIO_RPC_LGSE00861)/sizeof(UINT32));
			return NOT_OK;
		}
		break;
	case HAL_AUDIO_LGSE_VARIABLES:
		if(noParam == (sizeof(AUDIO_RPC_LGSE00876)/sizeof(UINT32))){
			IPC_memcpy((long*)&(lgse_config.dataVar),(long*)pParams, noParam);
		}else{
			AUDIO_INFO("[AUDH] %s Var data size isn't correct(%d != %d)!\n\n", __FUNCTION__,noParam,sizeof(AUDIO_RPC_LGSE00876)/sizeof(UINT32));
			return NOT_OK;
		}
		break;
	case HAL_AUDIO_LGSE_ALL: //need to check.
		if(noParam == ((sizeof(AUDIO_RPC_LGSE00861)/sizeof(UINT32))+(sizeof(AUDIO_RPC_LGSE00876)/sizeof(UINT32)))){

		}else{
			//printf("both of data size isn't correct(%d != %d)!\n",noParam,(sizeof(AUDIO_RPC_LGSE00861)/sizeof(UINT32))+(sizeof(AUDIO_RPC_LGSE00876)/sizeof(UINT32)));
			//return NOT_OK;
		}
		AUDIO_INFO("[AUDH] %s Warning(this case isn't ready!)\n", __FUNCTION__);
		return NOT_OK;
		break;
	default:
		AUDIO_INFO("[AUDH] %s The setting is out of range(>2)!\n", __FUNCTION__);
		return NOT_OK;
		break;
	}

	res = RTKAUDIO_RPC_TOAGENT_AO_LGSE_SETMAIN_SVC(&lgse_config);
	if(res != S_OK){
		AUDIO_ERROR("[%s:%d] RPC return != S_OK\n",__func__,__LINE__);
		return NOT_OK;
	}
	return OK;
}

extern long rtkaudio_alloc(int size, UINT8 *vir_addr_info, UINT8 **uncached_addr_info);
extern int rtkaudio_free(unsigned int phy_addr);

DTV_STATUS_T HAL_AUDIO_LGSE_SetFN000(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption, int index)
{
	UINT32 res;
	UINT8* nonCacheAddr = NULL;
	UINT32 PhyAddr = 0;
	AUDIO_RPC_LGSE_SETFN000 lgse000;
	if((g_lgse_fn_debug >> DEBUG_LGSE_FN000) & 0x1){
		UINT32 i;
		UINT32 *pParam = (UINT32*)pParams;
		AUDIO_INFO("[AUDH] %s argu0 = %x,argu1 = %d,argu2= %d \n", __FUNCTION__,(UINT32)pParams,noParam,dataOption);
		for(i=0; i<noParam; i++) {
			AUDIO_INFO("LGSE: pParams[%d] = %x\n", i, pParam[i]);
		}
	}

	memset(&lgse000, 0, sizeof(AUDIO_RPC_LGSE_SETFN000));
	PhyAddr = (UINT32)rtkaudio_alloc(sizeof(AUDIO_RPC_LGSE00855), NULL, &nonCacheAddr);

	lgse000.rpc_LGSE00855_set_address = UNCACHEABLE_VADDR(PhyAddr); //change physical address to uncachable for FW
	lgse000.instanceID = (long)index; //for kcontrol
	lgse000.dataOption = dataOption;

	switch(dataOption){
	case HAL_AUDIO_LGSE_INIT_ONLY:
		if(noParam == (sizeof(AUDIO_RPC_LGSE00855)/sizeof(UINT32))){
			IPC_memcpy((long*)nonCacheAddr, (long*)pParams, noParam);
		}else{
			AUDIO_INFO("[AUDH] %s Init data size isn't correct(%d != %d)!\n\n", __FUNCTION__,noParam,sizeof(AUDIO_RPC_LGSE00855)/sizeof(UINT32));
			return NOT_OK;
		}
		break;
	case HAL_AUDIO_LGSE_VARIABLES:
		if(noParam == (sizeof(AUDIO_RPC_LGSE00870)/sizeof(UINT32))){
			IPC_memcpy((long*)&(lgse000.dataVar._LGSE00754),(long*)pParams, noParam);
		}else{
			AUDIO_INFO("[AUDH] %s Var data size isn't correct(%d != %d)!\n\n", __FUNCTION__,noParam,sizeof(AUDIO_RPC_LGSE00870)/sizeof(UINT32));
			return NOT_OK;
		}
		break;
	case HAL_AUDIO_LGSE_ALL: //need to check.
		if(noParam == ((sizeof(AUDIO_RPC_LGSE00855)/sizeof(UINT32))+(sizeof(AUDIO_RPC_LGSE00870)/sizeof(UINT32)))){

		}else{

		}
		AUDIO_INFO("[AUDH] %s Warning(this case isn't ready!)\n", __FUNCTION__);
		return NOT_OK;
		break;
	default:
		AUDIO_INFO("[AUDH] %s The setting is out of range(>2)!\n", __FUNCTION__);
		return NOT_OK;
		break;
	}

	//pli_flushMemory;

	res = RTKAUDIO_RPC_TOAGENT_AO_LGSE_SETFN000_SVC(&lgse000);
	rtkaudio_free(PhyAddr);
	if(res != S_OK){
		AUDIO_ERROR("[%s:%d] RPC return != S_OK\n",__func__,__LINE__);
		return NOT_OK;
	}
	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_SetFN000_1(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption, int index)
{
	return NOT_OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_SetFN001(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption, int index)
{
	UINT32 res;
	UINT8* nonCacheAddr = NULL;
	UINT32 PhyAddr = 0;
	AUDIO_RPC_LGSE_SETFN001 lgse001;
	if((g_lgse_fn_debug >> DEBUG_LGSE_FN001) & 0x1){
		UINT32 i;
		UINT32 *pParam = (UINT32*)pParams;
		AUDIO_INFO("[AUDH] %s argu0 = %x,argu1 = %d,argu2= %d \n", __FUNCTION__,(UINT32)pParams,noParam,dataOption );
		for(i=0; i<noParam; i++) {
			AUDIO_INFO("LGSE: pParams[%d] = %x\n", i, pParam[i]);
		}
	}

	memset(&lgse001, 0, sizeof(AUDIO_RPC_LGSE_SETFN001));
	PhyAddr = (UINT32)rtkaudio_alloc(sizeof(AUDIO_RPC_LGSE00869), NULL, &nonCacheAddr);

	lgse001.rpc_LGSE00869_set_address = UNCACHEABLE_VADDR(PhyAddr); //change physical address to uncachable for FW
	lgse001.instanceID = (long)index; //for kcontrol
	lgse001.dataOption = dataOption;

	switch(dataOption){
	case HAL_AUDIO_LGSE_INIT_ONLY:
		if(noParam == (sizeof(AUDIO_RPC_LGSE00854)/sizeof(UINT32))){
			IPC_memcpy((long*)&(lgse001.dataInit),(long*)pParams, noParam);
		}else{
			AUDIO_INFO("[AUDH] %s Init data size isn't correct(%d != %d)!\n\n", __FUNCTION__,noParam,sizeof(AUDIO_RPC_LGSE00854)/sizeof(UINT32));
			return NOT_OK;
		}
		break;
	case HAL_AUDIO_LGSE_VARIABLES:
		if(noParam == (sizeof(AUDIO_RPC_LGSE00869)/sizeof(UINT32))){
			IPC_memcpy((long*)nonCacheAddr, (long*)pParams, noParam);
		}else{
			AUDIO_INFO("[AUDH] %s Var data size isn't correct(%d != %d)!\n\n", __FUNCTION__,noParam,sizeof(AUDIO_RPC_LGSE00869)/sizeof(UINT32));
			return NOT_OK;
		}
		break;
	case HAL_AUDIO_LGSE_ALL: //need to check.
		if(noParam == ((sizeof(AUDIO_RPC_LGSE00854)/sizeof(UINT32))+(sizeof(AUDIO_RPC_LGSE00869)/sizeof(UINT32)))){
		}else{
		}
		AUDIO_INFO("[AUDH] %s Warning(this case isn't ready!)\n", __FUNCTION__);
		return NOT_OK;
		break;
	default:
		AUDIO_INFO("[AUDH] %s The setting is out of range(>2)!\n", __FUNCTION__);
		return NOT_OK;
		break;
	}

	//pli_flushMemory;

	res = RTKAUDIO_RPC_TOAGENT_AO_LGSE_SETFN001_SVC(&lgse001);
	rtkaudio_free(PhyAddr);
	if(res != S_OK){
		AUDIO_ERROR("[%s:%d] RPC return != S_OK\n",__func__,__LINE__);
		return NOT_OK;
	}
	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_SetFN002(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption, int index)
{
	AUDIO_INFO("[AUDH] %s not implemnet!\n", __FUNCTION__);
	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_SetFN002_1(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption, int index)
{
	AUDIO_INFO("[AUDH] %s not implemnet!\n", __FUNCTION__);
	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_SetFN004(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption,        \
                                     HAL_AUDIO_LGSE_VARIABLE_MODE_T varOption, int index)
{
	UINT32 res;
	if((g_lgse_fn_debug >> DEBUG_LGSE_FN004) & 0x1){
		UINT32 i;
		UINT32 *pParam = (UINT32*)pParams;
		AUDIO_INFO("[AUDH] %s argu0 = %x,argu1 = %d,argu2= %d ,arg3=%d\n", __FUNCTION__,(UINT32)pParams,noParam,dataOption,varOption );
		for(i=0; i<noParam; i++) {
			AUDIO_INFO("LGSE: pParams[%d] = %x\n", i, pParam[i]);
		}
	}

	lgse_config_fn004.instanceID = (long)index; //for kcontrol
	// LGSE parameter setting.
	lgse_config_fn004.dataOption = dataOption;
	// deliver Init & Var to lgse_config
	switch(dataOption){
	case HAL_AUDIO_LGSE_INIT_ONLY:
		AUDIO_INFO("[AUDH] %s No use!\n", __FUNCTION__);
		return OK;
		break;
	case HAL_AUDIO_LGSE_VARIABLES:
		if((noParam == 25) && (varOption == 5)){
			memcpy((void*)&(lgse_config_fn004.data[0]),(void*)pParams, sizeof(int)*25);
			lgse_fn004_cnt = 5;
		}else if((noParam == 5) && (varOption != 5)){
			if(varOption == 0){
				memcpy((void*)&(lgse_config_fn004.data[0]),(void*)pParams, sizeof(int)*5);
				lgse_fn004_cnt ++;
			}else if(varOption == 1){
				memcpy((void*)&(lgse_config_fn004.data[5]),(void*)pParams, sizeof(int)*5);
				lgse_fn004_cnt ++;
			}else if(varOption == 2){
				memcpy((void*)&(lgse_config_fn004.data[10]),(void*)pParams, sizeof(int)*5);
				lgse_fn004_cnt ++;
			}else if(varOption == 3){
				memcpy((void*)&(lgse_config_fn004.data[15]),(void*)pParams, sizeof(int)*5);
				lgse_fn004_cnt ++;
			}else if(varOption == 4){
				memcpy((void*)&(lgse_config_fn004.data[20]),(void*)pParams, sizeof(int)*5);
				lgse_fn004_cnt ++;
			}else{
				AUDIO_INFO("[AUDH] %s varOption value isn't correct!\n\n",__FUNCTION__);
				return NOT_OK;
			}
		}else{
			AUDIO_INFO("[AUDH] %s Var data size isn't correct(%d != %d)!\n\n", __FUNCTION__,noParam,(sizeof(int)*25)/sizeof(UINT32));
			return NOT_OK;
		}
		break;
	case HAL_AUDIO_LGSE_ALL: //need to check.
		AUDIO_INFO("[AUDH] %s No use!\n", __FUNCTION__);
		return NOT_OK;
		break;
	default:
		AUDIO_INFO("[AUDH] %s The setting is out of range(>2)!\n", __FUNCTION__);
		return NOT_OK;
		break;
	}
	if(lgse_fn004_cnt == 5){
		res = RTKAUDIO_RPC_TOAGENT_AO_LGSE_SETFN004_SVC(&lgse_config_fn004);
		if(res != S_OK){
			AUDIO_ERROR("[%s:%d] RPC return != S_OK\n",__func__,__LINE__);
			return NOT_OK;
		}else{
			lgse_fn004_cnt = 0;
		}
	}
	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_SetFN004_1(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption, HAL_AUDIO_LGSE_VARIABLE_MODE_T varOption, int index)
{
	AUDIO_INFO("[AUDH] %s not implemnet!\n", __FUNCTION__);
	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_SetFN005(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption, int index)
{
	UINT32 res;
	AUDIO_RPC_LGSE_SETFN005 lgse_config;
	if((g_lgse_fn_debug >> DEBUG_LGSE_FN005) & 0x1){
		UINT32 i;
		UINT32 *pParam = (UINT32*)pParams;
		AUDIO_INFO("[AUDH] %s argu0 = %x,argu1 = %d,argu2= %d \n", __FUNCTION__,(UINT32)pParams,noParam,dataOption );
		for(i=0; i<noParam; i++) {
			AUDIO_INFO("LGSE: pParams[%d] = %x\n", i, pParam[i]);
		}
	}

	memset(&lgse_config, 0, sizeof(AUDIO_RPC_LGSE_SETFN005));
	lgse_config.instanceID = (long)index; //for kcontrol
	// LGSE parameter setting.
	lgse_config.dataOption = dataOption;

	// deliver Init & Var to lgse_config

	switch(dataOption){
	case HAL_AUDIO_LGSE_INIT_ONLY:
		AUDIO_INFO("[AUDH] %s No use!\n", __FUNCTION__);
		return OK;
		break;
	case HAL_AUDIO_LGSE_VARIABLES:
		if(noParam == 15){
			memcpy((void*)&(lgse_config.data[0]),(void*)pParams, sizeof(int)*15);
		}else{
			AUDIO_INFO("[AUDH] %s Var data size isn't correct(%d != %d)!\n\n", __FUNCTION__,noParam,(sizeof(int)*15)/sizeof(UINT32));
			return NOT_OK;
		}
		break;
	case HAL_AUDIO_LGSE_ALL: //need to check.
		AUDIO_INFO("[AUDH] %s No use!\n", __FUNCTION__);
		return NOT_OK;
		break;
	default:
		AUDIO_INFO("[AUDH] %s The setting is out of range(>2)!\n", __FUNCTION__);
		return NOT_OK;
		break;
	}
	res = RTKAUDIO_RPC_TOAGENT_AO_LGSE_SETFN005_SVC(&lgse_config);
	if(res != S_OK){
		AUDIO_ERROR("[%s:%d] RPC return != S_OK\n",__func__,__LINE__);
		return NOT_OK;
	}
	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_SetFN008(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption, int index)
{
	UINT32 res;
	UINT8* nonCacheAddr = NULL;
	UINT32 PhyAddr = 0;
	AUDIO_RPC_LGSE_SETFN008 lgse008;
	if((g_lgse_fn_debug >> DEBUG_LGSE_FN008) & 0x1){
		UINT32 i;
		UINT32 *pParam = (UINT32*)pParams;
		AUDIO_INFO("[AUDH] %s argu0 = %x,argu1 = %d,argu2= %d \n", __FUNCTION__,(UINT32)pParams,noParam,dataOption );
		for(i=0; i<noParam; i++) {
			AUDIO_INFO("LGSE: pParams[%d] = %x\n", i, pParam[i]);
		}
	}

	memset(&lgse008, 0, sizeof(AUDIO_RPC_LGSE_SETFN008));
	PhyAddr = (UINT32)rtkaudio_alloc(sizeof(AUDIO_RPC_LGSE00858), NULL, &nonCacheAddr);

	lgse008.rpc_LGSE00858_set_address = UNCACHEABLE_VADDR(PhyAddr); //change physical address to uncachable for FW
	lgse008.instanceID = (long)index; //for kcontrol
	lgse008.dataOption = dataOption;

	switch(dataOption){
	case HAL_AUDIO_LGSE_INIT_ONLY:
		if(noParam == (sizeof(AUDIO_RPC_LGSE00858)/sizeof(UINT32))){
			IPC_memcpy((long*)nonCacheAddr, (long*)pParams, noParam);
		}else{
			AUDIO_INFO("[AUDH] %s Init data size isn't correct(%d != %d)!\n\n", __FUNCTION__,noParam,sizeof(AUDIO_RPC_LGSE00858)/sizeof(UINT32));
			return NOT_OK;
		}
		break;
	case HAL_AUDIO_LGSE_VARIABLES:
		if(noParam == (sizeof(AUDIO_RPC_LGSE00873)/sizeof(UINT32))){
			IPC_memcpy((long*)&(lgse008.dataVar),(long*)pParams, noParam);
		}else{
			AUDIO_INFO("[AUDH] %s Var data size isn't correct(%d != %d)!\n\n", __FUNCTION__,noParam,sizeof(AUDIO_RPC_LGSE00873)/sizeof(UINT32));
			return NOT_OK;
		}
		break;
	case HAL_AUDIO_LGSE_ALL: //need to check.
		if(noParam == ((sizeof(AUDIO_RPC_LGSE00858)/sizeof(UINT32))+(sizeof(AUDIO_RPC_LGSE00873)/sizeof(UINT32)))){

		}else{

		}
		AUDIO_INFO("[AUDH] %s Warning(this case isn't ready!)\n", __FUNCTION__);
		return NOT_OK;
		break;
	default:
		AUDIO_INFO("[AUDH] %s The setting is out of range(>2)!\n", __FUNCTION__);
		return NOT_OK;
		break;
	}

	//pli_flushMemory;
	res = RTKAUDIO_RPC_TOAGENT_AO_LGSE_SETFN008_SVC(&lgse008);
	rtkaudio_free(PhyAddr);
	if(res != S_OK){
		AUDIO_ERROR("[%s:%d] RPC return != S_OK\n",__func__,__LINE__);
		return NOT_OK;
	}
	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_SetFN008_1(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption, int index)
{
	AUDIO_INFO("[AUDH] %s not implemnet!\n", __FUNCTION__);
	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_SetFN009(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption, int index)
{
	UINT32 res;
	AUDIO_RPC_LGSE_SETFN009 lgse_config;
	if((g_lgse_fn_debug >> DEBUG_LGSE_FN009) & 0x1){
		UINT32 i;
		UINT32 *pParam = (UINT32*)pParams;
		AUDIO_INFO("[AUDH] %s argu0 = %x,argu1 = %d,argu2= %d \n", __FUNCTION__,(UINT32)pParams,noParam,dataOption );
		for(i=0; i<noParam; i++) {
			AUDIO_INFO("LGSE: pParams[%d] = %x\n", i, pParam[i]);
		}
	}

	memset(&lgse_config, 0, sizeof(AUDIO_RPC_LGSE_SETFN009));
	lgse_config.instanceID = (long)index; //for kcontrol
	// LGSE parameter setting.
	lgse_config.dataOption = (short)dataOption;
	// deliver Init & Var to lgse_config

	switch(dataOption){
	case HAL_AUDIO_LGSE_INIT_ONLY:
		if(noParam == (sizeof(AUDIO_RPC_LGSE00857)/sizeof(UINT32))){
			//fix coverity:Event overrun-buffer-arg:   Overrunning buffer pointed to by "(void *)&lgse_config.dataInit._LGSE00762[0]" of 8 bytes by passing it to a function which accesses it at byte offset 39 using argument "40U".
			//memcpy((void*)&(lgse_config.dataInit._LGSE00762[0]),(void*)pParams, sizeof(AUDIO_RPC_LGSE00857));
			IPC_memcpy((long*)&(lgse_config.dataInit), (long*)pParams, noParam);
		}else{
			AUDIO_INFO("[AUDH] %s Init data size isn't correct(%d != %d)!\n\n", __FUNCTION__,noParam,sizeof(AUDIO_RPC_LGSE00857)/sizeof(UINT32));
			return NOT_OK;
		}
		break;
	case HAL_AUDIO_LGSE_VARIABLES:
		if(noParam == (sizeof(AUDIO_RPC_LGSE00872)/sizeof(UINT32))){
			IPC_memcpy((long*)&(lgse_config.dataVar), (long*)pParams, noParam);
		}else{
			AUDIO_INFO("[AUDH] %s Var data size isn't correct(%d != %d)!\n\n", __FUNCTION__,noParam,sizeof(AUDIO_RPC_LGSE00872)/sizeof(UINT32));
			return NOT_OK;
		}
		break;
	case HAL_AUDIO_LGSE_ALL: //need to check.
		if(noParam == ((sizeof(AUDIO_RPC_LGSE00857)/sizeof(UINT32))+(sizeof(AUDIO_RPC_LGSE00872)/sizeof(UINT32)))){

		}else{

		}
		AUDIO_INFO("[AUDH] %s Warning(this case isn't ready!)\n", __FUNCTION__);
		return NOT_OK;
		break;
	default:
		AUDIO_INFO("[AUDH] %s The setting is out of range(>2)!\n", __FUNCTION__);
		return NOT_OK;
		break;
	}
	res = RTKAUDIO_RPC_TOAGENT_AO_LGSE_SETFN009_SVC(&lgse_config);
	if(res != S_OK){
		AUDIO_ERROR("[%s:%d] RPC return != S_OK\n",__func__,__LINE__);
		return NOT_OK;
	}
	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_SetFN010(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_ACCESS_T dataOption, int index)
{
	UINT32 res;
	AUDIO_RPC_LGSE_SETFN010  lgse_config;
	AUDIO_RPC_LGSE_RETURNVAL lgse_ret;
	if((g_lgse_fn_debug >> DEBUG_LGSE_FN010) & 0x1){
		UINT32 i;
		UINT32 *pParam = (UINT32*)pParams;
		AUDIO_INFO("[AUDH] %s argu0 = %x,argu1 = %d,argu2= %d \n", __FUNCTION__,(UINT32)pParams,noParam,dataOption );
		for(i=0; i<noParam; i++) {
			AUDIO_INFO("LGSE: pParams[%d] = %x\n", i, pParam[i]);
		}
	}

	memset(&lgse_config, 0, sizeof(AUDIO_RPC_LGSE_SETFN010));
	lgse_config.instanceID = (long)index; //for kcontrol
	// LGSE parameter setting.
	lgse_config.dataOption = dataOption;

	// deliver Init & Var to lgse_config
	switch(dataOption){
	case HAL_AUDIO_LGSE_WRITE:
		// setFN010Var()
		if(noParam == (sizeof(AUDIO_RPC_LGSE00859)/sizeof(UINT32))){
			IPC_memcpy((long*)&(lgse_config.dataInit),(long*)pParams, noParam);
		}else{
			AUDIO_INFO("[AUDH] %s Var data size isn't correct(%d != %d)!\n\n", __FUNCTION__,noParam,sizeof(AUDIO_RPC_LGSE00859)/sizeof(UINT32));
			return NOT_OK;
		}
		res = RTKAUDIO_RPC_TOAGENT_AO_LGSE_SETFN010_SVC(&lgse_config, &lgse_ret);
		if(res != S_OK){
			AUDIO_ERROR("[%s:%d] RPC return != S_OK\n",__func__,__LINE__);
			return NOT_OK;
		}else if(lgse_ret.ret != S_OK){
			AUDIO_ERROR("[%s:%d] RPC return value!= S_OK\n",__func__,__LINE__);
			return NOT_OK;
		}
		break;
	case HAL_AUDIO_LGSE_READ:
		// getFN010Out()
		if(noParam != sizeof(AUDIO_RPC_LGSE00874)/sizeof(UINT32)){
			AUDIO_INFO("[AUDH] %s Out data size isn't correct(%d != %d)!\n\n", __FUNCTION__,noParam,sizeof(AUDIO_RPC_LGSE00874)/sizeof(UINT32));
			return NOT_OK;
		}
		res = RTKAUDIO_RPC_TOAGENT_AO_LGSE_SETFN010_SVC(&lgse_config, &lgse_ret);
		if(res != S_OK){
			AUDIO_ERROR("[%s:%d] RPC return != S_OK\n",__func__,__LINE__);
			return NOT_OK;
		}else{
			memcpy(pParams,&lgse_ret,sizeof(AUDIO_RPC_LGSE_RETURNVAL));
		}
		break;
	default:
		AUDIO_INFO("[AUDH] %s The setting is out of range(%d>1)!\n", __FUNCTION__, dataOption);
		return NOT_OK;
		break;
	}

	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_SetFN014(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption, int index)
{
	UINT32 res;
	UINT8* nonCacheAddr = NULL;
	UINT32 PhyAddr = 0;
	AUDIO_RPC_LGSE_SETFN014 lgse014;
	if((g_lgse_fn_debug >> DEBUG_LGSE_FN014) & 0x1){
		UINT32 i;
		UINT32 *pParam = (UINT32*)pParams;
		AUDIO_INFO("[AUDH] %s argu0 = %x,argu1 = %d,argu2= %d \n", __FUNCTION__,(UINT32)pParams,noParam,dataOption );
		for(i=0; i<noParam; i++) {
			AUDIO_INFO("LGSE: pParams[%d] = %x\n", i, pParam[i]);
		}
	}

	memset(&lgse014, 0, sizeof(AUDIO_RPC_LGSE_SETFN014));
	PhyAddr = (UINT32)rtkaudio_alloc(sizeof(AUDIO_RPC_LGSE00853), (UINT8*)NULL, &nonCacheAddr);

	lgse014.rpc_LGSE00853_set_address = UNCACHEABLE_VADDR(PhyAddr); //change physical address to uncachable for FW
	lgse014.instanceID = (long)index; //for kcontrol
	lgse014.dataOption = dataOption;

	switch(dataOption){
	case HAL_AUDIO_LGSE_INIT_ONLY:
		if(noParam == (sizeof(AUDIO_RPC_LGSE00853)/sizeof(UINT32))){
			IPC_memcpy((long*)nonCacheAddr, (long*)pParams, noParam);
		}else{
			AUDIO_INFO("[AUDH] %s Init data size isn't correct(%d != %d)!\n\n", __FUNCTION__,noParam,sizeof(AUDIO_RPC_LGSE00853)/sizeof(UINT32));
			return NOT_OK;
		}
		break;
	case HAL_AUDIO_LGSE_VARIABLES:
		if(noParam == (sizeof(AUDIO_RPC_LGSE00868)/sizeof(UINT32))){
			IPC_memcpy((long*)&(lgse014.dataVar._LGSE00008),(long*)pParams, noParam);
		}else{
			AUDIO_INFO("[AUDH] %s Var data size isn't correct(%d != %d)!\n\n", __FUNCTION__,noParam,sizeof(AUDIO_RPC_LGSE00868)/sizeof(UINT32));
			return NOT_OK;
		}
		break;
	case HAL_AUDIO_LGSE_ALL: //need to check.
		if(noParam == ((sizeof(AUDIO_RPC_LGSE00853)/sizeof(UINT32))+(sizeof(AUDIO_RPC_LGSE00868)/sizeof(UINT32)))){

		}else{

		}
		AUDIO_INFO("[AUDH] %s Warning(this case isn't ready!)\n", __FUNCTION__);
		return NOT_OK;
		break;
	default:
		AUDIO_INFO("[AUDH] %s The setting is out of range(>2)!\n", __FUNCTION__);
		return NOT_OK;
		break;
	}

	//pli_flushMemory;
	res = RTKAUDIO_RPC_TOAGENT_AO_LGSE_SETFN014_SVC(&lgse014);
	rtkaudio_free(PhyAddr);
	if(res != S_OK){
		AUDIO_ERROR("[%s:%d] RPC return != S_OK\n",__func__,__LINE__);
		return NOT_OK;
	}
	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_SetFN016(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption, int index)
{
	UINT32 res;
	UINT8* nonCacheAddr_LGSE03520 = NULL;
	UINT32 PhyAddr_LGSE03520 = 0;
	UINT8* nonCacheAddr_LGSE03521 = NULL;
	UINT32 PhyAddr_LGSE03521 = 0;
	AUDIO_RPC_LGSE_SETFN016 lgse016;
	if((g_lgse_fn_debug >> DEBUG_LGSE_FN016) & 0x1){
		UINT32 i;
		UINT32 *pParam = (UINT32*)pParams;
		AUDIO_INFO("[AUDH] %s argu0 = %x,argu1 = %d,argu2= %d \n", __FUNCTION__,(UINT32)pParams,noParam,dataOption );
		for(i=0; i<noParam; i++) {
			AUDIO_INFO("LGSE: pParams[%d] = %x\n", i, pParam[i]);
		}
	}

	memset(&lgse016, 0, sizeof(AUDIO_RPC_LGSE_SETFN016));
	PhyAddr_LGSE03520 = (UINT32)rtkaudio_alloc(sizeof(AUDIO_RPC_LGSE03520), (UINT8*)NULL, &nonCacheAddr_LGSE03520);
	PhyAddr_LGSE03521 = (UINT32)rtkaudio_alloc(sizeof(AUDIO_RPC_LGSE03520), (UINT8*)NULL, &nonCacheAddr_LGSE03521);

	lgse016.rpc_LGSE03520_set_address = UNCACHEABLE_VADDR(PhyAddr_LGSE03520); //change physical address to uncachable for FW
	lgse016.rpc_LGSE03521_set_address = UNCACHEABLE_VADDR(PhyAddr_LGSE03521); //change physical address to uncachable for FW
	lgse016.instanceID = (long)index; //for kcontrol
	lgse016.dataOption = dataOption;

	switch(dataOption){
	case HAL_AUDIO_LGSE_INIT_ONLY:
		if(noParam == (sizeof(AUDIO_RPC_LGSE03520)/sizeof(UINT32))){
			IPC_memcpy((long*)nonCacheAddr_LGSE03520, (long*)pParams, noParam);
		}else{
			AUDIO_INFO("[AUDH] %s Init data size isn't correct(%d != %d)!\n\n", __FUNCTION__,noParam,sizeof(AUDIO_RPC_LGSE03520)/sizeof(UINT32));
			return NOT_OK;
		}
		break;
	case HAL_AUDIO_LGSE_VARIABLES:
		if(noParam == (sizeof(AUDIO_RPC_LGSE03521)/sizeof(UINT32))){
			IPC_memcpy((long*)nonCacheAddr_LGSE03521, (long*)pParams, noParam);
		}else{
			AUDIO_INFO("[AUDH] %s Var data size isn't correct(%d != %d)!\n\n", __FUNCTION__,noParam,sizeof(AUDIO_RPC_LGSE03521)/sizeof(UINT32));
			return NOT_OK;
		}
		break;
	case HAL_AUDIO_LGSE_ALL: //need to check.
		if(noParam == ((sizeof(AUDIO_RPC_LGSE03520)/sizeof(UINT32))+(sizeof(AUDIO_RPC_LGSE03521)/sizeof(UINT32)))){
		}else{
		}
		AUDIO_INFO("[AUDH] %s Warning(this case isn't ready!)\n", __FUNCTION__);
		return NOT_OK;
		break;
	default:
		AUDIO_INFO("[AUDH] %s The setting is out of range(>2)!\n", __FUNCTION__);
		return NOT_OK;
		break;
	}

	//pli_flushMemory;
	res = RTKAUDIO_RPC_TOAGENT_AO_LGSE_SETFN016_SVC(&lgse016);
	rtkaudio_free(PhyAddr_LGSE03520);
	rtkaudio_free(PhyAddr_LGSE03521);
	if(res != S_OK){
		AUDIO_ERROR("[%s:%d] RPC return != S_OK\n",__func__,__LINE__);
		return NOT_OK;
	}
	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_SetFN023(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption, int index)
{
	AUDIO_INFO("[AUDH] %s not implement!\n", __FUNCTION__);
	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_SetFN024(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption, int index)
{
	UINT32 res;
	AUDIO_RPC_LGSE_SETFN024 lgse024;
	if((g_lgse_fn_debug >> DEBUG_LGSE_FN024) & 0x1){
		UINT32 i;
		UINT32 *pParam = (UINT32*)pParams;
		AUDIO_INFO("[AUDH] %s argu0 = %x,argu1 = %d,argu2= %d \n", __FUNCTION__,(UINT32)pParams,noParam,dataOption );
		for(i=0; i<noParam; i++) {
			AUDIO_INFO("LGSE: pParams[%d] = %x\n", i, pParam[i]);
		}
	}
	memset(&lgse024, 0, sizeof(AUDIO_RPC_LGSE_SETFN024));
	lgse024.instanceID = (long)index; //for kcontrol
	// LGSE parameter setting.
	lgse024.dataOption = (short)dataOption;
	// deliver Init & Var to lgse_config

	switch(dataOption){
	case HAL_AUDIO_LGSE_INIT_ONLY:
		if(noParam == (sizeof(AUDIO_RPC_LGSE02624)/sizeof(UINT32))){
			IPC_memcpy((long*)&(lgse024.dataInit),(long*)pParams, noParam);
		}else{
			AUDIO_INFO("[AUDH] %s Init data size isn't correct(%d != %d)!\n\n", __FUNCTION__,noParam,sizeof(AUDIO_RPC_LGSE02624)/sizeof(UINT32));
			return NOT_OK;
		}
		break;
	case HAL_AUDIO_LGSE_VARIABLES:
		AUDIO_INFO("[AUDH] %s No use!\n", __FUNCTION__);
		return OK;
		break;
	case HAL_AUDIO_LGSE_ALL: //need to check.
		AUDIO_INFO("[AUDH] %s No use!\n", __FUNCTION__);
		return NOT_OK;
		break;
	default:
		AUDIO_INFO("[AUDH] %s The setting is out of range(>2)!\n", __FUNCTION__);
		return NOT_OK;
		break;
	}

	res = RTKAUDIO_RPC_TOAGENT_AO_LGSE_SETFN024_SVC(&lgse024);
	if(res != S_OK){
		AUDIO_ERROR("[%s:%d] RPC return != S_OK\n",__func__,__LINE__);
		return NOT_OK;
	}
	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_SetFN017(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption, int index)
{
	UINT32 res;
	UINT8* nonCacheAddr = NULL;
	UINT32 PhyAddr = 0;
	AUDIO_RPC_LGSE_SETFN017 lgse017;
	if((g_lgse_fn_debug >> DEBUG_LGSE_FN017) & 0x1){
		UINT32 i;
		UINT32 *pParam = (UINT32*)pParams;
		AUDIO_INFO("[AUDH] %s argu0 = %x,argu1 = %d,argu2= %d \n", __FUNCTION__,(UINT32)pParams,noParam,dataOption );
		for(i=0; i<noParam; i++) {
			AUDIO_INFO("LGSE: pParams[%d] = %x\n", i, pParam[i]);
		}
	}

	memset(&lgse017, 0, sizeof(AUDIO_RPC_LGSE_SETFN017));
	PhyAddr = (UINT32)rtkaudio_alloc(sizeof(AUDIO_RPC_LGSE00879), (UINT8*)NULL, &nonCacheAddr);

	lgse017.rpc_LGSE00879_set_address = UNCACHEABLE_VADDR(PhyAddr); //change physical address to uncachable for FW
	lgse017.instanceID = (long)index; //for kcontrol
	lgse017.dataOption = dataOption;

	switch(dataOption){
	case HAL_AUDIO_LGSE_INIT_ONLY:
		AUDIO_INFO("[AUDH] %s No use!\n", __FUNCTION__);
		return OK;
		break;
	case HAL_AUDIO_LGSE_VARIABLES:
		if(noParam == (sizeof(AUDIO_RPC_LGSE00879)/sizeof(UINT32))){
			IPC_memcpy((long*)nonCacheAddr, (long*)pParams, noParam);
		}else{
			AUDIO_INFO("[AUDH] %s Init data size isn't correct(%d != %d)!\n\n", __FUNCTION__,noParam,sizeof(AUDIO_RPC_LGSE00879)/sizeof(UINT32));
			return NOT_OK;
		}
		break;
	case HAL_AUDIO_LGSE_ALL: //need to check.
		AUDIO_INFO("[AUDH] %s No use!\n", __FUNCTION__);
		return NOT_OK;
		break;
	default:
		AUDIO_INFO("[AUDH] %s The setting is out of range(>2)!\n", __FUNCTION__);
		return NOT_OK;
		break;
	}

	//pli_flushMemory;
	res = RTKAUDIO_RPC_TOAGENT_AO_LGSE_SETFN017_SVC(&lgse017);
	rtkaudio_free(PhyAddr);
	if(res != S_OK){
		AUDIO_ERROR("[%s:%d] RPC return != S_OK\n",__func__,__LINE__);
		return NOT_OK;
	}
	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_SetFN017_1(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption, int index)
{
	AUDIO_INFO("[AUDH] %s not implemnet!\n", __FUNCTION__);
	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_SetFN017_2(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption, int index)
{
	AUDIO_INFO("[AUDH] %s not implemnet!\n", __FUNCTION__);
	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_SetFN017_3(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption, int index)
{
	AUDIO_INFO("[AUDH] %s not implemnet!\n", __FUNCTION__);
	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_SetFN019(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption, int index)
{
	UINT32 res;
	UINT8* nonCacheAddr = NULL;
	UINT32 PhyAddr = 0;
	AUDIO_RPC_LGSE_SETFN019 lgse019;
	if((g_lgse_fn_debug >> DEBUG_LGSE_FN019) & 0x1){
		UINT32 i;
		UINT32 *pParam = (UINT32*)pParams;
		AUDIO_INFO("[AUDH] %s argu0 = %x,argu1 = %d,argu2= %d \n", __FUNCTION__,(UINT32)pParams,noParam,dataOption );
		for(i=0; i<noParam; i++) {
			AUDIO_INFO("LGSE: pParams[%d] = %x\n", i, pParam[i]);
		}
	}

	memset(&lgse019, 0, sizeof(AUDIO_RPC_LGSE_SETFN019));
	PhyAddr = (UINT32)rtkaudio_alloc(sizeof(AUDIO_RPC_LGSE00856), (UINT8*)NULL, &nonCacheAddr);

	lgse019.rpc_LGSE00856_set_address = UNCACHEABLE_VADDR(PhyAddr); //change physical address to uncachable for FW
	lgse019.instanceID = (long)index; //for kcontrol
	lgse019.dataOption = dataOption;

	switch(dataOption){
	case HAL_AUDIO_LGSE_INIT_ONLY:
		if(noParam == (sizeof(AUDIO_RPC_LGSE00856)/sizeof(UINT32))){
			IPC_memcpy((long*)nonCacheAddr, (long*)pParams, noParam);
		}else{
			AUDIO_INFO("[AUDH] %s Init data size isn't correct(%d != %d)!\n\n", __FUNCTION__,noParam,sizeof(AUDIO_RPC_LGSE00856)/sizeof(UINT32));
			return NOT_OK;
		}
		break;
	case HAL_AUDIO_LGSE_VARIABLES:
		if(noParam == (sizeof(AUDIO_RPC_LGSE00871)/sizeof(UINT32))){
			IPC_memcpy((long*)&(lgse019.dataVar),(long*)pParams, noParam);
		}else{
			AUDIO_INFO("[AUDH] %s Var data size isn't correct(%d != %d)!\n\n", __FUNCTION__,noParam,sizeof(AUDIO_RPC_LGSE00871)/sizeof(UINT32));
			return NOT_OK;
		}
		break;
	case HAL_AUDIO_LGSE_ALL: //need to check.
		if(noParam == ((sizeof(AUDIO_RPC_LGSE00856)/sizeof(UINT32))+(sizeof(AUDIO_RPC_LGSE00871)/sizeof(UINT32)))){

		}else{

		}
		AUDIO_INFO("[AUDH] %s Warning(this case isn't ready!)\n", __FUNCTION__);
		return NOT_OK;
		break;
	default:
		AUDIO_INFO("[AUDH] %s The setting is out of range(>2)!\n", __FUNCTION__);
		return NOT_OK;
		break;
	}

	//pli_flushMemory;
	res = RTKAUDIO_RPC_TOAGENT_AO_LGSE_SETFN019_SVC(&lgse019);
	rtkaudio_free(PhyAddr);
	if(res != S_OK){
		AUDIO_ERROR("[%s:%d] RPC return != S_OK\n",__func__,__LINE__);
		return NOT_OK;
	}
	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_SetFN022(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption, int index)
{
	UINT32 res;
	UINT8* nonCacheAddr = NULL;
	UINT32 PhyAddr = 0;
	AUDIO_RPC_LGSE_SETFN022 lgse022;
	if((g_lgse_fn_debug >> DEBUG_LGSE_FN022) & 0x1){
		UINT32 i;
		UINT32 *pParam = (UINT32*)pParams;
		AUDIO_INFO("[AUDH] %s argu0 = %x,argu1 = %d,argu2= %d \n", __FUNCTION__,(UINT32)pParams,noParam,dataOption );
		for(i=0; i<noParam; i++) {
			AUDIO_INFO("LGSE: pParams[%d] = %x\n", i, pParam[i]);
		}
	}

	memset(&lgse022, 0, sizeof(AUDIO_RPC_LGSE_SETFN022));
	PhyAddr = (UINT32)rtkaudio_alloc(sizeof(AUDIO_RPC_LGSE00878), (UINT8*)NULL, &nonCacheAddr);

	lgse022.rpc_LGSE00878_set_address = UNCACHEABLE_VADDR(PhyAddr); //change physical address to uncachable for FW
	lgse022.instanceID = (long)index; //for kcontrol
	lgse022.dataOption = dataOption;

	switch(dataOption){
	case HAL_AUDIO_LGSE_INIT_ONLY:
		if(noParam == (sizeof(AUDIO_RPC_LGSE00863)/sizeof(UINT32))){
			IPC_memcpy((long*)&(lgse022.dataInit),(long*)pParams, noParam);
		}else{
			AUDIO_INFO("[AUDH] %s Init data size isn't correct(%d != %d)!\n\n", __FUNCTION__,noParam,sizeof(AUDIO_RPC_LGSE00863)/sizeof(UINT32));
			return NOT_OK;
		}
		break;
	case HAL_AUDIO_LGSE_VARIABLES:
		if(noParam == (sizeof(AUDIO_RPC_LGSE00878)/sizeof(UINT32))){
			IPC_memcpy((long*)nonCacheAddr, (long*)pParams, noParam);
		}else{
			AUDIO_INFO("[AUDH] %s Var data size isn't correct(%d != %d)!\n\n", __FUNCTION__,noParam,sizeof(AUDIO_RPC_LGSE00878)/sizeof(UINT32));
			return NOT_OK;
		}
		break;
	case HAL_AUDIO_LGSE_ALL: //need to check.
		if(noParam == ((sizeof(AUDIO_RPC_LGSE00863)/sizeof(UINT32))+(sizeof(AUDIO_RPC_LGSE00878)/sizeof(UINT32)))){

		}else{

		}
		AUDIO_INFO("[AUDH] %s Warning(this case isn't ready!)\n", __FUNCTION__);
		return NOT_OK;
		break;
	default:
		AUDIO_INFO("[AUDH] %s The setting is out of range(>2)!\n", __FUNCTION__);
		return NOT_OK;
		break;
	}

	//pli_flushMemory;

	res = RTKAUDIO_RPC_TOAGENT_AO_LGSE_SETFN022_SVC(&lgse022);
	rtkaudio_free(PhyAddr);
	if(res != S_OK){
		AUDIO_ERROR("[%s:%d] RPC return != S_OK\n",__func__,__LINE__);
		return NOT_OK;
	}

	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_SetFN022_1(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption, int index)
{
	AUDIO_INFO("[AUDH] %s not implemnet!\n", __FUNCTION__);
	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_SetFN022_2(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption, int index)
{
	AUDIO_INFO("[AUDH] %s not implemnet!\n", __FUNCTION__);
	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_SetFN026(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption, int index)
{
	UINT32 res;
	UINT8* nonCacheAddr = NULL;
	UINT32 PhyAddr = 0;
	AUDIO_RPC_LGSE_SETFN026 lgse026;
	if((g_lgse_fn_debug >> DEBUG_LGSE_FN026) & 0x1){
		UINT32 i;
		UINT32 *pParam = (UINT32*)pParams;
		AUDIO_INFO("[AUDH] %s argu0 = %x,argu1 = %d,argu2= %d \n", __FUNCTION__,(UINT32)pParams,noParam,dataOption );
		for(i=0; i<noParam; i++) {
			AUDIO_INFO("LGSE: pParams[%d] = %x\n", i, pParam[i]);
		}
	}

	memset(&lgse026, 0, sizeof(AUDIO_RPC_LGSE_SETFN026));
	PhyAddr = (UINT32)rtkaudio_alloc(sizeof(AUDIO_RPC_LGSE_DATA026), (UINT8*)NULL, &nonCacheAddr);

	lgse026.rpc_data026_set_address = UNCACHEABLE_VADDR(PhyAddr); //change physical address to uncachable for FW
	lgse026.instanceID = (long)index; //for kcontrol
	lgse026.dataOption = dataOption;

	switch(dataOption){
	case HAL_AUDIO_LGSE_INIT_ONLY:
		AUDIO_INFO("[AUDH] %s No use!\n", __FUNCTION__);
		return OK;
		break;
	case HAL_AUDIO_LGSE_VARIABLES:
		if(noParam == (sizeof(AUDIO_RPC_LGSE_DATA026)/sizeof(UINT32))){
			IPC_memcpy((long*)nonCacheAddr, (long*)pParams, noParam);
		}else{
			AUDIO_INFO("[AUDH] %s Init data size isn't correct(%d != %d)!\n\n", __FUNCTION__,noParam,sizeof(AUDIO_RPC_LGSE_DATA026)/sizeof(UINT32));
			return NOT_OK;
		}
		break;
	case HAL_AUDIO_LGSE_ALL: //need to check.
		AUDIO_INFO("[AUDH] %s No use!\n", __FUNCTION__);
		return NOT_OK;
		break;
	default:
		AUDIO_INFO("[AUDH] %s The setting is out of range(>2)!\n", __FUNCTION__);
		return NOT_OK;
		break;
	}

	//pli_flushMemory;
	res = RTKAUDIO_RPC_TOAGENT_AO_LGSE_SETFN026_SVC(&lgse026);
	rtkaudio_free(PhyAddr);
	if(res != S_OK){
		AUDIO_ERROR("[%s:%d] RPC return != S_OK\n",__func__,__LINE__);
		return NOT_OK;
	}
	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_SetFN028(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption, int index)
{
	//related to SetFn017_1
	UINT32 res;
	AUDIO_RPC_LGSE_SETFN028 lgsefn028;
	if((g_lgse_fn_debug >> DEBUG_LGSE_FN028) & 0x1){
		UINT32 i;
		UINT32 *pParam = (UINT32*)pParams;
		AUDIO_INFO("[AUDH] %s argu0 = %x,argu1 = %d,argu2= %d \n", __FUNCTION__,(UINT32)pParams,noParam,dataOption );
		for(i=0; i<noParam; i++) {
			AUDIO_INFO("LGSE: pParams[%d] = %x\n", i, pParam[i]);
		}
	}
	memset(&lgsefn028, 0, sizeof(AUDIO_RPC_LGSE_SETFN028));
	lgsefn028.instanceID = (long)index; //for kcontrol
	// LGSE parameter setting.
	lgsefn028.dataOption = (short)dataOption;
	// deliver Init & Var to lgse_config

	switch(dataOption){
	case HAL_AUDIO_LGSE_INIT_ONLY:
		AUDIO_INFO("[AUDH] %s No use!\n", __FUNCTION__);
		return OK;
		break;
	case HAL_AUDIO_LGSE_VARIABLES:
		if(noParam == (sizeof(AUDIO_RPC_LGSE02871)/sizeof(UINT32))){
			IPC_memcpy((long*)&(lgsefn028.dataVar),(long*)pParams, noParam);
		}else{
			AUDIO_INFO("[AUDH] %s Init data size isn't correct(%d != %d)!\n\n", __FUNCTION__,noParam,sizeof(AUDIO_RPC_LGSE02871)/sizeof(UINT32));
			return NOT_OK;
		}
		break;
	case HAL_AUDIO_LGSE_ALL: //need to check.
		AUDIO_INFO("[AUDH] %s No use!\n", __FUNCTION__);
		return OK;
		break;
	default:
		AUDIO_INFO("[AUDH] %s The setting is out of range(>2)!\n", __FUNCTION__);
		return NOT_OK;
		break;
	}

	res = RTKAUDIO_RPC_TOAGENT_AO_LGSE_SETFN028_SVC(&lgsefn028);
	if(res != S_OK){
		AUDIO_ERROR("[%s:%d] RPC return != S_OK\n",__func__,__LINE__);
		return NOT_OK;
	}
	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_SetFN029(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption, int index)
{
	//related to SetFn019_1
	UINT32 res;
	UINT8* nonCacheAddr = NULL;
	UINT32 PhyAddr = 0;
	AUDIO_RPC_LGSE_SETFN029 lgse029;
	if((g_lgse_fn_debug >> DEBUG_LGSE_FN029) & 0x1){
		UINT32 i;
		UINT32 *pParam = (UINT32*)pParams;
		AUDIO_INFO("[AUDH] %s argu0 = %x,argu1 = %d,argu2= %d \n", __FUNCTION__,(UINT32)pParams,noParam,dataOption );
		for(i=0; i<noParam; i++) {
			AUDIO_INFO("LGSE: pParams[%d] = %x\n", i, pParam[i]);
		}
	}

	memset(&lgse029, 0, sizeof(AUDIO_RPC_LGSE_SETFN029));
	PhyAddr = (UINT32)rtkaudio_alloc(sizeof(AUDIO_RPC_LGSE02869), (UINT8*)NULL, &nonCacheAddr);

	lgse029.rpc_LGSE02869_set_address = UNCACHEABLE_VADDR(PhyAddr); //change physical address to uncachable for FW
	lgse029.instanceID = (long)index; //for kcontrol
	lgse029.dataOption = dataOption;

	switch(dataOption){
	case HAL_AUDIO_LGSE_INIT_ONLY:
		AUDIO_INFO("[AUDH] %s No use!\n", __FUNCTION__);
		return OK;
		break;
	case HAL_AUDIO_LGSE_VARIABLES:
		if(noParam == (sizeof(AUDIO_RPC_LGSE02869)/sizeof(UINT32))){
			IPC_memcpy((long*)nonCacheAddr, (long*)pParams, noParam);
		}else{
			AUDIO_INFO("[AUDH] %s Init data size isn't correct(%d != %d)!\n\n", __FUNCTION__,noParam,sizeof(AUDIO_RPC_LGSE02869)/sizeof(UINT32));
			return NOT_OK;
		}
		break;
	case HAL_AUDIO_LGSE_ALL: //need to check.
		AUDIO_INFO("[AUDH] %s No use!\n", __FUNCTION__);
		return NOT_OK;
		break;
	default:
		AUDIO_INFO("[AUDH] %s The setting is out of range(>2)!\n", __FUNCTION__);
		return NOT_OK;
		break;
	}

	//pli_flushMemory;
	res = RTKAUDIO_RPC_TOAGENT_AO_LGSE_SETFN029_SVC(&lgse029);
	rtkaudio_free(PhyAddr);
	if(res != S_OK){
		AUDIO_ERROR("[%s:%d] RPC return != S_OK\n",__func__,__LINE__);
		return NOT_OK;
	}
	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_SetFN030(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption, int index)
{
	AUDIO_INFO("[AUDH] %s not implemnet!\n", __FUNCTION__);
	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_SetFN030_1(int *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption, int index)
{
	AUDIO_INFO("[AUDH] %s not implemnet!\n", __FUNCTION__);
	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_GetData(HAL_AUDIO_LGSE_FUNCLIST_T funcList, HAL_AUDIO_LGSE_DATA_ACCESS_T rw,
                                    UINT32 *pParams, UINT16 noParam, HAL_AUDIO_LGSE_DATA_MODE_T dataOption, int index)
{
	UINT32 res;
	AUDIO_RPC_LGSE_GETDATA lgse_getdata;
	AUDIO_RPC_LGSE_RETURNVAL lgse_ret;
#ifdef LGSE_PRINT
	UINT32 i;
	AUDIO_INFO("[AUDH] %s argu0 = %d,argu1 = %d,argu2= %x ,argu3 =%d, argu4 = %d\n", __FUNCTION__,funcList,rw,(UINT32)pParams,noParam,dataOption);
#endif
	memset(&lgse_getdata, 0, sizeof(AUDIO_RPC_LGSE_GETDATA));
	lgse_getdata.instanceID = (long)index; //for kcontrol
	lgse_getdata.function_list = funcList;
	lgse_getdata.size = noParam;
	lgse_getdata.rw = rw;
	// deliver Init & Var to lgse_config

	//rw always = 1
	switch(rw){
	case HAL_AUDIO_LGSE_WRITE:
		// setFN010 & 14 Var()
		AUDIO_INFO("[AUDH] Writing is no use !\n\n");
		return NOT_OK;
		break;
	case HAL_AUDIO_LGSE_READ:
		// getFN010 & 14 Out()
		if(funcList == 14 || funcList == 10){
			if(funcList == 10){ // getFun010out datasize =2
				if(noParam != 2){
					AUDIO_INFO("[AUDH] %s Out data size isn't correct(%d != %d)!\n\n", __FUNCTION__,noParam,2);
					return NOT_OK;
				}
				res = RTKAUDIO_RPC_TOAGENT_AO_LGSE_GETDATA_SVC(&lgse_getdata, &lgse_ret);
				if(res != S_OK){
					AUDIO_ERROR("[%s:%d] RPC return != S_OK\n",__func__,__LINE__);
					return NOT_OK;
				}

				if(lgse_ret.ret == S_OK) {
					pParams[0] = lgse_ret.value[0];
					pParams[1] = lgse_ret.value[1];
#ifdef LGSE_PRINT
					for(i=0; i<2; i++) {
						AUDIO_INFO("LGSE: pParams[%d] = %x\n", i, pParams[i]);
					}
#endif

				}
			}else{ // getFun010out datasize =7
				if(noParam != 7){
					AUDIO_INFO("[AUDH] %s Out data size isn't correct(%d != %d)!\n\n", __FUNCTION__,noParam,7);
					return NOT_OK;
				}
				res = RTKAUDIO_RPC_TOAGENT_AO_LGSE_GETDATA_SVC(&lgse_getdata, &lgse_ret);
				if(res != S_OK){
					AUDIO_ERROR("[%s:%d] RPC return != S_OK\n",__func__,__LINE__);
					return NOT_OK;
				}else if(lgse_ret.ret != S_OK){
					AUDIO_ERROR("[%s:%d] RPC return value != S_OK\n",__func__,__LINE__);
					return NOT_OK;
				}

				if(lgse_ret.ret == S_OK) {
					pParams[0] = lgse_ret.value[0];
					pParams[1] = lgse_ret.value[1];
					pParams[2] = lgse_ret.value[2];
					pParams[3] = lgse_ret.value[3];
					pParams[4] = lgse_ret.value[4];
					pParams[5] = lgse_ret.value[5];
					pParams[6] = lgse_ret.value[6];
#ifdef LGSE_PRINT
					for(i=0; i<7; i++) {
						AUDIO_INFO("LGSE: pParams[%d] = %x\n", i, pParams[i]);
					}
#endif
				}else if(lgse_ret.ret != S_OK){
					AUDIO_ERROR("[%s:%d] RPC return value != S_OK\n",__func__,__LINE__);
					return NOT_OK;
				}
			}
		}else{
			AUDIO_INFO("[AUDH] This Function No Use In This Item!\n\n");
			return NOT_OK;
		}

		break;
	default:
		AUDIO_INFO("[AUDH] %s The setting is out of range(%d>1)!\n", __FUNCTION__, dataOption);
		return NOT_OK;
		break;
	}
	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_RegSmartSoundCallback(pfnLGSESmartSound pfnCallBack, UINT32 callbackPeriod, int index)
{
	return NOT_OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_SetSoundEngineMode(lgse_se_init_ext_type_t se_init, lgse_se_mode_ext_type_t seMode, int index)
{
	char para[20];
	UINT32 len = sizeof(para);
	UINT32 res = S_FALSE;
	DTV_STATUS_T ret = NOT_OK;
	int bt_sur_connected = IS_SE_BT_Connected();
	if(bt_sur_connected && index == LGSE_SPK && seMode != LGSE_MODE_LGSE_ONLY){
		AUDIO_INFO("[%s] BT_SUR connected, ignore setting for LGSE_SPK, index:%d se_init:%d seMode:%d\n",__FUNCTION__,index,se_init,seMode);
		return OK;
	}else if((bt_sur_connected == FALSE) && (index == LGSE_SPK_BTSUR || index == LGSE_BT_BTSUR)){
		AUDIO_INFO("[%s] only SPK, ignore setting for LGSE_SPK_BTSUR or LGSE_BT_BTSUR, index:%d se_init:%d seMode:%d\n",__FUNCTION__,index,se_init,seMode);
		return OK;
	}else
		AUDIO_INFO("[%s] se_init %d seMode %d index %d\n",__FUNCTION__, se_init, seMode, index);

	switch (se_init) {
		case LGSE_INIT_UNKNOWN:
			if(seMode == LGSE_MODE_UNKNOWN)
				ret = OK;
			break;
		case LGSE_INIT_LGSE_ONLY:
			if(seMode == LGSE_MODE_LGSE_ONLY){
				sprintf(para,"ms12_dap 0");
				res = RTKAUDIO_RPC_TOAGENT_DEBUG_SVC(para, len);
				ret = HAL_AUDIO_LGSE_VX_EnableVx(FALSE, index);
			}
			break;
		case LGSE_INIT_LGSE_VX:
			if((seMode == LGSE_MODE_LGSE_ONLY)){
				sprintf(para,"ms12_dap 0");
				res = RTKAUDIO_RPC_TOAGENT_DEBUG_SVC(para, len);
				ret = HAL_AUDIO_LGSE_VX_EnableVx(FALSE, index);
			}else if((seMode == LGSE_MODE_LGSE_VX)){
				sprintf(para,"ms12_dap 0");
				res = RTKAUDIO_RPC_TOAGENT_DEBUG_SVC(para, len);
				ret = HAL_AUDIO_LGSE_VX_EnableVx(TRUE, index);
			}
			break;
		case LGSE_INIT_LGSE_ATMOS:
			if(seMode == LGSE_MODE_LGSE_ONLY){
				sprintf(para,"ms12_dap 0");
				res = RTKAUDIO_RPC_TOAGENT_DEBUG_SVC(para, len);
				ret = HAL_AUDIO_LGSE_VX_EnableVx(FALSE, index);
			}else if((seMode == LGSE_MODE_LGSE_ATMOS)){
				sprintf(para,"ms12_dap 1");
				if(index == LGSE_SPK)
					res = RTKAUDIO_RPC_TOAGENT_DEBUG_SVC(para, len);
				else
					AUDIO_ERROR("BT_SUR should not enable DAP!\n");
				ret = HAL_AUDIO_LGSE_VX_EnableVx(FALSE, index);
			}
			break;
		case LGSE_INIT_LGSE_VX_AISOUND:
			if((seMode == LGSE_MODE_LGSE_ONLY) || (seMode == LGSE_MODE_LGSE_AISOUND)){
				sprintf(para,"ms12_dap 0");
				res = RTKAUDIO_RPC_TOAGENT_DEBUG_SVC(para, len);
				ret = HAL_AUDIO_LGSE_VX_EnableVx(FALSE, index);
			}
			if(seMode == LGSE_MODE_LGSE_VX){
				sprintf(para,"ms12_dap 0");
				res = RTKAUDIO_RPC_TOAGENT_DEBUG_SVC(para, len);
				ret = HAL_AUDIO_LGSE_VX_EnableVx(TRUE, index);
			}
			break;
		case LGSE_INIT_LGSE_ATMOS_AISOUND:
			if((seMode == LGSE_MODE_LGSE_ONLY) || (seMode == LGSE_MODE_LGSE_AISOUND)){
				sprintf(para,"ms12_dap 0");
				res = RTKAUDIO_RPC_TOAGENT_DEBUG_SVC(para, len);
				ret = HAL_AUDIO_LGSE_VX_EnableVx(FALSE, index);
			}else if((seMode == LGSE_MODE_LGSE_ATMOS)){
				sprintf(para,"ms12_dap 1");
				if(index == LGSE_SPK)
					res = RTKAUDIO_RPC_TOAGENT_DEBUG_SVC(para, len);
				else
					AUDIO_ERROR("BT_SUR should not enable DAP!\n");
				ret = HAL_AUDIO_LGSE_VX_EnableVx(FALSE, index);
			}
			break;
		default:
			ret = NOT_OK;
			AUDIO_ERROR("[%s] unknown SE init mode (%d)\n",__FUNCTION__, se_init);
			break;
	}

	if(ret != OK)
		AUDIO_ERROR("seMode not match with se_init or HAL_AUDIO_LGSE_VX_EnableVx fail\n");

	if(res != S_OK){
		AUDIO_ERROR("RPC to ADSP fail!\n");
		ret = NOT_OK;
	}

	g_lgseInfo.se_mode = seMode;
	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_GetSoundEngineMode(lgse_se_mode_ext_type_t *pSEMode)
{
    *pSEMode = g_lgseInfo.se_mode;
	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_SetLGSEDownMix(BOOLEAN bEnable, int index)
{
	return NOT_OK;
}

/* DAP API */
void* dap_param_malloc(int paramValueSize, AUDIO_RPC_PRIVATEINFO_PARAMETERS *parameter)
{
	UINT8* nonCachedAddr = NULL;
	void *p_return = NULL;
	UINT32 p_addr = 0;

	memset(parameter, 0, sizeof(struct AUDIO_RPC_PRIVATEINFO_PARAMETERS));

	p_addr = (UINT32)rtkaudio_alloc(paramValueSize, (UINT8*)NULL, &nonCachedAddr);
	parameter->privateInfo[0] = UNCACHEABLE_VADDR(p_addr);
	parameter->privateInfo[1] = p_addr;

	return p_return = (void*)nonCachedAddr;
}

DTV_STATUS_T RHAL_AUDIO_SetDAPParam(AUDIO_ENUM_PRIVAETINFO paramType, void *paramValue, int paramValueSize, int index)
{
	/* Get Avalaible Outpin */
	struct AUDIO_RPC_PRIVATEINFO_PARAMETERS parameter;
	AUDIO_RPC_PRIVATEINFO_RETURNVAL ret;
	void *p_dap_param = NULL;

	if(paramValue == NULL){
		AUDIO_ERROR("[RHAL] Error, RHAL_AUDIO_SetDAPParam fail! \n");
		return NOT_OK;
	}

	p_dap_param = dap_param_malloc(paramValueSize, &parameter);
	if(p_dap_param == NULL){
		AUDIO_ERROR("[RHAL] Error, dap_rtkaudio_malloc fail! (set)\n");
		return NOT_OK;
	}
	parameter.type = paramType;
	parameter.instanceID = index;

	memcpy(p_dap_param, paramValue, paramValueSize);
	KADP_AUDIO_PrivateInfo(&parameter, &ret);
	rtkaudio_free(parameter.privateInfo[1]);

	return OK;
}

DTV_STATUS_T RHAL_AUDIO_GetDAPParam(AUDIO_ENUM_PRIVAETINFO paramType, void *paramValue, int paramValueSize, int index)
{
	/* Get Avalaible Outpin */
	struct AUDIO_RPC_PRIVATEINFO_PARAMETERS parameter;
	AUDIO_RPC_PRIVATEINFO_RETURNVAL ret;
	void *p_dap_param = NULL;

	if(paramValue == NULL){
		AUDIO_ERROR("[RHAL] Error, RHAL_AUDIO_GetDAPParam fail! \n");
		return NOT_OK;
	}

	p_dap_param = dap_param_malloc(paramType, &parameter);
	if(p_dap_param == NULL){
		AUDIO_ERROR("[RHAL] Error, dap_rtkaudio_malloc fail! (get)\n");
		return NOT_OK;
	}
	parameter.type = paramType;
	parameter.instanceID = index;

	KADP_AUDIO_PrivateInfo(&parameter, &ret);
	memcpy(paramValue, p_dap_param, paramValueSize);
	rtkaudio_free(parameter.privateInfo[1]);

	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_SetPostProcess(lgse_postproc_mode_ext_type_t ePostProcMode, int index)
{
	DTV_STATUS_T ret = NOT_OK;
	ret = RHAL_AUDIO_SetDAPParam(ENUM_PRIVATEINFO_AUDIO_SET_DAP_POSTGAIN,
			&ePostProcMode, sizeof(lgse_postproc_mode_ext_type_t), index);
	if((g_lgse_dap_debug >> DEBUG_LGSE_POSTPROCESS) & 0x1){
		AUDIO_FATAL("[%s] postgain %d\n",__FUNCTION__, ePostProcMode);
	}

	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_GetPostProcess(lgse_postproc_mode_ext_type_t *pePostProcMode, int index)
{
	DTV_STATUS_T ret = NOT_OK;
	ret = RHAL_AUDIO_GetDAPParam(ENUM_PRIVATEINFO_AUDIO_GET_DAP_POSTGAIN,
			(void*)pePostProcMode, sizeof(lgse_postproc_mode_ext_type_t), index);
#if defined(LGSE_DAP_DEBUG)
	AUDIO_FATAL("[%s] postgain %d\n",__FUNCTION__, *pePostProcMode);
#endif

	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetSurroundVirtualizerMode(lgse_dap_surround_virtualizer_mode_t stSurroundVirtualizerMode, int index)
{
	DTV_STATUS_T ret = NOT_OK;
	ret = RHAL_AUDIO_SetDAPParam(ENUM_PRIVATEINFO_AUDIO_SET_DAP_SURROUND_VIRTUALIZER_MODE,
			&stSurroundVirtualizerMode, sizeof(lgse_dap_surround_virtualizer_mode_t), index);
	if((g_lgse_dap_debug >> DEBUG_LGSE_DAP_SURROUNDVIRTUALIZERMODE) & 0x1){
		AUDIO_FATAL("[%s] virtualizer_mode %d\n",__FUNCTION__, stSurroundVirtualizerMode);
	}

	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetSurroundVirtualizerMode(lgse_dap_surround_virtualizer_mode_t *pstSurroundVirtualizerMode, int index)
{
	DTV_STATUS_T ret = NOT_OK;
	ret = RHAL_AUDIO_GetDAPParam(ENUM_PRIVATEINFO_AUDIO_GET_DAP_SURROUND_VIRTUALIZER_MODE,
			(void*)pstSurroundVirtualizerMode, sizeof(lgse_dap_surround_virtualizer_mode_t), index);
#if defined(LGSE_DAP_DEBUG)
	AUDIO_FATAL("[%s] virtualizer_mode %d\n",__FUNCTION__, *pstSurroundVirtualizerMode);
#endif

	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetDialogueEnhancer(DAP_PARAM_DIALOGUE_ENHANCER stDialogueEnhancer, int index)
{
	DTV_STATUS_T ret = NOT_OK;
	ret = RHAL_AUDIO_SetDAPParam(ENUM_PRIVATEINFO_AUDIO_SET_DAP_DIALOGUE_ENHANCER,
			&stDialogueEnhancer, sizeof(DAP_PARAM_DIALOGUE_ENHANCER), index);
	if((g_lgse_dap_debug >> DEBUG_LGSE_DAP_DIALOGUEENHANCER) & 0x1){
		AUDIO_FATAL("[%s] de_enable %d de_amount %d de_ducking %d\n",__FUNCTION__,
				stDialogueEnhancer.de_enable,
				stDialogueEnhancer.de_amount,
				stDialogueEnhancer.de_ducking);
	}

	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetDialogueEnhancer(DAP_PARAM_DIALOGUE_ENHANCER *pstDialogueEnhancer, int index)
{
	DTV_STATUS_T ret = NOT_OK;
	ret = RHAL_AUDIO_GetDAPParam(ENUM_PRIVATEINFO_AUDIO_GET_DAP_DIALOGUE_ENHANCER,
			(void*)pstDialogueEnhancer , sizeof(DAP_PARAM_DIALOGUE_ENHANCER), index);
#if defined(LGSE_DAP_DEBUG)
	AUDIO_FATAL("[%s] de_enable %d de_amount %d de_ducking %d\n",__FUNCTION__,
			pstDialogueEnhancer->de_enable,
			pstDialogueEnhancer->de_amount,
			pstDialogueEnhancer->de_ducking);
#endif

	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetVolumeLeveler(DAP_PARAM_VOLUME_LEVELER stVolumeLeveler, int index)
{
	DTV_STATUS_T ret = NOT_OK;
	ret = RHAL_AUDIO_SetDAPParam(ENUM_PRIVATEINFO_AUDIO_SET_DAP_VOLUME_LEVELER,
			&stVolumeLeveler, sizeof(DAP_PARAM_VOLUME_LEVELER), index);

	ret |= RHAL_AUDIO_SetDAPParam(ENUM_PRIVATEINFO_AUDIO_SET_DAP_VOLUME_LEVELER_IO,
			&stVolumeLeveler, sizeof(DAP_PARAM_VOLUME_LEVELER), index);
	if((g_lgse_dap_debug >> DEBUG_LGSE_DAP_VOLUMELEVELER) & 0x1){
		AUDIO_FATAL("[%s] leveler_setting %d leveler_amount %d leveler_input %d leveler_output %d\n",__FUNCTION__,
				stVolumeLeveler.leveler_setting,
				stVolumeLeveler.leveler_amount,
				stVolumeLeveler.leveler_input,
				stVolumeLeveler.leveler_output);
	}

	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetVolumeLeveler(DAP_PARAM_VOLUME_LEVELER *pstVolumeLeveler, int index)
{
	DTV_STATUS_T ret = NOT_OK;
	DAP_PARAM_VOLUME_LEVELER tmp_vol_leveler;
	ret = RHAL_AUDIO_GetDAPParam(ENUM_PRIVATEINFO_AUDIO_GET_DAP_VOLUME_LEVELER,
			(void*)pstVolumeLeveler, sizeof(DAP_PARAM_VOLUME_LEVELER), index);
	tmp_vol_leveler.leveler_enable = pstVolumeLeveler->leveler_enable;
	tmp_vol_leveler.leveler_amount = pstVolumeLeveler->leveler_amount;

	ret |= RHAL_AUDIO_GetDAPParam(ENUM_PRIVATEINFO_AUDIO_GET_DAP_VOLUME_LEVELER_IO,
			(void*)pstVolumeLeveler, sizeof(DAP_PARAM_VOLUME_LEVELER), index);

	pstVolumeLeveler->leveler_enable = tmp_vol_leveler.leveler_enable;
	pstVolumeLeveler->leveler_amount = tmp_vol_leveler.leveler_amount;
#if defined(LGSE_DAP_DEBUG)
	AUDIO_FATAL("[%s] leveler_setting %d leveler_amount %d leveler_input %d leveler_output %d\n",__FUNCTION__,
			pstVolumeLeveler->leveler_setting,
			pstVolumeLeveler->leveler_amount,
			pstVolumeLeveler->leveler_input,
			pstVolumeLeveler->leveler_output);
#endif

	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetVolumeModeler(DAP_PARAM_VOLUME_MODELER stVolumeModeler, int index)
{
	DTV_STATUS_T ret = NOT_OK;
	ret = RHAL_AUDIO_SetDAPParam(ENUM_PRIVATEINFO_AUDIO_SET_DAP_VOLUME_MODELER,
			&stVolumeModeler, sizeof(DAP_PARAM_VOLUME_MODELER), index);
	if((g_lgse_dap_debug >> DEBUG_LGSE_DAP_VOLUMEMODELER) & 0x1){
		AUDIO_FATAL("[%s] modeler_enable %d modeler_calibration %d\n",__FUNCTION__,
				stVolumeModeler.modeler_enable,
				stVolumeModeler.modeler_calibration);
	}

	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetVolumeModeler(DAP_PARAM_VOLUME_MODELER *pstVolumeModeler, int index)
{
	DTV_STATUS_T ret = NOT_OK;
	ret = RHAL_AUDIO_GetDAPParam(ENUM_PRIVATEINFO_AUDIO_GET_DAP_VOLUME_MODELER,
			(void*)pstVolumeModeler, sizeof(DAP_PARAM_VOLUME_MODELER), index);
#if defined(LGSE_DAP_DEBUG)
	AUDIO_FATAL("[%s] modeler_enable %d modeler_calibration %d\n",__FUNCTION__,
			pstVolumeModeler->modeler_enable,
			pstVolumeModeler->modeler_calibration);
#endif

	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetVolumeMaximizer(int stVolumeMaximizer, int index)
{
	DTV_STATUS_T ret = NOT_OK;
	ret = RHAL_AUDIO_SetDAPParam(ENUM_PRIVATEINFO_AUDIO_SET_DAP_VOLUME_MAXIMIZER,
			&stVolumeMaximizer, sizeof(int), index);
	if((g_lgse_dap_debug >> DEBUG_LGSE_DAP_VOLUMEMAXIMIZER) & 0x1){
		AUDIO_FATAL("[%s] volmax_boost %d\n",__FUNCTION__, stVolumeMaximizer);
	}

	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetVolumeMaximizer(int *pstVolumeMaximizer, int index)
{
	DTV_STATUS_T ret = NOT_OK;
	ret = RHAL_AUDIO_GetDAPParam(ENUM_PRIVATEINFO_AUDIO_GET_DAP_VOLUME_MAXIMIZER,
			(void*)pstVolumeMaximizer, sizeof(int), index);
#if defined(LGSE_DAP_DEBUG)
	AUDIO_FATAL("[%s] volmax_boost %d\n",__FUNCTION__, *pstVolumeMaximizer);
#endif

	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetOptimizer(DAP_PARAM_AUDIO_OPTIMIZER stOptimizer, int index)
{
	DTV_STATUS_T ret = NOT_OK;
	ret = RHAL_AUDIO_SetDAPParam(ENUM_PRIVATEINFO_AUDIO_SET_DAP_AUDIO_OPTIMIZER,
			&stOptimizer, sizeof(DAP_PARAM_AUDIO_OPTIMIZER), index);
	if((g_lgse_dap_debug >> DEBUG_LGSE_DAP_OPTIMIZER) & 0x1){
		int i,j;
		AUDIO_FATAL("[%s] optimizer_enable %d optimizer_nb_bands %d\n",__FUNCTION__,
				stOptimizer.optimizer_enable, stOptimizer.optimizer_nb_bands);
		for(i = 0; i < 20; i++){
			AUDIO_FATAL("freq[%d] = %d\n",i,stOptimizer.a_opt_band_center_freq[i]);
		}
		for(j = 0; j < 8; j++){
			for(i = 0; i < 20; i++){
				AUDIO_FATAL("gain[%d][%d] = %d\n",j,i, stOptimizer.a_opt_band_gain[j][i]);
			}
		}
	}

	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetOptimizer(DAP_PARAM_AUDIO_OPTIMIZER *pstOptimizer, int index)
{
#if defined(LGSE_DAP_DEBUG)
	int i,j;
#endif
	DTV_STATUS_T ret = NOT_OK;
	ret = RHAL_AUDIO_GetDAPParam(ENUM_PRIVATEINFO_AUDIO_GET_DAP_AUDIO_OPTIMIZER,
			(void*)pstOptimizer, sizeof(DAP_PARAM_AUDIO_OPTIMIZER), index);
#if defined(LGSE_DAP_DEBUG)
	AUDIO_FATAL("[%s] optimizer_enable %d optimizer_nb_bands %d\n",__FUNCTION__,
			pstOptimizer->optimizer_enable, pstOptimizer->optimizer_nb_bands);
	for(i = 0; i < 20; i++){
		AUDIO_FATAL("freq[%d] = %d\n",i,pstOptimizer->a_opt_band_center_freq[i]);
	}
	for(j = 0; j < 8; j++){
		for(i = 0; i < 20; i++){
			AUDIO_FATAL("gain[%d][%d] = %d\n",j,i,pstOptimizer->a_opt_band_gain[j][i]);
		}
	}
#endif


	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetProcessOptimizer(HAL_AUDIO_LGSE_DAP_PROCESS_OPTIMIZER_T stProcessOptimizer, int index)
{
	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetProcessOptimizer(HAL_AUDIO_LGSE_DAP_PROCESS_OPTIMIZER_T *pstProcessOptimizer, int index)
{
	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetSurroundDecoder(int stSurroundDecoder, int index)
{
	DTV_STATUS_T ret = NOT_OK;
	ret = RHAL_AUDIO_SetDAPParam(ENUM_PRIVATEINFO_AUDIO_SET_DAP_SURROUND_DECODER,
			&stSurroundDecoder, sizeof(int), index);
	if((g_lgse_dap_debug >> DEBUG_LGSE_DAP_SURROUNDDECODER) & 0x1){
		AUDIO_FATAL("[%s] surround_decoder_enable %d\n",__FUNCTION__, stSurroundDecoder);
	}

	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetSurroundDecoder(int *pstSurroundDecoder, int index)
{
	DTV_STATUS_T ret = NOT_OK;
	ret = RHAL_AUDIO_GetDAPParam(ENUM_PRIVATEINFO_AUDIO_GET_DAP_SURROUND_DECODER,
			(void*)pstSurroundDecoder, sizeof(int), index);
#if defined(LGSE_DAP_DEBUG)
	AUDIO_FATAL("[%s] surround_decoder_enable %d\n",__FUNCTION__, *pstSurroundDecoder);
#endif

	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetSurroundCompressor(int stSurroundCompressor, int index)
{
	DTV_STATUS_T ret = NOT_OK;
	ret = RHAL_AUDIO_SetDAPParam(ENUM_PRIVATEINFO_AUDIO_SET_DAP_SURROUND_COMPRESSOR,
			&stSurroundCompressor, sizeof(int), index);
	if((g_lgse_dap_debug >> DEBUG_LGSE_DAP_SURROUNDCOMPRESSOR) & 0x1){
		AUDIO_FATAL("[%s] surround_boost %d\n",__FUNCTION__, stSurroundCompressor);
	}

	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetSurroundCompressor(int *pstSurroundCompressor, int index)
{
	DTV_STATUS_T ret = NOT_OK;
	ret = RHAL_AUDIO_GetDAPParam(ENUM_PRIVATEINFO_AUDIO_GET_DAP_SURROUND_COMPRESSOR,
			(void*)pstSurroundCompressor, sizeof(int), index);
#if defined(LGSE_DAP_DEBUG)
	AUDIO_FATAL("[%s] surround_boost %d\n",__FUNCTION__, *pstSurroundCompressor);
#endif

	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetVirtualizerSpeakerAngle(DAP_PARAM_VIRTUALIZER_SPEAKER_ANGLE stSpeaker_angle, int index)
{
	DTV_STATUS_T ret = NOT_OK;
	ret = RHAL_AUDIO_SetDAPParam(ENUM_PRIVATEINFO_AUDIO_SET_DAP_VIRTUALIZER_SPEAKER_ANGLE,
			&stSpeaker_angle, sizeof(DAP_PARAM_VIRTUALIZER_SPEAKER_ANGLE), index);
	if((g_lgse_dap_debug >> DEBUG_LGSE_DAP_VIRTUALIZERSPEAKERANGLE) & 0x1){
        AUDIO_FATAL("[%s] speaker_angle front/surround/height %d/%d/%d\n",__FUNCTION__,
                stSpeaker_angle.virtualizer_front_speaker_angle,
                stSpeaker_angle.virtualizer_surround_speaker_angle,
                stSpeaker_angle.virtualizer_height_speaker_angle);
	}

	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetVirtualizerSpeakerAngle(DAP_PARAM_VIRTUALIZER_SPEAKER_ANGLE *pstSpeaker_angle, int index)
{
	DTV_STATUS_T ret = NOT_OK;
	ret = RHAL_AUDIO_GetDAPParam(ENUM_PRIVATEINFO_AUDIO_GET_DAP_VIRTUALIZER_SPEAKER_ANGLE,
			(void*)pstSpeaker_angle, sizeof(DAP_PARAM_VIRTUALIZER_SPEAKER_ANGLE), index);
#if defined(LGSE_DAP_DEBUG)
	AUDIO_FATAL("[%s] speaker_angle front/surround/height %d/%d/%d\n",__FUNCTION__,
            *pstSpeaker_angle.virtualizer_front_speaker_angle,
            *pstSpeaker_angle.virtualizer_surround_speaker_angle,
            *pstSpeaker_angle.virtualizer_height_speaker_angle);
#endif

	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetIntelligenceEQ(DAP_PARAM_INTELLIGENT_EQUALIZER stIntelligenceEQ, int index)
{
	DTV_STATUS_T ret = NOT_OK;
	ret = RHAL_AUDIO_SetDAPParam(ENUM_PRIVATEINFO_AUDIO_SET_DAP_INTELLIGENT_EQUALIZER,
			&stIntelligenceEQ, sizeof(DAP_PARAM_INTELLIGENT_EQUALIZER), index);
	if((g_lgse_dap_debug >> DEBUG_LGSE_DAP_INTELLIGENCEEQ) & 0x1){
		int i;
		AUDIO_FATAL("[%s] ieq_nb_bands %d ieq_amount %d\n",__FUNCTION__, stIntelligenceEQ.ieq_nb_bands, stIntelligenceEQ.ieq_amount);
		for(i = 0; i < 20; i++){
			AUDIO_FATAL("freq[%d] = %d\n",i, stIntelligenceEQ.a_ieq_band_center[i]);
		}
		for(i = 0; i < 20; i++){
			AUDIO_FATAL("gain[%d] = %d\n",i, stIntelligenceEQ.a_ieq_band_target[i]);
		}
	}

	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetIntelligenceEQ(DAP_PARAM_INTELLIGENT_EQUALIZER *pstIntelligenceEQ, int index)
{
	DTV_STATUS_T ret = NOT_OK;
#if defined(LGSE_DAP_DEBUG)
	int i;
#endif
	ret = RHAL_AUDIO_GetDAPParam(ENUM_PRIVATEINFO_AUDIO_GET_DAP_INTELLIGENT_EQUALIZER,
			(void*)pstIntelligenceEQ, sizeof(DAP_PARAM_INTELLIGENT_EQUALIZER), index);
#if defined(LGSE_DAP_DEBUG)
	AUDIO_FATAL("[%s] ieq_nb_bands %d ieq_amount %d\n",__FUNCTION__, pstIntelligenceEQ->ieq_nb_bands, pstIntelligenceEQ->ieq_amount);
	for(i = 0; i < 20; i++){
		AUDIO_FATAL("freq[%d] = %d\n",i, pstIntelligenceEQ->a_ieq_band_center[i]);
	}
	for(i = 0; i < 20; i++){
		AUDIO_FATAL("gain[%d] = %d\n",i, pstIntelligenceEQ->a_ieq_band_target[i]);
	}
#endif

	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetMediaIntelligence(DAP_PARAM_MEDIA_INTELLIGENCE stMediaIntelligence, int index)
{
	DTV_STATUS_T ret = NOT_OK;
	ret = RHAL_AUDIO_SetDAPParam(ENUM_PRIVATEINFO_AUDIO_SET_DAP_MEDIA_INTELLIGENCE,
			&stMediaIntelligence, sizeof(DAP_PARAM_MEDIA_INTELLIGENCE), index);
	if((g_lgse_dap_debug >> DEBUG_LGSE_DAP_MEDIAINTELLIGENCE) & 0x1){
		AUDIO_FATAL("[%s] mi_ieq_enable %d mi_dv_enable %d mi_de_enable %d mi_surround_enable %d\n",__FUNCTION__,
				stMediaIntelligence.mi_steering_enable,
				stMediaIntelligence.mi_dv_enable,
				stMediaIntelligence.mi_de_enable,
				stMediaIntelligence.mi_surround_enable);
	}

	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetMediaIntelligence(DAP_PARAM_MEDIA_INTELLIGENCE *pstMediaIntelligence, int index)
{
	DTV_STATUS_T ret = NOT_OK;
	ret = RHAL_AUDIO_GetDAPParam(ENUM_PRIVATEINFO_AUDIO_GET_DAP_MEDIA_INTELLIGENCE,
			(void*)pstMediaIntelligence, sizeof(DAP_PARAM_MEDIA_INTELLIGENCE), index);
#if defined(LGSE_DAP_DEBUG)
	AUDIO_FATAL("[%s] mi_ieq_enable %d mi_dv_enable %d mi_de_enable %d mi_surround_enable %d\n",__FUNCTION__,
			pstMediaIntelligence->mi_steering_enable,
			pstMediaIntelligence->mi_dv_enable,
			pstMediaIntelligence->mi_de_enable,
			pstMediaIntelligence->mi_surround_enable);
#endif

	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetGraphicalEQ(DAP_PARAM_GRAPHICAL_EQUALIZER stGraphicalEQ, int index)
{
	DTV_STATUS_T ret = NOT_OK;
	ret = RHAL_AUDIO_SetDAPParam(ENUM_PRIVATEINFO_AUDIO_SET_DAP_GRAPHICAL_EQUALIZER,
			&stGraphicalEQ, sizeof(DAP_PARAM_GRAPHICAL_EQUALIZER), index);
	if((g_lgse_dap_debug >> DEBUG_LGSE_DAP_GRAPHICALEQ) & 0x1){
		int i;
		AUDIO_FATAL("[%s] eq_enable %d eq_nb_bands %d\n",__FUNCTION__, stGraphicalEQ.eq_enable, stGraphicalEQ.eq_nb_bands);
		for(i = 0; i < 20; i++){
			AUDIO_FATAL("freq[%d] = %d\n",i, stGraphicalEQ.a_geq_band_center[i]);
		}
		for(i = 0; i < 20; i++){
			AUDIO_FATAL("gain[%d] = %d\n",i, stGraphicalEQ.a_geq_band_target[i]);
		}
	}

	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetGraphicalEQ(DAP_PARAM_GRAPHICAL_EQUALIZER *pstGraphicalEQ, int index)
{
	DTV_STATUS_T ret = NOT_OK;
#if defined(LGSE_DAP_DEBUG)
	int i;
#endif
	ret = RHAL_AUDIO_GetDAPParam(ENUM_PRIVATEINFO_AUDIO_GET_DAP_GRAPHICAL_EQUALIZER,
			(void*)pstGraphicalEQ, sizeof(DAP_PARAM_GRAPHICAL_EQUALIZER), index);
#if defined(LGSE_DAP_DEBUG)
	AUDIO_FATAL("[%s] eq_enable %d eq_nb_bands %d\n",__FUNCTION__, pstGraphicalEQ->eq_enable, pstGraphicalEQ->eq_nb_bands);
	for(i = 0; i < 20; i++){
		AUDIO_FATAL("freq[%d] = %d\n",i, pstGraphicalEQ->a_geq_band_center[i]);
	}
	for(i = 0; i < 20; i++){
		AUDIO_FATAL("gain[%d] = %d\n",i, pstGraphicalEQ->a_geq_band_target[i]);
	}
#endif

	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetPerceptualHeightFilter(lgse_dap_perceptual_height_filter_mode_ext_type_t stPerceptualHeightFilter, int index)
{
	DTV_STATUS_T ret = NOT_OK;
	ret = RHAL_AUDIO_SetDAPParam(ENUM_PRIVATEINFO_AUDIO_SET_DAP_PERCEPTUAL_HEIGHT_FILTER,
			&stPerceptualHeightFilter, sizeof(lgse_dap_perceptual_height_filter_mode_ext_type_t), index);
	if((g_lgse_dap_debug >> DEBUG_LGSE_DAP_PERCEPTUALHEIGHTFILTER) & 0x1){
		AUDIO_FATAL("[%s] height_filter_mode %d\n",__FUNCTION__, stPerceptualHeightFilter);
	}

	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetPerceptualHeightFilter(lgse_dap_perceptual_height_filter_mode_ext_type_t *pstPerceptualHeightFilter, int index)
{
	DTV_STATUS_T ret = NOT_OK;
	ret = RHAL_AUDIO_GetDAPParam(ENUM_PRIVATEINFO_AUDIO_GET_DAP_PERCEPTUAL_HEIGHT_FILTER,
			(void*)pstPerceptualHeightFilter, sizeof(lgse_dap_perceptual_height_filter_mode_ext_type_t), index);
#if defined(LGSE_DAP_DEBUG)
	AUDIO_FATAL("[%s] height_filter_mode %d\n",__FUNCTION__, *pstPerceptualHeightFilter);
#endif

	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetBassEnhancer(DAP_PARAM_BASS_ENHANCER stBassEnhancer, int index)
{
	DTV_STATUS_T ret = NOT_OK;
	ret = RHAL_AUDIO_SetDAPParam(ENUM_PRIVATEINFO_AUDIO_SET_DAP_BASS_ENHANCER,
			&stBassEnhancer, sizeof(DAP_PARAM_BASS_ENHANCER), index);
	if((g_lgse_dap_debug >> DEBUG_LGSE_DAP_BASSENHANCER) & 0x1){
		AUDIO_FATAL("[%s] bass_enable %d bass_boost %d bass_cutoff %d bass_width %d\n",__FUNCTION__,
				stBassEnhancer.bass_enable,
				stBassEnhancer.bass_boost,
				stBassEnhancer.bass_cutoff,
				stBassEnhancer.bass_width);
	}

	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetBassEnhancer(DAP_PARAM_BASS_ENHANCER *pstBassEnhancer, int index)
{
	DTV_STATUS_T ret = NOT_OK;
	ret = RHAL_AUDIO_GetDAPParam(ENUM_PRIVATEINFO_AUDIO_GET_DAP_BASS_ENHANCER,
			(void*)pstBassEnhancer, sizeof(DAP_PARAM_BASS_ENHANCER), index);
#if defined(LGSE_DAP_DEBUG)
	AUDIO_FATAL("[%s] bass_enable %d bass_boost %d bass_cutoff %d bass_width %d\n",__FUNCTION__,
			pstBassEnhancer->bass_enable,
			pstBassEnhancer->bass_boost,
			pstBassEnhancer->bass_cutoff,
			pstBassEnhancer->bass_width);
#endif

	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetBassExtraction(HAL_AUDIO_LGSE_DAP_BASS_EXTRACTION_T stBassExtraction, int index)
{
#if 0
	DTV_STATUS_T ret = NOT_OK;
	ret = RHAL_AUDIO_SetDAPParam(,
			, sizeof(), index);

	return ret;
#else
	return OK;
#endif
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetBassExtraction(HAL_AUDIO_LGSE_DAP_BASS_EXTRACTION_T *pstBassExtraction, int index)
{
#if 0
	DTV_STATUS_T ret = NOT_OK;
	ret = RHAL_AUDIO_GetDAPParam(,
			, sizeof(), index);

	return ret;
#else
	return OK;
#endif
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetVirtualBass(DAP_PARAM_VIRTUAL_BASS stVirtualBass, int index)
{
	DTV_STATUS_T ret = NOT_OK;
	ret = RHAL_AUDIO_SetDAPParam(ENUM_PRIVATEINFO_AUDIO_SET_DAP_VIRTUAL_BASS,
			&stVirtualBass, sizeof(DAP_PARAM_VIRTUAL_BASS), index);
	if((g_lgse_dap_debug >> DEBUG_LGSE_DAP_VIRTUALBASS) & 0x1){
		AUDIO_FATAL("[%s] vb_mode %d vb_low_src_freq %d\n",__FUNCTION__,
				stVirtualBass.vb_mode,
				stVirtualBass.vb_low_src_freq);
		AUDIO_FATAL("[%s] vb_high_src_freq %d vb_overall_gain %d vb_slope_gain %d\n",__FUNCTION__,
				stVirtualBass.vb_high_src_freq,
				stVirtualBass.vb_overall_gain,
				stVirtualBass.vb_slope_gain);
		AUDIO_FATAL("[%s] vb_subgain %d / %d / %d\n",__FUNCTION__,
				stVirtualBass.vb_subgain[0],
				stVirtualBass.vb_subgain[1],
				stVirtualBass.vb_subgain[2]);
		AUDIO_FATAL("[%s] vb_mix_low_freq %d vb_mix_high_freq %d\n",__FUNCTION__,
				stVirtualBass.vb_mix_low_freq,
				stVirtualBass.vb_mix_high_freq);
	}

	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetVirtualBass(DAP_PARAM_VIRTUAL_BASS *pstVirtualBass, int index)
{
	DTV_STATUS_T ret = NOT_OK;
	ret = RHAL_AUDIO_GetDAPParam(ENUM_PRIVATEINFO_AUDIO_GET_DAP_VIRTUAL_BASS,
			(void*)pstVirtualBass, sizeof(DAP_PARAM_VIRTUAL_BASS), index);
#if defined(LGSE_DAP_DEBUG)
	AUDIO_FATAL("[%s] vb_mode %d vb_low_src_freq %d\n",__FUNCTION__,
			pstVirtualBass->vb_mode,
			pstVirtualBass->vb_low_src_freq);
	AUDIO_FATAL("[%s] vb_high_src_freq %d vb_overall_gain %d vb_slope_gain %d\n",__FUNCTION__,
			pstVirtualBass->vb_high_src_freq,
			pstVirtualBass->vb_overall_gain,
			pstVirtualBass->vb_slope_gain);
	AUDIO_FATAL("[%s] vb_subgain %d / %d / %d\n",__FUNCTION__,
			pstVirtualBass->vb_subgain[0],
			pstVirtualBass->vb_subgain[1],
			pstVirtualBass->vb_subgain[2]);
	AUDIO_FATAL("[%s] vb_mix_low_freq %d vb_mix_high_freq %d\n",__FUNCTION__,
			pstVirtualBass->vb_mix_low_freq,
			pstVirtualBass->vb_mix_high_freq);
#endif

	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetRegulator(DAP_PARAM_AUDIO_REGULATOR stRegulator, int index)
{
	DTV_STATUS_T ret = NOT_OK;
	ret = RHAL_AUDIO_SetDAPParam(ENUM_PRIVATEINFO_AUDIO_SET_DAP_AUDIO_REGULATOR,
			&stRegulator, sizeof(DAP_PARAM_AUDIO_REGULATOR), index);
	if((g_lgse_dap_debug >> DEBUG_LGSE_DAP_REGULATOR) & 0x1){
		int i;
		AUDIO_FATAL("[%s] regulator_enable %d reg_nb_bands %d\n",__FUNCTION__, stRegulator.regulator_enable, stRegulator.reg_nb_bands);
		for(i = 0; i < 20; i++){
			AUDIO_FATAL("band_center[%d] = %d\n",i, stRegulator.a_reg_band_center[i]);
		}
		for(i = 0; i < 20; i++){
			AUDIO_FATAL("low_thresholds[%d] = %d\n",i, stRegulator.a_reg_low_thresholds[i]);
		}
		for(i = 0; i < 20; i++){
			AUDIO_FATAL("high_threshold[%d] = %d\n",i, stRegulator.a_reg_high_thresholds[i]);
		}
		for(i = 0; i < 20; i++){
			AUDIO_FATAL("isolated_bands[%d] = %d\n",i, stRegulator.a_reg_isolated_bands[i]);
		}
		AUDIO_FATAL("[%s] regulator_overdrive %d regulator_timbre %d regulator_distortion %d regulator_mode %d\n",__FUNCTION__,
				stRegulator.regulator_overdrive,
				stRegulator.regulator_timbre,
				stRegulator.regulator_distortion,
				stRegulator.regulator_mode);
	}

	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetRegulator(DAP_PARAM_AUDIO_REGULATOR *pstRegulator, int index)
{
	DTV_STATUS_T ret = NOT_OK;
#if defined(LGSE_DAP_DEBUG)
	int i;
#endif
	ret = RHAL_AUDIO_GetDAPParam(ENUM_PRIVATEINFO_AUDIO_GET_DAP_AUDIO_REGULATOR,
			(void*)pstRegulator, sizeof(DAP_PARAM_AUDIO_REGULATOR), index);
#if defined(LGSE_DAP_DEBUG)
	AUDIO_FATAL("[%s] regulator_enable %d reg_nb_bands %d\n",__FUNCTION__, pstRegulator->regulator_enable, pstRegulator->reg_nb_bands);
	for(i = 0; i < 20; i++){
		AUDIO_FATAL("band_center[%d] = %d\n",i, pstRegulator->a_reg_band_center[i]);
	}
	for(i = 0; i < 20; i++){
		AUDIO_FATAL("low_thresholds[%d] = %d\n",i, pstRegulator->a_reg_low_thresholds[i]);
	}
	for(i = 0; i < 20; i++){
		AUDIO_FATAL("high_threshold[%d] = %d\n",i, pstRegulator->a_reg_high_thresholds[i]);
	}
	for(i = 0; i < 20; i++){
		AUDIO_FATAL("isolated_bands[%d] = %d\n",i, pstRegulator->a_reg_isolated_bands[i]);
	}
	AUDIO_FATAL("[%s] regulator_overdrive %d regulator_timbre %d regulator_distortion %d regulator_mode %d\n",__FUNCTION__,
			pstRegulator->regulator_overdrive,
			pstRegulator->regulator_timbre,
			pstRegulator->regulator_distortion,
			pstRegulator->regulator_mode);
#endif

	return ret;
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_SetSysParam(HAL_AUDIO_LGSE_DAP_SYS_PARAM_T stDapSysParam, int index)
{
#if 0
	DTV_STATUS_T ret = NOT_OK;
	ret = RHAL_AUDIO_SetDAPParam(,
			, sizeof(), index);

	return ret;
#else
	return OK;
#endif
}

DTV_STATUS_T HAL_AUDIO_LGSE_DAP_GetSysParam(HAL_AUDIO_LGSE_DAP_SYS_PARAM_T *pstDapSysParam, int index)
{
#if 0
	DTV_STATUS_T ret = NOT_OK;
	ret = RHAL_AUDIO_GetDAPParam(,
			, sizeof(), index);

	return ret;
#else
	return OK;
#endif
}

/* Virtual:X */
DTV_STATUS_T HAL_AUDIO_LGSE_VX_EnableVx(BOOLEAN bEnable, int index)
{
#if defined(DTS_VX_ENABLED)
	KADP_STATUS_T ret;
	AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO trusrndx_param;
	AUDIO_INFO("[%s] status %d\n", __FUNCTION__, bEnable);

	// [WOSQRTK-11221] mute spk to avoid sound be loud before VX on
	Switch_OnOff_MuteProtect(50);

	trusrndx_param.ParameterType = ENUM_DTS_PARAM_VX_ENABLE_I32;
	trusrndx_param.Value[0] = bEnable;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0]);
	ret = KADP_AUDIO_SetTruSrndXParam(&trusrndx_param, index);

	if (KADP_OK != ret)
		return NOT_OK;
#if 0
	if (bEnable)
	{
		trusrndx_param.ParameterType = ENUM_DTS_PARAM_VX_INPUT_MODE_I32;
		trusrndx_param.Value[0] = 1;
		trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0]);
		ret = KADP_AUDIO_SetTruSrndXParam(&trusrndx_param, index);

		if (KADP_OK != ret)
			return NOT_OK;

		trusrndx_param.ParameterType = ENUM_DTS_PARAM_VX_OUTPUT_MODE_I32;
		trusrndx_param.Value[0] = 0;
		trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0]);
		ret = KADP_AUDIO_SetTruSrndXParam(&trusrndx_param, index);

		if (KADP_OK != ret)
			return NOT_OK;
	}
#endif
#endif
	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_VX_SetVxStatus(BOOLEAN bEnable, int index)
{
	KADP_STATUS_T ret;
	AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO trusrndx_param;
	AUDIO_INFO("[%s] status %d\n", __FUNCTION__, bEnable);

	trusrndx_param.ParameterType = ENUM_DTS_PARAM_VX_ENABLE_I32;
	trusrndx_param.Value[0] = bEnable;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0]);
	ret = KADP_AUDIO_SetTruSrndXParam(&trusrndx_param, index);

	/**bEnable = trusrndx_param.Value[0];*/

	if (KADP_OK != ret)
		return NOT_OK;

	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_VX_GetVxStatus(BOOLEAN* bEnable, int index)
{
	KADP_STATUS_T ret;
	AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO trusrndx_param;
	if(bEnable == NULL) return NOT_OK;

	trusrndx_param.ParameterType = ENUM_DTS_PARAM_VX_ENABLE_I32;
	trusrndx_param.Value[0] = 0;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0]);
	ret = KADP_AUDIO_GetTruSrndXParam(&trusrndx_param, index);

	*bEnable = trusrndx_param.Value[0];

	if (KADP_OK != ret)
		return NOT_OK;

	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_VX_SetProcOutputGain(int procOutGain, int index)
{
	KADP_STATUS_T ret;
	AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO trusrndx_param;
	AUDIO_INFO("[%s] procOutGain %d\n", __FUNCTION__, procOutGain);

	trusrndx_param.ParameterType = ENUM_DTS_PARAM_VX_PROC_OUTPUT_GAIN_I32;
	trusrndx_param.Value[0] = procOutGain;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0]);
	ret = KADP_AUDIO_SetTruSrndXParam(&trusrndx_param, index);

	if (KADP_OK != ret)
		return NOT_OK;

	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_VX_GetProcOutputGain(int* procOutGain, int index)
{
	KADP_STATUS_T ret;
	AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO trusrndx_param;
	if(procOutGain == NULL) return NOT_OK;

	trusrndx_param.ParameterType = ENUM_DTS_PARAM_VX_PROC_OUTPUT_GAIN_I32;
	trusrndx_param.Value[0] = 0;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0]);
	ret = KADP_AUDIO_GetTruSrndXParam(&trusrndx_param, index);

	*procOutGain = trusrndx_param.Value[0];

	if (KADP_OK != ret)
		return NOT_OK;

	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_VX_EnableTsx(BOOLEAN bEnable, int index)
{
	KADP_STATUS_T ret;
	AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO trusrndx_param;
	AUDIO_INFO("[%s] bEnable %d\n", __FUNCTION__, bEnable);

	trusrndx_param.ParameterType = ENUM_DTS_PARAM_TSX_ENABLE_I32;
	trusrndx_param.Value[0] = bEnable;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0]);
	ret = KADP_AUDIO_SetTruSrndXParam(&trusrndx_param, index);

	if (KADP_OK != ret)
		return NOT_OK;

	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_VX_GetTsxStatus(BOOLEAN* bEnable, int index)
{
	KADP_STATUS_T ret;
	AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO trusrndx_param;
	if(bEnable == NULL) return NOT_OK;

	trusrndx_param.ParameterType = ENUM_DTS_PARAM_TSX_ENABLE_I32;
	trusrndx_param.Value[0] = 0;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0]);
	ret = KADP_AUDIO_GetTruSrndXParam(&trusrndx_param, index);

	*bEnable = trusrndx_param.Value[0];

	if (KADP_OK != ret)
		return NOT_OK;

	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_VX_SetTsxPassiveMatrixUpmix(BOOLEAN bEnable, int index)
{
	KADP_STATUS_T ret;
	AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO trusrndx_param;
	AUDIO_INFO("[%s] bEnable %d\n", __FUNCTION__, bEnable);

	trusrndx_param.ParameterType = ENUM_DTS_PARAM_TSX_PASSIVEMATRIXUPMIX_ENABLE_I32;
	trusrndx_param.Value[0] = bEnable;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0]);
	ret = KADP_AUDIO_SetTruSrndXParam(&trusrndx_param, index);

	if (KADP_OK != ret)
		return NOT_OK;

	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_VX_GetTsxPassiveMatrixUpmix(BOOLEAN* bEnable, int index)
{
	KADP_STATUS_T ret;
	AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO trusrndx_param;
	if(bEnable == NULL) return NOT_OK;

	trusrndx_param.ParameterType = ENUM_DTS_PARAM_TSX_PASSIVEMATRIXUPMIX_ENABLE_I32;
	trusrndx_param.Value[0] = 0;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0]);
	ret = KADP_AUDIO_GetTruSrndXParam(&trusrndx_param, index);

	*bEnable = trusrndx_param.Value[0];

	if (KADP_OK != ret)
		return NOT_OK;

	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_VX_SetTsxHeightUpmix(BOOLEAN bEnable, int index)
{
	KADP_STATUS_T ret;
	AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO trusrndx_param;
	AUDIO_INFO("[%s] bEnable %d\n", __FUNCTION__, bEnable);

	trusrndx_param.ParameterType = ENUM_DTS_PARAM_TSX_HEIGHT_UPMIX_ENABLE_I32;
	trusrndx_param.Value[0] = bEnable;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0]);
	ret = KADP_AUDIO_SetTruSrndXParam(&trusrndx_param, index);

	if (KADP_OK != ret)
		return NOT_OK;

	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_VX_GetTsxHeightUpmix(BOOLEAN* bEnable, int index)
{
	KADP_STATUS_T ret;
	AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO trusrndx_param;
	if(bEnable == NULL) return NOT_OK;

	trusrndx_param.ParameterType = ENUM_DTS_PARAM_TSX_HEIGHT_UPMIX_ENABLE_I32;
	trusrndx_param.Value[0] = 0;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0]);
	ret = KADP_AUDIO_GetTruSrndXParam(&trusrndx_param, index);

	*bEnable = trusrndx_param.Value[0];

	if (KADP_OK != ret)
		return NOT_OK;

	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_VX_SetTsxLPRGain(int lprGain, int index)
{
	KADP_STATUS_T ret;
	AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO trusrndx_param;
	AUDIO_INFO("[%s] lprGain %d\n", __FUNCTION__, lprGain);

	trusrndx_param.ParameterType = ENUM_DTS_PARAM_TSX_LPR_GAIN_I32;
	trusrndx_param.Value[0] = lprGain;
	trusrndx_param.Value[1] = 0xff;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0])*2;
	ret = KADP_AUDIO_SetTruSrndXParam(&trusrndx_param, index);

	if (KADP_OK != ret)
		return NOT_OK;

	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_VX_GetTsxLPRGain(int* lprGain, int index)
{
	KADP_STATUS_T ret;
	AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO trusrndx_param;
	if(lprGain == NULL) return NOT_OK;

	trusrndx_param.ParameterType = ENUM_DTS_PARAM_TSX_LPR_GAIN_I32;
	trusrndx_param.Value[0] = 0;
	trusrndx_param.Value[1] = 0xff;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0])*2;
	ret = KADP_AUDIO_GetTruSrndXParam(&trusrndx_param, index);

	*lprGain = trusrndx_param.Value[0];

	if (KADP_OK != ret)
		return NOT_OK;

	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_VX_SetTsxHorizVirEff(BOOLEAN bEnable, int index)
{
	KADP_STATUS_T ret;
	AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO trusrndx_param;
	AUDIO_INFO("[%s] bEnable %d\n", __FUNCTION__, bEnable);

	trusrndx_param.ParameterType = ENUM_DTS_PARAM_TSX_HORIZ_VIR_EFF_CTRL_I32;
	trusrndx_param.Value[0] = bEnable;
	trusrndx_param.Value[1] = 0xff;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0])*2;
	ret = KADP_AUDIO_SetTruSrndXParam(&trusrndx_param, index);

	if (KADP_OK != ret)
		return NOT_OK;

	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_VX_GetTsxHorizVirEff(BOOLEAN* bEnable, int index)
{
	KADP_STATUS_T ret;
	AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO trusrndx_param;
	if(bEnable == NULL) return NOT_OK;

	trusrndx_param.ParameterType = ENUM_DTS_PARAM_TSX_HORIZ_VIR_EFF_CTRL_I32;
	trusrndx_param.Value[0] = 0;
	trusrndx_param.Value[1] = 0xff;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0])*2;
	ret = KADP_AUDIO_GetTruSrndXParam(&trusrndx_param, index);

	*bEnable = trusrndx_param.Value[0];

	if (KADP_OK != ret)
		return NOT_OK;

	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_VX_SetTsxHeightMixCoeff(int heightCoeff, int index)
{
	KADP_STATUS_T ret;
	AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO trusrndx_param;
	AUDIO_INFO("[%s] heightCoeff %d\n", __FUNCTION__, heightCoeff);

	trusrndx_param.ParameterType = ENUM_DTS_PARAM_TSX_HEIGHTMIX_COEFF_I32;
	trusrndx_param.Value[0] = heightCoeff;
	trusrndx_param.Value[1] = 0xff;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0])*2;
	ret = KADP_AUDIO_SetTruSrndXParam(&trusrndx_param, index);

	if (KADP_OK != ret)
		return NOT_OK;

	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_VX_GetTsxHeightMixCoeff(int* heightCoeff, int index)
{
	KADP_STATUS_T ret;
	AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO trusrndx_param;
	if(heightCoeff == NULL) return NOT_OK;

	trusrndx_param.ParameterType = ENUM_DTS_PARAM_TSX_HEIGHTMIX_COEFF_I32;
	trusrndx_param.Value[0] = 0;
	trusrndx_param.Value[1] = 0xff;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0])*2;
	ret = KADP_AUDIO_GetTruSrndXParam(&trusrndx_param, index);

	*heightCoeff = trusrndx_param.Value[0];

	if (KADP_OK != ret)
		return NOT_OK;

	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_VX_SetCenterGain(int centerGain, int index)
{
	KADP_STATUS_T ret;
	AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO trusrndx_param;
	AUDIO_INFO("[%s] centerGain %d\n", __FUNCTION__, centerGain);

	trusrndx_param.ParameterType = ENUM_DTS_PARAM_TSX_CENTER_GAIN_I32;
	trusrndx_param.Value[0] = centerGain;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0]);
	ret = KADP_AUDIO_SetTruSrndXParam(&trusrndx_param, index);

	if (KADP_OK != ret)
		return NOT_OK;

	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_VX_GetCenterGain(int* centerGain, int index)
{
	KADP_STATUS_T ret;
	AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO trusrndx_param;
	if(centerGain == NULL) return NOT_OK;

	trusrndx_param.ParameterType = ENUM_DTS_PARAM_TSX_CENTER_GAIN_I32;
	trusrndx_param.Value[0] = 0;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0]);
	ret = KADP_AUDIO_GetTruSrndXParam(&trusrndx_param, index);

	*centerGain = trusrndx_param.Value[0];

	if (KADP_OK != ret)
		return NOT_OK;

	return OK;
}

DTV_STATUS_T HAL_AUDIO_LGSE_VX_SetCertParam(BOOLEAN bEnable, int index)
{
	KADP_STATUS_T ret;
	AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO trusrndx_param;
	AUDIO_VIRTUALX_MBHL_PARAM_INFO mbhl_param;
	AUDIO_INFO("[%s] bEnable %d\n", __FUNCTION__, bEnable);

	if(bEnable == FALSE) {
		memset(&trusrndx_param, 0, sizeof(AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO));
		trusrndx_param.ParameterType = ENUM_DTS_PARAM_VX_ENABLE_I32;
		trusrndx_param.Value[0] = 0;
		trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0]);
		ret = KADP_AUDIO_SetTruSrndXParam(&trusrndx_param, index);
		if(KADP_OK != ret) return NOT_OK;
		return OK;
	}

	memset(&trusrndx_param, 0, sizeof(AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO));
	trusrndx_param.ParameterType = ENUM_DTS_PARAM_VX_HEADROOM_GAIN_I32;
	trusrndx_param.Value[0] = 381178348;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0]);
	ret = KADP_AUDIO_SetTruSrndXParam(&trusrndx_param, index);
	if(KADP_OK != ret) return NOT_OK;

	memset(&trusrndx_param, 0, sizeof(AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO));
	trusrndx_param.ParameterType = ENUM_DTS_PARAM_VX_PROC_OUTPUT_GAIN_I32;
	trusrndx_param.Value[0] = 268435456;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0]);
	ret = KADP_AUDIO_SetTruSrndXParam(&trusrndx_param, index);
	if(KADP_OK != ret) return NOT_OK;

	memset(&trusrndx_param, 0, sizeof(AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO));
	trusrndx_param.ParameterType = ENUM_DTS_PARAM_TSX_ENABLE_I32;
	trusrndx_param.Value[0] = 1;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0]);
	ret = KADP_AUDIO_SetTruSrndXParam(&trusrndx_param, index);
	if(KADP_OK != ret) return NOT_OK;

	memset(&trusrndx_param, 0, sizeof(AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO));
	trusrndx_param.ParameterType = ENUM_DTS_PARAM_TSX_PASSIVEMATRIXUPMIX_ENABLE_I32;
	trusrndx_param.Value[0] = 1;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0]);
	ret = KADP_AUDIO_SetTruSrndXParam(&trusrndx_param, index);
	if(KADP_OK != ret) return NOT_OK;

	memset(&trusrndx_param, 0, sizeof(AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO));
	trusrndx_param.ParameterType = ENUM_DTS_PARAM_TSX_HEIGHT_UPMIX_ENABLE_I32;
	trusrndx_param.Value[0] = 1;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0]);
	ret = KADP_AUDIO_SetTruSrndXParam(&trusrndx_param, index);
	if(KADP_OK != ret) return NOT_OK;

	memset(&trusrndx_param, 0, sizeof(AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO));
	trusrndx_param.ParameterType = ENUM_DTS_PARAM_TSX_HORIZ_VIR_EFF_CTRL_I32;
	trusrndx_param.Value[0] = 0;
	trusrndx_param.Value[1] = 0xff;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0])*2;
	ret = KADP_AUDIO_SetTruSrndXParam(&trusrndx_param, index);
	if(KADP_OK != ret) return NOT_OK;

	memset(&trusrndx_param, 0, sizeof(AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO));
	trusrndx_param.ParameterType = ENUM_DTS_PARAM_TSX_LPR_GAIN_I32;
	trusrndx_param.Value[0] = 536870912;
	trusrndx_param.Value[1] = 0xff;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0])*2;
	ret = KADP_AUDIO_SetTruSrndXParam(&trusrndx_param, index);
	if(KADP_OK != ret) return NOT_OK;

	memset(&trusrndx_param, 0, sizeof(AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO));
	trusrndx_param.ParameterType = ENUM_DTS_PARAM_TSX_HEIGHTMIX_COEFF_I32;
	trusrndx_param.Value[0] = 536870912;
	trusrndx_param.Value[1] = 0xff;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0])*2;
	ret = KADP_AUDIO_SetTruSrndXParam(&trusrndx_param, index);
	if(KADP_OK != ret) return NOT_OK;

	memset(&trusrndx_param, 0, sizeof(AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO));
	trusrndx_param.ParameterType = ENUM_DTS_PARAM_TSX_CENTER_GAIN_I32;
	trusrndx_param.Value[0] = 536870912;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0]);
	ret = KADP_AUDIO_SetTruSrndXParam(&trusrndx_param, index);
	if(KADP_OK != ret) return NOT_OK;

	memset(&mbhl_param, 0, sizeof(AUDIO_VIRTUALX_MBHL_PARAM_INFO));
	mbhl_param.ParameterType = ENUM_DTS_PARAM_MBHL_BYPASS_GAIN_I32;
	mbhl_param.Value[0] = 1073741824;
	mbhl_param.ValueSize = sizeof(mbhl_param.Value[0]);
	ret = KADP_AUDIO_SetMbhlParam(&mbhl_param);
	if(KADP_OK != ret) return NOT_OK;

	memset(&mbhl_param, 0, sizeof(AUDIO_VIRTUALX_MBHL_PARAM_INFO));
	mbhl_param.ParameterType = ENUM_DTS_PARAM_MBHL_REFERENCE_LEVEL_I32;
	mbhl_param.Value[0] = 67108864;
	mbhl_param.ValueSize = sizeof(mbhl_param.Value[0]);
	ret = KADP_AUDIO_SetMbhlParam(&mbhl_param);
	if(KADP_OK != ret) return NOT_OK;

	memset(&mbhl_param, 0, sizeof(AUDIO_VIRTUALX_MBHL_PARAM_INFO));
	mbhl_param.ParameterType = ENUM_DTS_PARAM_MBHL_BOOST_I32;
	mbhl_param.Value[0] = 4194304;
	mbhl_param.ValueSize = sizeof(mbhl_param.Value[0]);
	ret = KADP_AUDIO_SetMbhlParam(&mbhl_param);
	if(KADP_OK != ret) return NOT_OK;

	memset(&mbhl_param, 0, sizeof(AUDIO_VIRTUALX_MBHL_PARAM_INFO));
	mbhl_param.ParameterType = ENUM_DTS_PARAM_MBHL_OUTPUT_GAIN_I32;
	mbhl_param.Value[0] = 1073741824;
	mbhl_param.ValueSize = sizeof(mbhl_param.Value[0]);
	ret = KADP_AUDIO_SetMbhlParam(&mbhl_param);
	if(KADP_OK != ret) return NOT_OK;

	memset(&mbhl_param, 0, sizeof(AUDIO_VIRTUALX_MBHL_PARAM_INFO));
	mbhl_param.ParameterType = ENUM_DTS_PARAM_MBHL_ENABLE_I32;
	mbhl_param.Value[0] = 1;
	mbhl_param.ValueSize = sizeof(mbhl_param.Value[0]);
	ret = KADP_AUDIO_SetMbhlParam(&mbhl_param);
	if(KADP_OK != ret) return NOT_OK;

	memset(&trusrndx_param, 0, sizeof(AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO));
	trusrndx_param.ParameterType = ENUM_DTS_PARAM_VX_ENABLE_I32;
	trusrndx_param.Value[0] = 1;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0]);
	ret = KADP_AUDIO_SetTruSrndXParam(&trusrndx_param, index);
	if(KADP_OK != ret) return NOT_OK;

	return OK;
}

/* Controls phantom center mix level to the center channel for selected input channel */
DTV_STATUS_T HAL_AUDIO_LGSE_VX_SetTsxLPRGainAsInputChannel(HAL_AUDIO_LGSE_VX_INPUT_CHANNEL_T inputChannel, SINT32 lprGain, int index)
{
	KADP_STATUS_T ret;
	AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO trusrndx_param;
	AUDIO_INFO("[%s] inputChannel %d lprGain %d\n", __FUNCTION__, inputChannel, lprGain);

	memset(&trusrndx_param, 0, sizeof(AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO));
	trusrndx_param.ParameterType = ENUM_DTS_PARAM_TSX_LPR_GAIN_I32;
	trusrndx_param.Value[0] = lprGain;
	trusrndx_param.Value[1] = inputChannel;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0])*2;
	ret = KADP_AUDIO_SetTruSrndXParam(&trusrndx_param, index);
	if(KADP_OK != ret) return NOT_OK;

	return OK;
}

/* Get phantom center mix level to the center channel for selected input channel */
DTV_STATUS_T HAL_AUDIO_LGSE_VX_GetTsxLPRGainAsInputChannel(HAL_AUDIO_LGSE_VX_INPUT_CHANNEL_T inputChannel, SINT32 *pLprGain, int index)
{
	KADP_STATUS_T ret;
	AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO trusrndx_param;

	if(pLprGain == NULL) return NOT_OK;

	memset(&trusrndx_param, 0, sizeof(AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO));
	trusrndx_param.ParameterType = ENUM_DTS_PARAM_TSX_LPR_GAIN_I32;
	trusrndx_param.Value[0] = 0;
	trusrndx_param.Value[1] = inputChannel;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0])*2;
	ret = KADP_AUDIO_GetTruSrndXParam(&trusrndx_param, index);

	*pLprGain = trusrndx_param.Value[0];
	AUDIO_INFO("[%s] inputChannel(%x),*pLprGain(%x)\n", __FUNCTION__, inputChannel,*pLprGain);

	if (KADP_OK != ret)
		return NOT_OK;

	return OK;
}

/* Controls virtualization effect strengths for horizontal plane sources for selected input channel */
DTV_STATUS_T HAL_AUDIO_LGSE_VX_SetTsxHorizVirEffAsInputChannel(HAL_AUDIO_LGSE_VX_INPUT_CHANNEL_T inputChannel, BOOLEAN bEnable, int index)
{
	KADP_STATUS_T ret;
	AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO trusrndx_param;
	AUDIO_INFO("[%s] inputChannel %d bEnable %d\n", __FUNCTION__, inputChannel, bEnable);

	memset(&trusrndx_param, 0, sizeof(AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO));
	trusrndx_param.ParameterType = ENUM_DTS_PARAM_TSX_HORIZ_VIR_EFF_CTRL_I32;
	trusrndx_param.Value[0] = bEnable;
	trusrndx_param.Value[1] = inputChannel;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0])*2;
	ret = KADP_AUDIO_SetTruSrndXParam(&trusrndx_param, index);
	if(KADP_OK != ret) return NOT_OK;

	return OK;
}

/* Get virtualization effect strengths for horizontal plane sources for selected input channel */
DTV_STATUS_T HAL_AUDIO_LGSE_VX_GetTsxHorizVirEffAsInputChannel(HAL_AUDIO_LGSE_VX_INPUT_CHANNEL_T inputChannel, BOOLEAN *pEnable, int index)
{
	KADP_STATUS_T ret;
	AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO trusrndx_param;

	if(pEnable == NULL) return NOT_OK;

	memset(&trusrndx_param, 0, sizeof(AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO));
	trusrndx_param.ParameterType = ENUM_DTS_PARAM_TSX_HORIZ_VIR_EFF_CTRL_I32;
	trusrndx_param.Value[0] = 0;
	trusrndx_param.Value[1] = inputChannel;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0])*2;
	ret = KADP_AUDIO_GetTruSrndXParam(&trusrndx_param, index);

	*pEnable = trusrndx_param.Value[0];
	AUDIO_INFO("[%s] inputChannel(%x),pEnable(%x)\n", __FUNCTION__, inputChannel,*pEnable);

	if (KADP_OK != ret)
		return NOT_OK;

	return OK;
}

/* Controls the level of height signal in downmix to horizontal plane channels for selected input channel */
DTV_STATUS_T HAL_AUDIO_LGSE_VX_SetTsxHeightMixCoeffAsInputChannel(HAL_AUDIO_LGSE_VX_INPUT_CHANNEL_T inputChannel, SINT32 heightCoeff, int index)
{
	KADP_STATUS_T ret;
	AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO trusrndx_param;
	AUDIO_INFO("[%s] inputChannel %d heightCoeff %d\n", __FUNCTION__, inputChannel, heightCoeff);

	memset(&trusrndx_param, 0, sizeof(AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO));
	trusrndx_param.ParameterType = ENUM_DTS_PARAM_TSX_HEIGHTMIX_COEFF_I32;
	trusrndx_param.Value[0] = heightCoeff;
	trusrndx_param.Value[1] = inputChannel;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0])*2;
	ret = KADP_AUDIO_SetTruSrndXParam(&trusrndx_param, index);
	if(KADP_OK != ret) return NOT_OK;

	return OK;
}

/* Get the level of height signal in downmix to horizontal plane channels for selected input channel */
DTV_STATUS_T HAL_AUDIO_LGSE_VX_GetTsxHeightMixCoeffAsInputChannel(HAL_AUDIO_LGSE_VX_INPUT_CHANNEL_T inputChannel, SINT32 *pHeightCoeff, int index)
{
	KADP_STATUS_T ret;
	AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO trusrndx_param;

	if(pHeightCoeff == NULL) return NOT_OK;

	memset(&trusrndx_param, 0, sizeof(AUDIO_VIRTUALX_TRUSRNDX_PARAM_INFO));
	trusrndx_param.ParameterType = ENUM_DTS_PARAM_TSX_HEIGHTMIX_COEFF_I32;
	trusrndx_param.Value[0] = 0;
	trusrndx_param.Value[1] = inputChannel;
	trusrndx_param.ValueSize = sizeof(trusrndx_param.Value[0])*2;
	ret = KADP_AUDIO_GetTruSrndXParam(&trusrndx_param, index);

	*pHeightCoeff = trusrndx_param.Value[0];
	AUDIO_INFO("[%s] inputChannel(%x),*pHeightCoeff(%x)\n", __FUNCTION__, inputChannel,*pHeightCoeff);

	if (KADP_OK != ret)
		return NOT_OK;

	return OK;
}

char *SEMODEName[] = {
    "UNKNOWN",
    "LGSE_ONLY",
    "LGSE_VX",
    "LGSE_ATMOS",
    "LGSE_AISOUND",
    "LGSE_MODE_UNKNOWN",
};
char* GetSeModeName(lgse_se_mode_ext_type_t index)
{
   int id;
   id = (int)index;
   if((int)index >= (sizeof(SEMODEName)/sizeof(SEMODEName[0])))
   {
       AUDIO_ERROR("unknown hal index %d \n", index);
       id = (sizeof(SEMODEName)/sizeof(SEMODEName[0])) - 1;// unknown
   }
   return SEMODEName[id];
}

int32_t get_lgse_status(char* str)
{
    int len, total = 0;
    int bt_sur_connected = IS_SE_BT_Connected();

    len = sprintf(str+total, "[LGSE_SPK]\n");
    total += len;
    len = sprintf(str+total, "seMode=%s\n", GetSeModeName(g_lgseInfo.se_mode));
    total += len;
    len = sprintf(str+total, "lgseModeFlag=0x%x\n", g_lgseInfo.spk_flag);
    total += len;
    len = sprintf(str+total, "connected=%d\n", !bt_sur_connected);
    total += len;

    len = sprintf(str+total, "\nLGSE_SPK_BTSUR]\n");
    total += len;
    len = sprintf(str+total, "seMode=%s\n", GetSeModeName(g_lgseInfo.se_mode));
    total += len;
    len = sprintf(str+total, "lgseModeFlag=0x%x\n", g_lgseInfo.spk_btsur_flag);
    total += len;
    len = sprintf(str+total, "connected=%d\n", bt_sur_connected);
    total += len;

    len = sprintf(str+total, "\n[LGSE_BT_BTSUR]\n");
    total += len;
    len = sprintf(str+total, "seMode=%s\n", GetSeModeName(g_lgseInfo.se_mode));
    total += len;
    len = sprintf(str+total, "lgseModeFlag=0x%x\n", g_lgseInfo.bt_btsur_flag);
    total += len;
    len = sprintf(str+total, "connected=%d\n", bt_sur_connected);
    total += len;

    return total;
}
