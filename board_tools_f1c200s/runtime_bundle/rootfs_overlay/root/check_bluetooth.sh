#!/bin/sh

echo "== kernel config/runtime =="
uname -a
ls -l /sys/class/bluetooth 2>/dev/null || true
ls -l /sys/class/rfkill 2>/dev/null || true

echo
echo "== firmware =="
ls -l /lib/firmware/rtl_bt 2>/dev/null || true

echo
echo "== processes =="
ps | grep -E 'bluetoothd|dbus-daemon' | grep -v grep || true

echo
echo "== tools =="
for t in rfkill bluetoothd bluetoothctl hciconfig hcitool btmon dbus-daemon dbus-uuidgen; do
    printf "%-14s " "$t"
    command -v "$t" || true
done

echo
echo "== hci =="
hciconfig -a 2>/dev/null || true
hcitool dev 2>/dev/null || true

echo
echo "== bluetoothctl =="
bluetoothctl show 2>/dev/null || true

echo
echo "== dmesg =="
dmesg | grep -iE 'bluetooth|btusb|btrtl|rtl.*bt|8723|firmware|hci|rfkill' | tail -80
