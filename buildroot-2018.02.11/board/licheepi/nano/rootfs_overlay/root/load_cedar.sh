#!/bin/sh

set -e

KVER=$(uname -r)
MODDIR="/lib/modules/$KVER/extra"
INSMOD=${INSMOD:-/sbin/insmod}

find_module() {
    name=$1

    for path in \
        "$MODDIR/$name" \
        "/root/$name"
    do
        [ -f "$path" ] && { echo "$path"; return 0; }
    done

    return 1
}

load_module() {
    mod=$1
    name=$(basename "$mod" .ko)

    if grep -q "^$name " /proc/modules 2>/dev/null; then
        echo "cedar: $name already loaded"
        return 0
    fi

    "$INSMOD" "$mod"
    echo "cedar: loaded $mod"
}

ION_MODULE=$(find_module sunxi_ion_core.ko) || {
    echo "cedar: missing sunxi_ion_core.ko"
    exit 1
}

CEDAR_MODULE=$(find_module cedar_ve.ko) || {
    echo "cedar: missing cedar_ve.ko"
    exit 1
}

echo "Loading Cedar VE modules..."
load_module "$ION_MODULE"
load_module "$CEDAR_MODULE"

[ -e /dev/ion ] && echo "  /dev/ion ready" || echo "  WARN: /dev/ion missing"
[ -e /dev/cedar_dev ] && echo "  /dev/cedar_dev ready" || echo "  WARN: /dev/cedar_dev missing"

echo "Cedar ready."
