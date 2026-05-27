#!/bin/sh

cd /root || exit 1
modprobe fb_st7789v 2>/dev/null
export SDL_VIDEODRIVER=fbcon
export SDL_FBDEV=/dev/fb1
export SDL_AUDIODRIVER=alsa
export AUDIODEV=plughw:1,0
export SDL_NOMOUSE=1
exec ./gpsp "$@"
