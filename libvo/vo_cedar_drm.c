/* Zero-copy Cedar MB32 output using the F1C200S private DRM ioctl. */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <drm/drm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "config.h"
#include "mp_msg.h"
#include "video_out.h"
#define NO_DRAW_FRAME
#define NO_DRAW_SLICE
#include "video_out_internal.h"

extern void vd_cedar_return_picture(void *picture);

#define DRM_SRGN_ATOMIC_COMMIT 0x00
#define DRM_SRGN_RESET_FB_CACHE 0x01
#define DRM_SRGN_MOUNT_FB_YUV 0x01
#define DRM_SRGN_SET_YUV_VIEW 0x04
#define DRM_SRGN_SET_YUV_CENTER_CROP_VIEW 0x05
#define DRM_SRGN_SET_YUV_PITCH 0x06
#define CEDAR_PHYS_MARKER 0x53475250U

struct srgn_commit_data {
    uint32_t layer_id, type, arg0, arg1, arg2;
};
struct srgn_commit {
    uint32_t size, data;
};

#define DRM_IOCTL_SRGN_ATOMIC_COMMIT \
    DRM_IOW(DRM_COMMAND_BASE + DRM_SRGN_ATOMIC_COMMIT, struct srgn_commit)
#define DRM_IOCTL_SRGN_RESET_FB_CACHE \
    DRM_IO(DRM_COMMAND_BASE + DRM_SRGN_RESET_FB_CACHE)

static const vo_info_t info = {
    "F1C200S Cedar DRM zero-copy output", "cedar_drm", "F1C200S", "MB32 only"
};

const LIBVO_EXTERN(cedar_drm)

enum display_mode { DISPLAY_FIT, DISPLAY_STRETCH, DISPLAY_CROP };
static int drm_fd = -1;
static int screen_w, screen_h, source_w, source_h;
static int view_x, view_y, view_w, view_h;
static enum display_mode display_mode = DISPLAY_FIT;
static void *displayed_picture;

static uint32_t pack_size(int w, int h)
{
    return ((uint32_t)(h & 0xffff) << 16) | (uint32_t)(w & 0xffff);
}

static int submit_view(void)
{
    struct srgn_commit_data data;
    struct srgn_commit commit;
    memset(&data, 0, sizeof(data));
    data.layer_id = 0;
    data.type = DRM_SRGN_SET_YUV_VIEW;
    data.arg0 = pack_size(source_w, source_h);
    data.arg1 = pack_size(view_w, view_h);
    data.arg2 = ((uint32_t)((int16_t)view_y) << 16) | (uint16_t)((int16_t)view_x);
    commit.size = 1;
    commit.data = (uint32_t)(uintptr_t)&data;
    return drmIoctl(drm_fd, DRM_IOCTL_SRGN_ATOMIC_COMMIT, &commit);
}

static int submit_crop_view(void)
{
    int crop_w = source_w, crop_h = source_h;
    struct srgn_commit_data data;
    struct srgn_commit commit;
    if ((int64_t)source_w * screen_h > (int64_t)source_h * screen_w)
        crop_w = source_h * screen_w / screen_h;
    else
        crop_h = source_w * screen_h / screen_w;
    crop_w &= ~1;
    crop_h &= ~1;
    memset(&data, 0, sizeof(data));
    data.layer_id = 0;
    data.type = DRM_SRGN_SET_YUV_CENTER_CROP_VIEW;
    data.arg0 = pack_size(source_w, source_h);
    data.arg1 = pack_size(crop_w, crop_h);
    data.arg2 = pack_size(screen_w, screen_h);
    commit.size = 1;
    commit.data = (uint32_t)(uintptr_t)&data;
    return drmIoctl(drm_fd, DRM_IOCTL_SRGN_ATOMIC_COMMIT, &commit);
}

static int query_format(uint32_t format)
{
    return format == IMGFMT_YV12 ? VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW : 0;
}

static int preinit(const char *arg)
{
    if (!arg || !strcmp(arg, "fit"))
        display_mode = DISPLAY_FIT;
    else if (!strcmp(arg, "stretch"))
        display_mode = DISPLAY_STRETCH;
    else if (!strcmp(arg, "crop"))
        display_mode = DISPLAY_CROP;
    else
        return ENOSYS;
    drm_fd = open("/dev/dri/card0", O_RDWR);
    if (drm_fd < 0)
        return 1;
    if (drmIoctl(drm_fd, DRM_IOCTL_SRGN_RESET_FB_CACHE, NULL) < 0) {
        close(drm_fd);
        drm_fd = -1;
        return 1;
    }
    return 0;
}

static int config(uint32_t width, uint32_t height, uint32_t d_width,
                  uint32_t d_height, uint32_t fullscreen, char *title,
                  uint32_t format)
{
    drmModeRes *res;
    drmModeCrtc *crtc;
    (void)d_width; (void)d_height; (void)fullscreen; (void)title;
    if (drm_fd < 0 || format != IMGFMT_YV12)
        return 1;
    res = drmModeGetResources(drm_fd);
    if (!res || !res->count_crtcs)
        return 1;
    crtc = drmModeGetCrtc(drm_fd, res->crtcs[0]);
    if (!crtc || !crtc->mode_valid)
        return 1;
    screen_w = crtc->mode.hdisplay;
    screen_h = crtc->mode.vdisplay;
    drmModeFreeCrtc(crtc);
    drmModeFreeResources(res);
    source_w = width;
    source_h = height;
    view_x = view_y = 0;
    view_w = screen_w;
    view_h = screen_h;
    if (display_mode == DISPLAY_FIT) {
        if ((int64_t)width * screen_h > (int64_t)height * screen_w) {
            view_h = height * screen_w / width;
            view_y = (screen_h - view_h) / 2;
        } else {
            view_w = width * screen_h / height;
            view_x = (screen_w - view_w) / 2;
        }
        view_x &= ~1; view_y &= ~1; view_w &= ~1; view_h &= ~1;
    }
    if (display_mode == DISPLAY_CROP)
        return submit_crop_view() ? 1 : 0;
    return submit_view() ? 1 : 0;
}

static int control(uint32_t request, void *data)
{
    struct srgn_commit_data commands[1];
    struct srgn_commit commit;
    mp_image_t *mpi;
    if (request == VOCTRL_QUERY_FORMAT)
        return query_format(*(uint32_t *)data);
    if (request != VOCTRL_DRAW_IMAGE || drm_fd < 0)
        return VO_NOTIMPL;
    mpi = data;
    if (!mpi || !mpi->priv)
        return VO_FALSE;
    memset(commands, 0, sizeof(commands));
    commands[0].layer_id = 0;
    commands[0].type = DRM_SRGN_MOUNT_FB_YUV;
    commands[0].arg0 = (uint32_t)(uintptr_t)mpi->planes[0];
    commands[0].arg1 = (uint32_t)(uintptr_t)mpi->planes[1];
    commands[0].arg2 = CEDAR_PHYS_MARKER;
    commit.size = 1;
    commit.data = (uint32_t)(uintptr_t)commands;
    if (drmIoctl(drm_fd, DRM_IOCTL_SRGN_ATOMIC_COMMIT, &commit) < 0)
        return VO_FALSE;
    if (displayed_picture)
        vd_cedar_return_picture(displayed_picture);
    displayed_picture = mpi->priv;
    return VO_TRUE;
}

static void draw_osd(void) { }
static void flip_page(void) { }
static void check_events(void) { }

static void uninit(void)
{
    if (displayed_picture)
        vd_cedar_return_picture(displayed_picture);
    displayed_picture = NULL;
    if (drm_fd >= 0)
        close(drm_fd);
    drm_fd = -1;
}
