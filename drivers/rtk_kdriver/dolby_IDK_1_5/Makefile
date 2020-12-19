#
# Makefile for the Realtek DolbyVision Extended Dynamic Range(EDR)
#
KBUILD_CPPFLAGS += -Idrivers/rtk_kdriver/dolby_IDK_1_5/IDK_1_5/libs/vdr_rpu/include
KBUILD_CPPFLAGS += -Idrivers/rtk_kdriver/dolby_IDK_1_5/IDK_1_5/libs/dm/dlb_dm_process/include
KBUILD_CPPFLAGS += -Idrivers/rtk_kdriver/dolby_IDK_1_5/IDK_1_5/libs/dm/dlb_dm_process/tcgen/include
KBUILD_CPPFLAGS += -Idrivers/rtk_kdriver/dolby_IDK_1_5/IDK_1_5/libs/dm/dlb_dm_process/tcgen/basicops
KBUILD_CPPFLAGS += -Idrivers/rtk_kdriver/include/rtk_kdriver
KBUILD_CPPFLAGS += -Idrivers/rtk_kdriver/dolby_IDK_1_5/edbec_inc
KBUILD_CPPFLAGS += -Idrivers/rtk_kdriver/dolby_IDK_1_5/
KBUILD_CPPFLAGS += -Idrivers/rtk_kdriver/demux/
KBUILD_CPPFLAGS += -Iarch/arm/
KBUILD_CPPFLAGS += -DRTK_EDBEC_CONTROL -DRTK_EDBEC_1_5

KBUILD_CFLAGS += -Werror

KBUILD_CFLAGS += -DCDM_FLOAT_POINT=0

#	   IDK_1_4_2_GA/libs/dm/dlb_dm_process/src/CdmUtilFxp.o \
#obj-y := edbec_api.o VdrDM.o VdrDmApiCmmt.o toneCurveLUT.o VdrDmApiCfg.o lc_2x1d_luts.o DmFunctions.o fx_basicops.o fx_math.o VdrDmDataIo.o file_ops.o dv_md_parser.o metadata_parser.o rpu_decoder.o rpu_parser.o crc32.o rpu_common.o rpu_err.o dolby_flowCtrl.o
#obj-y := dolby_flowCtrl.o edbec_api.o file_ops.o metadata_parser.o
obj-y += \
	   IDK_1_5/libs/vdr_rpu/src/crc32.o \
	   IDK_1_5/libs/vdr_rpu/src/dv_md_parser.o \
	   IDK_1_5/libs/vdr_rpu/src/rpu_common.o \
	   IDK_1_5/libs/vdr_rpu/src/rpu_decoder.o \
	   IDK_1_5/libs/vdr_rpu/src/rpu_err.o \
	   IDK_1_5/libs/vdr_rpu/src/rpu_parser.o \
	   IDK_1_5/libs/dm/dlb_dm_process/src/CdmCfgFxp.o \
	   IDK_1_5/libs/dm/dlb_dm_process/src/CdmCmmtFltFxp.o \
	   IDK_1_5/libs/dm/dlb_dm_process/src/CdmCmmtFxp.o \
	   IDK_1_5/libs/dm/dlb_dm_process/src/CdmMmg.o \
	   IDK_1_5/libs/dm/dlb_dm_process/src/CdmUtilFxp.o \
	   IDK_1_5/libs/dm/dlb_dm_process/src/CdmUtilFxp.o \
	   IDK_1_5/libs/dm/dlb_dm_process/src/dm2_x/Kdm3To2.o \
	   IDK_1_5/libs/dm/dlb_dm_process/src/tcgen/fx_basicops.o \
	   IDK_1_5/libs/dm/dlb_dm_process/src/tcgen/fx_math.o \
	   IDK_1_5/libs/dm/dlb_dm_process/src/tcgen/lc_2x1d_luts.o \
	   IDK_1_5/libs/dm/dlb_dm_process/src/tcgen/toneCurve2pxx.o \
	   IDK_1_5/libs/dm/dlb_dm_process/src/tcgen/toneCurve3pxx.o \
	   IDK_1_5/libs/dm/dlb_dm_process/src/tcgen/toneCurveInternalFxs.o \
	   control_path_api.o \
	   dolby_flowCtrl.o \
	   dovi_bec_api.o \
	   file_ops.o \
	   metadata_parser.o
