/*
 * PROJECT:     LiberNT Kernel
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARM32 hardware exception frame construction
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ntoskrnl/include/ntoskrnl.h>

VOID
KiArmBuildExceptionFrames(PULONG Registers, PCONTEXT Context, ULONG Vector,
                          ULONG UserSp, ULONG UserLr)
{
    PKEXCEPTION_FRAME Exception = (PVOID)(Context + 1);
    PKTRAP_FRAME Trap = (PVOID)(Exception + 1);
    PKARM_VFP_STATE Vfp = (PVOID)(Trap + 1);
    ULONG Mode = Registers[15] & CPSRM_MASK;
    ULONG Pc = Registers[14];
    ULONG Thumb = (Registers[15] & CPSRC_THUMB) != 0;

    if (Vector == 1) Pc -= Thumb ? 2 : 4;
    else if (Vector == 3 || Vector == 6 || Vector == 7) Pc -= 4;
    else if (Vector == 4) Pc -= 8;

    RtlZeroMemory(Context, sizeof(*Context));
    RtlZeroMemory(Exception, sizeof(*Exception));
    RtlZeroMemory(Trap, sizeof(*Trap));
    Vfp->Link = NULL;
    Vfp->Reserved = 0;
    Vfp->Reserved2 = 0;
    Context->ContextFlags = CONTEXT_FULL;
    RtlCopyMemory(&Context->R0, Registers, 13 * sizeof(ULONG));
    Context->Sp = Mode == CPSRM_SVC ? (ULONG_PTR)(Registers + 16) : UserSp;
    Context->Lr = Mode == CPSRM_SVC ? Registers[13] : UserLr;
    Context->Pc = Pc | Thumb;
    Context->Cpsr = Registers[15];
    Context->Fpscr = Vfp->Fpscr;
    RtlCopyMemory(Context->D, Vfp->VfpD, sizeof(Context->D));
    RtlCopyMemory(&Exception->R4, Registers + 4, 8 * sizeof(ULONG));
    Exception->TrapFrame = (ULONG_PTR)Trap;
    Exception->Return = Context->Pc;
    RtlCopyMemory(&Trap->R0, Registers, 4 * sizeof(ULONG));
    Trap->R12 = Registers[12];
    Trap->R11 = Registers[11];
    Trap->Sp = Context->Sp;
    Trap->Lr = Context->Lr;
    Trap->Pc = Context->Pc;
    Trap->Cpsr = Context->Cpsr;
    Trap->VfpState = Vfp;
    Trap->ExceptionActive = Vector != 2 && Vector != 6 && Vector != 7;
    Trap->PreviousMode = Mode == CPSRM_USER ? UserMode : KernelMode;
    Trap->Reserved = 0xBADB0D00;
    if (Vector == 3)
    {
        Trap->FaultStatus = _MoveFromCoprocessor(CP15_IFSR);
        Trap->FaultAddress = _MoveFromCoprocessor(CP15_IFAR);
    }
    else if (Vector == 4)
    {
        Trap->FaultStatus = _MoveFromCoprocessor(CP15_DFSR);
        Trap->FaultAddress = _MoveFromCoprocessor(CP15_DFAR);
    }
}
