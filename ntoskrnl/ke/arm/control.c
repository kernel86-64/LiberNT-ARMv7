/*
 * PROJECT:     LiberNT Kernel
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARMv7 debugger processor control state
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ntoskrnl/include/ntoskrnl.h>

VOID NTAPI KiSaveProcessorControlState(PKPROCESSOR_STATE State)
{
    PKSPECIAL_REGISTERS Special = &State->SpecialRegisters;
    PKARM_ARCH_STATE Arch = &State->ArchState;
    ULONG DebugId, Breakpoints, High;

    RtlZeroMemory(Special, sizeof(*Special));
    RtlZeroMemory(Arch, sizeof(*Arch));
    Arch->Cp15_Cr0_CpuId = _MoveFromCoprocessor(CP15_MIDR);
    Arch->Cp15_Cr1_Control = _MoveFromCoprocessor(CP15_SCTLR);
    Arch->Cp15_Cr1_AuxControl = _MoveFromCoprocessor(CP15_ACTLR);
    Arch->Cp15_Cr1_Cpacr = _MoveFromCoprocessor(CP15_CPACR);
    Arch->Cp15_Cr2_TtbControl = _MoveFromCoprocessor(CP15_TTBCR);
    Arch->Cp15_Cr3_Dacr = _MoveFromCoprocessor(CP15_DACR);
    Arch->Cp15_Cr5_Dfsr = _MoveFromCoprocessor(CP15_DFSR);
    Arch->Cp15_Cr5_Ifsr = _MoveFromCoprocessor(CP15_IFSR);
    Arch->Cp15_Cr6_Dfar = _MoveFromCoprocessor(CP15_DFAR);
    Arch->Cp15_Cr6_Ifar = _MoveFromCoprocessor(CP15_IFAR);
    Arch->Cp15_Cr10_PrimaryMemoryRemap = _MoveFromCoprocessor(CP15_PRRR);
    Arch->Cp15_Cr10_NormalMemoryRemap = _MoveFromCoprocessor(CP15_NMRR);
    Arch->Cp15_Cr12_VBARns = _MoveFromCoprocessor(CP15_VBAR);
    Arch->Cp15_Cr13_ContextId = _MoveFromCoprocessor(CP15_CONTEXTIDR);
    if (Arch->Cp15_Cr2_TtbControl & (1UL << 31))
    {
        __asm__ __volatile__("mrrc p15, 0, %0, %1, c2" : "=r" (Arch->Cp15_Cr2_Ttb0), "=r" (High));
        __asm__ __volatile__("mrrc p15, 1, %0, %1, c2" : "=r" (Arch->Cp15_Cr2_Ttb1), "=r" (High));
    }
    else
    {
        Arch->Cp15_Cr2_Ttb0 = _MoveFromCoprocessor(CP15_TTBR0);
        Arch->Cp15_Cr2_Ttb1 = _MoveFromCoprocessor(CP15_TTBR1);
    }
    /* TODO: represent LPAE TTBR high words and optional PMU state in the debugger snapshot. */
    Special->Cp15_Cr13_UsrRW = _MoveFromCoprocessor(CP15_TPIDRURW);
    Special->Cp15_Cr13_UsrRO = _MoveFromCoprocessor(CP15_TPIDRURO);
    Special->Cp15_Cr13_SvcRW = _MoveFromCoprocessor(CP15_TPIDRPRW);
    __asm__ __volatile__("vmrs %0, fpexc" : "=r" (Special->Fpexc));
    DebugId = _MoveFromCoprocessor(CP14_DBGDIDR);
    Breakpoints = ((DebugId >> 24) & 15) + 1;
    if (Breakpoints > 0)
    {
        Special->KernelBvr[0] = _MoveFromCoprocessor(CP14_DBGBVR0);
        Special->KernelBcr[0] = _MoveFromCoprocessor(CP14_DBGBCR0);
    }
    if (Breakpoints > 1)
    {
        Special->KernelBvr[1] = _MoveFromCoprocessor(CP14_DBGBVR1);
        Special->KernelBcr[1] = _MoveFromCoprocessor(CP14_DBGBCR1);
    }
    if (Breakpoints > 2)
    {
        Special->KernelBvr[2] = _MoveFromCoprocessor(CP14_DBGBVR2);
        Special->KernelBcr[2] = _MoveFromCoprocessor(CP14_DBGBCR2);
    }
    if (Breakpoints > 3)
    {
        Special->KernelBvr[3] = _MoveFromCoprocessor(CP14_DBGBVR3);
        Special->KernelBcr[3] = _MoveFromCoprocessor(CP14_DBGBCR3);
    }
    if (Breakpoints > 4)
    {
        Special->KernelBvr[4] = _MoveFromCoprocessor(CP14_DBGBVR4);
        Special->KernelBcr[4] = _MoveFromCoprocessor(CP14_DBGBCR4);
    }
    if (Breakpoints > 5)
    {
        Special->KernelBvr[5] = _MoveFromCoprocessor(CP14_DBGBVR5);
        Special->KernelBcr[5] = _MoveFromCoprocessor(CP14_DBGBCR5);
    }
    if (Breakpoints > 6)
    {
        Special->KernelBvr[6] = _MoveFromCoprocessor(CP14_DBGBVR6);
        Special->KernelBcr[6] = _MoveFromCoprocessor(CP14_DBGBCR6);
    }
    if (Breakpoints > 7)
    {
        Special->KernelBvr[7] = _MoveFromCoprocessor(CP14_DBGBVR7);
        Special->KernelBcr[7] = _MoveFromCoprocessor(CP14_DBGBCR7);
    }
    Special->KernelWvr[0] = _MoveFromCoprocessor(CP14_DBGWVR0);
    Special->KernelWcr[0] = _MoveFromCoprocessor(CP14_DBGWCR0);
    /* TODO: capture banked exception stacks and optional VFP exception instruction registers. */
}

VOID NTAPI KiRestoreProcessorControlState(PKPROCESSOR_STATE State)
{
    PKSPECIAL_REGISTERS Special = &State->SpecialRegisters;
    ULONG DebugId = _MoveFromCoprocessor(CP14_DBGDIDR);
    ULONG Breakpoints = ((DebugId >> 24) & 15) + 1;

    __asm__ __volatile__("dsb sy" ::: "memory");
    if (Breakpoints > 0)
    {
        _MoveToCoprocessor(0, CP14_DBGBCR0);
        _MoveToCoprocessor(Special->KernelBvr[0], CP14_DBGBVR0);
        _MoveToCoprocessor(Special->KernelBcr[0], CP14_DBGBCR0);
    }
    if (Breakpoints > 1)
    {
        _MoveToCoprocessor(0, CP14_DBGBCR1);
        _MoveToCoprocessor(Special->KernelBvr[1], CP14_DBGBVR1);
        _MoveToCoprocessor(Special->KernelBcr[1], CP14_DBGBCR1);
    }
    if (Breakpoints > 2)
    {
        _MoveToCoprocessor(0, CP14_DBGBCR2);
        _MoveToCoprocessor(Special->KernelBvr[2], CP14_DBGBVR2);
        _MoveToCoprocessor(Special->KernelBcr[2], CP14_DBGBCR2);
    }
    if (Breakpoints > 3)
    {
        _MoveToCoprocessor(0, CP14_DBGBCR3);
        _MoveToCoprocessor(Special->KernelBvr[3], CP14_DBGBVR3);
        _MoveToCoprocessor(Special->KernelBcr[3], CP14_DBGBCR3);
    }
    if (Breakpoints > 4)
    {
        _MoveToCoprocessor(0, CP14_DBGBCR4);
        _MoveToCoprocessor(Special->KernelBvr[4], CP14_DBGBVR4);
        _MoveToCoprocessor(Special->KernelBcr[4], CP14_DBGBCR4);
    }
    if (Breakpoints > 5)
    {
        _MoveToCoprocessor(0, CP14_DBGBCR5);
        _MoveToCoprocessor(Special->KernelBvr[5], CP14_DBGBVR5);
        _MoveToCoprocessor(Special->KernelBcr[5], CP14_DBGBCR5);
    }
    if (Breakpoints > 6)
    {
        _MoveToCoprocessor(0, CP14_DBGBCR6);
        _MoveToCoprocessor(Special->KernelBvr[6], CP14_DBGBVR6);
        _MoveToCoprocessor(Special->KernelBcr[6], CP14_DBGBCR6);
    }
    if (Breakpoints > 7)
    {
        _MoveToCoprocessor(0, CP14_DBGBCR7);
        _MoveToCoprocessor(Special->KernelBvr[7], CP14_DBGBVR7);
        _MoveToCoprocessor(Special->KernelBcr[7], CP14_DBGBCR7);
    }
    _MoveToCoprocessor(0, CP14_DBGWCR0);
    _MoveToCoprocessor(Special->KernelWvr[0], CP14_DBGWVR0);
    _MoveToCoprocessor(Special->KernelWcr[0], CP14_DBGWCR0);
    _MoveToCoprocessor(Special->Cp15_Cr13_UsrRW, CP15_TPIDRURW);
    _MoveToCoprocessor(Special->Cp15_Cr13_UsrRO, CP15_TPIDRURO);
    _MoveToCoprocessor(Special->Cp15_Cr13_SvcRW, CP15_TPIDRPRW);
    __asm__ __volatile__("vmsr fpexc, %0\n\tisb" :: "r" (Special->Fpexc) : "memory");
    /* TODO: restore architecture control state after a full ARM processor freeze. */
}
