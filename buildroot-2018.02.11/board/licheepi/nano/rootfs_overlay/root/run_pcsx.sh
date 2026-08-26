#!/bin/sh

[ -r /root/sdl_landscape_env.sh ] && . /root/sdl_landscape_env.sh

cd /root || exit 1

export SDL_FBCON_SKIP_VT_WAIT=1
export SDL_AUDIODRIVER=alsa
export AUDIODEV="${AUDIODEV:-default}"
export SDL_INPUT_LINUX_KEEP_KBD=1
export HOME=/root

PCSX=${PCSX:-/root/pcsx}
CFG=${PCSX_CFG:-/root/.pcsx/pcsx.cfg}
ROM=${1:-}

if [ ! -x "$PCSX" ]; then
    echo "pcsx binary not executable: $PCSX"
    exit 1
fi

mkdir -p \
    /root/roms/ps1 \
    /root/.pcsx/memcards \
    /root/.pcsx/sstates \
    /root/.pcsx/cheats \
    /root/.pcsx/patches \
    /root/.pcsx/cfg \
    /root/bios \
    /root/screenshots

if [ ! -e /bios ]; then
    ln -snf /root/bios /bios
fi

if [ ! -e /screenshots ]; then
    ln -snf /root/screenshots /screenshots
fi

if [ -n "$ROM" ]; then
    case "$ROM" in
        /*) ;;
        *) ROM="/root/$ROM" ;;
    esac

    if [ ! -f "$ROM" ]; then
        echo "ROM not found: $ROM"
        exit 1
    fi

    shift
    exec "$PCSX" -cdfile "$ROM" -cfg "$CFG" "$@"
fi

exec "$PCSX" -cfg "$CFG" "$@"

