#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <rtk_kdriver/rtk_otp_util.h>
#include <rtk_kdriver/rtk_otp_region.h>

bool rtk_is_dobly_Vision_supported(void)
{
	bool ret =  false;
    	unsigned int doblyVersion = 0x0;
    	unsigned int remark = 0;
	
    	rtk_otp_region_read(OTP_REGION_REMARK_EN, (unsigned char *)&remark, sizeof(remark));
	
    	if(remark) {
	    if(rtk_otp_region_read(OTP_REGION_DOBLY_VERSION_REMARK, (unsigned char *)&doblyVersion, sizeof(doblyVersion)) == 0) {
	        if(doblyVersion == 0)
	            ret = true;
	    } 
    	} else {
	   if(rtk_otp_region_read(OTP_REGION_DOBLY_VERSION, (unsigned char *)&doblyVersion, sizeof(doblyVersion)) == 0) {
	        if(doblyVersion == 0)
	            ret = true;
	    } 
    	}
    return ret;
}

int rtk_get_hdcp_1_4_bksv_key(unsigned char *buf, unsigned int len)
{
    if(!buf || !len)
        return -1;
    return rtk_otp_region_read(OTP_REGION_HDCP_1_4_BKSV_KEY, buf, len);
}

int rtk_get_hdcp_1_4_private_key(unsigned char *buf, unsigned int len)
{
    if(!buf || !len)
        return -1;
    return rtk_otp_region_read(OTP_REGION_HDCP_1_4_PRIVATE_KEY, buf, len);
}

int rtk_get_hdcp_2_2_key(unsigned char *buf, unsigned int len)
{
    if(!buf || !len)
        return -1;
    return rtk_otp_region_read(OTP_REGION_HDCP_2_2_KEY, buf, len);
}

int rtk_get_hdcp_2_2_bksb_key(unsigned char *buf, unsigned int len)
{
    if(!buf || !len)
        return -1;
    return rtk_otp_region_read(OTP_REGION_HDCP_2_2_BKSB_KEY, buf, len);
}

bool rtk_is_NNIP_function_disable(void)
{
    bool ret = false;
    unsigned char data = 0;
    if(rtk_otp_region_read(OTP_REGION_REMARK_EN, &data, 1) != 0)
        goto EXIT;
    
    if(data) {
        data = 0;
        if(rtk_otp_region_read(OTP_REGION_NNIP_DISABLE_REMK, &data, 1) != 0)
            goto EXIT;
    } else {
        data = 0;
        if(rtk_otp_region_read(OTP_REGION_NNIP_DISABLE_ORI, &data, 1) != 0)
            goto EXIT;
    }
    if(data)
        ret = true;

EXIT:
    return ret; 
}