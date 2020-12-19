#include <asm/io.h>
#include "clock.h"
#include <tp/tp_drv_global.h>
#include <rbus/tp_reg.h>
#include <rbus/timer_reg.h>

int64_t CLOCK_GetPTS(void)
{
	unsigned int ptrlo = rtd_inl(TIMER_SCPU_CLK90K_LO_reg) ;
	unsigned int ptrhi = rtd_inl(TIMER_SCPU_CLK90K_HI_reg) ;
	int64_t ret = ptrlo | (((int64_t)ptrhi) << 32) ;

	return ret ;
}

int64_t CLOCK_GetTPPTS(enum clock_type clock_type)
{
	int64_t ret = -1;

	unsigned int ptrlo = 0;
	unsigned int ptrhi = 0;


	switch (clock_type) {
#ifdef CONFIG_RTK_KDRV_MULTI_TP_CLOCK
	case CLOCK_PCRA:
		ptrlo = rtd_inl(TP_TP_PCRA_CNT_LOW_reg);
		ptrhi = rtd_inl(TP_TP_PCRA_CNT_HIGH_reg);
		break;
	case CLOCK_PCRB:
		ptrlo = rtd_inl(TP_TP_PCRB_CNT_LOW_reg);
		ptrhi = rtd_inl(TP_TP_PCRB_CNT_HIGH_reg);
		break;
#endif
	default:
	ptrlo = rtd_inl(TP_TP_PCRA_CNT_LOW_reg);
	ptrhi = rtd_inl(TP_TP_PCRA_CNT_HIGH_reg);
		break;
	}

	ret = ptrlo | (((int64_t)ptrhi) << 32);

	return ret;
}


int64_t CLOCK_GetAVSyncPTS(enum clock_type clock_type)
{
	if (clock_type == CLOCK_MISC)
		return CLOCK_GetPTS();
	else
		return CLOCK_GetTPPTS(clock_type);
}
