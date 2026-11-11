/*
 * PROJECT:     LiberNT NT Virtual Memory Subsystem
 * FILE:        ntoskrnl/nvs/arch/arm/descriptor.c
 * PURPOSE:     ARM memory manager architecture descriptor
 *
 * SPDX-FileCopyrightText: 2026 Ahmed ARIF
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <nvs/include/mienv.h>
#include "archdef.h"
#include "shortpte.h"
#include <nvs/include/miarchcheck.h>

static const MI_ARCH_DESCRIPTOR MiArchDescriptor =
{
    MI_ARCH_ID_VALUE,
    MI_ARCH_NAME,
    MI_ARCH_PAGING_LEVELS,
    MI_ARCH_VA_BITS,
    MI_ARCH_PA_BITS,
    MI_ARCH_PTE_BYTES,
    PAGE_SIZE,
    MI_ARCH_PAGE_SHIFT,
    (2ULL * 1024 * 1024),
    MI_ARCH_LARGE_LEVEL,
    {
        { MI_ARCH_L0_SHIFT, MI_ARCH_L0_BITS, (1ULL << MI_ARCH_L0_BITS) - 1, 1ULL << MI_ARCH_L0_BITS },
        { MI_ARCH_L1_SHIFT, MI_ARCH_L1_BITS, (1ULL << MI_ARCH_L1_BITS) - 1, 1ULL << MI_ARCH_L1_BITS },
        { MI_ARCH_L2_SHIFT, MI_ARCH_L2_BITS, (1ULL << MI_ARCH_L2_BITS) - 1, 1ULL << MI_ARCH_L2_BITS },
        { MI_ARCH_L3_SHIFT, MI_ARCH_L3_BITS, (1ULL << MI_ARCH_L3_BITS) - 1, 1ULL << MI_ARCH_L3_BITS },
        { MI_ARCH_L4_SHIFT, MI_ARCH_L4_BITS, (1ULL << MI_ARCH_L4_BITS) - 1, 1ULL << MI_ARCH_L4_BITS },
    },
    0x00010000ULL,
    0x7FFEFFFFULL,
    0x80000000ULL,
    0xFFFFFFFFULL,
    FALSE,
    FALSE,
    FALSE,
    TRUE,
    TRUE,
    TRUE,
    TRUE,
    TRUE,
    TRUE,
    TRUE,
    NULL,
    0,
    0,
    TRUE,
    FALSE,
    0x80000,
    FALSE
};

const MI_ARCH_DESCRIPTOR *
MiArchDescribe(VOID)
{
    static MI_ARCH_DESCRIPTOR Short;
    if (!MiArmShortDescriptors) return &MiArchDescriptor;
    if (!Short.PagingLevels)
    {
        Short = MiArchDescriptor;
        Short.PagingLevels = 2;
        Short.PhysicalAddressBits = 32;
        Short.LargePageSize = 0x100000;
        Short.Level[0].Shift = 12;
        Short.Level[0].IndexBits = 8;
        Short.Level[0].IndexMask = 255;
        Short.Level[0].EntryCount = 256;
        Short.Level[1].Shift = 20;
        Short.Level[1].IndexBits = 12;
        Short.Level[1].IndexMask = 4095;
        Short.Level[1].EntryCount = 4096;
        RtlZeroMemory(&Short.Level[2], 3 * sizeof(Short.Level[0]));
        Short.TablePrefixBytes[0] = 1024;
        Short.TablePrefixBytes[1] = 16384;
    }
    return &Short;
}
