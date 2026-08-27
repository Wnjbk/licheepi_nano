# HLS TS Prefetch Candidate

Feeds the existing Cedar MPlayer from a continuous FIFO while a bounded
three-segment downloader prefetches plain-HTTP VOD HLS TS files under
`/root/roms/tv/run`. It changes no driver, kernel, rootfs or Miracast file.

Test:
`PREFETCH_SEGMENTS=3 hls_ts_prefetch.sh http://test-streams.mux.dev/x36xhzz/url_2/193039199_mp4_h264_aac_ld_7.m3u8`