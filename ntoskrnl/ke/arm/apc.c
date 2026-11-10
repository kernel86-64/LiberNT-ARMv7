/*
 * PROJECT:     LiberNT Kernel
 * LICENSE:     BSD-2-Clause (see COPYING.ARM)
 * PURPOSE:     ARM32 user APC context preparation
 * COPYRIGHT:   Copyright 2008 ReactOS Portable Systems Group
 *              Copyright 2026 LiberNT contributors
 */

#include <ntoskrnl/include/ntoskrnl.h>

VOID
KiArmDeliverUserApc(PKTRAP_FRAME TrapFrame, PKEXCEPTION_FRAME ExceptionFrame)
{
    KIRQL OldIrql;

    if (KiGetPreviousMode(TrapFrame) != UserMode ||
        !KeGetCurrentThread()->ApcState.UserApcPending)
        return;
    OldIrql = KfRaiseIrql(APC_LEVEL);
    _enable();
    KiDeliverApc(UserMode, ExceptionFrame, TrapFrame);
    _disable();
    KfLowerIrql(OldIrql);
}

VOID
NTAPI
KiInitializeUserApc(IN PKEXCEPTION_FRAME ExceptionFrame,
                    IN PKTRAP_FRAME TrapFrame,
                    IN PKNORMAL_ROUTINE NormalRoutine,
                    IN PVOID NormalContext,
                    IN PVOID SystemArgument1,
                    IN PVOID SystemArgument2)
{
    CONTEXT Context = { 0 };
    ULONG_PTR Stack;
    ULONG ContextLength;
    Context.ContextFlags = CONTEXT_FULL;
    KeTrapFrameToContext(TrapFrame, ExceptionFrame, &Context);
    ContextLength = sizeof(CONTEXT);
    Stack = (ULONG_PTR)(Context.Sp & ~7) - ContextLength;
    ProbeForWrite((PVOID)Stack, ContextLength, sizeof(QUAD));
    RtlMoveMemory((PVOID)Stack, &Context, sizeof(CONTEXT));
    TrapFrame->R0 = (ULONG)NormalContext;
    TrapFrame->R1 = (ULONG)SystemArgument1;
    TrapFrame->R2 = (ULONG)SystemArgument2;
    TrapFrame->R3 = (ULONG)NormalRoutine;
    TrapFrame->Sp = Stack;
    TrapFrame->Pc = (ULONG)KeUserApcDispatcher | 1;
    TrapFrame->Cpsr = (TrapFrame->Cpsr & ~(CPSRM_MASK | CPSR_IT_MASK)) | CPSRM_USER | CPSRC_THUMB;
}
