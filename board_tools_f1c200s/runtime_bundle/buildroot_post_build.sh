#!/bin/sh

set -e

TARGET_DIR=$1

if [ -z "$TARGET_DIR" ] || [ ! -d "$TARGET_DIR" ]; then
    echo "usage: $0 <buildroot-target-dir>" >&2
    exit 1
fi

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
ROOT=${ROOT:-$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)}
BUILDROOT=${BUILDROOT:-$(CDPATH= cd -- "$SCRIPT_DIR/../../buildroot-2018.02.11" && pwd)}
PAYLOAD_DIR=${PAYLOAD_DIR:-$SCRIPT_DIR/payload}

export ROOT BUILDROOT TARGET_DIR PAYLOAD_DIR

"$SCRIPT_DIR/collect_runtime_payload.sh"
"$SCRIPT_DIR/install_payload_to_target.sh"

echo "runtime bundle post-build applied to $TARGET_DIR"
