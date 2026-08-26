#!/bin/sh

IFACE=${WIFI_IFACE:-wlan0}
OUT=/tmp/wifi_scan.txt

clear 2>/dev/null || true
echo "WiFi Scan"
echo "========="
echo
echo "Interface: $IFACE"
echo "Time: $(date 2>/dev/null || echo unknown)"
echo

if ! command -v iwlist >/dev/null 2>&1; then
    echo "iwlist not found."
    echo
    echo "Install wireless_tools in Buildroot."
    echo
    echo "Press ENTER to return."
    read dummy
    exit 1
fi

if ! ip link show "$IFACE" >/dev/null 2>&1 && ! ifconfig "$IFACE" >/dev/null 2>&1; then
    echo "Interface $IFACE not found."
    echo
    echo "Available interfaces:"
    ifconfig -a 2>/dev/null || ip link 2>/dev/null || true
    echo
    echo "Press ENTER to return."
    read dummy
    exit 1
fi

echo "Scanning, please wait..."
echo

ifconfig "$IFACE" up >/dev/null 2>&1 || true
iwlist "$IFACE" scan > "$OUT" 2>&1

if grep -q "No scan results" "$OUT"; then
    cat "$OUT"
elif grep -q "Cell " "$OUT"; then
    awk '
        /Cell [0-9]+/ {
            if (ssid != "" || addr != "") {
                printf "%2d. %-24s  %s  %s  %s\n", n, ssid, qual, sig, enc
            }
            n++
            addr=$5
            ssid="<hidden>"
            qual=""
            sig=""
            enc=""
        }
        /ESSID:/ {
            ssid=$0
            sub(/^.*ESSID:"/, "", ssid)
            sub(/"$/, "", ssid)
            if (ssid == "") ssid="<hidden>"
        }
        /Quality=/ {
            qual=$0
            sub(/^.*Quality=/, "Q=", qual)
            sub(/  Signal.*$/, "", qual)
            sig=$0
            sub(/^.*Signal level=/, "S=", sig)
            sub(/  Noise.*$/, "", sig)
        }
        /Encryption key:/ {
            enc=$0
            sub(/^.*Encryption key:/, "ENC=", enc)
        }
        END {
            if (ssid != "" || addr != "") {
                printf "%2d. %-24s  %s  %s  %s\n", n, ssid, qual, sig, enc
            }
            if (n == 0) print "No AP found."
        }
    ' "$OUT"
else
    cat "$OUT"
fi

echo
echo "Raw log: $OUT"
echo
echo "Press ENTER to return."
read dummy
