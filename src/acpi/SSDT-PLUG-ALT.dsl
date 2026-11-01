/*
 * Win Max 2 (Ryzen 7 6800U): firmware declares the 16 CPUs as ACPI0007 devices (\_SB.PLTF.P000-P00F),
 * which macOS can't use. Legacy Processor objects with the MADT processor IDs 1..16, Darwin only.
 */
DefinitionBlock ("", "SSDT", 2, "HKVM", "CpuPlugA", 0x00003000)
{
    Scope (\_SB)
    {
        Processor (CP00, 0x01, 0x00000000, 0x00)
        {
            Method (_STA, 0, NotSerialized) { If (_OSI ("Darwin")) { Return (0x0F) } Return (Zero) }
        }

        Processor (CP01, 0x02, 0x00000000, 0x00)
        {
            Method (_STA, 0, NotSerialized) { If (_OSI ("Darwin")) { Return (0x0F) } Return (Zero) }
        }

        Processor (CP02, 0x03, 0x00000000, 0x00)
        {
            Method (_STA, 0, NotSerialized) { If (_OSI ("Darwin")) { Return (0x0F) } Return (Zero) }
        }

        Processor (CP03, 0x04, 0x00000000, 0x00)
        {
            Method (_STA, 0, NotSerialized) { If (_OSI ("Darwin")) { Return (0x0F) } Return (Zero) }
        }

        Processor (CP04, 0x05, 0x00000000, 0x00)
        {
            Method (_STA, 0, NotSerialized) { If (_OSI ("Darwin")) { Return (0x0F) } Return (Zero) }
        }

        Processor (CP05, 0x06, 0x00000000, 0x00)
        {
            Method (_STA, 0, NotSerialized) { If (_OSI ("Darwin")) { Return (0x0F) } Return (Zero) }
        }

        Processor (CP06, 0x07, 0x00000000, 0x00)
        {
            Method (_STA, 0, NotSerialized) { If (_OSI ("Darwin")) { Return (0x0F) } Return (Zero) }
        }

        Processor (CP07, 0x08, 0x00000000, 0x00)
        {
            Method (_STA, 0, NotSerialized) { If (_OSI ("Darwin")) { Return (0x0F) } Return (Zero) }
        }

        Processor (CP08, 0x09, 0x00000000, 0x00)
        {
            Method (_STA, 0, NotSerialized) { If (_OSI ("Darwin")) { Return (0x0F) } Return (Zero) }
        }

        Processor (CP09, 0x0A, 0x00000000, 0x00)
        {
            Method (_STA, 0, NotSerialized) { If (_OSI ("Darwin")) { Return (0x0F) } Return (Zero) }
        }

        Processor (CP0A, 0x0B, 0x00000000, 0x00)
        {
            Method (_STA, 0, NotSerialized) { If (_OSI ("Darwin")) { Return (0x0F) } Return (Zero) }
        }

        Processor (CP0B, 0x0C, 0x00000000, 0x00)
        {
            Method (_STA, 0, NotSerialized) { If (_OSI ("Darwin")) { Return (0x0F) } Return (Zero) }
        }

        Processor (CP0C, 0x0D, 0x00000000, 0x00)
        {
            Method (_STA, 0, NotSerialized) { If (_OSI ("Darwin")) { Return (0x0F) } Return (Zero) }
        }

        Processor (CP0D, 0x0E, 0x00000000, 0x00)
        {
            Method (_STA, 0, NotSerialized) { If (_OSI ("Darwin")) { Return (0x0F) } Return (Zero) }
        }

        Processor (CP0E, 0x0F, 0x00000000, 0x00)
        {
            Method (_STA, 0, NotSerialized) { If (_OSI ("Darwin")) { Return (0x0F) } Return (Zero) }
        }

        Processor (CP0F, 0x10, 0x00000000, 0x00)
        {
            Method (_STA, 0, NotSerialized) { If (_OSI ("Darwin")) { Return (0x0F) } Return (Zero) }
        }
    }

    Method (\_SB.CP00._DSM, 4, NotSerialized)
    {
        If ((Arg2 == Zero)) { Return (Buffer (One) { 0x03 }) }
        Return (Package (0x02) { "plugin-type", One })
    }
}
