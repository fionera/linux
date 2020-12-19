#!/bin/bash

PRJ_SYSTEM_PATH=`pwd`/../../../../../../../system/
KDIR=$PRJ_SYSTEM_PATH/../linux/linux-4.4.3
CROSS_COMPILE_PATH=$PRJ_SYSTEM_PATH/tmp/toolchain/x86_64-starfishsdk-linux_5.0.0/sysroots/x86_64-starfishsdk-linux/usr/bin/arm-starfish-linux/
CROSS_COMPILE=$CROSS_COMPILE_PATH/arm-starfish-linux-

#CROSS_COMPILE_PATH=$PRJ_SYSTEM_PATH/tmp/toolchain/starfish-sdk-x86_64_4.5.0-20180321/sysroots/x86_64-starfishsdk-linux/usr/bin/aarch64-starfish-linux/
#CROSS_COMPILE=$CROSS_COMPILE_PATH/aarch64-starfish-linux-

make -C $KDIR M=`pwd` ARCH=arm CROSS_COMPILE=$CROSS_COMPILE modules V=1
#make -C $KDIR M=`pwd` ARCH=arm64 CROSS_COMPILE=$CROSS_COMPILE modules V=1

