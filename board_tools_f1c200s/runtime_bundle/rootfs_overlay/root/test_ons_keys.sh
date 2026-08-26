#!/bin/sh

DEV=$1

find_matrix_event() {
    for name in /sys/class/input/event*/device/name; do
        [ -f "$name" ] || continue
        if grep -qi 'matrix' "$name"; then
            base=$(basename "$(dirname "$(dirname "$name")")")
            echo "/dev/input/$base"
            return 0
        fi
    done
    return 1
}

if [ -z "$DEV" ]; then
    DEV=$(find_matrix_event)
elif [ "${DEV#/dev/input/}" = "$DEV" ]; then
    DEV="/dev/input/$DEV"
fi

if [ -z "$DEV" ] || [ ! -e "$DEV" ]; then
    echo "matrix keypad event device not found."
    echo
    echo "usage:"
    echo "  $0"
    echo "  $0 /dev/input/eventX"
    echo "  $0 eventX"
    exit 1
fi

echo "testing ONS key mapping on: $DEV"
echo
echo "ONS mapping:"
echo "  UP/DOWN/LEFT/RIGHT -> mouse move"
echo "  A                  -> left click"
echo "  START/ENTER        -> left click when SELECT is not held"
echo "  B                  -> right click"
echo "  START+SELECT+B     -> ONS menu"
echo "  START+SELECT+X     -> quit ONS"
echo "  X/Y/SELECT/L/R     -> no ordinary mouse button"
echo
echo "press keys, Ctrl-C to stop"

DEV="$DEV" python - <<'PY'
import os
import struct
import sys
import time

dev = os.environ["DEV"]
fmt = "llHHI"
size = struct.calcsize(fmt)

KEY_NAMES = {
    15: "SELECT",
    28: "START/ENTER",
    30: "A",
    45: "X",
    48: "B",
    47: "Y",
    103: "UP",
    108: "DOWN",
    105: "LEFT",
    106: "RIGHT",
    104: "L1",
    109: "R1",
    262: "L2",
    261: "R2",
}

def ons_action(code, pressed, held):
    start = held.get(28, False)
    select = held.get(15, False)

    if code == 103:
        return "mouse move up" if pressed else "release up"
    if code == 108:
        return "mouse move down" if pressed else "release down"
    if code == 105:
        return "mouse move left" if pressed else "release left"
    if code == 106:
        return "mouse move right" if pressed else "release right"
    if code == 30:
        return "left click" if pressed else "left release"
    if code == 28:
        if select:
            return "START modifier for combo" if pressed else "START release"
        return "left click" if pressed else "left release"
    if code == 48:
        if pressed and start and select:
            return "SPECIAL: ONS menu"
        return "right click" if pressed else "right release"
    if code == 45:
        if pressed and start and select:
            return "SPECIAL: quit ONS"
        return "ignored by ONS mouse bridge"
    if code == 15:
        return "SELECT modifier for combo" if pressed else "SELECT release"
    if code in (47, 104, 109, 261, 262):
        return "ignored by ONS mouse bridge"
    return "unmapped"

held = {}

try:
    f = open(dev, "rb", buffering=0)
except OSError as exc:
    print("open failed: %s" % exc)
    sys.exit(1)

while True:
    data = f.read(size)
    if len(data) != size:
        continue
    sec, usec, etype, code, value = struct.unpack(fmt, data)
    if etype != 1:
        continue

    if value == 0:
        state = "UP"
        pressed = False
        held[code] = False
    elif value == 1:
        state = "DOWN"
        pressed = True
        held[code] = True
    elif value == 2:
        state = "REPEAT"
        pressed = True
    else:
        state = "VALUE%d" % value
        pressed = bool(value)

    name = KEY_NAMES.get(code, "KEY_%d" % code)
    action = ons_action(code, pressed, held)
    print("[%d.%06d] %-5s code=%-3d %-12s -> %s" %
          (sec, usec, state, code, name, action))
    sys.stdout.flush()
PY
