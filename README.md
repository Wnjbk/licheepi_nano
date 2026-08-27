# wiliwili-lite-f1c200s

This is a new, lightweight F1C200S-oriented client prototype. It is not a
binary-compatible fork of upstream wiliwili. Upstream wiliwili requires
Borealis, OpenGL, and libmpv; those are not viable on the F1C200S target.

The prototype uses SDL 1.2 for an 800x480 UI and libcurl for API reachability.
It is deliberately split from video playback: a production target will invoke
the tracked Cedar player with a resolved H.264/HLS URL rather than embedding
libmpv or software FFmpeg decode.

## Host build

```sh
cmake -B build
cmake --build build
DISPLAY=:0 XAUTHORITY=/home/wnk/.Xauthority ./build/wiliwili-lite
```

Controls: Up/Down or mouse to select; Enter/P invokes the optional playback
backend; R checks the public Bilibili popular-feed API; Escape/Q exits.

Set `WILIWILI_LITE_PLAYER` to an explicitly chosen playback command before
pressing Enter/P. The prototype never starts a player without this variable.

## Board direction

The board build must use the existing SDL 1.2 package and an external Cedar
player adapter. Bilibili page/playurl parsing, WBI signing, authentication,
and DASH/HLS selection remain separate work items; no unsupported URL parser
is claimed by this prototype.
