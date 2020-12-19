ccflags-y += -Idrivers/rtk_kdriver
ccflags-y += -Idrivers/rtk_kdriver/include
ccflags-y += -Idrivers/rtk_kdriver/common/include
ccflags-y += -Idrivers/rtk_kdriver/common/IPC/include
ccflags-y += -Idrivers/rtk_kdriver/common/IPC/generate/include/system
ccflags-y += -Idrivers/rtk_kdriver/common
ccflags-y += -Werror

v4l2_vcodec-objs := \
	vcodec_drv.o \
	vcodec_utils.o \
	videobuf2-dma-carveout.o \
	vdec_ops.o \
	vdec_core.o \
	vdec_core_vdo.o \
	vdec_out.o \
	vdec_cap.o \
	vdec_attrs.o \
	vdo_ops.o \
	vcap_ops.o \
	vcap_core.o \
	venc_core.o \
	venc_ops.o \
	venc_test.o \
	vdogav_ops.o \

obj-$(CONFIG_RTK_KDRV_V4L2_VCODEC) += v4l2_vcodec.o

