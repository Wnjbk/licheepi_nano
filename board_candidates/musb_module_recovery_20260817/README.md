# MUSB module recovery candidate

Purpose: defer USB enumeration until the host stack is loaded explicitly, and
prevent the boot-time `btusb` binder from taking the RTL8723BU Bluetooth
interface before AIC8800 registration is tested.

Payload:

- `zImage`, `musb_hdrc.ko`, `sunxi.ko` are in the immutable host archive.
- `rootfs/etc/init.d/S16usb-host` loads `musb_hdrc` before `sunxi`.
- `rootfs/etc/init.d/S16zzrtl8723bluetooth` intentionally does nothing.

The candidate does not change U-Boot, DTB, AIC8800 files, normal WLAN scripts,
or Miracast files. Restore the backed-up init scripts to re-enable automatic
Bluetooth binding.
