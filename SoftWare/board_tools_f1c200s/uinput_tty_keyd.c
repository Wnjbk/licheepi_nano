#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <linux/uinput.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

struct key_spec {
    int code;
    bool shift;
};

static struct termios saved_termios;
static bool termios_saved;
static volatile sig_atomic_t stop_requested;

static void restore_terminal(void)
{
    if (termios_saved)
        tcsetattr(STDIN_FILENO, TCSANOW, &saved_termios);
}

static void handle_signal(int signo)
{
    (void)signo;
    stop_requested = 1;
}

static int set_raw_terminal(void)
{
    struct termios raw;

    if (!isatty(STDIN_FILENO)) {
        fprintf(stderr, "stdin is not a tty\n");
        return -1;
    }

    if (tcgetattr(STDIN_FILENO, &saved_termios) < 0) {
        perror("tcgetattr");
        return -1;
    }

    termios_saved = true;
    raw = saved_termios;
    cfmakeraw(&raw);
    raw.c_lflag |= ISIG;
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) < 0) {
        perror("tcsetattr");
        return -1;
    }

    atexit(restore_terminal);
    return 0;
}

static int read_byte_with_timeout(unsigned char *value, int timeout_ms)
{
    struct pollfd pfd;
    int ret;
    ssize_t nread;

    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;
    ret = poll(&pfd, 1, timeout_ms);
    if (ret <= 0)
        return ret;

    nread = read(STDIN_FILENO, value, 1);
    if (nread == 1)
        return 1;
    if (nread < 0 && errno == EINTR)
        return 0;
    return -1;
}

static int emit_event(int fd, uint16_t type, uint16_t code, int32_t value)
{
    struct input_event event;
    ssize_t written;

    memset(&event, 0, sizeof(event));
    event.type = type;
    event.code = code;
    event.value = value;

    written = write(fd, &event, sizeof(event));
    if (written != (ssize_t)sizeof(event)) {
        perror("write input_event");
        return -1;
    }

    return 0;
}

static int sync_events(int fd)
{
    return emit_event(fd, EV_SYN, SYN_REPORT, 0);
}

static int send_key(int fd, const struct key_spec *spec)
{
    if (spec->shift && emit_event(fd, EV_KEY, KEY_LEFTSHIFT, 1) < 0)
        return -1;
    if (emit_event(fd, EV_KEY, spec->code, 1) < 0)
        return -1;
    if (sync_events(fd) < 0)
        return -1;
    if (emit_event(fd, EV_KEY, spec->code, 0) < 0)
        return -1;
    if (spec->shift && emit_event(fd, EV_KEY, KEY_LEFTSHIFT, 0) < 0)
        return -1;
    if (sync_events(fd) < 0)
        return -1;
    return 0;
}

static int map_ascii_key(unsigned char ch, struct key_spec *spec)
{
    memset(spec, 0, sizeof(*spec));

    if (ch >= 'a' && ch <= 'z') {
        spec->code = KEY_A + (ch - 'a');
        return 0;
    }

    if (ch >= 'A' && ch <= 'Z') {
        spec->code = KEY_A + (ch - 'A');
        spec->shift = true;
        return 0;
    }

    switch (ch) {
    case '1': spec->code = KEY_1; return 0;
    case '2': spec->code = KEY_2; return 0;
    case '3': spec->code = KEY_3; return 0;
    case '4': spec->code = KEY_4; return 0;
    case '5': spec->code = KEY_5; return 0;
    case '6': spec->code = KEY_6; return 0;
    case '7': spec->code = KEY_7; return 0;
    case '8': spec->code = KEY_8; return 0;
    case '9': spec->code = KEY_9; return 0;
    case '0': spec->code = KEY_0; return 0;
    case '!': spec->code = KEY_1; spec->shift = true; return 0;
    case '@': spec->code = KEY_2; spec->shift = true; return 0;
    case '#': spec->code = KEY_3; spec->shift = true; return 0;
    case '$': spec->code = KEY_4; spec->shift = true; return 0;
    case '%': spec->code = KEY_5; spec->shift = true; return 0;
    case '^': spec->code = KEY_6; spec->shift = true; return 0;
    case '&': spec->code = KEY_7; spec->shift = true; return 0;
    case '*': spec->code = KEY_8; spec->shift = true; return 0;
    case '(': spec->code = KEY_9; spec->shift = true; return 0;
    case ')': spec->code = KEY_0; spec->shift = true; return 0;
    case '-': spec->code = KEY_MINUS; return 0;
    case '_': spec->code = KEY_MINUS; spec->shift = true; return 0;
    case '=': spec->code = KEY_EQUAL; return 0;
    case '+': spec->code = KEY_EQUAL; spec->shift = true; return 0;
    case '[': spec->code = KEY_LEFTBRACE; return 0;
    case '{': spec->code = KEY_LEFTBRACE; spec->shift = true; return 0;
    case ']': spec->code = KEY_RIGHTBRACE; return 0;
    case '}': spec->code = KEY_RIGHTBRACE; spec->shift = true; return 0;
    case ';': spec->code = KEY_SEMICOLON; return 0;
    case ':': spec->code = KEY_SEMICOLON; spec->shift = true; return 0;
    case '\'': spec->code = KEY_APOSTROPHE; return 0;
    case '"': spec->code = KEY_APOSTROPHE; spec->shift = true; return 0;
    case '`': spec->code = KEY_GRAVE; return 0;
    case '~': spec->code = KEY_GRAVE; spec->shift = true; return 0;
    case '\\': spec->code = KEY_BACKSLASH; return 0;
    case '|': spec->code = KEY_BACKSLASH; spec->shift = true; return 0;
    case ',': spec->code = KEY_COMMA; return 0;
    case '<': spec->code = KEY_COMMA; spec->shift = true; return 0;
    case '.': spec->code = KEY_DOT; return 0;
    case '>': spec->code = KEY_DOT; spec->shift = true; return 0;
    case '/': spec->code = KEY_SLASH; return 0;
    case '?': spec->code = KEY_SLASH; spec->shift = true; return 0;
    case ' ': spec->code = KEY_SPACE; return 0;
    case '\t': spec->code = KEY_TAB; return 0;
    case '\r':
    case '\n':
        spec->code = KEY_ENTER;
        return 0;
    case 0x08:
    case 0x7f:
        spec->code = KEY_BACKSPACE;
        return 0;
    default:
        return -1;
    }
}

static int map_escape_sequence(struct key_spec *spec)
{
    unsigned char second = 0;
    unsigned char third = 0;
    unsigned char seq[8];
    int len = 0;
    int value = 0;
    int ret;

    if (read_byte_with_timeout(&second, 40) <= 0) {
        spec->code = KEY_ESC;
        spec->shift = false;
        return 0;
    }

    if (second == '[' || second == 'O') {
        if (read_byte_with_timeout(&third, 40) <= 0) {
            spec->code = KEY_ESC;
            spec->shift = false;
            return 0;
        }

        if (second == 'O') {
            switch (third) {
            case 'P': spec->code = KEY_F1; return 0;
            case 'Q': spec->code = KEY_F2; return 0;
            case 'R': spec->code = KEY_F3; return 0;
            case 'S': spec->code = KEY_F4; return 0;
            case 'H': spec->code = KEY_HOME; return 0;
            case 'F': spec->code = KEY_END; return 0;
            default: return -1;
            }
        }

        if (third >= '0' && third <= '9') {
            seq[len++] = third;
            while (len < (int)sizeof(seq)) {
                ret = read_byte_with_timeout(&seq[len], 40);
                if (ret <= 0)
                    return -1;
                if (seq[len] == '~')
                    break;
                len++;
            }

            if (len >= (int)sizeof(seq) || seq[len] != '~')
                return -1;

            value = 0;
            for (ret = 0; ret < len; ret++) {
                if (!isdigit(seq[ret]))
                    return -1;
                value = value * 10 + (seq[ret] - '0');
            }

            switch (value) {
            case 1: spec->code = KEY_HOME; return 0;
            case 2: spec->code = KEY_INSERT; return 0;
            case 3: spec->code = KEY_DELETE; return 0;
            case 4: spec->code = KEY_END; return 0;
            case 5: spec->code = KEY_PAGEUP; return 0;
            case 6: spec->code = KEY_PAGEDOWN; return 0;
            case 7: spec->code = KEY_HOME; return 0;
            case 8: spec->code = KEY_END; return 0;
            case 11: spec->code = KEY_F1; return 0;
            case 12: spec->code = KEY_F2; return 0;
            case 13: spec->code = KEY_F3; return 0;
            case 14: spec->code = KEY_F4; return 0;
            case 15: spec->code = KEY_F5; return 0;
            case 17: spec->code = KEY_F6; return 0;
            case 18: spec->code = KEY_F7; return 0;
            case 19: spec->code = KEY_F8; return 0;
            case 20: spec->code = KEY_F9; return 0;
            case 21: spec->code = KEY_F10; return 0;
            case 23: spec->code = KEY_F11; return 0;
            case 24: spec->code = KEY_F12; return 0;
            default: return -1;
            }
        }

        switch (third) {
        case 'A': spec->code = KEY_UP; return 0;
        case 'B': spec->code = KEY_DOWN; return 0;
        case 'C': spec->code = KEY_RIGHT; return 0;
        case 'D': spec->code = KEY_LEFT; return 0;
        case 'H': spec->code = KEY_HOME; return 0;
        case 'F': spec->code = KEY_END; return 0;
        case 'Z': spec->code = KEY_TAB; spec->shift = true; return 0;
        default: return -1;
        }
    }

    spec->code = KEY_ESC;
    spec->shift = false;
    return 0;
}

static int read_key_spec(struct key_spec *spec)
{
    unsigned char ch;
    ssize_t nread;

    memset(spec, 0, sizeof(*spec));

    nread = read(STDIN_FILENO, &ch, 1);
    if (nread < 0) {
        if (errno == EINTR)
            return 1;
        perror("read");
        return -1;
    }
    if (nread == 0)
        return 1;

    if (ch == 0x1b)
        return map_escape_sequence(spec);

    return map_ascii_key(ch, spec);
}

static int set_key_bits(int fd)
{
    int code;
    const int extra_keys[] = {
        KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
        KEY_ENTER, KEY_BACKSPACE, KEY_ESC, KEY_TAB, KEY_SPACE,
        KEY_HOME, KEY_END, KEY_PAGEUP, KEY_PAGEDOWN, KEY_DELETE,
        KEY_INSERT, KEY_LEFTSHIFT,
        KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6,
        KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12
    };
    size_t index;

    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0)
        return -1;
    if (ioctl(fd, UI_SET_EVBIT, EV_SYN) < 0)
        return -1;

    for (code = KEY_A; code <= KEY_Z; code++) {
        if (ioctl(fd, UI_SET_KEYBIT, code) < 0)
            return -1;
    }

    for (code = KEY_1; code <= KEY_0; code++) {
        if (ioctl(fd, UI_SET_KEYBIT, code) < 0)
            return -1;
    }

    for (index = 0; index < sizeof(extra_keys) / sizeof(extra_keys[0]); index++) {
        if (ioctl(fd, UI_SET_KEYBIT, extra_keys[index]) < 0)
            return -1;
    }

    if (ioctl(fd, UI_SET_KEYBIT, KEY_MINUS) < 0 ||
        ioctl(fd, UI_SET_KEYBIT, KEY_EQUAL) < 0 ||
        ioctl(fd, UI_SET_KEYBIT, KEY_LEFTBRACE) < 0 ||
        ioctl(fd, UI_SET_KEYBIT, KEY_RIGHTBRACE) < 0 ||
        ioctl(fd, UI_SET_KEYBIT, KEY_SEMICOLON) < 0 ||
        ioctl(fd, UI_SET_KEYBIT, KEY_APOSTROPHE) < 0 ||
        ioctl(fd, UI_SET_KEYBIT, KEY_GRAVE) < 0 ||
        ioctl(fd, UI_SET_KEYBIT, KEY_BACKSLASH) < 0 ||
        ioctl(fd, UI_SET_KEYBIT, KEY_COMMA) < 0 ||
        ioctl(fd, UI_SET_KEYBIT, KEY_DOT) < 0 ||
        ioctl(fd, UI_SET_KEYBIT, KEY_SLASH) < 0)
        return -1;

    return 0;
}

static int create_virtual_keyboard(const char *device_path, const char *device_name)
{
    struct uinput_user_dev device;
    int fd;
    ssize_t written;

    fd = open(device_path, O_WRONLY | O_NONBLOCK);
    if (fd < 0)
        return -1;

    if (set_key_bits(fd) < 0) {
        perror("uinput ioctl");
        close(fd);
        return -1;
    }

    memset(&device, 0, sizeof(device));
    snprintf(device.name, UINPUT_MAX_NAME_SIZE, "%s", device_name);
    device.id.bustype = BUS_USB;
    device.id.vendor = 0x1209;
    device.id.product = 0xf1c2;
    device.id.version = 1;

    written = write(fd, &device, sizeof(device));
    if (written != (ssize_t)sizeof(device)) {
        perror("write uinput_user_dev");
        close(fd);
        return -1;
    }

    if (ioctl(fd, UI_DEV_CREATE) < 0) {
        perror("UI_DEV_CREATE");
        close(fd);
        return -1;
    }

    usleep(100000);
    return fd;
}

static int open_uinput_device(const char *path, const char *name)
{
    const char *candidates[3];
    int fd;
    int i;

    candidates[0] = path;
    candidates[1] = "/dev/uinput";
    candidates[2] = "/dev/input/uinput";

    for (i = 0; i < 3; i++) {
        if (!candidates[i] || !candidates[i][0])
            continue;
        fd = create_virtual_keyboard(candidates[i], name);
        if (fd >= 0)
            return fd;
    }

    return -1;
}

static void destroy_virtual_keyboard(int fd)
{
    if (fd >= 0) {
        ioctl(fd, UI_DEV_DESTROY);
        close(fd);
    }
}

static void print_usage(const char *program)
{
    printf("usage: %s [-d /dev/uinput] [-n device-name] [-q] [-k]\n", program);
    printf("keys: letters, digits, symbols, arrows, enter, tab, backspace, esc\n");
    printf("exit: Ctrl-C (default), or external kill with -k\n");
}

static void print_banner(const char *device_path, const char *device_name)
{
    printf("uinput device : %s\n", device_path);
    printf("device name   : %s\n", device_name);
    printf("send keys     : letters/digits/symbols/arrows/enter/tab/backspace/esc\n");
    printf("exit          : Ctrl-C\n");
    fflush(stdout);
}

int main(int argc, char **argv)
{
    struct sigaction sa;
    struct key_spec spec;
    const char *device_path = "";
    const char *device_name = "tty-uinput-bridge";
    bool quiet = false;
    bool keep_signals = true;
    int input_fd = -1;
    int opt;
    int ret;

    while ((opt = getopt(argc, argv, "d:n:qkh")) != -1) {
        switch (opt) {
        case 'd':
            device_path = optarg;
            break;
        case 'n':
            device_name = optarg;
            break;
        case 'q':
            quiet = true;
            break;
        case 'k':
            keep_signals = false;
            break;
        case 'h':
            print_usage(argv[0]);
            return 0;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);

    if (set_raw_terminal() < 0)
        return 1;

    if (!keep_signals) {
        struct termios raw;

        if (tcgetattr(STDIN_FILENO, &raw) < 0) {
            perror("tcgetattr");
            return 1;
        }
        raw.c_lflag &= ~ISIG;
        if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) < 0) {
            perror("tcsetattr");
            return 1;
        }
    }

    input_fd = open_uinput_device(device_path, device_name);
    if (input_fd < 0) {
        perror("open uinput");
        fprintf(stderr, "checked: %s, /dev/uinput, /dev/input/uinput\n",
            device_path[0] ? device_path : "(default)");
        return 1;
    }

    if (!quiet)
        print_banner(device_path[0] ? device_path : "/dev/uinput or /dev/input/uinput",
            device_name);

    while (!stop_requested) {
        ret = read_key_spec(&spec);
        if (ret < 0)
            break;
        if (ret > 0)
            continue;
        if (send_key(input_fd, &spec) < 0)
            break;
    }

    destroy_virtual_keyboard(input_fd);
    return 0;
}
