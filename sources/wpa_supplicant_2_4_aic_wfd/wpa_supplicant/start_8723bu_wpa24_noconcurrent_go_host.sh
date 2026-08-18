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
CONF="${CONF:-/tmp/wpa24_8723bu_p2p.conf}"
GO_IP="${GO_IP:-192.168.88.1}"
GO_CIDR="${GO_CIDR:-192.168.88.1/24}"
GO_DHCP_START="${GO_DHCP_START:-192.168.88.20}"
GO_DHCP_END="${GO_DHCP_END:-192.168.88.80}"
UPLINK_IFACE="${UPLINK_IFACE:-ens33}"

setup_go_ip()
{
    if [ -f /tmp/dnsmasq_8723bu_go.pid ]; then
        kill "$(cat /tmp/dnsmasq_8723bu_go.pid)" 2>/dev/null || true
        rm -f /tmp/dnsmasq_8723bu_go.pid
    fi

    ip addr add "$GO_CIDR" dev "$IFACE" 2>/dev/null || true
    ip link set "$IFACE" up
    sysctl -w net.ipv4.ip_forward=1 >/dev/null 2>&1 || true
    iptables -t nat -C POSTROUTING -s 192.168.88.0/24 -o "$UPLINK_IFACE" -j MASQUERADE 2>/dev/null || \
        iptables -t nat -A POSTROUTING -s 192.168.88.0/24 -o "$UPLINK_IFACE" -j MASQUERADE 2>/dev/null || true

    rm -f /tmp/dnsmasq_8723bu_go.log
    dnsmasq \
        --interface="$IFACE" \
        --listen-address="$GO_IP" \
        --bind-interfaces \
        --dhcp-range="$GO_DHCP_START","$GO_DHCP_END",255.255.255.0,12h \
        --dhcp-option=3,"$GO_IP" \
        --dhcp-option=6,"$GO_IP" \
        --pid-file=/tmp/dnsmasq_8723bu_go.pid \
        --log-facility=/tmp/dnsmasq_8723bu_go.log
}

pkill wpa_supplicant 2>/dev/null || true
pkill hostapd 2>/dev/null || true
pkill dnsmasq 2>/dev/null || true
pkill miracle-wifid 2>/dev/null || true
pkill miracle-sinkctl 2>/dev/null || true
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
cat >"$CONF" <<EOF
ctrl_interface=$CTRL_DIR
update_config=1
device_name=F1C200S-RTL8723BU
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

"$WPA_DIR/wpa_supplicant" -B -i "$IFACE" -D nl80211 -c "$CONF" -f /tmp/wpa24_8723bu_p2p.log -dd
sleep 2
"$WPA_DIR/wpa_cli" -p "$CTRL_DIR" -i "$IFACE" set wifi_display 1 >/dev/null
"$WPA_DIR/wpa_cli" -p "$CTRL_DIR" -i "$IFACE" wfd_subelem_set 0 000600111c4400c8 >/dev/null
"$WPA_DIR/wpa_cli" -p "$CTRL_DIR" -i "$IFACE" p2p_group_add freq=2437 >/tmp/wpa24_8723bu_group.log 2>&1 || \
    "$WPA_DIR/wpa_cli" -p "$CTRL_DIR" -i "$IFACE" p2p_group_add >/tmp/wpa24_8723bu_group.log 2>&1
sleep 5
setup_go_ip

echo "wpa_supplicant 2.4 p2p_concurrent=0 GO started"
"$WPA_DIR/wpa_cli" -p "$CTRL_DIR" -i "$IFACE" status || true
printf 'passphrase='
"$WPA_DIR/wpa_cli" -p "$CTRL_DIR" -i "$IFACE" p2p_get_passphrase || true
ip addr show dev "$IFACE" || true
tail -20 /tmp/dnsmasq_8723bu_go.log || true
iw dev
grep -E 'P2P_CONCURRENT|P2P-GROUP-STARTED|separate P2P|P2P_CAPABLE|AP-ENABLED|ssid=' /tmp/wpa24_8723bu_p2p.log | tail -80 || true
