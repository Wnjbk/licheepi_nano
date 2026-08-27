#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <curl/curl.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

const int kWidth = 800;
const int kHeight = 480;
const char* kFontPath = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";

struct Item {
  const char* title;
  const char* detail;
};

const Item kItems[] = {
    {"Popular videos", "Public Bilibili feed connectivity check"},
    {"Continue watching", "Local history adapter placeholder"},
    {"Search", "Keyboard and touch search adapter placeholder"},
    {"Network playback", "External Cedar or GStreamer player backend"},
};

SDL_Color Color(Uint8 r, Uint8 g, Uint8 b) {
  SDL_Color value;
  value.r = r;
  value.g = g;
  value.b = b;
  return value;
}

void Fill(SDL_Surface* screen, const SDL_Rect& rect, Uint32 color) {
  SDL_FillRect(screen, const_cast<SDL_Rect*>(&rect), color);
}

void Text(SDL_Surface* screen, TTF_Font* font, const std::string& value, int x,
          int y, SDL_Color color) {
  SDL_Surface* glyph = TTF_RenderUTF8_Blended(font, value.c_str(), color);
  if (!glyph) return;
  SDL_Rect dst = {static_cast<Sint16>(x), static_cast<Sint16>(y), 0, 0};
  SDL_BlitSurface(glyph, 0, screen, &dst);
  SDL_FreeSurface(glyph);
}

size_t DiscardCurl(void* data, size_t size, size_t count, void*) {
  (void)data;
  return size * count;
}

bool CheckPopularApi(std::string* status) {
  CURL* curl = curl_easy_init();
  if (!curl) {
    *status = "Network check: curl initialization failed";
    return false;
  }
  curl_easy_setopt(curl, CURLOPT_URL,
                   "https://api.bilibili.com/x/web-interface/popular?ps=1&pn=1");
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, DiscardCurl);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 8L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "wiliwili-lite-f1c200s/0.1");
  CURLcode result = curl_easy_perform(curl);
  long code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
  curl_easy_cleanup(curl);
  if (result == CURLE_OK && code == 200) {
    *status = "Network check: Bilibili public API reachable (HTTP 200)";
    return true;
  }
  *status = "Network check failed: curl=" + std::to_string(result) +
            " HTTP=" + std::to_string(code);
  return false;
}

void StartPlayer(std::string* status) {
  const char* command = std::getenv("WILIWILI_LITE_PLAYER");
  if (!command || !command[0]) {
    *status = "Set WILIWILI_LITE_PLAYER before starting playback";
    return;
  }
  std::string background = std::string(command) + " >/tmp/wiliwili-lite-player.log 2>&1 &";
  int result = std::system(background.c_str());
  *status = result == 0 ? "Playback backend started" : "Playback backend failed to start";
}

void RunSelectedAction(int selected, std::string* status) {
  switch (selected) {
    case 0:
      CheckPopularApi(status);
      break;
    case 1:
      *status = "Continue watching: history adapter is not implemented yet";
      break;
    case 2:
      *status = "Search: search adapter is not implemented yet";
      break;
    case 3:
      StartPlayer(status);
      break;
    default:
      *status = "Invalid menu selection";
      break;
  }
}

}  // namespace

int main() {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) return 1;
  if (TTF_Init() != 0) {
    SDL_Quit();
    return 1;
  }
  curl_global_init(CURL_GLOBAL_DEFAULT);

  SDL_Surface* screen = SDL_SetVideoMode(kWidth, kHeight, 32, SDL_SWSURFACE);
  TTF_Font* title_font = TTF_OpenFont(kFontPath, 26);
  TTF_Font* body_font = TTF_OpenFont(kFontPath, 17);
  if (!screen || !title_font || !body_font) {
    if (body_font) TTF_CloseFont(body_font);
    if (title_font) TTF_CloseFont(title_font);
    curl_global_cleanup();
    TTF_Quit();
    SDL_Quit();
    return 1;
  }

  const int item_count = sizeof(kItems) / sizeof(kItems[0]);
  int selected = 0;
  bool running = true;
  std::string status = "Select an item, then click it again or press Enter.";

  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) running = false;
      if (event.type == SDL_KEYDOWN) {
        SDLKey key = event.key.keysym.sym;
        if (key == SDLK_ESCAPE || key == SDLK_q) running = false;
        if (key == SDLK_UP) selected = (selected + item_count - 1) % item_count;
        if (key == SDLK_DOWN) selected = (selected + 1) % item_count;
        if (key == SDLK_r) CheckPopularApi(&status);
        if (key == SDLK_RETURN || key == SDLK_p) RunSelectedAction(selected, &status);
      }
      if (event.type == SDL_MOUSEBUTTONDOWN) {
        int row = (event.button.y - 132) / 64;
        if (event.button.button == SDL_BUTTON_LEFT && row >= 0 && row < item_count) {
          if (row == selected) {
            RunSelectedAction(selected, &status);
          } else {
            selected = row;
            status = std::string("Selected: ") + kItems[selected].title +
                     ". Click again or press Enter.";
          }
        }
      }
    }

    Uint32 background = SDL_MapRGB(screen->format, 20, 25, 35);
    Uint32 panel = SDL_MapRGB(screen->format, 34, 43, 58);
    Uint32 selected_panel = SDL_MapRGB(screen->format, 31, 105, 148);
    Uint32 accent = SDL_MapRGB(screen->format, 255, 97, 117);
    SDL_FillRect(screen, 0, background);
    SDL_Rect header = {0, 0, kWidth, 92};
    Fill(screen, header, accent);
    Text(screen, title_font, "wiliwili lite", 32, 24, Color(255, 255, 255));
    Text(screen, body_font, "F1C200S SDL and Cedar playback prototype", 34, 58,
         Color(255, 230, 232));

    for (int i = 0; i < item_count; ++i) {
      SDL_Rect row = {28, static_cast<Sint16>(132 + i * 64), 744, 54};
      Fill(screen, row, i == selected ? selected_panel : panel);
      Text(screen, body_font, kItems[i].title, 48, 140 + i * 64, Color(245, 247, 250));
      Text(screen, body_font, kItems[i].detail, 270, 140 + i * 64,
           Color(185, 198, 215));
    }

    SDL_Rect footer = {28, 412, 744, 42};
    Fill(screen, footer, panel);
    Text(screen, body_font, status, 42, 423, Color(214, 225, 238));
    SDL_Flip(screen);
    SDL_Delay(16);
  }

  TTF_CloseFont(body_font);
  TTF_CloseFont(title_font);
  curl_global_cleanup();
  TTF_Quit();
  SDL_Quit();
  return 0;
}
