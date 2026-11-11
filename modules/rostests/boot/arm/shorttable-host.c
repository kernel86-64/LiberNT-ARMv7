/*
 * PROJECT:     LiberNT boot tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Exercise hardware and software short-PTE backing without NT stubs
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <nvs/include/mm.h>
#include <nvs/arch/arm/shortpte.h>
#include <assert.h>

#define FRAME_COUNT 128
static _Alignas(65536) UCHAR Ram[FRAME_COUNT * PAGE_SIZE];
static MI_ARCH_DESCRIPTOR Arch;
static MI_PFN Pfn[FRAME_COUNT];
MI_SYSTEM MiSystem;
static ULONG Locks, Stores, Breaks;

const MI_ARCH_DESCRIPTOR *MiArchDescribe(VOID) { return &Arch; }
PVOID MiArchMapFrame(ULONG64 Frame) { assert(Frame < FRAME_COUNT); return Ram + (Frame << PAGE_SHIFT); }
VOID MiArchUnmapFrame(PVOID Mapping) { (void)Mapping; }
MI_PTE MiArchPteRead(PMI_PTE Slot) { return __atomic_load_n(Slot, __ATOMIC_ACQUIRE); }
VOID MiArchPteWrite(PMI_PTE Slot, MI_PTE Value) { assert(MiArmShortStore(Slot, 0, Value, FALSE)); }
ULONG64 MiArmShortPhysicalAddress(PVOID Address)
{
    ULONG_PTR Offset = (ULONG_PTR)Address - (ULONG_PTR)Ram;
    return Offset < sizeof(Ram) ? Offset : ~(ULONG64)0;
}
ULONG MiArmShortLock(VOID) { assert(!Locks++); return 0x80; }
VOID MiArmShortUnlock(ULONG State) { assert(State == 0x80 && Locks-- == 1); }
VOID MiArmShortHardwareStore(PULONG Slot, ULONG Value, BOOLEAN Break)
{
    assert(Locks == 1);
    if (Break) { assert(Value == 0); ++Breaks; }
    *Slot = Value;
    ++Stores;
}
VOID MiArmShortFailure(ULONG Code, ULONG64 Address, MI_PTE Value)
{
    (void)Code; (void)Address; (void)Value; abort();
}

int main(void)
{
    PULONG Root = MiArchMapFrame(16), Table = MiArchMapFrame(32);
    PMI_PTE Shadow, Slot;
    MI_PTE Clean, Copy, Dirty, Prototype = 0;
    ULONG Hardware, Saved, Before;
    ULONG64 Physical;
    BOOLEAN IsRoot;

    Arch.PagingLevels = 2;
    Arch.PteBytes = 8;
    Arch.PageSize = PAGE_SIZE;
    Arch.PageShift = PAGE_SHIFT;
    Arch.Level[0].EntryCount = 256;
    Arch.Level[1].EntryCount = 4096;
    Arch.TablePrefixBytes[0] = 1024;
    Arch.TablePrefixBytes[1] = 16384;
    MiArmShortDescriptors = TRUE;
    MiSystem.Arch = &Arch;
    MiSystem.Pfn.Pfn = Pfn;
    MiSystem.Pfn.FrameCount = FRAME_COUNT;
    MiSystem.SystemSpace.RootFrame = 16;
    for (ULONG Frame = 16; Frame <= 33; ++Frame)
    {
        Pfn[Frame].Flags = MI_PFN_FLAG_PAGE_TABLE;
        Pfn[Frame].PageTableOwner = &MiSystem.SystemSpace;
    }
    Clean = MiArchPteMakeLeaf(64, MI_PROT_READWRITE, MI_LEAF_USER);
    Copy = MiArchPteMakeLeaf(64, MI_PROT_WRITECOPY, MI_LEAF_USER);
    Dirty = MiArchPteSetDirty(Clean, TRUE);
    assert(MiArmShortEncodePte(Dirty, FALSE, &Table[7]));
    Root[3072] = (32 << PAGE_SHIFT) | 1;
    Root[4095] = Root[3072];
    memset((PUCHAR)Root + 16384, 0xcc, 32768);
    assert(!MiArmShortPrepareBoot(17, 16, 34));
    assert(!MiArmShortPrepareBoot(16, 16, 31));
    assert(MiArmShortPrepareBoot(16, 16, 34));
    Shadow = MiArchMapTableFrame(&Arch, 16, 1);
    assert(MiArchPteFrame(Shadow[4095]) == 32);
    assert(!Shadow[3071]);
    Slot = (PMI_PTE)MiArchMapTableFrame(&Arch, 32, 0) + 7;
    assert(MiArchPteFrame(*Slot) == 64 && MiArchPteIsDirty(*Slot));
    assert(MiArmShortLocate(MiArmShortPhysicalAddress(Slot), &Physical, &IsRoot));
    assert(Physical == (32 << PAGE_SHIFT) + 7 * 4 && !IsRoot);
    MiArchPteWrite(Slot, Clean);
    assert(*Slot == Clean && !MiArchPteIsDirty(*Slot));
    assert(MiArmShortEncodePte(Clean, FALSE, &Hardware) && Table[7] == Hardware);
    Saved = Table[7];
    MiArchPteWrite(Slot, Copy);
    assert(*Slot == Copy && Table[7] == Saved);
    Before = Stores;
    assert(!MiArmShortStore(Slot, Clean, Dirty, TRUE));
    assert(Stores == Before && *Slot == Copy && Table[7] == Saved);
    assert(MiArmShortStore(Slot, Copy, Dirty, TRUE));
    assert(*Slot == Dirty && MiArmShortEncodePte(Dirty, FALSE, &Hardware) && Table[7] == Hardware);
    assert(Breaks >= 2);
    MiArchPteWrite(Slot, MiSoftMake(MiSoftTransition, MI_PROT_READWRITE, 64));
    assert(!Table[7] && MiSoftKind(*Slot) == MiSoftTransition && MiSoftValue(*Slot) == 64);
    assert(MiArmShortLocate(MiArmShortPhysicalAddress(&Shadow[4095]), &Physical, &IsRoot));
    assert(IsRoot && Physical == (16 << PAGE_SHIFT) + 4095 * 4);
    MiArchPteWrite(&Shadow[4095], MiArchPteMakeTable(33, 0));
    assert(Root[4095] == ((33 << PAGE_SHIFT) | 1));
    Before = Stores;
    assert(MiArmShortStore(&Prototype, 0, Copy, TRUE));
    assert(Prototype == Copy && Stores == Before);
    assert(!MiArmShortStore(&Prototype, Clean, Dirty, TRUE));
    assert(!Locks);
    puts("NVS short-descriptor backing: PASS");
    return 0;
}
