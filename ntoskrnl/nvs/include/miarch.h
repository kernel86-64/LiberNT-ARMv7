/*
 * PROJECT:     LiberNT NT Virtual Memory Subsystem
 * FILE:        ntoskrnl/nvs/include/miarch.h
 * PURPOSE:     Memory manager architecture backend interface
 *
 * SPDX-FileCopyrightText: 2026 Ahmed ARIF
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#define MI_MAX_PAGING_LEVELS 5

typedef enum _MI_TLB_SCOPE
{
    MiTlbLocal = 0,
    MiTlbAllProcessors
} MI_TLB_SCOPE;

typedef struct _MI_LEVEL_DESCRIPTOR
{
    UCHAR Shift;
    UCHAR IndexBits;
    ULONG64 IndexMask;
    ULONG64 EntryCount;
} MI_LEVEL_DESCRIPTOR, *PMI_LEVEL_DESCRIPTOR;

typedef struct _MI_ARCH_DESCRIPTOR
{
    ULONG ArchId;
    PCSTR Name;

    UCHAR PagingLevels;
    UCHAR VirtualAddressBits;
    UCHAR PhysicalAddressBits;
    UCHAR PteBytes;

    ULONG PageSize;
    ULONG PageShift;
    ULONG64 LargePageSize;
    UCHAR LargePageLevel;

    MI_LEVEL_DESCRIPTOR Level[MI_MAX_PAGING_LEVELS];

    ULONG64 UserAddressStart;
    ULONG64 UserAddressEnd;
    ULONG64 SystemAddressStart;
    ULONG64 SystemAddressEnd;

    BOOLEAN HasRecursiveMap;
    BOOLEAN HasHardwareDirtyBit;
    BOOLEAN HasHardwareAccessedBit;
    BOOLEAN RequiresBreakBeforeMake;
    BOOLEAN RequiresExplicitIcacheSync;
    BOOLEAN HasCoherentDcache;
    BOOLEAN HasAsidTagging;
    BOOLEAN HasExecuteNever;
    BOOLEAN HasGlobalPages;
    BOOLEAN SupportsLargePages;

    const ULONG *ProtectionToPteMask;
    ULONG ProtectionToPteMaskCount;

    ULONG64 SystemReservedEnd;
    BOOLEAN SharedUserDataReadOnly;
    BOOLEAN SupportsEcCode;
    ULONG64 DirectFrameCount;
    BOOLEAN PrefersRegionRotation;
    ULONG TablePrefixBytes[MI_MAX_PAGING_LEVELS];
} MI_ARCH_DESCRIPTOR, *PMI_ARCH_DESCRIPTOR;

static __inline ULONG
MiArchRootPageCount(const MI_ARCH_DESCRIPTOR *Arch)
{
    ULONG64 Bytes;
    ULONG Pages;

    if (Arch == NULL || Arch->PagingLevels == 0 || Arch->PagingLevels > MI_MAX_PAGING_LEVELS ||
        Arch->PageSize == 0 || (Arch->PteBytes != 4 && Arch->PteBytes != 8) ||
        Arch->Level[Arch->PagingLevels - 1].EntryCount > (ULONG64)(~(ULONG)0) / Arch->PteBytes)
        return 0;
    Bytes = Arch->Level[Arch->PagingLevels - 1].EntryCount * Arch->PteBytes +
            Arch->TablePrefixBytes[Arch->PagingLevels - 1];
    if (Bytes > (ULONG64)(~(ULONG)0) - Arch->PageSize + 1) return 0;
    Pages = (ULONG)((Bytes + Arch->PageSize - 1) / Arch->PageSize);
    if (!Pages) return 0;
    ULONG Rounded = 1;
    while (Rounded < Pages)
    {
        if (Rounded > (~(ULONG)0) / 2) return 0;
        Rounded <<= 1;
    }
    return Rounded;
}

PVOID MiArchMapFrame(ULONG64 Frame);

static __inline PVOID
MiArchMapTableFrame(const MI_ARCH_DESCRIPTOR *Arch, ULONG64 Frame, ULONG Level)
{
    PUCHAR Mapping = MiArchMapFrame(Frame);
    return Mapping ? Mapping + Arch->TablePrefixBytes[Level] : NULL;
}

static __inline ULONG64
MiArchTableSlotAddress(const MI_ARCH_DESCRIPTOR *Arch, ULONG64 Frame, ULONG Index, ULONG Level)
{
    return (Frame << Arch->PageShift) + Arch->TablePrefixBytes[Level] + (ULONG64)Index * Arch->PteBytes;
}

typedef struct _MI_PHYSICAL_RANGE
{
    ULONG64 BasePage;
    ULONG64 PageCount;
    ULONG Flags;
} MI_PHYSICAL_RANGE, *PMI_PHYSICAL_RANGE;

const MI_ARCH_DESCRIPTOR *MiArchDescribe(VOID);

VOID MiArchInvalidateTlbSingle(_In_ PVOID VirtualAddress, _In_ MI_TLB_SCOPE Scope);
VOID MiArchInvalidateTlbRange(_In_ PVOID BaseAddress, _In_ SIZE_T Size, _In_ MI_TLB_SCOPE Scope);
VOID MiArchInvalidateTlbAll(_In_ MI_TLB_SCOPE Scope);

VOID MiArchSyncInstructionCache(_In_ PVOID BaseAddress, _In_ SIZE_T Length);
