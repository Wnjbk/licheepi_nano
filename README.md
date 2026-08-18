# F1C200S Miracast Stable Baseline (2026-08-18)

This repository snapshot preserves the host-side source and deployable
artifacts for the verified F1C200S live Miracast path:

Phone Wi-Fi Direct -> AIC8800 P2P GO at 5805 MHz -> lowest sink -> FIFO ->
Cedar H.264 decode -> 384x640 center crop on the ST7701 panel.

## Scope

- `sources/` contains complete source snapshots for the AIC v25 fdrv, AIC v18
  firmware loader, WFD supplicant, Miracast sink, and Cedar yuvcrop player.
- `scripts/` contains the scan-stop P2P GO candidate and its yuvcrop live
  wrapper. The wrapper is the active startup path for this baseline.
- `artifacts/` contains the running kernel image and matching configuration.

The kernel image is **binary recovery only**: the exact complete source tree
that produced `5.7.1 #148` is not available. Do not patch or rebuild it from
this snapshot. Restore the image only as documented in `BASELINE_MANIFEST.md`.

## Test Status

The user confirmed this snapshot connects and plays stably. This records a
known-good operating point, but it has not yet completed the required three
consecutive cold-boot acceptance cycles. Future source changes must preserve
this snapshot and pass that acceptance gate before replacing it.
