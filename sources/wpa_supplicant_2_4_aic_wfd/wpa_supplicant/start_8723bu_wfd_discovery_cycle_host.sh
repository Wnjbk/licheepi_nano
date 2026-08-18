#!/bin/sh
set -eu

if [ "$(id -u)" != "0" ]; then
    exec sudo -E "$0" "$@"
fi

IFACE="${IFACE:-wlx001f058056fd}"
RUN_USER="${SUDO_USER:-wnk}"
RUN_HOME="$(getent passwd "$RUN_USER" | cut -d: -f6)"
RTL_DIR="${RTL_DIR:-$RUN_HOME/LicheePi_Nano/third_party/rtl8723bu_lwfinger_host_ap_20260719_181717}"
WPA_DIR="${WPA_DIR:-$RUN_HOME/LicheePi_Nano/third_party/wpa_supplicant_2_4_p2p_no_concurrent_20260720/wpa_supplicant}"
CTRL_DIR="${CTRL_DIR:-/tmp/wpa24_8723bu_p2p}"
CONF="${CONF:-/tmp/wpa24_8723bu_wfd_discovery.conf}"
LOG="${LOG:-/tmp/wpa24_8723bu_wfd_discovery.log}"

pkill wpa_supplicant 2>/dev/null || true
pkill hostapd 2>/dev/null || true
pkill dnsmasq 2>/dev/null || true
pkill miracle-wifid 2>/dev/null || true
pkill miracle-sinkctl 2>/dev/null || true
pkill -f wpa24_8723bu_wfd_discovery_keeper.sh 2>/dev/null || true
nmcli dev set "$IFACE" managed no 2>/dev/null || true

ip link set "$IFACE" down 2>/dev/null || true
rmmod 8723bu 2>/dev/null || true
modprobe -r rtl8xxxu 2>/dev/null || true
modprobe cfg80211
insmod "$RTL_DIR/8723bu.ko" rtw_power_mgnt=0 rtw_enusbss=0
sleep 3

ip addr flush dev "$IFACE" 2>/dev/null || true
ip link set "$IFACE" up

rm -rf "$CTRL_DIR"
mkdir -p "$CTRL_DIR"
chmod 777 "$CTRL_DIR"
cat >"$CONF" <<EOF
ctrl_interface=$CTRL_DIR
update_config=1
device_name=F1C200S-SINK
manufacturer=F1C200S
model_name=RTL8723BU-WFD-Sink
model_number=1
serial_number=1
device_type=7-0050F204-1
config_methods=display push_button keypad pbc
p2p_go_intent=1
p2p_listen_reg_class=81
p2p_listen_channel=6
p2p_oper_reg_class=81
p2p_oper_channel=6
p2p_no_group_iface=1
ap_scan=1
EOF

rm -f "$LOG"
"$WPA_DIR/wpa_supplicant" -B -i "$IFACE" -D nl80211 -c "$CONF" -f "$LOG" -dd
sleep 2
chmod -R 777 "$CTRL_DIR" 2>/dev/null || true

"$WPA_DIR/wpa_cli" -p "$CTRL_DIR" -i "$IFACE" set device_name F1C200S-SINK >/dev/null
"$WPA_DIR/wpa_cli" -p "$CTRL_DIR" -i "$IFACE" set device_type 7-0050F204-1 >/dev/null
"$WPA_DIR/wpa_cli" -p "$CTRL_DIR" -i "$IFACE" set config_methods "display push_button keypad pbc" >/dev/null
"$WPA_DIR/wpa_cli" -p "$CTRL_DIR" -i "$IFACE" set wifi_display 1 >/dev/null
"$WPA_DIR/wpa_cli" -p "$CTRL_DIR" -i "$IFACE" wfd_subelem_set 0 000600111c4400c8 >/dev/null
"$WPA_DIR/wpa_cli" -p "$CTRL_DIR" -i "$IFACE" p2p_flush >/dev/null

cat >/tmp/wpa24_8723bu_wfd_discovery_keeper.sh <<EOF
#!/bin/sh
while :; do
    for ch in 1 6 11; do
        date
        echo "listen channel \$ch"
        "$WPA_DIR/wpa_cli" -p "$CTRL_DIR" -i "$IFACE" p2p_stop_find || true
        "$WPA_DIR/wpa_cli" -p "$CTRL_DIR" -i "$IFACE" p2p_set listen_channel "\$ch" || true
        "$WPA_DIR/wpa_cli" -p "$CTRL_DIR" -i "$IFACE" p2p_listen 6 || true
        sleep 7
    done
done
EOF
chmod +x /tmp/wpa24_8723bu_wfd_discovery_keeper.sh
nohup /tmp/wpa24_8723bu_wfd_discovery_keeper.sh >/tmp/wpa24_8723bu_wfd_discovery_cycle.log 2>&1 &

sleep 3
echo "RTL8723BU WFD discovery cycle started"
"$WPA_DIR/wpa_cli" -p "$CTRL_DIR" -i "$IFACE" status || true
"$WPA_DIR/wpa_cli" -p "$CTRL_DIR" -i "$IFACE" get wifi_display || true
iw dev
grep -nE 'WFD IE|listen|remain-on-channel|P2P: Going|Probe Request|Probe Response|Device Info' "$LOG" | tail -80 || true
