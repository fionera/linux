#ifndef __RDVB_DMX_CTRL__
#define __RDVB_DMX_CTRL__

struct dmx_buff_info_t{
    u32 bsPhyAddr ;                /* the address of BitStream RBH*/
    u32 ibPhyAddr ;                /* the address of InBand    RBH*/
    u32 refClockPhyAddr ;
    u32 bsHeaderSize;
    u32 ibHeaderSize;
    u32 refClockHeaderSize;

};
enum dmx_priv_cmd {
	DMX_PRIV_CMD_NOTIFY_FLUSHED,
	DMX_PRIV_CMD_VIDEO_DECODE_MODE,
	DMX_PRIV_CMD_VIDEO_FREERUN_THRESHOLD,
	DMX_PRIV_CMD_AUDIO_FORMAT,
	DMX_PRIV_CMD_AUDIO_PAUSE,
	DMX_PRIV_CMD_AUDIO_UNPAUSE,
	DMX_PRIV_CMD_VIDEO_FLUSH_BEGIN,
	DMX_PRIV_CMD_MAX,

};

enum pin_type {
	PIN_TYPE_VTP,
	PIN_TYPE_ATP,
	PIN_TYPE_PES,
	PIN_TYPE_MAX,
};

enum host_type {
	HOST_TYPE_SDEC,
	HOST_TYPE_VDEC,
	HOST_TYPE_ADEC,
	HOST_TYPE_MAX,
};

enum 
{
	VTP_PORT_0 = 0,
	VTP_PORT_1 = 1,
	VTP_PORT_MAX =2,

	ATP_PORT_0 = 0,
	ATP_PORT_1 = 1,
	ATP_PORT_MAX=2,
};

struct dmx_priv_data {
	union {
		u32 video_decode_mode;
		u32 video_freerun_threshlod;
		struct {
			u32 audio_format;
			u32 private_data[8];
		}audio_format;
	}data;
};

int rdvb_dmx_ctrl_privateInfo(enum dmx_priv_cmd dmx_priv_cmd,
	enum pin_type pin_type, int pin_port, struct dmx_priv_data* priv_data);
int rdvb_dmx_ctrl_getBufferInfo(enum pin_type pin_type, int pin_port,
	enum host_type host, int host_port, struct dmx_buff_info_t *dmx_buff_info);
int rdvb_dmx_ctrl_releaseBuffer(enum pin_type pin_type,
	int pin_port, enum host_type host, int host_port);
int rdvb_dmx_ctrl_currentSrc_isMTP(enum pin_type pin_type, int pin_port,
	uint8_t *retValue);

int rdvb_dmx_ctrl_ci_filter(unsigned char id, unsigned short pid , void **filter);
#endif