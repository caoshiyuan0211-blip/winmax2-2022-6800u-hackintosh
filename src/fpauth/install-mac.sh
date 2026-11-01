#!/bin/bash
# Install the CS9711 fingerprint login for sudo on macOS.  Run:  sudo ./install-mac.sh
# Undo:  sudo ./install-mac.sh remove
set -e
cd "$(dirname "$0")"
[[ $EUID == 0 ]] || { echo "run with sudo"; exit 1; }
MOD=/usr/local/lib/pam/pam_fpauth.so
LINE="auth       sufficient     $MOD"
if [[ $1 == remove ]]; then
  [[ -f /etc/pam.d/sudo_local ]] && sed -i '' "\|pam_fpauth|d" /etc/pam.d/sudo_local
  rm -f "$MOD" /usr/local/bin/fpauth
  echo "removed (prints kept in /var/db/fpauth; delete with: sudo rm -rf /var/db/fpauth)"
  exit 0
fi
mkdir -p /usr/local/bin /usr/local/lib/pam
install -m 755 -o root -g wheel fpauth /usr/local/bin/fpauth
install -m 444 -o root -g wheel pam_fpauth.so "$MOD"
codesign -s - -f /usr/local/bin/fpauth "$MOD" 2>/dev/null || true
xattr -c /usr/local/bin/fpauth "$MOD" 2>/dev/null || true
/usr/local/bin/fpauth pamtest "$MOD" || { echo "PAM module does not load -> NOT enabling it for sudo"; exit 1; }
if ! grep -q pam_fpauth /etc/pam.d/sudo_local 2>/dev/null; then
  [[ -f /etc/pam.d/sudo_local ]] && cp /etc/pam.d/sudo_local /etc/pam.d/sudo_local.bak-fpauth
  { echo "$LINE"; cat /etc/pam.d/sudo_local 2>/dev/null || true; } > /tmp/sudo_local.fpauth
  install -m 444 -o root -g wheel /tmp/sudo_local.fpauth /etc/pam.d/sudo_local
  rm -f /tmp/sudo_local.fpauth
fi
echo "--- /etc/pam.d/sudo_local:"; cat /etc/pam.d/sudo_local
if [[ ! -f /var/db/fpauth/${SUDO_USER}.tpl ]]; then
  echo; echo "Now enrolling your finger (user ${SUDO_USER}):"
  /usr/local/bin/fpauth enroll "$SUDO_USER" 20
fi
echo; echo "Done. Test in a NEW terminal:  sudo -k; sudo echo ok   (touch the sensor)"
