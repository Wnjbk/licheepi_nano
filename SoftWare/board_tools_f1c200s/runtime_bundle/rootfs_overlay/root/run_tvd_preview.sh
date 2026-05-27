#!/bin/sh

set -e

PREVIEW=${PREVIEW:-/root/tvd_fb_preview}
FBDEV=${FBDEV:-/dev/fb0}
DEVICE=${DEVICE:-/dev/video7}
STANDARD=${STANDARD:-pal}

if [ ! -x "$PREVIEW" ]; then
    echo "missing preview tool: $PREVIEW"
    exit 1
fi

if [ -f /root/load_tvd.sh ]; then
    STANDARD="$STANDARD" DEVICE="$DEVICE" /root/load_tvd.sh
fi

exec "$PREVIEW" -d "$DEVICE" -f "$FBDEV" -s "$STANDARD"
