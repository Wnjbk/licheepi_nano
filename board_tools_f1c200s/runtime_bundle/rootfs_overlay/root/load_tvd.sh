#!/bin/sh

set -e

MODULE=${MODULE:-/root/suniv_f1c100s_tvd.ko}
DEVICE=${DEVICE:-/dev/video7}
STANDARD=${STANDARD:-pal}
FORCE_TVD_CLK=${FORCE_TVD_CLK:-1}

if [ ! -f "$MODULE" ]; then
    echo "missing module: $MODULE"
    exit 1
fi

if lsmod | grep -q '^suniv_f1c100s_tvd '; then
    rmmod suniv_f1c100s_tvd 2>/dev/null || true
fi

insmod "$MODULE" force_tvd_clk="$FORCE_TVD_CLK"

if command -v v4l2-ctl >/dev/null 2>&1; then
    v4l2-ctl -d "$DEVICE" --set-standard="$STANDARD"
    v4l2-ctl -d "$DEVICE" --log-status
else
    echo "warning: v4l2-ctl not found, skip tv standard setup"
fi
