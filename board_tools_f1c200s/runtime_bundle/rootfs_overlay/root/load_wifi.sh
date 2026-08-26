#!/bin/sh

KVER=$(uname -r)
MODULE=${MODULE:-/lib/modules/$KVER/extra/esp8089-spi.ko}
FALLBACK_MODULE=/root/esp8089-spi.ko
BUS=${1:-0}
INSMOD=${INSMOD:-/sbin/insmod}

[ -f "$MODULE" ] || MODULE=$FALLBACK_MODULE

if [ ! -f "$MODULE" ]; then
    echo "wifi: module not found"
    exit 1
fi

if grep -q '^esp8089_spi ' /proc/modules 2>/dev/null; then
    echo "wifi: already loaded"
    exit 0
fi

keep_pa0_high() {
    if [ -d /sys/class/gpio ]; then
        [ -d /sys/class/gpio/gpio0 ] || echo 0 > /sys/class/gpio/export 2>/dev/null || true
        echo out > /sys/class/gpio/gpio0/direction 2>/dev/null || true
        echo 1 > /sys/class/gpio/gpio0/value 2>/dev/null || true
    fi
}

keep_pa0_high

echo "Loading ESP8089 WiFi driver from $MODULE..."
"$INSMOD" "$MODULE" esp_spi_bus="$BUS"

sleep 2

if grep -q '^esp8089_spi ' /proc/modules 2>/dev/null; then
    echo "wifi: module loaded OK"
    dmesg | grep -i "esp8089" | tail -5
else
    echo "wifi: module load FAILED"
    dmesg | tail -20
    exit 1
fi
