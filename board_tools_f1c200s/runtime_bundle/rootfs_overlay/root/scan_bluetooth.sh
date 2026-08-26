#!/bin/sh

OUT=/tmp/bluetooth_scan.txt

clear 2>/dev/null || true
echo "Bluetooth Scan"
echo "=============="
echo
echo "Time: $(date 2>/dev/null || echo unknown)"
echo

if [ -x /root/start_bluetooth.sh ]; then
    /root/start_bluetooth.sh >/tmp/start_bluetooth_scan.log 2>&1 || true
fi

if ! command -v bluetoothctl >/dev/null 2>&1 && ! command -v hcitool >/dev/null 2>&1; then
    echo "bluetoothctl/hcitool not found."
    echo
    echo "BlueZ is not installed in this rootfs."
    echo
    echo "Press ENTER to return."
    read dummy
    exit 1
fi

if command -v hciconfig >/dev/null 2>&1; then
    hciconfig hci0 up >/dev/null 2>&1 || true
    if ! hciconfig hci0 >/dev/null 2>&1; then
        echo "hci0 not found."
        echo
        echo "Check:"
        echo "  dmesg | grep -iE 'bluetooth|btusb|btrtl|8723|firmware|hci'"
        echo
        echo "Press ENTER to return."
        read dummy
        exit 1
    fi
fi

echo "Scanning, please wait..."
echo
rm -f "$OUT"

if command -v hcitool >/dev/null 2>&1; then
    hcitool scan 2>&1 | tee "$OUT"
elif command -v bluetoothctl >/dev/null 2>&1; then
    {
        echo "power on"
        echo "agent on"
        echo "default-agent"
        echo "scan on"
        sleep 10
        echo "scan off"
        echo "devices"
        echo "quit"
    } | bluetoothctl > "$OUT" 2>&1 || true
fi

if grep -q "^Device " "$OUT"; then
    awk '
        /^Device / {
            mac=$2
            name=$0
            sub(/^Device [^ ]+ /, "", name)
            if (name == "") name="<unknown>"
            if (!seen[mac]++) {
                n++
                printf "%2d. %-17s  %s\n", n, mac, name
            }
        }
        END {
            if (n == 0) print "No Bluetooth device found."
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
