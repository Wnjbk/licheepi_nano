#!/bin/sh
set -e

TARGET_DIR="$1"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BASE_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
LIBCEDAR_DIR="$BASE_DIR/output/build/libcedarc-master"
TARGET_LIB_DIR="$TARGET_DIR/usr/lib"

rm -f "$TARGET_DIR/etc/init.d/S40network"
rm -f "$TARGET_DIR/etc/init.d/S41dhcpcd"
rm -f "$TARGET_DIR/etc/init.d/S41usb-wifi"
rm -f "$TARGET_DIR/etc/init.d/S60audio-init"

mkdir -p "$TARGET_DIR/etc/xdg" "$TARGET_DIR/root" "$TARGET_LIB_DIR"

cat > "$TARGET_DIR/etc/xdg/gstomx.conf" <<'CONFEOF'
[omxh264dec]
type-name=GstOMXH264Dec
core-name=/usr/lib/libOmxCore.so
component-name=OMX.allwinner.video.decoder.avc
rank=257
in-port-index=0
out-port-index=1
hacks=no-disable-outport;no-empty-eos-buffer
CONFEOF

printf '/usr/lib/libOmxCore.so\n' > "$TARGET_DIR/root/.omxregister"

cp -f "$LIBCEDAR_DIR/vdecoder/.libs/libvdecoder.so" "$TARGET_LIB_DIR/"
cp -f "$LIBCEDAR_DIR/openmax/omxcore/.libs/libOmxCore.so" "$TARGET_LIB_DIR/"
cp -f "$LIBCEDAR_DIR/openmax/vdec/.libs/libOmxVdec.so" "$TARGET_LIB_DIR/"
cp -f "$LIBCEDAR_DIR/memory/.libs/libMemAdapter.so" "$TARGET_LIB_DIR/"
cp -f "$LIBCEDAR_DIR/base/.libs/libcdc_base.so" "$TARGET_LIB_DIR/"
cp -f "$LIBCEDAR_DIR/library/toolchain-sunxi-arm9-glibc/libVE.so" "$TARGET_LIB_DIR/"
cp -f "$LIBCEDAR_DIR/library/toolchain-sunxi-arm9-glibc/libvideoengine.so" "$TARGET_LIB_DIR/"
cp -f "$LIBCEDAR_DIR/library/toolchain-sunxi-arm9-glibc/libaw"*.so "$TARGET_LIB_DIR/"

if [ -f "$BASE_DIR/../cedar_direct_test_arm" ]; then
    cp -f "$BASE_DIR/../cedar_direct_test_arm" "$TARGET_DIR/root/cedar_direct_test_arm"
    chmod 0755 "$TARGET_DIR/root/cedar_direct_test_arm"
fi
