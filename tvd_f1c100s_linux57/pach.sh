#!/bin/bash
set -e

# ============================================================
# 配置区域 - 请根据实际路径修改
# ============================================================
_UBOOT_SIZE=1
_P1_SIZE=16
_IMG_SIZE=512
_IMG_FILE='f1c100s_sdcard_rootfs.img'

_UBOOT_FILE="u-boot/u-boot-sunxi-with-spl.bin"
_KERNEL_IMAGE_FILE="linux/arch/arm/boot/zImage"
_DTB_FILE="linux/arch/arm/boot/dts/suniv-f1c100s-licheepi-nano.dtb"
_BOOT_SCR_FILE="boot.scr"
_ROOTFS_TGZ_FILE="buildroot-2018.02.11/output/images/rootfs.tar"

_ESP8089_KO_FILE="/home/wnk/SoftWare/ESP8089-SPI-master/esp8089-spi.ko"
_WIFI_SSID="wnl64"
_WIFI_PSK="297475wnl"

# ============================================================
# 临时 overlay 目录
# ============================================================
OVERLAY_DIR="./overlay_tmp"
rm -rf "$OVERLAY_DIR"
mkdir -p "$OVERLAY_DIR/lib/modules"
mkdir -p "$OVERLAY_DIR/etc/wpa_supplicant"
mkdir -p "$OVERLAY_DIR/lib"

# ============================================================
# 这里故意不再改 rcS
# 原因：之前注入的 USB gadget 初始化会触发 g_webcam 抢占 UDC 并导致启动崩溃
# ============================================================

# ----- WiFi 驱动 -----
if [ -f "$_ESP8089_KO_FILE" ]; then
    cp "$_ESP8089_KO_FILE" "$OVERLAY_DIR/lib/modules/"
fi

# ----- wpa_supplicant.conf -----
cat > "$OVERLAY_DIR/etc/wpa_supplicant.conf" << EOF
ctrl_interface=/var/run/wpa_supplicant
update_config=1

network={
    ssid="${_WIFI_SSID}"
    psk="${_WIFI_PSK}"
}
EOF

# ----- asound.conf -----
cat > "$OVERLAY_DIR/etc/asound.conf" << EOF
defaults.ctl.card 1
defaults.pcm.card 1
defaults.timer.card 1
EOF

# ----- 额外文件 -----
for f in bad.mkv bad.mp4 bad128.mp4 ss.nes InfoNES; do
    [ -f "$f" ] && cp "$f" "$OVERLAY_DIR/lib/"
done

echo "Overlay 准备完成。"

# ============================================================
# 制作镜像
# ============================================================
echo "创建空镜像文件 $_IMG_FILE ..."
dd if=/dev/zero of="$_IMG_FILE" bs=1M count=$_IMG_SIZE

LOOP_DEV=$(sudo losetup -f)
sudo losetup "$LOOP_DEV" "$_IMG_FILE"

echo "分区..."
echo -e "${_UBOOT_SIZE}M,${_P1_SIZE}M,c\n,,L" | sudo sfdisk "$_IMG_FILE"

echo "格式化..."
sudo partx -u "$LOOP_DEV"
sudo mkfs.vfat "${LOOP_DEV}p1"
sudo mkfs.ext4 "${LOOP_DEV}p2"

echo "写入 U-Boot..."
sudo dd if="$_UBOOT_FILE" of="$LOOP_DEV" bs=1024 seek=8 conv=notrunc

echo "挂载分区..."
mkdir -p p1 p2
sudo mount "${LOOP_DEV}p1" p1
sudo mount "${LOOP_DEV}p2" p2

sudo cp "$_KERNEL_IMAGE_FILE" p1/zImage
sudo cp "$_DTB_FILE" p1/
[ -f "$_BOOT_SCR_FILE" ] && sudo cp "$_BOOT_SCR_FILE" p1/

echo "解压根文件系统到第二分区..."
sudo tar -xf "$_ROOTFS_TGZ_FILE" -C p2/

echo "应用 overlay 配置..."
sudo cp -a "$OVERLAY_DIR"/. p2/

# 内核模块编译安装
KERNEL_SRC="./linux"
if [ -d "$KERNEL_SRC" ]; then
    echo "编译内核模块..."
    make -C "$KERNEL_SRC" ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- modules
    sudo make -C "$KERNEL_SRC" ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- modules_install INSTALL_MOD_PATH=$(pwd)/p2
    MODULE_DIR=$(ls $(pwd)/p2/lib/modules/ 2>/dev/null | grep -E '^[0-9]' | head -1)
    [ -n "$MODULE_DIR" ] && sudo depmod -b $(pwd)/p2 "$MODULE_DIR"
fi

echo "清理..."
sudo umount p1 p2
sudo losetup -d "$LOOP_DEV"
rmdir p1 p2
rm -rf "$OVERLAY_DIR"

echo "完成！镜像：$_IMG_FILE"
echo "这个版本不会自动启动 USB gadget，优先保证系统稳定启动。"
