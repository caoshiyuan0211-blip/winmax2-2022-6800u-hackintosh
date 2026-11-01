#!/bin/bash
# The GPD BIOS throws away the OpenCore boot entry, but keeps its "nvme: grub" entry
# (\EFI\GRUB\grubx64.efi). So GRUB started from that path goes straight to OpenCore
# (no menu), and OpenCore's "Arch Linux" entry starts a second copy of GRUB in
# \EFI\GRUBLNX\, which skips the OpenCore jump and boots Linux.
# Also deletes the broken "sequoia" entry (Boot0080 = macOS boot.efi without OpenCore,
# fails, so the BIOS falls through to Windows).
# Undo: rm /etc/grub.d/00_0opencore && grub-mkconfig -o /boot/grub/grub.cfg
set -e
[ "$(id -u)" = 0 ] || { echo "run with sudo"; exit 1; }
mkdir -p /boot/EFI/GRUBLNX
cp /boot/EFI/GRUB/grubx64.efi /boot/EFI/GRUBLNX/grubx64.efi
cat > /etc/grub.d/00_0opencore <<'EOS'
#!/bin/sh
# Runs before 00_header: firmware-started GRUB (\EFI\GRUB) chains to OpenCore at once.
# OpenCore's Linux entry uses \EFI\GRUBLNX\grubx64.efi, which falls through to Linux.
cat <<'EOF'
insmod regexp
insmod chain
if ! regexp '/EFI/GRUBLNX$' "$cmdpath"; then
	if [ -f /EFI/OC/OpenCore.efi ]; then
		chainloader /EFI/OC/OpenCore.efi
		boot
	fi
fi
EOF
EOS
chmod +x /etc/grub.d/00_0opencore
grub-mkconfig -o /boot/grub/grub.cfg
grub-script-check /boot/grub/grub.cfg && echo "grub.cfg syntax OK"
echo "--- start of grub.cfg:"; sed -n '/00_0opencore/,/END/p' /boot/grub/grub.cfg
if efibootmgr | grep -q '^Boot0080.*sequoia'; then efibootmgr -b 0080 -B >/dev/null && echo "deleted Boot0080 sequoia"; fi
efibootmgr | grep -E '^Boot'
