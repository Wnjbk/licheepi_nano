#!/bin/sh

set -e

if [ -e /dev/uinput ]; then
    chmod 666 /dev/uinput 2>/dev/null || true
    echo "/dev/uinput already present"
    exit 0
fi

if [ -e /dev/input/uinput ]; then
    chmod 666 /dev/input/uinput 2>/dev/null || true
    ln -sf /dev/input/uinput /dev/uinput 2>/dev/null || true
    echo "/dev/input/uinput already present"
    exit 0
fi

if [ -w /sys/module/uinput/parameters ] 2>/dev/null; then
    :
fi

if command -v modprobe >/dev/null 2>&1; then
    modprobe uinput 2>/dev/null || true
fi

if [ ! -e /dev/uinput ] && [ ! -e /dev/input/uinput ] && [ -f /proc/misc ]; then
    MINOR=$(awk '$2 == "uinput" { print $1 }' /proc/misc)
    if [ -n "$MINOR" ]; then
        mkdir -p /dev/input
        mknod /dev/uinput c 10 "$MINOR" 2>/dev/null || true
    fi
fi

if [ -e /dev/uinput ]; then
    chmod 666 /dev/uinput 2>/dev/null || true
    echo "/dev/uinput ready"
    exit 0
fi

if [ -e /dev/input/uinput ]; then
    chmod 666 /dev/input/uinput 2>/dev/null || true
    ln -sf /dev/input/uinput /dev/uinput 2>/dev/null || true
    echo "/dev/input/uinput ready"
    exit 0
fi

echo "uinput device not available"
echo "check kernel config: CONFIG_INPUT=y CONFIG_INPUT_EVDEV=y CONFIG_INPUT_UINPUT=y|m"
exit 1
