#!/bin/sh

set -e

DEVICE_NAME=${DEVICE_NAME:-tty-uinput-gamepad}
BIN=${BIN:-/root/uinput_tty_keyd}
ENABLE=${ENABLE:-/root/enable_uinput.sh}
MODE=${MODE:-normal}

if [ -x "$ENABLE" ]; then
    "$ENABLE"
fi

if [ ! -x "$BIN" ]; then
    echo "missing binary: $BIN"
    exit 1
fi

echo "starting $BIN"
if [ "$MODE" = "grab" ]; then
    echo "grab mode: this terminal is input-only, stop from another terminal"
    exec "$BIN" -k -n "$DEVICE_NAME"
fi

echo "normal mode: arrows + a/b/x/y/l/r/t/s, Ctrl-C to stop"
exec "$BIN" -n "$DEVICE_NAME"
