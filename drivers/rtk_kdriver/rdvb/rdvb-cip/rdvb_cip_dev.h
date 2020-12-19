#ifndef __RDVB_CIP_DEV__
#define __RDVB_CIP_DEV__

#include <linux/types.h>
#include <mach/rtk_log.h>
#include <ca-ext.h>
#include <dvbdev.h>

#ifndef UINT8
typedef unsigned char  UINT8;
#endif
#ifndef UINT16
typedef unsigned short UINT16;
#endif
#ifndef UINT32
typedef unsigned int   UINT32;
#endif

extern UINT8 rdvb_cip_dbg_log_mask;

/***********************************************************
Enable debug log:
echo 1 > /sys/devices/platform/dvb_ca/rtk_cip_log

Enable trace log:
echo 2 > /sys/devices/platform/dvb_ca/rtk_cip_log

Enable both:
echo 3 > /sys/devices/platform/dvb_ca/rtk_cip_log
************************************************************/

#define CIP_LOG_MASK_NONE         	 0
#define CIP_LOG_MASK_BIT_0   	1 << 0
#define CIP_LOG_MASK_BIT_1   	1 << 1

#define rdvb_cip_debug_emerg_log(fmt, args...)        {rtd_printk(KERN_EMERG, "RDVB_CIP", fmt, ## args);}

#define RDVB_CIP_MUST_PRINT(fmt, args...)        {rdvb_cip_debug_emerg_log(fmt, ## args)}

#define RDVB_CIP_LOG_ERROR(fmt, args...)  \
{   \
        rdvb_cip_debug_emerg_log("[ERROR], "fmt, ## args)  \
}


#define RDVB_CIP_LOG_DEBUG(fmt, args...)  \
{   \
        if (rdvb_cip_dbg_log_mask & CIP_LOG_MASK_BIT_0)  \
        {  \
                rdvb_cip_debug_emerg_log("[DEBUG], "fmt, ## args)  \
        }   \
}

#define RDVB_CIP_LOG_TRACE(fmt, args...)  \
{   \
        if (rdvb_cip_dbg_log_mask & CIP_LOG_MASK_BIT_1)  \
        {  \
                rdvb_cip_debug_emerg_log("[TRACE], "fmt, ## args)  \
        }   \
}


enum ciplus_delivery_mode {
	CIPLUS_CICAM_DELIVERY_MODE = 0,
	CIPLUS_HOST_DELIVERY_MODE,
};


/* Private CA-interface information */
struct rdvb_cip_private {

	/* pointer back to the public data structure */
	struct rdvb_cip_tpp *tpp;

	/* the DVB device */
	struct dvb_device *dvbdev;

	/* Flags describing the interface (DVB_CA_FLAG_*) */
	u32 flags;
	
	/* tpp index */
	int index;

	/* tpp src  */
	enum ca_src_type   	input_src_type;
	unsigned int      	input_port_num;
	//enum ca_ext_port_type  	input_port_type; 

	enum ciplus_delivery_mode delivery_mode;
	
	unsigned int     	ts_mode;
	
	unsigned int     	bypass_mode;
	unsigned char*   	bypass_buf;
	unsigned int     	bypass_buf_size;

	/* wait queues for read() and write() operations */
	wait_queue_head_t wait_queue;

	/* PID of the monitoring thread */
	struct task_struct *thread;

	/* Flag indicating if the CA device is open */
	unsigned int open;

	/* Flag indicating the thread should wake up now */
	unsigned int wakeup;

	/* Delay the main thread should use */
	unsigned long delay;

	/* mutex serializing ioctls */
	struct mutex mutex;
};


struct rdvb_cip_tpp {
	struct module *owner;
	
	int (*init)(struct rdvb_cip_tpp *rdvb_tpp);
	int (*unInit)(struct rdvb_cip_tpp *rdvb_tpp);
	
	int (*write)(struct rdvb_cip_tpp *rdvb_tpp,const char __user *buf,size_t count);
	int (*read)(struct rdvb_cip_tpp *rdvb_tpp,const char __user *buf,size_t count);

	int (*setConfig)(struct rdvb_cip_tpp *rdvb_tpp, struct ca_ext_source source);
	int (*getFilterNum)(struct rdvb_cip_tpp *rdvb_tpp);
	
	int (*addFilter)(struct rdvb_cip_tpp *rdvb_tpp, UINT16 PID);
	int (*removeFilter)(struct rdvb_cip_tpp *rdvb_tpp, UINT16 PID);
	
	int (*sendingEnable)(struct rdvb_cip_tpp *rdvb_tpp, UINT8 enabel);
	int (*ReceivingEnable)(struct rdvb_cip_tpp *rdvb_tpp, UINT8 enabel);

	int (*getCamBitRate)(struct rdvb_cip_tpp *rdvb_tpp, UINT32 *inBitRate, UINT32 *outBitRate);

	int id;

	void *data;

	void *private;
};


extern int rdvb_cip_dev_init(struct dvb_adapter *dvb_adapter,struct rdvb_cip_tpp *tpp);

extern void rdvb_cip_dev_release(struct rdvb_cip_tpp *tpp);

#endif
