# F1C200S Build Commands

> READ THIS FILE FIRST before any F1C200S work.

## Resume Rules
```text
1. After any compact/resume, read this file first before touching F1C200S work.
2. Treat this file as the canonical memory for host paths, board access, build commands, and SDL integration rules.
3. Default to host-side Linux work. Do not treat the Windows workspace as the final build environment.
4. If board hardware is not present, keep preparing host-side source, scripts, config, and build outputs.
5. After a feature/fix is confirmed working, clean up its corresponding temporary
   local backup/work directories from the Windows Desktop. Keep only canonical
   docs, current active work dirs, deployable scripts, and genuinely reusable
   references. Do not let old `F1C200S_*_work`, `*_fix`, `*_debug`, `*_tmp`,
   or copied artifact folders accumulate after the function is finished.
```

## AIC Miracast Current Stable Chain - 2026-08-03
```text
Current best startup:
  /root/aic_miracast/register_aic_wlan1_v25.sh start
  /root/aic_miracast/start_miracast_go_v25_lowest_null.sh

Behavior:
- P2P GO mode on wlan1.
- No p2p_find.
- 5GHz GO frequency 5805.
- display name F1C200S-AIC.
- board GO IP 192.168.49.1.
- passphrase 12345678.
- SINK_BIN=/root/aic_miracast/miracast_sink_dump.lowest.
- CAPTURE_MODE=null, so no recording.

Module/script artifacts:
- v18 loader:
    /root/aic_miracast/candidates/aic8800_rx_lowmem_threshold1024_20260802_v18/aic_load_fw.ko
    md5 698b92823499cb1843fe9ab64a2e7df0
- v25 fdrv:
    /root/aic_miracast/candidates/aic8800_rx_msg_clamp_only_20260802_v25/aic8800_fdrv/aic8800_fdrv.ko
    md5 13ceac918ec83d77acb2ecb5a5c5d37e
- register script:
    /root/aic_miracast/register_aic_wlan1_v25.sh
    md5 4164af3f4ca4668468a5f7c07e8b8918
- lowest/null startup script:
    /root/aic_miracast/start_miracast_go_v25_lowest_null.sh
    md5 b7860438474304ae1b30a1bfac7f3875
- shared GO watcher script:
    /root/aic_miracast/start_aic_miracast_cdump_board.sh
    md5 56cf5e1662afcba965849549f1c1a3c1
- lowest sink binary:
    /root/aic_miracast/miracast_sink_dump.lowest
    md5 ceeeb9958f8f84f2a0b7c6c3df63db0e

Stable evidence:
- User reported this version streamed for about one to two hours without
  disconnecting.
- Board was still running:
    /root/aic_miracast/miracast_sink_dump.lowest 192.168.49.52 /dev/null
- wlan1 counters during the stable run:
    RX packets 2040305
    RX bytes   2195154967
- watch.log continued past t=1930 without rx_stalled.

Interpretation:
- Earlier normal/recording runs repeatedly failed as:
    RTP stops increasing,
    RTSP keepalive still works,
    aicwf bad RX skb/invalid vif messages accumulate,
    cmd_mgr_queue cmd timed-out.
- That points to AIC USB RX data-path desynchronization under sustained load,
  not a Miracast discovery or RTSP heartbeat issue.
- The current stable workaround is lowest sink binary plus no recording.

Rules:
- Keep AIC8800 manual/script-only. Do not autoload it at normal boot.
- If AIC enters cmd_mgr_queue timeout or modules cannot unload, hard power
  cycle before the next test. Do not trust half-dead a69c:8d83 state.
- Do not use p2p_find.
- Do not use repeated sink/listen loops.
- Do not use v24 msg-rx-filter candidate; it registered wlan1 but broke
  wlan1 up with Broken pipe.
- Miracast startup should enter low-memory mode by default:
    STOP_GMENU=1
    STOP_BLUETOOTH=1
    STOP_DBUS=1
    DROP_PAGE_CACHE=1
  This stops gmenu2x, bluetoothd/btmon, and dbus-daemon before starting the
  P2P GO path, then drops page cache. These services are intentionally not
  restarted by the Miracast script.
- Non-null H264 capture output must default to /root/roms/video, not
  /root/aic_runtime/captures or any other path under the small rootfs.

Dedicated docs:
- AIC_MIRACAST_INDEX.md
- AIC_MIRACAST_V25_LOWEST_STABLE_20260803.md
```

## AIC Miracast FIFO Output Guard - 2026-08-03
```text
Purpose:
- Prepare a new cdump variant for live Cedar/FIFO display tests without
  replacing the current stable lowest/null chain.
- The goal is to prevent the H264 output side from blocking RTP receive when
  the downstream player stalls.

Backup:
- Original source was backed up on the Ubuntu host:
  /home/wnk/F1C200S_host_archive/miracast_fifo_guard/20260803_014946/
  md5 ca49b021e1fe3c8690f48532f61e813b

New source:
- /home/wnk/LicheePi_Nano/third_party/lazycast_host_20260721/miracast_sink_dump_fifo_guard.c
- md5 557da1d9420262e7a4782e4fddeb1fbc

New binary:
- /home/wnk/LicheePi_Nano/third_party/lazycast_host_20260721/miracast_sink_dump.fifo_guard.brarm
- md5 afb23f480a63d67632007e6d14134b71

Implementation:
- Regular file output still uses normal blocking writes.
- FIFO/stdout pipe output is detected with fstat/stat.
- FIFO/stdout pipe output is set to nonblocking mode.
- It tries to enlarge the pipe buffer to 256KiB with F_SETPIPE_SZ.
- If output write returns EAGAIN/EWOULDBLOCK, the remaining current H264 payload
  chunk is dropped and dropped_bytes is counted.
- This is first-level protection only. It may drop partial H264 payloads, so
  picture corruption is possible, but it should prevent Cedar/player stalls from
  backpressuring RTP receive.

Next safer version:
- Add Annex-B NAL boundary parsing.
- Drop complete NALs only.
- After a drop, hold output until the next IDR and replay cached SPS/PPS.
```

## AIC Miracast Recording Disconnect Fix - 2026-08-02
```text
Symptom:
- After enabling H.264 recording, Miracast connected and streamed for about
  14 minutes, then disconnected.

Finding:
- This was not a new AIC v18 RTP instability.
- watch.log showed cdump writing to:
    /root/aic_runtime/captures/miracast_19700101_003718.h264
  even though start_miracast_go_v18.sh passed:
    OUT_DIR=/root/roms/video
- /dev/root reached 100%, and watch.log showed:
    tail: write error: No space left on device
- Final stats before disconnect:
    rtp_packets=223821
    rtp_bytes=227966440
    h264_bytes=43284400

Fix:
- Moved the misplaced H.264 capture from /root/aic_runtime/captures to
  /root/roms/video.
- Patched /root/aic_miracast/start_aic_miracast_cdump_board.sh so both:
    OUT_DIR=${OUT_DIR:-/root/roms/video}
  and the generated watcher:
    OUT_DIR=/root/roms/video
- Fixed board script md5:
    190f00e81fb331e2792ab74588ce1536

Rule:
- Recording captures must stay on /root/roms/video, not under
  /root/aic_runtime, because /root is the small rootfs.
```

## Stable Kernel Baseline Commit - 2026-07-30
```text
Host repo:
- /home/wnk/LicheePi_Nano

Branch:
- master

Commit:
- 26d5a6a Record stable F1C200S kernel baseline

Scope:
- Current kernel baseline reported stable by user.
- Committed only kernel source/config files:
  linux/.config
  linux/arch/arm/boot/dts/suniv-f1c100s-licheepi-nano.dts
  linux/arch/arm/boot/dts/suniv-f1c100s.dtsi

Important:
- Other unrelated dirty files in the host monorepo were intentionally left
  untouched and not included in this commit.
- The temporary branch `kernel-panel-mode-switch-test-20260730` still exists,
  but master now contains this stable kernel baseline commit.
```

## F1C200S OC SD-card Test Package - 2026-07-30
```text
Purpose:
- Prepare the user's separate system card for CPU/DDR overclock testing.

Host-side card during preparation:
- /dev/sdb
- /dev/sdb1 FAT boot
- /dev/sdb2 ext4 rootfs
- Card was unmounted after preparation.

Backup before modification:
- /home/wnk/F1C200S_host_archive/oc_sdcard_backups/20260730_sdb_before_oc
- first1M md5:
  07d2fe28c7e54965f45e3381cd6eb9f1

Flashed U-Boot/SPL:
- Source image:
  /home/wnk/LicheePi_Nano/u-boot/u-boot-sunxi-with-spl.ili9488_no_de_hvsync_oc1152_ddr216_20260730.bin
- Config:
  CONFIG_SYS_CLK_FREQ=1152000000
  CONFIG_DRAM_CLK=216
- md5:
  0a5eaba753bc94419cf37849f038e667
- SD readback md5:
  0a5eaba753bc94419cf37849f038e667

Boot partition zImage:
- md5:
  7d369a51864c4b4dd79602143bd94aec
- This is the confirmed ILI9488 no-DE/HV-sync landscape timing:
  480x320, hfp=14 hsync=2 hbp=2, vfp=30 vsync=2 vbp=2.

Rootfs package installed on the card:
- /root/oc_test/README_OC_TEST_20260730.txt
- /root/oc_test/oc_status.sh
- /root/oc_test/restore_known_good_uboot.sh
- /root/oc_test/flash_oc_uboot.sh
- /root/oc_test/uboot_oc1152_ddr216.bin
- /root/oc_test/uboot_known_good_408_ddr156.bin
- /root/oc_test/zImage_landscape_480x320_h14_2_2_v30_2_2

After booting this card on the board:
- Run:
  /root/oc_test/oc_status.sh
- If the OC card boots but needs safe-clock U-Boot restored:
  /root/oc_test/restore_known_good_uboot.sh

Voltage warning:
- This OC image is intended for the raised-rail hardware test discussed with
  the user, about 1.5V VCPU and 2.7V DDR.
- Do not use it as a stock-voltage default image.

OC step test results:
- 1152MHz CPU / DDR216:
  SPL prints DRAM 64 MiB and then hangs at "Trying to boot from MMC1".
  U-Boot proper/Linux do not start.
- 1008MHz CPU / DDR156:
  Same failure: SPL prints DRAM 64 MiB and then hangs at
  "Trying to boot from MMC1".
- 720MHz CPU / DDR156:
  Boots Linux successfully.
  Confirmed by /root/oc_test/oc_status.sh:
    pll-cpu = 720000000
    cpu = 720000000
    pll-ddr = 312000000
    AHB = 200000000
    APB = 100000000
    VE = 210000000
    tcon-pixel-clock = 12000000
    boot zImage md5 = 7d369a51864c4b4dd79602143bd94aec
    U-Boot/SPL md5 = a07b93a2d49c21be653ff3d857936b3b
  But USB/WiFi is not stable:
    USB hub/devices enumerate as full-speed, not high-speed.
    MUSB descriptor reads hit -110 timeouts.
    RTL8723BU wlan0 cannot be brought UP.
    Bluetooth HCI commands time out.
  Conclusion:
    720MHz CPU itself is active and can boot, but this is not yet a stable
    runtime configuration because USB high-speed breaks.

Next OC debug direction:
- Compare against a 408MHz/DDR156 safe-clock image on the same SD card and
  same hardware to confirm USB high-speed returns.
- If 408 works, inspect/fix clock tree or U-Boot clock handoff so 720MHz does
  not disturb USB/MUSB/high-speed enumeration.
- Do not continue to 864/1008/1152 until 720MHz keeps USB high-speed stable.

720MHz retest after RTL8723BU WiFi recovery - 2026-07-31:
- Board was booted from the current SD card after flashing:
  /home/wnk/LicheePi_Nano/u-boot/u-boot-sunxi-with-spl.ili9488_no_de_hvsync_oc720_ddr156_20260730.bin
- Image md5:
  a07b93a2d49c21be653ff3d857936b3b
- Board-side backup before flashing:
  /root/oc_test/sd_first1M_before_oc720_20260731.bin
  md5 0d3251b90cedf935745a364f6f1e74bd
- Flash offset:
  /dev/mmcblk0, bs=1024, seek=8
- Readback after flashing matched:
  a07b93a2d49c21be653ff3d857936b3b
- Reboot result:
  Linux booted and SSH returned at 10.0.0.161.
  RTL8723BU enumerated as USB high-speed:
    usb 1-1: new high-speed USB device number 2 using musb-hdrc
  WiFi associated with wnk641_2.4G and DHCP obtained 10.0.0.161.
  Ping to 10.0.0.1 and 8.8.8.8 succeeded.
- Confirmed clocks from /sys/kernel/debug/clk/clk_summary:
  pll-cpu = 720000000
  cpu = 720000000
  pll-ddr = 312000000
  ahb = 200000000
  apb = 100000000
  pll-ve = 210000000
  ve = 210000000
  tcon-pixel-clock = 12000000
- Note:
  Current WiFi init script can recover from the first failed scan by reloading
  8723bu, but logs show duplicate S45usb-wifi starts. Keep the successful
  720MHz result, then clean up WiFi init locking before using this as a final
  default runtime image.

720MHz CPU / DDR216 voltage-raised test - 2026-07-31:
- Hardware rails during this test:
  VCPU about 1.5V, DDR about 2.8V.
- Image:
  /home/wnk/LicheePi_Nano/u-boot/u-boot-sunxi-with-spl.ili9488_no_de_hvsync_oc720_ddr216_20260731.bin
- Image md5:
  da1cbb69b396aa54a52f2a46f6d3ef9b
- SD card flashed on host /dev/sdb at:
  bs=1024, seek=8
- Host-side backup before flashing:
  /home/wnk/F1C200S_host_archive/oc_sdcard_backups/20260731_sdb_before_oc720_ddr216/sdb_first1M_before_oc720_ddr216.bin
  md5 394a59aee8fb42df1e6f23fc0a9817d7
- Readback after flashing matched:
  da1cbb69b396aa54a52f2a46f6d3ef9b
- Boot result:
  User reported successful boot and SSH access.
  Board IP was 10.0.0.161.
- Confirmed clocks from /sys/kernel/debug/clk/clk_summary:
  pll-cpu = 720000000
  cpu = 720000000
  pll-ddr = 432000000
  ahb = 200000000
  apb = 100000000
  pll-ve = 210000000
  ve = 210000000
  tcon-pixel-clock = 12000000
- Runtime checks:
  wlan0 had DHCP address 10.0.0.161.
  ping 10.0.0.1 succeeded with low latency.
  lsusb showed RTL8723BU as 0bda:b720.
  dmesg showed USB high-speed enumeration:
    usb 1-1: new high-speed USB device number 2 using musb-hdrc
  dmesg grep for Oops/panic/Unable after boot showed no runtime crash.
- Remaining notes:
  Bluetooth still has the known first firmware-download timeout/reset, then
  succeeds on the second enumeration.
  WiFi script currently recovers by reloading 8723bu after the first scan miss,
  but duplicate S45usb-wifi starts are still visible and should be cleaned up.
  This is the best current OC candidate because 1008/156, 1008/216, and 864/216
  all produced boot/runtime crashes while 720/216 booted and networked.

Main system U-Boot updated to 720MHz CPU / DDR216 - 2026-07-31:
- User requested installing the confirmed 720/216 U-Boot onto the main system.
- Board was reachable over SSH at 10.0.0.107.
- Installed image:
  /root/u-boot-oc720-ddr216.bin
  md5 da1cbb69b396aa54a52f2a46f6d3ef9b
- Main-system backup before flashing:
  /root/oc_test/main_sd_first1M_before_oc720_ddr216_20260731.bin
  md5 6b88628d818e56c2e71e898f85c4e6ba
- Flash target:
  /dev/mmcblk0, bs=1024, seek=8
- Readback md5 after flashing:
  da1cbb69b396aa54a52f2a46f6d3ef9b
- Reboot verification:
  Board returned on SSH at 10.0.0.107.
  wlan0 had IP 10.0.0.107.
  lsusb showed RTL8723BU as 0bda:b720.
  USB enumerated as high-speed:
    usb 1-1: new high-speed USB device number 2 using musb-hdrc
  dmesg grep for Oops/panic/Unable to handle was clean.
- Confirmed main-system clocks:
  pll-cpu = 720000000
  cpu = 720000000
  pll-ddr = 432000000
  ahb = 200000000
  apb = 100000000
  pll-ve = 210000000
  ve = 210000000
  tcon-pixel-clock = 12000000

Main U-Boot source configured for ST7701 360x640 720MHz CPU / DDR216 - 2026-07-31:
- User requested keeping the main U-Boot on the previous ST7701 screen while
  making the confirmed stable overclock the source default.
- Correction:
  Do not use the older 320x480 fallback timing for this main ST7701 target.
  The main ST7701 panel timing is the 360x640 U-Boot timing preserved in the
  old U-Boot .config backups.
- Host repo:
  /home/wnk/LicheePi_Nano/u-boot
- Source config:
  CONFIG_SYS_CLK_FREQ=720000000
  CONFIG_DRAM_CLK=216
  CONFIG_VIDEO_LCD_MODE="x:360,y:640,depth:16,pclk_khz:14885,le:10,ri:8,up:6,lo:2,hs:3,vs:3,sync:3,vmode:0"
- Board init switched from ILI9488 to ST7701:
  board/sunxi/Makefile links st7701-spi-init.o.
  board/sunxi/board.c calls st7701_spi_bootloader_setup().
- Build command used:
  cd /home/wnk/LicheePi_Nano/u-boot
  export PATH=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
  /usr/bin/make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- licheepi_nano_defconfig
  /usr/bin/make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- -j4
- Build result:
  /home/wnk/LicheePi_Nano/u-boot/u-boot-sunxi-with-spl.st7701_360x640_oc720_ddr216_main_20260731.bin
  md5 8a758679fcf1e2c706d9b858dec69c3b
- Verification:
  include/generated/autoconf.h has CONFIG_SYS_CLK_FREQ 720000000 and
  CONFIG_DRAM_CLK 216, and CONFIG_VIDEO_LCD_MODE set to 360x640/pclk 14885.
  board/sunxi/built-in.o contains ST7701 strings and no ILI9488 init string.
- Rootfs boot scripts:
  Buildroot was re-run and regenerated output/images/rootfs.tar.
  Actual applied overlay is:
    /home/wnk/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay
  Final rootfs S17rtl8723bu md5:
    2a5f80565e6729aedcb2ad5d24fda045
  This final S17rtl8723bu only waits for and loads RTL8723BU (0bda:b720);
  it contains no AIC8800/a69c auto-load path, satisfying the normal-boot rule
  that AIC8800 must not be loaded automatically.
  Regenerated rootfs.tar md5:
    c1a6275245c543f350d1f75f882906dc

Main firmware image generation script - 2026-07-31:
- New host-side script:
  /home/wnk/LicheePi_Nano/make_firmware_st7701_oc720.sh
- Purpose:
  Generate a complete SD-card image using the current main artifacts:
    ST7701 360x640 U-Boot/SPL at CPU 720MHz / DDR216,
    linux/arch/arm/boot/zImage,
    linux/arch/arm/boot/dts/suniv-f1c100s-licheepi-nano.dtb,
    buildroot-2018.02.11/output/images/rootfs.tar,
    optional boot.scr.
- Default output:
  f1c200s_st7701_360x640_oc720_ddr216_main.img
- Default layout:
  U-Boot/SPL written to disk image offset 8KiB.
  p1 BOOT: start 1MiB, 32MiB, FAT.
  p2 rootfs: 768MiB, ext4.
  p3 ROMS: remaining space, FAT32.
- Optional environment flags:
  IMG_FILE=...
  IMG_SIZE_MB=...
  BUILD_UBOOT=1
  BUILD_ROOTFS=1
  SUDO='sudo -S' for non-interactive sudo testing.
- Safety check:
  The script refuses to pack if /etc/init.d/S17rtl8723bu inside rootfs.tar
  contains AIC8800/a69c auto-load logic.
- Validation:
  A temporary 900MiB test image was generated successfully at:
    /tmp/f1c_fw_pack_test.img
  md5:
    37762b1b8b6fa56e16fd34b7dbb84554

Rootfs runtime payload audit and fix - 2026-07-31:
- User requested confirming compiled software and written scripts are actually
  packed into rootfs.
- Final rootfs tar:
  /home/wnk/LicheePi_Nano/buildroot-2018.02.11/output/images/rootfs.tar
  md5 394fa25cfc4e5c3633cb3caf4336eee8
- Runtime manifest:
  /home/wnk/LicheePi_Nano/rootfs_runtime_manifest_20260731.txt
- Runtime bundle fixes:
  /home/wnk/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/collect_runtime_payload.sh
  now collects:
    backups/sfc_cleanup_20260623_010846/snes9x4d/snes9x4d
    tvd_f1c100s_linux57/tools/fb_preview/tvd_fb_preview
    board_tools_f1c200s/miracast_rtsp_sink
  /home/wnk/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/install_payload_to_target.sh
  now installs /root/miracast_rtsp_sink and enforces executable permissions
  for /etc/init.d/S*, /etc/init.d/rcS, /etc/init.d/rcK, and /root/*.sh.
- Confirmed in final rootfs.tar:
  Core apps:
    /root/gmenu2x/gmenu2x
    /root/gpsp
    /root/snes9x4d
    /root/snes9x4d_rs90
    /root/pcsx
    /root/picodrive/picodrive
    /root/onscripter-en
    /root/onscripter-gbk
    /root/DinguxCommander
    /root/cedar_drm_player
    /root/tvd_fb_preview
    /root/miracast_rtsp_sink
    /root/rom_httpd
    /usr/bin/links
  Modules:
    /lib/modules/5.7.1/extra/8723bu.ko
    /lib/modules/5.7.1/extra/8723du.ko
    /lib/modules/5.7.1/extra/8733bu.ko
    /lib/modules/5.7.1/extra/rtl8188fu.ko
    /lib/modules/5.7.1/extra/esp8089-spi.ko
    /lib/modules/5.7.1/extra/sunxi_ion_core.ko
    /lib/modules/5.7.1/extra/cedar_ve.ko
    /lib/modules/5.7.1/extra/suniv_f1c100s_tvd.ko
    /lib/modules/5.7.1/extra/aic_load_fw.ko
    /lib/modules/5.7.1/extra/aic8800_fdrv.ko
  Startup scripts:
    /etc/init.d/S02loopback
    /etc/init.d/S16usb-host
    /etc/init.d/S17rtl8723bu
    /etc/init.d/S18cedar
    /etc/init.d/S19roms
    /etc/init.d/S45usb-wifi
    /etc/init.d/S99gmenu2x
    all are executable in rootfs.tar.
- AIC rule:
  /etc/init.d/S17rtl8723bu in final rootfs.tar contains no AIC8800/a69c
  auto-load logic. AIC modules and firmware may exist as files for manual use,
  but normal boot does not load AIC8800.
- Remaining non-blocking optional gap:
  sdlgnuboy is still missing, so no GameBoy menu entry is generated unless it
  is built and added later.

Host cleanup of obsolete scripts/backups - 2026-07-31:
- User requested cleaning the many old helper scripts and backup files from the
  host repo, not just build caches.
- Cleaned:
  /home/wnk/LicheePi_Nano/pach.sh
  /home/wnk/LicheePi_Nano/linux/pach.sh
  /home/wnk/LicheePi_Nano/tvd_f1c100s_linux57/pach.sh
  /home/wnk/LicheePi_Nano/debian.sh
  /home/wnk/LicheePi_Nano/AUTO SCRIPT
  /home/wnk/LicheePi_Nano/f1c200s_driver
  /home/wnk/LicheePi_Nano/backup
  /home/wnk/LicheePi_Nano/backups
  /home/wnk/LicheePi_Nano/third_party/backup_snes9x4d_20260610_183953
  /home/wnk/LicheePi_Nano/u-boot/backups_scheme1_20260614_071212
  /home/wnk/LicheePi_Nano/u-boot/backups_scheme1_20260614_071233
  top-level duplicated board_tools_f1c200s helper scripts:
    enable_uinput.sh, export_kernel_delta.sh, replace_dtb.sh,
    start_matrix_pad_bridge.sh, stop_matrix_pad_bridge.sh
  old duplicated gmenu2x/gpsp/gnuboy helper scripts:
    gmenu2x/install_gmenu2x_f1c200s_layout.sh
    gmenu2x/run_gmenu2x.sh
    gmenu2x/disable_hwcheck_in_source.sh
    gpsp/run_gpsp.sh
    gpsp/run_gpsp_menu.sh
    gnuboy-master/cross_build.sh
  scattered stale backup files:
    *.bak, *.bak_*, *.before, *.before_*, *.orig, *.new
  stale driver build .tmp_versions directories under third_party.
- Preserved:
  /home/wnk/LicheePi_Nano/make_firmware_st7701_oc720.sh
  /home/wnk/LicheePi_Nano/board_tools_f1c200s/runtime_bundle
  /home/wnk/LicheePi_Nano/rootfs_runtime_manifest_20260731.txt
  current U-Boot/rootfs/kernel artifacts and source trees.
- Before deleting backups, the only runtime dependency still pointing into
  backups was moved:
  /home/wnk/LicheePi_Nano/board_tools_f1c200s/prebuilt_bins/snes9x4d
  and collect_runtime_payload.sh now uses that stable prebuilt path.
- Post-clean validation:
  find at maxdepth 3 found no remaining *.bak/*.before/*.orig/*.new files.
  Top-level .sh scan now shows only:
    /home/wnk/LicheePi_Nano/make_firmware_st7701_oc720.sh
  Runtime payload collection still works for snes9x4d, tvd_fb_preview, and
  miracast_rtsp_sink.

ST7701 no-DE/HV-sync correction - 2026-07-31:
- User identified the current board does not wire the RGB DE signal, so the
  ST7701 path must use HV-sync/no-DE behavior.
- U-Boot source now uses the ST7701 SPI init path with 720MHz CPU / DDR216 and
  384x640 LCD timing:
  CONFIG_VIDEO_LCD_MODE="x:384,y:640,depth:16,pclk_khz:24000,le:78,ri:60,up:9,lo:16,hs:6,vs:4,sync:3,vmode:0"
- U-Boot ST7701 init print now identifies:
  "ST7701: init 384x640 RGB565 no-DE/HV-sync panel"
- Built artifact:
  /home/wnk/LicheePi_Nano/u-boot/u-boot-sunxi-with-spl.st7701_384x640_node_hvsync_oc720_ddr216_20260731.bin
  md5 01f63b8d88b7c02b6408de5ffcd67292
- Kernel panel-simple lattland,mostima modes now mark the 384x640 mode as
  DRM_MODE_TYPE_PREFERRED instead of the old 480x320 mode.
- Rebuilt kernel:
  /home/wnk/LicheePi_Nano/linux/arch/arm/boot/zImage
  md5 386e54f812fb2ffe6f7cbbfa72e13a0d
  /home/wnk/LicheePi_Nano/linux/arch/arm/boot/dts/suniv-f1c100s-licheepi-nano.dtb
  md5 4ffa57f10b94b9f836e76e1021f67510
- Firmware generation script now points at the 384x640 no-DE/HV-sync U-Boot:
  /home/wnk/LicheePi_Nano/make_firmware_st7701_oc720.sh
- Board deployment was not completed by Codex in this turn because SSH to
  10.0.0.107 timed out from the Windows host.

Dropbear SSH boot-race fix - 2026-07-31:
- Symptom:
  When external clients repeatedly attempted SSH during boot, TCP port 22 could
  be open but banner exchange stalled or login became unreliable.
- Finding on board:
  /etc/init.d/S50dropbear always appended `-R`.
  Dropbear `-R` creates host keys lazily as required. On this small board, early
  boot client retries can collide with lazy key generation and cause banner
  exchange stalls.
- Board fix applied:
  Generated persistent keys:
    /etc/dropbear/dropbear_ecdsa_host_key
      md5 5aa064d54d396d80254791b63ee45c41
    /etc/dropbear/dropbear_rsa_host_key
      md5 68ea0618c0632b96fd238cc997e0a677
  Patched:
    /etc/init.d/S50dropbear
      md5 de76078dff4aeeac1ecf6189fa033c59
  New behavior:
    If any persistent host key exists, start dropbear without `-R`.
    Fall back to `-R` only when no host keys exist.
- Current running dropbear process may still show `-R` until dropbear is
  restarted or the board reboots, because it was launched before the script was
  patched. Do not restart it while the user is relying on the active SSH
  session unless requested.
- Host/rootfs persistence:
  Synced S50dropbear and the generated host keys into:
    /home/wnk/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/etc/init.d/S50dropbear
    /home/wnk/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/etc/dropbear/
  Updated install_payload_to_target.sh to enforce:
    /etc/dropbear mode 0700
    /etc/dropbear/dropbear_*_host_key mode 0600
  Regenerated:
    /home/wnk/LicheePi_Nano/buildroot-2018.02.11/output/images/rootfs.tar
    md5 8288f3d543f363e3edee47a627d63812
  Verified rootfs.tar contains:
    ./etc/init.d/S50dropbear mode 0755
    ./etc/dropbear/dropbear_ecdsa_host_key mode 0600
    ./etc/dropbear/dropbear_rsa_host_key mode 0600

RTL8723BU SSH recovery / btusb unbind fix - 2026-07-31:
- Symptom:
  SSH port could appear open, but SSH banner/login failed. Serial showed the
  real network state:
    wlan0 had stale IP/route or later no IP, wpa_supplicant was SCANNING or
    DISCONNECTED, iwconfig showed unassociated, and TX packets stayed at 0.
- Root cause found through COM3:
  RTL8723BU is a USB composite device at 0bda:b720.
  Interfaces:
    1-1.1:1.0 class e0 -> btusb
    1-1.1:1.1 class e0 -> btusb
    1-1.1:1.2 class ff -> rtl8723bu
  The btusb/HCI side timed out and the 8723bu WiFi side repeatedly hit
  `SurpriseRemoved`, after which `ifconfig wlan0 up` could fail with
  `SIOCSIFFLAGS: Operation not permitted` or scanning returned no results.
- Manual recovery that worked:
  1. kill wpa_supplicant/udhcpc
  2. bring wlan0 down and rmmod 8723bu
  3. unbind the btusb interfaces for 0bda:b720
  4. reset only USB device 1-1.1 through sysfs authorized 0/1
  5. reload 8723bu, bring wlan0 up, start wpa_supplicant, run udhcpc
  Result:
    wpa_state=COMPLETED
    ssid=wnk641_2.4G
    IP=10.0.0.161
    ping 10.0.0.1 succeeded
    SSH over 10.0.0.161 worked repeatedly.
- Persistent board fix:
  /etc/init.d/S17rtl8723bu
    md5 1de2bd46c0edd79d78e4f0774663bbac
  now runs disable_8723_bt() after detecting 0bda:b720 and before loading
  8723bu. It unbinds all class e0 interfaces of the RTL device from btusb,
  leaving only the WiFi interface for rtl8723bu.
- Host/rootfs overlay synced:
  /home/wnk/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/etc/init.d/S17rtl8723bu
    md5 1de2bd46c0edd79d78e4f0774663bbac
- Note:
  The board may get 10.0.0.161 from DHCP instead of the earlier 10.0.0.107.
  Check wpa_cli/ifconfig over serial before assuming dropbear itself is broken.

408MHz safe-clock SD-card comparison prepared:
- Date: 2026-07-30
- User inserted the same new system card back into host as /dev/sdb after
  720MHz tests showed USB high-speed/RTL8723BU problems.
- Backed up current 720MHz card state:
  /home/wnk/F1C200S_host_archive/oc_sdcard_backups/20260730_sdb_720_before_safe408/sdb_first1M_720_before_safe408.bin
  md5: 9e59db3bd5cd78df14609012e2cfba59
- Flashed known-good safe U-Boot/SPL:
  /home/wnk/LicheePi_Nano/u-boot/u-boot-sunxi-with-spl.ili9488_node_b6_nl3b_gpio_low_safeclk.bin
  md5: b10ff151bd13dc7580d056b01b62ca46
- SD readback md5:
  b10ff151bd13dc7580d056b01b62ca46
- Purpose:
  Boot this card at safe clock and check whether USB enumerates as high-speed
  and RTL8723BU networking works. This isolates whether the 720MHz clock setup
  caused the USB issue.

System card reinstalled for clean USB WiFi + OC testing:
- Date: 2026-07-30
- User determined the previous environment was broken and formatted the card.
- Host device during reinstall:
  /dev/sdb
- New partition layout:
  /dev/sdb1 BOOT FAT32, 32 MiB
  /dev/sdb2 rootfs ext4, 768 MiB
  /dev/sdb3 ROMS FAT32, rest of card
- Root filesystem:
  Extracted from:
    /home/wnk/LicheePi_Nano/buildroot-2018.02.11/output/images/rootfs.tar
  Then installed runtime bundle payload and overlay from:
    /home/wnk/LicheePi_Nano/board_tools_f1c200s/runtime_bundle
- USB WiFi environment installed:
  /lib/modules/5.7.1/extra/8723bu.ko
    md5 2360318f8529147f727798ebb4b7de29
  /lib/firmware/rtl_bt/rtl8723b_fw.bin
  /lib/firmware/rtl_bt/rtl8723b_config.bin
    md5 a305752bf420bd3d6936be3b9fe0446a
  /etc/init.d/S16usb-host
  /etc/init.d/S17rtl8723bu
    md5 2a5f80565e6729aedcb2ad5d24fda045
  /etc/init.d/S45usb-wifi
    md5 4166c64b434984f4a022521bf9650dbb
  /etc/default/usb-wifi
  /etc/wpa_supplicant.conf
- Boot files:
  /mnt/f1c_reinstall_boot/zImage
    md5 7d369a51864c4b4dd79602143bd94aec
  /mnt/f1c_reinstall_boot/suniv-f1c100s-licheepi-nano.dtb
    md5 4ffa57f10b94b9f836e76e1021f67510
- Initial flashed U-Boot/SPL:
  safe 408MHz/DDR156
    md5 b10ff151bd13dc7580d056b01b62ca46
  SD readback md5:
    b10ff151bd13dc7580d056b01b62ca46
- OC tools installed on rootfs:
  /root/oc_test/oc_status.sh
  /root/oc_test/flash_oc720.sh
  /root/oc_test/restore_safe408.sh
  /root/oc_test/uboot_safe408_ddr156.bin
  /root/oc_test/uboot_oc720_ddr156.bin
    md5 a07b93a2d49c21be653ff3d857936b3b
  /root/oc_test/zImage_landscape_480x320_h14_2_2_v30_2_2
- Card was synced and unmounted after reinstall.
- Next test order:
  1. Boot safe 408 first and verify USB high-speed + RTL8723BU DHCP/SSH.
  2. If clean, run /root/oc_test/flash_oc720.sh and reboot for 720MHz test.
  3. Use /root/oc_test/restore_safe408.sh if 720MHz needs rollback.
```

## Kernel Panel Mode Switch Test Branch - 2026-07-30
```text
Purpose:
- Test Linux/DRM/TCON screen parameter switching only.
- Do not add or depend on SPI panel command changes in this test.

Host repo:
- /home/wnk/LicheePi_Nano

Branch:
- kernel-panel-mode-switch-3modes-20260730

Commit:
- 465a9ab Expose three LCD modes for panel switch test

Changed source:
- linux/drivers/gpu/drm/panel/panel-simple.c

Important git note:
- This monorepo did not previously track this panel-simple.c path, so the test
  branch commit adds the full file. This is intentional for recoverability of
  the test branch and does not change master unless explicitly merged.

Exposed modes for `lattland,mostima`:
- 480x320 preferred:
  clock 12000 kHz
  hfp 8, hsync 3, hbp 8
  vfp 20, vsync 10, vbp 40
- 320x480:
  clock 12000 kHz
  hfp 20, hsync 10, hbp 40
  vfp 8, vsync 2, vbp 8
- 384x640 legacy:
  clock 24000 kHz
  hfp 60, hsync 6, hbp 78
  vfp 16, vsync 4, vbp 9

Build verification:
- Command:
  cd /home/wnk/LicheePi_Nano/linux
  export PATH=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
  /usr/bin/make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- -j4 zImage dtbs
- Result: build passed; panel-simple.o rebuilt.

Build artifact md5:
- zImage:
  06077978aeb4683e71a0da5ff53e5ac6
- suniv-f1c100s-licheepi-nano.dtb:
  674672527f5994fedaf3b965b090db67
- panel-simple.o:
  00e0ba048123d5f7d2011aa884218b8a
```

## AIC8800 Miracast Cedar Hard-Decode Live Display Progress - 2026-07-28
```text
Status:
- Board-side Miracast + Cedar hard decode reached a useful proof-of-life state.
- User confirmed: after connection, the screen had stable changing picture.
- This is not yet the final low-latency live pipeline. The currently proven
  working path is:
  1. Miracast RTSP/RTP receiver writes raw H264 to a normal file.
  2. cedar_drm_player reads that growing H264 file with raw-H264 mode and
     displays through VE + DRM.

Current board state captured during the working session:
- wpa/P2P GO:
  /root/aic_miracast/wpa24_aic_wfd_supplicant -B -Dnl80211 -i wlan1 \
      -c /tmp/aic_wpa.conf -f /root/aic_runtime/logs/aic_miracast_board.log
- DHCP:
  /root/aic_miracast/tiny_dhcpd_49 wlan1
- temporary watcher:
  /root/aic_runtime/run/watch_live_cedar.sh
- H264 receiver:
  /root/aic_miracast/miracast_sink_dump 192.168.49.52 \
      /root/aic_runtime/run/live_in.h264
- hard decode display:
  /root/cedar_drm_player --raw-h264 640 360 30 \
      /root/aic_runtime/run/live_in.h264

Important MD5s from the live board:
- /root/aic_miracast/start_aic_miracast_cdump_board.sh
  073055c5668ae79c66fa59b7df26e64f
- /root/aic_miracast/miracast_sink_dump
  4a9a6f9ccd35c2fe4cb859b9467315e4
- /root/cedar_drm_player
  49083ed88231ad8829f5cde175d6e94a
- /root/aic_runtime/run/watch_live_cedar.sh
  5301121d85d2c4a64beeb8f2b9414e68
- /root/aic_runtime/run/h264_late_cedar_relay
  12acdc9842db382bf86465b8d11798ad
- /root/aic_runtime/run/h264_relay_nonblock
  8c8d8c91a697c70c7748905e19688c1c

Confirmed WFD/RTSP format in this baseline:
- WFD string:
  wfd_video_formats: 00 00 02 04 00000000 00000000 00000040 00 0000 0000 00 none none
- RTSP reached SETUP/PLAY and requested initial IDR:
  ---- Negotiation successful ----
  request initial IDR after 12 RTP packets
- H264 stream starts with AUD/SPS/PPS/IDR. First bytes observed in
  /root/aic_runtime/run/live_in.h264:
  00 00 00 01 09 10 00 00 00 01 67 64 00 28 ...
- Cedar logs prove actual decode/display:
  picture frame=... size=640x384 ... expect=640x384
  DRM commit ok ...
  Later logs reached at least picture frame=1800 and DRM commit count=1800.

What worked:
- After normal Miracast connection, running cedar directly on the growing file:
  /root/cedar_drm_player --raw-h264 640 360 30 \
      /root/aic_runtime/run/live_in.h264
  produced continuous decoded frames and user-visible moving picture.
- This proves:
  * AIC8800 Miracast transport can deliver H264.
  * miracast_sink_dump's file output is valid Annex-B H264 for cedar.
  * cedar_drm_player raw-H264 path can decode this stream.
  * VE + DRM display path is usable for live Miracast display.

What failed / do not repeat blindly:
- Killing an already-running /dev/null cdump and starting a new cdump against
  the same phone IP breaks the RTSP session. The replacement cdump observed:
  connect 192.168.49.52:7236 failed: No route to host
  Conclusion: do not switch output mid-session. Start the desired receiver path
  at DHCP/RTSP session creation.
- h264_late_cedar_relay FIFO pipeline showed first frames, then stalled.
  During that test all three processes were alive:
  h264_late_cedar_relay, miracast_sink_dump, cedar_drm_player
  but wlan1 rx_bytes stopped increasing. This is consistent with FIFO/player
  backpressure propagating to cdump and then to the phone.
- h264_relay_nonblock as currently present on the board is not trusted as a
  working final relay. In the tested watcher path, live_in.h264 became a normal
  file and the relay process exited; only cdump continued writing the file.
  Therefore black screen in that test was not a wireless issue; cedar was not
  being fed by the relay.

Temporary watcher notes:
- /root/aic_runtime/run/watch_live_cedar.sh was created as a temporary hard
  decode experiment watcher.
- It responds to P2P-PROV-DISC-PBC-REQ with one wps_pbc, waits for DHCP, then
  tries to start relay + cedar + cdump.
- It is not yet a clean known-good production watcher because the relay path is
  not correct. Preserve it only as experiment evidence unless fixed.

Correct next implementation direction:
- Do not start from scratch.
- Keep the stable PBC-on-request Miracast startup baseline.
- For the next practical hard-decode version, use the proven growing-file path:
  1. On DHCP, remove old /root/aic_runtime/run/live_in.h264.
  2. Start:
     /root/aic_miracast/miracast_sink_dump <phone_ip> \
         /root/aic_runtime/run/live_in.h264
  3. Wait until the file exists and has a small amount of data or until SPS/PPS
     appears.
  4. Start:
     /root/cedar_drm_player --raw-h264 640 360 30 \
         /root/aic_runtime/run/live_in.h264
  5. Add cleanup on disconnect and avoid writing to /root/roms.
- This will have latency and file growth, but it is the confirmed route to a
  stable moving picture.
- Later optimization should replace the normal file with a proper bounded
  ring/drop-old-H264 relay that preserves SPS/PPS/IDR and never blocks cdump.
  Do not trust the current h264_relay_nonblock binary without source audit and
  a controlled test.

Connection/debug rule from this session:
- If phone reaches WPS and DHCP but disconnects immediately, first verify that
  watcher is alive. A previous failure was caused by no active watcher after
  DHCP, so miracast_sink_dump never started.
- Do not use p2p_find, all_sta polling, p2p_group_remove, or periodic WPS rearm
  while AIC GO is live.
```

## AIC8800 Miracast Startup Must Not Regress - 2026-07-28
```text
This section is the required startup rule for AIC8800D80 Miracast on the board.
Read this before touching wlan1, AIC modules, WFD formats, or Miracast startup.

Do not start Miracast from a cold AIC state in one SSH command.
Do not run the cold 8d80 firmware loader over SSH.
Do not use v14 aic_load_fw.ko for the cold 8d80 -> 8d83 transition.
Do not run p2p_group_remove to "refresh" a live AIC GO; it can crash the AIC cmd queue.
Do not mix 800x600 WFD/sink experiments into the recovery baseline.

Known-good baseline files for getting the device visible again:
- /root/aic_miracast/start_aic_miracast_cdump_board.sh
  md5 073055c5668ae79c66fa59b7df26e64f
- /root/aic_miracast/miracast_sink_dump
  md5 4a9a6f9ccd35c2fe4cb859b9467315e4
  This is the 360p30 HH-low sink, not the 800x600 test sink.
- cold loader:
  /lib/modules/5.7.1/extra/aic_load_fw.ko
  md5 81528a07455d757581c5414de2dc3ead
- after 8d83, use v14 modules:
  /root/aic_miracast/candidates/aic8800_v14_360p30_stable/aic_load_fw.ko
  md5 a762b1674cc4a55b727256fa542ac16f
  /root/aic_miracast/candidates/aic8800_v14_360p30_stable/aic8800_fdrv.ko
  md5 8259aa2ba4008452d75f26021e03969a

Confirmed correct sequence:

1. After board reboot, verify cold AIC state from SSH or serial:
   lsusb
   Expected:
   a69c:8d80
   No aic modules loaded:
   cat /proc/modules | grep aic

2. From SERIAL CONSOLE, not SSH, run only the cold firmware loader:
   dmesg -n 1
   echo "1 1 1 1" > /proc/sys/kernel/printk
   /sbin/insmod /lib/modules/5.7.1/extra/aic_load_fw.ko \
       aic_fw_path=/lib/firmware/aic8800D80 aicwf_dbg_level=0
   sleep 8
   lsusb

   Success condition:
   lsusb shows a69c:8d83.

   Explanation:
   Running this step over SSH is unsafe because wlan0 is on the same external
   USB hub. During AIC firmware upload/re-enumeration the MUSB bus can stall,
   causing SSH banner timeout and making the operator misdiagnose the state.
   Serial-local loader was confirmed to succeed on 2026-07-28.

3. After 8d83 appears, SSH is acceptable again. Initialize wlan1 only:
   dmesg -n 1
   echo "1 1 1 1" > /proc/sys/kernel/printk
   /sbin/rmmod aic8800_fdrv 2>/dev/null || true
   /sbin/rmmod aic_load_fw 2>/dev/null || true
   sleep 1
   /sbin/insmod /root/aic_miracast/candidates/aic8800_v14_360p30_stable/aic_load_fw.ko \
       aic_fw_path=/lib/firmware/aic8800D80 aicwf_dbg_level=0
   /sbin/insmod /root/aic_miracast/candidates/aic8800_v14_360p30_stable/aic8800_fdrv.ko \
       aicwf_dbg_level=0
   sleep 3
   /sbin/ifconfig -a | sed -n '/wlan1/,+8p'

   Success condition:
   wlan1 appears with MAC DC:2E:97:94:EB:68.

4. Only after wlan1 exists, start P2P GO/Miracast. Do not touch loader/fdrv:
   ARCHIVE_DIR=/root/aic_runtime \
   CAPTURE_MODE=null \
   AIC_DBG_LEVEL=0 \
   LOWMEM_TUNE=1 \
   STOP_GMENU=1 \
   LOAD_FW=/root/aic_miracast/candidates/aic8800_v14_360p30_stable/aic_load_fw.ko \
   FDRV=/root/aic_miracast/candidates/aic8800_v14_360p30_stable/aic8800_fdrv.ko \
   /root/aic_miracast/start_aic_miracast_cdump_board.sh start

   Expected output:
   P2P-GROUP-STARTED wlan1 GO ssid="DIRECT-xx" freq=5805
   ready: display=F1C200S-AIC iface=wlan1 freq=5805 passphrase=12345678

Known failure signatures:
- app_cmp followed by USB disconnect and then:
  device descriptor read/64, error -110
  device not accepting address
  Means firmware upload completed, but AIC failed to re-enumerate. wlan1 cannot
  appear until AIC is visible as 8d83.
- Failed to set beacon parameters
  Means wlan1/fdrv or GO state is dirty; do not p2p_group_remove/re-add in a
  loop. Stop and recover from a clean AIC state.
- cmd queue crashed
  Means AIC command path is broken. Do not continue WFD/Miracast operations.

If the phone cannot see the device, first check:
  lsusb
  cat /proc/modules | grep aic
  /sbin/ifconfig -a | sed -n '/wlan1/,+8p'
  /root/aic_miracast/wpa24_aic_wfd_cli -p /tmp/aic_wpa_ctrl -i wlan1 status
Do not change WFD format until these basics are correct.
```

## Host Source Backup Rule - 2026-07-20
```text
For emergency/source backups of /home/wnk, do not archive build products.
Default to a sources-only raw tar, not xz/gzip:

- Use .tar with `tar -cpf`, not `.tar.xz` and not `.tar.gz`, unless the user
  explicitly asks to save disk space.
- Exclude Buildroot output and downloads:
  /home/wnk/LicheePi_Nano/buildroot-2018.02.11/output
  /home/wnk/LicheePi_Nano/buildroot-2018.02.11/dl
- Exclude kernel/build artifacts:
  *.o, *.ko, *.cmd, *.mod, *.mod.c, .tmp_versions,
  linux/arch/arm/boot/Image, zImage, uImage, dts/*.dtb
- Exclude caches/trash and previous backup archives:
  /home/wnk/.cache, /home/wnk/.dbus, /home/wnk/.gvfs,
  /home/wnk/.local/share/Trash, /home/wnk/wnk_*BACKUP_*.tar*

Confirmed better command shape:
  tar --ignore-failed-read [excludes above] -cpf /home/wnk/wnk_SOURCE_BACKUP_<timestamp>.tar /home/wnk

Reason:
- xz was far too slow and pinned CPU.
- gzip still wasted CPU for this job.
- Including compiled Buildroot/kernel output produced multi-GB archives full
  of rebuildable files.
- The correct backup target is source, scripts, configs, docs, and local work.
```

## Verified Asset Organization Rule - 2026-07-20
```text
After a driver, script, application, launcher, runtime hook, or host helper is
confirmed working, move or copy the source-of-truth into the canonical archive
layout immediately. Do not leave verified assets scattered in temporary host
directories, Windows Desktop scratch folders, or ad-hoc board paths.

Canonical locations:
- Board/runtime scripts and deployable rootfs files:
  ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/
- Runtime bundle collection/deploy logic:
  ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/
- Standalone verified apps, drivers, host tools, wireless experiments, notes,
  and source snapshots outside linux/u-boot/buildroot:
  /home/wnk/F1C200S_host_archive/github_exports/github_exports_f1c200s_extras_*/f1c200s-extras/
  and GitHub repo:
  git@github.com:Wnjbk/f1c200s-extras.git

Required cleanup/status rule:
- If an experiment is abandoned, broken, superseded, or only kept as diagnostic
  evidence, mark it explicitly before moving on.
- Put a short README or STATUS file in that directory with:
  status: working / abandoned / broken / diagnostic / superseded
  date
  reason
  last known result
  replacement path, if any
- Do not mix abandoned/broken trees into the same "known good" location without
  that status marker.
- Before finalizing a feature, update this project record and push or prepare
  the extras repo update so future backup/export does not require forensic
  searching through scattered directories.
- At the end of every debug task, do a cleanup pass before moving to the next
  task:
  1. Move verified scripts/apps/drivers into the canonical runtime bundle or
     extras repo.
  2. Move still-useful temporary host scripts into an archive folder, not the
     Desktop or `/home/wnk` top level.
  3. Delete useless speed-test blobs, empty error directories, one-off logs, and
     failed scratch files.
  4. Mark broken/abandoned experiments with README/STATUS before archiving.
  5. Keep `/home/wnk` top level and the Windows Desktop clean: no scattered
     `_tmp*`, `_audit*`, `_augment*`, `_export*`, `_backup*`, `_aic*`, or
     `Codex_Temp` leftovers after the task is done.
- Windows Desktop cleanup folder created on 2026-07-20:
  `C:\Users\26301\Desktop\F1C200S_归档_脚本和资料`
  with `host_scripts/`, `work_dirs/`, and `notes/`.
  Keep `F1C200S_build_commands.md` and `AGENTS.md` on the Desktop as canonical
  context files; move other completed F1C200S scratch material into the archive
  folder or the host extras repo.

Reason:
- The 2026-07-20 GitHub archival pass was slow because useful emulator,
  driver, wireless, script, and host-helper work was scattered across
  third_party/, board_tools, backups/, home-level scripts, and temporary export
  directories.
- Going forward, verification must be followed by organization, status marking,
  and archiving while the context is still fresh.
```

## GitHub Source Export - 2026-07-20
```text
Host source-only backup completed:
- /home/wnk/F1C200S_host_archive/backups/wnk_SOURCE_BACKUP_20260720_022741.tar
- Size: 5.3G
- sha256:
  40b980661fd3c770e8f623688cbd9686e0649a8a0437f0a2532f47b8b4d86dc6
- Windows copy:
  E:\UBUNTU备份\wnk_SOURCE_BACKUP_20260720_022741.tar
  E:\UBUNTU备份\wnk_SOURCE_BACKUP_20260720_022741.tar.sha256

Host cleanup/archive root:
- /home/wnk/F1C200S_host_archive
  - host_scripts/: one-off backup/export/audit/push helper scripts
  - logs/: backup/export/audit logs
  - github_exports/: clean exported git worktrees
  - backups/: source-only backup tar files
  - board_verified_snapshot_20260720/: live board reference snapshot
  - diagnostics/configs/cedar_tests/work_dirs/: classified non-canonical archive

Board-based classification completed on 2026-07-20:
- Live board at 10.0.0.107 was used as the ground truth for useful runtime
  files.
- Snapshot:
  /home/wnk/F1C200S_host_archive/board_verified_snapshot_20260720
  - root_scripts/: current /root/*.sh from board
  - init.d/: current /etc/init.d/S* from board
  - modules_extra/: current /lib/modules/5.7.1/extra/*.ko from board
  - manifests/: runtime module/process/network state and file lists
- Classification:
  /home/wnk/F1C200S_host_archive/CLASSIFICATION_20260720.md
- Working runtime core confirmed from board:
  8723bu.ko, sunxi/phy_generic USB host, sunxi_ion_core.ko, cedar_ve.ko,
  dropbear, wpa_supplicant, run_gmenu2x.sh/gmenu2x.
- Useful board-deployed script groups:
  ADB wireless recording/mirror, Cedar DRM player, gmenu2x launch, emulator
  launchers, matrix/uinput helpers, ROM mounting/httpd, TVD/Cedar loaders,
  WiFi restore/scan/speed tools.
- Experimental/diagnostic:
  AIC8800, Miracast, and 8723 AP/P2P files are retained as references but are
  not canonical final runtime unless a future task re-confirms them.

Clean GitHub export base:
- /home/wnk/F1C200S_host_archive/github_exports/github_exports_f1c200s_20260720_031512

Exported independent git repos:
- linux
  commit: 12cc655 Initial F1C200S linux source export
  remote: git@github.com:Wnjbk/f1c200s-linux.git
  status: pushed; remote HEAD 12cc6553e1ad09eb25b8b4fba976dccc8602312e
- u-boot
  commit: 165810a Initial F1C200S u-boot source export
  remote: git@github.com:Wnjbk/f1c200s-u-boot.git
  status: pushed; remote HEAD 165810ad57077e8c0160486c100e0d104ed9be49
- buildroot-2018.02.11
  commit: f5ab6aa Initial F1C200S buildroot-2018.02.11 source export
  remote: git@github.com:Wnjbk/f1c200s-buildroot.git
  status: pushed; remote HEAD f5ab6aa19b273076d438c38b94b2dd15a99360ce
- f1c200s-extras
  path: /home/wnk/F1C200S_host_archive/github_exports/github_exports_f1c200s_extras_20260720_034741/f1c200s-extras
  commit: 6418d90 Initial F1C200S extras export
  remote: git@github.com:Wnjbk/f1c200s-extras.git
  status: pushed; remote HEAD 6418d900cc476bbdb77768158f5778df74c7e9b1
  size after export: includes apps, drivers, wireless experiments, notes, and deltas

Push script prepared:
- /home/wnk/F1C200S_host_archive/github_exports/github_exports_f1c200s_20260720_031512/push_all.sh

GitHub SSH works as user Wnjbk, but host has no gh/hub/token, so repo creation
cannot be done from SSH alone. Create these three empty repos on GitHub first,
then run the push script:
- f1c200s-linux
- f1c200s-u-boot
- f1c200s-buildroot

Note:
- u-boot export originally included a 103MB Linaro toolchain tarball and GitHub
  rejected the push. The export commit was amended to remove:
  gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi.tar.xz
- Extras repo contains useful material outside linux/u-boot/buildroot:
  board_tools_f1c200s, gmenu2x, gpsp, DinguxCommander, gnuboy,
  snes9x4d_rs90_f1c200s, ONScripter-EN, picodrive, miscraft experiments,
  Cedar DRM player, TVD tools, RTL8723BU board/host AP/P2P driver trees,
  AIC8800D80 host-good and WiFi-only loader trees, patched wpa_supplicant
  trees, miraclecast host experiment, notes, and exported_deltas.
- Emulator audit then added the missing emulator trees:
  pcsx_rearmed, ONScripter-GBK, arm-NES-linux-master,
  retrogo_snes_f1c200s, snes9x4d_miyoo, picodrive_probe,
  picodrive_tagtest, pocketsnes_f1c200s_snapshot,
  PocketSNES-FunKey_snapshot, PocketSNES-FunKey-clean_snapshot.
- Script coverage in extras remote HEAD 6418d90:
  board runtime root overlay includes 44 root/*.sh scripts, including
  ADB recording/wireless mirror, gmenu2x, emulator launchers, Cedar/TVD,
  Miracast probes, WiFi/AP/P2P, matrix bridge, Bluetooth, and test scripts.
  init.d includes S02loopback, S16usb-host, S17rtl8723bu, S18cedar,
  S19roms, S45usb-wifi, S99gmenu2x.
  drivers/wireless trees include 49 shell scripts.
- Extra host helper scripts are committed locally as second commit
  91ca118 Archive host helper scripts, but not yet pushed because GitHub SSH
  repeatedly closed the connection on 2026-07-20:
  host_tools/backup_miracle_sinkctl_host.sh,
  host_tools/backup_wpa_p2p_go_neg_host.sh,
  host_tools/inspect_miracle_host.sh.
```

## F1C200S Display Rotation Register Audit - 2026-06-24
```text
User provided register manual:
C:\Users\26301\Downloads\allwinner_f1c200s_user_manual_v1_2.pdf

Manual sections checked:
- 5.1 TCON, pages 180-195
- 5.2 Display Engine Front-End / DEFE, pages 196-211
- 5.3 Display Engine Back-End / DEBE, pages 212-232

Result:
- The F1C200S manual documents DEFE hardware scaling, CSC, and optional
  write-back, but no 90-degree rotate/transpose register or module.
- DEFE relevant registers match the current kernel frontend path:
  * base 0x01E00000
  * DEFE_BUF_ADDR0/1/2 at 0x20/0x24/0x28
  * DEFE_LINESTRD0/1/2 at 0x40/0x44/0x48
  * DEFE_INPUT_FMT at 0x4C
  * DEFE_OUTPUT_FMT at 0x5C
  * DEFE_INSIZE at 0x100
  * DEFE_OUTSIZE at 0x104
  * DEFE_HORZFACT at 0x108
  * DEFE_VERTFACT at 0x10C
  * coefficient RAM at 0x400/0x500/0x600/0x700
- These registers can scale horizontally and vertically independently, but
  they still scan memory linearly by address + line stride. The manual does
  not define negative stride, x/y swap, transpose, or rotation control.
- DEBE relevant registers match the current backend path:
  * base 0x01E60000
  * layer size, coordinate, line width, FB address, attribute/format, alpha,
    hardware cursor, writeback
  * no layer rotation/transpose control
- TCON relevant registers match current output path:
  * base 0x01C0C000
  * TCON0_CTRL bit 23 is only RED/BLUE swap
  * TCON0 supports HV and 8080 interface modes, timing, serial RGB/YUV output
    byte/order fields, IO polarity/tri-state
  * no framebuffer/image rotation control

Source audit:
- Current sun4i DRM driver does not create or consume a DRM plane rotation
  property for sun4i/F1C100S/F1C200S.
- `drivers/gpu/drm/sun4i/sun4i_tcon.c` only consumes the custom
  `srgn,swap-b-r` property to set `SUN4I_TCON0_CTL_SWAP`.
- The DTS `rotate = <90>;` currently present under the disabled SPI ST7789
  node is unrelated to the active RGB/DRM pipeline and is not consumed by the
  sun4i DEFE/DEBE/TCON path.
- Reference searches in `miyoo_src`, `f1c500s_kernel`, `FunKey-OS-master`,
  `rhodesepass_buildroot`, `retro-go_chaeng`, and `snes9x4d-rs90` did not
  reveal an F1C200S display/G2D hardware rotation path. Hits were emulator
  internal rotation, libretro comments, DirectDraw headers, or unrelated
  generic kernel drivers.

Practical conclusion:
- Treat F1C200S display hardware as supporting scaling/CSC/composition, not
  true 90-degree rotation.
- Best path for rotated output is hybrid:
  1. rotate at the smallest practical source size in userspace or a shared SDL
     framebuffer backend/presenter,
  2. then use DEFE/DRM plane scaling for final display size.
- For SFC specifically, rotate the 256x224/256x240 source buffer in the
  presenter if landscape orientation is required, then feed XRGB8888/YUV to
  the frontend for scaling. Avoid rotating a full 480x360/640x360 framebuffer
  unless there is no alternative.
```

## Miracast / P2P Archive - 2026-07-21
```text
Status:
- Miracast/P2P work is archived as high-progress diagnostic state, not final
  runtime.

Host archive:
- /home/wnk/F1C200S_host_archive/miracast_archive_20260721_152323.tar
- sha256:
  5b7f07aceb471b79574b0966cbc803ccb8b649e232fc6104e38a490f743fb41a
- final checksum file:
  /home/wnk/F1C200S_host_archive/miracast_archive_20260721_152323.tar.sha256

Windows docs:
- C:\Users\26301\Desktop\F1C200S_归档_脚本和资料\notes\Miracast_P2P_diagnosis_20260721.md
- C:\Users\26301\Desktop\F1C200S_归档_脚本和资料\notes\Miracast_ARCHIVE_MANIFEST_20260721.md
- C:\Users\26301\Desktop\F1C200S_归档_脚本和资料\notes\Casting_Next_Steps_20260721.md

Included host source trees:
- ~/LicheePi_Nano/third_party/miraclecast_host_20260720
- ~/LicheePi_Nano/third_party/wpa_supplicant_2_4_p2p_no_concurrent_20260720
- ~/LicheePi_Nano/third_party/wpa_supplicant_2_4_p2p_fake_peer_ch_20260721
- ~/LicheePi_Nano/third_party/wpa_supplicant_2_4_aic_wfd_20260720
- ~/LicheePi_Nano/third_party/rtl8723bu_lwfinger_host_p2p_concurrent_20260720
- ~/LicheePi_Nano/third_party/rtl8723bu_lwfinger_host_ap_20260719_181717

Important result:
- RTL8723BU can do P2P discovery and 2.4 GHz P2P-GO fallback.
- Phone WNK is discoverable as P2P device:
  6e:40:e8:1e:07:41
  pri_dev_type=10-0050F204-5
- Real Miracast GO negotiation fails when forced to 2.4 GHz:
  phone Confirm status=7
- Diagnostic fake-peer-channel patch proves the phone accepts GO negotiation
  when local sink advertises:
  regulatory class 124, channel 161, 5805 MHz
- The accepted diagnostic path reaches:
  P2P-GO-NEG-SUCCESS role=GO freq=5805
- Group formation then fails because RTL8723BU cannot actually operate on
  5 GHz channel 161.

Do not deploy:
- wpa_supplicant_2_4_p2p_fake_peer_ch_20260721 is diagnostic only.

Next direction:
- Keep RTL8723BU fallback for wireless ADB/custom streaming.
- For Android Miracast, use a real 5 GHz-capable adapter and retest the same
  WFD/P2P path.
- For PC source testing, evaluate lazycast/MS-MICE.
- Research practical alternatives: AirPlay, DLNA/UPnP, Google Cast, WebRTC,
  RTSP/RTP custom streaming, and adb/scrcpy-like wireless mirror.

Clarification added after follow-up:
- RTL8723BU fallback can create a P2P-GO group and the phone can join the
  generated DIRECT-* network. That proves P2P/IP transport, not full Miracast.
- Full Miracast still needs WFD discovery, accepted WFD IEs, GO negotiation,
  RTSP/WFD session setup, and H.264/RTP stream reception.
- Detailed next-step protocol record:
  C:\Users\26301\Desktop\F1C200S_归档_脚本和资料\notes\Casting_Next_Steps_20260721.md
```

## DLNA Probe - 2026-07-21
```text
Scope:
- Try a zero-install source-side casting path using DLNA/UPnP MediaRenderer.
- DLNA is for media-file casting, not full system screen mirroring.

Current host probe:
- Windows source:
  C:\Users\26301\Desktop\F1C200S_归档_脚本和资料\host_scripts\dlna_renderer_probe
- Host path:
  /home/wnk/F1C200S_host_archive/host_scripts/dlna_renderer_probe
- Script:
  f1c200s_dlna_renderer_probe.py
- Start:
  HOST_IP=192.168.16.37 NAME=F1C200S-DLNA ./start_dlna_probe_host.sh
- Stop:
  ./stop_dlna_probe_host.sh
- URL:
  http://192.168.16.37:8200/description.xml
- Log:
  /tmp/f1c200s_dlna_probe.log

Current result:
- Probe started successfully on the Ubuntu host.
- SSDP notify/reply loop is running.
- Multiple LAN clients fetched description and service XML:
  description.xml, ConnectionManager.xml, RenderingControl.xml, AVTransport.xml
- SOAP GetProtocolInfo requests were received and answered.
- After adding /dlna/Render/dmr_extra.xml and fuller SCPD action arguments,
  the phone player reported that it was casted to the device and continued
  playback.
- The phone sent SetAVTransportURI and Play.
- Example URL:
  http://192.168.16.29:8091/%2Fstorage%2Femulated%2F0%2FDCIM%2FCamera%2Fvideo_20260530_194015.mp4
- Host verified the URL is directly streamable over HTTP:
  HTTP/1.1 200 OK, Content-Type video/mp4, about 230 MB.
- Host GStreamer direct HTTP playback with fakesink ran for 10 seconds without
  errors, no local download needed.

Cedar HTTP stream work:
- Host backup before source edit:
  /home/wnk/backups/cedar_drm_player_20260721_0105_dlna_http_uri
- Local work dir:
  C:\Users\26301\Desktop\F1C200S_归档_脚本和资料\work_dirs\cedar_dlna_stream_20260721
- cedar_drm_player now passes http://, https://, and rtsp:// URLs directly to
  CedarX instead of forcing file://.
- URI/path buffers expanded from 256 to 2048 bytes.
- Rebuilt ARM player md5:
  6b9fd755d965ea0eae4c6e2fc112e046
- Runtime bundle payload updated:
  /home/wnk/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/payload/cedar_drm_player
- Runtime script updated to allow URL args:
  /home/wnk/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/root/run_cedar_drm_player.sh
- Board SSH at 10.0.0.107 timed out during this round, so board-side Cedar HTTP
  stream validation is pending.
- The current phone sample is H.265/HEVC inside MP4, not H.264. For F1C200S
  validation, test an H.264/AAC MP4 first.

Next validation:
- Source-side zero-install test:
  Windows media file "Cast to Device" / legacy Windows Media Player DLNA cast,
  or Android built-in Gallery/Video/Music DLNA cast if exposed by the phone ROM.
- Watch for SetAVTransportURI and Play in:
  /tmp/f1c200s_dlna_probe.log
- If control works, connect Play to a URL-capable Cedar player wrapper for the
  F1C200S board.
```

## Lazycast 2.4 GHz Miracast Retest - 2026-07-21
```text
Important correction:
- Lazycast can try Miracast on 2.4 GHz. Do not treat the previous RTL8723BU
  ch161 failure as proof that all Miracast paths require 5 GHz.
- Lazycast has two relevant paths:
  1. all.sh / normal P2P-GO Miracast, can be forced/tried on 2.4 GHz.
  2. mice.sh / MS-MICE for Windows 10 over Ethernet or secure WiFi, but README
     says discovery still needs Wi-Fi P2P beacon/probe response frames.

Host lazycast source:
- /home/wnk/LicheePi_Nano/third_party/lazycast_host_20260721
- git short commit:
  a089e1d

Host test wrapper:
- Windows:
  C:\Users\26301\Desktop\F1C200S_归档_脚本和资料\host_scripts\lazycast_host_2g_test
- Host:
  /home/wnk/F1C200S_host_archive/host_scripts/lazycast_host_2g_test

Current host test status:
- Started on RTL8723BU interface:
  wlx001f058056fd
- Interface state:
  type P2P-GO
- Display name:
  F1C200S-LAZYCAST
- PIN:
  31415926
- Frequency:
  2437 MHz / channel 6
- P2P-GO IP:
  192.168.173.1
- Expected source IP:
  192.168.173.80
- Logs:
  /tmp/lazycast_2g/main.log
  /tmp/lazycast_2g/wpa_supplicant.log
  /tmp/lazycast_2g/dnsmasq.log

Original-flow retest script added:
- Windows:
  C:\Users\26301\Desktop\F1C200S_归档_脚本和资料\host_scripts\lazycast_host_2g_test\start_lazycast_2g_original_flow_host.sh
- Host:
  /home/wnk/F1C200S_host_archive/host_scripts/lazycast_host_2g_test/start_lazycast_2g_original_flow_host.sh
- Purpose:
  follow upstream lazycast all.sh more closely by preferring p2p-dev-* as the
  control interface and p2p-wl* as the group interface. If RTL8723BU only
  supports the single-interface fallback, the script logs:
  flow=single-interface-fallback.
- Current blocker before retesting:
  On 2026-07-21 the host RTL8723BU again entered the known bad init state:
  `ip link set wlx001f058056fd up` returned `RTNETLINK answers: Operation not
  permitted`, and `/proc/net/rtl8723bu/wlx001f058056fd/mac_reg_dump` read all
  zeros. Module reload and EHCI controller soft rebind re-enumerated the USB
  device but did not recover hardware init. Physical replug/power-cycle is
  required before Lazycast discovery can be tested again.

Notes:
- The original lazycast d2.py uses Raspberry Pi OMX player by default. For
  host testing, d2_host_gst_20260721.py sets player_select=0 and enables the
  GStreamer playbin path instead of VLC.
- Stop with:
  /home/wnk/F1C200S_host_archive/host_scripts/lazycast_host_2g_test/stop_lazycast_2g_host.sh
```

## AIC8800 Lazycast Single-GO Miracast Success - 2026-07-22
```text
Status:
- Host-side AIC8800D80 Miracast/lazycast protocol path confirmed working.
- User confirmed the phone entered mirror mode.

Host:
- Ubuntu LAN IP: 192.168.175.138
- AIC bootrom: a69c:8d80
- AIC stage2: a69c:8d81
- Interface: aic0
- GO IP: 192.168.49.1/24
- Channel: 161 / 5805 MHz

Working path:
- Use the host-good AIC loader/fdrv from:
  /home/wnk/LicheePi_Nano/third_party/aic8800d80_repo_host_20260714/src/USB/driver_fw/drivers/aic8800
- Use patched wpa_supplicant from:
  /home/wnk/LicheePi_Nano/third_party/wpa_supplicant_2_4_aic_wfd_20260720
- Create autonomous single-interface P2P-GO on aic0.
- Do not use separate p2p-aic0-* group interface.
- Advertise:
  device_name=F1C200S-AIC
  device_type=7-0050F204-1
  WFD subelem 0 = 000600111c44012c
  WFD subelem 1 = 0006000000000000
  WFD subelem 6 = 000700000000000000
- Run DHCP on 192.168.49.1/24.
- After phone DHCP/ARP appears, run lazycast:
  python3 ./d2.py <phone-ip>

Critical correction:
- Lazycast RTSP direction is sink-to-source:
  Linux sink connects to phone/source TCP 7236.
- Waiting for the phone to connect to local TCP 7236 was wrong for this path.

Confirmed evidence:
- Phone DHCP lease:
  6e:40:e8:1e:07:41 192.168.49.48 iQOO-Neo10
- Lazycast watcher ran:
  d2.py 192.168.49.48
- TCP established:
  192.168.49.1:<ephemeral> -> 192.168.49.48:7236
- Host d2.py logged `vlc: not found`, so host playback is not validated yet.
  Protocol/mirror-mode entry is validated.

Reusable script:
- Windows:
  C:\Users\26301\Desktop\F1C200S_归档_脚本和资料\host_scripts\aic8800_lazycast_host\start_aic8800_lazycast_single_go.sh
- Host:
  /home/wnk/F1C200S_host_archive/host_scripts/aic8800_lazycast_host/start_aic8800_lazycast_single_go.sh

Full documentation:
- Windows:
  C:\Users\26301\Desktop\F1C200S_归档_脚本和资料\notes\AIC8800_Miracast_Lazycast_full_chain_20260722.md
- Host:
  /home/wnk/F1C200S_host_archive/notes/AIC8800_Miracast_Lazycast_full_chain_20260722.md

Archived evidence logs:
- /home/wnk/F1C200S_host_archive/logs/aic8800_lazycast_20260722/

GitHub archive:
- Repo:
  https://github.com/Wnjbk/f1c200s-casting
- Remote:
  https://github.com/Wnjbk/f1c200s-casting.git
- Branch:
  host-aic8800-miracast
- Branch commit:
  5bbe31f Archive casting source chain
- Main branch:
  e6d61f5 Keep host casting work off main
- Windows local export:
  C:\Users\26301\Desktop\F1C200S_归档_脚本和资料\github_exports\f1c200s-casting
- Host local export:
  /home/wnk/F1C200S_host_archive/github_exports/f1c200s-casting
- Contents:
  drivers/aic8800d80_host_good/
  software/wpa_supplicant_2_4_aic_wfd/
  software/lazycast_host_20260721/
  scripts/aic8800_lazycast_host/start_aic8800_lazycast_single_go.sh
  docs/ protocol notes and historical diagnosis.
- Purpose:
  This is a source/script/software archive for the working host-side casting
  chain, not a log archive. Because this chain is validated on the Ubuntu host
  rather than on the F1C200S board, it must stay on `host-aic8800-miracast`,
  not `main`.
- Runtime/debug artifacts intentionally not committed:
  logs, pcap captures, .ko/.o/.cmd/.d build products, and temporary tar files.
  Keep runtime logs only under the local host archive when needed.
- Push note:
  Host SSH authentication to GitHub works, but git transport repeatedly closed
  during this push. The successful push was done from Windows using HTTPS/Git
  Credential Manager.

Do not run original lazycast all.sh directly on AIC:
- It tries to create p2p-aic0-* group interfaces.
- AIC driver fails this path and can enter cmd queue crashed / Broken pipe.
```

## Fixed Requirements
```text
1. Prefer host-side Linux for all real F1C200S work and builds.
2. Host Linux is the place to modify kernel, buildroot, gmenu2x, gpsp, DinguxCommander, PocketSNES, and ONScripter.
3. If the board is not nearby, continue preparing host-side source, scripts, configs, and build artifacts.
4. Before kernel builds, confirm the real Buildroot toolchain prefix on the host instead of assuming `arm-linux-gnueabi-`.
5. All SDL app rootfs integration must be funneled through:
   `runtime_bundle/sync_overlay_to_buildroot.sh`
   `runtime_bundle/collect_runtime_payload.sh`
   `runtime_bundle/install_payload_to_target.sh`
6. When adding a new SDL app, update those three scripts and `runtime_bundle/rootfs_overlay/` instead of creating another temporary install path.
7. All SDL apps launched from gmenu2x must share the same baseline runtime style as gmenu2x:
   same framebuffer setup, same SDL video/audio environment, same keyboard/input assumptions, same launcher script pattern.
8. `uinput_tty_keyd` has been removed from the desired final app set. Do not restore it unless explicitly requested.
9. For script/config/DTS changes, edit and save them in the local Windows workspace first, then sync to the host Linux tree. This is required both to survive crashes/compact and to keep a local backup.
   - Preferred workflow: `scp` the host file to a local working folder, copy a `.before_*` local backup, edit the local file with `apply_patch`, then `scp` it back to the host.
   - Avoid complex inline SSH here-doc or nested-quote file edits from PowerShell; they repeatedly break quoting and waste time after compaction.
   - Simple inspection/build commands may still run directly over SSH.
10. Before changing critical host-side source trees such as kernel, Buildroot package sources, Cedar/OpenMAX, gmenu2x, gpsp, PocketSNES, or ONScripter, create a timestamped backup on the host and write a timestamped `BACKUP_LOG.txt` in that backup directory describing why the backup was made, which files were copied, what was changed afterward, and the build result.
11. Matrix keypad currently serves two roles:
   - keyboard semantics for gmenu2x and general shell/UI use
   - virtual uinput gamepad for selected SDL emulators
12. Do not replace the matrix keypad kernel driver with a joystick driver unless explicitly requested; the current design keeps keyboard semantics in-kernel and adds a user-space uinput bridge for emulator compatibility.
13. The current virtual gamepad plan is:
   - binary: `/root/matrix_pad_bridge`
   - helpers: `/root/start_matrix_pad_bridge.sh`, `/root/stop_matrix_pad_bridge.sh`
   - default behavior: grab `matrix-keypad` input and expose a virtual joystick through `uinput`/`joydev`
   - button layout: `0=A 1=B 2=X 3=Y 4=L1 5=R1 6=L2 7=R2 8=Select 9=Start`
14. Current integration status:
   - `run_gpsp.sh` uses the virtual gamepad bridge
   - `run_pocketsnes.sh` uses the virtual gamepad bridge
   - `run_pcsx.sh` currently stays on keyboard input until joystick mapping is verified
```

## Host
```text
Host IP: 192.168.175.135
User: wnk
Password: 1
```

Current host IP update - 2026-06-23:
```text
Use this host IP going forward:
192.168.175.137

Old host IP 192.168.175.135 is stale after network/IP change.
```

Current host IP update - 2026-07-16:
```text
Use this host IP going forward:
192.168.175.138

Old host IP 192.168.175.137 is stale after network/IP change.
```

Current host IP update - 2026-07-21:
```text
Use this host IP going forward:
192.168.16.37

Old host IP 192.168.175.138 is stale after host reboot/network change.
```

Host key directories:
```text
~/LicheePi_Nano/linux
~/LicheePi_Nano/buildroot-2018.02.11
~/LicheePi_Nano/gmenu2x
~/LicheePi_Nano/gpsp
~/LicheePi_Nano/SoftWare/DinguxCommander
~/pocketsnes_f1c200s
~/LicheePi_Nano/third_party/snes9x4d
~/LicheePi_Nano/board_tools_f1c200s
~/LicheePi_Nano/third_party/ONScripter-EN
~/LicheePi_Nano/third_party/picodrive
```

## Board
```text
Board IP: 172.28.73.247
User: root
Password: 1
```

## Toolchain
```text
~/LicheePi_Nano/buildroot-2018.02.11/output/host/bin/arm-buildroot-linux-gnueabi-gcc
~/LicheePi_Nano/buildroot-2018.02.11/output/host/bin/arm-buildroot-linux-gnueabi-g++
~/LicheePi_Nano/buildroot-2018.02.11/output/host/bin/arm-buildroot-linux-gnueabi-strip
```

Kernel toolchain found on host:
```text
/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi-gcc

Before kernel builds, export:
export PATH=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin:$PATH

Then use:
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- -j8
```

Default app/tool cross prefix:
```text
~/LicheePi_Nano/buildroot-2018.02.11/output/host/bin/arm-buildroot-linux-gnueabi-
```

## Kernel
Full build:
```sh
cd ~/LicheePi_Nano/linux
export PATH=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin:$PATH
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- -j8
```

DTB only:
```sh
cd ~/LicheePi_Nano/linux
export PATH=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin:$PATH
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- suniv-f1c100s-licheepi-nano.dtb -j8
```

Product:
```text
~/LicheePi_Nano/linux/arch/arm/boot/dts/suniv-f1c100s-licheepi-nano.dtb
```

## SPI ST7789 DMA Work
Current host-side SPI screen performance branch status:
```text
Date: 2026-06-10 to 2026-06-11
Host backup dir:
~/LicheePi_Nano/backups/kernel_dts_spi_dma_20260610_185031
```

Current intended kernel-side changes:
```text
1. ST7789 device tree `spi-max-frequency = <48000000>;`
2. SPI1 node has DMA properties in `suniv-f1c100s.dtsi`:
   dmas = <&dma SUN4I_DMA_NORMAL 0x05>, <&dma SUN4I_DMA_NORMAL 0x05>;
   dma-names = "rx", "tx";
3. Active SPI controller driver is `drivers/spi/spi-sun6i.c`
   because SPI node compatible resolves to the sun8i-h3/suniv path.
4. `spi-sun6i.c` has local F1C200S DMA support added for large transfers,
   keeping small transfers and non-TX-dominant cases on the original PIO path.
```

Verified host build artifacts:
```text
~/LicheePi_Nano/linux/arch/arm/boot/zImage
~/LicheePi_Nano/linux/arch/arm/boot/dts/suniv-f1c100s-licheepi-nano.dtb
~/LicheePi_Nano/linux/drivers/spi/spi-sun6i.o
```

Latest host verification:
```text
2026-06-11:
- Incremental kernel rebuild on host completed with EXIT=0
- DTS still shows 48 MHz on the ST7789 node
- SPI1 DMA properties still present
- spi-sun6i.c still contains the local DMA hooks
- Compared against local bare-metal `F1C100S_SPI娴嬭瘯_W25Q128`
- Tightened spi-sun6i DMA config to match the bare-metal SPI style more closely:
  * tx/rx DMA bus width = 1 byte
  * tx/rx DMA burst = 1
  * check `dmaengine_slave_config()` return values
- Rebuilt `drivers/spi/spi-sun6i.o` on host successfully after that change
```

## RGB 360x640 RGB565 Work
Current host-side RGB panel branch status:
```text
Date: 2026-06-11
Host backup dir:
~/LicheePi_Nano/backups/kernel_rgb565_360x640_20260611_115500
```

Current intended kernel-side changes:
```text
1. `suniv-f1c100s-licheepi-nano.dts`
   - panel-dpi keeps 360x640 timing
   - clock-frequency = <16494000> for the current 360x640 @ 60Hz timing set
   - bpc = <6>
   - bus-format = <0x1017>  // MEDIA_BUS_FMT_RGB565_1X16
   - st7789 node kept enabled after user clarification; RGB timing/bus-format work must coexist with the SPI panel node in DTS
2. `drivers/gpu/drm/panel/panel-simple.c`
   - generic panel-dpi parser reads optional DT properties:
     * `bpc`
     * `bus-format`
3. Existing TCON pin group remains `lcd_rgb666_keypad_pins`, but it is already
   the 20-pin subset that matches 16-bit RGB565 + sync signals on this board
   because PD0 and PD12 are intentionally excluded.
```

Verified host build artifacts:
```text
~/LicheePi_Nano/linux/arch/arm/boot/zImage
~/LicheePi_Nano/linux/arch/arm/boot/dts/suniv-f1c100s-licheepi-nano.dtb
~/LicheePi_Nano/linux/drivers/gpu/drm/panel/panel-simple.o
~/LicheePi_Nano/backups/kernel_rgb565_360x640_20260611_115500/artifacts_after_build/
```

Latest host verification:
```text
2026-06-11:
- DTB-only build passed
- panel-simple.o build passed
- full kernel build passed with EXIT=0
```

## Buildroot
```sh
cd ~/LicheePi_Nano/buildroot-2018.02.11
/usr/bin/make -j8
```

## Buildroot Runtime Bundle Automation - 2026-07-12
```text
Goal:
- make the F1C200S runtime apps/scripts deploy automatically as part of the
  normal Buildroot build, instead of requiring a separate manual
  install-to-target step after every build.

Canonical runtime source:
- ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay

Current intended Buildroot hooks:
- BR2_ROOTFS_OVERLAY="../board_tools_f1c200s/runtime_bundle/rootfs_overlay"
- BR2_ROOTFS_POST_BUILD_SCRIPT=
  "board/licheepi/nano/remove_legacy_init.sh ../board_tools_f1c200s/runtime_bundle/buildroot_post_build.sh"

Post-build runtime bundle hook:
- ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/buildroot_post_build.sh
- It runs:
  1. collect_runtime_payload.sh
  2. install_payload_to_target.sh

Practical result:
- regular `make` now re-applies the runtime bundle automatically
- gmenu2x package contents, launcher scripts, helper scripts, firmware, and
  selected extra binaries/modules are injected during the Buildroot post-build
  phase
- manual `sync_overlay_to_buildroot.sh` should no longer be required for the
  canonical path once the Buildroot config points directly at the runtime
  bundle overlay
```

## Snes9x4D RS90 F1C200S Large-Screen Adaptation - 2026-07-12
```text
Purpose in this round:
- keep the original default SFC path untouched
- continue the separate `SFC RS90` test path
- remove the RS90 hardcoded tiny-screen assumptions that were still forcing a
  240x160 framebuffer path

Local source-of-truth files changed:
- C:\Users\26301\Desktop\_tmp_snes9x4d_rs90\snes9x4d-rs90-master\dingux-sdl\sdlvideo.cpp
- C:\Users\26301\Desktop\_tmp_snes9x4d_rs90\snes9x4d-rs90-master\dingux-sdl\sdlmain.cpp
- C:\Users\26301\Desktop\board_tools_f1c200s\runtime_bundle\rootfs_overlay\root\run_snes9x4d_rs90.sh

Host backup for this round:
- ~/LicheePi_Nano/backups/snes9x4d_rs90_f1c200s_20260712_021445

Key changes:
1. `sdlvideo.cpp`
   - stop forcing `SDL_SetVideoMode(240, 160, 16, ...)`
   - use the requested `-xs/-ys` framebuffer size instead
2. `sdlmain.cpp`
   - when the framebuffer is large enough, center the native SNES frame with
     `SDL_BlitSurface()` instead of always downscaling to the RS90 160-line path
   - clear the destination surface each frame before blitting
   - keep the old RS90 downscale path only as fallback for genuinely small screens
3. `run_snes9x4d_rs90.sh`
   - now passes explicit size args:
     `-xs ${S9X4D_W:-480} -ys ${S9X4D_H:-360}`
   - adds launcher tunables:
     `S9X4D_W`, `S9X4D_H`, `S9X4D_BUF`, `S9X4D_SOUND`, `S9X4D_EXTRA_ARGS`

Host build result:
- rebuilt with:
  `cd ~/LicheePi_Nano/third_party/snes9x4d_rs90_f1c200s/dingux-sdl`
  `make -f Makefile.f1c200s clean`
  `make -f Makefile.f1c200s -j4`
- rebuild passed
- new binary md5:
  `19da74d8f683fe0fcd1113ec352d360b`

Runtime bundle refresh:
- `collect_runtime_payload.sh` passed
- `install_payload_to_target.sh /home/wnk/LicheePi_Nano/buildroot-2018.02.11/output/target` passed
- verified target md5 matches host binary:
  `19da74d8f683fe0fcd1113ec352d360b`

Board deployment:
- deployed to board:
  `/root/snes9x4d_rs90`
  `/root/run_snes9x4d_rs90.sh`
  `/root/gmenu2x/sections/emulators/sfc_rs90`
- verified board md5:
  `19da74d8f683fe0fcd1113ec352d360b`

Current limitation at end of this round:
- board-side runtime ROM test was still blocked by an empty current ROM path:
  `/root/roms/sfc` had no `.smc/.sfc/.zip` files at verification time
- so this round proved build/integration/deploy, but not yet gameplay/runtime behavior

Immediate next board-side validation:
1. put at least one test ROM under `/root/roms/sfc`
2. run:
   `/root/run_snes9x4d_rs90.sh /root/roms/sfc/<game>.smc`
3. judge:
   - launch success
   - control mapping
   - exit behavior
   - performance vs current default SFC path
```

## GMenu2X Browser
Current lightweight browser plan:
```text
Use Buildroot `links` in text mode. Do not use Qt/WebKit/Chromium on F1C200S
unless explicitly requested; they are too heavy for the current RAM/CPU budget.
```

Buildroot config:
```text
BR2_PACKAGE_LINKS=y
# BR2_PACKAGE_LINKS_GRAPHICS is not set
```

Runtime integration:
```text
/root/run_browser.sh
/root/browser_home.html
/usr/bin/links
/root/gmenu2x/sections/applications/browser
```

GMenu2X menu item:
```text
title=Browser
description=Links text web browser
exec=/root/run_browser.sh
workdir=/root
wrapper=true
```

Payload integration status:
```text
2026-06-17:
- `links-2.14.tar.bz2` downloaded from `http://sources.buildroot.net/links-2.14.tar.bz2`
  after the upstream twibright URL stalled.
- sha256 verified:
  f70d0678ef1c5550953bdc27b12e72d5de86e53b05dd59b0fc7f07c507f244b8
- `make links -j8` passed.
- Full Buildroot `/usr/bin/links` exists in:
  ~/LicheePi_Nano/buildroot-2018.02.11/output/target/usr/bin/links
- `runtime_bundle/collect_runtime_payload.sh` now copies `payload/links`.
- `runtime_bundle/install_payload_to_target.sh` now installs it to target
  `/usr/bin/links`.
- Full Buildroot rootfs rebuilt successfully:
  ~/LicheePi_Nano/buildroot-2018.02.11/output/images/rootfs.tar
```

## GMenu2X Video Player Integration - 2026-06-30
```text
User requirement:
- Keep the existing hardware-direct video display path unchanged.
- Add it into GMenu2X like the emulators: browse a directory, select a file,
  click it, and play.

Current chosen integration:
- Player binary:
  ~/LicheePi_Nano/third_party/cedar_drm_player/cedar_drm_player
- Thin launcher script:
  /root/run_cedar_drm_player.sh
- GMenu2X entry:
  title=Video
  exec=/root/run_cedar_drm_player.sh
  params=[selFullPath]
  selectordir=/root/roms/video
  selectorbrowser=true
  selectorfilter=.mp4,.m4v,.264,.h264

Runtime bundle files changed:
- ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/collect_runtime_payload.sh
  now copies:
  payload/cedar_drm_player
- ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/install_payload_to_target.sh
  now installs:
  /root/cedar_drm_player
- ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/root/install_f1c200s_sdl_apps.sh
  now creates the Video menu item and /root/roms/video
- ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/root/run_cedar_drm_player.sh
  added as the launcher script

Scope note:
- This change only wires the existing video hard-decode/direct-display player
  into GMenu2X.
- It intentionally does not change the video display path itself.
- Audio remains whatever the current cedar_drm_player supports at runtime; the
  integration layer does not add missing audio logic.
```

## TVD
```sh
cd ~/LicheePi_Nano/tvd_f1c100s_linux57/src
make KBUILD=~/LicheePi_Nano/linux ARCH=arm CROSS_COMPILE=arm-linux-gnueabi-
insmod /root/suniv_f1c100s_tvd.ko force_tvd_clk=1
v4l2-ctl -d /dev/video7 --set-standard=pal
v4l2-ctl -d /dev/video7 --stream-mmap=3 --stream-count=120 --stream-to=/dev/null
```

## TVD Preview
```sh
cd ~/LicheePi_Nano/tvd_f1c100s_linux57/tools/fb_preview
make CROSS_COMPILE=~/LicheePi_Nano/buildroot-2018.02.11/output/host/bin/arm-buildroot-linux-gnueabi-
/root/tvd_fb_preview -d /dev/video7 -f /dev/fb0 -s pal
```

## Cedar
```sh
cd ~/LicheePi_Nano/cedar_f1c200s_linux57/build
make KBUILD=~/LicheePi_Nano/linux CROSS_COMPILE=arm-linux-gnueabi-
ls -l /dev/ion /dev/cedar_dev
cat /proc/interrupts | grep -i cedar
```

## GMenu2X
```sh
cd ~/LicheePi_Nano/gmenu2x
make -f Makefile.f1c200s
/root/run_gmenu2x.sh
```

Host-side scripts:
```text
~/LicheePi_Nano/gmenu2x/run_gmenu2x.sh
~/LicheePi_Nano/gmenu2x/install_gmenu2x_f1c200s_layout.sh
```

Current matrix-keypad input mapping:
```text
GMenu2X reads `/root/gmenu2x/input.conf` directly from `exe_path()`.
Keep all generated/default input.conf files aligned with the matrix keypad:

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

L/R page or section switching depends on `KEY_PAGEUP/KEY_PAGEDOWN`, not literal
keyboard `l/r`.
```

GMenu2X L/R input fix - 2026-06-16:
```text
Problem:
L/R no longer flipped pages/sections because some gmenu2x packaged defaults
still used literal keyboard `l/r`:
section_prev=keyboard,108,SDLK_l
section_next=keyboard,114,SDLK_r
pageup=keyboard,108,SDLK_l
pagedown=keyboard,114,SDLK_r

Files updated on host:
~/LicheePi_Nano/gmenu2x/assets/linux/input.conf
~/LicheePi_Nano/gmenu2x/dist/f1c200s/input.conf

Already-correct generated installer:
~/LicheePi_Nano/gmenu2x/install_gmenu2x_f1c200s_layout.sh
~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/root/install_f1c200s_sdl_apps.sh

Local Windows backup/work:
C:\Users\26301\Desktop\F1C200S_gmenu2x_input_fix

Build/integration completed on host:
cd ~/LicheePi_Nano/gmenu2x
make -f Makefile.f1c200s -j4

cd ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle
./sync_overlay_to_buildroot.sh
./collect_runtime_payload.sh
./install_payload_to_target.sh

Verified:
~/LicheePi_Nano/gmenu2x/dist/f1c200s/input.conf
~/LicheePi_Nano/gmenu2x/dist/gmenu2x-f1c200s.tar.gz
~/LicheePi_Nano/buildroot-2018.02.11/output/target/root/gmenu2x/input.conf
all contain PAGEUP/PAGEDOWN mapping for section/page actions.

Board was offline at test time, SSH root@172.28.73.247 timed out.
Deploy when board is online through standard runtime payload or copy:
/root/gmenu2x/input.conf
/root/gmenu2x/gmenu2x if the rebuilt tar is deployed
Do not reboot automatically.
```

GMenu2X helper app integration status - 2026-07-11:
```text
Canonical generator:
- ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/root/install_f1c200s_sdl_apps.sh

Current application entries verified in both output/target and rootfs.tar:
- android_record
- bluetooth
- bluetooth_check
- bluetooth_scan
- browser
- files
- key_test
- poweroff
- reboot
- rom_server
- tvd_preview
- video
- wifi_scan
- wifi_speed

Current emulator entries verified:
- gba
- md
- ons
- ps1
- sfc

Current known omission:
- gb/gbc is still absent until a real `sdlgnuboy` binary is restored into the
  payload source path.
```

## gpSP
```sh
cd ~/LicheePi_Nano/gpsp/f1c200s
make
make info
/root/run_gpsp_menu.sh
```

Host-side scripts:
```text
~/LicheePi_Nano/gpsp/run_gpsp.sh
~/LicheePi_Nano/gpsp/run_gpsp_menu.sh
```

## DinguxCommander
```sh
cd ~/LicheePi_Nano/SoftWare/DinguxCommander
make CONFIG=f1c200s clean
make CONFIG=f1c200s -j4
```

Product:
```text
~/LicheePi_Nano/SoftWare/DinguxCommander/output/f1c200s/DinguxCommander
```

## PocketSNES / SFC
```sh
cd ~/pocketsnes_f1c200s
make -f Makefile.f1c200s clean
make -f Makefile.f1c200s -j4
make -f Makefile.f1c200s strip
```

Product:
```text
~/pocketsnes_f1c200s/pocketsnes/pocketsnes
```

Host-side scripts:
```text
~/pocketsnes_f1c200s/run_pocketsnes.sh
~/pocketsnes_f1c200s/install_gmenu2x_sfc_link.sh
```

## Snes9x4D / SFC
Current default SFC emulator. Keep the runtime and gmenu2x entry pinned to
`/root/run_snes9x4d.sh` unless explicitly redirected to another emulator.

SFC rotation note - 2026-06-30:
```text
Snes9x4D itself does not have a real whole-screen 90-degree rotation feature.
The menu item/comment "rotate through scalers" only cycles display/scaler modes;
it does not rotate the final framebuffer.

Current F1C200S rotation handling is in the local DRM presenter:
~/LicheePi_Nano/third_party/snes9x4d_miyoo/dingux-sdl/sdlmain.cpp

The presenter reads:
SFC_DRM_ROTATION=none|cw|ccw|180

It rotates the 256x224 SNES RGB565 frame while expanding into the XRGB8888 DRM
dumb buffer, then lets the DRM plane/frontend scale that rotated buffer to the
configured destination rectangle. This is the correct path for this SoC because
the F1C200S DEFE/DEBE/TCON path has scaling but no real 90-degree rotation.

Integration fix:
~/LicheePi_Nano/third_party/snes9x4d_miyoo/run_snes9x4d_drm.sh now maps the
global SDL/GMenu rotation variable to SFC_DRM_ROTATION when SFC_DRM_ROTATION is
not explicitly set:
- SDL_DISPLAY_ROTATION=CW/right/90 -> SFC_DRM_ROTATION=cw
- SDL_DISPLAY_ROTATION=CCW/left/270 -> SFC_DRM_ROTATION=ccw
- SDL_DISPLAY_ROTATION=UD/180 -> SFC_DRM_ROTATION=180
- otherwise -> none

The rotated DRM dumb-buffer width was also tightened to the actual rotated
source width:
f1c_drm_bo_w = f1c_drm_src_w
This avoids keeping a 224x256 rotated image inside a wider 256x256 BO.

Centering rule:
- The DRM launcher no longer defaults to filling the whole visible rectangle.
- It now computes a maximum-fit aspect-preserving destination rectangle inside
  SDL_FBCON_VISIBLE_W/H and centers it automatically:
  * non-rotated source aspect: 256x224
  * rotated source aspect: 224x256
- Quick scaling control is available through:
  `SFC_DRM_SCALE=fit|auto|none|1x|2x|3x|4x|1.25x|1.5x|0.75x|...`
  * `fit` / `auto`: maximum aspect-preserving centered fit
  * `none` / `1x`: no scaling, 1:1 centered
  * numeric / numeric-with-x values: centered scale factor, non-integer allowed
  * if the requested scaled size exceeds the visible area, the launcher now
    scales it back proportionally to fit; it no longer clamps width and height
    independently, which had caused aspect-ratio distortion on a 360x640 panel
- Manual overrides still work through:
  SFC_DRM_DST_X / SFC_DRM_DST_Y / SFC_DRM_DST_W / SFC_DRM_DST_H
```

```sh
cd ~/LicheePi_Nano/third_party/snes9x4d
make clean
make PREFIX=~/LicheePi_Nano/buildroot-2018.02.11/output/host/bin/arm-buildroot-linux-gnueabi -j4
```

Product:
```text
~/LicheePi_Nano/third_party/snes9x4d/snes9x4d
```

F1C200S local changes:
```text
src/sdlmenu/sdlmain.cpp:
- default SoundPlaybackRate = 6, meaning 44100Hz
- default SoundBufferSize = 1024
- default DisplayFrameRate = TRUE for performance testing

runtime:
- `run_snes9x4d.sh` stops `matrix_pad_bridge` before launch because this emulator
  uses SDL keyboard input rather than SDL joystick input
- launcher exports:
  `S9XKEYS=47,30,48,45,21,104,109,28,15,105,106,103,108`
  meaning:
  `QUIT=KEY_V`
  `A=KEY_A`
  `B=KEY_B`
  `X=KEY_X`
  `Y=KEY_Y`
  `L=KEY_PAGEUP`
  `R=KEY_PAGEDOWN`
  `START=KEY_ENTER`
  `SELECT=KEY_TAB`
  `LEFT/RIGHT/UP/DOWN=arrow keys`
```

Runtime:
```text
/root/snes9x4d
/root/run_snes9x4d.sh [rom]
/root/run_snes9x4d_from_gmenu2x.sh [rom]
```

Current audio/input launcher defaults:
```text
- Snes9x4D now uses direct evdev input:
  S9X_INPUT_DEV=/dev/input/event0
- Snes9x4D launcher exports Linux input keycodes through S9XKEYS
- Current audio target is forced to:
  AUDIODEV=hw:3,0
```

Snes9x4D hotkeys:
```text
Select + Start: emulator menu
Select + Start + X: quit emulator
Select + Start + B: reset ROM
Start + R: save state
Start + L: load state
```

## ONScripter-EN
```sh
cd ~/LicheePi_Nano/third_party/ONScripter-EN
make -f Makefile.f1c200s -j4 binary
```

Product:
```text
~/LicheePi_Nano/third_party/ONScripter-EN/onscripter-en
```

Runtime:
```text
/root/run_onscripter.sh [game_dir]
Default game root: /root/roms/ons
Per-game saves: /root/.ons_save/<game-dir-name>
```

ONS YZ text rendering fix - 2026-06-16:
```text
Problem:
YZ can start, play audio, show images/windows, and accept input, but Chinese text
is invisible. Glyph tracing proved:
- 0.utf is detected and UTF-8 mode is active.
- UTF-8 to Unicode conversion is correct.
- TTF_GlyphMetrics succeeds.
- TTF_RenderGlyph_Shaded returns valid 8-bit glyph surfaces.

Root-cause area:
32bpp text blending path, not script encoding/font loading/framebuffer.

Source fixes currently built:
~/LicheePi_Nano/third_party/ONScripter-EN/ONScripterLabel_text.cpp
- renderGlyph converts SDL_ttf 8-bit palette glyph brightness to 0..255 alpha.

~/LicheePi_Nano/third_party/ONScripter-EN/AnimationInfo.cpp
- blendText 32bpp path writes glyph alpha/color directly into the cached text image surface.

~/LicheePi_Nano/third_party/ONScripter-EN/ONScripterLabel_image.cpp
- alphaBlendText 32bpp path now keeps alpha when drawing directly to accumulation_surface.
- The old full-strength text path wrote only RGB and left alpha at 0; this matches
  the observed "glyph exists but text is invisible" symptom.

Host backup:
~/LicheePi_Nano/backups/ons_text_alpha_fix_20260616_182425

Build result:
cd ~/LicheePi_Nano/third_party/ONScripter-EN
make -f Makefile.f1c200s -j4 binary
md5sum onscripter-en
1ad1cafc64d182e7c23a5944be368883  onscripter-en

Local Windows artifact:
C:\Users\26301\Desktop\F1C200S_ONS_text_blend_fix\onscripter-en.text_alpha_tracegated

Buildroot target updated:
~/LicheePi_Nano/buildroot-2018.02.11/output/target/root/onscripter-en
md5: 1ad1cafc64d182e7c23a5944be368883

Additional source audit result:
- Common 32bpp graphics blend macros also tend to output RGB without preserving
  alpha. This is risky for intermediate surfaces but is too broad to change
  before board verification because it affects images/effects globally.
- F1C200S debug logs were writing to /tmp without environment gates in some
  paths. Since /tmp is tmpfs and the device has little RAM, this can contribute
  to OOM during ONS testing. Rebuilt binary now only writes those logs when:
  ONS_INPUT_TRACE, ONS_IMAGE_TRACE, ONS_BGMSTOP_TRACE, ONS_TRACE, or
  ONS_GLYPH_TRACE are explicitly set.

Trace gating backup:
~/LicheePi_Nano/backups/ons_trace_gating_20260616_183908

Board deployment is still pending because both known board IPs timed out:
10.67.68.247
172.28.73.247

When board is online, deploy:
scp onscripter-en.text_alpha_tracegated root@<BOARD_IP>:/root/onscripter-en.new
ssh root@<BOARD_IP> 'killall onscripter-en 2>/dev/null; killall matrix_ps2mouse_bridge 2>/dev/null; cp /root/onscripter-en /root/onscripter-en.before_text_alpha_fix_$(date +%Y%m%d_%H%M%S); mv /root/onscripter-en.new /root/onscripter-en; chmod 755 /root/onscripter-en; sed -i "/ONS_GLYPH_TRACE/d" /root/run_onscripter.sh; rm -f /tmp/ons_glyph_trace.log /tmp/ons_trace.log /tmp/ons_image_trace.log; sync; md5sum /root/onscripter-en'

Expected next test:
Launch YZ from gmenu2x. If text appears, remove temporary glyph/image trace code
from source and keep only the alpha/blend fixes plus YZ compatibility fixes.
If text is still invisible, capture /dev/fb0 at the text scene and inspect the
text surface/accumulation alpha path again.
```

Current landscape/mouse runtime:
```text
2026-06-16:
- ONS audio is currently working with launcher args:
  --scale --audiobuffer 8 --nomatch-audiodevice-to-bgm
  Do not remove these args while debugging mouse/input.
- ONS launcher accepts either a game directory or a file path such as
  /root/roms/ons/YZ/nscript.dat; file paths are converted to dirname.
- Common SDL landscape env sets SDL_NOMOUSE for most apps, but ONS must override it:
  unset SDL_NOMOUSE
  SDL_MOUSEDRV=PS2
  SDL_MOUSEDEV=/tmp/matrix_ps2mouse
- Temporary ONS software cursor overlay should stay disabled:
  unset ONS_F1C200S_DRAW_MOUSE
- matrix_ps2mouse_bridge now supports:
  -r none|cw|ccw
- ONS launcher starts the bridge with:
  ROTATE=${ONS_MOUSE_ROTATE:-cw}
- If ONS mouse direction feels wrong after landscape rotation, first test
  ONS_MOUSE_ROTATE=ccw in /root/run_onscripter.sh. Do not change kernel/display
  rotation for this.
```

Current YZ debug status:
```text
2026-06-13:
- Board was not available for final runtime verification.
- Host backup before source tracing:
  ~/LicheePi_Nano/backups/onscripter_trace_before_20260612_233119
- Local backup/work copies are in:
  C:\Users\26301\Desktop\F1C200S_Codex_Archive_20260613
- ONS source now has low-cost trace logging to:
  /tmp/ons_trace.log
  /tmp/ons_input_debug.log
  /tmp/matrix_ps2mouse_packets.log
- Trace points cover:
  executeLabel command before/after
  clickCommand enter/exit
  btnwaitCommand enter/exit/retry
  movieCommand file/flags
  waveCommand file/audio state
  matrix PS/2 FIFO packets consumed directly by ONS
- Host build passed after trace instrumentation.
- run_onscripter.sh in runtime overlay clears old trace logs before launch.
- ONS mouse bridge mapping is intentionally minimal:
  Direction keys -> mouse move
  A -> left click
  Start/Enter -> left click only when Select is not held
  B -> right click
  X/Y/Select/L/R do not emit ordinary mouse buttons
- ONS special combos through matrix_ps2mouse_bridge:
  Start + Select + B -> ONS menu/right-menu special packet
  Start + Select + X -> quit ONS special packet
- `collect_runtime_payload.sh` and `install_payload_to_target.sh` now include:
  `/root/matrix_ps2mouse_bridge`
- ONS key mapping test script:
  `/root/test_ons_keys.sh`
  Usage:
  `./test_ons_keys.sh`
  `./test_ons_keys.sh /dev/input/event0`
```

## PicoDrive / MD
```sh
cd ~/LicheePi_Nano/third_party/picodrive
make -f Makefile.f1c200s clean
make -f Makefile.f1c200s -j4
```

Products:
```text
~/LicheePi_Nano/third_party/picodrive/picodrive
~/LicheePi_Nano/third_party/picodrive/PicoDrive.zip
```

Runtime:
```text
/root/run_picodrive.sh [rom]
ROM root: /root/roms/md
save/config root: /root/picodrive/{cfg,mds,srm,brm,tape}
```

## Rootfs SDL Integration
```sh
cd ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle
./sync_overlay_to_buildroot.sh
./collect_runtime_payload.sh
./install_payload_to_target.sh
cd ~/LicheePi_Nano/buildroot-2018.02.11
/usr/bin/make -j8
```

Standard SDL integration entry points:
```text
~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/sync_overlay_to_buildroot.sh
~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/collect_runtime_payload.sh
~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/install_payload_to_target.sh
~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/root/install_f1c200s_sdl_apps.sh
```

## MPlayer
Current system-wide tuning file:
```text
~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/etc/mplayer/mplayer.conf
```

Current F1C200S AAC tuning:
```text
afm=faad,ffmpeg
```

Purpose:
```text
Prefer libfaad2 over MPlayer's default ffmpeg AAC path for local MP4/AAC
playback on ARM926-class targets, to reduce CPU usage and stutter risk.
```

SDL app integration checklist:
```text
1. Provide or update /root/run_*.sh launcher in rootfs_overlay/root/.
2. Add gmenu2x section/link generation to install_f1c200s_sdl_apps.sh.
3. Add host-built binary pickup to collect_runtime_payload.sh if the binary is not already inside the overlay.
4. Add payload install logic to install_payload_to_target.sh if a binary must be copied into /root.
5. Keep framebuffer, SDL video/audio, and key handling aligned with gmenu2x.
6. If an SDL app should use the matrix keypad as a joystick, integrate `/root/matrix_pad_bridge` through the launcher script instead of changing the kernel keypad driver.
```

## USB WiFi / rtl8723bu
Current USB WiFi tuning:
```text
Date: 2026-06-13
Driver observed on host: third_party/rtl8723bu/8723bu.ko
Driver version: v4.3.6.11_12942.20141204_BTCOEX20140507-4E40
Reason: USB WiFi SCP speed was around 300 KB/s and boot association/DHCP took 10+ seconds.
```

Configured files:
```text
Buildroot target:
~/LicheePi_Nano/buildroot-2018.02.11/output/target/etc/init.d/S17rtl8723bu
~/LicheePi_Nano/buildroot-2018.02.11/output/target/etc/init.d/S45usb-wifi
~/LicheePi_Nano/buildroot-2018.02.11/output/target/etc/default/usb-wifi
~/LicheePi_Nano/buildroot-2018.02.11/output/target/etc/wpa_supplicant.conf

Runtime overlay:
~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/etc/init.d/S17rtl8723bu
~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/etc/init.d/S45usb-wifi
~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/etc/default/usb-wifi
~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/etc/wpa_supplicant.conf
```

Current tuning:
```text
8723bu module options:
rtw_power_mgnt=0
rtw_ips_mode=0
rtw_enusbss=0
rtw_btcoex_enable=1
rtw_ampdu_enable=2
rtw_usb_rxagg_mode=1

S45usb-wifi:
WAIT_SEC=12
DHCP_TRIES=3
DHCP_TIMEOUT=3
loads 8723bu before waiting for wlan0
wpa_supplicant driver order: nl80211,wext
prints wifi init elapsed seconds

/etc/default/usb-wifi:
MODE=static
ADDR=172.28.73.247
MASK=255.255.255.0
GW=172.28.73.171
DNS1=172.28.73.171
DNS2=223.5.5.5

wpa_supplicant.conf:
scan_ssid=0
```

Live-board warning:
```text
Do not restart WiFi from an SSH session unless the user explicitly asks; restarting S45usb-wifi will drop the connection.
```

## Kernel toolchain rule - 2026-06-14
- Kernel compile command is fixed: `make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- -j8`.
- Do not change kernel CROSS_COMPILE prefix to Buildroot toolchain automatically.
- If compilation fails, report the error; do not silently switch toolchains.

## Pending Board Tests
Use this section whenever the board is not physically available. When a fix is
prepared but cannot be verified on target, record the exact deployment and test
steps here before ending the turn. After compact/resume, read this section and
continue from it instead of relying on memory.

### ST7701 RGB panel driver / srgn init - 2026-06-15
Host-side status:
```text
Today a kernel-side ST7701/SRGN-style panel init path was prepared according to
the rhodesepass/buildroot same-screen repository direction.

Important hardware/control pins for the panel init bit-bang bus:
CS  = PA0
SDA = PA1
SCL = PA2
RST = PA3

Important extra panel line:
DE must be held at a defined high level on PD19 because the reference hardware
pulls DE up. Our board should drive PD19 high if it is connected.

The user also found that for the laowu screen path SCL/SDA may need swapping,
but the currently connected panel appears to be the HSD screen.
```

Kernel/runtime observations already seen on board:
```text
dmesg showed:
panel-simple panel: supply power not found, using dummy regulator
srgn:init seq count=313
srgn: st7701 init work handler running
sun4i-drm display-engine: fb0: sun4i-drmdrmfb frame buffer device
srgn: st7701 init (work) completed

At one point /dev/fb0 existed and the previous abnormal colored lines improved,
but display was still abnormal. User later found MISO/RST hardware was not
connected correctly and planned to fly/fix reset wiring.
```

Kernel build rule reminder:
```sh
cd ~/LicheePi_Nano/linux
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- -j8
```

Do not silently switch to the Buildroot toolchain for kernel. If build fails,
report the exact error.

Deploy when board is online and user has built kernel or asks us to deploy:
```sh
# Typical products to deploy, depending on current boot layout:
~/LicheePi_Nano/linux/arch/arm/boot/zImage
~/LicheePi_Nano/linux/arch/arm/boot/dts/suniv-f1c100s-licheepi-nano.dtb
```

Board-side checks:
```sh
dmesg | grep -iE 'st7701|srgn|panel|drm|fb|tcon|lcd|rgb'
ls -l /dev/fb*
cat /sys/class/graphics/fb0/name 2>/dev/null
cat /sys/class/graphics/fb0/virtual_size 2>/dev/null
fbset -fb /dev/fb0
```

Framebuffer smoke tests:
```sh
# Clear screen black, then white, then simple RGB565-style raw fill if tools exist.
dd if=/dev/zero of=/dev/fb0 bs=1024 count=900 2>/dev/null
```

Expected result:
```text
After reset wiring is fixed and DE is held high, ST7701 init should complete and
fb0 should display a stable 360x640 RGB panel image without colored-line
tearing. If init logs complete but the screen stays white/abnormal, compare the
init table and timing again against the rhodesepass/buildroot HSD panel config.
```

If still broken, collect:
```sh
dmesg | grep -iE 'st7701|srgn|panel|drm|fb|tcon|lcd|rgb|pinctrl|gpio'
cat /sys/kernel/debug/pinctrl/*/pinmux-pins 2>/dev/null | grep -iE 'PA0|PA1|PA2|PA3|PD19|lcd|gpio'
cat /sys/kernel/debug/clk/clk_summary 2>/dev/null | grep -iE 'tcon|de|lcd|pll'
ls -l /dev/fb*
fbset -fb /dev/fb0
```

Important:
```text
Do not reboot automatically. User handles reboots.
Before further kernel source/DTS changes, create a timestamped host backup and
write BACKUP_LOG.txt.
```

### ONScripter YZ csel display fix - 2026-06-15
Host-side status:
```text
Built successfully on host:
~/LicheePi_Nano/third_party/ONScripter-EN/onscripter-en

Changed source:
~/LicheePi_Nano/third_party/ONScripter-EN/ONScripterLabel_command.cpp

Host backup:
~/LicheePi_Nano/backups/ons_csel_builtin_fix_20260615_114014

Local backup/work dirs:
C:\Users\26301\Desktop\F1C200S_ONS_csel_fix_20260615_114014
C:\Users\26301\Desktop\F1C200S_ONS_work
```

Purpose:
```text
YZ game enters *customsel after clicking Start, but the custom option bar is not
visible. ONS now supports env var ONS_F1C200S_BUILTIN_CSEL=1, which renders csel
with ONS built-in selectable text buttons and writes YZ's sentaku variable
directly. YZ defines `numalias sentaku, 812`, so this fallback writes %812.
```

Runtime script status:
```text
~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/root/run_onscripter.sh

The launcher enables ONS_F1C200S_BUILTIN_CSEL=1 only when GAME_NAME is YZ.
Other ONS games should keep the normal customsel path.
```

Deploy when board is online:
```sh
cd ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle
./sync_overlay_to_buildroot.sh
./collect_runtime_payload.sh
./install_payload_to_target.sh
```

Board-side test:
```sh
/root/run_gmenu2x.sh
# Open ONS -> YZ -> nscript.dat from gmenu2x.
# Click Start with the virtual mouse.
```

Expected result:
```text
After clicking Start, the first new-game prompts should show as visible built-in
text choices instead of an invisible custom option bar:
1. Chinese translation notice click screen
2. "鏄惁闇€瑕佹墦寮€鍏ㄥ浘閴淬€佸叏鎴愬氨鍙婇€氬叧鐘舵€侊紵"
3. difficulty choice
4. "鏄惁璺宠繃鎴樻枟涓殑鎹曢鍦烘櫙锛?
After the prompts, the game should reach the early story and eventually display
chara\iriasu_st01.jpg.
```

If still broken, collect:
```sh
cat /tmp/ons_trace.log
cat /tmp/ons_button_trace.log
cat /tmp/ons_image_trace.log
cat /tmp/ons_input_debug.log
cat /tmp/matrix_ps2mouse_packets.log
ps | grep onscripter
```

Current unresolved YZ behavior:
```text
2026-06-16:
User confirmed the Start button area changes state when clicked, so the click is
reaching the script. After the click, CPU usage drops and the game waits, which
matches an invisible choice/input wait rather than a mouse miss.

Likely paths to check/fix in source:
1. `select/csel` should use ONS_F1C200S_BUILTIN_CSEL=1 and draw built-in visible
   text choices instead of jumping to the script's `*customsel`.
2. If YZ reaches `cselbtn + btnwait + cselgoto/getcselstr` instead of plain
   `csel`, the current fallback is not enough. Add/verify a YZ-only fallback in
   `cselbtnCommand()` or `btnwaitCommand()` that creates visible TEXT_BUTTON
   entries from `root_select_link` and returns a valid button number.
3. Do not keep changing mouse mapping for this symptom unless traces show the
   button click is not delivered. The observed button color change means input is
   already entering the game.
```

Script evidence found on local Windows desktop:
```text
C:\Users\26301\Desktop\YZ_script_0.txt
C:\Users\26301\Desktop\YZ_script_7500000.txt
C:\Users\26301\Desktop\YZ_nscript_decoded.txt

`YZ_script_0.txt` starts at `*game_start` and uses repeated `csel` prompts after
the initial click.

`YZ_script_7500000.txt` defines global `*customsel`. It implements choices using:
`getcselnum`, `getcselstr`, `lsp`, `exbtn`, `btnwait`, and `cselgoto`.
Its visible choice text positions are:
1. x=172 y=203, button 554 -> cselgoto 0, sentaku=1
2. x=172 y=263, button 556 -> cselgoto 1, sentaku=2
3. x=172 y=323, button 558 -> cselgoto 2, sentaku=3
4. x=172 y=383, button 552 -> cselgoto 3, sentaku=4
```

Applied fixed-position fallback:
```text
2026-06-16:
Changed `ONScripterLabel_command.cpp` in the YZ-only
ONS_F1C200S_BUILTIN_CSEL path. It now creates built-in TEXT_BUTTON choices at
the same fixed positions used by the game's `*customsel` instead of relying on
the current sentence font cursor position.

Host backup:
~/LicheePi_Nano/backups/ons_yz_csel_fixedpos_20260615_101419

Local work/backup:
C:\Users\26301\Desktop\F1C200S_ONS_yz_csel_fixedpos

Build result:
`make -f Makefile.f1c200s -j4 binary` passed on host.
`collect_runtime_payload.sh` copied the new `onscripter-en` into payload.
```

Important:
```text
Do not reboot automatically. User handles reboots.
Do not restart WiFi from SSH unless explicitly requested.
```

Follow-up source/script analysis - 2026-06-16:
```text
YZ is an 800x600 ONS script. The script header uses:
;$V10000G1500S800,600L10000

Title flow:
1. `*title_menu` draws title buttons with `lsp 102..106`.
2. `spbtn 102,102` maps the Start sprite to button 102.
3. `btnwait %bwait` waits for the title click.
4. If `%bwait==102`, it jumps to `*game_start`.

Important behavior:
Clicking the title Start button does NOT immediately show the new-game choices.
It first enters `*game_start`, draws a Chinese translation notice with
`lsp 700..710 ":s/20,20,1;..."`, then runs `click`. The user must click once
more before the first `csel` prompt appears.

YZ custom choices:
`YZ_script_7500000.txt` defines `*customsel`. It uses:
`getcselnum`, `getcselstr`, `lsp`, `exbtn`, `btnwait`, and `cselgoto`.
Its visible choice bar comes from `system/select_window2.jpg` plus text sprites:
1. text 553 at x=172 y=203, bg/button 554 at x=150 y=190
2. text 555 at x=172 y=263, bg/button 556 at x=150 y=250
3. text 557 at x=172 y=323, bg/button 558 at x=150 y=310
4. text 551 at x=172 y=383, bg/button 552 at x=150 y=370

Current ONS source has a YZ-only env hack:
`ONS_F1C200S_BUILTIN_CSEL=1`
When enabled, `selectCommand()` does NOT jump to the script's `*customsel`.
Instead it creates built-in TEXT_BUTTON choices directly in C++ and writes
YZ's `sentaku` variable `%812` before jumping to the selected label.

Consequence:
With `ONS_F1C200S_BUILTIN_CSEL=1`, the original YZ option bar/background from
`system/select_window2.jpg` will not appear. Only built-in text choices may
appear. If text rendering or scale/position is wrong, the screen looks like it
is stuck waiting after Start.

Most likely root causes for the current "click Start then CPU drops and nothing
happens" symptom:
1. The engine is waiting at the `click` immediately after the translation notice,
   but the notice text sprite is invisible due font/text rendering or scaling.
2. Or it has reached `csel`, but the YZ-only built-in csel hack hides the game's
   normal option bar, so the expected option UI is absent.
3. This is less likely to be a mouse mapping issue because title Start visually
   changes state and reaches the script.

Recommended next source fix:
Prefer removing/disable the YZ-only `ONS_F1C200S_BUILTIN_CSEL` hack first and
make the original `*customsel` path work, because that preserves the game's
real UI and button/background behavior.

If original `*customsel` still does not show:
1. Use board logs:
   cat /tmp/ons_trace.log
   cat /tmp/ons_button_trace.log
   cat /tmp/ons_image_trace.log
   cat /tmp/ons_input_debug.log
2. Check whether `system/select_window2.jpg` and `:s` text sprites load and
   produce nonzero button rects.
3. If images/text are loaded but off-screen, fix scaling/viewport for 800x600
   scripts against the current 384x640 or 640x384 SDL profile.
4. If text sprites are not rendered, inspect font loading and GBK/CP932/UTF-8
   decoding instead of changing mouse mapping.

Keep ONS audio args unchanged while debugging this:
`--scale --audiobuffer 8 --nomatch-audiodevice-to-bgm`
```

Applied runtime config - 2026-06-16:
```text
Changed ONS launcher only; ONS source was not changed in this step.

Local Windows work/backup:
C:\Users\26301\Desktop\F1C200S_ONS_runtime_config
C:\Users\26301\Desktop\F1C200S_ONS_runtime_config\run_onscripter.sh.before_disable_builtin_csel

Host file changed:
~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/root/run_onscripter.sh

Change:
Removed the YZ-only:
`export ONS_F1C200S_BUILTIN_CSEL=1`

The launcher now always runs:
`unset ONS_F1C200S_BUILTIN_CSEL`

Purpose:
Let YZ use its original script-side `*customsel` path so the game can draw
`system/select_window2.jpg` option bars instead of the C++ built-in csel fallback.

Runtime integration already run on host:
cd ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle
./sync_overlay_to_buildroot.sh
./collect_runtime_payload.sh
./install_payload_to_target.sh

Verified in:
~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/root/run_onscripter.sh
~/LicheePi_Nano/buildroot-2018.02.11/output/target/root/run_onscripter.sh

Board was offline at 2026-06-16 14:55, SSH to root@172.28.73.247 timed out.
Deploy when board is online:
scp the updated `/root/run_onscripter.sh` or run the standard runtime payload
deployment to target. Do not reboot automatically.

Board-side test:
/root/run_gmenu2x.sh
Open ONS -> YZ -> nscript.dat
Click Start, then click once more if the Chinese notice page appears or the
script is waiting at the first `click`.

If still no option bar, collect:
cat /tmp/ons_trace.log
cat /tmp/ons_button_trace.log
cat /tmp/ons_image_trace.log
cat /tmp/ons_input_debug.log
cat /tmp/matrix_ps2mouse_packets.log
```

### SDL Rotation And Mouse Rule - 2026-06-16
```text
Current ST7701 framebuffer is portrait 384x640. Keep kernel/panel orientation
portrait unless explicitly requested otherwise.

If SDL applications are rotated in software, mouse handling must be rotated too:
- relative mouse movement direction
- absolute/logical click coordinates
- cursor draw/erase position
- cursor clipping bounds
- any PS2/uinput bridge coordinate transform

Do not enable landscape SDL rotation again by only setting
SDL_VIDEO_FBCON_ROTATION. That can rotate the framebuffer while leaving SDL 1.2
fbcon mouse/cursor paths inconsistent, causing ONS cursor invisibility or bad
click positions. Fix SDL 1.2 fbcon mouse/cursor rotation first if landscape is
needed again.

Temporary ONS software cursor overlay was removed and should not be restored
unless explicitly requested.
```

### ST7701 Visible Area And SDL Viewport Rule - 2026-06-16
```text
Kernel/DRM currently reports fb0 as 384x640, but the actual visible LCD area is
360x640. Do not blindly treat all 384 horizontal pixels as visible content.

Display adaptation order:
1. First fit output to the actual visible physical pixel area.
   Portrait visible area: 360x640 inside fb 384x640.
   Default portrait safe viewport: x=12, y=0, w=360, h=640.
2. Then choose each app/emulator's recommended logical/content resolution and
   scale or center it inside the safe viewport.

For landscape software presentation, the physical visible area should be treated
as 640x360. Recommended common content area:
480x360 centered in 640x360, with x=80, y=0.

Recommended logical/content resolutions:
- gmenu2x: 480x360 centered, because its original UI style is close to 4:3.
- ONS: 480x360 centered for 640x480/800x600 style games.
- SNES/SFC: 480x360 centered for 4:3 presentation.
- MD/Genesis: 480x360 by default; consider wider only if a core needs it.
- PS1: 480x360 centered for common 4:3 modes.
- GBA: 480x320 centered inside 640x360, x=80, y=20.
- GB/GBC: centered smaller viewport such as 360x324 or 320x288.
- Image viewers and video players: use the real landscape visible resolution
  640x360 as the target output area. For media playback, preserve aspect ratio
  when possible, but the target canvas/viewport should be 640x360 rather than
  the emulator-oriented 480x360 area.

Rule: first solve real visible pixels and clipping, then solve aspect ratio and
recommended emulator/app resolution. Do not stretch every app to 640x360 or
384x640 unless explicitly testing raw framebuffer output.

Required runtime behavior:
```text
SDL/app display profiles must be switched dynamically per application.
Do not use one fixed global logical resolution for all apps.

The launcher script for each app should select a display profile before exec:
- menu_4_3: gmenu2x and most 4:3 emulators, 480x360 centered in 640x360.
- gba: 480x320 centered in 640x360 at x=80, y=20.
- media_16_9: image viewers and video players, target full visible 640x360.
- portrait_safe: portrait-only tools, 360x640 inside fb 384x640 at x=12, y=0.

The implementation should be centralized in the shared SDL/app environment
instead of hardcoding unrelated geometry in each emulator. Per-app launchers
choose the profile; shared runtime code applies safe viewport, scaling, and
centering.
```

Kernel panel timing requirement:
```text
User wants to try different RGB ST7701 panels by changing only device tree.
Use fixed panel modes in panel-simple and switch them from DTS by changing the
panel compatible string. Do NOT convert this to `panel-dpi` unless user asks.

Current kernel timing is hardcoded through panel-simple compatible
`lattland,mostima` in:
drivers/gpu/drm/panel/panel-simple.c

Current hardcoded mode:
384x640 @ 24MHz
hfp=60 hsync=6 hbp=78
vfp=16 vsync=4 vbp=9

Required approach:
1. Add additional fixed `drm_display_mode` + `panel_desc` entries to
   `drivers/gpu/drm/panel/panel-simple.c`.
2. Add matching `of_device_id` compatible strings.
3. Switch screen timing in DTS by changing:
   `compatible = "lattland,mostima", "simple-panel";`
   to the matching compatible, for example a future 480x640 compatible.
4. Keep ST7701 init sequence in DTS under `st7701initseq`.

Detailed notes and candidate DTS snippets are saved locally:
C:\Users\26301\Desktop\F1C200S_ST7701_timing_notes.md
```
```

## RTL8723BU Bluetooth
Current Bluetooth enablement status:
```text
Date: 2026-06-15
Purpose: enable RTL8723BU Bluetooth userland and runtime helpers.

Kernel config already enabled:
CONFIG_BT=y
CONFIG_BT_BREDR=y
CONFIG_BT_LE=y
CONFIG_BT_RTL=y
CONFIG_BT_HCIBTUSB=y
CONFIG_BT_HCIBTUSB_RTL=y
CONFIG_RFKILL=y

Buildroot config enabled:
BR2_PACKAGE_DBUS=y
BR2_PACKAGE_BLUEZ5_UTILS=y
BR2_PACKAGE_BLUEZ5_UTILS_CLIENT=y
BR2_PACKAGE_BLUEZ5_UTILS_DEPRECATED=y
BR2_PACKAGE_UTIL_LINUX_RFKILL=y
BR2_PACKAGE_UTIL_LINUX_LIBSMARTCOLS=y
```

Host backups:
```text
~/LicheePi_Nano/backups/buildroot_bluetooth_20260615_144648
~/LicheePi_Nano/backups/rtl8723_bt_overlay_fw_20260615_000120
```

Buildroot products verified in target:
```text
/usr/bin/bluetoothctl
/usr/bin/btmon
/usr/bin/hciconfig
/usr/bin/hcitool
/usr/bin/dbus-daemon
/usr/libexec/bluetooth/bluetoothd
/usr/sbin/rfkill
/etc/init.d/S30dbus
/root/start_bluetooth.sh
/root/check_bluetooth.sh
```

Firmware note:
```text
Kernel drivers/bluetooth/btrtl.c requests `rtl_bt/rtl8723b_fw.bin` for RTL8723B.
The correct current file in both target and runtime overlay is 45048 bytes:
/lib/firmware/rtl_bt/rtl8723b_fw.bin

Do not blindly replace it with `third_party/rtl8723bu/rtl8723bu_bt.bin` unless
board dmesg proves the 45048-byte file is rejected. That file was 9120 bytes and
is not the filename requested by btrtl.

Missing `rtl8723b_config.bin` may be logged but is usually non-fatal unless the
kernel says config is required.
```

Runtime overlay scripts:
```text
~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/root/start_bluetooth.sh
~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/root/check_bluetooth.sh
```

GMenu2X integration:
```text
`install_f1c200s_sdl_apps.sh` adds an Applications entry named `Bluetooth`
that runs `/root/start_bluetooth.sh`.
```

Build commands:
```sh
cd ~/LicheePi_Nano/buildroot-2018.02.11
/usr/bin/make util-linux-dirclean
/usr/bin/make util-linux -j8
/usr/bin/make -j8
```

Board-side tests when available:
```sh
dmesg | grep -iE 'bluetooth|btusb|btrtl|rtl.*bt|8723|firmware|hci|rfkill'
ls -l /sys/class/bluetooth /sys/class/rfkill
/root/check_bluetooth.sh
/root/start_bluetooth.sh
hciconfig -a
bluetoothctl show
bluetoothctl scan on
```

Troubleshooting:
```text
If there is no hci0, inspect USB enumeration and btusb binding first:
lsusb
find /sys/bus/usb/devices -maxdepth 2 -type f -name idVendor -o -name idProduct

dmesg | grep -iE 'usb|btusb|bluetooth|8723|rtl'

Likely causes:
1. The RTL8723BU board exposes WiFi but not Bluetooth on USB.
2. btusb does not match this module's USB interface ID and needs an ID added.
3. Firmware failed; check exact requested filename in dmesg.
```

## GMenu2X Network Scan Entries
Current status:
```text
Date: 2026-06-15
Purpose: add gmenu2x Applications entries that scan and display nearby WiFi APs
and Bluetooth devices directly on the device screen.
```

Runtime scripts:
```text
/root/scan_wifi.sh
/root/scan_bluetooth.sh
```

Host overlay files:
```text
~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/root/scan_wifi.sh
~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/root/scan_bluetooth.sh
~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/root/install_f1c200s_sdl_apps.sh
```

GMenu2X entries generated by `install_f1c200s_sdl_apps.sh`:
```text
Applications/WiFi Scan -> /root/scan_wifi.sh
Applications/BT Scan   -> /root/scan_bluetooth.sh
Applications/Bluetooth -> /root/start_bluetooth.sh
```

Behavior:
```text
WiFi Scan uses `/sbin/iwlist wlan0 scan`, formats SSID/quality/signal/encryption,
then waits for ENTER before returning to gmenu2x.

BT Scan starts Bluetooth through `/root/start_bluetooth.sh`, runs a short
`bluetoothctl` scan, formats discovered devices, then waits for ENTER before
returning to gmenu2x.

These are scan/list display tools only. They do not yet select and connect to a
WiFi AP or pair/connect a Bluetooth device from the list.
```

Host backup:
```text
~/LicheePi_Nano/backups/gmenu2x_network_scan_20260615_000713
```

Deploy/sync commands:
```sh
cd ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle
./sync_overlay_to_buildroot.sh
ROOT_PREFIX=~/LicheePi_Nano/buildroot-2018.02.11/output/target \
  rootfs_overlay/root/install_f1c200s_sdl_apps.sh
./collect_runtime_payload.sh
./install_payload_to_target.sh
```

Board-side direct tests:
```sh
/root/scan_wifi.sh
/root/scan_bluetooth.sh
```

## RTL8723BU WiFi throughput tune - 2026-06-15
Current status:
```text
Purpose: improve USB RTL8723BU WiFi throughput after observed scp speed around 300KB/s.

Host source changed:
~/LicheePi_Nano/third_party/rtl8723bu/Makefile
~/LicheePi_Nano/third_party/rtl8723bu/os_dep/os_intfs.c

Runtime scripts changed:
~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/etc/init.d/S17rtl8723bu
~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/root/check_wifi_speed.sh
~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/collect_runtime_payload.sh
~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/install_payload_to_target.sh

Local Windows backup/work dir:
C:\Users\26301\Desktop\F1C200S_rtl8723bu_perf_20260615

Host backup:
~/LicheePi_Nano/backups/rtl8723bu_perf_tune_20260615_151651
```

Driver tuning:
```text
Makefile:
- use -O2 instead of driver-forced -O1
- remove -g from module build flags
- disable CONFIG_CONCURRENT_MODE for single STA use
- disable CONFIG_POWER_SAVING at compile time
- keep CONFIG_BT_COEXIST=y because RTL8723BU Bluetooth is required and coexist handling must remain available
- disable CONFIG_TRAFFIC_PROTECT
- set CONFIG_RTW_ADAPTIVITY_EN=disable
- platform branch set to CONFIG_PLATFORM_FS_MX61=y so it builds against ~/LicheePi_Nano/linux instead of host /lib/modules

os_dep/os_intfs.c defaults:
- rtw_smart_ps=0
- rtw_usb_rxagg_mode=1  (DMA aggregation)
- rtw_ampdu_enable=2    (force AMPDU)
```

Build command used successfully:
```sh
cd ~/LicheePi_Nano/third_party/rtl8723bu
make clean
make KERNEL_SRC=~/LicheePi_Nano/linux ARCH=arm CROSS_COMPILE=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi- -j8
make CROSS_COMPILE=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi- strip
```

Built product:
```text
~/LicheePi_Nano/third_party/rtl8723bu/8723bu.ko
size: 921K
vermagic: 5.7.1 mod_unload ARMv5 p2v8
installed into Buildroot target:
~/LicheePi_Nano/buildroot-2018.02.11/output/target/lib/modules/5.7.1/extra/8723bu.ko
```

Runtime module options now used by S17rtl8723bu:
```text
rtw_power_mgnt=0
rtw_ips_mode=0
rtw_smart_ps=0
rtw_low_power=0
rtw_enusbss=0
rtw_btcoex_enable=1
rtw_ht_enable=1
rtw_bw_mode=0x21
rtw_wmm_enable=1
rtw_ampdu_enable=2
rtw_usb_rxagg_mode=1
rtw_wifi_spec=0
```

Runtime integration commands already run:
```sh
cd ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle
./sync_overlay_to_buildroot.sh
./collect_runtime_payload.sh
./install_payload_to_target.sh
```

Board-side deploy/test when online:
```sh
# Deploy through existing runtime payload flow or rebuild/rootfs flash as appropriate.
# Do not restart WiFi over SSH unless user explicitly accepts losing the session.

/root/check_wifi_speed.sh
cat /sys/module/8723bu/parameters/rtw_power_mgnt
cat /sys/module/8723bu/parameters/rtw_usb_rxagg_mode
cat /sys/module/8723bu/parameters/rtw_ampdu_enable
iwconfig wlan0
```

Raw throughput test, avoiding scp encryption overhead:
```sh
# On host:
nc -l -p 5001 > /dev/null

# On board:
/root/check_wifi_speed.sh wlan0 192.168.175.135 5001 16
```

Important interpretation:
```text
If raw nc throughput is much higher than scp, bottleneck is encryption/CPU.
If raw nc is still around 300KB/s, inspect /root/check_wifi_speed.sh output for USB speed, iwconfig Bit Rate, signal level, drops/errors, and dmesg rxagg/ampdu messages.
If link rate is low or stuck in 11b/g rates, AP/channel/signal/rate negotiation is likely the remaining bottleneck rather than scp.
```

## RTL8733BU USB WiFi status - 2026-07-01
Current status:
```text
User tested additional USB Realtek adapters beyond the original RTL8723BU.

Observed USB IDs:
- 0bda:b720 -> RTL8723BU path, handled by `third_party/rtl8723bu`
- 0bda:b733 -> RTL8733BU / RTL8733B path, handled by `third_party/rtl8733bu_local/RTL8733BU-5.15.12-126-wb`
- 0bda:d723 -> appears in rtl8733bu source table as RTL8723D-family USB ID

Current working tree:
~/LicheePi_Nano/third_party/rtl8733bu_local/RTL8733BU-5.15.12-126-wb

Important runtime scripts on board / overlay:
- /etc/init.d/S17rtl8723bu
- /etc/init.d/S45usb-wifi

Current S17 behavior:
- non-blocking
- does not try SDIO 8723ds on USB path
- VID:PID-selective load:
  * 0bda:b720 -> 8723bu
  * 0bda:d723 / 0bda:b733 / 0bda:f72b -> 8733bu

Current S45 behavior:
- if `wlan0` does not appear, log and exit 0
- do not block boot if no supported USB WiFi is present
```

Source conclusion:
```text
Previous `8733bu` build on F1C200S kernel 5.7.1 could create `wlan0`,
but `iwlist wlan0 scan` returned no scan results and `wpa_state=SCANNING`.

Source audit showed:
- `CONFIG_PLATFORM_WB=y` was active
- under that branch, transmit-side `CONFIG_USE_USB_BUFFER_ALLOC_TX` was NOT enabled
- receive path already used coherent USB buffers
- transmit path only switches to coherent USB buffers when
  `CONFIG_USE_USB_BUFFER_ALLOC_TX` is defined

This matches the symptom: interface enumerates, but active scan TX likely fails.
Do NOT enable legacy `CONFIG_PLATFORM_ARM_SUNxI` just to get this macro; that
pulls in old Allwinner private platform power code which is not the desired path.
```

Patch and rebuild:
```text
Local edited file:
C:\Users\26301\Desktop\_codex_wifi_pkg\rtl8733bu_scanfix\Makefile

Host edited file:
~/LicheePi_Nano/third_party/rtl8733bu_local/RTL8733BU-5.15.12-126-wb/Makefile

Change applied under `CONFIG_PLATFORM_WB`:
ifeq ($(CONFIG_USB_HCI), y)
EXTRA_CFLAGS += -DCONFIG_USE_USB_BUFFER_ALLOC_TX
endif

Host backup before patch:
~/LicheePi_Nano/backups/rtl8733bu_scanfix_20260701_070407

Rebuild command used successfully:
cd ~/LicheePi_Nano/third_party/rtl8733bu_local/RTL8733BU-5.15.12-126-wb
make clean
make -j8 KSRC=~/LicheePi_Nano/linux ARCH=arm \
  CROSS_COMPILE=~/LicheePi_Nano/buildroot-2018.02.11/output/host/bin/arm-buildroot-linux-gnueabi-

Rebuilt module:
~/LicheePi_Nano/third_party/rtl8733bu_local/RTL8733BU-5.15.12-126-wb/8733bu.ko
md5: 03d2a13be9c939d9ad064de8cc4a3c5d

Local copy to deploy:
C:\Users\26301\Desktop\_codex_wifi_pkg\8733bu.ko
md5: 03d2a13be9c939d9ad064de8cc4a3c5d
```

Next board-side validation when online:
```text
1. Replace board `/lib/modules/5.7.1/extra/8733bu.ko` with the rebuilt module above.
2. Reload only `8733bu` for the 0bda:b733 adapter.
3. Re-test:
   - `iwlist wlan0 scan`
   - `wpa_cli -i wlan0 status`
   - `dmesg | grep -iE '8733|wlan|scan|auth|assoc'`
4. Only if scan still fails, inspect `/proc/net` / proc-debug entries from this driver.
```

Deployment update - 2026-07-01:
```text
Board currently online on the legacy RTL8723BU adapter:
- USB ID: 0bda:b720
- do not reload this live adapter path

Deployed the rebuilt 8733 module to the board:
- /lib/modules/5.7.1/extra/8733bu.ko
- md5: 03d2a13be9c939d9ad064de8cc4a3c5d

Verified on board:
- existing live adapter remains 0bda:b720
- live network was left untouched
- new 8733 module is staged for the next 0bda:b733 / 0bda:d723 / 0bda:f72b test
```

Wirenboard 8733 route - 2026-07-01:
```text
User redirected work to the newer Wiren Board style RTL8733BU driver route,
based on the referenced article about a complete 8733 USB port.

Current preferred source tree:
~/LicheePi_Nano/third_party/wirenboard_rtl8733bu

Current preferred module aliases from modinfo:
- 0bda:b733
- 0bda:f72b

Important correction:
- `0bda:d723` is not part of this preferred 8733 path.
- Keep `0bda:d723` on the separate 8723DU investigation path if revisited.

Host backup before switching Buildroot/runtime integration:
~/LicheePi_Nano/backups/rtl8733bu_wirenboard_switch_20260701_082439

Files changed in this step:
- ~/LicheePi_Nano/third_party/wirenboard_rtl8733bu/Makefile
- ~/LicheePi_Nano/buildroot-2018.02.11/package/rtl8733bu.mk
- ~/LicheePi_Nano/buildroot-2018.02.11/package/rtl8733bu/rtl8733bu.mk
- ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/etc/init.d/S17rtl8723bu

Source/path changes:
- Buildroot rtl8733bu package source now points to:
  $(TOPDIR)/../third_party/wirenboard_rtl8733bu
- Runtime S17 loader now keeps USB-ID based selection:
  * 0bda:b720 -> 8723bu
  * 0bda:d723 -> 8723du
  * 0bda:b733 / 0bda:f72b -> 8733bu

Makefile change in the Wiren Board tree:
- under `CONFIG_PLATFORM_WB7`, add:
  ifeq ($(CONFIG_USB_HCI), y)
  EXTRA_CFLAGS += -DCONFIG_USE_USB_BUFFER_ALLOC_TX
  endif

Reason:
- keep the clean WB7 cfg80211 platform path
- also force USB TX buffer allocation on this F1C200S MUSB host path

Rebuild command used successfully:
cd ~/LicheePi_Nano/third_party/wirenboard_rtl8733bu
make clean
make -j8 KSRC=~/LicheePi_Nano/linux ARCH=arm \
  CROSS_COMPILE=~/LicheePi_Nano/buildroot-2018.02.11/output/host/bin/arm-buildroot-linux-gnueabi-

Current preferred rebuilt module:
~/LicheePi_Nano/third_party/wirenboard_rtl8733bu/8733bu.ko
md5: 2c8433181264125e2ae43fd7284e3f7a
version: v5.13.0.1-112-g10248f4f3.20230626_COEX20230616-330e

Local deploy copy:
C:\Users\26301\Desktop\_codex_wifi_pkg\8733bu_wb.ko
md5: 2c8433181264125e2ae43fd7284e3f7a

Practical rule after resume:
- For 8733 USB testing, prefer this `wirenboard_rtl8733bu` build first.
- Do not reload WiFi on the live old `0bda:b720` adapter just to test packaging.
- Stage/copy the new module first, then only validate after the user inserts
  a `0bda:b733` or `0bda:f72b` adapter.

Board-side validation update:
- With `0bda:b720` and `0bda:b733` inserted together, the rebuilt
  `wirenboard_rtl8733bu` module loads successfully on the board and creates:
  * `wlan1`
  * `wlan2`
- Driver log confirms:
  * `CHIP TYPE: RTL8733B`
  * `rtl8733bu v5.13.0.1-112-g10248f4f3.20230626_COEX20230616-330e`
  * `rtw_ndev_init(wlan1)`
  * `rtw_ndev_init(wlan2)`

Critical runtime bug found and fixed:
- Previous `/etc/init.d/S17rtl8723bu` exited immediately if `wlan0` already
  existed.
- In a dual-adapter case, that prevented loading `8733bu` after `8723bu`
  created `wlan0`.
- Fix:
  * add `module_loaded()` helper
  * remove the global `have_iface && exit 0` short-circuit
  * load each driver independently by USB ID only if its module is not already
    loaded

Result after script fix:
- `8723bu` and `8733bu` can coexist.
- Current board state verified:
  * `wlan0` = live legacy 8723bu network
  * `wlan1/wlan2` = rtl8733bu interfaces from the b733 adapter

Caveat:
- Current `/etc/init.d/S45usb-wifi` still targets only:
  `IFACE=\"wlan0\"`
- So the new 8733 adapter now loads automatically, but it is not yet the
  default interface for auto-connect unless S45 is intentionally reworked for
  `wlan1` or multi-interface selection.

Single-8733 regression log and follow-up - 2026-07-01:
```text
User later tested with only the 0bda:b733 adapter inserted.

Important captured board log:
- `8733bu` loaded and took `wlan0`
- scan started through cfg80211:
  `cfg80211_rtw_scan(wlan0)`
- then USB transport failed during scan:
  `usb_read_port_complete => urb.status(-71)`
  `urb_write_port_complete status(-110)`
  `bSurpriseRemoved=TRUE`

This means:
- the driver is not failing at enumeration or firmware load
- the crash point is active scan / USB transaction stability

A first attempt to force a full STA-only build by disabling:
- AP_MODE
- P2P
- BR_EXT
- WIFI_MONITOR
- CONCURRENT_MODE
did NOT work; the tree no longer compiled cleanly because upstream code still
assumed those symbols/types in several paths.

Current narrower mitigation that DOES build:
1. Keep the previous working feature baseline.
2. Change only:
   - `CONFIG_BT_COEXIST = n`
   - in `os_dep/linux/os_intfs.c`, default
     `rtw_virtual_iface_num = 0` for the <=2 iface case
3. Keep the F1C200S WB7 USB TX buffer patch in place.

Reason:
- remove Bluetooth coexistence logic from the USB WiFi path
- stop creating the extra virtual concurrent interface by default
- keep the rest of the vendor tree close to the already-booting baseline

Backup before this narrowing step:
~/LicheePi_Nano/backups/rtl8733bu_sta_only_20260701_085043

Current rebuilt module after the narrower mitigation:
~/LicheePi_Nano/third_party/wirenboard_rtl8733bu/8733bu.ko
md5: b5273a5491264c14cde6d23600e00556

Local deploy copy:
C:\Users\26301\Desktop\_codex_wifi_pkg\8733bu_wb.ko
md5: b5273a5491264c14cde6d23600e00556

Next target-side test when board access is restored:
1. Replace board `/lib/modules/5.7.1/extra/8733bu.ko`
2. Boot with only the `0bda:b733` adapter
3. Verify whether only one wlan interface is created
4. Re-test scan and watch for:
   - `urb.status(-71)`
   - `status(-110)`
   - `bSurpriseRemoved=TRUE`
```
```

RTL8733BU local stabilization prep - 2026-07-02:
```text
Because the board fell back into "SSH port open but no banner" and serial was
not in a clean shell state, the next mitigation round was prepared locally but
not yet rebuilt/deployed.

Local files updated:
- C:\Users\26301\Desktop\_codex_wifi_pkg\overlay\etc\init.d\S17rtl8723bu
- C:\Users\26301\Desktop\_codex_wifi_pkg\wirenboard_rtl8733bu\Makefile
- C:\Users\26301\Desktop\_codex_wifi_pkg\wirenboard_rtl8733bu\os_dep\linux\os_intfs.c
- C:\Users\26301\Desktop\_codex_wifi_pkg\wirenboard_rtl8733bu\os_intfs.c

Exact local changes:
1. S17 `USB8733_OPTS`
   - `rtw_usb_rxagg_mode=0` (was 1 in the latest intended overlay)
   - keep:
     `rtw_dynamic_agg_enable=0`
     `rtw_en_napi=0`
     `rtw_en_gro=0`
     `rtw_en_dyn_rrsr=0`
     `rtw_rrsr_value=0`
2. Source defaults in `os_intfs.c`
   - `rtw_usb_rxagg_mode = 0`
   - `rtw_dynamic_agg_enable = 0`
   - `rtw_wifi_spec = 1`
   This is meant to keep the driver on the same conservative baseline even if
   runtime module parameters do not land cleanly on the board.
3. Makefile feature tightening
   - `CONFIG_RTW_NAPI = n`
   - `CONFIG_RTW_GRO = n`
   - `CONFIG_RTW_NETIF_SG = n`
4. WB7 USB path tweak in Makefile
   - under `CONFIG_PLATFORM_WB7` + `CONFIG_USB_HCI`, enable:
     `-DCONFIG_USE_USB_BUFFER_ALLOC_RX`
     `-DCONFIG_USE_USB_BUFFER_ALLOC_TX`
   The earlier WB7 patch only forced TX buffer alloc. This round also enables
   RX buffer alloc, because the active failure signature is RX/USB side:
   `urb.status(-71)`, `urb.status(-110)`, `RXFF maybe hang`, surprise remove.

Important caution:
- The first edit accidentally placed `CONFIG_USE_USB_BUFFER_ALLOC_RX` into the
  unrelated `CONFIG_PLATFORM_ARM_SUN50IW1P1` block. That was corrected in the
  local file. The intended active platform change is only the WB7 block.

Next execution plan when host/board access is clean again:
1. sync these local edits back into the host tree
2. rebuild `wirenboard_rtl8733bu/8733bu.ko`
3. verify new module md5
4. deploy with `dd ... conv=fsync` to `/lib/modules/5.7.1/extra/8733bu.ko`
5. replace board `/etc/init.d/S17rtl8723bu` with the matching conservative copy
6. cold boot with only the `0bda:b733` adapter first
7. re-test scan / association before reintroducing the older 8723 adapter
```

RTL8733BU deployment + board verification - 2026-07-02:
```text
Host rebuild completed after syncing the local conservative changes.

Host rebuilt module:
- ~/LicheePi_Nano/third_party/wirenboard_rtl8733bu/8733bu.ko
- md5: 4134a36494708e8fcff71d31ca07f697
- size: 2861460 bytes

Board backup created:
- /root/roms/runtime_backups/rtl8733bu_rx_tighten_19700101_000458

Important board-side runtime findings:
1. Board `/etc/init.d/S17rtl8723bu` had in fact been corrupted by an earlier
   noisy serial write. It was replaced with the clean local copy.
2. New module was deployed successfully, but writing it into
   `/lib/modules/5.7.1/extra/8733bu.ko` with:
   - `dd if=... of=... bs=1M conv=fsync`
   again produced a truncated/corrupt target on this board.
3. Reliable deployment method for this board in the current state was:
   - ensure `/tmp/8733bu_new.ko` md5 is correct
   - then:
     `rm -f /lib/modules/5.7.1/extra/8733bu.ko`
     `cat /tmp/8733bu_new.ko > /lib/modules/5.7.1/extra/8733bu.ko`
     `sync`
   After that:
   - board `/lib/modules/5.7.1/extra/8733bu.ko`
     md5 = 4134a36494708e8fcff71d31ca07f697
   So for this board, prefer `cat > target` over `dd` for this specific large
   module replacement path.

Cleaned runtime script note:
- `rtw_en_napi` and `rtw_en_gro` are not valid `8733bu` module parameters for
  this driver build and are logged as:
  `8733bu: unknown parameter 'rtw_en_napi' ignored`
  `8733bu: unknown parameter 'rtw_en_gro' ignored`
- They were removed from the local/host/board `S17rtl8723bu` script after this
  test round.

Live board test state during verification:
- USB devices present:
  * 0bda:b720
  * 0bda:b733
- old `8723bu` remained on `wlan0` and kept SSH alive
- new `8733bu` loaded successfully and created `wlan1`

What improved:
- The new `8733bu` no longer immediately collapsed into the older observed
  `urb.status(-71)` / `urb.status(-110)` / `bSurpriseRemoved=TRUE` pattern
  during first load on this board.
- It can enumerate, download firmware, register cfg80211, and expose `wlan1`
  while `8723bu` keeps `wlan0` online.

What still fails:
1. `wlan1` must be brought up first (`ip link set wlan1 up`), otherwise scan is
   a no-op with:
   - `bup=0`
   - `hw_init_completed=0`
2. After `wlan1` is up, `iwlist wlan1 scan` no longer crashes the whole device,
   but it still returns:
   - `wlan1 No scan results`
3. Kernel log after real scan still shows the core remaining blocker:
   - `RTW: rtw_scan_timeout_handler(wlan1) fw_state=808`
   - `RTW: ERROR RXFF maybe hang, trigger silent reset to recover`
   - repeated `usb_read_port_complete() RX Warning! bDriverStopped(False) OR bSurpriseRemoved(False)`
   - followed by internal `rtl8733bu_deinit` / `rtl8733bu_init` recovery

Current diagnosis after this round:
- This is now narrowed to an RX/scan-path instability after interface open,
  not an enumeration failure and not the old immediate surprise-remove path.
- The conservative compile/runtime changes helped, but did not eliminate the
  RXFF hang during active scan.
```

RTL8188FTV / 8188FU mapping - 2026-07-02:
```text
User checked the chip silk and confirmed the extra USB WiFi dongle is
`RTL8188FTV`, not 8188EU.

Important consequence:
- Linux kernel built-in `CONFIG_R8188EU` / module `r8188eu` is not the correct
  driver for this dongle.
- In the existing Realtek out-of-tree source, `RTL8188FTV` should be handled by
  the `CONFIG_RTL8188F` USB path, module name `8188fu`.

Verified in current host source:
- `~/LicheePi_Nano/third_party/wirenboard_rtl8733bu/Makefile`
  * `CONFIG_RTL8188F`
  * `MODULE_NAME = 8188fu`
- `~/LicheePi_Nano/third_party/wirenboard_rtl8733bu/os_dep/linux/usb_intf.c`
  contains:
  * `0xF179 -> RTL8188F`  (`8188FU 1*1`)

Also verified:
- host kernel source still has `drivers/staging/rtl8188eu/Kconfig`
  with `CONFIG_R8188EU`, but current `.config` has:
  * `# CONFIG_R8188EU is not set`
  This built-in kernel option is unrelated to the confirmed `RTL8188FTV` dongle
  and should not be treated as the primary target for this adapter.

Local runtime overlay preparation completed:
- `C:\Users\26301\Desktop\_codex_wifi_pkg\overlay\etc\init.d\S17rtl8723bu`
  now has:
  * `USB8188F_OPTS=...`
  * USB ID match for `0bda:f179`
  * module load target:
    `/lib/modules/*/extra/8188fu.ko`

Next 8188FTV steps when continuing on host:
1. create a dedicated 8188FU build path from the current Realtek source
2. build `8188fu.ko`
3. stage/install it into target extra modules
4. extend runtime payload scripts if needed so `8188fu.ko` is copied with the
   rest of the WiFi module set
5. board test with `lsusb` confirming `0bda:f179`
```

RTL8188FU standalone repo integration - 2026-07-02:
```text
User requested referencing:
https://github.com/kelebek333/rtl8188fu

Decision:
- Do not force RTL8188FTV into the existing 8733-specific build artifact.
- Use the standalone `kelebek333/rtl8188fu` tree as an independent driver line.

Host source path prepared:
- ~/LicheePi_Nano/third_party/rtl8188fu_kelebek333

Key verified facts from that repo:
- module name: `rtl8188fu`
- firmware file: `firmware/rtl8188fufw.bin`
- README recommends runtime options:
  `rtw_power_mgnt=0 rtw_enusbss=0 rtw_ips_mode=0`
- upstream alias note confirms the expected USB ID:
  `0BDA:F179`

Host build result against current F1C200S kernel tree:
- command style used:
  `make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- KSRC=~/LicheePi_Nano/linux KVER=5.7.1 -j4`
- build passed with:
  `BUILD_RC=0`
- output module:
  `~/LicheePi_Nano/third_party/rtl8188fu_kelebek333/rtl8188fu.ko`
- md5:
  `35b262f43ae7d4a7809c3a9bc9c89dbf`
- size:
  `1094340` bytes

Local Windows copies saved:
- `C:\Users\26301\Desktop\_codex_wifi_pkg\rtl8188fu.ko`
  md5 `35b262f43ae7d4a7809c3a9bc9c89dbf`
- `C:\Users\26301\Desktop\_codex_wifi_pkg\rtl8188fufw.bin`
  md5 `62e540665cf25e682864c1ef67b893ba`

Runtime bundle integration completed on host:
1. `collect_runtime_payload.sh`
   now copies:
   - `rtl8188fu.ko`
   - `rtl8188fufw.bin`
2. `install_payload_to_target.sh`
   now installs:
   - `/lib/modules/<ver>/extra/rtl8188fu.ko`
   - `/lib/firmware/rtlwifi/rtl8188fufw.bin`
3. `rootfs_overlay/etc/init.d/S17rtl8723bu`
   now recognizes:
   - USB ID `0bda:f179`
   - module name `rtl8188fu`
   - module file `/lib/modules/*/extra/rtl8188fu.ko`
   - runtime opts:
     `rtw_power_mgnt=0 rtw_enusbss=0 rtw_ips_mode=0`

Payload verification on host:
- `board_tools_f1c200s/runtime_bundle/collect_runtime_payload.sh` passed
- `board_tools_f1c200s/runtime_bundle/install_payload_to_target.sh` passed
- payload artifact md5 matches source:
  `payload/rtl8188fu.ko -> 35b262f43ae7d4a7809c3a9bc9c89dbf`

Board-side future verification checklist when hardware is back:
1. `lsusb` must show `0bda:f179`
2. confirm files exist:
   - `/lib/modules/5.7.1/extra/rtl8188fu.ko`
   - `/lib/firmware/rtlwifi/rtl8188fufw.bin`
3. run `/etc/init.d/S17rtl8723bu restart`
4. confirm module:
   - `grep rtl8188fu /proc/modules`
5. bring interface up and test:
   - `ip link`
   - `iwlist wlanX scan`

Important loader naming correction:
- For the standalone repo the right module name is `rtl8188fu`, not `8188fu`.
- Keep this distinction clear; the earlier quick local draft used the shorter
  name before the repo was inspected.
```


## ROM HTTP Server - 2026-06-15
Current status:
```text
Purpose: expose only /root/roms over LAN browser for large ROM upload/download,
avoiding SCP encryption overhead on the F1C200S CPU.

Local Windows backup/work dir:
C:\Users\26301\Desktop\F1C200S_http_upload_20260615

Host source:
~/LicheePi_Nano/board_tools_f1c200s/rom_httpd.c

Host binary:
~/LicheePi_Nano/board_tools_f1c200s/rom_httpd

Runtime files:
/root/rom_httpd
/root/run_rom_httpd.sh
```

Design:
```text
- Single small C program, no Python/Node/new Buildroot package needed.
- Default root is /root/roms only.
- Default port is 8080.
- Browser UI supports directory browsing, file download, file upload with PUT,
  and file delete inside /root/roms.
- Upload is streamed to disk and does not load full ROM into RAM.
- Do not expose this service to the internet; LAN use only.
```

Build command used:
```sh
cd ~/LicheePi_Nano/board_tools_f1c200s
~/LicheePi_Nano/buildroot-2018.02.11/output/host/bin/arm-buildroot-linux-gnueabi-gcc -Os -s -Wall -Wextra -o rom_httpd rom_httpd.c
```

Integrated through standard runtime flow:
```sh
cd ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle
./sync_overlay_to_buildroot.sh
./collect_runtime_payload.sh
./install_payload_to_target.sh
```

GMenu2X entry:
```text
Applications/ROM Server -> /root/run_rom_httpd.sh
```

Board-side use:
```sh
/root/run_rom_httpd.sh
# Then open on PC browser:
# http://172.28.73.247:8080/
```

Direct manual start:
```sh
/root/rom_httpd /root/roms 8080
```

Stop server:
```sh
kill $(cat /tmp/rom_httpd.pid)
```

## ST7701 Display Repo Sync - 2026-06-15
Current status:
```text
Purpose: continue fixing abnormal ST7701 RGB display after reset wire was fixed.
User requirement: follow rhodesepass/buildroot same-screen repo; DE must be fixed high on PD19; do not reboot board automatically.

Reference repo:
C:\Users\26301\Desktop\repo_tmp\rhodesepass_buildroot

Host backup before source changes:
~/LicheePi_Nano/backups/st7701_display_repo_sync_20260615_204957

Local Windows work copy / deployed artifacts:
C:\Users\26301\Desktop\F1C200S_ST7701_display_work
```

Changes applied to host kernel:
```text
~/LicheePi_Nano/linux/drivers/gpu/drm/sun4i/sun4i_tcon.h
- Added SUN4I_TCON0_CTL_SWAP BIT(23) from rhodesepass patch 0004.

~/LicheePi_Nano/linux/drivers/gpu/drm/sun4i/sun4i_tcon.c
- Added srgn,swap-b-r property support from rhodesepass patch 0004.
- HSD DTS does not currently set srgn,swap-b-r; this only adds support and does not force color swap.

~/LicheePi_Nano/linux/drivers/video/fbdev/core/fbcon.c
- Added SRGNVS8PIX_HACK from rhodesepass patch 0011.
- vc_resize calls now go through vc_resize_hack(width - 3, height).
```

Build/deploy:
```sh
cd ~/LicheePi_Nano/linux
export PATH=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin:$PATH
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- -j8
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- suniv-f1c100s-licheepi-nano.dtb -j8
```

Board deployment completed:
```text
Deployed to /mnt/boot/zImage and /mnt/boot/suniv-f1c100s-licheepi-nano.dtb
Boot partition backups:
/mnt/boot/zImage.before_display_repo_sync_20260615_205332
/mnt/boot/suniv-f1c100s-licheepi-nano.dtb.before_display_repo_sync_20260615_205332
```

Important observations before reboot:
```text
DRM connector reports only mode 384x640:
/sys/class/drm/card0-Unknown-1/modes = 384x640

fb0 still reported virtual_size 320,480 while stride was 1536 (=384*4), so there may be fbdev/user-space visible-size state separate from connector mode.
ST7701 init logs completed successfully:
srgn:init seq count=313
srgn: st7701 init work handler running
srgn: st7701 init (work) completed
```

Next board test after user reboots:
```sh
uname -a
cat /sys/class/drm/card0-Unknown-1/modes
cat /sys/class/graphics/fb0/virtual_size
cat /sys/class/graphics/fb0/stride
cat /sys/class/graphics/fb0/bits_per_pixel
dmesg | grep -iE 'srgn|st7701|panel|drm|sun4i|tcon|fb|lcd|gpio|pinctrl'
```

If display is still abnormal:
```text
1. Confirm whether fb0 virtual_size is still 320,480 or now matches 384,640.
2. If only colors are wrong, consider adding srgn,swap-b-r only after visual evidence.
3. If geometry/tearing is wrong, continue comparing current sun4i DRM/TCON/debe patches against rhodesepass 0003/0007, not the ST7701 init table first.
4. Do not restart WiFi from SSH and do not reboot automatically.
```

## ST7701 Display Rollback - 2026-06-15
```text
User reported the display became completely blank after deploying display_repo_sync kernel/DTB.
Immediate rollback was performed on board:
/mnt/boot/zImage restored from /mnt/boot/zImage.before_display_repo_sync_20260615_205332
/mnt/boot/suniv-f1c100s-licheepi-nano.dtb restored from /mnt/boot/suniv-f1c100s-licheepi-nano.dtb.before_display_repo_sync_20260615_205332

Host kernel sources were also restored from:
~/LicheePi_Nano/backups/st7701_display_repo_sync_20260615_204957

Important follow-up:
The deployed DTB size changed from 12156 bytes to 12301 bytes, so the blank screen may have been caused by DTB differences rather than the C-only TCON/fbcon changes. Do not redeploy the newer DTB blindly. Compare the board backup DTB against host DTB before further changes.
```

## ST7701 fbdev/gmenu2x 384x640 Fix - 2026-06-15
Current status:
```text
Root cause confirmed:
- Kernel DRM initially created fbdev as 384x640/32bpp.
- Starting gmenu2x changed fb0 back to 320x240 visible / 320x480 virtual.
- gmenu2x source had Linux platform defaults hardcoded to 320x240/16bpp.
- Kernel also had CONFIG_FB_TFT=y and CONFIG_FB_TFT_ST7789V=y, which kept the old ST7789 320x240 mode in fb0 modelist.
```

Changes applied:
```text
Kernel .config:
- CONFIG_FB_TFT disabled
- CONFIG_FB_TFT_ST7789V disabled
- CONFIG_FB_TFT_FBTFT_DEVICE disabled if present
- Rebuilt zImage only. DTB was NOT deployed because host DTB still contains PD19 gpio-hog that previously caused blank display.

GMenu2X:
~/LicheePi_Nano/gmenu2x/src/gmenu2x.h
- default w/h/bpp changed from 320x240/16 to 384x640/32

~/LicheePi_Nano/gmenu2x/src/platform/linux.h
- Linux hwInit() changed from w=320,h=240 to w=384,h=640,bpp=32
```

Builds completed:
```sh
cd ~/LicheePi_Nano/linux
export PATH=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin:$PATH
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- -j8

cd ~/LicheePi_Nano/gmenu2x
make -f Makefile.f1c200s clean
make -f Makefile.f1c200s -j4
```

Board deployment:
```text
Deployed zImage to real boot partition /dev/mmcblk0p1 mounted at /tmp/bootcheck.
Deployed rebuilt gmenu2x from dist/gmenu2x-f1c200s.tar to /root/gmenu2x.

Current boot backups retained:
/tmp/bootcheck/zImage.before_display_repo_sync_20260615_205332
/tmp/bootcheck/zImage.before_st7789off_20260615_222258
/tmp/bootcheck/suniv-f1c100s-licheepi-nano.dtb.before_display_repo_sync_20260615_205332
```

Important:
```text
Do not deploy host DTB blindly while it contains PD19 lcd-de-fixed-high gpio-hog. That DTB previously caused blank display. Current working DTB is the 12156-byte boot partition DTB.
Do not use /mnt/boot as boot partition; it is only a directory unless explicitly mounted. Real boot partition is /dev/mmcblk0p1 and should be mounted at /tmp/bootcheck or another verified mountpoint.
```

## ST7701 ID Read Test - 2026-06-16
Current status:
```text
Purpose: try reading ST7701 panel/chip ID without modifying kernel, using
board-side GPIO bitbang while Linux is running.

Reference manual supplied by user:
D:\BaiduNetdiskDownload\单屏-插接40P-T295H5-C40-01\ST7701S_SPEC_Preliminary V0.2.pdf

Current known control pins:
CS  = PA0
SDA = PA1
SCL = PA2
RST = PA3

Current kernel init driver uses 3-wire 9-bit serial style:
1 D/C bit followed by 8 command/data bits.
Command bit is 0; data bit is 1.
```

Local Windows test workspace:
```text
C:\Users\26301\Desktop\F1C200S_ST7701_read_id
```

Prepared board-side scripts:
```text
/root/read_st7701_id_gpio.sh
/root/read_st7701_id_devmem.sh
```

Local files:
```text
C:\Users\26301\Desktop\F1C200S_ST7701_read_id\read_st7701_id_gpio.sh
C:\Users\26301\Desktop\F1C200S_ST7701_read_id\read_st7701_id_devmem.sh
C:\Users\26301\Desktop\F1C200S_ST7701_read_id\deploy_to_board.ps1
```

Deploy when board is online:
```powershell
cd C:\Users\26301\Desktop\F1C200S_ST7701_read_id
.\deploy_to_board.ps1
```

Board test commands:
```sh
/root/read_st7701_id_gpio.sh
/root/read_st7701_id_devmem.sh
SWAP_SDA_SCL=1 /root/read_st7701_id_devmem.sh
DELAY_US=20 /root/read_st7701_id_devmem.sh
```

Read commands attempted by the scripts:
```text
0x04 Read Display ID, 4 bytes, with and without one dummy byte
0xDA Read ID1, with and without one dummy byte
0xDB Read ID2, with and without one dummy byte
0xDC Read ID3, with and without one dummy byte
```

Important interpretation:
```text
If every byte is 0x00 or every byte is 0xFF, it probably means the panel read
path is not electrically connected or SDA cannot turn around to input, not
necessarily that the command is wrong.

Reading ID only works if the panel exposes readback on the connected serial
line. If the 3-wire SDA is write-only on the board, or SDO/MISO is not wired,
software cannot read the chip ID.

The sysfs GPIO script may fail if the kernel st7701init driver already owns
PA0-PA3. The devmem script bypasses GPIO ownership and directly programs PA
registers, so it is the more useful board-side test.
```

## Pending Kernel/DTB Deployment - 2026-06-16
Current status:
```text
User compiled the kernel successfully on the host after the zero-byte object
file issue was fixed.

When the board is online, update both kernel and DTS/DTB on the boot partition.
Do not reboot automatically; user handles reboot.
```

Expected host artifacts:
```text
~/LicheePi_Nano/linux/arch/arm/boot/zImage
~/LicheePi_Nano/linux/arch/arm/boot/dts/suniv-f1c100s-licheepi-nano.dtb
```

Deployment reminder:
```sh
# Board: root@172.28.73.247 password 1
# Real boot partition must be mounted from /dev/mmcblk0p1.
# Do not assume /mnt/boot is mounted.

mkdir -p /tmp/bootcheck
mount | grep -q ' /tmp/bootcheck ' || mount /dev/mmcblk0p1 /tmp/bootcheck

cp /tmp/bootcheck/zImage /tmp/bootcheck/zImage.before_kernel_dtb_update_$(date +%Y%m%d_%H%M%S)
cp /tmp/bootcheck/suniv-f1c100s-licheepi-nano.dtb /tmp/bootcheck/suniv-f1c100s-licheepi-nano.dtb.before_kernel_dtb_update_$(date +%Y%m%d_%H%M%S)

# Then copy new zImage and suniv-f1c100s-licheepi-nano.dtb into /tmp/bootcheck.
sync
```

After user reboot, verify:
```sh
ls /dev/fb*
cat /sys/class/graphics/fb0/name
cat /sys/class/graphics/fb0/virtual_size
cat /sys/class/graphics/fb0/modes
dmesg | grep -iE 'st7789|fb_tft|srgn fbdev|drmfb|frame buffer'
/root/run_gmenu2x.sh
cat /sys/class/graphics/fb0/virtual_size
```

Expected:
```text
- No fb_st7789v registration.
- fb0 remains sun4i-drmdrmfb.
- gmenu2x no longer switches fb0 back to 320x240.
```

## ST7701 TCON B/R Color Swap - 2026-06-17
Current status:
```text
Purpose:
Current ST7701 RGB panel shows red and blue channels swapped.

Implementation:
Added a DTS-controlled TCON0 B/R swap bit so this can be toggled per panel
without hardcoding a color swap for every screen.

Host backup:
~/LicheePi_Nano/backups/kernel_tcon_swap_br_20260617_030925

Source changes:
~/LicheePi_Nano/linux/drivers/gpu/drm/sun4i/sun4i_tcon.h
- Added:
  #define SUN4I_TCON0_CTL_SWAP BIT(23)

~/LicheePi_Nano/linux/drivers/gpu/drm/sun4i/sun4i_tcon.c
- In sun4i_tcon0_mode_set_rgb(), read:
  srgn,swap-b-r
  from tcon->dev->of_node.
- If present, set SUN4I_TCON0_CTL_SWAP.
- If absent, clear SUN4I_TCON0_CTL_SWAP.

~/LicheePi_Nano/linux/arch/arm/boot/dts/suniv-f1c100s-licheepi-nano.dts
- Current panel enables:
  &tcon0 {
      srgn,swap-b-r;
  };

Build result:
cd ~/LicheePi_Nano/linux
export PATH=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin:$PATH
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- drivers/gpu/drm/sun4i/sun4i_tcon.o -j8
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- suniv-f1c100s-licheepi-nano.dtb -j8
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- -j8

Builds passed. Current artifacts:
~/LicheePi_Nano/linux/arch/arm/boot/zImage
size: 4902016
~/LicheePi_Nano/linux/arch/arm/boot/dts/suniv-f1c100s-licheepi-nano.dtb
size: 12182
~/LicheePi_Nano/linux/drivers/gpu/drm/sun4i/sun4i_tcon.o
size: 23828

Deploy when board is online:
- Deploy both zImage and suniv-f1c100s-licheepi-nano.dtb to the real boot
  partition, not /mnt/boot unless it is verified mounted.
- Do not reboot automatically.

If colors become correct, keep srgn,swap-b-r for this panel.
If colors become wrong on another panel, remove srgn,swap-b-r from &tcon0 and
rebuild/deploy DTB. The driver support can stay.
```

## SDL fbcon Visible Area 360x640 on 384x640 Panel - 2026-06-20
Current status:
```text
Problem:
- Current ST7701 panel exposes framebuffer/mode as 384x640, stride 1536.
- Physical visible width is only 360 pixels, so the right 24 pixels are cut.
- Fix must be shared by gmenu2x and ONS/emulators instead of hardcoding each app.

Implementation:
Patched SDL 1.2 fbcon in:
~/LicheePi_Nano/buildroot-2018.02.11/output/build/sdl-1.2.15/src/video/fbcon/SDL_fbvideo.c
~/LicheePi_Nano/buildroot-2018.02.11/output/build/sdl-1.2.15/src/video/fbcon/SDL_fbvideo.h

New env variables:
SDL_FBCON_VISIBLE_W=360
SDL_FBCON_VISIBLE_H=640

When these are set and the kernel fb is larger:
- SDL keeps the physical fb mode, e.g. 384x640.
- SDL exposes logical visible size to apps.
- SDL forces shadow fb and copies only the logical visible area to physical fb.
- Apps requesting 384-wide modes are clamped to 360 logical width.

Shared runtime env updated:
~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/root/sdl_landscape_env.sh
~/LicheePi_Nano/buildroot-2018.02.11/output/target/root/sdl_landscape_env.sh

Current script contents include:
export SDL_FB_BROKEN_MODES=1
export SDL_FBCON_VISIBLE_W=360
export SDL_FBCON_VISIBLE_H=640

Build/deploy:
cd ~/LicheePi_Nano/buildroot-2018.02.11/output/build/sdl-1.2.15
make -j4
make DESTDIR=~/LicheePi_Nano/buildroot-2018.02.11/output/target install

Board deployment completed to 10.67.68.247:
/usr/lib/libSDL-1.2.so.0.11.4 md5 ef550db5ee1a13259221ec7dda9a7284
/root/sdl_landscape_env.sh md5 2f455fce6757929946402358b5369387

Board backup:
/root/roms/runtime_backups/sdl_visible_19700101_001031

No kernel/DTB was changed for this SDL fix. No automatic reboot was performed.
Restart gmenu2x or launch a new SDL app process to load the new library.

Follow-up fix:
ONS-GBK checks SDL_ListModes before SDL_SetVideoMode and rejected 384x288 when
SDL reported only 360x640. SDL fbcon now reports both logical visible mode and
physical mode when SDL_FB_BROKEN_MODES is set:
- first mode: 360x640 logical visible area
- compatibility mode: 384x640 physical fb
SDL_SetVideoMode still clamps 384-wide requests to the 360-pixel visible area.

Rebuilt/deployed SDL:
/usr/lib/libSDL-1.2.so.0.11.4 md5 5f6efd92fce254a59a992d850cb4b7b6
Board backup:
/root/roms/runtime_backups/sdl_visible_modes_19700101_000318
```


## ONS-GBK Mouse FIFO Stability - 2026-06-20
Current status:
```text
Problem:
After SDL visible width was changed to 360, ONS-GBK often started with no usable
mouse or failed with "Couldn't initialize SDL: Unable to open mouse". Source
review showed ONS mouse coordinates are already based on screen_width /
screen_device_width; with PDA_WIDTH=360 both are 360, so this is not a 384->360
coordinate scaling bug.

Root cause found during board testing:
- /root/sdl_landscape_env.sh sets SDL_MOUSEDEV=/dev/null for general apps, so
  /root/run_onscripter_gbk.sh must force SDL_MOUSEDEV=/tmp/matrix_ps2mouse.
- ONS-GBK must not set ONS_F1C200S_CURSOR; the old self-drawn cursor experiment
  caused framebuffer corruption and must stay disabled.
- matrix_ps2mouse_bridge is stable when started without EVIOCGRAB and with the
  explicit matrix input device /dev/input/event0.
- A stale stop_matrix_ps2mouse_bridge.sh process can race with a new ONS launch
  and delete /tmp/matrix_ps2mouse after the bridge starts, causing SDL fbcon to
  fail opening the mouse FIFO.

Deployed script fix:
/root/run_onscripter_gbk.sh now:
- unsets SDL_NOMOUSE
- forces SDL_MOUSEDRV=PS2
- forces SDL_MOUSEDEV=/tmp/matrix_ps2mouse
- does not set ONS_F1C200S_CURSOR
- kills stale stop_matrix_ps2mouse_bridge.sh and stops old bridge before launch
- starts bridge with INPUT_DEV=/dev/input/event0 and GRAB_INPUT=0
- waits for /tmp/matrix_ps2mouse FIFO before launching ONS

Verified on board 10.67.68.247:
- /root/onscripter-gbk md5: abc72d215ae105b38405d69d225b0117
- /usr/lib/libSDL-1.2.so.0.11.4 md5: ef550db5ee1a13259221ec7dda9a7284
- /root/run_onscripter_gbk.sh md5 after fix should be checked when needed.
- ONS starts with:
  ONS-GBK SDL mouse: drv=PS2 dev=/tmp/matrix_ps2mouse rotate=none
- Running processes include matrix_ps2mouse_bridge and onscripter-gbk, FIFO exists.

Open issue:
Visible pointer is still not solved. ONS logs show game cursor resources are
missing (cursor0.bmp/cursor1.bmp, uoncur.bmp/uoffcur.bmp/doncur.bmp/doffcur.bmp).
SDL software cursor enable inside ONS was tested and caused black screen in this
fbcon/shadow setup; do not keep that patch. The next cursor-visibility fix should
be an ONS internal fallback cursor or proper game cursor resources, not SDL
software cursor or direct framebuffer self-drawing.
```

## Shared GMenu2X/SDL Display Config - 2026-06-20
Current policy:
```text
All SDL apps/emulators launched from gmenu2x inherit display geometry from the
same shared config. Do not hardcode per-emulator resolution in launcher scripts.

Runtime config file:
/root/gmenu2x/display.conf

Canonical host overlay file:
~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/root/gmenu2x/display.conf

Current values:
GMENU2X_DISPLAY_W=360
GMENU2X_DISPLAY_H=640
GMENU2X_DISPLAY_BPP=32
GMENU2X_FBDEV=/dev/fb0
```

Implementation:
```text
/root/sdl_landscape_env.sh reads /root/gmenu2x/display.conf and exports:
SDL_DISPLAY_W
SDL_DISPLAY_H
SDL_DISPLAY_BPP
SDL_FBDEV
SDL_FBCON_VISIBLE_W
SDL_FBCON_VISIBLE_H

Launcher scripts should source /root/sdl_landscape_env.sh and must not set their
own visible width/height. App-specific input/audio overrides are still allowed.
ONS launchers still override SDL_NOMOUSE/SDL_MOUSEDRV/SDL_MOUSEDEV because ONS
uses the PS2 mouse FIFO.
```

Source changes:
```text
GMenu2X:
~/LicheePi_Nano/gmenu2x/src/platform/linux.h
- hwInit() reads SDL_DISPLAY_W/SDL_DISPLAY_H/SDL_DISPLAY_BPP instead of using
  fixed 384x640.

ONS-GBK:
~/LicheePi_Nano/third_party/ONScripter-GBK/Makefile.f1c200s
- uses -DPDA_AUTOSIZE instead of -DPDA_WIDTH=360.

~/LicheePi_Nano/third_party/ONScripter-GBK/ONScripter.cpp
- PDA_AUTOSIZE first reads SDL_DISPLAY_W, then SDL_FBCON_VISIBLE_W, then falls
  back to SDL_ListModes.
```

Build/integration completed on host:
```sh
cd ~/LicheePi_Nano/gmenu2x
make -f Makefile.f1c200s -j4

cd ~/LicheePi_Nano/third_party/ONScripter-GBK
make -f Makefile.f1c200s -j4

cd ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle
./sync_overlay_to_buildroot.sh
./collect_runtime_payload.sh
./install_payload_to_target.sh
```

Artifacts verified:
```text
~/LicheePi_Nano/buildroot-2018.02.11/output/target/root/gmenu2x/display.conf
~/LicheePi_Nano/buildroot-2018.02.11/output/target/root/sdl_landscape_env.sh
~/LicheePi_Nano/buildroot-2018.02.11/output/target/root/run_onscripter_gbk.sh
~/LicheePi_Nano/buildroot-2018.02.11/output/target/root/onscripter-gbk
onscripter-gbk md5: 75333b58eacb7eb12b65f662a9036e8d
```

Deployment status:
```text
Host-side buildroot target and runtime payload are updated.
Board deploy is pending because both known SSH addresses timed out:
10.67.68.247
172.28.73.247

When board is online, deploy through the standard runtime payload flow. Do not
reboot automatically.
```

Shared display deployment follow-up - 2026-06-20:
```text
Deployed unified display launcher scripts to board 10.67.68.247 for:
- GBA/gpSP
- SFC/Snes9x4D
- PS1/PCSX-ReARMed
- MD/PicoDrive
- DinguxCommander
- ONS-GBK/ONS-EN launchers already source the same env; ONS-GBK keeps its required mouse override and fallback cursor env.

Board backup:
/root/roms/runtime_backups/emulator_scripts_19700101_000215

Board verification:
/root/sdl_landscape_env.sh exports:
SDL_DISPLAY_W=360
SDL_DISPLAY_H=640
SDL_FBCON_VISIBLE_W=360
SDL_FBCON_VISIBLE_H=640
SDL_FBDEV=/dev/fb0

Checked board launcher scripts and found no direct SDL_FBDEV/SDL_FBCON_VISIBLE_W/H
or SDL_DISPLAY_W/H assignments in the emulator launchers. Resolution now flows
through /root/gmenu2x/display.conf.
```

## Snes9x4D F1C200S Performance Tuning - 2026-06-20
Problem:
```text
SFC games were visibly stuttering on the F1C200S board.
Source review showed the SDL port defaulted to relatively heavy audio settings:
44100Hz, stereo, interpolated sound, 1024 sample buffer, and auto frameskip max 3.
The launcher also forced 44100Hz/stereo.
```

Changes:
```text
~/LicheePi_Nano/third_party/snes9x4d/src/sdlmenu/sdlmain.cpp
- MaxAutoFrameSkip: 3 -> 5
- Settings.SoundPlaybackRate: 6 (44100Hz) -> 5 (32000Hz)
- Settings.Stereo: TRUE -> FALSE
- Settings.SoundBufferSize: 1024 -> 512
- Settings.InterpolatedSound: TRUE -> FALSE

~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/root/run_snes9x4d_console.sh
- launcher now uses:
  /root/snes9x4d -r 5 -b 512 -mono -ne -mfs 5 "$ROM_PATH"
```

Build/deploy:
```sh
cd ~/LicheePi_Nano/third_party/snes9x4d
make clean
make PREFIX=~/LicheePi_Nano/buildroot-2018.02.11/output/host/bin/arm-buildroot-linux-gnueabi -j4

cd ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle
./sync_overlay_to_buildroot.sh
./collect_runtime_payload.sh
./install_payload_to_target.sh
```

Board deployment:
```text
Board: 10.67.68.247
/root/snes9x4d md5: 5565aca3158732ac7c47974aaa973646
/root/run_snes9x4d_console.sh uses -r 5 -b 512 -mono -ne -mfs 5
Backup: /root/roms/runtime_backups/snes9x4d_perf_19700101_000853
```

Next tuning options if still slow:
```text
1. Try -r 4 -b 512 -mono -ne -mfs 7.
2. Consider disabling transparency (-nt) for problematic games.
3. Consider lowering CyclesPercentage with -h only if game compatibility remains acceptable.
4. Profile whether SDL_Flip/shadow copy or APU is the remaining bottleneck.
```

## SFC Fast / PocketSNES Test Path - 2026-06-20
Reason:
```text
Snes9x4D remained slow on Castlevania/Castlevania XX even after SDL UpdateRect
and light audio settings. FunKey-OS was checked at:
~/Downloads/FunKey-OS-master
It uses package/PocketSNES for SNES, not Snes9x4D, with a downscaling launcher.
```

## Miyoo F1C200S Snes9x4D Test Path - 2026-06-20
Source:
```text
~/Downloads/miyoo_src/snes9x4d
copied to:
~/LicheePi_Nano/third_party/snes9x4d_miyoo
```

Reason:
```text
User found miyoo_src, an F1C200S-oriented repository with its own snes9x4d.
This is being tested as a separate SFC candidate and does not replace the
current /root/snes9x4d, /root/pocketsnes, or /root/psnes_funkey paths.
```

Build:
```sh
cd ~/LicheePi_Nano/third_party/snes9x4d_miyoo/dingux-sdl
make clean
make -j4
```

Local changes:
```text
dingux-sdl/Makefile
- uses Buildroot app toolchain:
  /home/wnk/LicheePi_Nano/buildroot-2018.02.11/output/host/bin/arm-buildroot-linux-gnueabi-
- ARM926 flags:
  -march=armv5te -mtune=arm926ej-s -marm -msoft-float

dingux-sdl/sdlvideo.cpp
- video mode selection still follows the Miyoo order, but uses SDL_SWSURFACE
  instead of SDL_HWSURFACE|SDL_DOUBLEBUF.

dingux-sdl/sdlmain.cpp and dingux-sdl/sdlmenu.cpp
- frame presentation uses SDL_UpdateRect instead of SDL_Flip.

dingux-sdl/sdlmain.cpp
- default audio changed to 44100Hz mono, 512 buffer to avoid the 32000Hz I2S
  divider problem previously seen with FunKey PocketSNES.
- direct evdev input from the existing locally modified Snes9x4D was ported into
  the Miyoo SDL main loop because SDL keyboard state did not receive
  matrix-keypad events on this board.
- default Transparency changed from FALSE to TRUE. With transparency disabled,
  Castlevania/Castlevania XX showed background/sprite layer ordering artifacts.
- for the first stutter test after speed became normal, default SkipFrames was
  changed from AUTO_FRAMERATE to fixed 1. The goal is steadier frame pacing
  instead of Miyoo's uneven auto-skip cadence.
- Fixed SkipFrames=1 was smooth but very slow because in this old Snes9x4D code
  it effectively renders every frame. Changed to fixed SkipFrames=2 for the
  next test, which should render every other frame with a steadier cadence.
```

Board deployment:
```text
Board: 10.67.68.247
/root/snes9x4d_miyoo md5 after evdev input patch:
172d44decfc4aa1d26e61a4d3ae8d9ca
/root/snes9x4d_miyoo md5 after enabling transparency:
39b1b88e9a86843de56b8b92bc92ec40
/root/snes9x4d_miyoo md5 after fixed SkipFrames=1 pacing test:
f50652756b457b0118c92757c89673b5
/root/snes9x4d_miyoo md5 after fixed SkipFrames=2 pacing test:
fdf9a2358b713ecb2d8e41186ab38eac
/root/snes9x4d_miyoo md5 after direct /dev/fb0 write test:
5c4baae7777433f1f679b3a8a5e3c248
/root/run_snes9x4d_miyoo.sh
/root/gmenu2x/sections/emulators/sfc-miyoo

Menu title:
SFC Miyoo
```

Runtime notes:
```text
run_snes9x4d_miyoo.sh sources /root/sdl_landscape_env.sh and uses direct
matrix-keypad evdev input:
S9X_INPUT_DEV=/dev/input/event0
S9XKEYS=102,30,48,45,21,104,109,28,15,105,106,103,108
Order: QUIT,A,B,X,Y,L,R,START,SELECT,LEFT,RIGHT,UP,DOWN.

It stops matrix_pad_bridge before launch and exports SDL_NOMOUSE=1; without SDL_NOMOUSE
this Miyoo SDL build fails at SDL_Init with "Unable to open mouse".

The script must use Unix LF line endings on the board. CRLF caused:
sh: /root/run_snes9x4d_miyoo.sh: not found
```

Smoke test:
```text
/root/run_snes9x4d_miyoo.sh /root/roms/sfc/165_恶魔城XX.smc

ROM loaded successfully:
"DRACULA XX" [checksum ok] LoROM, 16Mbits

The test process stayed running after several seconds. Any duplicate manual
test process can occupy the audio device and cause "Sound device open failed";
clean up old snes9x4d_miyoo processes before retesting.

After enabling transparency, old per-ROM cfg files were moved from:
/root/.snes9x4d/*.cfg
to:
/root/.snes9x4d_cfg_backup/
because S9xReadConfig() can override compiled defaults.
```

Source comparison notes:
```text
~/Downloads/miyoo_src:
- Contains suniv-specific app paths such as pcsx_rearmed/frontend/plat_suniv.c.
- The original Miyoo runtime assumes a 320x240 16bpp fb path with
  /dev/miyoo_fb0 ioctls:
  MIYOO_FB0_SET_MODE and MIYOO_FB0_SET_FLIP.
- plat_suniv.c maps two framebuffer pages from /dev/fb0 and flips between them.

~/Downloads/f1c500s_kernel:
- miyoo_defconfig disables DRM and uses an fbdev-oriented panel driver path
  such as CONFIG_FB_R61520.
- f1c100s_config enables DRM/SUN4I/DRM fbdev emulation instead.

Current board:
- /sys/class/graphics/fb0/virtual_size reports 384,640 and modes show
  U:384x640p-0.
- /sys/class/graphics/fb0/bits_per_pixel reports 32 and stride reports 1536.
- This differs from Miyoo's expected 320x240/16bpp double-buffer fb path, so
  SDL presentation cost and frame pacing are not directly equivalent.

Direct framebuffer test:
- sdlvideo.cpp now mmap()s /dev/fb0 and creates an in-memory 320x240 16bpp SDL
  surface instead of using SDL_SetVideoMode for presentation.
- sdlmain.cpp now converts the 16bpp SNES frame to the current 32bpp fb0 and
  writes it directly, bypassing SDL_UpdateRect/SDL_Flip.
- This is not true hardware page-flip double buffering yet because the current
  DRM fbdev exposes 384x640x32 with no confirmed second page. It is a direct
  fb write test that can be rolled back with:
  cp /root/snes9x4d_miyoo.before_directfb /root/snes9x4d_miyoo
```

Implemented test path:
```text
Built/deployed existing host PocketSNES tree:
~/pocketsnes_f1c200s

Board files:
/root/pocketsnes
/root/run_pocketsnes.sh
/root/gmenu2x/sections/emulators/sfc-fast

Menu entry:
title=SFC Fast
description=PocketSNES
exec=/root/run_pocketsnes.sh
params=[selFullPath]
selectordir=/root/roms/sfc
selectorfilter=.sfc,.smc,.fig,.zip
```

Runtime integration:
```text
run_pocketsnes.sh now sources /root/sdl_landscape_env.sh and uses the standard
matrix_pad_bridge lifecycle, matching other SDL emulators.
```

Board deployment:
```text
Board: 10.67.68.247
/root/pocketsnes md5: 0c60d765c4290b94fd7911a12627a8bb
Backup: /root/roms/runtime_backups/pocketsnes_fast_19700101_000601
GMenu2X was restarted and now has both entries:
- sfc      = Snes9x4D current path
- sfc-fast = PocketSNES test path
```

Next step:
```text
Test SFC Fast with the same Castlevania ROM. If PocketSNES is materially faster
and controls/audio are acceptable, make SFC Fast the default SFC entry and keep
Snes9x4D as alternate/compatibility path.
```

PocketSNES follow-up tuning - 2026-06-20:
```text
User confirmed SFC Fast / PocketSNES is much smoother than Snes9x4D, but still
has some slowdown.

Host backup:
~/LicheePi_Nano/backups/pocketsnes_tune_20260620_020036

Source changes:
~/pocketsnes_f1c200s/sal.c
- SDL video mode changed from SDL_HWSURFACE + SDL_DOUBLEBUF to SDL_SWSURFACE.
- sal_VideoFlip now calls SDL_UpdateRect(full screen) instead of SDL_Flip.

~/pocketsnes_f1c200s/menu/menu.cpp
- Default showFps changed 1 -> 0 to avoid drawing FPS text every frame.
- Default soundRate changed 44100 -> 32000.

~/pocketsnes_f1c200s/menu/main.cpp
- Snes9x initial SoundPlaybackRate changed 44100 -> 32000.
- Snes9x initial Stereo changed TRUE -> FALSE.

Build:
cd ~/pocketsnes_f1c200s
make -f Makefile.f1c200s -j4
make -f Makefile.f1c200s strip

Board deployment:
Board: 10.67.68.247
/root/pocketsnes md5: 7eb1d36012fe516a546a58473649c564
Backup: /root/roms/runtime_backups/pocketsnes_tune_*

Board check:
No PocketSNES .opt/.cfg override was found under /root, so the new default
32000Hz mono/no-FPS settings should apply directly.

Next test:
Launch SFC Fast again with the same Castlevania ROM and judge:
1. If smoother and audio is acceptable, keep this build.
2. If audio underruns/clicks appear, inspect sal/linux/sal_sound.c buffering.
3. If still slow but audio is fine, next candidate is increasing default
   frameSkip or adding a launcher/env option for a more aggressive fast mode.
```

PocketSNES actual video-path fix - 2026-06-20:
```text
Important correction:
The first video-path patch edited ~/pocketsnes_f1c200s/sal.c, but Makefile.f1c200s
actually compiles ~/pocketsnes_f1c200s/sal/linux/sal.c. That explains why the
user saw no difference after the first deployment.

Host backup:
~/LicheePi_Nano/backups/pocketsnes_video_real_20260620_020900

Actual source changed:
~/pocketsnes_f1c200s/sal/linux/sal.c
- SDL_SetVideoMode now requests SDL_SWSURFACE instead of HWSURFACE/DOUBLEBUF.
- sal_VideoFlip now calls SDL_UpdateRect(full screen) instead of SDL_Flip.

Build/deploy:
cd ~/pocketsnes_f1c200s
rm -f sal/linux/sal.o
make -f Makefile.f1c200s -j4
make -f Makefile.f1c200s strip

Board deployment:
Board: 10.67.68.247
/root/pocketsnes md5 after actual video patch: 6fc01eea56bc5a93edff12b02fcf4279
Backup: /root/roms/runtime_backups/pocketsnes_real_video_*
```

PocketSNES no-scale fast default - 2026-06-20:
```text
Reason:
Default fullScreen=1 uses fast software scaling to 320x240. On F1C200S this is
still per-frame scaler work. For a fast SFC path, default to native SNES width
centered on the SDL surface.

Host backup:
~/LicheePi_Nano/backups/pocketsnes_no_scale_20260620_021009

Source changed:
~/pocketsnes_f1c200s/menu/menu.cpp
- Default fullScreen changed 1 -> 0.

Board deployment:
Board: 10.67.68.247
/root/pocketsnes md5: 9c89c63a1987a8b86b465a8fbe3e0e6e
Backup: /root/roms/runtime_backups/pocketsnes_no_scale_*

Test note:
This may make the image smaller but should reduce scaling overhead. If the user
prefers larger image and performance is unchanged, revert fullScreen default to 1
while keeping the real sal/linux/sal.c video patch.
```

PocketSNES FunKey-OS comparison - 2026-06-20:
```text
User pointed to:
~/Downloads/FunKey-OS-master

Finding:
FunKey-OS uses FunKey-Project/PocketSNES, but it is not the same configuration
as the current F1C200S build.

FunKey package:
~/Downloads/FunKey-OS-master/FunKey/package/PocketSNES/PocketSNES.mk

Important FunKey differences:
1. CPU/toolchain target is V3s/Cortex-A7 class:
   -march=armv7-a -mtune=cortex-a7 -mfpu=neon -mfloat-abi=hard
   These must NOT be used on F1C200S/ARM926EJ-S.

2. FunKey compiler macros include:
   -DRC_OPTIMIZED -D__LINUX__ -D__DINGUX__ -DNO_ROM_BROWSER -DGCW_ZERO

3. FunKey source also enables 16-bit-only sound through:
   pocketsnes/linux/port.h:
   #define FOREVER_16_BIT
   #define FOREVER_16_BIT_SOUND

4. FunKey desktop entry has:
   X-OD-NeedsDownscaling=true

5. FunKey source is substantially modified:
   /tmp/PocketSNES-FunKey/menu/main.cpp
   - disables the old generic 320x240 software scaling block with #if 0
   - uses 240x240-specific crop/downscale logic through RES_HW_SCREEN_HORIZONTAL
     and RES_HW_SCREEN_VERTICAL
   - has aspect-ratio config support not present in the current gameblabla tree

Current F1C200S low-risk alignment applied:
~/pocketsnes_f1c200s/Makefile.f1c200s
- Added -DFOREVER_16_BIT_SOUND.
- Added -fexpensive-optimizations.
- Did not add ARMv7/NEON/hard-float flags.
- Did not add GCW_ZERO yet because the current tree's GCW_ZERO/fullScreen==3
  path is not equivalent to FunKey's 240x240 renderer and may need source
  adaptation first.

Build/deploy:
cd ~/pocketsnes_f1c200s
make -f Makefile.f1c200s clean
make -f Makefile.f1c200s -j4
make -f Makefile.f1c200s strip

Board deployment:
Board: 10.67.68.247
/root/pocketsnes md5: 84e9e2fc51b4d0e94deea48b4fe6b67b
Backup: /root/roms/runtime_backups/pocketsnes_funkey_flags_*

Next options:
1. Test this build first.
2. If still slow, either:
  - port FunKey's 240x240 crop/downscale path to current tree for F1C200S, or
  - build FunKey-Project/PocketSNES directly with ARM926 soft-float flags and
     adapt its input/launcher/runtime to this board.
```

FunKey PocketSNES direct test build - 2026-06-20:
```text
Purpose:
Deploy a direct FunKey-Project/PocketSNES build as an independent test path,
without replacing the current /root/pocketsnes.

Host source:
~/pocketsnes_funkey_f1c200s
Copied from:
/tmp/PocketSNES-FunKey
Upstream:
https://github.com/FunKey-Project/PocketSNES.git

Host local Windows working folder:
C:\Users\26301\Desktop\F1C200S_pocketsnes_funkey

Makefile adaptations:
- Use Buildroot ARM926 soft-float toolchain:
  /home/wnk/LicheePi_Nano/buildroot-2018.02.11/output/host/bin/arm-buildroot-linux-gnueabi-
- Remove V3s-only flags:
  -march=armv7-a -mtune=cortex-a7 -mfpu=neon -mfloat-abi=hard
- Use:
  -march=armv5te -mtune=arm926ej-s -marm -msoft-float
- Keep FunKey macros:
  -DNO_ROM_BROWSER -DGCW_ZERO

Display adaptation:
FunKey defaults RES_HW_SCREEN_HORIZONTAL/VERTICAL to 240x240.
For the current F1C200S SDL landscape setup, changed both definitions in:
~/pocketsnes_funkey_f1c200s/sal/linux/include/sal.h
~/pocketsnes_funkey_f1c200s/sal/linux/sal.c
to:
RES_HW_SCREEN_HORIZONTAL 320
RES_HW_SCREEN_VERTICAL   240

Build fix:
menu/configfile_fk.cpp used C99/C++20-style designated initializers unsupported
by the current cross g++. Replaced them with positional initializers.

Build:
cd ~/pocketsnes_funkey_f1c200s
make clean
make -j4
strip with:
/home/wnk/LicheePi_Nano/buildroot-2018.02.11/output/host/bin/arm-buildroot-linux-gnueabi-strip psnes

Board deployment:
Board: 10.67.68.247
/root/psnes_funkey md5: 543294cbbff0ed1c4a43343e2c7d4885
/root/run_psnes_funkey.sh
/root/gmenu2x/sections/emulators/sfc-funkey

Menu entry:
title=SFC FunKey
description=FunKey PocketSNES test
exec=/root/run_psnes_funkey.sh
params=[selFullPath]
selectordir=/root/roms/sfc
selectorfilter=.sfc,.smc,.fig,.zip

Smoke test:
Started /root/run_psnes_funkey.sh /root/roms/sfc/165_恶魔城XX.smc on board.
/root/psnes_funkey stayed running after several seconds.
Note: an existing /root/pocketsnes process was also running during smoke test,
so performance must be tested from gmenu2x after exiting the old SFC Fast process.

Next:
Test SFC FunKey from gmenu2x against SFC Fast on the same ROM. If faster and
controls/audio are acceptable, either make it default SFC Fast or port its
renderer changes back into the current tree.
```

FunKey PocketSNES input fix - 2026-06-20:
```text
Problem:
SFC FunKey started and displayed, but buttons did not respond.

Root cause:
FunKey sal/linux/sal.c input layer only handled SDL keyboard letter events:
a/b/x/y/m/n/s/k/u/d/l/r/q/h.
Our launcher starts matrix_pad_bridge, so controls arrive as SDL joystick js0,
which the original FunKey input code ignored.

Fix:
~/pocketsnes_funkey_f1c200s/sal/linux/sal.c
- Added SDL_Joystick polling.
- Mapped matrix_pad_bridge layout:
  button 0 A
  button 1 B
  button 2 X
  button 3 Y
  button 4 L
  button 5 R
  button 8 Select
  button 9 Start
  buttons 6/7 or Select+Start -> menu
  Select+L -> menu
  Select+R -> aspect ratio
- Kept keyboard fallback for current gmenu2x key mapping.
- Changed SDL_Init to SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_JOYSTICK.
- Set SDL_NOMOUSE=1 and SDL_INPUT_LINUX_KEEP_KBD=1.
- Disabled joystick events with SDL_JoystickEventState(SDL_IGNORE), using poll.
- Closed joystick in sal_Reset().

Build/deploy:
cd ~/pocketsnes_funkey_f1c200s
rm -f sal/linux/sal.o
make -j4
arm-buildroot-linux-gnueabi-strip psnes

Board deployment:
Board: 10.67.68.247
/root/psnes_funkey md5: b214dd3459d64c3721d8d197fb0765be
Backup: /root/roms/runtime_backups/psnes_funkey_input_*

Smoke test:
/root/run_psnes_funkey.sh /root/roms/sfc/165_恶魔城XX.smc starts and logs:
cfg_file_rom/default_config paths and "soft".
Manual kill produced the expected SDL_QUIT message; not a crash.

Next:
User should retest SFC FunKey controls from gmenu2x.
```

FunKey PocketSNES frame pacing test - 2026-06-20:
```text
Problem:
User reported SFC FunKey still "卡", but it felt like no slowdown, just very
uneven/stuttery. This points to frame pacing / sync rather than raw CPU speed.

Source findings:
~/pocketsnes_funkey_f1c200s/menu/main.cpp
- S9xSyncSpeed had a FunKey-specific forced AUTO_FRAMERATE block:
  #warning forcing frameSkip
  Settings.SkipFrames = AUTO_FRAMERATE;
- This can create uneven visible frames because rendering is controlled by audio
  buffer state, not a stable display cadence.

~/pocketsnes_funkey_f1c200s/sal/linux/sal.c
- sal_VideoFlip used SDL_Flip(hw_screen), unlike the current F1C200S SDL path
  where SDL_UpdateRect has been smoother/more direct.

Changes:
~/pocketsnes_funkey_f1c200s/menu/main.cpp
- Removed the forced AUTO_FRAMERATE override in S9xSyncSpeed.
- Run() still forces mMenuOptions.frameSkip = 0, so this test is no frameskip,
  fixed render cadence if the CPU can keep up.

~/pocketsnes_funkey_f1c200s/sal/linux/sal.c
- sal_VideoFlip now uses:
  SDL_UpdateRect(hw_screen, 0, 0, hw_screen->w, hw_screen->h)
  instead of SDL_Flip(hw_screen).

Build/deploy:
cd ~/pocketsnes_funkey_f1c200s
rm -f sal/linux/sal.o menu/main.o
make -j4
arm-buildroot-linux-gnueabi-strip psnes

Board deployment:
Board: 10.67.68.247
/root/psnes_funkey md5: a91a0c0bdc91d071107bd1550657b488
Backup: /root/roms/runtime_backups/psnes_funkey_sync_*

Interpretation for next test:
- If smoother but slower: raw CPU is short; try fixed frameskip=1.
- If same stutter: add explicit frame pacing around sal_VideoFlip or inspect
  SDL fbcon UpdateRect timing.
- If better: keep this sync path and make SFC FunKey the default candidate.
```

FunKey PocketSNES explicit 60Hz pacing test - 2026-06-20:
```text
Problem:
User reported it is still not smooth after removing forced AUTO_FRAMERATE and
using SDL_UpdateRect.

Finding:
~/pocketsnes_funkey_f1c200s/sal/linux/sal.c
- sal_VideoEnterGame had the mRefreshRate assignment inside a disabled #if 0
  block, so the video layer never updated/reset frame timing for the loaded ROM.

Change:
~/pocketsnes_funkey_f1c200s/sal/linux/sal.c
- sal_VideoEnterGame now sets:
  mRefreshRate = refreshRate ? refreshRate : 60
  and resets mFramePaceLastTicks.
- sal_VideoFlip(vsync=0), used by gameplay, now waits to maintain approximately
  1000 / mRefreshRate ms between displayed frames before SDL_UpdateRect.
- sal_VideoFlip(vsync=1), used by menus, is not paced.

Build/deploy:
cd ~/pocketsnes_funkey_f1c200s
rm -f sal/linux/sal.o
make -j4
arm-buildroot-linux-gnueabi-strip psnes

Board deployment:
Board: 10.67.68.247
/root/psnes_funkey md5: b85939612bad946a2bc097c215747717
Backup: /root/roms/runtime_backups/psnes_funkey_pace_*

Next if still uneven:
1. Replace SDL_GetTicks millisecond pacing with gettimeofday microsecond pacing.
2. Try fixed frameskip=1 if pacing makes speed too tight.
3. Inspect SDL fbcon UpdateRect timing or consider reverting to current
   PocketSNES core with only FunKey renderer pieces.
```

FunKey PocketSNES fixed frameskip=1 test - 2026-06-20:
```text
Problem:
Explicit millisecond pacing still did not make SFC FunKey smooth. User described
no obvious slowdown, only lack of smoothness.

Change:
~/pocketsnes_funkey_f1c200s/menu/main.cpp
- Run() now forces:
  mMenuOptions.frameSkip = 2
  which maps to Settings.SkipFrames = 1, i.e. fixed skip 1 rendered frame.
- This intentionally targets steadier ~30fps-like output instead of unstable
  auto skip or full-rate rendering.

~/pocketsnes_funkey_f1c200s/menu/menu.cpp
- Default options changed to:
  frameSkip = 2
  soundRate = 32000
  stereo = 0
  soundSync = 0

Board config handling:
Existing generated FunKey cfg files can override defaults, so deployment moved:
/root/roms/sfc/default_config.fkcfg -> default_config.fkcfg.before_fs1
/root/roms/sfc/165_恶魔城XX.fkcfg -> 165_恶魔城XX.fkcfg.before_fs1
Those files were empty at deploy time, but moving them ensures the new compiled
defaults are used.

Build/deploy:
cd ~/pocketsnes_funkey_f1c200s
rm -f menu/main.o menu/menu.o
make -j4
arm-buildroot-linux-gnueabi-strip psnes

Board deployment:
Board: 10.67.68.247
/root/psnes_funkey md5: 7f0f018c2ae3a1a445c1a10ae1371385
Backup: /root/roms/runtime_backups/psnes_funkey_fs1_*

Interpretation:
- If this becomes stable but lower frame rate: 60fps/full-render is too tight
  for the board or current SDL path; keep fixed skip or tune skip cadence.
- If still uneven: likely SDL/fbcon update cadence or renderer copy pattern,
  not only emulation load.
```

Miyoo Snes9x4D direct-fb dummy SDL test - 2026-06-20:
```text
User requested repository-style double buffering/direct framebuffer writes and
reported screen flashing during runtime.

Host source:
~/LicheePi_Nano/third_party/snes9x4d_miyoo/dingux-sdl

Backup before rect/dummyfb work:
~/LicheePi_Nano/backups/snes9x4d_miyoo_rectfb_20260620_193315

Changes:
- sdlvideo.cpp maps /dev/fb0 directly and allocates a framebuffer shadow buffer.
- It attempts FBIOPUT_VSCREENINFO yres_virtual=yres*2 and FBIOPAN_DISPLAY, but
  the current DRM fbdev reports virtual_y=640 on a 384x640 fb, so pages=1.
- Since true hardware page flipping is unavailable, fallback now clears the full
  framebuffer once at init and then flushes only the game rectangle each frame.
- SDL video driver is forced to dummy inside S9xInitDisplay so SDL no longer
  opens fbcon and no longer creates a second /dev/fb0 mmap. The emulator creates
  memory SDL surfaces only; presentation is entirely through the direct /dev/fb0
  path.
- Direct FB startup log is written to stderr and flushed immediately.

Build/deploy:
cd ~/LicheePi_Nano/third_party/snes9x4d_miyoo/dingux-sdl
make -j4
/home/wnk/LicheePi_Nano/buildroot-2018.02.11/output/host/bin/arm-buildroot-linux-gnueabi-strip snes9x4d.dge

Board deployment:
/root/snes9x4d_miyoo md5: ab7009ff4cfabf2f8432bdc57ff5452a
Backup on board: /root/snes9x4d_miyoo.before_dummyfb

Runtime verification:
/root/run_snes9x4d_miyoo.sh /root/roms/sfc/165_恶魔城XX.smc
/tmp/snes9x4d_miyoo.log shows:
Direct FB: /dev/fb0 384x640 virtual_y=640 visible=360x640 bpp=32 stride=1536 size=983040 pages=1

Captured framebuffer:
C:\Users\26301\Desktop\fb0_sfc_dummyfb_visible.png
The title screen is visible and not horizontally split in the captured frame.
A previous split capture was caused by two snes9x4d_miyoo processes writing fb0
at the same time. Avoid manual duplicate launches while testing.

Remaining limitation:
This is not true Miyoo /dev/miyoo_fb0 hardware double buffering because current
kernel exposes sun4i DRM fbdev with a single virtual page. If visible flashing
persists, next options are adding scan/pacing mitigation in userspace or adding
kernel/DRM fbdev support for a second virtual framebuffer page.
```

Kernel fbdev overalloc 200 test - 2026-06-20:
```text
Purpose:
Expose a double-height DRM fbdev buffer so direct framebuffer emulators can use
FBIOPAN_DISPLAY instead of single-page direct writes.

Change:
/home/wnk/LicheePi_Nano/linux/.config
/home/wnk/LicheePi_Nano/linux/arch/arm/configs/f1c100s_nano_linux_defconfig
/home/wnk/LicheePi_Nano/linux/arch/arm/configs/linux-licheepi_nano_defconfig
CONFIG_DRM_FBDEV_OVERALLOC=100 -> 200

Reason:
drivers/gpu/drm/drm_fb_helper.c multiplies sizes.surface_height by
CONFIG_DRM_FBDEV_OVERALLOC / 100 before creating the fbdev framebuffer. With the
current 384x640x32 mode, this should expose about 384x1280x32 and allow two
pages for user-space pan/display tests.

Kernel build command used:
cd ~/LicheePi_Nano/linux
export PATH=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
/usr/bin/make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- -j8 zImage suniv-f1c100s-licheepi-nano.dtb

Build result: success.
Artifacts copied to Windows Desktop:
C:\Users\26301\Desktop\zImage.fbdev_overalloc200
C:\Users\26301\Desktop\suniv-f1c100s-licheepi-nano.fbdev_overalloc200.dtb

Board deployment:
Boot partition /dev/mmcblk0p1 was mounted at /tmp/bootp1 and new zImage/dtb were
copied to:
/tmp/bootp1/zImage
/tmp/bootp1/suniv-f1c100s-licheepi-nano.dtb

Deployed file sizes:
zImage = 4902008
suniv-f1c100s-licheepi-nano.dtb = 12182

Important:
Board has not been rebooted yet, so the running kernel still reports the old
single-page fbdev until reboot. After reboot, verify:
cat /sys/class/graphics/fb0/virtual_size
Expected: 384,1280 or another double-height value. Then SFC Miyoo should log
pages=2 instead of pages=1.

Impact expectation:
Other fbdev/SDL programs should keep drawing to the first visible 384x640 page
unless they explicitly pan. Memory/CMA usage increases by roughly one extra
framebuffer page, about 960 KiB for 384x640x32. Programs that incorrectly clear
by smem_len may spend extra time clearing the hidden page but should not show a
visual change.

Deployment caveat:
The first backup command used a remote shell variable that PowerShell expanded
locally, so the intended backup_fbdev_overalloc200 directory was not created
correctly. Existing boot partition backups remain under backup_20260616_000619
and backup_20260617_rb_swap, and the current new artifacts are saved on the
Windows Desktop and host build tree.
```

Miyoo Snes9x4D true fbdev double buffer fix - 2026-06-20:
```text
After rebooting with CONFIG_DRM_FBDEV_OVERALLOC=200, board reports:
/sys/class/graphics/fb0/virtual_size = 384,1280

SFC Miyoo now logs:
Direct FB: /dev/fb0 384x640 virtual_y=1280 visible=360x640 bpp=32 stride=1536 size=1966080 pages=2

Problem observed after pages=2:
Game rectangle did not flicker, proving FBIOPAN_DISPLAY worked, but borders
alternated between black and stale gmenu2x background. Root cause was that the
emulator updated only the game rectangle on the back page, leaving each fb page
with different non-game border contents.

Fix:
~/LicheePi_Nano/third_party/snes9x4d_miyoo/dingux-sdl/sdlvideo.cpp
- At direct fb init, memset the entire mapped fb memory (both pages) to black.
- Reset yoffset to 0 before starting and on S9xDeinitDisplay().
- In pages>=2 path, copy the complete shadow page to the selected back page
  before FBIOPAN_DISPLAY. The shadow page is initialized black and only the game
  rectangle changes each frame, so both pages keep identical borders.

Current deployed board binary:
/root/snes9x4d_miyoo md5: 116951de44703f5b07e4dc0a196ed97a
Board backup before this binary:
/root/snes9x4d_miyoo.before_fullbackpage

Verification:
Captured /dev/fb0 two-page raw and compared page0/page1 visible 360x640 area:
diff_bbox=None, meaning both displayed pages are identical at capture time.
Desktop captures:
C:\Users\26301\Desktop\fb0_sfc_fullbackpage_page0_visible.png
C:\Users\26301\Desktop\fb0_sfc_fullbackpage_page1_visible.png

Remaining SFC issue:
User still reports slowdown when the character moves. That is now separate from
fb page flicker/residual-border artifacts. Investigate emulation load, PPU
transparency, audio sync/buffer, and frameskip cadence next.
```

## Shared SDL 360x480 Center Window - 2026-06-20
```text
Goal:
Physical framebuffer remains 360x640 visible on a 384-stride fbdev, but all
SDL/gmenu2x applications should draw only in the center 360x480 window: skip top
80 rows and bottom 80 rows.

Runtime geometry:
/root/gmenu2x/display.conf
GMENU2X_DISPLAY_W=360
GMENU2X_DISPLAY_H=480
GMENU2X_DISPLAY_X=0
GMENU2X_DISPLAY_Y=80
GMENU2X_DISPLAY_BPP=32
GMENU2X_FBDEV=/dev/fb0

/root/sdl_landscape_env.sh exports:
SDL_FBCON_VISIBLE_W/H from GMENU2X_DISPLAY_W/H
SDL_FBCON_VISIBLE_X/Y from GMENU2X_DISPLAY_X/Y

SDL source changed:
~/LicheePi_Nano/buildroot-2018.02.11/output/build/sdl-1.2.15/src/video/fbcon/SDL_fbvideo.h
- Added visible_x and visible_y fields/macros.

~/LicheePi_Nano/buildroot-2018.02.11/output/build/sdl-1.2.15/src/video/fbcon/SDL_fbvideo.c
- Reads SDL_FBCON_VISIBLE_X and SDL_FBCON_VISIBLE_Y.
- Clamps offset so visible_x+visible_w and visible_y+visible_h stay inside the
  physical framebuffer.
- Treats nonzero visible offset as a visible-area/shadow-fb mode.
- FB_DirectUpdate() adds visible_x/visible_y to destination framebuffer
  coordinates only; app logical coordinates remain 0..359 and 0..479.

Build/deploy:
cd ~/LicheePi_Nano/buildroot-2018.02.11/output/build/sdl-1.2.15
rm -f build/.libs/SDL_fbvideo.o build/.libs/libSDL-1.2.so.0.11.4
make -j4
make DESTDIR=~/LicheePi_Nano/buildroot-2018.02.11/output/target install

cd ~/LicheePi_Nano/gmenu2x
make -f Makefile.f1c200s -j4

cd ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle
./sync_overlay_to_buildroot.sh
./collect_runtime_payload.sh

Runtime bundle scripts updated:
collect_runtime_payload.sh now copies payload/libSDL-1.2.so.0.11.4.
install_payload_to_target.sh now installs the SDL library and symlinks
libSDL-1.2.so.0 and libSDL.so.

Board deployment:
Board IP used: 10.67.68.247
Installed /usr/lib/libSDL-1.2.so.0.11.4 md5:
06ae629ab3f2d102ad2ec3e6f1e68276

After restart, /tmp/gmenu2x_360x480.log shows:
Surface double buffer: screen=360x480 32bpp pitch=1440 raw=360x480 32bpp pitch=1440

Framebuffer capture verification:
Captured /dev/fb0 with stride 1536, 640 lines.
Non-black bbox in physical visible 360x640 area:
(0,80)-(360,560)
Top 80 rows non-black pixels: 0
Center 480 rows non-black pixels: 169003
Bottom 80 rows non-black pixels: 0

Local work/artifacts:
C:\Users\26301\Desktop\F1C200S_display_360x480_window
Host backup:
~/LicheePi_Nano/backups/display_360x480_20260620_075557
Board backup:
/root/roms/runtime_backups/display_360x480_*

Caveat:
SDL fbcon apps inherit this through /root/sdl_landscape_env.sh. Any emulator that
bypasses SDL and writes /dev/fb0 directly must separately read the same display
configuration or implement its own equivalent x/y offset.
```

## Shared SDL 90 Degree Rotation - 2026-06-20
```text
Goal:
On top of the centered physical 360x480 window, rotate gmenu2x and SDL emulator
output by 90 degrees.

Runtime geometry:
/root/gmenu2x/display.conf
GMENU2X_DISPLAY_W=480
GMENU2X_DISPLAY_H=360
GMENU2X_DISPLAY_X=0
GMENU2X_DISPLAY_Y=80
GMENU2X_DISPLAY_ROTATION=CW
GMENU2X_DISPLAY_BPP=32
GMENU2X_FBDEV=/dev/fb0

/root/sdl_landscape_env.sh now exports SDL_VIDEO_FBCON_ROTATION from
GMENU2X_DISPLAY_ROTATION when set. If unset, rotation is disabled.

SDL source changed:
~/LicheePi_Nano/buildroot-2018.02.11/output/build/sdl-1.2.15/src/video/fbcon/SDL_fbvideo.c
- phys_w/phys_h now preserve the real framebuffer vinfo.xres/yres even when SDL
  logical width/height are swapped for CW/CCW rotation.
- visible offset clamp accounts for rotated physical footprint:
  CW/CCW: visible_phys_w=visible_h, visible_phys_h=visible_w.
- This prevents GMENU2X_DISPLAY_Y=80 from being clamped incorrectly when logical
  mode is 480x360 on a physical 360x640 framebuffer.

Build/deploy:
cd ~/LicheePi_Nano/buildroot-2018.02.11/output/build/sdl-1.2.15
rm -f build/.libs/SDL_fbvideo.o build/.libs/libSDL-1.2.so.0.11.4
make -j4
make DESTDIR=~/LicheePi_Nano/buildroot-2018.02.11/output/target install

cd ~/LicheePi_Nano/gmenu2x
make -f Makefile.f1c200s -j4

cd ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle
./sync_overlay_to_buildroot.sh
./collect_runtime_payload.sh

Board deployment:
Board IP used: 10.67.68.247
Installed /usr/lib/libSDL-1.2.so.0.11.4 md5:
6225ecbf981739410ecb43de872e8fbb

After restart, /tmp/gmenu2x_rotate90.log shows:
Surface double buffer: screen=480x360 32bpp pitch=1920 raw=480x360 32bpp pitch=1920

Framebuffer capture verification:
Captured /dev/fb0 with stride 1536, 640 lines.
Non-black bbox in physical visible 360x640 area:
(0,80)-(360,560)
Top 80 rows non-black pixels: 0
Center 480 rows non-black pixels: 164784
Bottom 80 rows non-black pixels: 0

Local work/artifacts:
C:\Users\26301\Desktop\F1C200S_display_360x480_window\fb0_rotate90.png
Host backup:
~/LicheePi_Nano/backups/display_rotate90_20260620_082438
Board backup:
/root/roms/runtime_backups/display_rotate90_*

Caveat:
This covers SDL fbcon apps that source /root/sdl_landscape_env.sh. Any emulator
that bypasses SDL and writes /dev/fb0 directly still needs equivalent rotation
or must be routed back through SDL/shared display config.
```

## RTL8723BU Bluetooth Continuation - 2026-06-23
```text
Context:
User wants to continue adapting RTL8723BU Bluetooth after WiFi throughput work
was abandoned. Board may be offline during this work; continue host-side source
and Buildroot/runtime preparation when the board is unavailable.

Important recovery note:
During WiFi experiments, a no-BT-coexist 8723bu.ko test module was deployed and
made WiFi fail after reboot. If the board boots without SSH, recover locally or
over serial:

cp /root/roms/runtime_backups/rtl8723bu_no_btcoex_19700101_000748/8723bu.ko.before /lib/modules/5.7.1/extra/8723bu.ko
sync
busybox reboot

The known working WiFi module before that test is:
63589b67c073c5127d5bc86a6a481504  /lib/modules/5.7.1/extra/8723bu.ko

Host-side WiFi driver source was restored to keep Bluetooth coexist support:
~/LicheePi_Nano/third_party/rtl8723bu/Makefile
- CONFIG_BT_COEXIST = y
- core/rtw_btcoex.o is included in rtk_core.

~/LicheePi_Nano/third_party/rtl8723bu/hal/usb_halinit.c
- Restored original #else branch:
  rtw_btcoex_HAL_Initialize(padapter, _TRUE);

Restored host build product:
~/LicheePi_Nano/third_party/rtl8723bu/8723bu.ko
md5: cba9ab27419ce54e86ef8cdd15864296
This module is built with CONFIG_BT_COEXIST=y and should be the Bluetooth
baseline for future deployments.
```

Kernel Bluetooth support already present:
```text
CONFIG_BT=y
CONFIG_BT_BREDR=y
CONFIG_BT_LE=y
CONFIG_BT_RTL=y
CONFIG_BT_HCIBTUSB=y
CONFIG_BT_HCIBTUSB_RTL=y
CONFIG_RFKILL=y

btusb and btrtl are built into the kernel, not external modules:
drivers/bluetooth/btusb.o
drivers/bluetooth/btrtl.o
```

Buildroot / runtime Bluetooth support already present:
```text
BR2_PACKAGE_DBUS=y
BR2_PACKAGE_BLUEZ5_UTILS=y
BR2_PACKAGE_BLUEZ5_UTILS_CLIENT=y
BR2_PACKAGE_BLUEZ5_UTILS_DEPRECATED=y
BR2_PACKAGE_UTIL_LINUX_RFKILL=y

Verified target files:
~/LicheePi_Nano/buildroot-2018.02.11/output/target/usr/bin/bluetoothctl
~/LicheePi_Nano/buildroot-2018.02.11/output/target/usr/bin/btmon
~/LicheePi_Nano/buildroot-2018.02.11/output/target/usr/bin/hciconfig
~/LicheePi_Nano/buildroot-2018.02.11/output/target/usr/bin/hcitool
~/LicheePi_Nano/buildroot-2018.02.11/output/target/usr/libexec/bluetooth/bluetoothd
~/LicheePi_Nano/buildroot-2018.02.11/output/target/usr/sbin/rfkill
~/LicheePi_Nano/buildroot-2018.02.11/output/target/etc/init.d/S30dbus

Runtime helper scripts are in overlay and target:
~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/root/start_bluetooth.sh
~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/root/check_bluetooth.sh
~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/root/scan_bluetooth.sh

Overlay was synced to Buildroot target after rechecking scripts:
cd ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle
./sync_overlay_to_buildroot.sh
ROOT_PREFIX=~/LicheePi_Nano/buildroot-2018.02.11/output/target \
  rootfs_overlay/root/install_f1c200s_sdl_apps.sh
```

Firmware status:
```text
Current firmware file in both Buildroot target and runtime overlay:
/lib/firmware/rtl_bt/rtl8723b_fw.bin
size: 45048
md5: b3363f17ba07a53bfd891fe1b4072e0b

Do not blindly replace this with third_party/rtl8723bu/rtl8723bu_bt.bin unless
board dmesg proves this firmware is rejected. btrtl requests
rtl_bt/rtl8723b_fw.bin for RTL8723B.

Missing rtl8723b_config.bin may be logged but btrtl marks 8723B config_needed
false in the current kernel source.
```

btusb VID:PID fix prepared:
```text
Windows identifies the USB Bluetooth interface as:
USB\VID_0BDA&PID_B720&MI_00

Kernel source originally had generic Realtek class matching and one explicit
8723BU ID:
{ USB_DEVICE(0x7392, 0xa611), .driver_info = BTUSB_REALTEK },

It did not have explicit 0bda:b720. If the composite Bluetooth interface does
not match the generic class rule, btusb will not bind and no hci0 appears.

Source changed:
~/LicheePi_Nano/linux/drivers/bluetooth/btusb.c

Added under "Additional Realtek 8723BU Bluetooth devices":
{ USB_DEVICE(0x0bda, 0xb720), .driver_info = BTUSB_REALTEK },

Host backup:
~/LicheePi_Nano/backups/kernel_btusb_rtl8723bu_b720_20260622_184622

Kernel build command used:
cd ~/LicheePi_Nano/linux
export PATH=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin:$PATH
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- -j8 zImage suniv-f1c100s-licheepi-nano.dtb

Build result:
btusb.o rebuilt successfully.
zImage rebuilt successfully.

Host products:
~/LicheePi_Nano/linux/arch/arm/boot/zImage
~/LicheePi_Nano/linux/arch/arm/boot/dts/suniv-f1c100s-licheepi-nano.dtb

Local Windows artifacts:
C:\Users\26301\Desktop\F1C200S_rtl8723bt_work\zImage.btusb_b720
C:\Users\26301\Desktop\F1C200S_rtl8723bt_work\suniv-f1c100s-licheepi-nano.btusb_b720.dtb

Artifact md5:
zImage.btusb_b720 md5: 4f238bff60d86a33d4b4afc0bc532026
suniv-f1c100s-licheepi-nano.btusb_b720.dtb md5: afb742d84e4b6583c03f6261a6cfcc20

Deployment is pending because board was not online.
```

Board-side deployment/test plan when online:
```sh
# First recover WiFi if the no-BT-coexist module is still installed.
md5sum /lib/modules/5.7.1/extra/8723bu.ko

# Deploy zImage.btusb_b720 to the real boot partition.
# Do not use /mnt/boot unless verified mounted. Real boot partition is
# /dev/mmcblk0p1 and should be mounted under /tmp/bootp1 or another verified dir.
mkdir -p /tmp/bootp1
mount /dev/mmcblk0p1 /tmp/bootp1
cp /tmp/bootp1/zImage /tmp/bootp1/zImage.before_btusb_b720_$(date +%Y%m%d_%H%M%S) 2>/dev/null || true
cp /tmp/bootp1/suniv-f1c100s-licheepi-nano.dtb /tmp/bootp1/suniv-f1c100s-licheepi-nano.dtb.before_btusb_b720_$(date +%Y%m%d_%H%M%S) 2>/dev/null || true
cp /path/to/zImage.btusb_b720 /tmp/bootp1/zImage
cp /path/to/suniv-f1c100s-licheepi-nano.btusb_b720.dtb /tmp/bootp1/suniv-f1c100s-licheepi-nano.dtb
sync
umount /tmp/bootp1
busybox reboot

# After reboot:
dmesg | grep -iE 'bluetooth|btusb|btrtl|rtl.*bt|8723|firmware|hci|rfkill'
ls -l /sys/class/bluetooth /sys/class/rfkill
/root/check_bluetooth.sh
/root/start_bluetooth.sh
hciconfig -a
bluetoothctl show
bluetoothctl scan on
```

Expected next diagnostic:
```text
If hci0 appears after btusb 0bda:b720 patch:
- Continue with BlueZ scan/pair/trust/connect workflow.
- Then decide whether to add Bluetooth audio support.

If hci0 still does not appear:
- Inspect lsusb / sysfs interface descriptors for 0bda:b720 MI_00.
- Check whether btusb bound to the interface:
  readlink /sys/bus/usb/devices/*:*/driver
- Inspect exact dmesg firmware request/failure.
- Possible remaining issue: USB interface class does not bind or the adapter's
  Bluetooth side is not exposed on the board's USB wiring/power state.
```

Bluetooth audio direction, not yet configured:
```text
The current rootfs has BlueZ control tools and bluetoothd, but no confirmed
Bluetooth audio backend.

For F1C200S, prefer bluez-alsa instead of PulseAudio because PulseAudio is too
heavy for this RAM/CPU budget.

Two possible roles:

1. Board as Bluetooth audio source connecting to a Bluetooth speaker:
   - Need hci0 working.
   - Need pair/trust/connect to speaker using bluetoothctl.
   - Need bluealsa daemon with A2DP source support.
   - Playback command is expected to use an ALSA PCM like:
     aplay -D bluealsa:DEV=<speaker-mac>,PROFILE=a2dp file.wav
   - Exact bluealsa command depends on packaged version.

2. Board as Bluetooth speaker / A2DP sink:
   - Need hci0 working.
   - Need bluealsa daemon with A2DP sink support.
   - Make board discoverable/pairable:
     bluetoothctl
     power on
     agent on
     default-agent
     discoverable on
     pairable on
   - Use bluealsa-aplay to route received audio to local sound card, likely:
     bluealsa-aplay -D hw:3,0 00:00:00:00:00:00
   - Local I2S DAC is card 3 on recent board logs:
     3 [I2Smaster] I2S-master
   - F1C100s internal codec is card 2 and may not have a simple Master mixer.

Buildroot still needs bluez-alsa or an equivalent lightweight A2DP backend.
Do not assume Bluetooth audio works merely because bluetoothctl works.
First milestone remains: hci0 appears and scan/pair works.
```

Bluetooth bring-up verified - 2026-06-23:
```text
Board access during test:
- WiFi/SSH working again at 10.67.68.247 after RTL8723BU conservative WiFi
  parameter fix.

Verified Bluetooth state:
- hci0 exists:
  /sys/class/bluetooth/hci0
- Firmware loads:
  rtl_bt/rtl8723b_fw.bin
- Missing rtl8723b_config.bin is logged, but hci0 still works.
- `hciconfig hci0` reports:
  UP RUNNING
  BD Address: 00:1F:05:80:56:FE
  HCI Version: 4.0
  Manufacturer: Realtek Semiconductor Corporation

Scan result:
- `hcitool scan` works and found devices, including:
  7C:FD:82:59:0F:62
  5C:BA:EF:3A:DF:EC DESKTOP-20PEF44

Script fixes:
- `/root/start_bluetooth.sh` previously failed because `bluetoothd` is not in
  PATH. It is located at:
  /usr/libexec/bluetooth/bluetoothd
- The script now explicitly uses that path if `command -v bluetoothd` fails.
- The script now removes stale dbus pid files before starting dbus if no
  dbus-daemon is running:
  /var/run/messagebus.pid
  /var/run/dbus/pid
- Removed `bluetoothctl show` from start script because it can hang in
  non-interactive SSH.
- `/root/scan_bluetooth.sh` now prefers the verified `hcitool scan` path before
  falling back to bluetoothctl.

Files updated:
- Board:
  /root/start_bluetooth.sh
  /root/scan_bluetooth.sh
- Host overlay:
  ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/root/start_bluetooth.sh
  ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/root/scan_bluetooth.sh
- Buildroot target synced through:
  cd ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle
  ./sync_overlay_to_buildroot.sh

Remaining work:
- Pair/connect workflow still needs testing with a real target device.
- Bluetooth audio still needs bluez-alsa or another lightweight A2DP backend;
  basic BlueZ control and classic scanning now work.
```

## RTL8723BU WiFi Stable Scan Fix - 2026-06-23
```text
Symptom:
- After enabling Bluetooth/kernel btusb work, board booted with wlan0 present
  but no network.
- wpa_supplicant stayed SCANNING.
- `iwlist wlan0 scan` returned `No scan results`, so it was not a password or
  DHCP problem.

Diagnostics:
- USB device was high-speed and active:
  0bda:b720 speed=480 product=802.11n WLAN Adapter
- rfkill showed WiFi not blocked.
- EFUSE could be read, so USB/chip register access was alive.
- Unbinding btusb interfaces 1-1:1.0/1-1:1.1 did not restore scan.
- Tested multiple modules:
  /lib/modules/5.7.1/extra/8723bu.ko md5 63589b67c073c5127d5bc86a6a481504
  /tmp/8723bu_old.ko md5 b8d7773b4cb2a52ef93e648c45b77e89
  /tmp/8723bu_cba9.ko md5 cba9ab27419ce54e86ef8cdd15864296
- The module version was not the direct cause.

Root-cause configuration:
- The previous high-throughput options caused scan to return empty:
  rtw_ht_enable=1
  rtw_bw_mode=0x21
  rtw_wmm_enable=1
  rtw_ampdu_enable=2
  rtw_usb_rxagg_mode=1
- Conservative legacy/20MHz settings restored scanning immediately.

Stable startup parameters now used:
MODULE_OPTS="rtw_power_mgnt=0 rtw_ips_mode=0 rtw_smart_ps=0 rtw_low_power=0 rtw_enusbss=0 rtw_btcoex_enable=0 rtw_ht_enable=0 rtw_bw_mode=0 rtw_wmm_enable=0 rtw_ampdu_enable=0 rtw_usb_rxagg_mode=0 rtw_wifi_spec=1 rtw_channel_plan=0x7F rtw_antdiv_cfg=0 rtw_ant_num=1 rtw_adaptivity_en=0"

Files updated:
- Board:
  /etc/init.d/S17rtl8723bu
- Host overlay:
  ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/etc/init.d/S17rtl8723bu
- Buildroot target synced:
  ~/LicheePi_Nano/buildroot-2018.02.11/output/target/etc/init.d/S17rtl8723bu

Verified after fix:
- `iwlist wlan0 scan` sees APs including `wnk641_2.4G`.
- wpa_supplicant connects to ssid `wnl64`.
- DHCP obtains:
  10.67.68.247
- `wpa_cli -i wlan0 status` reports:
  wpa_state=COMPLETED

Tradeoff:
- This restores reliable WiFi by disabling 802.11n HT/WMM/AMPDU/RX aggregation.
- Throughput will be lower than the unstable high-throughput config.
- Future performance work should re-enable only one option at a time, starting
  from this known-good baseline.
```

## SFC retro-go_chaeng Investigation - 2026-06-23
```text
User pointed to host repository:
~/Downloads/retro-go_chaeng

Purpose:
Earlier SFC/PocketSNES/Snes9x4D work still felt poor on F1C200S. User noted
that retro-go runs SNES on ESP32, so it may provide a higher-success approach.

Inspection result:
- SNES code lives in:
  ~/Downloads/retro-go_chaeng/retro-core/components/snes9x
  ~/Downloads/retro-go_chaeng/retro-core/main/main_snes.c
- It is a small Snes9x-derived core, README says likely based on libretro
  snes9x2010.
- Component build flags:
  -DRIGHTSHIFT_IS_SAR
  -DFAST_LSB_WORD_ACCESS
  -DNO_ZERO_LUT
  -O2
- Video path is direct 16bpp RGB565:
  rg_surface_create(SNES_WIDTH, SNES_HEIGHT_EXTENDED, RG_PIXEL_565_LE, 0)
  GFX.Pitch = SNES_WIDTH * 2
- Audio sample rate in shared.h is 32000 Hz.
- APU can be toggled off from the retro-go menu.
- Critical performance setting:
  main_snes.c sets app->frameskip = 3 by default.
  Its main loop then skips rendering for that many frames after a rendered
  frame unless dynamic logic overrides. So retro-go is not evidence that full
  60fps full-render SNES is easy on weaker hardware; it relies on fixed
  frameskip and a very thin host loop.

Comparison with current host Snes9x4D:
- Current default SFC launcher:
  /root/run_snes9x4d_console.sh
  exec /root/snes9x4d -r 5 -b 512 -mono -ne -mfs 3 "$ROM_PATH"
- In current Snes9x4D source, -mfs 3 only sets MaxAutoFrameSkip for auto mode.
  It is not the same as retro-go app->frameskip = 3.
- Current Snes9x4D supports original fixed frame skip with:
  -f 3
  because src/snes9x.cpp parses -f/-frameskip into Settings.SkipFrames.
- Current Snes9x4D reset defaults keep:
  Settings.SkipFrames = AUTO_FRAMERATE
  Settings.SoundPlaybackRate = 5
  Settings.SoundBufferSize = 512
  Settings.Transparency = TRUE
  Settings.SixteenBit = TRUE
  Settings.SupportHiRes = FALSE
  Settings.ShutdownMaster = TRUE
- Current ARM Makefile already uses ARM926 tuning and strong optimization:
  -Ofast -march=armv5te -mtune=arm926ej-s -marm -flto=4
  -DFAST_ALIGNED_LSB_WORD_ACCESS
  -DFOREVER_16_BIT
  -DFOREVER_16_BIT_SOUND
  -DLAGFIX
  -DMIYOO
  -DSNESADVANCE_SPEEDHACKS
- Do not blindly copy retro-go -DFAST_LSB_WORD_ACCESS to F1C200S/ARM926.
  ARM may fault or behave badly on unaligned 16-bit word access. The current
  ARM build uses FAST_ALIGNED_LSB_WORD_ACCESS for a reason.
- retro-go -DNO_ZERO_LUT may be a possible low-risk experiment, but it changes
  16bpp color subtraction from a 64 KiB LUT to inline arithmetic. It saves RAM
  but may or may not improve speed.

Recommended next SFC tests:
1. First test current Snes9x4D with true fixed frameskip instead of auto:
   change launcher from:
     -r 5 -b 512 -mono -ne -mfs 3
   to a test variant:
     -r 5 -b 512 -mono -ne -f 3
   If stable but visibly choppy, test -f 2 as a quality/speed compromise.
2. If fixed frameskip still feels bad, build a "thin launcher" variant around
   current Snes9x4D or the retro-go snes9x2010 core:
   - no SDL menu
   - direct evdev input
   - direct framebuffer/SDL shared display output
   - 32000 Hz mono audio
   - fixed frameskip option exposed by launcher
3. Treat full core migration from retro-go as larger work:
   it needs a Linux host shim for display, ALSA, evdev, ROM loading, saves,
   and gmenu2x launcher integration.
```

## SFC snes9x4d-rs90 Investigation - 2026-06-23
```text
User pointed to host repository:
~/Downloads/snes9x4d-rs90

Purpose:
This is for lower-performance Linux handhelds, so it is more directly relevant
than retro-go ESP32 when looking for practical SFC compromises.

Repository/source:
- Remote:
  https://github.com/drowsnug95/snes9x4d-rs90.git
- Recent commits observed:
  8cef959 fix 204line scaler
  750c384 fix Tranparent math on COLOR_SUB1_2
  0b90e52 Code Cleanup
  20936b2 remove COLOR_SUB1_2, its unused.
  e633d81 Revert "Enable FAST_LSB_WORD_ACCESS"
- Important: it explicitly reverted FAST_LSB_WORD_ACCESS. Do not copy that
  unsafe optimization to ARM/F1C200S.

Build/features:
- RS90 build is Makefile.rs90.
- It uses the dingux-sdl frontend:
  dingux-sdl/sdlmain.cpp
  dingux-sdl/sdlvideo.cpp
  dingux-sdl/scaler.cpp
- Makefile.rs90 defines:
  -D_RS90
  -DVAR_CYCLES
  -DCPU_SHUTDOWN
  -DSPC700_SHUTDOWN
  -DSPC700_C
  -D__SDL__
  -DDINGOO
  -DZLIB
- It also uses -Ofast, profile-use/branch probabilities when profile data
  exists, and MIPS-specific tuning.

Comparison with current F1C200S Snes9x4D:
- Current host Snes9x4D already has VAR_CYCLES and CPU_SHUTDOWN in src/port.h.
- Current Miyoo/directfb path already has VAR_CYCLES, CPU_SHUTDOWN, and
  DSPC700_SHUTDOWN in its Makefile.
- Therefore the main useful RS90 difference is not those CPU macros. The useful
  difference is RS90-specific graphics reduction.

Key RS90 performance strategy:
1. Render/display target is much smaller:
   dingux-sdl/sdlvideo.cpp:
     screen = SDL_SetVideoMode(240, 160, 16, SDL_HWSURFACE);
   Then it downscales SNES 256-wide output to 240x160.

2. Dedicated downscalers:
   dingux-sdl/scaler.cpp:
     downscale_208to160()
     downscale_224to160()
   These convert 256x208/224-like SNES visible areas to 240x160 using packed
   16bpp operations. This cuts both emulated visible-area work and SDL update
   bandwidth.

3. _RS90 source-level clipping:
   gfx.cpp contains many _RS90 branches marked "only draw visible area".
   Examples:
   - clips horizontal sprite drawing to roughly Left=8, Right=248 instead of
     0..256
   - clamps rendered Y range:
       StartY >= 8 when not scaled
       EndY <= 218 when not scaled
   - disables some 16bpp lookup tables/allocation paths and replaces some math.
   This is a real emulation-side speed tradeoff, not just post-scaling.

4. Audio defaults are not lower than current F1C200S defaults:
   RS90 defaults:
     Settings.SoundPlaybackRate = 3  // 16000 Hz
     Settings.Stereo = TRUE
     Settings.SoundBufferSize = 256
   Current F1C200S launcher often uses mono and lower/simple settings already.

5. Hotkeys/menu are reduced:
   RS90 disables most save/load/reset hotkeys in S9xProcessEvents and keeps
   main menu only. This reduces event-path complexity but is not likely the
   primary speed win.

Practical next direction for F1C200S:
1. Do not try to use RS90 binary directly; it is MIPS.
2. Better first experiment:
   - Add a new F1C200S SFC test build mode based on the current Snes9x4D/Miyoo
     source.
   - Define an F1C200S equivalent of _RS90, e.g. -DF1C200S_LOWRES_SFC.
   - Port the safe visible-area clamps from RS90 gfx.cpp.
   - Add the 240x160 downscale path or an equivalent 256x224 -> small SDL
     surface path.
   - Let shared SDL scale/rotate/place that smaller surface into the current
     360x480 window.
3. Keep current normal SFC binary as fallback. Deploy the lowres build as a
   separate binary/menu entry first, e.g. /root/snes9x4d_lowres, until verified.
4. If lowres improves speed but looks too coarse, test a middle mode:
   render/crop to 256x208 or 256x216 and present without expensive 320x240
   scaling, relying on SDL display placement/nearest scaling instead.
```

## SFC Hardware Display Direction - 2026-06-23
```text
Important correction after reviewing retro-go_chaeng and snes9x4d-rs90:
The common useful idea is not "use 240x160" or "use ESP32 code". Both projects
avoid expensive host-side software presentation. They let the platform display
path handle final presentation as directly as possible.

For F1C200S, stop spending effort on SDL software scaling/rotation as the main
SFC performance path. The next serious SFC path should be:

1. Emulator core renders a small/native RGB565 frame:
   - SNES normal frame is about 256x224 RGB565.
   - Optional source crop can reduce this, but do not upscale in CPU.

2. Present that frame through DRM/DE hardware:
   - Use /dev/dri/card0.
   - Use an RGB565 dumb buffer or CMA-backed buffer.
   - Use DRM plane src/dst coordinates to scale the source frame to the active
     display window.
   - Let the sun8i UI scaler / DE hardware do scaling.

3. Current evidence in source:
   - Kernel `drivers/gpu/drm/sun4i/sun8i_ui_layer.c` enables hardware scaling
     when plane src size differs from dst size:
       sun8i_ui_scaler_setup(...)
       sun8i_ui_scaler_enable(..., true)
   - `cedar_drm_player` already has a useful DRM wrapper and RGB565 support:
       DRM_WARPPER_LAYER_MODE_RGB565
       drm_warpper_allocate_buffer()
       drm_warpper_mount_layer()
   - Existing wrapper currently mounts 1:1 only:
       dst_w = buf->width
       dst_h = buf->height
       src_w = buf->width << 16
       src_h = buf->height << 16
     So add a new function that accepts independent source and destination
     rectangles, for example:
       drm_warpper_mount_layer_scaled(layer, dst_x, dst_y, dst_w, dst_h,
                                      src_x, src_y, src_w, src_h, buf)

4. Frame update strategy:
   - Initialize DRM plane once with scaled src/dst.
   - Per frame, only switch the buffer address/fb quickly.
   - Reuse the cedar_drm_player fast ioctl path where possible, but note that
     current SRGN ioctl for RGB normal only switches physical address. Plane
     size/scaling setup should be done once through DRM/KMS or extended if
     dynamic size changes are needed.

5. Expected CPU benefit:
   - Writing a 256x224 RGB565 frame is about 112 KiB/frame.
   - Avoiding CPU-side upscale/rotate to the full SDL surface removes hundreds
     of KiB of per-frame pixel writes plus interpolation/rotation work.

6. Rotation caveat:
   - Hardware scaling is confirmed in code path.
   - Hardware 90-degree rotation is not yet confirmed.
   - If DE/DRM cannot rotate, keep scaling in hardware and handle rotation
     separately:
       a) have emulator/output code write rotated RGB565 layout, or
       b) do a lightweight CPU rotation without scaling, or
       c) investigate whether the display engine exposes rotation properties.

Recommended next implementation:
- Create a separate SFC test binary/menu entry, not replacing the current one:
  /root/snes9x4d_drm
- Build a minimal DRM presenter for 256x224 RGB565 frames using the
  cedar_drm_player wrapper style.
- Keep audio/input from the current known-working Snes9x4D launcher.
- Keep existing /root/snes9x4d and /root/run_snes9x4d.sh as fallback until the
  DRM presenter is verified on hardware.

Implementation status:
```text
2026-06-23:
User approved using SFC first to validate hardware display/scaling.

Blocked at this moment:
- Windows cannot reach host Linux SSH:
  wnk@192.168.175.135:22 timed out.
- Windows Desktop does not currently have local copies of:
  ~/LicheePi_Nano/third_party/snes9x4d_miyoo/dingux-sdl
  ~/LicheePi_Nano/third_party/cedar_drm_player

Resume immediately when host SSH is back:
1. Inspect:
   cd ~/LicheePi_Nano/third_party/snes9x4d_miyoo/dingux-sdl
   sed -n '1,220p' sdlvideo.cpp
   sed -n '1,150p' Makefile
   sed -n '760,850p' sdlmain.cpp

   cd ~/LicheePi_Nano/third_party/cedar_drm_player
   sed -n '259,455p' driver/drm_warpper.c
   sed -n '1,80p' config.h
   sed -n '1,80p' Makefile

2. Create host backup before changing source:
   ~/LicheePi_Nano/backups/sfc_drm_presenter_YYYYMMDD_HHMMSS
   Include BACKUP_LOG.txt with files copied and intended change.

3. Add DRM wrapper scaled mount function, likely in cedar_drm_player wrapper or
   copied local SFC presenter helper:
   int drm_warpper_mount_layer_scaled(drm_warpper_t *drm, int layer_id,
       int dst_x, int dst_y, int dst_w, int dst_h,
       int src_x, int src_y, int src_w, int src_h,
       buffer_object_t *buf)
   It should call drmModeSetPlane with:
       crtc_x = dst_x
       crtc_y = dst_y
       crtc_w = dst_w
       crtc_h = dst_h
       src_x = src_x << 16
       src_y = src_y << 16
       src_w = src_w << 16
       src_h = src_h << 16

4. Add a minimal SFC DRM presenter:
   - Open /dev/dri/card0.
   - Create 2 or 3 RGB565 dumb buffers sized 256x224 or 256x240.
   - Add FB2 with DRM_FORMAT_RGB565.
   - Initial setPlane uses src 256x224 and dst equal to the desired gmenu2x
     display window, read from /root/gmenu2x/display.conf when possible.
   - Each frame copies GFX.Screen/native SNES RGB565 to the next DRM buffer and
     flips the plane fb. If using the existing SRGN fast ioctl, keep scaling
     setup fixed from the initial drmModeSetPlane.

5. Integrate as a separate test binary:
   /root/snes9x4d_drm
   /root/run_snes9x4d_drm.sh
   Add a separate gmenu2x entry for testing only.
   Do not replace /root/snes9x4d or existing launcher.

6. Keep current input/audio behavior:
   - S9X_INPUT_DEV=/dev/input/event0
   - existing S9XKEYS mapping
   - AUDIODEV=hw:3,0/default as current launcher uses
   - Use current run_snes9x4d_console.sh as reference.

7. First runtime test:
   - Verify no SDL scaling path is active for SFC DRM output.
   - Check /tmp/snes9x4d_drm.log for plane size:
     src=256x224 dst=<display-window>
   - Capture framebuffer to confirm hardware-scaled image fills expected region.
   - Compare perceived smoothness before changing frameskip policy.
```
```

## SFC Emulator Cleanup - 2026-06-23
```text
User requirement:
- Remove previously installed/tested SNES/SFC emulator variants.
- Keep only the currently used/debugged SFC implementation.
- Current kept implementation:
  ~/LicheePi_Nano/third_party/snes9x4d_miyoo
  ~/LicheePi_Nano/third_party/snes9x4d_miyoo/snes9x4d_drm
  ~/LicheePi_Nano/third_party/snes9x4d_miyoo/run_snes9x4d_drm.sh

Host cleanup completed:
- Backup created before deletion:
  ~/LicheePi_Nano/backups/sfc_cleanup_20260623_010846
- Removed old host-side SFC/SNES emulator variants:
  ~/LicheePi_Nano/third_party/snes9x4d
  ~/LicheePi_Nano/third_party/snes9x4d-rzx50
  ~/LicheePi_Nano/third_party/PocketSNES-FunKey
  ~/LicheePi_Nano/third_party/PocketSNES-FunKey-clean
  ~/pocketsnes_f1c200s
- Removed old runtime payload entries:
  runtime_bundle/payload/pocketsnes
  runtime_bundle/payload/psnes_funkey
  runtime_bundle/payload/snes9x4d
- Removed old SFC launch scripts from runtime overlay and Buildroot target root:
  run_snes9x4d.sh
  run_snes9x4d_console.sh
  run_snes9x4d_from_gmenu2x.sh
  output/target/root/snes9x4d

Not removed:
- External reference repositories under ~/Downloads, such as retro-go_chaeng,
  snes9x4d-rs90, and miyoo_src. These are source references, not deployed
  runtime variants.
- Historical backups under ~/LicheePi_Nano/backups, except future cleanup may
  remove them only if explicitly requested.

Board cleanup status:
- Board root@10.67.68.247 was offline during cleanup.
- When the board is online, remove old board-side SFC runtime files while
  preserving the current/debugged SFC test only:
    rm -f /root/pocketsnes /root/psnes_funkey /root/snes9x4d
    rm -f /root/run_pocketsnes.sh /root/run_psnes_funkey.sh
    rm -f /root/run_snes9x4d.sh /root/run_snes9x4d_console.sh
    rm -f /root/run_snes9x4d_from_gmenu2x.sh
  Keep:
    /root/snes9x4d_drm
    /root/run_snes9x4d_drm.sh

Future rule:
- Do not add or deploy additional SNES/SFC emulator variants unless explicitly
  requested.
- If SFC work continues, use the current snes9x4d_miyoo / snes9x4d_drm path.
```

## SFC DRM Crash Logging / 1:1 Plane Fallback - 2026-06-23
```text
Symptom:
- Launching SFC from GMenu2X printed only gmenu2x startup/preview lines and appeared to return immediately.

Actions:
- Read AGENTS.md and F1C200S_build_commands.md after compaction before touching F1C200S work.
- Host backup created:
  ~/LicheePi_Nano/backups/sfc_crash_logfix_20260623_215926
- Current active SFC only remains:
  Host: ~/LicheePi_Nano/third_party/snes9x4d_miyoo
  Board: /root/snes9x4d_drm and /root/run_snes9x4d_drm.sh
- Launcher now writes all emulator stdout/stderr to:
  /tmp/snes9x4d_drm.log
- Launcher default DRM destination changed from 480x360 to 256x224 because prior testing showed KMS plane scaling rejects scaled dst sizes with errno 22, while 1:1 256x224 succeeds.
- Added SFC display init diagnostics in dingux-sdl/sdlvideo.cpp and startup/ROM diagnostics in dingux-sdl/sdlmain.cpp.

Build/deploy:
- Built on host with Buildroot app toolchain through dingux-sdl Makefile.
- Deployed to board root@10.67.68.247.
- Board md5 after deploy:
  bca6ae831f29ebed81505e9840632f2a  /root/snes9x4d_drm
  5467a58c9d8f88630c165e6a56f1b0f3  /root/run_snes9x4d_drm.sh

Verification:
- Manual board test:
  /root/run_snes9x4d_drm.sh /root/roms/sfc/165_恶魔城XX.smc
- Process stayed running after 5 seconds.
- /tmp/snes9x4d_drm.log showed ROM loaded and DRM presenter enabled:
  SFC_DRM: plane=31 crtc=47 src=256x224 dst=0,0 256x224
  SFC display: DRM presenter enabled
- Manual test process was killed afterward so GMenu2X can launch it cleanly.

Next SFC work:
- If GMenu2X still appears to return, immediately inspect /tmp/snes9x4d_drm.log rather than guessing.
- Smoothness remains unresolved; current 1:1 DRM avoids CPU scaling but does not fill the screen. Hardware DRM scaling still needs kernel/plane investigation because drmModeSetPlane scaled dst returned EINVAL earlier.
```

## Retro-Go SNES F1C200S Port - 2026-06-23
```text
User requested switching direction from current snes9x4d tweaks to porting:
~/Downloads/retro-go_chaeng

Source inspected:
- SNES core: ~/Downloads/retro-go_chaeng/retro-core/components/snes9x/src
- Frontend reference: ~/Downloads/retro-go_chaeng/retro-core/main/main_snes.c

Important source findings:
- retro-go SNES uses direct RGB565 buffer:
  GFX.Pitch = SNES_WIDTH * 2
  GFX.Screen = currentUpdate->data
- It defaults to fixed frameskip:
  app->frameskip = 3
- Therefore retro-go performance relies on a thin frontend plus fixed frame skipping, not full 60fps rendering.

New host port created:
~/LicheePi_Nano/third_party/retrogo_snes_f1c200s

Files:
- Makefile
- linux_main.c
- run_retrogo_snes.sh

Build:
- Uses Buildroot app toolchain:
  ~/LicheePi_Nano/buildroot-2018.02.11/output/host/bin/arm-buildroot-linux-gnueabi-
- References retro-go SNES source directly.
- Compile flags include:
  -DRIGHTSHIFT_IS_SAR -DNO_ZERO_LUT
  plus global includes for stddef/stdio/stdint to satisfy Linux build.

Host build result:
- Binary built successfully:
  ~/LicheePi_Nano/third_party/retrogo_snes_f1c200s/retrogo_snes_f1c200s
- md5:
  0a9f96609516ce81978beef89884fe9b  retrogo_snes_f1c200s
  c40360f569c41497ec4ee2ecd0f94cdd  run_retrogo_snes.sh

Board deployment:
- Deployed without replacing existing snes9x4d_drm:
  /root/retrogo_snes_f1c200s
  /root/run_retrogo_snes.sh
- Added separate GMenu2X entry:
  /root/gmenu2x/sections/emulators/sfc_retrogo
  title=SFC RetroGo
  exec=/root/run_retrogo_snes.sh

Runtime verification:
- Manual run with:
  /root/run_retrogo_snes.sh /root/roms/sfc/165_恶魔城XX.smc
- Process stays running after 5 seconds.
- Log:
  /tmp/retrogo_snes.log
- Log showed:
  retrogo-snes fb: 384x640 bpp=32 stride=1536 size=1966080
  retrogo-snes running rom=/root/roms/sfc/165_恶魔城XX.smc frameskip=3

Known issue:
- ALSA opens but snd_pcm_set_params fails on both default and hw:3,0:
  ALSA lib pcm.c:8498:(snd_pcm_set_params) Unable to set hw params for PLAYBACK: Invalid argument
- Current run can test video/input but audio needs explicit ALSA hw/sw params matching the board device.

Next steps:
1. User should test SFC RetroGo menu entry for visual smoothness.
2. If visual smoothness is better, fix ALSA by querying hw params and using supported format/rate/channels.
3. Add better presentation path later: current first port writes 256x224 RGB565 into fb0 32bpp at 1:1, no scaling.
4. Existing /root/snes9x4d_drm remains available as fallback.
```

## Retro-Go SNES Smoothness Pass - 2026-06-23
```text
Problem after first retro-go SNES port:
- User reported it is playable and no longer slows down, CPU usage is acceptable, but there is still slight stutter.

Fixes applied:
1. Disabled audio by default in launcher:
   RETROGO_SNES_AUDIO=0
   Reason: ALSA snd_pcm_set_params fails on current board for default/hw:3,0, but the first port still kept pcm non-null and could enter failed write/recover paths.
2. audio_init now closes pcm and disables audio if snd_pcm_set_params fails.
3. Added framebuffer double-buffer pan support:
   - Detects yres_virtual >= yres * 2 and smem_len enough for two pages.
   - Draws to alternating page.
   - Calls FBIOPAN_DISPLAY after each drawn frame.

Build/deploy:
- Host:
  ~/LicheePi_Nano/third_party/retrogo_snes_f1c200s
- Board:
  /root/retrogo_snes_f1c200s
  /root/run_retrogo_snes.sh
- md5 after deploy:
  41f775eda96c29f4702b65dd107081c6  /root/retrogo_snes_f1c200s
  0bb690db0707fcd72440293ec8b3aec2  /root/run_retrogo_snes.sh

Verification:
- Manual run stayed alive after 5 seconds.
- /tmp/retrogo_snes.log shows:
  retrogo-snes fb: 384x640 virtual_y=1280 bpp=32 stride=1536 size=1966080 pages=2
  retrogo-snes audio disabled
  retrogo-snes running rom=/root/roms/sfc/165_恶魔城XX.smc frameskip=3

Current test path:
- Use GMenu2X entry `SFC RetroGo`.
- Log file:
  /tmp/retrogo_snes.log

Next work:
- If stutter remains, test RETROGO_SNES_FRAMESKIP=2 vs 3 and inspect whether pan itself tears/stutters.
- Fix ALSA separately with explicit hw/sw params instead of snd_pcm_set_params.
```

## GMenu2X Built-In Functional Apps Rule - 2026-06-24
```text
User requirement:
- Functional items in GMenu2X must not simply launch a shell command and immediately end.
- They should be integrated into the GMenu2X UI like the file manager experience.
- Use GMenu2X APIs/dialogs so soft keyboard/input dialogs can be reused.

Implementation direction:
- Do not wrap shell scripts with a separate SDL app.
- Add GMenu2X built-in app handling through LinkApp using a new link key:
  builtin=<name>
- Link files keep exec=/bin/true only as an existence-compatible placeholder.
- LinkApp::run() intercepts builtin entries before selector/launch and stays inside GMenu2X.

Source changes:
- ~/LicheePi_Nano/gmenu2x/src/builtinapps.h
- ~/LicheePi_Nano/gmenu2x/src/builtinapps.cpp
- ~/LicheePi_Nano/gmenu2x/src/linkapp.h
- ~/LicheePi_Nano/gmenu2x/src/linkapp.cpp

Built-in apps added:
- builtin=wifi_scan
  Uses GMenu2X TextDialog to show iwlist results.
- builtin=bluetooth_start
  Runs /root/start_bluetooth.sh and shows a simplified in-GMenu2X status page.
- builtin=bluetooth_scan
  Starts Bluetooth if needed, scans with hcitool/bluetoothctl, then shows an
  in-GMenu2X selectable device list instead of raw logs.
- builtin=rom_server
  Uses GMenu2X InputDialog soft keyboard to edit port, starts /root/rom_httpd, shows URL in TextDialog.
- builtin=poweroff
  Uses GMenu2X MessageBox confirmation, then /sbin/poweroff.
- builtin=reboot
  Uses GMenu2X MessageBox confirmation, then /sbin/reboot.

Generator updated:
- ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/root/install_f1c200s_sdl_apps.sh
- Generated application entries now contain builtin=... for:
  bluetooth, bluetooth_scan, wifi_scan, rom_server, poweroff, reboot

Build/sync result:
- GMenu2X build and pack succeeded:
  cd ~/LicheePi_Nano/gmenu2x
  make -f Makefile.f1c200s -j4 pack
- Runtime overlay synced and Buildroot target updated through:
  cd ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle
  ./sync_overlay_to_buildroot.sh
  ./collect_runtime_payload.sh
  ./install_payload_to_target.sh

Current host artifacts:
- baa593a5bddff41e93a4b82d99ddef26  ~/LicheePi_Nano/gmenu2x/dist/f1c200s/gmenu2x
- 255dc458bce5deb1f05d8be10dd6bb1e  ~/LicheePi_Nano/gmenu2x/dist/gmenu2x-f1c200s.tar.gz

Buildroot target verification:
- builtin entries present under:
  ~/LicheePi_Nano/buildroot-2018.02.11/output/target/root/gmenu2x/sections/applications

Board deployment:
- Not deployed in this step because the board is not present/online.
- When board is online, deploy updated gmenu2x package and regenerated sections.
```

## GMenu2X Built-In Browser - 2026-06-24
```text
Correction from user:
- Browser still had no usable external UI and returned to GMenu2X immediately.
- Functional tools should stay inside GMenu2X and use GMenu2X APIs, same overall UX as the file manager.

Fix:
- Added builtin=browser to GMenu2X builtin app handling.
- Browser now uses GMenu2X InputDialog soft keyboard to enter URL.
- It runs links in dump mode inside the GMenu2X process path:
  links -dump -width 80 <url>
  fallback: wget -qO- <url>
- Output is displayed using GMenu2X TextDialog, so it does not leave/relaunch GMenu2X and can be exited with GMenu2X keys.

Generator updated:
- ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/root/install_f1c200s_sdl_apps.sh
- Browser link now generates:
  builtin=browser
  exec=/bin/true

Build/sync result:
- GMenu2X build and pack succeeded.
- Runtime overlay and Buildroot target updated.
- Current host artifacts:
  9971ea8b326a5b8f3ad22b707e356d70  ~/LicheePi_Nano/gmenu2x/dist/f1c200s/gmenu2x
  9446b284923383357bc98f0c3c569370  ~/LicheePi_Nano/gmenu2x/dist/gmenu2x-f1c200s.tar.gz

Buildroot target verification:
- ~/LicheePi_Nano/buildroot-2018.02.11/output/target/root/gmenu2x/sections/applications/browser contains builtin=browser.

Board deployment:
- Not deployed in this step because board is not online/present.
```

## Hardware Scaling / Rotation Research - 2026-06-24
```text
User asked to inspect bare-metal project:
C:\Users\26301\Desktop\RISCV\F1C100S_keil开发文件\源码\裸机\F1C100S_JPEG解码与视频播放

Important source findings:
- The project is useful for hardware scaling research.
- It contains direct DEFE/DEBE/TCON code:
  Driver/Source/sys_defe.c
  Driver/Include/sys_defe.h
  Driver/Source/sys_lcd.c
  Driver/Include/reg-debe.h
- Video playback path decodes MJPEG to Y/UV buffers, then configures:
  Defe_Config_yuv_to_argb_video(...)
  DE_SCAL_Set_Scaling_Factor(...)
  DE_SCAL_Set_Init_Phase(...)
  DE_SCAL_Set_Scaling_Coef_for_video(...)
  DE_SCAL_Output_Select(0, 0)  // output to DEBE0
  Defe_conversion_buff(...)    // per-frame address switch only
- This proves the F1C100S display frontend/scaler can do hardware scaling and
  YUV/RGB CSC directly into DEBE/TCON, avoiding CPU-side full-frame scaling.
- No clear 90-degree rotation register/path was found in this project. It is
  useful for scaling, CSC, and direct video output, not yet proof of hardware
  rotation support.

Linux DRM source findings:
- Current kernel already has F1C100S/A10-style frontend support:
  ~/LicheePi_Nano/linux/drivers/gpu/drm/sun4i/sun4i_frontend.c
  ~/LicheePi_Nano/linux/drivers/gpu/drm/sun4i/sun4i_backend.c
- DTS has FE0 enabled and connected to BE0:
  arch/arm/boot/dts/suniv-f1c100s.dtsi
  arch/arm/boot/dts/suniv-f1c100s-licheepi-nano.dts
- `sun4i_backend_plane_is_supported()` rejects scaling unless a plane uses
  frontend.
- `sun4i_frontend_format_is_supported()` supports XRGB8888/BGRX8888/YUV, but
  not RGB565.
- Therefore the previous SFC DRM scaled-plane failure with RGB565 is expected:
  RGB565 is supported by backend but not frontend; backend alone rejects
  non-1:1 scaling, so drmModeSetPlane returns EINVAL.
- Follow-up comparison against the bare-metal JPEG/video project:
  The kernel and bare-metal project are not fully configured the same.
  Matching parts:
  * FE/DEFE register base and BE/DEBE route are the same hardware path.
  * Runtime resume enables FE clocks/reset and FE module, like bare-metal
    Defe_Init/Open_Dev_Clock/DE_SCAL_Enable.
  * Kernel frontend writes input size, output size, horizontal/vertical factors,
    input/output format, CSC bypass/coefs, REG_RDY, and FRM_START, matching the
    same register categories as the bare-metal DE_SCAL calls.
  * Kernel backend sets VDOEN and XRGB8888 layer format when frontend is used,
    equivalent to bare-metal enabling the video layer and selecting BE output.
  Important differences:
  * Bare-metal video path computes scaling factor, initial phase, and FIR
    coefficients with the actual video source/destination sizes every time
    Defe_Config_yuv_to_argb_video() is called.
  * Current kernel `sun4i_frontend_scaler_init()` computes FIR coefficients only
    at runtime_resume, with hardcoded 384x640 -> 384x640 YUV422 parameters.
    `sun4i_frontend_update_coord()` later updates actual in/out sizes and scale
    factors, but does not recompute FIR coefficients for the current plane.
  * Bare-metal calls DE_SCAL_Set_Init_Phase() with actual source offsets; kernel
    uses fixed phase defaults from frontend data and does not recompute dynamic
    phase from the current crop/offset.
  * Kernel supports XRGB8888/BGRX8888/YUV frontend inputs but not RGB565, while
    the bare-metal example mostly demonstrates YUV input and RGB888 output.
  Practical implication:
  * For SFC 256x224 -> 480x360, XRGB8888 should at least allow frontend scaling
    to be selected. If image quality/stability is poor, next kernel fix should
    move the bare-metal-style coefficient calculation into
    sun4i_frontend_update_coord() using the actual plane src/dst sizes instead
    of hardcoded scaler_init dimensions.

SFC test build prepared on host:
- Current kept SFC path remains:
  ~/LicheePi_Nano/third_party/snes9x4d_miyoo
- Host backup before change:
  ~/LicheePi_Nano/backups/sfc_xrgb_frontend_20260623_185419
- Local Windows work copy:
  C:\Users\26301\Desktop\F1C200S_sfc_xrgb_frontend
- Changed dingux-sdl/sdlmain.cpp DRM presenter:
  * DRM dumb buffers now use 32bpp / DRM_FORMAT_XRGB8888.
  * Plane probing now requires XRGB8888.
  * Per-frame present expands emulator RGB565 256x224 to XRGB8888 256x224.
  * Scaling destination remains independent and should now trigger frontend.
- Changed launcher:
  * SFC_DRM_DST_W defaults to ${SDL_FBCON_VISIBLE_W:-480}
  * SFC_DRM_DST_H defaults to ${SDL_FBCON_VISIBLE_H:-360}
  * Environment overrides still work for A/B tests.

Build result:
cd ~/LicheePi_Nano/third_party/snes9x4d_miyoo
make -C dingux-sdl clean
make -C dingux-sdl -j4
cp dingux-sdl/snes9x4d.dge snes9x4d_drm

Host artifacts:
e4db95cda3ee75a14ab7341d0785de48  ~/LicheePi_Nano/third_party/snes9x4d_miyoo/snes9x4d_drm
c201fad31ff9bfaf09e2a58664f1884d  ~/LicheePi_Nano/third_party/snes9x4d_miyoo/run_snes9x4d_drm.sh

Board deployment:
- Not deployed/tested in this step because root@10.67.68.247 SSH timed out.
- When board is online, deploy only these current SFC files:
  /root/snes9x4d_drm
  /root/run_snes9x4d_drm.sh
- First check after launch:
  cat /tmp/snes9x4d_drm.log
  Expected success clue:
    SFC_DRM: probe ... xrgb8888=1
    SFC_DRM: plane=... src=256x224 dst=0,0 480x360
  If it still says set failed errno=22, the frontend path is still being
  rejected and the next place to instrument is sun4i_backend_atomic_check().
```

## SFC Internal Rotation - 2026-06-24
```text
Scope:
- SFC only. No other emulator changes.
- Active source path modified:
  ~/LicheePi_Nano/third_party/snes9x4d_miyoo

Reason:
- F1C200S display hardware audit found scaling/CSC/composition but no true
  90-degree rotate/transpose register. For SFC, the lowest-cost internal
  rotation point is the emulator presenter, before the 256x224 frame is handed
  to DRM.

Host backup:
- ~/LicheePi_Nano/backups/sfc_internal_rotation_20260623_193531

Files changed:
- dingux-sdl/sdlmain.cpp
  * Added SFC_DRM_ROTATION parsing: none/cw/ccw/ud, also accepts 0/90/270/180.
  * For cw/ccw, DRM dumb BO source size changes from 256x224 to 224x256.
  * f1c_present_drm() rotates pixels while expanding SNES RGB565 to XRGB8888.
  * drmModeSetPlane source width/height now follows the rotated BO size.
  * Default remains none, so existing behavior is unchanged unless env is set.
- run_snes9x4d_drm.sh
  * Exports SFC_DRM_ROTATION=${SFC_DRM_ROTATION:-none}.
  * Logs rotation in /tmp/snes9x4d_drm.log.

Build:
cd ~/LicheePi_Nano/third_party/snes9x4d_miyoo
make -C dingux-sdl clean
make -C dingux-sdl -j4
cp dingux-sdl/snes9x4d.dge snes9x4d_drm

Build result:
- Build passed with only existing string-constant warnings.
- Host md5 after build:
  549677aa7d996351ab4c537f220e8b5c  snes9x4d_drm
  f2ae7f01499124f1954723e553844134  run_snes9x4d_drm.sh

Runtime test examples after deployment:
- Normal, unchanged:
  SFC_DRM_ROTATION=none /root/run_snes9x4d_drm.sh /root/roms/sfc/165_恶魔城XX.smc
- Rotate clockwise:
  SFC_DRM_ROTATION=cw /root/run_snes9x4d_drm.sh /root/roms/sfc/165_恶魔城XX.smc
- Rotate counter-clockwise:
  SFC_DRM_ROTATION=ccw /root/run_snes9x4d_drm.sh /root/roms/sfc/165_恶魔城XX.smc

Expected log clue:
- /tmp/snes9x4d_drm.log should include:
  SFC launcher: ... rotation=<mode>
  SFC_DRM: plane=... src=224x256 ... rotation=1/2 for cw/ccw

Deployment status:
- Built on host only in this step. Deploy to board when online/requested:
  scp ~/LicheePi_Nano/third_party/snes9x4d_miyoo/snes9x4d_drm root@10.67.68.247:/root/
  scp ~/LicheePi_Nano/third_party/snes9x4d_miyoo/run_snes9x4d_drm.sh root@10.67.68.247:/root/
  ssh root@10.67.68.247 'chmod 755 /root/snes9x4d_drm /root/run_snes9x4d_drm.sh'
```

## Conversation Handoff Summary - 2026-06-24
```text
Purpose:
- This section is the compact handoff for the next Codex conversation.
- Read AGENTS.md and this file before doing any F1C200S work.
- User explicitly corrected scope several times: when working on SFC, do not wander into other emulators unless explicitly requested.

Current host / board access:
- Host Linux: wnk@192.168.175.137, password 1.
- Board SSH currently: root@10.70.36.247, password 1.
- Previous board IP root@10.67.68.247 is stale on the current network.
- Board serial: COM3, 115200, login root, password 1.
- If SSH is down but board is reachable by serial, ymodem transfer is acceptable.

Toolchain rules:
- Kernel builds must use Linaro arm-linux-gnueabi toolchain:
  export PATH=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin:$PATH
  make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- ...
- Application builds use Buildroot app toolchain:
  ~/LicheePi_Nano/buildroot-2018.02.11/output/host/bin/arm-buildroot-linux-gnueabi-
- Do not use arm-buildroot-linux-gnueabi- for kernel builds.

Desktop cleanup status:
- Cleaned Windows Desktop temporary Codex/F1C200S work directories and one-off scripts after the SFC internal rotation build.
- Removed temporary dirs/files such as:
  F1C200S_*_work, F1C200S_sfc_* temporary work dirs, _codex_tmp_retrogo_apu,
  temporary 8723bu ko copies, serial upload/test scripts, and one-off SFC shell scripts.
- Kept durable docs:
  C:\Users\26301\Desktop\AGENTS.md
  C:\Users\26301\Desktop\F1C200S_build_commands.md
  C:\Users\26301\Desktop\F1C200S_ST7701_timing_notes.md

SFC current scope and source of truth:
- Only handle SFC when user asks about SFC. Do not inspect or change GBA/ONS/etc. during SFC tasks.
- Current kept/debugged SFC implementation remains:
  Host: ~/LicheePi_Nano/third_party/snes9x4d_miyoo
  Binary: ~/LicheePi_Nano/third_party/snes9x4d_miyoo/snes9x4d_drm
  Launcher: ~/LicheePi_Nano/third_party/snes9x4d_miyoo/run_snes9x4d_drm.sh
  Board paths when deployed:
    /root/snes9x4d_drm
    /root/run_snes9x4d_drm.sh
- There is also a Retro-Go SFC port used earlier for comparison:
  ~/LicheePi_Nano/third_party/retrogo_snes_f1c200s
  But the cleanup rule says do not add/deploy additional SNES/SFC variants unless explicitly requested.

SFC display/rotation conclusions:
- F1C200S hardware audit result:
  DEFE/DEBE/TCON support scaling/CSC/composition/B-R swap, but no real 90-degree rotate/transpose register.
- SDL fbcon has rotation support, but current SFC DRM path bypasses SDL video output.
- Therefore for SFC rotation, rotate inside the SFC presenter at source frame size, not as a full-screen framebuffer operation.
- Implemented in snes9x4d_miyoo DRM presenter:
  File: dingux-sdl/sdlmain.cpp
  Env: SFC_DRM_ROTATION=none|cw|ccw|ud
  Also accepts 0/90/270/180.
  Default is none, so existing behavior is unchanged if env is not set.
- For cw/ccw, DRM dumb BO source size changes from 256x224 to 224x256.
- f1c_present_drm() rotates while expanding SNES RGB565 to XRGB8888.
- drmModeSetPlane source width/height follows the rotated BO size.
- Launcher file run_snes9x4d_drm.sh exports:
  SFC_DRM_ROTATION=${SFC_DRM_ROTATION:-none}
  and logs it to /tmp/snes9x4d_drm.log.

Latest SFC internal rotation build:
- Host backup before change:
  ~/LicheePi_Nano/backups/sfc_internal_rotation_20260623_193531
- Build commands used:
  cd ~/LicheePi_Nano/third_party/snes9x4d_miyoo
  make -C dingux-sdl clean
  make -C dingux-sdl -j4
  cp dingux-sdl/snes9x4d.dge snes9x4d_drm
- Build passed with existing string-constant warnings only.
- Host md5 after build:
  549677aa7d996351ab4c537f220e8b5c  snes9x4d_drm
  f2ae7f01499124f1954723e553844134  run_snes9x4d_drm.sh
- Deployment was not done in that turn. Deploy when board is online/requested:
  scp ~/LicheePi_Nano/third_party/snes9x4d_miyoo/snes9x4d_drm root@10.67.68.247:/root/
  scp ~/LicheePi_Nano/third_party/snes9x4d_miyoo/run_snes9x4d_drm.sh root@10.67.68.247:/root/
  ssh root@10.67.68.247 'chmod 755 /root/snes9x4d_drm /root/run_snes9x4d_drm.sh'

SFC runtime test commands after deployment:
- Normal unchanged:
  SFC_DRM_ROTATION=none /root/run_snes9x4d_drm.sh /root/roms/sfc/165_恶魔城XX.smc
- Clockwise rotation:
  SFC_DRM_ROTATION=cw /root/run_snes9x4d_drm.sh /root/roms/sfc/165_恶魔城XX.smc
- Counter-clockwise rotation:
  SFC_DRM_ROTATION=ccw /root/run_snes9x4d_drm.sh /root/roms/sfc/165_恶魔城XX.smc
- Inspect:
  cat /tmp/snes9x4d_drm.log
- Expected log clues:
  SFC launcher: ... rotation=<mode>
  For cw/ccw: SFC_DRM: ... src=224x256 ... rotation=1/2

Known SFC issues / direction:
- User has repeatedly reported SFC smoothness/stutter issues, sometimes speed normal but frame pacing not smooth.
- Previous XRGB8888 DRM/frontend path was prepared to allow hardware scaling because RGB565 scaled planes were rejected with EINVAL.
- If continuing smoothness work, inspect /tmp/snes9x4d_drm.log first; do not guess.
- If scaled drmModeSetPlane still fails, instrument kernel sun4i backend/frontend atomic check path.
- Current DRM presenter uses XRGB8888 dumb buffers and expands 256x224 RGB565 to XRGB8888.

Other durable project facts from this long conversation:
- GMenu2X and SDL apps were moved to a 480x360 landscape visible window at one point, with SDL/FBCON visible-window env handling.
- GMenu2X functional scripts should be implemented as built-in GMenu2X UI entries using GMenu2X dialogs/API, not as external scripts that immediately return.
- Hard decode video uses cedar/ion drivers; old working GStreamer test involved loading sunxi_ion_core.ko and cedar_ve.ko, then omxh264dec. Later direction was to integrate audio into the direct hardware decode player, but board was offline during part of that work.
- WiFi RTL8723BU was unstable/slow; user later deprioritized WiFi. Bluetooth RTL8723B supports host mode for connecting Bluetooth speakers in principle; A2DP sink/source support depends on user-space BlueZ/audio stack configuration.
- ONS GBK path eventually displayed dialogue text correctly; ONS EN text issue was rooted in rendering/blending/encoding path discussions, but current SFC tasks should not touch ONS.

## Cedar In-Tree Integration - 2026-07-09
```text
Goal:
- Move the current Cedar hard-decode driver package from external obj-m style
  into the main kernel tree so it can be controlled by .config.

Source package:
- C:\Users\26301\Desktop\cedar_f1c200s_linux57

Local integration overlay:
- C:\Users\26301\Desktop\cedar_kernel_integration_overlay

Host kernel tree:
- ~/LicheePi_Nano/linux

Host backup for this change:
- ~/LicheePi_Nano/backups/kernel_cedar_intree_20260709_182312

Kernel tree location chosen:
- drivers/staging/media/sunxi/cedar

Files wired into the kernel tree:
- drivers/staging/media/sunxi/Kconfig
  added:
    source "drivers/staging/media/sunxi/cedar/Kconfig"
- drivers/staging/media/sunxi/Makefile
  added:
    obj-y += cedar/

Cedar build-system conversion:
- cedar/Makefile
  changed from external:
    obj-m += ve/
    obj-m += ion/
  to in-tree:
    obj-$(CONFIG_VIDEO_SUNXI_CEDAR_VE) += ve/
    obj-$(CONFIG_VIDEO_SUNXI_CEDAR_ION) += ion/
- cedar/ve/Makefile
  changed from:
    obj-m += cedar_ve.o
  to:
    obj-$(CONFIG_VIDEO_SUNXI_CEDAR_VE) += cedar_ve.o
- cedar/ion/Makefile
  changed from:
    obj-m += sunxi_ion_core.o
  to:
    obj-$(CONFIG_VIDEO_SUNXI_CEDAR_ION) += sunxi_ion_core.o

ION conflict guard:
- cedar/ion/Kconfig now depends on:
    ARCH_SUNXI && OF_ADDRESS && HAS_DMA && MMU && !ION
- Reason:
  the main kernel tree already contains Android ION under
  drivers/staging/android/ion, so the Cedar-specific sunxi_ion_core path
  should not be enabled together with CONFIG_ION.

Host-side config verification after scripts/config + olddefconfig:
- CONFIG_STAGING_MEDIA=y
- CONFIG_VIDEO_SUNXI=y
- CONFIG_VIDEO_SUNXI_CEDAR_VE=m
- CONFIG_VIDEO_SUNXI_CEDAR_ION=m

Host build command used:
- cd ~/LicheePi_Nano/linux
- export PATH=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin:$PATH
- make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- \
    drivers/staging/media/sunxi/cedar/ve/cedar_ve.ko \
    drivers/staging/media/sunxi/cedar/ion/sunxi_ion_core.ko -j8

Build result:
- PASS
- Produced:
  * ~/LicheePi_Nano/linux/drivers/staging/media/sunxi/cedar/ve/cedar_ve.ko
  * ~/LicheePi_Nano/linux/drivers/staging/media/sunxi/cedar/ion/sunxi_ion_core.ko

Practical conclusion:
- Cedar hard-decode support is no longer only an external module package.
- It is now integrated into the host kernel source tree as .config-controlled
  in-tree modules, while still building as loadable .ko artifacts.
- Board deployment and runtime validation were not done in this step.
```

## ADB Loopback Fix - 2026-07-09
```text
Symptom:
- `/root/adb start-server` or `/root/adb devices` hangs.
- Trace/log showed the client stuck at `host:version`.
- USB device enumeration was still correct, e.g. Google device `18d1:4ee7`
  appeared in `lsusb`.

Root cause:
- The board had a `lo` interface but no `127.0.0.1` address assigned.
- ADB 1.0.31 on the board uses a local client/server connection through TCP
  port 5037, so without loopback IP the local ADB handshake breaks.

Manual recovery:
- `busybox ifconfig lo 127.0.0.1 netmask 255.0.0.0 up`
- Then:
  * `/root/adb start-server`
  * `/root/adb devices`
- Verified result:
  `List of devices attached`
  `dda57287    device`

Persistent fix deployed to board:
- Added init script:
  `/etc/init.d/S02loopback`
- Script content brings up:
  `lo -> 127.0.0.1/8`
- Final permissions:
  `-rwxr-xr-x /etc/init.d/S02loopback`

Practical note:
- If ADB hangs again after boot, first check:
  `busybox ifconfig lo`
- If `inet addr:127.0.0.1` is missing, restore loopback before debugging USB.
```

## ESP8089 In-Tree Integration - 2026-07-10
```text
Goal:
- Move the currently working ESP8089 SPI WiFi driver from the external
  out-of-tree package into the main kernel tree, matching the cedar
  integration style.

Source package:
- ~/LicheePi_Nano/SoftWare/ESP8089-SPI-master

Local integration overlay:
- C:\Users\26301\Desktop\esp8089_kernel_integration_overlay

Host backup for this change:
- ~/LicheePi_Nano/backups/kernel_esp8089_intree_20260710_101958

Kernel tree location chosen:
- drivers/net/wireless/esp8089

Kernel wiring:
- drivers/net/wireless/Kconfig
  added:
    source "drivers/net/wireless/esp8089/Kconfig"
- drivers/net/wireless/Makefile
  added:
    obj-$(CONFIG_ESP8089_SPI) += esp8089/

Driver Kconfig added:
- drivers/net/wireless/esp8089/Kconfig
  config name:
    CONFIG_ESP8089_SPI
  current dependency shape:
    depends on WLAN && MAC80211 && SPI && GPIOLIB
    depends on CFG80211

Driver Makefile conversion:
- Replaced the old external-module Makefile with an in-tree Kbuild file.
- Current in-tree object:
    obj-$(CONFIG_ESP8089_SPI) += esp8089-spi.o
- Current object list:
    esp_debug.o
    sdio_sif_esp.o
    spi_sif_esp.o
    esp_io.o
    esp_file.o
    esp_main.o
    esp_sip.o
    esp_ext.o
    esp_ctrl.o
    esp_mac80211.o
    esp_utils.o
    esp_pm.o
    testmode.o
- `spi_stub.c` and `sdio_stub.c` remain included from the sif source files,
  matching the existing external build behavior.

Compiler flags preserved from external build:
- FAST_TX_STATUS
- RX_SENDUP_SYNC
- KERNEL_IV_WAR
- SIF_DSR_WAR
- HAS_INIT_DATA
- P2P_CONCURRENT
- HAS_FW
- ESP_ACK_INTERRUPT
- ESP_USE_SPI
- REGISTER_SPI_BOARD_INFO

Host-side config verification:
- CONFIG_ESP8089_SPI=m
- CONFIG_VIDEO_SUNXI_CEDAR_VE=m
- CONFIG_VIDEO_SUNXI_CEDAR_ION=m

Host build command used:
- cd ~/LicheePi_Nano/linux
- export PATH=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin:$PATH
- make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- olddefconfig
- make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- \
    drivers/net/wireless/esp8089/esp8089-spi.ko -j8

Build result:
- PASS
- Output:
  * ~/LicheePi_Nano/linux/drivers/net/wireless/esp8089/esp8089-spi.ko

Warnings observed:
- C90 mixed-declaration warnings in esp_main.c / eagle_fw1.h
- implicit-fallthrough warnings in esp_ctrl.c
- No link or modpost failure; module build completed successfully.

Cleanup:
- Removed generated `.o`, `.ko`, `.cmd`, `.mod*`, `Module.symvers`,
  `modules.order`, `built-in.a`, `dkms.conf`, and old backup source variants
  from the local overlay before syncing.

Deployment status:
- Host kernel tree integration completed.
- Host config enabled.
- Host module build verified.
- Board deployment was not completed in this step because the board went
  offline before the non-destructive upload of `esp8089-spi.intree.ko`.
```

## TVD In-Tree Integration - 2026-07-10
```text
Goal:
- Move the current F1C100S/F1C200S TVD capture driver from the external
  module tree into the main kernel tree, with .config control.

External source reference:
- C:\Users\26301\Desktop\tvd_f1c100s_linux57\src\suniv_f1c100s_tvd.c

Local integration overlay:
- C:\Users\26301\Desktop\tvd_kernel_integration_overlay

Host backup for this change:
- ~/LicheePi_Nano/backups/kernel_tvd_intree_20260710_103658

Kernel tree location chosen:
- drivers/media/platform/sunxi/suniv-tvd

Important pre-existing kernel state:
- DTS already contains the TVD device node in:
  `arch/arm/boot/dts/suniv-f1c100s.dtsi`
- Current node state in the host tree:
  `status = "okay"`
- Clocks/resets/interrupt fields for TVD were already present before this
  driver integration step.

Kernel wiring:
- drivers/media/platform/sunxi/Kconfig
  added:
    source "drivers/media/platform/sunxi/suniv-tvd/Kconfig"
- drivers/media/platform/sunxi/Makefile
  added:
    obj-$(CONFIG_VIDEO_SUNIV_F1C100S_TVD) += suniv-tvd/

Driver Kconfig added:
- drivers/media/platform/sunxi/suniv-tvd/Kconfig
  config name:
    CONFIG_VIDEO_SUNIV_F1C100S_TVD
  current dependency shape:
    depends on VIDEO_DEV && VIDEO_V4L2
    depends on ARCH_SUNXI || COMPILE_TEST
    depends on OF
  selected helpers:
    VIDEOBUF2_DMA_CONTIG
    VIDEOBUF2_V4L2
    DMA_SHARED_BUFFER

Driver Makefile added:
- drivers/media/platform/sunxi/suniv-tvd/Makefile
  content:
    obj-$(CONFIG_VIDEO_SUNIV_F1C100S_TVD) += suniv_f1c100s_tvd.o

Host-side config verification:
- CONFIG_VIDEO_SUNIV_F1C100S_TVD=m

Host build command used:
- cd ~/LicheePi_Nano/linux
- export PATH=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin:$PATH
- make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- olddefconfig
- make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- \
    drivers/media/platform/sunxi/suniv-tvd/suniv_f1c100s_tvd.ko -j8

Build result:
- PASS
- Output:
  * ~/LicheePi_Nano/linux/drivers/media/platform/sunxi/suniv-tvd/suniv_f1c100s_tvd.ko

Practical conclusion:
- TVD is now integrated into the host kernel tree as a .config-controlled
  in-tree module source, matching the cedar/esp8089 workflow.
- Board deployment was not done in this step because the board was offline.
```

## In-Tree Driver Runtime Verification - 2026-07-10
```text
Board IP during verification:
- 10.0.0.107

Uploaded test modules:
- /root/sunxi_ion_core.intree.ko
- /root/cedar_ve.intree.ko
- /root/esp8089-spi.intree.ko
- /root/suniv_f1c100s_tvd.intree.ko

Cedar verification:
- Loaded with:
  * /sbin/insmod /root/sunxi_ion_core.intree.ko
  * /sbin/insmod /root/cedar_ve.intree.ko
- Result:
  * PASS
  * /dev/ion present
  * /dev/cedar_dev present
  * /proc/modules shows:
    - sunxi_ion_core
    - cedar_ve

TVD verification:
- Loaded with:
  * /sbin/insmod /root/suniv_f1c100s_tvd.intree.ko
- Result:
  * PASS
  * /proc/modules shows suniv_f1c100s_tvd
  * new video node appeared as /dev/video7
  * dmesg confirms:
    - clock rates
    - DRAM gate enable
    - forced TVD clock
    - registered /dev/video node for TVD capture

ESP8089 verification:
- Existing running network already depended on esp8089_spi.
- /proc/modules showed:
  * esp8089_spi 258048 ...
- New in-tree module was uploaded as:
  * /root/esp8089-spi.intree.ko
- Old and new module files are not byte-identical:
  * old md5: b3d605dd601deec48bc29384e3d73b2d
  * new md5: 6c7ad8aa88f26488fec5621a4d61e728
- Hot replacement was intentionally not performed in-session because unloading
  the active esp8089_spi module would drop SSH immediately.
- Practical status:
  * build verified
  * upload verified
  * runtime swap pending an offline/serial-safe test window
```

## Runtime Bundle Normalization - 2026-07-10
```text
Scope:
- Normalize Buildroot runtime bundle so integrated drivers come from kernel
  build output rather than stale overlay copies.

Canonical local path:
- C:\Users\26301\Desktop\board_tools_f1c200s\runtime_bundle

Host path synced:
- ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle

Scripts updated:
- collect_runtime_payload.sh
- install_payload_to_target.sh
- sync_overlay_to_buildroot.sh

Behavior changes:
- collect_runtime_payload.sh now pulls integrated modules from kernel output:
  * sunxi_ion_core.ko from drivers/staging/media/sunxi/cedar/ion/
  * cedar_ve.ko from drivers/staging/media/sunxi/cedar/ve/
  * esp8089-spi.ko from drivers/net/wireless/esp8089/
  * suniv_f1c100s_tvd.ko from drivers/media/platform/sunxi/suniv-tvd/
- install_payload_to_target.sh now installs these modules via a common
  install_kmod helper into:
  * target/lib/modules/<kernel>/extra/
  with fallback to /root only if no module directory exists.
- sync_overlay_to_buildroot.sh no longer chmods `*.ko` under /root because
  integrated modules are no longer shipped there as canonical overlay files.

Overlay cleanup:
- Removed old integrated module copies from runtime overlay root:
  * cedar_ve.ko
  * sunxi_ion_core.ko
  * esp8089-spi.ko
- Removed backup/temp files from overlay root:
  * install_f1c200s_sdl_apps.sh.before_btfix
  * run_onscripter_gbk.sh.before_no_fallback_cursor_
  * sdl_landscape_env.sh.before_sdl_visible_20260619_095644

Overlay additions:
- etc/init.d/S02loopback
- etc/init.d/S18cedar

Launcher script cleanup:
- root/load_cedar.sh rewritten to search:
  * /lib/modules/$(uname -r)/extra/
  * fallback /root
- root/load_tvd.sh rewritten to prefer:
  * /lib/modules/$(uname -r)/extra/suniv_f1c100s_tvd.ko
  * fallback /root/suniv_f1c100s_tvd.ko
- root/load_wifi.sh rewritten to prefer:
  * /lib/modules/$(uname -r)/extra/esp8089-spi.ko
  * fallback /root/esp8089-spi.ko

Current deployment rule after 2026-07-10 validation:
- ESP8089 default source remains the kernel tree build:
  `~/LicheePi_Nano/linux/drivers/net/wireless/esp8089/esp8089-spi.ko`
- Backup snapshot kept at:
  `~/LicheePi_Nano/board_tools_f1c200s/prebuilt_modules/esp8089-spi-working.ko`
  and local mirror:
  `C:\Users\26301\Desktop\board_tools_f1c200s\prebuilt_modules\esp8089-spi-working.ko`
- `runtime_bundle/collect_runtime_payload.sh` must prefer the kernel-tree ESP8089
  build unless `ESP8089_SPI_KO` is explicitly overridden.

Result:
- runtime bundle now treats integrated drivers as kernel outputs, not rootfs
  overlay-owned binary blobs.
```

Behavioral notes for next assistant:
- User is frustrated by broad exploration and repeated wrong assumptions. Read source first, cite exact file/function, and keep scope narrow.
- For SFC work, start from:
  ~/LicheePi_Nano/third_party/snes9x4d_miyoo/dingux-sdl/sdlmain.cpp
  ~/LicheePi_Nano/third_party/snes9x4d_miyoo/run_snes9x4d_drm.sh
  /tmp/snes9x4d_drm.log on board
- Avoid creating Windows Desktop temp folders unless necessary; if created, clean them after the confirmed result is recorded.
- Before any critical host source change, create a timestamped backup under ~/LicheePi_Nano/backups and write BACKUP_LOG.txt.
```

## Network Update - 2026-06-24
```text
Board current SSH IP changed:
root@10.70.36.247, password 1
Previous board IP 10.67.68.247 may be stale on the current network.
Host remains:
wnk@192.168.175.137, password 1
```

## GMenu2X Bluetooth Scan UI - 2026-06-24
```text
User requirement:
- Bluetooth scan must not display raw scan/log output.
- After devices are found, show a selectable option list inside GMenu2X.
- Selecting a device should attempt to connect Bluetooth.

Implementation:
- Source changed:
  ~/LicheePi_Nano/gmenu2x/src/builtinapps.cpp
- builtin=bluetooth_scan now:
  1. runs /root/start_bluetooth.sh silently,
  2. scans through bluetoothctl first, hcitool fallback,
  3. parses "Device <MAC> <name>" / hcitool scan output,
  4. displays an in-GMenu2X selectable list,
  5. A/Start connects selected device, B/Select exits,
  6. connect path runs bluetoothctl power/agent/pair/trust/connect,
     with hcitool cc fallback.
- builtin=bluetooth_start was also simplified to avoid dumping hciconfig -a
  into the UI.
- /root/start_bluetooth.sh changed to use pidof instead of pgrep on the board
  and no longer prints long hciconfig -a output, preventing duplicate
  bluetoothd/dbus-daemon instances and noisy UI.

Build/deploy:
- Host build passed:
  cd ~/LicheePi_Nano/gmenu2x
  make -f Makefile.f1c200s -j4 pack
- Deployed board binary:
  /root/gmenu2x/gmenu2x
- Deployed board script:
  /root/start_bluetooth.sh
- Board md5 after deploy:
  4677e4b66d41e183e3d63fe76e48581a  /root/gmenu2x/gmenu2x

Verification:
- Manual bluetoothctl scan on board found devices:
  7C:FD:82:59:0F:62 罗祺凯
  10:2D:41:20:F9:B5 我的电视
- Next user-facing test: open GMenu2X -> BT Scan and verify the list appears,
  then select a device to attempt connection.
```

## GMenu2X Bluetooth Manual MAC Input - 2026-06-24
```text
User feedback:
- BT Scan can still end up showing no devices found.
- The desired UX should behave like the ROM/file transfer flow and reuse the
  GMenu2X soft keyboard when manual input is needed.

Follow-up change:
- Source changed:
  ~/LicheePi_Nano/gmenu2x/src/builtinapps.cpp
- Added helper:
  promptBluetoothMac(...)
  Uses InputDialog soft keyboard with default text:
  00:11:22:33:44:55
- Bluetooth device list now always includes a final entry:
  Manual input MAC...
- If scan returns zero devices, the same chooser still appears with only the
  manual input entry available, instead of a dead-end "No device found" page.
- Manual input validates MAC format XX:XX:XX:XX:XX:XX before continuing.
- Connect path reuses the same bluetoothctl pair/trust/connect flow.

Host build result:
- Rebuilt successfully:
  cd ~/LicheePi_Nano/gmenu2x
  make -f Makefile.f1c200s -j4 pack

Deployment status:
- Built on host only in this step because the board is offline.
- Next deployment should copy:
  ~/LicheePi_Nano/gmenu2x/dist/f1c200s/gmenu2x
  to /root/gmenu2x/gmenu2x on the board.
```

## GMenu2X Bluetooth Host Verification - 2026-06-25
```text
Purpose:
- Resume-side verification of the latest Bluetooth scan/manual MAC work before
  board deployment.

Verified on host:
- Host reachable at:
  wnk@192.168.175.137
- Source still contains the manual MAC input flow in:
  ~/LicheePi_Nano/gmenu2x/src/builtinapps.cpp
  Key confirmed markers:
  * promptBluetoothMac(...)
  * "Manual input MAC..."
  * /tmp/start_bluetooth_scan.log logging path
- Runtime overlay still contains the quieter Bluetooth startup script in:
  ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/root/start_bluetooth.sh
  with pidof-based dbus-daemon / bluetoothd checks.
- Runtime app generator still creates:
  ~/LicheePi_Nano/buildroot-2018.02.11/output/target/root/gmenu2x/sections/applications/bluetooth_scan
  containing:
  builtin=bluetooth_scan

Current host artifacts:
- ~/LicheePi_Nano/gmenu2x/dist/f1c200s/gmenu2x
  md5: cf1a4c55be1b4f6f0e4b0f83fba17b77
- ~/LicheePi_Nano/gmenu2x/dist/gmenu2x-f1c200s.tar.gz
  md5: 5f211aee330e19360fcdc7a2b2386ebd

Board status at verification time:
- SSH to root@10.70.36.247 timed out on 2026-06-25.
- Because the board was offline/unreachable, this step did not deploy or test
  the Bluetooth UI on target.

Next deploy when board is reachable:
- scp ~/LicheePi_Nano/gmenu2x/dist/f1c200s/gmenu2x root@10.70.36.247:/root/gmenu2x/gmenu2x
- scp ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/root/start_bluetooth.sh root@10.70.36.247:/root/start_bluetooth.sh
- ssh root@10.70.36.247 'chmod 755 /root/gmenu2x/gmenu2x /root/start_bluetooth.sh'

Expected user-facing verification on board:
- Open GMenu2X -> BT Scan
- If scan finds devices, chooser must show selectable device rows plus:
  Manual input MAC...
- If scan finds nothing, chooser must still open with:
  Manual input MAC...
- Selecting a scanned device or typed MAC should attempt bluetoothctl connect.
```

## WiFi / Bluetooth / Reboot Recovery Notes - 2026-06-28
```text
Purpose:
- Preserve the exact failure chain and recovery state from the 2026-06-28 work.
- After compact/resume, read this section before touching RTL8723BU WiFi,
  Bluetooth UI, or reboot behavior again.

Board access used in this round:
- Serial: COM3, 115200, login root, password 1.
- Host Linux: wnk@192.168.175.137, password 1.
- User-provided board WiFi IP during this round: 10.128.140.247
  (DHCP can change after reconnect/reboot; do not assume it is permanent).

User scope correction:
- The important instruction from the user was:
  when WiFi regressed, stop blaming generic driver/hardware first and inspect
  the network startup/configuration path after the driver is up.
- In practice, both layers mattered:
  1. some script/config regressions were real and were fixed,
  2. later a deeper scan failure remained even with the startup script restored.
- Future work must separate those two layers explicitly.

1. Reboot failure and watchdog result
- Symptom:
  `reboot` printed:
  `Requesting system reboot`
  `reboot: Restarting system`
  `Reboot failed -- System halted`
- A watchdog-based reboot path was then investigated and rebuilt in the kernel.
- During kernel config, watchdog options were enabled interactively.
- Final user confirmation in this round:
  `reboot` can now work normally.
- Durable rule:
  if reboot breaks again, inspect the watchdog-enabled kernel/DTS state first,
  not only userspace shutdown scripts.

2. Bluetooth UI work completed earlier in the same effort
- GMenu2X Bluetooth scan was changed from raw log dumping to an in-UI
  selectable device list.
- Manual MAC entry fallback was added through the GMenu2X soft keyboard.
- Board-side Bluetooth scanning was verified from shell with devices found.
- User later confirmed Bluetooth scanning behavior was working.

3. WiFi UI / config issues that were real and fixed
- GMenu2X WiFi connect flow had a real config-generation bug:
  generated `wpa_supplicant` content contained escaped quotes such as:
    `ssid=\"wnl64\"`
    `psk=\"297475wnl\"`
- That broke parsing and had to be fixed in the GMenu2X WiFi connect path.
- The WiFi UI was also changed to behave more like Bluetooth:
  scan result list becomes selectable/clickable instead of only showing text.
- WiFi connect path was changed to:
  1. generate config,
  2. run `wpa_passphrase` or open-network block generation,
  3. start `wpa_supplicant`,
  4. wait for `wpa_state=COMPLETED`,
  5. then request DHCP.
- Connected config was also copied to `/etc/wpa_supplicant.conf` so reboot can
  reuse the last network.

Relevant local work files from this round:
- `C:\Users\26301\Desktop\_codex_gmenu2x_bt_fix\builtinapps.cpp`
- `C:\Users\26301\Desktop\_codex_gmenu2x_bt_fix\S45usb-wifi`
- `C:\Users\26301\Desktop\_codex_gmenu2x_bt_fix\hal_btcoex.c`

4. Observed WiFi startup scripts and runtime files
- Main runtime files involved:
  `/etc/init.d/S17rtl8723bu`
  `/etc/init.d/S45usb-wifi`
  `/etc/default/usb-wifi`
  `/etc/wpa_supplicant.conf`
- Important boot order:
  `S17rtl8723bu` loads the module first.
  `S45usb-wifi` later configures interface / `wpa_supplicant` / DHCP.
- Therefore `S17rtl8723bu` module options matter even if `S45usb-wifi` also has
  a `MODULE_OPTS` string.

5. Known script/config states seen on 2026-06-28
- Current board `S45usb-wifi` during debugging used:
  - `IFACE="wlan0"`
  - `MODE="dhcp"`
  - `wpa_supplicant -B -D nl80211,wext -i "$IFACE" -c /etc/wpa_supplicant.conf`
- Current `/etc/default/usb-wifi` during debugging:
  - `MODE="dhcp"`
  - static fallback still kept:
    `ADDR=172.28.73.247`
    `MASK=255.255.255.0`
    `GW=172.28.73.171`
    `DNS1=172.28.73.171`
    `DNS2=223.5.5.5`
- Current `/etc/wpa_supplicant.conf` during debugging was valid:
  `ssid="wnl64"`
  `psk="297475wnl"`

6. WiFi regression evidence captured in this round
- Even after cleaning the obvious UI/config bug, a deeper regression appeared:
  - `iwlist wlan0 scan` returned `No scan results`
  - `wpa_cli -i wlan0 status` stayed at `wpa_state=SCANNING`
  - `ifconfig wlan0` showed the interface existed and was UP
  - Bluetooth scanning could still work
- This proved the blocker was no longer only password formatting or DHCP.

7. Critical runtime evidence from serial shell
- A stale test process existed at one point:
  `wpa_supplicant -B -D nl80211,wext -i wlan0 -c /tmp/wpa_try.conf`
- This had to be killed before trusting startup-script tests.
- After restoring a clean test path, `sh -x /etc/init.d/S45usb-wifi restart`
  showed the script itself was behaving as written:
  it started `wpa_supplicant`, waited, timed out in `SCANNING`, then ran DHCP.
- That means:
  the startup script path can be correct while association is still broken.

8. RTL8723BU modules and md5 values seen in this round
- Host-built custom module placed on Windows for manual transfer:
  `C:\Users\26301\Downloads\8723bu.ko`
  md5:
  `2360318f8529147f727798ebb4b7de29`
- Board runtime module at one debug point:
  `/lib/modules/5.7.1/extra/8723bu.ko`
  md5:
  `63589b67c073c5127d5bc86a6a481504`
- Board local backup candidates found:
  `/root/roms/runtime_backups/rtl8723bu_439_test_19700101_002604/8723bu.ko.before`
  md5 `63589b67c073c5127d5bc86a6a481504`
  `/root/roms/runtime_backups/rtl8723bu_no_btcoex_19700101_000748/8723bu.ko.before`
  md5 `63589b67c073c5127d5bc86a6a481504`
  `/root/roms/runtime_backups/rtl8723bu_439_ampdu_19700101_000547/8723bu.ko.before`
  md5 `904074e1f05af6f87a44b044ba14f999`
  `/root/roms/runtime_backups/rtl8723bu_ampdu_19700101_001803/8723bu.ko.before`
  md5 `8aa7817a59715129a4abed67c59dad0a`
  `/root/roms/runtime_backups/rtl8723bu_ratedbg_19700101_000326/8723bu.ko.before`
  md5 `4c7ee5966423f1da7b42dcc182f33007`
  `/root/roms/runtime_backups/rtl8723bu_rollback_19700101_000953/8723bu.ko.before`
  md5 `b8b87a36c4733692bebf07085e4a9b2c`
  `/root/roms/runtime_backups/rtl8723bu_usb_rxagg_19700101_000820/8723bu.ko.before`
  md5 `e8a30ae52eac39a4a2d1b644427bc047`

9. Driver parameter states compared in this round
- Old host backup `S17rtl8723bu` from
  `~/LicheePi_Nano/backups/rom_partition_auto_mount_20260616_065709/init.d.before/S17rtl8723bu`
  used aggressive params:
  `rtw_btcoex_enable=1 rtw_ht_enable=1 rtw_bw_mode=0x21 rtw_wmm_enable=1 rtw_ampdu_enable=2 rtw_usb_rxagg_mode=1 rtw_wifi_spec=0`
- Current conservative recovery params used later in this round:
  `rtw_btcoex_enable=0 rtw_ht_enable=0 rtw_bw_mode=0 rtw_wmm_enable=0 rtw_ampdu_enable=0 rtw_usb_rxagg_mode=0 rtw_wifi_spec=1 rtw_channel_plan=0x7F rtw_antdiv_cfg=0 rtw_ant_num=1 rtw_adaptivity_en=0`
- Both styles were tested during the round.
- Important caution:
  at least once, old/aggressive params were restored but scan still failed.
  Therefore parameter strings alone did not explain every bad state seen.

10. Driver / runtime inspection results worth preserving
- `iwconfig wlan0` during failure:
  managed mode, channel/frequency available, `Access Point: Not-Associated`
- `iwlist wlan0 channel` worked and showed channels 1-14.
- `iwpriv wlan0` was available; the driver was not completely dead.
- `/proc/net/rtl8723bu/wlan0/...` existed and was readable.
- Notable proc values during failure:
  - `adapter_state`: `bSurpriseRemoved=0, bDriverStopped=0`
  - `fwstate=0x8`
  - `bw_mode=0x00`
  - `ht_enable=0`
  - `rf_info` showed:
    `cur_ch=1, cur_bw=0`
    `oper_ch=7, oper_bw=0`
- `btcoex` proc dump during failure showed:
  - `Wifi bLink/ bRoam/ bScan/ bHi-Pri = 0/ 0/ 0/ 0`
  - This is important because user-space reported `SCANNING`,
    but the driver-side coexist dump did not show an active scan.
- `wext` backend test produced:
  - `ioctl[SIOCSIWAP]: Operation not permitted`
  - `ioctl[SIOCSIWENCODEEXT]: Invalid argument`
  - final status still `wpa_state=SCANNING`
- `nl80211` backend test produced:
  - successful init message only
  - final status still `wpa_state=SCANNING`

11. Strong conclusions from this round
- Do not assume every WiFi regression is only a bad password or DHCP issue.
- Do not assume every WiFi regression is only a bad startup-script issue either.
- Once `iwlist wlan0 scan` returns `No scan results` and `wpa_state` stays
  `SCANNING`, the failure has already moved below normal DHCP/user-config level.
- Script tracing is still useful to prove whether the script path is clean, but
  after that point further work should focus on:
  1. actual loaded `8723bu.ko`,
  2. module params from `S17rtl8723bu`,
  3. whether some stale runtime state survived until a power cut,
  4. procfs/private-ioctl state, not only shell wrappers.

12. Important practical recovery fact from the user
- At the end of this round, the user power-cycled the board and reported:
  after unplug/replug, WiFi networking worked again.
- This means at least one bad state in this session was recoverable by full
  power loss, not just soft reboot or service restart.
- Future debugging rule:
  after repeated `No scan results` / stuck `SCANNING` with scripts already
  cleaned, record the current module md5 + script state, then include a cold
  power-cycle as a distinct diagnostic step before rewriting more code.

13. What to check first after any future compact/resume
- Read this section first.
- Confirm current board IP instead of assuming an old DHCP lease.
- Confirm current `/lib/modules/5.7.1/extra/8723bu.ko` md5.
- Confirm current `/etc/init.d/S17rtl8723bu` contents.
- Confirm no stale manual test process is running:
  `ps | grep wpa_supplicant`
- If WiFi is broken:
  1. kill stale `wpa_supplicant` / `udhcpc`,
  2. inspect `/etc/wpa_supplicant.conf`,
  3. run `iwlist wlan0 scan`,
  4. run `wpa_cli -i wlan0 status`,
  5. inspect `/proc/net/rtl8723bu/wlan0/*`,
  6. only then decide whether to edit scripts, swap modules, or cold power-cycle.
```

## GMenu2X WiFi Saved Networks UX - 2026-06-28
```text
User requirement:
- After entering a password and connecting once, WiFi information must be saved
  across power loss.
- Networks that were connected before must be separated from networks never
  connected before, similar to phone/PC WiFi UX.

Source changed:
- `~/LicheePi_Nano/gmenu2x/src/builtinapps.cpp`

Board binary deployed:
- `/root/gmenu2x/gmenu2x`

Host / board artifact md5 after deploy:
- Host:
  `~/LicheePi_Nano/gmenu2x/dist/f1c200s/gmenu2x`
  md5 `3e67349e3a3bbc08444d5572cb50064b`
- Board:
  `/root/gmenu2x/gmenu2x`
  md5 `3e67349e3a3bbc08444d5572cb50064b`
- Previous board binary backup kept at:
  `/root/gmenu2x/gmenu2x.before_saved_wifi_20260628`
  md5 `50e3254cf4bbf945d2f5fe7f5abfd95b`

Implementation details:
1. WiFi scan results are still obtained from `iwlist`.
2. Saved SSIDs are parsed from `/etc/wpa_supplicant.conf`.
3. Current connected SSID is parsed from:
   `wpa_cli -i wlan0 status`
4. Scan list is grouped into:
   - `Saved Networks`
   - `Other Networks`
5. Rows already saved are marked:
   - `[Saved]`
   - current connected row shows `[Connected]`
6. On connect, GMenu2X no longer blindly replaces the whole
   `/etc/wpa_supplicant.conf` with only the latest network.
7. Instead it now:
   - builds the new target `network={...}` block,
   - removes any old block for the same SSID from `/etc/wpa_supplicant.conf`,
   - appends the refreshed block,
   - writes the merged result back to `/etc/wpa_supplicant.conf`,
   - keeps:
     `ctrl_interface=/var/run/wpa_supplicant`
     `update_config=1`
8. This gives power-loss persistence similar to normal phone/PC saved-network
   behavior.

Current connect path caveat:
- GMenu2X still launches runtime association with:
  `/tmp/wpa_supplicant.gmenu2x.conf`
  then persists the merged config into:
  `/etc/wpa_supplicant.conf`
- This is intentional so the current attempt stays narrow and debuggable while
  saved networks remain durable.

Runtime verification snapshot at deploy time:
- `/etc/wpa_supplicant.conf` on board contained:
  `ssid="wnl64"`
  `psk="297475wnl"`
- A `wpa_supplicant` process started from:
  `/tmp/wpa_supplicant.gmenu2x.conf`
  was visible while GMenu2X was running.

If this feature needs follow-up:
- First inspect:
  `/etc/wpa_supplicant.conf`
- Then verify the UI grouping in GMenu2X:
  saved networks must appear above unsaved ones.
- If a network password changes, reconnecting to the same SSID should refresh
  that SSID block instead of duplicating it.
```

## GMenu2X WiFi Password Prefill UX - 2026-06-28
```text
Follow-up user requirement:
- Clicking a saved WiFi should still open the password input box.
- The input box should be prefilled with the previously saved password so the
  user can:
  1. press Start immediately to reconnect, or
  2. edit the password and then connect.

Source changed:
- `~/LicheePi_Nano/gmenu2x/src/builtinapps.cpp`

Implementation:
- Saved SSIDs are still discovered from `/etc/wpa_supplicant.conf`.
- Additional parsing now also tries to extract a reusable saved password from
  each `network={...}` block.
- Preferred source is:
  `#psk="plain_text_password"`
  generated by `wpa_passphrase`.
- Fallback is:
  `psk="plain_text_password"`
  if that form exists.
- If only a hashed `psk=` value exists and no plain text is present, the input
  box cannot be prefilled meaningfully and may fall back to empty.
- `promptWifiPassword(...)` now accepts a default value and preloads it into the
  GMenu2X soft keyboard dialog.

Behavior after this change:
- Unsaved network:
  password dialog opens empty.
- Saved network with plain password recoverable from config:
  password dialog opens prefilled with old password.
- User can keep it unchanged and connect immediately, or modify it.

Host build result:
- Rebuilt successfully on host:
  `cd ~/LicheePi_Nano/gmenu2x && make -f Makefile.f1c200s -j4`
- Host artifact md5 after this change:
  `~/LicheePi_Nano/gmenu2x/dist/f1c200s/gmenu2x`
  md5 `aa3efcd706d7c3c39a25c28f1a801008`

Deployment status at record time:
- Host build completed.
- Board was later deployed successfully.
- Final deployed board binary:
  `/root/gmenu2x/gmenu2x`
  md5 `aa3efcd706d7c3c39a25c28f1a801008`
- Board backup files kept during deployment:
  `/root/gmenu2x/gmenu2x.before_prefill_20260628`
  md5 `3e67349e3a3bbc08444d5572cb50064b`
  `/root/gmenu2x/gmenu2x.before_prefill_20260628_final`
  md5 `3e67349e3a3bbc08444d5572cb50064b`

Board-side runtime note:
- During this deploy, `run_gmenu2x.sh` itself did not overwrite the binary.
- The apparent md5 mismatch seen once during remote checking was a transient
  timing artifact while the process was being replaced/restarted.
- Final on-board file state confirmed:
  `/root/gmenu2x/gmenu2x`
  md5 `aa3efcd706d7c3c39a25c28f1a801008`

Verification checklist after resume:
1. open WiFi scan,
2. choose a saved SSID,
3. confirm the password box is prefilled,
4. press Start directly to reconnect,
5. change the password once and verify the saved config refreshes.
```

## USB WiFi Runtime Payload Extension - 2026-07-02
```text
User scope in this round:
- Keep pushing the USB WiFi driver line forward.
- Two active targets remain:
  1. repair/test `8723du` and `8733bu`
  2. add support for the separate RTL8188FTV dongle through:
     https://github.com/kelebek333/rtl8188fu

Important result completed in this step:
- The unified runtime payload/deploy path now includes all of:
  * `8723bu.ko`
  * `8723du.ko`
  * `8733bu.ko`
  * `rtl8188fu.ko`
  * `rtl8188fufw.bin`
- This means future board deployment can use the normal runtime bundle instead
  of ad hoc one-off copies for these three USB WiFi families.

Local Windows source-of-truth files changed:
- `C:\Users\26301\Desktop\_codex_wifi_pkg\runtime_bundle\collect_runtime_payload.sh`
- `C:\Users\26301\Desktop\_codex_wifi_pkg\runtime_bundle\install_payload_to_target.sh`
- `C:\Users\26301\Desktop\_codex_wifi_pkg\overlay\etc\init.d\S17rtl8723bu`

What changed:
1. `collect_runtime_payload.sh`
   now copies:
   - `third_party/rtl8723du/8723du.ko`
   - `third_party/wirenboard_rtl8733bu/8733bu.ko`
   - `third_party/rtl8188fu_kelebek333/rtl8188fu.ko`
   - `third_party/rtl8188fu_kelebek333/firmware/rtl8188fufw.bin`
2. `install_payload_to_target.sh`
   now installs into Buildroot target:
   - `/lib/modules/5.7.1/extra/8723du.ko`
   - `/lib/modules/5.7.1/extra/8733bu.ko`
   - `/lib/modules/5.7.1/extra/rtl8188fu.ko`
   - `/lib/firmware/rtlwifi/rtl8188fufw.bin`
3. `S17rtl8723bu`
   remains the common USB WiFi loader and now covers:
   - `0bda:b720` -> `8723bu`
   - `0bda:d723` -> `8723du`
   - `0bda:b733` / `0bda:f72b` -> `8733bu`
   - `0bda:f179` -> `rtl8188fu`

Current verified module/firmware md5:
- `8723du.ko`
  `976e8e8db0645bfdc324136fcae7ecab`
- `8733bu.ko` (wirenboard tree, current preferred test build)
  `4134a36494708e8fcff71d31ca07f697`
- `rtl8188fu.ko`
  `35b262f43ae7d4a7809c3a9bc9c89dbf`
- `rtl8188fufw.bin`
  `62e540665cf25e682864c1ef67b893ba`

Host sync / verification completed:
- Host backup created before sync:
  `~/LicheePi_Nano/backups/wifi_payload_extend_20260701_183331`
- Synced to host:
  `~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/collect_runtime_payload.sh`
  `~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/install_payload_to_target.sh`
  `~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/etc/init.d/S17rtl8723bu`
- Verified on host:
  `cd ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle && ./collect_runtime_payload.sh`
  `cd ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle && ./install_payload_to_target.sh`
- Verified resulting target files and md5:
  `/home/wnk/LicheePi_Nano/buildroot-2018.02.11/output/target/lib/modules/5.7.1/extra/8723du.ko`
    -> `976e8e8db0645bfdc324136fcae7ecab`
  `/home/wnk/LicheePi_Nano/buildroot-2018.02.11/output/target/lib/modules/5.7.1/extra/8733bu.ko`
    -> `4134a36494708e8fcff71d31ca07f697`
  `/home/wnk/LicheePi_Nano/buildroot-2018.02.11/output/target/lib/modules/5.7.1/extra/rtl8188fu.ko`
    -> `35b262f43ae7d4a7809c3a9bc9c89dbf`
  `/home/wnk/LicheePi_Nano/buildroot-2018.02.11/output/target/lib/firmware/rtlwifi/rtl8188fufw.bin`
    -> `62e540665cf25e682864c1ef67b893ba`

Remaining work after this step:
- Board-side runtime validation is still needed for:
  * `8723du`
  * `8733bu`
  * `rtl8188fu`
- Do not assume the board IP; confirm current DHCP address first.
- When the board is online, preferred first checks are:
  1. `lsusb`
  2. `ls /lib/modules/5.7.1/extra`
  3. `/etc/init.d/S17rtl8723bu restart`
  4. `ifconfig -a`
  5. `iwlist wlan0 scan` or the actual created wlanX
```

## AIC8800D80 USB WiFi / MUSB DMA Finding - 2026-07-03
```text
Active dongle under test:
- USB ID before stage-1 firmware: `a69c:8d80`

Critical root cause found:
- On F1C200S current Linux 5.7.1, MUSB host DMA being enabled (`/sys/module/musb_hdrc/parameters/use_dma = Y`)
  prevents the AIC `aic_load_fw` first vendor bulk command from getting any RX response.
- Symptom with DMA enabled:
  * `aic cmd tx id=1024 ... raw=10 00 11 00 ... 00 00 50 40`
  * TX completion status is `0`
  * then `cmd timed-out`
  * then `40500000 rd fail: -110`
  * no successful RX complete for the command
- This was reproduced with:
  * the local/patched `aic_load_fw`
  * the separate `shenmintao/aic8800d80` driver tree
- So the blocker is not only one bad driver tree; the key issue is the F1C MUSB DMA path.

Runtime proof that fixes stage-1:
- Through serial, the following sequence was tested:
  1. `echo N > /sys/module/musb_hdrc/parameters/use_dma`
  2. unload USB host stack leaf module: `rmmod sunxi`
  3. reload: `insmod /lib/modules/5.7.1/kernel/drivers/usb/musb/sunxi.ko`
  4. insert `aic_load_fw.ko`
- After that, stage-1 immediately started receiving replies:
  * successful RX complete logs for `id=1025`
  * `chip_id=7, chip_mcu_id = 0`
  * firmware upload proceeded
  * device re-enumerated from `a69c:8d80` to `a69c:8d83`

Second-stage result:
- `a69c:8d83` is supported by `aic8800_fdrv`.
- With MUSB DMA already disabled and USB host reloaded:
  * `insmod /lib/modules/5.7.1/extra/aic8800_fdrv.ko`
  * creates `wlan0`
  * manual `iwlist wlan0 scan` works and returns real AP list
- Therefore:
  * stage-1 firmware load is fixed by disabling MUSB DMA
  * stage-2 basic bring-up and scanning also work under the same condition

Current remaining problem:
- If old auto-connect processes (`wpa_supplicant`, DHCP, previous saved-network auto-connect path) immediately drive the new AIC interface,
  the driver can enter a bad state:
  * `rwnx_send_sm_connect_req ... connect to wnl64`
  * later `SM_DISCONNECT_REQ` times out
  * command queue crashes
  * subsequent scans report `is scanning, abort`
- Clean minimal path that is known good:
  1. stop old auto-connect processes first
  2. load `aic_load_fw`
  3. let device become `a69c:8d83`
  4. load `aic8800_fdrv`
  5. test manual scan before any auto-connect

Important practical conclusion:
- For AIC USB WiFi on this board, first focus on keeping `musb_hdrc.use_dma=0`.
- Do not spend more time on `aic_load_fw` command formatting until DMA is disabled first; with DMA enabled the first command timeout is expected on this board.
```

## AIC8800D80 RX Prealloc Fix / Hot Reload Finding - 2026-07-04
```text
Problem being fixed in this round:
- AIC file transfer previously triggered:
  `BUG: scheduling while atomic`
- stack showed:
  `aicwf_usb_rx_submit_all_urb [aic8800_fdrv]`
  -> `aicwf_prealloc_rxbuff_alloc [aic_load_fw]`
  -> sleeping allocation path

Root cause confirmed:
- parent Makefile already exported:
  `CONFIG_PREALLOC_RX_SKB = y`
  `CONFIG_PREALLOC_TXQ = y`
- but subdir:
  `~/LicheePi_Nano/third_party/aic8800_linux_driver/drivers/aic8800/aic_load_fw/Makefile`
  still hardcoded:
  `CONFIG_PREALLOC_RX_SKB = n`
  `CONFIG_PREALLOC_TXQ = n`
- that allowed `aicwf_rx_prealloc.c` to compile its fallback
  `kzalloc(..., GFP_KERNEL)` path, which is not safe in the observed RX submit context

Source changes made:
- local Windows source-of-truth edited first:
  `C:\Users\26301\Desktop\_ugreen_cm762_driver\Linux\aic8800_linux_driver\drivers\aic8800\aic_load_fw\Makefile`
- synced to host:
  `~/LicheePi_Nano/third_party/aic8800_linux_driver/drivers/aic8800/aic_load_fw/Makefile`
- changes:
  1. `CONFIG_PREALLOC_RX_SKB ?= y`
  2. `CONFIG_PREALLOC_TXQ ?= y`
  3. switched prealloc objects to conditional inclusion:
     `$(MODULE_NAME)-$(CONFIG_PREALLOC_RX_SKB) += aicwf_rx_prealloc.o`
     `$(MODULE_NAME)-$(CONFIG_PREALLOC_TXQ) += aicwf_txq_prealloc.o`

Host backup for this change:
- `~/LicheePi_Nano/backups/aic_prealloc_fix_20260703_204116`
- includes:
  `aic_load_fw/Makefile`
  `aic_load_fw/aic_bluetooth_main.c`
  `aic8800_fdrv/rwnx_main.c`
  `BACKUP_LOG.txt`

Host rebuild command used:
- keep kernel toolchain rule unchanged:
  `export PATH=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin:$PATH`
- then:
  `cd ~/LicheePi_Nano/third_party/aic8800_linux_driver/drivers/aic8800`
  `make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- KDIR=~/LicheePi_Nano/linux -j4`

Host verification after rebuild:
- `.aicwf_rx_prealloc.o.cmd` now contains both:
  `-DCONFIG_PREALLOC_RX_SKB`
  `-DCONFIG_PREALLOC_TXQ`
- host module md5 after rebuild:
  `aic_load_fw.ko` -> `923b2eaf36d0d8fc096dc590d6756b7a`
  `aic8800_fdrv.ko` -> `d1a57c2025c03d123aa9b053add155de`

Board deployment result in this round:
- board was reachable by serial on COM3, but not reliably reachable by SSH
- board could reach the Windows host on:
  `10.128.140.9`
- temporary HTTP server on Windows:
  `python -m http.server 8000 --bind 10.128.140.9`
- board successfully fetched the new module by:
  `wget http://10.128.140.9:8000/aic_load_fw.ko -O /tmp/aic_load_fw.new.ko`
- verified on board:
  `/tmp/aic_load_fw.new.ko` md5
  `923b2eaf36d0d8fc096dc590d6756b7a`
- copied into place on board:
  `/lib/modules/5.7.1/extra/aic_load_fw.ko`
- verified final board on-disk md5:
  `923b2eaf36d0d8fc096dc590d6756b7a`
- board kept backup:
  `/lib/modules/5.7.1/extra/aic_load_fw.ko.before_prealloc_fix`
- old board module md5 before replacement:
  `acc6748ef98fb70a666d5fce9aaca250`

Important hot-reload finding:
- replacing the file on disk succeeded
- but attempting to live-switch the active AIC path on the running board was not safe
- observed AIC topology on board:
  device `1-1.3`
  interface symlink under driver dir:
  `/sys/bus/usb/drivers/aic8800_fdrv/1-1.3:1.0`
- out-of-tree `aic8800_fdrv` sysfs did not expose normal `bind/unbind` files
- forcing live USB deauthorization with:
  `echo 0 > /sys/bus/usb/devices/1-1.3/authorized`
  led to a bad board state:
  serial still echoed characters, but shell stopped returning command results
  network stopped replying
- practical conclusion:
  do not continue using live deauthorize/rebind as the validation path for AIC on this board

Next validation rule:
- the new `aic_load_fw.ko` is already on the board filesystem
- next test should be a cold boot / power cycle, then validate:
  1. AIC init duration
  2. whether `wlan0` comes up cleanly
  3. file transfer no longer triggers `BUG: scheduling while atomic`
- if needed next round, deploy host `aic8800_fdrv.ko` too from:
  `C:\Users\26301\Desktop\_codex_aic_fastinit\deploy\aic8800_fdrv.ko`
  host md5:
  `d1a57c2025c03d123aa9b053add155de`
```

## AIC8800D80 RX Prealloc Follow-up - 2026-07-04
```text
Cold-boot validation after the first prealloc patch showed the original crash
had changed shape but was not actually gone.

Observed board state after cold boot:
- board on-disk module md5 before this follow-up:
  `aic_load_fw.ko` -> `923b2eaf36d0d8fc096dc590d6756b7a`
  `aic8800_fdrv.ko` -> `b0918651d58e1708701e2efece06acfa`
- `musb_hdrc.use_dma` still correctly came up as `N`
- `aic_load_fw` and `aic8800_fdrv` both loaded
- but no `wlan0` / no wlan interface appeared

Critical dmesg finding from that boot:
- stage-1 firmware download completed
- then probe hit:
  `aicwf_prealloc_rxbuff_alloc WARNING rxbuff is running out 0`
- followed by:
  `Unable to handle kernel NULL pointer dereference`
  `PC is at aicwf_prealloc_rxbuff_alloc+0x64/0x124 [aic_load_fw]`
  called from:
  `aicwf_usb_rx_submit_all_urb [aic8800_fdrv]`
- boot then continued far enough to start userspace, but:
  `[usb-wifi] no wlan interface found, skip wifi setup`

Important source root causes confirmed in this follow-up:
1. `aic8800_fdrv/aicwf_txrxif.c` had the prealloc lifecycle disabled:
   - `aicwf_prealloc_init();` was commented out
   - `aicwf_prealloc_exit();` was commented out
2. `aic_load_fw/aicwf_rx_prealloc.c` exported only:
   - `aicwf_rxbuff_size_get`
   - `aicwf_prealloc_rxbuff_alloc`
   - `aicwf_prealloc_rxbuff_free`
   but not:
   - `aicwf_prealloc_init`
   - `aicwf_prealloc_exit`
   so re-enabling the calls in `aic8800_fdrv` first failed at modpost until
   those exports were added.
3. The vendor default:
   - `aic_rxbuff_num_max = 1000`
   - `aic_rxbuff_size = 20480`
   is unrealistic for this 64 MB board.
   That implies roughly 20 MB of high-order `kzalloc()` RX data buffers,
   which is a poor fit for this target.

Local Windows source files changed in this follow-up:
- `C:\Users\26301\Desktop\_codex_aic_fastinit\src\aicwf_txrxif.c`
- `C:\Users\26301\Desktop\_ugreen_cm762_driver\Linux\aic8800_linux_driver\drivers\aic8800\aic_load_fw\aicwf_rx_prealloc.c`

Host backups created:
- `~/LicheePi_Nano/backups/aic_prealloc_call_fix_20260703_211332`
  for re-enabling `aicwf_prealloc_init/exit`
- `~/LicheePi_Nano/backups/aic_prealloc_export_fix_20260703_211433`
  for exporting `aicwf_prealloc_init/exit`
- `~/LicheePi_Nano/backups/aic_prealloc_size_fix_20260703_212425`
  for shrinking the pool / adding fallback logic

Host-side source changes now in effect:
1. `aic8800_fdrv/aicwf_txrxif.c`
   - `aicwf_prealloc_init();` re-enabled in `aicwf_rx_init`
   - `aicwf_prealloc_exit();` re-enabled in `aicwf_rx_deinit`
2. `aic_load_fw/aicwf_rx_prealloc.c`
   - added:
     `EXPORT_SYMBOL(aicwf_prealloc_init);`
     `EXPORT_SYMBOL(aicwf_prealloc_exit);`
   - reduced:
     `aic_rxbuff_num_max = 32`
   - added explicit init reset:
     `atomic_set(..., 0);`
   - added atomic fallback allocator using `GFP_ATOMIC`
   - if prealloc list is empty/uninitialized, `aicwf_prealloc_rxbuff_alloc()`
     now falls back to `GFP_ATOMIC` allocation instead of walking a bad list
   - `aicwf_prealloc_rxbuff_free()` now repairs the list head before adding
     buffers back if needed

Host rebuild status after all follow-up patches:
- rebuilt from:
  `~/LicheePi_Nano/third_party/aic8800_linux_driver/drivers/aic8800`
  with:
  `export PATH=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin:$PATH`
  `make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- KDIR=~/LicheePi_Nano/linux -j4`
- current host module md5:
  `aic_load_fw.ko` -> `cbf431e857d03bfe623d1e6e4a87a594`
  `aic8800_fdrv.ko` -> `d1a57c2025c03d123aa9b053add155de`

Board deployment status in this follow-up:
- `aic8800_fdrv.ko` was serial-transferred and verified on board:
  `/lib/modules/5.7.1/extra/aic8800_fdrv.ko`
  md5 `d1a57c2025c03d123aa9b053add155de`
- board backup kept:
  `/lib/modules/5.7.1/extra/aic8800_fdrv.ko.before_prealloc_call_fix`
- latest `aic_load_fw.ko` was also serial-transferred and verified on board:
  `/lib/modules/5.7.1/extra/aic_load_fw.ko`
  md5 `cbf431e857d03bfe623d1e6e4a87a594`
- board backups now include:
  `/lib/modules/5.7.1/extra/aic_load_fw.ko.before_prealloc_fix`
  `/lib/modules/5.7.1/extra/aic_load_fw.ko.before_prealloc_size_fix`

Current blocker at end of this follow-up:
- after the last reboot request used to activate the newest `aic_load_fw.ko`,
  serial no longer showed a clean shell/login session
- typed input was echoed, but no command results returned
- network ARP entry for the old IP disappeared and SSH was not available
- therefore the newest candidate pair is already on disk, but the final
  cold-boot validation result still needs one more board restart / reconnect

Next step after resume:
1. hard reboot or cleanly restart the board once more
2. immediately capture serial boot log
3. verify on-disk md5:
   - `aic_load_fw.ko` -> `cbf431e857d03bfe623d1e6e4a87a594`
   - `aic8800_fdrv.ko` -> `d1a57c2025c03d123aa9b053add155de`
4. check whether `wlan0` appears
5. if interface exists, run file-transfer stress and confirm the original
   `scheduling while atomic` / null-deref no longer occurs
```

## USB WiFi Boot Policy Update - 2026-07-04
```text
Problem after the AIC DMA fix:
- `S17rtl8723bu` on the board had already evolved into the working "AIC first"
  loader, but the host runtime overlay still contained the older loader order.
- `S45usb-wifi` still assumed:
  * fixed `IFACE="wlan0"`
  * fixed `8723bu`-style startup
- With both AIC (`wlan0`) and RTL8188FU (`wlan1`) inserted, that made boot-time
  auto-connect hit the AIC interface first, which is not the desired default
  network path yet.

Local source-of-truth files now aligned:
- `C:\Users\26301\Desktop\_codex_wifi_pkg\overlay\etc\init.d\S16usb-host`
- `C:\Users\26301\Desktop\_codex_wifi_pkg\overlay\etc\init.d\S17rtl8723bu`
- `C:\Users\26301\Desktop\_codex_wifi_pkg\overlay\etc\init.d\S45usb-wifi`

Host overlay synced and backed up:
- backup:
  `~/LicheePi_Nano/backups/wifi_overlay_sync_20260704_*`
- synced target:
  `~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/etc/init.d/`

Current intended boot policy:
1. `S16usb-host`
   - forces `musb_hdrc.use_dma=N`
   - reloads `sunxi` host if needed so AIC stage-1 works in PIO mode
2. `S17rtl8723bu`
   - loads AIC first when present
   - unloads conflicting USB WiFi modules before AIC stage-1
   - waits for `a69c:8d83`
   - then loads `aic8800_fdrv`
   - then reloads the other USB WiFi modules such as `rtl8188fu`
3. `S45usb-wifi`
   - no longer assumes `wlan0`
   - no longer force-loads `8723bu`
   - default is now `IFACE="auto"`
   - auto-select priority is:
     `rtl8188fu` -> `8723bu` -> `8723du` -> `8733bu` -> `aic8800_fdrv`

Runtime proof captured on board:
- after restarting `S45usb-wifi`, the board selected `wlan1`
- `wpa_supplicant` ran on:
  `wlan1 -c /etc/wpa_supplicant.conf`
- `wpa_state=COMPLETED`
- `ip_address=10.128.140.241`
- `wlan0` remained the AIC interface and was not used for default DHCP

Critical follow-up root cause found during reboot validation:
- Buildroot `rcS` executes every file matching:
  `/etc/init.d/S??*`
- The board still had backup files:
  * `/etc/init.d/S17rtl8723bu.bak_1214`
  * `/etc/init.d/S45usb-wifi.bak_1214`
- Because those names still match `S??*`, the backup scripts were also executed
  at boot and could override the intended WiFi path.
- Fix applied on board:
  * moved to `/root/S17rtl8723bu.bak_1214.disabled`
  * moved to `/root/S45usb-wifi.bak_1214.disabled`
- Durable rule:
  never leave backup copies inside `/etc/init.d/` with names beginning `SNN`.
  Move them out of the directory or rename them so they do not match `S??*`.

Cold reboot validation after removing the backup init scripts:
- board rebooted cleanly
- `musb_hdrc.use_dma` still came up as `N`
- the USB WiFi auto-connect path succeeded after boot
- final runtime state:
  * `wpa_supplicant -i wlan0 -c /etc/wpa_supplicant.conf`
  * `wpa_state=COMPLETED`
  * `ip_address=10.128.140.241`
- Important note:
  interface numbering is not stable across driver/probe order.
  On an earlier runtime the RTL8188FU interface was `wlan1`; after the clean
  reboot validation it became `wlan0`.
  This is another reason `S45usb-wifi` must stay on auto-selection instead of
  hard-coding a fixed `wlanX`.

Known remaining AIC issue:
- when AIC is left present but not used as the default network interface, the
  driver can still emit repeated:
  `rwnx_cfg80211_scan is scanning, abort`
- manually forcing `ifconfig wlan0 down` is not a clean workaround:
  it triggers a warning in `rwnx_close` and `cmd queue crashed` messages.
- Therefore do not auto-bring AIC down in `S45usb-wifi` for now.
- Treat AIC as:
  * stage-1/2 bring-up proven
  * non-default experimental interface
  * still needing driver-level cleanup before it can become the quiet/stable
    primary boot interface
```

## AIC8800D80 PREALLOC Flag + Slowpath Fix - 2026-07-04
```text
Resume context for this step:
- board was rebooted and reachable again by serial on `COM3`
- current goal was to finish the AIC USB WiFi line:
  1. restore clean boot-time bring-up with `wlan0`
  2. verify the old `BUG: scheduling while atomic` no longer returns
  3. remove the transfer-time log storm / slowdown

Critical root cause #1 found in this step:
- `aic8800_fdrv/Makefile` still had:
  `CONFIG_PREALLOC_RX_SKB ?= n`
- even though the parent Makefile exported PREALLOC on, this local default
  compiled out the `#ifdef CONFIG_PREALLOC_RX_SKB` path in:
  `aic8800_fdrv/aicwf_txrxif.c`
- that meant `aicwf_prealloc_init()` / `aicwf_prealloc_exit()` were not
  actually called in the running `aic8800_fdrv.ko`, which directly matched the
  boot failure:
  `aicwf_prealloc_rxbuff_alloc prealloc list not initialized`
  `failed to alloc rxbuff`
  `aicwf_usb_bus_start rx prepare fail`
  no `wlan0`

Local Windows source-of-truth change:
- `C:\Users\26301\Desktop\_ugreen_cm762_driver\Linux\aic8800_linux_driver\drivers\aic8800\aic8800_fdrv\Makefile`
  changed:
  `CONFIG_PREALLOC_RX_SKB ?= n`
  -> `CONFIG_PREALLOC_RX_SKB ?= y`

Host backup and sync for this flag fix:
- backup:
  `~/LicheePi_Nano/backups/aic_fdrv_prealloc_flag_20260704_124330`
- synced host file:
  `~/LicheePi_Nano/third_party/aic8800_linux_driver/drivers/aic8800/aic8800_fdrv/Makefile`

Host build verification after the flag fix:
- rebuild used the kernel toolchain rule unchanged:
  `export PATH=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin:$PATH`
  `make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- KDIR=~/LicheePi_Nano/linux -j4`
- verified:
  `aic8800_fdrv/.aicwf_txrxif.o.cmd` contains `-DCONFIG_PREALLOC_RX_SKB`
- verified:
  `arm-linux-gnueabi-nm -u aic8800_fdrv/aic8800_fdrv.ko`
  now references:
  `aicwf_prealloc_init`
  `aicwf_prealloc_exit`
  `aicwf_prealloc_rxbuff_alloc`
  `aicwf_prealloc_rxbuff_free`

Host module md5 after this flag fix:
- `aic_load_fw.ko`
  `4b50214c3dc03d605dffcf6361feb408`
- `aic8800_fdrv.ko`
  `f2f53c2ee805028d54bff68b1f7b15f4`

Board deployment and reboot result after the flag fix:
- deployed by serial:
  `/lib/modules/5.7.1/extra/aic_load_fw.ko`
  -> `4b50214c3dc03d605dffcf6361feb408`
  `/lib/modules/5.7.1/extra/aic8800_fdrv.ko`
  -> `f2f53c2ee805028d54bff68b1f7b15f4`
- cold reboot result:
  * `musb_hdrc.use_dma = N`
  * `wlan0` created successfully
  * DHCP succeeded
  * board obtained:
    `10.128.140.136`
- important:
  the old boot-time failure
  `prealloc list not initialized`
  disappeared

Transfer-stress validation after the flag fix:
- uploaded a 12 MiB file to the board over the AIC WiFi path
- the old crash did not return:
  * no `BUG: scheduling while atomic`
  * no `Unable to handle kernel NULL pointer dereference`
- but throughput was still poor and dmesg flooded with:
  `aicwf_prealloc_rxbuff_alloc WARNING rxbuff is running out ...`

Critical root cause #2 found in this step:
- `aic_load_fw/aicwf_rx_prealloc.c` still had a low-watermark slowpath:
  when `rx_buff_list_ava < 10`, it printed a warning for nearly every packet
  and executed:
  `mdelay(10);`
- this was enough to destroy RX throughput and spam dmesg during file upload
  even though the driver no longer crashed

Local Windows source-of-truth change for the slowpath fix:
- `C:\Users\26301\Desktop\_ugreen_cm762_driver\Linux\aic8800_linux_driver\drivers\aic8800\aic_load_fw\aicwf_rx_prealloc.c`
- removed the low-watermark warning + `mdelay(10)` block entirely

Host backup and sync for the slowpath fix:
- backup:
  `~/LicheePi_Nano/backups/aic_prealloc_slowpath_fix_20260704_130257`
- synced host file:
  `~/LicheePi_Nano/third_party/aic8800_linux_driver/drivers/aic8800/aic_load_fw/aicwf_rx_prealloc.c`

Host build verification after the slowpath fix:
- rebuilt again with the same kernel toolchain rule
- new md5:
  `aic_load_fw.ko`
  `339bd0d0061b9c798a4e315dc0986e7d`
- `aic8800_fdrv.ko` stayed:
  `f2f53c2ee805028d54bff68b1f7b15f4`
- `strings aic_load_fw/aic_load_fw.ko` no longer shows:
  `rxbuff is running out`

Board deployment and reboot result after the slowpath fix:
- deployed via SSH to board:
  `/lib/modules/5.7.1/extra/aic_load_fw.ko`
  -> `339bd0d0061b9c798a4e315dc0986e7d`
- rebooted board
- post-reboot verification:
  * `wlan0` comes up normally
  * DHCP still succeeds to `10.128.140.136`
  * boot log still shows no `prealloc list not initialized`
  * boot log still shows no kernel oops / BUG

Final transfer-stress validation after the slowpath fix:
- same 12 MiB upload to `/tmp/stress_rx_12m.bin`
- previous timing:
  about `71.49 s`
- new timing:
  about `24.92 s`
- practical throughput improvement:
  roughly from `~172 KB/s` to `~505 KB/s`
- post-transfer dmesg check:
  * no `BUG: scheduling while atomic`
  * no `Unable to handle`
  * no `aicwf_prealloc_rxbuff_alloc WARNING rxbuff is running out`

Current known AIC state at end of this round:
- AIC USB WiFi now:
  * cold-boots successfully on this board
  * creates `wlan0`
  * auto-associates and gets DHCP
  * survives at least a 12 MiB RX-direction upload stress without the old RX
    prealloc crash
  * no longer floods dmesg with low-watermark RX buffer warnings
- current verified board module md5:
  `aic_load_fw.ko`
  `339bd0d0061b9c798a4e315dc0986e7d`
  `aic8800_fdrv.ko`
  `f2f53c2ee805028d54bff68b1f7b15f4`

Useful current board access note:
- serial remains the most reliable recovery path:
  `COM3`, `115200`, `root / 1`
- during this round, new local helper tools were created for repeat work:
  `C:\Users\26301\Desktop\_codex_serial_tools\serial_send_file.py`
  `C:\Users\26301\Desktop\_codex_serial_tools\serial_login_exec.py`
```

## U-Boot ST7701 360x640 Port - 2026-07-04
```text
User request:
- port the current working ST7701 screen init to U-Boot first

Chosen approach in this round:
- keep the existing legacy U-Boot sunxi LCD console path
- do not switch to a new DM/panel driver path yet
- run the current ST7701 3-wire SPI init sequence in `board_early_init_r()`
- then let legacy `VIDEO_SUNXI` bring up the framebuffer console

Current Linux-side source of truth used:
- `~/LicheePi_Nano/linux/arch/arm/boot/dts/suniv-f1c100s-licheepi-nano.dts`

Current panel wiring matched in U-Boot:
- `PA0 = CS`
- `PA1 = SDA / MOSI`
- `PA2 = SCL`
- `PA3 = RST`

## ESP8089 SPI PA0 CS Hold - 2026-07-22
```text
User reminder:
- ESP8089 SPI CS is PA0.
- After entering Linux, PA0 must be kept high.

Audit result:
- Current Linux pinctrl node `spi1_pa_pins` only muxes PA1/PA2/PA3 as spi1.
- PA0 is not part of `spi1_pa_pins`.
- The ESP8089 `spi_stub.c` registers `.chip_select = 0` but does not request or
  drive a separate PA0 GPIO.
- Current board `/root/load_wifi.sh` had lost the earlier PA0-high sysfs
  fallback before `insmod`.
- Therefore Linux did not guarantee PA0 stayed high before/probing the driver.

Host fixes applied:
- `/home/wnk/LicheePi_Nano/linux/arch/arm/boot/dts/suniv-f1c100s.dtsi`
  added a GPIO hog under `pio`:
  `esp8089_cs_pa0_hog`, `gpios = <0 0 0>`, `output-high`,
  `line-name = "esp8089-cs-pa0"`.
- `/home/wnk/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/root/load_wifi.sh`
  restored `keep_pa0_high()` before `insmod`.

Backup:
- `/home/wnk/LicheePi_Nano/backups/pa0_cs_high_20260722_065936`

Build verification:
- DTB build passed:
  `make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- suniv-f1c100s-licheepi-nano.dtb -j4`
- Built DTB:
  `/home/wnk/LicheePi_Nano/linux/arch/arm/boot/dts/suniv-f1c100s-licheepi-nano.dtb`
- sha256:
  `d78c981d1f2539d3bc52c2073b677a9884ac48d3394db860420512b4210cebd6`

Deployment note:
- Script fallback can be deployed to `/root/load_wifi.sh` immediately once
  board shell/network is available.
- GPIO hog requires deploying the rebuilt DTB to the boot partition and
  rebooting.
```

## U-Boot ILI9488 RGB Verified Index - 2026-07-29
```text
Detailed record:
- C:\Users\26301\Desktop\F1C200S_归档_脚本和资料\work_dirs\uboot_ili9488_rgb_20260727\README.md
- C:\Users\26301\Desktop\F1C200S_归档_脚本和资料\work_dirs\uboot_ili9488_rgb_20260727\DEBUG_LOG_20260729.md

Rule:
- This top-level document is only an index plus current known-good summary.
- Put future detailed ILI9488 U-Boot records in the U-Boot folder above.

Status:
- Verified working on board.
- Portrait 320x480 baseline passed: panel lights and U-Boot characters are
  normal.
- Final landscape 480x320 passed. User confirmed: display is perfect.

Current known-good landscape configuration:
- 0x36 MADCTL = 0x28
- address window:
  0x2A Column Address Set = 0x0000..0x01DF  (0..479)
  0x2B Page Address Set   = 0x0000..0x013F  (0..319)
- LCD mode:
  CONFIG_VIDEO_LCD_MODE="x:480,y:320,depth:16,pclk_khz:12000,le:8,ri:8,up:40,lo:20,hs:2,vs:10,sync:3,vmode:0"

Host source:
- /home/wnk/LicheePi_Nano/u-boot
- branch: panel-ili9488-rgb-320x480

Known-good host archive:
- /home/wnk/F1C200S_host_archive/known_good/20260729_uboot_ili9488_landscape_480x320
- md5:
  8b476b640c96b0ef41a0023f95c0e2a9
- sha256:
  3e1732a6088e364a9d44c83645ec2f4a115995eb2ac9196d4d864963dc2bc743

Windows archive copy:
- C:\Users\26301\Desktop\F1C200S_归档_脚本和资料\work_dirs\uboot_ili9488_rgb_20260727\u-boot-sunxi-with-spl.ili9488_landscape_480x320.bin

Board deployment/test:
- Board write/readback was verified with cmp result: WRITE_VERIFY_OK.
- Backup before this write:
  /root/roms/uboot_backups_20260729/mmcblk0_first1M_before_ili9488_landscape_480x320.bin
```

Host U-Boot files involved:
- `~/LicheePi_Nano/u-boot/board/sunxi/board.c`
- `~/LicheePi_Nano/u-boot/board/sunxi/Makefile`
- `~/LicheePi_Nano/u-boot/board/sunxi/ili9488-spi-init.c`
- `~/LicheePi_Nano/u-boot/configs/licheepi_nano_defconfig`
- `~/LicheePi_Nano/u-boot/include/configs/suniv.h`

Key source changes now in effect:
1. `include/configs/suniv.h`
   - added:
     `#define CONFIG_BOARD_EARLY_INIT_R`
2. `configs/licheepi_nano_defconfig`
   - enabled:
     `CONFIG_VIDEO_SUNXI=y`
     `CONFIG_VIDEO_LCD_MODE="x:360,y:640,depth:16,pclk_khz:14885,le:10,ri:8,up:6,lo:2,hs:3,vs:3,sync:3,vmode:0"`
     `CONFIG_VIDEO_LCD_DCLK_PHASE=0`
     `CONFIG_VIDEO_LCD_PANEL_PARALLEL=y`
     `CONFIG_VIDEO=y`
     `CONFIG_CFB_CONSOLE=y`
     `CONFIG_VGA_AS_SINGLE_DEVICE=y`
     `CONFIG_VIDEO_SW_CURSOR=y`
3. `board/sunxi/st7701-spi-init.c`
   - rewritten as an ASCII-clean helper for the current panel
   - includes reset handling on `PA3`
   - uses 3-wire 9-bit command/data write style
   - init table aligned to the currently working Linux ST7701 sequence
   - prints:
     `ST7701: init 360x640 RGB565 panel`
     `ST7701: init done`

Already-present repo hooks confirmed:
- `board/sunxi/board.c`
  calls `st7701_spi_bootloader_setup()` inside `board_early_init_r()`
- `board/sunxi/Makefile`
  includes `st7701-spi-init.o`

Important U-Boot display path note:
- current U-Boot legacy sunxi display path still uses:
  `drivers/video/sunxi/sunxi_display.c`
- this path already matches the local PD pin policy where `PD0` and `PD12`
  are skipped, which suits the current RGB565 wiring style on this board

Host backup for this port:
- `~/LicheePi_Nano/backups/uboot_st7701_port_20260703_225302`
- `BACKUP_LOG.txt` in that directory was rewritten cleanly in this round

Local Windows source-of-truth work dir:
- `C:\Users\26301\Desktop\F1C200S_uboot_st7701_work`

Host build environment result:
- initial build blocker was not ST7701 code but host-side Python headers for
  U-Boot `pylibfdt`
- host packages installed to resolve it:
  `python3-dev`
  `python-dev`
- U-Boot `setup.py` here uses `/usr/bin/env python`, so Python 2 headers were
  also required on this Ubuntu 16.04 host

Host build command that passed:
- `cd ~/LicheePi_Nano/u-boot`
- `export PATH=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin`
- `/usr/bin/make CROSS_COMPILE=arm-linux-gnueabi- licheepi_nano_defconfig`
- `/usr/bin/make CROSS_COMPILE=arm-linux-gnueabi- -j4`

Built artifacts verified on host:
- `~/LicheePi_Nano/u-boot/u-boot.bin`
- `~/LicheePi_Nano/u-boot/u-boot-sunxi-with-spl.bin`
- `~/LicheePi_Nano/u-boot/spl/u-boot-spl.bin`

Useful host verification captured:
- `strings u-boot.bin | grep 'ST7701:'`
  shows:
  `ST7701: init 360x640 RGB565 panel`
  `ST7701: init done`

Current scope limit:
- this round only ports panel wake/init + legacy LCD console enable
- no dedicated U-Boot color swap fix was added yet
- no dedicated U-Boot rotation feature was added
- no board-side flash/deploy test was done yet in this round

Next board-side validation target:
1. deploy the new U-Boot image to the board boot media
2. power on and capture serial log on `COM3`
3. confirm serial prints:
   `ST7701: init 360x640 RGB565 panel`
   `ST7701: init done`
4. confirm the panel wakes before Linux and U-Boot console is visible
5. then judge whether color order or timing still needs a U-Boot-side tweak
```

## Kernel ST7701 Init Disabled For U-Boot Isolation Test - 2026-07-04
```text
Purpose:
- temporarily disable the kernel-side custom ST7701 SPI init helper so the next
  display validation can isolate whether U-Boot alone brings the panel up

Important distinction:
- the active kernel-side ST7701 init on this tree is not the generic
  `panel-sitronix-st7701` DSI driver
- it is the custom platform helper:
  `drivers/staging/shirogane/st7701init.c`
- that helper is instantiated by the DTS node:
  `st7701initseq: st7701initseq`
  in:
  `arch/arm/boot/dts/suniv-f1c100s-licheepi-nano.dts`

Chosen minimal change:
- keep the RGB DRM/simple-panel display chain intact
- only disable the custom ST7701 init node in DTS:
  `status = "okay"` -> `status = "disabled"`

Local Windows source-of-truth file:
- `C:\Users\26301\Desktop\F1C200S_uboot_st7701_work\suniv-f1c100s-licheepi-nano.dts`

Host backup for this DTS-only test:
- `~/LicheePi_Nano/backups/kernel_disable_st7701init_20260703_230735`

Host file synced:
- `~/LicheePi_Nano/linux/arch/arm/boot/dts/suniv-f1c100s-licheepi-nano.dts`

Host rebuild command used:
- `cd ~/LicheePi_Nano/linux`
- `export PATH=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin:$PATH`
- `make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- suniv-f1c100s-licheepi-nano.dtb -j4`

Verified artifact:
- `~/LicheePi_Nano/linux/arch/arm/boot/dts/suniv-f1c100s-licheepi-nano.dtb`

Expected runtime effect on next board boot:
- Linux should no longer print the custom init helper messages such as:
  `srgn: st7701 init work handler running`
  `srgn: st7701 init (work) completed`
- panel wake/init should come only from U-Boot if the new U-Boot image is
  deployed together with this DTB

Practical next deployment bundle for isolation test:
1. updated U-Boot image from:
   `~/LicheePi_Nano/u-boot/u-boot-sunxi-with-spl.bin`
2. updated kernel DTB from:
   `~/LicheePi_Nano/linux/arch/arm/boot/dts/suniv-f1c100s-licheepi-nano.dtb`
3. then cold boot and observe both serial and screen behavior
```

## Android ADB Host + 360x640 Recording Breakthrough - 2026-07-08 to 2026-07-09
```text
User goal in this round:
- make the F1C200S board act as an ADB host over USB
- use that path as the foundation for future wired/wireless phone casting
- first milestone: record the Android screen at 360x640 on the board

Board state used in this round:
- board IP during successful tests:
  `10.0.0.107`
- board login:
  `root / 1`
- phone enumerated on board USB as:
  `18d1:4ee7`
- detected ADB serial:
  `dda57287`

What was proven first:
1. USB enumeration was already working once the phone exposed the right ADB mode.
2. Host authentication was also working:
   old logs already showed:
   `AUTH -> CNXN -> device`
3. The actual blocker was the old local ADB server path on the board, not USB.

Critical root causes found:
1. Old target-side `adb` on the board did not exist in a usable host form, so a
   target binary had to be built manually from Buildroot `android-tools`.
2. The old host path attempted to use the local smartsocket/server model and
   initially failed on:
   `cannot bind 'tcp:5037'`
3. The board's loopback/local server behavior was unreliable enough that:
   - server bind on loopback could fail
   - client-to-local-server access over `127.0.0.1` was also a practical source
     of hangs during debugging

Local Windows work directory for this round:
- `C:\Users\26301\Desktop\_adb_host_enable`

Important local files created/updated:
- `C:\Users\26301\Desktop\_adb_host_enable\adb.c`
- `C:\Users\26301\Desktop\_adb_host_enable\src\commandline.c`
- `C:\Users\26301\Desktop\_adb_host_enable\record_android_360x640.sh`

Target-side ADB binaries produced in this round:
1. `/root/adb`
   - manually built target-side ADB host baseline
2. `/root/adb_usbonly`
   - patched host mode with:
     `ADB_HOST_USB_ONLY=1`
   - skips local smartsocket setup and proved USB/auth/CNXN were good
3. `/root/adb_fallback`
   - added listener fallback in `local_name_to_fd()`:
     if `socket_loopback_server()` fails for `tcp:5037`,
     fall back to `socket_inaddr_any_server()`
   - this was the critical fix that brought up a working board-side ADB server
4. `/root/adb_execout`
   - first pass adding `exec-out`
5. `/root/adb_execout2`
   - final improved version
   - `exec-out` added in commandline
   - shell v2 packet parsing added so raw stdout is emitted correctly instead of
     forwarding framed shell protocol bytes

Source-level changes confirmed in this round:
1. `core/adb/adb.c`
   - in `local_name_to_fd("tcp:...")`:
     attempt loopback server first
     if that fails, log and fall back to INADDR_ANY listener
2. `core/adb/commandline.c`
   - added:
     `adb exec-out <command>`
   - implemented `read_shell_v2_raw()` to parse shell v2 packets:
     * packet type `0x01` -> stdout
     * packet type `0x02` -> stderr
     * packet type `0x03` -> exit code
   - this made `exec-out echo ...` return plain data instead of framed bytes

Concrete ADB functionality verified on the board:
- `/root/adb_fallback devices`
- `/root/adb_fallback get-state`
- `/root/adb_fallback shell`
- `/root/adb_fallback push`
- `/root/adb_fallback pull`
- `/root/adb_execout2 exec-out echo pure_test`

Important observed outputs:
- `adb devices` showed:
  `dda57287    device`
- `adb get-state` returned:
  `device`
- `adb shell getprop ro.product.model` worked
- push/pull roundtrip worked
- `exec-out echo pure_test` returned exactly:
  `pure_test`

This is the major breakthrough:
- the F1C200S board can now operate as a usable Android ADB USB host
- this removes the main control-plane blocker for future screen casting

Phone recording findings:
1. `screenrecord` on the phone works through the board ADB host path.
2. File-based record+pull is already reliable.
3. `screenrecord` at `360x640` was explicitly verified.
4. Earlier tiny MP4 files were not a command-format bug; they happened because
   the recorded scene was effectively static and the encoder only emitted the
   initial frame.
5. When the phone was placed on a changing screen / video playback, recording
   produced a full real clip.

Verified successful 360x640 recording command shape:
- board command path:
  `/root/adb_execout2 shell screenrecord --verbose --size 360x640 --bit-rate 4000000 --time-limit <N> /sdcard/<name>.mp4`
  then:
  `/root/adb_execout2 pull /sdcard/<name>.mp4 /root/<name>.mp4`

Important validated 360x640 samples:
1. Settings-motion sample:
   - `codec_name=h264`
   - `width=360`
   - `height=640`
   - `duration=6.330700`
   - `nb_frames=364`
   - `size=619985`
2. Video-playback sample:
   - saved on board as:
     `/root/f1c_now_video.mp4`
   - `codec_name=h264`
   - `width=360`
   - `height=640`
   - `duration=9.998300`
   - `nb_frames=389`
   - effective source fps reported around 60
   - `size=862629`

Useful recorder logs from the successful video-playback sample:
- `Configuring recorder for 360x640 video/avc at 4.00Mbps`
- `recorded 389 frames in 10 seconds`

Current ready-to-use helper script deployed to the board:
- `/root/record_android_360x640.sh`

Script purpose:
- start board-side ADB server
- record Android screen at 360x640
- pull the MP4 back to `/root`
- print `ffprobe` info

Script default behavior:
- ADB binary:
  `/root/adb_execout2`
- default output filename:
  `f1c_android_360x640.mp4`
- default duration:
  `10`
- default bitrate:
  `4000000`
- default size:
  `360x640`

Script usage examples:
- default:
  `/root/record_android_360x640.sh`
- custom filename:
  `/root/record_android_360x640.sh myvideo.mp4`
- override duration/bitrate/size:
  `DURATION=15 BIT_RATE=6000000 SIZE=360x640 /root/record_android_360x640.sh movie.mp4`

Current scope status at end of this round:
- ADB host control plane: working
- 360x640 phone recording to MP4 file on board: working
- raw stdout `exec-out` path: working for simple commands
- direct non-file video streaming path: not finished yet

Most important next step after resume:
1. stop using file-based `screenrecord` as the final path
2. find the correct direct stdout video emission path from the phone side
3. connect that stream to the board decode/display pipeline
4. then extend to wireless transport after the wired path is stable

Practical significance:
- this is the first confirmed end-to-end phone-to-board video acquisition path
  at the target portrait resolution `360x640`
- it materially reduces the remaining distance to wired casting first, then
  wireless casting
```

## AIC8800 USB Bluetooth BlueZ Port Prep - 2026-07-13
```text
Purpose in this round:
- continue the AIC8800 USB combo-dongle line, specifically the Bluetooth side
- verify whether the board can use a standard Linux-host Bluetooth path instead
  of the current Android/vendor-only path

Key source finding:
- the currently used board-side/vendor tree:
  `~/LicheePi_Nano/third_party/aic8800_linux_driver/drivers/aic8800/aic_load_fw`
  does include Bluetooth-related code, but it does not expose a standard Linux
  `hci0` device by itself
- the missing piece is a separate USB Bluetooth HCI driver:
  `aic_btusb`

Reference trees used:
- host clone:
  `~/LicheePi_Nano/third_party/aic8800_radxa_ref`
- host clone:
  `~/LicheePi_Nano/third_party/aic8800_bt_fix_ref`

Important practical correction:
- the community BlueZ patch found in the wild only flips:
  `CONFIG_PLATFORM_UBUNTU` branch in `aic_btusb.h`
- that is not sufficient for this F1C200S board build, because the module here
  is compiled with:
  `CONFIG_PLATFORM_ALLWINNER=y`
- therefore both branches in `aic_btusb.h` must use:
  `CONFIG_BLUEDROID = 0`
  or the board will still build the Android/Bluedroid path instead of BlueZ

Additional compatibility tweak prepared in this round:
- added support in `aic_btusb.c` for:
  `USB_PRODUCT_ID_AIC8800D80_ALT = 0x8d80`
- and added that ID to the `btusb_table[]`
- this is to match the user's observed hardware IDs where community guides
  mention both `a69c:8d80` and `a69c:8d81`

Host-side source-of-truth local work dir:
- `C:\Users\26301\Desktop\_aic_btusb_bluez_work`

Local files changed in this round:
- `C:\Users\26301\Desktop\_aic_btusb_bluez_work\aic_btusb.h`
- `C:\Users\26301\Desktop\_aic_btusb_bluez_work\aic_btusb.c`

Host backup for this round:
- `~/LicheePi_Nano/backups/aic_btusb_bluez_prepare_<timestamp>`
- contains `BACKUP_LOG.txt` and the pre-change `aic_btusb` directory copy

Important host repo anomaly found:
- in the initial worktree clone, the checked-out file:
  `src/USB/driver_fw/drivers/aic_btusb/aic_btusb.c`
  was unexpectedly `0` bytes
- but the git object itself was intact:
  `git ls-tree -l HEAD .../aic_btusb.c`
  reported:
  `161772`
- the full source was recovered from:
  `git show HEAD:src/USB/driver_fw/drivers/aic_btusb/aic_btusb.c`

Host-side build result after the BlueZ prep:
- Bluetooth USB driver built successfully against the board kernel:
  `~/LicheePi_Nano/third_party/aic8800_radxa_ref/src/USB/driver_fw/drivers/aic_btusb/aic_btusb.ko`
- build command used:
  `make KDIR=~/LicheePi_Nano/linux ARCH=arm CROSS_COMPILE=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi- CONFIG_PLATFORM_UBUNTU=n CONFIG_PLATFORM_ALLWINNER=y -j4`
- matching loader module also built successfully:
  `~/LicheePi_Nano/third_party/aic8800_radxa_ref/src/USB/driver_fw/drivers/aic8800/aic_load_fw/aic_load_fw.ko`

Current conclusion:
- yes, this driver is for a Linux USB host and the F1C200S board qualifies in
  principle
- the earlier failure was not because the board is not a Linux host
- the real blockers were:
  1. using only the vendor loader tree without `aic_btusb`
  2. the default Android/Bluedroid mode instead of BlueZ mode
  3. the checked-out `aic_btusb.c` anomaly in the reference host clone

Immediate next board-side validation when deploying:
1. install matching `aic_load_fw.ko` and `aic_btusb.ko`
2. ensure required AIC firmware files are present under `/lib/firmware`
3. insert the AIC USB dongle
4. verify:
   - `dmesg | grep -i aic`
   - `ls /sys/class/bluetooth`
   - `hciconfig -a`
5. success criterion:
   - `hci0` appears and BlueZ tools can see it
```

## AIC8800D80 Host Validation Via olamellberg Repo - 2026-07-14
```text
User direction in this round:
- stop using the old UGREEN tree as the main base
- switch to the workflow implied by:
  https://github.com/olamellberg/AIC8800D80
- first make the Linux host succeed with both WiFi and Bluetooth
- only then continue board-side porting

Important repo/layout conclusion:
- `olamellberg/AIC8800D80` is mainly an installer/wrapper repo
- the actual working source base is the Radxa AIC tree used by that repo
- host active source tree for this round:
  `~/LicheePi_Nano/third_party/aic8800d80_repo_host_20260714`

Key working host-side source changes:
1. WiFi driver `aic8800_fdrv`
   - add `0x8d83` USB ID support
   - files:
     `src/USB/driver_fw/drivers/aic8800/aic8800_fdrv/aicwf_usb.h`
     `src/USB/driver_fw/drivers/aic8800/aic8800_fdrv/aicwf_usb.c`
2. Bluetooth driver `aic_btusb`
   - force BlueZ mode in both platform branches:
     `CONFIG_BLUEDROID = 0`
   - add a clean single `0x8d83` USB ID entry
   - keep existing `0x8d80` and `0x8d81` support
   - files:
     `src/USB/driver_fw/drivers/aic_btusb/aic_btusb.h`
     `src/USB/driver_fw/drivers/aic_btusb/aic_btusb.c`

Important host result proved in this round:
- with the AIC USB card inserted, host `lsusb` becomes:
  `a69c:8d81`
- WiFi initializes successfully:
  interface observed as:
  `wlxdc2e9794eb68`
- Bluetooth also initializes successfully:
  controller observed as:
  `DC:2E:97:94:E3:87`

Two host Bluetooth paths were validated:
1. Generic kernel `btusb`
   - after loading the working Radxa loader + WiFi driver, generic `btusb`
     already binds and exposes the AIC controller successfully
2. Repo-side `aic_btusb`
   - after enabling BlueZ mode and cleaning the ID table, `aic_btusb`
     also binds successfully and exposes the AIC controller successfully

Concrete host validation snapshot:
- `lsusb -t` showed:
  * interface 0 -> `aic_btusb` or `btusb`
  * interface 1 -> `aic_btusb` or `btusb`
  * interface 2 -> `aic8800_fdrv`
- `hciconfig -a` showed:
  * `hci0` or `hci1`
  * `BD Address: DC:2E:97:94:E3:87`
  * `Name: 'ubuntu'` or previously `AIC8820`
  * `UP RUNNING`

Built host x86_64 modules in this round:
- `~/LicheePi_Nano/third_party/aic8800d80_repo_host_20260714/src/USB/driver_fw/drivers/aic8800/aic_load_fw/aic_load_fw.ko`
- `~/LicheePi_Nano/third_party/aic8800d80_repo_host_20260714/src/USB/driver_fw/drivers/aic8800/aic8800_fdrv/aic8800_fdrv.ko`
- `~/LicheePi_Nano/third_party/aic8800d80_repo_host_20260714/src/USB/driver_fw/drivers/aic_btusb/aic_btusb.ko`

Board-side ARM cross-build note discovered here:
- `aic_load_fw.ko` builds directly against the board kernel tree
- `aic8800_fdrv.ko` depends on exported symbols from `aic_load_fw`
- therefore the board-side `aic8800_fdrv` build must pass:
  `KBUILD_EXTRA_SYMBOLS=<...>/aic_load_fw/Module.symvers`
- without that, modpost fails with undefined references such as:
  `get_fw_path`, `set_testmode`, `aicwf_prealloc_txq_alloc`

Board-side ARM build commands that passed:
- export kernel toolchain path first:
  `export PATH=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin:$PATH`
- loader:
  `make KDIR=~/LicheePi_Nano/linux ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- CONFIG_PLATFORM_UBUNTU=n CONFIG_PLATFORM_ALLWINNER=y`
- WiFi:
  `make KDIR=~/LicheePi_Nano/linux ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- CONFIG_PLATFORM_UBUNTU=n CONFIG_PLATFORM_ALLWINNER=y KBUILD_EXTRA_SYMBOLS=~/LicheePi_Nano/third_party/aic8800d80_repo_host_20260714/src/USB/driver_fw/drivers/aic8800/aic_load_fw/Module.symvers`
- Bluetooth:
  `make KDIR=~/LicheePi_Nano/linux ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- CONFIG_PLATFORM_UBUNTU=n CONFIG_PLATFORM_ALLWINNER=y`

Host backup created during the cleanup step:
- `~/LicheePi_Nano/backups/aic8800d80_host_btusb_cleanup_20260714_104449`
```

## AIC8800D80 Host Known-Good Baseline - 2026-07-14
```text
Purpose in this round:
- stop mixing host and board experiments
- rebuild host support again from a clean tree
- preserve a known-good host baseline before any later board-side port

Clean host source tree used:
- `~/LicheePi_Nano/third_party/aic8800d80_host_fresh_20260714`

How the clean tree was prepared:
- `git clone https://github.com/radxa-pkg/aic8800 aic8800d80_host_fresh_20260714`
- `git submodule update --init --recursive`

Important discipline from this point:
- this host good-state tree is the current source of truth for AIC host work
- do not reuse the older mixed experiment tree as the primary host baseline
- for host validation, do not use `aic_btusb` unless there is a specific reason
- the currently verified good host Bluetooth path is the kernel's generic `btusb`

Host build commands that produced the good state:
- loader:
  `make KDIR=/lib/modules/$(uname -r)/build CONFIG_PLATFORM_UBUNTU=y`
- WiFi:
  `make KDIR=/lib/modules/$(uname -r)/build CONFIG_PLATFORM_UBUNTU=y KBUILD_EXTRA_SYMBOLS=~/LicheePi_Nano/third_party/aic8800d80_host_fresh_20260714/src/USB/driver_fw/drivers/aic8800/aic_load_fw/Module.symvers`

Built host module fingerprints:
- `aic_load_fw.ko`
  `cb0d868f28d272ee2b67ddd91b4590ea`
- `aic8800_fdrv.ko`
  `ac3d58b6b5fc76b953fdea23f33b95e5`

Known-good host load/result sequence:
1. keep system `btusb`
2. insert host-built `aic_load_fw.ko`
3. insert host-built `aic8800_fdrv.ko`
4. device transitions:
   - initial: `a69c:8d80`
   - working combo state: `a69c:8d81`
5. expected USB binding in the good state:
   - `If0 -> btusb`
   - `If1 -> btusb`
   - `If2 -> aic8800_fdrv`

Concrete host verification captured:
- `lsusb` showed:
  `Bus 001 Device 047: ID a69c:8d81`
- `lsusb -t` showed:
  `If0/1 -> btusb`
  `If2 -> aic8800_fdrv`
- network interface appeared as:
  `wlxdc2e9794eb68`
- WiFi successfully connected to:
  `DCKJ`
- link info on the working host state:
  * freq `2462`
  * signal `-54 dBm`
  * tx bitrate `180.0 MBit/s VHT-MCS 9 40MHz VHT-NSS 1`

Important network observation during host testing:
- on `DCKJ`, the card received:
  `192.168.16.11/24`
- local gateway `192.168.16.1` was reachable
- public Internet from that WiFi path was not reachable at test time
- therefore failure to download/speed-test in this round was not a driver bring-up failure

Bluetooth verification on the good host state:
- AIC Bluetooth stayed on generic `btusb`
- controller observed as:
  `hci0`
  `BD Address: DC:2E:97:94:E3:87`
  `UP RUNNING`
- `btmgmt -i hci0 find` successfully discovered nearby LE devices
- so the currently verified good host combo solution is:
  * Bluetooth: generic `btusb`
  * WiFi: `aic_load_fw + aic8800_fdrv`

Host backup created for this good state:
- `~/LicheePi_Nano/backups/aic8800d80_host_good_20260714_114612`
- contains:
  * `source_tree/`
  * `BACKUP_LOG.txt`

Practical next-step rule:
- for future board-side porting, start from this host-known-good baseline
- port WiFi first from this tree
- treat Bluetooth-on-board as a separate later step
```

## AIC8800D80 Board MUSB Re-enumeration Fix Attempt - 2026-07-15
```text
Current board IP used for direct Windows SSH:
- 10.0.0.107

Important current conclusion:
- AIC loader reaches firmware completion on the F1C200S board.
- Logs show:
  fw download complete
  app_cmp
- The remaining failure is after firmware download, when the AIC device resets
  and should re-enumerate as the second-stage USB device.
- The F1C200S MUSB host then often fails descriptor reads:
  device descriptor read/64, error -110
  unable to enumerate USB device
- So the current blocker is MUSB/post-download re-enumeration timing, not
  missing firmware or missing aic8800_fdrv ID support.

Runtime script changed:
- Source-of-truth:
  C:\Users\26301\Desktop\board_tools_f1c200s\runtime_bundle\rootfs_overlay\etc\init.d\S17rtl8723bu
- Synced to host:
  ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/etc/init.d/S17rtl8723bu
- Deployed to board:
  /etc/init.d/S17rtl8723bu
- Current md5:
  b42f8fbffb89b826b1aa2ff6b3886bf3

Current script strategy:
- Start aic_load_fw in background.
- Poll dmesg for app_cmp.
- As soon as app_cmp appears, immediately reload the sunxi MUSB host before
  the descriptor retry loop starts.
- Do not sleep after app_cmp; measured log window from app_cmp to AIC USB
  disconnect was only about 0.24 seconds in the user's captured log.

Validation needed:
1. Boot or insert with the AIC card present.
2. Confirm the log prints:
   [wifi-mod] aic app_cmp seen, reload sunxi before descriptor retry
3. Confirm whether a69c:8d83 appears and aic8800_fdrv creates wlan.
4. If it still fails, tune around immediate MUSB reload timing instead of
   changing firmware/module source first.
```

## AIC8800D80 Board USB Stage2 Failure Update - 2026-07-15
```text
Current board default:
- Keep RTL8723 (`0bda:b720`) as default network path.
- Do not auto-run AIC on boot when fallback WiFi exists.
- AIC test remains manual:
  `/etc/init.d/S17rtl8723bu aic-test`
- MUSB DMA must remain off:
  `/sys/module/musb_hdrc/parameters/use_dma = N`

Current deployed script:
- Source-of-truth:
  C:\Users\26301\Desktop\board_tools_f1c200s\runtime_bundle\rootfs_overlay\etc\init.d\S17rtl8723bu
- Deployed board md5:
  `4f51d99afd07fa27a825e74a44033c1c`
- Synced host runtime bundle:
  ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/etc/init.d/S17rtl8723bu
- New behavior:
  failed manual AIC tests call `/sbin/rmmod aic_load_fw` so a failed loader
  does not remain registered and poison the next test.

Loader experiment result:
- Built and deployed a test `aic_load_fw_manual.ko` that, after firmware
  download, printed:
  `aic loader fw complete, release stage1 usb interface`
  then deinitialized the stage1 bus/URBs and returned `-ENODEV`.
- The experiment proved the change took effect, but did not fix stage2:
  AIC still disconnected after firmware and failed descriptor reads:
  `usb 1-1.1: device descriptor read/64, error -110`
- Original loader restored on board:
  `/lib/modules/5.7.1/extra/aic_load_fw_manual.ko`
  md5 `409d303bf2651c18d3e1f98a7f028256`
- Experimental loader md5 was:
  `61b2085255e89dd0a177d3983c210754`

Isolation result:
- Removing/ignoring RTL8723 and testing AIC alone did not fix the problem.
- Therefore the failure is not caused by the fallback RTL8723 driver owning
  another hub port.

Current failure signatures:
1. Sometimes AIC reaches firmware completion:
   `fw download complete`
   `app_cmp`
   then fails stage2 enumeration:
   `usb 1-1.1: device descriptor read/64, error -110`
   `unable to enumerate USB device`
2. Other times, after a previous failed stage2/reset cycle, AIC no longer
   reaches firmware upload and fails earlier at:
   `40500000 rd fail try 1: -110`
   `cmd queue crashed`
   `aic d80 system_config try N failed: -32`

USB topology and power observations:
- Hub:
  `1-1 214b:7250 USB2.0 HUB`, self-reports `bMaxPower=100mA`
- AIC:
  `1-1.1 a69c:8d80 AIC Wlan`, self-reports `bMaxPower=500mA`
- RTL8723 fallback:
  `1-1.3 0bda:b720 802.11n WLAN Adapter`, self-reports `bMaxPower=500mA`
- DTS only enables `&usb_otg` host and `&usbphy`; no real USB VBUS regulator
  is declared.
- Runtime logs show:
  `usb_phy_generic usb_phy_generic.0.auto: supply vcc not found, using dummy regulator`
- `/sys/class/regulator` is empty on the board.

Current conclusion:
- Shell timing, AIC firmware path, AIC loader lingering after app_cmp, and
  fallback RTL8723 contention have been tested and are not the root cause.
- Remaining blocker is the physical/MUSB/hub stage2 enumeration path after the
  AIC firmware-triggered USB reset.
- Strong candidates:
  1. AIC stage2 reset causes a power transient that the shared hub/VBUS cannot
     sustain.
  2. AIC stage2 high-speed chirp/descriptor phase is marginal on this board's
     D+/D- routing or hub path.
  3. F1C200S MUSB host has a compatibility issue with this device's immediate
     post-firmware disconnect/reconnect sequence.

Practical next hardware tests:
1. Test AIC through a separately powered external USB hub.
2. Test AIC alone on the board without RTL8723 installed and with an independent
   fallback console path.
3. Probe/measure 5V and 3.3V rails during `fw download complete -> USB disconnect`.
4. If possible, force a real port power cycle for AIC port 1 after app_cmp using
   hub hardware, because this kernel/sysfs hub exposes no per-port `disable`.
```

## AIC8800D80 Board Stage2 Update - 2026-07-16
```text
Current stable board defaults:
- Board IP: 10.0.0.107
- Keep RTL8723 fallback as the default network path.
- AIC remains manual only:
  /etc/init.d/S17rtl8723bu aic-test
- MUSB DMA remains off:
  /sys/module/musb_hdrc/parameters/use_dma = N

Current deployed AIC loader:
- /lib/modules/5.7.1/extra/aic_load_fw_manual.ko
- md5:
  81cab939b799d3c8ea2b0cfda9b075e5
- This is the Radxa ARM loader build.
- It is better than the previous 409d303b loader for this board because it
  reliably reaches the D80 patch/firmware path and app_cmp when the firmware
  path is available.

Current deployed WiFi init script:
- Source-of-truth:
  C:\Users\26301\Desktop\board_tools_f1c200s\runtime_bundle\rootfs_overlay\etc\init.d\S17rtl8723bu
- Board md5 after this round:
  5b508bf5c8650da42fa9b30b3b7c5e9e
- Host sync path:
  ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/etc/init.d/S17rtl8723bu
- New behavior:
  prepare_aic_firmware_path creates:
  /vendor/etc/firmware -> /lib/firmware/aic8800D80
  before AIC loader tests.
- This is needed because the Radxa loader asks for firmware under:
  /vendor/etc/firmware/

Important confirmed result:
- With Radxa loader and /vendor/etc/firmware symlink, AIC reaches:
  fw_patch_table_8800d80_u02.bin loaded
  fw_adid_8800d80_u02.bin loaded
  fw_patch_8800d80_u02.bin loaded
  fmacfw_8800d80_u02.bin loaded
  Read FW mem: 00120198
  120198=16f094
  1201a0=174000
  rd_version_val=00000000
  app_cmp
- After app_cmp, the device disconnects and the second-stage enumeration still
  fails:
  usb 1-1.1: device descriptor read/64, error -110
  device not accepting address
  unable to enumerate USB device

Kernel descriptor-delay experiment:
- A 1500 ms MUSB descriptor-read settle patch was deployed first.
- A 5000 ms variant was built, deployed, and tested.
- The 5000 ms variant did not fix either boot-time AIC enumeration instability
  or post-app_cmp stage2 enumeration.
- Board boot partition was reverted to the 1500 ms zImage.
- Host kernel source was also reverted to the 1500 ms patch and rebuilt so
  future builds do not accidentally carry the failed 5000 ms experiment.
- Current host rebuilt 1500 ms zImage md5:
  b9265854ca1bb0575dab8e9d0f7f92a9
- Current board booted kernel observed after revert:
  Linux 5.7.1 #113 Wed Jul 15 08:23:09 PDT 2026

Isolation result update:
- Testing while 8723bu was unloaded or temporarily hidden did not produce a
  working AIC stage2 path.
- In a bad post-failure state, AIC may disappear from lsusb entirely until a
  reboot or stronger power cycle.
- This further supports the conclusion that the remaining blocker is the
  physical/MUSB/hub/AIC reset re-enumeration path, not firmware file selection
  or fallback WiFi driver contention.

USB hub/sysfs result:
- The onboard hub path exposes normal device authorized/remove files but no
  useful per-port power disable control for port 1.
- Software MUSB host reload has already been tested and does not substitute for
  a real AIC port VBUS power cycle.

Recommended next step:
1. Test the AIC module through a separately powered external USB hub.
2. If it still fails, test the AIC module on a short direct/high-quality USB path.
3. Measure VBUS/3.3V around:
   fmacfw upload complete -> app_cmp -> USB disconnect -> stage2 descriptor read.
4. If hardware can be modified, add a controllable AIC VBUS/load-switch or hub
   port power switch so the script can do a real post-app_cmp port power cycle.
```

## AIC8800D80 WiFi-Only Loader Result - 2026-07-17
```text
Current board IP:
- 10.0.0.107

Current host IP:
- 192.168.175.138

User-directed scope:
- only prove AIC8800D80 WiFi-only firmware download
- do not load fdrv/stage2 driver yet
- script should stop at app_cmp
- keep RTL8723 fallback as the live SSH/network path
- keep MUSB DMA off

Important source result:
- UGREEN CM762 zip source is RELEASE DATE 2025_0605_3d4cc869.
- On the F1C200S board this 2025 loader completed the first bulk OUT command
  but got no RX response from bootrom:
  cmd timed-out
  40500000 rd fail: -110
- Enabling CONFIG_USB_NO_TRANS_DMA_MAP caused a board reset on this MUSB path.
  Do not enable it here; MUSB must remain non-DMA/PIO.

Working source:
- Host tree:
  ~/LicheePi_Nano/third_party/aic8800d80_shen_wifi_only_loader_20260717
- Base:
  ~/LicheePi_Nano/third_party/aic8800d80_shen
- Release:
  2026_0123_5f7be68d

Working loader changes:
- drivers/aic8800/aic_load_fw/Makefile
  CONFIG_USB_BT = n
  CONFIG_USB_MSG_EP = n
  CONFIG_USB_NO_TRANS_DMA_MAP = n
- drivers/aic8800/aic_load_fw/aicwf_usb.c
  changed the two D80 patch table guards from:
    #if 1//def CONFIG_USB_BT
  to:
    #ifdef CONFIG_USB_BT
- drivers/aic8800/aic_load_fw/aicbluetooth_cmds.c
  cmd_mgr_queue returns the real cmd result on timeout instead of false success
  and only frees cmd when result == 0.
- drivers/aic8800/aic_load_fw/aic_compat_8800d80x2.c
  moved int i declaration outside CONFIG_USB_BT so WiFi-only builds compile.

Build command:
cd ~/LicheePi_Nano/third_party/aic8800d80_shen_wifi_only_loader_20260717/drivers/aic8800/aic_load_fw
export PATH=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin:$PATH
/usr/bin/make KDIR=/home/wnk/LicheePi_Nano/linux ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- CONFIG_PLATFORM_UBUNTU=n CONFIG_PLATFORM_ALLWINNER=y -j4

Built module:
- md5:
  88d46d3abbf127f6e288f2f88e1f7bab

Deployed board path:
- /lib/modules/5.7.1/extra/aic_load_fw_wifi_only.ko

Board validation:
- /etc/init.d/S17rtl8723bu aic-test
- Result:
  chip_id=7, chip_mcu_id=0
  fw_patch_table_8800d80_u02.bin uploaded
  fw_adid_8800d80_u02.bin uploaded
  fw_patch_8800d80_u02.bin uploaded
  fmacfw_8800d80_u02.bin uploaded
  fw download complete
  rd_version_val=00000000
  app_cmp
  [wifi-mod] aic wifi-only firmware upload reached app_cmp, stop here

Confirmed non-errors in the working 2026 loader test:
- no cmd timed-out before firmware upload
- no 40500000 rd fail before firmware upload
- no bin upload fail
- no aic_load_fw module remains loaded afterward
- RTL8723 fallback remains loaded
- /sys/module/musb_hdrc/parameters/use_dma remains N

Remaining separate issue:
- After app_cmp the AIC device still performs its USB disconnect/re-enumeration.
- On this board/hub/MUSB path stage2 descriptor read can still fail:
  usb 1-1.1: device descriptor read/64, error -110
- That is post-firmware stage2 enumeration, not the WiFi-only firmware download
  phase. Do not confuse it with the 2025 UGREEN bootrom timeout failure.
```

## AIC8800D80 Host-Good Source Board Rebuild - 2026-07-22
```text
User direction:
- Do not reuse old board-failed diagnostic AIC drivers.
- Use the current Ubuntu host AIC source that is proven working for Miracast.

Host working chain:
- Script:
  /home/wnk/F1C200S_host_archive/host_scripts/aic8800_lazycast_host/start_aic8800_miracast_cdump_host.sh
- AIC source root used by that script:
  /home/wnk/LicheePi_Nano/third_party/aic8800d80_repo_host_20260714/src/USB/driver_fw/drivers/aic8800
- Host verified state before board rebuild:
  lsusb: a69c:8d81
  modules: aic_load_fw + aic8800_fdrv
  interface: aic0

Board ARM rebuild tree:
- /home/wnk/LicheePi_Nano/third_party/aic8800d80_host_good_board_arm_20260722
- Created by copying the host-good AIC source root above.
- The host-good tree itself was not edited.

Build rule used:
cd /home/wnk/LicheePi_Nano/third_party/aic8800d80_host_good_board_arm_20260722/aic_load_fw
export PATH=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin:$PATH
make KDIR=/home/wnk/LicheePi_Nano/linux ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- CONFIG_PLATFORM_UBUNTU=n CONFIG_PLATFORM_ALLWINNER=y -j4

cd /home/wnk/LicheePi_Nano/third_party/aic8800d80_host_good_board_arm_20260722/aic8800_fdrv
make KDIR=/home/wnk/LicheePi_Nano/linux ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- CONFIG_PLATFORM_UBUNTU=n CONFIG_PLATFORM_ALLWINNER=y KBUILD_EXTRA_SYMBOLS=/home/wnk/LicheePi_Nano/third_party/aic8800d80_host_good_board_arm_20260722/aic_load_fw/Module.symvers -j4

Built ARM modules:
- aic_load_fw.ko md5:
  d1538608be2ceca2ac77d59792681562
- aic8800_fdrv.ko md5:
  952b4453ca590dcd96525c7fb6783253

Board deployment:
- /lib/modules/5.7.1/extra/aic_load_fw_host_good_arm.ko
- /lib/modules/5.7.1/extra/aic8800_fdrv_host_good_arm.ko
- Existing board AIC modules backed up:
  /root/backup_aic_host_good_20260722
- Standalone test script:
  /root/aic_host_good_test.sh
- Windows copy:
  C:\Users\26301\Desktop\F1C200S_归档_脚本和资料\work_dirs\aic8800_board_host_good_20260722\aic_host_good_test.sh

Board validation:
- Board state before test:
  MUSB DMA=N
  AIC a69c:8d80
  RTL8723 wlan0 online at 10.0.0.107
- Running /root/aic_host_good_test.sh loaded host-good ARM aic_load_fw.
- Firmware upload completed:
  fw_patch_table_8800d80_u02.bin
  fw_adid_8800d80_u02.bin
  fw_patch_8800d80_u02.bin
  fmacfw_8800d80_u02.bin
  fw download complete
  rd_version_val=00000000
  app_cmp
- Therefore the current host-good source compiles and performs stage1 firmware
  download on the F1C200S board.

Remaining failure:
- Immediately after app_cmp, AIC soft-disconnects and tries stage2 enumeration.
- Board log:
  usb 1-1.3: USB disconnect, device number 4
  usb 1-1.3: new high-speed USB device number 5 using musb-hdrc
  usb 1-1.3: musb descriptor timeout, extra port reset
  usb 1-1.3: device descriptor read/64, error -110
  usb 1-1.3: new high-speed USB device number 6 using musb-hdrc
  usb 1-1.3: musb descriptor timeout, extra port reset
- AIC disappears from lsusb afterward; RTL8723 remains online.
- aic_load_fw module was unloaded after the test to keep the board clean.

Conclusion:
- The current board blocker is not the host-good loader source or firmware
  file transmission. It is the post-app_cmp USB soft-disconnect / MUSB / hub /
  stage2 descriptor-read path.
- Keep MUSB DMA=N for AIC bootrom stage1.
- Next work should inspect/rework MUSB stage2 re-enumeration behavior instead
  of rotating more AIC loader variants.

Follow-up kernel test patch:
- Source file changed:
  /home/wnk/LicheePi_Nano/linux/drivers/usb/core/hub.c
- Backup before edit:
  /home/wnk/LicheePi_Nano/backups/hub.c.before_remove_musb_extra_reset_20260722
- Change:
  removed the custom MUSB-only branch that printed:
  "musb descriptor timeout, extra port reset"
  and forced an additional hub_port_reset() after the first high-speed
  descriptor read timeout.
- Rationale:
  The extra reset did not recover AIC8800D80 after app_cmp and changed the
  normal USB core retry sequence. Restore standard descriptor retry behavior
  before trying deeper MUSB changes.
- Build command:
  cd /home/wnk/LicheePi_Nano/linux
  export PATH=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin:$PATH
  make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- -j8
- Build result:
  arch/arm/boot/zImage md5 9136ce4d1aaee7aa16832687e58bc43f
- Board deployment:
  new zImage copied to /tmp/bootcheck/zImage
  previous boot zImage backed up as:
  /tmp/bootcheck/zImage.before_remove_musb_extra_reset_19700101_001759
- DTB was not replaced for this test.
- Board was not rebooted automatically. User must reboot before testing this
  kernel.
- After reboot, retest with:
  /root/aic_host_good_test.sh

Post-reboot validation of remove-extra-reset kernel:
- Booted kernel:
  Linux buildroot 5.7.1 #128 Wed Jul 22 08:07:31 PDT 2026
- MUSB DMA remained N.
- AIC host-good ARM loader still reached:
  fw download complete
  app_cmp
- The removed message no longer appeared:
  "musb descriptor timeout, extra port reset"
- Stage2 still failed:
  usb 1-1.3: new high-speed USB device number 5 using musb-hdrc
  usb 1-1.3: device descriptor read/64, error -110
- Conclusion:
  removing the extra reset cleaned up the trace but did not fix AIC stage2.

Runtime USB core A/B tests:
- old_scheme_first=Y plus initial_descriptor_timeout=20000:
  loading aic_load_fw caused SSH drop and serial showed a fresh boot log with
  no /tmp AIC test log. Treat this as worse/unsafe for now; do not keep using
  old_scheme_first for AIC.
- old_scheme_first=N plus initial_descriptor_timeout=20000:
  app_cmp succeeded, then stage2 waited about 20 seconds and still failed with
  device descriptor read/64, error -110.
- Conclusion:
  stage2 is not merely missing the default 5 second descriptor timeout.

Next diagnostic kernel:
- Added temporary MUSB descriptor failure logging in:
  /home/wnk/LicheePi_Nano/linux/drivers/usb/core/hub.c
- Backup:
  /home/wnk/LicheePi_Nano/backups/hub.c.before_musb_desc_status_log_insert_20260722
- Added log string:
  "musb descriptor fail retry=%d op=%d r=%d speed=%d timeout=%d port_ret=%d status=%04x change=%04x"
- Built zImage md5:
  d000b68b14b5ee40a5f3c442186c470d
- Deployed to /tmp/bootcheck/zImage.
- Previous boot zImage backed up:
  /tmp/bootcheck/zImage.before_musb_desc_status_log_19700101_000648
- Board was not rebooted automatically. After user reboot, run:
  /root/aic_host_good_test.sh
  and inspect the new "musb descriptor fail" status lines.

Descriptor status diagnostic result:
- Booted diagnostic kernel:
  Linux buildroot 5.7.1 #129 Wed Jul 22 08:54:24 PDT 2026
- AIC stage2 descriptor failure status lines:
  musb descriptor fail retry=0 op=0 r=-110 speed=3 timeout=5000 port_ret=0 status=0503 change=0000
  musb descriptor fail retry=1 op=3 r=-110 speed=3 timeout=5000 port_ret=0 status=0503 change=0000
  musb descriptor fail retry=0 op=0 r=-110 speed=3 timeout=5000 port_ret=0 status=0503 change=0000
- Interpretation:
  portstatus 0x0503 = connected + enabled + high-speed, change=0.
  The hub/MUSB port is stable after app_cmp. The failure is default-address
  EP0 control transfer timeout, not a disconnected or disabled port.

Next EP0 diagnostic kernel:
- Added MUSB host-side logging in:
  /home/wnk/LicheePi_Nano/linux/drivers/usb/musb/musb_host.c
- Backup before edit:
  /home/wnk/LicheePi_Nano/backups/musb_host.c.before_ep0_aic_log_20260723
- Enabled the previously empty musb_aic_log_qh() hook to print:
  aic-musb <tag> ctrl devnum=... addr=... hub=... port=... ep=... csr=... setup=...
- Added EP0 IRQ logging for default-address control URBs:
  aic-musb ep0_irq csr=... count=... stage=... actual=... status=... setup=...
- Built zImage md5:
  b539576df22f1cc4e64fdcdb4131f827
- Deployed to:
  /tmp/bootcheck/zImage
- Boot partition cleanup:
  boot partition was full because multiple zImage.before_* backups were stored
  there. Moved old zImage.before_* files to:
  /root/boot_zimage_backups_20260723
- Current pre-EP0-diagnostic zImage backup:
  /root/boot_zimage_backups_20260723/zImage.before_musb_ep0_aic_log_19700101_001012
- Board was not rebooted automatically. After user reboot, run:
  /root/aic_host_good_test.sh
  and inspect "aic-musb ... ctrl" and "aic-musb ep0_irq" lines.

EP0 verbose diagnostic result:
- Booted kernel:
  Linux buildroot 5.7.1 #130 Wed Jul 22 09:05:52 PDT 2026
- Running /root/aic_host_good_test.sh caused SSH drop and the board rebooted.
- /tmp/aic_host_good_test.log was empty after reboot.
- The EP0 logging was too broad: it printed default-address control transfers
  for normal boot enumeration of the hub, RTL8723, and AIC bootrom. Do not keep
  this verbose EP0 kernel as the test baseline.
- Boot partition was restored away from b539576df22f1cc4e64fdcdb4131f827.
- Bad verbose kernel saved only as:
  /root/boot_zimage_backups_20260723/zImage.bad_ep0_verbose_19700101_001012

Next pre-descriptor settle kernel:
- Restored musb_host.c from:
  /home/wnk/LicheePi_Nano/backups/musb_host.c.before_ep0_aic_log_20260723
- Kept the hub.c descriptor-failure status logging.
- Added a narrow MUSB high-speed pre-descriptor wait in hub.c after the:
  "new high-speed USB device number ... using musb-hdrc"
  message and before the first descriptor-read sequence:
  "musb pre-descriptor settle 5000 ms"
- Backup before edit:
  /home/wnk/LicheePi_Nano/backups/hub.c.before_musb_pre_descriptor_delay_byline_20260723
- Built zImage md5:
  c4c2b89eedc965f0536d8e064509fb0d
- Deployed to:
  /tmp/bootcheck/zImage
- Previous stable diagnostic zImage backed up to:
  /root/boot_zimage_backups_20260723/zImage.before_musb_predesc_5000_19700101_000413
- Board was not rebooted automatically. After user reboot, run:
  /root/aic_host_good_test.sh
  and check whether the extra 5 second settle lets AIC stage2 answer EP0.
```

## RTL8723BU Host AP Baseline - 2026-07-20
```text
Scope:
- User moved the RTL8723BU USB WiFi card to the Ubuntu host first.
- Goal was to prove AP mode on the host before porting AP/Miracast work back
  to the F1C200S board.
- Do not use AIC8800 for this path.

Host:
- IP: 192.168.175.138
- User: wnk
- Password: 1
- USB device:
  0bda:b720 Realtek Semiconductor Corp.
- Interface:
  wlx001f058056fd
- MAC:
  00:1f:05:80:56:fd

Important result:
- The in-kernel Ubuntu driver `rtl8xxxu` only reports:
  managed, monitor
- It does not report AP mode, so it cannot be used for AP/Miracast validation.

Working host source and module:
- Source tree:
  ~/LicheePi_Nano/third_party/rtl8723bu_lwfinger_host_ap_20260719_181717
- Base:
  ~/LicheePi_Nano/third_party/rtl8723bu
  remote https://github.com/lwfinger/rtl8723bu.git
- Host-only Makefile change in the copied tree:
  CONFIG_PLATFORM_I386_PC = y
  CONFIG_PLATFORM_FS_MX61 = n
- Built successfully against Ubuntu kernel:
  4.15.0-142-generic
- Loaded with:
  modprobe cfg80211
  insmod ./8723bu.ko rtw_power_mgnt=0 rtw_enusbss=0

Working AP capability:
- With the vendor/lwfinger `8723bu` module loaded, `iw phy phy0 info` reports:
  IBSS, managed, AP, monitor, P2P-client, P2P-GO
- Private ioctls are also present via `iwpriv`, but host AP was proven through
  nl80211, not Realtek `rtl871xdrv`.

Host AP test scripts:
- Start:
  ~/LicheePi_Nano/third_party/rtl8723bu_lwfinger_host_ap_20260719_181717/start_8723bu_host_ap.sh
- Stop:
  ~/LicheePi_Nano/third_party/rtl8723bu_lwfinger_host_ap_20260719_181717/stop_8723bu_host_ap.sh
- Local Windows copies:
  C:\Users\26301\Desktop\_rtl8723_work\start_8723bu_host_ap.sh
  C:\Users\26301\Desktop\_rtl8723_work\stop_8723bu_host_ap.sh

Host AP parameters:
- hostapd:
  driver=nl80211
  ssid=RTL8723BU_AP
  channel=6
  ieee80211n=1
  WPA2-PSK/CCMP
- Password:
  12345678
- AP IP:
  192.168.88.1/24
- DHCP:
  dnsmasq range 192.168.88.20-192.168.88.80
- NAT:
  192.168.88.0/24 -> ens33 MASQUERADE

Confirmed success:
- hostapd log:
  AP-ENABLED
- `iw dev`:
  interface type AP, ssid RTL8723BU_AP
- Phone connected:
  iQOO-Neo10
  MAC 4e:23:17:82:fe:4e
  DHCP IP 192.168.88.68
- Host ping to phone:
  3/3 replies, 0% packet loss

Practical conclusion:
- The known-good RTL8723BU AP path is:
  vendor/lwfinger 8723bu + cfg80211/nl80211 + hostapd
- For board-side AP/Miracast work, use this as the reference path first.
- Avoid basing AP on Ubuntu `rtl8xxxu`; it lacks AP mode for this card.
```

## RTL8723BU Board AP Working Baseline - 2026-07-20
```text
Scope:
- Deploy the host-confirmed RTL8723BU AP path onto the F1C200S board.
- Use this for board-hosted AP / later Miracast experiments.

Source-of-truth scripts:
- Windows:
  C:\Users\26301\Desktop\F1C200S_归档_脚本和资料\host_scripts\rtl8723bu_ap_board_deploy\start_8723_host_ap.sh
  C:\Users\26301\Desktop\F1C200S_归档_脚本和资料\host_scripts\rtl8723bu_ap_board_deploy\stop_8723_host_ap.sh
- Host runtime bundle:
  ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/root/start_8723_host_ap.sh
  ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/root/stop_8723_host_ap.sh
- Board deployed:
  /root/start_8723_host_ap.sh
  /root/stop_8723_host_ap.sh

Deployment backup:
- Existing board scripts were backed up to:
  /root/backup_8723_ap_20260720_072602

Working board AP settings:
- hostapd driver: nl80211
- module: /lib/modules/5.7.1/extra/8723bu.ko
- SSID: RTL8723BU_AP
- password: 12345678
- channel: 6
- AP IP: 192.168.88.1/24
- DHCP server: tiny_dhcpd_8723
- DHCP range: 192.168.88.20-192.168.88.80

Validation:
- Started from serial/SSH with:
  DURATION=300 /root/start_8723_host_ap.sh start
- hostapd log:
  wlan0: AP-ENABLED
- Board interface:
  wlan0 192.168.88.1/24 UP RUNNING
- Phone connected successfully.
- DHCP requests observed:
  6c:40:e8:1d:07:41 -> 192.168.88.50
  4e:23:17:82:fe:4e -> 192.168.88.70
- Board ping to 192.168.88.70:
  2/2 replies, 0% packet loss

Operational notes:
- Normal STA SSH at 10.0.0.107 drops when AP mode starts because the same
  8723BU interface is switched from client mode to AP mode.
- Use serial COM3 for control while AP mode is active, or connect to the AP
  and access the board at 192.168.88.1.
- Default script has a 300 second restore watchdog for safety.
- For persistent AP mode, start with:
  DURATION=0 /root/start_8723_host_ap.sh start
- Restore normal WiFi with:
  /root/stop_8723_host_ap.sh
```

## RTL8723BU Board P2P-GO Working Baseline - 2026-07-20
```text
Scope:
- Reproduce the host-side RTL8723BU autonomous P2P-GO path on the F1C200S
  board using the board's ARM wpa_supplicant 2.6.
- This is the current board Wi-Fi Direct / P2P base for later Miracast work.

Source-of-truth scripts:
- Windows:
  C:\Users\26301\Desktop\F1C200S_归档_脚本和资料\host_scripts\rtl8723bu_ap_board_deploy\start_8723_p2p_go.sh
  C:\Users\26301\Desktop\F1C200S_归档_脚本和资料\host_scripts\rtl8723bu_ap_board_deploy\stop_8723_p2p_go.sh
- Host runtime bundle:
  ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/root/start_8723_p2p_go.sh
  ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/root/stop_8723_p2p_go.sh
- Board deployed:
  /root/start_8723_p2p_go.sh
  /root/stop_8723_p2p_go.sh

Board deployed md5:
- /root/start_8723_p2p_go.sh:
  96d2169b9ca485fd7b140ae7eb8bde15
- /root/stop_8723_p2p_go.sh:
  747e5ff3856c3e01c82bea78269ced3c

Working P2P-GO settings:
- driver: nl80211
- module: /lib/modules/5.7.1/extra/8723bu.ko
- wpa_supplicant: /usr/sbin/wpa_supplicant v2.6
- device_name: F1C200S-SINK
- channel/freq: channel 6 / 2437 MHz
- P2P-GO IP: 192.168.88.1/24
- DHCP server: tiny_dhcpd_8723
- DHCP range: 192.168.88.20-192.168.88.80

Validation:
- Started from serial with:
  /root/start_8723_p2p_go.sh start
- wpa_cli status:
  mode=P2P GO
  ssid=DIRECT-pz
  freq=2437
  wpa_state=COMPLETED
  ip_address=192.168.88.1
  p2p_device_address=14:0a:02:2f:dc:7a
- P2P-GO passphrase:
  aR7VJiqV
- Phone connected successfully:
  MAC 32:fd:eb:01:71:61
  IP 192.168.88.37
- Board ping to phone:
  4/4 replies, 0% packet loss

Operational notes:
- While P2P-GO is active, normal STA SSH at 10.0.0.107 is unavailable.
- Use serial COM3 for control, or connect to the P2P-GO network and access
  the board at 192.168.88.1.
- Stop P2P-GO with:
  /root/stop_8723_p2p_go.sh
- Restore normal STA WiFi with:
  /root/stop_8723_host_ap.sh
  or the normal /etc/init.d/S17rtl8723bu + /etc/init.d/S45usb-wifi path.
```

## RTL8723BU Host P2P-GO / WFD Baseline - 2026-07-20
```text
Scope:
- Host-side RTL8723BU Miracast/Wi-Fi Direct experiments.
- A patched wpa_supplicant 2.4 build is used to force the legacy
  single-interface P2P path:
  info->p2p_concurrent = 0;
- This follows the known RTL8723BU workaround where the driver advertises
  concurrent P2P, but the separate P2P group interface is unreliable.

Host source:
- wpa_supplicant:
  ~/LicheePi_Nano/third_party/wpa_supplicant_2_4_p2p_no_concurrent_20260720
- RTL8723BU module:
  ~/LicheePi_Nano/third_party/rtl8723bu_lwfinger_host_ap_20260719_181717/8723bu.ko

Working GO script:
- Local:
  C:\Users\26301\Desktop\_rtl8723_work\start_8723bu_wpa24_noconcurrent_go_host.sh
- Host:
  ~/LicheePi_Nano/third_party/wpa_supplicant_2_4_p2p_no_concurrent_20260720/wpa_supplicant/start_8723bu_wpa24_noconcurrent_go_host.sh

Confirmed result:
- Script starts a single-interface P2P-GO on:
  wlx001f058056fd
- Interface type:
  P2P-GO
- Phone sees the generated `DIRECT-*` SSID in the ordinary Wi-Fi list.
- User connected phone successfully to:
  SSID: DIRECT-Mm
  Passphrase: QFHL6Xm5
- wpa_supplicant log showed:
  AP-STA-CONNECTED 86:57:a0:43:f0:2d
  pairwise key handshake completed (RSN)
- Host-side IP/DHCP setup used:
  ip addr add 192.168.88.1/24 dev wlx001f058056fd
  dnsmasq --port=0 --conf-file=/tmp/dnsmasq_8723bu_go.conf ...
- DHCP lease / ARP result:
  phone IP: 192.168.88.68
  current associated MAC: 86:57:a0:43:f0:2d
- Host ping to phone passed:
  3/3 replies, 0% packet loss
- The GO startup script was then updated so future starts automatically set:
  * GO interface IP: 192.168.88.1/24
  * DHCP range: 192.168.88.20-192.168.88.80
  * DNS on 192.168.88.1
  * NAT from 192.168.88.0/24 to ens33

Important negative result:
- A pure `p2p_listen`/WFD-listen-only script did not make the device visible
  to the phone's Wi-Fi Direct/Miracast discovery, while autonomous GO was
  visible in the ordinary Wi-Fi list.
- Therefore the current proven layer is:
  RTL8723BU can operate as an autonomous P2P-GO with WFD/P2P IEs and normal
  WPA2/IP connectivity.
- The remaining Miracast work is above this layer:
  discovery/session negotiation and RTSP/WFD sink integration.
- Packet capture while the phone was connected to the autonomous GO showed
  ordinary IP traffic plus `_googlecast._tcp.local` mDNS and multicast probes,
  but no WFD RTSP connection on TCP 7236 and no P2P/WFD negotiation toward the
  sink. This means the tested phone casting entry was using GoogleCast/vendor
  discovery behavior at that moment, not an active Miracast/Wi-Fi Display
  source session.
- Host-only MiracleCast patch added for diagnostics:
  `miracle-sinkctl direct <ip>`
  This skips miracle-wifid/DBus and calls the existing RTSP sink connector
  directly against `<ip>:7236`.
- Direct test against the connected phone:
  `miracle-sinkctl direct 192.168.88.68`
  Result:
  `Connection refused`
  so the phone was not listening as a WFD RTSP source while merely connected
  to the ordinary `DIRECT-*` Wi-Fi network.

WLAN Direct / real P2P discovery update:
- When the phone was placed in its WLAN Direct page, host-side P2P discovery
  started working.
- The phone appeared as:
  P2P MAC: 6e:40:e8:1e:07:41
  Name: WNK
  Primary device type: 10-0050F204-5
  WFD subelements: 00000600101c440032
- Host/sink advertised name for this test:
  F1C200S-SINK
- A host-initiated PBC connection reached GO Negotiation:
  p2p_connect 6e:40:e8:1e:07:41 pbc go_intent=15
- Initial failure was:
  P2P-GO-NEG-FAILURE status=7
  Logs showed:
  `No common channels found`
  because phone WNK proposed 5 GHz operating channel:
  regulatory class 124, channel 161
  while RTL8723BU is 2.4 GHz only.
- Host wpa_supplicant patch added:
  src/p2p/p2p_go_neg.c
  When local go_intent=15, do not fail on empty peer channel intersection;
  keep local 2.4 GHz GO channel for RTL8723BU.
- Patch markers:
  `No common channels found; keep local 2.4 GHz GO channel for RTL8723BU`
  `No common channels found; force local 2.4 GHz GO channel for RTL8723BU`
- Rebuilt host wpa_supplicant successfully after the patch.
- Immediate retest after rebuilding did not rediscover the phone before
  p2p_connect, so it failed with:
  `Cannot connect to unknown P2P Device 6e:40:e8:1e:07:41`
This means the patched channel path still needs validation while the phone
is actively visible in WLAN Direct discovery.
```

## AIC8800 Host Miracast C Dump - 2026-07-22
```text
Scope:
- One-to-one C port of lazycast d2.py RTSP/WFD sink path for host validation.
- No board deployment yet.
- Goal is H.264 stream extraction, not playback.

GitHub repo/branch:
- repo: https://github.com/Wnjbk/f1c200s-casting
- branch: host-aic8800-miracast
- main remains placeholder because this chain is host-validated only.

Source files:
- software/lazycast_host_20260721/miracast_sink_dump.c
- scripts/aic8800_lazycast_host/start_aic8800_miracast_cdump_host.sh

Host paths:
- /home/wnk/LicheePi_Nano/third_party/lazycast_host_20260721/miracast_sink_dump.c
- /home/wnk/LicheePi_Nano/third_party/lazycast_host_20260721/miracast_sink_dump
- /home/wnk/F1C200S_host_archive/host_scripts/aic8800_lazycast_host/start_aic8800_miracast_cdump_host.sh

Build command on Ubuntu host:
cd /home/wnk/LicheePi_Nano/third_party/lazycast_host_20260721
gcc -O2 -Wall -Wextra -o miracast_sink_dump miracast_sink_dump.c

Host start command:
echo 1 | sudo -S /home/wnk/F1C200S_host_archive/host_scripts/aic8800_lazycast_host/start_aic8800_miracast_cdump_host.sh start

Working host parameters:
- AIC8800D80 stage2: a69c:8d81
- interface: aic0
- display name: F1C200S-AIC
- WPS Primary Device Type: 8-0050F204-5
- P2P-GO freq: 5805 MHz / channel 161
- GO IP: 192.168.49.1/24
- passphrase: 12345678
- phone lease during validation:
  6e:40:e8:1e:07:41 -> 192.168.49.48 iQOO-Neo10

Validation result:
- Pure C test confirmed; no python d2.py process running.
- RTSP reached:
  ---- Negotiation successful ----
- Output file:
  /tmp/aic8800_miracast_cdump/miracast.h264
- Output grew from 228K to 1.2M during a short live test.
- File begins with H.264 Annex-B start codes and SPS/PPS/IDR:
  00 00 00 01 09 10 00 00 00 01 67 ...

Important C fix:
- TCP RTSP messages can be coalesced.
- The first pure C attempt failed because the M2 response and M3 GET_PARAMETER
  arrived in one recv(), so the C port consumed M3 as part of M2 and then saw
  peer closed.
- miracast_sink_dump.c now frames RTSP messages by header terminator plus
  Content-Length before processing M1-M7.
- Do not close the session merely because RTP is idle. A static/low-motion
  Miracast screen can pause RTP for tens of seconds while RTSP and the Wi-Fi
  station remain connected. A previous 15 second RTP-idle reconnect watchdog
  caused false self-disconnects and was removed.

Video-format update:
- Lazycast default advertises broad capability:
  0001FEFF 3FFFFFFF 00000FFF
  The phone selected 00002000, which produced 1080p50 output and was too heavy.
- The C dump path now advertises a conservative low mode:
  wfd_video_formats: 00 00 02 04 00000007 00000000 00000000 00 0000 0000 00 none none
- Phone validation after the change selected:
  00000002
- Result after removing RTP-idle exit:
  RTSP stayed ESTAB past 90 seconds; station remained associated; C process
  remained running. Observed RTP stats before static idle:
  3902 packets, 3624464 bytes, output file 372K.

Important script fix:
- stop_runtime also kills the old lazycast d2_watch.sh.
- cdump watcher no longer trusts stale DHCP leases blindly; it prefers a live
  neighbor entry and only uses the lease after a successful ping.

Important discovery update - 2026-07-22:
- The old unstable phone became stable after changing the AIC GO WPS Primary
  Device Type from `7-0050F204-1` to `8-0050F204-5`.
- Keep `device_name=F1C200S-AIC` and `device_type=8-0050F204-5` in the host
  start script unless a future A/B test proves a better value.
- Runtime config confirmed:
  `/tmp/aic_wpa.conf: device_type=8-0050F204-5`
  host script line:
  `/home/wnk/F1C200S_host_archive/host_scripts/aic8800_lazycast_host/start_aic8800_miracast_cdump_host.sh`
- Current WFD subelements remained unchanged during the successful test:
  `wfd_subelem_set 0 000600111c44012c`
  `wfd_subelem_set 1 0006000000000000`
  `wfd_subelem_set 6 000700000000000000`
- Evidence from the stable run:
  RTSP reached `---- Negotiation successful ----`.
  The phone sent periodic `GET_PARAMETER` requests after PLAY, with CSeq
  incrementing at least from 4 through 10, and the sink replied `200 OK`.
  H.264 output kept growing from about 61.5 MB to 62.7 MB during observation.
- Earlier `7-0050F204-1` runs could reach PLAY but old-phone RTP stalled, for
  example at `packets=2992 bytes=3028676`, followed by repeated
  `waiting for RTP packets...`.

Next steps:
- Convert the output side from file dump to stdout/pipe mode for board Cedar
  playback or recording.
- Cross-compile the C program only after host behavior remains repeatable.
- Do not deploy to F1C200S board until the host chain has been retested after
  stop/start and phone reconnect.
```

## AIC8800 Board MUSB Recovery - 2026-07-23
```text
After resume/reboot, board was running:
  Linux buildroot 5.7.1 #131 Wed Jul 22 09:17:40 PDT 2026
  /tmp/bootcheck/zImage md5 c4c2b89eedc965f0536d8e064509fb0d

Observed:
- AIC bootrom enumerated as a69c:8d80.
- RTL8723BU STA network was up at 10.0.0.107.
- MUSB DMA remained N.
- dmesg still printed broad EP0 logs:
  "aic-musb start_urb ctrl ..."
  "aic-musb ep0_irq ..."
- Therefore #131 is not a safe AIC test baseline, even though normal boot can
  complete.

Recovery performed:
- Board boot partition zImage was restored to the stable descriptor-status
  diagnostic kernel:
  /root/boot_zimage_backups_20260723/zImage.before_musb_predesc_5000_19700101_000413
  md5 d000b68b14b5ee40a5f3c442186c470d
- This only affects the next boot; the currently running kernel remains #131
  until the user reboots.
- Host kernel source was also restored away from the pre-descriptor 5000 ms
  delay. Current source check should show:
  no "musb pre-descriptor"
  no "aic-musb ep0_irq"
  no "aic-musb start_urb ctrl"
  keep "musb descriptor fail retry=..."

Next AIC test rule:
- Do not run /root/aic_host_good_test.sh while uname reports #131.
- After user reboot, confirm the running zImage is the stable diagnostic build
  before testing:
    uname -a
    md5sum /tmp/bootcheck/zImage
    dmesg | grep 'musb descriptor fail\|aic-musb ep0_irq\|musb pre-descriptor'
```

## AIC8800 UGREEN V1.4 8d83 Board STA Success - 2026-07-23
```text
Scope:
- Use the UGREEN CM762 V1.4 / 2025_0605_3d4cc869 WiFi-only path on the
  F1C200S board.
- Target stage2 PID: a69c:8d83.
- Goal for this pass: firmware download + fdrv init + basic STA networking.

Host source:
- Base UGREEN source:
  /home/wnk/LicheePi_Nano/third_party/ugreen_cm762_v14_20260723
- Board build tree with fixes:
  /home/wnk/LicheePi_Nano/third_party/ugreen_cm762_v14_board_5p7_noprealloc_20260723
- Windows patch scratch:
  C:\Users\26301\Desktop\F1C200S_归档_脚本和资料\work_dirs\aic8800_ugreen_board_patch_20260723

Build rule used:
- Kernel toolchain rule preserved:
  export PATH=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
  make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- KDIR=/home/wnk/LicheePi_Nano/linux \
    CONFIG_PLATFORM_UBUNTU=n CONFIG_PLATFORM_ALLWINNER=n CONFIG_PLATFORM_ROCKCHIP=n \
    CONFIG_PLATFORM_AMLOGIC=n CONFIG_PLATFORM_HI=n CONFIG_PREALLOC_RX_SKB=n \
    CONFIG_PREALLOC_TXQ=n -j4

Important fixes:
- UGREEN top Makefile default `CONFIG_PREALLOC_RX_SKB=y` is not suitable for
  the 64MB F1C200S board. It caused order-3 allocation OOM in
  `aicwf_prealloc_init` before firmware download.
- Rebuilt with:
  CONFIG_PREALLOC_RX_SKB=n
  CONFIG_PREALLOC_TXQ=n
- Fixed a loader RX skb leak in `aic_load_fw/aic_txrxif.c`:
  `aicwf_process_rxframes()` split multiple packets out of one USB aggregate,
  but only freed the last `skb_inblock`. During firmware upload this leaked
  many ACK/config skbs and drove `kmalloc-1k` to about 17MB, then caused
  OOM, command queue overflow, and panic.
- Added `cmd_dump(NULL)` guard in `aic_load_fw/aicbluetooth_cmds.c` to avoid
  a panic on the error path after command queue failure.

Board deployed files:
- Backup before deployment:
  /root/backup_aic_ugreen8d83_20260723_041949
- /lib/modules/5.7.1/extra/aic_load_fw.ko
  md5: 2c2a1b80f8366a58ca36ad16d536f487
- /lib/modules/5.7.1/extra/aic8800_fdrv.ko
  md5: 2ad8ea8ac492c40942368f345fce3127
- /lib/firmware/aic8800D80/fmacfw_8800d80_u02.bin
  md5: 01acfbebdfb15755e3fe853e7bc95c7d
- Added symlink for fdrv userconfig lookup:
  /lib/firmware/aic8800D80/aic8800D80 -> .

Validation on board:
- Board kernel:
  Linux buildroot 5.7.1 #129 Wed Jul 22 08:54:24 PDT 2026
- Initial USB:
  a69c:8d80
- Load firmware:
  insmod /lib/modules/5.7.1/extra/aic_load_fw.ko \
    aic_fw_path=/lib/firmware/aic8800D80 aicwf_dbg_level=3
- Evidence:
  chip_id=7, chip_mcu_id=0
  file md5:01acfbebdfb15755e3fe853e7bc95c7d
  fw download complete
  app_cmp
  USB disconnect/re-enumeration
  lsusb then shows a69c:8d83
- Load fdrv:
  insmod /lib/modules/5.7.1/extra/aic8800_fdrv.ko aicwf_dbg_level=3
- Evidence:
  Firmware Version: Mar 23 2024 19:29:24 - ge9bde4e
  is 5g support = 1
  HT supp 1, VHT supp 1, HE supp 1
  New interface create wlan1
- Scan:
  wlan1 scanned both 2.4GHz and 5GHz networks.
  Target `wnk641_2.4G` found at 2412 MHz, signal about -33 dBm.
- STA association:
  wpa_supplicant on wlan1 associated with 50:4f:3b:cc:67:52.
  WPA key negotiation completed.
  iwconfig showed ESSID "wnk641_2.4G", AP 50:4F:3B:CC:67:52, 2412 MHz.
- IP test:
  ifconfig wlan1 10.0.0.188 netmask 255.255.255.0 up
  ping -I wlan1 -c 3 -W 2 10.0.0.1
  Result: 3/3 replies, 0% packet loss.

Known-good archive:
- Directory:
  /home/wnk/F1C200S_host_archive/known_good/aic8800_ugreen8d83_board_sta_20260723
- Tar:
  /home/wnk/F1C200S_host_archive/known_good/aic8800_ugreen8d83_board_sta_20260723.tar
- Tar sha256:
  1b1cb6c016f2d0f17d960c0fee4b7ab7cff5f11b012d12f5d00a4ed3c24ad740
- Tar checksum file:
  /home/wnk/F1C200S_host_archive/known_good/aic8800_ugreen8d83_board_sta_20260723.tar.sha256
- Archive includes:
  README.md, MANIFEST.txt, MANIFEST.sha256, full source_tree copy,
  artifacts/aic_load_fw.ko, artifacts/aic8800_fdrv.ko,
  artifacts/firmware_aic8800D80/, artifacts/test_aic_sta.sh,
  changed_files/aic_txrxif.c, changed_files/aicbluetooth_cmds.c,
  and build logs.
- README intentionally points to the external .tar.sha256 file instead of
  embedding the tar hash, because embedding a tar hash inside a file that is
  itself inside the tar creates a circular checksum.

Current status:
- AIC8800D80 UGREEN WiFi-only 8d83 path is now proven on the F1C200S board
  for firmware download, driver init, scan, WPA STA association, and gateway
  ping.
- This is not yet a boot init integration. Do not add it to startup until the
  load/unload and coexistence strategy with 8723BU is defined.
```

## U-Boot ST7701 SPI Pin Mapping After PCB Fix - 2026-07-25
```text
Scope:
- Hardware wiring was corrected, so the U-Boot ST7701 3-wire SPI panel init
  must use the normal pin mapping instead of any software SCL/SDA swap.

Confirmed source:
- `/home/wnk/LicheePi_Nano/u-boot/board/sunxi/st7701-spi-init.c`
- Actual mapping:
  PA0 = CS
  PA1 = SCL
  PA2 = SDA / MOSI
  PA3 = RST
- Code constants after verification:
  `#define BIT_SCLK  1`
  `#define BIT_MOSI  2`
- Only the U-Boot source comment was updated to explicitly document
  "Pin mapping after the PCB wiring fix"; the functional bit mapping was
  already correct in the current source.

Backup:
- Host source backup before comment update:
  `/home/wnk/LicheePi_Nano/u-boot/board/sunxi/st7701-spi-init.c.bak_before_pcb_wiring_fix_20260725`
- Board raw boot-area backup before deployment:
  `/root/uboot_backups_20260725/mmcblk0_first1M_before_pcbfix.bin`
  md5: `08a6af8b9df433c759f26c719a2fd35d`

Build:
- Host command:
  `cd /home/wnk/LicheePi_Nano/u-boot`
  `export PATH=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin`
  `/usr/bin/make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- -j8`
- Build passed.

Artifact:
- `/home/wnk/LicheePi_Nano/u-boot/u-boot-sunxi-with-spl.bin`
  size: 536 KiB
  md5: `27853fda8ece5a97986a86ed7c7ca14a`

Deployment:
- Copied artifact to board:
  `/tmp/u-boot-sunxi-with-spl.pcbfix.bin`
- Wrote to whole-card U-Boot area, not a partition:
  `dd if=/tmp/u-boot-sunxi-with-spl.pcbfix.bin of=/dev/mmcblk0 bs=1024 seek=8 conv=fsync`
- Readback verification:
  `dd if=/dev/mmcblk0 of=/tmp/uboot_written_readback.bin bs=1024 skip=8 count=536`
  md5 matched artifact:
  `27853fda8ece5a97986a86ed7c7ca14a`
  `VERIFY_OK`

Next validation:
- Reboot or power-cycle the board.
- Check serial log for:
  `ST7701: init 360x640 RGB565 panel`
  `ST7701: init done`
- Confirm the panel initializes with the repaired hardware wiring.
```

## U-Boot ST7701 SCLK/MOSI Swapped Test Build - 2026-07-25
```text
Reason:
- User clarified that the repaired hardware still requires U-Boot to test the
  opposite bit mapping from the previous build.

Source change:
- File:
  `/home/wnk/LicheePi_Nano/u-boot/board/sunxi/st7701-spi-init.c`
- Backup before change:
  `/home/wnk/LicheePi_Nano/u-boot/board/sunxi/st7701-spi-init.c.bak_before_swap_sclk_mosi_20260725`
- New mapping:
  PA0 = CS
  PA1 = SDA / MOSI
  PA2 = SCL
  PA3 = RST
- Code constants:
  `#define BIT_SCLK  2`
  `#define BIT_MOSI  1`

Build:
- Command:
  `cd /home/wnk/LicheePi_Nano/u-boot`
  `export PATH=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin`
  `/usr/bin/make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- -j8`
- Build passed.

Artifact:
- `/home/wnk/LicheePi_Nano/u-boot/u-boot-sunxi-with-spl.bin`
  size: 536 KiB
  md5: `e2de6728c42364874dca86d070e4da41`

Board deployment:
- Board backup before writing swapped build:
  `/root/uboot_backups_20260725/mmcblk0_first1M_before_swap_sclk_mosi.bin`
  md5: `4b1b3665cc7aac04285a4f84558320d8`
- Board temp artifact:
  `/tmp/u-boot-sunxi-with-spl.swap_sclk_mosi.bin`
  md5: `e2de6728c42364874dca86d070e4da41`
- Write command:
  `dd if=/tmp/u-boot-sunxi-with-spl.swap_sclk_mosi.bin of=/dev/mmcblk0 bs=1024 seek=8 conv=fsync`
- Readback:
  `/tmp/uboot_readback_swap.bin`
  md5: `e2de6728c42364874dca86d070e4da41`
  `VERIFY_OK`

Next validation:
- Reboot or power-cycle board and check whether U-Boot panel init now works
  with the repaired hardware.

Validation update:
- User confirmed after reboot that the screen is normal with this swapped
  build. Treat this as the current working U-Boot ST7701 pin mapping.
```

## AIC8800 Board Miracast DHCP49 Runtime Fix - 2026-07-25
```text
Scope:
- Continue board-side AIC8800 Miracast after U-Boot display fix.
- Keep RTL8723 `wlan0` for SSH and use AIC `wlan1` for Miracast.

Problem found:
- `/root/aic_miracast/tiny_dhcpd_49` on the board was 0 bytes after reboot.
- This would let P2P-GO start but prevent the phone from receiving the correct
  Miracast subnet DHCP lease.

Fix:
- Recreated Miracast-only DHCP server by binary patching the known-working
  `/usr/sbin/tiny_dhcpd_8723` strings from `192.168.88` to `192.168.49`.
- Original `/usr/sbin/tiny_dhcpd_8723` was not modified, so RTL8723 AP/P2P
  scripts remain unaffected.

Host patched file:
- `/tmp/tiny_dhcpd_49_patched`
  md5: `2e8c7e6533eeb85095799092da06707f`
- Confirmed strings:
  `192.168.49.1`
  `192.168.49.20`
  `192.168.49.80`
  `tiny_dhcpd_8723 iface=%s server=192.168.49.1 pool=192.168.49.20-80`

Board deployed file:
- `/root/aic_miracast/tiny_dhcpd_49`
  size: 9696 bytes
  md5: `2e8c7e6533eeb85095799092da06707f`

Runtime validation:
- Started:
  `/root/aic_miracast/start_aic_miracast_cdump_board.sh start`
- AIC firmware path worked:
  `a69c:8d80` -> `a69c:8d83`
- Runtime processes:
  one `wpa24_aic_wfd_supplicant -i wlan1`
  one `/root/aic_miracast/tiny_dhcpd_49 wlan1`
  one `/tmp/aic_miracast/watch.sh`
  RTL8723 `wlan0` supplicant remained alive.
- P2P-GO:
  SSID: `DIRECT-0b`
  freq: 5805 MHz / channel 161
  IP: `192.168.49.1`
  passphrase: `12345678`
- DHCP log:
  `tiny_dhcpd_8723 iface=wlan1 server=192.168.49.1 pool=192.168.49.20-80`
- No `Register frame command failed ... Operation already in progress` was
  seen in this clean start.
```

## AIC8800 Board Miracast Success Snapshot - 2026-07-25
```text
Status:
- Board-side AIC8800 Miracast reached a working runtime state.
- Android source connected to F1C200S-AIC over AIC8800 `wlan1` P2P-GO.
- RTSP/WFD negotiation completed and an H.264 dump was produced on the board.
- Keep RTL8723 `wlan0` as SSH/control network. Do not merge AIC Miracast into
  boot init until reconnect/cleanup behavior is deliberately tested.

Board runtime:
- Kernel:
  Linux buildroot 5.7.1 #129 Wed Jul 22 08:54:24 PDT 2026
- USB:
  RTL8723BU remains present for wlan0 control.
  AIC8800 is stage2 `a69c:8d83`.
- AIC interface:
  `wlan1`
- P2P-GO:
  display name: `F1C200S-AIC`
  SSID during success run: `DIRECT-gM`
  frequency: 5805 MHz / channel 161
  GO IP: `192.168.49.1`
  passphrase: `12345678`
  device_type: `8-0050F204-5`
  WFD subelements:
    `wfd_subelem_set 0 000600111c44012c`
    `wfd_subelem_set 1 0006000000000000`
    `wfd_subelem_set 6 000700000000000000`

Successful phone/session evidence:
- Phone:
  `Redmi K60`
  MAC: `a6:00:33:22:26:9e`
  IP: `192.168.49.52`
- Board log evidence:
  `P2P-PROV-DISC-PBC-REQ ... name='Redmi K60'`
  `P2P: Add client ... (p2p=1 wfd=1 client_info=1)`
  `WPS: Negotiation completed successfully`
  `wlan1: WPS-SUCCESS`
  `wlan1: AP-STA-CONNECTED a6:00:33:22:26:9e`
  `DISCOVER ... ip=192.168.49.52`
  `REQUEST ... ip=192.168.49.52`
- RTSP/WFD evidence:
  `OPTIONS`
  `GET_PARAMETER`
  `SET_PARAMETER`
  `SETUP`
  `PLAY`
  `---- Negotiation successful ----`
  `GET_PARAMETER` heartbeat responses were present.
- RTP/H.264 evidence:
  Board output:
  `/tmp/aic_miracast/miracast_19700101_005555.h264`
  captured size in snapshot: about 15.3 MiB
  RTSP log observed:
  `packets=38512 bytes=40771788`

Important script behavior that made this work:
- Do not manually connect the phone to `DIRECT-*` as ordinary Wi-Fi for the
  Miracast test. That creates a non-WFD client (`p2p=0 wfd=0`) and prevents the
  proper WFD path.
- Use the phone wireless display entry, or keep the phone in wireless display
  discovery so it sends P2P/WFD frames.
- The watcher must not rely only on ARP. It parses DHCP log IPs such as
  `ip=192.168.49.52`.
- The watcher now starts `miracast_sink_dump` immediately once it has a client
  IP. Do not pre-check TCP 7236 with `nc`; that can miss the source's RTSP
  availability window.
- If cdump exits with a zero-sized output, the watcher does not immediately
  deauthenticate the phone. This avoids destroying a still-forming WFD session.
- Suppress AIC fdrv `rx eapol`, `rxfrag`, and `sub rx fail` spam. The working
  fdrv md5 is listed below.

Board deployed md5 from success snapshot:
- `/lib/modules/5.7.1/extra/aic_load_fw.ko`
  `81528a07455d757581c5414de2dc3ead`
- `/lib/modules/5.7.1/extra/aic8800_fdrv.ko`
  `6400244a523052102f9686a2d68b6db6`
- `/root/aic_miracast/start_aic_miracast_cdump_board.sh`
  `b8a4355f69cf377acc42c5effc7ef2d6`
- `/root/aic_miracast/miracast_sink_dump`
  `95babf6aafcef01987293a4e853f095a`
- `/root/aic_miracast/tiny_dhcpd_49`
  `2e8c7e6533eeb85095799092da06707f`
- `/root/aic_miracast/wpa24_aic_wfd_supplicant`
  `58e2ac9e14914b908ef23c682e61c38c`
- `/root/aic_miracast/wpa24_aic_wfd_cli`
  `97f7384a530c9514f75533ab9e530a56`

Archive:
- Board tar:
  `/root/roms/20260725_board_aic_miracast_success.tar`
- Board tar md5:
  `6c1ea1c9ba7746032498856d86e1889f`
- Host copy:
  `/home/wnk/F1C200S_host_archive/known_good/aic8800_board_miracast_success_20260725/20260725_board_aic_miracast_success.tar`
- Host extracted copy:
  `/home/wnk/F1C200S_host_archive/known_good/aic8800_board_miracast_success_20260725/extracted/20260725_board_aic_miracast_success`
- Archive contents:
  driver modules, firmware, wpa_supplicant/wpa_cli, Miracast startup script,
  tiny DHCP server, `miracast_sink_dump`, runtime wpa config, board logs,
  cdump logs, watcher logs, state snapshot, md5 manifest, and H.264 sample.

Known caveat:
- The archive state script ran with a reduced PATH, so `ifconfig` in
  `state.txt` reports not found. The important interface state is still
  captured through wpa status, ARP, process list, logs, and file manifests.

Reconnect/watchdog update - 2026-07-25:
- A post-success auto-disconnect was checked after a working H.264 dump.
- `/tmp` was not full at diagnosis:
  `/tmp` 26.4M total, 16.3M used, 10.1M available, 62%.
- Existing H.264 dump in `/tmp/aic_miracast` was about 15.3M, so `/tmp` is a
  real risk for longer captures but was not the direct failure cause in this
  run.
- `miracast_sink_dump` remained alive, but `iw dev wlan1 station dump` was
  empty and RTP counters were stalled:
  `waiting for RTP packets... packets=38512 bytes=40771788 idle=...`
- Kernel log then showed AIC-side trouble:
  `aicwf_process_rxframes pkt_len:... skb->len:...`,
  `MM_GET_STA_INFO_REQ` timeout, and `cmd queue crashed`.
- Conclusion:
  the phone/source had left or stopped RTP, while the watcher did not detect
  the missing station and left cdump running against stale state. It was not a
  simple ENOSPC/no-space write failure.
- Script update deployed to board:
  `/root/aic_miracast/start_aic_miracast_cdump_board.sh`
  md5 `d744022f1ca68610e702752f91cb3b83`
- Backup before deployment:
  `/root/aic_miracast/backups/start_aic_miracast_cdump_board.sh.before_outdir_watch_20260725_1905`
- Important script changes:
  H.264 output now defaults to `/root/roms/aic_miracast`, not `/tmp`.
  `/tmp/aic_miracast` is kept for logs, pid files, and symlink only.
  old `/tmp/aic_miracast/*.h264` files are removed at startup.
  watcher monitors cdump while it is running; if wlan1 station disappears and
  output stops growing, it kills cdump, flushes stale neighbor state, and
  re-arms `wps_pbc`.
  stop logic no longer kills `tiny_dhcpd_8723`, so RTL8723-related fallback/AP
  helpers are not touched by AIC Miracast stop.
- After this change, the stale AIC fdrv had to be recovered without resetting
  MUSB and without touching RTL8723:
  `rmmod aic8800_fdrv; insmod /lib/modules/5.7.1/extra/aic8800_fdrv.ko aicwf_dbg_level=3`
- Restart validation after fdrv reload:
  P2P-GO ready as `DIRECT-54`, freq `5805`, IP `192.168.49.1`,
  passphrase `12345678`, display `F1C200S-AIC`.
- Follow-up cause check:
  the RTSP dump from the working session showed phone `GET_PARAMETER`
  heartbeats did receive `RTSP/1.0 200 OK` replies, so the observed disconnect
  was not simply "phone heartbeat unhandled".
  A separate bug was found in the watcher: while idle it refreshed
  `wps_pbc any` every ~20 seconds. Each refresh rebuilt WPS/P2P beacon IEs and
  called nl80211 set beacon. On AIC board this later produced:
  `nl80211: Beacon set failed: -32 (Broken pipe)`.
  Repeated beacon updates are therefore unsafe on this driver.
- Second script update deployed:
  `/root/aic_miracast/start_aic_miracast_cdump_board.sh`
  md5 `872682feeff43fa01fef2f3ae4f6b4da`
- Backup before second update:
  `/root/aic_miracast/backups/start_aic_miracast_cdump_board.sh.before_no_periodic_wps_20260725_1918`
- Change:
  remove periodic idle `wps_pbc any`; keep initial startup `wps_pbc` and
  session-end re-arm only.
- After reloading only `aic8800_fdrv`, validation showed:
  P2P-GO ready as `DIRECT-1r`, freq `5805`, IP `192.168.49.1`,
  passphrase `12345678`, display `F1C200S-AIC`.
  After an idle wait, `watch.log` contained only the initial watch start and no
  repeated `refresh wps_pbc` lines.
- After board reboot, AIC is not auto-started by design. Reboot state:
  RTL8723 `wlan0` works at `10.0.0.107`, AIC enumerates as bootrom
  `a69c:8d80`, and only `8723bu` is loaded.
- Added controlled discovery command to the Miracast script:
  `/root/aic_miracast/start_aic_miracast_cdump_board.sh rearm`
  It performs exactly one `wps_pbc any` and exits. Use this when the phone does
  not see the display after the GO is already up. Do not restore periodic idle
  `wps_pbc` refresh, because repeated beacon updates caused AIC
  `Beacon set failed: -32 (Broken pipe)`.
- Deployed script with `rearm` support:
  md5 `f6eefed9817e4d60233f992a9ab9512c`
  backup:
  `/root/aic_miracast/backups/start_aic_miracast_cdump_board.sh.before_rearm_cmd_20260725`
- Startup validation after reboot:
  AIC firmware path `a69c:8d80` -> `a69c:8d83`, fdrv loaded,
  P2P-GO ready as `DIRECT-XZ`, freq `5805`, IP `192.168.49.1`,
  passphrase `12345678`, display `F1C200S-AIC`.
- Manual rearm validation:
  `/root/aic_miracast/start_aic_miracast_cdump_board.sh rearm`
  logged `rearm wps_pbc once` and status remained `mode=P2P GO`,
  `wpa_state=COMPLETED`.
- Discovery-window correction:
  Keeping the PBC window closed after `WPS-TIMEOUT` is wrong for the sink
  runtime. When there is no connected station, the device should remain
  connectable. However, restoring a blind 20-second `wps_pbc any` loop is also
  wrong because repeated beacon updates previously caused AIC
  `Beacon set failed: -32 (Broken pipe)`.
- Final watcher logic:
  initialize a numeric count of `WPS-TIMEOUT` lines using:
  `grep 'WPS-TIMEOUT' "$LOG" | wc -l`
  while no station is connected, watch for the count increasing.
  on each new timeout, run exactly one:
  `wpa_cli ... wps_pbc any`
  and log:
  `wps timeout, rearm once`.
  This keeps the connection window open across WPS timeouts without high-rate
  beacon churn.
- Important bug fixed:
  do not use `grep -c ... || echo 0` for this counter. With no match, grep
  prints `0` and returns nonzero, which can produce an invalid multi-line
  value and break integer comparison in BusyBox shell.
- Deployed watcher-count fix:
  `/root/aic_miracast/start_aic_miracast_cdump_board.sh`
  md5 `df9d9d892a50275e72f59ceabb45ebbf`
  backup:
  `/root/aic_miracast/backups/start_aic_miracast_cdump_board.sh.before_wps_rearm_countfix_20260725`
- Validation:
  AIC recovered and started as `DIRECT-4O`, freq `5805`, IP `192.168.49.1`,
  passphrase `12345678`.
  After the first WPS timeout, logs showed:
  `wlan1: WPS-TIMEOUT`
  then:
  `wlan1: WPS-PBC-ACTIVE`
  and watcher:
  `wps timeout, rearm once Thu Jan  1 00:34:55 UTC 1970`.
  Status remained `mode=P2P GO`, `wpa_state=COMPLETED`.
- Logs moved out of `/tmp`:
  deployed script md5 `40deec807383dfb4aefb84b2f945c3e3`.
  Runtime paths:
  logs: `/root/roms/aic_miracast/logs/aic_miracast_board.log`
  run/watch/cdump logs: `/root/roms/aic_miracast/run/`
  H.264 captures: `/root/roms/aic_miracast/captures/`
  `/tmp` is only for control sockets/transient files.
- Important start/recover distinction:
  after a board reboot the AIC device is back at `a69c:8d80`; use `start`, not
  `recover`. `recover` assumes stage2 `8d83` and loaded `aic_load_fw` symbols.
  If `recover` is run on fresh `8d80`, fdrv fails with unknown symbols such as
  `get_fw_path`.
- Short-connect disconnect diagnosis after logs moved:
  RTSP negotiation completed successfully:
  OPTIONS, GET_PARAMETER, SET_PARAMETER, SETUP, PLAY, and
  `---- Negotiation successful ----`.
  DHCP assigned phone `192.168.49.52`.
  H.264 output grew to about 1.9M in:
  `/root/roms/aic_miracast/captures/miracast_19700101_000742.h264`.
  Then RTP stalled at:
  `packets=3999 bytes=4337960`.
  Station still existed but was inactive about 150s; driver station data was
  clearly unreliable (`signal 0 dBm`, huge `tx failed` value).
  Dmesg showed AIC driver errors:
  `Frame received but no active vif`,
  `aicwf_process_rxframes pkt_len:63401 skb->len:61`,
  followed by continuous `cmd queue crashed`.
  Therefore this disconnect is not WPS, DHCP, RTSP, or `/tmp` space. It is
  AIC fdrv/USB receive path entering a bad state after live RTP starts.
- Watcher mitigation deployed:
  `/root/aic_miracast/start_aic_miracast_cdump_board.sh`
  md5 `337830c1586c908a3ba4a4c286aeee3d`
  backup:
  `/root/aic_miracast/backups/start_aic_miracast_cdump_board.sh.before_no_iw_poll_recover_20260725`
- Watcher changes:
  remove high-rate `iw dev wlan1 station dump` polling from the watch loop.
  During active cdump, only monitor output file growth.
  If output size is unchanged for 75 seconds, kill cdump and launch
  `start_aic_miracast_cdump_board.sh recover` in the background.
  Reason: `iw station dump` itself goes through AIC/nl80211 command paths and
  can stress or amplify the same `cmd queue crashed` failure being debugged.
- Validation:
  recovered without MUSB reset or RTL8723 changes.
  New GO: `DIRECT-LW`, freq `5805`, IP `192.168.49.1`, passphrase `12345678`.
  Generated watcher no longer contains `station dump`; it contains
  `output stalled, recover AIC ...`.
- Immediate disconnect after the no-iw watcher still drove the board into a
  half-alive state (`10.0.0.107:22` TCP accepts but SSH banner stalls), so the
  failure is not caused solely by the watcher polling. After board recovery,
  reduce the Miracast advertised video format further to lower RTP pressure on
  AIC USB RX.
- Source patched:
  `/home/wnk/LicheePi_Nano/third_party/lazycast_host_20260721/miracast_sink_dump.c`
  advertised `wfd_video_formats` changed from:
  `00 00 02 04 00000007 00000000 00000000 00 0000 0000 00 none none`
  to:
  `00 00 02 04 00000001 00000000 00000000 00 0000 0000 00 none none`
  This leaves only the lowest CEA bit instead of allowing the phone to choose
  bit `00000002`, which was still enough to crash the AIC receive path.
- Build:
  `arm-linux-gnueabi-gcc -O2 -Wall -Wextra -static -o miracast_sink_dump.lowest.arm miracast_sink_dump.c`
  md5 `e2e246cc2058782346c08223b2666281`
- Board deployed:
  `/root/aic_miracast/miracast_sink_dump`
  md5 `e2e246cc2058782346c08223b2666281`
  backup:
  `/root/aic_miracast/backups/miracast_sink_dump.before_lowest_video_20260725`
- Startup validation after deploying lowest-video dump:
  `DIRECT-Zt`, freq `5805`, IP `192.168.49.1`,
  passphrase `12345678`, `mode=P2P GO`, `wpa_state=COMPLETED`.
  Script md5 remains `337830c1586c908a3ba4a4c286aeee3d`.
- Next test:
  connect phone again and check whether source SET_PARAMETER now selects
  `wfd_video_formats ... 00000001`.
  If it still disconnects quickly and AIC logs show `aicwf_process_rxframes`
  plus `cmd queue crashed`, the remaining issue is in AIC fdrv/USB RX under RTP
  load, not the Miracast negotiation path.
- Lowest-video experiment was rolled back because the user observed immediate
  disconnect, whereas the previous known-good dump could last longer.
  Restored:
  `/root/aic_miracast/miracast_sink_dump`
  md5 `95babf6aafcef01987293a4e853f095a`
  backup of lowest-video binary:
  `/root/aic_miracast/backups/miracast_sink_dump.before_restore_known_good_20260725`
- Post-rollback disconnect diagnosis:
  after recover the current P2P peer was `6e:40:e8:1e:07:41`, device name
  `WNK`, flags `[PROBE_REQ_ONLY]`, `wps_method=not-ready`,
  `wfd_subelems=00000600101c440032`.
  Dmesg showed station add/del for `6e:40:e8:1e:07:41`, not the previous
  successful `a6:00:33:22:26:9e` station. That path did not reach DHCP/RTSP.
  Treat this as likely WLAN-Direct / ordinary P2P probing or direct-connect,
  not the successful Android wireless-display WFD source path.
- Log preservation fix:
  start/recover no longer overwrites the previous log silently. Before
  truncating the active log, it copies:
  `aic_miracast_board.log`,
  `watch.log`,
  `cdump.log`
  to `/root/roms/aic_miracast/logs/*.prev.<timestamp>.<pid>.log`.
  This is necessary because recover can happen immediately after disconnect
  and otherwise erase the failure details.
- Deployed log-preserving script:
  `/root/aic_miracast/start_aic_miracast_cdump_board.sh`
  md5 `77f48251150bd56537464dd09707b9ae`
  backup:
  `/root/aic_miracast/backups/start_aic_miracast_cdump_board.sh.before_preserve_logs_20260725`
- Current state after log-preserving recover:
  known-good dump md5 `95babf6aafcef01987293a4e853f095a`
  GO `DIRECT-YT`, freq `5805`, IP `192.168.49.1`,
  passphrase `12345678`, display `F1C200S-AIC`.

Next engineering work:
- Turn the successful manual/runtime flow into a controlled board command:
  start, stop, reconnect, dump-to-file, and optional pipe-to-player.
- Add cleanup for old `/tmp/aic_miracast/*.h264` files before startup, because
  `/tmp` is only 26 MiB and successful dumps fill it quickly.
- Decide whether h264 output should be written to `/root/roms`, streamed to a
  player, or piped into a recorder instead of accumulating in `/tmp`.
```

## AIC8800 Miracast Baseline Lock - 2026-07-25
```text
Current rule:
- The only board-side AIC8800 Miracast baseline that may be treated as known
  working is the archived success snapshot:
  `/home/wnk/F1C200S_host_archive/known_good/aic8800_board_miracast_success_20260725/`
- Do not mix AIC modules from other source trees into this runtime.
- In particular, do not replace `aic_load_fw.ko` unless the matching
  `aic8800_fdrv.ko`, exported symbols, and board firmware-download behavior
  are proven together.

Known-good baseline fingerprints:
- `aic_load_fw.ko`
  `81528a07455d757581c5414de2dc3ead`
- `aic8800_fdrv.ko`
  `6400244a523052102f9686a2d68b6db6`
- `miracast_sink_dump`
  `95babf6aafcef01987293a4e853f095a`
- `tiny_dhcpd_49`
  `2e8c7e6533eeb85095799092da06707f`
- `wpa24_aic_wfd_supplicant`
  `58e2ac9e14914b908ef23c682e61c38c`
- `wpa24_aic_wfd_cli`
  `97f7384a530c9514f75533ab9e530a56`
- original success script:
  `b8a4355f69cf377acc42c5effc7ef2d6`

Reason for this lock:
- A later attempted monitor-frame fdrv rebuild from
  `ugreen_cm762_v14_board_5p7_noprealloc_20260723` produced:
  `aic_load_fw.ko` `b04030c9693fbc1463e411b1203e66c9`
  `aic8800_fdrv.ko` `7f2bfbe44344ce62b556bf50e5143d2e`
- Deploying that matching pair made the board drop/reset during the
  `load firmware from 8d80` stage. Treat that pair as rejected for board use.
- Deploying only the rebuilt fdrv against the old loader failed with unknown
  symbols, confirming that loader/fdrv pairs cannot be mixed casually.

Current runtime caveat:
- `/root/roms` is a vfat partition and was remounted read-only after:
  `FAT-fs (mmcblk0p3): error, fat_free_clusters: deleting FAT entry beyond EOF`
  `FAT-fs (mmcblk0p3): Filesystem has been set read-only`
- Until fsck is run, AIC Miracast logs and captures should use:
  `ARCHIVE_DIR=/root/aic_runtime`
  not `/root/roms/aic_miracast`.

Current disconnect diagnosis:
- RTSP heartbeats are answered. Logs show phone `GET_PARAMETER` followed by
  `RTSP/1.0 200 OK`.
- Memory leak is not the primary evidence. `mem_watch.log` showed
  `MemAvailable` still around 24 MB after disconnect and no OOM.
- The recurring failure signature is AIC RX corruption and command queue
  failure:
  `aicwf_process_rxframes pkt_len:... skb->len:...`
  followed later by:
  `cmd queue crashed`
- Host-side AIC Miracast also previously showed auto-disconnect behavior, so
  the issue is likely AIC driver/firmware behavior under Miracast RTP load,
  not a board-only script issue.

Next repair rule:
- Before modifying driver code again, locate the source tree or archived source
  that corresponds to the known-good module pair above.
- If the exact matching source cannot be found, keep the binary baseline and
  limit work to scripts, logging, bitrate/format negotiation, and external
  mitigations. Do not deploy rebuilt AIC modules from unrelated trees.
```

## AIC8800 Miracast Current Working Version Record - 2026-07-26
```text
Purpose:
- This is the current version to preserve before continuing AIC8800 Miracast
  disconnect repair.
- Treat this as the rollback target for board-side Miracast testing unless a
  later version is explicitly confirmed better by a real phone connection and
  H.264/RTP output test.

Preserved baseline:
- Archive root:
  `/home/wnk/F1C200S_host_archive/known_good/aic8800_board_miracast_success_20260725/`
- Original success archive includes:
  modules, AIC firmware, wpa_supplicant/wpa_cli, tiny DHCP, Miracast dump
  program, startup script, runtime config, logs, state snapshot, md5 manifest,
  and a successful H.264 sample.
- Current runtime snapshot before further repair:
  `/home/wnk/F1C200S_host_archive/known_good/aic8800_board_miracast_current_runtime_20260726_0009/`
  tar:
  `/home/wnk/F1C200S_host_archive/known_good/aic8800_board_miracast_current_runtime_20260726_0009.tar`

Known-good binary fingerprints that must not be mixed:
- `aic_load_fw.ko`
  `81528a07455d757581c5414de2dc3ead`
- `aic8800_fdrv.ko`
  `6400244a523052102f9686a2d68b6db6`
- `miracast_sink_dump`
  `95babf6aafcef01987293a4e853f095a`
- `tiny_dhcpd_49`
  `2e8c7e6533eeb85095799092da06707f`
- `wpa24_aic_wfd_supplicant`
  `58e2ac9e14914b908ef23c682e61c38c`
- `wpa24_aic_wfd_cli`
  `97f7384a530c9514f75533ab9e530a56`

Current board script lineage:
- Runtime script:
  `/root/aic_miracast/start_aic_miracast_cdump_board.sh`
- Current observed board script md5 on 2026-07-26:
  `7ee939a970afd3d64776b8a8496348c9`
- Last preserved/log-retaining deployed version:
  md5 `77f48251150bd56537464dd09707b9ae`
- Previous backups on board:
  `/root/aic_miracast/backups/start_aic_miracast_cdump_board.sh.before_preserve_logs_20260725`
  `/root/aic_miracast/backups/start_aic_miracast_cdump_board.sh.before_no_iw_poll_recover_20260725`
  `/root/aic_miracast/backups/start_aic_miracast_cdump_board.sh.before_wps_rearm_countfix_20260725`
  `/root/aic_miracast/backups/start_aic_miracast_cdump_board.sh.before_rearm_cmd_20260725`
- Script behavior to preserve:
  start/recover/stop/rearm commands, no MUSB reset, no RTL8723 cleanup,
  no high-rate `iw station dump` polling, no periodic blind `wps_pbc` loop,
  preserve previous logs before truncating active logs, use one-shot rearm on
  WPS timeout or manual `rearm`.
- Current script still defaults `ARCHIVE_DIR` to `/root/roms/aic_miracast`;
  when testing before fsck, launch it with:
  `ARCHIVE_DIR=/root/aic_runtime /root/aic_miracast/start_aic_miracast_cdump_board.sh start`
  or patch the default only after backing up the script.

Current runtime directory rule:
- `/root/roms` had been remounted read-only by FAT errors during this debug
  cycle. Until fsck is intentionally run, use:
  `ARCHIVE_DIR=/root/aic_runtime`
- Do not place new long captures/logs in `/tmp`; `/tmp` is too small for
  Miracast H.264 dumps.

Known Miracast settings:
- Interface: AIC8800 `wlan1`; keep RTL8723 `wlan0` for SSH/control network.
- Display name: `F1C200S-AIC`
- GO IP: `192.168.49.1`
- Passphrase: `12345678`
- Frequency/channel when successful: 5805 MHz / channel 161.
- P2P primary device type must remain:
  `8-0050F204-5`
- WFD subelements from known-good path:
  `wfd_subelem_set 0 000600111c44012c`
  `wfd_subelem_set 1 0006000000000000`
  `wfd_subelem_set 6 000700000000000000`
- 2026-07-26 startup validation with current script and
  `ARCHIVE_DIR=/root/aic_runtime`:
  AIC firmware path succeeded: `a69c:8d80` -> `a69c:8d83`.
  P2P-GO ready:
  SSID `DIRECT-hn`, freq `5805`, IP `192.168.49.1`,
  passphrase `12345678`, display `F1C200S-AIC`.

Confirmed failures and rollbacks:
- Rebuilt module pair from `ugreen_cm762_v14_board_5p7_noprealloc_20260723`
  is rejected for board use:
  `aic_load_fw.ko` `b04030c9693fbc1463e411b1203e66c9`
  `aic8800_fdrv.ko` `7f2bfbe44344ce62b556bf50e5143d2e`
  Symptom: board dropped/reset during firmware download from `8d80`.
- Lowest-video `miracast_sink_dump` experiment was rolled back because it
  disconnected faster. Current dump binary should be the original
  `95babf6aafcef01987293a4e853f095a`.

Current diagnosis:
- RTSP negotiation can complete and phone GET_PARAMETER heartbeats have been
  answered with `RTSP/1.0 200 OK`; do not assume heartbeat handling is the
  only failure.
- `/tmp` full and memory leak are not the primary evidence from current logs.
- The recurring bad signature is AIC RX path corruption under live Miracast
  RTP load:
  `aicwf_process_rxframes pkt_len:... skb->len:...`
  followed by `cmd queue crashed`, station removal, or SSH becoming half-alive.

Next work rule:
- First look for the exact source tree matching the known-good module md5 pair.
- If no matching source is found, continue with runtime-only mitigation:
  better logging, controlled reconnect, lower source pressure, and clean
  restart. Do not rebuild/deploy unrelated AIC modules.

Source search result on 2026-07-26:
- Searched `/home/wnk/LicheePi_Nano` and `/home/wnk/F1C200S_host_archive`
  for `aic_load_fw.ko` md5 `81528a...` and `aic8800_fdrv.ko` md5
  `640024...`.
- Matches were found only in:
  `/home/wnk/F1C200S_host_archive/known_good/aic8800_board_miracast_success_20260725/...`
  and:
  `/home/wnk/F1C200S_host_archive/known_good/aic8800_board_miracast_current_runtime_20260726_0009/...`
- No source tree with rebuilt artifacts matching those exact module md5s was
  found in the normal LicheePi_Nano or host archive trees.
- Therefore do not patch/rebuild AIC driver for the next test round. Continue
  from the binary baseline and collect failure evidence first.
- Candidate checked after user pointed to:
  `/home/wnk/LicheePi_Nano/third_party/aic8800_hostgood_fdrv_8d83_20260723/aic8800_fdrv`
  Result:
  this is useful as the host-side 8d83 fdrv source/reference, but it is not the
  exact board baseline build output.
  Evidence:
  `aic8800_fdrv/aic8800_fdrv.ko` md5 is
  `6414bdcf221a5150ab3b90092aad8a74`, not board baseline
  `6400244a523052102f9686a2d68b6db6`.
  `modinfo` shows `vermagic: 4.15.0-142-generic SMP mod_unload`, and
  `.aic8800_fdrv.ko.cmd` links with `ld -r -m elf_x86_64`, so the existing ko
  in that directory is a host x86 module, not an ARM 5.7.1 board module.
  The tree does contain the important 8d83 source changes:
  `USB_PRODUCT_ID_AIC8800D83 0x8d83`,
  `pid == ... || pid == USB_PRODUCT_ID_AIC8800D83`,
  and a `USB_DEVICE(... AIC8800D83)` table entry.
  Release marker:
  `RELEASE_DATE "2026_0123_5f7be68d"`.
  Use it only as a reference/candidate for controlled future rebuilds, not as
  a drop-in replacement for the current board baseline.

Controlled source-build reproduction - 2026-07-26:
- User requested using the current source-built version to reproduce, even
  though it is not the locked binary baseline.
- Source:
  `/home/wnk/LicheePi_Nano/third_party/aic8800_hostgood_fdrv_8d83_20260723`
- Build work tree:
  `/home/wnk/LicheePi_Nano/third_party/aic8800_hostgood_fdrv_8d83_board_repro_20260725_092535`
- Build output archive:
  `/home/wnk/F1C200S_host_archive/known_good/aic8800_hostgood_8d83_board_repro_20260725_092535/`
- Build command shape followed the documented toolchain rule:
  `PATH=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin:...`
  `make KDIR=/home/wnk/LicheePi_Nano/linux ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- CONFIG_PLATFORM_UBUNTU=n CONFIG_PREALLOC_RX_SKB=n CONFIG_PREALLOC_TXQ=y modules`
- Candidate ARM/5.7.1 md5:
  `aic_load_fw.ko`
  `48e6d88aa79518428238eee32341bc29`
  `aic8800_fdrv.ko`
  `ed39b51b13e1105650b089a59f8dab59`
- Candidate deployed only to:
  `/root/aic_miracast/candidates/aic8800_hostgood_8d83_board_repro_20260725_092535/`
  The locked baseline files in `/lib/modules/5.7.1/extra/` were not
  overwritten.
- Important testing detail:
  the first candidate start attempt did not actually switch modules because
  non-interactive board SSH did not have `rmmod` in PATH. The fix was to use
  `/sbin/rmmod`.
- Confirmed real switch:
  after `/sbin/rmmod aic8800_fdrv; /sbin/rmmod aic_load_fw`,
  `/proc/modules` showed only `8723bu`.
  Starting with:
  `ARCHIVE_DIR=/root/aic_runtime LOAD_FW=/root/aic_miracast/candidates/aic8800_hostgood_8d83_board_repro_20260725_092535/aic_load_fw.ko FDRV=/root/aic_miracast/candidates/aic8800_hostgood_8d83_board_repro_20260725_092535/aic8800_fdrv.ko /root/aic_miracast/start_aic_miracast_cdump_board.sh start`
  loaded:
  `aic_load_fw 65536`
  `aic8800_fdrv 544768`
- Candidate runtime status:
  AIC device was already stage2 `a69c:8d83`.
  Candidate fdrv bound successfully.
  P2P-GO ready as:
  SSID `DIRECT-mz`, freq `5805`, IP `192.168.49.1`,
  passphrase `12345678`, display `F1C200S-AIC`.
- This run tests candidate fdrv on an already-enumerated `8d83` device. To test
  the candidate `aic_load_fw` firmware-download path from `8d80`, reboot or
  replug AIC and launch the same command with `LOAD_FW/FDRV` candidate paths.

AIC8800 driver source consolidation - 2026-07-26:
- After the source-built candidate successfully projected on the board, active
  AIC8800 driver development was consolidated to one source tree only:
  `/home/wnk/LicheePi_Nano/third_party/aic8800_hostgood_fdrv_8d83_20260723`
- This tree now contains:
  `STATUS_CURRENT_BASELINE.md`
- Rule:
  future AIC8800 driver changes start from this tree. Do not revive old AIC8800
  experiment directories unless explicitly comparing against the obsolete
  archive.
- Current source backup:
  `/home/wnk/F1C200S_host_archive/known_good/aic8800_current_board_miracast_20260726_002.tar`
  sha256:
  `8246037a9dffacd759d97fab22793684f424641088a16161169de7546c4533e4`
- Current board repro build archive remains:
  `/home/wnk/F1C200S_host_archive/known_good/aic8800_hostgood_8d83_board_repro_20260725_092535/`
- Obsolete old AIC8800 driver directories from
  `/home/wnk/LicheePi_Nano/third_party` and `/home/wnk/LicheePi_Nano/backups`
  were archived and removed from active locations.
  Obsolete archive:
  `/home/wnk/F1C200S_host_archive/obsolete_aic8800_drivers_20260726_002.tar`
  sha256:
  `6d816438288744d5e26e5bf820a554c508a222aa9f43544c2ff8d2a714e1d64a`
- Post-clean active AIC8800 driver dirs under `third_party`/`backups`:
  only `/home/wnk/LicheePi_Nano/third_party/aic8800_hostgood_fdrv_8d83_20260723`

Disconnect/system-memory diagnosis - 2026-07-26:
- User observed board-side Miracast disconnect followed by allocator failures:
  `SLUB: Unable to allocate memory ... gfp=0xa20(GFP_ATOMIC)`
  `aicwf_busrx_thr: page allocation failure`
  stack entered `rtl8723bu_recv_tasklet` / `rtw_os_alloc_recvframe`.
- Interpretation:
  this is not a pure RTSP/Miracast heartbeat failure. It is a low-memory /
  atomic-allocation pressure failure while AIC8800 and RTL8723BU are both
  active. The current task context may be `aicwf_busrx_thr`, but the stack can
  run RTL8723 RX softirq work, so both USB WiFi drivers are involved.
- Reboot baseline memory:
  `MemAvailable` around 26-27 MB,
  default `vm.min_free_kbytes` only `765`.
  `kmalloc-64` was already nearly full after boot:
  `1264/1280` active objects.
- Source inspection:
  current AIC source has many `GFP_ATOMIC` allocations in RX/TX paths,
  including `aicwf_usb.c`, `aicwf_txrxif.c`, and `rwnx_rx.c`.
- Low-pressure runtime patch:
  `/root/aic_miracast/start_aic_miracast_cdump_board.sh`
  md5 after patch:
  `1203b10bd7fb95ff257d3edec44f5c55`
  backup:
  `/root/aic_miracast/backups/start_aic_miracast_cdump_board.sh.before_lowpressure_20260726`
  Changes:
  add `AIC_DBG_LEVEL=${AIC_DBG_LEVEL:-1}`,
  default `WPA_DEBUG` to empty,
  pass `aicwf_dbg_level="$AIC_DBG_LEVEL"` instead of hardcoded `3`,
  call wpa_supplicant with `${WPA_DEBUG:+$WPA_DEBUG}` to avoid an empty argv.
- Candidate loader result:
  source-built candidate `aic_load_fw.ko`
  `48e6d88aa79518428238eee32341bc29`
  still must not be used for cold `8d80` firmware download on the board.
  Starting with this candidate loader from `8d80` closed the SSH connection and
  the board rebooted/back to `8d80`.
- Candidate fdrv result:
  source-built candidate `aic8800_fdrv.ko`
  `ed39b51b13e1105650b089a59f8dab59`
  requires matching candidate `aic_load_fw.ko` symbols. It cannot be loaded
  against the locked baseline loader; otherwise `insmod aic8800_fdrv.ko`
  reports unknown symbols.
- Safe reproduction sequence:
  1. Use locked baseline loader only to perform `8d80 -> 8d83` firmware
     download, or start from a board already at `8d83`.
  2. Unload baseline loader if present.
  3. Insert candidate `aic_load_fw.ko` only after the device is already `8d83`
     so it provides matching exported symbols without doing firmware download.
  4. Insert candidate `aic8800_fdrv.ko`.
  5. Start Miracast with:
     `ARCHIVE_DIR=/root/aic_runtime AIC_DBG_LEVEL=0 LOAD_FW=/root/aic_miracast/candidates/aic8800_hostgood_8d83_board_repro_20260725_092535/aic_load_fw.ko FDRV=/root/aic_miracast/candidates/aic8800_hostgood_8d83_board_repro_20260725_092535/aic8800_fdrv.ko /root/aic_miracast/start_aic_miracast_cdump_board.sh start`
- Low-pressure reproduction state:
  `vm.min_free_kbytes=8192`
  GO ready as:
  SSID `DIRECT-mi`, freq `5805`, IP `192.168.49.1`,
  passphrase `12345678`, display `F1C200S-AIC`.
  Memory/slab watcher log:
  `/root/aic_runtime/logs/mem_slab_watch.log`

Low-memory AIC fdrv optimization candidate - 2026-07-26:
- Current memory usage breakdown during AIC Miracast idle/ready state:
  user processes are not the main consumer. `miracast_sink_dump`,
  `wpa24_aic_wfd_supplicant`, and shell/watch helpers are all small.
  Main pressure is kernel memory:
  `Slab` about 11 MB, `SUnreclaim` about 9.9 MB, with `kmalloc-4k`,
  `kernfs_node_cache`, `kmalloc-512`, `kmalloc-128`, and `kmalloc-64` high.
- Atomic allocation issue:
  `/proc/buddyinfo` showed very few order-0 normal pages and no HighAtomic
  reserve. This explains why `GFP_ATOMIC` can fail even when `MemFree` still
  looks nonzero.
- Runtime mitigation already applied:
  `vm.min_free_kbytes=8192` during tests.
  AIC driver log level default reduced through `AIC_DBG_LEVEL`.
  wpa debug default disabled.
- Source optimization candidate built from current baseline:
  `/home/wnk/F1C200S_host_archive/known_good/aic8800_lowmem_candidate_20260725_100811/`
- lowmem candidate md5:
  `aic_load_fw.ko`
  `2f5e6c10c6fd58688a7d7782ac8dc6c3`
  `aic8800_fdrv.ko`
  `8aa9a113125596651b37a1a31879415a`
- lowmem candidate changes:
  `AICWF_USB_RX_URBS 20 -> 8`
  `AICWF_USB_MSG_RX_URBS 100 -> 8`
  `AICWF_USB_TX_URBS 200 -> 48`
  `AICWF_USB_AGGR_MAX_PKT_SIZE 2048*10 -> 2048*4`
  `AICWF_USB_MAX_AMSDU_PKT_SIZE 2048*6 -> 2048*3`
  `MAX_REORD_RXFRAME 250 -> 64`
  `CONFIG_RWNX_DBG n`
  `CONFIG_MCC n`
  `CONFIG_STA_SCAN_WHEN_P2P_WORKING n`
  `CONFIG_FILTER_TCP_ACK n`
- `CONFIG_RWNX_RADAR n` was attempted but failed to compile because
  `rwnx_msg_rx.c` still references radar structures unconditionally. Keep
  radar compiled for now.
- Deployment pending:
  board entered SSH-banner-timeout state while the previous candidate was
  active. TCP port 22 may accept but SSH login stalls. Use serial or reboot
  before deploying the lowmem candidate.
- Planned deployment rule:
  Do not use lowmem candidate `aic_load_fw.ko` for cold `8d80` firmware
  download until separately proven. Use locked baseline loader for `8d80 ->
  8d83`, then load lowmem candidate `aic_load_fw.ko` only for matching exported
  symbols and insert lowmem candidate `aic8800_fdrv.ko`.
- Follow-up test after user provided OOM log:
  board was cleanly up with `a69c:8d83`, no AIC fdrv loaded, and lowmem
  candidate modules present under `/root/aic_miracast/candidates/`.
  Loading the lowmem candidate pair succeeded at module insertion level, but
  `wlan1` was never registered. Dmesg showed `aicwf_usb_probe` failed with
  errno `-1`.
  Unloading that pair and starting the locked baseline fdrv in the same USB
  device state also failed to register `wlan1`; dmesg showed
  `aicwf_usb_probe` errno `-110`.
  Conclusion: this lowmem candidate is not runnable as-is, and after this
  failed probe the AIC device state should be considered dirty. Continue the
  next test from a clean reboot/replug and do not use this candidate for normal
  startup.

AIC RX lowmem fix v1 - 2026-07-26:
- Source tree:
  `/home/wnk/LicheePi_Nano/third_party/aic8800_rx_lowmem_fix_20260726`
- Host archive:
  `/home/wnk/F1C200S_host_archive/known_good/aic8800_rx_lowmem_fix_20260726_1`
- Board candidate:
  `/root/aic_miracast/candidates/aic8800_rx_lowmem_fix_20260726_1/`
- md5:
  `aic_load_fw.ko` `2dc383bad054168ac31803700b267351`
  `aic8800_fdrv.ko` `b7532c91afafbff4ae6a90f60b4ee2f0`
- Changes:
  `MAX_RXQLEN 2000 -> 128`,
  `AICWF_USB_MSG_RX_URBS 100 -> 32`,
  `CONFIG_TXRX_THREAD_PRIO y -> n`.
- Startup result:
  using locked baseline loader for `8d80 -> 8d83`, then loading v1 candidate
  fdrv successfully registered `wlan1` and started P2P GO:
  SSID `DIRECT-TP`, freq `5805`, passphrase `12345678`.
- Failure result:
  after phone connected, v1 quickly spammed:
  `usb_err:<aicwf_usb_rx_complete,550>: rx_priv->rxq is over flow!!!`
  and then hit OOM within seconds. The queue cap of 128 is too low for Miracast
  burst RX; it drops packets and still does not prevent slab pressure.
- Do not use v1 as normal runtime. Next candidate should keep tighter URB
  ingress limits but raise RX queue headroom, e.g. RX URB 12 and RXQ 512.
```

## AIC8800 Miracast Low-Memory Runtime Mitigation - 2026-07-26
```text
Context:
- User rebooted board after SLUB/GFP_ATOMIC failures during AIC8800 Miracast
  while RTL8723BU wlan0 was also active.
- Board reachable over COM3; wlan0 had 10.0.0.107 but Windows/board ping was
  not reliable during this check, so changes were deployed through serial.

Observed after reboot/start:
- AIC baseline successfully in stage2:
  `a69c:8d83`
- Modules:
  `aic_load_fw` md5 baseline, `aic8800_fdrv` baseline, plus `8723bu`.
- Miracast GO ready:
  SSID `DIRECT-Qm`, freq 5805/channel 161, IP 192.168.49.1,
  passphrase 12345678, display F1C200S-AIC.
- Memory idle/ready after mitigation:
  MemAvailable about 15 MB, Slab about 11 MB, SUnreclaim about 9.8 MB.
  `kmalloc-4k` around 996/996, `kmalloc-512` around 1255/1264,
  `kmalloc-128` around 1664/1664, `kmalloc-64` around 1258/1280.

Runtime script patched:
- `/root/aic_miracast/start_aic_miracast_cdump_board.sh`
- New md5:
  `9f69d3d000dc4555c7ad0ab2c080cbcb`
- Backups:
  `/root/aic_miracast/backups/start_aic_miracast_cdump_board.sh.before_memopt_20260726`
  md5 `1203b10bd7fb95ff257d3edec44f5c55`
  `/root/aic_miracast/backups/start_aic_miracast_cdump_board.sh.before_watchkill2_20260726`
  md5 `82d7da6c3ea57247f7f4ed770e35986f`

Patch details:
- Default `ARCHIVE_DIR` changed from `/root/roms/aic_miracast` to
  `/root/aic_runtime` to avoid the previously dirty/read-only FAT ROMS
  partition and avoid `/tmp`.
- Default `AIC_DBG_LEVEL` reduced to 0.
- Added `LOWMEM_TUNE=1` default:
  set `vm.min_free_kbytes=8192`, run `sync`, then `drop_caches=3` once at
  Miracast start.
- Added `STOP_GMENU=1` default:
  kill `gmenu2x` and `run_gmenu2x.sh` during Miracast start to free memory and
  avoid display contention.
- Fixed stale watcher cleanup:
  `stop_all` now also calls `killall watch.sh`.
  Reason: old watcher path is `/root/aic_runtime/run/watch.sh`; previous
  matcher only looked for `aic_miracast/...watch.sh`, leaving duplicate
  watchers after restart.
- Watcher loop sleep increased from 0.5s to 2s.

Live monitor:
- Started lightweight 5s memory/file-growth monitor:
  `/root/aic_runtime/run/mem_watch.sh`
- PID file:
  `/root/aic_runtime/run/mem_watch.pid`
- Log:
  `/root/aic_runtime/logs/mem_slab_watch.log`
- Records:
  meminfo key lines, buddyinfo, selected slabinfo caches, and current
  `/root/aic_runtime/captures/miracast_latest.h264` byte count.

Current test instruction:
- Ask user to connect phone Miracast to `F1C200S-AIC`.
- If disconnect happens, collect:
  `/root/aic_runtime/logs/mem_slab_watch.log`
  `/root/aic_runtime/logs/aic_miracast_board.log`
  `/root/aic_runtime/run/watch.log`
  `/root/aic_runtime/run/cdump.log`
  `dmesg | grep -E 'SLUB|GFP|aicwf|cmd queue|rxframes|RTL871X'`
```

## AIC8800 Miracast No-Capture Test and Minute Memory Logger - 2026-07-26
```text
Result:
- Running Miracast with H.264 output redirected to `/dev/null` lasted more
  than 10 minutes, compared with roughly 3 minutes when writing H.264 files.
- Therefore H.264 file writing/page cache pressure is a real accelerator, but
  not the root cause. The remaining failure is likely kernel-side memory
  pressure/resource growth in AIC/USB/network RX path, possibly involving
  skbuff/slab/GFP_ATOMIC pressure while RTL8723BU wlan0 is also active.

Diagnostic watcher:
- Local archived script:
  `C:\Users\26301\Desktop\F1C200S_归档_脚本和资料\work_dirs\_codex_serial_tools\aic_miracast_20260726\monitors\board_aic_cast_minute_memlog_20260726.sh`
- Intended board path:
  `/root/aic_runtime/run/cast_minute_mem.sh`
- Log path:
  `/root/aic_runtime/logs/cast_minute_mem.log`
- It records once per minute:
  process VSZ/RSS for Miracast/wpa/tiny_dhcp/watch/dropbear/gmenu/cedar/adb
  related processes,
  meminfo,
  buddyinfo,
  selected slabinfo caches including kmalloc and skbuff caches,
  wlan0/wlan1 state,
  AIC/8723 module state,
  and dmesg error tail.
- This log is on the root filesystem under `/root/aic_runtime/logs`, not in
  `/tmp`, so it should survive a board reboot unless the rootfs is damaged.

No-capture helper:
- Local archived script:
  `...\aic_miracast_20260726\monitors\watch_null_direct_20260726.sh`
- Intended board path:
  `/root/aic_runtime/run/watch_null_direct.sh`
- It waits for DHCP log IP and then runs:
  `miracast_sink_dump <phone_ip> /dev/null`
  to keep RTSP/RTP reception while avoiding H.264 file writes.

Desktop organization:
- Windows Desktop root was cleaned of AIC/F1C one-off debug scripts.
- Current local AIC/Miracast scripts are grouped under:
  `C:\Users\26301\Desktop\F1C200S_归档_脚本和资料\work_dirs\_codex_serial_tools\aic_miracast_20260726\`
- A cleanup recovery note exists at:
  `...\host_scripts\aic_miracast_debug_20260726_desktop_cleanup_dir\README_STATUS.md`
- Rule reinforced:
  do not leave new one-off debug scripts on the Desktop root. Put them under
  the dated archive/work folder immediately, with status notes.
```

## AIC8800 Miracast Standard Start Script - 2026-07-26
```text
Status: working on board; user confirmed phone connected.
Board path:
- /root/aic_miracast/start_aic_miracast_cdump_board.sh
Script md5:
- 94e65755ebb8ecadb771f9de4f463623
Board backup before final fixes:
- /root/aic_miracast/backups/start_aic_miracast_cdump_board.sh.before_standard_20260726
- /root/aic_miracast/backups/start_aic_miracast_cdump_board.sh.before_no_wpacli_stop_20260726
- /root/aic_miracast/backups/start_aic_miracast_cdump_board.sh.before_lf_ascii_20260726
- /root/aic_miracast/backups/start_aic_miracast_cdump_board.sh.before_bom_fix2_20260726
Host archive:
- /home/wnk/F1C200S_host_archive/known_good/20260726_aic_miracast_standard_start_success

Confirmed runtime:
- AIC USB: a69c:8d80 -> a69c:8d83
- Interface: wlan1
- Mode: P2P GO
- Display name: F1C200S-AIC
- Observed SSID: DIRECT-C3
- Frequency: 5805 MHz / channel 161
- Passphrase: 12345678
- Runtime/log directory: /root/aic_runtime
- RTL8723BU wlan0 left untouched for SSH/control network.

Critical fixes:
- Removed blocking wpa_cli terminate from pre-start cleanup. That had caused SSH banner hang before firmware load.
- Converted script to pure LF/ASCII; removed UTF-8 BOM and CRLF.
- Generates /root/aic_runtime/run/watch_aic_miracast.sh correctly; no zero-byte watch.sh.
- Exactly one wpa24_aic_wfd_supplicant, one tiny_dhcpd_49, one watcher, one mem_watch.
- Watcher re-arms WPS-PBC every 15 seconds while idle.
- Watcher reads phone IP from DHCP log and starts miracast_sink_dump immediately; do not ping-scan 192.168.49.20-80.
- Capture defaults to /dev/null to reduce file I/O pressure.

Do not regress:
- Do not restore the old generated watch.sh logic.
- Do not start duplicate tiny_dhcpd_49 processes.
- Do not use ping sweep to discover phone IP.
- Do not reset MUSB or disturb RTL8723 wlan0 during Miracast start.
```

## AIC8800 Miracast Retest On Main ST7701/OC720 System - 2026-07-31
```text
Board:
- Main system reachable over SSH at 10.0.0.107.
- Keep RTL8723BU wlan0 untouched for control SSH.
- AIC normal boot remains manual-only; do not auto-load AIC8800 at boot.

Finding:
- Candidate directory:
  /root/aic_miracast/candidates/aic8800_v14_360p30_stable/
- Its file aic_load_fw.baseline_8d80_loader.ko is not usable with the current
  kernel because it has no 5.7.1 vermagic and insmod reports:
  "invalid module format".
- Using current rootfs /lib/modules/5.7.1/extra/aic_load_fw.ko successfully
  changes AIC USB from a69c:8d80 to a69c:8d83.
- v14 aic8800_fdrv.ko cannot be paired with that /lib loader; it needs symbols
  not exported by the /lib loader, e.g. aicwf_prealloc_txq_alloc.

Working startup on this boot:
- Use the matched current rootfs pair:
  /lib/modules/5.7.1/extra/aic_load_fw.ko
    md5 81528a07455d757581c5414de2dc3ead
  /lib/modules/5.7.1/extra/aic8800_fdrv.ko
    md5 6400244a523052102f9686a2d68b6db6
- Command:
  ARCHIVE_DIR=/root/aic_runtime CAPTURE_MODE=null AIC_DBG_LEVEL=0 \
  LOWMEM_TUNE=1 STOP_GMENU=1 \
  LOAD_FW=/lib/modules/5.7.1/extra/aic_load_fw.ko \
  FDRV=/lib/modules/5.7.1/extra/aic8800_fdrv.ko \
  /root/aic_miracast/start_aic_miracast_cdump_board.sh start
- Result:
  START_RC=0
  P2P GO ready on wlan1, display F1C200S-AIC.
  Example SSID from this boot: DIRECT-BV.
  Frequency: 5805 MHz.
  Passphrase: 12345678.
  IP: 192.168.49.1.

Next test:
- Connect phone Miracast to F1C200S-AIC and collect:
  /root/aic_runtime/logs/aic_miracast_board.log
  /root/aic_runtime/run/watch.log
  /root/aic_runtime/run/cdump.log
  /root/aic_runtime/logs/mem_slab_watch.log
  dmesg grep for aic/wlan1/OOM/SLUB/GFP.
```

## AIC8800 Miracast PBC Immediate Watcher - 2026-07-26
```text
Status: working on board; user confirmed phone connected.
Board script:
- /root/aic_miracast/start_aic_miracast_cdump_board.sh
Current md5:
- 12a343491b6d283af2319b809e4a1da3
Host archive:
- /home/wnk/F1C200S_host_archive/known_good/20260726_aic_miracast_pbc_immediate_success

Critical connection rule:
- Do not periodically rearm WPS-PBC.
- Do not poll all_sta during idle or casting.
- Wait for phone P2P-PROV-DISC-PBC-REQ, then immediately run wps_pbc any.
- This is the confirmed standard connection method: keep listening, respond
  immediately to the phone's PBC request, and do not pre-spam/rearm PBC.
- WPS-TIMEOUT is logged only; it must not automatically rearm.
- DHCP log provides the phone IP.
- During cast, detect disconnect by wlan1 rx_packets/rx_bytes not increasing.

Confirmed working watcher log:
- watch start pbc-req-immediate + rx-counter mode; no periodic wps, no all_sta
- wps timeout observed; wait for phone pbc_req
- event rearm reason=pbc_req rc=0 count=1
- start cdump ip=192.168.49.52 out=/dev/null

Reason:
- Periodic WPS rearm produced repeated netlink attribute warnings and could trigger sched RT throttling.
- all_sta polling triggers MM_GET_STA_INFO_REQ and correlated with AIC cmd queue crashes.
```

## AIC8800 RX Invalid Frame Drop Candidate v4 - 2026-07-26
```text
Purpose:
- Investigate Miracast disconnect/OOM under RTP load after v3 still showed:
  `Frame received but no active vif`,
  `Received monitor frame but there is no monitor interface open`,
  `4addr Frame received`,
  `rxfrag`,
  followed by OOM in/around AIC RX paths.

Source:
- /home/wnk/LicheePi_Nano/third_party/aic8800_rx_drop_invalid_fix_20260726_v4

Host archive:
- /home/wnk/F1C200S_host_archive/known_good/aic8800_rx_drop_invalid_fix_20260726_4

Board candidate:
- /root/aic_miracast/candidates/aic8800_rx_drop_invalid_fix_20260726_4

md5:
- aic_load_fw.ko  e965d7aa9eca49b28dc8004acb859083
- aic8800_fdrv.ko ca28067466c0126715c863ea26b234e3

Base:
- v3 RX budget candidate:
  /home/wnk/LicheePi_Nano/third_party/aic8800_rx_budget_fix_20260726_v3

Changes:
- In rwnx_rx.c, drop and free monitor frames immediately when there is no
  monitor vif instead of continuing into normal RX processing.
- Drop unsupported 4addr frames quietly when CONFIG_SUPPORT_4ADDR is disabled.
- Remove high-rate `no active vif`, `4addr` hex dump, and `rxfrag` prints from
  the hot RX path.
- Keep v3 RX budget behavior unchanged.

Build:
- Used documented toolchain:
  PATH=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin:...
  make KDIR=/home/wnk/LicheePi_Nano/linux ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- CONFIG_PLATFORM_UBUNTU=n CONFIG_PREALLOC_RX_SKB=n CONFIG_PREALLOC_TXQ=y modules
- Build succeeded from top-level tree. Building only aic8800_fdrv is not enough
  because modpost needs symbols from matching aic_load_fw.

Deployment/test status:
- Board was cold AIC state `a69c:8d80`.
- Used locked baseline loader first:
  /root/aic_miracast/start_aic_miracast_cdump_board.sh loader
  Result: `loader: 8d83 ready after 1s`.
- Then unloaded baseline modules and started v4 candidate with:
  ARCHIVE_DIR=/root/aic_runtime CAPTURE_MODE=null AIC_DBG_LEVEL=0
  LOWMEM_TUNE=1 STOP_GMENU=1
  LOAD_FW=/root/aic_miracast/candidates/aic8800_rx_drop_invalid_fix_20260726_4/aic_load_fw.ko
  FDRV=/root/aic_miracast/candidates/aic8800_rx_drop_invalid_fix_20260726_4/aic8800_fdrv.ko
  /root/aic_miracast/start_aic_miracast_cdump_board.sh start
- GO started successfully:
  display F1C200S-AIC, SSID DIRECT-pD, freq 5805, IP 192.168.49.1,
  passphrase 12345678.
- Idle status after start:
  SSH alive, MemAvailable about 14 MB, SUnreclaim about 9.6 MB.
  watch.log showed only start and normal WPS timeout while waiting for phone
  PBC request.
  dmesg idle snapshot showed no `no active vif`, monitor, 4addr, rxfrag, or
  OOM spam.
- Runtime RTP-load phone test still pending at time of this note.

Follow-up after user-reported disconnect:
- User reported disconnect and rebooted board.
- Persisted logs were copied locally to:
  C:\Users\26301\Desktop\F1C200S_归档_脚本和资料\work_dirs\_codex_serial_tools\aic_miracast_20260726\v4_disconnect_logs_20260726_192456
- Logs showed successful PBC immediate connection:
  P2P-PROV-DISC-PBC-REQ -> WPS success -> AP-STA-CONNECTED ->
  DHCP phone IP 192.168.49.52 -> start cdump.
- cdump completed RTSP SETUP/PLAY and kept replying to GET_PARAMETER through
  CSeq 16. This again argues against a missing RTSP heartbeat response.
- mem_slab_watch during the captured period did not show runaway memory growth:
  MemAvailable stayed roughly 14.9-15.5 MB, SUnreclaim about 9.7 MB,
  skbuff_head_cache around 60-70 used objects, kmalloc-4k stable at about 999.
- No OOM/SLUB endpoint was captured because board was rebooted after disconnect.
- The v4 invalid-frame printk spam did not appear in captured logs.

Runtime script updated for next disconnect capture:
- Board backup before patch:
  /root/aic_miracast/backups/start_aic_miracast_cdump_board.sh.before_v4_disconnect_probe_20260726
  md5 12a343491b6d283af2319b809e4a1da3
- New board script:
  /root/aic_miracast/start_aic_miracast_cdump_board.sh
  md5 b97e23ab07f4eb8223bd9ba20aea8a0c
- Connection behavior intentionally unchanged:
  PBC request immediate response, no all_sta, no periodic WPS rearm during idle.
- Probe additions:
  watcher logs `cast tick` during active cast every 30 seconds with wlan1
  RX/TX packets/bytes and MemAvailable.
  watcher detects AP-STA-DISCONNECTED/CTRL-EVENT-DISCONNECTED and records a
  dmesg key snapshot before stopping cdump.
  watcher records a snapshot on RX stall or cdump exit.
  mem_watch was made lighter: no top, no per-loop dmesg grep, 30-second
  interval, wlan1 stats included.
  mem_slab_watch.log is rotated/truncated at each new start to avoid huge logs.

Current restart after probe patch:
- Board cold AIC state was a69c:8d80.
- Baseline loader again succeeded: `loader: 8d83 ready after 1s`.
- v4 candidate restarted successfully:
  display F1C200S-AIC, SSID DIRECT-ms, freq 5805, IP 192.168.49.1,
  passphrase 12345678.
- Idle status: no phone connection yet; wlan1 rx_packets=0, tx_packets slowly
  increasing only from GO/WPS management traffic; MemAvailable about 15.2 MB.
```
## AIC8800 Miracast v10 Backpressure Failure - 2026-07-26
```text
Status:
- v10 is failed/unsafe. Do not use as normal runtime.

Candidate:
- Host source:
  /home/wnk/LicheePi_Nano/third_party/aic8800_rx_backpressure_fix_20260726_v10
- Host archive:
  /home/wnk/F1C200S_host_archive/known_good/aic8800_rx_backpressure_fix_20260726_10
- Board candidate:
  /root/aic_miracast/candidates/aic8800_rx_backpressure_fix_20260726_10
- md5:
  aic_load_fw.ko d132bb7db5aa7dbe29da92b7b6b6939e
  aic8800_fdrv.ko a4cf0d9c5f848382703680ee6f37e1e1

Change:
- Added RX queue high-water checks before refilling data/message RX URBs.
- Refilled RX URBs from bus RX threads after queued frames were processed.
- Did not change WPS/PBC/RTSP logic.

Runtime result:
- Phone connection reached WPS success, AP-STA-CONNECTED, and DHCP
  DISCOVER/REQUEST.
- Board reset before miracast_sink_dump/RTSP started.
- This is worse than v9, which reached RTSP/RTP streaming.

Important cleanup finding:
- Root filesystem was at 100% after repeated tests because old generated
  /root/aic_runtime/captures/*.h264 and large old debug logs accumulated.
- Cleaned generated runtime artifacts under /root/aic_runtime:
  captures/*.h264, fast_watch.log, old *.prev logs, and v10_last_copy.
- Root filesystem recovered to about 80% used.

Current recovery state:
- Reverted runtime test to v9:
  /root/aic_miracast/candidates/aic8800_rx_skip_unexpected_4addr_cb_20260726_9
- Current GO after restart:
  display F1C200S-AIC, SSID DIRECT-fZ, freq 5805, passphrase 12345678.
```

## AIC8800 Miracast Restart Test On Main System - 2026-08-01
```text
Board:
- After user power-cycled/rebooted, board was reachable over SSH at 10.0.0.107.
- RTL8723BU control network remained healthy:
  wlan0 associated to wnk641_2.4G, DHCP IP 10.0.0.107.

Initial AIC state:
- lsusb showed AIC already in stage2 as a69c:8d83.
- /proc/modules showed only aic_load_fw plus 8723bu; aic8800_fdrv was not loaded.

v14 pair result:
- v14 candidate files:
  /root/aic_miracast/candidates/aic8800_v14_360p30_stable/aic_load_fw.ko
    md5 a762b1674cc4a55b727256fa542ac16f
  /root/aic_miracast/candidates/aic8800_v14_360p30_stable/aic8800_fdrv.ko
    md5 8259aa2ba4008452d75f26021e03969a
- Inserting v14 aic8800_fdrv registered wlan1, but starting WFD failed:
  ifconfig wlan1 up timed out, wpa_supplicant reported Broken pipe /
  Failed to initialize driver interface.
- Treat this v14 state as unreliable for this main-system boot.

Working recovery:
- Unloaded aic8800_fdrv and aic_load_fw without touching 8723bu/wlan0.
- Restarted Miracast using the current rootfs matched pair:
  /lib/modules/5.7.1/extra/aic_load_fw.ko
    md5 81528a07455d757581c5414de2dc3ead
  /lib/modules/5.7.1/extra/aic8800_fdrv.ko
    md5 6400244a523052102f9686a2d68b6db6
- Command environment:
  ARCHIVE_DIR=/root/aic_runtime CAPTURE_MODE=null AIC_DBG_LEVEL=0
  LOWMEM_TUNE=1 STOP_GMENU=1 GO_FREQ=5805
  LOAD_FW=/lib/modules/5.7.1/extra/aic_load_fw.ko
  FDRV=/lib/modules/5.7.1/extra/aic8800_fdrv.ko
  /root/aic_miracast/start_aic_miracast_cdump_board.sh start
- Result:
  START_RC=0
  P2P GO ready on wlan1.
  SSID DIRECT-I0, freq 5805, IP 192.168.49.1, passphrase 12345678.
- Runtime processes:
  wpa24_aic_wfd_supplicant, tiny_dhcpd_49, watch_aic_miracast.sh,
  dmesg_watch.sh, mem_watch.sh.

Next user test:
- Search/connect phone Miracast to F1C200S-AIC / DIRECT-I0.
- If it disconnects or board resets, collect:
  /root/aic_runtime/logs/aic_miracast_board.log
  /root/aic_runtime/run/watch.log
  /root/aic_runtime/run/cdump.log
  /root/aic_runtime/logs/mem_slab_watch.log
  dmesg grep for aic/wlan1/OOM/SLUB/GFP.
```

## AIC8800 wlan1 Registration Helper - 2026-08-01
```text
Purpose:
- User clarified current test is ordinary 5GHz WiFi host/device bring-up, not
  Miracast direct WiFi.
- Added a small helper that only performs AIC8800 USB firmware/driver bring-up
  until wlan1 is registered. It does not start P2P GO, DHCP server, or
  Miracast sink, and it does not touch RTL8723BU wlan0.

Board script:
- /root/aic_miracast/aic8800_register_wlan1.sh
- md5 1a669fff4a2f18da6e2b1797635f4afd

Host/rootfs overlay copy:
- /home/wnk/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/root/aic_miracast/aic8800_register_wlan1.sh
- md5 1a669fff4a2f18da6e2b1797635f4afd

Default matched module pair:
- LOAD_FW=/lib/modules/5.7.1/extra/aic_load_fw.ko
- FDRV=/lib/modules/5.7.1/extra/aic8800_fdrv.ko
- FW_PATH=/lib/firmware/aic8800D80

Usage after reboot:
- /root/aic_miracast/aic8800_register_wlan1.sh start
- /root/aic_miracast/aic8800_register_wlan1.sh status
- /root/aic_miracast/aic8800_register_wlan1.sh stop

Verified current boot:
- Running `start` while wlan1 was already registered returned rc=0.
- State remained:
  lsusb includes a69c:8d83 and 0bda:b720.
  modules include aic_load_fw, aic8800_fdrv, and 8723bu.
  wlan1 exists.
  wlan0 stayed associated to wnk641_2.4G with IP 10.0.0.107.
```

## Dropbear Direct-Start SSH Stability Fix - 2026-08-01
```text
Symptom after reboot:
- wlan0 was healthy:
  ssid wnk641_2.4G, wpa_state COMPLETED, IP 10.0.0.107.
- dropbear process existed and /proc/net/tcp showed port 22 listening, but SSH
  clients could still get banner/login instability after earlier restarts.
- Serial COM3 was used for diagnosis and recovery.

Finding:
- The previous S50dropbear used start-stop-daemon for startup. During serial
  recovery, `S50dropbear stop` returned OK, but the subsequent start path could
  appear to hang from the serial shell.
- Starting dropbear directly with explicit persistent host keys was stable:
  /usr/sbin/dropbear \
    -r /etc/dropbear/dropbear_rsa_host_key \
    -r /etc/dropbear/dropbear_ecdsa_host_key \
    -p 22 -P /var/run/dropbear.pid -E

Board fix applied:
- /etc/init.d/S50dropbear
- md5 24361d8259e0f31764fd4f12d16689a2
- New behavior:
  - Does not use start-stop-daemon for start.
  - Builds explicit `-r` key arguments from persistent host keys.
  - Falls back to `-R` only if no persistent key exists.
  - Removes stale pidfile / half-started dropbear before binding port 22.
  - Starts dropbear directly with `-p 22 -P /var/run/dropbear.pid -E`.
  - Verifies listening state through /proc/net/tcp and /proc/net/tcp6.
  - `status` reports whether port 22 is listening.

Verification:
- `/etc/init.d/S50dropbear restart` returned:
  Stopping dropbear sshd: OK
  Starting dropbear sshd: OK
- Reconnected over SSH immediately after restart.
- 8 consecutive Paramiko SSH handshakes/commands to 10.0.0.107 succeeded.
- wlan0 remained associated to wnk641_2.4G with IP 10.0.0.107.

Persistence note:
- Host Linux VM 192.168.175.138 was offline during this fix:
  ping timed out and TCP/22 timed out.
- Therefore the board fix is applied live, but the rootfs overlay copy still
  needs to be synced when the VM/repo is reachable:
  /home/wnk/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/etc/init.d/S50dropbear

Follow-up after host VM was powered on:
- Synced the live board S50dropbear into host overlay:
  /home/wnk/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/etc/init.d/S50dropbear
  md5 24361d8259e0f31764fd4f12d16689a2
- Ran:
  board_tools_f1c200s/runtime_bundle/sync_overlay_to_buildroot.sh
  board_tools_f1c200s/runtime_bundle/install_payload_to_target.sh
- Regenerated:
  /home/wnk/LicheePi_Nano/buildroot-2018.02.11/output/images/rootfs.tar
  md5 7ee08991bd8a68690c76d30495197245
- Verified in output/target and extracted rootfs.tar:
  /etc/init.d/S50dropbear
    md5 24361d8259e0f31764fd4f12d16689a2, mode 0755
  /root/aic_miracast/aic8800_register_wlan1.sh
    md5 1a669fff4a2f18da6e2b1797635f4afd, mode 0755
- Normal boot AIC rule rechecked:
  /etc/init.d/S17rtl8723bu contains no `aic8800` or `a69c` autoload text.
```

## AIC8800 Miracast Driver-Init Split - 2026-08-01
```text
Rule from user:
- AIC8800 driver initialization must only be done by:
  /root/aic_miracast/aic8800_register_wlan1.sh
- The Miracast/start script must not perform AIC module load/unload or USB
  8d80->8d83 initialization itself.

Cold reboot validation:
- Reboot state:
  wlan0 associated to wnk641_2.4G with IP 10.0.0.107.
  AIC USB was cold a69c:8d80.
  Only 8723bu was loaded; no aic_load_fw/aic8800_fdrv.
- Ran:
  /root/aic_miracast/aic8800_register_wlan1.sh start
- Result:
  8d80 detected; loading /lib/modules/5.7.1/extra/aic_load_fw.ko.
  a69c:8d83 ready.
  loaded /lib/modules/5.7.1/extra/aic8800_fdrv.ko.
  wlan1 registered, MAC dc:2e:97:94:eb:68.
  wlan0 remained COMPLETED with IP 10.0.0.107.

Miracast script patch:
- Board script:
  /root/aic_miracast/start_aic_miracast_cdump_board.sh
  md5 3afc4ff4e119df03db1356d216475e3a
- Backup before patch:
  /root/aic_miracast/backups/start_aic_miracast_cdump_board.sh.before_no_driver_init_20260801
  md5 073055c5668ae79c66fa59b7df26e64f
- New ensure_aic_iface behavior:
  If wlan1 exists, continue.
  If wlan1 is missing, log:
    ERROR: wlan1 not registered; run /root/aic_miracast/aic8800_register_wlan1.sh start first
  and return failure.
  It no longer insmods/rmmods any AIC module.

GO startup after split:
- Sequence:
  1. /root/aic_miracast/aic8800_register_wlan1.sh start
  2. ARCHIVE_DIR=/root/aic_runtime CAPTURE_MODE=null AIC_DBG_LEVEL=0
     LOWMEM_TUNE=1 STOP_GMENU=1 GO_FREQ=5805
     /root/aic_miracast/start_aic_miracast_cdump_board.sh start
- Result:
  START_RC=0.
  P2P GO ready on wlan1.
  SSID DIRECT-Yb, freq 5805, IP 192.168.49.1, passphrase 12345678.
  SSH remained available after GO startup.
  wlan0 remained associated to wnk641_2.4G with IP 10.0.0.107.

Host/rootfs persistence:
- Synced script into:
  /home/wnk/LicheePi_Nano/board_tools_f1c200s/runtime_bundle/rootfs_overlay/root/aic_miracast/start_aic_miracast_cdump_board.sh
  md5 3afc4ff4e119df03db1356d216475e3a
- Synced overlay, installed payload, and regenerated:
  /home/wnk/LicheePi_Nano/buildroot-2018.02.11/output/images/rootfs.tar
  md5 43a2769e29841be558dae30d4287e662
- Verified rootfs.tar contains:
  /root/aic_miracast/start_aic_miracast_cdump_board.sh
  md5 3afc4ff4e119df03db1356d216475e3a
```

## AIC8800 AP/STA Failure Isolation - 2026-08-01
```text
Purpose:
- Determine whether the SSH/banner/system hangs are caused by AIC firmware
  download, fdrv registration, P2P GO/AP mode, or ordinary STA operation.

Register script split:
- /root/aic_miracast/aic8800_register_wlan1.sh now has separate subcommands:
  loader: only perform a69c:8d80 -> a69c:8d83 firmware load.
  fdrv: only load aic8800_fdrv and wait for wlan1.
  start/register: existing full flow.
- md5:
  ed3f8d87c3052f8cf0dcff73893111b1

Miracast script monitor default change:
- /root/aic_miracast/start_aic_miracast_cdump_board.sh
- md5:
  f4a07a90e1680ab1a45ff0955a7aa6aa
- Defaults:
  START_MEM_WATCH=0
  START_DMESG_WATCH=0
- Driver-init rule remains:
  start_aic_miracast_cdump_board.sh does not initialize AIC; it requires wlan1
  and tells the user to run aic8800_register_wlan1.sh first.

Verified stable:
- Cold reboot state:
  AIC USB a69c:8d80, only 8723bu loaded, wlan0 connected to wnk641_2.4G
  at 10.0.0.107.
- `aic8800_register_wlan1.sh loader`:
  succeeded, AIC became a69c:8d83, only aic_load_fw loaded.
  SSH stayed stable through repeated probes.
- `aic8800_register_wlan1.sh fdrv`:
  succeeded, wlan1 registered, aic8800_fdrv loaded.
  SSH stayed stable through repeated probes.
- `ifconfig wlan1 up/down`:
  succeeded.
- `iwlist wlan1 scan`:
  succeeded and saw both 2.4GHz APs and 5GHz APs, including:
  wnk641_5G at BSSID 50:4F:3B:CC:67:53.

P2P GO / AP result:
- Minimal wpa_supplicant P2P test did not immediately kill the system, but
  `p2p_group_add` failed with:
  Failed to set beacon parameters
  Interface initialization failed
  Failed to initialize AP interface
- Running the fuller WFD/P2P sequence up to `p2p_group_add freq=5805` caused
  SSH banner hangs again.
- Conclusion:
  AIC P2P GO/AP/beacon setup path is unsafe on this current driver/firmware
  combination.

STA result:
- After P2P attempts, wlan1 could enter a dirty state where `ifconfig wlan1 up`
  returns:
  SIOCSIFFLAGS: Broken pipe
- Unloading/reloading only aic8800_fdrv while keeping 8d83 present restored
  wlan1 and scanning.
- Starting regular STA wpa_supplicant for `wnk641_5G` then caused control
  network loss / board reset. Serial later showed a fresh boot with uptime
  around 2 minutes, wlan1 absent, only 8723bu/dropbear/wlan0 active.
- Conclusion:
  AIC STA data/association path is also unsafe after current fdrv, even though
  registration and scanning are stable.

Current practical boundary:
- Safe:
  loader, fdrv registration, wlan1 up/down, scanning.
- Unsafe:
  P2P GO/AP beacon setup and STA association.
- Recovery after dirty AIC state:
  ifconfig wlan1 down
  rmmod aic8800_fdrv
  /root/aic_miracast/aic8800_register_wlan1.sh fdrv
- If association/P2P causes a reboot, normal RTL8723BU wlan0 SSH should return
  after boot.

Host/rootfs persistence:
- Synced to host overlay and regenerated:
  /home/wnk/LicheePi_Nano/buildroot-2018.02.11/output/images/rootfs.tar
  md5 3a82cb98ec058d47cdc9788e162c8279
- Verified output/target contains:
  /etc/init.d/S50dropbear
    md5 24361d8259e0f31764fd4f12d16689a2
  /root/aic_miracast/aic8800_register_wlan1.sh
    md5 ed3f8d87c3052f8cf0dcff73893111b1
  /root/aic_miracast/start_aic_miracast_cdump_board.sh
    md5 f4a07a90e1680ab1a45ff0955a7aa6aa
```

## AIC8800 v4 Miracast GO Restart Result - 2026-08-01
```text
Board reboot continuation:
- Board reachable over SSH at 10.0.0.107.
- wlan0 remained healthy:
  ssid wnk641_2.4G, bssid 50:4f:3b:cc:67:52, wpa_state COMPLETED,
  ip_address 10.0.0.107.

Initial AIC state after reboot:
- USB already showed a69c:8d83.
- aic_load_fw and aic8800_fdrv were loaded and wlan1 existed, but the first
  `ifconfig wlan1 up` failed with:
    SIOCSIFFLAGS: Broken pipe
- No /etc/init.d or /root script reference was found that auto-loads AIC.
  This likely came from warm-reboot/stage2 state or prior manual test state.

Recovery:
- Kept USB in a69c:8d83 stage.
- Ran:
  ifconfig wlan1 down
  rmmod aic8800_fdrv
  /root/aic_miracast/aic8800_register_wlan1.sh fdrv
- Result:
  wlan1 registered again and `ifconfig wlan1 up` plus `iwlist wlan1 scan`
  worked. wlan0 stayed connected.

v4 candidate test:
- Candidate:
  /root/aic_miracast/candidates/aic8800_rx_drop_invalid_fix_20260726_4
- md5:
  aic_load_fw.ko  e965d7aa9eca49b28dc8004acb859083
  aic8800_fdrv.ko ca28067466c0126715c863ea26b234e3
- Switch command:
  ifconfig wlan1 down
  rmmod aic8800_fdrv
  rmmod aic_load_fw
  LOAD_FW=/root/aic_miracast/candidates/aic8800_rx_drop_invalid_fix_20260726_4/aic_load_fw.ko \
  FDRV=/root/aic_miracast/candidates/aic8800_rx_drop_invalid_fix_20260726_4/aic8800_fdrv.ko \
  /root/aic_miracast/aic8800_register_wlan1.sh fdrv
- Result:
  wlan1 registered, `ifconfig wlan1 up` worked, scan worked, and scan saw
  2.4GHz/5GHz APs.

Miracast GO with v4:
- Command:
  ARCHIVE_DIR=/root/aic_runtime CAPTURE_MODE=null AIC_DBG_LEVEL=0 \
  LOWMEM_TUNE=1 STOP_GMENU=1 GO_FREQ=5805 START_MEM_WATCH=0 \
  START_DMESG_WATCH=0 \
  /root/aic_miracast/start_aic_miracast_cdump_board.sh start
- Result:
  START_RC=0.
  P2P GO ready on wlan1:
    display F1C200S-AIC
    SSID DIRECT-X1
    freq 5805
    IP 192.168.49.1
    passphrase 12345678
- Runtime processes include:
  wpa24_aic_wfd_supplicant, tiny_dhcpd_49, watch_aic_miracast.sh.
- wlan0 stayed associated to wnk641_2.4G with IP 10.0.0.107 after GO startup.

Next step:
- Use phone/receiver to search for Miracast device F1C200S-AIC / DIRECT-X1.
- If connection/disconnect occurs, collect:
  /root/aic_runtime/logs/aic_miracast_board.log
  /root/aic_runtime/run/watch.log
  /root/aic_runtime/run/cdump.log
  dmesg grep for aic/wlan1/OOM/SLUB/GFP.

Follow-up when phone did not show a cast target:
- GO stayed up, but the phone did not list F1C200S-AIC as a Miracast device.
- `p2p_peer` showed the phone was at least probing the board:
  a6:00:33:22:26:9e, device_name=Redmi K60, flags=[PROBE_REQ_ONLY],
  wfd_subelems=00000600101c440032.
- This means RF/P2P probe reception works, but WFD service discovery /
  invitation visibility is not correct enough for Android's cast UI.
- Unsafe command found:
  `wpa_cli -p /tmp/aic_wpa_ctrl -i wlan1 p2p_find 30`
  After this, `p2p_peers` timed out, `p2p_listen` returned connection refused,
  SSH command execution hung, and serial recovery showed stuck
  `start_aic_miracast... stop` processes. `rmmod aic8800_fdrv` also blocked.
- Do not use `p2p_find` on the current AIC driver/firmware path.
- Recovery from this state required hard reset/power cycle; serial Ctrl-C,
  `reboot -f`, and serial BREAK+b did not recover the shell.

Next safer direction:
- After reboot, do not start with autonomous GO plus `p2p_find`.
- Try a phone-initiated listen/provisioning flow without running `p2p_find`,
  or test a different candidate/firmware where P2P discovery commands do not
  wedge the AIC control path.

Sink/listen correction attempt:
- User clarified the board must behave as a Miracast sink/client side, not as
  an autonomous P2P GO/AP. Android cast discovery will not necessarily show a
  device that has already forced itself into GO mode with `p2p_group_add`.
- Board script was replaced live:
  /root/aic_miracast/start_aic_miracast_cdump_board.sh
  md5 b17dd46c2f71a5d6a136752627b96af2
- New default behavior:
  ROLE=sink.
  Does not run `p2p_group_add`.
  Does not start tiny_dhcpd.
  Does not run `p2p_find`.
  Starts wpa_supplicant with WFD subelements, then runs short
  `p2p_listen "$LISTEN_SEC"` and watches for phone PBC requests.
  On PBC request it attempts:
    p2p_connect <peer> pbc join
    wps_pbc <peer>
  If a client group forms, it runs udhcpc on wlan1 and starts
  miracast_sink_dump toward the phone GO address.

v4 sink/listen result:
- v4 fdrv registered and scanned normally.
- Starting sink/listen with single `p2p_listen 0` did not immediately crash but
  no phone peer was observed during about one minute.
- Manually running:
  p2p_listen 120
  caused SSH banner/exec failure and then board reboot. After reboot only
  8723bu remained loaded and wlan0 returned normally.
- Conclusion:
  v4 is not safe for P2P listen/discovery.

v3 sink/listen result:
- Candidate:
  /root/aic_miracast/candidates/aic8800_rx_budget_fix_20260726_3
- md5:
  aic_load_fw.ko  64a850c62754c3585fa3b0800ca77a7a
  aic8800_fdrv.ko 68ed482cd56d53d1640d68c00d2eed8a
- v3 fdrv registered, `ifconfig wlan1 up`, and `iwlist wlan1 scan` worked.
- Starting sink/listen with `LISTEN_SEC=10` returned normally and wlan0 stayed
  connected.
- Repeated short listen rearm attempts later caused SSH command timeout and
  board reboot. After reboot only 8723bu remained loaded and wlan0 returned
  normally.
- Conclusion:
  v3 is also not safe for repeated P2P listen/discovery.

Current boundary:
- AIC registration and scan remain usable on v3/v4.
- GO mode can start on v4, but Android did not show it as a cast target.
- Sink/client mode needs P2P listen/provision discovery, and this command path
  reboots or wedges the board on tested candidates.
- Do not run `p2p_find`; avoid repeated/long `p2p_listen` until the AIC P2P
  command path is fixed or a different driver/firmware pair is found.

Additional sink/listen candidate sweep:
- v15 candidate:
  /root/aic_miracast/candidates/aic8800_rx_keep_small_ctrl_20260727_15
  md5:
    aic_load_fw.ko  a7a3a3509c79d8c3a46ea11e86fe2cb1
    aic8800_fdrv.ko 5ca7b5d055604f6de96b0e0075f01f22
  Result:
    wlan1 registered, up/scan worked.
    Single LISTEN_SEC=10 sink/listen stayed alive.
    Single LISTEN_SEC=30 caused SSH banner failure and reboot.

- v16 candidate:
  /root/aic_miracast/candidates/aic8800_rx_keep_ctrl768_20260727_16
  md5:
    aic_load_fw.ko  f957706f28a1f27c2e942bcc0645a689
    aic8800_fdrv.ko cc4fc263b0912b98298f6ab7ccccd920
  Result:
    fdrv probe failed; wlan1 did not register.
    dmesg showed:
      usb_err:<aicwf_usb_probe,2549>: failed with errno -1
      aic8800_fdrv: probe ... failed with error -1

- v17 candidate:
  /root/aic_miracast/candidates/aic8800_rx_lowmem_threshold_20260727_17
  md5:
    aic_load_fw.ko  90f90b748f0080d8ff2df917c0e1286c
    aic8800_fdrv.ko 6a4ebce9a195785b158715ebd15222d7
  Result:
    fdrv probe failed; wlan1 did not register.
    Same aicwf_usb_probe errno -1 / fdrv probe failure pattern as v16.

- hostgood candidate:
  /root/aic_miracast/candidates/aic8800_hostgood_8d83_board_repro_20260725_092535
  md5:
    aic_load_fw.ko  48e6d88aa79518428238eee32341bc29
    aic8800_fdrv.ko ed39b51b13e1105650b089a59f8dab59
  Result:
    wlan1 registered, up/scan worked.
    Single LISTEN_SEC=10 stayed alive.
    Single LISTEN_SEC=30 stayed alive.
    LISTEN_SEC=60 caused SSH/banner failure and reboot.

- lowmem candidate:
  /root/aic_miracast/candidates/aic8800_lowmem_candidate_20260725_100811
  md5:
    aic_load_fw.ko  2f5e6c10c6fd58688a7d7782ac8dc6c3
    aic8800_fdrv.ko 8aa9a113125596651b37a1a31879415a
  Result:
    wlan1 registered, up/scan worked.
    Single LISTEN_SEC=30 stayed alive.
    Single LISTEN_SEC=60 stayed alive.
    Running a second consecutive LISTEN_SEC=60 caused SSH/banner failure and
    reboot.

Best current sink/listen candidate:
- `aic8800_lowmem_candidate_20260725_100811`.
- Use one explicit LISTEN_SEC=60 window for phone search.
- Do not auto-loop/rearm listen. Wait for user to start phone search, then arm
  exactly one 60-second listen window and observe logs.

Lowmem one-shot listen after reboot:
- Reboot state:
  AIC cold a69c:8d80, only 8723bu loaded, wlan0 connected to wnk641_2.4G at
  10.0.0.107.
- Used /lib loader for 8d80 -> 8d83, then loaded lowmem candidate fdrv:
  /root/aic_miracast/candidates/aic8800_lowmem_candidate_20260725_100811
- Started:
  LISTEN_SEC=60 CAPTURE_MODE=null \
  /root/aic_miracast/start_aic_miracast_cdump_board.sh start
- Result:
  Script entered sink/listen mode:
    no GO, no p2p_find, no local DHCP server.
    wpa_state=DISCONNECTED with p2p_device_address dc:2e:97:94:eb:68.
  12 polls over about 60 seconds showed no P2P peer, no PBC request, and no
  group formation.
  Board stayed stable after the 60-second window:
    wlan0 remained connected at 10.0.0.107.
    aic8800_fdrv and aic_load_fw remained loaded.
- Interpretation:
  Lowmem can hold a single 60-second listen window, but the phone still did not
  discover/probe the sink in this run. Contrast with previous GO mode where
  Redmi K60 appeared as PROBE_REQ_ONLY. Next debug should focus on sink listen
  channel/advertised WFD/P2P IE rather than generic RF registration.

5GHz-only listen sweep:
- User required 5GHz Wi-Fi Direct/Miracast discovery, not 2.4GHz.
- Script was patched live to make sink listen channel configurable:
  /root/aic_miracast/start_aic_miracast_cdump_board.sh
  md5 35445e435dab93a110ed78e932cd501e
- New default variables:
  LISTEN_REG_CLASS=${LISTEN_REG_CLASS:-124}
  LISTEN_CHANNEL=${LISTEN_CHANNEL:-161}
- Confirmed config output for 5GHz channel 161:
  p2p_listen_reg_class=124
  p2p_listen_channel=161
- Runtime `p2p_set listen_channel 161` and `p2p_set oper_channel 161`
  returned FAIL on this wpa_supplicant build. `p2p_set disallow_freq
  2412-2472` returned OK, but did not make the phone discover the device.

5GHz channel test results with lowmem candidate:
- 161:
  LISTEN_REG_CLASS=124 LISTEN_CHANNEL=161 LISTEN_SEC=60.
  Started successfully and stayed stable, but no peer/PBC event was observed.
- 149:
  LISTEN_REG_CLASS=124 LISTEN_CHANNEL=149 LISTEN_SEC=60.
  Started successfully and stayed stable, but no peer/PBC event was observed.
- 153:
  LISTEN_REG_CLASS=124 LISTEN_CHANNEL=153 LISTEN_SEC=60.
  Started successfully and stayed stable, but no peer/PBC event was observed.
- 157:
  LISTEN_REG_CLASS=124 LISTEN_CHANNEL=157 LISTEN_SEC=60.
  wpa_supplicant failed during start.

Current 5GHz conclusion:
- The lowmem driver can keep wlan1 registered and can hold 5GHz sink/listen
  windows on 149/153/161.
- The phone still does not show a Wi-Fi Direct or Miracast device, and the
  board sees no P2P peer/PBC request during those 5GHz windows.
- This points to AIC/wpa_supplicant not actually advertising a discoverable P2P
  device/WFD sink in listen mode, not merely a wrong 5GHz channel choice.

Follow-up after reboot, 2026-08-01:
- Board rebooted and returned on SSH at 10.0.0.107.
- Initial state:
  wlan0 associated to wnk641_2.4G, IP 10.0.0.107.
  AIC USB was already a69c:8d83, with only 8723bu loaded.
- Board script had reverted to an older GO/AP version:
  /root/aic_miracast/start_aic_miracast_cdump_board.sh
  md5 006da20f346204cc50b8fc3e175f833b
  It still contained p2p_group_add and 2.4GHz listen defaults.
- Reapplied a narrow sink/listen-only script on the board:
  /root/aic_miracast/start_aic_miracast_cdump_board.sh
  md5 d81c1e71c68b9c60f164b7f6dcc7c37d
  Behavior:
    no p2p_find
    no p2p_group_add
    no local DHCP server
    no AIC driver initialization
    default LISTEN_REG_CLASS=124, LISTEN_CHANNEL=149
    default OPER_REG_CLASS=124, OPER_CHANNEL=149
    default LISTEN_SEC=60
- Loaded the known lowmem AIC pair:
  /root/aic_miracast/candidates/aic8800_lowmem_candidate_20260725_100811
  aic_load_fw.ko md5 2f5e6c10c6fd58688a7d7782ac8dc6c3
  aic8800_fdrv.ko md5 8aa9a113125596651b37a1a31879415a
- 5GHz one-shot sink/listen tests:
  124/149 for 60 seconds: stable, no P2P peer.
  124/153 for 60 seconds: stable, no P2P peer.
  124/161 for 60 seconds: stable, no P2P peer.
  wlan0 stayed connected at 10.0.0.107 after each test.
- Monitor-frame investigation:
  `iw dev wlan1 interface add mon1 type monitor` failed with -22.
  Kernel log:
    ieee80211 phy3: Monitor+Data interface support (MON_DATA) disabled
  Source check:
    third_party/aic8800_rx_lowmem_fix_20260726/aic8800_fdrv/Makefile
    has CONFIG_RWNX_MON_DATA =n.
    rwnx_rx.c emits:
      "Received monitor frame but there is no monitor interface open"
    when firmware marks received management frames as monitor_vif but no
    monitor_vif exists.
  Interpretation:
    The current driver cannot add a separate monitor interface alongside wlan1.
    This matches the earlier serial spam and likely explains why P2P discovery
    frames are not reaching wpa_supplicant in sink/listen mode.
- MON_DATA test build:
  Built a new host candidate from:
    third_party/aic8800_rx_lowmem_fix_20260726
  with:
    CONFIG_RWNX_MON_DATA =y
  using:
    export PATH=/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin:$PATH
    make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- KDIR=/home/wnk/LicheePi_Nano/linux -j4
  Output:
    third_party/aic8800_rx_lowmem_mondat_20260801/aic_load_fw/aic_load_fw.ko
      md5 a8b8dc6bf8213a0a332767c00f87646c
    third_party/aic8800_rx_lowmem_mondat_20260801/aic8800_fdrv/aic8800_fdrv.ko
      md5 bc398a99261203e14a31a1d228423767
- MON_DATA runtime result:
  Loading the newly built aic_load_fw.ko caused the SSH session to close and
  the board rebooted.
  After reboot, wlan0 recovered normally.
  Loading the new MON_DATA fdrv with the known lowmem aic_load_fw.ko failed:
    Unknown symbol aicwf_rxbuff_size_get
    Unknown symbol aicwf_prealloc_rxbuff_alloc
    Unknown symbol aicwf_prealloc_rxbuff_free
  Therefore this new MON_DATA fdrv is not ABI-compatible with the board's
  known lowmem loader.
- Host search result:
  The host repo does not currently contain source or built modules matching
  the board lowmem MD5 pair:
    2f5e6c10c6fd58688a7d7782ac8dc6c3
    8aa9a113125596651b37a1a31879415a
  Do not mix fdrv/load_fw from different AIC candidates.
- Final board state after recovery:
  Re-loaded the known lowmem pair.
  wlan1 registered as dc:2e:97:94:eb:68.
  wlan0 remained associated to wnk641_2.4G at 10.0.0.107.
  AIC modules loaded:
    aic_load_fw, aic8800_fdrv
```

## AIC USB Missing After Reboot / Soft Reset Limit - 2026-08-01
```text
Continuation after user reboot:
- Board was reachable by SSH at 10.0.0.107.
- wlan0 was healthy:
  ssid wnk641_2.4G, wpa_state COMPLETED, ip_address 10.0.0.107.
- AIC USB was absent:
  lsusb showed only:
    1d6b:0002 root hub
    214b:7250 USB hub
    0bda:b720 RTL8723BU
  No a69c:8d80 or a69c:8d83.
- Do not load AIC modules in this state. The problem is below the driver
  registration layer.

Recovery attempts:
- Ran /root/reset_musb_restore_8723.sh.
- MUSB rebind did not restore AIC. dmesg showed the downstream AIC port
  failing descriptor/address setup:
    usb 1-1.3: device descriptor read/64, error -110
    usb 1-1.3: device not accepting address, error -110
    usb 1-1-port3: unable to enumerate USB device
- Tried resetting the enumerated hub with /root/usbreset /dev/bus/usb/001/002.
  This also did not restore AIC.

Control-network recovery note:
- MUSB/hub reset can leave RTL8723BU half-initialized with MAC:
    00:E0:4C:87:23:00
  and ifconfig/wpa_supplicant failing with:
    SIOCSIFFLAGS: Operation not permitted
- Waiting for the USB/BT timeout churn to settle, then unloading/reloading
  8723bu with btusb unbound restored the normal MAC:
    14:0A:02:2F:DC:7A
  and wlan0 was set back to static 10.0.0.107.
- Final state:
  SSH works at 10.0.0.107.
  wlan0 is associated to wnk641_2.4G.
  AIC USB is still absent from lsusb.

Conclusion:
- If AIC disappears from lsusb, MUSB soft reset and hub reset are not enough
  on this hardware state. Continue Miracast/AIC testing only after a real
  AIC power reset / board hard power cycle makes a69c:8d80 or a69c:8d83 appear.
```

## AIC8800 Lowmem Sink Listen Reboot Continuation - 2026-08-01
```text
After user hard power cycle:
- Board returned on SSH at 10.0.0.107.
- wlan0 was healthy on wnk641_2.4G.
- AIC cold USB state was present as a69c:8d80.

Correct recovery/start sequence used:
1. Do not use lowmem aic_load_fw for cold 8d80 firmware download.
   Attempting that over SSH earlier caused a board reboot.
2. Use serial COM3 to run the current /lib baseline loader:
   /root/aic_miracast/aic8800_register_wlan1.sh loader
   Result:
   - firmware download completed
   - AIC re-enumerated as a69c:8d83
   - aic_load_fw loaded from /lib/modules/5.7.1/extra/aic_load_fw.ko
3. After 8d83, switch to the lowmem candidate modules for exported symbols and fdrv:
   C=/root/aic_miracast/candidates/aic8800_lowmem_candidate_20260725_100811
   rmmod aic8800_fdrv
   rmmod aic_load_fw
   insmod $C/aic_load_fw.ko aic_fw_path=/lib/firmware/aic8800D80 aicwf_dbg_level=0
   insmod $C/aic8800_fdrv.ko aicwf_dbg_level=0
   Result:
   - wlan1 registered
   - MAC dc:2e:97:94:eb:68
   - AIC stayed at a69c:8d83
   - wlan0/SSH stayed up

Negative check:
- Loading lowmem aic8800_fdrv directly while the /lib loader is still inserted fails:
  Unknown symbol aicwf_prealloc_txq_alloc
- Therefore after 8d83, lowmem aic_load_fw.ko must be inserted before lowmem fdrv.

One-shot sink/listen test:
- Command:
  LISTEN_REG_CLASS=124 LISTEN_CHANNEL=149 OPER_REG_CLASS=124 OPER_CHANNEL=149 \
  LISTEN_SEC=60 CAPTURE_MODE=null LOWMEM_TUNE=1 STOP_GMENU=1 \
  /root/aic_miracast/start_aic_miracast_cdump_board.sh start
- Result:
  - script stayed in sink/listen-only mode
  - no p2p_find
  - no p2p_group_add
  - no local DHCP server
  - p2p_listen 60 returned OK
  - final status: wpa_state=DISCONNECTED, p2p_device_address=dc:2e:97:94:eb:68
  - peers list was empty
  - wlan1 RX/TX stayed 0
  - wlan0/SSH remained stable

Conclusion:
- Driver/USB registration is recovered.
- 5GHz 149 listen is stable, but the phone still did not discover/probe this sink in that window.
- Avoid repeated listen loops; use one-shot windows only unless actively observing with the phone.
```

## AIC8800 Miracast GO Chain Reconfirmed - 2026-08-02
```text
User reported that the phone could see and connect to the cast device again.
This reconfirms the earlier known-good direction:
- Use the host backup / board script chain based on P2P GO mode, not the later
  sink/listen-only experiments.
- Do not use p2p_find, repeated p2p_listen, or autonomous sink/listen loops on
  the current AIC driver path.

Board access/state during confirmation:
- Board SSH: root@10.0.0.107, password 1.
- wlan0 control network remained up.
- AIC state: a69c:8d83, wlan1 MAC dc:2e:97:94:eb:68.
- Script on board:
  /root/aic_miracast/start_aic_miracast_cdump_board.sh
  patched md5 b104bd18e9ecc65e7f2865156e7ec71d.
- Original restored known-good script was backed up on board before patching:
  md5 073055c5668ae79c66fa59b7df26e64f.

Module/script chain used:
- v14 module pair:
  /root/aic_miracast/candidates/aic8800_rx_data_pressure_guard_20260726_14/aic_load_fw.ko
    md5 a762b1674cc4a55b727256fa542ac16f
  /root/aic_miracast/candidates/aic8800_rx_data_pressure_guard_20260726_14/aic8800_fdrv.ko
    md5 8259aa2ba4008452d75f26021e03969a
- Start command:
  ARCHIVE_DIR=/root/aic_runtime CAPTURE_MODE=null AIC_DBG_LEVEL=0 \
  LOWMEM_TUNE=1 STOP_GMENU=1 \
  LOAD_FW=/root/aic_miracast/candidates/aic8800_rx_data_pressure_guard_20260726_14/aic_load_fw.ko \
  FDRV=/root/aic_miracast/candidates/aic8800_rx_data_pressure_guard_20260726_14/aic8800_fdrv.ko \
  /root/aic_miracast/start_aic_miracast_cdump_board.sh start

Working GO result:
- Display name: F1C200S-AIC.
- SSID during successful run: DIRECT-Yf.
- Frequency: 5805 MHz.
- Mode: P2P GO.
- Board IP on wlan1: 192.168.49.1.
- Passphrase: 12345678.

Successful phone sequence observed with Redmi K60:
- P2P-DEVICE-FOUND a6:00:33:22:26:9e name='Redmi K60'.
- P2P-PROV-DISC-PBC-REQ received.
- watcher rearmed WPS immediately on pbc_req.
- WPS-REG-SUCCESS and WPS-SUCCESS.
- AP-STA-CONNECTED a6:00:33:22:26:9e.
- DHCP REQUEST for 192.168.49.52.
- Static ARP entry was installed:
  192.168.49.52 -> a6:00:33:22:26:9e on wlan1.

Patch applied to watcher:
- Start cdump only on DHCP REQUEST/ACK instead of DISCOVER.
- Parse the phone MAC from DHCP logs.
- Install a static ARP entry for phone IP/MAC.
- Wait 2 seconds before launching miracast_sink_dump.
- This fixed the prior immediate failure:
  connect 192.168.49.52:7236 failed: No route to host.

RTSP/RTP result after patch:
- miracast_sink_dump started:
  /root/aic_miracast/miracast_sink_dump 192.168.49.52 /dev/null
- RTSP negotiation reached:
  SETUP, PLAY, and "---- Negotiation successful ----".
- RTP data was received:
  about 1278 RTP packets / 1203496 bytes in this run.
- Current remaining issue:
  RTP later stalled after about 1.2 MiB:
  "RTP packet counter stalled, keep RTSP session alive".

Practical conclusion:
- Wi-Fi Direct discovery and Miracast RTSP negotiation are working again when
  following the GO + PBC watcher backup flow.
- The remaining problem is no longer device discovery; it is RTP stream
  stability after negotiation.
```
## AIC Miracast v18 Stable Record - 2026-08-02
```text
Dedicated docs:
- AIC_MIRACAST_INDEX.md
- AIC_MIRACAST_V18_STABLE_20260802.md

Backup copy:
- /home/wnk/F1C200S_host_archive/miracast_docs/20260802_v18/

Current best AIC Miracast candidate:
- Board:
  /root/aic_miracast/candidates/aic8800_rx_lowmem_threshold1024_20260802_v18
- Host source:
  /home/wnk/LicheePi_Nano/third_party/aic8800_rx_data_pressure_guard_20260802_v18

Module md5:
- aic_load_fw.ko  698b92823499cb1843fe9ab64a2e7df0
- aic8800_fdrv.ko 0517fde23a9c0f68ae99203c10a0bedb

Key fix:
- v18 lowers aicwf_rx_lowmem() guard in aic8800_fdrv/aicwf_txrxif.c:
  from freeram < 5200 to freeram < 1024.
- This avoids the previous premature "drop rx data under skb pressure" RTP
  stall around 18-20 MB available memory on the 64 MB board.

Confirmed run:
- GO + PBC watcher, 5GHz 5805, display F1C200S-AIC.
- RTSP negotiated successfully.
- RTP final stats:
  rtp_packets=151132 rtp_bytes=160175760
- Session ended by phone-side TEARDOWN after the phone powered off, not by RTP
  stall.

Rules:
- AIC8800 remains script/manual only; do not autoload at normal boot.
- Do not use p2p_find.
- Do not switch this working chain to sink/listen-only mode.
- Do not mix aic_load_fw.ko and aic8800_fdrv.ko from different candidates.

Two-script startup now installed on board:
- /root/aic_miracast/register_aic_wlan1_v18.sh start
- /root/aic_miracast/start_miracast_go_v18.sh start

Script backup/source paths:
- /home/wnk/LicheePi_Nano/board_tools_f1c200s/aic_miracast/
- /home/wnk/F1C200S_host_archive/miracast_docs/20260802_v18/scripts/

Behavior:
- register_aic_wlan1_v18.sh only registers wlan1. For cold a69c:8d80 it first
  uses the baseline /lib aic_load_fw.ko to download firmware to 8d83, then
  loads the v18 module pair.
- start_miracast_go_v18.sh refuses to register drivers. It requires wlan1 and
  starts only the GO + PBC watcher Miracast flow.
- start_miracast_go_v18.sh records H.264 by default to /root/roms/video.
  The underlying watcher writes miracast_YYYYMMDD_HHMMSS.h264 when
  CAPTURE_MODE is not null. Use CAPTURE_MODE=null for no-record debug runs.
```
