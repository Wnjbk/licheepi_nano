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
unset ONS_TRACE
unset ONS_IMAGE_TRACE
unset ONS_GLYPH_TRACE

if [ -f /root/ons_trace_enable ]; then
    export ONS_TRACE=1
    export ONS_IMAGE_TRACE=1
    export ONS_GLYPH_TRACE=1
fi

cleanup() {
    /root/stop_matrix_ps2mouse_bridge.sh >/dev/null 2>&1 || true
    stty sane 2>/dev/null || true
    [ -w /sys/class/graphics/fb0/blank ] && echo 0 > /sys/class/graphics/fb0/blank 2>/dev/null || true
}

ONS=${ONS:-/root/onscripter-en}
GAME_ROOT=${1:-/root/roms/ons}
SAVE_ROOT=${SAVE_ROOT:-/root/.ons_save}

if [ -f "$GAME_ROOT" ]; then
    GAME_ROOT=$(dirname "$GAME_ROOT")
fi

GAME_NAME=$(basename "$GAME_ROOT")
SAVE_DIR=$SAVE_ROOT/$GAME_NAME

ORIG_GAME_ROOT=$GAME_ROOT
if [ -f "$ORIG_GAME_ROOT/0.txt" ] &&
   [ -f "$ORIG_GAME_ROOT/default.TTF" ] &&
   [ -f "$ORIG_GAME_ROOT/arc1.nsa" ] &&
   grep -q 'SayaVoiFirstEdition' "$ORIG_GAME_ROOT/0.txt" 2>/dev/null; then
    WRAP_ROOT=/root/.ons_runtime/saya_utf8
    rm -rf "$WRAP_ROOT"
    mkdir -p "$WRAP_ROOT"
    if [ -f /root/.ons_runtime_saya_0.utf ]; then
        cp /root/.ons_runtime_saya_0.utf "$WRAP_ROOT/0.utf"
    else
        cp "$ORIG_GAME_ROOT/0.txt" "$WRAP_ROOT/0.utf"
    fi
    [ -f "$ORIG_GAME_ROOT/envdata" ] && cp "$ORIG_GAME_ROOT/envdata" "$WRAP_ROOT/envdata"
    [ -f "$ORIG_GAME_ROOT/default.TTF" ] && ln -s "$ORIG_GAME_ROOT/default.TTF" "$WRAP_ROOT/default.TTF"
    [ -f "$ORIG_GAME_ROOT/default.TTF" ] && ln -s "$ORIG_GAME_ROOT/default.TTF" "$WRAP_ROOT/default.ttf"
    [ -f "$ORIG_GAME_ROOT/arc.nsa" ] && ln -s "$ORIG_GAME_ROOT/arc.nsa" "$WRAP_ROOT/arc.nsa"
    [ -f "$ORIG_GAME_ROOT/arc1.nsa" ] && ln -s "$ORIG_GAME_ROOT/arc1.nsa" "$WRAP_ROOT/arc1.nsa"
    GAME_ROOT=$WRAP_ROOT
fi

if [ ! -x "$ONS" ]; then
    echo "onscripter binary not executable: $ONS"
    exit 1
fi

if [ ! -d "$GAME_ROOT" ]; then
    echo "game root not found: $GAME_ROOT"
    exit 1
fi

mkdir -p "$SAVE_DIR"
rm -f /tmp/ons_trace.log \
      /tmp/ons_video_trace.log \
      /tmp/ons_input_debug.log \
      /tmp/ons_image_trace.log \
      /tmp/ons_button_trace.log \
      /tmp/ons_bgmstop_trace.log \
      /tmp/matrix_ps2mouse_packets.log

ROTATE=${ONS_MOUSE_ROTATE:-none}
GRAB_INPUT=0 ROTATE="$ROTATE" /root/start_matrix_ps2mouse_bridge.sh >/tmp/matrix_ps2mouse_bridge.log 2>&1 || {
    echo "failed to start matrix ps2 mouse bridge"
    cat /tmp/matrix_ps2mouse_bridge.log 2>/dev/null
    exit 1
}
echo "ONS SDL mouse: drv=$SDL_MOUSEDRV dev=$SDL_MOUSEDEV rotate=$ROTATE"
trap cleanup EXIT INT TERM

# Keep the game's original csel/customsel path. The YZ-specific built-in csel
# fallback hides the game's own option bar and makes the wait state look stuck.
unset ONS_F1C200S_BUILTIN_CSEL

if [ "$GAME_NAME" = "YZ" ] && [ -z "$ONS_F1C200S_SKIP_BGMSTOP" ]; then
    export ONS_F1C200S_SKIP_BGMSTOP=1
fi

if [ "$GAME_NAME" = "YZ" ] && [ -z "$ONS_F1C200S_YZ_COMPAT" ]; then
    export ONS_F1C200S_YZ_COMPAT=1
fi

cd "$GAME_ROOT" || exit 1

shift 2>/dev/null || true
ONS_ARGS=${ONS_ARGS:---scale --audiobuffer 8 --nomatch-audiodevice-to-bgm}
GAME_FONT=$(find "$GAME_ROOT" -maxdepth 1 \( -type f -o -type l \) \( -iname 'default.ttf' -o -iname 'default.ttc' -o -iname 'default.otf' -o -iname 'default.otc' \) | head -n 1)
if [ -n "$GAME_FONT" ]; then
    ONS_ARGS="$ONS_ARGS --font $GAME_FONT"
fi
{
    echo "ONS_BIN=$ONS"
    md5sum "$ONS" 2>/dev/null || true
    echo "ONS_ARGS=$ONS_ARGS --root $GAME_ROOT --save $SAVE_DIR $*"
    echo "ONS_TRACE=${ONS_TRACE:-0} ONS_IMAGE_TRACE=${ONS_IMAGE_TRACE:-0} ONS_GLYPH_TRACE=${ONS_GLYPH_TRACE:-0}"
} >/tmp/ons_launch.log
"$ONS" $ONS_ARGS --root "$GAME_ROOT" --save "$SAVE_DIR" "$@"
ret=$?
cleanup
exit $ret


