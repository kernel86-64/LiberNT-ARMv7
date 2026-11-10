/*
 * PROJECT:     LiberNT VirtIO Input
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     VirtIO MMIO HID minidriver
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */
#include "virtinput.h"

static volatile LONG ViClaims;

static VI_DEVICE *ViExtension(PDEVICE_OBJECT Device)
{
    return ((PHID_DEVICE_EXTENSION)Device->DeviceExtension)->MiniDeviceExtension;
}

static NTSTATUS ViComplete(VI_DEVICE *Device, PIRP Irp, NTSTATUS Status, ULONG Length)
{
    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = Length;
    IoReleaseRemoveLock(&Device->RemoveLock, Irp);
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

static NTSTATUS NTAPI ViInsert(PIO_CSQ Csq, PIRP Irp, PVOID Context)
{
    VI_DEVICE *Device = CONTAINING_RECORD(Csq, VI_DEVICE, Csq);
    UNREFERENCED_PARAMETER(Context);
    if (!Device->Started) return STATUS_DEVICE_NOT_READY;
    InsertTailList(&Device->Reads, &Irp->Tail.Overlay.ListEntry);
    KeInsertQueueDpc(&Device->Dpc, NULL, NULL);
    return STATUS_SUCCESS;
}
static VOID NTAPI ViRemove(PIO_CSQ Csq, PIRP Irp)
{
    UNREFERENCED_PARAMETER(Csq);
    RemoveEntryList(&Irp->Tail.Overlay.ListEntry);
}
static PIRP NTAPI ViPeek(PIO_CSQ Csq, PIRP Irp, PVOID Context)
{
    VI_DEVICE *Device = CONTAINING_RECORD(Csq, VI_DEVICE, Csq);
    PLIST_ENTRY Entry = Irp ? Irp->Tail.Overlay.ListEntry.Flink : Device->Reads.Flink;
    UNREFERENCED_PARAMETER(Context);
    return Entry == &Device->Reads ? NULL : CONTAINING_RECORD(Entry, IRP, Tail.Overlay.ListEntry);
}
static VOID NTAPI ViLock(PIO_CSQ Csq, PKIRQL Irql)
{
    KeAcquireSpinLock(&CONTAINING_RECORD(Csq, VI_DEVICE, Csq)->Lock, Irql);
}
static VOID NTAPI ViUnlock(PIO_CSQ Csq, KIRQL Irql)
{
    KeReleaseSpinLock(&CONTAINING_RECORD(Csq, VI_DEVICE, Csq)->Lock, Irql);
}
static VOID NTAPI ViCancel(PIO_CSQ Csq, PIRP Irp)
{
    ViComplete(CONTAINING_RECORD(Csq, VI_DEVICE, Csq), Irp, STATUS_CANCELLED, 0);
}

static UCHAR ViConfig(VI_DEVICE *Device, UCHAR Select, UCHAR SubSelect, PVOID Buffer, ULONG Length)
{
    UCHAR Size;
    WRITE_REGISTER_UCHAR(Device->Registers + 0x100, Select);
    WRITE_REGISTER_UCHAR(Device->Registers + 0x101, SubSelect);
    KeMemoryBarrier();
    Size = READ_REGISTER_UCHAR(Device->Registers + 0x102);
    RtlZeroMemory(Buffer, Length);
    READ_REGISTER_BUFFER_UCHAR(Device->Registers + 0x108, Buffer, min((ULONG)Size, Length));
    return Size;
}

static VOID ViPublish(VI_QUEUE *Queue, USHORT Id)
{
    Queue->Available[2 + Queue->NextAvailable % Queue->Size] = Id;
    ++Queue->NextAvailable;
    KeMemoryBarrier();
    Queue->Available[1] = Queue->NextAvailable;
    KeMemoryBarrier();
}

static VOID ViSetupQueue(VI_DEVICE *Device, VI_QUEUE *Queue, ULONG Number,
                         ULONG Descriptors, ULONG Available, ULONG Used, ULONG Events, USHORT Size)
{
    ULONG Index;
    ULONGLONG Address = Device->DmaAddress.QuadPart;
    Queue->Descriptors = (VI_DESC *)((PUCHAR)Device->Dma + Descriptors);
    Queue->Available = (PUSHORT)((PUCHAR)Device->Dma + Available);
    Queue->Used = (PUSHORT)((PUCHAR)Device->Dma + Used);
    Queue->Events = (VI_EVENT *)((PUCHAR)Device->Dma + Events);
    Queue->Size = Size;
    Queue->LastUsed = Queue->NextAvailable = 0;
    for (Index = 0; Index < Size; ++Index)
    {
        Queue->Descriptors[Index].Address = Address + Events + Index * sizeof(VI_EVENT);
        Queue->Descriptors[Index].Length = sizeof(VI_EVENT);
        Queue->Descriptors[Index].Flags = Number == 0 ? 2 : 0;
    }
    ViWrite(Device, 0x30, Number);
    ViWrite(Device, 0x38, Size);
    ViWrite(Device, 0x80, (ULONG)(Address + Descriptors));
    ViWrite(Device, 0x84, (ULONG)((Address + Descriptors) >> 32));
    ViWrite(Device, 0x90, (ULONG)(Address + Available));
    ViWrite(Device, 0x94, (ULONG)((Address + Available) >> 32));
    ViWrite(Device, 0xa0, (ULONG)(Address + Used));
    ViWrite(Device, 0xa4, (ULONG)((Address + Used) >> 32));
    KeMemoryBarrier();
    ViWrite(Device, 0x44, 1);
    if (!Number)
        for (Index = 0; Index < Size; ++Index) ViPublish(Queue, (USHORT)Index);
}

static VOID ViLeds(VI_DEVICE *Device)
{
    VI_QUEUE *Queue = &Device->StatusQueue;
    VI_USED Entry;
    ULONG Id, Bit;
    UCHAR Changed;
    while (Queue->LastUsed != Queue->Used[1])
    {
        KeMemoryBarrier();
        Entry = ((volatile VI_USED *)(Queue->Used + 2))[Queue->LastUsed++ % Queue->Size];
        if (Entry.Id < VI_STATUS) Device->StatusBusy &= ~(1u << Entry.Id);
    }
    Changed = (Device->DesiredLeds ^ Device->SentLeds) & Device->LedMask;
    for (Bit = 0; Bit < 5; ++Bit)
    {
        if (!(Changed & (1u << Bit))) continue;
        for (Id = 0; Id < VI_STATUS; ++Id)
            if (!(Device->StatusBusy & (1u << Id))) break;
        if (Id == VI_STATUS) break;
        Device->StatusBusy |= 1u << Id;
        Queue->Events[Id].Type = 17;
        Queue->Events[Id].Code = (USHORT)Bit;
        Queue->Events[Id].Value = !!(Device->DesiredLeds & (1u << Bit));
        Device->SentLeds ^= 1u << Bit;
        ViPublish(Queue, (USHORT)Id);
        ViWrite(Device, 0x50, 1);
    }
}

static VOID NTAPI ViDpc(PKDPC Dpc, PVOID Context, PVOID Argument1, PVOID Argument2)
{
    VI_DEVICE *Device = Context;
    VI_QUEUE *Queue = &Device->EventQueue;
    VI_USED Entry;
    VI_EVENT Event;
    KIRQL Irql;
    PIRP Irp;
    ULONG Length;
    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(Argument1);
    UNREFERENCED_PARAMETER(Argument2);

    for (;;)
    {
        KeAcquireSpinLock(&Device->Lock, &Irql);
        if (!Device->Started)
        {
            KeReleaseSpinLock(&Device->Lock, Irql);
            return;
        }
        ViLeds(Device);
        if (!Device->ReportLength)
            Device->ReportLength = ViTakeReport(&Device->State, Device->Report);
        while (!Device->ReportLength && Queue->LastUsed != Queue->Used[1])
        {
            KeMemoryBarrier();
            Entry = ((volatile VI_USED *)(Queue->Used + 2))[Queue->LastUsed++ % Queue->Size];
            if (Entry.Id >= Queue->Size || Entry.Length != sizeof(Event))
            {
                KeReleaseSpinLock(&Device->Lock, Irql);
                DbgPrint("VirtIO Input: malformed event completion %lu/%lu\n", Entry.Id, Entry.Length);
                return;
            }
            Event = Queue->Events[Entry.Id];
            ViPublish(Queue, (USHORT)Entry.Id);
            ViWrite(Device, 0x50, 0);
            ViDecodeEvent(&Device->State, &Event);
            Device->ReportLength = ViTakeReport(&Device->State, Device->Report);
        }
        Length = Device->ReportLength;
        KeReleaseSpinLock(&Device->Lock, Irql);
        if (!Length) return;
        Irp = IoCsqRemoveNextIrp(&Device->Csq, NULL);
        if (!Irp) return;
        KeAcquireSpinLock(&Device->Lock, &Irql);
        if (!Device->Started)
        {
            KeReleaseSpinLock(&Device->Lock, Irql);
            ViComplete(Device, Irp, STATUS_DEVICE_NOT_READY, 0);
            return;
        }
        RtlCopyMemory(Irp->UserBuffer, Device->Report, Length);
        Device->ReportLength = 0;
        KeReleaseSpinLock(&Device->Lock, Irql);
        ViComplete(Device, Irp, STATUS_SUCCESS, Length);
    }
}

static BOOLEAN NTAPI ViInterrupt(PKINTERRUPT Interrupt, PVOID Context)
{
    VI_DEVICE *Device = Context;
    ULONG Status = ViRead(Device, 0x60);
    UNREFERENCED_PARAMETER(Interrupt);
    if (!Status) return FALSE;
    ViWrite(Device, 0x64, Status);
    if (Device->Started) KeInsertQueueDpc(&Device->Dpc, NULL, NULL);
    return TRUE;
}

static VOID ViStop(VI_DEVICE *Device)
{
    PIRP Irp;
    KIRQL Irql;
    KeAcquireSpinLock(&Device->Lock, &Irql);
    Device->Started = FALSE;
    KeReleaseSpinLock(&Device->Lock, Irql);
    if (Device->Interrupt)
    {
        IoDisconnectInterrupt(Device->Interrupt);
        Device->Interrupt = NULL;
    }
    KeRemoveQueueDpc(&Device->Dpc);
    KeFlushQueuedDpcs();
    if (Device->Registers) ViWrite(Device, 0x70, 0);
    while ((Irp = IoCsqRemoveNextIrp(&Device->Csq, NULL)))
        ViComplete(Device, Irp, STATUS_DEVICE_NOT_READY, 0);
    if (Device->Dma)
    {
        MmFreeContiguousMemorySpecifyCache(Device->Dma, 4 * PAGE_SIZE, MmNonCached);
        Device->Dma = NULL;
    }
    if (Device->Registers)
    {
        MmUnmapIoSpace(Device->Registers, 0x200);
        Device->Registers = NULL;
        InterlockedAnd(&ViClaims, ~(1u << Device->Slot));
    }
    Device->ReportLength = 0;
    Device->StatusBusy = 0;
}

static NTSTATUS ViStart(VI_DEVICE *Device)
{
    UCHAR Bits[128];
    USHORT Ids[4];
    LONG AbsInfo[5];
    ULONG Slot, Vector, Index, Size;
    KIRQL Irql;
    KAFFINITY Affinity;
    PHYSICAL_ADDRESS Low, High, Boundary;
    NTSTATUS Status;
    BOOLEAN Keyboard;

    if (Device->Started) return STATUS_SUCCESS;
    Keyboard = Device->State.Keyboard;
    RtlZeroMemory(&Device->State, sizeof(Device->State));
    Device->State.Keyboard = Keyboard;
    for (Slot = 0; Slot < 32; ++Slot)
    {
        if (InterlockedOr(&ViClaims, 1u << Slot) & (1u << Slot)) continue;
        Device->Registers = ViPlatformMap(Slot, &Vector);
        Device->Slot = Slot;
        if (Device->Registers && ViRead(Device, 0) == 0x74726976 &&
            ViRead(Device, 4) == 2 && ViRead(Device, 8) == 18)
        {
            ViConfig(Device, 0x11, 1, Bits, sizeof(Bits));
            if ((Keyboard && (Bits[30 / 8] & (1u << (30 % 8)))) ||
                (!Keyboard && (Bits[272 / 8] & (1u << (272 % 8))))) break;
        }
        if (Device->Registers) MmUnmapIoSpace(Device->Registers, 0x200);
        Device->Registers = NULL;
        InterlockedAnd(&ViClaims, ~(1u << Slot));
    }
    if (!Device->Registers) return STATUS_DEVICE_NOT_CONNECTED;
    ViWrite(Device, 0x70, 0);
    ViWrite(Device, 0x70, 3);
    ViWrite(Device, 0x14, 1);
    if (!(ViRead(Device, 0x10) & 1)) { Status = STATUS_NOT_SUPPORTED; goto Failure; }
    ViWrite(Device, 0x24, 0);
    ViWrite(Device, 0x20, 0);
    ViWrite(Device, 0x24, 1);
    ViWrite(Device, 0x20, 1);
    ViWrite(Device, 0x70, 11);
    if (!(ViRead(Device, 0x70) & 8)) { Status = STATUS_NOT_SUPPORTED; goto Failure; }
    ViConfig(Device, 3, 0, Ids, sizeof(Ids));
    Device->Attributes.Size = sizeof(Device->Attributes);
    Device->Attributes.VendorID = Ids[1];
    Device->Attributes.ProductID = Ids[2];
    Device->Attributes.VersionNumber = Ids[3];
    ViConfig(Device, 0x11, 17, Bits, sizeof(Bits));
    Device->LedMask = Bits[0] & 0x1f;
    Device->SentLeds = Device->DesiredLeds = 0;
    if (!Keyboard)
    {
        ViConfig(Device, 0x11, 3, Bits, sizeof(Bits));
        Device->State.Absolute = (Bits[0] & 3) == 3;
        if (Device->State.Absolute)
        {
            for (Index = 0; Index < 2; ++Index)
            {
                Size = ViConfig(Device, 0x12, (UCHAR)Index, AbsInfo, sizeof(AbsInfo));
                if (Size < sizeof(AbsInfo) || AbsInfo[1] <= AbsInfo[0])
                { Status = STATUS_DEVICE_CONFIGURATION_ERROR; goto Failure; }
                Device->State.Min[Index] = AbsInfo[0];
                Device->State.Max[Index] = AbsInfo[1];
            }
        }
    }
    for (Index = 0; Index < 2; ++Index)
    {
        ViWrite(Device, 0x30, Index);
        if (ViRead(Device, 0x34) < (Index ? VI_STATUS : VI_EVENTS))
        { Status = STATUS_DEVICE_CONFIGURATION_ERROR; goto Failure; }
    }
    Low.QuadPart = Boundary.QuadPart = 0;
    High.QuadPart = MAXULONG;
    Device->Dma = MmAllocateContiguousMemorySpecifyCache(4 * PAGE_SIZE, Low, High, Boundary, MmNonCached);
    if (!Device->Dma) { Status = STATUS_INSUFFICIENT_RESOURCES; goto Failure; }
    RtlZeroMemory(Device->Dma, 4 * PAGE_SIZE);
    Device->DmaAddress = MmGetPhysicalAddress(Device->Dma);
    ViSetupQueue(Device, &Device->EventQueue, 0, 0, 0x400, 0x1000, 0x2000, VI_EVENTS);
    ViSetupQueue(Device, &Device->StatusQueue, 1, 0x3000, 0x3080, 0x3100, 0x3200, VI_STATUS);
    Device->Vector = HalGetInterruptVector(Internal, 0, Vector, Vector, &Irql, &Affinity);
    if (!Device->Vector) { Status = STATUS_DEVICE_CONFIGURATION_ERROR; goto Failure; }
    Status = IoConnectInterrupt(&Device->Interrupt, ViInterrupt, Device, NULL,
                               Device->Vector, Irql, Irql, LevelSensitive, FALSE, Affinity, FALSE);
    if (!NT_SUCCESS(Status)) goto Failure;
    InterlockedExchange(&Device->Started, TRUE);
    ViWrite(Device, 0x70, 15);
    ViWrite(Device, 0x50, 0);
    DbgPrint("VirtIO Input: %s started slot %lu IRQ %lu HID %04x:%04x\n",
             Keyboard ? "keyboard" : (Device->State.Absolute ? "tablet" : "mouse"),
             Slot, Device->Vector, Ids[1], Ids[2]);
    return STATUS_SUCCESS;
Failure:
    ViStop(Device);
    return Status;
}

static NTSTATUS NTAPI ViLowerComplete(PDEVICE_OBJECT Device, PIRP Irp, PVOID Context)
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(Irp);
    KeSetEvent(Context, IO_NO_INCREMENT, FALSE);
    return STATUS_MORE_PROCESSING_REQUIRED;
}
static NTSTATUS ViForwardSync(VI_DEVICE *Device, PIRP Irp)
{
    KEVENT Event;
    NTSTATUS Status;
    KeInitializeEvent(&Event, NotificationEvent, FALSE);
    IoCopyCurrentIrpStackLocationToNext(Irp);
    IoSetCompletionRoutine(Irp, ViLowerComplete, &Event, TRUE, TRUE, TRUE);
    Status = IoCallDriver(Device->Hid->NextDeviceObject, Irp);
    if (Status == STATUS_PENDING) KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
    return Irp->IoStatus.Status;
}

static NTSTATUS NTAPI ViPnp(PDEVICE_OBJECT Object, PIRP Irp)
{
    VI_DEVICE *Device = ViExtension(Object);
    PIO_STACK_LOCATION Stack = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS Status = IoAcquireRemoveLock(&Device->RemoveLock, Irp);
    if (!NT_SUCCESS(Status))
    {
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }
    switch (Stack->MinorFunction)
    {
        case IRP_MN_START_DEVICE:
            Status = ViForwardSync(Device, Irp);
            if (NT_SUCCESS(Status)) Status = ViStart(Device);
            return ViComplete(Device, Irp, Status, 0);
        case IRP_MN_STOP_DEVICE:
        case IRP_MN_SURPRISE_REMOVAL:
            ViStop(Device);
            Irp->IoStatus.Status = STATUS_SUCCESS;
            break;
        case IRP_MN_REMOVE_DEVICE:
            ViStop(Device);
            IoReleaseRemoveLockAndWait(&Device->RemoveLock, Irp);
            Irp->IoStatus.Status = STATUS_SUCCESS;
            IoSkipCurrentIrpStackLocation(Irp);
            return IoCallDriver(Device->Hid->NextDeviceObject, Irp);
    }
    IoReleaseRemoveLock(&Device->RemoveLock, Irp);
    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(Device->Hid->NextDeviceObject, Irp);
}

static NTSTATUS NTAPI ViControl(PDEVICE_OBJECT Object, PIRP Irp)
{
    VI_DEVICE *Device = ViExtension(Object);
    PIO_STACK_LOCATION Stack = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS Status = IoAcquireRemoveLock(&Device->RemoveLock, Irp);
    const VOID *Data = NULL;
    ULONG Length = 0;
    HID_DESCRIPTOR Descriptor;
    PHID_XFER_PACKET Packet;
    KIRQL Irql;
    if (!NT_SUCCESS(Status))
    {
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }
    if (!Device->Started) return ViComplete(Device, Irp, STATUS_DEVICE_NOT_READY, 0);
    Status = STATUS_SUCCESS;
    switch (Stack->Parameters.DeviceIoControl.IoControlCode)
    {
        case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
            RtlZeroMemory(&Descriptor, sizeof(Descriptor));
            Descriptor.bLength = sizeof(Descriptor);
            Descriptor.bDescriptorType = HID_HID_DESCRIPTOR_TYPE;
            Descriptor.bcdHID = 0x0111;
            Descriptor.bNumDescriptors = 1;
            Descriptor.DescriptorList[0].bReportType = HID_REPORT_DESCRIPTOR_TYPE;
            ViReportDescriptor(Device->State.Keyboard, Device->State.Absolute, &Length);
            Descriptor.DescriptorList[0].wReportLength = (USHORT)Length;
            Length = sizeof(Descriptor);
            Data = &Descriptor;
            break;
        case IOCTL_HID_GET_REPORT_DESCRIPTOR:
            Data = ViReportDescriptor(Device->State.Keyboard, Device->State.Absolute, &Length);
            break;
        case IOCTL_HID_GET_DEVICE_ATTRIBUTES:
            Data = &Device->Attributes;
            Length = sizeof(Device->Attributes);
            break;
        case IOCTL_HID_READ_REPORT:
            Length = Device->State.Keyboard ? 9 : 7;
            if (!Irp->UserBuffer || Stack->Parameters.DeviceIoControl.OutputBufferLength < Length)
                return ViComplete(Device, Irp, STATUS_BUFFER_TOO_SMALL, 0);
            IoMarkIrpPending(Irp);
            Status = IoCsqInsertIrpEx(&Device->Csq, Irp, NULL, NULL);
            if (!NT_SUCCESS(Status)) ViComplete(Device, Irp, Status, 0);
            return STATUS_PENDING;
        case IOCTL_HID_WRITE_REPORT:
        case IOCTL_HID_SET_OUTPUT_REPORT:
            Packet = Irp->UserBuffer;
            if (!Device->State.Keyboard || !Packet || !Packet->reportBuffer ||
                Packet->reportId != 1 || Packet->reportBufferLen < 2)
                return ViComplete(Device, Irp, STATUS_INVALID_PARAMETER, 0);
            KeAcquireSpinLock(&Device->Lock, &Irql);
            if (!Device->Started)
            {
                KeReleaseSpinLock(&Device->Lock, Irql);
                return ViComplete(Device, Irp, STATUS_DEVICE_NOT_READY, 0);
            }
            Device->DesiredLeds = Packet->reportBuffer[1] & Device->LedMask;
            KeInsertQueueDpc(&Device->Dpc, NULL, NULL);
            KeReleaseSpinLock(&Device->Lock, Irql);
            return ViComplete(Device, Irp, STATUS_SUCCESS, 2);
        case IOCTL_HID_ACTIVATE_DEVICE:
        case IOCTL_HID_DEACTIVATE_DEVICE:
            break;
        default:
            Status = STATUS_NOT_SUPPORTED;
    }
    if (Data)
    {
        if (!Irp->UserBuffer || Stack->Parameters.DeviceIoControl.OutputBufferLength < Length)
            return ViComplete(Device, Irp, STATUS_BUFFER_TOO_SMALL, 0);
        RtlCopyMemory(Irp->UserBuffer, Data, Length);
    }
    return ViComplete(Device, Irp, Status, Length);
}

static NTSTATUS NTAPI ViPower(PDEVICE_OBJECT Object, PIRP Irp)
{
    VI_DEVICE *Device = ViExtension(Object);
    PoStartNextPowerIrp(Irp);
    IoSkipCurrentIrpStackLocation(Irp);
    return PoCallDriver(Device->Hid->NextDeviceObject, Irp);
}
static NTSTATUS NTAPI ViPass(PDEVICE_OBJECT Object, PIRP Irp)
{
    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(ViExtension(Object)->Hid->NextDeviceObject, Irp);
}
static NTSTATUS NTAPI ViOpen(PDEVICE_OBJECT Object, PIRP Irp)
{
    UNREFERENCED_PARAMETER(Object);
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}
static VOID NTAPI ViUnload(PDRIVER_OBJECT Driver)
{
    UNREFERENCED_PARAMETER(Driver);
}
static NTSTATUS NTAPI ViAdd(PDRIVER_OBJECT Driver, PDEVICE_OBJECT Object)
{
    VI_DEVICE *Device = ViExtension(Object);
    WCHAR HardwareId[128];
    ULONG Length;
    NTSTATUS Status;
    UNREFERENCED_PARAMETER(Driver);
    RtlZeroMemory(Device, sizeof(*Device));
    Device->Hid = Object->DeviceExtension;
    Device->Device = Object;
    Status = IoGetDeviceProperty(Device->Hid->PhysicalDeviceObject, DevicePropertyHardwareID,
                                 sizeof(HardwareId), HardwareId, &Length);
    if (!NT_SUCCESS(Status)) return Status;
    if (!wcscmp(HardwareId, L"ROOT\\LIBERNTVIRTIOKBD")) Device->State.Keyboard = TRUE;
    else if (wcscmp(HardwareId, L"ROOT\\LIBERNTVIRTIOPOINTER")) return STATUS_DEVICE_CONFIGURATION_ERROR;
    KeInitializeSpinLock(&Device->Lock);
    KeInitializeDpc(&Device->Dpc, ViDpc, Device);
    KeSetTargetProcessorDpc(&Device->Dpc, 0);
    InitializeListHead(&Device->Reads);
    IoInitializeRemoveLock(&Device->RemoveLock, VI_TAG, 0, 0);
    return IoCsqInitializeEx(&Device->Csq, ViInsert, ViRemove, ViPeek, ViLock, ViUnlock, ViCancel);
}
NTSTATUS NTAPI DriverEntry(PDRIVER_OBJECT Driver, PUNICODE_STRING RegistryPath)
{
    HID_MINIDRIVER_REGISTRATION Registration;
    RtlZeroMemory(&Registration, sizeof(Registration));
    Driver->DriverExtension->AddDevice = ViAdd;
    Driver->DriverUnload = ViUnload;
    Driver->MajorFunction[IRP_MJ_PNP] = ViPnp;
    Driver->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = ViControl;
    Driver->MajorFunction[IRP_MJ_POWER] = ViPower;
    Driver->MajorFunction[IRP_MJ_SYSTEM_CONTROL] = ViPass;
    Driver->MajorFunction[IRP_MJ_CREATE] = ViOpen;
    Driver->MajorFunction[IRP_MJ_CLOSE] = ViOpen;
    Registration.Revision = HID_REVISION;
    Registration.DriverObject = Driver;
    Registration.RegistryPath = RegistryPath;
    Registration.DeviceExtensionSize = sizeof(VI_DEVICE);
    return HidRegisterMinidriver(&Registration);
}
