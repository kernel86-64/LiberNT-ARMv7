/*
 * PROJECT:     LiberNT Kernel
 * LICENSE:     BSD-2-Clause (see COPYING.ARM)
 * PURPOSE:     ARM32 context conversion and user processor-state sanitization
 * COPYRIGHT:   Copyright 2008 ReactOS Portable Systems Group
 *              Copyright 2026 LiberNT contributors
 */

#include <ntoskrnl/include/ntoskrnl.h>

VOID
NTAPI
KeContextToTrapFrame(IN PCONTEXT Context,
                     IN OUT PKEXCEPTION_FRAME ExceptionFrame,
                     IN OUT PKTRAP_FRAME TrapFrame,
                     IN ULONG ContextFlags,
                     IN KPROCESSOR_MODE PreviousMode)
{
    KIRQL OldIrql;
    OldIrql = KeGetCurrentIrql();
    if (OldIrql < APC_LEVEL) KeRaiseIrql(APC_LEVEL, &OldIrql);
    if ((ContextFlags & CONTEXT_CONTROL) == CONTEXT_CONTROL)
    {
        TrapFrame->Sp = Context->Sp;
        TrapFrame->Lr = Context->Lr;
        TrapFrame->Pc = Context->Pc;
        TrapFrame->Cpsr = PreviousMode == UserMode ?
                          (Context->Cpsr & (CPSRF_N | CPSRF_Z | CPSRF_C | CPSRF_V |
                                            CPSRF_Q | CPSR_IT_MASK | 0x000F0000)) |
                          CPSRM_USER | CPSRC_THUMB : Context->Cpsr;
    }
    if ((ContextFlags & CONTEXT_INTEGER) == CONTEXT_INTEGER)
    {
        TrapFrame->R0 = Context->R0;
        TrapFrame->R1 = Context->R1;
        TrapFrame->R2 = Context->R2;
        TrapFrame->R3 = Context->R3;
        ExceptionFrame->R4 = Context->R4;
        ExceptionFrame->R5 = Context->R5;
        ExceptionFrame->R6 = Context->R6;
        ExceptionFrame->R7 = Context->R7;
        ExceptionFrame->R8 = Context->R8;
        ExceptionFrame->R9 = Context->R9;
        ExceptionFrame->R10 = Context->R10;
        ExceptionFrame->R11 = Context->R11;
        TrapFrame->R12 = Context->R12;
    }

    if ((ContextFlags & CONTEXT_FLOATING_POINT) == CONTEXT_FLOATING_POINT && TrapFrame->VfpState)
    {
        TrapFrame->VfpState->Fpscr = Context->Fpscr;
        RtlCopyMemory(TrapFrame->VfpState->VfpD, Context->D, sizeof(Context->D));
    }
    if (OldIrql < APC_LEVEL) KeLowerIrql(OldIrql);
}

VOID
NTAPI
KeTrapFrameToContext(IN PKTRAP_FRAME TrapFrame,
                     IN PKEXCEPTION_FRAME ExceptionFrame,
                     IN OUT PCONTEXT Context)
{
    KIRQL OldIrql;
    OldIrql = KeGetCurrentIrql();
    if (OldIrql < APC_LEVEL) KeRaiseIrql(APC_LEVEL, &OldIrql);
    if ((Context->ContextFlags & CONTEXT_CONTROL) == CONTEXT_CONTROL)
    {
        Context->Sp = TrapFrame->Sp;
        Context->Lr = TrapFrame->Lr;
        Context->Pc = TrapFrame->Pc;
        Context->Cpsr = TrapFrame->Cpsr;
    }
    if ((Context->ContextFlags & CONTEXT_INTEGER) == CONTEXT_INTEGER)
    {
        Context->R0 = TrapFrame->R0;
        Context->R1 = TrapFrame->R1;
        Context->R2 = TrapFrame->R2;
        Context->R3 = TrapFrame->R3;
        Context->R4 = ExceptionFrame->R4;
        Context->R5 = ExceptionFrame->R5;
        Context->R6 = ExceptionFrame->R6;
        Context->R7 = ExceptionFrame->R7;
        Context->R8 = ExceptionFrame->R8;
        Context->R9 = ExceptionFrame->R9;
        Context->R10 = ExceptionFrame->R10;
        Context->R11 = ExceptionFrame->R11;
        Context->R12 = TrapFrame->R12;
    }

    if ((Context->ContextFlags & CONTEXT_FLOATING_POINT) == CONTEXT_FLOATING_POINT && TrapFrame->VfpState)
    {
        Context->Fpscr = TrapFrame->VfpState->Fpscr;
        RtlCopyMemory(Context->D, TrapFrame->VfpState->VfpD, sizeof(Context->D));
    }
    if (OldIrql < APC_LEVEL) KeLowerIrql(OldIrql);
}
