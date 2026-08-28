# HLS TS Live Relay Candidate

A single relay writer keeps one FIFO open and streams newly published HLS TS
bytes directly to the unchanged Cedar MPlayer, mirroring the Miracast sink to
FIFO ownership model. It stages no TS files and waits for no prefetch window.

Real-time latency cannot be below the HLS source's published segment duration.
Use a plain-HTTP live playlist with `EXT-X-TARGETDURATION` of one or two
seconds. The 10-second Mux VOD source is only a FIFO functional test.