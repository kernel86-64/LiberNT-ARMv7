/*
 * PROJECT:     LiberNT API tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Verify the production PL011 debugger receive path
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ntoskrnl/include/ntoskrnl.h>

static ULONG Flags, Data, Reads, Clears;

static ULONG ArmUartRead(PULONG Address)
{
    if (((ULONG_PTR)Address & 0xFFF) == 0x18) return Flags;
    ++Reads;
    return Data;
}

static VOID ArmUartWrite(PULONG Address, ULONG Value)
{
    if (((ULONG_PTR)Address & 0xFFF) == 4 && Value == 0) ++Clears;
}

#undef READ_REGISTER_ULONG
#undef WRITE_REGISTER_ULONG
#define READ_REGISTER_ULONG ArmUartRead
#define WRITE_REGISTER_ULONG ArmUartWrite
#include <ntoskrnl/kd/arm/kdserial.c>

BOOLEAN ArmTestUartReceive(VOID)
{
    UCHAR Byte = 0x5A;

    Flags = 0x10;
    Reads = Clears = 0;
    if (KdPortGetByteEx(NULL, &Byte) || Byte != 0x5A || Reads) return FALSE;
    Flags = 0;
    Data = 0xA5;
    if (!KdPortGetByteEx(NULL, &Byte) || Byte != 0xA5 || Reads != 1) return FALSE;
    Data = 0xF42;
    if (KdPortGetByteEx(NULL, &Byte) || Byte != 0xA5 || Reads != 2 || Clears != 1) return FALSE;
    Data = 0;
    return KdPortGetByteEx(NULL, &Byte) && Byte == 0 && Reads == 3 && Clears == 1;
}
