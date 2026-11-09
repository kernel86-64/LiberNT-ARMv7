/*
 * PROJECT:     LiberNT Native Library
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARM32 user callback table dispatch
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ntdll.h>

DECLSPEC_NORETURN VOID
KiArmUserCallbackDispatcher(ULONG Index, PVOID Argument, ULONG Length)
{
    typedef NTSTATUS (NTAPI *PCALLBACK_ROUTINE)(PVOID, ULONG);
    PCALLBACK_ROUTINE *Table = NtCurrentPeb()->KernelCallbackTable;
    NTSTATUS Status;

    Status = Table[Index](Argument, Length);
    Status = NtCallbackReturn(NULL, 0, Status);
    RtlRaiseStatus(Status);
}

DECLSPEC_NORETURN VOID
KiArmUserExceptionDispatcher(PEXCEPTION_RECORD Record, PCONTEXT Context)
{
    NTSTATUS Status;

    if (RtlDispatchException(Record, Context))
        Status = NtContinue(Context, FALSE);
    else
        Status = NtRaiseException(Record, Context, FALSE);
    RtlRaiseStatus(Status);
}
