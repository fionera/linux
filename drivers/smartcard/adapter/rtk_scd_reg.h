#ifndef __RTK_SCD_REG_H__
#define __RTK_SCD_REG_H__

#include <mach/irqs.h>
#include <rbus/crt_reg.h>
#include <rbus/misc_reg.h>

#define MISC_IRQ                IRQ_MISC

/* Smart Card Feature Block*/

/* SC_FP - SC Frequency Programmer*/
#define SC_CLK_EN(x)            ((x & 0x00000001)<<24)
#define SC_CLKDIV(x)            ((x & 0x0000003F)<<18)
#define SC_BAUDDIV2(x)          ((x & 0x00000003)<<16)
#define SC_BAUDDIV1(x)          ((x & 0x000000FF)<<8)
#define SC_PRE_CLKDIV(x)        ((x & 0x000000FF))
#define SC_CLKDIV_MASK          SC_CLKDIV(0x3F)
#define SC_BAUDDIV2_MASK        SC_BAUDDIV2(0x3)
#define SC_BAUDDIV1_MASK        SC_BAUDDIV1(0xFF)
#define SC_BAUDDIV_MASK         (SC_BAUDDIV1_MASK | SC_BAUDDIV2_MASK)
#define SC_PRE_CLKDIV_MASK      SC_PRE_CLKDIV(0xFF)

#define SC_CLK_EN_VAL(x)        ((x << 24) & 0x00000001)
#define SC_CLKDIV_VAL(x)        ((x << 18) & 0x0000003F)
#define SC_BAUDDIV2_VAL(x)      ((x << 16) & 0x00000003)
#define SC_BAUDDIV1_VAL(x)      ((x <<  8) & 0x000000FF)
#define SC_PRE_CLKDIV_VAL(x)    ((x & 0x000000FF))

/*SC_CR - SC Control Register*/
#define SC_FIFO_RST(x)          ((x & 0x00000001)<<31)
#define SC_RST(x)               ((x & 0x00000001)<<30)
#define SC_SCEN(x)              ((x & 0x00000001)<<29)
#define SC_TX_GO(x)             ((x & 0x00000001)<<28)
#define SC_AUTO_ATR(x)          ((x & 0x00000001)<<27)
#define SC_CONV(x)              ((x & 0x00000001)<<26)
#define SC_CLK_STOP(x)          ((x & 0x00000001)<<25)
#define SC_PS(x)                ((x & 0x00000001)<<24)

#define SC_PS_VAL(x)            ((x >> 24) & 0x00000001)

/*SCPCR - SC Protocol control Register */
#define SC_TXGRDT(x)          ((x & 0x000000FF)<<24)
#define SC_CWI(x)             ((x & 0x0000000F)<<20)
#define SC_BWI(x)             ((x & 0x0000000F)<<16)
#define SC_WWI(x)             ((x & 0x0000000F)<<12)
#define SC_BGT(x)             ((x & 0x0000001F)<<7)
#define SC_EDC_EN(x)          ((x & 0x00000001)<<6)
#define SC_CRC(x)             ((x & 0x00000001)<<5)
#define SC_PROTOCOL_T(x)      ((x & 0x00000001)<<4)
#define SC_T0RTY(x)           ((x & 0x00000001)<<3)
#define SC_T0RTY_CNT(x)       ((x & 0x00000007))

#define MASK_SC_TXGRDT        SC_TXGRDT(0xFF)
#define MASK_SC_CWI           SC_CWI(0xF)
#define MASK_SC_BWI           SC_BWI(0xF)
#define MASK_SC_WWI           SC_WWI(0xF)
#define MASK_SC_BGT           SC_BGT(0x1F)
#define MASK_SC_EDC_EN        SC_EDC_EN(1)
#define MASK_SC_EDC_TYPE      SC_CRC(1)
#define MASK_SC_PROTOCOL      SC_PROTOCOL_T(1)
#define MASK_SC_T0RTY         SC_T0RTY(1)
#define MASK_SC_T0RTY_CNT     SC_T0RTY_CNT(1)

/*SC_FCR - Flow Control Register*/
#define SC_RXFLOW(x)          ((x & 0x00000001)<<1)
#define SC_FLOW_EN(x)         ((x & 0x00000001))

/*SCIRSR & SCIRER - SC Interrupt Status/Enable Register*/
#define SC_DRDY_INT           0x00000001
#define SC_RCV_INT            0x00000002
#define SC_BWT_INT            0x00000004
#define SC_WWT_INT            0x00000008
#define SC_RLEN_INT           0x00000010
#define SC_CWT_INT            0x00000020
#define SC_BGT_INT            0x00000040
#define SC_ATRS_INT           0x00000080
#define SC_RXP_INT            0x00000100
#define SC_RX_FOVER_INT       0x00000200
#define SC_EDCERR_INT         0x00000400
#define SC_TXEMPTY_INT        0x00000800
#define SC_TXDONE_INT         0x00001000
#define SC_TXP_INT            0x00002000
#define SC_TXFLOW_INT         0x00004000
#define SC_CPRES_INT          0x00008000
#define SC_PRES               0x00010000

/*********************************************************/

/*#ifdef CONFIG_MACH_RTK_294X*/
#ifdef CONFIG_ARCH_RTKS2B
#define MAX_IFD_CNT 1
#define IFD_MODOLE  "RTD294x"
#define SYSTEM_CLK  180000000

static const SC_REG SCReg[] = {
	{
#if 0
	 .INT_EN = SCPU_INT_EN_reg,
	 .INT_MASK = SCPU_INT_EN_sc0_mask,
	 .ISR = ISR_reg,
	 .ISR_MASK = ISR_sc0_int_mask,
#else
	 /*there is some error in misc reg.h, use force mode to instead. */
	 .INT_EN = 0xb801b004,
	 .INT_MASK = (1 << 24),
	 .ISR = 0xb801b000,
	 .ISR_MASK = (1 << 24),
#endif

	 .FP = SC0_FP_reg,
	 .CR = SC0_CR_reg,
	 .PCR = SC0_PCR_reg,
	 .TXFIFO = SC0_TXFIFO_reg,
	 .RXFIFO = SC0_RXFIFO_reg,
	 .RXLENR = SC0_RXLENR_reg,
	 .FCR = SC0_FCR_reg,
	 .IRSR = SC0_IRSR_reg,
	 .IRER = SC0_IRER_reg,
	 },
};

#define SC_CLK_EN_REG         SYS_CLKEN2_reg
#define SC_CLK_EN_MASK        SYS_CLKEN2_clken_sc_mask

#define SC_HW_RST_REG         SYS_SRST2_reg
#define SC_HW_RST_MASK        SYS_SRST2_rstn_sc_mask

#endif

/*********************************************************/

#ifdef CONFIG_MACH_RTK_298x

#define MAX_IFD_CNT             1
#define IFD_MODOLE              "RTD298x"
#define SYSTEM_CLK              27000000

static const SC_REG SCReg[2] = {
	{
	 .INT_EN = MIS_SCPU_INT_EN_reg,
	 .INT_MASK = MIS_SCPU_INT_EN_sc0_mask,
	 .ISR = MIS_ISR_reg,
	 .ISR_MASK = MIS_ISR_sc0_int_mask,
	 .FP = MIS_SC0_FP_reg,
	 .CR = MIS_SC0_CR_reg,
	 .PCR = MIS_SC0_PCR_reg,
	 .TXFIFO = MIS_SC0_TXFIFO_reg,
	 .RXFIFO = MIS_SC0_RXFIFO_reg,
	 .RXLENR = MIS_SC0_RXLENR_reg,
	 .FCR = MIS_SC0_FCR_reg,
	 .IRSR = MIS_SC0_IRSR_reg,
	 .IRER = MIS_SC0_IRER_reg,
	 },
	{
	 .INT_EN = MIS_SCPU_INT_EN_reg,
	 .INT_MASK = MIS_SCPU_INT_EN_sc0_mask,
	 .ISR = MIS_ISR_reg,
	 .ISR_MASK = MIS_ISR_sc1_int_mask,
	 .FP = MIS_SC1_FP_reg,
	 .CR = MIS_SC1_CR_reg,
	 .PCR = MIS_SC1_PCR_reg,
	 .TXFIFO = MIS_SC1_TXFIFO_reg,
	 .RXFIFO = MIS_SC1_RXFIFO_reg,
	 .RXLENR = MIS_SC1_RXLENR_reg,
	 .FCR = MIS_SC1_FCR_reg,
	 .IRSR = MIS_SC1_IRSR_reg,
	 .IRER = MIS_SC1_IRER_reg,
	 },
};

#define SC_CLK_EN_REG         CRT_CLOCK_ENABLE2_VADDR
#define SC_CLK_EN_MASK        (1<<13)

#define SC_HW_RST_REG         CRT_SOFT_RESET2_VADDR
#define SC_HW_RST_MASK        (1<<23)

#endif

/*********************************************************/

#ifdef CONFIG_MACH_RTK_299x
#define SC_AYNC_RBUS_READ_RECOVERY
#define IFD_MODOLE              "RTD299x"
#define SYSTEM_CLK              180000000

static const SC_REG SCReg[2] = {
	{
	 .INT_EN = MIS_SCPU_INT_EN_reg,
	 .INT_MASK = MIS_SCPU_INT_EN_sc0_mask,
	 .ISR = MIS_ISR_reg,
	 .ISR_MASK = MIS_ISR_sc0_int_mask,
	 .FP = MIS_SC0_FP_reg,
	 .CR = MIS_SC0_CR_reg,
	 .PCR = MIS_SC0_PCR_reg,
	 .TXFIFO = MIS_SC0_TXFIFO_reg,
	 .RXFIFO = MIS_SC0_RXFIFO_reg,
	 .RXLENR = MIS_SC0_RXLENR_reg,
	 .FCR = MIS_SC0_FCR_reg,
	 .IRSR = MIS_SC0_IRSR_reg,
	 .IRER = MIS_SC0_IRER_reg,
	 },
	{
	 .INT_EN = MIS_SCPU_INT_EN_reg,
	 .INT_MASK = MIS_SCPU_INT_EN_sc0_mask,
	 .ISR = MIS_ISR_reg,
	 .ISR_MASK = MIS_ISR_sc1_int_mask,
	 .FP = MIS_SC1_FP_reg,
	 .CR = MIS_SC1_CR_reg,
	 .PCR = MIS_SC1_PCR_reg,
	 .TXFIFO = MIS_SC1_TXFIFO_reg,
	 .RXFIFO = MIS_SC1_RXFIFO_reg,
	 .RXLENR = MIS_SC1_RXLENR_reg,
	 .FCR = MIS_SC1_FCR_reg,
	 .IRSR = MIS_SC1_IRSR_reg,
	 .IRER = MIS_SC1_IRER_reg,
	 },
};

#define SC_CLK_EN_REG         CRT_CLOCK_ENABLE2_VADDR
#define SC_CLK_EN_MASK        (1<<13)

#define SC_HW_RST_REG         CRT_SOFT_RESET2_VADDR
#define SC_HW_RST_MASK        (1<<23)

#endif

/*#ifdef CONFIG_MACH_RTK_299S*/
#if defined(CONFIG_ARCH_RTK299S) || defined(CONFIG_ARCH_RTK299O) || defined(CONFIG_ARCH_RTK293x) || defined(CONFIG_ARCH_RTK289X)

#include <rbus/rbusInterruptReg.h>

#define MAX_IFD_CNT             2
#define IFD_MODOLE              "RTD299S"
#define SYSTEM_CLK              250000000  /*180000000 ?*/

static const SC_REG SCReg[MAX_IFD_CNT] = {
		{
				.INT_EN   = INTERRUPT_MIS_ISR_VADDR,    /*299S has no SCPU INT EN mask*/
				.INT_MASK = 0,
				.ISR      = INTERRUPT_MIS_ISR_VADDR,
				.ISR_MASK = (1<<24),
				.FP       = SC0_FP_reg,
				.CR       = SC0_CR_reg,
				.PCR      = SC0_PCR_reg,
				.TXFIFO   = SC0_TXFIFO_reg,
				.RXFIFO   = SC0_RXFIFO_reg,
				.RXLENR   = SC0_RXLENR_reg,
				.FCR      = SC0_FCR_reg,
				.IRSR     = SC0_IRSR_reg,
				.IRER     = SC0_IRER_reg,
		},
		{
				.INT_EN   = INTERRUPT_MIS_ISR_VADDR,      /*299S has no SCPU INT EN mas*/
				.INT_MASK = 0,
				.ISR      = INTERRUPT_MIS_ISR_VADDR,
				.ISR_MASK = (1<<23),
				.FP       = SC1_FP_reg,
				.CR       = SC1_CR_reg,
				.PCR      = SC1_PCR_reg,
				.TXFIFO   = SC1_TXFIFO_reg,
				.RXFIFO   = SC1_RXFIFO_reg,
				.RXLENR   = SC1_RXLENR_reg,
				.FCR      = SC1_FCR_reg,
				.IRSR     = SC1_IRSR_reg,
				.IRER     = SC1_IRER_reg,
		},
};


#define SC_CLK_EN_REG         CRT_SYS_CLKEN2_reg
#define SC_CLK_EN_MASK        CRT_SYS_CLKEN2_clken_sc_mask

#define SC_HW_RST_REG         CRT_SYS_SRST2_reg
#define SC_HW_RST_MASK        CRT_SYS_SRST2_rstn_sc_mask

#endif

#endif /*__RTK_SCD_REG_H__*/
