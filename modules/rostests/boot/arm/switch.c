/*
 * PROJECT:     LiberNT boot tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARM32 thread stack and VFP switch regression tests
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ntoskrnl/include/ntoskrnl.h>

static DECLSPEC_ALIGN(PAGE_SIZE) KIPCR SwitchPcr;
static KPROCESS SwitchOldProcess, SwitchNewProcess;
static KTHREAD SwitchWorker;
static DECLSPEC_ALIGN(8) UCHAR SwitchStack[16384];
static DECLSPEC_ALIGN(8) UCHAR SwitchLargeStack[16384];
static volatile ULONG SwitchSteps, SwitchResumes, SwitchFailures;
KTHREAD ArmSwitchMainThread;

VOID NTAPI KiRetireDpcList(PKPRCB Prcb)
{
    ULONG_PTR Sp;

    __asm__ __volatile__("mov %0, sp" : "=r"(Sp));
    if (Prcb != &SwitchPcr.Prcb || Sp < (ULONG_PTR)SwitchLargeStack ||
        Sp >= (ULONG_PTR)(SwitchLargeStack + sizeof(SwitchLargeStack)) || (Sp & 7))
        ++SwitchFailures;
    ++SwitchSteps;
}

static BOOLEAN SwitchKernelStack(VOID)
{
    struct
    {
        KEXCEPTION_FRAME Exception;
        KTRAP_FRAME Trap;
        KARM_VFP_STATE Vfp;
    } Frame = {0};
    KTRAP_FRAME Linked = {0};
    KARM_VFP_STATE LinkedVfp = {0};
    PVOID OldBase = SwitchWorker.StackBase;
    ULONG_PTR OldLimit = SwitchWorker.StackLimit;
    PVOID NewBase = SwitchLargeStack + sizeof(SwitchLargeStack);
    ULONG Before, After, Sp;
    volatile ULONG Marker = 0x12345678;
    BOOLEAN Passed;

    Frame.Exception.TrapFrame = (ULONG_PTR)&Frame.Trap;
    Frame.Trap.Cpsr = CPSRM_USER | CPSRC_THUMB;
    Frame.Trap.Sp = 0x12340000;
    Frame.Trap.TrapFrame = (ULONG_PTR)&Linked;
    Frame.Trap.VfpState = &Frame.Vfp;
    Frame.Vfp.Link = &LinkedVfp;
    Frame.Vfp.VfpD[31] = 0x1122334455667788ULL;
    SwitchWorker.TrapFrame = &Frame.Trap;
    __asm__ __volatile__("mrs %0, cpsr" : "=r"(Before));
    Passed = KeSwitchKernelStack(NewBase, SwitchLargeStack) == OldBase;
    __asm__ __volatile__("mrs %0, cpsr\n\tmov %1, sp" : "=r"(After), "=r"(Sp));
    Passed = Passed && Marker == 0x12345678 && Sp >= (ULONG_PTR)SwitchLargeStack &&
             Sp < (ULONG_PTR)NewBase && !((Before ^ After) & 0xC0) &&
             SwitchWorker.StackBase == NewBase && SwitchWorker.LargeStack &&
             SwitchWorker.TrapFrame == &Frame.Trap && Frame.Trap.VfpState == &Frame.Vfp &&
             Frame.Vfp.Link == &LinkedVfp && Frame.Trap.TrapFrame == (ULONG_PTR)&Linked &&
             Frame.Exception.TrapFrame == (ULONG_PTR)&Frame.Trap &&
             Frame.Trap.Sp == 0x12340000 && Frame.Vfp.VfpD[31] == 0x1122334455667788ULL;
    Passed = KeSwitchKernelStack(OldBase, (PVOID)OldLimit) == NewBase && Passed;
    Passed = Passed && Marker == 0x12345678 && SwitchWorker.TrapFrame == &Frame.Trap &&
             Frame.Trap.VfpState == &Frame.Vfp && Frame.Vfp.Link == &LinkedVfp &&
             Frame.Exception.TrapFrame == (ULONG_PTR)&Frame.Trap;
    SwitchWorker.LargeStack = FALSE;
    SwitchWorker.TrapFrame = NULL;
    KiRetireDpcListInDpcStack(&SwitchPcr.Prcb, NewBase);
    __asm__ __volatile__("mov %0, sp" : "=r"(Sp));
    Passed = Passed && Sp >= OldLimit && Sp < (ULONG_PTR)OldBase && Marker == 0x12345678;
    return Passed;
}

extern BOOLEAN ArmTestSwitchRegisters(VOID);
VOID KiArmSetThreadTeb(PKTHREAD Thread);
extern VOID KiArmInitializeSwitchFrame(PVOID, PKSYSTEM_ROUTINE,
                                       PKSTART_ROUTINE, PVOID, PCONTEXT);

BOOLEAN KiArmSwapContextResume(KIRQL WaitIrql, PKTHREAD Old, PKTHREAD New)
{
    ++SwitchResumes;
    KiArmSetThreadTeb(New);
    if ((PVOID)NtCurrentTeb() != New->Teb) ++SwitchFailures;
    if (New != SwitchPcr.Prcb.CurrentThread ||
        (Old != &SwitchWorker && Old != &ArmSwitchMainThread) ||
        (New != &SwitchWorker && New != &ArmSwitchMainThread) || Old == New)
        ++SwitchFailures;
    if (SwitchResumes == 1 ? WaitIrql != APC_LEVEL : WaitIrql != PASSIVE_LEVEL)
        ++SwitchFailures;
    __asm__ __volatile__("vmov.f64 d0, #16.0\n\tvmov.f64 d16, #16.0" ::: "d0", "d16");
    return WaitIrql == PASSIVE_LEVEL;
}

VOID ArmSwitchLowerIrql(KIRQL Irql)
{
    if (Irql != APC_LEVEL || SwitchPcr.Prcb.CurrentThread != &SwitchWorker)
        ++SwitchFailures;
    SwitchPcr.CurrentIrql = Irql;
}

DECLSPEC_NORETURN VOID NTAPI KeBugCheck(ULONG Code)
{
    UNREFERENCED_PARAMETER(Code);
    SwitchFailures = MAXULONG;
    for (;;) __asm__ __volatile__("wfi");
}

static DECLSPEC_NORETURN VOID NTAPI SwitchWorkerStart(PVOID Parameter)
{
    ULONG Low, High;

    if (Parameter != &SwitchWorker) ++SwitchFailures;
    if (!SwitchKernelStack()) ++SwitchFailures;
    __asm__ __volatile__("vmov %0, %1, d8" : "=r"(Low), "=r"(High));
    if (Low || High) ++SwitchFailures;
    __asm__ __volatile__("vmov.f64 d8, #16.0" ::: "d8");
    for (;;)
    {
        ++SwitchSteps;
        SwitchPcr.Prcb.CurrentThread = &ArmSwitchMainThread;
        if (!KiSwapContext(PASSIVE_LEVEL, &SwitchWorker)) ++SwitchFailures;
        __asm__ __volatile__("vmov %0, %1, d8" : "=r"(Low), "=r"(High));
        if (Low || High != 0x40300000) ++SwitchFailures;
    }
}

static VOID NTAPI SwitchWorkerSystem(PKSTART_ROUTINE Entry, PVOID Parameter)
{
    Entry(Parameter);
    KeBugCheck(NO_USER_MODE_CONTEXT);
}

BOOLEAN ArmTestKernelSwitch(VOID)
{
    ULONG Pcr, Cpsr, Teb;
    BOOLEAN Passed = TRUE;
    ULONG Index;
    SIZE_T FrameSize = sizeof(KSWITCH_FRAME) + sizeof(KEXCEPTION_FRAME) + sizeof(KARM_VFP_STATE);

    RtlZeroMemory(&SwitchPcr, sizeof(SwitchPcr));
    RtlZeroMemory(&SwitchWorker, sizeof(SwitchWorker));
    RtlZeroMemory(&ArmSwitchMainThread, sizeof(ArmSwitchMainThread));
    SwitchSteps = SwitchResumes = SwitchFailures = 0;
    SwitchWorker.Teb = (PVOID)SwitchStack;
    ArmSwitchMainThread.Teb = (PVOID)&SwitchOldProcess;
    SwitchPcr.CurrentPrcb = &SwitchPcr.Prcb;
    SwitchWorker.InitialStack = SwitchStack + sizeof(SwitchStack);
    SwitchWorker.StackBase = SwitchWorker.InitialStack;
    SwitchWorker.StackLimit = (ULONG_PTR)SwitchStack;
    SwitchWorker.KernelStack = (PUCHAR)SwitchWorker.InitialStack - FrameSize;
    KiArmInitializeSwitchFrame(SwitchWorker.KernelStack, SwitchWorkerSystem,
                               SwitchWorkerStart, &SwitchWorker, NULL);
    __asm__ __volatile__("mrs %0, cpsr\n\tcpsid if" : "=r"(Cpsr) :: "memory");
    __asm__ __volatile__("mrc p15, 0, %0, c13, c0, 4" : "=r"(Pcr));
    __asm__ __volatile__("mrc p15, 0, %0, c13, c0, 2" : "=r"(Teb));
    __asm__ __volatile__("mcr p15, 0, %0, c13, c0, 4\n\tisb" :: "r"(&SwitchPcr) : "memory");
    SwitchPcr.Prcb.GroupSetMember = 1;
    SwitchOldProcess.DirectoryTableBase = SwitchNewProcess.DirectoryTableBase = PAGE_SIZE;
    SwitchOldProcess.ActiveProcessors = 1;
    SwitchNewProcess.ActiveProcessors = 0;
    KiSwapProcess(&SwitchNewProcess, &SwitchOldProcess);
    if (SwitchOldProcess.ActiveProcessors != 0 || SwitchNewProcess.ActiveProcessors != 1)
        Passed = FALSE;
    KiSwapProcess(&SwitchNewProcess, &SwitchNewProcess);
    if (SwitchNewProcess.ActiveProcessors != 1)
        Passed = FALSE;
    for (Index = 0; Index < 2; ++Index)
    {
        SwitchPcr.Prcb.CurrentThread = &SwitchWorker;
        Passed = ArmTestSwitchRegisters() && Passed;
        if ((PVOID)NtCurrentTeb() != ArmSwitchMainThread.Teb) Passed = FALSE;
    }
    __asm__ __volatile__("cpsid if\n\tmcr p15, 0, %0, c13, c0, 4\n\tisb" :: "r"(Pcr) : "memory");
    __asm__ __volatile__("mcr p15, 0, %0, c13, c0, 2\n\tisb" :: "r"(Teb) : "memory");
    __asm__ __volatile__("msr cpsr_c, %0" :: "r"(Cpsr) : "memory");
    return Passed && SwitchSteps == 3 && SwitchResumes == 4 && SwitchFailures == 0;
}
