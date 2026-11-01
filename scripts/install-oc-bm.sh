#!/bin/bash
# Install OpenCore (efi/EFI/OC) on the Linux ESP (mounted at /boot) and add an
# "OpenCore" UEFI boot entry at the END of BootOrder (GRUB stays default; pick OpenCore in the BIOS boot menu).
# Re-run after every efi change: only the OC folder is replaced, the boot entry is created once.
# Usage: sudo scripts/install-oc-bm.sh
set -euo pipefail
cd "$(dirname "$0")/.."
# Your ESP: disk + partition number (lsblk). OCVALIDATE = ocvalidate from the OpenCore 1.0.5 release zip.
ESP_DISK=${ESP_DISK:-/dev/nvme0n1}
ESP_PART=${ESP_PART:-8}
OCVALIDATE=${OCVALIDATE:-build/ocvalidate.linux}
[[ $EUID == 0 ]] || { echo "run with sudo"; exit 1; }
mountpoint -q /boot && [[ $(findmnt -no SOURCE /boot) == ${ESP_DISK}p${ESP_PART} ]] || { echo "/boot is not ${ESP_DISK}p${ESP_PART}"; exit 1; }
"$OCVALIDATE" efi/EFI/OC/config.plist | tail -1

if [[ -d /boot/EFI/OC ]]; then
  mkdir -p backup && cp /boot/EFI/OC/opencore-*.txt backup/ 2>/dev/null || true   # keep boot logs
  rm -rf /boot/EFI/OC
fi
cp -r efi/EFI/OC /boot/EFI/OC
sync
du -sh /boot/EFI/OC; df -h /boot | tail -1

if ! efibootmgr | grep -q 'OpenCore'; then
  order=$(efibootmgr | sed -n 's/^BootOrder: //p')
  efibootmgr --create --disk "$ESP_DISK" --part "$ESP_PART" --label OpenCore --loader '\EFI\OC\OpenCore.efi' >/dev/null
  new=$(efibootmgr | sed -n 's/^Boot\([0-9A-F]\{4\}\)\*\? OpenCore.*/\1/p')
  efibootmgr --bootorder "$order,$new" >/dev/null
fi
efibootmgr | head -8
