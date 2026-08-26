#!/bin/sh

set -e

cd /root || exit 1

ROM_PATH=$1
if [ -z "$ROM_PATH" ]; then
    echo "usage: $0 /root/roms/sfc/game.smc"
    exit 1
fi

export HOME=/root
export SDL_VIDEODRIVER=fbcon
export SDL_FBDEV="${FBDEV:-/dev/fb0}"
export SDL_FBCON_SKIP_VT_WAIT=1
export SDL_AUDIODRIVER=alsa
export AUDIODEV="${AUDIODEV:-default}"
export SDL_NOMOUSE=1
export SDL_INPUT_LINUX_KEEP_KBD=1
export S9X4D_HOME=/root/.snes9x4d
export S9X_INPUT_DEV="${S9X_INPUT_DEV:-/dev/input/event0}"
export S9X_INPUT_LOG="${S9X_INPUT_LOG:-/tmp/snes9x4d_input.log}"

# Order: QUIT,A,B,X,Y,L,R,START,SELECT,LEFT,RIGHT,UP,DOWN
# Uses Linux input-event keycodes for the direct evdev path in snes9x4d.
export S9XKEYS="${S9XKEYS:-102,30,48,45,21,104,109,28,15,105,106,103,108}"

mkdir -p "$S9X4D_HOME"

cleanup() {
    stty sane 2>/dev/null || true
    [ -w /sys/class/graphics/fb0/blank ] && echo 0 > /sys/class/graphics/fb0/blank 2>/dev/null || true
}

trap cleanup EXIT INT TERM

/root/stop_matrix_pad_bridge.sh >/dev/null 2>&1 || true

exec /root/snes9x4d -r 6 -b 1024 -stereo "$ROM_PATH"
