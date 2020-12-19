KERNEL_DIR = $(shell ./env.py -q kernel_dir)
CROSS = $(shell ./env.py -q cross_compile)
SYSROOT= $(shell ./env.py -q sysroot)
COMMON = $(KERNEL_DIR)/drivers/rtk_kdriver/common
CFLAGS = -Wall --sysroot=$(SYSROOT)
INCS = -I$(COMMON)/IPC/include -I$(COMMON)/IPC/generate/include/system -I$(COMMON)/include/ioctrl -I./
LIBS = -lpthread
TARGET = rtkdmaUserTest

all: options $(TARGET)

options: rtkdma.h

rtkdmaUserTest: rtkdmaUserTest.o syswrap.o ringbuf.o
	$(CROSS)gcc $(CFLAGS) $(LIBS) -o $@ $^

%.o: %.c
	$(CROSS)gcc $(CFLAGS) $(INCS) -c $<

clean:
	rm -f *.o $(TARGET)
