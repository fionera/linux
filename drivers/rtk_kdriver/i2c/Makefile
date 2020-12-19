#
# Makefile for the i2c core.
#

#kdriver support
KBUILD_CFLAGS += -Werror
ifdef CONFIG_RTK_KDRIVER_SUPPORT
KBUILD_CPPFLAGS += -Idrivers/rtk_kdriver/include
endif
obj-y				+= algos/ busses/ 

ccflags-$(CONFIG_I2C_DEBUG_CORE) := -DDEBUG


