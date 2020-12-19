#ifndef __RDVB_DMX__
#define __RDVB_DMX__
#include <linux/spinlock.h>
#include "dmxdev.h"
#include "descrambler.h"
#define DMX_VERSION_ID 0X0001
#define DMX_MODEL_ID   0X0005
#define DMX_CHIP_ID    400

#define DMX_MAX_FILTER_NUM 32
#define DMX_MAX_SECTION_FILTER_NUM 128
#define DMX_MAX_CHANNEL_NUM 4

struct rdvb_dmx{
	struct dmxdev dmxdev;
	struct rdvb_dmxdev * rdvbdmxdev;
	int index;

	int filter_num;
	struct mutex     mutex;
	spinlock_t lock;                  //protect pes filter state_initialized
	struct list_head filter_list;
	struct dmxfilter *dmx_filters;

	bool is_audio_paused;
	// for uplayers
	enum dmx_platform dmx_platform;
	enum dmx_country  dmx_country;
	unsigned int      dmx_chip_id;
	unsigned int      dmx_model_id;
	unsigned int      dmx_ver_id;

	enum dmx_src_type      input_src_type;
	unsigned int           input_port_num;
	enum dmx_ext_port_type input_port_type;

	bool pcr_recov_on;
	struct descrambler dscmb;
	
	struct dmx_frontend	fe_hw;
	struct dmx_frontend	fe_mem;
};

struct rdvb_dmxdev {
	int dmxnum;
	struct rdvb_dmx *rdvb_dmx;
	struct mutex mutex; //protect channel config

	unsigned int sdecnum;
	struct dmxsec *dmxsecs;
	spinlock_t lock; //protect section filter state_initialized
};
#endif