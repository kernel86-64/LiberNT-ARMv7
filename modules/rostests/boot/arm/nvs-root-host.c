/*
 * PROJECT:     LiberNT API tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Exercise NVS allocation and adoption of multi-page root tables
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <nvs/include/mm.h>
#include <assert.h>

#define TEST_FRAMES 128u
static _Alignas(4096) UCHAR Ram[TEST_FRAMES * PAGE_SIZE];
static MI_PFN Frames[TEST_FRAMES];
static MI_SYSTEM System;
static MI_ARCH_DESCRIPTOR Arch;
_Thread_local ULONG MiHostCpu;
_Thread_local KIRQL MiHostIrql;
_Thread_local ULONG MiHostReadClusterSize;

KIRQL MiHostRaiseIrql(KIRQL Irql) { KIRQL Old = MiHostIrql; MiHostIrql = Irql; return Old; }
VOID MiHostLowerIrql(KIRQL Irql) { MiHostIrql = Irql; }
VOID MiHostQueueDpc(MI_HOST_DPC_ROUTINE Routine, PVOID Context) { Routine(Context); }
const MI_ARCH_DESCRIPTOR *MiArchDescribe(VOID) { return &Arch; }
PVOID MiArchMapFrame(ULONG64 Frame) { assert(Frame < TEST_FRAMES); return Ram + Frame * PAGE_SIZE; }
VOID MiArchUnmapFrame(PVOID Mapping) { (void)Mapping; }
VOID MiArchPteWrite(PMI_PTE Slot, MI_PTE Value) { *Slot = Value; }
MI_PTE MiArchPteRead(PMI_PTE Slot) { return *Slot; }
ULONG64 MiArchPteFrame(MI_PTE Value) { return Value >> PAGE_SHIFT; }
BOOLEAN MiArchPteIsValid(MI_PTE Value) { return (BOOLEAN)(Value & 1); }
BOOLEAN MiArchPteIsBlock(MI_PTE Value, ULONG Level) { (void)Value; (void)Level; return FALSE; }
MI_PTE MiArchPteMakeTable(ULONG64 Frame, ULONG Flags) { (void)Flags; return (MI_PTE)(Frame << PAGE_SHIFT) | 1; }
PVOID MiHostWindowPointer(PVOID Window, ULONG Slot) { (void)Window; (void)Slot; abort(); }
ULONG64 MiHostWindowSlot(PVOID Window, PVOID Mapping) { (void)Window; (void)Mapping; abort(); }
VOID MiHostWindowRelease(PVOID Window, ULONG Slot) { (void)Window; (void)Slot; abort(); }
VOID MiArchTlbInvalidate(ULONG64 Address, ULONG64 Count, BOOLEAN All)
{
    (void)Address; (void)Count; (void)All; abort();
}
MI_PTE MiArchPteMakeLeaf(ULONG64 Frame, ULONG Protection, ULONG Flags)
{
    (void)Frame; (void)Protection; (void)Flags; abort();
}
NTSTATUS MiArchSetFrameCache(ULONG Frame, ULONG Count, ULONG Flags)
{
    assert(Frame + Count <= TEST_FRAMES);
    (void)Flags;
    return STATUS_SUCCESS;
}
VOID MiArchSyncInstructionCache(PVOID Base, SIZE_T Length) { (void)Base; (void)Length; }
VOID MiArchInitializeProcessRoot(ULONG64 SystemRoot, ULONG64 ProcessRoot)
{
    (void)SystemRoot; (void)ProcessRoot; abort();
}
VOID MiLockRangeReset(PMI_ADDRESS_SPACE Space) { assert(!Space->LockRanges && !Space->LockedPages); }
VOID MiTlbBatchAdd(PMI_ADDRESS_SPACE Space, ULONG64 Address, ULONG Frame,
                    struct _MI_SEGMENT *Segment, PMI_PTE Prototype, BOOLEAN Private)
{
    (void)Space; (void)Address; (void)Frame; (void)Segment; (void)Prototype; (void)Private; abort();
}

static VOID Initialize(ULONG Pages, ULONG FreeFrames)
{
    memset(&System, 0, sizeof(System));
    memset(&Arch, 0, sizeof(Arch));
    memset(Ram, 0xa5, sizeof(Ram));
    Arch.PagingLevels = 2;
    Arch.PageSize = PAGE_SIZE;
    Arch.PageShift = PAGE_SHIFT;
    Arch.PteBytes = sizeof(MI_PTE);
    Arch.VirtualAddressBits = 32;
    Arch.Level[0].Shift = 12;
    Arch.Level[1].Shift = sizeof(MI_PTE) == 4 ? 20 : 21;
    Arch.Level[0].EntryCount = 1u << (Arch.Level[1].Shift - 12);
    Arch.Level[0].IndexMask = Arch.Level[0].EntryCount - 1;
    Arch.Level[1].EntryCount = Pages * PAGE_SIZE / sizeof(MI_PTE);
    Arch.Level[1].IndexMask = Arch.Level[1].EntryCount - 1;
    Arch.UserAddressStart = 0x10000;
    Arch.UserAddressEnd = 0x7ffeffff;
    Arch.SystemAddressStart = 0x80000000;
    Arch.SystemAddressEnd = 0xffffffff;
    System.Arch = &Arch;
    MiPfnDbInitialize(&System.Pfn, Frames, TEST_FRAMES, 1);
    MiPfnDbAddRange(&System.Pfn, 1, FreeFrames);
}

int main(void)
{
    for (ULONG Kind = MiSoftNone; Kind <= MiSoftResident; ++Kind)
        for (ULONG Protection = 0; Protection <= MI_PROT_MASK; ++Protection)
        {
            MI_PTE Entry = MiSoftMake((MI_SOFT_KIND)Kind, Protection, 0xfffff);
            assert(MiSoftKind(Entry) == (MI_SOFT_KIND)Kind);
            assert(MiSoftProtection(Entry) == Protection);
            assert(MiSoftValue(Entry) == 0xfffff);
            if (MI_SOFT_COMPACT) assert((Entry & 3) == 0);
            assert(MiSoftProtection(MiSoftWithProtection(Entry, 31 - Protection)) == 31 - Protection);
            assert(MiSoftValue(MiSoftWithProtection(Entry, 31 - Protection)) == 0xfffff);
            if (Kind == MiSoftPageFile || Kind == MiSoftResident)
                for (ULONG File = 0; File < 8; ++File)
                {
                    MI_PTE Backed = MiSoftWithFile(Entry, File);
                    assert(MiSoftFile(Backed) == File);
                    assert(MiSoftKind(Backed) == (MI_SOFT_KIND)Kind);
                    assert(MiSoftProtection(Backed) == Protection);
                    assert(MiSoftValue(Backed) == 0xfffff);
                    if (MI_SOFT_COMPACT) assert((Backed & 3) == 0);
                }
        }

    ULONG Pages, Index, Root, Before, Leaf;
    ULONG64 Virtual, Slot;
    NTSTATUS Status;

    for (Pages = 1; Pages <= 4; Pages *= 4)
    {
        Initialize(Pages, TEST_FRAMES - 1);
        Before = (ULONG)MiPfnAvailablePages(&System.Pfn);
        Status = MiAddressSpaceCreate(&System, &System.SystemSpace);
        assert(Status == STATUS_SUCCESS);
        Root = System.SystemSpace.RootFrame;
        assert(!(Root & (Pages - 1)));
        assert(MiPfnAvailablePages(&System.Pfn) == Before - Pages);
        for (Index = 0; Index < Pages; ++Index)
        {
            assert(Frames[Root + Index].State == MiPageActive);
            assert(Frames[Root + Index].PageTableOwner == &System.SystemSpace);
            assert(MI_PFN_FLAGS(&Frames[Root + Index]) & MI_PFN_FLAG_PAGE_TABLE);
        }
        for (Index = 0; Index < Pages * PAGE_SIZE; ++Index)
            assert(Ram[Root * PAGE_SIZE + Index] == 0);
        MiAddressSpaceDestroy(&System.SystemSpace);
        assert(MiPfnAvailablePages(&System.Pfn) == Before);
    }
    Initialize(4, 2);
    assert(MiAddressSpaceCreate(&System, &System.SystemSpace) == STATUS_NO_MEMORY);
    assert(MiPfnAvailablePages(&System.Pfn) == 2);
    Initialize(4, TEST_FRAMES - 5);
    assert(MiAddressSpaceAdopt(&System, &System.SystemSpace, 125, TRUE) == STATUS_INVALID_PARAMETER);
    assert(MiAddressSpaceAdopt(&System, &System.SystemSpace, 124, TRUE) == STATUS_SUCCESS);
    for (Index = 124; Index < TEST_FRAMES; ++Index)
    {
        assert(Frames[Index].State == MiPageActive);
        assert(Frames[Index].PageTableOwner == &System.SystemSpace);
        assert(MI_PFN_FLAGS(&Frames[Index]) & MI_PFN_FLAG_PINNED);
    }
    Leaf = MiPfnAllocatePage(&System.Pfn, MI_ALLOCATE_ZEROED);
    assert(Leaf != MI_FRAME_INVALID);
    MiPfnInitializePage(&System.Pfn, Leaf, 0, 124, 0, MI_PFN_FLAG_PAGE_TABLE);
    for (Index = (ULONG)(Arch.SystemAddressStart >> Arch.Level[1].Shift);
         Index < Arch.Level[1].EntryCount; ++Index)
    {
        Slot = ((ULONG64)124 << PAGE_SHIFT) + Index * sizeof(MI_PTE);
        Frames[Leaf].PteAddress = Slot;
        assert(MiPtSlotAddressAtLevel(&Arch, 124, (ULONG64)Index << Arch.Level[1].Shift, 1) == Slot);
        assert(MiPtVirtualAddressFromSlot(&System.SystemSpace, (ULONG64)Leaf << PAGE_SHIFT, &Virtual));
        assert(Virtual == ((ULONG64)Index << Arch.Level[1].Shift));
    }
    if (sizeof(MI_PTE) == 8)
    {
        ULONG Table[3], Level;

        Initialize(1, TEST_FRAMES - 1);
        Arch.PagingLevels = 4;
        Arch.VirtualAddressBits = 48;
        Arch.SystemAddressStart = 0xffff800000000000ULL;
        Arch.SystemAddressEnd = ~(ULONG64)0;
        for (Level = 0; Level < 4; ++Level)
        {
            Arch.Level[Level].Shift = (UCHAR)(12 + 9 * Level);
            Arch.Level[Level].EntryCount = 512;
            Arch.Level[Level].IndexMask = 511;
        }
        assert(MiAddressSpaceCreate(&System, &System.SystemSpace) == STATUS_SUCCESS);
        Root = System.SystemSpace.RootFrame;
        for (Level = 0; Level < 3; ++Level)
        {
            Table[Level] = MiPfnAllocatePage(&System.Pfn, MI_ALLOCATE_ZEROED);
            assert(Table[Level] != MI_FRAME_INVALID);
            MiPfnInitializePage(&System.Pfn, Table[Level], 0, 0, 0, MI_PFN_FLAG_PAGE_TABLE);
        }
        Frames[Table[0]].PteAddress = ((ULONG64)Table[1] << PAGE_SHIFT) + 5 * sizeof(MI_PTE);
        Frames[Table[1]].PteAddress = ((ULONG64)Table[2] << PAGE_SHIFT) + 7 * sizeof(MI_PTE);
        Frames[Table[2]].PteAddress = ((ULONG64)Root << PAGE_SHIFT) + 256 * sizeof(MI_PTE);
        Slot = ((ULONG64)Table[0] << PAGE_SHIFT) + 3 * sizeof(MI_PTE);
        assert(MiPtVirtualAddressFromSlot(&System.SystemSpace, Slot, &Virtual));
        assert(Virtual == 0xffff8001c0a03000ULL);
        for (Level = 0; Level < 3; ++Level) MiPfnShareDecrement(&System.Pfn, Table[Level], TRUE);
        MiAddressSpaceDestroy(&System.SystemSpace);
        assert(MiPfnAvailablePages(&System.Pfn) == TEST_FRAMES - 1);
    }
    if (sizeof(MI_PTE) == 8)
    {
        Initialize(8, TEST_FRAMES - 1);
        Arch.Level[0].EntryCount = 256;
        Arch.Level[0].IndexMask = 255;
        Arch.Level[1].Shift = 20;
        Arch.Level[1].EntryCount = 4096;
        Arch.Level[1].IndexMask = 4095;
        Arch.TablePrefixBytes[0] = 1024;
        Arch.TablePrefixBytes[1] = 16384;
        assert(MiArchRootPageCount(&Arch) == 16);
        assert(MiAddressSpaceCreate(&System, &System.SystemSpace) == STATUS_SUCCESS);
        Root = System.SystemSpace.RootFrame;
        assert(!(Root & 15));
        assert(MiArchMapTableFrame(&Arch, Root, 1) == Ram + Root * PAGE_SIZE + 16384);
        Leaf = MiPfnAllocatePage(&System.Pfn, MI_ALLOCATE_ZEROED);
        MiPfnInitializePage(&System.Pfn, Leaf,
                           MiArchTableSlotAddress(&Arch, Root, 3584, 1), Root, 0, MI_PFN_FLAG_PAGE_TABLE);
        Slot = MiArchTableSlotAddress(&Arch, Leaf, 19, 0);
        assert(MiPtVirtualAddressFromSlot(&System.SystemSpace, Slot, &Virtual));
        assert(Virtual == 0xe0013000);
        assert(MiPtSlotAddressAtLevel(&Arch, Root, Virtual, 1) ==
               ((ULONG64)Root << PAGE_SHIFT) + 16384 + 3584 * 8);
        MiPfnShareDecrement(&System.Pfn, Leaf, TRUE);
        MiAddressSpaceDestroy(&System.SystemSpace);
        assert(MiPfnAvailablePages(&System.Pfn) == TEST_FRAMES - 1);
    }
    printf("NVS root lifecycle: PASS (%zu-byte PTEs, 4K/16K roots)\n", sizeof(MI_PTE));
    return 0;
}
