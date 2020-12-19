#ifndef __RDVB_ACAS__
#define __RDVB_ACAS__

#include <linux/types.h>
#include <acas-ext.h>
#include <adapter/rtk_scd_priv.h>


#define ACAS_APDU_RESP_MAX_LEN		512

typedef enum {
    SC_PROTOCOL_T0 = 0,
    SC_PROTOCOL_T1 = 1,
    SC_PROTOCOL_T14 = 14,    
    SC_PROTOCOL_MAX,
}SC_PROTOCOL;


/* Private acas-interface information */
struct rdvb_acas_priv {

	/* pointer back to the public data structure */
	struct module *owner;

	/* the DVB device */
	struct dvb_device *dvbdev;
	
	/* reset time */
	int reset_time;

	/* Flags describing the interface (DVB_CA_FLAG_*) */
	u32 flags;

	/* Flag indicating if the CA plus device is open */
	unsigned int open;

	/* Flag indicating the thread should wake up now */
	unsigned int wakeup:1;

	/* Delay the main thread should use */
	unsigned long delay;


	/* mutex serializing ioctls */
	struct mutex ioctl_mutex;

	/* smartcard */
	scd *pscd;
};


#endif
