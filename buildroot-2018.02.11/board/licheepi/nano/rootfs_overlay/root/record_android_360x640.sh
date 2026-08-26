#!/bin/sh

set -eu

ADB_BIN="${ADB_BIN:-/root/adb_execout2}"
OUT_NAME="${1:-f1c_android_360x640.mp4}"
DURATION="${DURATION:-10}"
BIT_RATE="${BIT_RATE:-4000000}"
SIZE="${SIZE:-360x640}"

REMOTE_PATH="/sdcard/${OUT_NAME}"
LOCAL_PATH="/root/${OUT_NAME}"
ADB_LOG="/tmp/adb_record_360x640.log"
REC_OUT="/tmp/adb_record_360x640.out"
REC_ERR="/tmp/adb_record_360x640.err"
PULL_LOG="/tmp/adb_record_360x640.pull.log"

killall "$(basename "$ADB_BIN")" 2>/dev/null || true
"$ADB_BIN" nodaemon server >"$ADB_LOG" 2>&1 &
sleep 2

rm -f "$LOCAL_PATH" "$REC_OUT" "$REC_ERR" "$PULL_LOG"

"$ADB_BIN" shell screenrecord \
    --verbose \
    --size "$SIZE" \
    --bit-rate "$BIT_RATE" \
    --time-limit "$DURATION" \
    "$REMOTE_PATH" >"$REC_OUT" 2>"$REC_ERR"

"$ADB_BIN" pull "$REMOTE_PATH" "$LOCAL_PATH" >"$PULL_LOG" 2>&1
"$ADB_BIN" shell rm "$REMOTE_PATH" >/dev/null 2>&1 || true

echo "saved: $LOCAL_PATH"
ffprobe -v error \
    -show_entries format=duration,size \
    -show_entries stream=codec_name,width,height,avg_frame_rate,r_frame_rate,nb_frames \
    -of default=noprint_wrappers=1 \
    "$LOCAL_PATH" || true

echo "--- record log ---"
cat "$REC_OUT" 2>/dev/null || true
cat "$REC_ERR" 2>/dev/null || true

echo "--- pull log ---"
cat "$PULL_LOG" 2>/dev/null || true
