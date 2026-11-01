#!/bin/bash
# GRUB boots Arch Linux at once: no menu, no countdown, no Windows/firmware/BootNext entries
# (OpenCore is the OS picker now). Backups: /etc/default/grub.bak-pre-linuxonly,
# /boot/grub/grub.cfg.bak-pre-linuxonly. Undo: copy them back, chmod +x the two /etc/grub.d files.
# Optional: --oc-first puts OpenCore (Boot0001) first in the firmware BootOrder.
set -e
[ "$(id -u)" = 0 ] || { echo "run with sudo"; exit 1; }
cp -n /etc/default/grub /etc/default/grub.bak-pre-linuxonly
cp -n /boot/grub/grub.cfg /boot/grub/grub.cfg.bak-pre-linuxonly
set_key() {
	if grep -q "^$1=" /etc/default/grub; then sed -i "s|^$1=.*|$1=$2|" /etc/default/grub
	else echo "$1=$2" >> /etc/default/grub; fi
}
set_key GRUB_TIMEOUT 0
set_key GRUB_TIMEOUT_STYLE hidden
set_key GRUB_DISABLE_OS_PROBER true
for f in 30_uefi-firmware 31_efi_bootnext; do [ -e /etc/grub.d/$f ] && chmod -x /etc/grub.d/$f; done
grub-mkconfig -o /boot/grub/grub.cfg
echo "--- top-level entries now:"; grep -E "^(menuentry|submenu)" /boot/grub/grub.cfg | cut -c1-80
if [ "$1" = --oc-first ]; then
	rest=$(efibootmgr | sed -n 's/^BootOrder: //p' | tr ',' '\n' | grep -v '^0001$' | paste -sd,)
	efibootmgr -o "0001,$rest" | grep BootOrder
fi
