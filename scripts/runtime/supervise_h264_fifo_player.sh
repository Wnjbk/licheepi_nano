#!/bin/sh
set -u
PATH=/sbin:/bin:/usr/sbin:/usr/bin:/root:/root/aic_miracast
FIFO=${FIFO:-/tmp/aic_h264_live.fifo}
PLAYER=${PLAYER:-/root/cedar_drm_player.yuvlive}
WIDTH=${WIDTH:-720}
HEIGHT=${HEIGHT:-480}
FPS=${FPS:-30}
LOG=${LOG:-/tmp/aic_live_player.log}
RUN_DIR=${RUN_DIR:-/root/aic_runtime/run}
mkdir -p "$RUN_DIR" /tmp
stop_old()
{
    ps w | grep -E '/root/cedar_drm_player|play_h264_fifo_to_lcd.sh' | grep -v grep | awk '{print $1}' | while read p; do [ -n "$p" ] && kill "$p" 2>/dev/null || true; done
    sleep 1
    ps w | grep -E '/root/cedar_drm_player|play_h264_fifo_to_lcd.sh' | grep -v grep | awk '{print $1}' | while read p; do [ -n "$p" ] && kill -9 "$p" 2>/dev/null || true; done
}
case "${1:-start}" in
  stop) stop_old; rm -f "$FIFO"; exit 0 ;;
esac
stop_old
while true; do
    rm -f "$FIFO"
    echo "[fifo-supervisor] $(date '+%H:%M:%S' 2>/dev/null) start player" >>/tmp/aic_live_supervisor.log
    PLAYER="$PLAYER" WIDTH="$WIDTH" HEIGHT="$HEIGHT" FPS="$FPS" FIFO="$FIFO" LOG="$LOG" CEDAR_VIEW_STRETCH=${CEDAR_VIEW_STRETCH:-1}         /root/aic_miracast/play_h264_fifo_to_lcd.sh >>/tmp/aic_live_player.stdout 2>&1
    rc=$?
    echo "[fifo-supervisor] $(date '+%H:%M:%S' 2>/dev/null) player exited rc=$rc; restart" >>/tmp/aic_live_supervisor.log
    sleep 1
done
