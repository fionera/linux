KBUILD_CFLAGS += -Werror

KBUILD_CPPFLAGS += -Idrivers/rtk_kdriver/audio
KBUILD_CPPFLAGS += -Idrivers/rtk_kdriver/audio_hw
KBUILD_CPPFLAGS += -Idrivers/rtk_kdriver/include/rtk_kdriver/hal_inc

obj-y += rtkaudio_driver.o rtkaudio_debug.o
obj-y += khal_audio.o khal_audio_sound.o kadp_audio.o
obj-y += audio_base.o audio_common.o audio_pin.o audio_refclock.o audio_util.o audio_memout.o
obj-y += audio_in.o audio_out.o audio_flow.o audio_dec.o audio_enc.o audio_dtvcom.o audio_esplay.o
obj-y += audio_process.o audio_process_node.o audio_manager_process_node.o resample_coef_system.o
obj-y += audio_rpc.o
obj-y += audio_voice.o

DEFS = -DUSE_INBAND_SYSPROC_COMMUNICATION

KBUILD_CFLAGS += $(DEFS)
