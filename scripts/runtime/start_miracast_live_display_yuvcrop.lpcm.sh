#!/bin/sh
set -u

PATH=/sbin:/bin:/usr/sbin:/usr/bin:/root:/root/aic_miracast
BASE=${BASE:-/root/aic_miracast}
GO_SCRIPT=${GO_SCRIPT:-$BASE/candidates/scanstop_20260817/start_aic_miracast_cdump_board.scanstop.sh}
FIFO=${FIFO:-/tmp/aic_h264_live.fifo}
AUDIO_FIFO=${AUDIO_FIFO:-/tmp/aic_lpcm_live.fifo}
APLAY_PID_FILE=${APLAY_PID_FILE:-/tmp/aic_lpcm_aplay.pid}
APLAY_LOG=${APLAY_LOG:-/tmp/aic_lpcm_aplay.log}
APLAYER=${APLAYER:-/usr/bin/aplay}
PLAYER=${PLAYER:-/root/cedar_drm_player.yuvcrop}
SINK_BIN=${SINK_BIN:-$BASE/miracast_sink_dump.lowest}

stop_audio()
{
    if [ -r "$APLAY_PID_FILE" ]; then
        pid=$(cat "$APLAY_PID_FILE" 2>/dev/null || true)
        [ -n "$pid" ] && kill "$pid" 2>/dev/null || true
        rm -f "$APLAY_PID_FILE"
    fi
}

case "${1:-start}" in
stop)
    "$GO_SCRIPT" stop 2>/dev/null || true
    stop_audio
    "$BASE/supervise_h264_fifo_player.sh" stop 2>/dev/null || true
    rm -f "$FIFO" "$AUDIO_FIFO" "$APLAY_LOG"
    exit 0
    ;;
esac

[ -x "$GO_SCRIPT" ] || { echo "missing GO candidate: $GO_SCRIPT" >&2; exit 1; }
[ -x "$PLAYER" ] || { echo "missing player: $PLAYER" >&2; exit 1; }
[ -x "$SINK_BIN" ] || { echo "missing sink: $SINK_BIN" >&2; exit 1; }
[ -x "$APLAYER" ] || { echo "missing aplay: $APLAYER" >&2; exit 1; }
[ -e /sys/class/net/wlan1 ] || { echo "wlan1 missing" >&2; exit 1; }

"$BASE/supervise_h264_fifo_player.sh" stop 2>/dev/null || true
stop_audio
rm -f "$FIFO" "$AUDIO_FIFO" "$APLAY_LOG" /tmp/aic_live_player.log /tmp/aic_live_player.stdout
mkfifo "$AUDIO_FIFO" || { echo "audio FIFO create failed: $AUDIO_FIFO" >&2; exit 1; }
amixer -c 2 set Headphone 50% unmute >/dev/null 2>&1 || true
"$APLAYER" -D hw:2,0 -t raw -f S16_BE -c 2 -r 48000 \
    --period-time 20000 --buffer-time 200000 "$AUDIO_FIFO" >"$APLAY_LOG" 2>&1 &
echo $! >"$APLAY_PID_FILE"
PLAYER="$PLAYER" WIDTH=640 HEIGHT=480 FPS=60 FIFO="$FIFO" \
LOG=/tmp/aic_live_player.log CEDAR_NO_PACE=1 CEDAR_VIEW_STRETCH=0 \
CEDAR_VIEW_CENTER_CROP=1 CEDAR_VIEW_X=0 CEDAR_VIEW_Y=0 \
CEDAR_VIEW_W=384 CEDAR_VIEW_H=640 \
"$BASE/supervise_h264_fifo_player.sh" start >/tmp/aic_live_supervisor.stdout 2>&1 &

i=0
while [ "$i" -lt 20 ]; do
    [ -p "$FIFO" ] && break
    i=$((i + 1))
    sleep 1
done
[ -p "$FIFO" ] || { echo "FIFO not ready: $FIFO" >&2; exit 1; }

SINK_BIN="$SINK_BIN" CAPTURE_MODE=fifo LIVE_FIFO="$FIFO" AUDIO_FIFO="$AUDIO_FIFO" OUT_DIR=/tmp \
"$GO_SCRIPT" start || { stop_audio; exit 1; }
