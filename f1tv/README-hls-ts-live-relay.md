# HLS TS Live Relay Candidate

A single relay writer keeps one FIFO open and streams HLS TS bytes directly to
the unchanged Cedar MPlayer, mirroring the Miracast sink to FIFO ownership
model. Runtime media is held only in `/dev/shm`; no TS media is written to
persistent flash.

The default bounded jitter buffer holds two 10-second segments: the segment
currently being written into the FIFO and one downloaded successor. It costs
one additional segment of live latency but avoids visible stalls when a
playlist refresh or HTTP segment request is briefly late. Override
`PREFETCH_SEGMENTS=1` only when minimum latency matters more than continuity.
The MPlayer input cache is limited to 512 KiB; it is not a disk cache and does
not replace the full-segment RAM queue.

Real-time latency cannot be below the HLS source's published segment duration.
Use a plain-HTTP live playlist with `EXT-X-TARGETDURATION` of one or two
seconds. The 10-second Mux VOD source is only a FIFO functional test.