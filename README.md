# F1TV

Minimal SDL 1.2 channel launcher for F1C200S.

`channels.txt` contains `display name|direct media URL` lines. The application
does not parse video websites or bypass site protection. It launches the Cedar
MPlayer candidate for the selected direct H.264 source and returns to the list
when playback exits.

Default paths on the board:

```text
Application: /root/candidates/f1tv_20260826/f1tv
Channels:    /root/roms/tv/channels.txt
Player:      /root/candidates/mplayer_cedar_20260825/mplayer-cedar
```

Controls: Up/Down and Enter, touch/mouse click, F5 reload, Esc exit.

Use `F1TV_CHANNELS` and `F1TV_PLAYER` to override the channel list and player.
