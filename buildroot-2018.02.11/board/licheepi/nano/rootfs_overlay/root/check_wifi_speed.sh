#!/bin/sh

IFACE="${1:-wlan0}"
SERVER="${2:-}"
PORT="${3:-5001}"
SIZE_MB="${4:-16}"

echo "== wifi status =="
ifconfig "$IFACE" 2>/dev/null || true
iwconfig "$IFACE" 2>/dev/null || true

echo
echo "== rtl8723bu module parameters =="
for p in rtw_power_mgnt rtw_ips_mode rtw_smart_ps rtw_low_power rtw_enusbss \
         rtw_btcoex_enable rtw_ht_enable rtw_bw_mode rtw_wmm_enable \
         rtw_ampdu_enable rtw_usb_rxagg_mode rtw_wifi_spec; do
    f="/sys/module/8723bu/parameters/$p"
    [ -r "$f" ] && echo "$p=$(cat "$f")"
done

echo
echo "== usb link =="
for d in /sys/bus/usb/devices/*; do
    [ -r "$d/idVendor" ] || continue
    v="$(cat "$d/idVendor" 2>/dev/null)"
    p="$(cat "$d/idProduct" 2>/dev/null)"
    case "$v:$p" in
        0bda:*|7392:*)
            echo "$d vendor=$v product=$p speed=$(cat "$d/speed" 2>/dev/null)M"
            ;;
    esac
done

echo
echo "== kernel messages =="
dmesg | grep -iE '8723|rtl|wlan|usb.*speed|firmware|ampdu|rxagg' | tail -80

echo
echo "== interface counters =="
cat "/sys/class/net/$IFACE/statistics/rx_bytes" 2>/dev/null | awk '{print "rx_bytes="$1}'
cat "/sys/class/net/$IFACE/statistics/tx_bytes" 2>/dev/null | awk '{print "tx_bytes="$1}'
cat "/sys/class/net/$IFACE/statistics/rx_errors" 2>/dev/null | awk '{print "rx_errors="$1}'
cat "/sys/class/net/$IFACE/statistics/tx_errors" 2>/dev/null | awk '{print "tx_errors="$1}'
cat "/sys/class/net/$IFACE/statistics/rx_dropped" 2>/dev/null | awk '{print "rx_dropped="$1}'
cat "/sys/class/net/$IFACE/statistics/tx_dropped" 2>/dev/null | awk '{print "tx_dropped="$1}'

if [ -n "$SERVER" ]; then
    if ! command -v nc >/dev/null 2>&1; then
        echo
        echo "nc not found; cannot run raw throughput test."
        exit 0
    fi

    echo
    echo "== raw tcp upload test =="
    echo "host side: nc -l -p $PORT > /dev/null"
    echo "sending ${SIZE_MB}MiB to $SERVER:$PORT ..."
    dd if=/dev/zero bs=1M count="$SIZE_MB" 2>/tmp/wifi_dd.log | nc "$SERVER" "$PORT"
    cat /tmp/wifi_dd.log
fi
