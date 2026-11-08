/*
 * PROJECT:     LiberNT Kernel
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARMv7 LPAE processor and page-table access
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <nvs/nt/mint.h>

VOID MiArmWriteBootPtePhysical(PMI_PTE Slot, MI_PTE Value);

VOID NTAPI
MiInitializeKernelVaLayout(const LOADER_PARAMETER_BLOCK *LoaderBlock)
{
    ULONG Ttbcr;

    UNREFERENCED_PARAMETER(LoaderBlock);
    __asm__ __volatile__("mrc p15, 0, %0, c2, c0, 2" : "=r" (Ttbcr));
    if (!(Ttbcr & (1UL << 31)))
        KeBugCheckEx(MEMORY_MANAGEMENT, 0x4C504145, Ttbcr, 0, 0);
    MmSystemRangeStart = (PVOID)KSEG0_BASE;
}

VOID
MiArchSyncInstructionCache(PVOID BaseAddress, SIZE_T Length)
{
    ULONG Ctr, DLine, ILine;
    ULONG_PTR Address, Last;

    if (Length == 0)
        return;
    __asm__ __volatile__("mrc p15, 0, %0, c0, c0, 1" : "=r" (Ctr));
    DLine = 4UL << ((Ctr >> 16) & 15);
    ILine = 4UL << (Ctr & 15);
    Last = (ULONG_PTR)BaseAddress + Length - 1;
    for (Address = (ULONG_PTR)BaseAddress & ~(ULONG_PTR)(DLine - 1);; Address += DLine)
    {
        __asm__ __volatile__("mcr p15, 0, %0, c7, c11, 1" :: "r" (Address) : "memory");
        if (Last - Address < DLine)
            break;
    }
    __asm__ __volatile__("dsb ish" ::: "memory");
    for (Address = (ULONG_PTR)BaseAddress & ~(ULONG_PTR)(ILine - 1);; Address += ILine)
    {
        __asm__ __volatile__("mcr p15, 0, %0, c7, c5, 1" :: "r" (Address) : "memory");
        if (Last - Address < ILine)
            break;
    }
    __asm__ __volatile__("dsb ish\n\tisb" ::: "memory");
}

NTSTATUS
MiArchSetFrameCache(ULONG Frame, ULONG Count, ULONG Flags)
{
    return MiSetDirectFrameCache(Frame, Count, Flags);
}

ULONG64
MiArchBootFrameAlias(ULONG Frame)
{
    if (Frame >= (KSEG0_BASE >> PAGE_SHIFT))
        return 0;
    return KSEG0_BASE | ((ULONG64)Frame << PAGE_SHIFT);
}

PVOID
MiArchMapFrame(ULONG64 Frame)
{
    ASSERT(Frame < (KSEG0_BASE >> PAGE_SHIFT));
    if (Frame >= (KSEG0_BASE >> PAGE_SHIFT))
        return NULL;
    return (PVOID)(ULONG_PTR)(KSEG0_BASE | (Frame << PAGE_SHIFT));
}

PVOID
MiArchDebugMapFrame(ULONG64 Frame)
{
    return Frame < (KSEG0_BASE >> PAGE_SHIFT) ? MiArchMapFrame(Frame) : NULL;
}

VOID
MiArchUnmapFrame(PVOID Mapping)
{
    UNREFERENCED_PARAMETER(Mapping);
}

MI_PTE
MiArchPteRead(PMI_PTE Slot)
{
    return __atomic_load_n(Slot, __ATOMIC_ACQUIRE);
}

VOID
MiArchPteWrite(PMI_PTE Slot, MI_PTE Value)
{
    MI_PTE Old = MiArchPteRead(Slot);

    if (MiArchPteNeedsBreak(Old, Value))
    {
        __atomic_store_n(Slot, 0, __ATOMIC_RELEASE);
        MiArchTlbInvalidateAll(TRUE);
    }
    __atomic_store_n(Slot, Value, __ATOMIC_RELEASE);
    __asm__ __volatile__("dsb ishst\n\tisb" ::: "memory");
}

BOOLEAN
MiArchPteCompareExchange(PMI_PTE Slot, MI_PTE Expected, MI_PTE Value)
{
    BOOLEAN Changed = __atomic_compare_exchange_n(Slot, &Expected, Value, FALSE,
                                                  __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);

    if (Changed)
        __asm__ __volatile__("dsb ishst\n\tisb" ::: "memory");
    return Changed;
}

VOID
MiArchWriteBootPte(PMI_PTE Slot, MI_PTE Value)
{
    typedef VOID (*PMI_ARM_BOOT_WRITE)(PMI_PTE, MI_PTE);
    PMI_ARM_BOOT_WRITE Write = (PVOID)((ULONG_PTR)MiArmWriteBootPtePhysical & ~KSEG0_BASE);

    Write(Slot, Value);
}

ULONG64
MiArchBootRootFrame(VOID)
{
    ULONG Low, High;

    __asm__ __volatile__("mrrc p15, 0, %0, %1, c2" : "=r" (Low), "=r" (High));
    return (((ULONG64)High << 32 | Low) & 0xFFFFFFF000ULL) >> PAGE_SHIFT;
}

ULONG64
MiArchDebugRootFrame(ULONG64 VirtualAddress)
{
    UNREFERENCED_PARAMETER(VirtualAddress);
    return MiArchBootRootFrame();
}

VOID
MiArchBootClearUserHalf(ULONG64 RootFrame)
{
    PMI_PTE Root = MiArchMapFrame(RootFrame);

    MiArchPteWrite(&Root[0], 0);
    MiArchPteWrite(&Root[1], 0);
    MiArchTlbInvalidateAll(TRUE);
}
