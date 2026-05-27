#!/bin/sh

set -e

ROOT=${ROOT:-$HOME/LicheePi_Nano}
BUILDROOT=${BUILDROOT:-$ROOT/buildroot-2018.02.11}
PAYLOAD_DIR=${PAYLOAD_DIR:-$(dirname "$0")/payload}
TARGET_DIR=${TARGET_DIR:-$BUILDROOT/output/target}

if [ ! -d "$PAYLOAD_DIR" ]; then
    echo "payload not found: $PAYLOAD_DIR"
    exit 1
fi

if [ ! -d "$TARGET_DIR" ]; then
    echo "buildroot target not found: $TARGET_DIR"
    exit 1
fi

if [ -f "$PAYLOAD_DIR/gmenu2x-f1c200s.tar.gz" ]; then
    mkdir -p "$TARGET_DIR/root/gmenu2x"
    gzip -dc "$PAYLOAD_DIR/gmenu2x-f1c200s.tar.gz" | tar -xf - -C "$TARGET_DIR/root/gmenu2x"
fi

if [ -f "$PAYLOAD_DIR/gpsp" ]; then
    cp -a "$PAYLOAD_DIR/gpsp" "$TARGET_DIR/root/gpsp"
    chmod 755 "$TARGET_DIR/root/gpsp"
fi

if [ -f "$PAYLOAD_DIR/uinput_tty_keyd" ]; then
    cp -a "$PAYLOAD_DIR/uinput_tty_keyd" "$TARGET_DIR/root/uinput_tty_keyd"
    chmod 755 "$TARGET_DIR/root/uinput_tty_keyd"
fi

if [ -f "$PAYLOAD_DIR/suniv_f1c100s_tvd.ko" ]; then
    cp -a "$PAYLOAD_DIR/suniv_f1c100s_tvd.ko" "$TARGET_DIR/root/suniv_f1c100s_tvd.ko"
    chmod 755 "$TARGET_DIR/root/suniv_f1c100s_tvd.ko"
fi

if [ -f "$PAYLOAD_DIR/tvd_fb_preview" ]; then
    cp -a "$PAYLOAD_DIR/tvd_fb_preview" "$TARGET_DIR/root/tvd_fb_preview"
    chmod 755 "$TARGET_DIR/root/tvd_fb_preview"
fi

mkdir -p "$TARGET_DIR/root/roms/gba" "$TARGET_DIR/root/roms/gb"

echo "payload installed to $TARGET_DIR"
