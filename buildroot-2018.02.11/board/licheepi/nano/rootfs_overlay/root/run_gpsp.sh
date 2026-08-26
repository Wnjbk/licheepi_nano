#!/bin/sh

[ -r /root/sdl_landscape_env.sh ] && . /root/sdl_landscape_env.sh

cd /root || exit 1
export SDL_FBCON_SKIP_VT_WAIT=1
export SDL_AUDIODRIVER=alsa
export AUDIODEV="${AUDIODEV:-default}"
export SDL_INPUT_LINUX_KEEP_KBD=1

cleanup() {
    /root/stop_matrix_pad_bridge.sh >/dev/null 2>&1 || true
    stty sane 2>/dev/null || true
    [ -w /sys/class/graphics/fb0/blank ] && echo 0 > /sys/class/graphics/fb0/blank 2>/dev/null || true
}

/root/start_matrix_pad_bridge.sh
trap cleanup EXIT INT TERM

./gpsp "$@"
ret=$?
cleanup
exit $ret

