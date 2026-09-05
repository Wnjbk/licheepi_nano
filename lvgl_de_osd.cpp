#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "drm_compat.h"

extern "C" {
#include "lvgl.h"
}

#define DRM_SRGN_ATOMIC_COMMIT 0x00
#define DRM_SRGN_ATOMIC_COMMIT_MOUNT_FB_NORMAL 0x00
#define DRM_SRGN_ATOMIC_COMMIT_MOUNT_SET_ALPHA 0x03
#define DRM_SRGN_ATOMIC_COMMIT_MOUNT_CONFIG_RGB565_OSD 0x06

struct drm_srgn_atomic_commit_data {
  uint32_t layer_id;
  uint32_t type;
  uint32_t arg0;
  uint32_t arg1;
  uint32_t arg2;
};

struct drm_srgn_atomic_commit {
  uint32_t size;
  uint32_t data;
};

#define DRM_IOCTL_SRGN_ATOMIC_COMMIT \
  DRM_IOW(DRM_COMMAND_BASE + DRM_SRGN_ATOMIC_COMMIT, struct drm_srgn_atomic_commit)

namespace {

const uint32_t kWidth = 640;
const uint32_t kHeight = 480;
const uint32_t kLayer = 1;
const uint32_t kOverlayAlpha = 160;

struct Buffer {
  uint32_t handle;
  uint32_t pitch;
  uint64_t size;
  void* data;
};

int g_drm = -1;
uint32_t g_stride = 0;
void* g_first_data = 0;
void* g_second_data = 0;
uint32_t g_ticks = 0;
lv_obj_t* g_clock = 0;
volatile sig_atomic_t g_running = 1;
volatile sig_atomic_t g_flush_failed = 0;

int Submit(drm_srgn_atomic_commit_data* commands, uint32_t count) {
  drm_srgn_atomic_commit request;
  request.size = count;
  request.data = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(commands));
  return ioctl(g_drm, DRM_IOCTL_SRGN_ATOMIC_COMMIT, &request);
}

bool CreateBuffer(Buffer* buffer) {
  drm_mode_create_dumb create;
  memset(&create, 0, sizeof(create));
  create.width = kWidth;
  create.height = kHeight;
  create.bpp = 16;
  if (ioctl(g_drm, DRM_IOCTL_MODE_CREATE_DUMB, &create) != 0) return false;

  drm_mode_map_dumb map;
  memset(&map, 0, sizeof(map));
  map.handle = create.handle;
  if (ioctl(g_drm, DRM_IOCTL_MODE_MAP_DUMB, &map) != 0) {
    drm_mode_destroy_dumb destroy;
    memset(&destroy, 0, sizeof(destroy));
    destroy.handle = create.handle;
    ioctl(g_drm, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
    return false;
  }

  buffer->handle = create.handle;
  buffer->pitch = create.pitch;
  buffer->size = create.size;
  buffer->data = mmap(0, create.size, PROT_READ | PROT_WRITE, MAP_SHARED, g_drm, map.offset);
  if (buffer->data == MAP_FAILED) {
    buffer->data = 0;
    drm_mode_destroy_dumb destroy;
    memset(&destroy, 0, sizeof(destroy));
    destroy.handle = create.handle;
    ioctl(g_drm, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
    buffer->handle = 0;
    return false;
  }
  memset(buffer->data, 0, create.size);
  return true;
}

void DestroyBuffer(Buffer* buffer) {
  if (buffer->data && buffer->data != MAP_FAILED) munmap(buffer->data, buffer->size);
  if (buffer->handle) {
    drm_mode_destroy_dumb destroy;
    memset(&destroy, 0, sizeof(destroy));
    destroy.handle = buffer->handle;
    ioctl(g_drm, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
  }
  memset(buffer, 0, sizeof(*buffer));
}

void CopyDirtyArea(const lv_area_t* area, lv_color_t* source) {
  lv_color_t* destination = source == static_cast<lv_color_t*>(g_first_data)
                            ? static_cast<lv_color_t*>(g_second_data)
                            : static_cast<lv_color_t*>(g_first_data);
  const size_t row_bytes = static_cast<size_t>(area->x2 - area->x1 + 1) * sizeof(lv_color_t);
  for (int y = area->y1; y <= area->y2; ++y) {
    const size_t offset = static_cast<size_t>(y) * g_stride +
                          static_cast<size_t>(area->x1) * sizeof(lv_color_t);
    memcpy(reinterpret_cast<unsigned char*>(destination) + offset,
           reinterpret_cast<const unsigned char*>(source) + offset, row_bytes);
  }
}

void Flush(lv_disp_drv_t* display, const lv_area_t* area, lv_color_t* colors) {
  /* Direct mode draws dirty rectangles into a full-screen buffer. Keep its
   * twin coherent before LVGL makes it the next draw target. */
  CopyDirtyArea(area, colors);

  if (lv_disp_flush_is_last(display)) {
  drm_srgn_atomic_commit_data command;
  memset(&command, 0, sizeof(command));
  command.layer_id = kLayer;
  command.type = DRM_SRGN_ATOMIC_COMMIT_MOUNT_FB_NORMAL;
  command.arg0 = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(colors));
  if (Submit(&command, 1) != 0) g_flush_failed = 1;
  }
  lv_disp_flush_ready(display);
}

void Stop(int) {
  g_running = 0;
}

void DisableOsd() {
  if (g_drm < 0) return;
  drm_srgn_atomic_commit_data command;
  memset(&command, 0, sizeof(command));
  command.layer_id = kLayer;
  command.type = DRM_SRGN_ATOMIC_COMMIT_MOUNT_CONFIG_RGB565_OSD;
  Submit(&command, 1);
}

void Tick(lv_timer_t*) {
  ++g_ticks;
  char text[64];
  snprintf(text, sizeof(text), "LVGL DE OSD  %02u:%02u", g_ticks / 60, g_ticks % 60);
  lv_label_set_text(g_clock, text);
}

void BuildUi() {
  lv_obj_t* screen = lv_scr_act();
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x10202a), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  lv_obj_t* title = lv_label_create(screen);
  lv_label_set_text(title, "F1C200S LVGL / Display Engine");
  lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 32, 26);

  lv_obj_t* subtitle = lv_label_create(screen);
  lv_label_set_text(subtitle, "RGB565 double buffer on DE layer 1");
  lv_obj_set_style_text_color(subtitle, lv_color_hex(0xb9d7e4), 0);
  lv_obj_align(subtitle, LV_ALIGN_TOP_LEFT, 32, 54);

  const char* rows[] = {"Home", "Live display", "Library", "Settings"};
  for (unsigned i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i) {
    lv_obj_t* row = lv_btn_create(screen);
    lv_obj_set_size(row, 480, 58);
    lv_obj_align(row, LV_ALIGN_TOP_LEFT, 42, 108 + i * 66);
    lv_obj_set_style_bg_color(row, lv_color_hex(i == 0 ? 0x216b8f : 0x223746), 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_t* label = lv_label_create(row);
    lv_label_set_text(label, rows[i]);
    lv_obj_center(label);
  }

  g_clock = lv_label_create(screen);
  lv_obj_set_style_text_color(g_clock, lv_color_hex(0x92d9a4), 0);
  lv_obj_align(g_clock, LV_ALIGN_BOTTOM_LEFT, 32, -28);
  Tick(0);
  lv_timer_create(Tick, 1000, 0);
}

}  // namespace

int main() {
  g_drm = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
  if (g_drm < 0) {
    perror("open /dev/dri/card0");
    return 1;
  }

  signal(SIGINT, Stop);
  signal(SIGTERM, Stop);

  Buffer first = {};
  Buffer second = {};
  if (!CreateBuffer(&first)) {
    perror("create first RGB565 dumb buffer");
    close(g_drm);
    return 1;
  }
  if (!CreateBuffer(&second)) {
    perror("create second RGB565 dumb buffer");
    DestroyBuffer(&first);
    close(g_drm);
    return 1;
  }
  if (first.pitch != second.pitch) {
    fprintf(stderr, "RGB565 dumb-buffer pitch mismatch: %u != %u\n",
            first.pitch, second.pitch);
    DestroyBuffer(&second);
    DestroyBuffer(&first);
    close(g_drm);
    return 1;
  }
  g_stride = first.pitch;
  g_first_data = first.data;
  g_second_data = second.data;

  drm_srgn_atomic_commit_data setup[3];
  memset(setup, 0, sizeof(setup));
  setup[0].layer_id = kLayer;
  setup[0].type = DRM_SRGN_ATOMIC_COMMIT_MOUNT_CONFIG_RGB565_OSD;
  setup[0].arg0 = (kHeight << 16) | kWidth;
  setup[0].arg1 = 0;
  setup[0].arg2 = g_stride;
  setup[1].layer_id = kLayer;
  setup[1].type = DRM_SRGN_ATOMIC_COMMIT_MOUNT_SET_ALPHA;
  setup[1].arg0 = kOverlayAlpha;
  setup[2].layer_id = kLayer;
  setup[2].type = DRM_SRGN_ATOMIC_COMMIT_MOUNT_FB_NORMAL;
  setup[2].arg0 = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(first.data));
  if (Submit(setup, 3) != 0) {
    perror("configure RGB565 OSD");
    DestroyBuffer(&second);
    DestroyBuffer(&first);
    close(g_drm);
    return 1;
  }

  lv_init();
  lv_disp_draw_buf_t draw_buffer;
  lv_disp_draw_buf_init(&draw_buffer, static_cast<lv_color_t*>(first.data),
                        static_cast<lv_color_t*>(second.data), kWidth * kHeight);
  lv_disp_drv_t display;
  lv_disp_drv_init(&display);
  display.hor_res = kWidth;
  display.ver_res = kHeight;
  display.direct_mode = 1;
  display.draw_buf = &draw_buffer;
  display.flush_cb = Flush;
  if (!lv_disp_drv_register(&display)) {
    fprintf(stderr, "lv_disp_drv_register failed\n");
    DisableOsd();
    DestroyBuffer(&second);
    DestroyBuffer(&first);
    close(g_drm);
    return 1;
  }
  BuildUi();

  while (g_running && !g_flush_failed) {
    uint32_t delay_ms = lv_timer_handler();
    if (delay_ms == LV_NO_TIMER_READY || delay_ms > 1000) delay_ms = 1000;
    if (delay_ms == 0) delay_ms = 1;
    usleep(delay_ms * 1000);
    lv_tick_inc(delay_ms);
  }

  if (g_flush_failed) fprintf(stderr, "DE layer-1 page flip failed: %s\n", strerror(errno));
  DisableOsd();
  DestroyBuffer(&second);
  DestroyBuffer(&first);
  close(g_drm);
  return g_flush_failed ? 1 : 0;
}
