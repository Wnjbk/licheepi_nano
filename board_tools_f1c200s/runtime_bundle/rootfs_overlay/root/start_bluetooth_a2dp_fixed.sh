#!/bin/sh

# Fixed A2DP startup for F1C200S RTL8723BU (#222 kernel)
# - No `hciconfig hci0 down` before `up` (that step is flaky).
# - Keep bluetoothd/bluealsa alive across connect retries.
# - Retry connect up to 5 times without tearing services down.

PATH=/usr/sbin:/usr/bin:/sbin:/bin
export DBUS_SYSTEM_BUS_ADDRESS=unix:path=/var/run/dbus/system_bus_socket

BT_DEVICE=${BT_DEVICE:-12:11:71:41:9C:4A}
BT_OBJECT=$( /bin/busybox printf '%s' "$BT_DEVICE" | /bin/busybox tr ':' '_' )
BLUETOOTHD=/usr/libexec/bluetooth/bluetoothd
BLUEALSA=/root/bluealsa/bin/bluealsa
ALSA_TEMPLATE=/root/asound-bluealsa-default.conf.in
LOG=/tmp/bluetooth-a2dp-fixed.log

log() {
    echo "[bt-fixed] $*" | tee -a "$LOG"
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

start_audio() {
    : > "$LOG"

    log "waiting for wlan0"
    wait_wifi || { log "wlan0 not ready"; return 1; }

    [ -d /sys/class/bluetooth/hci0 ] || { log "hci0 missing"; return 1; }

    log "stopping stale bluealsa/bluetoothd"
    /bin/busybox killall bluealsa bluetoothd 2>/dev/null || true
    sleep 1

    log "hci0 up (no prior down)"
    /usr/bin/hciconfig hci0 up || { log "hci0 up failed"; return 1; }

    if ! pidof dbus-daemon >/dev/null 2>&1; then
        /etc/init.d/S30dbus start || true
    fi

    log "starting bluetoothd"
    "$BLUETOOTHD" -C >/tmp/bluetoothd.log 2>&1 &
    sleep 8

    log "starting BlueALSA"
    LD_LIBRARY_PATH=/root/bluealsa/lib "$BLUEALSA" -i hci0 -p a2dp-source \
        >/tmp/bluealsa.log 2>&1 &
    sleep 2

    attempt=1
    while [ "$attempt" -le 5 ]; do
        log "connect attempt $attempt"
        { printf 'agent NoInputNoOutput\ndefault-agent\nconnect %s\n' "$BT_DEVICE"; \
          sleep 4; printf 'yes\n'; sleep 12; } | \
            /usr/bin/bluetoothctl > /tmp/bctl-fixed-$attempt.log 2>&1 || true

        if /usr/bin/dbus-send --system --print-reply --dest=org.bluealsa \
            /org/bluealsa org.bluealsa.Manager1.GetPCMs 2>/dev/null | \
            grep -q "dev_${BT_OBJECT}/a2dp"; then
            [ -e /etc/asound.conf.before-bluealsa ] || \
                cp -p /etc/asound.conf /etc/asound.conf.before-bluealsa
            sed "s/@BT_DEVICE@/$BT_DEVICE/g" "$ALSA_TEMPLATE" > /etc/asound.conf
            log "connected $BT_DEVICE; ALSA default -> BlueALSA A2DP"
            return 0
        fi
        log "attempt $attempt: no A2DP PCM"
        attempt=$((attempt + 1))
        sleep 3
    done
    log "all connect attempts failed"
    return 1
}

stop_audio() {
    /bin/busybox killall bluealsa 2>/dev/null || true
    if [ -e /etc/asound.conf.before-bluealsa ]; then
        cp -p /etc/asound.conf.before-bluealsa /etc/asound.conf
        log "restored local ALSA default"
    fi
}

case "${1:-start}" in
    start) start_audio ;;
    stop) stop_audio ;;
    *)
        echo "usage: $0 {start|stop}" >&2
        exit 2
        ;;
esac
