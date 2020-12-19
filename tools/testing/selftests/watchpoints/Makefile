# Taken from perf makefile
uname_M := $(shell uname -m 2>/dev/null || echo not)
#ARCH ?= $(shell echo $(uname_M) | sed -e s/i.86/i386/)
ifeq ($(ARCH),i386)
        ARCH := x86
endif
ifeq ($(ARCH),x86_64)
	ARCH := x86
endif

export BASE_DIR=../../../../../../system/tmp/toolchain/starfish-sdk-i686_4.0.0
export SDKTARGETSYSROOT=$(BASE_DIR)/sysroots/ca9v1-starfish-linux-gnueabi
export CFLAGS=--sysroot="$(SDKTARGETSYSROOT)" 

all:
ifeq ($(ARCH),x86)
	gcc watchpoint_test.c -o watchpoint_test
else
	$(BASE_DIR)/sysroots/i686-starfishsdk-linux/usr/bin/arm-starfish-linux/arm-starfish-linux-gcc -g $(CFLAGS) watchpoint_test.c -o watchpoint_test
#	../../../../../../system/tmp/toolchain/arm-lg115x-linux-gnueabi-4.8-2014.11-i686/bin/arm-linux-gcc -g watchpoint_test.c -o watchpoint_test
#	echo "Not an x86 target, can't build breakpoints selftests"
endif

#run_tests:
#	./watchpoint_test

clean:
	rm -fr watchpoint_test
