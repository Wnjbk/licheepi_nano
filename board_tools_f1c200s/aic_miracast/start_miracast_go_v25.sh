#!/bin/sh
# Start Miracast GO on an already registered AIC8800 wlan1.
# Driver registration is intentionally not performed here.
set -u

PATH=/sbin:/bin:/usr/sbin:/usr/bin:/root:/root/aic_miracast

IFACE=${IFACE:-wlan1}
BASE=${BASE:-/root/aic_miracast}
V25_DIR=${V25_DIR:-$BASE/candidates/aic8800_rx_msg_clamp_only_20260802_v25}
LOAD_FW=${LOAD_FW:-/root/aic_miracast/candidates/aic8800_rx_lowmem_threshold1024_20260802_v18/aic_load_fw.ko}
FDRV=${FDRV:-$V25_DIR/aic8800_fdrv/aic8800_fdrv.ko}
ARCHIVE_DIR=${ARCHIVE_DIR:-/root/aic_runtime}
CAPTURE_MODE=${CAPTURE_MODE:-h264}
OUT_DIR=${OUT_DIR:-/root/roms/video}
LIVE_FIFO=${LIVE_FIFO:-/tmp/aic_h264_live.fifo}
AIC_DBG_LEVEL=${AIC_DBG_LEVEL:-0}
LOWMEM_TUNE=${LOWMEM_TUNE:-1}
STOP_GMENU=${STOP_GMENU:-1}
GO_FREQ=${GO_FREQ:-5805}
LOG=${LOG:-/root/aic_runtime/logs/start_miracast_go_v25.log}

mkdir -p "$(dirname "$LOG")"
: >"$LOG"

log()
{
    echo "[aic-v25-go] $(date '+%H:%M:%S' 2>/dev/null) $*" | tee -a "$LOG"
}

need_iface()
{
    [ -e "/sys/class/net/$IFACE" ] || ifconfig -a 2>/dev/null | grep -q "^$IFACE[[:space:]]"
}

status()
{
    echo "--- usb ---"
    lsusb 2>/dev/null || true
    echo "--- modules ---"
    cat /proc/modules | grep -E 'aic|8723' || true
    echo "--- $IFACE ---"
    ifconfig "$IFACE" 2>&1 || true
    echo "--- miracast ---"
    "$BASE/start_aic_miracast_cdump_board.sh" status 2>/dev/null || true
}

case "${1:-start}" in
    start)
        if ! need_iface; then
            log "ERROR: $IFACE is not registered. Run:"
            log "  /root/aic_miracast/register_aic_wlan1_v25.sh start"
            status
            exit 1
        fi

        [ -f "$LOAD_FW" ] || { log "ERROR: missing $LOAD_FW"; exit 1; }
        [ -f "$FDRV" ] || { log "ERROR: missing $FDRV"; exit 1; }
        mkdir -p "$OUT_DIR" || { log "ERROR: cannot create $OUT_DIR"; exit 1; }

        log "start Miracast GO only; iface=$IFACE freq=$GO_FREQ capture=$CAPTURE_MODE out=$OUT_DIR"
        echo '1 1 1 1' > /proc/sys/kernel/printk 2>/dev/null || true
        killall dmesg_watch.sh 2>/dev/null || true
        killall mem_watch.sh 2>/dev/null || true
        killall klogd 2>/dev/null || true

        ARCHIVE_DIR="$ARCHIVE_DIR" \
        CAPTURE_MODE="$CAPTURE_MODE" \
        OUT_DIR="$OUT_DIR" \
        LIVE_FIFO="$LIVE_FIFO" \
        AIC_DBG_LEVEL="$AIC_DBG_LEVEL" \
        LOWMEM_TUNE="$LOWMEM_TUNE" \
        STOP_GMENU="$STOP_GMENU" \
        GO_FREQ="$GO_FREQ" \
        LOAD_FW="$LOAD_FW" \
        FDRV="$FDRV" \
        "$BASE/start_aic_miracast_cdump_board.sh" start

        killall dmesg_watch.sh 2>/dev/null || true
        killall mem_watch.sh 2>/dev/null || true
        echo '1 1 1 1' > /proc/sys/kernel/printk 2>/dev/null || true
        dmesg -c >/dev/null 2>&1 || true
        ;;
    stop)
        "$BASE/start_aic_miracast_cdump_board.sh" stop
        ;;
    rearm)
        "$BASE/start_aic_miracast_cdump_board.sh" rearm
        ;;
    status)
        status
        ;;
    *)
        echo "Usage: $0 {start|stop|rearm|status}"
        exit 2
        ;;
esac
