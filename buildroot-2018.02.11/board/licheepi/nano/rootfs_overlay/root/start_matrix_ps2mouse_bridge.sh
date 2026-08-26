#!/bin/sh

set -e

BRIDGE_BIN=${BRIDGE_BIN:-/root/matrix_ps2mouse_bridge}
ENABLE=${ENABLE:-/root/enable_uinput.sh}
PIDFILE=${PIDFILE:-/tmp/matrix_ps2mouse_bridge.pid}
LOGFILE=${LOGFILE:-/tmp/matrix_ps2mouse_bridge.log}
FIFO=${FIFO:-/tmp/matrix_ps2mouse}
INPUT_DEV=${INPUT_DEV:-}
GRAB_INPUT=${GRAB_INPUT:-1}
STEP=${STEP:-10}
ROTATE=${ROTATE:-none}

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

CMD="$BRIDGE_BIN -q -o $FIFO -s $STEP -r $ROTATE"
if [ -n "$INPUT_DEV" ]; then
    CMD="$CMD -i $INPUT_DEV"
fi
if [ "$GRAB_INPUT" = "1" ]; then
    CMD="$CMD -g"
fi

rm -f "$FIFO"
sh -c "exec $CMD >>'$LOGFILE' 2>&1" &
echo $! >"$PIDFILE"

count=0
while [ ! -p "$FIFO" ] && [ "$count" -lt 20 ]; do
    sleep 0.1
    count=$((count + 1))
done

if ! kill -0 "$(cat "$PIDFILE" 2>/dev/null)" 2>/dev/null; then
    echo "matrix ps2 mouse bridge failed to start"
    [ -f "$LOGFILE" ] && tail -n 40 "$LOGFILE"
    exit 1
fi

if [ ! -p "$FIFO" ]; then
    echo "matrix ps2 mouse fifo not created: $FIFO"
    exit 1
fi
