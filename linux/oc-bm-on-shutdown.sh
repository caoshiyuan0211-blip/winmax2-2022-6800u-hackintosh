#!/bin/bash
# Run by hkvm-oc-bm.service at every Linux shutdown/reboot (ExecStop, before /boot is unmounted):
# installs efi/EFI/OC to /boot via scripts/install-oc-bm.sh, but only if it changed and passes ocvalidate.
# Log: journalctl -u hkvm-oc-bm -b -1
set -uo pipefail
cd "$(dirname "$(readlink -f "$0")")/.." || exit 0
if [[ -d /boot/EFI/OC ]] && diff -rq -x 'opencore-*.txt' efi/EFI/OC /boot/EFI/OC >/dev/null 2>&1; then
  echo "oc-bm: /boot/EFI/OC already matches efi, nothing to do"
  exit 0
fi
out=$("${OCVALIDATE:-build/ocvalidate.linux}" efi/EFI/OC/config.plist 2>&1)
if ! grep -q 'No issues found' <<<"$out"; then
  echo "oc-bm: ocvalidate FAILED, NOT installing:"; echo "$out" | tail -20
  exit 0
fi
echo "oc-bm: efi changed, installing"
scripts/install-oc-bm.sh
