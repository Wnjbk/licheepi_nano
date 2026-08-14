#!/bin/sh
# Register AIC8800 as wlan1 using the v25 Miracast module pair.
# This script does not start P2P, DHCP, RTSP, or Miracast.
set -u

PATH=/sbin:/bin:/usr/sbin:/usr/bin:/root:/root/aic_miracast

IFACE=${IFACE:-wlan1}
BASE=${BASE:-/root/aic_miracast}
V25_DIR=${V25_DIR:-$BASE/candidates/aic8800_rx_msg_clamp_only_20260802_v25}
BASELINE_LOAD_FW=${BASELINE_LOAD_FW:-/lib/modules/5.7.1/extra/aic_load_fw.ko}
LOAD_FW=${LOAD_FW:-/root/aic_miracast/candidates/aic8800_rx_lowmem_threshold1024_20260802_v18/aic_load_fw.ko}
FDRV=${FDRV:-$V25_DIR/aic8800_fdrv/aic8800_fdrv.ko}
FW_PATH=${FW_PATH:-/lib/firmware/aic8800D80}
AIC_DBG_LEVEL=${AIC_DBG_LEVEL:-0}
LOG=${LOG:-/root/aic_runtime/logs/register_aic_wlan1_v25.log}

mkdir -p "$(dirname "$LOG")"
: >"$LOG"

log()
{
    echo "[aic-v25-reg] $(date '+%H:%M:%S' 2>/dev/null) $*" | tee -a "$LOG"
}

has_usb()
{
    lsusb 2>/dev/null | grep -qi "$1"
}

has_iface()
{
    [ -e "/sys/class/net/$IFACE" ] || ifconfig -a 2>/dev/null | grep -q "^$IFACE[[:space:]]"
}

show_status()
{
    echo "--- usb ---"
    lsusb 2>/dev/null || true
    echo "--- modules ---"
    cat /proc/modules | grep -E 'aic|8723' || true
    echo "--- net ---"
    ls /sys/class/net 2>/dev/null || true
    echo "--- $IFACE ---"
    ifconfig "$IFACE" 2>&1 || true
}

stop_aic_runtime()
{
    killall wpa24_aic_wfd_supplicant 2>/dev/null || true
    killall tiny_dhcpd_49 2>/dev/null || true
    killall miracast_sink_dump 2>/dev/null || true
    ps w | grep '[w]atch_aic_miracast.sh' | awk '{print $1}' | while read pid; do
        kill "$pid" 2>/dev/null || true
    done
}

verify_files()
{
    [ -f "$BASELINE_LOAD_FW" ] || { log "ERROR: missing baseline loader $BASELINE_LOAD_FW"; return 1; }
    [ -f "$LOAD_FW" ] || { log "ERROR: missing v25 loader $LOAD_FW"; return 1; }
    [ -f "$FDRV" ] || { log "ERROR: missing v25 fdrv $FDRV"; return 1; }
    log "module md5:"
    md5sum "$LOAD_FW" "$FDRV" | tee -a "$LOG"
}

download_fw_if_needed()
{
    if has_usb 'a69c:8d83'; then
        log "AIC already at 8d83"
        return 0
    fi
    if ! has_usb 'a69c:8d80'; then
        log "ERROR: no AIC USB device; need a69c:8d80 or a69c:8d83"
        return 1
    fi

    # Cold 8d80 firmware download is more reliable with the baseline loader.
    log "8d80 detected; firmware download with baseline loader"
    rmmod aic8800_fdrv 2>/dev/null || true
    rmmod aic_load_fw 2>/dev/null || true
    insmod "$BASELINE_LOAD_FW" aic_fw_path="$FW_PATH" aicwf_dbg_level="$AIC_DBG_LEVEL" >>"$LOG" 2>&1 || {
        log "ERROR: baseline aic_load_fw insmod failed"
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

    log "ERROR: timeout waiting for 8d83"
    return 1
}

load_v25_pair()
{
    if ! has_usb 'a69c:8d83'; then
        log "ERROR: cannot load v25 fdrv without 8d83"
        return 1
    fi

    if has_iface; then
        log "$IFACE already exists; stop/start not needed"
        return 0
    fi

    rmmod aic8800_fdrv 2>/dev/null || true
    rmmod aic_load_fw 2>/dev/null || true
    sleep 1

    log "loading v25 aic_load_fw exports"
    insmod "$LOAD_FW" aic_fw_path="$FW_PATH" aicwf_dbg_level="$AIC_DBG_LEVEL" >>"$LOG" 2>&1 || {
        log "ERROR: v25 aic_load_fw insmod failed"
        return 1
    }
    sleep 1

    log "loading v25 fdrv"
    insmod "$FDRV" aicwf_dbg_level="$AIC_DBG_LEVEL" >>"$LOG" 2>&1 || {
        log "ERROR: v25 aic8800_fdrv insmod failed"
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

    log "ERROR: $IFACE not registered after v25 fdrv load"
    return 1
}

case "${1:-start}" in
    start|register)
        log "start register $IFACE with v25"
        verify_files || { show_status; exit 1; }
        stop_aic_runtime
        download_fw_if_needed || { show_status; exit 1; }
        load_v25_pair || {
            show_status
            dmesg | grep -Ei 'aic|wlan1|Unknown symbol|probe|fail|timeout|Unable|panic|Oops' | tail -160
            exit 1
        }
        show_status
        ;;
    status)
        show_status
        ;;
    stop)
        stop_aic_runtime
        ifconfig "$IFACE" down 2>/dev/null || true
        rmmod aic8800_fdrv 2>/dev/null || true
        rmmod aic_load_fw 2>/dev/null || true
        show_status
        ;;
    *)
        echo "Usage: $0 {start|register|status|stop}"
        exit 2
        ;;
esac
