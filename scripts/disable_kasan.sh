#!/bin/bash

echo "#!/bin/sh" > ./a.sh

#find makefile and echo not do kasan makefile into sh file
find ./ -name Makefile | awk -F '.' '{print "echo KASAN_SANITIZE := n > ./tmp";print "cat ."$2" >> tmp";print "cp ./tmp ."$2;}' >> ./a.sh

chmod u+x ./a.sh
./a.sh


find crypto/ -name Makefile | xargs git checkout
find drivers/video/backlight/ -name Makefile | xargs git checkout
find drivers/net/wireless/realtek/rtlwifi -name Makefile | xargs git checkout
find drivers/usb/host -name Makefile | xargs git checkout
find drivers/media/usb/gspca -name Makefile | xargs git checkout
find drivers/media/usb/gspca -name Makefile | xargs git checkout
find drivers/bluetooth -name Makefile | xargs git checkout


# add driver folder that you want to check it by kasan
#find drivers/thermal/ -name Makefile | xargs git checkout
#find drivers/usb/host/ -name Makefile | xargs git checkout
#find drivers/mmc/host/ -name Makefile | xargs git checkout
#find drivers/i2c/busses/ -name Makefile | xargs git checkout
#find drivers/net/ethernet/realtek/ -name Makefile | xargs git checkout

# add kdriver folder that you want to check it by kasan
cd drivers/rtk_kdriver
git reset --hard
cd - 

