/*
 * PROJECT:     LiberNT ARM platform support
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Identify Exynos4210 firmware and preserve early boot output
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#pragma once
#include <drivers/acpi/acpi.h>

#define EXYNOS_BOOT_LOG_PHYSICAL 0x5e900000UL
#define EXYNOS_BOOT_LOG_VIRTUAL  0xfff40000UL
#define EXYNOS_BOOT_LOG_SIZE     0x80000UL
#define EXYNOS_BOOT_LOG_MAGIC    0x474c534fUL
#define EXYNOS_BOOT_GIC_DIST     0x10490000UL
#define EXYNOS_BOOT_GIC_CPU      0x10480000UL

typedef struct
{
    ULONG Magic;
    ULONG Head;
    ULONG Reserved[2];
    UCHAR Data[EXYNOS_BOOT_LOG_SIZE - 16];
} EXYNOS_BOOT_LOG;

static inline BOOLEAN
ExynosBootFind(PCONFIGURATION_COMPONENT_DATA Node, ULONG_PTR ClearBits, ULONG Depth)
{
    ULONG Count = 0;

    if (Depth > 32) return FALSE;
    while (Node && Count++ < 256)
    {
        PCM_PARTIAL_RESOURCE_LIST List;
        PACPI_BIOS_MULTI_NODE Bios;
        PDESCRIPTION_HEADER Header;
        PCSTR Identifier;

        Node = (PVOID)((ULONG_PTR)Node & ~ClearBits);
        Identifier = (PVOID)((ULONG_PTR)Node->ComponentEntry.Identifier & ~ClearBits);
        if (Identifier && Node->ComponentEntry.IdentifierLength >= sizeof("ACPI BIOS") &&
            !memcmp(Identifier, "ACPI BIOS", sizeof("ACPI BIOS")) &&
            Node->ComponentEntry.ConfigurationDataLength >=
                FIELD_OFFSET(CM_PARTIAL_RESOURCE_LIST, PartialDescriptors[1]) + sizeof(*Bios))
        {
            List = (PVOID)((ULONG_PTR)Node->ConfigurationData & ~ClearBits);
            if (List && List->Count == 1 && List->PartialDescriptors[0].Type == CmResourceTypeDeviceSpecific &&
                List->PartialDescriptors[0].u.DeviceSpecificData.DataSize >= sizeof(*Bios))
            {
                Bios = (PVOID)&List->PartialDescriptors[1];
                if (Bios->RsdtAddress.QuadPart && Bios->RsdtAddress.HighPart == 0)
                {
                    Header = (PVOID)(ULONG_PTR)Bios->RsdtAddress.LowPart;
                    if ((Header->Signature == RSDT_SIGNATURE || Header->Signature == XSDT_SIGNATURE) &&
                        Header->Length >= sizeof(*Header) &&
                        !memcmp(Header->OEMID, "SAMSNG", 6) &&
                        !memcmp(Header->OEMTableID, "EXY4210 ", 8))
                        return TRUE;
                }
            }
        }
        if (ExynosBootFind(Node->Child, ClearBits, Depth + 1)) return TRUE;
        Node = Node->Sibling;
    }
    return FALSE;
}

static inline BOOLEAN
ExynosBootLogMapped(VOID)
{
    ULONG Ttbcr, Ttbr, RootEntry, Leaf;
    volatile ULONG *Root, *Table;

    __asm__ __volatile__("mrc p15, 0, %0, c2, c0, 2" : "=r"(Ttbcr));
    if (Ttbcr != 0) return FALSE;
    __asm__ __volatile__("mrc p15, 0, %0, c2, c0, 0" : "=r"(Ttbr));
    Root = (PVOID)(ULONG_PTR)(Ttbr & 0xffffc000u);
    RootEntry = Root[EXYNOS_BOOT_LOG_VIRTUAL >> 20];
    if ((RootEntry & 3u) != 1u) return FALSE;
    Table = (PVOID)(ULONG_PTR)(RootEntry & 0xfffffc00u);
    Leaf = Table[(EXYNOS_BOOT_LOG_VIRTUAL >> 12) & 255u];
    return (Leaf & 2u) && (Leaf & 0xfffff000u) == EXYNOS_BOOT_LOG_PHYSICAL;
}

static inline VOID
ExynosBootWrite(ULONG_PTR Address, PCSTR Text, ULONG Length)
{
    volatile EXYNOS_BOOT_LOG *Ring = (PVOID)Address;
    ULONG Head, Index;
    ULONG_PTR Byte;

    if (Ring->Magic != EXYNOS_BOOT_LOG_MAGIC)
    {
        Ring->Head = 0;
        Ring->Magic = EXYNOS_BOOT_LOG_MAGIC;
    }
    Head = Ring->Head;
    for (Index = 0; Index < Length; ++Index)
    {
        Byte = (ULONG_PTR)&Ring->Data[Head++ % sizeof(Ring->Data)];
        *(volatile UCHAR *)Byte = Text[Index];
        __asm__ __volatile__("mcr p15, 0, %0, c7, c10, 1" :: "r"(Byte) : "memory");
    }
    __asm__ __volatile__("dsb sy" ::: "memory");
    Ring->Head = Head;
    __asm__ __volatile__("mcr p15, 0, %0, c7, c10, 1\n\tdsb sy" :: "r"(Address) : "memory");
}

static inline VOID
ExynosBootMessage(ULONG_PTR Address, PCSTR Text)
{
    ExynosBootWrite(Address, Text, (ULONG)strlen(Text));
}
