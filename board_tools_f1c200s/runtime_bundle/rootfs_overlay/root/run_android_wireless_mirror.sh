#!/bin/sh

set -eu

PATH=/sbin:/bin:/usr/sbin:/usr/bin:/root

ADB_BIN="${ADB_BIN:-/root/adb_execout2}"
PHONE_ADDR="${PHONE_ADDR:-10.0.0.225:5555}"
SIZE="${SIZE:-360x640}"
BIT_RATE="${BIT_RATE:-4000000}"
TIME_LIMIT="${TIME_LIMIT:-180}"
FPS="${FPS:-30}"
PIPE="${PIPE:-/tmp/android_mirror.h264}"
ADB_LOG="${ADB_LOG:-/tmp/android_mirror_adb.log}"
PLAYER_LOG="${PLAYER_LOG:-/tmp/android_mirror_player.log}"

PLAYER_PID=""
ADB_PID=""

cleanup() {
    [ -n "$ADB_PID" ] && kill "$ADB_PID" 2>/dev/null || true
    [ -n "$PLAYER_PID" ] && kill "$PLAYER_PID" 2>/dev/null || true
    sleep 1
    [ -n "$ADB_PID" ] && kill -KILL "$ADB_PID" 2>/dev/null || true
    [ -n "$PLAYER_PID" ] && kill -KILL "$PLAYER_PID" 2>/dev/null || true
    rm -f "$PIPE"
    stty sane 2>/dev/null || true
}
trap cleanup EXIT INT TERM

connect_adb() {
    "$ADB_BIN" start-server >/dev/null 2>&1 || true
    "$ADB_BIN" connect "$PHONE_ADDR" >/dev/null 2>&1 || true
    "$ADB_BIN" devices | grep -q "^$PHONE_ADDR[[:space:]]*device"
}

if ! connect_adb; then
    echo "wireless adb not ready: $PHONE_ADDR"
    "$ADB_BIN" devices -l || true
    exit 1
fi

WIDTH="${SIZE%x*}"
HEIGHT="${SIZE#*x}"
if [ -z "$WIDTH" ] || [ -z "$HEIGHT" ] || [ "$WIDTH" = "$SIZE" ]; then
    echo "invalid SIZE: $SIZE"
    exit 1
fi

rm -f "$PIPE" "$ADB_LOG" "$PLAYER_LOG"
mkfifo "$PIPE"

/root/cedar_drm_player --raw-h264 "$WIDTH" "$HEIGHT" "$FPS" "$PIPE" >"$PLAYER_LOG" 2>&1 &
PLAYER_PID=$!

# Give the raw reader time to open the FIFO before adb starts writing.
sleep 1

"$ADB_BIN" exec-out screenrecord \
    --output-format=h264 \
    --size "$SIZE" \
    --bit-rate "$BIT_RATE" \
    --time-limit "$TIME_LIMIT" \
    - >"$PIPE" 2>"$ADB_LOG" &
ADB_PID=$!

wait "$ADB_PID" || true
ADB_PID=""

wait "$PLAYER_PID" || true
PLAYER_PID=""
