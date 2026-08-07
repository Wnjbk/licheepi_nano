#!/bin/sh

export DBUS_SYSTEM_BUS_ADDRESS=${DBUS_SYSTEM_BUS_ADDRESS:-unix:path=/var/run/dbus/system_bus_socket}
MODULE_DIR=${MODULE_DIR:-/root/roms/system_modules/rtl8723bluetooth}

load_bt_modules() {
    for mod in btrtl btbcm btintel btusb; do
        if grep -q "^${mod} " /proc/modules 2>/dev/null; then
            continue
        fi

        if [ ! -r "$MODULE_DIR/${mod}.ko" ]; then
            echo "bluetooth: missing $MODULE_DIR/${mod}.ko"
            return 1
        fi

        echo "bluetooth: loading ${mod}.ko"
        /sbin/insmod "$MODULE_DIR/${mod}.ko" || return 1
    done
}

wait_for_hci() {
    count=0
    while [ ! -d /sys/class/bluetooth/hci0 ] && [ "$count" -lt 15 ]; do
        sleep 1
        count=$((count + 1))
    done
    [ -d /sys/class/bluetooth/hci0 ]
}

load_bt_modules || exit 1
wait_for_hci || {
    echo "bluetooth: hci0 did not appear"
    exit 1
}

mkdir -p /var/run/dbus /var/lock/subsys /tmp/dbus

BLUETOOTHD=
if command -v bluetoothd >/dev/null 2>&1; then
    BLUETOOTHD="$(command -v bluetoothd)"
elif [ -x /usr/libexec/bluetooth/bluetoothd ]; then
    BLUETOOTHD=/usr/libexec/bluetooth/bluetoothd
fi

if command -v rfkill >/dev/null 2>&1; then
    rfkill unblock bluetooth 2>/dev/null || rfkill unblock all 2>/dev/null || true
fi

if ! pidof dbus-daemon >/dev/null 2>&1; then
    rm -f /var/run/messagebus.pid /var/run/dbus/pid
    if [ -x /etc/init.d/S30dbus ]; then
        /etc/init.d/S30dbus start || true
    elif command -v dbus-uuidgen >/dev/null 2>&1 && command -v dbus-daemon >/dev/null 2>&1; then
        dbus-uuidgen --ensure
        dbus-daemon --system
    fi
fi

if ! pidof bluetoothd >/dev/null 2>&1; then
    if [ -n "$BLUETOOTHD" ]; then
        "$BLUETOOTHD" -n >/tmp/bluetoothd.log 2>&1 &
        sleep 1
    else
        echo "bluetoothd not found"
        exit 1
    fi
fi

if command -v hciconfig >/dev/null 2>&1; then
    hciconfig hci0 up || true
fi

echo "bluetooth start sequence completed"
