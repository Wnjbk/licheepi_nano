# uinput_tty_keyd

`uinput_tty_keyd` reads keys from the current terminal and injects a real Linux virtual gamepad.

## Build

```sh
cd ~/LicheePi_Nano/board_tools_f1c200s
make -f Makefile.f1c200s
```

Output:

```text
dist/f1c200s/uinput_tty_keyd
dist/uinput_tty_keyd-f1c200s.tar.gz
```

## Deploy

BusyBox `tar` without `-z`:

```sh
gzip -dc uinput_tty_keyd-f1c200s.tar.gz | tar -xf -
```

Or copy files directly:

```sh
scp dist/f1c200s/uinput_tty_keyd root@192.168.0.2:/root/
scp enable_uinput.sh root@192.168.0.2:/root/
scp run_uinput_tty_keyd.sh root@192.168.0.2:/root/
scp S50uinput-keyd root@192.168.0.2:/etc/init.d/
```

## Kernel requirements

```text
CONFIG_INPUT=y
CONFIG_INPUT_EVDEV=y
CONFIG_INPUT_UINPUT=y or =m
CONFIG_INPUT_JOYDEV=y or =m
```

Check on board:

```sh
zcat /proc/config.gz | grep -E 'CONFIG_INPUT=|CONFIG_INPUT_EVDEV=|CONFIG_INPUT_UINPUT=|CONFIG_INPUT_JOYDEV='
ls -l /dev/uinput /dev/input/uinput /dev/input/js0
```

## Board setup

```sh
chmod +x /root/enable_uinput.sh
/root/enable_uinput.sh
```

If `/dev/input/js0` never appears after startup, `joydev` is missing or not loaded.

## Runtime

Session 1 runs the target program:

```sh
export SDL_VIDEODRIVER=fbcon
export SDL_FBDEV=/dev/fb0
cd /root/gmenu2x
./gmenu2x
```

Session 2 runs the bridge:

```sh
/root/run_uinput_tty_keyd.sh
```

## Terminal keys

- arrows -> d-pad
- `a` -> A
- `b` -> B
- `x` -> X
- `y` -> Y
- `l` -> L
- `r` -> R
- `t` or Enter -> Start
- `s` or Backspace -> Select

## Virtual pad

- buttons: `A B X Y L R Start Select`
- axes: `ABS_X ABS_Y`
- hat: `ABS_HAT0X ABS_HAT0Y`

This is a joystick/gamepad device, not keyboard event injection.

## Verify

```sh
cat /proc/bus/input/devices
ls -l /dev/input/js0 /dev/input/event*
```

Expected:

- a device named `tty-uinput-gamepad`
- `Handlers=` contains both `eventX` and `js0`

## Grab mode

```sh
MODE=grab /root/run_uinput_tty_keyd.sh
```

In grab mode:

- current terminal is input-only
- `Ctrl-C` is injected, not used as local stop
- stop it from another terminal with `killall uinput_tty_keyd`

## Boot init

```sh
chmod +x /etc/init.d/S50uinput-keyd
/etc/init.d/S50uinput-keyd start
```

Default behavior only prepares `uinput` and `joydev`.

For fixed tty autostart:

```sh
cp /root/uinput_tty_keyd.conf.example /root/uinput_tty_keyd.conf
```

Then set:

```sh
TTY_DEVICE=/dev/ttyS0
```
