#!/bin/sh

cd /root || exit 1
modprobe fb_st7789v 2>/dev/null
export SDL_VIDEODRIVER=fbcon
FBDEV=${FBDEV:-/dev/fb0}
export SDL_FBDEV="$FBDEV"
export SDL_AUDIODRIVER=alsa
export AUDIODEV=plughw:1,0
export SDL_NOMOUSE=1

cleanup() {
    stty sane 2>/dev/null || true
    [ -w /sys/class/graphics/fb0/blank ] && echo 0 > /sys/class/graphics/fb0/blank 2>/dev/null || true
}

./gpsp "$@"
ret=$?
cleanup
exit $ret
