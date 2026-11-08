/*
 * PROJECT:     LiberNT Kernel
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARM32 initial switch context construction
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ntoskrnl/include/ntoskrnl.h>

extern VOID NTAPI KiThreadStartup(VOID);

VOID
KiArmInitializeSwitchFrame(PVOID Frame, PKSYSTEM_ROUTINE SystemRoutine,
                           PKSTART_ROUTINE StartRoutine, PVOID StartContext,
                           PCONTEXT Context)
{
    PKSWITCH_FRAME Switch = Frame;
    PKEXCEPTION_FRAME Exception = (PVOID)(Switch + 1);
    PKARM_VFP_STATE Vfp = (PVOID)(Exception + 1);

    RtlZeroMemory(Frame, sizeof(*Switch) + sizeof(*Exception) + sizeof(*Vfp));
    Switch->ApcBypass = APC_LEVEL;
    Switch->Return = (ULONG_PTR)KiThreadStartup;
    Exception->R4 = (ULONG_PTR)Context;
    Exception->R5 = (ULONG_PTR)StartContext;
    Exception->R6 = (ULONG_PTR)StartRoutine;
    Exception->R7 = (ULONG_PTR)SystemRoutine;
    if (Context)
    {
        Vfp->Fpscr = Context->Fpscr;
        RtlCopyMemory(Vfp->VfpD, Context->D, sizeof(Vfp->VfpD));
    }
}
