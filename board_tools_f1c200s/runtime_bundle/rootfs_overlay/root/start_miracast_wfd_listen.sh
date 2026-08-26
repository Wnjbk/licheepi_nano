#!/bin/sh
set -u

PATH=/sbin:/bin:/usr/sbin:/usr/bin:/root

LOG=${LOG:-/tmp/start_miracast_wfd_listen.log}
RTSP_LOG=${RTSP_LOG:-/tmp/miracast_rtsp_sink.log}
CTRL_DIR=${CTRL_DIR:-/tmp/wpa_8723_wfd_listen}
CONF=${CONF:-/tmp/wpa_8723_wfd_listen.conf}
IFACE=${IFACE:-wlan0}
DEVICE_NAME=${DEVICE_NAME:-F1C200S-SINK}
AP_MODULE=${AP_MODULE:-/lib/modules/5.7.1/extra/8723bu.ko}

log() {
    echo "[wfd-listen] $(date '+%H:%M:%S' 2>/dev/null) $*" | tee -a "$LOG"
}

stop_all() {
    killall miracast_rtsp_sink 2>/dev/null || true
    killall tiny_dhcpd_8723 2>/dev/null || true
    killall hostapd 2>/dev/null || true
    /usr/sbin/wpa_cli -p "$CTRL_DIR" -i "$IFACE" terminate >/dev/null 2>&1 || true
    /usr/sbin/wpa_cli -p /tmp/wpa_8723_p2p -i "$IFACE" terminate >/dev/null 2>&1 || true
    /usr/sbin/wpa_cli -i "$IFACE" terminate >/dev/null 2>&1 || true
    killall wpa_supplicant 2>/dev/null || true
    sleep 1
    killall -9 miracast_rtsp_sink tiny_dhcpd_8723 hostapd wpa_supplicant 2>/dev/null || true
}

case "${1:-start}" in
    stop)
        stop_all
        ifconfig "$IFACE" down 2>/dev/null || true
        exit 0
        ;;
esac

rm -f "$LOG" "$RTSP_LOG"
log "start WFD listen device_name=$DEVICE_NAME"
stop_all

ifconfig "$IFACE" down 2>/dev/null || true
if ! grep -q '^8723bu ' /proc/modules 2>/dev/null; then
    rmmod 8723bu 2>/dev/null || true
    insmod "$AP_MODULE" rtw_power_mgnt=0 rtw_ips_mode=0 rtw_smart_ps=0 rtw_low_power=0 rtw_enusbss=0 >>"$LOG" 2>&1 || exit 1
    sleep 2
fi
ifconfig "$IFACE" up >>"$LOG" 2>&1 || true

rm -rf "$CTRL_DIR"
mkdir -p "$CTRL_DIR"
cat > "$CONF" <<EOF
ctrl_interface=$CTRL_DIR
update_config=1
device_name=$DEVICE_NAME
device_type=7-0050F204-1
config_methods=display push_button keypad pbc
p2p_go_intent=15
p2p_listen_reg_class=81
p2p_listen_channel=6
p2p_oper_reg_class=81
p2p_oper_channel=6
p2p_no_group_iface=1
ap_scan=1
EOF

/usr/sbin/wpa_supplicant -B -i "$IFACE" -D nl80211 -c "$CONF" -f "$LOG" -dd >>"$LOG" 2>&1 || exit 1
sleep 2
/usr/sbin/wpa_cli -p "$CTRL_DIR" -i "$IFACE" set wifi_display 1 >>"$LOG" 2>&1 || true
/usr/sbin/wpa_cli -p "$CTRL_DIR" -i "$IFACE" wfd_subelem_set 0 000600111c440032 >>"$LOG" 2>&1 || true
/usr/sbin/wpa_cli -p "$CTRL_DIR" -i "$IFACE" p2p_set listen_channel 6 >>"$LOG" 2>&1 || true
/usr/sbin/wpa_cli -p "$CTRL_DIR" -i "$IFACE" p2p_ext_listen 500 2000 >>"$LOG" 2>&1 || true
/usr/sbin/wpa_cli -p "$CTRL_DIR" -i "$IFACE" p2p_find type=social >>"$LOG" 2>&1 || true

if [ -x /root/miracast_rtsp_sink ]; then
    /root/miracast_rtsp_sink "$RTSP_LOG" >>"$LOG" 2>&1 &
    log "rtsp pid=$!"
fi

log "listen status:"
/usr/sbin/wpa_cli -p "$CTRL_DIR" -i "$IFACE" status >>"$LOG" 2>&1 || true
/usr/sbin/wpa_cli -p "$CTRL_DIR" -i "$IFACE" wfd_subelem_get 0 >>"$LOG" 2>&1 || true
tail -120 "$LOG"
