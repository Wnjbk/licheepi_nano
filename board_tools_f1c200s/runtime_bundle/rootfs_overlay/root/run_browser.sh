#!/bin/sh

# Lightweight browser launcher for F1C200S.
# Uses links in text mode to avoid X11/Qt/WebKit memory cost.

export HOME=/root
export TERM=${TERM:-linux}
export LANG=C
export LC_ALL=C
export LINKS_HOME=/root/.links

URL=${1:-}
if [ -z "$URL" ]; then
    URL="file:///root/browser_home.html"
fi

mkdir -p "$LINKS_HOME"
clear 2>/dev/null || true

if ! command -v links >/dev/null 2>&1; then
    echo "links browser is not installed."
    echo "Rebuild rootfs with BR2_PACKAGE_LINKS=y."
    echo
    echo "Press ENTER to return."
    read dummy
    exit 1
fi

links "$URL"
ret=$?
stty sane 2>/dev/null || true
clear 2>/dev/null || true
exit $ret
