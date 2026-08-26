#!/bin/sh

set -e

PIDFILE=${PIDFILE:-/tmp/matrix_ps2mouse_bridge.pid}
FIFO=${FIFO:-/tmp/matrix_ps2mouse}

if [ -f "$PIDFILE" ]; then
    PID=$(cat "$PIDFILE" 2>/dev/null || true)
    if [ -n "$PID" ]; then
        kill "$PID" 2>/dev/null || true
        sleep 1
        kill -9 "$PID" 2>/dev/null || true
    fi
    rm -f "$PIDFILE"
else
    killall matrix_ps2mouse_bridge 2>/dev/null || true
fi

rm -f "$FIFO"
