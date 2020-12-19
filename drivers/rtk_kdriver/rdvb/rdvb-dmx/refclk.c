#include <linux/kernel.h>
#include <linux/string.h>
#include "refclk.h"
#include "rdvb-common/misc.h"

void REFCLK_Reset(REFCLOCK *pRefClock, AV_SYNC_MODE avSyncMode, 
	unsigned long videoFreeRunThreshold)
{
	unsigned long clockMode = pRefClock->clockMode;
#ifdef CONFIG_ARM64
	memset_io(pRefClock, 0 , sizeof(REFCLOCK)) ;
#else
	memset(pRefClock, 0 , sizeof(REFCLOCK)) ;
#endif
	/* don't reset clockMode */
	pRefClock->clockMode = clockMode;

	pRefClock->GPTSTimeout             = reverse_int64(0) ;
	pRefClock->videoFreeRunThreshold   = reverse_int32(videoFreeRunThreshold) ;
	pRefClock->audioFreeRunThreshold   = reverse_int32(DEFAULT_FREE_RUN_AUDIO_THRESHOLD) ;
	pRefClock->audioFreeRunThresholdToWait   = reverse_int32(DEFAULT_FREE_RUN_AUDIO_THRESHOLD) ;
	pRefClock->RCD                     = reverse_int64(-1) ;
	pRefClock->RCD_ext                 = reverse_int32(-1) ;
	pRefClock->videoSystemPTS          = reverse_int64(-1) ;
	pRefClock->audioSystemPTS          = reverse_int64(-1) ;
	pRefClock->videoRPTS               = reverse_int64(-1) ;
	pRefClock->audioRPTS               = reverse_int64(-1) ;
	pRefClock->videoContext            = reverse_int32(-1) ;
	pRefClock->audioContext            = reverse_int32(-1) ;
	pRefClock->masterGPTS              = reverse_int64(-1) ;
	pRefClock->VO_Underflow            = reverse_int32(0) ;
	pRefClock->AO_Underflow            = reverse_int32(0) ;
	pRefClock->videoEndOfSegment       = reverse_int32(-1) ;
	pRefClock->audioEndOfSegment       = reverse_int32(-1) ;
	pRefClock->RCD_Audio               = reverse_int64(-1) ;
	pRefClock->RCD_Video               = reverse_int64(-1) ;
	pRefClock->audioRPTS               = reverse_int64(-1);


	REFCLK_SetMasterShip(pRefClock, avSyncMode);


	dmx_notice(CHANNEL_UNKNOWN,"%s _%d \n", __func__, __LINE__);
	/* change clock mode in DEMUX_StartPCRRecovery()/DEMUX_StopPCRRecovery */
	/* pRefClock->clockMode			   = DEMUX_reverseInteger(MISC_90KHz); */
}
void REFCLK_SetMasterShip(REFCLOCK *pRefClock, AV_SYNC_MODE avSyncMode)
{	
	if (avSyncMode == NAV_AVSYNC_PCR_MASTER) {
		pRefClock->mastership.systemMode   = (unsigned char)AVSYNC_FORCED_MASTER;
		pRefClock->mastership.videoMode    = (unsigned char)AVSYNC_FORCED_SLAVE;
		pRefClock->mastership.audioMode    = (unsigned char)AVSYNC_FORCED_SLAVE;
		pRefClock->mastership.masterState  = (unsigned char)AUTOMASTER_NOT_MASTER;
	} else if (avSyncMode == NAV_AVSYNC_AUDIO_MASTER_AUTO) {
		pRefClock->mastership.systemMode   = (unsigned char)AVSYNC_FORCED_SLAVE;
		pRefClock->mastership.videoMode    = (unsigned char)AVSYNC_AUTO_SLAVE;
		pRefClock->mastership.audioMode    = (unsigned char)AVSYNC_AUTO_MASTER;
		pRefClock->mastership.masterState  = (unsigned char)AUTOMASTER_NOT_MASTER;
	} else if (avSyncMode == NAV_AVSYNC_AUDIO_MASTER_AF) {
		pRefClock->mastership.systemMode   = (unsigned char)AVSYNC_FORCED_SLAVE;
		pRefClock->mastership.videoMode    = (unsigned char)AVSYNC_FORCED_SLAVE;
		pRefClock->mastership.audioMode    = (unsigned char)AVSYNC_FORCED_MASTER;
		pRefClock->mastership.masterState  = (unsigned char)AUTOMASTER_NOT_MASTER;
	} else if (avSyncMode == NAV_AVSYNC_VIDEO_ONLY) {
		pRefClock->mastership.systemMode   = (unsigned char)AVSYNC_FORCED_SLAVE;
		pRefClock->mastership.videoMode    = (unsigned char)AVSYNC_FORCED_MASTER;
		pRefClock->mastership.audioMode    = (unsigned char)AVSYNC_FORCED_SLAVE;
		pRefClock->mastership.masterState  = (unsigned char)AUTOMASTER_NOT_MASTER;
	} else if (avSyncMode == NAV_AVSYNC_AUDIO_ONLY) {
		pRefClock->mastership.systemMode   = (unsigned char)AVSYNC_FORCED_SLAVE;
		pRefClock->mastership.videoMode    = (unsigned char)AVSYNC_FORCED_SLAVE;
		pRefClock->mastership.audioMode    = (unsigned char)AVSYNC_FORCED_MASTER;
		pRefClock->mastership.masterState  = (unsigned char)AUTOMASTER_NOT_MASTER;
	} else if (avSyncMode == NAV_AVSYNC_FREE_RUN) {
		pRefClock->mastership.systemMode  = (unsigned char)AVSYNC_FORCED_SLAVE;
		pRefClock->mastership.videoMode   = (unsigned char)AVSYNC_FORCED_MASTER;
		pRefClock->mastership.audioMode   = (unsigned char)AVSYNC_FORCED_MASTER;
		pRefClock->mastership.masterState = (unsigned char)AUTOMASTER_NOT_MASTER;
	}
}
int64_t REFCLK_GetGPTSTimeout(REFCLOCK *pRefClock)
{
    int64_t timeout = reverse_int64(pRefClock->GPTSTimeout);
    return timeout;
}
int64_t REFCLK_GetRCD(REFCLOCK *pRefClock)
{
    int64_t rcd = reverse_int64(pRefClock->RCD);
    return rcd;
}
void REFCLK_GetPresentationPositions(REFCLOCK *pRefClock, PRESENTATION_POSITIONS *pPosition)
{
    pPosition->referenceClock = CLOCK_GetPTS();
    pPosition->RCD = reverse_int64(pRefClock->RCD);
    pPosition->videoSystemPTS = reverse_int64(pRefClock->videoSystemPTS);
    pPosition->audioSystemPTS = reverse_int64(pRefClock->audioSystemPTS);
    pPosition->videoRPTS = reverse_int64(pRefClock->videoRPTS);
    pPosition->audioRPTS = reverse_int64(pRefClock->audioRPTS);
    pPosition->videoContext = reverse_int32(pRefClock->videoContext);
    pPosition->audioContext = reverse_int32(pRefClock->audioContext);
    pPosition->videoEndOfSegment = reverse_int32(pRefClock->videoEndOfSegment);
}
void REFCLK_SetRCD(REFCLOCK *pRefClock, int64_t rcd)
{
    pRefClock->RCD = reverse_int64(rcd);
}
void REFCLK_SetFreeRunThreshold(REFCLOCK *pRefClock, long videoFreeRunThr,
	long audioFreeRunThr, long audioFreeRunThrToWait)
{
    pRefClock->videoFreeRunThreshold = reverse_int32(videoFreeRunThr);
    pRefClock->audioFreeRunThreshold = reverse_int32(audioFreeRunThr);
    pRefClock->audioFreeRunThresholdToWait = reverse_int32(audioFreeRunThrToWait);
}
void REFCLK_SetAudioFreeRunThreshold(REFCLOCK *pRefClock, long audioFreeRunThr,
	long audioFreeRunThrToWait)
{
    pRefClock->audioFreeRunThreshold = reverse_int32(audioFreeRunThr);
    pRefClock->audioFreeRunThresholdToWait = reverse_int32(audioFreeRunThrToWait);
}
void REFCLK_SetVideoFreeRunThreshold(REFCLOCK *pRefClock, long videoFreeRunThr)
{
    pRefClock->videoFreeRunThreshold = reverse_int32(videoFreeRunThr);
}
void REFCLK_GetFreeRunThreshold(REFCLOCK *pRefClock, long *pVideoFreeRunThr,
	long *pAudioFreeRunThr, long *pAudioFreeRunThrToWait)
{
    *pVideoFreeRunThr = reverse_int32(pRefClock->videoFreeRunThreshold);
    *pAudioFreeRunThr = reverse_int32(pRefClock->audioFreeRunThreshold);
    *pAudioFreeRunThrToWait = reverse_int32(pRefClock->audioFreeRunThresholdToWait);
}
void REFCLK_CheckUnderflow(REFCLOCK *pRefClock, long *pVideoUnderflow,
	long *pAudioUnderflow)
{
    *pVideoUnderflow = reverse_int32(pRefClock->VO_Underflow);
    *pAudioUnderflow = reverse_int32(pRefClock->AO_Underflow);
}
void REFCLK_SetTimeout(REFCLOCK *pRefClock, int64_t timeout)
{
    pRefClock->GPTSTimeout =
    		reverse_int64(CLOCK_GetAVSyncPTS(REFCLK_GetClockMode(pRefClock))+timeout);
}
void REFCLK_SetClockMode(REFCLOCK *pRefClock, enum clock_type clock_type)
{
	pRefClock->clockMode = reverse_int32(clock_type);
}
enum clock_type REFCLK_GetClockMode(REFCLOCK *pRefClock)
{
	return (enum clock_type)reverse_int32(pRefClock->clockMode);
}

void REFCLOCK_SetDemuxAudioPTS(REFCLOCK *pRefClock, int64_t pts)
{
	pRefClock->audioRPTS = reverse_int64(pts);
}