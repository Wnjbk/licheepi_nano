#!/bin/sh
# Stream each newly announced HLS TS segment directly into one Cedar FIFO.
set -eu
[ "$#" -eq 1 ] || { echo "usage: $0 <http-live-hls-playlist>" >&2; exit 2; }
URL=$1
MPLAYER=${MPLAYER_BIN:-/root/roms/tv/candidates/mplayer_cedar_hls_async_drain_20260827/mplayer-cedar.80baf63.async-drain.stripped}
MPLAYER_ARGS=${MPLAYER_ARGS:--nosound -vc cedarh264 -vo cedar_drm:fit -demuxer lavf}
ROOT=${RUN_ROOT:-/root/roms/tv/run}
POLL_SECONDS=${POLL_SECONDS:-1}
START_AT_LATEST=${START_AT_LATEST:-1}
SEEN_LIMIT=${SEEN_LIMIT:-16}
SEGMENT_RETRIES=${SEGMENT_RETRIES:-3}
RUN="$ROOT/hls-ts-live-relay-$$"
FIFO="/tmp/hls-ts-live-relay-$$.ts"
SEEN="$RUN/seen"
RELAY_PID=
cleanup() {
    [ -n "$RELAY_PID" ] && kill "$RELAY_PID" 2>/dev/null || true
    [ -n "$RELAY_PID" ] && wait "$RELAY_PID" 2>/dev/null || true
    rm -f "$FIFO"
    rm -rf "$RUN"
}
trap cleanup EXIT INT TERM
case "$URL" in http://*) ;; *) echo "plain HTTP HLS only" >&2; exit 2;; esac
mkdir -p "$RUN"
: > "$SEEN"
mkfifo "$FIFO"
BASE=${URL%/*}/
segment_url() { case "$1" in http://*) printf '%s\n' "$1";; *) printf '%s%s\n' "$BASE" "$1";; esac; }
refresh_segments() { wget -q -O "$RUN/playlist.next" "$URL" || return 1; tr -d '\r' < "$RUN/playlist.next" | sed -n '/^[^#][^[:space:]]*$/p' > "$RUN/segments.next"; test -s "$RUN/segments.next" || return 1; mv "$RUN/segments.next" "$RUN/segments"; rm -f "$RUN/playlist.next"; }
remember_segment() {
    printf '%s\n' "$1" >> "$SEEN"
    tail -n "$SEEN_LIMIT" "$SEEN" > "$SEEN.next"
    mv "$SEEN.next" "$SEEN"
}
fetch_segment() {
    tries=0
    while ! wget -q -O - "$(segment_url "$1")" >&3; do
        tries=$((tries + 1))
        [ "$tries" -ge "$SEGMENT_RETRIES" ] && return 1
        sleep "$POLL_SECONDS"
    done
}
relay() {
    # Keep this descriptor open across segments so MPlayer never sees EOF.
    exec 3>"$FIFO"
    first=1
    while :; do
        if refresh_segments; then
            if [ "$first" -eq 1 ]; then
                # Production live playback starts from the newest segment.
                if [ "$START_AT_LATEST" -eq 1 ]; then
                    tail -n 1 "$RUN/segments" > "$RUN/new"
                else
                    cp "$RUN/segments" "$RUN/new"
                fi
                first=0
            else
                : > "$RUN/new"
                while IFS= read -r segment; do
                    grep -Fqx "$segment" "$SEEN" || printf '%s\n' "$segment" >> "$RUN/new"
                done < "$RUN/segments"
            fi
            while IFS= read -r segment; do
                [ -n "$segment" ] || continue
                grep -Fqx "$segment" "$SEEN" && continue
                # Direct byte stream to the same FIFO, with no TS file staging.
                fetch_segment "$segment" && remember_segment "$segment" || true
            done < "$RUN/new"
        fi
        sleep "$POLL_SECONDS"
    done
}
relay &
RELAY_PID=$!
"$MPLAYER" $MPLAYER_ARGS "$FIFO"
