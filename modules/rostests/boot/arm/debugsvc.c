/*
 * PROJECT:     LiberNT API tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARM32 runtime debug service argument regression tests
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ntoskrnl/include/ntoskrnl.h>

static struct { KEXCEPTION_FRAME Exception; KTRAP_FRAME Trap; } Frames;
static ULONG Failures, Calls;
static CHAR TestInput[8], TestOutput[16];

VOID KiArmDebugService(PKTRAP_FRAME Trap, ULONG Argument4);

NTSTATUS NTAPI KdpPrint(ULONG Component, ULONG Level, PCHAR String, USHORT Length,
                        KPROCESSOR_MODE Mode, PKTRAP_FRAME Trap, PKEXCEPTION_FRAME Exception,
                        PBOOLEAN Handled)
{
    if (Component != 7 || Level != 5 || String != TestInput || Length != 8 || Mode != KernelMode ||
        Trap != &Frames.Trap || Exception != &Frames.Exception)
        ++Failures;
    *Handled = TRUE;
    ++Calls;
    return STATUS_BUFFER_OVERFLOW;
}

USHORT NTAPI KdpPrompt(PCHAR Prompt, USHORT PromptLength, PCHAR Response, USHORT Maximum,
                       KPROCESSOR_MODE Mode, PKTRAP_FRAME Trap, PKEXCEPTION_FRAME Exception)
{
    if (Prompt != TestInput || PromptLength != 8 || Response != TestOutput || Maximum != 16 || Mode != KernelMode ||
        Trap != &Frames.Trap || Exception != &Frames.Exception)
        ++Failures;
    ++Calls;
    return 12;
}

BOOLEAN ArmTestDebugServices(VOID)
{
    Failures = Calls = 0;
    RtlZeroMemory(&Frames, sizeof(Frames));
    Frames.Trap.Cpsr = CPSRM_SVC | CPSRC_THUMB;
    Frames.Trap.PreviousMode = KernelMode;
    Frames.Trap.Pc = 0x1001;
    Frames.Trap.R0 = BREAKPOINT_PRINT;
    Frames.Trap.R1 = (ULONG_PTR)TestInput;
    Frames.Trap.R2 = sizeof(TestInput);
    Frames.Trap.R3 = 7;
    KiArmDebugService(&Frames.Trap, 5);
    if (Frames.Trap.R0 != (ULONG)STATUS_BUFFER_OVERFLOW || Frames.Trap.Pc != 0x1003)
        ++Failures;
    Frames.Trap.R0 = BREAKPOINT_PROMPT;
    Frames.Trap.R3 = (ULONG_PTR)TestOutput;
    KiArmDebugService(&Frames.Trap, sizeof(TestOutput));
    if (Frames.Trap.R0 != 12 || Frames.Trap.Pc != 0x1005)
        ++Failures;
    Frames.Trap.R0 = MAXULONG;
    KiArmDebugService(&Frames.Trap, 0);
    if (Frames.Trap.R0 != (ULONG)STATUS_NOT_IMPLEMENTED || Frames.Trap.Pc != 0x1007)
        ++Failures;
    return Calls == 2 && Failures == 0;
}
