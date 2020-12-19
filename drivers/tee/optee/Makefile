obj-$(CONFIG_OPTEE) += rtk_optee.o
obj-$(CONFIG_OPTEE) += optee.o
optee-objs += core.o
optee-objs += call.o
optee-objs += rpc.o
optee-objs += supp.o
optee-objs += optee_debugfs.o
obj-$(CONFIG_OPTEE_SUPPORT_MC_ALLOCATOR) += optee_mc_pgtable.o

ccflags-y += -include $(TOP_DIR_OPTEE_DRIVER)/rtk_conf.h
ccflags-y += -include $(TOP_DIR_OPTEE_DRIVER)/tee_private.h
