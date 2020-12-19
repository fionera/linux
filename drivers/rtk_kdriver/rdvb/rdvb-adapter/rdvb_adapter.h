#ifndef __RDVB_ADAPTER_H__
#define __RDVB_ADAPTER_H__

#include <dvbdev.h>
#include <rdvb-dmx/rdvb_dmx.h>
struct rdvb_adapter{
	struct dvb_adapter *dvb_adapter;
	bool is_master;

	struct rdvb_dmxdev rdvb_dmxdev;
	struct proc_dir_entry * proc_dbg_entry;
};
extern struct proc_dir_entry* rdvb_adap_register_proc(const char * name);
extern struct rdvb_adapter* rdvb_get_adapter(void);
#endif
