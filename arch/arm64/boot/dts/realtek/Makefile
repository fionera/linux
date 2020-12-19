dtb-$(CONFIG_ARCH_RTK287O) += rtd287o.dtb

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)

targets += dtbs
targets += $(dtb-y)

dtbs: $(addprefix $(obj)/, $(dtb-y))

embedded_dtb: $(addprefix $(obj)/, $(dtb-y))
ifneq ($(CONFIG_BUILD_ARM_APPENDED_DTB_IMAGE_NAMES_EXT),)
	$(Q)cp $(obj)/$(CONFIG_BUILD_ARM_APPENDED_DTB_IMAGE_NAMES_EXT) embedded_dtb
endif

clean-files	:= *.dtb
