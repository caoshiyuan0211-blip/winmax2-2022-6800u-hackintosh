/*
 * Win Max 2: USB4 PCIe switch upstream ports GP11.SWUS / GP12.SWUS must not wake macOS from S3
 * (needs OC ACPI patches SWUS _PRW -> XPRW in SSDT9), Darwin only. Linux leaves both disabled in
 * /proc/acpi/wakeup and never self-wakes; macOS woke with "system wake events: SWUS" alone (10-27 14:20).
 * _PRW sleep state 0 = no wake from S3 (GPE 0x1A kept, shared with XHC2/XHC3/XHC4/NHI1).
 */
DefinitionBlock ("", "SSDT", 2, "HKVM", "SWUSNOWK", 0x00001000)
{
    External (_SB_.PCI0.GP11.SWUS, DeviceObj)
    External (_SB_.PCI0.GP11.SWUS.XPRW, MethodObj)
    External (_SB_.PCI0.GP12.SWUS, DeviceObj)
    External (_SB_.PCI0.GP12.SWUS.XPRW, MethodObj)

    Scope (\_SB.PCI0.GP11.SWUS)
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

    Scope (\_SB.PCI0.GP12.SWUS)
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
