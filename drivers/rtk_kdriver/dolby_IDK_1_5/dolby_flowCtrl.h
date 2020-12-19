#ifndef _DOLBY_FLOWCTRL_
#define _DOLBY_FLOWCTRL_
#ifndef WIN32
#include <rbus/misc_reg.h>

#include "dolby_flowCtrl.h"

//#define IDK_CRF_USE	1
//#define DOLBLY_12B_MODE	1
//#define DOLBYVISION_IDK_TRIGGER_PORT	0xb800501c
//#define NBC_12B420_WORKAROUND	1
//#define NBC_12B420_WORKAROUND_2		1
#ifndef RTK_EDBEC_1_4_3
#define NBC_12B420_WORKAROUND_3 	1		// suggest by Sudheesh BaBu (IDK final use)
#endif

#define SUPPORT_HDR10_TO_MD 1
#define SUPPORT_HDR10_TO_MD_SIZE 128

//#define RPU_METADATA_OUTPUT

#define RPU_MD_SIZE	0x100000


#define DV_CMD_QUEUE_SIZE			32

#define DV_SUCCESS				   0
#define DV_ERR_NO_MEMORY		  -1
#define DV_ERR_RUN_FAIL		  -2


// Swap 2 byte, 16 bit values:
#define Swap2Bytes(val) \
 ( (((val) >> 8) & 0x00FF) | (((val) << 8) & 0xFF00) )

// Swap 4 byte, 32 bit values:
 #define Swap4Bytes(val) \
 ( (((val) >> 24) & 0x000000FF) | (((val) >>  8) & 0x0000FF00) | \
   (((val) <<  8) & 0x00FF0000) | (((val) << 24) & 0xFF000000) )

// Swap 8 byte, 64 bit values:
#define Swap8Bytes(val) \
 ( (((val) >> 56) & 0x00000000000000FF) | (((val) >> 40) & 0x000000000000FF00) | \
   (((val) >> 24) & 0x0000000000FF0000) | (((val) >>  8) & 0x00000000FF000000) | \
   (((val) <<  8) & 0x000000FF00000000) | (((val) << 24) & 0x0000FF0000000000) | \
   (((val) << 40) & 0x00FF000000000000) | (((val) << 56) & 0xFF00000000000000) )





typedef struct
{
	unsigned int base;
	unsigned int limit;
	unsigned long *pWrPtr;
	unsigned long *pRdPtr;
	unsigned int write;
	unsigned int read;
} VP_INBAND_RING_BUF;

typedef enum
{
	DV_FLOW_STOP = 0,
	DV_FLOW_RUN,
	DV_FLOW_PAUSE,
	DV_FLOW_INIT
} DV_FLOW_STATE;

/* dolbyvision meta-parser output control */
typedef struct _DV_FLOW_ {

	DV_FLOW_STATE state;
	DV_FLOW_STATE state_chk;

	/* InBand & MD output command queue */
	VP_INBAND_RING_BUF INBAND_ICQ;

	/* MD Output pointers */
	//unsigned int *pBsRead;
	unsigned int *pBsWrite;
	//unsigned int bsRead;
	unsigned int bsWrite;
	unsigned int bsBase;
	unsigned int bsLimit;
	unsigned int bsUnitSize;
	unsigned int bsSize;
} DOLBYVISION_FLOW_STRUCT;


//variable
extern struct semaphore g_dv_sem;
extern struct semaphore g_dv_pq_sem;

// proto-type
unsigned int DV_SearchAvailOutBuffer(void);
int DV_RingBuffer_Init(DOLBYVISION_INIT_STRUCT *data);
int DV_HDMI_Init(void);
int DV_Run(void);
int DV_Stop(void);
int DV_Pause(void);
int DV_Flush(void);
int DV_MD_ThreadProcess(void *qq);
void DV_MD_Output_Gen(unsigned char *mdAddr, unsigned char *dm_out, unsigned char *comp_out);
void DV_HDR10_MD_Output_Gen(unsigned char *mdAddr, unsigned char *dm_out, unsigned char *comp_out);
unsigned int DV_pickUp_MD(unsigned int ptsh, unsigned int ptsl);
unsigned int DV_MD_DumpOutput_Info(int print);
void Check_DV_Mode(void);
void Set_OTT_Force_update(unsigned char enable);
unsigned char Get_OTT_Force_update(void);
void el_not_support_handler(void);//when detect FEL type notice omx

/*For scaler usage,
 we provide API to anyone who have to control dolby or DM related Registers.
 */
void DV_disable_DM(void);
void set_dolby_init_flag(unsigned char enable);
unsigned char get_dolby_init_flag(void);

#endif


#endif /* end of _DOLBY_FLOWCTRL_  */
