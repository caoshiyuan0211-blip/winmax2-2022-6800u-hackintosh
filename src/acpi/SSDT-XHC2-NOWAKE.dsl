/*
 * Win Max 2: XHC2 (74:00.0, internal USB2 hub with camera 058e:3864 + fingerprint 2541:9711) must not
 * wake macOS from S3 (needs OC ACPI patch XHC2 _PRW -> XPRW), Darwin only.
 * Every BM sleep woke right away with "system wake events: SWUS XHC2" into a dark wake.
 * _PRW sleep state 0 = no wake from S3 (GPE 0x1A kept, shared with XHC3/XHC4/NHI1 = USB-C ports).
 */
DefinitionBlock ("", "SSDT", 2, "HKVM", "XHC2NOWK", 0x00001000)
{
    External (_SB_.PCI0.GP19.XHC2, DeviceObj)
    External (_SB_.PCI0.GP19.XHC2.XPRW, MethodObj)

    Scope (\_SB.PCI0.GP19.XHC2)
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
