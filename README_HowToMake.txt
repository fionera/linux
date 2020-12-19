#========================================
# How to compile the Linux kernel/rtk_driver/common
#========================================

#----------------------------------------
@@. Elements in the package
#----------------------------------------
+ kernel-1.0.0-608.tar.gz
- README_HowToMake.txt : describe how to compile source codes using toolchain

#----------------------------------------
@@. The order to compile codes
#----------------------------------------
1)./starfish-sdk-i686-ca9v1-toolchain-5.0.0-20190510.sh
- Enter target directory for SDK (default: /opt/starfish-sdk-i686/5.0.0-20190510): /HOME/USERNAME/TOOLCHAIN
- You are about to install the SDK to "DIRECTORY". Proceed[Y/n]? y
- You need remember you toolchain install path.
2) export toolchain PATH:
- PATH=/HOME/USERNAME/TOOLCHAIN/sysroots/i686-starfishsdk-linux/usr/bin/arm-starfish-linux:$PATH
3) make config and make
- make realtek/config.develop.rtd288o.tv006.emmc.old_defconfig
- CROSS_COMPILE=arm-starfish-linux- make KALLSYMS_EXTRA_PASS=1

#----------------------------------------
@@. Output path and file
#----------------------------------------
- .:
-- vmlinux.bin : linux binary file