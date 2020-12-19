/*****************************************************************************
 *
 *   Realtek Delivery driver
 *
 *   Copyright(c) 2019 Realtek Semiconductor Corp. All rights reserved.
 *
 *****************************************************************************/
#include "rtkdelivery.h"
#include "rtkdelivery_debug.h"
#include "tvscalercontrol/vip/scalerColor.h"

#include <linux/sched.h>
#include <asm/current.h>

#define DELIVERY_CHECK_CH(x)  \
{\
        if (!delivery_device->pChArray || x >= delivery_device->chNum || !delivery_device->pChArray[x].isInit) { \
                delivery_warn(x,"func %s, line %d, Error(isInit %d, ch %d, chNum %d)!!\n", __func__, __LINE__, (delivery_device->pChArray != 0), x, delivery_device->chNum); \
                return -ENOTTY; \
        } \
}
#define DELIVERY_GET_DELIVERY_ID(_ch) (((_ch) == DELIVERY_CH_A) ? TP_TP0 : (((_ch) == DELIVERY_CH_B) ? TP_TP1 : TP_TP2))

UINT8 boot_status_isACOn = 1;
TPK_TP_ENGINE_T DELIVERY_JAPAN4K_Channel_Mapping(TPK_TP_ENGINE_T input)
{
#if 1
	switch(input)
	{
		case TP_TP0:
			return TP_TP2;
		case TP_TP1:
			return TP_TP3;
		default:
			return TP_TP2;
	}
#else
	switch(input)
	{
		case TP_TP0:
			return TP_TP0;
		case TP_TP1:
			return TP_TP1;
		default:
			return TP_TP2;
	}
#endif
}


#ifdef CONFIG_RTK_KDRV_ATSC3_DMX
int IOCTL_ReConfigTPBuffer(DELIVERY_CHANNEL_T ch);

typedef enum
{
	DELIVERY_MODE_ATSC30 = 0,
	DELIVERY_MODE_JAPAN4K,
	DELIVERY_MODE_MAX
} DELIVERY_MODE_T;


UINT8 deliveryMode = DELIVERY_MODE_ATSC30;

/*------------------------------------------------------------------
 * Func : IOCTL_DeliveryINIT
 *
 * Desc :
 *
 * Retn :
 *------------------------------------------------------------------*/
int IOCTL_Delivery_INIT(void)
{
        return 0;
}

/*------------------------------------------------------------------
 * Func : IOCTL_DeliveryUNINIT
 *
 * Desc :
 *
 * Retn :
 *------------------------------------------------------------------*/
int IOCTL_Delivery_UNINIT(void)
{
        return 0;
}

/*------------------------------------------------------------------
 * Func : IOCTL_DeliveryOpen
 *
 * Desc :
 *
 * Retn :
 *------------------------------------------------------------------*/
int IOCTL_Delivery_Open(DELIVERY_DELIVERY_CHANNEL_T *pChannel)
{
        TPK_TP_ENGINE_T tp_id;

        DELIVERY_CHECK_CH(pChannel->ch);

        tp_id = DELIVERY_GET_DELIVERY_ID(pChannel->ch);

		if( deliveryMode == DELIVERY_MODE_ATSC30)
		{
        	if(RHAL_ATSC30_Open(tp_id) != TPK_SUCCESS)
           	     return -1;
		}
		else if( deliveryMode == DELIVERY_MODE_JAPAN4K )
		{
			if(RHAL_JAPAN4K_Open(tp_id) != TPK_SUCCESS)
				 return -1;
		}

        return 0;
}

/*------------------------------------------------------------------
 * Func : IOCTL_DeliveryClose
 *
 * Desc :
 *
 * Retn :
 *------------------------------------------------------------------*/
int IOCTL_Delivery_Close(DELIVERY_DELIVERY_CHANNEL_T *pChannel)
{
        TPK_TP_ENGINE_T tp_id;

        DELIVERY_CHECK_CH(pChannel->ch);

        tp_id = DELIVERY_GET_DELIVERY_ID(pChannel->ch);

		if( deliveryMode == DELIVERY_MODE_ATSC30)
		{
	        if(RHAL_ATSC30_Close(tp_id) != TPK_SUCCESS)
	                return -1;
		}
		else if( deliveryMode == DELIVERY_MODE_JAPAN4K )
		{
			if(RHAL_JAPAN4K_Close(tp_id) != TPK_SUCCESS)
				 return -1;
		}

        return 0;
}

/*------------------------------------------------------------------
 * Func : IOCTL_DeliverySetTPInputConfig
 *
 * Desc :
 *
 * Retn :
 *------------------------------------------------------------------*/
int IOCTL_Delivery_SetTPInputConfig(DELIVERY_TP_SOURCE_CONFIG_T *pInfo)
{
        TPK_TP_ENGINE_T tp_id = DELIVERY_GET_DELIVERY_ID(pInfo->ch);

        if( deliveryMode == DELIVERY_MODE_ATSC30)
        {
                RHAL_Delivery_ATSC30_SetInputConfig(tp_id, pInfo->portType, pInfo->inputType);
        }
        else if(deliveryMode == DELIVERY_MODE_JAPAN4K)
        {
                RHAL_Delivery_JAPAN4K_SetInputConfig(tp_id, pInfo->portType, pInfo->inputType);
        }
        return 0;
}

/*------------------------------------------------------------------
 * Func : IOCTL_DeliveryRequestBBFrame
 *
 * Desc :
 *
 * Retn :
 *------------------------------------------------------------------*/
int IOCTL_Delivery_RequestBBFrame(DELIVERY_DELIVERY_CHANNEL_T *pChannel)
{
        //delivery_channel *pCh;
        TPK_TP_ENGINE_T tp_id;

        DELIVERY_CHECK_CH(pChannel->ch);

        tp_id = DELIVERY_GET_DELIVERY_ID(pChannel->ch);

		if( deliveryMode == DELIVERY_MODE_ATSC30)
		{
			// only run in raw mode
			//if(RHAL_EnablePIDFilter(tp_id, 0) != TPK_SUCCESS)
			//        return -1;

			//pCh = &delivery_device->pChArray[pChannel->ch];
			//pCh->startStreaming = 1;

	        RHAL_ATSC30_RequestBBFrame(tp_id);
		}
		fwif_color_set_force_I_De_XC_Disable(1);
#ifdef CONFIG_SUPPORT_SCALER
        fwif_color_set_force_I_De_XC_Disable(1);
#endif
        return 0;
}

/*------------------------------------------------------------------
 * Func : IOCTL_DeliveryCancelBBFrame
 *
 * Desc :
 *
 * Retn :
 *------------------------------------------------------------------*/
int IOCTL_Delivery_CancelBBFrame(DELIVERY_DELIVERY_CHANNEL_T *pChannel)
{
        TPK_TP_ENGINE_T tp_id;
        //delivery_channel *pCh;

        DELIVERY_CHECK_CH(pChannel->ch);

        tp_id = DELIVERY_GET_DELIVERY_ID(pChannel->ch);

		if( deliveryMode == DELIVERY_MODE_ATSC30)
		{
			// only run in raw mode
			//pCh = &delivery_device->pChArray[pChannel->ch];
			//pCh->startStreaming = 0;

	        RHAL_ATSC30_CancelBBFrame(tp_id);
		}

#ifdef CONFIG_SUPPORT_SCALER
		fwif_color_set_force_I_De_XC_Disable(0);
#endif
        return 0;
}

/*------------------------------------------------------------------
 * Func : IOCTL_DeliveryGetBBFrameBuffer
 *
 * Desc :
 *
 * Retn :
 *------------------------------------------------------------------*/
int IOCTL_Delivery_GetBBFrameBuffer(DELIVERY_DELIVERY_GET_BBF_BUFFER_T *pBBFBuff)
{
        TPK_TP_ENGINE_T tp_id;

        DELIVERY_CHECK_CH(pBBFBuff->ch);

        tp_id = DELIVERY_GET_DELIVERY_ID(pBBFBuff->ch);

		if( deliveryMode == DELIVERY_MODE_ATSC30)
		{
	        if (RHAL_ATSC30_GetBBFrame(tp_id, &(pBBFBuff->bbframe_address), &(pBBFBuff->count)) != TPK_SUCCESS)
	                return -1;
		}

        return 0;
}

/*------------------------------------------------------------------
 * Func : IOCTL_DeliveryReturnFrameBuffer
 *
 * Desc :
 *
 * Retn :
 *------------------------------------------------------------------*/
int IOCTL_Delivery_ReturnFrameBuffer(DELIVERY_DELIVERY_GET_BBF_BUFFER_T *pBBFBuff)
{
        TPK_TP_ENGINE_T tp_id;

        DELIVERY_CHECK_CH(pBBFBuff->ch);

        tp_id = DELIVERY_GET_DELIVERY_ID(pBBFBuff->ch);

		if( deliveryMode == DELIVERY_MODE_ATSC30)
		{
	        if (RHAL_ATSC30_ReturnBBFrame(tp_id, pBBFBuff->bbframe_address) != TPK_SUCCESS)
	                return -1;
		}

        return 0;
}

/*------------------------------------------------------------------
 * Func : IOCTL_DeliveryGetBBFrameBufferInfo
 *
 * Desc :
 *
 * Retn :
 *------------------------------------------------------------------*/
int IOCTL_Delivery_GetBBFrameBufferInfo(DELIVERY_DELIVERY_BBF_BUFFER_INFO_T *pBBFBuffInfo)
{
        TPK_TP_ENGINE_T tp_id;

        DELIVERY_CHECK_CH(pBBFBuffInfo->ch);

        tp_id = DELIVERY_GET_DELIVERY_ID(pBBFBuffInfo->ch);

		if( deliveryMode == DELIVERY_MODE_ATSC30)
		{
	        if (RHAL_ATSC30_GetBBFrameInfo(tp_id, &(pBBFBuffInfo->bbf_buff_phy), &(pBBFBuffInfo->bbf_buff_size)) != TPK_SUCCESS)
	                return -1;
		}

        return 0;
}

/*------------------------------------------------------------------
 * Func : IOCTL_DeliveryGetTimeInfo
 *
 * Desc :
 *
 * Retn :
 *------------------------------------------------------------------*/
int IOCTL_Delivery_GetTimeInfo(DELIVERY_DELIVERY_TIME_INFO_T *pTimeInfo)
{
        TPK_TP_ENGINE_T tp_id;

        DELIVERY_CHECK_CH(pTimeInfo->ch);

        tp_id = DELIVERY_GET_DELIVERY_ID(pTimeInfo->ch);

        if(RHAL_ATSC30_GetTimeInfo(tp_id, &(pTimeInfo->sec), &(pTimeInfo->nsec), &(pTimeInfo->wall_clock) ) != TPK_SUCCESS)
        {
                return -1;
        }

        return 0;
}

/*------------------------------------------------------------------
 * Func : IOCTL_DeliverySetTimeInfo
 *
 * Desc :
 *
 * Retn :
 *------------------------------------------------------------------*/
int IOCTL_Delivery_SetTimeInfo(DELIVERY_DELIVERY_TIME_INFO_T *pTimeInfo)
{
        TPK_TP_ENGINE_T tp_id;

        DELIVERY_CHECK_CH(pTimeInfo->ch);

        tp_id = DELIVERY_GET_DELIVERY_ID(pTimeInfo->ch);

        if (RHAL_ATSC30_SetTimeInfo(tp_id, pTimeInfo->sec, pTimeInfo->nsec, pTimeInfo->wall_clock ) != TPK_SUCCESS)
                return -1;

        return 0;
}

/*------------------------------------------------------------------
 * Func : IOCTL_DeliverySetSystemTimeInfo
 *
 * Desc :
 *
 * Retn :
 *------------------------------------------------------------------*/
int IOCTL_Delivery_SetSystemTimeInfo(DELIVERY_DELIVERY_SYSTEM_TIME_INFO_T *pSystemTimeInfo)
{
        TPK_TP_ENGINE_T tp_id;

        DELIVERY_CHECK_CH(pSystemTimeInfo->ch);

        tp_id = DELIVERY_GET_DELIVERY_ID(pSystemTimeInfo->ch);

        if (RHAL_ATSC30_SetSystemTimeInfo(tp_id, pSystemTimeInfo->current_utc_offset, pSystemTimeInfo->ptp_prepend, pSystemTimeInfo->leap59, pSystemTimeInfo->leap61) != TPK_SUCCESS)
                return -1;

        return 0;
}

/*------------------------------------------------------------------
 * Func : IOCTL_DeliverySetClockRecovery
 *
 * Desc :
 *
 * Retn :
 *------------------------------------------------------------------*/
int IOCTL_Delivery_SetClockRecovery(DELIVERY_DELIVERY_CLOCK_RECOVERY_T *pClockRecovery)
{
        TPK_TP_ENGINE_T tp_id;

        DELIVERY_CHECK_CH(pClockRecovery->ch);

        tp_id = DELIVERY_GET_DELIVERY_ID(pClockRecovery->ch);

        if (RHAL_ATSC30_SetClockRecovery(tp_id, pClockRecovery->isNeedRecovery) != TPK_SUCCESS)
                return -1;

        return 0;
}

/*------------------------------------------------------------------
 * Func : IOCTL_DeliverySetMode
 *
 * Desc :
 *
 * Retn :
 *------------------------------------------------------------------*/
int IOCTL_Delivery_SetMode(DELIVERY_DELIVERY_SET_MODE_T *pMode)
{
		deliveryMode = pMode->mode;

		if ( RHAL_Delivery_SetMode(pMode->mode) != TPK_SUCCESS)
			return -1;

        return 0;
}

/*------------------------------------------------------------------
 * Func : IOCTL_DeliveryRequestData
 *
 * Desc :
 *
 * Retn :
 *------------------------------------------------------------------*/
int IOCTL_Delivery_RequestData(DELIVERY_DELIVERY_CHANNEL_T *channel)
{
        TPK_TP_ENGINE_T tp_id;
#if 0 // USE_RAW_TO_GET_TLV
        TPK_TP_ENGINE_T tp_id_remapping;
#endif

        DELIVERY_CHECK_CH(channel->ch);

        tp_id = DELIVERY_GET_DELIVERY_ID(channel->ch);

        if( deliveryMode == DELIVERY_MODE_JAPAN4K) {
#if 0 // USE_RAW_TO_GET_TLV
                tp_id_remapping = DELIVERY_JAPAN4K_Channel_Mapping(tp_id);

                if(RHAL_TPRawModeEnable(tp_id_remapping, TP_RAW_ENABLE) != TPK_SUCCESS)
                        return -1;

                if(RHAL_EnablePIDFilter(tp_id_remapping, 0) != TPK_SUCCESS)
                        return -1;

                if(RHAL_TPStreamControl(tp_id_remapping, TP_STREAM_START) != TPK_SUCCESS)
                        return -1;
#endif
                if(RHAL_Delivery_RequestData(tp_id) != TPK_SUCCESS)
                        return -1;
        }

#ifdef CONFIG_SUPPORT_SCALER
        fwif_color_set_force_I_De_XC_Disable(1);
#endif
        return 0;
}

/*------------------------------------------------------------------
 * Func : IOCTL_DeliveryCancelData
 *
 * Desc :
 *
 * Retn :
 *------------------------------------------------------------------*/
int IOCTL_Delivery_CancelData(DELIVERY_DELIVERY_CHANNEL_T *channel)
{
        TPK_TP_ENGINE_T tp_id;
        TPK_TP_ENGINE_T tp_id_remapping;

        DELIVERY_CHECK_CH(channel->ch);

        tp_id = DELIVERY_GET_DELIVERY_ID(channel->ch);

		if(deliveryMode == DELIVERY_MODE_JAPAN4K)
		{
			tp_id_remapping = DELIVERY_JAPAN4K_Channel_Mapping(tp_id);

#if 0 // USE_RAW_TO_GET_TLV
	        if (RHAL_TPStreamControl(tp_id_remapping, TP_STREAM_STOP) != TPK_SUCCESS)
	                return -1;
#endif

	        if(RHAL_Delivery_CancelData(tp_id) != TPK_SUCCESS)
	                return -1;
		}

#ifdef CONFIG_SUPPORT_SCALER
        fwif_color_set_force_I_De_XC_Disable(0);
#endif
        return 0;
}

/*------------------------------------------------------------------
 * Func : IOCTL_DeliveryGetData
 *
 * Desc :
 *
 * Retn :
 *------------------------------------------------------------------*/
int IOCTL_Delivery_GetData(DELIVERY_DELIVERY_READ_DATA_T *pData)
{
        TPK_TP_ENGINE_T tp_id;
        TPK_TP_ENGINE_T tp_id_remapping;
		UINT32 data_size;

        DELIVERY_CHECK_CH(pData->ch);

        tp_id = DELIVERY_GET_DELIVERY_ID(pData->ch);

		if(deliveryMode == DELIVERY_MODE_JAPAN4K)
		{
			data_size = pData->data_size;

			tp_id_remapping = DELIVERY_JAPAN4K_Channel_Mapping(tp_id);

			if( RHAL_Delivery_GetData(tp_id, pData->data, &data_size) != TPK_SUCCESS)
				return -1;

			pData->data_size = data_size;
		}
        return 0;
}

/*------------------------------------------------------------------
 * Func : IOCTL_Delivery_GetBootStatus
 *
 * Desc :
 *
 * Retn :
 *------------------------------------------------------------------*/
int IOCTL_Delivery_GetBootStatus(DELIVERY_DELIVERY_GET_BOOT_STATUS_T *pData)
{
        pData->isACON = boot_status_isACOn;
        boot_status_isACOn = 0;
        return 0;
}

/*------------------------------------------------------------------
 * Func : IOCTL_Delivery_SaveBootStatus
 *
 * Desc :
 *
 * Retn :
 *------------------------------------------------------------------*/
int IOCTL_Delivery_SaveBootStatus(void)
{
        UINT32 reg_suslt = rtd_inl(0xB8060100);
        if(reg_suslt == 0x2379beef)
                boot_status_isACOn = 0;
        else
                boot_status_isACOn = 1;
        return 0;
}
#endif

