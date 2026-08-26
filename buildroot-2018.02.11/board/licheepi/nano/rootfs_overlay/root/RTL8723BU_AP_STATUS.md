# RTL8723BU Board AP Scripts

status: working
date: 2026-07-20

Host baseline confirmed:
- device: 0bda:b720 RTL8723BU
- driver: lwfinger/vendor 8723bu
- hostapd driver: nl80211
- SSID: RTL8723BU_AP
- password: 12345678
- AP IP: 192.168.88.1/24
- phone connected successfully.

Board deployment target:
- /root/start_8723_host_ap.sh
- /root/stop_8723_host_ap.sh

Board validation:
- deployed to board over SSH after backing up old /root scripts under:
  /root/backup_8723_ap_20260720_072602
- started with:
  DURATION=300 /root/start_8723_host_ap.sh start
- hostapd log showed:
  wlan0: AP-ENABLED
- board AP:
  SSID: RTL8723BU_AP
  password: 12345678
  wlan0: 192.168.88.1/24
- DHCP requests observed:
  6c:40:e8:1d:07:41 -> 192.168.88.50
  4e:23:17:82:fe:4e -> 192.168.88.70
- board ping to 192.168.88.70 passed:
  2/2 replies, 0% packet loss

P2P-GO validation:
- scripts:
  /root/start_8723_p2p_go.sh
  /root/stop_8723_p2p_go.sh
- board md5:
  start_8723_p2p_go.sh 96d2169b9ca485fd7b140ae7eb8bde15
  stop_8723_p2p_go.sh 747e5ff3856c3e01c82bea78269ced3c
- started from serial with:
  /root/start_8723_p2p_go.sh start
- wpa_supplicant status:
  mode=P2P GO
  ssid=DIRECT-pz
  freq=2437
  wpa_state=COMPLETED
  ip_address=192.168.88.1
- P2P-GO passphrase:
  aR7VJiqV
- phone connected:
  MAC 32:fd:eb:01:71:61
  IP 192.168.88.37
- board ping to phone:
  4/4 replies, 0% packet loss

Notes:
- This is separate from normal STA boot scripts.
- Default start has a 300 second watchdog that restores STA mode.
- Use DURATION=0 only when serial access is available.
