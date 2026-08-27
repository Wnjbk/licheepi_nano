#!/bin/sh
set -eu
[ "$#" -eq 1 ] || { echo "usage: $0 <http-hls-vod-playlist>" >&2; exit 2; }
URL=$1
MPLAYER=${MPLAYER_BIN:-/root/roms/tv/candidates/mplayer_cedar_hls_async_drain_20260827/mplayer-cedar.80baf63.async-drain.stripped}
ROOT=${RUN_ROOT:-/root/roms/tv/run}
PREFETCH=${PREFETCH_SEGMENTS:-3}
RUN="$ROOT/hls-ts-prefetch-$$"
FIFO="/tmp/hls-ts-prefetch-$$.ts"
DL= WR=
    rm -f "$FIFO"
cleanup() { [ -n "$DL" ] && kill "$DL" 2>/dev/null || true; [ -n "$WR" ] && kill "$WR" 2>/dev/null || true; rm -rf "$RUN"; }
trap cleanup EXIT INT TERM
case "$URL" in http://*) ;; *) echo "HTTP VOD only" >&2; exit 2;; esac
mkdir -p "$RUN"
wget -q -O "$RUN/playlist.m3u8" "$URL"
sed -n '/^[^#][^[:space:]]*$/p' "$RUN/playlist.m3u8" > "$RUN/segments"
COUNT=$(wc -l < "$RUN/segments")
[ "$COUNT" -gt 0 ] || { echo "no media segments" >&2; exit 1; }
BASE=${URL%/*}/
mkfifo "$FIFO"
printf '%s\n' -1 > "$RUN/consumed"
segurl() { n=$1; s=$(sed -n "$((n + 1))p" "$RUN/segments"); case "$s" in http://*) echo "$s";; *) echo "$BASE$s";; esac; }
download() { n=$1; p="$RUN/$n.part"; f="$RUN/$n.ts"; wget -q -O "$p" "$(segurl "$n")" && test -s "$p" && mv "$p" "$f"; }
downloader() { n=0; while [ "$n" -lt "$COUNT" ]; do c=$(cat "$RUN/consumed"); while [ "$n" -gt "$((c + PREFETCH))" ]; do sleep 1; c=$(cat "$RUN/consumed"); done; download "$n" || { echo "$n" > "$RUN/failed"; exit 1; }; n=$((n + 1)); done; }
writer() { exec 3>"$FIFO"; n=0; while [ "$n" -lt "$COUNT" ]; do f="$RUN/$n.ts"; while [ ! -s "$f" ]; do [ -f "$RUN/failed" ] && exit 1; sleep 1; done; cat "$f" >&3; rm -f "$f"; echo "$n" > "$RUN/consumed"; n=$((n + 1)); done; }
downloader & DL=$!
writer & WR=$!
"$MPLAYER" -nosound -vc cedarh264 -vo cedar_drm:fit -demuxer lavf "$FIFO"