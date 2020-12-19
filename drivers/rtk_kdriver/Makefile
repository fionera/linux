###################################################################
# Setup Include Path
###################################################################
KBUILD_CPPFLAGS += -Idrivers/rtk_kdriver/include
KBUILD_CPPFLAGS += -Idrivers/rtk_kdriver/include/rtk_kdriver
KBUILD_CPPFLAGS += -Idrivers/rtk_kdriver/common/include
KBUILD_CPPFLAGS += -Idrivers/rtk_kdriver/common/include/rbus
KBUILD_CPPFLAGS += -Idrivers/rtk_kdriver
KBUILD_CPPFLAGS += -Idrivers/rtk_kdriver/common/IPC/generate/include/system
KBUILD_CPPFLAGS += -Idrivers/rtk_kdriver/common/IPC/include
KBUILD_CPPFLAGS += -Idrivers/rtk_kdriver/common
KBUILD_CPPFLAGS += -Idrivers/rtk_kdriver/common/linux
KBUILD_CPPFLAGS += -Idrivers/rtk_kdriver/common/linux/alsa-ext
KBUILD_CPPFLAGS += -Idrivers/rtk_kdriver/common/linux/dvbv5-ext
KBUILD_CPPFLAGS += -Idrivers/rtk_kdriver/common/linux/v4l2-ext
KBUILD_CPPFLAGS += -Idrivers/rtk_kdriver/gal
KBUILD_CPPFLAGS += -Idrivers/staging/android/ion

KBUILD_CPPFLAGS += -DLITTLE_ENDIAN
KBUILD_CFLAGS += -fno-unsigned-char

###################################################################
# Must have
###################################################################
obj-$(CONFIG_RTK_KDRIVER_SUPPORT) += crt/

###################################################################
# Generic drivers
###################################################################
#PCBMGR
obj-$(CONFIG_REALTEK_PCBMGR) += pcbmgr/

# RPC
ifeq ($(CONFIG_ARM64),y)
obj-$(CONFIG_RTK_KDRV_RPC) += RPC64/
else
obj-$(CONFIG_RTK_KDRV_RPC) += RPC.o
RPC-objs := RPCDriver.o RPCintr.o RPCpoll.o RPCkern.o RPClog.o
endif

# AVCPU
obj-$(CONFIG_RTK_KDRV_AVCPU) += rtk_avcpu.o

# SPDIF
obj-y+= rtk_spdif.o

# rtk_semaphore
obj-$(CONFIG_RTK_SEMAPHORE) += rtk_semaphore.o
obj-$(CONFIG_RTK_SEMAPHORE_TEST) += rtk_semaphore_test.o

# AMP init driver
# Please notice that this is only for REALTEK_AMP_TAS5711 & REALTEK_AMP_ACL1310
# If use different IC, please change this.
obj-$(CONFIG_RTK_KDRV_AMP) += amp/

# EMCU
ifeq (y,$(filter y, $(CONFIG_CUSTOMER_TV006)))
obj-$(CONFIG_RTK_KDRV_EMCU) += rtk_emcu_tv006.o
else
obj-$(CONFIG_RTK_KDRV_EMCU) += rtk_emcu.o
obj-$(CONFIG_RTK_KDRV_EMCU) += emcu/
endif

# RTD LOGBUF
obj-$(CONFIG_REALTEK_LOGBUF) += rtd_logger.o

# HW-MONITOR
obj-y += hw_monitor/

# SB2
ifneq (y,$(filter y, $(CONFIG_ARM64)))
obj-$(CONFIG_RTK_KDRV_SB2) += rtk_busto.o
endif

#DBG HELP
obj-$(CONFIG_RTK_ONLINE_HELP) += rtk_online_help.o

obj-y += platform/
# GPIO
obj-$(CONFIG_RTK_KDRV_GPIO) += gpio/

#obj-$(CONFIG_RTK_KDRV_GPIO) += lnbgpio/

# SWITCH
obj-$(CONFIG_RTK_KDRV_SWITCH) += switch/

# PWM
obj-$(CONFIG_RTK_KDRV_PWM) += pwm/

# Keypad Driver
obj-$(CONFIG_RTK_KDRV_KEYPAD) += keypad/

# PCMCIA
obj-$(CONFIG_RTK_KDRV_PCMCIA) += pcmcia/rtk_pcmcia.o

# SMARTCARD
obj-$(CONFIG_RTK_KDRV_SMARTCARD) += smartcard/

# CEC
#obj-$(CONFIG_RTK_KDRV_CEC) += cec/
obj-$(CONFIG_RTK_KDRV_CEC) += rtk_cec/

# DDC
obj-$(CONFIG_RTK_KDRV_DDC) += ddc/
# CP
obj-$(CONFIG_RTK_KDRV_MCP) += mcp/

# CW
obj-$(CONFIG_RTK_KDRV_CW) += cw/

# OTP
obj-$(CONFIG_RTK_KDRV_OTP) += otp/

# RNG
obj-$(CONFIG_RTK_KDRV_RNG) += rng/

# DV
ifeq (y,$(filter y, $(CONFIG_RTK_KDRV_DV)))
ifeq (y,$(filter y, $(CONFIG_DOLBY_IDK_1_6_1_1)))
obj-y += dolby_idk_1_6_1_1/
else
obj-y += dolby_IDK_1_5/
endif
endif
#obj-$(CONFIG_RTK_KDRV_DV) += dolby_idk_1_6_1_1/

#USBDUMP
obj-$(CONFIG_RTK_USBDUMP_ENABLE) += rtk_usbdump.o

# MD
obj-$(CONFIG_RTK_KDRV_MD) += md/

# RTC
obj-$(CONFIG_RTK_KDRV_RTC) += rtk_rtc.o

# Watch Dog
obj-$(CONFIG_RTK_KDRV_WATCHDOG) += rtk_watchdog.o

# Thermal Sensor
obj-$(CONFIG_RTK_KDRV_THERMAL_SENSOR) += thermal/

# CPU Freq
obj-$(CONFIG_RTK_KDRV_CPU_FREQUENCY) += rtk_clock.o
obj-$(CONFIG_CPU_FREQ) += rtk_cpufreq.o

# QoS
obj-$(CONFIG_RTK_KDRV_QOS_CONTROL) += rtk_qos.o

# HW Watchpoint
obj-$(CONFIG_RTK_KDRV_WATCHPOINT) += rtk_venus_watch.o

# Frame Buffer
obj-$(CONFIG_RTK_KDRV_VENUS_FB) += rtk_venusfb.o

# LSADC
obj-$(CONFIG_RTK_KDRV_LSADC) += lsadc/

# UART
obj-$(CONFIG_RTK_KDRV_SERIAL_8250) += uart/

# RTICE Driver
obj-$(CONFIG_RTK_KDRV_RTICE) += rtice/

# IRDA Driver
obj-$(CONFIG_RTK_KDRV_IR) += ir/

#IRTX Driver
obj-$(CONFIG_RTK_KDRV_IRTX) += irtx/

# VGIP ISR Driver
obj-$(CONFIG_SUPPORT_SCALER) += rtk_vgip_isr.o
obj-$(CONFIG_SUPPORT_SCALER) += vgip_isr/

# DDOMAIN ISR Driver
obj-$(CONFIG_SUPPORT_SCALER) += rtk_ddomain_isr.o

# PIF ISR Driver
obj-$(CONFIG_SUPPORT_SCALER) += rtk_pif_isr.o

# VO ISR Driver
obj-$(CONFIG_RTK_KDEV_VO_INTERRUPT) += rtk_vo_isr.o

# MEMC ISR Driver
obj-$(CONFIG_SUPPORT_SCALER) += rtk_memc_isr.o
obj-$(CONFIG_SUPPORT_SCALER) += memc_isr/

# SCALER
obj-$(CONFIG_SUPPORT_SCALER) += tvscalercontrol/

# HDMI RX
obj-$(CONFIG_RTK_HDMI_RX) += tvscalercontrol/hdmirx/

# LG BOARD
obj-$(CONFIG_RTK_KDRV_LG_BOARD) += rtk_lg_board.o

# LNB Uart patch
#obj-$(CONFIG_RTK_KDRV_LNB_UART) += lnbuart/

# Pinmux Driver
obj-$(CONFIG_RTK_KDRV_PINMUX) += rtk_pinmux.o

# GAL Driver
obj-$(CONFIG_RTK_KDRV_SE) += gal/
obj-$(CONFIG_RTK_KDRV_GDMA) += gal/
obj-$(CONFIG_CUSTOMER_TV006) += gal/

# Misc Driver
obj-$(CONFIG_RTK_KDRV_MISC_DEV) += misc.o
# MMC
ifeq (y,$(filter y, $(CONFIG_ARCH_RTK284X) $(CONFIG_ARCH_RTK287X) $(CONFIG_ARCH_RTK288O)))
obj-$(CONFIG_MMC_RTKEMMC_PLUS) += mmc/
endif

# I2C Driver
obj-$(CONFIG_RTK_KDRV_I2C) += i2c/

# Audio HW Driver
obj-$(CONFIG_RTK_KDRV_AUDIO_HW) += audio_hw/

# ETN Driver
obj-$(CONFIG_RTK_KDRV_R8168) += rtk_8168.o


# DDR DM scan
obj-$(CONFIG_RTK_DRV_DDR_SCAN) += rtk_scan.o

# USB host controller
obj-$(CONFIG_RTK_KDRV_USB) += usb/

###################################################################
# DTV Related drivers
###################################################################

#DEMOD Kernel driver
obj-$(CONFIG_RTK_KDRV_DEMOD) += demod/
obj-$(CONFIG_RTK_KDRV_KHAL_DEMOD) += demod/

#TP Kernel driver
obj-$(CONFIG_RTK_KDRV_TP) += tp/

#ATSC3 DMX
obj-$(CONFIG_RTK_KDRV_ATSC3_DMX) += atsc3_dmx/
obj-$(CONFIG_RTK_KDRV_ATSC3_DMX) += delivery/

#JAPAN4k DMX
obj-$(CONFIG_RTK_KDRV_ATSC3_DMX) += japan4k_dmx/

###################################################################
# HAL Related drivers
###################################################################

# Demux HAL Driver
#ifeq (y,$(filter y, $(CONFIG_CUSTOMER_TV006)))
#obj-$(CONFIG_RTK_KDRV_DEMUX) += demux/
#else
#obj-$(CONFIG_RTK_KDRV_DEMUX) += demux_tv001/
#endif


obj-$(CONFIG_RTK_KDRV_DEMUX) += rdvb/

obj-$(CONFIG_RTK_KDRV_AUDIO) += audio/
obj-$(CONFIG_RTK_KDRV_VDEC) += vdec/
obj-$(CONFIG_RTK_KDRV_VUTILS) += vutils/
obj-$(CONFIG_RTK_KDRV_V4L2_VCODEC) += v4l2_vcodec/

obj-$(CONFIG_RTK_KDRV_TEEPROXY) += teeproxy/
obj-$(CONFIG_RTK_KDRV_DVO) += directvo/
obj-$(CONFIG_RTK_KDRV_DMABUF) += dmabuf/

# SPI
obj-$(CONFIG_RTK_KDRV_SPI) += spi/

# SPI_MASTER_GENERAL
obj-$(CONFIG_RTK_KDRV_SPI_MASTER_GENERAL) += spi_dev/

# LGTV related
obj-$(CONFIG_LGTV_DRIVER_PROCFS_SUPPORT) += lgtv_driver_proc/

KBUILD_CFLAGS += -Werror


