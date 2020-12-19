#ifndef __DMX_INBANDCMD_H__
#define __DMX_INBANDCMD_H__
#include "rdvb_dmx_ctrl.h"
#include "dmx_common.h"

int dmx_ib_send_pts(enum pin_type pin_type, int pin_port, void *pInfo, bool is_ecp);
int dmx_ib_send_video_new_seg(enum pin_type pin_type, int pin_port, bool is_ecp);
int dmx_ib_send_video_decode_cmd(enum pin_type pin_type, int pin_port,
												void *pInfo, bool is_ecp);
int dmx_ib_send_video_dtv_src(enum pin_type pin_type, int pin_port, bool is_ecp);
int dmx_ib_send_audio_new_fmt(enum pin_type pin_type, int pin_port,
									u32 format, void *prive, bool is_ecp);
int dmx_ib_send_ad_info(enum pin_type pin_type, int pin_port, void *pInfo, bool is_ecp);
#endif