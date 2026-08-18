#!/bin/sh
set -eu

IFACE="${IFACE:-wlx40554834708e}"
RUN_USER="${SUDO_USER:-wnk}"
RUN_HOME="$(getent passwd "$RUN_USER" | cut -d: -f6)"
WPA_DIR="${WPA_DIR:-$RUN_HOME/LicheePi_Nano/third_party/wpa_supplicant_2_4_aic_wfd_20260720/wpa_supplicant}"
CTRL_DIR="${CTRL_DIR:-/tmp/wpa24_aic8800_wfd}"
CONF="${CONF:-/tmp/wpa24_aic8800_wfd.conf}"
LOG="${LOG:-/tmp/wpa24_aic8800_wfd.log}"

if [ "$(id -u)" != "0" ]; then
    exec sudo -E "$0" "$@"
fi

pkill wpa_supplicant 2>/dev/null || true
pkill hostapd 2>/dev/null || true
pkill dnsmasq 2>/dev/null || true
nmcli dev set "$IFACE" managed no 2>/dev/null || true

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
model_name=AIC8800-WFD-Sink
model_number=1
serial_number=1
device_type=7-0050F204-1
config_methods=display push_button keypad pbc
p2p_go_intent=15
p2p_listen_reg_class=81
p2p_listen_channel=6
p2p_oper_reg_class=124
p2p_oper_channel=161
p2p_no_group_iface=1
ap_scan=1
EOF

rm -f "$LOG"
"$WPA_DIR/wpa_supplicant" -B -i "$IFACE" -D nl80211 -c "$CONF" -f "$LOG" -dd
sleep 2
chmod -R 777 "$CTRL_DIR" 2>/dev/null || true

"$WPA_DIR/wpa_cli" -p "$CTRL_DIR" -i "$IFACE" set wifi_display 1 >/dev/null
"$WPA_DIR/wpa_cli" -p "$CTRL_DIR" -i "$IFACE" wfd_subelem_set 0 000600111c4400c8 >/dev/null
"$WPA_DIR/wpa_cli" -p "$CTRL_DIR" -i "$IFACE" p2p_ext_listen 1000 1000 >/dev/null 2>&1 || true
"$WPA_DIR/wpa_cli" -p "$CTRL_DIR" -i "$IFACE" p2p_find 20 type=social >/dev/null
sleep 20
"$WPA_DIR/wpa_cli" -p "$CTRL_DIR" -i "$IFACE" p2p_stop_find >/dev/null 2>&1 || true
"$WPA_DIR/wpa_cli" -p "$CTRL_DIR" -i "$IFACE" p2p_listen 0 >/dev/null

echo "AIC8800 WFD persistent listen started on $IFACE"
"$WPA_DIR/wpa_cli" -p "$CTRL_DIR" -i "$IFACE" status || true
iw dev
tail -100 "$LOG" || true
