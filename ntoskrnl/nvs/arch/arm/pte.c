/*
 * PROJECT:     LiberNT NT Virtual Memory Subsystem
 * FILE:        ntoskrnl/nvs/arch/arm/pte.c
 * PURPOSE:     ARMv7 LPAE page-table entry operations
 *
 * SPDX-FileCopyrightText: 2026 Ahmed ARIF
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <nvs/include/mienv.h>

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

    if (Frame >= (1ULL << 28) || !MI_PROT_IS_ACCESSIBLE(Protection) || MI_PROT_IS_GUARD(Protection))
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

    return Pte;
}

MI_PTE
MiArchPteMakeTable(
    _In_ ULONG64 Frame,
    _In_ ULONG Flags)
{
    UNREFERENCED_PARAMETER(Flags);
    if (Frame >= (1ULL << 28))
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
    if (Frame & 0x1FF)
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
    PMI_PTE System = MiArchMapFrame(SystemRootFrame);
    PMI_PTE Process = MiArchMapFrame(ProcessRootFrame);
    ULONG Index;

    for (Index = 0; Index < PAGE_SIZE / sizeof(MI_PTE); Index++)
        MiArchPteWrite(&Process[Index], Index == 2 || Index == 3 ? MiArchPteRead(&System[Index]) : 0);

    MiArchUnmapFrame(Process);
    MiArchUnmapFrame(System);
}
