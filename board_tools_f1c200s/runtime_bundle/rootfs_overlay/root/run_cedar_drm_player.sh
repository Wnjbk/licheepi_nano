#!/bin/sh

[ -r /root/sdl_landscape_env.sh ] && . /root/sdl_landscape_env.sh

VIDEO_PATH=$1
if [ -z "$VIDEO_PATH" ]; then
    echo "usage: $0 /root/roms/video/file.mp4"
    echo "       $0 http://phone:port/path/video.mp4"
    exit 1
fi

case "$VIDEO_PATH" in
    http://*|https://*|rtsp://*)
        ;;
    *)
        if [ ! -f "$VIDEO_PATH" ]; then
            echo "video not found: $VIDEO_PATH"
            exit 1
        fi
        ;;
esac

cd /root || exit 1
export HOME=/root
export CEDAR_AUDIODEV="${CEDAR_AUDIODEV:-default}"

find_key_event() {
    for ev in /sys/class/input/event*; do
        [ -e "$ev" ] || continue
        name=$(cat "$ev/device/name" 2>/dev/null || true)
        case "$name" in
            *matrix*|*Matrix*|*keypad*|*Keypad*)
                printf '/dev/input/%s\n' "$(basename "$ev")"
                return 0
                ;;
        esac
    done
    return 1
}

watch_exit_key() {
    dev=$1
    [ -n "$dev" ] || return 0
    [ -r "$dev" ] || return 0

    exec 3<"$dev"
    while :; do
        line=$(dd bs=16 count=1 2>/dev/null <&3 | hexdump -v -e '1/4 "%u " 1/4 "%06u " 1/2 "%u " 1/2 "%u " 1/4 "%d\n"' 2>/dev/null || true)
        [ -n "$line" ] || continue
        set -- $line
        type=$3
        code=$4
        value=$5

        [ "$type" = "1" ] || continue
        [ "$value" = "1" ] || continue

        case "$code" in
            48|15|102|107)
                kill -TERM "$PLAYER_PID" 2>/dev/null || true
                sleep 1
                kill -KILL "$PLAYER_PID" 2>/dev/null || true
                return 0
                ;;
        esac
    done
}

cleanup() {
    stty sane 2>/dev/null || true
    [ -n "${WATCH_PID:-}" ] && kill "$WATCH_PID" 2>/dev/null || true
    [ -w /sys/class/graphics/fb0/blank ] && echo 0 > /sys/class/graphics/fb0/blank 2>/dev/null || true
}
trap cleanup EXIT INT TERM

KEY_EVENT_DEV=$(find_key_event || true)

/root/cedar_drm_player "$VIDEO_PATH" &
PLAYER_PID=$!

if [ -n "$KEY_EVENT_DEV" ]; then
    watch_exit_key "$KEY_EVENT_DEV" &
    WATCH_PID=$!
fi

wait "$PLAYER_PID"
exit $?
