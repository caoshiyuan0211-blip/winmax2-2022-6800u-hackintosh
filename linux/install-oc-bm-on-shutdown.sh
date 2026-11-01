#!/bin/bash
# One-time setup: sudo linux/install-oc-bm-on-shutdown.sh
# After this, every Linux poweroff/reboot runs scripts/install-oc-bm.sh (if efi changed).
set -euo pipefail
[[ $EUID == 0 ]] || { echo "run with sudo"; exit 1; }
cd "$(dirname "$0")"
chmod 755 oc-bm-on-shutdown.sh
sed "s|@REPO@|$(readlink -f ..)|g" hkvm-oc-bm.service > /etc/systemd/system/hkvm-oc-bm.service
systemctl daemon-reload
systemctl enable --now hkvm-oc-bm.service
systemctl status hkvm-oc-bm.service --no-pager | head -5
