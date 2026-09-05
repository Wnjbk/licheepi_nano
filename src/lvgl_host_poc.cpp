#include <SDL/SDL.h>

#include <algorithm>
#include <vector>

extern "C" {
#include "lvgl.h"
}

namespace {

const int kWidth = 800;
const int kHeight = 480;
const int kSidebarWidth = 104;
const int kCardCount = 3;

SDL_Surface* g_screen = 0;
std::vector<lv_obj_t*> g_sidebar;
std::vector<lv_obj_t*> g_cards;
int g_focus = 0;
bool g_detail = false;

void SetText(lv_obj_t* parent, const char* text, const lv_font_t* font,
             lv_color_t color, lv_align_t align, int x, int y) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  lv_obj_align(label, align, x, y);
}

void StyleSurface(lv_obj_t* object, int color, int radius) {
  lv_obj_set_style_bg_color(object, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(object, 0, 0);
  lv_obj_set_style_radius(object, radius, 0);
}

void ApplyFocus() {
  for (size_t i = 0; i < g_sidebar.size(); ++i) {
    const bool selected = !g_detail && g_focus == static_cast<int>(i);
    StyleSurface(g_sidebar[i], selected ? 0x3c5d83 : 0x182231, 8);
  }
  for (size_t i = 0; i < g_cards.size(); ++i) {
    const bool selected = !g_detail && g_focus == static_cast<int>(i) + 3;
    lv_obj_set_style_border_width(g_cards[i], selected ? 3 : 0, 0);
    lv_obj_set_style_border_color(g_cards[i], lv_color_hex(0x4fa9e8), 0);
    lv_obj_set_style_bg_color(g_cards[i],
        lv_color_hex(selected ? 0x243a51 : 0x202b3a), 0);
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

void BuildSidebar(lv_obj_t* root) {
  lv_obj_t* bar = lv_obj_create(root);
  lv_obj_set_size(bar, kSidebarWidth, kHeight);
  lv_obj_align(bar, LV_ALIGN_LEFT_MID, 0, 0);
  StyleSurface(bar, 0x101721, 0);

  SetText(bar, "wiliwili", &lv_font_montserrat_16, lv_color_hex(0xffffff),
          LV_ALIGN_TOP_MID, 0, 30);
  SetText(bar, "LVGL", &lv_font_montserrat_12, lv_color_hex(0x63b8ef),
          LV_ALIGN_TOP_MID, 0, 54);

  const char* labels[] = {"Home", "Popular", "Mine"};
  for (int i = 0; i < 3; ++i) {
    lv_obj_t* button = lv_btn_create(bar);
    lv_obj_set_size(button, 82, 48);
    lv_obj_align(button, LV_ALIGN_TOP_MID, 0, 102 + i * 58);
    StyleSurface(button, 0x182231, 8);
    SetText(button, labels[i], &lv_font_montserrat_14, lv_color_hex(0xdce7f2),
            LV_ALIGN_CENTER, 0, 0);
    g_sidebar.push_back(button);
  }

  SetText(bar, "ESC", &lv_font_montserrat_12, lv_color_hex(0x6f8398),
          LV_ALIGN_BOTTOM_MID, 0, -34);
  SetText(bar, "back", &lv_font_montserrat_12, lv_color_hex(0x6f8398),
          LV_ALIGN_BOTTOM_MID, 0, -16);
}

lv_obj_t* AddVideoCard(lv_obj_t* root, int index, const char* category,
                       const char* title, const char* meta, int art_color) {
  lv_obj_t* card = lv_btn_create(root);
  lv_obj_set_size(card, 202, 274);
  lv_obj_align(card, LV_ALIGN_TOP_LEFT, 128 + index * 218, 124);
  StyleSurface(card, 0x202b3a, 10);

  lv_obj_t* art = lv_obj_create(card);
  lv_obj_set_size(art, 184, 118);
  lv_obj_align(art, LV_ALIGN_TOP_MID, 0, 9);
  StyleSurface(art, art_color, 8);
  SetText(art, category, &lv_font_montserrat_14, lv_color_hex(0xffffff),
          LV_ALIGN_BOTTOM_LEFT, 12, -10);

  lv_obj_t* title_label = lv_label_create(card);
  lv_label_set_text(title_label, title);
  lv_label_set_long_mode(title_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(title_label, 174);
  lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(title_label, lv_color_hex(0xf3f7fb), 0);
  lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 140);

  SetText(card, meta, &lv_font_montserrat_12, lv_color_hex(0x9cafc2),
          LV_ALIGN_BOTTOM_LEFT, 14, -14);
  return card;
}

void BuildHome() {
  lv_obj_t* screen = lv_scr_act();
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x141923), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  BuildSidebar(screen);
  SetText(screen, "Home", &lv_font_montserrat_24, lv_color_hex(0xffffff),
          LV_ALIGN_TOP_LEFT, 130, 28);
  SetText(screen, "Recommended", &lv_font_montserrat_14, lv_color_hex(0x8da4ba),
          LV_ALIGN_TOP_LEFT, 132, 62);

  lv_obj_t* search = lv_obj_create(screen);
  lv_obj_set_size(search, 238, 34);
  lv_obj_align(search, LV_ALIGN_TOP_RIGHT, -28, 30);
  StyleSurface(search, 0x202b3a, 17);
  SetText(search, "Search Bilibili", &lv_font_montserrat_12, lv_color_hex(0x94a8bb),
          LV_ALIGN_LEFT_MID, 16, 0);

  g_cards.push_back(AddVideoCard(screen, 0, "POPULAR", "Explore public Bilibili recommendations",
                                  "1.2M views", 0x6b4f9e));
  g_cards.push_back(AddVideoCard(screen, 1, "CONTINUE", "Resume the last watched video",
                                  "12:48 remaining", 0x227c88));
  g_cards.push_back(AddVideoCard(screen, 2, "NETWORK", "Open a resolved stream with Cedar",
                                  "external player", 0xa96045));

  SetText(screen, "Arrow keys: move    Enter: detail    Esc: quit", &lv_font_montserrat_12,
          lv_color_hex(0x8095aa), LV_ALIGN_BOTTOM_LEFT, 130, -22);
  ApplyFocus();
}

void BuildDetail() {
  lv_obj_clean(lv_scr_act());
  g_sidebar.clear();
  g_cards.clear();
  g_detail = true;
  lv_obj_t* screen = lv_scr_act();
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x141923), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  BuildSidebar(screen);

  lv_obj_t* preview = lv_obj_create(screen);
  lv_obj_set_size(preview, 430, 242);
  lv_obj_align(preview, LV_ALIGN_TOP_LEFT, 132, 86);
  StyleSurface(preview, 0x285a7d, 12);
  SetText(preview, "VIDEO PREVIEW", &lv_font_montserrat_16, lv_color_hex(0xffffff),
          LV_ALIGN_CENTER, 0, 0);

  SetText(screen, "Video detail", &lv_font_montserrat_24, lv_color_hex(0xffffff),
          LV_ALIGN_TOP_LEFT, 132, 28);
  SetText(screen, "A compact LVGL recreation of wiliwili navigation", &lv_font_montserrat_16,
          lv_color_hex(0xe7eff7), LV_ALIGN_TOP_LEFT, 132, 350);
  SetText(screen, "BVID / quality / playback are separate adapters.", &lv_font_montserrat_14,
          lv_color_hex(0x9dafc0), LV_ALIGN_TOP_LEFT, 132, 382);

  lv_obj_t* play = lv_btn_create(screen);
  lv_obj_set_size(play, 170, 48);
  lv_obj_align(play, LV_ALIGN_BOTTOM_RIGHT, -40, -42);
  StyleSurface(play, 0x2580ba, 9);
  SetText(play, "Play adapter", &lv_font_montserrat_14, lv_color_hex(0xffffff),
          LV_ALIGN_CENTER, 0, 0);
  SetText(screen, "Esc: Home", &lv_font_montserrat_12, lv_color_hex(0x8095aa),
          LV_ALIGN_BOTTOM_LEFT, 132, -22);
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
  BuildHome();

  bool running = true;
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) running = false;
      if (event.type == SDL_KEYDOWN) {
        const SDLKey key = event.key.keysym.sym;
        if (key == SDLK_q) running = false;
        if (key == SDLK_ESCAPE) {
          if (g_detail) {
            lv_obj_clean(lv_scr_act());
            g_sidebar.clear();
            g_cards.clear();
            g_detail = false;
            g_focus = 0;
            BuildHome();
          } else {
            running = false;
          }
        }
        if (!g_detail && key == SDLK_LEFT) {
          g_focus = std::max(0, g_focus - 1);
          ApplyFocus();
        }
        if (!g_detail && key == SDLK_RIGHT) {
          g_focus = std::min(kCardCount + 2, g_focus + 1);
          ApplyFocus();
        }
        if (!g_detail && key == SDLK_UP) {
          g_focus = std::max(0, g_focus - (g_focus >= 3 ? 3 : 1));
          ApplyFocus();
        }
        if (!g_detail && key == SDLK_DOWN) {
          g_focus = std::min(kCardCount + 2, g_focus + (g_focus >= 3 ? 3 : 1));
          ApplyFocus();
        }
        if (!g_detail && (key == SDLK_RETURN || key == SDLK_p) && g_focus >= 3) BuildDetail();
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
