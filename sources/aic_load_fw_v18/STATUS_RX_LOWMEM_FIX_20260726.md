status: candidate
date: 2026-07-26
base: aic8800_hostgood_fdrv_8d83_20260723
changes: MAX_RXQLEN 2000->128; AICWF_USB_MSG_RX_URBS 100->32; CONFIG_TXRX_THREAD_PRIO y->n
rule: do not overwrite board baseline modules; deploy only under /root/aic_miracast/candidates
