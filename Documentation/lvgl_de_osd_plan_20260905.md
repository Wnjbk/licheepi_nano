# LVGL / DE OSD Candidate Plan

## Scope

This document records the reversible starting point for an LVGL 8.2 user
interface on the F1C200S SII9022 HDMI platform.  It is intentionally limited
to a display-engine OSD path.  It does not replace or modify the AIC8800,
Miracast, Cedar decoder, HDMI/SII9022, rootfs, or startup chains.

## Source Baseline

| Item | Value |
| --- | --- |
| Candidate worktree | `/home/wnk/F1C200S_host_archive/linux_sii9022_lvgl_de_osd_20260905` |
| Candidate branch | `lvgl-de-osd-20260905` |
| Base commit | `6d24e044a3cae9175e96d6d4688938992f079738` |
| Source baseline branch | `kernel-sii9022-native-20260823` |
| Reproducible baseline | Linux 5.7.1 `#236`, source-backed |
| Baseline zImage MD5 | `dec7b250c0f9213c4b1beca225597dfe` |
| Baseline DTB MD5 | `dc40f1b4c19ccaa8710ef49204889f86` |
| Baseline `.config` MD5 | `7f4c74dbe4dcfdf51359f266e0bb9b27` |

The branch was pushed without source changes before this document was added.
The deployed board remains on the protected #236 baseline.

## Read-Only DE Audit

The source-backed kernel has `CONFIG_DRM_SUN4I_BACKEND=y`, GEM CMA helpers,
and the existing `SRGN` DRM ioctl interface.  The backend has four layers and
two composition pipes.

* Cedar's current display path configures frontend-converted XRGB output on
  layer 0 through `MOUNT_SET_YUV_VIEW` / `MOUNT_SET_YUV_CENTER_CROP_VIEW`.
* Layer 1 is available for a separate normal RGB framebuffer.  It must never
  share Cedar's layer 0 or reconfigure its frontend state.
* The backend natively supports RGB565, with a 640 x 480 double-buffer cost
  of 1,228,800 bytes (2 x 640 x 480 x 2).  No full 32-bit framebuffer is
  required for the initial LVGL path.
* The existing `MOUNT_FB_NORMAL` ioctl only changes the framebuffer address.
  It cannot safely initialize an independent OSD plane because it does not
  set layer size, line width, RGB565 format, pipe/priority, or enable state.
* Global alpha is already available through `MOUNT_SET_ALPHA`.  For the OSD,
  use layer 1, priority 1, pipe 1, and alpha less than 255.  This avoids the
  documented lowest-plane alpha limitation of the sun4i backend.  RGB565 has
  global alpha only, so the initial path can make an OSD rectangle translucent
  but cannot provide transparent pixels inside that rectangle.  A pixel-alpha
  ARGB format is a separate, later candidate.

## Candidate Contract

The first code candidate may add only a narrow SRGN RGB OSD configure ioctl:

1. Validate layer is 1..3, width/height/stride are non-zero and within the
   active display bounds, and the requested pixel format is RGB565.
2. Program that normal layer's size, coordinate, line width, RGB565 format,
   pipe, priority, and enable bit.  Clear YUV/frontend flags for that OSD
   layer only. A zero width and height request disables layer 1 before its
   CMA/GEM buffers are released.
3. Keep `MOUNT_FB_NORMAL` as the address-only page-flip operation.  LVGL's
   flush callback will submit the inactive DMA/CMA buffer address after
   drawing, with no CPU composition or Cedar-buffer copy.
4. Do not touch layer 0, frontend registers, Cedar code, panel timing, or
   existing Miracast scripts.

The separate host LVGL proof-of-concept will use two RGB565 buffers and a
dirty-area flush callback.  It will be developed outside this kernel tree.
There will be no build or board deployment until the kernel diff is committed,
reviewed, artifact metadata is archived, and an explicit rollback package is
recorded.

## Planned Validation

1. Boot OSD candidate with OSD disabled: HDMI and the existing Cedar/Miracast
   layer-0 path must be unchanged.
2. Show RGB565 OSD alone on layer 1, verify its framebuffer address flips.
3. Show OSD with global alpha over Cedar layer 0 and verify both remain live.
4. Only after the above succeeds, perform three cold-boot regression cycles
   covering HDMI, normal WLAN, AIC registration, wlan1, and Miracast display.
