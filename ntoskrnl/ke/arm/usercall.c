/*
 * PROJECT:         ReactOS Kernel
 * LICENSE:         BSD - See COPYING.ARM in the top level directory
 * FILE:            ntoskrnl/ke/arm/usercall.c
 * PURPOSE:         Implements system calls and user-mode callbacks for ARM
 * PROGRAMMERS:     ReactOS Portable Systems Group
 */

/* INCLUDES *******************************************************************/

#include <ntoskrnl.h>
#define NDEBUG
#include <debug.h>

typedef struct _KI_ARM_CALLBACK_STATE
{
    PVOID PreviousCallback;
    PVOID InitialStack;
    PKTRAP_FRAME TrapFrame;
    PVOID ReturnStack;
    PVOID *OutputBuffer;
    PULONG OutputLength;
    ULONG Cpsr;
    KPROCESSOR_MODE PreviousMode;
    struct
    {
        KEXCEPTION_FRAME Exception;
        KTRAP_FRAME Trap;
        KARM_VFP_STATE Vfp;
    } User;
} KI_ARM_CALLBACK_STATE, *PKI_ARM_CALLBACK_STATE;

DECLSPEC_NORETURN VOID KiArmCallbackReturn(PVOID Stack, NTSTATUS Status, ULONG Cpsr);
NTSTATUS NTAPI KiCallUserMode(PVOID *OutputBuffer, PULONG OutputLength);

NTSTATUS
KiArmEnterCallback(PVOID *OutputBuffer, PULONG OutputLength, PVOID ReturnStack)
{
    PKTHREAD Thread = KeGetCurrentThread();
    KI_ARM_CALLBACK_STATE State;
    ULONG_PTR Stack;
    NTSTATUS Status;

    ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);
    if (!KeUserCallbackDispatcher) return STATUS_PROCEDURE_NOT_FOUND;
    __asm__ __volatile__("mov %0, sp" : "=r"(Stack));
    Stack = ALIGN_DOWN_BY(Stack - 64, 8);
    if (Stack - KERNEL_STACK_SIZE < Thread->StackLimit)
    {
        if (!Thread->LargeStack) return STATUS_STACK_OVERFLOW;
        Status = MmGrowKernelStack((PVOID)Stack);
        if (!NT_SUCCESS(Status)) return Status;
    }
    State.PreviousCallback = Thread->CallbackStack;
    State.InitialStack = Thread->InitialStack;
    State.TrapFrame = Thread->TrapFrame;
    State.PreviousMode = Thread->PreviousMode;
    State.ReturnStack = ReturnStack;
    State.OutputBuffer = OutputBuffer;
    State.OutputLength = OutputLength;
    State.User.Exception = *((PKEXCEPTION_FRAME)State.TrapFrame - 1);
    State.User.Trap = *State.TrapFrame;
    RtlZeroMemory(&State.User.Vfp, sizeof(State.User.Vfp));
    if (State.TrapFrame->VfpState) State.User.Vfp = *State.TrapFrame->VfpState;
    State.User.Vfp.Link = NULL;
    State.User.Trap.VfpState = &State.User.Vfp;
    State.User.Trap.TrapFrame = (ULONG_PTR)State.TrapFrame;
    State.User.Trap.Pc = (ULONG_PTR)KeUserCallbackDispatcher | 1;
    State.User.Trap.Cpsr = (State.User.Trap.Cpsr & ~(CPSRM_MASK | CPSR_IT_MASK)) |
                          CPSRM_USER | CPSRC_THUMB;
    State.User.Exception.TrapFrame = (ULONG_PTR)&State.User.Trap;
    __asm__ __volatile__("mrs %0, cpsr\n\tcpsid if" : "=r"(State.Cpsr) :: "memory");
    Thread->CallbackStack = &State;
    Thread->InitialStack = (PVOID)Stack;
    Thread->TrapFrame = &State.User.Trap;
    KeGetCurrentPrcb()->SpBase = (PVOID)Stack;
    __asm__ __volatile__("dsb sy" ::: "memory");
    KiExceptionExit(&State.User.Trap, &State.User.Exception);
}

NTSTATUS
NTAPI
KeUserModeCallback(ULONG RoutineIndex, PVOID Argument, ULONG ArgumentLength,
                   PVOID *Result, PULONG ResultLength)
{
    PKTHREAD Thread = KeGetCurrentThread();
    PKTRAP_FRAME Trap = Thread->TrapFrame;
    PUCALLOUT_FRAME Frame;
    ULONG_PTR OldStack, Stack, Page;
    NTSTATUS Status;

    ASSERT(KeGetPreviousMode() == UserMode);
    ASSERT(!Thread->ApcState.KernelApcInProgress);
    if (Thread->CallbackNestingLevel >= KI_MAX_USER_CALLBACK_NESTING)
        return STATUS_STACK_OVERFLOW;
    OldStack = Trap->Sp;
    if (OldStack < sizeof(*Frame) || ArgumentLength > OldStack - sizeof(*Frame))
        return STATUS_ACCESS_VIOLATION;
    Stack = ALIGN_DOWN_BY(OldStack - sizeof(*Frame) - ArgumentLength, 8);
    Frame = (PVOID)Stack;
    _SEH2_TRY
    {
        for (Page = PAGE_ROUND_DOWN(OldStack); Page > Stack;)
        {
            Page -= PAGE_SIZE;
            ProbeForWrite((PVOID)Page, sizeof(UCHAR), sizeof(UCHAR));
        }
        ProbeForWrite(Frame, OldStack - Stack, sizeof(ULONGLONG));
        Frame->Buffer = Frame + 1;
        Frame->Length = ArgumentLength;
        Frame->ApiNumber = RoutineIndex;
        Frame->OriginalLr = Trap->Lr;
        Frame->MachineFrame.Sp = OldStack;
        Frame->MachineFrame.Pc = Trap->Pc;
        RtlCopyMemory(Frame->Buffer, Argument, ArgumentLength);
    }
    _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
    {
        _SEH2_YIELD(return _SEH2_GetExceptionCode());
    }
    _SEH2_END;
    Trap->Sp = Stack;
    ++Thread->CallbackNestingLevel;
    Status = KiCallUserMode(Result, ResultLength);
    --Thread->CallbackNestingLevel;
    if (Status != STATUS_CALLBACK_POP_STACK) Trap->Sp = OldStack;
    return Status;
}

NTSTATUS
NTAPI
NtCallbackReturn(PVOID Result, ULONG ResultLength, NTSTATUS CallbackStatus)
{
    PKTHREAD Thread = KeGetCurrentThread();
    PKI_ARM_CALLBACK_STATE State = Thread->CallbackStack;
    PKTRAP_FRAME Trap;
    PKARM_VFP_STATE Vfp, VfpLink;
    ULONG Link;

    if (!State) return STATUS_NO_CALLBACK_ACTIVE;
    *State->OutputBuffer = Result;
    *State->OutputLength = ResultLength;
    __asm__ __volatile__("cpsid if" ::: "memory");
    if (CallbackStatus == STATUS_CALLBACK_POP_STACK)
    {
        Trap = State->TrapFrame;
        Vfp = Trap->VfpState;
        Link = Trap->TrapFrame;
        *((PKEXCEPTION_FRAME)Trap - 1) = *((PKEXCEPTION_FRAME)Thread->TrapFrame - 1);
        ((PKEXCEPTION_FRAME)Trap - 1)->TrapFrame = (ULONG_PTR)Trap;
        if (Vfp && Thread->TrapFrame->VfpState)
        {
            VfpLink = Vfp->Link;
            *Vfp = *Thread->TrapFrame->VfpState;
            Vfp->Link = VfpLink;
        }
        *Trap = *Thread->TrapFrame;
        Trap->VfpState = Vfp;
        Trap->TrapFrame = Link;
    }
    Thread->InitialStack = State->InitialStack;
    Thread->TrapFrame = State->TrapFrame;
    Thread->PreviousMode = State->PreviousMode;
    Thread->CallbackStack = State->PreviousCallback;
    KeGetCurrentPrcb()->SpBase = State->InitialStack;
    __asm__ __volatile__("dsb sy" ::: "memory");
    KiArmCallbackReturn(State->ReturnStack, CallbackStatus, State->Cpsr);
}
