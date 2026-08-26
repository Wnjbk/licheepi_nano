#!/bin/sh

set -u

PATH=/sbin:/bin:/usr/sbin:/usr/bin:/root
LOG=/tmp/miracast_sink_probe.log
RTSP_LOG=/tmp/miracast_rtsp_sink.log
MOD=/lib/modules/5.7.1/extra/8723bu_p2p_concurrent.ko
BASE_OPTS="rtw_power_mgnt=0 rtw_ips_mode=0 rtw_smart_ps=0 rtw_low_power=0 rtw_enusbss=0 rtw_btcoex_enable=0 rtw_ht_enable=1 rtw_bw_mode=1 rtw_wmm_enable=1 rtw_ampdu_enable=1 rtw_usb_rxagg_mode=0 rtw_wifi_spec=0 rtw_channel_plan=0x7F rtw_antdiv_cfg=0 rtw_ant_num=1 rtw_adaptivity_en=0"
P2P_CTRL="${P2P_CTRL:-wlan0}"
GO_FREQ="${GO_FREQ:-2412}"

log() {
    echo "[miracast] $*" | tee -a "$LOG"
}

wait_iface() {
    iface="$1"
    i=0
    while [ "$i" -lt 40 ]; do
        [ -d "/sys/class/net/$iface" ] && return 0
        sleep 0.25
        i=$((i + 1))
    done
    return 1
}

stop_old() {
    kill "$(cat /tmp/miracast_rtsp_sink.pid 2>/dev/null)" 2>/dev/null || true
    rm -f /tmp/miracast_rtsp_sink.pid
    /usr/sbin/wpa_cli -i wlan0 p2p_group_remove p2p0 >>"$LOG" 2>&1 || true
    /usr/sbin/wpa_cli -i p2p0 p2p_group_remove p2p0 >>"$LOG" 2>&1 || true
    /usr/sbin/wpa_cli -i wlan0 terminate >>"$LOG" 2>&1 || true
    /usr/sbin/wpa_cli -i p2p0 terminate >>"$LOG" 2>&1 || true
    killall wpa_supplicant >>"$LOG" 2>&1 || true
    killall wpa_cli >>"$LOG" 2>&1 || true
}

load_p2p_module() {
    if [ -d /sys/class/net/p2p0 ]; then
        return 0
    fi

    ifconfig wlan0 down >>"$LOG" 2>&1 || true
    rmmod 8723bu >>"$LOG" 2>&1 || true
    sleep 1
    log "load 8723bu p2p concurrent module"
    insmod "$MOD" $BASE_OPTS ifname=wlan%d if2name=p2p%d >>"$LOG" 2>&1 || return 1
    wait_iface p2p0
}

start_wpa() {
    cat > /tmp/miracast_p2p.conf <<'EOF'
ctrl_interface=/var/run/wpa_supplicant
update_config=1
device_name=F1C200S-Miracast
device_type=7-0050F204-1
config_methods=virtual_push_button physical_display keypad
p2p_go_intent=15
p2p_no_group_iface=0
EOF
    mkdir -p /var/run /var/run/wpa_supplicant /run/wpa_supplicant
    ifconfig wlan0 up >>"$LOG" 2>&1 || true
    /usr/sbin/wpa_supplicant -B -D nl80211,wext -i wlan0 -c /tmp/miracast_p2p.conf >>"$LOG" 2>&1 || return 1
    sleep 1
}

setup_wfd() {
    /usr/sbin/wpa_cli -i "$P2P_CTRL" set device_name F1C200S-Miracast >>"$LOG" 2>&1 || true
    /usr/sbin/wpa_cli -i "$P2P_CTRL" set device_type 7-0050F204-1 >>"$LOG" 2>&1 || true
    /usr/sbin/wpa_cli -i "$P2P_CTRL" set config_methods virtual_push_button physical_display keypad >>"$LOG" 2>&1 || true
    /usr/sbin/wpa_cli -i "$P2P_CTRL" set wifi_display 1 >>"$LOG" 2>&1 || true
    /usr/sbin/wpa_cli -i "$P2P_CTRL" wfd_subelem_set 0 000600111c440032 >>"$LOG" 2>&1 || true
    /usr/sbin/wpa_cli -i "$P2P_CTRL" wfd_subelem_set 7 00020000 >>"$LOG" 2>&1 || true
    /usr/sbin/wpa_cli -i "$P2P_CTRL" p2p_flush >>"$LOG" 2>&1 || true
    /usr/sbin/wpa_cli -i "$P2P_CTRL" p2p_set listen_channel 1 >>"$LOG" 2>&1 || true
    /usr/sbin/wpa_cli -i "$P2P_CTRL" p2p_ext_listen 500 2000 >>"$LOG" 2>&1 || true
}

start_go() {
    log "start autonomous GO freq=$GO_FREQ"
    /usr/sbin/wpa_cli -i "$P2P_CTRL" p2p_group_remove p2p0 >>"$LOG" 2>&1 || true
    /usr/sbin/wpa_cli -i "$P2P_CTRL" p2p_group_add freq="$GO_FREQ" >>"$LOG" 2>&1 || true
    sleep 4
    ifconfig p2p0 192.168.49.1 netmask 255.255.255.0 up >>"$LOG" 2>&1 || true
    /usr/sbin/wpa_cli -i "$P2P_CTRL" status >>"$LOG" 2>&1 || true
    /usr/sbin/wpa_cli -i p2p0 status >>"$LOG" 2>&1 || true
    /usr/sbin/wpa_cli -i p2p0 p2p_get_passphrase >>"$LOG" 2>&1 || true
}

start_rtsp() {
    if [ ! -x /root/miracast_rtsp_sink ]; then
        log "missing /root/miracast_rtsp_sink"
        return 1
    fi
    rm -f "$RTSP_LOG"
    /root/miracast_rtsp_sink "$RTSP_LOG" &
    echo $! >/tmp/miracast_rtsp_sink.pid
    log "rtsp sink pid $(cat /tmp/miracast_rtsp_sink.pid), log $RTSP_LOG"
}

run_discovery_loop() {
    log "start p2p listen/find; open Android Cast/Wi-Fi Direct now"
    while :; do
        /usr/sbin/wpa_cli -i "$P2P_CTRL" p2p_find 8 >>"$LOG" 2>&1 || true
        sleep 9
        /usr/sbin/wpa_cli -i "$P2P_CTRL" p2p_stop_find >>"$LOG" 2>&1 || true
        /usr/sbin/wpa_cli -i "$P2P_CTRL" p2p_listen 8 >>"$LOG" 2>&1 || true
        sleep 9
        /usr/sbin/wpa_cli -i "$P2P_CTRL" p2p_peers >>"$LOG" 2>&1 || true
        /usr/sbin/wpa_cli -i "$P2P_CTRL" status >>"$LOG" 2>&1 || true
        ifconfig p2p0 192.168.49.1 netmask 255.255.255.0 up >>"$LOG" 2>&1 || true
    done
}

rm -f "$LOG"
log "manual Miracast/WFD sink probe"
stop_old
load_p2p_module || { log "failed to load p2p module"; exit 1; }
start_wpa || { log "failed to start wpa_supplicant"; exit 1; }
setup_wfd
start_go
start_rtsp || true
log "ifaces: $(ls /sys/class/net | tr '\n' ' ')"
log "wfd: $(/usr/sbin/wpa_cli -i "$P2P_CTRL" wfd_subelem_get 0 2>/dev/null)"
run_discovery_loop
