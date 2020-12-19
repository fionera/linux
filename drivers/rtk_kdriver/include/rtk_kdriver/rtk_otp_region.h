#ifndef __RTK_OTP_REGION_H__
#define __RTK_OTP_REGION_H__

typedef enum {
    OTP_REGION_DOBLY_VERSION = 0x1,
    OTP_REGION_DOBLY_VERSION_REMARK = 0x2,
    OTP_REGION_REMARK_EN = 0x3,
    OTP_REGION_HDCP_1_4_BKSV_KEY = 0x4,
    OTP_REGION_HDCP_1_4_PRIVATE_KEY = 0x5,
    OTP_REGION_HDCP_2_2_KEY = 0x6,
    OTP_REGION_HDCP_2_2_BKSB_KEY = 0x7,
    OTP_REGION_NNIP_DISABLE_ORI = 0x8,
    OTP_REGION_NNIP_DISABLE_REMK = 0x9,

}OTP_REGION_ID;

extern int rtk_otp_region_read(OTP_REGION_ID region_id, unsigned char *data, unsigned int len);

#endif
