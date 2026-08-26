#!/bin/sh
set -u

PATH=/sbin:/bin:/usr/sbin:/usr/bin:/root
LOG=${LOG:-/tmp/start_8723_host_ap.log}

/root/start_8723_host_ap.sh restore
