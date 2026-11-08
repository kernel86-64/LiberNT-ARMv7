/*
 * PROJECT:     LiberNT API tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Verify ARM loader APC arguments and continuation context
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ntoskrnl/include/ntoskrnl.h>
#include <setjmp.h>
#include <setjmpex.h>

static jmp_buf ApcJump;
static PCONTEXT ApcExpected;
static volatile ULONG ApcSeen, ApcFailures;
extern VOID ArmTestApcThunk(VOID);
extern DECLSPEC_NORETURN VOID ArmTestInvokeApc(PCONTEXT Context, PVOID Routine);

VOID ArmTestApcInit(PCONTEXT Context, PVOID Argument1, PVOID Argument2)
{
    ApcSeen++;
    if (Context != ApcExpected || Argument1 != (PVOID)0x10000000 || Argument2 != NULL ||
        Context->ContextFlags != CONTEXT_FULL || Context->R0 != 0x100 || Context->Pc != 0x10001)
        ApcFailures++;
}

DECLSPEC_NORETURN VOID ArmTestApcContinue(PCONTEXT Context, BOOLEAN Alert)
{
    if (Context != ApcExpected || !Alert || ApcSeen != 1)
        ApcFailures++;
    longjmp(ApcJump, 42);
}

DECLSPEC_NORETURN VOID ArmTestApcRaise(NTSTATUS Status)
{
    UNREFERENCED_PARAMETER(Status);
    ApcFailures++;
    longjmp(ApcJump, 42);
}

BOOLEAN ArmTestApcDispatch(PCONTEXT Context)
{
    volatile int Result;

    ApcExpected = Context;
    ApcSeen = ApcFailures = 0;
    Result = _setjmp(ApcJump, NULL);
    if (!Result)
        ArmTestInvokeApc(Context, ArmTestApcThunk);
    return Result == 42 && ApcSeen == 1 && ApcFailures == 0;
}
