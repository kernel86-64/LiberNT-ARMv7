/*
 * PROJECT:     LiberNT HAL
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Preserve normal-memory attributes for Exynos ACPI tables
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <hal.h>
#include <arm/exynos4boot.h>

typedef struct
{
    ULONGLONG First;
    ULONGLONG End;
} HALP_EXYNOS_CACHED_RANGE;

static HALP_EXYNOS_CACHED_RANGE *HalpExynosCachedRanges;
static ULONG HalpExynosCachedRangeCount;
static PPHYSICAL_MEMORY_RANGE HalpExynosRamRanges;

NTSTATUS NTAPI
HalpExynosInitializeCacheRanges(PLOADER_PARAMETER_BLOCK LoaderBlock)
{
    PLIST_ENTRY Link;
    PMEMORY_ALLOCATION_DESCRIPTOR Descriptor;
    ULONG Count = 0;
    ULONGLONG First, End;

    for (Link = LoaderBlock->MemoryDescriptorListHead.Flink;
         Link != &LoaderBlock->MemoryDescriptorListHead; Link = Link->Flink)
    {
        Descriptor = CONTAINING_RECORD(Link, MEMORY_ALLOCATION_DESCRIPTOR, ListEntry);
        if (Descriptor->MemoryType == LoaderSpecialMemory && Descriptor->PageCount) ++Count;
    }
    if (Count > MAXULONG / (2 * sizeof(*HalpExynosCachedRanges)))
        return STATUS_INSUFFICIENT_RESOURCES;
    HalpExynosRamRanges = MmGetPhysicalMemoryRanges();
    if (!HalpExynosRamRanges) return STATUS_INSUFFICIENT_RESOURCES;
    if (!Count) return STATUS_SUCCESS;
    HalpExynosCachedRanges = ExAllocatePoolWithTag(NonPagedPool,
                                                  Count * 2 * sizeof(*HalpExynosCachedRanges), TAG_HAL);
    if (!HalpExynosCachedRanges)
    {
        ExFreePool(HalpExynosRamRanges);
        HalpExynosRamRanges = NULL;
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    for (Link = LoaderBlock->MemoryDescriptorListHead.Flink;
         Link != &LoaderBlock->MemoryDescriptorListHead; Link = Link->Flink)
    {
        Descriptor = CONTAINING_RECORD(Link, MEMORY_ALLOCATION_DESCRIPTOR, ListEntry);
        if (Descriptor->MemoryType != LoaderSpecialMemory || !Descriptor->PageCount) continue;
        First = (ULONGLONG)Descriptor->BasePage << PAGE_SHIFT;
        End = First + ((ULONGLONG)Descriptor->PageCount << PAGE_SHIFT);
        if (First < EXYNOS_BOOT_LOG_PHYSICAL + EXYNOS_BOOT_LOG_SIZE && End > EXYNOS_BOOT_LOG_PHYSICAL)
        {
            if (First < EXYNOS_BOOT_LOG_PHYSICAL)
                HalpExynosCachedRanges[HalpExynosCachedRangeCount++] =
                    (HALP_EXYNOS_CACHED_RANGE){First, EXYNOS_BOOT_LOG_PHYSICAL};
            if (End > EXYNOS_BOOT_LOG_PHYSICAL + EXYNOS_BOOT_LOG_SIZE)
                HalpExynosCachedRanges[HalpExynosCachedRangeCount++] =
                    (HALP_EXYNOS_CACHED_RANGE){EXYNOS_BOOT_LOG_PHYSICAL + EXYNOS_BOOT_LOG_SIZE, End};
        }
        else
            HalpExynosCachedRanges[HalpExynosCachedRangeCount++] = (HALP_EXYNOS_CACHED_RANGE){First, End};
    }
    return STATUS_SUCCESS;
}

NTSTATUS NTAPI
HalpExynosMemoryCachingRequirements(PHYSICAL_ADDRESS Address, SIZE_T Length,
                                    MEMORY_CACHING_TYPE *CacheType)
{
    ULONGLONG First = Address.QuadPart, End;
    PPHYSICAL_MEMORY_RANGE Ranges;
    ULONG Index;

    if (!Length || First > MAXULONGLONG - Length) return STATUS_INVALID_PARAMETER;
    End = First + Length;
    *CacheType = MmNonCached;
    if (First < EXYNOS_BOOT_LOG_PHYSICAL + EXYNOS_BOOT_LOG_SIZE && End > EXYNOS_BOOT_LOG_PHYSICAL)
        return STATUS_SUCCESS;
    for (Index = 0; Index < HalpExynosCachedRangeCount; ++Index)
        if (First >= HalpExynosCachedRanges[Index].First && End <= HalpExynosCachedRanges[Index].End)
        {
            *CacheType = MmCached;
            return STATUS_SUCCESS;
        }
    Ranges = HalpExynosRamRanges;
    if (!Ranges) return STATUS_INSUFFICIENT_RESOURCES;
    for (Index = 0; Ranges[Index].NumberOfBytes.QuadPart; ++Index)
        if (First >= (ULONGLONG)Ranges[Index].BaseAddress.QuadPart &&
            End <= (ULONGLONG)Ranges[Index].BaseAddress.QuadPart + Ranges[Index].NumberOfBytes.QuadPart)
        {
            *CacheType = MmCached;
            break;
        }
    return STATUS_SUCCESS;
}
