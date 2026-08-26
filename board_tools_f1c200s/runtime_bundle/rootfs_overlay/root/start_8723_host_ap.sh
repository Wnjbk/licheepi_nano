#!/bin/sh
set -u

PATH=/sbin:/bin:/usr/sbin:/usr/bin:/root

LOG=${LOG:-/tmp/start_8723_host_ap.log}
IFACE=${IFACE:-wlan0}
SSID=${SSID:-RTL8723BU_AP}
PASSPHRASE=${PASSPHRASE:-12345678}
CHANNEL=${CHANNEL:-6}
AP_ADDR=${AP_ADDR:-192.168.88.1}
AP_NETMASK=${AP_NETMASK:-255.255.255.0}
DURATION=${DURATION:-300}
AP_MODULE=${AP_MODULE:-/lib/modules/5.7.1/extra/8723bu.ko}
CONF=${CONF:-/tmp/hostapd_8723bu_ap.conf}
PID=${PID:-/tmp/hostapd_8723bu_ap.pid}
RESTORE_FLAG=${RESTORE_FLAG:-/tmp/8723_ap_restore_pending}

log() {
    echo "[8723-ap] $(date '+%H:%M:%S' 2>/dev/null) $*" | tee -a "$LOG"
}

stop_userland() {
    kill "$(cat "$PID" 2>/dev/null)" 2>/dev/null || true
    killall hostapd 2>/dev/null || true
    killall wpa_supplicant 2>/dev/null || true
    killall tiny_dhcpd_8723 2>/dev/null || true
}

start_dhcp() {
    if command -v tiny_dhcpd_8723 >/dev/null 2>&1; then
        tiny_dhcpd_8723 "$IFACE" "$AP_ADDR" 192.168.88.20 192.168.88.80 >>"$LOG" 2>&1 &
        dhcp_pid=$!
        sleep 1
        if kill -0 "$dhcp_pid" 2>/dev/null; then
            log "tiny_dhcpd_8723 pid=$dhcp_pid"
            return 0
        fi
        log "tiny_dhcpd_8723 with args exited; retry without args"
        tiny_dhcpd_8723 >>"$LOG" 2>&1 &
        log "tiny_dhcpd_8723 fallback pid=$!"
        return 0
    fi
    log "tiny_dhcpd_8723 not found; AP has no DHCP server"
}

restore_sta() {
    log "restore STA begin"
    stop_userland
    ifconfig "$IFACE" down 2>>"$LOG" || true
    rmmod 8723bu >>"$LOG" 2>&1 || true
    sleep 1
    /etc/init.d/S17rtl8723bu start >>"$LOG" 2>&1 || true
    sleep 2
    /etc/init.d/S45usb-wifi restart >>"$LOG" 2>&1 || /etc/init.d/S45usb-wifi start >>"$LOG" 2>&1 || true
    rm -f "$RESTORE_FLAG"
    ifconfig "$IFACE" >>"$LOG" 2>&1 || true
    log "restore STA done"
}

case "${1:-start}" in
    stop|restore)
        restore_sta
        exit 0
        ;;
esac

rm -f "$LOG"
log "start host AP ssid=$SSID channel=$CHANNEL duration=$DURATION module=$AP_MODULE"

stop_userland
ifconfig "$IFACE" down 2>>"$LOG" || true
rmmod 8723bu >>"$LOG" 2>&1 || true
sleep 1

insmod "$AP_MODULE" rtw_power_mgnt=0 rtw_ips_mode=0 rtw_smart_ps=0 rtw_low_power=0 rtw_enusbss=0 >>"$LOG" 2>&1 || {
    log "insmod failed"
    exit 1
}
sleep 2

ifconfig "$IFACE" "$AP_ADDR" netmask "$AP_NETMASK" up >>"$LOG" 2>&1 || {
    log "ifconfig AP address failed"
    restore_sta
    exit 1
}

cat > "$CONF" <<EOF
interface=$IFACE
driver=nl80211
ssid=$SSID
hw_mode=g
channel=$CHANNEL
ieee80211n=1
wmm_enabled=1
ignore_broadcast_ssid=0
auth_algs=1
wpa=2
wpa_passphrase=$PASSPHRASE
wpa_key_mgmt=WPA-PSK
rsn_pairwise=CCMP
EOF

log "hostapd config:"
cat "$CONF" >>"$LOG"

/usr/sbin/hostapd -B -P "$PID" -dd "$CONF" >>"$LOG" 2>&1 || {
    log "hostapd failed"
    restore_sta
    exit 1
}

start_dhcp

touch "$RESTORE_FLAG"
if [ "$DURATION" != "0" ]; then
    (
        sleep "$DURATION"
        [ -f "$RESTORE_FLAG" ] && /root/start_8723_host_ap.sh restore
    ) >>"$LOG" 2>&1 &
    log "auto-restore pid=$! after ${DURATION}s"
fi

/usr/bin/hostapd_cli -i "$IFACE" status >>"$LOG" 2>&1 || true
ifconfig "$IFACE" >>"$LOG" 2>&1 || true
dmesg | tail -80 >>"$LOG" 2>&1 || true
log "AP active: SSID=$SSID password=$PASSPHRASE"
