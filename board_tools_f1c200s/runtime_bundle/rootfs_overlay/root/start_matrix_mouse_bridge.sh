#!/bin/sh

set -e

BRIDGE_BIN=${BRIDGE_BIN:-/root/matrix_mouse_bridge}
ENABLE=${ENABLE:-/root/enable_uinput.sh}
PIDFILE=${PIDFILE:-/tmp/matrix_mouse_bridge.pid}
LOGFILE=${LOGFILE:-/tmp/matrix_mouse_bridge.log}
DEVICE_NAME=${DEVICE_NAME:-matrix-uinput-mouse}
INPUT_DEV=${INPUT_DEV:-}
GRAB_INPUT=${GRAB_INPUT:-1}
STEP=${STEP:-12}

if [ -x "$ENABLE" ]; then
    "$ENABLE" >>"$LOGFILE" 2>&1 || true
fi

if [ ! -x "$BRIDGE_BIN" ]; then
    echo "missing binary: $BRIDGE_BIN"
    exit 1
fi

if [ -f "$PIDFILE" ] && kill -0 "$(cat "$PIDFILE" 2>/dev/null)" 2>/dev/null; then
    exit 0
fi

CMD="$BRIDGE_BIN -q -n $DEVICE_NAME -s $STEP"
if [ -n "$INPUT_DEV" ]; then
    CMD="$CMD -i $INPUT_DEV"
fi
if [ "$GRAB_INPUT" = "1" ]; then
    CMD="$CMD -g"
fi

sh -c "exec $CMD >>'$LOGFILE' 2>&1" &
echo $! >"$PIDFILE"
sleep 1

if ! kill -0 "$(cat "$PIDFILE" 2>/dev/null)" 2>/dev/null; then
    echo "matrix mouse bridge failed to start"
    [ -f "$LOGFILE" ] && tail -n 40 "$LOGFILE"
    exit 1
fi
