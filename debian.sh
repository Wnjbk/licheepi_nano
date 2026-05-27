#!/bin/bash

# --- 配置区域：请根据你的实际文件路径修改 ---
_UBOOT_SIZE=1                 # U-Boot占用大小（通常不用改）
_P1_SIZE=16                   # 第一个分区(FAT)大小，存放内核和设备树，单位MiB
_IMG_SIZE=1024                 # 最终镜像文件大小，需大于_rootfs的大小+40M，单位MiB
_IMG_FILE='f1c100s_sdcard.img'
# 输出的镜像文件名

# 你的所有原材料文件路径
_UBOOT_FILE="u-boot/u-boot-sunxi-with-spl.bin"
_KERNEL_IMAGE_FILE="linux/arch/arm/boot/zImage"
_DTB_FILE="linux/arch/arm/boot/dts/suniv-f1c100s-licheepi-nano.dtb"
_BOOT_SCR_FILE="boot.scr"          # 启动脚本 (可选，但建议有)
_ROOTFS_TGZ_FILE="debian_bookworm_rootfs.tar.gz"   # 根文件系统的压缩包
# --- 配置结束 ---

# 1. 创建一个空的镜像文件
dd if=/dev/zero of=$_IMG_FILE bs=1M count=$_IMG_SIZE

# 2. 将空镜像文件挂载为循环设备
LOOP_DEV=$(sudo losetup -f)
sudo losetup $LOOP_DEV $_IMG_FILE

# 3. 对镜像文件进行分区 (sfdisk命令)
#    创建一个主分区（类型c，即FAT32），剩余空间给第二个分区
cat <<EOT | sudo sfdisk ${_IMG_FILE}
${_UBOOT_SIZE}M,${_P1_SIZE}M,c
,,L
EOT

# 4. 让内核重新读取分区表，并格式化分区
sudo partx -u $LOOP_DEV
sudo mkfs.vfat ${LOOP_DEV}p1          # 格式化第一个分区为 FAT32
sudo mkfs.ext4 ${LOOP_DEV}p2          # 格式化第二个分区为 EXT4

# 5. 将U-Boot写入到镜像的8KB偏移处（不占用分区）
sudo dd if=$_UBOOT_FILE of=$LOOP_DEV bs=1024 seek=8 conv=notrunc

# 6. 挂载两个分区，并拷贝文件
mkdir -p p1 p2
sudo mount ${LOOP_DEV}p1 p1
sudo mount ${LOOP_DEV}p2 p2

# 拷贝内核、设备树、启动脚本到第一分区
sudo cp $_KERNEL_IMAGE_FILE p1/zImage
sudo cp $_DTB_FILE p1/
sudo cp $_BOOT_SCR_FILE p1/     # 如果不需要boot.scr可以注释掉

# 将根文件系统解压到第二分区
sudo tar -xzf $_ROOTFS_TGZ_FILE -C p2/
sudo cp test.wav p2/
sudo cp ~/SoftWare/ESP8089-SPI-master/esp8089-spi.ko p2/lib
# 进入内核源码目录（请根据实际情况修改路径）
KERNEL_SRC="./linux"   # 假设内核源码在当前目录的 linux 子目录下
if [ -d "$KERNEL_SRC" ]; then
    echo "编译内核模块..."
    make -C $KERNEL_SRC ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- modules

    echo "安装内核模块到根文件系统..."
    sudo make -C $KERNEL_SRC ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- modules_install INSTALL_MOD_PATH=$(pwd)/p2

    echo "更新模块依赖..."
    # 获取实际安装的模块目录名（第一个非空目录）
	MODULE_DIR=$(ls $(pwd)/p2/lib/modules/ 2>/dev/null | head -1)
	if [ -n "$MODULE_DIR" ]; then
    	echo "检测到模块目录: $MODULE_DIR，更新依赖..."
    	sudo depmod -b $(pwd)/p2 $MODULE_DIR
	else
    	echo "警告：未找到任何模块目录，可能模块安装失败"
	fi
else
    echo "警告：内核源码目录 $KERNEL_SRC 不存在，跳过模块安装"
fi

# 7. 清理并卸载
sudo umount p1 p2
sudo losetup -d $LOOP_DEV
rmdir p1 p2

echo "镜像文件 $_IMG_FILE 生成完毕！"
