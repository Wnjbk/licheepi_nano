#!/bin/sh

LOG=/tmp/run_snes9x4d_from_gmenu2x.log

{
echo "==== $(date '+%Y-%m-%d %H:%M:%S') ===="
echo "pwd(before)=$(pwd)"
echo "argv0=$0"
echo "argv=$*"
echo "HOME=$HOME"
echo "PATH=$PATH"
echo "SDL_VIDEODRIVER=$SDL_VIDEODRIVER"
echo "SDL_FBDEV=$SDL_FBDEV"
echo "SDL_AUDIODRIVER=$SDL_AUDIODRIVER"
echo "AUDIODEV=$AUDIODEV"
echo "SDL_NOMOUSE=$SDL_NOMOUSE"
echo "SDL_INPUT_LINUX_KEEP_KBD=$SDL_INPUT_LINUX_KEEP_KBD"
echo "SDL_FBCON_SKIP_VT_WAIT=$SDL_FBCON_SKIP_VT_WAIT"
echo "TERM=$TERM"
echo "TTY=$(tty 2>/dev/null || echo none)"
echo "ps(before):"
ps | grep -E 'gmenu2x|snes9x4d|matrix_pad_bridge|run_snes9x4d' | grep -v grep || true
echo "-- run_snes9x4d.sh begin --"
} >> "$LOG" 2>&1

/root/run_snes9x4d.sh "$@" >> "$LOG" 2>&1
ret=$?

{
echo "-- run_snes9x4d.sh end ret=$ret --"
echo "ps(after):"
ps | grep -E 'gmenu2x|snes9x4d|matrix_pad_bridge|run_snes9x4d' | grep -v grep || true
echo
} >> "$LOG" 2>&1

exit $ret
