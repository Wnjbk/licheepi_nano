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

Set `F1TV_AUTOPLAY_INDEX` to a zero-based channel index to play that channel
once immediately at startup. The launcher returns to the channel list when playback exits.

Use `F1TV_CHANNELS` and `F1TV_PLAYER` to override the channel list and player.

`channels.official_20260826.txt` is a conservative, network-validated
candidate list. It intentionally uses direct HLS URLs that have an attributable
broadcaster/CDN origin and an unencrypted MPEG-TS segment. It is not a claim
that every stream has passed the F1C200S Cedar playback test.
