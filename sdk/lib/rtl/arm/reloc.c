/*
 * PROJECT:     LiberNT runtime library
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     PE ARM MOV32 relocation handling
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ntdef.h>

static USHORT RtlpThumbImmediate(PUSHORT Instruction)
{
    return ((Instruction[0] & 15) << 12) | ((Instruction[0] & 0x400) << 1) |
           ((Instruction[1] & 0x7000) >> 4) | (Instruction[1] & 255);
}

static VOID RtlpSetThumbImmediate(PUSHORT Instruction, USHORT Value)
{
    Instruction[0] = (Instruction[0] & ~0x40F) | (Value >> 12) | ((Value >> 1) & 0x400);
    Instruction[1] = (Instruction[1] & ~0x70FF) | ((Value << 4) & 0x7000) | (Value & 255);
}

BOOLEAN RtlpProcessMachineRelocation(ULONG Type, PUSHORT Address, LONGLONG Delta)
{
    ULONG Value, Low, High;

    if (Type == 7)
    {
        if ((Address[0] & 0xFBF0) != 0xF240 || (Address[2] & 0xFBF0) != 0xF2C0 ||
            ((Address[1] | Address[3]) & 0x8000) || ((Address[1] ^ Address[3]) & 0xF00))
            return FALSE;
        Value = RtlpThumbImmediate(Address) | ((ULONG)RtlpThumbImmediate(Address + 2) << 16);
        Value += (ULONG)Delta;
        RtlpSetThumbImmediate(Address, (USHORT)Value);
        RtlpSetThumbImmediate(Address + 2, (USHORT)(Value >> 16));
        return TRUE;
    }
    if (Type == 5)
    {
        Low = Address[0] | ((ULONG)Address[1] << 16);
        High = Address[2] | ((ULONG)Address[3] << 16);
        if ((Low & 0x0FF00000) != 0x03000000 || (High & 0x0FF00000) != 0x03400000 ||
            ((Low ^ High) & 0xF000))
            return FALSE;
        Value = (Low & 0xFFF) | ((Low >> 4) & 0xF000) |
                ((High & 0xFFF) << 16) | ((High << 12) & 0xF0000000);
        Value += (ULONG)Delta;
        Low = (Low & ~0xF0FFF) | (Value & 0xFFF) | ((Value << 4) & 0xF0000);
        High = (High & ~0xF0FFF) | ((Value >> 16) & 0xFFF) | ((Value >> 12) & 0xF0000);
        Address[0] = (USHORT)Low;
        Address[1] = (USHORT)(Low >> 16);
        Address[2] = (USHORT)High;
        Address[3] = (USHORT)(High >> 16);
        return TRUE;
    }
    return FALSE;
}
