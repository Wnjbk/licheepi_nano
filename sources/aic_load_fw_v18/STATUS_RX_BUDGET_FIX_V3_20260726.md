status: candidate-v3
date: 2026-07-26
base: aic8800_rx_lowmem_fix_20260726_2
changes: v2 plus RX process budget 64 frames per aicwf_process_rxframes invocation; re-trigger busrx and cond_resched after each budget slice
reason: v2 passed RTSP/RTP start but board hard-reset shortly after; no final OOM captured, likely RX thread/kernel starvation or watchdog under RTP burst
rule: deploy only under /root/aic_miracast/candidates
