#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/delay.h>

#include "rdvb-adapter/rdvb_adapter.h"
#include "rdvb-dmx/rdvb_dmx.h"
#include "rdvb-dmx/rdvb_dmx_ctrl.h"


#include <tp/tp_drv_api.h>
#include "rdvb_cip_ctrl.h"


#include <mach/rtk_platform.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#define to_user_ptr(x)          compat_ptr((UINT32) x)
#else
#define to_user_ptr(x)          ((void* __user)(x)) // convert 32 bit value to user pointer
#endif

/************** globa para *****************/
tpp_priv_data* tpp_list;


int filter_cnt_list[MAX_TP_P_COUNT] = { TP0_PID_FILTER_COUNT,TP1_PID_FILTER_COUNT,TP2_PID_FILTER_COUNT };
int sync_byte_list[MAX_TP_P_COUNT]  = { 0x47, 0x48, 0x49 };



static int cip_set_mtpbuf(tpp_priv_data *tpp_priv)
{
	unsigned int tp_buf_align  = 0;
	void* uncached;
	int size = CIP_MTP_BUFFER_SIZE;
	
	RDVB_CIP_LOG_TRACE("%s tpp_mtp_id=%d \n", __func__,tpp_priv->mtp_id-TP_TPP0_MTP);

	tpp_priv->mtpBuffer.cachedaddr = (UINT8 *)dvr_malloc_uncached_specific(size,GFP_DCU1,&uncached);
			
	tpp_priv->mtpBuffer.nonCachedaddr = (unsigned char *)uncached;
	if (!(tpp_priv->mtpBuffer.cachedaddr)) {
		RDVB_CIP_LOG_ERROR("%s cannot allocate memory by using dvr_malloc_uncached_specific. size = %x\n", __func__, size) ;
		return -EFAULT ;
	} else {
		tpp_priv->mtpBuffer.phyAddr = dvr_to_phys(tpp_priv->mtpBuffer.cachedaddr) ;
		tpp_priv->mtpBuffer.virAddr = tpp_priv->mtpBuffer.nonCachedaddr ;
		tpp_priv->mtpBuffer.isCache = false ;
	}
	tpp_priv->mtpBuffer.fromPoll = 0 ;

	tpp_priv->mtpBuffer.phyAddrOffset = (long)tpp_priv->mtpBuffer.phyAddr - (long)tpp_priv->mtpBuffer.virAddr;
	if (tpp_priv->tsPacketSize == 192)
		tp_buf_align = 192;
	else
		tp_buf_align = 188;

	tpp_priv->mtpBuffer.size = size - (size % tp_buf_align);
	RDVB_CIP_LOG_DEBUG("[%s %d] mtpBuffer.phyAddr = 0x%lx\n",__func__, __LINE__, (long)tpp_priv->mtpBuffer.phyAddr);
		 
	tpp_priv->mtpBuffer.fromPoll = 0 ;

	if (RHAL_SetMTPBuffer(tpp_priv->mtp_id, (UINT8 *)tpp_priv->mtpBuffer.phyAddr, (UINT8 *)tpp_priv->mtpBuffer.virAddr,
				tpp_priv->mtpBuffer.size) != TPK_SUCCESS) {
		RDVB_CIP_LOG_ERROR("[%s %d] Error !!RHAL_SetMTPBuffer failed!\n",__func__, __LINE__);
		return -1;
	}
	tpp_priv->mtpBuffer.inited = 1 ;

	return 0;
}

static int cip_unset_mtpbuf(tpp_priv_data *tpp_priv)
{
	int ret = 0;
	
	RDVB_CIP_LOG_TRACE("%s tpp_mtp_id=%d \n", __func__,tpp_priv->mtp_id-TP_TPP0_MTP);

	//In IP-Delivery mode, we need stop mtp butter to tpp
	ret = RHAL_MTPStreamControl(tpp_priv->mtp_id,MTP_STREAM_STOP);

	if (tpp_priv->mtpBuffer.cachedaddr && !tpp_priv->mtpBuffer.fromPoll) {
		dvr_free(tpp_priv->mtpBuffer.cachedaddr) ;
	}

	tpp_priv->mtpBuffer.phyAddr = 0;
	tpp_priv->mtpBuffer.allocSize = 0;
	tpp_priv->mtpBuffer.inited = 0;
	tpp_priv->mtpBuffer.cachedaddr = 0;
	
	/*Notify TP that mtp buffer was freed*/
	RHAL_SetMTPBuffer(tpp_priv->mtp_id, NULL, NULL, 0);

	return ret;
}



/*****************************************************************************
  rdvb_cip_ctrl_init : RHAL_TPP_Init
  allocate tpp ring buffer
*****************************************************************************/

int rdvb_cip_init(struct rdvb_cip_tpp *rdvb_tpp)
{
	int ret = 0;
	tpp_priv_data *tpp_priv = rdvb_tpp->data;
	
	RDVB_CIP_LOG_TRACE("%s tpp_id=%d \n", __func__,tpp_priv->tpp_id);
	
	//ret = RHAL_TPP_Init(tpp_priv->tpp_id);
	
	if (ret < 0) {
		RDVB_CIP_LOG_ERROR("%s tpp_id=%d RHAL_TPP_Init fail\n", __func__,tpp_priv->tpp_id);
		return ret;
	}
	return ret;
}

int rdvb_cip_uninit(struct rdvb_cip_tpp *rdvb_tpp)
{
	int ret = 0;
	tpp_priv_data *tpp_priv = rdvb_tpp->data;

	RDVB_CIP_LOG_TRACE("%s tpp_id=%d \n", __func__,tpp_priv->tpp_id);

	if(tpp_priv->tpp_src == MTP){
		//In IP-Delivery mode, we need stop mtp butter to tpp
		RHAL_MTPStreamControl(tpp_priv->mtp_id,MTP_STREAM_STOP);
	}else{
		//In TS mode, we need stop tpp framer 
		RHAL_TPP_StreamControl(tpp_priv->tpp_id,TPP_STREAM_STOP);
	}

	if(tpp_priv->mtpBuffer.inited){
		cip_unset_mtpbuf(tpp_priv);
	}
	
	//disable tpp output to TPO
	RHAL_TPP_OutEnable(tpp_priv->tpp_id,0);
	
	//ret = RHAL_TPP_Uninit(tpp_priv->tpp_id);
	
	tpp_priv->tpp_src = TS_UNMAPPING;
	
	if (ret < 0) {
		RDVB_CIP_LOG_ERROR("%s tpp_id=%d RHAL_TPP_Init fail\n", __func__,tpp_priv->tpp_id);
		return ret;
	}
	return ret;
}


int rdvb_cip_set_config(struct rdvb_cip_tpp *rdvb_tpp, struct ca_ext_source source)
{
	TPK_TP_SOURCE_T	 tpp_src 	= TS_UNMAPPING;
	TPK_INPUT_PORT_T portType 	= TPK_PORT_NONE;
	tpp_priv_data *tpp_priv   	= rdvb_tpp->data;
	int ret = 0;
	
	RDVB_CIP_LOG_TRACE("%s tpp_id=%d \n", __func__,tpp_priv->tpp_id);

	switch(source.input_src_type){
	    case CA_EXT_SRC_TYPE_IN_DEMOD:
	        tpp_src = Internal_Demod;
	        break;
	    case CA_EXT_SRC_TYPE_EXT_DEMOD:
	        if(source.input_port_num == 0)
	            portType = TPK_PORT_EXT_INPUT0;
	        else if(source.input_port_num == 1)
	            portType = TPK_PORT_EXT_INPUT1;
	        else if(source.input_port_num == 2)
	            portType = TPK_PORT_EXT_INPUT2;

	        tpp_src = RHAL_GetTPSource(portType);
	        break;
	    case CA_EXT_SRC_TYPE_MEM:
	        tpp_src = MTP;
	        break;
	    case CA_EXT_SRC_TYPE_NULL:
	        tpp_src = TS_SRC_2;
	        break;
	    default:
	        tpp_src = TS_UNMAPPING;
	        break;
    }
	
	if(tpp_src == MTP){
	    if(!tpp_priv->mtpBuffer.inited){
	        cip_set_mtpbuf(tpp_priv);
	    }
	}else if(tpp_src == TS_UNMAPPING){
	    cip_unset_mtpbuf(tpp_priv);
	}
	
	//set sync byte
	RHAL_TPP_SetSyncReplace(tpp_priv->tpp_id,tpp_priv->syncbyte);
	
	RDVB_CIP_LOG_DEBUG("%s tpp_id=%d , tpp_src=%d(0:TS_UNMAPPING/1:MTP/2:INDEMOD/3:EXTDEMOD0/4:EXTDEMOD1/5:EXTDEMOD2) , syncbyte=%x\n", __func__,tpp_priv->tpp_id,tpp_src,tpp_priv->syncbyte);

	ret = RHAL_TPP_SetDataSource(tpp_priv->tpp_id,tpp_src);
	if (ret < 0) {
	    RDVB_CIP_LOG_ERROR("%s set tpp source fail , tpp_priv->tpp_id=%d,input_port_num=%d,input_src_type=%d,tpp_src=%d(0:TS_UNMAPPING/1:MTP/2:INDEMOD/3:EXTDEMOD0/4:EXTDEMOD1/5:EXTDEMOD2)\n", __func__,tpp_priv->tpp_id,source.input_port_num,source.input_src_type,tpp_src);
	    return ret;
	}

	tpp_priv->tpp_src = tpp_src;

	return  ret;
}

int rdvb_cip_get_filternum(struct rdvb_cip_tpp *rdvb_tpp)
{
	tpp_priv_data *tpp_priv = rdvb_tpp->data;
	
	RDVB_CIP_LOG_TRACE("%s tpp_id=%d \n", __func__,tpp_priv->tpp_id);

	return  tpp_priv->filter_num;
}


int rdvb_cip_add_filter(struct rdvb_cip_tpp *rdvb_tpp, UINT16 PID)
{
	int ret = 0,i;
	tpp_priv_data *tpp_priv = rdvb_tpp->data;
	
	RDVB_CIP_LOG_TRACE("%s tpp_id=%d,PID=%x\n", __func__,tpp_priv->tpp_id, PID);

	if(PID==0x1FFF){
		//all pass
		ret = RHAL_TPP_EnablePIDFilter(tpp_priv->tpp_id,0);
	}else{
		//call seetv demux setbypasspid function
		for(i = 0; i < PID_MAX_NUM_TO_SET; i++) {
			//RDVB_CIP_LOG_DEBUG("%s add tpp_priv->pidTable.pidList[%d] valid = %d, PID= %x count=%d ,filter=%p, &filter=%p\n", __func__, i, tpp_priv->pidTable.pidList[i].valid, tpp_priv->pidTable.pidList[i].pid,tpp_priv->pidTable.count,tpp_priv->pidTable.pidList[i].filter,&tpp_priv->pidTable.pidList[i].filter);
			if(!tpp_priv->pidTable.pidList[i].valid) {
				ret = rdvb_dmx_ctrl_ci_filter(tpp_priv->tpp_id,PID,&tpp_priv->pidTable.pidList[i].filter);
				if(ret==0){
					tpp_priv->pidTable.pidList[i].valid = true;
					tpp_priv->pidTable.pidList[i].pid = PID;
					tpp_priv->pidTable.count++;
				}
				break;
			}
		}
		if(i == PID_MAX_NUM_TO_SET){
			ret = -2;
			RDVB_CIP_LOG_ERROR("%s fail , pid full , ret = %d, tpp_id = %d, PID= %x MAX pid count=%d\n", __func__, ret, tpp_priv->tpp_id, PID ,PID_MAX_NUM_TO_SET);
		}else{
			ret = RHAL_TPP_EnablePIDFilter(tpp_priv->tpp_id,1);
		}
		
	}

	if (ret < 0) {
		RDVB_CIP_LOG_ERROR("%s fail , ret = %d, tpp_id = %d, PID= %x \n", __func__, ret, tpp_priv->tpp_id, PID);
		return ret;
	}
	return  ret;
}

int rdvb_cip_remove_filter(struct rdvb_cip_tpp *rdvb_tpp, UINT16 PID)
{
	int ret = 0,i;
	tpp_priv_data *tpp_priv = rdvb_tpp->data;
	
	RDVB_CIP_LOG_TRACE("%s tpp_id=%d,PID=%x\n", __func__,tpp_priv->tpp_id, PID);
	
	if(PID==0x1FFF){
		//remove all pid filter
		for(i = 0; i < PID_MAX_NUM_TO_SET; i++) {
			if(tpp_priv->pidTable.pidList[i].valid) {
				ret = rdvb_dmx_ctrl_ci_filter(tpp_priv->tpp_id,PID,&tpp_priv->pidTable.pidList[i].filter);
				if(ret==0){
					tpp_priv->pidTable.pidList[i].valid = false;
					tpp_priv->pidTable.pidList[i].pid = 0x1FFF;
					tpp_priv->pidTable.count--;
				}
			}
		}
	}else{
		//find out the pid,and remove the filter
		for(i = 0; i < PID_MAX_NUM_TO_SET; i++) {
			if(tpp_priv->pidTable.pidList[i].valid && (tpp_priv->pidTable.pidList[i].pid == PID)) {
				ret = rdvb_dmx_ctrl_ci_filter(tpp_priv->tpp_id,PID,&tpp_priv->pidTable.pidList[i].filter);
				if(ret==0){
					tpp_priv->pidTable.pidList[i].valid = false;
					tpp_priv->pidTable.pidList[i].pid = 0x1FFF;
					tpp_priv->pidTable.count--;
				}
				break;
			}
		}
	}
	
	if (ret < 0) {
		RDVB_CIP_LOG_ERROR("%s fail , ret= %d, tpp_id = %d, PID= %x \n", __func__, ret, tpp_priv->tpp_id, PID);
		return ret;
	}
	return  ret;
}
/*****************************************************************************
  rtk_tpp_startSendingData : tpp start send data to TPO
  In IP-Delivery mode, we need let mtp buffer to tpp, then tpp will output to TPO
  In TS mode, we need stream start tpp, then tpp will output to TPO
*****************************************************************************/

int rdvb_cip_enable_sending(struct rdvb_cip_tpp *rdvb_tpp, UINT8 enable)
{
	int ret = 0;
	tpp_priv_data *tpp_priv = rdvb_tpp->data;
	
	RDVB_CIP_LOG_TRACE("%s tpp_id=%d ,enable=%d\n", __func__,tpp_priv->tpp_id,enable);
	
	if(enable){
		if(tpp_priv->tpp_src == MTP){
			ret = RHAL_MTPStreamControl(tpp_priv->mtp_id,MTP_STREAM_START);
			if ( ret != TPK_SUCCESS){
				RDVB_CIP_LOG_ERROR("%s  RHAL_MTPStreamControl start fail , mtp_id = %d , ret = %d \n", __func__, (tpp_priv->mtp_id-TP_TPP0_MTP), ret);
				return TPK_FAIL;
			}
		}else{
			ret = RHAL_TPP_StreamControl(tpp_priv->tpp_id,TPP_STREAM_START);
			if ( ret != TPK_SUCCESS){
				RDVB_CIP_LOG_ERROR("%s	RHAL_TPP_StreamControl start fail , tpp_id = %d , ret = %d \n", __func__, tpp_priv->tpp_id, ret);
				return TPK_FAIL;
			}
		}	
	}else{
		if(tpp_priv->tpp_src == MTP){
			//In IP-Delivery mode, we need stop mtp butter to tpp
			ret = RHAL_MTPStreamControl(tpp_priv->mtp_id,MTP_STREAM_STOP);
			if ( ret != TPK_SUCCESS){
				RDVB_CIP_LOG_ERROR("%s  RHAL_MTPStreamControl stop fail , mtp_id = %d , ret = %d \n", __func__, (tpp_priv->mtp_id-TP_TPP0_MTP), ret);
				return TPK_FAIL;
			}
		}else{
			//In TS mode, we need stop tpp framer 
			ret = RHAL_TPP_StreamControl(tpp_priv->tpp_id,TPP_STREAM_STOP);
			if ( ret != TPK_SUCCESS){
				RDVB_CIP_LOG_ERROR("%s	RHAL_TPP_StreamControl stop fail , tpp_id = %d , ret = %d \n", __func__, tpp_priv->tpp_id, ret);
				return TPK_FAIL;
			}
		}
	}
	
	//enable tpp output to TPO
	ret = RHAL_TPP_OutEnable(tpp_priv->tpp_id,enable);

	if (ret < 0) {
		RDVB_CIP_LOG_ERROR("%s RHAL_TPP_OutEnable fail , tpp_id = %d enable=%d ret = %d\n", __func__, tpp_priv->tpp_id,enable, ret);
		return ret;
	}

	return  ret;
}



int rdvb_cip_wrtie(struct rdvb_cip_tpp *rdvb_tpp, const char __user *buf, size_t count)
{

	UINT8 *pWritePhyAddr = 0;
	UINT8 *pMtpBufUpper = 0;
	UINT8 *pMtpBufLower = 0;
	UINT32 contiguousWritableSize = 0;
	UINT32 writableSize = 0;
	UINT32 requireWriteSize = count;
	UINT8 mtp_id;
	tpp_priv_data *tpp_priv = rdvb_tpp->data;
		
	RDVB_CIP_LOG_TRACE("%s tpp_id = %d count=%d \n", __func__,tpp_priv->tpp_id,(int)count);
	
	if (!buf || count <=0) {
		RDVB_CIP_LOG_ERROR("[%s, %d]:Invalid parameters,buf=%p,count=%d!!\n",__func__, __LINE__, buf, count);
		return -EFAULT;
	}
	if(!tpp_priv->mtpBuffer.inited) {
		RDVB_CIP_LOG_ERROR("[%s, %d]:TPP buffer not inited , tpp src=%d(0:TS_UNMAPPING/1:MTP/2:INDEMOD/3:EXTDEMOD0/4:EXTDEMOD1/5:EXTDEMOD2)!!\n",__func__, __LINE__,  tpp_priv->tpp_src);
		return -EFAULT;
	}

	mtp_id = tpp_priv->mtp_id;
	pMtpBufLower = (UINT8 *)tpp_priv->mtpBuffer.phyAddr;
	pMtpBufUpper = (UINT8 *)tpp_priv->mtpBuffer.phyAddr+ tpp_priv->mtpBuffer.size;

	if (RHAL_GetMTPBufferStatus(mtp_id, &pWritePhyAddr, &contiguousWritableSize,&writableSize) != TPK_SUCCESS){
		RDVB_CIP_LOG_ERROR("[%s, %d]:RHAL_GetMTPBufferStatus fail !!\n",__func__, __LINE__);
		return -EFAULT;
	}
	
	if(writableSize < count) {
		//overflow
		RDVB_CIP_LOG_ERROR("[%s, %d]:Mtp buffer overflow require/free (%d/ %d) !! \n",__func__, __LINE__, count, writableSize);
		return -EBUSY;
	}
	
	if((pWritePhyAddr + count) > pMtpBufUpper) {
		//handle mtp buffer wrap-around
		if (copy_from_user((void *)pWritePhyAddr - tpp_priv->mtpBuffer.phyAddrOffset, buf,pMtpBufUpper -pWritePhyAddr)) {
		
			RDVB_CIP_LOG_ERROR("[%s, %d]:Copy data to mtp buffer failed !! \n",__func__, __LINE__);
			return -EFAULT;
		}
		
		RDVB_CIP_LOG_DEBUG("[%s, %d]: wrap-around RHAL_WriteMTPBuffer  Wp/Cnt(%p/%d) Base=%p,Limit=%p !\n",__func__, __LINE__, pWritePhyAddr, (pMtpBufUpper -pWritePhyAddr),pMtpBufLower,pMtpBufUpper);

		if (RHAL_WriteMTPBuffer(mtp_id, pWritePhyAddr, pMtpBufUpper -pWritePhyAddr)!= TPK_SUCCESS) {
			RDVB_CIP_LOG_ERROR("[%s, %d]: RHAL_WriteMTPBuffer failed!!\n",__func__, __LINE__);
			return -EFAULT;
		}
		buf = buf + (pMtpBufUpper -pWritePhyAddr);
		count = (pWritePhyAddr + count) - pMtpBufUpper;
		pWritePhyAddr = pMtpBufLower;
	}

	if (copy_from_user((void *)pWritePhyAddr - tpp_priv->mtpBuffer.phyAddrOffset, buf, count)) {
		RDVB_CIP_LOG_ERROR("[%s, %d]: Copy data to mtp buffer failed !! \n",__func__, __LINE__);
		return -EFAULT;
	}
	
	RDVB_CIP_LOG_DEBUG("[%s, %d]:  RHAL_WriteMTPBuffer  Wp/Cnt(%p/%d) Base=%p,Limit=%p !\n",__func__, __LINE__, pWritePhyAddr, count,pMtpBufLower,pMtpBufUpper);
	
	if (RHAL_WriteMTPBuffer(mtp_id, pWritePhyAddr, count) != TPK_SUCCESS) {
		RDVB_CIP_LOG_ERROR("[%s, %d]: RHAL_WriteMTPBuffer failed!! \n",__func__, __LINE__);
		return -EFAULT;
	}

	return requireWriteSize;
}


/*****************************************************************************
  rtk_tpp_startReceivingData : start handle the data come from CAM
  In IP-Delivery mode: 
      Host-IP-Delivery: enable tp frame, then webos can get the tp mass buffer
      CAM-IP-Delivery: enable tp frame, demux will consume the tp mass buffer
  In TS mode,  enable tp frame, demux will consume the tp mass buffer
*****************************************************************************/

int rdvb_cip_enable_receiving(struct rdvb_cip_tpp *rdvb_tpp, UINT8 enable)
{
	int ret = 0;
	tpp_priv_data *tpp_priv = rdvb_tpp->data;
	
	RDVB_CIP_LOG_TRACE("%s tpp_id = %d ,enable=%d \n", __func__,tpp_priv->tpp_id,enable);

	if(enable){
		ret = RHAL_TPStreamControl((TPK_TP_ENGINE_T)tpp_priv->tpp_id,TP_STREAM_START);
	}else{
		ret = RHAL_TPStreamControl((TPK_TP_ENGINE_T)tpp_priv->tpp_id,TP_STREAM_STOP);
	}
	
	return  0;//for tp buffer is not inited
	
	/*
	if (ret < 0) {
		RDVB_CIP_LOG_ERROR("%s fail , ret = %d, tpp_id = %d \n", __func__, ret, tpp_priv->tpp_id);
		return ret;
	}

	return  ret;
	*/
}


int rdvb_cip_read(struct rdvb_cip_tpp *rdvb_tpp, const char __user *buf, size_t count)
{
	unsigned char *tp_buf_vir;
	int   tp_read_size = 0; 	 
	tpp_priv_data *tpp_priv = rdvb_tpp->data;
	long phyAddrOffset=0;
	
	RDVB_CIP_LOG_TRACE("%s tpp_id = %d count=%d \n", __func__,tpp_priv->tpp_id,(int)count);

	RHAL_CIP_ReadData((TPK_TP_ENGINE_T)tpp_priv->tpp_id,&tp_buf_vir,&tp_read_size,188,&phyAddrOffset);
	tp_buf_vir -= phyAddrOffset;
	if(tp_read_size > count) {
		tp_read_size = count;
	}
	
	RDVB_CIP_LOG_DEBUG("%s  tpp_id = %d, tp_buf_vir=%p , phyAddrOffset=%ld, tp_read_size=%d\n", __func__, tpp_priv->tpp_id, tp_buf_vir , phyAddrOffset, tp_read_size);

	tp_read_size = (tp_read_size / tpp_priv->tsPacketSize) * tpp_priv->tsPacketSize;
		
	if(tp_read_size > 0){
		if (copy_to_user(to_user_ptr(buf), tp_buf_vir,tp_read_size)){
			RDVB_CIP_LOG_ERROR("%s copy_to_user Fail\n", __func__);
			return -EFAULT;
		} 
		RHAL_CIP_ReleaseData(tpp_priv->tpp_id, tp_buf_vir += phyAddrOffset, tp_read_size);
	}
	
	return  tp_read_size;
}


int rdvb_cip_get_cam_rate(struct rdvb_cip_tpp *rdvb_tpp, UINT32 *inBitRate, UINT32 *outBitRate)
{
	int ret = 0;
	UINT32 inputBitRate,outputBitRate;
	tpp_priv_data *tpp_priv = rdvb_tpp->data;
	
	RDVB_CIP_LOG_TRACE("%s tpp_id = %d \n", __func__,tpp_priv->tpp_id);

	ret = RHAL_CIP_GetCAMBitRate(&inputBitRate,&outputBitRate);

	RDVB_CIP_LOG_DEBUG("%s inputBitRate = %d outputBitRate=%d \n", __func__,inputBitRate,outputBitRate);

	if (ret < 0) {
		RDVB_CIP_LOG_ERROR("%s fail , ret = %d, tpp_id = %d \n", __func__, ret, tpp_priv->tpp_id);
		return ret;
	}
	*inBitRate = inputBitRate;
	*outBitRate = outputBitRate;

	return  ret;
}


tpp_priv_data* rdvb_get_cip(int index)
{
	return  &tpp_list[index];
}

EXPORT_SYMBOL(rdvb_get_cip);


/*************************************************************/
int rdvb_cip_moudle_init(void)
{
	int ret,i,j=0;
	struct rdvb_adapter* adapter = rdvb_get_adapter();

	tpp_list = vmalloc(sizeof(tpp_priv_data)* MAX_TP_P_COUNT);
	memset(tpp_list,0,sizeof(tpp_priv_data)* MAX_TP_P_COUNT);

	/* register the DVB device */
	for(i= 0 ;i < MAX_TP_P_COUNT ; i++){
		tpp_priv_data *tpp_priv      	= &tpp_list[i];
		tpp_priv->tpp_id            	= (TPK_TPP_ENGINE_T)i;
		tpp_priv->mtp_id            	= (TPK_TP_MTP_T)(i+TP_TPP0_MTP);
		tpp_priv->tpp_pid_en          	= 0;
		
		tpp_priv->tpp_src           	= TS_UNMAPPING;
		tpp_priv->filter_num           	= PID_MAX_NUM_TO_SET;
		tpp_priv->syncbyte          	= sync_byte_list[i];
		tpp_priv->mtpBuffer.inited    	= 0; 
		tpp_priv->tsPacketSize       	= 188;
		
		tpp_priv->tpp.owner         	= THIS_MODULE;
		tpp_priv->tpp.id            	= i;
		tpp_priv->tpp.init          	= rdvb_cip_init;
		tpp_priv->tpp.unInit         	= rdvb_cip_uninit;
		tpp_priv->tpp.write          	= rdvb_cip_wrtie;
		tpp_priv->tpp.read           	= rdvb_cip_read;
		tpp_priv->tpp.setConfig      	= rdvb_cip_set_config;
		tpp_priv->tpp.getFilterNum    	= rdvb_cip_get_filternum;
		tpp_priv->tpp.addFilter       	= rdvb_cip_add_filter;
		tpp_priv->tpp.removeFilter   	= rdvb_cip_remove_filter;
		tpp_priv->tpp.sendingEnable   	= rdvb_cip_enable_sending;
		tpp_priv->tpp.ReceivingEnable 	= rdvb_cip_enable_receiving;
		tpp_priv->tpp.getCamBitRate   	= rdvb_cip_get_cam_rate;

		tpp_priv->tpp.data          	= tpp_priv;
		
		ret = RHAL_TPP_Init(tpp_priv->tpp_id);

		for(j = 0; j < PID_MAX_NUM_TO_SET;j++) {
		    tpp_priv->pidTable.pidList[j].valid 	= false;
		    tpp_priv->pidTable.pidList[j].pid 		= 0x1FFF;
		    tpp_priv->pidTable.pidList[j].filter 	= NULL;
		}
		tpp_priv->pidTable.count = 0;

		ret = rdvb_cip_dev_init(adapter->dvb_adapter, &tpp_priv->tpp);
		if (ret)
			goto err;

	}

	RDVB_CIP_MUST_PRINT(" %s_%d, done success\n", __func__, __LINE__);
	return 0;
	
err:
	RDVB_CIP_LOG_ERROR(" %s_%d, fail!!!!!\n", __func__, __LINE__);
	return -ENOMEM;
}

void rdvb_cip_moudle_exit(void)
{
	int i=0;
	for(i= 0 ;i < MAX_TP_P_COUNT ; i++){
		rdvb_cip_dev_release(&tpp_list[i].tpp);
	}
	RDVB_CIP_MUST_PRINT(" %s_%d, done success\n", __func__, __LINE__);

}

module_init(rdvb_cip_moudle_init);
module_exit(rdvb_cip_moudle_exit);

MODULE_AUTHOR("xiangzhu_yang@apowertec.com>");
MODULE_DESCRIPTION("RTK TV dvb CI PLUS14 Driver");
MODULE_LICENSE("GPL");

