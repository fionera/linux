#ifndef __RDVB_CA__
#define __RDVB_CA__
#include <dvb_ca_en50221.h>
#include <mach/rtk_log.h>
#include <ca-ext.h>

#include "rtk_pcmcia.h"
#include "rdvb_ca_parse.h"


#define CA_ENABLE  1
#define CA_DISABLE 0

#define LOG_MASK_NONE         	0
#define ATTR_LOG_MASK_BIT_0   	1 << 0
#define IO_LOG_MASK_BIT_1   	1 << 1
#define DEBUG_LOG_MASK_BIT_2  	1 << 2

typedef struct ca {
    struct rdvb_adapter* rdvb_adapter;
    struct dvb_ca_en50221 pubca;
    CI_CAM_INFO_T   ci_camInfo;
    struct ca_ext_source ca_src;
    RTK_PCMCIA *rtkpcmcia[2];
    int laster_status;
    void (*rdvb_ca_camchange_irq)(struct dvb_ca_en50221 *pubca, int slot, int change_type);
    void (*rdvb_ca_camready_irq)(struct dvb_ca_en50221 *pubca, int slot);
    void (*rdvb_ca_frda_irq)(struct dvb_ca_en50221 *pubca, int slot);
}RDVB_CA;



extern UINT8 rdvb_ca_dbg_log_mask;
extern RDVB_CA* rdvb_get_ca(void);


/***********************************************************
Enable attr log:
echo 1 > /sys/devices/platform/dvb_ca/rtk_ca_log

Enable io log:
echo 2 > /sys/devices/platform/dvb_ca/rtk_ca_log

Enable both:
echo 3 > /sys/devices/platform/dvb_ca/rtk_ca_log

************************************************************/

#define rdvb_ca_debug_emerg_log(fmt, args...)        {rtd_printk(KERN_EMERG, "RDVB_CA", fmt, ## args);}

#define RDVB_CA_MUST_PRINT(fmt, args...)        {rdvb_ca_debug_emerg_log(fmt, ## args)}

#define RDVB_CA_ERROR(fmt, args...)  \
{   \
        rdvb_ca_debug_emerg_log("ERROR, "fmt, ## args)  \
}

#define RDVB_CA_ATTR_DEBUG(fmt, args...)  \
{   \
        if (rdvb_ca_dbg_log_mask & ATTR_LOG_MASK_BIT_0)  \
        {  \
                rdvb_ca_debug_emerg_log("[ATTR DEBUG], "fmt, ## args)  \
        }   \
}

#define RDVB_CA_IO_DEBUG(fmt, args...)  \
{   \
        if (rdvb_ca_dbg_log_mask & IO_LOG_MASK_BIT_1)  \
        {  \
                rdvb_ca_debug_emerg_log("[IO DEBUG], "fmt, ## args)  \
        }   \
}

#define RDVB_CA_NORMAL_DEBUG(fmt, args...)  \
{   \
        if (rdvb_ca_dbg_log_mask & DEBUG_LOG_MASK_BIT_2)  \
        {  \
                rdvb_ca_debug_emerg_log("[DEBUG], "fmt, ## args)  \
        }   \
}


#endif
