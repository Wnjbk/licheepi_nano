# LVGL DE OSD candidate

This is a board-only LVGL 8.2 proof of concept for the F1C200S candidate
kernel `fc1474c47b5614b128103f49c4cc433cc2ebf0e4`.

It allocates two 640x480 RGB565 DRM dumb buffers and presents them on SRGN DE
layer 1 using the candidate-only RGB565 OSD ioctl. Cedar and Miracast remain
on layer 0 and are not started or modified by this program.

The program is deliberately not an SDL application and does not open
`/dev/fb0`.  `SIGINT` and `SIGTERM` disable layer 1 before releasing its
buffers.  It is a candidate, not a normal boot service or a stable baseline.

The program carries the small DRM dumb-buffer UAPI subset it needs, so the
userspace build does not include kernel-internal headers. Build on the Ubuntu
host with `make`. The Makefile explicitly targets the board's ARM926EJ-S
(`armv5te`) CPU; do not use the Linaro toolchain default target. Deploy only
to a versioned candidate
directory, verify the binary checksum, stop GMenu2X for the isolated test,
and restore it after the test.
