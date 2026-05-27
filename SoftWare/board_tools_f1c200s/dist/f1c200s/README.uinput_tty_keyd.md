# uinput_tty_keyd

`uinput_tty_keyd` reads keys directly from the current terminal and injects them as Linux keyboard events through `uinput`.

## Build

On Ubuntu:

```sh
cd ~/LicheePi_Nano/board_tools_f1c200s
make -f Makefile.f1c200s
```

Build output:

```text
dist/f1c200s/uinput_tty_keyd
dist/uinput_tty_keyd-f1c200s.tar.gz
```

## Deploy

If BusyBox `tar` has no `-z`, extract like this:

```sh
gzip -dc uinput_tty_keyd-f1c200s.tar.gz | tar -xf -
```

Or copy individual files:

```sh
scp dist/f1c200s/uinput_tty_keyd root@192.168.0.2:/root/
scp enable_uinput.sh root@192.168.0.2:/root/
scp run_uinput_tty_keyd.sh root@192.168.0.2:/root/
scp S50uinput-keyd root@192.168.0.2:/etc/init.d/
```

## Kernel requirements

The target kernel must provide:

```text
CONFIG_INPUT=y
CONFIG_INPUT_EVDEV=y
CONFIG_INPUT_UINPUT=y or =m
```

Board-side checks:

```sh
zcat /proc/config.gz | grep -E 'CONFIG_INPUT=|CONFIG_INPUT_EVDEV=|CONFIG_INPUT_UINPUT='
ls -l /dev/uinput /dev/input/uinput
```

## Board-side setup

Run once after boot:

```sh
chmod +x /root/enable_uinput.sh
/root/enable_uinput.sh
```

If `/dev/uinput` is still missing, the kernel does not currently expose `uinput`.

## Runtime flow

Use two sessions.

Session 1 starts the target program, for example:

```sh
export SDL_VIDEODRIVER=fbcon
export SDL_FBDEV=/dev/fb0
cd /root/gmenu2x
./gmenu2x
```

Session 2 runs the injector:

```sh
/root/run_uinput_tty_keyd.sh
```

Keys are injected immediately. No Enter is required.

Supported input:

- letters `a-z`, `A-Z`
- digits `0-9`
- common symbols
- arrow keys
- `Enter`
- `Tab`
- `Backspace`
- `Esc`
- `Home`, `End`, `PageUp`, `PageDown`, `Delete`

Exit the injector with:

```text
Ctrl-C
```

`Ctrl-C` stops `uinput_tty_keyd` itself. It does not occupy a target-side hotkey.

To fully dedicate the current terminal to key injection:

```sh
MODE=grab /root/run_uinput_tty_keyd.sh
```

In grab mode:

- all typed keys are injected
- `Ctrl-C` is also injected, not used as local stop
- stop it from another terminal with `killall uinput_tty_keyd`

## Boot-time init script

Install and enable:

```sh
chmod +x /etc/init.d/S50uinput-keyd
/etc/init.d/S50uinput-keyd start
```

Default behavior:

- prepares `/dev/uinput`
- does not attach to a terminal automatically

If you want autostart on a fixed tty, create:

```sh
cp /root/uinput_tty_keyd.conf.example /root/uinput_tty_keyd.conf
```

Then edit:

```sh
TTY_DEVICE=/dev/ttyS0
```

After that:

```sh
/etc/init.d/S50uinput-keyd restart
```

## Notes

- If the target program only reacts to arrows or Enter, press the real arrow keys and Enter in Session 2.
- If the target program has letter hotkeys such as `c`, `m`, `v`, `d`, `r`, `n`, those letters are injected directly.
- If you only have one shell session, this approach is not practical. The target program and the injector need separate sessions.
