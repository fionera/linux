#ifndef _RTK_EMCU_H_
#define _RTK_EMCU_H_

#if defined(CONFIG_REALTEK_INT_MICOM)
#include <mach/rtk_log.h>
#endif

//------------------------------------------
// Definitions of Bits
//------------------------------------------
#define _ZERO   						(0x00)
#define _BIT0   						(0x01UL)
#define _BIT1   						(0x02UL)
#define _BIT2   						(0x04UL)
#define _BIT3   						(0x08UL)
#define _BIT4   						(0x10UL)
#define _BIT5   						(0x20UL)
#define _BIT6   						(0x40UL)
#define _BIT7   						(0x80UL)
#define _BIT8   						(0x0100UL)
#define _BIT9   						(0x0200UL)
#define _BIT10  						(0x0400UL)
#define _BIT11  						(0x0800UL)
#define _BIT12  						(0x1000UL)
#define _BIT13  						(0x2000UL)
#define _BIT14  						(0x4000UL)
#define _BIT15  						(0x8000UL)
#define _BIT16   						(0x00010000UL)
#define _BIT17  						(0x00020000UL)
#define _BIT18   						(0x00040000UL)
#define _BIT19   						(0x00080000UL)
#define _BIT20   						(0x00100000UL)
#define _BIT21   						(0x00200000UL)
#define _BIT22   						(0x00400000UL)
#define _BIT23   						(0x00800000UL)
#define _BIT24   						(0x01000000UL)
#define _BIT25   						(0x02000000UL)
#define _BIT26  						(0x04000000UL)
#define _BIT27  						(0x08000000UL)
#define _BIT28  						(0x10000000UL)
#define _BIT29  						(0x20000000UL)
#define _BIT30  						(0x40000000UL)
#define _BIT31  						(0x80000000UL)

#if defined(CONFIG_REALTEK_INT_MICOM)
#ifdef CONFIG_CUSTOMER_TV006
/******  share memory section ***************/ 


#define INTMICOM_COMMAND_reg	(0xB8060500)
#define INTMICOM_DATA_reg	(0xB8060504)
#define INTMICOM_CTRL_reg   (0xB8060574)
#define INTMICOM_CHECK_IRDA_reg (0xB8060158)

#define INTMICOM_SHARE_MEMORY_SPACE_BYTES (112)
#define ONE_REGISTER_ADDDRESS (4)


#define INT_MCU_WARN(fmt, args...)	rtd_printk(KERN_WARNING,  "SM_M" , "[Warn]" fmt, ## args);
#define INT_MCU_ERR(fmt, args...)	rtd_printk(KERN_ERR,  "SM_M" , "[ERR]" fmt, ## args);



#define SHARE_MEMORY_TIMEOUT_ms (5000)

typedef unsigned char UINT8;

enum _SHARE_MEMORY_WRITE_ID{
	SH_MEM_WRT_ID_KCPU	= 0,
	SH_MEM_WRT_ID_ACPU1	= 1,
	SH_MEM_WRT_ID_ACPU2	= 2,
	SH_MEM_WRT_ID_VCPU1	= 3,
	SH_MEM_WRT_ID_VCPU2	= 4,
	SH_MEM_WRT_ID_SCPU	= 5,
	SH_MEM_WRT_ID_8051	= 6,
	SH_MEM_WRT_ID_UNKNOW ,
};


enum _SHARE_MEMORY_READ_ID{
	SH_MEM_RD_ID_KCPU	= 0,
	SH_MEM_RD_ID_ACPU1	= 1,
	SH_MEM_RD_ID_ACPU2	= 2,
	SH_MEM_RD_ID_VCPU1	= 3,
	SH_MEM_RD_ID_VCPU2	= 4,
	SH_MEM_RD_ID_SCPU	= 5,
	SH_MEM_RD_ID_8051	= 6,
	SH_MEM_RD_ID_UNKNOW ,
};


enum _SHARE_MEMORY_OPERATION{
	SH_MEM_OP_WRITE	= 0,
	SH_MEM_OP_READ	= 1,
	SH_MEM_OP_UNKNOW ,
};

enum _SHARE_MEMORY_COMMAND_POS{
	SH_MEM_CMMD_WRT_ID_POS	= 0,
	SH_MEM_CMMD_RD_ID_POS	= 8,
	SH_MEM_CMMD_OP_POS	= 16,
	SH_MEM_CMMD_LENGTH_POS	= 24,
	SH_MEM_CMMD_UNKNOW	,
};

enum _SHARE_MEMORY_RETURN_VALUE{
	SH_MEM_OK	= 0,
	SH_MEM_DATALEN_OVERFLOW,
	SH_MEM_UNKNOWN_CMD,
	SH_MEM_NO_RESPONSE,
};

/******  share memory section end ***************/ 
#endif
#endif



#endif



