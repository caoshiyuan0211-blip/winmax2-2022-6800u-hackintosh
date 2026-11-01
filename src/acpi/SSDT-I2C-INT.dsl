/*
 * Win Max 2 I2C HID interrupt fixup (needs OC ACPI patches TPDD/TPNL _CRS -> XCRS), Darwin only.
 * - TPNL (Goodix GXTP7385 touchscreen): firmware GpioInt is Edge; VoodooI2C disables the pin while it
 *   reads a report, so an edge during that window is lost and the line stays low (HID over I2C INT is
 *   level, active low) -> same GpioInt as Level.
 * - TPDD (PixArt touchpad): GpioInt dropped so VoodooI2C keeps polling it (works today), since the
 *   global -vi2c-force-polling boot-arg is gone.
 */
DefinitionBlock ("", "SSDT", 2, "HKVM", "I2CINT", 0x00001000)
{
    External (_SB_.I2CA.TPDD, DeviceObj)
    External (_SB_.I2CA.TPDD.XCRS, MethodObj)
    External (_SB_.I2CB.TPNL, DeviceObj)
    External (_SB_.I2CB.TPNL.XCRS, MethodObj)

    Scope (\_SB.I2CA.TPDD)
    {
        Method (_CRS, 0, NotSerialized)
        {
            If (_OSI ("Darwin"))
            {
                Return (ResourceTemplate ()
                {
                    I2cSerialBusV2 (0x002C, ControllerInitiated, 0x00061A80,
                        AddressingMode7Bit, "\\_SB.I2CA",
                        0x00, ResourceConsumer, , Exclusive,
                        )
                })
            }

            Return (XCRS ())
        }
    }

    Scope (\_SB.I2CB.TPNL)
    {
        Method (_CRS, 0, NotSerialized)
        {
            If (_OSI ("Darwin"))
            {
                Return (ResourceTemplate ()
                {
                    I2cSerialBusV2 (0x005D, ControllerInitiated, 0x00061A80,
                        AddressingMode7Bit, "\\_SB.I2CB",
                        0x00, ResourceConsumer, , Exclusive,
                        )
                    GpioInt (Level, ActiveLow, ExclusiveAndWake, PullUp, 0x0000,
                        "\\_SB.GPIO", 0x00, ResourceConsumer, ,
                        )
                        {   // Pin list
                            0x0059
                        }
                })
            }

            Return (XCRS ())
        }
    }
}
