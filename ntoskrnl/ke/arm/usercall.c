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
    DPRINT1("User APC: %p %p %p\n", NormalContext, SystemArgument1, SystemArgument2);

    //
    // Build the user mode context
    //
    Context.ContextFlags = CONTEXT_FULL;
    KeTrapFrameToContext(TrapFrame, ExceptionFrame, &Context);

    //
    // Setup the context on the user stack
    //
    ContextLength = sizeof(CONTEXT);
    Stack = (ULONG_PTR)(Context.Sp & ~7) - ContextLength;

    //
    // Make sure the stack is valid, and copy the context
    //
    ProbeForWrite((PVOID)Stack, ContextLength, sizeof(QUAD));
    RtlMoveMemory((PVOID)Stack, &Context, sizeof(CONTEXT));

    //
    // Setup the trap frame when we return to user mode
    //
    TrapFrame->R0 = (ULONG)NormalContext;
    TrapFrame->R1 = (ULONG)SystemArgument1;
    TrapFrame->R2 = (ULONG)SystemArgument2;
    TrapFrame->R3 = (ULONG)NormalRoutine;
    TrapFrame->Sp = Stack;
    TrapFrame->Lr = (ULONG)KeUserApcDispatcher;
}

NTSTATUS
NTAPI
KeUserModeCallback(IN ULONG RoutineIndex,
                   IN PVOID Argument,
                   IN ULONG ArgumentLength,
                   OUT PVOID *Result,
                   OUT PULONG ResultLength)
{
    ASSERT(FALSE);
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
NTAPI
KiCallUserMode(
    IN PVOID *OutputBuffer,
    IN PULONG OutputLength)
{
    ASSERT(FALSE);
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
NTAPI
NtCallbackReturn(
    _In_ PVOID Result,
    _In_ ULONG ResultLength,
    _In_ NTSTATUS CallbackStatus)
{
    ASSERT(FALSE);
    return STATUS_NOT_IMPLEMENTED;
}

