#ifndef __DMX_FILTER__
#define __DMX_FILTER__
#include <linux/list.h>
#include <linux/kref.h>
#include "dmx_common.h"
#include "sdec.h"

typedef struct multi_filter {
	bool is_enable;
	struct kref kref;
}multi_func_t;
typedef struct multi_filter af_ref_t;

struct dmxfilter {
	struct dmxfilterpriv {
		struct list_head filter_list; //filter list
		struct kref kref;
		bool state_initialized;
		int index;
		void * priv;  //struct rdvb_dmx *
		struct mutex mutex; //for dmx filter op.

		struct ts_parse_context *tpc;
	}priv;

	unsigned short pid;
	DEMUX_PID_TYPE_T type;
	DEMUX_PES_DEST_T dest;

	bool is_scrambleCheck;
	bool is_descrmb_en;
	bool is_pcr;
	bool is_temi;
	bool is_ap; //audio paused;

	af_ref_t af_ref;
	multi_func_t bypass;
	multi_func_t psi;

	bool is_updated;
	void *priv_data;

	union{
		struct dmx_ts_feed *ts;
		struct dmx_temi_feed * temi;
	}feed;
	union {
		dmx_ts_cb ts;
		dmx_temi_cb temi;
	}cb;

	//section
	struct list_head section_list;
};

struct dmxsec {
	struct dmxsecpriv{
		unsigned short idx;
		bool state_initialized;
		void *priv;
	}priv;
	struct list_head next; //filter list
	struct dmxfilter * parent;
	struct dmx_section_filter * dsf;
	int    cb_error;
	dmx_section_cb dsc;

	void * sec_handle;

};


void dmx_filter_release(SDEC_CHANNEL_T* sdec, struct dmxfilter * df);

int dmx_filter_setpAV(SDEC_CHANNEL_T* sdec, struct dmxfilter *df );
int dmx_filter_removeAV(SDEC_CHANNEL_T* sdec, struct dmxfilter *df);
int dmx_filter_setPES(SDEC_CHANNEL_T* sdec, struct dmxfilter *df,
						ts_callback dfc);
int dmx_filter_removePES(SDEC_CHANNEL_T* sdec, struct dmxfilter *df);

int dmx_filter_setPcr(SDEC_CHANNEL_T* sdec, struct dmxfilter *df);
int dmx_filter_removePcr(SDEC_CHANNEL_T* sdec, struct dmxfilter *df);

int dmx_filter_set_ts_callback(SDEC_CHANNEL_T* sdec, struct dmxfilter *df,
								bool enable, ts_callback dfc);

int dmx_filter_enable_scramcheck(SDEC_CHANNEL_T* sdec, struct dmxfilter *df);
int dmx_filter_disable_scramcheck(SDEC_CHANNEL_T* sdec, struct dmxfilter *df);
int dmx_filter_get_scramstatus(SDEC_CHANNEL_T* sdec, struct dmxfilter *df,
								int *status);


int dmx_filter_temi_enable(SDEC_CHANNEL_T* sdec, struct dmxfilter *df,
							u8 timeline_id, ts_callback tc);
int dmx_filter_temi_disable(SDEC_CHANNEL_T* sdec, struct dmxfilter *df);
#endif
