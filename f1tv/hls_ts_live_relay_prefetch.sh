#!/bin/sh
# Shell-only HLS relay with a bounded one-segment lookahead queue.
set -eu
[ "$#" -eq 1 ] || { echo "usage: $0 <http-live-hls-playlist>" >&2; exit 2; }
URL=$1
MPLAYER=${MPLAYER_BIN:-/root/roms/tv/candidates/mplayer_cedar_hls_async_drain_20260827/mplayer-cedar.80baf63.async-drain.stripped}
# Small RAM input cache smooths HLS segment handoff without disk buffering.
MPLAYER_ARGS=${MPLAYER_ARGS:--cache 4096 -nosound -vc cedarh264 -vo cedar_drm:fit -demuxer lavf}
# Runtime queue only: never write HLS media or bookkeeping to persistent flash.
ROOT=/dev/shm
POLL_SECONDS=${POLL_SECONDS:-1}
START_AT_LATEST=${START_AT_LATEST:-1}
SEEN_LIMIT=${SEEN_LIMIT:-16}
SEGMENT_RETRIES=${SEGMENT_RETRIES:-2}
PREFETCH_SEGMENTS=${PREFETCH_SEGMENTS:-2}
# Do not start on the playlist's still-being-written tail segment. This trades
# one segment of latency for a complete initial queue and continuous playback.
START_BEHIND_SEGMENTS=${START_BEHIND_SEGMENTS:-1}
RUN="$ROOT/hls-ts-live-relay-prefetch-$$"
FIFO="$RUN/input.ts"
SEEN="$RUN/seen"
RELAY_PID=
MPLAYER_PID=
cleanup() {
    trap - EXIT INT TERM
    [ -n "$MPLAYER_PID" ] && kill "$MPLAYER_PID" 2>/dev/null || true
    [ -n "$RELAY_PID" ] && kill "$RELAY_PID" 2>/dev/null || true
    [ -n "$MPLAYER_PID" ] && wait "$MPLAYER_PID" 2>/dev/null || true
    [ -n "$RELAY_PID" ] && wait "$RELAY_PID" 2>/dev/null || true
    rm -f "$FIFO"
    rm -rf "$RUN"
}
trap cleanup EXIT INT TERM
case "$URL" in http://*) ;; *) echo "plain HTTP HLS only" >&2; exit 2;; esac
mount | grep -Eq '(^tmpfs on /dev/shm | on /dev/shm type tmpfs)' || {
    echo "/dev/shm is not a tmpfs; refusing persistent HLS cache" >&2
    exit 1
}
mkdir -p "$RUN/q"
: > "$SEEN"
mkfifo "$FIFO"
BASE=${URL%/*}/
segment_url() { case "$1" in http://*) printf '%s\n' "$1";; *) printf '%s%s\n' "$BASE" "$1";; esac; }
refresh_segments() {
    wget -q -O "$RUN/playlist.next" "$URL" || return 1
    tr -d '\r' < "$RUN/playlist.next" | sed -n '/^[^#][^[:space:]]*$/p' > "$RUN/segments.next"
    test -s "$RUN/segments.next" || return 1
    mv "$RUN/segments.next" "$RUN/segments"
    rm -f "$RUN/playlist.next"
}
remember_segment() {
    printf '%s\n' "$1" >> "$SEEN"
    tail -n "$SEEN_LIMIT" "$SEEN" > "$RUN/seen.next"
    mv "$RUN/seen.next" "$SEEN"
}
fetch_segment() {
    tries=0
    while ! wget -q -T 5 -t 1 -O "$2.part" "$(segment_url "$1")"; do
        rm -f "$2.part"
        tries=$((tries + 1))
        [ "$tries" -ge "$SEGMENT_RETRIES" ] && return 1
        sleep "$POLL_SECONDS"
    done
    mv "$2.part" "$2"
}
queued_count() {
    find "$RUN/q" -type f \( -name '*.ts' -o -name '*.part' \) 2>/dev/null | wc -l
}
downloader() {
    first=1
    seq=0
    while :; do
        if refresh_segments; then
            count=$(queued_count)
            if [ "$first" -eq 1 ] && [ "$START_AT_LATEST" -eq 1 ]; then
                # For a three-entry playlist and a two-segment queue, enqueue
                # entries one and two, leaving the not-yet-final entry to poll.
                total=$((PREFETCH_SEGMENTS + START_BEHIND_SEGMENTS))
                tail -n "$total" "$RUN/segments" | head -n "$PREFETCH_SEGMENTS" > "$RUN/new"
                first=0
            else
                : > "$RUN/new"
                while IFS= read -r segment; do
                    grep -Fqx "$segment" "$SEEN" || printf '%s\n' "$segment" >> "$RUN/new"
                done < "$RUN/segments"
                first=0
            fi
            while IFS= read -r segment; do
                [ -n "$segment" ] || continue
                [ "$count" -lt "$PREFETCH_SEGMENTS" ] || break
                grep -Fqx "$segment" "$SEEN" && continue
                seq=$((seq + 1))
                file="$RUN/q/$(printf '%08d' "$seq").ts"
                if fetch_segment "$segment" "$file"; then
                    remember_segment "$segment"
                    count=$((count + 1))
                fi
            done < "$RUN/new"
        fi
        sleep "$POLL_SECONDS"
    done
}
writer() {
    exec 3>"$FIFO"
    while :; do
        file=$(find "$RUN/q" -type f -name '*.ts' -print 2>/dev/null | sort | head -n 1)
        if [ -n "$file" ] && [ -f "$file" ]; then
            cat "$file" >&3 || return 0
            rm -f "$file"
        else
            sleep 1
        fi
    done
}
downloader &
RELAY_PID=$!
writer &
WRITER_PID=$!
"$MPLAYER" $MPLAYER_ARGS "$FIFO" &
MPLAYER_PID=$!
wait "$MPLAYER_PID"
kill "$WRITER_PID" 2>/dev/null || true
wait "$WRITER_PID" 2>/dev/null || true
