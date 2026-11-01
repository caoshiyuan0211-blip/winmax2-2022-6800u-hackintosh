/*
 * Win Max 2: XHC0/XHC1 (GP17, GPE 0x19) and XHC3/XHC4 (GP19 USB4 xHCIs, GPE 0x1A) must not wake macOS
 * from S3 (needs OC ACPI patches _PRW -> XPRW: XHC0/XHC1 in SSDT10 20419 bytes, XHC3/XHC4 in SSDT9 38009),
 * Darwin only. After the SWUS fix (10-27 14:50) every sleep still woke at once:
 * "DarkWake from Normal Sleep [CDN] : due to XHC0 XHC1 XHC3 XHC4" -> sleep/wake loop.
 * Linux keeps USB wake disarmed on every root hub/device (power/wakeup disabled) and never self-wakes;
 * this matches it. Power button (and EC) still wake. _PRW sleep state 0 = no wake from S3.
 */
DefinitionBlock ("", "SSDT", 2, "HKVM", "USBNOWK", 0x00001000)
{
    External (_SB_.PCI0.GP17.XHC0, DeviceObj)
    External (_SB_.PCI0.GP17.XHC0.XPRW, MethodObj)
    External (_SB_.PCI0.GP17.XHC1, DeviceObj)
    External (_SB_.PCI0.GP17.XHC1.XPRW, MethodObj)
    External (_SB_.PCI0.GP19.XHC3, DeviceObj)
    External (_SB_.PCI0.GP19.XHC3.XPRW, MethodObj)
    External (_SB_.PCI0.GP19.XHC4, DeviceObj)
    External (_SB_.PCI0.GP19.XHC4.XPRW, MethodObj)

    Scope (\_SB.PCI0.GP17.XHC0)
    {
        Method (_PRW, 0, NotSerialized)
        {
            If (_OSI ("Darwin"))
            {
                Return (Package (0x02)
                {
                    0x19,
                    Zero
                })
            }

            Return (XPRW ())
        }
    }

    Scope (\_SB.PCI0.GP17.XHC1)
    {
        Method (_PRW, 0, NotSerialized)
        {
            If (_OSI ("Darwin"))
            {
                Return (Package (0x02)
                {
                    0x19,
                    Zero
                })
            }

            Return (XPRW ())
        }
    }

    Scope (\_SB.PCI0.GP19.XHC3)
    {
        Method (_PRW, 0, NotSerialized)
        {
            If (_OSI ("Darwin"))
            {
                Return (Package (0x02)
                {
                    0x1A,
                    Zero
                })
            }

            Return (XPRW ())
        }
    }

    Scope (\_SB.PCI0.GP19.XHC4)
    {
        Method (_PRW, 0, NotSerialized)
        {
            If (_OSI ("Darwin"))
            {
                Return (Package (0x02)
                {
                    0x1A,
                    Zero
                })
            }

            Return (XPRW ())
        }
    }
}
