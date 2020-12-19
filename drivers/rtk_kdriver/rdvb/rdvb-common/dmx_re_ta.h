#ifndef __DMX_RE_TA_H__
#define __DMX_RE_TA_H__

#include "dmx_ta_def.h"

int dmx_ta_init(void);

int dmx_ta_dmx_init(unsigned char ch, unsigned char pksize, struct ta_tpc_info *tpcs) ;
unsigned int dmx_ta_buf_map(unsigned char ch, unsigned char type,
							unsigned int bs_pa, unsigned int bs_size,
							unsigned int bs_hdr_pa, unsigned int bs_hdr_size,
							unsigned int ib_pa, unsigned int ib_size,
							unsigned int ib_hdr_pa, unsigned int ib_hdr_size);
void dmx_ta_buf_unmap(u8 idx, u8 type);
void dmx_ta_tpc_create(u8 idx, unsigned short pid, unsigned char type, unsigned char port);
void dmx_ta_tpc_release(u8 idx, unsigned short pid);
int dmx_ta_ib_write(u8 idx, u8 type, void * data, u32 size);
unsigned int dmx_ta_data_process(unsigned char ch, unsigned int cur,
			unsigned int end, int64_t* vpts, int64_t *apts, uint16_t *pes_pid0, uint16_t *pes_pid1);

int dmx_ta_pvr_auth_verify(u8 key[32]);
#endif