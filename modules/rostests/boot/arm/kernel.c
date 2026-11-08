/*
 * PROJECT:     LiberNT boot tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARM32 kernel architecture helper regression tests
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ntoskrnl/include/ntoskrnl.h>

static KPRCB Prcb;

BOOLEAN ArmTestKernelFaultDecoding(void)
{
    return MI_IS_NOT_PRESENT_FAULT(0x206) && MI_IS_NOT_PRESENT_FAULT(0xA07) &&
           !MI_IS_NOT_PRESENT_FAULT(0x20F) && !MI_IS_WRITE_ACCESS(0x206) &&
           MI_IS_WRITE_ACCESS(0xA06) && !MI_IS_INSTRUCTION_FETCH(0x206) &&
           MI_IS_INSTRUCTION_FETCH(0x206 | MI_ARM_FAULT_EXECUTE);
}

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

BOOLEAN ArmTestKernelPrcbHooks(void)
{
    ULONG64 Before, After;

    Prcb.DpcRequestSummary = 0x40010002;
    if (KiPrcbTimerPending(&Prcb)) return FALSE;
    KiPrcbRequestTimer(&Prcb, NULL, 0x12345678);
    if (!KiPrcbTimerPending(&Prcb) || Prcb.LastTimerHand != 0x12345678)
        return FALSE;
    if (KiPrcbTakeTimer(&Prcb) != 0x12345678 || KiPrcbTimerPending(&Prcb) ||
        Prcb.DpcRequestSummary != 0x40010002)
        return FALSE;
    InterlockedOr(&Prcb.DpcRequestSummary, 4);
    if (!KiTakeDpcThreadSignal(&Prcb) || KiTakeDpcThreadSignal(&Prcb) ||
        Prcb.DpcRequestSummary != 0x40010002)
        return FALSE;
    Prcb.Number = 3;
    Prcb.GroupSetMember = 8;
    Prcb.CoreProcessorSet = 0;
    if (KiPrcbAffinity(&Prcb) != 8 || KiPrcbCoreSet(&Prcb) != 8 ||
        KiPrcbCoreIndex(&Prcb) != 3 || !KiPrcbIsCorePrimary(&Prcb))
        return FALSE;
    Prcb.CoreProcessorSet = 12;
    if (KiPrcbCoreIndex(&Prcb) != 2 || KiPrcbIsCorePrimary(&Prcb))
        return FALSE;
    Prcb.Number = 2;
    Prcb.GroupSetMember = 4;
    if (!KiPrcbIsCorePrimary(&Prcb) || KiPrcbCoreSet(&Prcb) != 12)
        return FALSE;
    Before = ReadTimeStampCounter();
    YieldProcessor();
    After = ReadTimeStampCounter();
    return After >= Before && After != 0;
}
