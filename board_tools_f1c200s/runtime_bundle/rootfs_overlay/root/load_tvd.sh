#!/bin/sh

set -e

KVER=$(uname -r)
MODULE=${MODULE:-/lib/modules/$KVER/extra/suniv_f1c100s_tvd.ko}
FALLBACK_MODULE=/root/suniv_f1c100s_tvd.ko
DEVICE=${DEVICE:-/dev/video7}
STANDARD=${STANDARD:-pal}
FORCE_TVD_CLK=${FORCE_TVD_CLK:-1}
INSMOD=${INSMOD:-/sbin/insmod}

[ -f "$MODULE" ] || MODULE=$FALLBACK_MODULE

if [ ! -f "$MODULE" ]; then
    echo "missing module: $MODULE"
    exit 1
fi

if grep -q '^suniv_f1c100s_tvd ' /proc/modules 2>/dev/null; then
    rmmod suniv_f1c100s_tvd 2>/dev/null || true
fi

"$INSMOD" "$MODULE" force_tvd_clk="$FORCE_TVD_CLK"

if command -v v4l2-ctl >/dev/null 2>&1; then
    v4l2-ctl -d "$DEVICE" --set-standard="$STANDARD" || true
    v4l2-ctl -d "$DEVICE" --log-status || true
else
    echo "warning: v4l2-ctl not found, skip tv standard setup"
fi
