#ifndef __RDVB_DMX_FILTERMGR__
#define __RDVB_DMX_FILTERMGR__
#include <demux.h>
#include "dmx_filter.h"
#include "rdvb_dmx.h"



struct dmxfilter *
rdvbdmx_filter_add (struct rdvb_dmx* rdvb_dmx, struct dvb_demux_feed *feed);
struct dmxfilter *
rdvbdmx_filter_bypass_add (struct rdvb_dmx* rdvb_dmx, unsigned short pid);
int rdvbdmx_filter_del(struct dmxfilter * df, struct dvb_demux_feed *feed);
int rdvbdmx_filter_bypass_del(struct dmxfilter * df);
int rdvbdmx_filter_start_scrambcheck(struct dmxfilter *df);
int rdvbdmx_filter_stop_scrambcheck(struct dmxfilter *df);
int rdvbdmx_filter_get_scramstatus(struct dmxfilter * df, int * scramble_status);
int rdvbdmx_filter_descrmb_ctrl(struct dmxfilter * df, u8 is_enable);

int rdvbdmx_filter_temi_enable(struct dmxfilter *df, struct dvb_demux_feed *feed);
int rdvbdmx_filter_temi_disable(struct dmxfilter *df);

void rdvbdmx_filter_audio_pause_on(struct list_head *filters);
void rdvbdmx_filter_audio_pause_off(struct list_head *filters);

struct dmxsec * rdvbdmx_filter_section_add
	(struct dmxfilter *df, struct dmx_section_filter * dsf, dmx_section_cb dsc);
int rdvbdmx_filter_section_del(struct dmxsec * ds);

char * rdvbdmx_filter_dump(struct list_head *filters, char *buf);

char * rdvbdmx_filter_dump_pes(struct list_head *filters, char *buf);
char * rdvbdmx_filter_dump_sec(struct list_head *filters, char *buf);
char * rdvbdmx_filter_dump_scrmbck(struct list_head *filters, char *buf);

int rdvbdmx_filter_reset(struct list_head *df);
#endif