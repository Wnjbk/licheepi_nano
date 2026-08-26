#!/bin/sh

set -u

PATH=/sbin:/bin:/usr/sbin:/usr/bin:/root
LOG=/tmp/use_8723_p2p_concurrent.log
IFACE=wlan0
ADDR=10.0.0.107
MASK=255.255.255.0
GW=10.0.0.1
WPA_CONF=/etc/wpa_supplicant.conf
BASE_OPTS="rtw_power_mgnt=0 rtw_ips_mode=0 rtw_smart_ps=0 rtw_low_power=0 rtw_enusbss=0 rtw_btcoex_enable=0 rtw_ht_enable=1 rtw_bw_mode=1 rtw_wmm_enable=1 rtw_ampdu_enable=1 rtw_usb_rxagg_mode=0 rtw_wifi_spec=0 rtw_channel_plan=0x7F rtw_antdiv_cfg=0 rtw_ant_num=1 rtw_adaptivity_en=0"
P2P_MOD=/lib/modules/5.7.1/extra/8723bu_p2p_concurrent.ko
BASE_MOD=/lib/modules/5.7.1/extra/8723bu.ko

log() {
    echo "[8723-p2p] $*" | tee -a "$LOG"
}

load_mod() {
    mod="$1"
    log "insmod $mod"
    /sbin/insmod "$mod" $BASE_OPTS ifname=wlan%d if2name=p2p%d >>"$LOG" 2>&1
}

wait_iface() {
    i=0
    while [ "$i" -lt 30 ]; do
        [ -d /sys/class/net/$IFACE ] && return 0
        sleep 0.2
        i=$((i + 1))
    done
    return 1
}

restore_net() {
    /sbin/ifconfig "$IFACE" up >>"$LOG" 2>&1 || true
    mkdir -p /var/run /var/run/wpa_supplicant /run/wpa_supplicant
    log "start wpa_supplicant"
    /usr/sbin/wpa_supplicant -B -D nl80211,wext -i "$IFACE" -c "$WPA_CONF" >>"$LOG" 2>&1 || true
    i=0
    while [ "$i" -lt 20 ]; do
        /usr/sbin/wpa_cli -i "$IFACE" status 2>/dev/null | grep -q 'wpa_state=COMPLETED' && break
        sleep 0.5
        i=$((i + 1))
    done
    /sbin/ifconfig "$IFACE" "$ADDR" netmask "$MASK" up >>"$LOG" 2>&1 || true
    /sbin/route del default >>"$LOG" 2>&1 || true
    /sbin/route add default gw "$GW" "$IFACE" >>"$LOG" 2>&1 || true
}

rm -f "$LOG"
log "switch to p2p/concurrent test module"
kill "$(cat /tmp/miracast_probe.pid 2>/dev/null)" 2>/dev/null || true
killall wpa_cli 2>/dev/null || true
/usr/sbin/wpa_cli -i "$IFACE" terminate >>"$LOG" 2>&1 || killall wpa_supplicant 2>/dev/null || true
sleep 1
/sbin/ifconfig "$IFACE" down >>"$LOG" 2>&1 || true
/sbin/rmmod 8723bu >>"$LOG" 2>&1 || true
sleep 1

if load_mod "$P2P_MOD" && wait_iface; then
    log "p2p/concurrent module loaded"
else
    log "p2p/concurrent module failed, fallback to base module"
    /sbin/rmmod 8723bu >>"$LOG" 2>&1 || true
    load_mod "$BASE_MOD" || true
    wait_iface || true
fi

restore_net
log "ifaces: $(ls /sys/class/net | tr '\n' ' ')"
cat /proc/modules | grep 8723bu >>"$LOG" 2>&1 || true
/usr/sbin/wpa_cli -i "$IFACE" status >>"$LOG" 2>&1 || true
log "done"
