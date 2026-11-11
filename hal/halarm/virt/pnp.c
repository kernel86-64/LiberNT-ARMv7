/*
 * PROJECT:     LiberNT HAL
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARM32 hardware-reduced ACPI root bus hooks
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <hal.h>
#include <halpnp.h>

PWCHAR HalHardwareIdString = L"ACPIARMv7";

PVOID NTAPI
HalpMapPhysicalMemory64(PHYSICAL_ADDRESS Address, PFN_COUNT PageCount)
{
    if (Address.QuadPart >= KSEG0_BASE ||
        PageCount > (KSEG0_BASE - (Address.LowPart & ~(PAGE_SIZE - 1))) / PAGE_SIZE)
        return NULL;
    return (PVOID)(ULONG_PTR)(Address.LowPart | KSEG0_BASE);
}

VOID NTAPI
HalpUnmapVirtualAddress(PVOID Address, PFN_COUNT PageCount)
{
    UNREFERENCED_PARAMETER(Address);
    UNREFERENCED_PARAMETER(PageCount);
}

PVOID NTAPI
HalpAcpiMapCachedTable(PHYSICAL_ADDRESS Address, PFN_COUNT PageCount)
{
    return HalpMapPhysicalMemory64(Address, PageCount);
}

NTSTATUS NTAPI
HalpQueryAcpiResourceRequirements(PIO_RESOURCE_REQUIREMENTS_LIST *Requirements)
{
    ULONG Size = FIELD_OFFSET(IO_RESOURCE_REQUIREMENTS_LIST, List[0].Descriptors);
    PIO_RESOURCE_REQUIREMENTS_LIST List = ExAllocatePoolWithTag(PagedPool, Size, TAG_HAL);
    if (!List) return STATUS_INSUFFICIENT_RESOURCES;
    RtlZeroMemory(List, Size);
    List->ListSize = Size;
    List->InterfaceType = PNPBus;
    List->BusNumber = MAXULONG;
    List->AlternativeLists = 1;
    List->List[0].Version = 1;
    List->List[0].Revision = 1;
    *Requirements = List;
    return STATUS_SUCCESS;
}

NTSTATUS NTAPI
HalpQueryBusInterface(PDEVICE_OBJECT DeviceObject, CONST GUID *InterfaceType,
                     USHORT Version, ULONG InterfaceBufferSize,
                     PINTERFACE Interface, PULONG Length)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(InterfaceType);
    UNREFERENCED_PARAMETER(Version);
    UNREFERENCED_PARAMETER(InterfaceBufferSize);
    UNREFERENCED_PARAMETER(Interface);
    if (Length) *Length = 0;
    return STATUS_NOT_SUPPORTED;
}
