#ifndef _RTK_EMCU_EXPORT_H_
#define _RTK_EMCU_EXPORT_H_

#if defined(CONFIG_REALTEK_INT_MICOM)
#include <linux/i2c.h>
#include "rtk_gpio.h"
#include "rtk_gpio-dev.h"
#endif

#define SUSPEND_BOOTCODE    0
#define SUSPEND_NORMAL      1
#define SUSPEND_RAM         2
#define SUSPEND_WAKEUP      3


typedef enum {
    WKSOR_UNDEF,    // 0: undefine
    WKSOR_KEYPAD,   // 1: wakeup via keypad
    WKSOR_WUT,      // 2: wakeup via timer
    WKSOR_IRDA,     // 3: wakeup via remote control
    WKSOR_CEC,      // 4: wakeup via CEC
    WKSOR_PPS,      // 5: wakeup via VGA
    WKSOR_WOW,      // 6: wakeup via WOW
    WKSOR_MHL,      // 7: wakeup via HML
    WKSOR_RTC,      // 8: wakeup via RTC
    WKSOR_WOV,      // 9: wakeup via voice
    WKSOR_END,      //end
} WAKE_UP_T;

int powerMgr_get_wakeup_source(unsigned int* row, unsigned int* status);

#if defined(CONFIG_REALTEK_INT_MICOM)
typedef unsigned char UINT8;

#if defined( CONFIG_CUSTOMER_TV006 )

#define LG_MICOM_ADDRESS (0x29)
#define LG_MICOM_I2C_CHANNEL (0x00)	
#define LG_NVRAM_ADDRESS (0x50)
#define LG_NVRAM_I2C_CHANNEL (0x05)

//#define CPU_INT2 (0xB801A10C)
//#define CPU_INT_EN2 (0xB801A110)
#define INT_MCU_TO_SCPU (_BIT2)
#define INT_SCPU_TO_MCU (_BIT1)

int do_intMicomShareMemory(UINT8 *pBuf,UINT8 Len, bool RorW);
void shareMemoryScpuInfo8051(void);
#endif


#endif
#endif



