#include <SDL/SDL.h>

#include <vector>

extern "C" {
#include "lvgl.h"
}

namespace {

const int kWidth = 800;
const int kHeight = 480;

SDL_Surface* g_screen = 0;
std::vector<lv_obj_t*> g_rows;
int g_selected = 0;

void ApplyRowStyle(int selected) {
  for (size_t i = 0; i < g_rows.size(); ++i) {
    lv_obj_set_style_bg_color(g_rows[i],
        lv_color_hex(static_cast<int>(i) == selected ? 0x1f6994 : 0x222b3a), 0);
    lv_obj_set_style_border_width(g_rows[i], 0, 0);
  }
}

void Flush(lv_disp_drv_t*, const lv_area_t* area, lv_color_t* colors) {
  if (SDL_MUSTLOCK(g_screen) && SDL_LockSurface(g_screen) != 0) {
    lv_disp_flush_ready(lv_disp_get_default()->driver);
    return;
  }

  for (int y = area->y1; y <= area->y2; ++y) {
    Uint32* out = reinterpret_cast<Uint32*>(
        static_cast<unsigned char*>(g_screen->pixels) + y * g_screen->pitch);
    for (int x = area->x1; x <= area->x2; ++x) {
      const uint16_t pixel = colors->full;
      const Uint8 red = static_cast<Uint8>(((pixel >> 11) & 0x1f) * 255 / 31);
      const Uint8 green = static_cast<Uint8>(((pixel >> 5) & 0x3f) * 255 / 63);
      const Uint8 blue = static_cast<Uint8>((pixel & 0x1f) * 255 / 31);
      out[x] = SDL_MapRGB(g_screen->format, red, green, blue);
      ++colors;
    }
  }

  if (SDL_MUSTLOCK(g_screen)) SDL_UnlockSurface(g_screen);
  lv_disp_flush_ready(lv_disp_get_default()->driver);
}

void BuildUi() {
  lv_obj_t* screen = lv_scr_act();
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x141923), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  lv_obj_t* title = lv_label_create(screen);
  lv_label_set_text(title, "wiliwili - LVGL host migration");
  lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 30, 24);

  lv_obj_t* subtitle = lv_label_create(screen);
  lv_label_set_text(subtitle, "LVGL layout / RGB565 double buffer / dirty flush");
  lv_obj_set_style_text_color(subtitle, lv_color_hex(0xaeb8c7), 0);
  lv_obj_align(subtitle, LV_ALIGN_TOP_LEFT, 30, 52);

  const char* rows[] = {
      "Popular videos", "Continue watching", "Search",
      "Network playback", "Account / QR login"};
  for (int i = 0; i < 5; ++i) {
    lv_obj_t* row = lv_btn_create(screen);
    lv_obj_set_size(row, 720, 52);
    lv_obj_align(row, LV_ALIGN_TOP_LEFT, 38, 104 + i * 58);
    lv_obj_t* label = lv_label_create(row);
    lv_label_set_text(label, rows[i]);
    lv_obj_center(label);
    g_rows.push_back(row);
  }
  ApplyRowStyle(g_selected);

  lv_obj_t* footer = lv_label_create(screen);
  lv_label_set_text(footer, "Up/Down: select    Enter: open    Esc: quit");
  lv_obj_set_style_text_color(footer, lv_color_hex(0x9da8b9), 0);
  lv_obj_align(footer, LV_ALIGN_BOTTOM_LEFT, 30, -22);
}

}  // namespace

int main() {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) return 1;
  g_screen = SDL_SetVideoMode(kWidth, kHeight, 32, SDL_SWSURFACE);
  if (!g_screen) {
    SDL_Quit();
    return 1;
  }

  lv_init();
  std::vector<lv_color_t> buffer_a(kWidth * kHeight);
  std::vector<lv_color_t> buffer_b(kWidth * kHeight);
  lv_disp_draw_buf_t draw_buffer;
  lv_disp_draw_buf_init(&draw_buffer, buffer_a.data(), buffer_b.data(), kWidth * kHeight);

  lv_disp_drv_t display;
  lv_disp_drv_init(&display);
  display.hor_res = kWidth;
  display.ver_res = kHeight;
  display.flush_cb = Flush;
  display.draw_buf = &draw_buffer;
  lv_disp_drv_register(&display);
  BuildUi();

  bool running = true;
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) running = false;
      if (event.type == SDL_KEYDOWN) {
        const SDLKey key = event.key.keysym.sym;
        if (key == SDLK_ESCAPE || key == SDLK_q) running = false;
        if (key == SDLK_UP && g_selected > 0) --g_selected;
        if (key == SDLK_DOWN && g_selected + 1 < static_cast<int>(g_rows.size())) ++g_selected;
        ApplyRowStyle(g_selected);
      }
    }
    lv_tick_inc(16);
    lv_timer_handler();
    SDL_Flip(g_screen);
    SDL_Delay(16);
  }

  SDL_Quit();
  return 0;
}
