#!/bin/sh
set -u

PATH=/sbin:/bin:/usr/sbin:/usr/bin:/root:/root/aic_miracast

FIFO=${FIFO:-/tmp/aic_h264_live.fifo}
LOG_DIR=${LOG_DIR:-/root/aic_runtime/logs}
LOG=${LOG:-$LOG_DIR/h264_fifo_inject.log}
LOOP=${LOOP:-0}

mkdir -p "$LOG_DIR"

log()
{
    echo "[h264-fifo-inject] $(date '+%H:%M:%S' 2>/dev/null) $*" | tee -a "$LOG"
}

usage()
{
    echo "usage: $0 /path/video.h264"
    echo "       FIFO=/tmp/aic_h264_live.fifo $0 /path/video.mp4"
    echo
    echo "raw .h264/.264 streams are copied directly."
    echo "mp4/mkv extraction requires ffmpeg on the board."
}

[ $# -ge 1 ] || {
    usage >&2
    exit 2
}

IN=$1

[ -f "$IN" ] || {
    echo "input not found: $IN" >&2
    exit 1
}

[ -p "$FIFO" ] || {
    echo "fifo not found: $FIFO" >&2
    echo "start player first: /root/aic_miracast/play_h264_fifo_to_lcd.sh" >&2
    exit 1
}

stream_once()
{
    case "$IN" in
        *.h264|*.H264|*.264)
            log "inject raw h264 input=$IN fifo=$FIFO"
            cat "$IN" >"$FIFO"
            ;;
        *)
            if command -v ffmpeg >/dev/null 2>&1; then
                log "extract h264 with ffmpeg input=$IN fifo=$FIFO"
                ffmpeg -loglevel error -i "$IN" -an -c:v copy -bsf:v h264_mp4toannexb -f h264 - >"$FIFO"
            else
                echo "container input needs ffmpeg, but ffmpeg is not installed: $IN" >&2
                return 1
            fi
            ;;
    esac
}

if [ "$LOOP" = "1" ]; then
    while :; do
        stream_once || exit $?
    done
else
    stream_once
fi

rc=$?
log "inject exit rc=$rc"
exit "$rc"
