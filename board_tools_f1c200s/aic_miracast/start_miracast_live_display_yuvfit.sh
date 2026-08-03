#!/bin/sh
set -u
BASE=${BASE:-/root/aic_miracast}
export PLAYER=${PLAYER:-/root/cedar_drm_player.yuvlive}
export WIDTH=${WIDTH:-640}
export HEIGHT=${HEIGHT:-480}
export FPS=${FPS:-60}
export SINK_BIN=${SINK_BIN:-/root/aic_miracast/miracast_sink_dump.lowest}
export CEDAR_NO_PACE=${CEDAR_NO_PACE:-1}
export CEDAR_VIEW_STRETCH=${CEDAR_VIEW_STRETCH:-0}
export CEDAR_VIEW_X=${CEDAR_VIEW_X:-12}
export CEDAR_VIEW_Y=${CEDAR_VIEW_Y:-0}
export CEDAR_VIEW_W=${CEDAR_VIEW_W:-360}
export CEDAR_VIEW_H=${CEDAR_VIEW_H:-640}
exec "$BASE/start_miracast_live_display_yuvdrop.sh" "${1:-start}"
