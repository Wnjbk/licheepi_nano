# F1C200S runtime solidification

This directory turns the current working setup into a repeatable recovery flow.

## Goal

After reflashing the base system, restore these runtime pieces within about 10 minutes:

- `gmenu2x`
- `gpSP`
- `uinput_tty_keyd`
- `suniv_f1c100s_tvd.ko`
- `tvd_fb_preview`
- launcher scripts under `/root`

## What is included

- `rootfs_overlay/`
  - `/root/run_gmenu2x.sh`
  - `/root/run_gpsp.sh`
  - `/root/run_gpsp_menu.sh`
  - `/root/enable_uinput.sh`
  - `/root/run_uinput_tty_keyd.sh`
  - `/root/load_tvd.sh`
  - `/root/run_tvd_preview.sh`
  - `/etc/init.d/S99gmenu2x`
- `collect_runtime_payload.sh`
  - collects the already-built binaries
- `sync_overlay_to_buildroot.sh`
  - copies static scripts into `buildroot/output/target`
- `install_payload_to_target.sh`
  - copies built payload into `buildroot/output/target`

## Expected host paths

Default assumptions:

```text
~/LicheePi_Nano/buildroot-2018.02.11
~/LicheePi_Nano/gmenu2x/dist/gmenu2x-f1c200s.tar.gz
~/LicheePi_Nano/gpsp/f1c200s/gpsp
~/LicheePi_Nano/board_tools_f1c200s/dist/f1c200s/uinput_tty_keyd
~/LicheePi_Nano/tvd_f1c100s_linux57/src/suniv_f1c100s_tvd.ko
~/LicheePi_Nano/tvd_f1c100s_linux57/src/tvd_fb_preview
```

Override any of them with environment variables when needed.

## One-time host-side preparation

Copy this whole `runtime_bundle` directory into:

```text
~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle
```

## 10-minute recovery flow

### 1. Rebuild or confirm the required runtime artifacts

```sh
cd ~/LicheePi_Nano/gmenu2x
make -f Makefile.f1c200s

cd ~/LicheePi_Nano/board_tools_f1c200s
make -f Makefile.f1c200s

cd ~/LicheePi_Nano/tvd_f1c100s_linux57/src
make
```

`gpSP` binary should already exist at:

```text
~/LicheePi_Nano/gpsp/f1c200s/gpsp
```

### 2. Sync launcher scripts into Buildroot target

```sh
cd ~/LicheePi_Nano/board_tools_f1c200s/runtime_bundle
./sync_overlay_to_buildroot.sh
```

### 3. Collect the current binaries

```sh
./collect_runtime_payload.sh
```

### 4. Install the payload into Buildroot target

```sh
./install_payload_to_target.sh
```

### 5. Repack images

```sh
cd ~/LicheePi_Nano/buildroot-2018.02.11
make
```

Use the updated `output/images/` files to reflash the board.

## Post-flash quick verification

On the board:

```sh
ls -l /root/run_gmenu2x.sh /root/run_gpsp.sh /root/uinput_tty_keyd /root/tvd_fb_preview /root/suniv_f1c100s_tvd.ko
```

### `gmenu2x`

```sh
/root/run_gmenu2x.sh
```

### `gpSP`

```sh
/root/run_gpsp_menu.sh
```

### `uinput`

```sh
/root/enable_uinput.sh
MODE=grab /root/run_uinput_tty_keyd.sh
```

Stop it from another terminal:

```sh
killall uinput_tty_keyd
```

### `TVD`

```sh
STANDARD=pal /root/load_tvd.sh
/root/run_tvd_preview.sh
```

## Notes

- `S99gmenu2x` is present in the overlay, but whether it starts automatically depends on your init flow.
- `run_tvd_preview.sh` assumes `v4l2-ctl` is present in the rootfs.
- `gmenu2x` is installed from its tarball into `/root/gmenu2x`.
- BusyBox `tar` on the board may not support `-z`; extraction is done on the host side in `install_payload_to_target.sh`.
