/*
 * PROJECT:     LiberNT boot tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARM32 kernel architecture helper regression tests
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ntoskrnl/include/ntoskrnl.h>

static KPRCB Prcb;

BOOLEAN ArmTestKernelDpcState(void)
{
    LONG Preserved = 0x40000008;

    Prcb.DpcRequestSummary = Preserved;
    KiSetDpcInterruptRequested(&Prcb);
    if (!KiIsDpcInterruptRequested(&Prcb) || !Prcb.DpcNormalProcessingRequested)
        return FALSE;
    KiSetDpcPresent(&Prcb);
    KiSetDpcThreadActive(&Prcb);
    KiSetDpcThreadRequested(&Prcb);
    if (!KiIsDpcThreadActive(&Prcb) || !KiIsDpcThreadRequested(&Prcb) ||
        !Prcb.DpcNormalDpcPresent || !Prcb.DpcThreadActive || !Prcb.DpcThreadRequested)
        return FALSE;
    KiClearDpcInterruptRequested(&Prcb);
    if (KiIsDpcInterruptRequested(&Prcb) || !Prcb.DpcNormalDpcPresent)
        return FALSE;
    KiClearDpcRequestState(&Prcb);
    if (Prcb.DpcNormalProcessingRequested || Prcb.DpcNormalDpcPresent ||
        !KiIsDpcThreadActive(&Prcb) || !KiIsDpcThreadRequested(&Prcb))
        return FALSE;
    KiClearDpcThreadState(&Prcb);
    return !KiIsDpcThreadActive(&Prcb) && !KiIsDpcThreadRequested(&Prcb) &&
           Prcb.DpcRequestSummary == Preserved;
}

BOOLEAN ArmTestKernelMmio(void)
{
    {
        volatile UCHAR Registers[5] = {1, 2, 3, 4, 5};
        UCHAR Buffer[5] = {6, 7, 8, 9, 10};
        ULONG Index;

        WRITE_REGISTER_UCHAR(&Registers[1], 0xa5);
        if (READ_REGISTER_UCHAR(&Registers[1]) != 0xa5 ||
            Registers[0] != 1 || Registers[2] != 3)
            return FALSE;
        READ_REGISTER_BUFFER_UCHAR(&Registers[1], &Buffer[1], 3);
        if (Buffer[0] != 6 || Buffer[4] != 10)
            return FALSE;
        for (Index = 1; Index < 4; ++Index)
            if (Buffer[Index] != Registers[Index]) return FALSE;
        Buffer[1] = 11;
        Buffer[2] = 12;
        Buffer[3] = 13;
        WRITE_REGISTER_BUFFER_UCHAR(&Registers[1], &Buffer[1], 3);
        if (Registers[0] != 1 || Registers[4] != 5)
            return FALSE;
        for (Index = 1; Index < 4; ++Index)
            if (Registers[Index] != Buffer[Index]) return FALSE;
        READ_REGISTER_BUFFER_UCHAR(NULL, Buffer, 0);
        WRITE_REGISTER_BUFFER_UCHAR(NULL, Buffer, 0);
    }
    {
        volatile USHORT Registers[5] = {1, 2, 3, 4, 5};
        USHORT Buffer[5] = {6, 7, 8, 9, 10};
        ULONG Index;

        WRITE_REGISTER_USHORT(&Registers[1], 0xa5b6);
        if (READ_REGISTER_USHORT(&Registers[1]) != 0xa5b6 ||
            Registers[0] != 1 || Registers[2] != 3)
            return FALSE;
        READ_REGISTER_BUFFER_USHORT(&Registers[1], &Buffer[1], 3);
        if (Buffer[0] != 6 || Buffer[4] != 10)
            return FALSE;
        for (Index = 1; Index < 4; ++Index)
            if (Buffer[Index] != Registers[Index]) return FALSE;
        Buffer[1] = 11;
        Buffer[2] = 12;
        Buffer[3] = 13;
        WRITE_REGISTER_BUFFER_USHORT(&Registers[1], &Buffer[1], 3);
        if (Registers[0] != 1 || Registers[4] != 5)
            return FALSE;
        for (Index = 1; Index < 4; ++Index)
            if (Registers[Index] != Buffer[Index]) return FALSE;
        READ_REGISTER_BUFFER_USHORT(NULL, Buffer, 0);
        WRITE_REGISTER_BUFFER_USHORT(NULL, Buffer, 0);
    }
    {
        volatile ULONG Registers[5] = {1, 2, 3, 4, 5};
        ULONG Buffer[5] = {6, 7, 8, 9, 10};
        ULONG Index;

        WRITE_REGISTER_ULONG(&Registers[1], 0xa5b6c7d8);
        if (READ_REGISTER_ULONG(&Registers[1]) != 0xa5b6c7d8 ||
            Registers[0] != 1 || Registers[2] != 3)
            return FALSE;
        READ_REGISTER_BUFFER_ULONG(&Registers[1], &Buffer[1], 3);
        if (Buffer[0] != 6 || Buffer[4] != 10)
            return FALSE;
        for (Index = 1; Index < 4; ++Index)
            if (Buffer[Index] != Registers[Index]) return FALSE;
        Buffer[1] = 11;
        Buffer[2] = 12;
        Buffer[3] = 13;
        WRITE_REGISTER_BUFFER_ULONG(&Registers[1], &Buffer[1], 3);
        if (Registers[0] != 1 || Registers[4] != 5)
            return FALSE;
        for (Index = 1; Index < 4; ++Index)
            if (Registers[Index] != Buffer[Index]) return FALSE;
        READ_REGISTER_BUFFER_ULONG(NULL, Buffer, 0);
        WRITE_REGISTER_BUFFER_ULONG(NULL, Buffer, 0);
    }
    return TRUE;
}
