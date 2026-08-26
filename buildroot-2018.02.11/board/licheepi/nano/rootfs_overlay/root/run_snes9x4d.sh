#!/bin/sh

set -e

ROM_PATH=$1
if [ -z "$ROM_PATH" ]; then
    echo "usage: $0 /root/roms/sfc/game.smc"
    exit 1
fi

exec /root/run_snes9x4d_console.sh "$ROM_PATH"
