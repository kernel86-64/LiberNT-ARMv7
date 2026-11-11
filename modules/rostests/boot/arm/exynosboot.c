/*
 * PROJECT:     LiberNT boot tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Validate Exynos firmware identification and persistent boot logs
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <uefildr.h>
#include <arm/exynos4boot.h>

static EXYNOS_BOOT_LOG Ring;

BOOLEAN ArmTestExynosBoot(VOID)
{
    CONFIGURATION_COMPONENT_DATA Node = {0};
    DESCRIPTION_HEADER Header = {0};
    DECLSPEC_ALIGN(8) UCHAR Data[FIELD_OFFSET(CM_PARTIAL_RESOURCE_LIST, PartialDescriptors[1]) + sizeof(ACPI_BIOS_MULTI_NODE)];
    PCM_PARTIAL_RESOURCE_LIST List = (PVOID)Data;
    PACPI_BIOS_MULTI_NODE Bios = (PVOID)&List->PartialDescriptors[1];

    RtlZeroMemory(Data, sizeof(Data));
    Header.Signature = XSDT_SIGNATURE;
    Header.Length = sizeof(Header);
    RtlCopyMemory(Header.OEMID, "SAMSNG", 6);
    RtlCopyMemory(Header.OEMTableID, "EXY4210 ", 8);
    Node.ComponentEntry.Identifier = "ACPI BIOS";
    Node.ComponentEntry.IdentifierLength = sizeof("ACPI BIOS");
    Node.ComponentEntry.ConfigurationDataLength = sizeof(Data);
    Node.ConfigurationData = Data;
    List->Count = 1;
    List->PartialDescriptors[0].Type = CmResourceTypeDeviceSpecific;
    List->PartialDescriptors[0].u.DeviceSpecificData.DataSize = sizeof(*Bios);
    Bios->RsdtAddress.QuadPart = (ULONG_PTR)&Header;
    if (!ExynosBootFind(&Node, 0, 0)) return FALSE;
    Header.Signature = RSDT_SIGNATURE;
    if (!ExynosBootFind(&Node, 0, 0)) return FALSE;
    Header.OEMID[0] = 'X';
    if (ExynosBootFind(&Node, 0, 0)) return FALSE;
    Header.OEMID[0] = 'S';
    List->Count = 0;
    if (ExynosBootFind(&Node, 0, 0)) return FALSE;
    List->Count = 1;
    Node.ComponentEntry.Identifier = (PVOID)((ULONG_PTR)Node.ComponentEntry.Identifier | 0x80000000u);
    Node.ConfigurationData = (PVOID)((ULONG_PTR)Data | 0x80000000u);
    if (!ExynosBootFind((PVOID)((ULONG_PTR)&Node | 0x80000000u), 0x80000000u, 0)) return FALSE;
    ExynosBootMessage((ULONG_PTR)&Ring, "abc");
    if (Ring.Magic != EXYNOS_BOOT_LOG_MAGIC || Ring.Head != 3 || memcmp(Ring.Data, "abc", 3)) return FALSE;
    Ring.Head = sizeof(Ring.Data) - 1;
    ExynosBootMessage((ULONG_PTR)&Ring, "xy");
    return Ring.Head == sizeof(Ring.Data) + 1 && Ring.Data[sizeof(Ring.Data) - 1] == 'x' && Ring.Data[0] == 'y';
}
