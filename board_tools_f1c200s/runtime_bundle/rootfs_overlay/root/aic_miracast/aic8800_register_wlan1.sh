#!/bin/sh
# Bring AIC8800 USB WiFi to wlan1 registration only. Does not start P2P/GO.
set -u
PATH=/sbin:/bin:/usr/sbin:/usr/bin:/root:/root/aic_miracast

IFACE=${IFACE:-wlan1}
LOAD_FW=${LOAD_FW:-/lib/modules/5.7.1/extra/aic_load_fw.ko}
FDRV=${FDRV:-/lib/modules/5.7.1/extra/aic8800_fdrv.ko}
FW_PATH=${FW_PATH:-/lib/firmware/aic8800D80}
AIC_DBG_LEVEL=${AIC_DBG_LEVEL:-0}
LOG=${LOG:-/root/aic_runtime/logs/aic8800_register_wlan1.log}

mkdir -p "$(dirname "$LOG")"
log(){ echo "[aic-reg] $(date '+%H:%M:%S' 2>/dev/null) $*" | tee -a "$LOG"; }
has_mod(){ grep -q "^$1 " /proc/modules 2>/dev/null; }
has_usb(){ lsusb 2>/dev/null | grep -qi "$1"; }
has_iface(){ ifconfig -a 2>/dev/null | grep -q "^$IFACE[[:space:]]" || [ -e "/sys/class/net/$IFACE" ]; }
show_status(){
    echo '--- usb ---'; lsusb 2>/dev/null || true
    echo '--- modules ---'; cat /proc/modules | grep -E 'aic|8723' || true
    echo '--- net ---'; ls /sys/class/net 2>/dev/null || true
    echo '--- wlan1 ---'; ifconfig "$IFACE" 2>&1 || true
}
stop_aic_users(){
    killall wpa24_aic_wfd_supplicant 2>/dev/null || true
    killall tiny_dhcpd_49 2>/dev/null || true
    killall miracast_sink_dump 2>/dev/null || true
    ps w | grep '[w]atch_aic_miracast.sh' | awk '{print $1}' | while read pid; do kill "$pid" 2>/dev/null || true; done
    ps w | grep '[d]mesg_watch.sh' | awk '{print $1}' | while read pid; do kill "$pid" 2>/dev/null || true; done
    ps w | grep '[m]em_watch.sh' | awk '{print $1}' | while read pid; do kill "$pid" 2>/dev/null || true; done
}
load_fw_to_8d83(){
    if has_usb 'a69c:8d83'; then
        log 'AIC already in 8d83 stage2'
        return 0
    fi
    if ! has_usb 'a69c:8d80'; then
        log 'ERROR: no AIC USB device a69c:8d80 or a69c:8d83 found'
        return 1
    fi
    log "8d80 detected; loading firmware with $LOAD_FW"
    rmmod aic8800_fdrv 2>/dev/null || true
    rmmod aic_load_fw 2>/dev/null || true
    insmod "$LOAD_FW" aic_fw_path="$FW_PATH" aicwf_dbg_level="$AIC_DBG_LEVEL" >>"$LOG" 2>&1 || {
        log 'ERROR: insmod aic_load_fw failed'
        return 1
    }
    i=0
    while [ "$i" -lt 45 ]; do
        if has_usb 'a69c:8d83'; then
            log "8d83 ready after ${i}s"
            sleep 2
            return 0
        fi
        i=$((i + 1))
        sleep 1
    done
    log 'ERROR: timeout waiting for 8d83'
    return 1
}
load_fdrv_for_wlan1(){
    if has_iface; then
        log "$IFACE already registered"
        return 0
    fi
    if ! has_usb 'a69c:8d83'; then
        log 'ERROR: 8d83 not present before fdrv load'
        return 1
    fi
    if ! has_mod aic_load_fw; then
        log "loading aic_load_fw exports from $LOAD_FW"
        insmod "$LOAD_FW" aic_fw_path="$FW_PATH" aicwf_dbg_level="$AIC_DBG_LEVEL" >>"$LOG" 2>&1 || true
        sleep 1
    fi
    rmmod aic8800_fdrv 2>/dev/null || true
    log "loading fdrv from $FDRV"
    insmod "$FDRV" aicwf_dbg_level="$AIC_DBG_LEVEL" >>"$LOG" 2>&1 || {
        log 'ERROR: insmod aic8800_fdrv failed'
        return 1
    }
    i=0
    while [ "$i" -lt 30 ]; do
        if has_iface; then
            log "$IFACE registered"
            return 0
        fi
        i=$((i + 1))
        sleep 1
    done
    log "ERROR: $IFACE not registered after fdrv load"
    return 1
}

case "${1:-start}" in
    loader)
        : >"$LOG"
        log "loader only iface=$IFACE"
        load_fw_to_8d83 || { show_status; exit 1; }
        show_status
        exit 0
        ;;
    fdrv)
        : >"$LOG"
        log "fdrv only iface=$IFACE"
        load_fdrv_for_wlan1 || { show_status; dmesg | grep -Ei 'aic|wlan1|Unknown symbol|probe|fail|timeout|Unable|panic|Oops' | tail -120; exit 1; }
        show_status
        exit 0
        ;;
    status)
        show_status
        exit 0
        ;;
    stop)
        stop_aic_users
        ifconfig "$IFACE" down 2>/dev/null || true
        rmmod aic8800_fdrv 2>/dev/null || true
        rmmod aic_load_fw 2>/dev/null || true
        show_status
        exit 0
        ;;
    start|register)
        : >"$LOG"
        log "start AIC wlan1 registration only iface=$IFACE"
        log "LOAD_FW=$LOAD_FW"
        log "FDRV=$FDRV"
        stop_aic_users
        load_fw_to_8d83 || { show_status; exit 1; }
        load_fdrv_for_wlan1 || { show_status; dmesg | grep -Ei 'aic|wlan1|Unknown symbol|probe|fail|timeout|Unable|panic|Oops' | tail -120; exit 1; }
        show_status
        exit 0
        ;;
    *)
        echo "Usage: $0 {start|register|status|stop}"
        exit 2
        ;;
esac
