# winmax2-2022-6800u-hackintosh

macOS on a Radeon 680M, bare metal.

macOS Sequoia 15.8.1 installed directly on a **GPD Win Max 2 (Ryzen 7 6800U, Radeon 680M)**, booted with
OpenCore, with **full Metal acceleration on the 680M** and the laptop's own 2560x1600 panel.

Apple never shipped a driver for Rembrandt (RDNA2 APU: GC 10.3.3, DCN 3.1.2, PSP 13, SMU 13). This project makes
Apple's own **Navi 23 driver stack** (`AMDRadeonX6000`, `AMDRadeonX6000Framebuffer`, HWLibs) run on it through
a Lilu plugin, **RembrandtGfx**, which patches the differences at run time.

It started as a KVM/VFIO passthrough project (see
[macos-kvm-680m](https://github.com/caoshiyuan0211-blip/macos-kvm-680m)). This repo is the bare-metal version
that grew out of it.

## What works

| | |
|---|---|
| Metal / GPU acceleration | ✅ GFX, compute, SDMA (Apple's Navi 23 kexts + RembrandtGfx) |
| Internal eDP panel 2560x1600 @ 60 Hz | ✅ Displays pane sizes, brightness slider and Fn keys |
| HDMI out | ✅ up to 4K |
| USB-C DisplayPort (alt mode) | ✅ 4K30 at 2 lanes, 4K60 with `-rmbusbc4lane` (needs a good, short cable) |
| Sleep (real S3) and wake | ✅ panel and GPU come back |
| Shutdown / restart | ✅ |
| Wi-Fi | ✅ Intel AX210 (AirportItlwm), native Wi-Fi menu |
| Bluetooth | ✅ AX210 BT |
| Touchpad, touchscreen | ✅ VoodooI2C (touchscreen on a real GPIO interrupt, with a tap fix) |
| Keyboard, Mac-style F-row, Fn+F10 screenshot | ✅ VoodooPS2 + SSDT |
| Battery | ✅ SMCBatteryManager |
| Fingerprint reader (CS9711) for `sudo` | ✅ own matcher + PAM module (convenience grade, not Touch ID) |
| OpenCore GUI, boot chime | ✅ chime on warm boot (see below) |

## What doesn't (yet)

- **Hardware video decode/encode (VCN)** is turned off: Apple's VCN jobs program Navi MMHUB 2.0 registers that are
  other registers on Rembrandt's MMHUB 2.3, so the GPU hangs. VideoToolbox falls back to software codecs.
- **OpenCore boot chime on a cold boot** is silent (warm boot plays). DMA, codec state, GPIO and EC were ruled out.
- **The second USB-C (USB4) port** shows no picture with a plain DP-alt monitor. It doesn't work on Linux or Windows
  either. DisplayPort over USB4 tunnels (DPIA) is not implemented.
- With an NVIDIA eGPU driven by the community NullMoth driver, a monitor on the eGPU can make the internal panel
  update at ~0.5 fps. There is a fix in the kext for this, but it hasn't been tested yet.

## How it works (short)

`src/RembrandtGfx` is a Lilu plugin. Main pieces:

- **Identity:** reports the 680M as Navi 23 (`0x73ff`) to config-space reads, fakes PSP 13.0.3 → 11.0.11 and
  GC 10.3.3 → Navi 23 versions where Apple checks them.
- **SMU:** a shim that sends real SMU 13 mailbox messages instead of Navi 23 ones; PowerPlay steps that cannot
  apply to an APU are skipped.
- **Firmware:** loads the yellow_carp GC/SDMA/DMUB firmware (split from linux-firmware at build time; not shipped).
- **Memory:** UMA carve-out + GART/VMID fixes, IH ring cache flush (bare metal has no IOMMU coherency).
- **Display:** remaps every DCN 3.0.2 register access to DCN 3.1.2, filters DMUB commands that kill the PHY,
  programs things Navi DAL never does (DET, system aperture, pipe count 5 → 4, USB-C PHY mux, z10 off),
  retrains the eDP link after wake and revives the panel power sequencer.
- **Sleep:** PSP teardown, GART save/restore, USB4 host-router sleep handshake (the firmware resets the machine if
  that is skipped).
- **Misc:** USB xHCI interrupter fix (Bluetooth was 1000x slow), disk kernel log at
  `/private/var/log/rmb-msgbuf.log`.

Most fixes can be turned off with an `-rmbno...` boot-arg. `grep checkKernelArgument src/RembrandtGfx/*.cpp` lists them.

## Layout

```
src/RembrandtGfx         the GPU/display kext (GPL-2.0)
src/HdaSnoop             small UEFI driver for the OpenCore chime (HDA codec power-up)
src/acpi                 SSDTs (CPU plug, EC/USBX, PNLF, I2C GPIO interrupt, USB/USB4 no-wake, F-row map)
src/VoodooI2C-rmb        VoodooI2C 2.9.1 + touchscreen tap fix (VoodooI2CHID satellite has a Makefile)
src/VoodooPS2Keyboard-rmb  VoodooPS2 2.3.7 keyboard plugin + Fn+F10 = Cmd+Shift+3
src/BrightnessKeys-rmb   BrightnessKeys 1.0.3 + ACPI _BCL/_BCM panel fallback
src/fpauth               CS9711 fingerprint capture, matcher, CLI, PAM module, macOS installer
efi/config.plist.example OpenCore 1.0.5 config (serials and VBIOS removed)
scripts/                 VBIOS extraction, firmware split, OpenCore install, NVRAM panic decoder
linux/                   optional Linux-side helpers (GRUB → OpenCore chain, install OpenCore at shutdown)
```

## Building

Everything builds on Linux with clang:

- an ld64 from [cctools-port](https://github.com/tpoechtrager/cctools-port) at `build/cctools`
- [MacKernelSDK](https://github.com/acidanthera/MacKernelSDK) at `src/MacKernelSDK`
- [Lilu](https://github.com/acidanthera/Lilu) 1.7.2 source at `src/Lilu-1.7.2`
- for the keyboard plugin, the VoodooPS2 2.3.7 source at `build/dl/vps2-237/VoodooPS2-2.3.7`

```sh
cd src/RembrandtGfx
make            # also splits /usr/lib/firmware/amdgpu/yellow_carp_*.bin into fw/ (FWSRC=... to override)
make install    # copies RembrandtGfx.kext into efi/EFI/OC/Kexts
```

`make check` (part of `install`) compares the kext's imports with symbol lists of the macOS 15.8.1 kernel and
Lilu in `build/kpi/` (`llvm-nm` of the KDK kernel). Drop it from the `install` rule if you don't have them.

`src/HdaSnoop`: `make` (clang + lld-link). `src/fpauth`: `make` cross-builds the macOS binaries against an
Xcode `MacOSX.sdk` (`SDK=...`).

## Installing (short version)

1. Install macOS 15.8.1 on its own APFS partition (we copied a VM's disk with `dd`; an installer USB works too).
2. Build the EFI from `efi/config.plist.example` + OpenCore 1.0.5 + the kexts listed in it (Lilu, VirtualSMC,
   SMCBatteryManager, RTCMemoryFixup, NVMeFix, AmdTscSync, VoodooPS2, VoodooI2C, BrightnessKeys,
   AppleMCEReporterDisabler, BlueToolFixup, IntelBluetoothFirmware, IntelBTPatcher, AirportItlwm for Sequoia)
   plus RembrandtGfx and the SSDTs.
3. Make the VBIOS for `ATY,bin_image` (removed from the example): on Linux, copy
   `/sys/firmware/acpi/tables/VFCT`, then `python3 scripts/vfct-rom.py VFCT` and
   `python3 scripts/augment-rom.py roms/680m-73ff.rom roms/680m-73ff-aug.rom`. Add it as data under the
   GPU's `DeviceProperties` (`PciRoot(0x0)/Pci(0x8,0x1)/Pci(0x0,0x0)` here), next to `device-id` `ff730000`.
4. Generate your own SMBIOS (MacPro7,1) for the `CHANGE-ME` fields.
5. Copy to the ESP (`scripts/install-oc-bm.sh`, set `ESP_DISK`/`ESP_PART`).

Notes for this machine: the GPD (AMI) BIOS drops OpenCore's boot entry. `linux/grub-chain-oc.sh` lets the GRUB entry
the BIOS keeps chain to OpenCore. `MmioWhitelist 0xFD000000` is required: without it, EFI runtime `SetVariable`
page-faults in the FCH MMIO region. SSDT-PLUG-ALT is required, because the firmware only has ACPI0007 CPU objects.

Fingerprint: copy `fpauth`, `pam_fpauth.so` and `install-mac.sh` to macOS, run `sudo ./install-mac.sh`. It enrolls
20 presses, then adds the module to `/etc/pam.d/sudo_local`.

## Legal

This repository has only original code and patches. It contains no Apple software, no AMD firmware, no VBIOS
and no serial numbers. You need your own legally obtained copy of macOS. Running macOS on non-Apple hardware may
violate Apple's license terms. Use at your own risk: this is a research project, and a wrong DMUB command can
leave the panel showing lines until you fully power off.

## License

RembrandtGfx, HdaSnoop, fpauth, the SSDTs and the scripts are GPL-2.0 (`LICENSE`). The forks keep their upstream
licenses: VoodooI2C GPL-2.0, VoodooPS2 APSL 2.0, BrightnessKeys BSD-3-Clause.
