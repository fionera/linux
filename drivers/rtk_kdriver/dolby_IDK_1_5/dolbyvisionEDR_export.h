#ifndef _DOLBYVISION_EDR_DRIVER_EXPORT_H_
#define _DOLBYVISION_EDR_DRIVER_EXPORT_H_
//#include <../demux/rtkdemux_common.h>

#ifndef WIN32
/* data structure for IOCTL call */
#define DOLBYVISIONEDR_IOC_MAGIC  'h'

#define DOLBYVISIONEDR_IOC_RB_INIT				_IOWR(DOLBYVISIONEDR_IOC_MAGIC, 0, int)
#define DOLBYVISIONEDR_IOC_RUN					_IOR(DOLBYVISIONEDR_IOC_MAGIC, 1, int)
#define DOLBYVISIONEDR_IOC_STOP					_IOR(DOLBYVISIONEDR_IOC_MAGIC, 2, int)
#define DOLBYVISIONEDR_IOC_PAUSE				_IOR(DOLBYVISIONEDR_IOC_MAGIC, 3, int)
#define DOLBYVISIONEDR_IOC_RB_FLUSH 			_IOR(DOLBYVISIONEDR_IOC_MAGIC, 4, int)

#define DOLBYVISIONEDR_IOC_3DLUT_UPDATE 		_IOR(DOLBYVISIONEDR_IOC_MAGIC, 5, int)


#define DOLBYVISIONEDR_HDMI_IN_TEST 			_IOR(DOLBYVISIONEDR_IOC_MAGIC, 6, int)
#define DOLBYVISIONEDR_NORMAL_IN_TEST			_IOR(DOLBYVISIONEDR_IOC_MAGIC, 7, int)
#define DOLBYVISIONEDR_DM_CRF_420_TEST		_IOR(DOLBYVISIONEDR_IOC_MAGIC, 8, int)
#define DOLBYVISIONEDR_DM_CRF_422_TEST		_IOR(DOLBYVISIONEDR_IOC_MAGIC, 9, int)
#define DOLBYVISIONEDR_COMPOSER_CRF_TEST		_IOR(DOLBYVISIONEDR_IOC_MAGIC, 10, int)
#define DOLBYVISIONEDR_CRF_DUMP_TEST			_IOR(DOLBYVISIONEDR_IOC_MAGIC, 11, int)
#define DOLBYVISIONEDR_CRF_TRIGGER_TEST		_IOWR(DOLBYVISIONEDR_IOC_MAGIC, 12, int)
#define DOLBYVISIONEDR_TEST_TEST				_IOR(DOLBYVISIONEDR_IOC_MAGIC, 13, int)

#define DOLBYVISIONEDR_DRV_VPQ_SETDOLBYPICMODE 		_IOR(DOLBYVISIONEDR_IOC_MAGIC, 14, int)
#define DOLBYVISIONEDR_DRV_VPQ_SETDOLBYBACKLIGHT 	_IOR(DOLBYVISIONEDR_IOC_MAGIC, 15, int)
#define DOLBYVISIONEDR_DRV_VPQ_SETDOLBYCONTRAST 	_IOR(DOLBYVISIONEDR_IOC_MAGIC, 16, int)
#define DOLBYVISIONEDR_DRV_VPQ_SETDOLBYBRIGHTNESS 	_IOR(DOLBYVISIONEDR_IOC_MAGIC, 17, int)
#define DOLBYVISIONEDR_DRV_VPQ_SETDOLBYCOLOR 		_IOR(DOLBYVISIONEDR_IOC_MAGIC, 18, int)
#define DOLBYVISIONEDR_DRV_VPQ_INITDOLBYPICCONFIG	_IOR(DOLBYVISIONEDR_IOC_MAGIC, 19, int)
#define DOLBYVISIONEDR_DRV_VPQ_GETDOLBYSWVERSION	_IOR(DOLBYVISIONEDR_IOC_MAGIC, 20, int)
#define DOLBYVISIONEDR_DRV_SUPPORT_STATUS		_IOR(DOLBYVISIONEDR_IOC_MAGIC, 21, int)



#define DOLBYVISIONEDR_IOC_MAXNR				 22


/* dolbyvision meta-parser & DM for flow control */
typedef struct _DV_INIT_STRUCT_
{
	/* in-band & MD ring buffer header */
	unsigned int inband_rbh_addr;
	unsigned int md_rbh_addr;

	/* MD-parser output buffer */
	unsigned int md_output_buf_addr;
	unsigned int md_output_buf_size;
} DOLBYVISION_INIT_STRUCT;

/* composer & DM parameters(include 1D LUT) by MD parser */
typedef struct _DV_MDOUT_STRUCT_
{
	unsigned int own_bit;	/* 0: available 1: in-use for MD use*/
	unsigned int PTSH;
	unsigned int PTSL;
	unsigned int TimeStampH;
	unsigned int TimeStampL;
	unsigned int Real_PTSH;//PTSH2
	unsigned int Real_PTSL;//PTSL2
	unsigned int PicMode;//Record from which picture mode
	unsigned int BackLight;//Record from which BackLight
	unsigned int own_bit2; /* 0: available 1: in-use for gst and vo use*/
	unsigned int h_v_size;	/*get h size[16-31]and v size [0-15] from omx*/
	unsigned int BacklightSliderScalar;	/*BacklightSliderScalar for backlight control*/
	unsigned int FEL;	/*IF true means dual layer*/
	/*rpu_ext_config_fixpt_main_t  main_cfg;*/
	char comp_metadata[1800];
	/*DmExecFxp_t DmExec;*/
	char dm_ExecFxp[43236];//[41004];//[11528];
} DOLBYVISION_MD_OUTPUT;


#endif

#define MD_OFFSET 52// structure DOLBYVISION_MD_OUTPUT  Offset = 4*13 = 52 shown below
	//own_bit + own_bit2 + PTSH + PTSL + TimeStampH + TimeStampL + Real_PTSH + Real_PTSL + PicMode + BackLight + h_v_size + reserved2 + reserved3


#endif		/* end of _DOLBYVISION_EDR_DRIVER_EXPORT_H_ */
