# Formatting SD Card

### See if SD-CARD mounted:

```
lsblk
```

### See space on SD-Card

```
df -h
```

This can be done using the `gparted` application. Open gparted and delete all
partitions on the SD-Card. If you do not have write permissions to do this,
you must unmount the card:

```
umount /dev/mmcblk0
umount /dev/mmcblk0p1
umount /dev/mmcblk0p2
```

1. Using `gparted`, creat a parition for BOOT, select min space possible
leading, approx 256 mb for partition and vfat. Give it a `BOOT` label.
Right click on partition, set flags: `boot` and `lda`.

2. Create another partition of type ext4 for rootfs. Call it ROOTFS. 
Partition comes after boot partition and should take up all remaining 
space.

## Migrating local .img file to SD-Card rootfs partition

Locate .img file and right click on it and select "Open with another app".
Select the "Disk Image Mounter" option. At this point it should have
mounted your image on a loop mount under `/dev/media/rootfs`. Copy the 
entire contents of the .img mounted folder onto rootfs partition of 
memory card. In my case:

```
cd /media/kieranmc/rootfs
cp -a * /media/kieranmc/ROOTFS
```

## Setting up UART 

UART cables can be plugged into J1 male pins on BBB.
Starting from pin 0 (where J1 is), it goes as below from perspective of
adapter (host machine).

|  0   |  1  |  2  |  3  |  4  |  5  |
  GND     _     _    TX     RX   _

UART hardware settings:

/dev/ttyUSB
115200 8N1
Hardware Flow Control: No
Softwre Flow Control: No

Start up monitoring UART traffic:

```
python3 -m serial.tools.miniterm /dev/ttyUSB0 115200 --eol lf --filter colorize
```

If you have permission issues, you may have to do following:

```
sudo chmod 777 /dev/ttyUSB0
```

## Boot Sequence of BeagleBone Black

BBB has 2 different boot sequences. The 2 different boot sequences can be
toggled once the board has been started up by pressing the S2 button 
(not the one that says `power` or `reset`). Pressing S2 changes bits in
SYSBOOT register to select boot sequence.

Boot sequence outlines in what order it attempts to boot an image from.

### Normal boot sequence:

1. MMC1 (eMMC)
2. MMC0 (uSD)
3. UART0
4. USB0

### Alternate boot sequence:

1. SPI0
2. MMC0
3. USB0
4. UART0 

## Booting via SD-Card

1. Make sure board powered off
2. Insert uSD card into slot on board
3. Plug BB in to give power
4. Press and hold S2 to change boot sequence to alternative seq (read above).
Do step 5 with S2 still puhed down.
5. Press and hold power button (S3) until blue LED turns off and on.
6. Release S2 button once start up done.

Note: On BBB, mmcblk1 is the eMMC (embedded multi-media card). mmcblk0 will
be uSD.

## Compiling Linux Kernel

```
cd source/linux

# Clean
make ARCH=arm distclean

# Creates .config by usind default from vendor
make ARCH=arm bb.org_defconfig

# (Optional) Run menuconfig to modify configs
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- menuconfig

# Compilation: Creates kernel image "uImage"
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- uImage dtbs LOADADDR=0x80008000 -j4

## If your recv an error about /bin/sh: 1: lz4c: not found. Run again with gzip compress
scripts/config --disable KERNEL_LZ4
scripts/config --enable KERNEL_GZIP

# Build and generate the in-tree loadable kernel (.ko) modules
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- modules -j4

# Installs all the generated .ko fies to default path of computer (/lib/modules/<kernel_ver>)
sudo make ARCH=arm modules_install 
```

## Updating Kernel Images and Kernel Modules

1. Copy `uImage` to board and update boot partition of SD card
2. Copy `am335x-boneblack.dtb` to board and then update boot partition of SD card
3. Copy newly installed 5.10.168 folder to board's `/lib/modules/` folder
4. Reset the board for updated kernel
5. Plug in SD Card to hostmachine, under the linux dir, copy:

```
cp arch/arm/boot/uImage /media/kieranmc/BOOT/
cp arch/arm/boot/dts/am335x-boneblack.dtb /media/kieranmc/BOOT/
cd /lib/modules
sudo cp -a 5.10.168/ /media/kieranmc/ROOTFS/lib/modules/
```

## Providing Internet to BBB over USB to host

### Target Settings

1. Add name server address in `/etc/resolv.conf`
	- nameserver 8.8.8.8
	- nameserver 8.8.4.4
2. Add default gateway address by running the command below:
```
# Uses PC as default gateway
route add default gw 192.168.7.1
```

## Host Settings

1. Enable packet forwarding for IPv4 in `/etc/sysctl.conf` by uncommenting
or adding `net.ipv4.ip_forward=1`

2. Run commands to forward BBB IPv4 traffic (every time hostmachine reboots)

```
sudo iptables --table nat --append POSTROUTING --out-interface wlp2s0 -j MASQUERADE
sudo iptables --append FORWARD --in-interface wlp2s0 -j ACCEPT
sudo echo 1 > /proc/sys/net/ipv4/ip_forward
```

## Building Kernel Modules

Kernel modules can be build statically (compiled and linked into kernel), or
dynamically (to be loaded at runtime). If building dynamically, they can be
built 1 of 2 ways. "In tree" meaning the kernel module (ko) is in the main
linux kernel source tree and approved by maintainers. Or out of tree meaning
the opposite, built by unverified source. Out of tree will throw kernel
warning.

### Building Out of Tree KO

When building, you must have a version of kernel built in your workspace so
ko has symbols to link to. You need to build kernel module by invoking
Makefile of source of linux dir, and provide a path to your module to be
compiled as part of build. Command below:

```
make -C <path to linux kernel tree> M=<path to your module> [target(s)]
```

For `[target(s)]`, you can pick one of the following:
1. `modules`: Default target for external modules
2. `modules_install`: Install the external module(s). Default location is
/lib/modules/<kernel_release>/extra/
3. `clean`: Remove all generated files in the module dir only
4. `help`: List all available targets for external modules

For cross compiling ko:

```
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -C /home/kieranmc/git/beaglebone-linux-drivers/source/linux/ M=$PWD modules
```

You can build kernel module for your host machine by providing path to your current
linux build.

```
# Get current build
uname -r

# Above will return kernel ver (6.8.0-57-generic), provide that as your root Makefile
make -C /lib/modules/6.8.0-57-generic/build/ M=$PWD modules
```

### Kernel Module Load/Unload/Info

```
# Load kernel module
sudo insmod <module name>

# Unload kernel module
sudo rmmod <module name>

# Get module info
modinfo <module name>
```

