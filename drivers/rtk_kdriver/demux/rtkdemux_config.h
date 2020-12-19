/*
	this file only define  demux configs.
	these configs may be different by ic
*/
#ifndef RTKDEMUX_CONFIG_H
#define RTKDEMUX_CONFIG_H

#define DEMUX_TP_NUM                         4
#define DEMUX_VD_NUM                         2
#define DEMUX_AD_NUM                         2

#define DEMUX_TP_BUFF_SIZE           (16*1024*1024)
#define SDT_CH_BUFF_SIZE			 (512*1024)
#define MTP_BUFFER_SIZE              (6*1024*1024)
#define TPP_MTP_BUFFER_SIZE          (30*188*1024)

#define DEMUX_VD_BS_SIZE             (8*1024*1024)
#define DEMUX_VD_IB_SIZE             (256*1024)
#define DEMUX_AD_BS_SIZE             (256*1024)
#define DEMUX_AD_IB_SIZE             (256*1024)

//TP ralated
/* 
    USE_STATIC_RESERVED_BUF:
        enable : TP(notSDT) buffer is reserved(carvedout)
        disable: TP buffer is allocated from cma
*/
#define USE_STATIC_RESERVED_BUF

/*
    SHARE_TPBUFFER_WITH_MTPBUFFER
        enable : mtp buffer allocate from related TP buffer
        disable: mtp buffer is allocated from cma
*/ 
#define SHARE_TPBUFFER_WITH_MTPBUFFER

//video
/*
    WITH_SVP
        enable : video es buffer Allocate frome SVP
        disable: video es buffer is allocated from cma
*/
#define WITH_SVP 
//audio
//#define DEMUX_AUDIO_USE_GLOBAL_BUFFER
#endif