#!/bin/sh
set -u

PATH=/sbin:/bin:/usr/sbin:/usr/bin:/root:/root/aic_miracast

FIFO=${FIFO:-/tmp/aic_h264_live.fifo}
WIDTH=${WIDTH:-640}
HEIGHT=${HEIGHT:-360}
FPS=${FPS:-30}
PLAYER=${PLAYER:-/root/cedar_drm_player}
LOG_DIR=${LOG_DIR:-/root/aic_runtime/logs}
RUN_DIR=${RUN_DIR:-/root/aic_runtime/run}
LOG=${LOG:-$LOG_DIR/h264_fifo_player.log}
STOP_GMENU=${STOP_GMENU:-1}
LOWMEM=${LOWMEM:-1}

mkdir -p "$LOG_DIR" "$RUN_DIR"

log()
{
    echo "[h264-fifo-player] $(date '+%H:%M:%S' 2>/dev/null) $*" | tee -a "$LOG"
}

cleanup()
{
    [ -n "${PLAYER_PID:-}" ] && kill "$PLAYER_PID" 2>/dev/null || true
    [ -w /sys/class/graphics/fb0/blank ] && echo 0 > /sys/class/graphics/fb0/blank 2>/dev/null || true
}
trap cleanup INT TERM EXIT

case "${1:-start}" in
    stop)
        killall cedar_drm_player 2>/dev/null || true
        rm -f "$FIFO"
        exit 0
        ;;
esac

[ -x "$PLAYER" ] || {
    echo "player not executable: $PLAYER" >&2
    exit 1
}

[ -x /root/load_cedar.sh ] && /root/load_cedar.sh >>"$LOG" 2>&1 || true

if [ "$STOP_GMENU" = "1" ]; then
    killall gmenu2x 2>/dev/null || true
    killall run_gmenu2x.sh 2>/dev/null || true
fi

if [ "$LOWMEM" = "1" ]; then
    killall bluetoothd 2>/dev/null || true
    killall btmon 2>/dev/null || true
    killall dbus-daemon 2>/dev/null || true
    sync 2>/dev/null || true
    echo 3 >/proc/sys/vm/drop_caches 2>/dev/null || true
fi

rm -f "$FIFO"
mkfifo "$FIFO" || {
    echo "mkfifo failed: $FIFO" >&2
    exit 1
}
chmod 666 "$FIFO" 2>/dev/null || true

log "start player fifo=$FIFO mode=${WIDTH}x${HEIGHT}@${FPS}"
"$PLAYER" --raw-h264 "$WIDTH" "$HEIGHT" "$FPS" "$FIFO" >>"$LOG" 2>&1 &
PLAYER_PID=$!
echo "$PLAYER_PID" >"$RUN_DIR/h264_fifo_player.pid"

log "ready pid=$PLAYER_PID fifo=$FIFO"
wait "$PLAYER_PID"
rc=$?
log "player exit rc=$rc"
exit "$rc"
