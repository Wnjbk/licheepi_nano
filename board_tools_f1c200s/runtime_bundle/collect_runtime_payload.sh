#!/bin/sh

set -e

ROOT=${ROOT:-$HOME/LicheePi_Nano}
OUT_DIR=${OUT_DIR:-$(dirname "$0")/payload}

GMENU_TAR=${GMENU_TAR:-$ROOT/gmenu2x/dist/gmenu2x-f1c200s.tar.gz}
GPSP_BIN=${GPSP_BIN:-$ROOT/gpsp/f1c200s/gpsp}
SNES9X4D_BIN=${SNES9X4D_BIN:-$ROOT/board_tools_f1c200s/prebuilt_bins/snes9x4d}
SNES9X4D_RS90_BIN=${SNES9X4D_RS90_BIN:-$ROOT/third_party/snes9x4d_rs90_f1c200s/dingux-sdl/snes9x4d_rs90}
DINGUX_BIN=${DINGUX_BIN:-$ROOT/SoftWare/DinguxCommander/output/f1c200s/DinguxCommander}
SDLGNUBOY_BIN=${SDLGNUBOY_BIN:-$ROOT/buildroot-2018.02.11/output/target/root/sdlgnuboy}
PCSX_BIN=${PCSX_BIN:-$ROOT/third_party/pcsx_rearmed/pcsx}
PICODRIVE_ZIP=${PICODRIVE_ZIP:-$ROOT/third_party/picodrive/PicoDrive.zip}
KERNEL_TREE=${KERNEL_TREE:-$ROOT/linux}
CEDAR_ION_KO=${CEDAR_ION_KO:-$KERNEL_TREE/drivers/staging/media/sunxi/cedar/ion/sunxi_ion_core.ko}
CEDAR_VE_KO=${CEDAR_VE_KO:-$KERNEL_TREE/drivers/staging/media/sunxi/cedar/ve/cedar_ve.ko}
ESP8089_PREBUILT_KO=${ESP8089_PREBUILT_KO:-$ROOT/board_tools_f1c200s/prebuilt_modules/esp8089-spi-working.ko}
ESP8089_KERNEL_KO=${ESP8089_KERNEL_KO:-$KERNEL_TREE/drivers/net/wireless/esp8089/esp8089-spi.ko}
ESP8089_SPI_KO=${ESP8089_SPI_KO:-$ESP8089_KERNEL_KO}
TVD_KO=${TVD_KO:-$KERNEL_TREE/drivers/media/platform/sunxi/suniv-tvd/suniv_f1c100s_tvd.ko}
TVD_PREVIEW=${TVD_PREVIEW:-$ROOT/tvd_f1c100s_linux57/tools/fb_preview/tvd_fb_preview}
MATRIX_PAD_BRIDGE_BIN=${MATRIX_PAD_BRIDGE_BIN:-$ROOT/board_tools_f1c200s/dist/f1c200s/matrix_pad_bridge}
MATRIX_PS2MOUSE_BRIDGE_BIN=${MATRIX_PS2MOUSE_BRIDGE_BIN:-$ROOT/board_tools_f1c200s/matrix_ps2mouse_bridge}
RTL8723BU_KO=${RTL8723BU_KO:-$(dirname "$0")/verified_modules/8723bu.production_20260809.ko}
RTL8188FU_KO=${RTL8188FU_KO:-$ROOT/third_party/rtl8188fu_kelebek333/rtl8188fu.ko}
RTL8188FU_FW=${RTL8188FU_FW:-$ROOT/third_party/rtl8188fu_kelebek333/firmware/rtl8188fufw.bin}
AIC_BASE_DIR=${AIC_BASE_DIR:-$ROOT/third_party/aic8800d80_host_fresh_20260714/src/USB/driver_fw}
AIC_LOAD_FW_KO=${AIC_LOAD_FW_KO:-$AIC_BASE_DIR/drivers/aic8800/aic_load_fw/aic_load_fw.ko}
AIC8800_FDRV_KO=${AIC8800_FDRV_KO:-$AIC_BASE_DIR/drivers/aic8800/aic8800_fdrv/aic8800_fdrv.ko}
AIC8800_FW_DIR=${AIC8800_FW_DIR:-$AIC_BASE_DIR/fw/aic8800D80}
ROM_HTTPD_BIN=${ROM_HTTPD_BIN:-$ROOT/board_tools_f1c200s/rom_httpd}
MIRACAST_RTSP_SINK_BIN=${MIRACAST_RTSP_SINK_BIN:-$ROOT/board_tools_f1c200s/miracast_rtsp_sink}
LINKS_BIN=${LINKS_BIN:-$ROOT/buildroot-2018.02.11/output/target/usr/bin/links}
ONS_GBK_BIN=${ONS_GBK_BIN:-$ROOT/third_party/ONScripter-GBK/onscripter-gbk}
SDL12_LIB=${SDL12_LIB:-$ROOT/buildroot-2018.02.11/output/target/usr/lib/libSDL-1.2.so.0.11.4}
CEDAR_DRM_PLAYER_BIN=${CEDAR_DRM_PLAYER_BIN:-$ROOT/third_party/cedar_drm_player/cedar_drm_player}
ADB_BIN=${ADB_BIN:-$ROOT/buildroot-2018.02.11/output/build/android-tools-4.2.2+git20130218/build-adb/adb}
ADB_USBONLY_BIN=${ADB_USBONLY_BIN:-$ROOT/buildroot-2018.02.11/output/build/android-tools-4.2.2+git20130218/build-adb_usbonly/adb}
ADB_FALLBACK_BIN=${ADB_FALLBACK_BIN:-$ROOT/buildroot-2018.02.11/output/build/android-tools-4.2.2+git20130218/build-adb_fallback/adb}
ADB_EXECOUT_BIN=${ADB_EXECOUT_BIN:-$ROOT/buildroot-2018.02.11/output/build/android-tools-4.2.2+git20130218/build-adb_execout/adb}
ADB_EXECOUT2_BIN=${ADB_EXECOUT2_BIN:-$ROOT/buildroot-2018.02.11/output/build/android-tools-4.2.2+git20130218/build-adb_execout2/adb}

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

find_ons_bin() {
    if [ -n "$ONS_BIN" ] && [ -f "$ONS_BIN" ]; then
        echo "$ONS_BIN"
        return 0
    fi

    for candidate in \
        "$ROOT/third_party/ONScripter-EN/onscripter-en" \
        "$ROOT/third_party/ONScripter-EN/src/onscripter" \
        "$ROOT/third_party/onscripter-en/onscripter-en" \
        "$ROOT/third_party/onscripter-en/src/onscripter" \
        "$HOME/LicheePi_Nano/third_party/ONScripter-EN/onscripter-en" \
        "$HOME/LicheePi_Nano/third_party/ONScripter-EN/src/onscripter"; do
        if [ -f "$candidate" ]; then
            echo "$candidate"
            return 0
        fi
    done

    return 1
}

ONS_BIN_FOUND=$(find_ons_bin || true)

[ -f "$ESP8089_SPI_KO" ] || ESP8089_SPI_KO=$ESP8089_PREBUILT_KO

copy_if_exists "$GMENU_TAR" "$OUT_DIR/gmenu2x-f1c200s.tar.gz"
copy_if_exists "$GPSP_BIN" "$OUT_DIR/gpsp"
copy_if_exists "$SNES9X4D_BIN" "$OUT_DIR/snes9x4d"
copy_if_exists "$SNES9X4D_RS90_BIN" "$OUT_DIR/snes9x4d_rs90"
copy_if_exists "$DINGUX_BIN" "$OUT_DIR/DinguxCommander"
copy_if_exists "$SDLGNUBOY_BIN" "$OUT_DIR/sdlgnuboy"
copy_if_exists "$PCSX_BIN" "$OUT_DIR/pcsx"
copy_if_exists "$PICODRIVE_ZIP" "$OUT_DIR/PicoDrive.zip"
copy_if_exists "$ONS_BIN_FOUND" "$OUT_DIR/onscripter-en"
copy_if_exists "$ONS_GBK_BIN" "$OUT_DIR/onscripter-gbk"
copy_if_exists "$CEDAR_DRM_PLAYER_BIN" "$OUT_DIR/cedar_drm_player"
copy_if_exists "$ADB_BIN" "$OUT_DIR/adb"
copy_if_exists "$ADB_USBONLY_BIN" "$OUT_DIR/adb_usbonly"
copy_if_exists "$ADB_FALLBACK_BIN" "$OUT_DIR/adb_fallback"
copy_if_exists "$ADB_EXECOUT_BIN" "$OUT_DIR/adb_execout"
copy_if_exists "$ADB_EXECOUT2_BIN" "$OUT_DIR/adb_execout2"
copy_if_exists "$CEDAR_ION_KO" "$OUT_DIR/sunxi_ion_core.ko"
copy_if_exists "$CEDAR_VE_KO" "$OUT_DIR/cedar_ve.ko"
copy_if_exists "$ESP8089_SPI_KO" "$OUT_DIR/esp8089-spi.ko"
copy_if_exists "$TVD_KO" "$OUT_DIR/suniv_f1c100s_tvd.ko"
copy_if_exists "$TVD_PREVIEW" "$OUT_DIR/tvd_fb_preview"
copy_if_exists "$MATRIX_PAD_BRIDGE_BIN" "$OUT_DIR/matrix_pad_bridge"
copy_if_exists "$MATRIX_PS2MOUSE_BRIDGE_BIN" "$OUT_DIR/matrix_ps2mouse_bridge"
copy_if_exists "$RTL8723BU_KO" "$OUT_DIR/8723bu.ko"
copy_if_exists "$RTL8188FU_KO" "$OUT_DIR/rtl8188fu.ko"
copy_if_exists "$RTL8188FU_FW" "$OUT_DIR/rtl8188fufw.bin"
copy_if_exists "$AIC_LOAD_FW_KO" "$OUT_DIR/aic_load_fw.ko"
copy_if_exists "$AIC8800_FDRV_KO" "$OUT_DIR/aic8800_fdrv.ko"
if [ -d "$AIC8800_FW_DIR" ]; then
    rm -rf "$OUT_DIR/aic8800D80"
    cp -a "$AIC8800_FW_DIR" "$OUT_DIR/aic8800D80"
    echo "copied: $AIC8800_FW_DIR"
else
    echo "missing: $AIC8800_FW_DIR"
fi
copy_if_exists "$ROM_HTTPD_BIN" "$OUT_DIR/rom_httpd"
copy_if_exists "$MIRACAST_RTSP_SINK_BIN" "$OUT_DIR/miracast_rtsp_sink"
copy_if_exists "$LINKS_BIN" "$OUT_DIR/links"
copy_if_exists "$SDL12_LIB" "$OUT_DIR/libSDL-1.2.so.0.11.4"

chmod 755 "$OUT_DIR"/gpsp "$OUT_DIR"/snes9x4d "$OUT_DIR"/snes9x4d_rs90 "$OUT_DIR"/DinguxCommander "$OUT_DIR"/sdlgnuboy "$OUT_DIR"/pcsx "$OUT_DIR"/onscripter-en "$OUT_DIR"/onscripter-gbk "$OUT_DIR"/cedar_drm_player "$OUT_DIR"/tvd_fb_preview "$OUT_DIR"/matrix_pad_bridge "$OUT_DIR"/matrix_ps2mouse_bridge 2>/dev/null || true
chmod 755 "$OUT_DIR"/adb "$OUT_DIR"/adb_usbonly "$OUT_DIR"/adb_fallback "$OUT_DIR"/adb_execout "$OUT_DIR"/adb_execout2 2>/dev/null || true
chmod 644 "$OUT_DIR"/sunxi_ion_core.ko 2>/dev/null || true
chmod 644 "$OUT_DIR"/cedar_ve.ko 2>/dev/null || true
chmod 644 "$OUT_DIR"/esp8089-spi.ko 2>/dev/null || true
chmod 644 "$OUT_DIR"/8723bu.ko 2>/dev/null || true
chmod 644 "$OUT_DIR"/rtl8188fu.ko 2>/dev/null || true
chmod 644 "$OUT_DIR"/rtl8188fufw.bin 2>/dev/null || true
chmod 644 "$OUT_DIR"/aic_load_fw.ko 2>/dev/null || true
chmod 644 "$OUT_DIR"/aic8800_fdrv.ko 2>/dev/null || true
chmod 644 "$OUT_DIR"/aic8800D80/* 2>/dev/null || true
chmod 755 "$OUT_DIR"/rom_httpd 2>/dev/null || true
chmod 755 "$OUT_DIR"/miracast_rtsp_sink 2>/dev/null || true
chmod 755 "$OUT_DIR"/links 2>/dev/null || true
chmod 755 "$OUT_DIR"/libSDL-1.2.so.0.11.4 2>/dev/null || true

echo "payload ready: $OUT_DIR"
