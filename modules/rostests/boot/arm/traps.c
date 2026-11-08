/*
 * PROJECT:     LiberNT boot tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARM32 hardware exception entry and return regression tests
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ntoskrnl/include/ntoskrnl.h>

static volatile ULONG Seen;
static volatile ULONG TrapFailures;
ULONG ArmTrapTestExpectedSp;

extern VOID KiArmInstallVectors(VOID);
extern BOOLEAN ArmTestTrapRegisters(VOID);
extern BOOLEAN ArmTestDispatchSvc(PKTRAP_FRAME Trap);

static VOID CheckFrame(PKTRAP_FRAME Trap, ULONG Kind)
{
    PKEXCEPTION_FRAME Exception = (PVOID)((PUCHAR)Trap - sizeof(KEXCEPTION_FRAME));
    PCONTEXT Context = (PVOID)((PUCHAR)Exception - sizeof(CONTEXT));
    ULONG Index;

    Seen |= 1u << Kind;
    if (Trap->Reserved != 0xBADB0D00 || Trap->Sp != ArmTrapTestExpectedSp ||
        Trap->R11 != 75 || Trap->R12 != 0x12345678 || !(Trap->Cpsr & CPSRC_THUMB) ||
        Trap->PreviousMode != KernelMode || !Trap->VfpState ||
        Context->ContextFlags != CONTEXT_FULL || Context->Sp != Trap->Sp ||
        Context->Pc != Trap->Pc || Context->D[8] != 0x3ff0000000000000ULL ||
        Trap->VfpState->VfpD[8] != 0x3ff0000000000000ULL)
        TrapFailures |= 1u << Kind;
    for (Index = 0; Index < 8; ++Index)
        if ((&Exception->R4)[Index] != 68 + Index || (&Context->R4)[Index] != 68 + Index)
            TrapFailures |= 1u << Kind;
}

VOID KiSoftwareInterruptHandler(PKTRAP_FRAME Trap)
{
    if (ArmTestDispatchSvc(Trap))
        return;
    CheckFrame(Trap, 2);
    if (Trap->R0 != 7) TrapFailures |= 4;
    Trap->R0 = 42;
}

NTSTATUS KiUndefinedExceptionHandler(PKTRAP_FRAME Trap)
{
    CheckFrame(Trap, 1);
    if (Trap->R0 != 17) TrapFailures |= 2;
    Trap->Pc += 2;
    Trap->R0 = 18;
    return STATUS_SUCCESS;
}

NTSTATUS KiDataAbortHandler(PKTRAP_FRAME Trap)
{
    CheckFrame(Trap, 4);
    if (Trap->FaultAddress != 0xfffff000 || !Trap->FaultStatus)
        TrapFailures |= 16;
    Trap->Pc += 2;
    Trap->R0 = 99;
    return STATUS_SUCCESS;
}

NTSTATUS KiPrefetchAbortHandler(PKTRAP_FRAME Trap)
{
    UNREFERENCED_PARAMETER(Trap);
    TrapFailures = MAXULONG;
    for (;;) __asm__ __volatile__("wfi");
}

VOID KiInterruptHandler(PKTRAP_FRAME Trap, ULONG Reserved)
{
    UNREFERENCED_PARAMETER(Trap);
    UNREFERENCED_PARAMETER(Reserved);
    TrapFailures = MAXULONG;
    for (;;) __asm__ __volatile__("wfi");
}

VOID NTAPI KiUnexpectedInterrupt(VOID)
{
    TrapFailures = MAXULONG;
    for (;;) __asm__ __volatile__("wfi");
}

BOOLEAN ArmTestKernelTraps(VOID)
{
    ULONG Cpsr, Sctlr, Vbar;
    BOOLEAN Passed;

    Seen = 0;
    TrapFailures = 0;
    __asm__ __volatile__("mrs %0, cpsr\n\tcpsid if" : "=r"(Cpsr) :: "memory");
    __asm__ __volatile__("mrc p15, 0, %0, c1, c0, 0" : "=r"(Sctlr));
    __asm__ __volatile__("mrc p15, 0, %0, c12, c0, 0" : "=r"(Vbar));
    KiArmInstallVectors();
    Passed = ArmTestTrapRegisters();
    __asm__ __volatile__("dsb sy\n\tmcr p15, 0, %0, c12, c0, 0\n\tmcr p15, 0, %1, c1, c0, 0\n\tisb"
                         :: "r"(Vbar), "r"(Sctlr) : "memory");
    __asm__ __volatile__("msr cpsr_c, %0" :: "r"(Cpsr) : "memory");
    return Passed && Seen == ((1u << 1) | (1u << 2) | (1u << 4)) && TrapFailures == 0;
}
