#!/bin/sh
set -u
BASE=${BASE:-/root/aic_miracast}
export SINK_BIN=${SINK_BIN:-/root/aic_miracast/miracast_sink_dump_cea480p60_plus_576p50.brarm}
export PLAYER=${PLAYER:-/root/cedar_drm_player.yuvcrop}
export WIDTH=${WIDTH:-720}
export HEIGHT=${HEIGHT:-576}
export FPS=${FPS:-50}
export CEDAR_NO_PACE=${CEDAR_NO_PACE:-1}
export CEDAR_VIEW_STRETCH=0
export CEDAR_VIEW_CENTER_CROP=1
export CEDAR_VIEW_X=${CEDAR_VIEW_X:-0}
export CEDAR_VIEW_Y=${CEDAR_VIEW_Y:-0}
export CEDAR_VIEW_W=${CEDAR_VIEW_W:-384}
export CEDAR_VIEW_H=${CEDAR_VIEW_H:-640}
exec "$BASE/start_miracast_live_display_yuvdrop.sh" "${1:-start}"
