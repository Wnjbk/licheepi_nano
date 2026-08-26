#!/bin/sh

set -e

DEVICE_ARG=$1

find_matrix_event() {
    for ev in /sys/class/input/event*; do
        [ -e "$ev" ] || continue
        name=$(cat "$ev/device/name" 2>/dev/null || true)
        case "$name" in
            *matrix*|*Matrix*|*keypad*|*Keypad*)
                basename "$ev"
                return 0
                ;;
        esac
    done
    return 1
}

show_inputs() {
    echo "input devices:"
    for ev in /sys/class/input/event*; do
        [ -e "$ev" ] || continue
        evn=$(basename "$ev")
        name=$(cat "$ev/device/name" 2>/dev/null || echo unknown)
        echo "  /dev/input/$evn -> $name"
    done
}

key_name() {
    case "$1" in
        103) echo KEY_UP ;;
        108) echo KEY_DOWN ;;
        105) echo KEY_LEFT ;;
        106) echo KEY_RIGHT ;;
        28) echo KEY_ENTER ;;
        15) echo KEY_TAB ;;
        21) echo KEY_Y ;;
        30) echo KEY_A ;;
        48) echo KEY_B ;;
        45) echo KEY_X ;;
        47) echo KEY_V ;;
        102) echo KEY_HOME ;;
        104) echo KEY_PAGEUP ;;
        107) echo KEY_END ;;
        109) echo KEY_PAGEDOWN ;;
        256) echo BTN_0 ;;
        257) echo BTN_1 ;;
        258) echo BTN_2 ;;
        259) echo BTN_3 ;;
        260) echo BTN_4 ;;
        261) echo BTN_5 ;;
        262) echo BTN_6 ;;
        263) echo BTN_7 ;;
        *) echo "KEY_$1" ;;
    esac
}

event_value_name() {
    case "$1" in
        0) echo UP ;;
        1) echo DOWN ;;
        2) echo REPEAT ;;
        *) echo "VALUE_$1" ;;
    esac
}

pretty_dump() {
    exec 3<"$1"
    while :; do
        line=$(dd bs=16 count=1 2>/dev/null <&3 | hexdump -v -e '1/4 "%u " 1/4 "%06u " 1/2 "%u " 1/2 "%u " 1/4 "%d\n"' 2>/dev/null || true)
        [ -n "$line" ] || continue
        set -- $line
        sec=$1
        usec=$2
        type=$3
        code=$4
        value=$5
        case "$type" in
            0)
                printf '[%s.%06s] SYN_REPORT\n' "$sec" "$usec"
                ;;
            1)
                printf '[%s.%06s] KEY        code=%-4s %-12s %s\n' \
                    "$sec" "$usec" "$code" "$(key_name "$code")" "$(event_value_name "$value")"
                ;;
            4)
                if [ "$code" -eq 4 ]; then
                    printf '[%s.%06s] MSC_SCAN   scan=0x%02x (%d)\n' \
                        "$sec" "$usec" "$value" "$value"
                else
                    printf '[%s.%06s] MSC        code=%-4s value=%d\n' \
                        "$sec" "$usec" "$code" "$value"
                fi
                ;;
            *)
                printf '[%s.%06s] TYPE=%-4s code=%-4s value=%d\n' \
                    "$sec" "$usec" "$type" "$code" "$value"
                ;;
        esac
    done
}

if [ -n "$DEVICE_ARG" ]; then
    case "$DEVICE_ARG" in
        /dev/input/event*) DEV=$DEVICE_ARG ;;
        event*) DEV=/dev/input/$DEVICE_ARG ;;
        *) DEV=$DEVICE_ARG ;;
    esac
else
    ev=$(find_matrix_event || true)
    if [ -n "$ev" ]; then
        DEV=/dev/input/$ev
    fi
fi

if [ -z "$DEV" ] || [ ! -e "$DEV" ]; then
    echo "matrix keypad event device not found automatically."
    show_inputs
    echo
    echo "usage:"
    echo "  $0 /dev/input/eventX"
    echo "  $0 eventX"
    exit 1
fi

echo "testing matrix keypad on: $DEV"
show_inputs
echo

if command -v evtest >/dev/null 2>&1; then
    exec evtest "$DEV"
fi

echo "evtest not found, using decoded input_event output"
echo "press keys, Ctrl-C to stop"
pretty_dump "$DEV"
