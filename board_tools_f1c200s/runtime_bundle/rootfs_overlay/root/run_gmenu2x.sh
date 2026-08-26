#!/bin/sh

cd /root/gmenu2x || exit 1

[ -r /root/sdl_landscape_env.sh ] && . /root/sdl_landscape_env.sh

export SDL_FBCON_SKIP_VT_WAIT=1
export SDL_AUDIODRIVER=alsa
export AUDIODEV="${AUDIODEV:-default}"
export SDL_INPUT_LINUX_KEEP_KBD=1
export SDL_VIDEODRIVER=fbcon
export SDL_FBDEV="${SDL_FBDEV:-/dev/fb0}"
export SDL_NOMOUSE=1
export SDL_MOUSEDEV=/dev/null
export SDL_FB_BROKEN_MODES=1

# GMenu2X reads these env vars itself, while the shared SDL helper provides
# the fbcon rotation/crop geometry. Keep both in sync.
export GMENU2X_VISIBLE_W="${GMENU2X_VISIBLE_W:-${SDL_DISPLAY_W:-480}}"
export GMENU2X_VISIBLE_H="${GMENU2X_VISIBLE_H:-${SDL_DISPLAY_H:-360}}"
export GMENU2X_BPP="${GMENU2X_BPP:-${SDL_DISPLAY_BPP:-32}}"

cleanup() {
    stty sane 2>/dev/null || true
    [ -w /sys/class/graphics/fb0/blank ] && echo 0 > /sys/class/graphics/fb0/blank 2>/dev/null || true
}

./gmenu2x
ret=$?
cleanup
exit $ret
