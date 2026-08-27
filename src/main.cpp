#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include <curl/curl.h>
#include <qrencode.h>

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <fstream>
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

struct Video {
  std::string bvid;
  std::string title;
  std::string owner;
  std::string owner_face_url;
  std::string description;
  std::string cover_url;
  std::string cid;
  std::string views;
};

const Item kItems[] = {
    {"Popular videos", "Browse Bilibili public popular feed"},
    {"Continue watching", "Local history adapter placeholder"},
    {"Search", "Keyboard and touch search adapter placeholder"},
    {"Network playback", "External Cedar or GStreamer player backend"},
    {"Account / QR login", "Authorize Bilibili playback access"},
};

enum Page {
  kHome,
  kPopular,
  kDetail,
  kLogin,
};

struct PopularFetch {
  std::mutex mutex;
  std::vector<Video> videos;
  std::string error;
  std::atomic<bool> done;
  PopularFetch() : done(false) {}
};

struct CoverFetch {
  std::mutex mutex;
  std::string bytes;
  std::string error;
  std::atomic<bool> done;
  CoverFetch() : done(false) {}
};

struct LoginFetch {
  std::mutex mutex;
  std::string qr_url;
  std::string qr_key;
  std::string error;
  std::atomic<bool> done;
  LoginFetch() : done(false) {}
};

struct LoginPoll {
  std::mutex mutex;
  std::string error;
  bool success;
  std::atomic<bool> done;
  LoginPoll() : success(false), done(false) {}
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

void TextWrapped(SDL_Surface* screen, TTF_Font* font, const std::string& value,
                 int x, int y, int max_width, int line_height, int max_lines,
                 SDL_Color color) {
  std::string line;
  int line_count = 0;
  for (size_t offset = 0; offset < value.size();) {
    size_t length = 1;
    const unsigned char lead = static_cast<unsigned char>(value[offset]);
    if ((lead & 0xf0) == 0xf0) length = 4;
    else if ((lead & 0xe0) == 0xe0) length = 3;
    else if ((lead & 0xc0) == 0xc0) length = 2;
    if (offset + length > value.size()) length = 1;
    const std::string character = value.substr(offset, length);
    int width = 0;
    int height = 0;
    TTF_SizeUTF8(font, (line + character).c_str(), &width, &height);
    if (width > max_width && !line.empty()) {
      Text(screen, font, line, x, y + line_count * line_height, color);
      line.clear();
      ++line_count;
      if (line_count == max_lines) return;
    }
    line += character;
    offset += length;
  }
  if (!line.empty() && line_count < max_lines)
    Text(screen, font, line, x, y + line_count * line_height, color);
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

std::string JsonString(const std::string& object, const std::string& key) {
  const std::string marker = "\"" + key + "\":\"";
  const size_t start = object.find(marker);
  if (start == std::string::npos) return "";
  const size_t first = start + marker.size();
  bool escaped = false;
  for (size_t end = first; end < object.size(); ++end) {
    if (!escaped && object[end] == '"')
      return JsonUnescape(object.substr(first, end - first));
    if (!escaped && object[end] == '\\') escaped = true;
    else escaped = false;
  }
  return "";
}

std::string JsonNumber(const std::string& object, const std::string& key) {
  const std::string marker = "\"" + key + "\":";
  const size_t start = object.find(marker);
  if (start == std::string::npos) return "";
  const size_t first = start + marker.size();
  size_t last = first;
  while (last < object.size() && object[last] >= '0' && object[last] <= '9') ++last;
  return object.substr(first, last - first);
}

std::vector<std::string> JsonObjectsInList(const std::string& response) {
  std::vector<std::string> objects;
  const size_t list = response.find("\"list\":[");
  if (list == std::string::npos) return objects;
  size_t cursor = response.find('{', list);
  while (cursor != std::string::npos && objects.size() < 12) {
    int depth = 0;
    bool in_string = false;
    bool escaped = false;
    size_t end = cursor;
    for (; end < response.size(); ++end) {
      const char c = response[end];
      if (in_string) {
        if (!escaped && c == '"') in_string = false;
        if (!escaped && c == '\\') escaped = true;
        else escaped = false;
        continue;
      }
      if (c == '"') in_string = true;
      else if (c == '{') ++depth;
      else if (c == '}' && --depth == 0) break;
    }
    if (end == response.size()) break;
    objects.push_back(response.substr(cursor, end - cursor + 1));
    cursor = response.find('{', end + 1);
  }
  return objects;
}

std::vector<Video> ExtractVideos(const std::string& response) {
  std::vector<Video> videos;
  const std::vector<std::string> objects = JsonObjectsInList(response);
  for (size_t i = 0; i < objects.size(); ++i) {
    Video video;
    video.bvid = JsonString(objects[i], "bvid");
    video.title = JsonString(objects[i], "title");
    video.owner = JsonString(objects[i], "name");
    video.owner_face_url = JsonString(objects[i], "face");
    video.description = JsonString(objects[i], "desc");
    video.cover_url = JsonString(objects[i], "pic");
    video.cid = JsonNumber(objects[i], "cid");
    const size_t stat = objects[i].find("\"stat\":{");
    video.views = stat == std::string::npos ? "" : JsonNumber(objects[i].substr(stat), "view");
    if (!video.bvid.empty() && !video.title.empty()) videos.push_back(video);
  }
  return videos;
}

bool FetchPopular(std::vector<Video>* videos, std::string* error) {
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
  *videos = ExtractVideos(response);
  if (videos->empty()) {
    *error = "The popular feed response had no readable videos";
    return false;
  }
  return true;
}

bool FetchCover(const std::string& url, std::string* bytes, std::string* error) {
  CURL* curl = curl_easy_init();
  if (!curl) {
    *error = "Cover download: curl initialization failed";
    return false;
  }
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StoreCurl);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, bytes);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 8L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "wiliwili-lite-f1c200s/0.3");
  const CURLcode result = curl_easy_perform(curl);
  long code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
  curl_easy_cleanup(curl);
  if (result != CURLE_OK || code != 200 || bytes->empty()) {
    *error = "Cover download failed";
    return false;
  }
  return true;
}

bool FetchLoginQr(std::string* qr_url, std::string* qr_key, std::string* error) {
  CURL* curl = curl_easy_init();
  if (!curl) {
    *error = "QR login: curl initialization failed";
    return false;
  }
  std::string response;
  curl_easy_setopt(curl, CURLOPT_URL,
                   "https://passport.bilibili.com/x/passport-login/web/qrcode/generate");
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StoreCurl);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 8L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 wiliwili-lite-f1c200s/0.4");
  const CURLcode result = curl_easy_perform(curl);
  long code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
  curl_easy_cleanup(curl);
  *qr_url = JsonString(response, "url");
  *qr_key = JsonString(response, "qrcode_key");
  if (result != CURLE_OK || code != 200 || qr_url->empty() || qr_key->empty()) {
    *error = "QR login request failed";
    return false;
  }
  return true;
}

bool PollLoginQr(const std::string& qr_key, std::string* error) {
  const char* home = std::getenv("HOME");
  const std::string cookie_file =
      std::string(home && home[0] ? home : "/tmp") + "/.wiliwili-lite-cookies.txt";
  CURL* curl = curl_easy_init();
  if (!curl) return false;
  std::string response;
  char* escaped_key = curl_easy_escape(curl, qr_key.c_str(), qr_key.size());
  const std::string url = std::string(
      "https://passport.bilibili.com/x/passport-login/web/qrcode/poll?qrcode_key=") +
      (escaped_key ? escaped_key : "");
  if (escaped_key) curl_free(escaped_key);
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StoreCurl);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookie_file.c_str());
  curl_easy_setopt(curl, CURLOPT_COOKIEJAR, cookie_file.c_str());
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 wiliwili-lite-f1c200s/0.4");
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
  const CURLcode result = curl_easy_perform(curl);
  curl_easy_cleanup(curl);
  const size_t data = response.find("\"data\":{");
  const std::string status_code =
      data == std::string::npos ? "" : JsonNumber(response.substr(data), "code");
  if (result != CURLE_OK) {
    *error = "QR login polling failed";
    return false;
  }
  if (status_code == "0") return true;
  if (status_code == "86101") *error = "Waiting for scan";
  else if (status_code == "86090") *error = "Confirm login on phone";
  else if (status_code == "86038") *error = "QR code expired";
  else *error = "QR login status " + status_code;
  return false;
}

bool HasSavedLogin() {
  const char* home = std::getenv("HOME");
  const std::string cookie_file =
      std::string(home && home[0] ? home : "/tmp") + "/.wiliwili-lite-cookies.txt";
  std::ifstream input(cookie_file.c_str());
  std::string line;
  while (std::getline(input, line)) {
    if (line.find("SESSDATA") != std::string::npos) return true;
  }
  return false;
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
  Video detail_video;
  std::string status = "Select an item, then click it again or press Enter.";
  PopularFetch fetch;
  std::thread worker;
  bool fetch_active = false;
  CoverFetch cover_fetch;
  std::thread cover_worker;
  bool cover_active = false;
  SDL_Surface* cover = 0;
  CoverFetch avatar_fetch;
  std::thread avatar_worker;
  bool avatar_active = false;
  SDL_Surface* avatar = 0;
  LoginFetch login_fetch;
  std::thread login_worker;
  bool login_active = false;
  QRcode* login_qr = 0;
  Uint32 next_login_poll = 0;
  bool logged_in = HasSavedLogin();
  LoginPoll login_poll;
  std::thread login_poll_worker;
  bool login_poll_active = false;
  bool running = true;

  while (running) {
    if (fetch_active && fetch.done.load()) {
      worker.join();
      fetch_active = false;
      std::lock_guard<std::mutex> lock(fetch.mutex);
      status = fetch.error.empty() ? "Popular list loaded" : fetch.error;
      selected = 0;
    }
    if (cover_active && cover_fetch.done.load()) {
      cover_worker.join();
      cover_active = false;
      std::lock_guard<std::mutex> lock(cover_fetch.mutex);
      SDL_RWops* rw = SDL_RWFromConstMem(cover_fetch.bytes.data(), cover_fetch.bytes.size());
      SDL_Surface* decoded = rw ? IMG_Load_RW(rw, 1) : 0;
      if (decoded) {
        if (cover) SDL_FreeSurface(cover);
        cover = SDL_DisplayFormat(decoded);
        SDL_FreeSurface(decoded);
        status = cover ? "Cover loaded" : "Cover format conversion failed";
      } else {
        status = cover_fetch.error.empty() ? "Cover decode failed" : cover_fetch.error;
      }
    }
    if (avatar_active && avatar_fetch.done.load()) {
      avatar_worker.join();
      avatar_active = false;
      std::lock_guard<std::mutex> lock(avatar_fetch.mutex);
      SDL_RWops* rw = SDL_RWFromConstMem(avatar_fetch.bytes.data(), avatar_fetch.bytes.size());
      SDL_Surface* decoded = rw ? IMG_Load_RW(rw, 1) : 0;
      if (decoded) {
        if (avatar) SDL_FreeSurface(avatar);
        avatar = SDL_DisplayFormat(decoded);
        SDL_FreeSurface(decoded);
      }
    }
    if (login_active && login_fetch.done.load()) {
      login_worker.join();
      login_active = false;
      std::lock_guard<std::mutex> lock(login_fetch.mutex);
      if (login_fetch.error.empty() && !login_fetch.qr_url.empty()) {
        if (login_qr) QRcode_free(login_qr);
        login_qr = QRcode_encodeString8bit(login_fetch.qr_url.c_str(), 0, QR_ECLEVEL_M);
        status = login_qr ? "Scan QR code with Bilibili, then confirm on phone" :
                            "QR code generation failed";
        next_login_poll = SDL_GetTicks() + 2000;
      } else {
        status = login_fetch.error.empty() ? "QR login request failed" : login_fetch.error;
      }
    }
    if (login_poll_active && login_poll.done.load()) {
      login_poll_worker.join();
      login_poll_active = false;
      std::lock_guard<std::mutex> lock(login_poll.mutex);
      if (login_poll.success) {
        logged_in = true;
        status = "Login complete. Playback authorization cookie saved.";
        if (login_qr) { QRcode_free(login_qr); login_qr = 0; }
      } else {
        status = login_poll.error;
        next_login_poll = SDL_GetTicks() + 2000;
      }
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
          } else if (page == kHome && selected == 4) {
            page = kLogin;
          } else if (page == kPopular) {
            std::lock_guard<std::mutex> lock(fetch.mutex);
            if (!fetch.videos.empty()) {
              selected %= static_cast<int>(fetch.videos.size());
              detail_video = fetch.videos[selected];
              if (cover) { SDL_FreeSurface(cover); cover = 0; }
              if (avatar) { SDL_FreeSurface(avatar); avatar = 0; }
              cover_fetch.bytes.clear();
              cover_fetch.error.clear();
              cover_fetch.done.store(false);
              avatar_fetch.bytes.clear();
              avatar_fetch.error.clear();
              avatar_fetch.done.store(false);
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
              else if (row == 4) page = kLogin;
              else status = std::string(kItems[row].title) + " is not implemented yet";
            } else {
              selected = row;
              status = std::string("Selected: ") + kItems[row].title;
            }
          }
        } else if (page == kPopular) {
          const int row = (event.button.y - 122) / 50;
          std::lock_guard<std::mutex> lock(fetch.mutex);
          if (row >= 0 && row < static_cast<int>(fetch.videos.size())) {
            if (row == selected) {
              detail_video = fetch.videos[row];
              if (cover) { SDL_FreeSurface(cover); cover = 0; }
              if (avatar) { SDL_FreeSurface(avatar); avatar = 0; }
              cover_fetch.bytes.clear();
              cover_fetch.error.clear();
              cover_fetch.done.store(false);
              avatar_fetch.bytes.clear();
              avatar_fetch.error.clear();
              avatar_fetch.done.store(false);
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
    if (page == kPopular && !fetch_active && fetch.videos.empty() && fetch.error.empty()) {
      fetch_active = true;
      worker = std::thread([&fetch]() {
        std::vector<Video> videos;
        std::string error;
        FetchPopular(&videos, &error);
        std::lock_guard<std::mutex> lock(fetch.mutex);
        fetch.videos.swap(videos);
        fetch.error = error;
        fetch.done.store(true);
      });
      status = "Loading popular videos...";
    }
    if (page == kLogin && !login_active && !login_qr && !login_fetch.done.load()) {
      login_active = true;
      login_worker = std::thread([&login_fetch]() {
        std::string qr_url;
        std::string qr_key;
        std::string error;
        FetchLoginQr(&qr_url, &qr_key, &error);
        std::lock_guard<std::mutex> lock(login_fetch.mutex);
        login_fetch.qr_url = qr_url;
        login_fetch.qr_key = qr_key;
        login_fetch.error = error;
        login_fetch.done.store(true);
      });
      status = "Requesting Bilibili QR login...";
    }
    if (page == kLogin && login_qr && !login_poll_active &&
        SDL_GetTicks() >= next_login_poll) {
      login_poll_active = true;
      login_poll.error.clear();
      login_poll.success = false;
      login_poll.done.store(false);
      std::string qr_key;
      {
        std::lock_guard<std::mutex> lock(login_fetch.mutex);
        qr_key = login_fetch.qr_key;
      }
      login_poll_worker = std::thread([&login_poll, qr_key]() {
        std::string error;
        const bool success = PollLoginQr(qr_key, &error);
        std::lock_guard<std::mutex> lock(login_poll.mutex);
        login_poll.success = success;
        login_poll.error = error;
        login_poll.done.store(true);
      });
    }
    if (page == kDetail && !cover_active && !cover && !detail_video.cover_url.empty() &&
        !cover_fetch.done.load()) {
      cover_active = true;
      const std::string cover_url = detail_video.cover_url;
      cover_worker = std::thread([&cover_fetch, cover_url]() {
        std::string bytes;
        std::string error;
        FetchCover(cover_url, &bytes, &error);
        std::lock_guard<std::mutex> lock(cover_fetch.mutex);
        cover_fetch.bytes.swap(bytes);
        cover_fetch.error = error;
        cover_fetch.done.store(true);
      });
      status = "Loading cover...";
    }
    if (page == kDetail && !avatar_active && !avatar && !detail_video.owner_face_url.empty() &&
        !avatar_fetch.done.load()) {
      avatar_active = true;
      const std::string avatar_url = detail_video.owner_face_url;
      avatar_worker = std::thread([&avatar_fetch, avatar_url]() {
        std::string bytes;
        std::string error;
        FetchCover(avatar_url, &bytes, &error);
        std::lock_guard<std::mutex> lock(avatar_fetch.mutex);
        avatar_fetch.bytes.swap(bytes);
        avatar_fetch.error = error;
        avatar_fetch.done.store(true);
      });
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
      for (size_t i = 0; i < fetch.videos.size() && i < 6; ++i) {
        const SDL_Rect row = {28, static_cast<Sint16>(122 + i * 50), 744, 42};
        Fill(screen, row, static_cast<int>(i) == selected ? selected_panel : panel);
        Text(screen, body_font, std::to_string(i + 1) + ". " + fetch.videos[i].title,
             44, 132 + static_cast<int>(i) * 50, Color(245, 247, 250));
      }
      if (fetch_active) Text(screen, body_font, "Loading from Bilibili public API...", 44, 140,
                             Color(245, 247, 250));
    } else if (page == kLogin) {
      DrawHeader(screen, title_font, body_font, "Account login - Esc returns to home");
      if (logged_in) {
        Text(screen, title_font, "Login complete", 62, 145, Color(245, 247, 250));
        TextWrapped(screen, body_font,
                    "Bilibili session credentials are saved only in this user's home directory. "
                    "Return to a video detail page to request its playback stream.",
                    62, 205, 640, 28, 4, Color(185, 198, 215));
      } else if (login_qr) {
        const int modules = login_qr->width;
        const int scale = modules <= 37 ? 8 : 6;
        const int size = modules * scale;
        const int origin_x = 62;
        const int origin_y = 120;
        const SDL_Rect background = {static_cast<Sint16>(origin_x - 12),
                                     static_cast<Sint16>(origin_y - 12),
                                     static_cast<Uint16>(size + 24),
                                     static_cast<Uint16>(size + 24)};
        Fill(screen, background, SDL_MapRGB(screen->format, 255, 255, 255));
        const Uint32 black = SDL_MapRGB(screen->format, 0, 0, 0);
        for (int y = 0; y < modules; ++y) {
          for (int x = 0; x < modules; ++x) {
            if (login_qr->data[y * modules + x] & 1) {
              const SDL_Rect block = {static_cast<Sint16>(origin_x + x * scale),
                                      static_cast<Sint16>(origin_y + y * scale),
                                      static_cast<Uint16>(scale), static_cast<Uint16>(scale)};
              Fill(screen, block, black);
            }
          }
        }
        Text(screen, title_font, "Scan with Bilibili", 390, 142, Color(245, 247, 250));
        TextWrapped(screen, body_font, "After scanning, confirm the login on your phone. "
                    "The session cookie stays only on this device.",
                    390, 195, 340, 25, 4, Color(185, 198, 215));
      } else {
        Text(screen, body_font, "Loading QR code...", 62, 150, Color(245, 247, 250));
      }
    } else {
      DrawHeader(screen, title_font, body_font, "Video detail - click or Esc to return");
      if (cover) {
        SDL_Rect source = {0, 0, static_cast<Uint16>(cover->w), static_cast<Uint16>(cover->h)};
        SDL_Rect destination = {42, 150, 160, 90};
        SDL_SoftStretch(cover, &source, screen, &destination);
      }
      TextWrapped(screen, title_font, detail_video.title, 225, 150, 510, 40, 2,
                  Color(245, 247, 250));
      if (avatar) {
        SDL_Rect source = {0, 0, static_cast<Uint16>(avatar->w), static_cast<Uint16>(avatar->h)};
        SDL_Rect destination = {225, 245, 48, 48};
        SDL_SoftStretch(avatar, &source, screen, &destination);
      }
      Text(screen, body_font, "Uploader: " + detail_video.owner, 285, 248,
           Color(185, 198, 215));
      Text(screen, body_font, "Views: " + detail_video.views, 285, 272,
           Color(185, 198, 215));
      Text(screen, body_font, "BVID: " + detail_video.bvid + "    CID: " + detail_video.cid,
           42, 305, Color(185, 198, 215));
      TextWrapped(screen, body_font, detail_video.description, 42, 335, 700, 24, 3,
                  Color(214, 225, 238));
    }

    const SDL_Rect footer = {28, 412, 744, 42};
    Fill(screen, footer, panel);
    Text(screen, body_font, status, 42, 423, Color(214, 225, 238));
    SDL_Flip(screen);
    SDL_Delay(16);
  }

  if (worker.joinable()) worker.join();
  if (cover_worker.joinable()) cover_worker.join();
  if (avatar_worker.joinable()) avatar_worker.join();
  if (login_worker.joinable()) login_worker.join();
  if (login_poll_worker.joinable()) login_poll_worker.join();
  if (cover) SDL_FreeSurface(cover);
  if (avatar) SDL_FreeSurface(avatar);
  if (login_qr) QRcode_free(login_qr);
  TTF_CloseFont(body_font);
  TTF_CloseFont(title_font);
  curl_global_cleanup();
  TTF_Quit();
  SDL_Quit();
  return 0;
}
