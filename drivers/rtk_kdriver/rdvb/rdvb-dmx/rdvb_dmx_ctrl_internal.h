#ifndef __RDVB_DMX_CTRL_INTERNAL__
#define __RDVB_DMX_CTRL_INTERNAL__
#include "sdec.h"
#include "demux.h"

int rdvb_dmx_ctrl_config(SDEC_CHANNEL_T *sdec, enum dmx_src_type input_src_type, 
	unsigned int input_port_num, enum dmx_ext_port_type input_port_type);
int rdvb_dmx_ctrl_get_stc(SDEC_CHANNEL_T *sdec, u64 *stc, unsigned int *base);
int rdvb_dmx_ClockRecovery(SDEC_CHANNEL_T *pCh, bool bOnOff);

int rdvb_dmx_ctrl_pvr_open(SDEC_CHANNEL_T *pCh,dvr_rec_cb dvr_rec_cb, void *source);
int rdvb_dmx_ctrl_pvr_close(struct dmx_demux *demux);
int rdvb_dmx_ctrl_pvr_write(SDEC_CHANNEL_T *sdec, const char __user *buf, size_t count);
int rdvb_dmx_ctrl_pvr_SetRate(SDEC_CHANNEL_T *sdec, int speed);
int rdvb_dmx_ctrl_pvr_reset(SDEC_CHANNEL_T *sdec);
int rdvb_dmx_ctrl_pvr_auth_verify(SDEC_CHANNEL_T *sdec, char *buf);

char * rdvb_dmx_ctrl_dump_tp_status (SDEC_CHANNEL_T *sdec, char *buf);
char * rdvb_dmx_ctrl_dump_pcr_status (SDEC_CHANNEL_T *sdec, char *buf);

void rdvb_dmx_ctrl_ecp_on(void);
void rdvb_dmx_ctrl_ecp_off(void);
#endif