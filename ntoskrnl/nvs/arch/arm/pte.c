/*
 * PROJECT:     LiberNT NT Virtual Memory Subsystem
 * FILE:        ntoskrnl/nvs/arch/arm/pte.c
 * PURPOSE:     ARMv7 LPAE page-table entry operations
 *
 * SPDX-FileCopyrightText: 2026 Ahmed ARIF
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <nvs/include/mienv.h>
#include "shortpte.h"

#define A32_VALID          (1ULL << 0)
#define A32_TYPE_PAGE      (1ULL << 1)
#define A32_ATTR_SHIFT     2
#define A32_ATTR_NORMAL    (0ULL << A32_ATTR_SHIFT)
#define A32_ATTR_DEVICE    (1ULL << A32_ATTR_SHIFT)
#define A32_ATTR_NOCACHE   (2ULL << A32_ATTR_SHIFT)
#define A32_ATTR_WC        (3ULL << A32_ATTR_SHIFT)
#define A32_AP_USER        (1ULL << 6)
#define A32_AP_READONLY    (1ULL << 7)
#define A32_SH_INNER       (3ULL << 8)
#define A32_AF             (1ULL << 10)
#define A32_NG             (1ULL << 11)
#define A32_PXN            (1ULL << 53)
#define A32_XN            (1ULL << 54)
#define A32_SW_WRITE       (1ULL << 55)
#define A32_SW_COPY        (1ULL << 56)
#define A32_FRAME_MASK     0x000000FFFFFFF000ULL

MI_PTE
MiArchPteMakeLeaf(
    _In_ ULONG64 Frame,
    _In_ ULONG Protection,
    _In_ ULONG Flags)
{
    MI_PTE Pte = A32_VALID | A32_TYPE_PAGE | A32_AF | ((Frame << 12) & A32_FRAME_MASK);

    if (Frame >= (MiArmShortDescriptors ? (1ULL << 20) : (1ULL << 28)) || !MI_PROT_IS_ACCESSIBLE(Protection) || MI_PROT_IS_GUARD(Protection))
        return 0;

    Flags |= MI_PROT_LEAF_CACHE(Protection);
    if (Flags & MI_LEAF_DEVICE)
        Pte |= A32_ATTR_DEVICE;
    else if (Flags & MI_LEAF_NOCACHE)
        Pte |= A32_ATTR_NOCACHE | A32_SH_INNER;
    else if (Flags & MI_LEAF_WRITECOMBINE)
        Pte |= A32_ATTR_WC | A32_SH_INNER;
    else
        Pte |= A32_ATTR_NORMAL | A32_SH_INNER;

    if (Flags & MI_LEAF_USER)
        Pte |= A32_AP_USER | A32_NG;
    else if (!(Flags & MI_LEAF_GLOBAL))
        Pte |= A32_NG;

    if (MI_PROT_IS_EXECUTE(Protection))
        Pte |= (Flags & MI_LEAF_USER) ? A32_PXN : 0;
    else
        Pte |= A32_PXN | A32_XN;

    if (MI_PROT_IS_WRITABLE(Protection))
    {
        Pte |= A32_SW_WRITE;
        if (!(Flags & MI_LEAF_DIRTY))
            Pte |= A32_AP_READONLY;
    }
    else
    {
        Pte |= A32_AP_READONLY;
        if (MI_PROT_IS_COPY(Protection))
            Pte |= A32_SW_COPY;
    }

    if (MiArmShortDescriptors && !(Flags & MI_LEAF_DEVICE)) Pte &= ~A32_SH_INNER;
    return Pte;
}

MI_PTE
MiArchPteMakeTable(
    _In_ ULONG64 Frame,
    _In_ ULONG Flags)
{
    UNREFERENCED_PARAMETER(Flags);
    if (Frame >= (MiArmShortDescriptors ? (1ULL << 20) : (1ULL << 28)))
        return 0;
    return A32_VALID | A32_TYPE_PAGE | (Frame << 12);
}

BOOLEAN
MiArchPteIsBlock(_In_ MI_PTE Pte, _In_ ULONG Level)
{
    return (Level == 1 || Level == 2) && (Pte & (A32_VALID | A32_TYPE_PAGE)) == A32_VALID;
}

MI_PTE
MiArchPteMakeBlock(_In_ ULONG64 Frame, _In_ ULONG Protection, _In_ ULONG Flags)
{
    if (Frame & (MiArmShortDescriptors ? 0xffu : 0x1ffu))
        return 0;
    return MiArchPteMakeLeaf(Frame, Protection, Flags) & ~A32_TYPE_PAGE;
}

BOOLEAN
MiArchPteIsValid(_In_ MI_PTE Pte)
{
    return (Pte & A32_VALID) != 0;
}

MI_PTE
MiArchPteBlockToPage(MI_PTE Pte, ULONG64 Frame)
{
    return (Pte & ~A32_FRAME_MASK) | ((Frame << PAGE_SHIFT) & A32_FRAME_MASK) | A32_TYPE_PAGE;
}

MI_PTE
MiArchPteWithCache(MI_PTE Pte, ULONG Flags)
{
    MI_PTE Attributes = MiArchPteMakeLeaf(0, MI_PROT_READWRITE, Flags);

    return (Pte & ~(7ULL << A32_ATTR_SHIFT)) | (Attributes & (7ULL << A32_ATTR_SHIFT));
}

ULONG64
MiArchPteFrame(_In_ MI_PTE Pte)
{
    return (Pte & A32_FRAME_MASK) >> 12;
}

BOOLEAN
MiArchPteIsWritable(_In_ MI_PTE Pte)
{
    return (Pte & A32_SW_WRITE) != 0;
}

BOOLEAN
MiArchPteIsCopyOnWrite(_In_ MI_PTE Pte)
{
    return (Pte & A32_SW_COPY) != 0;
}

BOOLEAN
MiArchPteIsHardwareWritable(_In_ MI_PTE Pte)
{
    return (Pte & (A32_VALID | A32_AP_READONLY)) == A32_VALID;
}

ULONG
MiArchPteLeafFlags(_In_ MI_PTE Pte)
{
    switch (Pte & (7ULL << A32_ATTR_SHIFT))
    {
        case A32_ATTR_DEVICE:
            return MI_LEAF_DEVICE;
        case A32_ATTR_NOCACHE:
            return MI_LEAF_NOCACHE;
        case A32_ATTR_WC:
            return MI_LEAF_WRITECOMBINE;
        default:
            return 0;
    }
}

BOOLEAN
MiArchPteIsDirty(_In_ MI_PTE Pte)
{
    return (Pte & A32_SW_WRITE) != 0 && (Pte & A32_AP_READONLY) == 0;
}

BOOLEAN
MiArchPteIsAccessed(_In_ MI_PTE Pte)
{
    return (Pte & A32_AF) != 0;
}

BOOLEAN
MiArchPteIsLeafDescriptor(_In_ MI_PTE Pte)
{
    return (Pte & (A32_VALID | A32_TYPE_PAGE)) == (A32_VALID | A32_TYPE_PAGE);
}

BOOLEAN
MiArchIsSelfMapAddress(_In_ ULONG64 VirtualAddress)
{
    UNREFERENCED_PARAMETER(VirtualAddress);
    return FALSE;
}

BOOLEAN
MiArchTranslateWindow(
    _In_ ULONG64 VirtualAddress,
    _Out_ PULONG64 PhysicalAddress,
    _Out_opt_ PMI_PTE LeafPte)
{
    UNREFERENCED_PARAMETER(VirtualAddress);
    UNREFERENCED_PARAMETER(PhysicalAddress);
    UNREFERENCED_PARAMETER(LeafPte);
    return FALSE;
}

BOOLEAN
MiArchPteIsUser(_In_ MI_PTE Pte)
{
    return (Pte & A32_AP_USER) != 0;
}

BOOLEAN
MiArchPteIsExecutable(_In_ MI_PTE Pte, _In_ BOOLEAN UserMode)
{
    return (Pte & A32_XN) == 0 &&
           (UserMode ? MiArchPteIsUser(Pte) : (Pte & A32_PXN) == 0);
}

MI_PTE
MiArchPteGrantUserExecute(_In_ MI_PTE Pte)
{
    return (Pte & A32_XN) ? 0 : Pte;
}

MI_PTE
MiArchPteRevokeUserExecute(_In_ MI_PTE Pte)
{
    return Pte;
}

MI_PTE
MiArchPteSetDirty(_In_ MI_PTE Pte, _In_ BOOLEAN Dirty)
{
    if (!(Pte & A32_SW_WRITE))
        return Pte;

    return Dirty ? (Pte & ~A32_AP_READONLY) : (Pte | A32_AP_READONLY);
}

MI_PTE
MiArchPteSetAccessed(_In_ MI_PTE Pte, _In_ BOOLEAN Accessed)
{
    return Accessed ? (Pte | A32_AF) : (Pte & ~A32_AF);
}

BOOLEAN
MiArchPteNeedsBreak(_In_ MI_PTE Old, _In_ MI_PTE New)
{
    const MI_PTE Sensitive = A32_FRAME_MASK | (7ULL << A32_ATTR_SHIFT) | A32_SH_INNER | A32_NG | A32_TYPE_PAGE;

    if (!(Old & A32_VALID) || !(New & A32_VALID))
        return FALSE;

    return ((Old ^ New) & Sensitive) != 0;
}

VOID
MiArchInitializeProcessRoot(_In_ ULONG64 SystemRootFrame, _In_ ULONG64 ProcessRootFrame)
{
    const MI_ARCH_DESCRIPTOR *Arch = MiArchDescribe();
    PMI_PTE System = MiArchMapTableFrame(Arch, SystemRootFrame, Arch->PagingLevels - 1);
    PMI_PTE Process = MiArchMapTableFrame(Arch, ProcessRootFrame, Arch->PagingLevels - 1);
    ULONG Index;

    for (Index = 0; Index < Arch->Level[Arch->PagingLevels - 1].EntryCount; Index++)
        MiArchPteWrite(&Process[Index],
                      Index >= (MiArmShortDescriptors ? 2048u : 2u) ? MiArchPteRead(&System[Index]) : 0);

    MiArchUnmapFrame(Process);
    MiArchUnmapFrame(System);
}


BOOLEAN
MiArmShortEncodePte(MI_PTE Value, BOOLEAN Root, PULONG Hardware)
{
    ULONG Physical, Entry;

    *Hardware = 0;
    if (!(Value & A32_VALID)) return TRUE;
    if ((Value & A32_FRAME_MASK) > 0xfffff000ULL) return FALSE;
    Physical = (ULONG)(Value & A32_FRAME_MASK);
    if (Root && (Value & A32_TYPE_PAGE))
    {
        *Hardware = Physical | 1u;
        return TRUE;
    }
    if (Root ? (Physical & 0xfffffu) != 0 : !(Value & A32_TYPE_PAGE)) return FALSE;

    Entry = 2u;
    if (Value & A32_XN) Entry |= 1u;
    if (Value & A32_AF) Entry |= 1u << 4;
    if (Value & A32_AP_USER) Entry |= 1u << 5;
    if (Value & A32_AP_READONLY) Entry |= 1u << 9;
    if (Value & A32_SH_INNER) Entry |= 1u << 10;
    if (Value & A32_NG) Entry |= 1u << 11;
    switch (Value & (7ULL << A32_ATTR_SHIFT))
    {
        case A32_ATTR_NORMAL:
            Entry |= 0x4cu;
            break;
        case A32_ATTR_DEVICE:
            Entry |= 0x404u;
            break;
        case A32_ATTR_NOCACHE:
        case A32_ATTR_WC:
            Entry |= 0x40u;
            break;
        default:
            return FALSE;
    }
    if (Root)
        Entry = 2u | (Entry & 12u) | ((Entry & 1u) << 4) | ((Entry & 0xff0u) << 6);
    *Hardware = Physical | Entry;
    return TRUE;
}

BOOLEAN
MiArmShortDecodePte(ULONG Hardware, BOOLEAN Root, PMI_PTE Value)
{
    ULONG Entry, Physical;
    MI_PTE Pte;

    *Value = 0;
    if (!(Hardware & 3u)) return TRUE;
    if (Root && (Hardware & 3u) == 1u)
    {
        if (Hardware & 0xc00u) return FALSE;
        *Value = (Hardware & 0xfffff000u) | A32_VALID | A32_TYPE_PAGE;
        return TRUE;
    }
    if (Root)
    {
        if ((Hardware & 3u) != 2u || (Hardware & 0x40000u)) return FALSE;
        Physical = Hardware & 0xfff00000u;
        Entry = 2u | (Hardware & 12u) | ((Hardware >> 4) & 1u) | ((Hardware >> 6) & 0xff0u);
    }
    else
    {
        if (!(Hardware & 2u)) return FALSE;
        Physical = Hardware & 0xfffff000u;
        Entry = Hardware;
    }
    Pte = Physical | A32_VALID | (Root ? 0 : A32_TYPE_PAGE);
    if (Entry & 1u) Pte |= A32_XN | A32_PXN;
    if (Entry & (1u << 4)) Pte |= A32_AF;
    if (Entry & (1u << 5)) Pte |= A32_AP_USER | A32_PXN;
    if (Entry & (1u << 9)) Pte |= A32_AP_READONLY;
    else Pte |= A32_SW_WRITE;
    if (Entry & (1u << 10)) Pte |= A32_SH_INNER;
    if (Entry & (1u << 11)) Pte |= A32_NG;
    switch (Entry & 0x1ccu)
    {
        case 0x4cu:
            Pte |= A32_ATTR_NORMAL;
            break;
        case 4u:
            Pte |= A32_ATTR_DEVICE;
            break;
        case 0x40u:
            Pte |= A32_ATTR_NOCACHE;
            break;
        default:
            return FALSE;
    }
    *Value = Pte;
    return TRUE;
}
