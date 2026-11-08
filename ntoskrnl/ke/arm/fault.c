/*
 * PROJECT:     LiberNT Kernel
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARMv7 LPAE abort delivery to NVS and exception dispatch
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ntoskrnl.h>

static NTSTATUS KiArmMemoryAbort(PKTRAP_FRAME Trap, BOOLEAN Execute)
{
    ULONG Fault = Trap->FaultStatus;
    ULONG StatusCode = Fault & 0x3F;
    KPROCESSOR_MODE Mode = KiGetPreviousMode(Trap);
    PKEXCEPTION_FRAME Exception = (PKEXCEPTION_FRAME)Trap - 1;
    PKTHREAD Thread = KeGetCurrentThread();
    PKTRAP_FRAME OldTrap = Thread->TrapFrame;
    NTSTATUS Status = STATUS_ACCESS_VIOLATION;
    EXCEPTION_RECORD Record = {0};

    Thread->TrapFrame = Trap;
    if (Execute)
        Fault |= MI_ARM_FAULT_EXECUTE;
    if (StatusCode >= 4 && StatusCode <= 15)
    {
        _enable();
        Status = MmAccessFault(Fault, (PVOID)Trap->FaultAddress, Mode, Trap);
        _disable();
        if (NT_SUCCESS(Status))
        {
            Thread->TrapFrame = OldTrap;
            return STATUS_SUCCESS;
        }
    }
    Record.ExceptionCode = Status;
    Record.ExceptionAddress = (PVOID)(Trap->Pc & ~1UL);
    Record.NumberParameters = 2;
    Record.ExceptionInformation[0] = Execute ? 8 : (MI_IS_WRITE_ACCESS(Fault) ? 1 : 0);
    Record.ExceptionInformation[1] = Trap->FaultAddress;
    if (StatusCode == 0x21)
    {
        Record.ExceptionCode = STATUS_DATATYPE_MISALIGNMENT;
        Record.NumberParameters = 0;
    }
    else if (Status != STATUS_ACCESS_VIOLATION && Status != STATUS_GUARD_PAGE_VIOLATION && Status != STATUS_STACK_OVERFLOW)
    {
        Record.ExceptionCode = STATUS_IN_PAGE_ERROR;
        Record.NumberParameters = 3;
        Record.ExceptionInformation[2] = Status;
    }
    KiDispatchException(&Record, Exception, Trap, Mode, TRUE);
    Thread->TrapFrame = OldTrap;
    return STATUS_SUCCESS;
}

NTSTATUS KiDataAbortHandler(PKTRAP_FRAME TrapFrame)
{
    return KiArmMemoryAbort(TrapFrame, FALSE);
}

NTSTATUS KiPrefetchAbortHandler(PKTRAP_FRAME TrapFrame)
{
    EXCEPTION_RECORD Record = {0};

    if ((TrapFrame->FaultStatus & 0x3F) == 0x22)
    {
        Record.ExceptionCode = STATUS_BREAKPOINT;
        Record.ExceptionAddress = (PVOID)(TrapFrame->Pc & ~1UL);
        Record.NumberParameters = 1;
        KiDispatchException(&Record, (PKEXCEPTION_FRAME)TrapFrame - 1, TrapFrame,
                            KiGetPreviousMode(TrapFrame), TRUE);
        return STATUS_SUCCESS;
    }
    return KiArmMemoryAbort(TrapFrame, TRUE);
}
