/*
 * PROJECT:     LiberNT Kernel
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARMv7 application processor startup
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ntoskrnl.h>
#include <debug.h>

typedef struct _ARM_AP_BOOT_DATA
{
    ULONG Ttbr0;
    ULONG Ttbcr;
    ULONG Dacr;
    ULONG Prrr;
    ULONG Nmrr;
    ULONG Sctlr;
    ULONG Entry;
    ULONG LoaderBlock;
    ULONG Vbar;
    ULONG Progress;
    ULONG Actlr;
} ARM_AP_BOOT_DATA, *PARM_AP_BOOT_DATA;

C_ASSERT(FIELD_OFFSET(ARM_AP_BOOT_DATA, Actlr) == 0x28);

extern UCHAR KiArmApBootStart[];
extern UCHAR KiArmApBootEnd[];
extern UCHAR KiArmApBootThumb[];

static PVOID KiArmApCode;
static PARM_AP_BOOT_DATA KiArmApData;
static PHYSICAL_ADDRESS KiArmApCodePhysical;
static PHYSICAL_ADDRESS KiArmApDataPhysical;

static BOOLEAN
KiArmPrepareApBoot(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock)
{
    PHYSICAL_ADDRESS Low = {{0}};
    PHYSICAL_ADDRESS High = {{MAXULONG}};
    PHYSICAL_ADDRESS Boundary = {{0}};
    ULONG Size;
    ULONG_PTR BootStart;
    ULONG_PTR BootThumb;
    ULONG_PTR BootEnd;

    if (KiArmApCode)
        return TRUE;

    KiArmApCode = MmAllocateContiguousMemorySpecifyCache(2 * PAGE_SIZE, Low, High,
                                                        Boundary, MmNonCached);
    if (!KiArmApCode)
        return FALSE;

    KiArmApData = (PARM_AP_BOOT_DATA)((PUCHAR)KiArmApCode + PAGE_SIZE);
    KiArmApCodePhysical = MmGetPhysicalAddress(KiArmApCode);
    KiArmApDataPhysical = MmGetPhysicalAddress(KiArmApData);
    if (KiArmApCodePhysical.QuadPart > MAXULONG ||
        KiArmApDataPhysical.QuadPart > MAXULONG)
        return FALSE;

    BootStart = (ULONG_PTR)KiArmApBootStart & ~(ULONG_PTR)1;
    BootThumb = (ULONG_PTR)KiArmApBootThumb & ~(ULONG_PTR)1;
    BootEnd = (ULONG_PTR)KiArmApBootEnd & ~(ULONG_PTR)1;
    Size = (ULONG)(BootEnd - BootStart);
    if (Size == 0 || Size > PAGE_SIZE)
        return FALSE;

    RtlZeroMemory(KiArmApData, PAGE_SIZE);
    RtlCopyMemory(KiArmApCode, (PVOID)BootStart, Size);
    ((PULONG)KiArmApCode)[2] = (ULONG)KiArmApCodePhysical.QuadPart +
                               (ULONG)(BootThumb - BootStart) + 1;
    KiArmApData->Ttbr0 = _MoveFromCoprocessor(CP15_TTBR0);
    KiArmApData->Ttbcr = _MoveFromCoprocessor(CP15_TTBCR);
    KiArmApData->Dacr = _MoveFromCoprocessor(CP15_DACR);
    KiArmApData->Prrr = _MoveFromCoprocessor(CP15_PRRR);
    KiArmApData->Nmrr = _MoveFromCoprocessor(CP15_NMRR);
    KiArmApData->Sctlr = _MoveFromCoprocessor(CP15_SCTLR);
    KiArmApData->Actlr = _MoveFromCoprocessor(CP15_ACTLR);
    KiArmApData->Entry = (ULONG)(ULONG_PTR)KiSystemStartup;
    KiArmApData->LoaderBlock = (ULONG)(ULONG_PTR)LoaderBlock;
    KiArmApData->Vbar = _MoveFromCoprocessor(CP15_VBAR);
    KeMemoryBarrier();
    HalSweepDcache();
    HalSweepIcache();
    return TRUE;
}

CODE_SEG("INIT")
VOID
NTAPI
KeStartAllProcessors(
    VOID)
{
    ULONG ProcessorCount;
    ULONG MaximumProcessors;
    PVOID Raw;
    PKIPCR Pcr;
    PKTHREAD Thread;
    PVOID KernelStack;
    PVOID InterruptStack;
    PVOID PanicStack;
    KPROCESSOR_STATE State;
    LARGE_INTEGER Start, Now, Frequency;

    MaximumProcessors = KeMaximumProcessors;
    if (KeNumprocSpecified)
        MaximumProcessors = min(MaximumProcessors, KeNumprocSpecified);
    if (KeBootprocSpecified)
        MaximumProcessors = min(MaximumProcessors, KeBootprocSpecified);
    if (MaximumProcessors > 2)
        MaximumProcessors = 2;

    if (!KiArmPrepareApBoot(KeLoaderBlock))
    {
        DPRINT1("ARM SMP: AP trampoline allocation failed\n");
        return;
    }

    for (ProcessorCount = 1; ProcessorCount < MaximumProcessors; ProcessorCount++)
    {
        Raw = ExAllocatePoolZero(NonPagedPool, sizeof(KIPCR) + sizeof(ETHREAD) + PAGE_SIZE, TAG_KERNEL);
        if (!Raw)
            break;
        Pcr = (PKIPCR)ALIGN_UP_BY((ULONG_PTR)Raw, PAGE_SIZE);
        Thread = (PKTHREAD)ALIGN_UP_BY((ULONG_PTR)Pcr + sizeof(KIPCR), 16);
        KernelStack = MmCreateKernelStack(FALSE, 0);
        InterruptStack = MmCreateKernelStack(FALSE, 0);
        PanicStack = ExAllocatePoolZero(NonPagedPool, 3 * KERNEL_STACK_SIZE, TAG_KERNEL);
        if (!KernelStack || !InterruptStack || !PanicStack)
            break;

        KiInitializePcr(ProcessorCount, Pcr, Thread,
                        (PVOID)((ULONG_PTR)PanicStack + 3 * KERNEL_STACK_SIZE),
                        InterruptStack);
        RtlZeroMemory(&Pcr->Prcb.ProcessorState, sizeof(Pcr->Prcb.ProcessorState));
        RtlZeroMemory(&State, sizeof(State));
        State.ContextFrame.Pc = (ULONG)KiArmApCodePhysical.QuadPart;
        State.ContextFrame.R0 = (ULONG)KiArmApDataPhysical.QuadPart;

        KeLoaderBlock->KernelStack = (ULONG_PTR)KernelStack;
        KeLoaderBlock->Prcb = (ULONG_PTR)&Pcr->Prcb;
        KeLoaderBlock->Thread = (ULONG_PTR)Thread;
        KeLoaderBlock->u.Arm.PanicStack = (ULONG_PTR)PanicStack + 3 * KERNEL_STACK_SIZE;
        KeLoaderBlock->u.Arm.InterruptStack = (ULONG_PTR)InterruptStack;
        KeLoaderBlock->u.Arm.PcrPage = (ULONG_PTR)Pcr >> PAGE_SHIFT;

        KiArmApData->LoaderBlock = (ULONG)(ULONG_PTR)KeLoaderBlock;
        if (!HalStartNextProcessor(KeLoaderBlock, &State))
        {
            KeLoaderBlock->Prcb = 0;
            break;
        }

        Start = KeQueryPerformanceCounter(&Frequency);
        while (KeLoaderBlock->Prcb != 0)
        {
            Now = KeQueryPerformanceCounter(NULL);
            if (Now.QuadPart - Start.QuadPart > Frequency.QuadPart * 5)
            {
                KeBugCheckEx(HAL_INITIALIZATION_FAILED, ProcessorCount,
                             (ULONG_PTR)KeLoaderBlock->Prcb, 0, 0);
            }
            YieldProcessor();
            KeMemoryBarrier();
        }
        DPRINT1("ARM SMP: CPU%lu online\n", ProcessorCount);
    }
}
