#include <linux/types.h>
#include <linux/string.h>
#include <AudioInbandAPI.h>
#include <InbandAPI.h>
#include <rdvb-buf/rdvb_dmx_buf.h>

int dmx_ib_send_pts(enum pin_type pin_type, int pin_port, void *pInfo, bool is_ecp)
{
	int64_t * pts = (int64_t *) pInfo;
	PTS_INFO cmd;
	cmd.header.type = INBAND_CMD_TYPE_PTS;
	cmd.header.size = sizeof(PTS_INFO);
	cmd.PTSH = (*pts) >> 32;
	cmd.PTSL = *pts;

	return rdvb_dmxbuf_writeIB(pin_type, pin_port, (void*)&cmd,
										sizeof(PTS_INFO), &cmd.wPtr, is_ecp);
}

int dmx_ib_send_video_new_seg(enum pin_type pin_type, int pin_port, bool is_ecp)

{
	
	NEW_SEG cmd;
	int ret = 0;

	cmd.header.type = INBAND_CMD_TYPE_NEW_SEG;
	cmd.header.size = sizeof(NEW_SEG);
	ret = rdvb_dmxbuf_writeIB(pin_type, pin_port, (void*)&cmd,
					sizeof(NEW_SEG), &cmd.wPtr, is_ecp);
	dmx_info(NOCH, "NEW_SEG: ret:%d, wp:0x%08x\n", ret, cmd.wPtr);

	return ret;
}

int dmx_ib_send_video_decode_cmd(enum pin_type pin_type, int pin_port,
												void *pInfo, bool is_ecp)

{
	DECODE cmd;
	int64_t duration = -1;
	cmd.header.type  = INBAND_CMD_TYPE_DECODE;
	cmd.header.size  = sizeof(DECODE);
	cmd.RelativePTSH = 0;
	cmd.RelativePTSL = 0;
	cmd.PTSDurationH = duration >> 32;
	cmd.PTSDurationL = duration;
	cmd.skip_GOP     = 0;
	cmd.mode         = *((DECODE_MODE*)pInfo);

	return rdvb_dmxbuf_writeIB(pin_type, pin_port, (void *)&cmd, sizeof(DECODE),NULL, is_ecp);
}


int dmx_ib_send_video_dtv_src(enum pin_type pin_type, int pin_port, bool is_ecp)

{
		INBAND_SOURCE_DTV cmd;
		cmd.header.type = VIDEO_INBAND_CMD_TYPE_SOURCE_DTV;
		cmd.header.size = sizeof(INBAND_SOURCE_DTV);

		return rdvb_dmxbuf_writeIB(pin_type, pin_port, (void *)&cmd,
									sizeof(INBAND_SOURCE_DTV), &cmd.wPtr, is_ecp);
}

int dmx_ib_send_audio_new_fmt(enum pin_type pin_type, int pin_port,
									u32 format, void *priv, bool is_ecp)

{
	AUDIO_DEC_NEW_FORMAT cmd;
	cmd.header.type = AUDIO_DEC_INBAMD_CMD_TYPE_NEW_FORMAT;
	cmd.header.size = sizeof(AUDIO_DEC_NEW_FORMAT);
	cmd.audioType   = format;
	cmd.wPtr        = 0;
	memcpy(cmd.privateInfo, priv, sizeof (cmd.privateInfo));
	return rdvb_dmxbuf_writeIB(pin_type, pin_port, (void *)&cmd,
						sizeof(AUDIO_DEC_NEW_FORMAT), (unsigned int *)&cmd.wPtr, is_ecp);

}

int dmx_ib_send_ad_info(enum pin_type pin_type, int pin_port, void *pInfo, bool is_ecp)
{
	AUDIO_DEC_AD_DESCRIPTOR cmd;
	memset(&cmd,0, sizeof(AUDIO_DEC_AD_DESCRIPTOR));
	cmd.header.type =  AUDIO_DEC_INBAND_CMD_TYPE_AD_DESCRIPTOR;
	cmd.header.size = sizeof(AUDIO_DEC_AD_DESCRIPTOR);
	cmd. PTSH = 0;
	cmd. PTSL = 0;
	//adinfo
	cmd.AD_fade_byte       = (((AUDIO_AD_INFO *)pInfo)->AD_fade_byte);
	cmd.AD_pan_byte        = (((AUDIO_AD_INFO *)pInfo)->AD_pan_byte);
	cmd.gain_byte_center   = (((AUDIO_AD_INFO *)pInfo)->AD_gain_byte_center);
	cmd.gain_byte_front    = (((AUDIO_AD_INFO *)pInfo)->AD_gain_byte_front);
	cmd.gain_byte_surround = (((AUDIO_AD_INFO *)pInfo)->AD_gain_byte_surround);
	return rdvb_dmxbuf_writeIB(pin_type, pin_port, (void *)&cmd,
		sizeof(AUDIO_DEC_AD_DESCRIPTOR), (unsigned int *)&cmd.wPtr, is_ecp);
}