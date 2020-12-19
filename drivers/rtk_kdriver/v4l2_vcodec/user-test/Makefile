ENV_RESULT := $(shell ./env.py -o env.mk)
include env.mk

COMMON = $(KERNEL_DIR)/drivers/rtk_kdriver/common
CFLAGS = -Werror -Wall --sysroot=$(SYSROOT)
LDFLAGS = --sysroot=$(SYSROOT)
INCS = -I$(COMMON)/IPC/include -I$(COMMON)/IPC/generate/include/system -I$(COMMON)
LIBS = -lpthread
VDEC_OBJS = vdec-user-test.o syswrap.o ringbuf.o
VENC_OBJS = venc-user-test.o syswrap.o ringbuf.o
VSC_OBJS = vsc-setup.o
TARGET = vdec-user-test vsc-setup venc-user-test

include $(filter-out env.mk, $(wildcard *.mk))

all: options $(TARGET)

options: options_vdec.h options_vsc.h

options_vdec.h: options.py options_vdec.xml
	./options.py -i options_vdec.xml -o options_vdec.h

options_vsc.h: options.py options_vsc.xml
	./options.py -i options_vsc.xml -o options_vsc.h

vdec-user-test: $(VDEC_OBJS)
	$(CROSS_COMPILE)gcc $(LDFLAGS) $(LIBS) -o $@ $^

venc-user-test: $(VENC_OBJS)
	$(CROSS_COMPILE)gcc $(LDFLAGS) $(LIBS) -o $@ $^

vsc-setup: $(VSC_OBJS)
	$(CROSS_COMPILE)gcc $(LDFLAGS) $(LIBS) -o $@ $^

%.o: %.c
	$(CROSS_COMPILE)gcc $(CFLAGS) $(INCS) -c $<

clean:
	rm -f *.o $(TARGET) env.mk
