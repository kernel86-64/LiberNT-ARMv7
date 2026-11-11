/*
 * PROJECT:     LiberNT HAL
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Coherent identity-mapped bus-master DMA for ARM32 platforms
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <hal.h>
#include <acpiioct.h>
#include <haldma_coherent.h>

#define ARM_DMA_TAG 'mDrA'

typedef struct _ARM_DMA_ADAPTER
{
    DMA_ADAPTER Header;
    ULONG MapRegisters;
} ARM_DMA_ADAPTER;

static NTSTATUS
HalpArmDmaEvaluate(PDEVICE_OBJECT Device, PCSTR Name, PACPI_EVAL_OUTPUT_BUFFER Output, ULONG Length)
{
    ACPI_EVAL_INPUT_BUFFER Input = {0};
    IO_STATUS_BLOCK Iosb;
    KEVENT Event;
    PIRP Irp;
    NTSTATUS Status;

    Input.Signature = ACPI_EVAL_INPUT_BUFFER_SIGNATURE;
    RtlCopyMemory(Input.MethodName, Name, sizeof(Input.MethodName));
    KeInitializeEvent(&Event, NotificationEvent, FALSE);
    Irp = IoBuildDeviceIoControlRequest(IOCTL_ACPI_EVAL_METHOD, Device,
                                        &Input, sizeof(Input), Output, Length, FALSE, &Event, &Iosb);
    if (!Irp) return STATUS_INSUFFICIENT_RESOURCES;
    Status = IoCallDriver(Device, Irp);
    if (Status == STATUS_PENDING)
    {
        KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
        Status = Iosb.Status;
    }
    return Status;
}

static BOOLEAN
HalpArmDmaCoherent(PDEVICE_OBJECT Device)
{
    union
    {
        ULONGLONG Alignment;
        UCHAR Bytes[sizeof(ACPI_EVAL_OUTPUT_BUFFER) + sizeof(ULONGLONG)];
    } Storage = {0};
    PACPI_EVAL_OUTPUT_BUFFER Output = (PVOID)Storage.Bytes;
    ULONGLONG Value = 0;
    NTSTATUS Status;
    ULONG Length;

    Status = HalpArmDmaEvaluate(Device, "_CCA", Output, sizeof(Storage.Bytes));
    if (!NT_SUCCESS(Status) || Output->Signature != ACPI_EVAL_OUTPUT_BUFFER_SIGNATURE ||
        Output->Count != 1 || Output->Length > sizeof(Storage.Bytes) ||
        Output->Argument[0].Type != ACPI_METHOD_ARGUMENT_INTEGER)
        return FALSE;
    Length = Output->Argument[0].DataLength;
    if ((Length != sizeof(ULONG) && Length != sizeof(Value)) ||
        Output->Length < FIELD_OFFSET(ACPI_EVAL_OUTPUT_BUFFER, Argument[0].Data) + Length)
        return FALSE;
    RtlCopyMemory(&Value, Output->Argument[0].Data, Length);
    if (Value != 1) return FALSE;
    Status = HalpArmDmaEvaluate(Device, "_DMA", Output, sizeof(Storage.Bytes));
    return Status == STATUS_OBJECT_NAME_NOT_FOUND;
}

static VOID NTAPI
HalpArmPutDmaAdapter(PDMA_ADAPTER Adapter)
{
    ExFreePoolWithTag(Adapter, ARM_DMA_TAG);
}

static PVOID NTAPI
HalpArmAllocateCommonBuffer(PDMA_ADAPTER Adapter, ULONG Length,
                            PPHYSICAL_ADDRESS LogicalAddress, BOOLEAN CacheEnabled)
{
    PHYSICAL_ADDRESS Low = {{0}}, High, Boundary = {{0}};
    PVOID Buffer;

    UNREFERENCED_PARAMETER(Adapter);
    UNREFERENCED_PARAMETER(CacheEnabled);
    if (!Length || !LogicalAddress) return NULL;
    High.QuadPart = MAXULONG;
    Buffer = MmAllocateContiguousMemorySpecifyCache(Length, Low, High, Boundary,
                                                    MmCached);
    if (Buffer)
    {
        *LogicalAddress = MmGetPhysicalAddress(Buffer);
        KeMemoryBarrier();
    }
    return Buffer;
}

static VOID NTAPI
HalpArmFreeCommonBuffer(PDMA_ADAPTER Adapter, ULONG Length, PHYSICAL_ADDRESS LogicalAddress,
                        PVOID VirtualAddress, BOOLEAN CacheEnabled)
{
    UNREFERENCED_PARAMETER(Adapter);
    UNREFERENCED_PARAMETER(Length);
    UNREFERENCED_PARAMETER(LogicalAddress);
    UNREFERENCED_PARAMETER(CacheEnabled);
    MmFreeContiguousMemory(VirtualAddress);
}

static NTSTATUS NTAPI
HalpArmAllocateDmaChannel(PDMA_ADAPTER Adapter, PDEVICE_OBJECT Device, ULONG Registers,
                          PDRIVER_CONTROL Routine, PVOID Context)
{
    KIRQL OldIrql = KeGetCurrentIrql();

    if (!Device || !Routine || OldIrql > DISPATCH_LEVEL ||
        Registers > ((ARM_DMA_ADAPTER *)Adapter)->MapRegisters)
        return STATUS_INVALID_PARAMETER;
    if (OldIrql < DISPATCH_LEVEL) KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);
    Routine(Device, Device->CurrentIrp, NULL, Context);
    if (OldIrql < DISPATCH_LEVEL) KeLowerIrql(OldIrql);
    return STATUS_SUCCESS;
}

static BOOLEAN NTAPI
HalpArmFlushDma(PDMA_ADAPTER Adapter, PMDL Mdl, PVOID Registers,
                PVOID CurrentVa, ULONG Length, BOOLEAN WriteToDevice)
{
    UNREFERENCED_PARAMETER(Adapter);
    UNREFERENCED_PARAMETER(Mdl);
    UNREFERENCED_PARAMETER(Registers);
    UNREFERENCED_PARAMETER(CurrentVa);
    UNREFERENCED_PARAMETER(Length);
    UNREFERENCED_PARAMETER(WriteToDevice);
    KeMemoryBarrier();
    return TRUE;
}

static VOID NTAPI HalpArmFreeDmaChannel(PDMA_ADAPTER Adapter)
{
    UNREFERENCED_PARAMETER(Adapter);
}

static VOID NTAPI HalpArmFreeMapRegisters(PDMA_ADAPTER Adapter, PVOID Registers, ULONG Count)
{
    UNREFERENCED_PARAMETER(Adapter);
    UNREFERENCED_PARAMETER(Registers);
    UNREFERENCED_PARAMETER(Count);
}

static PHYSICAL_ADDRESS NTAPI
HalpArmMapTransfer(PDMA_ADAPTER Adapter, PMDL Mdl, PVOID Registers,
                   PVOID CurrentVa, PULONG Length, BOOLEAN WriteToDevice)
{
    PHYSICAL_ADDRESS Address = {{0}};
    ULONG_PTR Offset;
    ULONGLONG Byte;
    ULONG Page;

    UNREFERENCED_PARAMETER(Adapter);
    UNREFERENCED_PARAMETER(Registers);
    UNREFERENCED_PARAMETER(WriteToDevice);
    if (!Length) return Address;
    if (!Mdl || !*Length || (ULONG_PTR)CurrentVa < (ULONG_PTR)MmGetMdlVirtualAddress(Mdl))
        goto Fail;
    Offset = (ULONG_PTR)CurrentVa - (ULONG_PTR)MmGetMdlVirtualAddress(Mdl);
    if (Offset >= Mdl->ByteCount) goto Fail;
    *Length = min(*Length, Mdl->ByteCount - (ULONG)Offset);
    Byte = (ULONGLONG)Offset + Mdl->ByteOffset;
    Page = (ULONG)(Byte >> PAGE_SHIFT);
    Address.QuadPart = ((ULONGLONG)MmGetMdlPfnArray(Mdl)[Page] << PAGE_SHIFT) +
                       (Byte & (PAGE_SIZE - 1));
    *Length = min(*Length, PAGE_SIZE - (ULONG)(Byte & (PAGE_SIZE - 1)));
    if ((ULONGLONG)Address.QuadPart > MAXULONG ||
        *Length - 1 > MAXULONG - (ULONGLONG)Address.QuadPart)
        goto Fail;
    KeMemoryBarrier();
    return Address;
Fail:
    *Length = 0;
    Address.QuadPart = 0;
    return Address;
}

static ULONG NTAPI HalpArmDmaAlignment(PDMA_ADAPTER Adapter)
{
    UNREFERENCED_PARAMETER(Adapter);
    return 1;
}

static ULONG NTAPI HalpArmReadDmaCounter(PDMA_ADAPTER Adapter)
{
    UNREFERENCED_PARAMETER(Adapter);
    return 0;
}

static DMA_OPERATIONS HalpArmDmaOperations =
{
    .Size = FIELD_OFFSET(DMA_OPERATIONS, CalculateScatterGatherList),
    .PutDmaAdapter = HalpArmPutDmaAdapter,
    .AllocateCommonBuffer = HalpArmAllocateCommonBuffer,
    .FreeCommonBuffer = HalpArmFreeCommonBuffer,
    .AllocateAdapterChannel = HalpArmAllocateDmaChannel,
    .FlushAdapterBuffers = HalpArmFlushDma,
    .FreeAdapterChannel = HalpArmFreeDmaChannel,
    .FreeMapRegisters = HalpArmFreeMapRegisters,
    .MapTransfer = HalpArmMapTransfer,
    .GetDmaAlignment = HalpArmDmaAlignment,
    .ReadDmaCounter = HalpArmReadDmaCounter,
    .GetScatterGatherList = HalpCoherentGetScatterGatherList,
    .PutScatterGatherList = HalpCoherentPutScatterGatherList
};

PDMA_ADAPTER NTAPI
HalpArmGetDmaAdapter(PVOID Device, PDEVICE_DESCRIPTION Description, PULONG Registers)
{
    ARM_DMA_ADAPTER *Adapter;
    PPHYSICAL_MEMORY_RANGE Ranges, Range;
    BOOLEAN Fits = TRUE;

    if (Registers) *Registers = 0;
    if (!Device || !Registers || !Description || !Description->Master ||
        Description->Version > DEVICE_DESCRIPTION_VERSION1 || !Description->MaximumLength ||
        (!Description->Dma32BitAddresses && !Description->Dma64BitAddresses) ||
        KeGetCurrentIrql() != PASSIVE_LEVEL || !HalpArmDmaCoherent(Device))
        return NULL;
    Ranges = MmGetPhysicalMemoryRanges();
    if (!Ranges) return NULL;
    for (Range = Ranges; Range->NumberOfBytes.QuadPart; ++Range)
    {
        if (Range->BaseAddress.QuadPart < 0 || Range->NumberOfBytes.QuadPart < 0 ||
            (ULONGLONG)Range->BaseAddress.QuadPart > MAXULONG ||
            (ULONGLONG)Range->NumberOfBytes.QuadPart - 1 > MAXULONG - (ULONGLONG)Range->BaseAddress.QuadPart)
            Fits = FALSE;
    }
    ExFreePool(Ranges);
    if (!Fits) return NULL;
    Adapter = ExAllocatePoolWithTag(NonPagedPool, sizeof(*Adapter), ARM_DMA_TAG);
    if (!Adapter) return NULL;
    Adapter->Header.Version = 1;
    Adapter->Header.Size = sizeof(DMA_ADAPTER);
    Adapter->Header.DmaOperations = &HalpArmDmaOperations;
    Adapter->MapRegisters = (Description->MaximumLength >> PAGE_SHIFT) + 2;
    *Registers = Adapter->MapRegisters;
    return &Adapter->Header;
}
