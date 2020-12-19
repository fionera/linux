
ccflags-y += -Idrivers/rtk_kdriver/include
ccflags-y += -Idrivers/rtk_kdriver/common/include
ccflags-y += -Idrivers/rtk_kdriver/common/IPC/include
ccflags-y += -Idrivers/rtk_kdriver/common/IPC/generate/include/system
ccflags-y += -Werror

vutils-objs := \
	vfifo.o \
	vrpc.o \
	vutils_drv.o \
	vdma_carveout.o \

obj-$(CONFIG_RTK_KDRV_VUTILS) = vutils.o
