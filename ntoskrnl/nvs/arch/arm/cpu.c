/*
 * PROJECT:     LiberNT Kernel
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARMv7 LPAE processor and page-table access
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <nvs/nt/mint.h>
#include "shortpte.h"

VOID MiArmWriteBootPtePhysical(PMI_PTE Slot, MI_PTE Value);

VOID NTAPI
MiInitializeKernelVaLayout(const LOADER_PARAMETER_BLOCK *LoaderBlock)
{
    ULONG Ttbcr;
    PLIST_ENTRY Link;
    ULONG Root, First = 0, Last = 0;

    __asm__ __volatile__("mrc p15, 0, %0, c2, c0, 2" : "=r" (Ttbcr));
    MiArmShortDescriptors = (Ttbcr & (1UL << 31)) == 0;
    if (MiArmShortDescriptors)
    {
        Root = LoaderBlock->u.Arm.PdrPage;
        for (Link = LoaderBlock->MemoryDescriptorListHead.Flink;
             Link != &LoaderBlock->MemoryDescriptorListHead; Link = Link->Flink)
        {
            PMEMORY_ALLOCATION_DESCRIPTOR Descriptor = CONTAINING_RECORD(Link, MEMORY_ALLOCATION_DESCRIPTOR, ListEntry);
            if (Descriptor->MemoryType == LoaderStartupPdrPage && Root >= Descriptor->BasePage &&
                Root - Descriptor->BasePage < Descriptor->PageCount)
            {
                First = Descriptor->BasePage;
                Last = Descriptor->BasePage + Descriptor->PageCount;
                break;
            }
        }
        if (Ttbcr != 0 || !MiArmShortPrepareBoot(Root, First, Last))
            KeBugCheckEx(MEMORY_MANAGEMENT, 0x53445054, Root, First, Last);
        DbgPrintEarly("ARM short-descriptor NVS tables prepared: root=%lx\n", Root);
    }
    MmSystemRangeStart = (PVOID)KSEG0_BASE;
}

static volatile UCHAR MiArmShortTableLock;

ULONG MiArmShortLock(VOID)
{
    ULONG State;
    __asm__ __volatile__("mrs %0, cpsr\n\tcpsid i" : "=r"(State) :: "memory");
    while (__atomic_test_and_set(&MiArmShortTableLock, __ATOMIC_ACQUIRE))
        __asm__ __volatile__("yield" ::: "memory");
    return State;
}

VOID MiArmShortUnlock(ULONG State)
{
    __atomic_clear(&MiArmShortTableLock, __ATOMIC_RELEASE);
    __asm__ __volatile__("msr cpsr_c, %0" :: "r"(State) : "memory");
}

VOID MiArmShortHardwareStore(PULONG Slot, ULONG Value, BOOLEAN Break)
{
    __atomic_store_n(Slot, Value, __ATOMIC_RELEASE);
    __asm__ __volatile__("mcr p15, 0, %0, c7, c10, 1\n\tdsb sy" :: "r"(Slot) : "memory");
    if (Break) MiArchTlbInvalidateAll(TRUE);
}

ULONG64 MiArmShortPhysicalAddress(PVOID Address)
{
    ULONG64 Physical;
    if (MiSystem.Arch && MiSystem.SystemSpace.RootFrame &&
        MiPtTranslateRoot(MiSystem.Arch, MiSystem.SystemSpace.RootFrame, (ULONG_PTR)Address, &Physical, NULL))
        return Physical;
    return ~(ULONG64)0;
}

VOID MiArmShortFailure(ULONG Code, ULONG64 Address, MI_PTE Value)
{
    KeBugCheckEx(MEMORY_MANAGEMENT, 0x53445045, Code, (ULONG_PTR)Address, (ULONG_PTR)Value);
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
    if (MiArmShortDescriptors)
    {
        MiArmShortStore(Slot, 0, Value, FALSE);
        return;
    }
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
    if (MiArmShortDescriptors) return MiArmShortStore(Slot, Expected, Value, TRUE);
    BOOLEAN Changed = __atomic_compare_exchange_n(Slot, &Expected, Value, FALSE,
                                                  __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);

    if (Changed)
        __asm__ __volatile__("dsb ishst\n\tisb" ::: "memory");
    return Changed;
}

VOID
MiArchWriteBootPte(PMI_PTE Slot, MI_PTE Value)
{
    if (MiArmShortDescriptors)
    {
        typedef VOID (*PMI_ARM_SHORT_BOOT_WRITE)(ULONG, ULONG, ULONG, PMI_PTE);
        extern VOID MiArmWriteShortBootPtePhysical(ULONG, ULONG, ULONG, PMI_PTE);
        PMI_ARM_SHORT_BOOT_WRITE WriteShort = (PVOID)((ULONG_PTR)MiArmWriteShortBootPtePhysical & ~KSEG0_BASE);
        ULONG64 Physical = MiArmShortPhysicalAddress(Slot), Hardware;
        ULONG Entry, State;
        BOOLEAN Root;
        if (!MiArmShortLocate(Physical, &Hardware, &Root) || !MiArmShortEncodePte(Value, Root, &Entry))
            MiArmShortFailure(2, Physical, Value);
        State = MiArmShortLock();
        WriteShort((ULONG)Hardware, Entry, (ULONG)Physical, &Value);
        MiArmShortUnlock(State);
        return;
    }
    typedef VOID (*PMI_ARM_BOOT_WRITE)(PMI_PTE, MI_PTE);
    PMI_ARM_BOOT_WRITE Write = (PVOID)((ULONG_PTR)MiArmWriteBootPtePhysical & ~KSEG0_BASE);

    Write(Slot, Value);
}

ULONG64
MiArchBootRootFrame(VOID)
{
    ULONG Low, High;

    if (MiArmShortDescriptors)
    {
        __asm__ __volatile__("mrc p15, 0, %0, c2, c0, 0" : "=r"(Low));
        return (Low & 0xffffc000u) >> PAGE_SHIFT;
    }
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
    const MI_ARCH_DESCRIPTOR *Arch = MiArchDescribe();
    PMI_PTE Root = MiArchMapTableFrame(Arch, RootFrame, Arch->PagingLevels - 1);
    ULONG Index, Count = MiArmShortDescriptors ? 2048 : 2;

    for (Index = 0; Index < Count; ++Index) MiArchPteWrite(&Root[Index], 0);
    MiArchTlbInvalidateAll(TRUE);
}
