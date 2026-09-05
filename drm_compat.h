#ifndef LVGL_DE_OSD_DRM_COMPAT_H
#define LVGL_DE_OSD_DRM_COMPAT_H

#include <stdint.h>
#include <sys/ioctl.h>

/* Stable userspace portion of the DRM dumb-buffer UAPI used by this POC. */
struct drm_mode_create_dumb {
  uint32_t height;
  uint32_t width;
  uint32_t bpp;
  uint32_t flags;
  uint32_t handle;
  uint32_t pitch;
  uint64_t size;
};

struct drm_mode_map_dumb {
  uint32_t handle;
  uint32_t pad;
  uint64_t offset;
};

struct drm_mode_destroy_dumb {
  uint32_t handle;
};

#define DRM_IOCTL_BASE 'd'
#define DRM_COMMAND_BASE 0x40
#define DRM_IOW(nr, type) _IOW(DRM_IOCTL_BASE, nr, type)
#define DRM_IOWR(nr, type) _IOWR(DRM_IOCTL_BASE, nr, type)
#define DRM_IOCTL_MODE_CREATE_DUMB DRM_IOWR(0xB2, struct drm_mode_create_dumb)
#define DRM_IOCTL_MODE_MAP_DUMB DRM_IOWR(0xB3, struct drm_mode_map_dumb)
#define DRM_IOCTL_MODE_DESTROY_DUMB DRM_IOWR(0xB4, struct drm_mode_destroy_dumb)

#endif
