#!/bin/sh

PATH=/usr/sbin:/usr/bin:/sbin:/bin
export DBUS_SYSTEM_BUS_ADDRESS=unix:path=/var/run/dbus/system_bus_socket

BT_DEVICE=${BT_DEVICE:-12:11:71:41:9C:4A}
BT_OBJECT=$( /bin/busybox printf '%s' "$BT_DEVICE" | /bin/busybox tr ':' '_' )
BLUETOOTHD=/usr/libexec/bluetooth/bluetoothd
BLUEALSA=/root/bluealsa/bin/bluealsa
ALSA_TEMPLATE=/root/asound-bluealsa-default.conf.in
ALSA_BACKUP=/etc/asound.conf.before-bluealsa
LOG=/tmp/bluetooth-a2dp.log

log() {
    echo "[bluetooth-a2dp] $*" | tee -a "$LOG"
}

wait_wifi() {
    n=0
    while [ "$n" -lt 30 ]; do
        /usr/sbin/wpa_cli -i wlan0 status 2>/dev/null | grep -q '^wpa_state=COMPLETED$' && return 0
        sleep 1
        n=$((n + 1))
    done
    return 1
}

wait_pcm() {
    n=0
    while [ "$n" -lt 20 ]; do
        /usr/bin/dbus-send --system --print-reply --dest=org.bluealsa \
            /org/bluealsa org.bluealsa.Manager1.GetPCMs 2>/dev/null | \
            grep -q "dev_${BT_OBJECT}/a2dp" && return 0
        sleep 1
        n=$((n + 1))
    done
    return 1
}

start_audio() {
    : > "$LOG"

    log "waiting for wlan0 association"
    if ! wait_wifi; then
        log "wlan0 did not reach WPA COMPLETED"
        return 1
    fi

    if [ ! -d /sys/class/bluetooth/hci0 ]; then
        log "hci0 is missing; boot Bluetooth modules were not initialized"
        return 1
    fi

    log "bringing hci0 up"
    /usr/bin/hciconfig hci0 up || return 1

    if ! pidof dbus-daemon >/dev/null 2>&1; then
        /etc/init.d/S30dbus start || return 1
    fi

    if ! pidof bluetoothd >/dev/null 2>&1; then
        log "starting bluetoothd and waiting for initialization"
        "$BLUETOOTHD" -C >/tmp/bluetoothd.log 2>&1 &
        sleep 12
    fi

    log "starting BlueALSA"
    /bin/busybox killall bluealsa 2>/dev/null || true
    LD_LIBRARY_PATH=/root/bluealsa/lib "$BLUEALSA" -i hci0 -p a2dp-source \
        >/tmp/bluealsa.log 2>&1 &
    sleep 3

    log "connecting $BT_DEVICE"
    /bin/busybox printf 'connect %s\nquit\n' "$BT_DEVICE" | /usr/bin/bluetoothctl >/tmp/bluetoothctl-connect.log 2>&1

    log "waiting for A2DP PCM"
    if ! wait_pcm; then
        log "A2DP PCM did not appear for $BT_DEVICE"
        return 1
    fi

    [ -e "$ALSA_BACKUP" ] || cp -p /etc/asound.conf "$ALSA_BACKUP"
    sed "s/@BT_DEVICE@/$BT_DEVICE/g" "$ALSA_TEMPLATE" > /etc/asound.conf
    log "connected $BT_DEVICE; ALSA default output is now BlueALSA A2DP"
}

stop_audio() {
    /bin/busybox killall bluealsa 2>/dev/null || true
    if [ -e "$ALSA_BACKUP" ]; then
        cp -p "$ALSA_BACKUP" /etc/asound.conf
        log "restored local ALSA default output"
    fi
}

case "${1:-start}" in
    start) start_audio ;;
    stop) stop_audio ;;
    status)
        /usr/bin/hciconfig hci0
        /usr/bin/dbus-send --system --print-reply --dest=org.bluealsa \
            /org/bluealsa org.bluealsa.Manager1.GetPCMs
        ;;
    *)
        echo "usage: $0 {start|stop|status}" >&2
        exit 2
        ;;
esac
