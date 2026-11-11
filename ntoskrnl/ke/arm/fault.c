/*
 * PROJECT:     LiberNT Kernel
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARMv7 LPAE abort delivery to NVS and exception dispatch
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ntoskrnl.h>
#include <internal/arm/abort.h>

NTSTATUS KiArmGrowKernelFaultStack(ULONG_PTR StackPointer);

static NTSTATUS KiArmMemoryAbort(PKTRAP_FRAME Trap, BOOLEAN Execute)
{
    ULONG Fault = Trap->FaultStatus;
    KPROCESSOR_MODE Mode = KiGetPreviousMode(Trap);
    PKEXCEPTION_FRAME Exception = (PKEXCEPTION_FRAME)Trap - 1;
    PKTHREAD Thread = KeGetCurrentThread();
    PKTRAP_FRAME OldTrap = Thread->TrapFrame;
    NTSTATUS Status = STATUS_ACCESS_VIOLATION;
    EXCEPTION_RECORD Record = {0};

    Thread->TrapFrame = Trap;
    if (Execute)
        Fault |= MI_ARM_FAULT_EXECUTE;
    if (KiArmAbortCanPage(Fault))
    {
        ULONG_PTR StackPointer;

        __asm__ __volatile__("mov %0, sp" : "=r"(StackPointer));
        _enable();
        Status = KiArmGrowKernelFaultStack(StackPointer);
        if (NT_SUCCESS(Status))
            Status = MmAccessFault(Fault, (PVOID)Trap->FaultAddress, Mode, Trap);
        _disable();
        if (NT_SUCCESS(Status))
        {
            Thread->TrapFrame = OldTrap;
            return STATUS_SUCCESS;
        }
    }
    if (Mode == KernelMode)
    {
        ULONG Frame = Trap->R11, Index;
        ULONG_PTR Limit = (ULONG_PTR)Thread->StackLimit;
        ULONG_PTR Base = (ULONG_PTR)Thread->StackBase;
        DbgPrint("ARM abort pc=%08lx lr=%08lx sp=%08lx fp=%08lx fsr=%08lx far=%08lx r0=%08lx r1=%08lx thread=%p stack=%p-%p\n",
                 Trap->Pc, Trap->Lr, Trap->Sp, Trap->R11, Trap->FaultStatus,
                 Trap->FaultAddress, Trap->R0, Trap->R1, Thread, Thread->StackLimit, Thread->StackBase);
        for (Index = 0; Index < 12 && !(Frame & 3) && Frame >= Limit && Frame <= Base - 8; ++Index)
        {
            PULONG Link = (PULONG)(ULONG_PTR)Frame;
            DbgPrint("ARM frame %lu fp=%08lx return=%08lx\n", Index, Frame, Link[1]);
            if (Link[0] <= Frame) break;
            Frame = Link[0];
        }
        if (Trap->R0 == Trap->FaultAddress && Trap->R1 >= FIELD_OFFSET(IRP, IoStatus))
        {
            PIRP Irp = CONTAINING_RECORD((PIO_STATUS_BLOCK)(ULONG_PTR)Trap->R1, IRP, IoStatus);
            if (MmIsAddressValid(Irp) && MmIsAddressValid((PUCHAR)Irp + sizeof(*Irp) - 1) &&
                Irp->Type == IO_TYPE_IRP && Irp->StackCount > 0 && Irp->StackCount < 32)
            {
                PIO_STACK_LOCATION Top = (PIO_STACK_LOCATION)(Irp + 1) + Irp->StackCount - 1;
                DbgPrint("ARM IOSB irp=%p flags=%08lx pending=%u mode=%u iosb=%p event=%p apcthread=%p spare=%08lx\n",
                         Irp, Irp->Flags, Irp->PendingReturned, Irp->RequestorMode,
                         Irp->UserIosb, Irp->UserEvent, Irp->Tail.Apc.Thread, Irp->Tail.Apc.SpareLong0);
                if (MmIsAddressValid(Top) && MmIsAddressValid((PUCHAR)Top + sizeof(*Top) - 1))
                    DbgPrint("ARM IOSB top major=%u minor=%u device=%p file=%p\n",
                             Top->MajorFunction, Top->MinorFunction, Top->DeviceObject, Top->FileObject);
            }
        }
    }
    Record.ExceptionCode = Status;
    Record.ExceptionAddress = (PVOID)(Trap->Pc & ~1UL);
    Record.NumberParameters = 2;
    Record.ExceptionInformation[0] = Execute ? 8 : (MI_IS_WRITE_ACCESS(Fault) ? 1 : 0);
    Record.ExceptionInformation[1] = Trap->FaultAddress;
    if (KiArmAbortAlignment(Fault))
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

    if (KiArmAbortDebug(TrapFrame->FaultStatus))
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
