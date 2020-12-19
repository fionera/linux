#
# Makefile for the linux kernel.
#

obj-y				+= mach.o
obj-y 				+= firmware.o
obj-$(CONFIG_SMP)		+= smp.o headsmp.o
obj-$(CONFIG_CACHE_L2X0)	+= l2cc.o
obj-y				+= smc.o headsmc.o
obj-$(CONFIG_SERIAL_AMBA_PL011)	+= uart.o
obj-$(CONFIG_PM)		+= pm.o
obj-$(CONFIG_ION_LG115X)	+= ion.o
obj-y				+= ksyms.o
obj-${CONFIG_HOTPLUG_CPU}	+= pms.o
obj-y				+= sram.o
obj-y				+= devmap.o
