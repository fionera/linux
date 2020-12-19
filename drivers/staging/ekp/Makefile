# Makefile for Enhanced Kernel Protection (EKP)

KASAN_SANITIZE := n

firmware := $(subst $(quote),,$(CONFIG_EKP_HISE_FIRMWARE))
ifeq ($(wildcard $(firmware)),)
firmware :=
endif
asflags-y += -DEKP_USE_FW=$(if $(firmware),1,0)

obj-y					+= ekp.o ekp-hise.o
obj-$(CONFIG_SECURITY_EKP)		+= ekp_lsm.o
obj-$(CONFIG_EKP_DEBUGFS_TEST_HELPERS)	+= ekp_test_helpers.o
obj-$(CONFIG_EKP_ROMEMPOOL)		+= romempool.o

$(obj)/ekp-hise.o: $(firmware) FORCE
