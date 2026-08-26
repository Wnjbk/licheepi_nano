#!/bin/sh

set -eu

IFACE="${IFACE:-wlan0}"
WPA_CLI="${WPA_CLI:-/usr/sbin/wpa_cli}"
NAME="${NAME:-F1C200S-Miracast}"
LOG="${LOG:-/tmp/miracast_probe.log}"

cmd() {
    echo "# $*" | tee -a "$LOG"
    "$WPA_CLI" -i "$IFACE" "$@" 2>&1 | tee -a "$LOG"
}

rm -f "$LOG"
echo "miracast probe iface=$IFACE name=$NAME" | tee -a "$LOG"
date 2>/dev/null | tee -a "$LOG" || true

cmd status
cmd set device_name "$NAME"
cmd set device_type 7-0050F204-1
cmd set config_methods virtual_push_button physical_display keypad
cmd set wifi_display 1

# WFD Device Information subelement:
# length=6, primary sink available, RTSP port=7236, max throughput=50 Mbps.
cmd wfd_subelem_set 0 000600111c440032
cmd wfd_subelem_get 0

cmd p2p_flush
cmd p2p_set listen_channel 1
cmd p2p_set ssid_postfix Miracast
cmd p2p_ext_listen 500 2000

echo "Open Android wireless display/cast now, then watch:" | tee -a "$LOG"
echo "  tail -f $LOG" | tee -a "$LOG"
echo "Press Ctrl-C to stop." | tee -a "$LOG"

(
    "$WPA_CLI" -i "$IFACE" -a /bin/true 2>&1
) | while IFS= read -r line; do
    echo "$line" | tee -a "$LOG"
done &
MON_PID=$!

trap 'kill "$MON_PID" 2>/dev/null || true; cmd p2p_stop_find >/dev/null 2>&1 || true; cmd p2p_ext_listen >/dev/null 2>&1 || true' EXIT INT TERM

while :; do
    cmd p2p_find 8
    sleep 9
    cmd p2p_stop_find
    cmd p2p_listen 8
    sleep 9
    cmd p2p_peers discovered
done
