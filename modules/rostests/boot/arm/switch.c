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
static volatile ULONG SwitchSteps, SwitchResumes, SwitchFailures;
KTHREAD ArmSwitchMainThread;

extern BOOLEAN ArmTestSwitchRegisters(VOID);
extern VOID KiArmInitializeSwitchFrame(PVOID, PKSYSTEM_ROUTINE,
                                       PKSTART_ROUTINE, PVOID, PCONTEXT);

BOOLEAN KiArmSwapContextResume(KIRQL WaitIrql, PKTHREAD Old, PKTHREAD New)
{
    ++SwitchResumes;
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
    ULONG Pcr, Cpsr;
    BOOLEAN Passed = TRUE;
    ULONG Index;
    SIZE_T FrameSize = sizeof(KSWITCH_FRAME) + sizeof(KEXCEPTION_FRAME) + sizeof(KARM_VFP_STATE);

    RtlZeroMemory(&SwitchPcr, sizeof(SwitchPcr));
    RtlZeroMemory(&SwitchWorker, sizeof(SwitchWorker));
    RtlZeroMemory(&ArmSwitchMainThread, sizeof(ArmSwitchMainThread));
    SwitchSteps = SwitchResumes = SwitchFailures = 0;
    SwitchPcr.CurrentPrcb = &SwitchPcr.Prcb;
    SwitchWorker.InitialStack = SwitchStack + sizeof(SwitchStack);
    SwitchWorker.KernelStack = (PUCHAR)SwitchWorker.InitialStack - FrameSize;
    KiArmInitializeSwitchFrame(SwitchWorker.KernelStack, SwitchWorkerSystem,
                               SwitchWorkerStart, &SwitchWorker, NULL);
    __asm__ __volatile__("mrs %0, cpsr\n\tcpsid if" : "=r"(Cpsr) :: "memory");
    __asm__ __volatile__("mrc p15, 0, %0, c13, c0, 4" : "=r"(Pcr));
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
    }
    __asm__ __volatile__("cpsid if\n\tmcr p15, 0, %0, c13, c0, 4\n\tisb" :: "r"(Pcr) : "memory");
    __asm__ __volatile__("msr cpsr_c, %0" :: "r"(Cpsr) : "memory");
    return Passed && SwitchSteps == 2 && SwitchResumes == 4 && SwitchFailures == 0;
}
