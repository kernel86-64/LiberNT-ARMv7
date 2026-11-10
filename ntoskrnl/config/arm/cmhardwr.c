/*
 * PROJECT:         ReactOS Kernel
 * LICENSE:         BSD - See COPYING.ARM in the top level directory
 * FILE:            ntoskrnl/config/arm/cmhardwr.c
 * PURPOSE:         Configuration Manager - ARM Specific Code
 * PROGRAMMERS:     ReactOS Portable Systems Group
 */

/* INCLUDES ******************************************************************/

#include "ntoskrnl.h"
#include <internal/arm/processor.h>
#define NDEBUG
#include "debug.h"

/* GLOBALS *******************************************************************/

/* FUNCTIONS *****************************************************************/

static NTSTATUS CmpArmSetProcessorIdentity(HANDLE Key, ULONG Midr)
{
    WCHAR Buffer[64];
    UNICODE_STRING Name;
    const CHAR *ProcessorName = KiArmProcessorName(Midr);
    NTSTATUS Status;

    if (ProcessorName)
        Status = RtlStringCbPrintfW(Buffer, sizeof(Buffer), L"%hs", ProcessorName);
    else
        Status = RtlStringCbPrintfW(Buffer, sizeof(Buffer), L"ARM Processor (MIDR %08lx)", Midr);
    if (!NT_SUCCESS(Status)) return Status;
    RtlInitUnicodeString(&Name, L"ProcessorNameString");
    Status = NtSetValueKey(Key, &Name, 0, REG_SZ, Buffer, (ULONG)(wcslen(Buffer) + 1) * sizeof(WCHAR));
    if (!NT_SUCCESS(Status)) return Status;
    if ((Midr >> 24) == 0x41)
        Status = RtlStringCbCopyW(Buffer, sizeof(Buffer), L"ARM Limited");
    else
        Status = RtlStringCbPrintfW(Buffer, sizeof(Buffer), L"ARM implementer 0x%02lx", Midr >> 24);
    if (!NT_SUCCESS(Status)) return Status;
    RtlInitUnicodeString(&Name, L"VendorIdentifier");
    return NtSetValueKey(Key, &Name, 0, REG_SZ, Buffer, (ULONG)(wcslen(Buffer) + 1) * sizeof(WCHAR));
}

NTSTATUS
NTAPI
CmpInitializeMachineDependentConfiguration(IN PLOADER_PARAMETER_BLOCK LoaderBlock)
{
    USHORT IndexTable[MaximumType + 1] = {0};
    CONFIGURATION_COMPONENT_DATA ConfigData;
    OBJECT_ATTRIBUTES Attributes;
    UNICODE_STRING Name;
    HANDLE System, Key;
    ULONG Processor, Midr, Disposition;
    CHAR Identifier[64];
    NTSTATUS Status;

    UNREFERENCED_PARAMETER(LoaderBlock);
    RtlInitUnicodeString(&Name, L"\\Registry\\Machine\\Hardware\\Description\\System");
    InitializeObjectAttributes(&Attributes, &Name, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
    Status = NtOpenKey(&System, KEY_READ | KEY_WRITE, &Attributes);
    if (!NT_SUCCESS(Status)) return Status;
    RtlInitUnicodeString(&Name, L"CentralProcessor");
    InitializeObjectAttributes(&Attributes, &Name, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, System, NULL);
    Status = NtCreateKey(&Key, KEY_READ | KEY_WRITE, &Attributes, 0, NULL, REG_OPTION_NON_VOLATILE, &Disposition);
    if (!NT_SUCCESS(Status))
    {
        NtClose(System);
        return Status;
    }
    NtClose(Key);
    if (Disposition != REG_CREATED_NEW_KEY)
    {
        NtClose(System);
        return STATUS_SUCCESS;
    }
    CmpConfigurationData = ExAllocatePoolWithTag(PagedPool, CmpConfigurationAreaSize, TAG_CM);
    if (!CmpConfigurationData)
    {
        NtClose(System);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    for (Processor = 0; Processor < (ULONG)KeNumberProcessors; ++Processor)
    {
        Midr = KiProcessorBlock[Processor]->ProcessorState.ArchState.Cp15_Cr0_CpuId;
        Status = RtlStringCbPrintfA(Identifier, sizeof(Identifier), "ARM MIDR %08lx", Midr);
        if (!NT_SUCCESS(Status)) break;
        RtlZeroMemory(&ConfigData, sizeof(ConfigData));
        ConfigData.ComponentEntry.Class = ProcessorClass;
        ConfigData.ComponentEntry.Type = CentralProcessor;
        ConfigData.ComponentEntry.Key = Processor;
        ConfigData.ComponentEntry.AffinityMask = AFFINITY_MASK(Processor);
        ConfigData.ComponentEntry.Identifier = Identifier;
        ConfigData.ComponentEntry.IdentifierLength = (ULONG)strlen(Identifier) + 1;
        Status = CmpInitializeRegistryNode(&ConfigData, System, &Key,
                                          InterfaceTypeUndefined, MAXULONG, IndexTable);
        if (!NT_SUCCESS(Status)) break;
        Status = CmpArmSetProcessorIdentity(Key, Midr);
        NtClose(Key);
        if (!NT_SUCCESS(Status)) break;
    }
    ExFreePoolWithTag(CmpConfigurationData, TAG_CM);
    CmpConfigurationData = NULL;
    NtClose(System);
    return Status;
}
