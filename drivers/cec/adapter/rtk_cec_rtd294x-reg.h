/*Copyright (C) 2007-2013 Realtek Semiconductor Corporation.*/
#ifndef __RTK_CEC_RTD294x__REG_H__
#define __RTK_CEC_RTD294x__REG_H__

#include "rtk_cec_rtd299x.h"
#include <mach/irqs.h>

#define ISO_IRQ					IRQ_CEC
#define write_reg32(addr, val)	rtd_outl(addr, val)
#define read_reg32(addr)		rtd_inl(addr)
#define RTD294x_CEC_INT_EN		(0xB8000290)	/* cec interrupt address */

#define RTD294x_ST_CLKEN1		(0xb8060044)
#define RTD294x_ST_SRST1		(0xb8060034)

#define RTD294x_CEC_CR0			(0xB8061E00)
#define RTD294x_CEC_RTCR0		(0xB8061E04)
#define RTD294x_CEC_RXCR0		(0xB8061E08)
#define RTD294x_CEC_TXCR0		(0xB8061E0C)
#define RTD294x_CEC_TXDR0		(0xB8061E10)
#define RTD294x_CEC_TXDR1		(0xB8061E14)
#define RTD294x_CEC_TXDR2		(0xB8061E18)
#define RTD294x_CEC_TXDR3		(0xB8061E1C)
#define RTD294x_CEC_TXDR4		(0xB8061E20)
#define RTD294x_CEC_RXDR1		(0xB8061E24)
#define RTD294x_CEC_RXDR2		(0xB8061E28)
#define RTD294x_CEC_RXDR3		(0xB8061E2C)
#define RTD294x_CEC_RXDR4		(0xB8061E30)
#define RTD294x_CEC_RXTCR0		(0xB8061E34)
#define RTD294x_CEC_TXTCR0		(0xB8061E38)
#define RTD294x_CEC_TXTCR1		(0xB8061E3C)

#define RTD294x_GDI_CEC_COMPARE_OPCODE_01		(0xB8061E40)
#define RTD294x_GDI_CEC_SEND_OPCODE_01			(0xB8061E44)
#define RTD294x_GDI_CEC_SEND_OPERAND_NUMBER_01	(0xB8061E48)
#define RTD294x_GDI_CEC_OPERAND_01				(0xB8061E4C)
#define RTD294x_GDI_CEC_OPERAND_02				(0xB8061E50)
#define RTD294x_GDI_CEC_OPERAND_03				(0xB8061E54)
#define RTD294x_GDI_CEC_COMPARE_OPCODE_02		(0xB8061E58)
#define RTD294x_GDI_CEC_SEND_OPCODE_02			(0xB8061E5C)
#define RTD294x_GDI_CEC_SEND_OPERAND_NUMBER_02	(0xB8061E60)
#define RTD294x_GDI_CEC_OPERAND_04				(0xB8061E64)
#define RTD294x_GDI_CEC_OPERAND_05				(0xB8061E68)
#define RTD294x_GDI_CEC_OPERAND_06				(0xB8061E6C)
#define RTD294x_GDI_CEC_COMPARE_OPCODE_03		(0xB8061E70)
#define RTD294x_GDI_CEC_SEND_OPCODE_03			(0xB8061E74)
#define RTD294x_GDI_CEC_SEND_OPERAND_NUMBER_03	(0xB8061E78)
#define RTD294x_GDI_CEC_OPERAND_07				(0xB8061E7C)
#define RTD294x_GDI_CEC_OPERAND_08				(0xB8061E80)
#define RTD294x_GDI_CEC_OPERAND_09				(0xB8061E84)
#define RTD294x_GDI_CEC_COMPARE_OPCODE_04		(0xB8061E88)
#define RTD294x_GDI_CEC_SEND_OPCODE_04			(0xB8061E8C)
#define RTD294x_GDI_CEC_SEND_OPERAND_NUMBER_04	(0xB8061E90)
#define RTD294x_GDI_CEC_OPERAND_10				(0xB8061E94)
#define RTD294x_GDI_CEC_OPERAND_11				(0xB8061E98)
#define RTD294x_GDI_CEC_OPERAND_12				(0xB8061E9C)
#define RTD294x_GDI_CEC_COMPARE_OPCODE_05		(0xB8061EA0)
#define RTD294x_GDI_CEC_COMPARE_OPCODE_06		(0xB8061EA4)
#define RTD294x_GDI_CEC_COMPARE_OPCODE_07		(0xB8061EA8)
#define RTD294x_GDI_CEC_COMPARE_OPCODE_08		(0xB8061EAC)
#define RTD294x_GDI_CEC_COMPARE_OPCODE_09		(0xB8061EB0)
#define RTD294x_GDI_CEC_COMPARE_OPCODE_10		(0xB8061EB4)
#define RTD294x_GDI_CEC_COMPARE_OPCODE_11		(0xB8061EB8)
#define RTD294x_GDI_CEC_COMPARE_OPCODE_12		(0xB8061EBC)
#define RTD294x_GDI_CEC_COMPARE_OPCODE_13		(0xB8061EC0)
#define RTD294x_GDI_CEC_COMPARE_OPCODE_14		(0xB8061EC4)
#define RTD294x_GDI_CEC_COMPARE_OPCODE_15		(0xB8061EC8)

#define RTD294x_GDI_CEC_OPCODE_ENABLE			(0xB8061ECC)
#define RTD294x_GDI_POWER_SAVING_MODE			(0xB8061ED0)
#define RTD294x_CEC_RxACKTCR0					(0xB8061ED4)
#define RTD294x_CEC_RxACKTCR1					(0xB8061ED8)
#define RTD294x_CEC_TxRxACKOPTION				(0xB8061EDC)

#ifdef	CONFIG_MACH_RTK_298x
#define CEC_MODEL_NAME			"RTD298x"
#else
#define CEC_MODEL_NAME			"RTD299x"
#endif

#define CEC_CLKEN				RTD294x_ST_CLKEN1
#define CEC_SRST				RTD294x_ST_SRST1
#define CEC_CR0					RTD294x_CEC_CR0
#define CEC_RTCR0				RTD294x_CEC_RTCR0
#define CEC_RXCR0				RTD294x_CEC_RXCR0
#define CEC_TXCR0				RTD294x_CEC_TXCR0
#define CEC_TXDR0				RTD294x_CEC_TXDR0
#define CEC_TXDR1				RTD294x_CEC_TXDR1
#define CEC_TXDR2				RTD294x_CEC_TXDR2
#define CEC_TXDR3				RTD294x_CEC_TXDR3
#define CEC_TXDR4				RTD294x_CEC_TXDR4
#define CEC_RXDR1				RTD294x_CEC_RXDR1
#define CEC_RXDR2				RTD294x_CEC_RXDR2
#define CEC_RXDR3				RTD294x_CEC_RXDR3
#define CEC_RXDR4				RTD294x_CEC_RXDR4
#define CEC_RXTCR0				RTD294x_CEC_RXTCR0
#define CEC_TXTCR0				RTD294x_CEC_TXTCR0
#define CEC_TXTCR1				RTD294x_CEC_TXTCR1
#define CEC_INT_EN				RTD294x_CEC_INT_EN
#define CEC_INT_EN_MASK			(0x1<<5)
#define CEC_INT_EN_VAL			(0x1<<5)

#define CEC_RxACKTCR0			RTD294x_CEC_RxACKTCR0
#define CEC_RxACKTCR1			RTD294x_CEC_RxACKTCR1

/* CLKEN */
#define CLKEN_CEC                       (0x1<<7)
#define RSTN_CEC                        (0x1<<7)

/* CR0 */
#define LOGICAL_ADDR_SHIFT      		24

#define CEC_MODE(x)             		((x & 0x3)<<30)
#define TEST_MODE_PAD_EN        		(0x1<<28)
#define LOGICAL_ADDR(x)         		((x & 0xF)<<LOGICAL_ADDR_SHIFT)
#define TIMER_DIV(x)            		((x & 0xFF)<<16)
#define PRE_DIV(x)              		((x & 0xFF)<<8)
#define UNREG_ACK_EN            		(1<<7)
#define CDC_ARBITRATION_EN      		(0x1<<5)
#define TEST_MODE_DATA(x)        	    (x & 0x1F)

#define CEC_MODE_MASK           		(CEC_MODE(3))
#define LOGICAL_ADDR_MASK       		(LOGICAL_ADDR(0xF))
#define CTRL_MASK1              		(CEC_MODE_MASK | LOGICAL_ADDR_MASK)

/* RTCR0 */
#define CEC_PAD_IN              		(0x1<<31)
#ifdef	CONFIG_MACH_RTK_298x
#define	CEC_PAD_EN						(0x1<<17)
#define	CEC_PAD_EN_MODE					(0x1<<16)
#define	CEC_HW_RETRY_EN					(0x01<<12)
#else
#define CEC_EOM_ON						(0x01<<12)
#endif
#define CLEAR_CEC_INT           		(0x1<<11)
#define WT_CNT(x)               		((x & 0x3F)<<5)
#define LATTEST                 		(0x1<<4)
#define RETRY_NO_MASK					(0x0F)
#define RETRY_NO(x)             		(x & 0xF)

/* RXCR0 */
#define INIT_ADDR_SHIFT         		(8)

#define BROADCAST_ADDR          		(0x1<<31)
#define RX_SAME_LA_ACK					(0x1<<24)
#define STB_ADDR_SEL					(0x1<<20)
#define STB_INIT_ADDR(x)				((x & 0xF)<<16)
#define RX_EN                   		(0x1<<15)
#define RX_RST                  		(0x1<<14)
#define RX_CONTINUOUS           		(0x1<<13)
#define RX_INT_EN               		(0x1<<12)
#define INIT_ADDR(x)            		((x & 0xF)<<INIT_ADDR_SHIFT)
#define RX_EOM                  		(0x1<<7)
#define RX_INT                  		(0x1<<6)
#define RX_FIFO_OV              		(0x1<<5)
#define RX_FIFO_CNT(x)          		(x & 0x1F)

#define INIT_ADDR_MASK          		INIT_ADDR(0xF)

/* TXCR0 */
#define TX_ADDR_EN              		(0x1<<20)
#define TX_ADDR(x)              		((x & 0xF)<<16)
#define TX_EN                   		(0x1<<15)
#define TX_RST                  		(0x1<<14)
#define TX_CONTINUOUS          		(0x1<<13)
#define TX_INT_EN               		(0x1<<12)
#define DEST_ADDR(x)            		((x & 0xF)<<8)
#define TX_EOM                  		(0x1<<7)
#define TX_INT                  		(0x1<<6)
#define TX_FIFO_UD              		(0x1<<5)
#define TX_FIFO_CNT(x)          		(x & 0x1F)

/* TXDR0 */
#define TX_ADD_CNT              		(0x1<<6)
#define RX_SUB_CNT              		(0x1<<5)
#define FIFO_CNT(x)             		(x & 0x1F)

/* RXTCR0 */
#define RX_START_LOW(x)         		((x & 0xFF)<<24)
#define RX_START_PERIOD(x)      		((x & 0xFF)<<16)
#define RX_DATA_SAMPLE(x)       		((x & 0xFF)<<8)
#define RX_DATA_PERIOD(x)       		(x & 0xFF)

/* TXCR0/1 */
#define TX_START_LOW(x)         		((x & 0xFF)<<8)
#define TX_START_HIGH(x)        		((x & 0xFF))

#define TX_DATA_LOW(x)          		((x & 0xFF)<<16)
#define TX_DATA_01(x)           		((x & 0xFF)<<8)
#define TX_DATA_HIGH(x)         		((x & 0xFF))

/* RxACKTCR1 */
#define RX_ACK_LOW_SEL			(0x01<<26)
#define RX_ACK_BIT_SEL			(0X01<<25)
#define RX_ACK_ACK_LINE_ERR_SEL	(0X01<<24)
#define RX_ACK_LOW(x)			((x & 0xFF)<<16)
#define RX_ACK_BIT(x)			((x & 0xFF)<<8)
#define RX_ACK_LINE_ERR(x)		((x & 0xFF))

#endif /* __RTK_CEC_RTD294x__REG_H__ */
