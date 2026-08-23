#!/bin/sh
set -u

PATH=/sbin:/bin:/usr/sbin:/usr/bin:/root:/root/aic_miracast
BASE=${BASE:-/root/aic_miracast}
GO_SCRIPT=${GO_SCRIPT:-$BASE/candidates/scanstop_20260817/start_aic_miracast_cdump_board.scanstop.sh}
FIFO=${FIFO:-/tmp/aic_h264_live.fifo}
PLAYER=${PLAYER:-/root/cedar_drm_player.yuvcrop}
SINK_BIN=${SINK_BIN:-$BASE/miracast_sink_dump.lowest}

case "${1:-start}" in
stop)
    "$GO_SCRIPT" stop 2>/dev/null || true
    "$BASE/supervise_h264_fifo_player.sh" stop 2>/dev/null || true
    rm -f "$FIFO"
    exit 0
    ;;
esac

[ -x "$GO_SCRIPT" ] || { echo "missing GO candidate: $GO_SCRIPT" >&2; exit 1; }
[ -x "$PLAYER" ] || { echo "missing player: $PLAYER" >&2; exit 1; }
[ -x "$SINK_BIN" ] || { echo "missing sink: $SINK_BIN" >&2; exit 1; }
[ -e /sys/class/net/wlan1 ] || { echo "wlan1 missing" >&2; exit 1; }

"$BASE/supervise_h264_fifo_player.sh" stop 2>/dev/null || true
rm -f "$FIFO" /tmp/aic_live_player.log /tmp/aic_live_player.stdout
PLAYER="$PLAYER" WIDTH=640 HEIGHT=480 FPS=60 FIFO="$FIFO" \
LOG=/tmp/aic_live_player.log CEDAR_NO_PACE=1 CEDAR_VIEW_STRETCH=0 \
CEDAR_VIEW_CENTER_CROP=0 \
"$BASE/supervise_h264_fifo_player.sh" start >/tmp/aic_live_supervisor.stdout 2>&1 &

i=0
while [ "$i" -lt 20 ]; do
    [ -p "$FIFO" ] && break
    i=$((i + 1))
    sleep 1
done
[ -p "$FIFO" ] || { echo "FIFO not ready: $FIFO" >&2; exit 1; }

SINK_BIN="$SINK_BIN" CAPTURE_MODE=fifo LIVE_FIFO="$FIFO" OUT_DIR=/tmp \
"$GO_SCRIPT" start
