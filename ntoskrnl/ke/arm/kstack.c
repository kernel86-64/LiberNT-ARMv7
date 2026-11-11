/*
 * PROJECT:     LiberNT Kernel
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARM32 kernel stack relocation
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ntoskrnl/include/ntoskrnl.h>

NTSTATUS KiArmGrowKernelFaultStack(ULONG_PTR StackPointer)
{
    PKTHREAD Thread = KeGetCurrentThread();

    if (!Thread->LargeStack || KeGetCurrentIrql() > APC_LEVEL ||
        StackPointer < Thread->StackLimit || StackPointer > (ULONG_PTR)Thread->StackBase ||
        StackPointer - Thread->StackLimit >= KERNEL_STACK_SIZE)
        return STATUS_SUCCESS;
    return MmGrowKernelStack((PVOID)StackPointer);
}

static BOOLEAN KiArmInStack(ULONG_PTR Address, SIZE_T Length, ULONG_PTR Low, ULONG_PTR High)
{
    return Address >= Low && Address <= High && Length <= High - Address;
}

PVOID KiArmSwitchKernelStack(PVOID StackBase, PVOID StackLimit, PVOID StackPointer, ULONG_PTR FramePointer)
{
    PKTHREAD Thread = KeGetCurrentThread();
    ULONG_PTR OldBase = (ULONG_PTR)Thread->StackBase;
    ULONG_PTR Low = (ULONG_PTR)StackPointer;
    ULONG_PTR Delta = (ULONG_PTR)StackBase - OldBase;
    ULONG_PTR Frame, Previous;
    SIZE_T Size = OldBase - Low, Remaining;
    PKTRAP_FRAME Trap, Next, NewTrap;
    PKARM_VFP_STATE Vfp, Link, NewVfp;
    PKEXCEPTION_FRAME Exception;

    ASSERT(Low >= Thread->StackLimit && Low < OldBase);
    ASSERT(Size <= (ULONG_PTR)StackBase - (ULONG_PTR)StackLimit);
    RtlCopyMemory((PVOID)(Low + Delta), StackPointer, Size);
    Frame = FramePointer;
    Remaining = Size / (2 * sizeof(ULONG_PTR));
    while (Remaining-- && KiArmInStack(Frame, 2 * sizeof(ULONG_PTR), Low, OldBase))
    {
        Previous = *(PULONG_PTR)Frame;
        if (!KiArmInStack(Previous, 2 * sizeof(ULONG_PTR), Low, OldBase) || Previous <= Frame) break;
        *(PULONG_PTR)(Frame + Delta) = Previous + Delta;
        Frame = Previous;
    }
    Trap = Thread->TrapFrame;
    Remaining = Size / sizeof(KTRAP_FRAME);
    while (Remaining-- && KiArmInStack((ULONG_PTR)Trap, sizeof(*Trap), Low, OldBase))
    {
        NewTrap = (PVOID)((ULONG_PTR)Trap + Delta);
        Next = (PVOID)Trap->TrapFrame;
        if (KiArmInStack((ULONG_PTR)Next, sizeof(*Next), Low, OldBase))
            NewTrap->TrapFrame = (ULONG_PTR)Next + Delta;
        Vfp = Trap->VfpState;
        if (KiArmInStack((ULONG_PTR)Vfp, sizeof(*Vfp), Low, OldBase))
        {
            SIZE_T States = Size / sizeof(*Vfp);
            NewTrap->VfpState = (PVOID)((ULONG_PTR)Vfp + Delta);
            while (States-- && KiArmInStack((ULONG_PTR)Vfp, sizeof(*Vfp), Low, OldBase))
            {
                NewVfp = (PVOID)((ULONG_PTR)Vfp + Delta);
                Link = Vfp->Link;
                if (KiArmInStack((ULONG_PTR)Link, sizeof(*Link), Low, OldBase))
                    NewVfp->Link = (PVOID)((ULONG_PTR)Link + Delta);
                if (Link == Vfp) break;
                Vfp = Link;
            }
        }
        Exception = (PKEXCEPTION_FRAME)Trap - 1;
        if (KiArmInStack((ULONG_PTR)Exception, sizeof(*Exception), Low, OldBase) &&
            Exception->TrapFrame == (ULONG_PTR)Trap)
            ((PKEXCEPTION_FRAME)((ULONG_PTR)Exception + Delta))->TrapFrame = (ULONG_PTR)NewTrap;
        if ((Trap->Cpsr & CPSRM_MASK) == CPSRM_USER) break;
        if (KiArmInStack(Trap->Sp, 0, Low, OldBase)) NewTrap->Sp += Delta;
        if (KiArmInStack(Trap->R11, 0, Low, OldBase)) NewTrap->R11 += Delta;
        if (Next == Trap) break;
        Trap = Next;
    }
    if (KiArmInStack((ULONG_PTR)Thread->TrapFrame, sizeof(KTRAP_FRAME), Low, OldBase))
        Thread->TrapFrame = (PVOID)((ULONG_PTR)Thread->TrapFrame + Delta);
    Thread->InitialStack = (PVOID)((ULONG_PTR)Thread->InitialStack + Delta);
    Thread->StackBase = StackBase;
    Thread->StackLimit = (ULONG_PTR)StackLimit;
    Thread->KernelStack = (PVOID)(Low + Delta);
    Thread->LargeStack = TRUE;
    KeGetCurrentPrcb()->SpBase = Thread->InitialStack;
    return (PVOID)OldBase;
}

NTSTATUS KiExpandKernelStackCallout(PEXPAND_STACK_CALLOUT Callout, PVOID Parameter,
                                     SIZE_T Size, BOOLEAN Wait)
{
    PKTHREAD Thread = KeGetCurrentThread();
    PVOID Stack, StackLimit, OldBase, OldInitial, OldKernelStack;
    ULONG_PTR OldLimit, StackPointer;
    ULONG Flags;
    BOOLEAN OldLarge, OldCallout;
    NTSTATUS Status;

    __asm__ __volatile__("mov %0, sp" : "=r"(StackPointer));
    if (StackPointer - Thread->StackLimit >= Size + PAGE_SIZE)
    {
        Callout(Parameter);
        return STATUS_SUCCESS;
    }
    Status = MiAllocateKernelCalloutStack(Wait, &Stack, &StackLimit);
    if (!NT_SUCCESS(Status)) return STATUS_NO_MEMORY;
    __asm__ __volatile__("mrs %0, cpsr\n\tcpsid if" : "=r"(Flags) :: "memory");
    OldBase = Thread->StackBase;
    OldInitial = Thread->InitialStack;
    OldKernelStack = Thread->KernelStack;
    OldLimit = Thread->StackLimit;
    OldLarge = Thread->LargeStack;
    OldCallout = Thread->CalloutActive;
    Thread->StackBase = Thread->InitialStack = Stack;
    Thread->StackLimit = (ULONG_PTR)StackLimit;
    Thread->KernelStack = Stack;
    Thread->LargeStack = TRUE;
    Thread->CalloutActive = TRUE;
    KeGetCurrentPrcb()->SpBase = Stack;
    KiArmCallOnKernelStack(Callout, Parameter, Stack, Flags);
    Thread->StackBase = OldBase;
    Thread->InitialStack = OldInitial;
    Thread->KernelStack = OldKernelStack;
    Thread->StackLimit = OldLimit;
    Thread->LargeStack = OldLarge;
    Thread->CalloutActive = OldCallout;
    KeGetCurrentPrcb()->SpBase = OldInitial;
    __asm__ __volatile__("dsb sy\n\tmsr cpsr_c, %0\n\tisb" :: "r"(Flags) : "memory");
    MmDeleteKernelStack(Stack, TRUE);
    return STATUS_SUCCESS;
}
