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
mkdir -p "$OVERLAY_DIR/etc/init.d"
mkdir -p "$OVERLAY_DIR/lib/modules"
mkdir -p "$OVERLAY_DIR/etc/wpa_supplicant"
mkdir -p "$OVERLAY_DIR/lib"

# ----- 提取原始 rcS（需要 sudo 因为 rootfs.tar 内含设备节点）-----
TMP_ROOTFS="./tmp_rootfs_extract"
rm -rf "$TMP_ROOTFS"
mkdir -p "$TMP_ROOTFS"

echo "提取原始 rcS 脚本..."
sudo tar -xf "$_ROOTFS_TGZ_FILE" -C "$TMP_ROOTFS"

ORIGINAL_RCS="$TMP_ROOTFS/etc/init.d/rcS"
if [ ! -f "$ORIGINAL_RCS" ]; then
    echo "错误：找不到 $ORIGINAL_RCS"
    exit 1
fi

# ----- 生成新的 rcS（头部插入网络初始化）-----
# ★★★ 关键修改：IP 改为 192.168.0.2，并加入网关和 DNS 配置 ★★★
cat > "$OVERLAY_DIR/etc/init.d/rcS" << 'EOF'
#!/bin/sh

# ===== USB RNDIS 自动初始化 + 共享上网配置 =====
echo "### Starting USB Ethernet Gadget ###"
modprobe sunxi 2>/dev/null || true
modprobe g_ether 2>/dev/null || true

# 等待 usb0 出现
for i in 1 2 3 4 5; do
    [ -d /sys/class/net/usb0 ] && break
    sleep 1
done

# 配置静态 IP（改为 192.168.0.2）
ifconfig usb0 192.168.0.2 netmask 255.255.255.0 up 2>/dev/null || \
ip addr add 192.168.0.2/24 dev usb0 2>/dev/null
ip link set usb0 up 2>/dev/null

# 设置默认网关（指向电脑的 RNDIS 地址）
route del default 2>/dev/null
route add default gw 192.168.0.1 2>/dev/null

# 设置 DNS（确保域名解析）
echo "nameserver 8.8.8.8" > /etc/resolv.conf
echo "nameserver 114.114.114.114" >> /etc/resolv.conf

echo "### usb0 ready: 192.168.0.2, gateway 192.168.0.1 ###"
# ===== 初始化结束 =====

EOF

# 追加原始 rcS 内容（去掉原有的 shebang 行）
tail -n +2 "$ORIGINAL_RCS" >> "$OVERLAY_DIR/etc/init.d/rcS"
chmod +x "$OVERLAY_DIR/etc/init.d/rcS"

# 清理临时 rootfs
sudo rm -rf "$TMP_ROOTFS"

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
echo "烧录后连接 USB，板子 IP: 192.168.0.2，网关: 192.168.0.1（电脑）"
echo "SSH 登录: ssh root@192.168.0.2"
echo "板子已配置 DNS，可直接访问外网。"
