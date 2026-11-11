/*
 * PROJECT:     LiberNT boot tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Exercise Exynos firmware RAM, MMIO and OS-log cache attributes
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <hal.h>

static DECLSPEC_ALIGN(8) UCHAR CachePool[128];
static PHYSICAL_MEMORY_RANGE CacheRam[2];

static PVOID CacheAllocate(POOL_TYPE Type, SIZE_T Bytes, ULONG Tag)
{
    UNREFERENCED_PARAMETER(Type);
    UNREFERENCED_PARAMETER(Tag);
    return Bytes <= sizeof(CachePool) ? CachePool : NULL;
}

static VOID CacheFree(PVOID Address)
{
    UNREFERENCED_PARAMETER(Address);
}

static PPHYSICAL_MEMORY_RANGE CacheGetRam(VOID)
{
    return CacheRam;
}

#undef ExAllocatePoolWithTag
#undef ExFreePool
#define ExAllocatePoolWithTag CacheAllocate
#define ExFreePool CacheFree
#define MmGetPhysicalMemoryRanges CacheGetRam
#include "../../../../hal/halarm/exynos4210/cache.c"

static BOOLEAN CacheCheck(ULONGLONG Physical, SIZE_T Length, MEMORY_CACHING_TYPE Expected)
{
    PHYSICAL_ADDRESS Address;
    MEMORY_CACHING_TYPE Cache = MmMaximumCacheType;
    Address.QuadPart = Physical;
    return NT_SUCCESS(HalpExynosMemoryCachingRequirements(Address, Length, &Cache)) && Cache == Expected;
}

BOOLEAN ArmTestExynosCache(VOID)
{
    LOADER_PARAMETER_BLOCK Loader = {0};
    MEMORY_ALLOCATION_DESCRIPTOR Acpi = {0}, Combined = {0};
    PHYSICAL_ADDRESS Address;
    MEMORY_CACHING_TYPE Cache;

    InitializeListHead(&Loader.MemoryDescriptorListHead);
    Acpi.MemoryType = LoaderSpecialMemory;
    Acpi.BasePage = 0x7b74a;
    Acpi.PageCount = 1;
    Combined.MemoryType = LoaderSpecialMemory;
    Combined.BasePage = 0x5e8ff;
    Combined.PageCount = 130;
    InsertTailList(&Loader.MemoryDescriptorListHead, &Acpi.ListEntry);
    InsertTailList(&Loader.MemoryDescriptorListHead, &Combined.ListEntry);
    CacheRam[0].BaseAddress.QuadPart = 0x40000000;
    CacheRam[0].NumberOfBytes.QuadPart = 0x10000000;
    if (!NT_SUCCESS(HalpExynosInitializeCacheRanges(&Loader))) return FALSE;
    if (!CacheCheck(0x7b74af10, 36, MmCached) ||
        !CacheCheck(0x7b74afff, 1, MmCached) ||
        !CacheCheck(0x7b74afff, 2, MmNonCached) ||
        !CacheCheck(0x40010000, 4096, MmCached) ||
        !CacheCheck(0x10050000, 4096, MmNonCached) ||
        !CacheCheck(0x5e900000, 4096, MmNonCached) ||
        !CacheCheck(0x5e97ffff, 1, MmNonCached) ||
        !CacheCheck(0x5e8fffff, 1, MmCached) ||
        !CacheCheck(0x5e980000, 1, MmCached)) return FALSE;
    Address.QuadPart = MAXULONGLONG - 1;
    return HalpExynosMemoryCachingRequirements(Address, 4, &Cache) == STATUS_INVALID_PARAMETER;
}
