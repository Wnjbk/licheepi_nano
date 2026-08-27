#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <curl/curl.h>

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

const int kWidth = 800;
const int kHeight = 480;
const char* kFontPath = "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc";

struct Item {
  const char* title;
  const char* detail;
};

const Item kItems[] = {
    {"Popular videos", "Browse Bilibili public popular feed"},
    {"Continue watching", "Local history adapter placeholder"},
    {"Search", "Keyboard and touch search adapter placeholder"},
    {"Network playback", "External Cedar or GStreamer player backend"},
};

enum Page {
  kHome,
  kPopular,
  kDetail,
};

struct PopularFetch {
  std::mutex mutex;
  std::vector<std::string> titles;
  std::string error;
  std::atomic<bool> done;
  PopularFetch() : done(false) {}
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

size_t StoreCurl(void* data, size_t size, size_t count, void* user_data) {
  std::string* response = static_cast<std::string*>(user_data);
  response->append(static_cast<const char*>(data), size * count);
  return size * count;
}

void AppendUtf8(std::string* output, unsigned value) {
  if (value <= 0x7f) {
    output->push_back(static_cast<char>(value));
  } else if (value <= 0x7ff) {
    output->push_back(static_cast<char>(0xc0 | (value >> 6)));
    output->push_back(static_cast<char>(0x80 | (value & 0x3f)));
  } else {
    output->push_back(static_cast<char>(0xe0 | (value >> 12)));
    output->push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
    output->push_back(static_cast<char>(0x80 | (value & 0x3f)));
  }
}

std::string JsonUnescape(const std::string& value) {
  std::string output;
  for (size_t i = 0; i < value.size(); ++i) {
    if (value[i] != '\\' || i + 1 >= value.size()) {
      output.push_back(value[i]);
      continue;
    }
    const char escaped = value[++i];
    if (escaped == 'n') output.push_back(' ');
    else if (escaped == 'r' || escaped == 't') output.push_back(' ');
    else if (escaped == '"' || escaped == '\\' || escaped == '/') output.push_back(escaped);
    else if (escaped == 'u' && i + 4 < value.size()) {
      unsigned code = 0;
      bool valid = true;
      for (int digit = 0; digit < 4; ++digit) {
        const char c = value[i + 1 + digit];
        code <<= 4;
        if (c >= '0' && c <= '9') code |= c - '0';
        else if (c >= 'a' && c <= 'f') code |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') code |= c - 'A' + 10;
        else valid = false;
      }
      if (valid && (code < 0xd800 || code > 0xdfff)) AppendUtf8(&output, code);
      i += 4;
    } else {
      output.push_back(escaped);
    }
  }
  return output;
}

std::vector<std::string> ExtractTitles(const std::string& response) {
  std::vector<std::string> titles;
  const std::string key = "\"title\":\"";
  size_t cursor = 0;
  while (titles.size() < 12) {
    const size_t begin = response.find(key, cursor);
    if (begin == std::string::npos) break;
    size_t end = begin + key.size();
    bool escaped = false;
    for (; end < response.size(); ++end) {
      if (!escaped && response[end] == '"') break;
      if (!escaped && response[end] == '\\') escaped = true;
      else escaped = false;
    }
    if (end == response.size()) break;
    titles.push_back(JsonUnescape(response.substr(begin + key.size(), end - begin - key.size())));
    cursor = end + 1;
  }
  return titles;
}

bool FetchPopular(std::vector<std::string>* titles, std::string* error) {
  CURL* curl = curl_easy_init();
  if (!curl) {
    *error = "curl initialization failed";
    return false;
  }
  std::string response;
  curl_easy_setopt(curl, CURLOPT_URL,
                   "https://api.bilibili.com/x/web-interface/popular?ps=12&pn=1");
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StoreCurl);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 8L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "wiliwili-lite-f1c200s/0.2");
  const CURLcode result = curl_easy_perform(curl);
  long code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
  curl_easy_cleanup(curl);
  if (result != CURLE_OK || code != 200) {
    *error = "Request failed: curl=" + std::to_string(result) +
             " HTTP=" + std::to_string(code);
    return false;
  }
  *titles = ExtractTitles(response);
  if (titles->empty()) {
    *error = "The popular feed response had no readable titles";
    return false;
  }
  return true;
}

void StartPlayer(std::string* status) {
  const char* command = std::getenv("WILIWILI_LITE_PLAYER");
  if (!command || !command[0]) {
    *status = "Playback backend is not configured";
    return;
  }
  const std::string background =
      std::string(command) + " >/tmp/wiliwili-lite-player.log 2>&1 &";
  const int result = std::system(background.c_str());
  *status = result == 0 ? "Playback backend started" : "Playback backend failed to start";
}

void DrawHeader(SDL_Surface* screen, TTF_Font* title_font, TTF_Font* body_font,
                const std::string& subtitle) {
  const Uint32 accent = SDL_MapRGB(screen->format, 255, 97, 117);
  const SDL_Rect header = {0, 0, kWidth, 92};
  Fill(screen, header, accent);
  Text(screen, title_font, "wiliwili lite", 32, 24, Color(255, 255, 255));
  Text(screen, body_font, subtitle, 34, 58, Color(255, 230, 232));
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
  if (!screen || !title_font || !body_font) return 1;

  const int item_count = sizeof(kItems) / sizeof(kItems[0]);
  int selected = 0;
  Page page = kHome;
  std::string detail_title;
  std::string status = "Select an item, then click it again or press Enter.";
  PopularFetch fetch;
  std::thread worker;
  bool fetch_active = false;
  bool running = true;

  while (running) {
    if (fetch_active && fetch.done.load()) {
      worker.join();
      fetch_active = false;
      std::lock_guard<std::mutex> lock(fetch.mutex);
      status = fetch.error.empty() ? "Popular list loaded" : fetch.error;
      selected = 0;
    }

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) running = false;
      if (event.type == SDL_KEYDOWN) {
        const SDLKey key = event.key.keysym.sym;
        if (key == SDLK_ESCAPE || key == SDLK_q) {
          if (page == kHome) running = false;
          else page = kHome;
        }
        if (key == SDLK_UP) selected = selected > 0 ? selected - 1 : 0;
        if (key == SDLK_DOWN) ++selected;
        if (key == SDLK_RETURN || key == SDLK_p) {
          if (page == kHome && selected == 0) {
            page = kPopular;
          } else if (page == kHome && selected == 3) {
            StartPlayer(&status);
          } else if (page == kPopular) {
            std::lock_guard<std::mutex> lock(fetch.mutex);
            if (!fetch.titles.empty()) {
              selected %= static_cast<int>(fetch.titles.size());
              detail_title = fetch.titles[selected];
              page = kDetail;
            }
          }
        }
      }
      if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        if (page == kHome) {
          const int row = (event.button.y - 132) / 64;
          if (row >= 0 && row < item_count) {
            if (row == selected) {
              if (row == 0) page = kPopular;
              else if (row == 3) StartPlayer(&status);
              else status = std::string(kItems[row].title) + " is not implemented yet";
            } else {
              selected = row;
              status = std::string("Selected: ") + kItems[row].title;
            }
          }
        } else if (page == kPopular) {
          const int row = (event.button.y - 122) / 50;
          std::lock_guard<std::mutex> lock(fetch.mutex);
          if (row >= 0 && row < static_cast<int>(fetch.titles.size())) {
            if (row == selected) {
              detail_title = fetch.titles[row];
              page = kDetail;
            } else {
              selected = row;
              status = "Selected video. Click again or press Enter.";
            }
          }
        } else if (page == kDetail) {
          page = kPopular;
        }
      }
    }

    if (page == kHome && selected >= item_count) selected = item_count - 1;
    if (page == kPopular && !fetch_active && fetch.titles.empty() && fetch.error.empty()) {
      fetch_active = true;
      worker = std::thread([&fetch]() {
        std::vector<std::string> titles;
        std::string error;
        FetchPopular(&titles, &error);
        std::lock_guard<std::mutex> lock(fetch.mutex);
        fetch.titles.swap(titles);
        fetch.error = error;
        fetch.done.store(true);
      });
      status = "Loading popular videos...";
    }

    const Uint32 background = SDL_MapRGB(screen->format, 20, 25, 35);
    const Uint32 panel = SDL_MapRGB(screen->format, 34, 43, 58);
    const Uint32 selected_panel = SDL_MapRGB(screen->format, 31, 105, 148);
    SDL_FillRect(screen, 0, background);

    if (page == kHome) {
      DrawHeader(screen, title_font, body_font, "F1C200S SDL and Cedar playback prototype");
      for (int i = 0; i < item_count; ++i) {
        const SDL_Rect row = {28, static_cast<Sint16>(132 + i * 64), 744, 54};
        Fill(screen, row, i == selected ? selected_panel : panel);
        Text(screen, body_font, kItems[i].title, 48, 140 + i * 64, Color(245, 247, 250));
        Text(screen, body_font, kItems[i].detail, 270, 140 + i * 64,
             Color(185, 198, 215));
      }
    } else if (page == kPopular) {
      DrawHeader(screen, title_font, body_font, "Popular videos - Esc returns to home");
      std::lock_guard<std::mutex> lock(fetch.mutex);
      for (size_t i = 0; i < fetch.titles.size() && i < 6; ++i) {
        const SDL_Rect row = {28, static_cast<Sint16>(122 + i * 50), 744, 42};
        Fill(screen, row, static_cast<int>(i) == selected ? selected_panel : panel);
        Text(screen, body_font, std::to_string(i + 1) + ". " + fetch.titles[i],
             44, 132 + static_cast<int>(i) * 50, Color(245, 247, 250));
      }
      if (fetch_active) Text(screen, body_font, "Loading from Bilibili public API...", 44, 140,
                             Color(245, 247, 250));
    } else {
      DrawHeader(screen, title_font, body_font, "Video detail - click or Esc to return");
      Text(screen, title_font, detail_title, 42, 150, Color(245, 247, 250));
      Text(screen, body_font, "Video URL parsing and Cedar playback are the next adapter.",
           42, 224, Color(185, 198, 215));
    }

    const SDL_Rect footer = {28, 412, 744, 42};
    Fill(screen, footer, panel);
    Text(screen, body_font, status, 42, 423, Color(214, 225, 238));
    SDL_Flip(screen);
    SDL_Delay(16);
  }

  if (worker.joinable()) worker.join();
  TTF_CloseFont(body_font);
  TTF_CloseFont(title_font);
  curl_global_cleanup();
  TTF_Quit();
  SDL_Quit();
  return 0;
}
