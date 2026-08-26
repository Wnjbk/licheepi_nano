#!/bin/sh
set -u

PATH=/sbin:/bin:/usr/sbin:/usr/bin:/root

LOG=${LOG:-/tmp/start_miracast_wfd_sink.log}
RTSP_LOG=${RTSP_LOG:-/tmp/miracast_rtsp_sink.log}
RTSP_PID=${RTSP_PID:-/tmp/miracast_rtsp_sink.pid}

log() {
    echo "[miracast] $(date '+%H:%M:%S' 2>/dev/null) $*" | tee -a "$LOG"
}

case "${1:-start}" in
    stop)
        kill "$(cat "$RTSP_PID" 2>/dev/null)" 2>/dev/null || true
        killall miracast_rtsp_sink 2>/dev/null || true
        /root/stop_8723_p2p_go.sh 2>/dev/null || true
        exit 0
        ;;
esac

rm -f "$LOG" "$RTSP_LOG"
log "start WFD sink"

/root/start_8723_p2p_go.sh start >>"$LOG" 2>&1 || {
    log "P2P-GO start failed"
    exit 1
}

kill "$(cat "$RTSP_PID" 2>/dev/null)" 2>/dev/null || true
killall miracast_rtsp_sink 2>/dev/null || true
if [ ! -x /root/miracast_rtsp_sink ]; then
    log "missing /root/miracast_rtsp_sink"
    exit 1
fi

/root/miracast_rtsp_sink "$RTSP_LOG" >>"$LOG" 2>&1 &
echo $! > "$RTSP_PID"
sleep 1

log "rtsp pid=$(cat "$RTSP_PID" 2>/dev/null) log=$RTSP_LOG"
/usr/sbin/wpa_cli -p /tmp/wpa_8723_p2p -i wlan0 status >>"$LOG" 2>&1 || true
printf "passphrase=" >>"$LOG"
/usr/sbin/wpa_cli -p /tmp/wpa_8723_p2p -i wlan0 p2p_get_passphrase >>"$LOG" 2>&1 || true
netstat -lntup >>"$LOG" 2>&1 || true
tail -120 "$LOG"
