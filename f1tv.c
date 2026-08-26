#include <SDL/SDL.h>
#include <SDL/SDL_gfxPrimitives.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_CHANNELS 48
#define NAME_LEN 64
#define URL_LEN 512

struct channel {
    char name[NAME_LEN];
    char url[URL_LEN];
};

static struct channel channels[MAX_CHANNELS];
static int channel_count;
static int selected;

static char *trim(char *text)
{
    char *end;

    while (isspace((unsigned char)*text))
        text++;
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1]))
        *--end = '\0';
    return text;
}

static int load_channels(const char *path)
{
    FILE *file;
    char line[NAME_LEN + URL_LEN + 16];

    channel_count = 0;
    selected = 0;
    file = fopen(path, "r");
    if (file == NULL)
        return -1;

    while (fgets(line, sizeof(line), file) != NULL && channel_count < MAX_CHANNELS) {
        char *name = trim(line);
        char *separator;
        char *url;

        if (*name == '\0' || *name == '#')
            continue;
        separator = strchr(name, '|');
        if (separator == NULL)
            continue;
        *separator++ = '\0';
        url = trim(separator);
        name = trim(name);
        if (*name == '\0' || *url == '\0')
            continue;
        snprintf(channels[channel_count].name, NAME_LEN, "%s", name);
        snprintf(channels[channel_count].url, URL_LEN, "%s", url);
        channel_count++;
    }
    fclose(file);
    return channel_count;
}

static void draw_text(SDL_Surface *screen, int x, int y, const char *text,
                      Uint8 red, Uint8 green, Uint8 blue)
{
    stringRGBA(screen, x, y, text, red, green, blue, 255);
}

static void draw_screen(SDL_Surface *screen, const char *channel_path)
{
    int i;
    int first;
    int row;
    char status[96];

    boxRGBA(screen, 0, 0, screen->w, screen->h, 20, 25, 32, 255);
    boxRGBA(screen, 0, 0, screen->w, 38, 19, 97, 148, 255);
    draw_text(screen, 14, 14, "F1TV  Network Television", 255, 255, 255);
    draw_text(screen, 14, 50, "Channel sources", 186, 208, 224);

    if (channel_count == 0) {
        draw_text(screen, 18, 82, "No channels configured.", 255, 208, 112);
        draw_text(screen, 18, 98, "Create /root/roms/tv/channels.txt", 212, 220, 228);
    } else {
        first = selected - 11;
        if (first < 0)
            first = 0;
        if (first > channel_count - 23)
            first = channel_count > 23 ? channel_count - 23 : 0;
        for (i = first, row = 0; i < channel_count && row < 23; i++, row++) {
            int y = 76 + row * 15;
            if (i == selected) {
                boxRGBA(screen, 10, y - 2, screen->w - 10, y + 11,
                        35, 132, 186, 255);
                draw_text(screen, 18, y, channels[i].name, 255, 255, 255);
            } else {
                draw_text(screen, 18, y, channels[i].name, 214, 224, 232);
            }
        }
    }

    boxRGBA(screen, 0, screen->h - 36, screen->w, screen->h,
            12, 15, 20, 255);
    snprintf(status, sizeof(status), "Enter/click: play   Up/Down: choose   F5: reload   Esc: exit");
    draw_text(screen, 12, screen->h - 23, status, 174, 194, 208);
    draw_text(screen, 12, screen->h - 11, channel_path, 105, 135, 156);
    SDL_Flip(screen);
}

static void play_channel(const struct channel *channel)
{
    const char *player = getenv("F1TV_PLAYER");
    pid_t pid;

    if (player == NULL || *player == '\0')
        player = "/root/candidates/mplayer_cedar_20260825/mplayer-cedar";

    pid = fork();
    if (pid == 0) {
        execl(player, player, "-demuxer", "lavf", "-vc", "cedarh264",
              "-vo", "cedar_drm:fit",
              channel->url, (char *)NULL);
        execl("/usr/bin/mplayer", "/usr/bin/mplayer", channel->url,
              (char *)NULL);
        _exit(127);
    }
    if (pid > 0)
        (void)waitpid(pid, NULL, 0);
}

int main(int argc, char **argv)
{
    const char *channel_path = getenv("F1TV_CHANNELS");
    const char *autoplay_text = getenv("F1TV_AUTOPLAY_INDEX");
    SDL_Surface *screen;
    int autoplay_index = -1;
    int autoplay_pending;
    int running = 1;

    if (channel_path == NULL || *channel_path == '\0')
        channel_path = argc > 1 ? argv[1] : "/root/roms/tv/channels.txt";
    if (autoplay_text != NULL && *autoplay_text != '\0')
        autoplay_index = atoi(autoplay_text);
    autoplay_pending = autoplay_index >= 0;

    while (running) {
        SDL_Event event;

        (void)load_channels(channel_path);
        if (autoplay_pending && autoplay_index < channel_count) {
            selected = autoplay_index;
            autoplay_pending = 0;
            play_channel(&channels[selected]);
            continue;
        }
        autoplay_pending = 0;
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
            return 1;
        }
        screen = SDL_SetVideoMode(640, 480, 16, SDL_SWSURFACE | SDL_FULLSCREEN);
        if (screen == NULL) {
            fprintf(stderr, "SDL video failed: %s\n", SDL_GetError());
            SDL_Quit();
            return 1;
        }
        draw_screen(screen, channel_path);

        for (;;) {
            SDL_WaitEvent(&event);
            if (event.type == SDL_QUIT) {
                running = 0;
                break;
            }
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = 0;
                    break;
                }
                if (event.key.keysym.sym == SDLK_F5) {
                    (void)load_channels(channel_path);
                    draw_screen(screen, channel_path);
                }
                if (event.key.keysym.sym == SDLK_UP && selected > 0) {
                    selected--;
                    draw_screen(screen, channel_path);
                }
                if (event.key.keysym.sym == SDLK_DOWN && selected + 1 < channel_count) {
                    selected++;
                    draw_screen(screen, channel_path);
                }
                if ((event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) && channel_count) {
                    SDL_Quit();
                    play_channel(&channels[selected]);
                    break;
                }
            }
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                int row = (event.button.y - 74) / 15;
                int first = selected - 11;
                int index;
                if (first < 0)
                    first = 0;
                if (first > channel_count - 23)
                    first = channel_count > 23 ? channel_count - 23 : 0;
                index = first + row;
                if (row >= 0 && index >= 0 && index < channel_count) {
                    selected = index;
                    SDL_Quit();
                    play_channel(&channels[selected]);
                    break;
                }
            }
        }
        SDL_Quit();
    }
    return 0;
}
