#!/bin/sh

[ -r /root/sdl_landscape_env.sh ] && . /root/sdl_landscape_env.sh

cd /root || exit 1
export SDL_AUDIODRIVER=alsa
export AUDIODEV="${AUDIODEV:-default}"
export SDL_INPUT_LINUX_KEEP_KBD=1

exec /root/DinguxCommander
