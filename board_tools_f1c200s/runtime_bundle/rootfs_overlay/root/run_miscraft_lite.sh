#!/bin/sh

set -eu

[ -r /root/sdl_landscape_env.sh ] && . /root/sdl_landscape_env.sh

export HOME=/root
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-fbcon}"
export SDL_FBDEV="${SDL_FBDEV:-/dev/fb0}"
export SDL_FBCON_SKIP_VT_WAIT=1
export SDL_NOMOUSE=1
export SDL_MOUSEDEV=/dev/null
export SDL_FB_BROKEN_MODES=1
export SDL_INPUT_LINUX_KEEP_KBD=1

cleanup() {
    stty sane 2>/dev/null || true
    [ -w /sys/class/graphics/fb0/blank ] && echo 0 > /sys/class/graphics/fb0/blank 2>/dev/null || true
}
trap cleanup EXIT INT TERM

cd /root
exec /root/miscraft_lite
