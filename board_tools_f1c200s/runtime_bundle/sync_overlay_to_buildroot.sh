#!/bin/sh

set -e

ROOT=${ROOT:-$HOME/LicheePi_Nano}
BUILDROOT=${BUILDROOT:-$ROOT/buildroot-2018.02.11}
OVERLAY_DIR=${OVERLAY_DIR:-$(dirname "$0")/rootfs_overlay}
TARGET_DIR=${TARGET_DIR:-$BUILDROOT/output/target}

if [ ! -d "$OVERLAY_DIR" ]; then
    echo "overlay not found: $OVERLAY_DIR"
    exit 1
fi

if [ ! -d "$TARGET_DIR" ]; then
    echo "buildroot target not found: $TARGET_DIR"
    exit 1
fi

cp -a "$OVERLAY_DIR"/. "$TARGET_DIR"/

find "$TARGET_DIR/root" -maxdepth 1 -type f \( -name '*.sh' -o -name 'uinput_tty_keyd' -o -name 'gpsp' -o -name 'tvd_fb_preview' -o -name '*.ko' \) -exec chmod 755 {} \;
find "$TARGET_DIR/etc/init.d" -maxdepth 1 -type f -name 'S*' -exec chmod 755 {} \;

echo "overlay synced to $TARGET_DIR"
