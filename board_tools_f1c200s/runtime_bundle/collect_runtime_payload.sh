#!/bin/sh

set -e

ROOT=${ROOT:-$HOME/LicheePi_Nano}
OUT_DIR=${OUT_DIR:-$(dirname "$0")/payload}

GMENU_TAR=${GMENU_TAR:-$ROOT/gmenu2x/dist/gmenu2x-f1c200s.tar.gz}
GPSP_BIN=${GPSP_BIN:-$ROOT/gpsp/f1c200s/gpsp}
UINPUT_BIN=${UINPUT_BIN:-$ROOT/board_tools_f1c200s/dist/f1c200s/uinput_tty_keyd}
TVD_KO=${TVD_KO:-$ROOT/tvd_f1c100s_linux57/src/suniv_f1c100s_tvd.ko}
TVD_PREVIEW=${TVD_PREVIEW:-$ROOT/tvd_f1c100s_linux57/src/tvd_fb_preview}

mkdir -p "$OUT_DIR"

copy_if_exists() {
    SRC=$1
    DST=$2
    if [ -f "$SRC" ]; then
        cp -a "$SRC" "$DST"
        echo "copied: $SRC"
    else
        echo "missing: $SRC"
    fi
}

copy_if_exists "$GMENU_TAR" "$OUT_DIR/gmenu2x-f1c200s.tar.gz"
copy_if_exists "$GPSP_BIN" "$OUT_DIR/gpsp"
copy_if_exists "$UINPUT_BIN" "$OUT_DIR/uinput_tty_keyd"
copy_if_exists "$TVD_KO" "$OUT_DIR/suniv_f1c100s_tvd.ko"
copy_if_exists "$TVD_PREVIEW" "$OUT_DIR/tvd_fb_preview"

chmod 755 "$OUT_DIR"/gpsp "$OUT_DIR"/uinput_tty_keyd "$OUT_DIR"/tvd_fb_preview 2>/dev/null || true

echo "payload ready: $OUT_DIR"
