#!/bin/sh
set -u
PATH=/sbin:/bin:/usr/sbin:/usr/bin:/root:/root/aic_miracast
BASE=${BASE:-/root/aic_miracast}
IFACE=${IFACE:-wlan1}
GO_ADDR=${GO_ADDR:-192.168.49.1}
GO_NETMASK=${GO_NETMASK:-255.255.255.0}
GO_FREQ=${GO_FREQ:-5805}
DEVICE_NAME=${DEVICE_NAME:-F1C200S-AIC}
DEVICE_TYPE=${DEVICE_TYPE:-8-0050F204-5}
CTRL_DIR=${CTRL_DIR:-/tmp/aic_wpa_ctrl}
CONF=${CONF:-/tmp/aic_wpa.conf}
ARCHIVE_DIR=${ARCHIVE_DIR:-/root/aic_runtime}
LOG_DIR=${LOG_DIR:-$ARCHIVE_DIR/logs}
RUN=${RUN:-$ARCHIVE_DIR/run}
OUT_DIR=${OUT_DIR:-/root/roms/video}
LOG=/root/aic_runtime/logs/aic_miracast_board.log
SINK_BIN=${SINK_BIN:-/root/aic_miracast/miracast_sink_dump}
WPA_SUPPLICANT=${WPA_SUPPLICANT:-$BASE/wpa24_aic_wfd_supplicant}
WPA_CLI=${WPA_CLI:-$BASE/wpa24_aic_wfd_cli}
CDUMP=${CDUMP:-$SINK_BIN}
DHCPD=${DHCPD:-$BASE/tiny_dhcpd_49}
LOAD_FW=${LOAD_FW:-/lib/modules/5.7.1/extra/aic_load_fw.ko}
FDRV=${FDRV:-/lib/modules/5.7.1/extra/aic8800_fdrv.ko}
FW_PATH=${FW_PATH:-/lib/firmware/aic8800D80}
AIC_DBG_LEVEL=${AIC_DBG_LEVEL:-0}
WPA_DEBUG=${WPA_DEBUG:-}
LOWMEM_TUNE=${LOWMEM_TUNE:-1}
STOP_GMENU=${STOP_GMENU:-1}
CAPTURE_MODE=${CAPTURE_MODE:-null}
mkdir -p "$LOG_DIR" "$RUN" "$OUT_DIR" "$BASE/backups"
log(){ echo "[aic-miracast] $(date '+%H:%M:%S' 2>/dev/null) $*" | tee -a "$LOG"; }
kill_ps_match(){ pat="$1"; ps w | grep "$pat" | grep -v grep | awk '{print $1}' | while read pid; do [ -n "$pid" ] && kill "$pid" 2>/dev/null || true; done; sleep 1; ps w | grep "$pat" | grep -v grep | awk '{print $1}' | while read pid; do [ -n "$pid" ] && kill -9 "$pid" 2>/dev/null || true; done; }
stop_runtime(){
    kill_ps_match 'miracast_sink_dump'
    kill_ps_match 'watch_aic_miracast.sh'
    kill_ps_match 'watch_null_direct_rearm.sh'
    kill_ps_match 'dmesg -w'
    kill_ps_match 'tiny_dhcpd_49 wlan1'
    kill_ps_match 'wpa24_aic_wfd_supplicant'
    ps w | grep '[w]pa_supplicant' | grep -E -- "-i ?$IFACE|-i$IFACE" | awk '{print $1}' | while read pid; do kill -9 "$pid" 2>/dev/null || true; done
}
ensure_aic_iface(){
 if ifconfig -a 2>/dev/null | grep -q "^$IFACE[[:space:]]"; then return 0; fi
 if lsusb | grep -qi 'a69c:8d80'; then
  log "load firmware from 8d80"; rmmod aic8800_fdrv aic_load_fw 2>/dev/null || true
  insmod "$LOAD_FW" aic_fw_path="$FW_PATH" aicwf_dbg_level="$AIC_DBG_LEVEL" >>"$LOG" 2>&1 || { log "insmod aic_load_fw failed"; return 1; }
  i=0; while [ "$i" -lt 35 ]; do lsusb | grep -qi 'a69c:8d83' && break; i=$((i+1)); sleep 1; done
 fi
 if ! ifconfig -a 2>/dev/null | grep -q "^$IFACE[[:space:]]"; then
  log "load fdrv for 8d83"
  if ! grep -q '^aic_load_fw ' /proc/modules 2>/dev/null; then insmod "$LOAD_FW" aic_fw_path="$FW_PATH" aicwf_dbg_level="$AIC_DBG_LEVEL" >>"$LOG" 2>&1 || true; sleep 1; fi
  rmmod aic8800_fdrv 2>/dev/null || true
  insmod "$FDRV" aicwf_dbg_level="$AIC_DBG_LEVEL" >>"$LOG" 2>&1 || { log "insmod aic8800_fdrv failed"; return 1; }
 fi
 i=0; while [ "$i" -lt 25 ]; do if ifconfig -a 2>/dev/null | grep -q "^$IFACE[[:space:]]"; then return 0; fi; i=$((i+1)); sleep 1; done
 log "$IFACE not found after AIC init"; return 1
}
write_watcher(){
cat >"$RUN/watch_aic_miracast.sh" <<'EOF'
#!/bin/sh
PATH=/sbin:/bin:/usr/sbin:/usr/bin:/root:/root/aic_miracast
IFACE=wlan1
CTRL_DIR=/tmp/aic_wpa_ctrl
WPA_CLI=/root/aic_miracast/wpa24_aic_wfd_cli
CDUMP=__SINK_BIN_PLACEHOLDER__
RUN=/root/aic_runtime/run
OUT_DIR=/root/roms/video
LOG=/root/aic_runtime/logs/aic_miracast_board.log
CAPTURE_MODE=${CAPTURE_MODE:-null}
LIVE_FIFO=${LIVE_FIFO:-/tmp/aic_h264_live.fifo}
REARM_MAX=8
REARM_COUNT=0
LAST_WPS_TIMEOUT=0
LAST_PBC_REQ=0
LAST_DHCP_LINE=0
RX_STALE_LIMIT=${RX_STALE_LIMIT:-60}
logw(){ echo "[$(date '+%H:%M:%S')] $*" >>"$RUN/watch.log"; }
rx_packets(){ cat /sys/class/net/$IFACE/statistics/rx_packets 2>/dev/null || echo 0; }
rx_bytes(){ cat /sys/class/net/$IFACE/statistics/rx_bytes 2>/dev/null || echo 0; }
tx_packets(){ cat /sys/class/net/$IFACE/statistics/tx_packets 2>/dev/null || echo 0; }
tx_bytes(){ cat /sys/class/net/$IFACE/statistics/tx_bytes 2>/dev/null || echo 0; }
mem_avail(){ awk '/MemAvailable:/ {print $2}' /proc/meminfo 2>/dev/null; }
dhcp_count(){ awk '/(REQUEST|ACK)/ && /ip=192\.168\.49\./ {c++} END{print c+0}' "$LOG" 2>/dev/null; }
dhcp_ip(){ grep -E 'ACK|REQUEST' "$LOG" 2>/dev/null | sed -n 's/.*ip=\(192\.168\.49\.[0-9][0-9]*\).*/\1/p' | tail -1; }
dhcp_mac(){ grep -E 'ACK|REQUEST' "$LOG" 2>/dev/null | sed -n 's/.*mac=\([0-9a-fA-F:][0-9a-fA-F:]*\).*/\1/p' | tail -1; }
wps_timeout_count(){ awk '/WPS-TIMEOUT/ {c++} END{print c+0}' "$LOG" 2>/dev/null; }
pbc_req_count(){ awk '/P2P-PROV-DISC-PBC-REQ/ {c++} END{print c+0}' "$LOG" 2>/dev/null; }
sta_disc_count(){ awk '/AP-STA-DISCONNECTED|CTRL-EVENT-DISCONNECTED/ {c++} END{print c+0}' "$LOG" 2>/dev/null; }
snapshot_reason(){
  reason="$1"
  logw "snapshot reason=$reason rxp=$(rx_packets) rxb=$(rx_bytes) txp=$(tx_packets) txb=$(tx_bytes) memavail=$(mem_avail)"
  dmesg | grep -E 'sched: RT|throttling|SLUB|Unable to allocate|page allocation|oom|aicwf|cmd queue|Frame received|Received monitor|4addr|rxfrag|Del sta|Add sta|deauth|wlan1|RTL871X|musb' | tail -120 >>"$RUN/watch.log" 2>&1
}
rearm_pbc(){
  reason=$1
  consume_budget=${2:-1}
  if [ "$consume_budget" = '1' ] && [ $REARM_COUNT -ge $REARM_MAX ]; then
    logw "skip rearm reason=$reason max=$REARM_MAX"
    return
  fi
  "$WPA_CLI" -p "$CTRL_DIR" -i "$IFACE" wps_pbc any >>"$RUN/watch.log" 2>&1
  rc=$?
  if [ "$consume_budget" = '1' ]; then
    REARM_COUNT=$((REARM_COUNT+1))
  fi
  logw "event rearm reason=$reason rc=$rc count=$REARM_COUNT"
}
logw 'watch start pbc-req-immediate + rx-counter mode; no periodic wps, no all_sta'
LAST_DHCP_LINE=$(dhcp_count)
LAST_WPS_TIMEOUT=$(wps_timeout_count)
LAST_PBC_REQ=$(pbc_req_count)
while true; do
  if ! ps w | grep -q '[w]pa24_aic_wfd_supplicant'; then logw 'wpa_supplicant gone, exit'; exit 1; fi
  cur_dhcp=$(dhcp_count)
  if [ "$cur_dhcp" -gt "$LAST_DHCP_LINE" ]; then
    LAST_DHCP_LINE=$cur_dhcp
    ip=$(dhcp_ip)
    [ -z "$ip" ] && { logw 'dhcp event but no ip parsed'; sleep 1; continue; }
    mac=$(dhcp_mac)
    if [ -n "$mac" ]; then
      arp -d "$ip" 2>/dev/null || true
      arp -s "$ip" "$mac" -i "$IFACE" 2>/dev/null || true
      logw "peer arp ip=$ip mac=$mac"
    fi
    sleep 2
    if [ "$CAPTURE_MODE" = 'null' ]; then out=/dev/null; elif [ "$CAPTURE_MODE" = 'fifo' ]; then out="$LIVE_FIFO"; else stamp=$(date +%Y%m%d_%H%M%S 2>/dev/null || echo cast); out="$OUT_DIR/miracast_$stamp.h264"; fi
    logw "start cdump ip=$ip out=$out rxp=$(rx_packets) rxb=$(rx_bytes)"
    "$CDUMP" "$ip" "$out" >"$RUN/cdump.stdout" 2>"$RUN/cdump.log" &
    CDUMP_PID=$!; last_rx=$(rx_packets); last_bytes=$(rx_bytes); last_tx=$(tx_packets); last_tx_bytes=$(tx_bytes); stale=0; tick=0; last_disc=$(sta_disc_count)
    while kill -0 "$CDUMP_PID" 2>/dev/null; do
      sleep 3; cur_rx=$(rx_packets); cur_bytes=$(rx_bytes); cur_tx=$(tx_packets); cur_tx_bytes=$(tx_bytes); cur_disc=$(sta_disc_count); tick=$((tick+1))
      if [ "$cur_disc" -gt "$last_disc" ]; then logw "sta disconnected event count=$cur_disc"; snapshot_reason sta_disconnect; kill "$CDUMP_PID" 2>/dev/null || true; break; fi
      if [ "$cur_rx" = "$last_rx" ] && [ "$cur_bytes" = "$last_bytes" ]; then stale=$((stale+1)); else stale=0; last_rx=$cur_rx; last_bytes=$cur_bytes; fi
      if [ "$tick" -eq 1 ] || [ $((tick % 10)) -eq 0 ]; then logw "cast tick t=${tick} rxp=$cur_rx rxb=$cur_bytes txp=$cur_tx txb=$cur_tx_bytes memavail=$(mem_avail)"; fi
      if [ "$stale" -eq 1 ] || [ "$stale" -eq 5 ] || [ "$stale" -eq 10 ]; then logw "rx stale count=$stale rxp=$cur_rx rxb=$cur_bytes"; fi
      if [ "$stale" -ge "$RX_STALE_LIMIT" ]; then logw "rx stalled, stop cdump pid=$CDUMP_PID rxp=$cur_rx rxb=$cur_bytes"; snapshot_reason rx_stalled; kill "$CDUMP_PID" 2>/dev/null || true; sleep 1; kill -9 "$CDUMP_PID" 2>/dev/null || true; break; fi
      last_tx=$cur_tx; last_tx_bytes=$cur_tx_bytes
    done
    wait "$CDUMP_PID" 2>/dev/null; cdump_rc=$?
    logw "cdump ended rc=$cdump_rc; wait for next phone event rxp=$(rx_packets) rxb=$(rx_bytes) txp=$(tx_packets) txb=$(tx_bytes)"
    snapshot_reason cdump_ended
  else
    cur_req=$(pbc_req_count)
    cur_to=$(wps_timeout_count)
    if [ "$cur_req" -gt "$LAST_PBC_REQ" ]; then LAST_PBC_REQ=$cur_req; rearm_pbc pbc_req; fi
    if [ "$cur_to" -gt "$LAST_WPS_TIMEOUT" ]; then LAST_WPS_TIMEOUT=$cur_to; rearm_pbc wps_timeout 0; fi
    sleep 1
  fi
done
EOF
sed -i "s#__SINK_BIN_PLACEHOLDER__#$SINK_BIN#g" "$RUN/watch_aic_miracast.sh"
chmod 700 "$RUN/watch_aic_miracast.sh"
}
start_mem_watch(){
cat >"$RUN/mem_watch.sh" <<EOF
#!/bin/sh
LOGF='$LOG_DIR/mem_slab_watch.log'
echo "runtime watch start \$(date)" >>"\$LOGF"
while true; do
  echo "--- \$(date) ---" >>"\$LOGF"
  echo "--- load ---" >>"\$LOGF"; cat /proc/loadavg >>"\$LOGF" 2>&1
  echo "--- meminfo ---" >>"\$LOGF"; grep -E 'MemFree|MemAvailable|Buffers|Cached|SwapCached|SwapFree|Slab|SReclaimable|SUnreclaim|KernelStack|PageTables' /proc/meminfo >>"\$LOGF" 2>&1
  echo "--- buddyinfo ---" >>"\$LOGF"; cat /proc/buddyinfo >>"\$LOGF" 2>&1
  echo "--- slab selected ---" >>"\$LOGF"; grep -E 'skbuff|kmalloc-4k|kmalloc-2k|kmalloc-1k|kmalloc-512|kmalloc-256|kmalloc-128|kmalloc-64|sock_inode|dentry|inode_cache|kernfs_node' /proc/slabinfo >>"\$LOGF" 2>&1
  echo "--- ps selected ---" >>"\$LOGF"; ps w | grep -E 'miracast_sink_dump|wpa24_aic|tiny_dhcpd|watch_aic|dropbear|gmenu|cedar|aic|ksoftirqd|irq' | grep -v grep >>"\$LOGF" 2>&1
  echo "--- wlan1 stats ---" >>"\$LOGF"; for n in rx_packets rx_bytes rx_errors rx_dropped tx_packets tx_bytes tx_errors tx_dropped; do printf "%s=" "\$n" >>"\$LOGF"; cat /sys/class/net/wlan1/statistics/\$n >>"\$LOGF" 2>/dev/null || echo 0 >>"\$LOGF"; done
  sync
  sleep 30
done
EOF
chmod 700 "$RUN/mem_watch.sh"; kill_ps_match 'mem_watch.sh'; nohup "$RUN/mem_watch.sh" >"$RUN/mem_watch.stdout" 2>&1 & echo $! >"$RUN/mem_watch.pid"
}
start_dmesg_watch(){
cat >"$RUN/dmesg_watch.sh" <<EOF
#!/bin/sh
LOGF='$LOG_DIR/dmesg_live.log'
while true; do
  {
    echo "dmesg snapshot \$(date)"
    dmesg -s 262144
  } >"\$LOGF.tmp" 2>&1
  mv "\$LOGF.tmp" "\$LOGF"
  sync
  sleep 2
done
EOF
chmod 700 "$RUN/dmesg_watch.sh"; kill_ps_match 'dmesg_watch.sh'; nohup "$RUN/dmesg_watch.sh" >"$RUN/dmesg_watch.stdout" 2>&1 & echo $! >"$RUN/dmesg_watch.pid"
}
loader_only(){
    mkdir -p "$LOG_DIR"
    : >"$LOG_DIR/loader_only.log"
    if lsusb | grep -qi 'a69c:8d83'; then echo "already 8d83" | tee -a "$LOG_DIR/loader_only.log"; return 0; fi
    if ! lsusb | grep -qi 'a69c:8d80'; then echo "no aic 8d80/8d83" | tee -a "$LOG_DIR/loader_only.log"; return 1; fi
    rmmod aic8800_fdrv aic_load_fw 2>/dev/null || true
    echo "loader: insmod $LOAD_FW from 8d80" | tee -a "$LOG_DIR/loader_only.log"
    insmod "$LOAD_FW" aic_fw_path="$FW_PATH" aicwf_dbg_level="$AIC_DBG_LEVEL" >>"$LOG_DIR/loader_only.log" 2>&1 || return 1
    i=0; while [ "$i" -lt 45 ]; do if lsusb | grep -qi 'a69c:8d83'; then echo "loader: 8d83 ready after ${i}s" | tee -a "$LOG_DIR/loader_only.log"; sleep 3; return 0; fi; i=$((i+1)); sleep 1; done
    echo "loader: timeout waiting for 8d83" | tee -a "$LOG_DIR/loader_only.log"; return 1
}
case "${1:-start}" in
    loader)
        loader_only
        exit $?
        ;;
 stop) stop_runtime; ifconfig "$IFACE" down 2>/dev/null || true; exit 0 ;;
 rearm) "$WPA_CLI" -p "$CTRL_DIR" -i "$IFACE" wps_pbc any >>"$LOG" 2>&1 || exit 1; log "rearm wps_pbc once"; exit 0 ;;
 status) echo '--- processes ---'; ps w | grep -E 'aic|wpa24|tiny_dhcpd|watch_aic|miracast|mem_watch' | grep -v grep || true; echo '--- wpa status ---'; "$WPA_CLI" -p "$CTRL_DIR" -i "$IFACE" status 2>&1 || true; echo '--- logs ---'; tail -80 "$LOG" 2>/dev/null || true; tail -80 "$RUN/watch.log" 2>/dev/null || true; exit 0 ;;
esac
SAVE_TAG="$(date +%Y%m%d_%H%M%S 2>/dev/null || echo boot).$$"
[ -s "$LOG" ] && cp "$LOG" "$LOG_DIR/aic_miracast_board.prev.$SAVE_TAG.log" 2>/dev/null || true
[ -s "$RUN/watch.log" ] && cp "$RUN/watch.log" "$LOG_DIR/watch.prev.$SAVE_TAG.log" 2>/dev/null || true
[ -s "$RUN/cdump.log" ] && cp "$RUN/cdump.log" "$LOG_DIR/cdump.prev.$SAVE_TAG.log" 2>/dev/null || true
[ -s "$LOG_DIR/mem_slab_watch.log" ] && cp "$LOG_DIR/mem_slab_watch.log" "$LOG_DIR/mem_slab_watch.prev.$SAVE_TAG.log" 2>/dev/null || true
[ -s "$LOG_DIR/dmesg_live.log" ] && cp "$LOG_DIR/dmesg_live.log" "$LOG_DIR/dmesg_live.prev.$SAVE_TAG.log" 2>/dev/null || true
stop_runtime
rm -f "$RUN"/*.log "$RUN"/*.stdout "$RUN"/*.pid "$RUN"/watch*.sh "$RUN"/mem_watch.sh "$RUN"/dmesg_watch.sh 2>/dev/null || true
rm -rf "$CTRL_DIR"; mkdir -p "$RUN" "$CTRL_DIR" "$OUT_DIR" "$LOG_DIR"; : >"$LOG"; : >"$LOG_DIR/mem_slab_watch.log"
: >"$LOG_DIR/dmesg_live.log"
log "start standard AIC Miracast iface=$IFACE name=$DEVICE_NAME freq=$GO_FREQ capture=$CAPTURE_MODE"
if [ "$LOWMEM_TUNE" = '1' ]; then echo 4096 >/proc/sys/vm/min_free_kbytes 2>/dev/null || true; sync 2>/dev/null || true; echo 3 >/proc/sys/vm/drop_caches 2>/dev/null || true; log "lowmem tune: min_free_kbytes=$(cat /proc/sys/vm/min_free_kbytes 2>/dev/null)"; fi
if [ "$STOP_GMENU" = '1' ]; then killall gmenu2x 2>/dev/null || true; killall run_gmenu2x.sh 2>/dev/null || true; log 'stopped gmenu2x for Miracast memory headroom'; fi
ensure_aic_iface || { tail -160 "$LOG"; exit 1; }
ifconfig "$IFACE" down >>"$LOG" 2>&1 || true; ifconfig "$IFACE" up >>"$LOG" 2>&1 || true
cat >"$CONF" <<EOF
ctrl_interface=DIR=$CTRL_DIR GROUP=netdev
update_config=1
device_name=$DEVICE_NAME
device_type=$DEVICE_TYPE
manufacturer=F1C200S
model_name=F1C200S-AIC-WFD
model_number=1
serial_number=1
config_methods=virtual_display virtual_push_button pbc
p2p_go_intent=15
p2p_oper_reg_class=124
p2p_oper_channel=161
p2p_listen_reg_class=81
p2p_listen_channel=6
p2p_no_group_iface=1
driver_param=use_p2p_group_interface=0
EOF
log 'wpa config:'; cat "$CONF" >>"$LOG"
"$WPA_SUPPLICANT" -B -Dnl80211 -i "$IFACE" -c "$CONF" -f "$LOG" ${WPA_DEBUG:+$WPA_DEBUG} >>"$LOG" 2>&1 || { log 'wpa_supplicant failed'; exit 1; }
sleep 2
"$WPA_CLI" -p "$CTRL_DIR" -i "$IFACE" set wifi_display 1 >>"$LOG" 2>&1 || true
"$WPA_CLI" -p "$CTRL_DIR" -i "$IFACE" set device_name "$DEVICE_NAME" >>"$LOG" 2>&1 || true
"$WPA_CLI" -p "$CTRL_DIR" -i "$IFACE" set device_type "$DEVICE_TYPE" >>"$LOG" 2>&1 || true
"$WPA_CLI" -p "$CTRL_DIR" -i "$IFACE" wfd_subelem_set 0 000600111c44012c >>"$LOG" 2>&1 || true
"$WPA_CLI" -p "$CTRL_DIR" -i "$IFACE" wfd_subelem_set 1 0006000000000000 >>"$LOG" 2>&1 || true
"$WPA_CLI" -p "$CTRL_DIR" -i "$IFACE" wfd_subelem_set 6 000700000000000000 >>"$LOG" 2>&1 || true
"$WPA_CLI" -p "$CTRL_DIR" -i "$IFACE" wfd_subelem_set 11 00020001 >>"$LOG" 2>&1 || true
"$WPA_CLI" -p "$CTRL_DIR" -i "$IFACE" p2p_flush >>"$LOG" 2>&1 || true
"$WPA_CLI" -p "$CTRL_DIR" -i "$IFACE" p2p_stop_find >>"$LOG" 2>&1 || true
sleep 2
"$WPA_CLI" -p "$CTRL_DIR" -i "$IFACE" p2p_group_add freq="$GO_FREQ" >>"$LOG" 2>&1 || { log 'p2p_group_add failed'; tail -160 "$LOG"; exit 1; }
sleep 5
ifconfig "$IFACE" "$GO_ADDR" netmask "$GO_NETMASK" up >>"$LOG" 2>&1 || true
"$DHCPD" "$IFACE" >>"$LOG" 2>&1 & echo $! >"$RUN/tiny_dhcpd.pid"; log "tiny_dhcpd_49 pid=$(cat "$RUN/tiny_dhcpd.pid")"
"$WPA_CLI" -p "$CTRL_DIR" -i "$IFACE" wps_pbc any >>"$LOG" 2>&1 || true
write_watcher; nohup "$RUN/watch_aic_miracast.sh" >"$RUN/watch.stdout" 2>&1 & echo $! >"$RUN/watch.pid"
# start_dmesg_watch disabled for cast stability
# start_mem_watch disabled for cast stability
log 'status:'; "$WPA_CLI" -p "$CTRL_DIR" -i "$IFACE" status >>"$LOG" 2>&1 || true; printf 'passphrase=' >>"$LOG"; "$WPA_CLI" -p "$CTRL_DIR" -i "$IFACE" p2p_get_passphrase >>"$LOG" 2>&1 || true; ifconfig "$IFACE" >>"$LOG" 2>&1 || true; log "ready: display=$DEVICE_NAME iface=$IFACE freq=$GO_FREQ passphrase=12345678 logs=$LOG"
grep -E 'ready:|mode=P2P GO|ssid=|freq=|ip_address=|passphrase=' "$LOG" 2>/dev/null | tail -40
