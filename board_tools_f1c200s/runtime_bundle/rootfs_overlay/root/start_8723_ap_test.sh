#!/bin/sh
set -u

PATH=/sbin:/bin:/usr/sbin:/usr/bin:/root
LOG=/root/8723_ap_test_last.log
SSID=${SSID:-F1C-RTLAP}
CHANNEL=${CHANNEL:-1}
DURATION=${1:-60}
CONF=/root/8723_hostapd.conf
PID=/root/8723_ap_hostapd.pid
AP_MODULE=${AP_MODULE:-/lib/modules/5.7.1/extra/8723bu_ap_hostapd_clean.ko}

log() {
    echo "[8723-ap] $(date '+%H:%M:%S' 2>/dev/null) $*" >>"$LOG"
}

stop_ap_parts() {
    kill "$(cat "$PID" 2>/dev/null)" 2>/dev/null || true
    killall hostapd 2>/dev/null || true
    killall wpa_supplicant 2>/dev/null || true
    ifconfig wlan0 down 2>>"$LOG" || true
}

restore_wifi() {
    log "restore default wifi begin"
    stop_ap_parts
    rmmod 8723bu >>"$LOG" 2>&1 || true
    sleep 1
    /etc/init.d/S17rtl8723bu start >>"$LOG" 2>&1 || true
    sleep 8
    ifconfig wlan0 >>"$LOG" 2>&1 || true
    ps | grep -E 'wpa|hostapd' | grep -v grep >>"$LOG" 2>&1 || true
    log "restore default wifi done"
}

case "${2:-}" in
    restore)
        restore_wifi
        exit 0
        ;;
esac

rm -f "$LOG"
log "start rtl8723bu hostapd AP test ssid=$SSID channel=$CHANNEL duration=${DURATION}s module=$AP_MODULE"
log "hostapd version: $(/usr/sbin/hostapd -v 2>&1 | head -1)"

(
    sleep "$DURATION"
    /root/start_8723_ap_test.sh 0 restore
) >>"$LOG" 2>&1 &
log "restore watchdog pid=$!"

stop_ap_parts
rmmod 8723bu >>"$LOG" 2>&1 || true
sleep 1
insmod "$AP_MODULE" rtw_power_mgnt=0 rtw_ips_mode=0 rtw_smart_ps=0 rtw_low_power=0 rtw_enusbss=0 rtw_btcoex_enable=0 rtw_ht_enable=0 rtw_channel_plan=0x7F >>"$LOG" 2>&1 || {
    log "insmod 8723bu failed"
    exit 1
}
sleep 2
ifconfig wlan0 up >>"$LOG" 2>&1 || true

cat > "$CONF" <<EOF
interface=wlan0
driver=rtl871xdrv
ssid=$SSID
hw_mode=g
channel=$CHANNEL
beacon_int=100
dtim_period=2
auth_algs=1
ignore_broadcast_ssid=0
wmm_enabled=0
ieee80211n=0
EOF

log "hostapd config begin"
cat "$CONF" >>"$LOG"
log "hostapd config end"
/usr/sbin/hostapd -B -P "$PID" -dd "$CONF" >>"$LOG" 2>&1
rc=$?
case "$rc" in
    ''|*[!0-9]*) rc=1 ;;
esac
log "hostapd rc=$rc pid=$(cat "$PID" 2>/dev/null || true)"
ifconfig wlan0 192.168.49.1 netmask 255.255.255.0 up >>"$LOG" 2>&1 || true
/usr/bin/hostapd_cli -i wlan0 status >>"$LOG" 2>&1 || true
cat /proc/modules | grep 8723bu >>"$LOG" 2>&1 || true
cat /proc/net/rtl8723bu/wlan0/fwstate >>"$LOG" 2>&1 || true
cat /proc/net/rtl8723bu/wlan0/mlmext_state >>"$LOG" 2>&1 || true
cat /proc/net/rtl8723bu/wlan0/ap_info >>"$LOG" 2>&1 || true
dmesg | tail -100 >>"$LOG" 2>&1 || true
log "AP test active"
exit $rc
