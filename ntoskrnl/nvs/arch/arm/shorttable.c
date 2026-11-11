/*
 * PROJECT:     LiberNT NT Virtual Memory Subsystem
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Keep short hardware descriptors separate from NVS software PTEs
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <nvs/include/mm.h>
#include "shortpte.h"

extern MI_SYSTEM MiSystem;
BOOLEAN MiArmShortDescriptors;
ULONG MiArmShortBootRoot;

static BOOLEAN
MiArmShortRootSlot(ULONG64 Physical, ULONG RootFrame, PULONG64 Hardware)
{
    ULONG64 Offset, Start = ((ULONG64)RootFrame << PAGE_SHIFT) + 16384;

    if (Physical < Start || Physical - Start >= 4096 * sizeof(MI_PTE)) return FALSE;
    Offset = Physical - Start;
    if (Offset % sizeof(MI_PTE)) return FALSE;
    *Hardware = ((ULONG64)RootFrame << PAGE_SHIFT) + (Offset / sizeof(MI_PTE)) * sizeof(ULONG);
    return TRUE;
}

BOOLEAN
MiArmShortLocate(ULONG64 Physical, PULONG64 Hardware, PBOOLEAN Root)
{
    ULONG Frame = (ULONG)(Physical >> PAGE_SHIFT);
    ULONG Offset = (ULONG)(Physical & (PAGE_SIZE - 1));
    PMI_PFN Pfn;
    PMI_ADDRESS_SPACE Owner;

    if (Physical > 0x7fffffff) return FALSE;
    if (MiArmShortBootRoot && MiArmShortRootSlot(Physical, MiArmShortBootRoot, Hardware))
    {
        *Root = TRUE;
        return TRUE;
    }
    if (!MiSystem.Pfn.Pfn || Frame >= MiSystem.Pfn.FrameCount) return FALSE;
    Pfn = &MiSystem.Pfn.Pfn[Frame];
    if (!(MI_PFN_FLAGS(Pfn) & MI_PFN_FLAG_PAGE_TABLE)) return FALSE;
    Owner = Pfn->PageTableOwner;
    if (Owner && MiArmShortRootSlot(Physical, Owner->RootFrame, Hardware))
    {
        *Root = TRUE;
        return TRUE;
    }
    if (Owner && Frame >= Owner->RootFrame && Frame - Owner->RootFrame < 16) return FALSE;
    if (Offset < 1024 || Offset >= 1024 + 256 * sizeof(MI_PTE) || Offset % sizeof(MI_PTE)) return FALSE;
    *Root = FALSE;
    *Hardware = ((ULONG64)Frame << PAGE_SHIFT) + ((Offset - 1024) / sizeof(MI_PTE)) * sizeof(ULONG);
    return TRUE;
}

BOOLEAN
MiArmShortStore(PMI_PTE Slot, MI_PTE Expected, MI_PTE Value, BOOLEAN Compare)
{
    ULONG64 Physical = MiArmShortPhysicalAddress(Slot), Hardware;
    ULONG State, Entry;
    BOOLEAN Root;
    MI_PTE Old;
    PULONG HardwareSlot;

    if (!MiArmShortLocate(Physical, &Hardware, &Root))
    {
        if (Compare)
            return __atomic_compare_exchange_n(Slot, &Expected, Value, FALSE, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
        __atomic_store_n(Slot, Value, __ATOMIC_RELEASE);
        return TRUE;
    }
    State = MiArmShortLock();
    Old = __atomic_load_n(Slot, __ATOMIC_ACQUIRE);
    if (Compare && Old != Expected)
    {
        MiArmShortUnlock(State);
        return FALSE;
    }
    if (!MiArmShortEncodePte(Value, Root, &Entry))
    {
        MiArmShortFailure(1, Physical, Value);
        MiArmShortUnlock(State);
        return FALSE;
    }
    HardwareSlot = (PVOID)((PUCHAR)MiArchMapFrame(Hardware >> PAGE_SHIFT) + (Hardware & (PAGE_SIZE - 1)));
    if (*HardwareSlot != Entry)
        MiArmShortHardwareStore(HardwareSlot, 0, (*HardwareSlot & 3u) != 0);
    __atomic_store_n(Slot, Value, __ATOMIC_RELEASE);
    MiArmShortHardwareStore(HardwareSlot, Entry, FALSE);
    MiArmShortUnlock(State);
    return TRUE;
}

BOOLEAN
MiArmShortPrepareBoot(ULONG RootFrame, ULONG FirstFrame, ULONG LastFrame)
{
    PULONG Root;
    PMI_PTE Shadow;
    ULONG Index, Page;
    MI_PTE Value;

    if ((RootFrame & 15) || RootFrame < FirstFrame || RootFrame >= LastFrame || LastFrame - RootFrame < 16)
        return FALSE;
    Root = MiArchMapFrame(RootFrame);
    Shadow = (PVOID)((PUCHAR)Root + 16384);
    for (Index = 0; Index < 4096; ++Index)
    {
        if (!MiArmShortDecodePte(Root[Index], TRUE, &Value)) return FALSE;
        __atomic_store_n(&Shadow[Index], Value, __ATOMIC_RELEASE);
        if ((Root[Index] & 3u) == 1u)
        {
            ULONG Frame = (Root[Index] & 0xfffff000u) >> PAGE_SHIFT;
            PULONG Table;
            PMI_PTE Software;

            if (Frame < RootFrame + 16 || Frame < FirstFrame || Frame >= LastFrame) return FALSE;
            Table = MiArchMapFrame(Frame);
            Software = (PVOID)((PUCHAR)Table + 1024);
            for (Page = 0; Page < 256; ++Page)
            {
                if (!MiArmShortDecodePte(Table[Page], FALSE, &Value)) return FALSE;
                __atomic_store_n(&Software[Page], Value, __ATOMIC_RELEASE);
            }
        }
    }
    MiArmShortBootRoot = RootFrame;
    return TRUE;
}
