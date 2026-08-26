#!/bin/sh

[ -r /root/sdl_landscape_env.sh ] && . /root/sdl_landscape_env.sh

cd /root || exit 1
export SDL_FBCON_SKIP_VT_WAIT=1
export SDL_AUDIODRIVER=alsa
export AUDIODEV="${AUDIODEV:-default}"
unset SDL_NOMOUSE
export SDL_MOUSEDRV=PS2
export SDL_MOUSEDEV=/tmp/matrix_ps2mouse
export SDL_INPUT_LINUX_KEEP_KBD=1
export HOME=/root
unset ONS_F1C200S_DRAW_MOUSE
unset ONS_F1C200S_FORCE_CURSOR

cleanup() {
    /root/stop_matrix_ps2mouse_bridge.sh >/dev/null 2>&1 || true
    stty sane 2>/dev/null || true
    [ -w /sys/class/graphics/fb0/blank ] && echo 0 > /sys/class/graphics/fb0/blank 2>/dev/null || true
}

ONS=${ONS:-/root/onscripter-gbk}
GAME_ROOT=${1:-/root/roms/ons}

if [ -f "$GAME_ROOT" ]; then
    GAME_ROOT=$(dirname "$GAME_ROOT")
fi

if [ ! -x "$ONS" ]; then
    echo "onscripter gbk binary not executable: $ONS"
    exit 1
fi

if [ ! -d "$GAME_ROOT" ]; then
    echo "game root not found: $GAME_ROOT"
    exit 1
fi

killall stop_matrix_ps2mouse_bridge.sh 2>/dev/null || true
/root/stop_matrix_ps2mouse_bridge.sh >/dev/null 2>&1 || true
sleep 0.2

rm -f /tmp/ons_gbk_stdout.log /tmp/matrix_ps2mouse_packets.log "$SDL_MOUSEDEV"
INPUT_DEV=${ONS_MOUSE_INPUT_DEV:-/dev/input/event0} GRAB_INPUT=${ONS_MOUSE_GRAB:-0} \
ROTATE=${ONS_MOUSE_ROTATE:-none} /root/start_matrix_ps2mouse_bridge.sh >/tmp/matrix_ps2mouse_bridge.log 2>&1 || {
    echo "failed to start matrix ps2 mouse bridge"
    cat /tmp/matrix_ps2mouse_bridge.log 2>/dev/null
    exit 1
}

i=0
while [ ! -p "$SDL_MOUSEDEV" ] && [ "$i" -lt 20 ]; do
    sleep 0.1
    i=$((i + 1))
done
if [ ! -p "$SDL_MOUSEDEV" ]; then
    echo "matrix ps2 mouse fifo not ready: $SDL_MOUSEDEV"
    cat /tmp/matrix_ps2mouse_bridge.log 2>/dev/null
    exit 1
fi

echo "ONS-GBK SDL mouse: drv=$SDL_MOUSEDRV dev=$SDL_MOUSEDEV rotate=${ONS_MOUSE_ROTATE:-none}"
trap cleanup EXIT INT TERM

cd "$GAME_ROOT" || exit 1
shift 2>/dev/null || true
ONS_GBK_ARGS=${ONS_GBK_ARGS:---audiobuffer 8 --root "$GAME_ROOT"}
"$ONS" $ONS_GBK_ARGS "$@"
ret=$?
cleanup
if [ "$ret" -lt 0 ]; then
    ret=$((256 + ret))
fi
exit $ret
