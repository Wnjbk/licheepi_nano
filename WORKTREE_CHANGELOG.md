# Worktree Change Log

Worktree: /home/wnk/LicheePi_Nano/linux_musb_clean_ep1_20260811
Component: Linux kernel MUSB host driver
Base: source-reproducible SII9022 #236, commit 6d24e044a3cae9175e96d6d4688938992f079738
Target release: 5.7.1, runtime candidate #237
Purpose: keep F1C200S USB FIFO allocation inside 2 KiB and verify RTL8723BU Wi-Fi/Bluetooth coexistence.

## 2026-09-15 / accepted evidence / audit and bounded FIFO allocation
Hypothesis: the generic Sunxi table allocated FIFO addresses beyond F1C200S 2 KiB USB SRAM, causing endpoint aliasing.
Files: drivers/usb/musb/sunxi.c, drivers/usb/musb/musb_core.c
Commit: 1aad258, 9aab5c8, ca8a380, cd287e2
Build: documented Linaro top-level make with LOCALVERSION= and .version 236; release 5.7.1.
Deploy and rollback: board backups under /root/roms/kernel_backups/usb_fifo_audit_237_20260915 and /root/roms/kernel_backups/usb_fifo_fix_237_20260915.
Tests: boot log proved the original static FIFO offsets exceeded 2 KiB; bounded initial layout booted, associated wlan0, and completed a 4.9 MiB SCP transfer with matching MD5.
Decision: accepted as evidence. Initial bounded layout was not sufficient for HCI because its EP3 FIFO was 256 bytes.

## 2026-09-15 / rejected / broad RTL endpoint diagnostic
Hypothesis: record RTL endpoint-to-MUSB allocation before changing scheduling.
Files: drivers/usb/musb/musb_host.c
Commit: 716bc4c
Build: zImage 9e4281dbfa60018da27d6295bfde1943, release 5.7.1.
Deploy and rollback: old zImage e3081e59be6f9955a37eeec5d7208156 saved at /root/roms/kernel_backups/usb_fifo_endpoint_map_237_20260915/zImage.before.
Tests: the log included EP0 control URBs and flooded the console, preventing WLAN startup.
Decision: rejected. Board was restored through its saved zImage. Follow-up commits 63ed3df and a57eb52 restricted logging to RTL non-control IN endpoints.

## 2026-09-15 / pending candidate / 512-byte ACL receive FIFO
Hypothesis: allocate a 512-byte EP3 shared FIFO so a 512-byte RTL ACL IN URB has a direct non-mux hardware endpoint after Wi-Fi consumes EP2.
Files: drivers/usb/musb/sunxi.c; diagnostic-only drivers/usb/musb/musb_host.c
Commit: 9f8c01fe11e73b5b885237861303e3a87fcb99af
Build: make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- -j8 LOCALVERSION= with .version 236; zImage f38eda1a9de749d3fdaa3bfa0cf2becd; sunxi.ko 1434316e72399f410beb6490be897f4d; vermagic 5.7.1 mod_unload ARMv5 p2v8.
Deploy and rollback: candidate archive /home/wnk/F1C200S_host_archive/kernel_usb_fifo_acl_ep3_237_20260915; board backup /root/roms/kernel_backups/usb_fifo_acl_ep3_237_20260915 with prior zImage 952afe16475d07548e675afa91c849f1 and prior sunxi.ko 32914592dbc35e210dcfdeece483fe56.
Tests: active FIFO layout is EP1 TX 512/RX 64, EP2 shared 512, EP3 shared 512, total 1664 bytes including EP0. wlan0 associated. HCI initialized with RTL firmware, BLE discovery found Wt-070, and the final mapping during connection was HCI interrupt USB ep1 to MUSB EP1 plus ACL bulk USB ep2 to MUSB EP3.
Decision: pending baseline acceptance. Three physical cold boots and AIC8800/Miracast regression remain required.

## 2026-09-16 / pending runtime candidate / BlueALSA A2DP on Wt-070
Hypothesis: after the bounded FIFO fixes, real A2DP can coexist with WLAN when BlueALSA runtime and D-Bus policy are present.
Files: no kernel source change. Board runtime candidate: /tmp/bluealsa/runtime and /etc/dbus-1/system.d/bluealsa.conf.
Commit: runtime-only; source record is C:/Users/26301/Desktop/F1C200S_A2DP_HANDOFF.md.
Build: BlueALSA runtime from /home/wnk/LicheePi_Nano/third_party/bluealsa-f1c200s/bluealsa-f1c200s-runtime.tar.gz.
Deploy and rollback: runtime is under /tmp and disappears at reboot. D-Bus policy is a candidate persistent rootfs change and is not integrated into an image. BlueALSA ALSA plugin was copied only for the playback test and removed afterward.
Tests: Wt-070 12:11:71:41:9C:4A discovered, paired, trusted, connected, and services-resolved. BlueALSA PCM is SBC, 2 channels, 48000 Hz. speaker-test completed Front Left and Front Right through A2DP. Concurrent wlan0 ping was 15/15 with 0 percent packet loss. HCI recorded zero errors.
Decision: pending. Confirm actual audible playback with the user, test sustained audio under WLAN traffic, complete three physical cold boots, and regress AIC8800 Miracast before promoting #237 to a baseline.

## 2026-09-16 / pending regression / post-reboot BlueALSA audio
Hypothesis: the #237 bounded FIFO layout can re-establish real RTL8723BU A2DP after a fresh board boot while wlan0 is active.
Files: no source change. Temporary board runtime only: /tmp/btrtl.ko, /tmp/btbcm.ko, /tmp/btintel.ko, /tmp/btusb.ko, and /tmp/bluealsa/runtime.
Commit: 83db67f06d13930e1e1dc7d3bcd72013ba3db644 remains the active source record.
Build: no build.
Deploy and rollback: temporary modules and BlueALSA runtime transferred by SCP; existing /etc/dbus-1/system.d/bluealsa.conf policy and persisted Wt-070 pairing were reused. ALSA config and temporary BlueALSA plugin were restored after the audio test.
Tests: wlan0 associated; Wt-070 discovered at RSSI -58, connected, paired, trusted, and services-resolved. BlueALSA returned an SBC 48000 Hz stereo PCM. speaker-test completed Front Left and Front Right. HCI ACL counters reached 36 RX and 36 TX with zero errors.
Decision: pending. This is a fresh boot-session regression, but physical power removal was not independently observed. Keep it separate from the required three physical cold-boot acceptance cycles and still run AIC8800/Miracast regression.

## 2026-09-16 / pass / Cedar hard decode with Wt-070 audio
Hypothesis: Cedar hard video decode can send its audio stream through the active BlueALSA default ALSA device without disrupting WLAN or HCI.
Files: no source change. Runtime-only wrapper configured /etc/asound.conf and /usr/lib/alsa-lib/libasound_module_pcm_bluealsa.so for the player lifetime; video file /root/roms/video/bad.mp4.
Commit: a9417b39e2eb4a5bf0c4a9d6857260ebf3879c4d is the prior worktree-log record.
Build: no build.
Deploy and rollback: Cedar wrapper restores the prior ALSA config and removes the temporary plugin when it exits. BlueALSA remains under /tmp for this boot session.
Tests: /root/cedar_drm_player hard-decoded bad.mp4 and reported playback started, H.264 plugin registration, AAC audio codec 44100 Hz two channels, and audio output default. HCI ACL counters increased to RX 41 and TX 272 with zero errors. User confirmed audible audio from Wt-070.
Decision: pass for this boot session. Physical cold-boot count and AIC/Miracast regression remain pending before baseline promotion.

## 2026-09-17 / planned / AIC8800 nonshared FIFO RX priority
Hypothesis: the current EP2/EP3 512-byte FIFO_RXTX configuration permits an IN QH and OUT QH to overwrite each other on the same shared endpoint during AIC 5 GHz P2P traffic. Use only independent directions within the 2 KiB SRAM: EP1 TX512/RX512, EP2 RX512, EP3 RX256, including EP0 total 1856 bytes. Route AIC (a69c) bulk IN to idle EP1 RX; generic RTL Wi-Fi bulk IN continues to use EP2 RX; bulk OUT uses MUSB's same-direction mux on EP1.
Files: drivers/usb/musb/sunxi.c; drivers/usb/musb/musb_host.c.
Commit: pending.
Build: exact documented top-level zImage command with LOCALVERSION=; current output files are stale from rejected four-direction FIFO candidate and will be regenerated by dependency-aware make.
Deploy and rollback: none while board offline. Preserve #237 board zImage and sunxi.ko before any later deployment.
Tests: board offline; no runtime test.
Decision: pending.

## 2026-09-18 / planned / TK032F8004 panel graph enable
Hypothesis: the #237 DTS leaves the DPI panel and its LG4573A GPIO initializer
disabled while routing TCON0 to SII9022, so the built-in sun4i DRM driver has
no enabled panel connector to bind. Select the external TK032F8004 panel.
Files: arch/arm/boot/dts/suniv-f1c100s-licheepi-nano.dts.
Build: documented DTS-only target with the documented Linaro toolchain.
Deploy and rollback: backup the board DTB before any replacement; restoring it
returns the SII9022 display route. No zImage or module change is planned.
Tests: DTB syntax/decompile, cold boot, tk032 init probe, DRM card0/fb0, then
panel illumination. USB, wlan0, and AIC remain protected regressions.
Decision: pending.
