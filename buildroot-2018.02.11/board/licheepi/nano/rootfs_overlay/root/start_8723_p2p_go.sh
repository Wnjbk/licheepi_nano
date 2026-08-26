#!/bin/sh
set -u

PATH=/sbin:/bin:/usr/sbin:/usr/bin:/root

LOG=${LOG:-/tmp/start_8723_p2p_go.log}
IFACE=${IFACE:-wlan0}
DEVICE_NAME=${DEVICE_NAME:-F1C200S-SINK}
GO_ADDR=${GO_ADDR:-192.168.88.1}
GO_NETMASK=${GO_NETMASK:-255.255.255.0}
GO_DHCP_START=${GO_DHCP_START:-192.168.88.20}
GO_DHCP_END=${GO_DHCP_END:-192.168.88.80}
CTRL_DIR=${CTRL_DIR:-/tmp/wpa_8723_p2p}
CONF=${CONF:-/tmp/wpa_8723_p2p.conf}
AP_MODULE=${AP_MODULE:-/lib/modules/5.7.1/extra/8723bu.ko}
WPA_SUPPLICANT=${WPA_SUPPLICANT:-/usr/sbin/wpa_supplicant}
WPA_CLI=${WPA_CLI:-/usr/sbin/wpa_cli}

log() {
    echo "[8723-p2p-go] $(date '+%H:%M:%S' 2>/dev/null) $*" | tee -a "$LOG"
}

stop_all() {
    killall hostapd 2>/dev/null || true
    killall tiny_dhcpd_8723 2>/dev/null || true
    "$WPA_CLI" -p "$CTRL_DIR" -i "$IFACE" terminate >/dev/null 2>&1 || true
    "$WPA_CLI" -i "$IFACE" terminate >/dev/null 2>&1 || true
    killall wpa_supplicant 2>/dev/null || true
    sleep 1
    killall -9 hostapd 2>/dev/null || true
    killall -9 tiny_dhcpd_8723 2>/dev/null || true
    killall -9 wpa_supplicant 2>/dev/null || true
}

case "${1:-start}" in
    stop)
        stop_all
        ifconfig "$IFACE" down 2>/dev/null || true
        exit 0
        ;;
esac

rm -f "$LOG"
log "start P2P-GO device_name=$DEVICE_NAME iface=$IFACE"

stop_all
ifconfig "$IFACE" down 2>>"$LOG" || true

if ! grep -q '^8723bu ' /proc/modules 2>/dev/null; then
    rmmod 8723bu >>"$LOG" 2>&1 || true
    insmod "$AP_MODULE" rtw_power_mgnt=0 rtw_ips_mode=0 rtw_smart_ps=0 rtw_low_power=0 rtw_enusbss=0 >>"$LOG" 2>&1 || {
        log "insmod failed"
        exit 1
    }
    sleep 2
fi

ifconfig "$IFACE" up >>"$LOG" 2>&1 || true

rm -rf "$CTRL_DIR"
mkdir -p "$CTRL_DIR" /var/run/wpa_supplicant /run/wpa_supplicant
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

log "wpa config:"
cat "$CONF" >>"$LOG"

"$WPA_SUPPLICANT" -B -i "$IFACE" -D nl80211 -c "$CONF" -f "$LOG" -dd >>"$LOG" 2>&1 || {
    log "wpa_supplicant failed"
    exit 1
}
sleep 2

"$WPA_CLI" -p "$CTRL_DIR" -i "$IFACE" set wifi_display 1 >>"$LOG" 2>&1 || true
"$WPA_CLI" -p "$CTRL_DIR" -i "$IFACE" wfd_subelem_set 0 000600111c4400c8 >>"$LOG" 2>&1 || true
"$WPA_CLI" -p "$CTRL_DIR" -i "$IFACE" p2p_flush >>"$LOG" 2>&1 || true
"$WPA_CLI" -p "$CTRL_DIR" -i "$IFACE" p2p_group_add freq=2437 >>"$LOG" 2>&1 || \
    "$WPA_CLI" -p "$CTRL_DIR" -i "$IFACE" p2p_group_add >>"$LOG" 2>&1 || {
        log "p2p_group_add failed"
        tail -120 "$LOG"
        exit 1
    }
sleep 5

ifconfig "$IFACE" "$GO_ADDR" netmask "$GO_NETMASK" up >>"$LOG" 2>&1 || true
if command -v tiny_dhcpd_8723 >/dev/null 2>&1; then
    tiny_dhcpd_8723 "$IFACE" "$GO_ADDR" "$GO_DHCP_START" "$GO_DHCP_END" >>"$LOG" 2>&1 &
    log "tiny_dhcpd_8723 pid=$!"
else
    log "tiny_dhcpd_8723 missing; clients need static IP"
fi

log "P2P-GO status:"
"$WPA_CLI" -p "$CTRL_DIR" -i "$IFACE" status >>"$LOG" 2>&1 || true
printf "passphrase=" >>"$LOG"
"$WPA_CLI" -p "$CTRL_DIR" -i "$IFACE" p2p_get_passphrase >>"$LOG" 2>&1 || true
ifconfig "$IFACE" >>"$LOG" 2>&1 || true
cat /proc/modules | grep 8723 >>"$LOG" 2>&1 || true
tail -160 "$LOG"
