/*
 * PROJECT:     LiberNT API tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Armv7 LPAE descriptor regression tests
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <nvs/include/mienv.h>

static ULONG Checks, Failures;
static MI_PTE Roots[2][512];

#define CHECK(e) do { Checks++; if (!(e)) { if (Failures++ < 20) fprintf(stderr, "%u: %s\n", __LINE__, #e); } } while (0)

PVOID MiArchMapFrame(ULONG64 Frame) { MI_ASSERT(Frame < 2); return Roots[Frame]; }
VOID MiArchUnmapFrame(PVOID Mapping) { UNREFERENCED_PARAMETER(Mapping); }
MI_PTE MiArchPteRead(PMI_PTE Slot) { return *Slot; }
VOID MiArchPteWrite(PMI_PTE Slot, MI_PTE Value) { *Slot = Value; }

int main(void)
{
    ULONG Protection, Flags, Index;
    MI_PTE Pte, Updated;
    const ULONG64 Frame = 0xABCDEF0;
    const MI_ARCH_DESCRIPTOR *Arch = MiArchDescribe();

    CHECK(sizeof(MI_PTE) == 8);
    CHECK(Arch->UserAddressStart == 0x10000);
    CHECK(Arch->PagingLevels == 3 && Arch->VirtualAddressBits == 32 && Arch->PhysicalAddressBits == 40);
    CHECK(Arch->Level[2].EntryCount == 4 && Arch->Level[1].EntryCount == 512 && Arch->Level[0].EntryCount == 512);
    CHECK(Arch->PteBytes == 8 && Arch->LargePageSize == 0x200000);
    CHECK(!Arch->HasHardwareDirtyBit && !Arch->HasHardwareAccessedBit && Arch->RequiresBreakBeforeMake);
    for (Protection = 0; Protection <= MI_PROT_MASK; Protection++)
    {
        for (Flags = 0; Flags < 128; Flags++)
        {
            Pte = MiArchPteMakeLeaf(Frame, Protection, Flags);
            if (!MI_PROT_IS_ACCESSIBLE(Protection) || MI_PROT_IS_GUARD(Protection))
            {
                CHECK(Pte == 0);
                continue;
            }
            CHECK(MiArchPteIsValid(Pte));
            CHECK(MiArchPteFrame(Pte) == Frame);
            CHECK((Pte & 3) == 3);
            CHECK((Pte & (1ULL << 51)) == 0);
            CHECK(MiArchPteIsUser(Pte) == !!(Flags & MI_LEAF_USER));
            CHECK(MiArchPteIsExecutable(Pte, FALSE) == !!(MI_PROT_IS_EXECUTE(Protection) && !(Flags & MI_LEAF_USER)));
            CHECK(MiArchPteIsExecutable(Pte, TRUE) == !!(MI_PROT_IS_EXECUTE(Protection) && (Flags & MI_LEAF_USER)));
            CHECK(MiArchPteIsWritable(Pte) == !!MI_PROT_IS_WRITABLE(Protection));
            CHECK(MiArchPteIsCopyOnWrite(Pte) == !!MI_PROT_IS_COPY(Protection));
            CHECK(MiArchPteIsHardwareWritable(Pte) == !!(MI_PROT_IS_WRITABLE(Protection) && (Flags & MI_LEAF_DIRTY)));
            CHECK(MiArchPteIsDirty(Pte) == !!(MI_PROT_IS_WRITABLE(Protection) && (Flags & MI_LEAF_DIRTY)));
            Updated = MiArchPteSetDirty(Pte, TRUE);
            CHECK(MiArchPteFrame(Updated) == Frame);
            CHECK(MiArchPteIsDirty(Updated) == !!MI_PROT_IS_WRITABLE(Protection));
            CHECK(!MiArchPteIsDirty(MiArchPteSetDirty(Updated, FALSE)));
            CHECK(!MiArchPteIsAccessed(MiArchPteSetAccessed(Pte, FALSE)));
            CHECK(MiArchPteIsAccessed(MiArchPteSetAccessed(Pte, TRUE)));
            CHECK(!MiArchPteNeedsBreak(Pte, MiArchPteSetDirty(Pte, TRUE)));
            CHECK(MiArchPteNeedsBreak(Pte, MiArchPteBlockToPage(Pte, Frame + 1)));
        }
    }
    CHECK(MiArchPteMakeLeaf(0x10000000, MI_PROT_READWRITE, 0) == 0);
    CHECK(MiArchPteMakeTable(0x10000000, 0) == 0);
    CHECK(MiArchPteMakeTable(Frame, 0) == (Frame << 12 | 3));
    CHECK(MiArchPteMakeBlock(1, MI_PROT_READWRITE, 0) == 0);
    Pte = MiArchPteMakeBlock(0x400, MI_PROT_EXECUTE_READWRITE, MI_LEAF_DIRTY);
    CHECK((Pte & 3) == 1);
    CHECK(MiArchPteIsBlock(Pte, 1));
    CHECK(!MiArchPteIsBlock(Pte, 0));
    CHECK(MiArchPteIsLeafDescriptor(MiArchPteBlockToPage(Pte, 0x401)));
    for (Index = 0; Index < 512; Index++)
    {
        Roots[0][Index] = Index + 16;
        Roots[1][Index] = ~(MI_PTE)0;
    }
    MiArchInitializeProcessRoot(0, 1);
    for (Index = 0; Index < 512; Index++)
        CHECK(Roots[1][Index] == ((Index == 2 || Index == 3) ? Roots[0][Index] : 0));
    for (Index = MiSoftDemandZero; Index <= MiSoftResident; Index++)
        CHECK(!MiArchPteIsValid(MiSoftMake((MI_SOFT_KIND)Index, MI_PROT_EXECUTE_READWRITE, Frame)));
    printf("ARM LPAE PTE: %u checks, %u failures\n", Checks, Failures);
    return Failures != 0;
}
