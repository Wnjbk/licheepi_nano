#!/bin/sh

PATH=/usr/sbin:/usr/bin:/sbin:/bin
export DBUS_SYSTEM_BUS_ADDRESS=unix:path=/var/run/dbus/system_bus_socket

DEVICE=${1:-12:11:71:41:9C:4A}
OBJECT=$(printf '%s' "$DEVICE" | tr ':' '_')
TEMPLATE=/root/asound-bluealsa-default.conf.in

if [ ! -r "$TEMPLATE" ]; then
    echo "missing BlueALSA template: $TEMPLATE" >&2
    exit 1
fi

if ! dbus-send --system --print-reply --dest=org.bluealsa \
    /org/bluealsa org.bluealsa.Manager1.GetPCMs 2>/dev/null |
    grep -q "dev_${OBJECT}/a2dp"; then
    echo "A2DP PCM is not available for $DEVICE" >&2
    exit 1
fi

sed "s/@BT_DEVICE@/$DEVICE/g" "$TEMPLATE" > /etc/asound.conf
sync
echo "ALSA default output now uses BlueALSA A2DP device $DEVICE"
