#include <errno.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <linux/kd.h>
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

struct termios saved_termios;
static bool termios_saved;
static int saved_kbmode = -1;
static bool kbmode_saved;
static bool mediumraw_enabled;
static volatile sig_atomic_t stop_requested;

#define PAD_AXIS_MIN   (-32767)
#define PAD_AXIS_MAX   32767

struct gamepad_state {
    bool a;
    bool b;
    bool x;
    bool y;
    bool l;
    bool r;
    bool start;
    bool select;
    bool up;
    bool down;
    bool left;
    bool right;
    int pad_x;
    int pad_y;
};

struct discrete_event {
    int type;
    int code;
    int value;
};

enum {
    EVENT_NONE = 0,
    EVENT_BUTTON,
    EVENT_HAT
};

static void restore_input_mode(void)
{
    if (kbmode_saved)
        ioctl(STDIN_FILENO, KDSKBMODE, saved_kbmode);

    if (termios_saved)
        tcsetattr(STDIN_FILENO, TCSANOW, &saved_termios);
}

static void on_signal(int signo)
{
    (void)signo;
    stop_requested = 1;
}

static int set_raw_terminal(bool keep_signals)
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
    if (keep_signals)
        raw.c_lflag |= ISIG;
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) < 0) {
        perror("tcsetattr");
        return -1;
    }

    atexit(restore_input_mode);
    return 0;
}

static int try_enable_mediumraw(void)
{
    int mode;

    if (ioctl(STDIN_FILENO, KDGKBMODE, &mode) < 0)
        return -1;

    saved_kbmode = mode;
    kbmode_saved = true;

    if (ioctl(STDIN_FILENO, KDSKBMODE, K_MEDIUMRAW) < 0) {
        kbmode_saved = false;
        return -1;
    }

    mediumraw_enabled = true;
    return 0;
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

static int emit_sync(int fd)
{
    return emit_event(fd, EV_SYN, SYN_REPORT, 0);
}

static int send_button_event(int fd, int code, int pressed)
{
    if (emit_event(fd, EV_KEY, code, pressed) < 0)
        return -1;
    if (emit_sync(fd) < 0)
        return -1;
    return 0;
}

static int send_pad_motion(int fd, int pad_x, int pad_y)
{
    if (emit_event(fd, EV_ABS, ABS_X, pad_x * PAD_AXIS_MAX) < 0)
        return -1;
    if (emit_event(fd, EV_ABS, ABS_Y, pad_y * PAD_AXIS_MAX) < 0)
        return -1;
    if (emit_event(fd, EV_ABS, ABS_HAT0X, pad_x) < 0)
        return -1;
    if (emit_event(fd, EV_ABS, ABS_HAT0Y, pad_y) < 0)
        return -1;
    if (emit_sync(fd) < 0)
        return -1;
    return 0;
}

static int set_joystick_bits(int fd)
{
    const int buttons[] = {
        BTN_A, BTN_B, BTN_X, BTN_Y,
        BTN_TL, BTN_TR, BTN_START, BTN_SELECT
    };
    size_t index;

    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0)
        return -1;
    if (ioctl(fd, UI_SET_EVBIT, EV_ABS) < 0)
        return -1;
    if (ioctl(fd, UI_SET_EVBIT, EV_SYN) < 0)
        return -1;

    for (index = 0; index < sizeof(buttons) / sizeof(buttons[0]); index++) {
        if (ioctl(fd, UI_SET_KEYBIT, buttons[index]) < 0)
            return -1;
    }

    if (ioctl(fd, UI_SET_ABSBIT, ABS_X) < 0)
        return -1;
    if (ioctl(fd, UI_SET_ABSBIT, ABS_Y) < 0)
        return -1;
    if (ioctl(fd, UI_SET_ABSBIT, ABS_HAT0X) < 0)
        return -1;
    if (ioctl(fd, UI_SET_ABSBIT, ABS_HAT0Y) < 0)
        return -1;

    return 0;
}

static int create_virtual_gamepad(const char *device_path, const char *device_name)
{
    struct uinput_user_dev device;
    int fd;
    ssize_t written;

    fd = open(device_path, O_WRONLY | O_NONBLOCK);
    if (fd < 0)
        return -1;

    if (set_joystick_bits(fd) < 0) {
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
    device.absmin[ABS_X] = PAD_AXIS_MIN;
    device.absmax[ABS_X] = PAD_AXIS_MAX;
    device.absmin[ABS_Y] = PAD_AXIS_MIN;
    device.absmax[ABS_Y] = PAD_AXIS_MAX;
    device.absmin[ABS_HAT0X] = -1;
    device.absmax[ABS_HAT0X] = 1;
    device.absmin[ABS_HAT0Y] = -1;
    device.absmax[ABS_HAT0Y] = 1;

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
    int index;

    candidates[0] = path;
    candidates[1] = "/dev/uinput";
    candidates[2] = "/dev/input/uinput";

    for (index = 0; index < 3; index++) {
        if (!candidates[index] || !candidates[index][0])
            continue;
        fd = create_virtual_gamepad(candidates[index], name);
        if (fd >= 0)
            return fd;
    }

    return -1;
}

static void destroy_virtual_gamepad(int fd)
{
    if (fd >= 0) {
        ioctl(fd, UI_DEV_DESTROY);
        close(fd);
    }
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

static int map_mediumraw_button(int keycode)
{
    switch (keycode) {
    case KEY_A: return BTN_A;
    case KEY_B: return BTN_B;
    case KEY_X: return BTN_X;
    case KEY_Y: return BTN_Y;
    case KEY_L: return BTN_TL;
    case KEY_R: return BTN_TR;
    case KEY_T: return BTN_START;
    case KEY_S: return BTN_SELECT;
    default: return -1;
    }
}

static int update_button_state(struct gamepad_state *state, int code, bool pressed)
{
    bool *slot = NULL;

    switch (code) {
    case BTN_A: slot = &state->a; break;
    case BTN_B: slot = &state->b; break;
    case BTN_X: slot = &state->x; break;
    case BTN_Y: slot = &state->y; break;
    case BTN_TL: slot = &state->l; break;
    case BTN_TR: slot = &state->r; break;
    case BTN_START: slot = &state->start; break;
    case BTN_SELECT: slot = &state->select; break;
    default: return 0;
    }

    if (*slot == pressed)
        return 0;

    *slot = pressed;
    return 1;
}

static int update_hat_state(struct gamepad_state *state, int keycode, bool pressed)
{
    switch (keycode) {
    case KEY_LEFT:
        state->left = pressed;
        break;
    case KEY_RIGHT:
        state->right = pressed;
        break;
    case KEY_UP:
        state->up = pressed;
        break;
    case KEY_DOWN:
        state->down = pressed;
        break;
    default:
        return 0;
    }

    {
        int new_x = 0;
        int new_y = 0;

        if (state->left && !state->right)
            new_x = -1;
        else if (state->right && !state->left)
            new_x = 1;

        if (state->up && !state->down)
            new_y = -1;
        else if (state->down && !state->up)
            new_y = 1;

        if (new_x == state->pad_x && new_y == state->pad_y)
            return 0;

        state->pad_x = new_x;
        state->pad_y = new_y;
    }

    return 1;
}

static int send_neutral_motion(int fd)
{
    return send_pad_motion(fd, 0, 0);
}

static int send_temporary_direction(int fd, int keycode)
{
    int pad_x = 0;
    int pad_y = 0;

    switch (keycode) {
    case KEY_UP:
        pad_y = -1;
        break;
    case KEY_DOWN:
        pad_y = 1;
        break;
    case KEY_LEFT:
        pad_x = -1;
        break;
    case KEY_RIGHT:
        pad_x = 1;
        break;
    default:
        return 0;
    }

    if (send_pad_motion(fd, pad_x, pad_y) < 0)
        return -1;
    return send_neutral_motion(fd);
}

static int handle_console_key(int fd, struct gamepad_state *state, int keycode, bool pressed)
{
    int button;
    int changed;

    changed = update_hat_state(state, keycode, pressed);
    if (changed)
        return send_pad_motion(fd, state->pad_x, state->pad_y);

    button = map_mediumraw_button(keycode);
    if (button < 0)
        return 0;

    changed = update_button_state(state, button, pressed);
    if (!changed)
        return 0;

    return send_button_event(fd, button, pressed ? 1 : 0);
}

static int read_console_keycode(int *keycode, bool *pressed)
{
    unsigned char byte;
    ssize_t nread;

    nread = read(STDIN_FILENO, &byte, 1);
    if (nread < 0) {
        if (errno == EINTR)
            return 1;
        perror("read");
        return -1;
    }
    if (nread == 0)
        return 1;

    *pressed = (byte & 0x80) == 0;
    *keycode = byte & 0x7f;
    return 0;
}

static int read_escape_sequence(struct discrete_event *event)
{
    unsigned char second = 0;
    unsigned char third = 0;

    if (read_byte_with_timeout(&second, 40) <= 0)
        return -1;

    if ((second == '[' || second == 'O') &&
        read_byte_with_timeout(&third, 40) > 0) {
        event->type = EVENT_HAT;
        event->value = 1;
        switch (third) {
        case 'A': event->code = KEY_UP; return 0;
        case 'B': event->code = KEY_DOWN; return 0;
        case 'C': event->code = KEY_RIGHT; return 0;
        case 'D': event->code = KEY_LEFT; return 0;
        default: return -1;
        }
    }

    return -1;
}

static int read_discrete_event(struct discrete_event *event)
{
    unsigned char ch;
    ssize_t nread;

    memset(event, 0, sizeof(*event));

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
        return read_escape_sequence(event);

    event->type = EVENT_BUTTON;
    event->value = 1;

    switch (ch) {
    case 'a':
    case 'A':
        event->code = BTN_A;
        return 0;
    case 'b':
    case 'B':
        event->code = BTN_B;
        return 0;
    case 'x':
    case 'X':
        event->code = BTN_X;
        return 0;
    case 'y':
    case 'Y':
        event->code = BTN_Y;
        return 0;
    case 'l':
    case 'L':
        event->code = BTN_TL;
        return 0;
    case 'r':
    case 'R':
        event->code = BTN_TR;
        return 0;
    case 't':
    case 'T':
    case '\r':
    case '\n':
        event->code = BTN_START;
        return 0;
    case 's':
    case 'S':
    case 0x08:
    case 0x7f:
        event->code = BTN_SELECT;
        return 0;
    default:
        return -1;
    }
}

static int send_discrete_event(int fd, struct gamepad_state *state, const struct discrete_event *event)
{
    int ret;

    if (event->type == EVENT_BUTTON) {
        ret = send_button_event(fd, event->code, 1);
        if (ret < 0)
            return ret;
        return send_button_event(fd, event->code, 0);
    }

    if (event->type == EVENT_HAT) {
        return send_temporary_direction(fd, event->code);
    }

    return 0;
}

static void print_usage(const char *program)
{
    printf("usage: %s [-d /dev/uinput] [-n device-name] [-q] [-k]\n", program);
    printf("virtual pad: ABS_X/Y + HAT + A/B/X/Y/L/R/START/SELECT\n");
    printf("input keys : arrows, a/b/x/y/l/r/t(start)/s(select)\n");
}

static void print_banner(const char *device_path, const char *device_name)
{
    printf("uinput device : %s\n", device_path);
    printf("device name   : %s\n", device_name);
    printf("mode          : %s\n", mediumraw_enabled ? "gamepad console" : "gamepad fallback");
    printf("virtual pad   : ABS_X/Y + HAT + A/B/X/Y/L/R/START/SELECT\n");
    printf("input keys    : arrows, a/b/x/y/l/r/t(start)/s(select)\n");
    fflush(stdout);
}

int main(int argc, char **argv)
{
    struct sigaction sa;
    struct gamepad_state state;
    const char *device_path = "";
    const char *device_name = "tty-uinput-gamepad";
    bool quiet = false;
    bool keep_signals = true;
    int input_fd = -1;
    int opt;

    memset(&state, 0, sizeof(state));

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
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);

    if (set_raw_terminal(keep_signals) < 0)
        return 1;

    try_enable_mediumraw();

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
        if (mediumraw_enabled) {
            int keycode;
            bool pressed;
            int ret = read_console_keycode(&keycode, &pressed);
            if (ret < 0)
                break;
            if (ret > 0)
                continue;
            if (handle_console_key(input_fd, &state, keycode, pressed) < 0)
                break;
        } else {
            struct discrete_event event;
            int ret = read_discrete_event(&event);
            if (ret < 0)
                continue;
            if (ret > 0)
                continue;
            if (send_discrete_event(input_fd, &state, &event) < 0)
                break;
        }
    }

    destroy_virtual_gamepad(input_fd);
    return 0;
}
