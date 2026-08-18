# Baseline Manifest

Date: 2026-08-18

## Runtime Identity

| Component | Baseline |
|---|---|
| Kernel | `5.7.1 #148`, binary-only recovery artifact |
| Kernel image MD5 | `b777b5bbf42614a20d8ebdd5d8f560ef` |
| AIC loader | v18 `aic_load_fw.ko`, MD5 `698b92823499cb1843fe9ab64a2e7df0` |
| AIC fdrv | v25 `aic8800_fdrv.ko`, MD5 `13ceac918ec83d77acb2ecb5a5c5d37e` |
| WFD supplicant | `wpa24_aic_wfd_supplicant`, MD5 `a9ae2b53fc4a3469fad24fd1abbd42fb` |
| WFD CLI | `wpa24_aic_wfd_cli`, MD5 `97f7384a530c9514f75533ab9e530a56` |
| Sink | `miracast_sink_dump.lowest`, MD5 `ceeeb9958f8f84f2a0b7c6c3df63db0e` |
| Player | `cedar_drm_player.yuvcrop`, MD5 `a0879c896f48d40e2579b093ff960a99` |
| GO script | scan-stop candidate MD5 `1db5533d299f51d2f6c2ae7b0deefbe2` |
| Live wrapper | yuvcrop scan-stop wrapper MD5 `7b1bb7d00d86aa852fe6cccd22c906bb` |

## Startup

1. Cold AIC recovery: from board console load `/lib/modules/5.7.1/extra/aic_load_fw.ko` and wait for USB ID `a69c:8d83`.
2. Register `wlan1` with `/root/aic_miracast/register_aic_wlan1_v25.sh start`.
3. Start `/root/aic_miracast/candidates/scanstop_20260817/start_miracast_live_display_yuvcrop.scanstop.sh start`.

The GO script must call `p2p_stop_find` then wait two seconds before
`p2p_group_add freq=5805`. Do not use `p2p_find` or start the older GO script.

## Source Layout

| Directory | Source origin |
|---|---|
| `sources/aic8800_fdrv_v25` | `third_party/aic8800_rx_msg_clamp_only_20260802_v25` |
| `sources/aic_load_fw_v18` | `third_party/aic8800_rx_data_pressure_guard_20260802_v18` |
| `sources/lazycast_sink` | `third_party/lazycast_host_20260721` |
| `sources/wpa_supplicant_2_4_aic_wfd` | `third_party/wpa_supplicant_2_4_aic_wfd_arm_board_20260723` |
| `sources/cedar_drm_player_yuvcrop` | `board_tools_f1c200s/cedar_drm_player_yuvcrop_src` |

## Dependency Regression Set

- AIC cold USB firmware transition: `a69c:8d80 -> a69c:8d83`
- `wlan1` registration with the v18 loader and v25 fdrv
- P2P GO beacon and Android cast discovery on 5805 MHz
- Miracast RTSP/RTP into FIFO and yuvcrop display
- `wlan0` association and SSH at `10.0.0.107`

Do not include Bluetooth/A2DP as a prerequisite for this baseline; it remains
outside the current scope.
