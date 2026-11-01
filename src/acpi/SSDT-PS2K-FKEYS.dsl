/*
 * GPD (Ryzen 6800U) internal PS/2 keyboard -> real-Mac function row for VoodooPS2Keyboard.
 *
 * Hardware (Linux capture 2026-10-31): F1-F12 alone = plain F-key scancodes 3b-44,57,58.
 *   Fn+F1/F2 = ACPI LCD notify 0x87/0x86 (BrightnessKeys), Fn+F3 e020 mute, Fn+F4 e02e vol-,
 *   Fn+F5 e030 vol+, Fn+F6 e024 stop, Fn+F7 e010 prev, Fn+F8 e022 play, Fn+F9 e019 next,
 *   Fn+F10 e037 PrtSc, Fn+F11 46 ScrLk, Fn+F12 e052 Insert.
 *
 * Row (user choice 2026-10-31): F1/F2 brightness, F3 mute, F4 vol-, F5 vol+, F6 plain,
 *   F7 prev, F8 play/pause, F9 next, F10-F12 plain; Fn+F10 = Cmd+Shift+3 in both modes
 *   (pseudo scancode e0f5, handled in src/VoodooPS2Keyboard-rmb).
 * "Function Keys Special" = macOS default (alone = Mac function, Fn = plain F-key).
 * "Function Keys Standard" = System Settings "Use F1, F2, etc. keys as standard function keys"
 *   (alone = plain F-key, Fn = Mac function).
 * e0f1/e0f2 are unused pseudo scancodes turned into ADB codes by "Custom ADB Map".
 */
DefinitionBlock ("", "SSDT", 2, "RMB", "PS2KFKEY", 0x00000000)
{
    External (_SB.PCI0.SBRG.PS2K, DeviceObj)

    Scope (_SB.PCI0.SBRG.PS2K)
    {
        Name (RMCF, Package ()
        {
            "Keyboard", Package ()
            {
                "Custom ADB Map", Package ()
                {
                    Package () {},
                    "e0f1=71",  // brightness up   (F15)
                    "e0f2=6b",  // brightness down (F14)
                },
                "Function Keys Special", Package ()
                {
                    Package () {},
                    // F-keys alone -> Mac functions
                    "3b=e0f2",
                    "3c=e0f1",
                    "3d=e020",
                    "3e=e02e",
                    "3f=e030",
                    "40=40",
                    "41=e010",
                    "42=e022",
                    "43=e019",
                    "44=44",
                    "57=57",
                    "58=58",
                    // Fn+F-keys -> plain F-keys
                    "e020=3d",
                    "e02e=3e",
                    "e030=3f",
                    "e024=40",
                    "e010=41",
                    "e022=42",
                    "e019=43",
                    "e037=e0f5",  // Fn+F10 -> Cmd+Shift+3 (patched VoodooPS2Keyboard)
                    "46=57",
                    "e052=58",
                },
                "Function Keys Standard", Package ()
                {
                    Package () {},
                    // F-keys alone -> plain F-keys
                    "3b=3b",
                    "3c=3c",
                    "3d=3d",
                    "3e=3e",
                    "3f=3f",
                    "40=40",
                    "41=41",
                    "42=42",
                    "43=43",
                    "44=44",
                    "57=57",
                    "58=58",
                    // Fn+F-keys -> Mac function of that key position
                    "e020=e020",
                    "e02e=e02e",
                    "e030=e030",
                    "e024=40",
                    "e010=e010",
                    "e022=e022",
                    "e019=e019",
                    "e037=e0f5",  // Fn+F10 -> Cmd+Shift+3 (patched VoodooPS2Keyboard)
                    "46=57",
                    "e052=58",
                },
            },
        })
    }
}
