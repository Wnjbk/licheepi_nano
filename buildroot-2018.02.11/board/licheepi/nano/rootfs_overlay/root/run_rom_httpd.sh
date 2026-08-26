#!/bin/sh

ROOT_DIR=${1:-/root/roms}
PORT=${2:-8080}
LOG=/tmp/rom_httpd.log
PID=/tmp/rom_httpd.pid

mkdir -p "$ROOT_DIR"

if [ -f "$PID" ] && kill -0 "$(cat "$PID" 2>/dev/null)" 2>/dev/null; then
    echo "ROM server already running."
else
    /root/rom_httpd "$ROOT_DIR" "$PORT" >"$LOG" 2>&1 &
    echo $! > "$PID"
    sleep 1
fi

IP=$(ip addr show wlan0 2>/dev/null | awk '/inet /{sub(/\/.*/, "", $2); print $2; exit}')
[ -z "$IP" ] && IP=$(ifconfig wlan0 2>/dev/null | sed -n 's/.*inet addr:\([^ ]*\).*/\1/p' | head -1)
[ -z "$IP" ] && IP="BOARD_IP"

clear 2>/dev/null || true
echo "ROM HTTP server"
echo
echo "Open on PC:"
echo "  http://$IP:$PORT/"
echo
echo "Root:"
echo "  $ROOT_DIR"
echo
echo "Use browser to upload/download ROM files."
echo "Press ENTER to return."
read dummy
