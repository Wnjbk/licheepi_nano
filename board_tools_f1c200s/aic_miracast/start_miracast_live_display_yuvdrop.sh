#!/bin/sh
set -u
PATH=/sbin:/bin:/usr/sbin:/usr/bin:/root:/root/aic_miracast
BASE=${BASE:-/root/aic_miracast}
FIFO=${FIFO:-/tmp/aic_h264_live.fifo}
PLAYER=${PLAYER:-/root/cedar_drm_player.yuvdrop}
WIDTH=${WIDTH:-720}
HEIGHT=${HEIGHT:-480}
FPS=${FPS:-30}
LOG=${LOG:-/dev/null}
GO_FREQ=${GO_FREQ:-5805}
SINK_BIN=${SINK_BIN:-/root/aic_miracast/miracast_sink_dump.fifo_guard}
CAPTURE_MODE=fifo

kill_match()
{
    pat="$1"
    ps w | grep "$pat" | grep -v grep | awk '{print $1}' | while read pid; do [ -n "$pid" ] && kill "$pid" 2>/dev/null || true; done
    sleep 1
    ps w | grep "$pat" | grep -v grep | awk '{print $1}' | while read pid; do [ -n "$pid" ] && kill -9 "$pid" 2>/dev/null || true; done
}

case "${1:-start}" in
  stop)
    "$BASE/start_miracast_go_v25.sh" stop 2>/dev/null || true
    kill_match '/root/cedar_drm_player'
    kill_match 'play_h264_fifo_to_lcd.sh'
    rm -f "$FIFO"
    exit 0
    ;;
esac

[ -x "$PLAYER" ] || { echo "missing player: $PLAYER" >&2; exit 1; }
[ -x "$SINK_BIN" ] || { echo "missing sink: $SINK_BIN" >&2; exit 1; }
[ -e /sys/class/net/wlan1 ] || { echo "wlan1 missing; run $BASE/register_aic_wlan1_v25.sh start first" >&2; exit 1; }

"$0" stop 2>/dev/null || true
rm -f "$FIFO"
PLAYER="$PLAYER" WIDTH="$WIDTH" HEIGHT="$HEIGHT" FPS="$FPS" FIFO="$FIFO" LOG="$LOG" CEDAR_VIEW_STRETCH=${CEDAR_VIEW_STRETCH:-1}     "$BASE/play_h264_fifo_to_lcd.sh" >/tmp/aic_live_player.stdout 2>&1 &
echo $! >/root/aic_runtime/run/aic_live_player_script.pid

i=0
while [ "$i" -lt 20 ]; do [ -p "$FIFO" ] && break; i=$((i+1)); sleep 1; done
[ -p "$FIFO" ] || { echo "fifo not ready: $FIFO" >&2; exit 1; }

echo "[live-display] player ready fifo=$FIFO sink=$SINK_BIN"
SINK_BIN="$SINK_BIN" CAPTURE_MODE=fifo LIVE_FIFO="$FIFO" OUT_DIR=/tmp GO_FREQ="$GO_FREQ"     "$BASE/start_miracast_go_v25.sh" start
