#!/bin/sh

set -e

ROOT_PREFIX=${ROOT_PREFIX:-}
ROOT_DIR=${ROOT_PREFIX}/root
GMENU=${ROOT_DIR}/gmenu2x
ROM_BASE=${ROOT_DIR}/roms

mkdir -p "${GMENU}/sections/emulators" "${GMENU}/sections/applications"
mkdir -p "${ROM_BASE}/gba" "${ROM_BASE}/gb" "${ROM_BASE}/md" "${ROM_BASE}/sfc" "${ROM_BASE}/ps1" "${ROM_BASE}/ons" "${ROM_BASE}/video"

cat > "${GMENU}/input.conf" <<'EOF_INPUT'
up=keyboard,273,SDLK_UP
down=keyboard,274,SDLK_DOWN
left=keyboard,276,SDLK_LEFT
right=keyboard,275,SDLK_RIGHT
confirm=keyboard,97,SDLK_a
cancel=keyboard,98,SDLK_b
menu=keyboard,13,SDLK_RETURN
settings=keyboard,9,SDLK_TAB
manual=keyboard,120,SDLK_x
modifier=keyboard,121,SDLK_y
section_prev=keyboard,280,SDLK_PAGEUP
section_next=keyboard,281,SDLK_PAGEDOWN
pageup=keyboard,280,SDLK_PAGEUP
pagedown=keyboard,281,SDLK_PAGEDOWN
dec=keyboard,121,SDLK_y
inc=keyboard,120,SDLK_x
power=keyboard,279,SDLK_END
EOF_INPUT

if [ -x "${ROOT_DIR}/run_gpsp_menu.sh" ] || [ -x "${ROOT_DIR}/gpsp" ]; then
cat > "${GMENU}/sections/emulators/gba" <<'EOF_GBA'
title=GBA
description=gpSP GameBoy Advance
exec=/root/run_gpsp_from_gmenu2x.sh
workdir=/root
params=[selFullPath]
selectordir=/root/roms/gba
selectorbrowser=true
selectorfilter=.gba,.zip
wrapper=true
EOF_GBA
fi

if [ -x "${ROOT_DIR}/sdlgnuboy" ]; then
cat > "${GMENU}/sections/emulators/gb" <<'EOF_GB'
title=GB/GBC
description=sdlgnuboy GameBoy
exec=/root/sdlgnuboy
workdir=/root
params=[selFullPath]
selectordir=/root/roms/gb
selectorbrowser=true
selectorfilter=.gb,.gbc,.zip
wrapper=true
EOF_GB
fi

rm -f "${GMENU}/sections/emulators/sfc"

if [ -x "${ROOT_DIR}/run_snes9x4d.sh" ] || [ -x "${ROOT_DIR}/snes9x4d" ]; then
cat > "${GMENU}/sections/emulators/sfc" <<'EOF_SFC'
title=SFC
description=Snes9x4D
exec=/root/run_snes9x4d.sh
workdir=/root
params=[selFullPath]
selectordir=/root/roms/sfc
selectorbrowser=true
selectorfilter=.sfc,.smc,.fig,.zip
wrapper=true
EOF_SFC
fi

if [ -x "${ROOT_DIR}/run_snes9x4d_rs90.sh" ] || [ -x "${ROOT_DIR}/snes9x4d_rs90" ]; then
cat > "${GMENU}/sections/emulators/sfc_rs90" <<'EOF_SFC_RS90'
title=SFC RS90
description=Snes9x4D RS90 lowres test
exec=/root/run_snes9x4d_rs90.sh
workdir=/root
params=[selFullPath]
selectordir=/root/roms/sfc
selectorbrowser=true
selectorfilter=.sfc,.smc,.fig,.zip
wrapper=true
EOF_SFC_RS90
fi

if [ -x "${ROOT_DIR}/run_picodrive.sh" ] || [ -x "${ROOT_DIR}/picodrive/picodrive" ]; then
cat > "${GMENU}/sections/emulators/md" <<'EOF_MD'
title=MD
description=PicoDrive Mega Drive
exec=/root/run_picodrive.sh
workdir=/root/picodrive
params=[selFullPath]
selectordir=/root/roms/md
selectorbrowser=true
selectorfilter=.md,.bin,.gen,.smd,.sms,.gg,.sg,.cue,.chd,.zip
wrapper=true
EOF_MD
fi

if [ -x "${ROOT_DIR}/run_pcsx.sh" ] || [ -x "${ROOT_DIR}/pcsx" ]; then
cat > "${GMENU}/sections/emulators/ps1" <<'EOF_PS1'
title=PS1
description=PCSX-ReARMed
exec=/root/run_pcsx.sh
workdir=/root
params=[selFullPath]
selectordir=/root/roms/ps1
selectorbrowser=true
selectorfilter=.cue,.chd,.pbp,.iso,.bin,.img
wrapper=true
EOF_PS1
fi

if [ -x "${ROOT_DIR}/run_onscripter.sh" ] || [ -x "${ROOT_DIR}/onscripter-en" ]; then
cat > "${GMENU}/sections/emulators/ons" <<'EOF_ONS'
title=ONS
description=ONScripter-EN
exec=/root/run_onscripter.sh
workdir=/root
params=[selFullPath]
selectordir=/root/roms/ons
selectorbrowser=true
directories=true
wrapper=true
EOF_ONS
fi

if [ -x "${ROOT_DIR}/DinguxCommander.sh" ] || [ -x "${ROOT_DIR}/DinguxCommander" ]; then
cat > "${GMENU}/sections/applications/files" <<'EOF_FILES'
title=Files
description=DinguxCommander
exec=/root/DinguxCommander.sh
workdir=/root
wrapper=true
EOF_FILES
fi

if [ -x "${ROOT_DIR}/start_bluetooth.sh" ]; then
cat > "${GMENU}/sections/applications/bluetooth" <<'EOF_BT'
title=Bluetooth
description=Start RTL8723BU Bluetooth
builtin=bluetooth_start
exec=/bin/true
workdir=/root
wrapper=true
EOF_BT
fi

if [ -x "${ROOT_DIR}/scan_wifi.sh" ]; then
cat > "${GMENU}/sections/applications/wifi_scan" <<'EOF_WIFI_SCAN'
title=WiFi Scan
description=Search nearby WiFi APs
builtin=wifi_scan
exec=/bin/true
workdir=/root
wrapper=true
EOF_WIFI_SCAN
fi

if [ -x "${ROOT_DIR}/start_bluetooth.sh" ]; then
cat > "${GMENU}/sections/applications/bluetooth_scan" <<'EOF_BT_SCAN'
title=BT Scan
description=Search nearby Bluetooth devices
builtin=bluetooth_scan
exec=/bin/true
workdir=/root
wrapper=true
EOF_BT_SCAN
fi

if [ -x "${ROOT_DIR}/run_rom_httpd.sh" ]; then
cat > "${GMENU}/sections/applications/rom_server" <<'EOF_ROM_SERVER'
title=ROM Server
description=Browser upload/download for /root/roms
builtin=rom_server
exec=/bin/true
workdir=/root
wrapper=true
EOF_ROM_SERVER
fi

if [ -x "${ROOT_DIR}/run_browser.sh" ] || [ -x "${ROOT_PREFIX}/usr/bin/links" ]; then
cat > "${GMENU}/sections/applications/browser" <<'EOF_BROWSER'
title=Browser
description=Links text web browser
builtin=browser
exec=/bin/true
workdir=/root
wrapper=true
EOF_BROWSER
fi

cat > "${GMENU}/sections/applications/command_line" <<'EOF_COMMAND_LINE'
title=Command Line
description=Run shell commands in GMenu2X
exec=internal:command_line
workdir=/root
params=
wrapper=true
EOF_COMMAND_LINE

if [ -x "${ROOT_DIR}/record_android_360x640.sh" ] && [ -x "${ROOT_DIR}/adb_execout2" ]; then
cat > "${GMENU}/sections/applications/android_record" <<'EOF_ANDROID_RECORD'
title=Android Rec
description=Record Android screen 360x640
exec=/root/record_android_360x640.sh
workdir=/root
params=f1c_android_360x640.mp4
wrapper=true
EOF_ANDROID_RECORD
fi

if [ -x "${ROOT_DIR}/run_cedar_drm_player.sh" ] || [ -x "${ROOT_DIR}/cedar_drm_player" ]; then
cat > "${GMENU}/sections/applications/video" <<'EOF_VIDEO'
title=Video
description=Cedar hardware video player
exec=/root/run_cedar_drm_player.sh
workdir=/root
params=[selFullPath]
selectordir=/root/roms/video
selectorbrowser=true
selectorfilter=.mp4,.m4v,.264,.h264
wrapper=true
EOF_VIDEO
fi

if [ -x "${ROOT_DIR}/run_tvd_preview.sh" ] || [ -x "${ROOT_DIR}/tvd_fb_preview" ]; then
cat > "${GMENU}/sections/applications/tvd_preview" <<'EOF_TVD_PREVIEW'
title=TVD Preview
description=Preview TV input on framebuffer
exec=/root/run_tvd_preview.sh
workdir=/root
wrapper=true
EOF_TVD_PREVIEW
fi

if [ -x "${ROOT_DIR}/check_wifi_speed.sh" ]; then
cat > "${GMENU}/sections/applications/wifi_speed" <<'EOF_WIFI_SPEED'
title=WiFi Speed
description=Run WiFi throughput check
exec=/root/check_wifi_speed.sh
workdir=/root
wrapper=true
EOF_WIFI_SPEED
fi

if [ -x "${ROOT_DIR}/check_bluetooth.sh" ]; then
cat > "${GMENU}/sections/applications/bluetooth_check" <<'EOF_BT_CHECK'
title=BT Check
description=Show Bluetooth status
exec=/root/check_bluetooth.sh
workdir=/root
wrapper=true
EOF_BT_CHECK
fi

if [ -x "${ROOT_DIR}/test_matrix_keys.sh" ]; then
cat > "${GMENU}/sections/applications/key_test" <<'EOF_KEY_TEST'
title=Key Test
description=Test matrix keypad events
exec=/root/test_matrix_keys.sh
workdir=/root
wrapper=true
EOF_KEY_TEST
fi

cat > "${GMENU}/sections/applications/reboot" <<'EOF_REBOOT'
title=Reboot
description=Restart system
builtin=reboot
exec=/bin/true
workdir=/root
wrapper=true
EOF_REBOOT

cat > "${GMENU}/sections/applications/poweroff" <<'EOF_POWEROFF'
title=Poweroff
description=Shutdown system
builtin=poweroff
exec=/bin/true
workdir=/root
wrapper=true
EOF_POWEROFF

sync
echo "F1C200S SDL app layout installed into ${ROOT_DIR}"
