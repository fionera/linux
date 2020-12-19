#ifndef MISC_EXPORT_H
#define MISC_EXPORT_H

#include <linux/types.h>

struct misc_dev {
    struct semaphore sem;     /* mutual exclusion semaphore     */
    struct cdev cdev;   /* Char device structure          */
};

/*
 * Ioctl definitions
 */
#if defined(CONFIG_REALTEK_INT_MICOM)
typedef unsigned char UINT8;
typedef unsigned short UINT16;
typedef unsigned int UINT32;
#endif

typedef struct
{
    unsigned int  totalBytes_chA;
    unsigned int  percent_chA;                //ratio for how much bandwidth of the channel A is used.
    unsigned int  totalBytes_chB;
    unsigned int  percent_chB;                //ratio for how much bandwidth of the channel B is used.
} DRAM_BW_INFO_T;

/* SPREAD SPECTRUM ENUMERATIONS HAVE TO BE SAME AS KAPI */
typedef enum
{
	SPREAD_SPECTRUM_MODULE_CPU = 0,		// CPU PLL
	SPREAD_SPECTRUM_MODULE_MAIN_0,		// Main PLL 0
	SPREAD_SPECTRUM_MODULE_MAIN_1,		// Main PLL 1
	SPREAD_SPECTRUM_MODULE_MAIN_2,		// Main PLL 2
	SPREAD_SPECTRUM_MODULE_DISPLAY,		// Display PLL
} SPREAD_SPRECTRUM_MODULE_T;

typedef enum
{
	SPREAD_SPECTRUM_OFF = 0,
	SPREAD_SPECTRUM_RATIO_0_25,		// 0.25%
	SPREAD_SPECTRUM_RATIO_0_50,		// 0.50%
	SPREAD_SPECTRUM_RATIO_0_75,		// 0.75%
	SPREAD_SPECTRUM_RATIO_1_00,		// 1.00%
	SPREAD_SPECTRUM_RATIO_1_25,		// 1.25%
	SPREAD_SPECTRUM_RATIO_1_50,		// 1.50%
	SPREAD_SPECTRUM_RATIO_1_75,
} SPREAD_SPECTRUM_RATIO_T;


typedef struct
{
   SPREAD_SPRECTRUM_MODULE_T module;
   SPREAD_SPECTRUM_RATIO_T ratio;
} SPREAD_SPECTRUM_PARA;

/* module test get pin*/
typedef struct
{
	unsigned int pin_signal;
	unsigned int pin_scl;
	unsigned int pin_sda;
	unsigned int pin_status0;
	unsigned int pin_status1;
} MODULE_TEST_PIN_T;

#if defined(CONFIG_REALTEK_INT_MICOM)

typedef struct
{
    UINT8   CmdSize;
    UINT32 pCmdBuf;
    UINT8   DataSize;
    UINT32 pDataBuf;
    UINT8   retryCnt;
} IPC_ARG_T;

typedef struct
{
    UINT8   Cmd;
    UINT16  DataSize;
    UINT32  pDataBuf;
} UCOM_ARG_T;

enum _SHARE_MEMORY_OPERATION{
    SH_MEM_OP_WRITE	= 0,
    SH_MEM_OP_READ	= 1,
    SH_MEM_OP_UNKNOW ,
};

enum _SHARE_MEMORY_RETURN_VALUE{
    SH_MEM_OK	= 0,
    SH_MEM_DATALEN_OVERFLOW,
    SH_MEM_UNKNOWN_CMD,
    SH_MEM_NO_RESPONSE,
};
#endif

/* Use 's' as magic number */
#define MISC_IOC_MAGIC  's'

#define MISC_WAKEUP_ECPU    		_IO(MISC_IOC_MAGIC, 0)
#define MISC_SET_WOL_ENABLE    		_IOW(MISC_IOC_MAGIC, 1,int)
#define MISC_GET_WOL_ENABLE    		_IOR(MISC_IOC_MAGIC, 2,UINT32)
#define MISC_SET_DEBUG_LOCK    		_IOW(MISC_IOC_MAGIC, 3,int)
#define MISC_SET_DRAM_BW_ENABLE    	_IOW(MISC_IOC_MAGIC, 4,int)
#define MISC_GET_DRAM_BW_INFO    	_IOR(MISC_IOC_MAGIC, 5,UINT32)
#define MISC_SET_SPREAD_SPECTRUM    _IOW(MISC_IOC_MAGIC, 6,UINT32)

#define MISC_SET_RTDLOG          _IOW(MISC_IOC_MAGIC, 7, unsigned int)
#define MISC_GET_RTDLOG          _IOR(MISC_IOC_MAGIC, 8, UINT32)
//#define MISC_SET_RTDLOG_MODULE          _IOW(MISC_IOC_MAGIC, 9, unsigned int)
//#define MISC_GET_RTDLOG_MODULE          _IOR(MISC_IOC_MAGIC, 10, unsigned int*)
#define MISC_SET_WARM_WOL_ENABLE    		_IOW(MISC_IOC_MAGIC, 11,int)
#define MISC_GET_WARM_WOL_ENABLE    		_IOR(MISC_IOC_MAGIC, 12,UINT32)
#define MISC_QSM_OFF                        _IO(MISC_IOC_MAGIC, 13)
#ifdef CONFIG_RTK_KDRV_WATCHDOG
#define MISC_SET_WATCHDOG_ENABLE  _IOW(MISC_IOC_MAGIC, 14, int)
#endif
#define MISC_GET_MODULE_TEST_PIN          _IOR(MISC_IOC_MAGIC, 15, UINT32)
#endif
#if defined(CONFIG_REALTEK_INT_MICOM)
#define MISC_IPC_WRITE    		_IOW(MISC_IOC_MAGIC, 16,UINT32)
#define MISC_IPC_READ    		_IOR(MISC_IOC_MAGIC, 17,UINT32)
#define MISC_UCOM_WRITE    		_IOW(MISC_IOC_MAGIC, 18,UINT32)
#define MISC_UCOM_READ    		_IOR(MISC_IOC_MAGIC, 19,UINT32)
#define MISC_UCOM_WHOLE_CHIP_RESET  _IOW(MISC_IOC_MAGIC, 20,UINT32)
#define MISC_VOICE_UPDATE_SENSORY_MODEL  _IOW(MISC_IOC_MAGIC, 21,UINT32)

#define MICOM_LOG_REG 0xb8060524
#define MICOM_LOG_LEN 76

extern int do_intMicomShareMemory(UINT8 *pBuf,UINT8 Len, bool RorW);
void misc_get_micom_log(void);
#endif

#define MISC_GET_STR_ENABLE    		_IOR(MISC_IOC_MAGIC, 21,int)


