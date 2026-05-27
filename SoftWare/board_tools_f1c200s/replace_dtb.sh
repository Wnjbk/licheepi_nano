#!/bin/sh

set -e

BOOT_DEV=${BOOT_DEV:-/dev/mmcblk0p1}
BOOT_MNT=${BOOT_MNT:-/mnt/boot}

if [ $# -lt 1 ]; then
echo "usage: $0 new.dtb [target-name.dtb]"
echo "example: $0 /root/suniv-f1c100s-licheepi-nano.dtb"
echo "example: $0 /root/new.dtb suniv-f1c100s-licheepi-nano.dtb"
exit 1
fi

SRC=$1
TARGET=$2

if [ ! -f "$SRC" ]; then
echo "source dtb not found: $SRC"
exit 1
fi

mkdir -p "$BOOT_MNT"

if ! mount | grep -q " $BOOT_MNT "; then
mount "$BOOT_DEV" "$BOOT_MNT"
fi

if [ -z "$TARGET" ]; then
TARGET=$(find "$BOOT_MNT" -maxdepth 2 -type f -name '*.dtb' | head -n 1 | sed "s|$BOOT_MNT/||")
fi

if [ -z "$TARGET" ]; then
echo "no target dtb found in $BOOT_MNT"
echo "files in boot partition:"
find "$BOOT_MNT" -maxdepth 2 -type f
exit 1
fi

DST="$BOOT_MNT/$TARGET"

if [ ! -f "$DST" ]; then
echo "target dtb not found: $DST"
echo "available dtb files:"
find "$BOOT_MNT" -maxdepth 2 -type f -name '*.dtb'
exit 1
fi

BACKUP="$DST.bak.$(date +%Y%m%d-%H%M%S)"
cp "$DST" "$BACKUP"
cp "$SRC" "$DST"
sync

if command -v cmp >/dev/null 2>&1; then
cmp "$SRC" "$DST"
fi

echo "dtb replaced: $DST"
echo "backup: $BACKUP"
echo "reboot to apply"