# F1C200S Stable Source Snapshot - 2026-08-14

This snapshot records the known F1C200S stable source inputs without copying
kernel build products, module binaries, firmware blobs, or experimental work.

## Kernel Baseline

- Linux source: `linux_musb_clean_ep1_20260811`
- Commit: `47919e725c0900212cf85d8387b36d50db13adef`
- Subject: `musb_pin_rtl8723bu_bt_acl_to_ep3`
- The patch reserves MUSB EP3 only for the RTL8723BU Bluetooth ACL bulk-IN
  endpoint. It does not force the HCI interrupt endpoint to EP4.
- Build with the documented kernel toolchain:
  `/opt/gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi-`
- Before building, create `.scmversion` and verify `kernelrelease` is exactly
  `5.7.1`.

## Runtime Baseline

- `register_aic_wlan1_v25.sh`: manual-only AIC8800 wlan1 registration.
- `start_miracast_live_display_yuvcrop.sh`: stable 640x480 source centre-crop
  display launcher.
- `start_bluetooth_a2dp.sh`: original A2DP source startup script retained as
  the pre-experiment baseline. Bluetooth A2DP is not yet declared stable.

## Excluded

- HCI-to-EP4 and QH/error-path kernel candidates.
- AIC experimental modules, firmware, kernel/rootfs images, and build output.
- Unrelated worktree modifications and credentials.

## Verification

The stable inputs in this directory have SHA/MD5 values recorded in the commit
message. Deployable binaries must be built and checksum-verified separately.
