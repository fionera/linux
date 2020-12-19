#ifndef __RTK_SPDIF_H__
#define __RTK_SPDIF_H__

#include <linux/kfifo.h>
#include <rtk_kdriver/rtk_gpio.h>

#define SPDIF_COUNT 5;

typedef struct 
{    
    RTK_GPIO_ID   gid;                  // GPIO that associate with the SPDIF 
    unsigned int  pad_mux_reg;          // pin function selection register
    unsigned int  pad_mux_mask_start_bit;         // pin function selection mask
    unsigned int  pad_mux_val_spdif;    // pin function selection value for spdif
    unsigned int  pad_mux_val_gpio;     // pin function selection value for gpio   
}RTK_SPDIF_Info;

typedef enum{
    SPDIF_ENABLE = 0,
    SPDIF_DISABLE = 1,
}SPDIF_STATUS;

#define SPDIF_INFO(fmt, args...)	rtd_printk(KERN_INFO , "SPDIF" , "[Info]" fmt, ## args)
#define SPDIF_WARN(fmt, args...)	rtd_printk(KERN_WARNING , "SPDIF" , "[Warn]" fmt, ## args)
#define SPDIF_ERROR(fmt, args...)	rtd_printk(KERN_ERR , "SPDIF" , "[Error]" fmt, ## args)


#endif
