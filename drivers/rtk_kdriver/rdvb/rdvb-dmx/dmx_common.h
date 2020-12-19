#ifndef __RDVB_DMX_COMMON__
#define __RDVB_DMX_COMMON__
#include <linux/types.h>

typedef struct {
  unsigned char AD_fade_byte;
  unsigned char AD_pan_byte;
  char AD_gain_byte_center;
  char AD_gain_byte_front;
  char AD_gain_byte_surround;
} AUDIO_AD_INFO;

typedef enum {
  DEMUX_DEST_NONE = 0,
  DEMUX_DEST_ADEC0 ,
  DEMUX_DEST_ADEC1,
  DEMUX_DEST_VDEC0,
  DEMUX_DEST_VDEC1,
  DEMUX_DEST_BUFFER,
  DEMUX_DEST_NUM
} DEMUX_PES_DEST_T ;


typedef enum {
  DEMUX_PID_VIDEO = 0,
  DEMUX_PID_AUDIO ,
  DEMUX_PID_AUDIO_DESCRIPTION = DEMUX_PID_AUDIO,
  DEMUX_PID_PES,
  DEMUX_PID_PCR ,
  DEMUX_PID_PSI,
  DEMUX_PID_BYPASS,
  DEMUX_PID_TYPE_NUM
} DEMUX_PID_TYPE_T ;

 
#endif