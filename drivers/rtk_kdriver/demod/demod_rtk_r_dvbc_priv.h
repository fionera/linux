#ifndef  __DEMOD_REALTEK_I_DVBC_PRIV_H__
#define  __DEMOD_REALTEK_I_DVBC_PRIV_H__

#include "qam_demod_rtk_r.h"
#include "demod_rtk_common.h"

#define GET_SIGNAL_STRENGTH_FROM_SNR

typedef struct {
	QAM_DEMOD_MODULE*      pDemod;
	BASE_INTERFACE_MODULE*  pBaseInterface;
	I2C_BRIDGE_MODULE*      pI2CBridge;
	unsigned char           DeviceAddr;
	U32BITS           CrystalFreqHz;
} REALTEK_R_DVBC_DRIVER_DATA;


#define DECODE_LOCK(x)      ((x==YES) ? DTV_SIGNAL_LOCK : DTV_SIGNAL_NOT_LOCK)


extern REALTEK_R_DVBC_DRIVER_DATA* AllocRealtekRDvbcDriver(
	COMM*               pComm,
	unsigned char       Addr,
	U32BITS       CrystalFreq
);

extern void ReleaseRealtekRDvbcDriver(REALTEK_R_DVBC_DRIVER_DATA *pDriver);

//--------------------------------------------------------------------------
// Optimization Setting for Tuner
//--------------------------------------------------------------------------

#endif // __DEMOD_REALTEK_R_DVBC_PRIV_H__
