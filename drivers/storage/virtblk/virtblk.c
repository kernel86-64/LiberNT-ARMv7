/*
 * PROJECT:     LiberNT Storage
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Boot disk access through VirtIO MMIO split queues
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include "virtblk.h"

static ULONG VbRead(VB_CONTROLLER *Controller, ULONG Offset)
{
    return READ_REGISTER_ULONG((PULONG)(Controller->Registers + Offset));
}

static VOID VbWrite(VB_CONTROLLER *Controller, ULONG Offset, ULONG Value)
{
    WRITE_REGISTER_ULONG((PULONG)(Controller->Registers + Offset), Value);
}

static BOOLEAN VbReset(VB_CONTROLLER *Controller)
{
    ULONG Attempt;

    VbWrite(Controller, 0x70, 0);
    for (Attempt = 0; Attempt < 10000; ++Attempt)
    {
        if (!VbRead(Controller, 0x70))
            return TRUE;
        KeStallExecutionProcessor(10);
    }
    return FALSE;
}

static NTSTATUS VbRequest(VB_CONTROLLER *Controller, ULONG Type, ULONGLONG Sector,
                          PVOID Buffer, ULONG Length)
{
    VB_DESCRIPTOR *Descriptors = (PVOID)Controller->Arena;
    volatile USHORT *Available = (PVOID)(Controller->Arena + VB_QUEUE_SIZE * sizeof(VB_DESCRIPTOR));
    volatile USHORT *Used = (PVOID)(Controller->Arena + PAGE_SIZE);
    volatile ULONG *UsedEntry;
    volatile UCHAR *Result = Controller->Arena + 2 * PAGE_SIZE + 16;
    PUCHAR Data = Controller->Arena + VB_DATA_OFFSET;
    PULONG Header = (PVOID)(Controller->Arena + 2 * PAGE_SIZE);
    ULONG Attempt, Last = Length ? 2 : 1;
    KIRQL OldIrql;
    NTSTATUS Status;

    KeAcquireSpinLock(&Controller->Lock, &OldIrql);
    if (Controller->Failed || (VbRead(Controller, 0x70) & (0x40 | 0x80)))
    {
        KeReleaseSpinLock(&Controller->Lock, OldIrql);
        return STATUS_DEVICE_NOT_READY;
    }
    Header[0] = Type;
    Header[1] = 0;
    RtlCopyMemory(Header + 2, &Sector, sizeof(Sector));
    *Result = 0xFF;
    if (Type == 1 && Length)
        RtlCopyMemory(Data, Buffer, Length);
    Descriptors[0].Address = Controller->ArenaPa.QuadPart + 2 * PAGE_SIZE;
    Descriptors[0].Length = 16;
    Descriptors[0].Flags = 1;
    Descriptors[0].Next = 1;
    if (Length)
    {
        Descriptors[1].Address = Controller->ArenaPa.QuadPart + VB_DATA_OFFSET;
        Descriptors[1].Length = Length;
        Descriptors[1].Flags = Type == 0 ? 3 : 1;
        Descriptors[1].Next = 2;
    }
    Descriptors[Last].Address = Controller->ArenaPa.QuadPart + 2 * PAGE_SIZE + 16;
    Descriptors[Last].Length = 1;
    Descriptors[Last].Flags = 2;
    Descriptors[Last].Next = 0;
    Available[2 + Controller->Available % VB_QUEUE_SIZE] = 0;
    KeMemoryBarrier();
    Available[1] = ++Controller->Available;
    KeMemoryBarrier();
    VbWrite(Controller, 0x50, 0);
    /* TODO: complete requests asynchronously from the device interrupt. */
    for (Attempt = 0; Attempt < 10000; ++Attempt)
    {
        if (Used[1] != Controller->Used)
            break;
        KeStallExecutionProcessor(10);
    }
    KeMemoryBarrier();
    if (Attempt == 10000)
    {
        Controller->Failed = TRUE;
        VbReset(Controller);
        Status = STATUS_IO_TIMEOUT;
    }
    else
    {
        UsedEntry = (PVOID)(Controller->Arena + PAGE_SIZE + 4 +
                           (Controller->Used % VB_QUEUE_SIZE) * 8);
        if ((USHORT)(Used[1] - Controller->Used) != 1 || UsedEntry[0] != 0)
        {
            Controller->Failed = TRUE;
            VbReset(Controller);
            Status = STATUS_IO_DEVICE_ERROR;
        }
        else if (*Result == 0 && UsedEntry[1] == (Type == 0 ? Length + 1 : 1))
        {
            Status = STATUS_SUCCESS;
            if (Type == 0 && Length)
                RtlCopyMemory(Buffer, Data, Length);
        }
        else
            Status = *Result == 2 ? STATUS_NOT_SUPPORTED : STATUS_IO_DEVICE_ERROR;
        Controller->Used++;
        VbWrite(Controller, 0x64, VbRead(Controller, 0x60));
    }
    KeReleaseSpinLock(&Controller->Lock, OldIrql);
    return Status;
}

static NTSTATUS VbInitialize(VB_CONTROLLER *Controller)
{
    PHYSICAL_ADDRESS Low = {{0}}, High, Boundary = {{0}};
    ULONG Features, State, Generation, Config;
    ULONG Attempts;

    Controller->Version = VbRead(Controller, 4);
    if (Controller->Version != 1 && Controller->Version != 2)
        return STATUS_NOT_SUPPORTED;
    if (!VbReset(Controller))
        return STATUS_IO_TIMEOUT;
    VbWrite(Controller, 0x70, 1);
    VbWrite(Controller, 0x70, 3);
    VbWrite(Controller, 0x14, 0);
    Features = VbRead(Controller, 0x10);
    if (!(Features & (1UL << 9)))
        return STATUS_NOT_SUPPORTED;
    Controller->Features = Features & ((1UL << 4) | (1UL << 5) | (1UL << 9));
    VbWrite(Controller, 0x24, 0);
    VbWrite(Controller, 0x20, Controller->Features);
    State = 3;
    if (Controller->Version == 2)
    {
        VbWrite(Controller, 0x14, 1);
        if (!(VbRead(Controller, 0x10) & 1))
            return STATUS_NOT_SUPPORTED;
        VbWrite(Controller, 0x24, 1);
        VbWrite(Controller, 0x20, 1);
        State |= 8;
        VbWrite(Controller, 0x70, State);
        if (!(VbRead(Controller, 0x70) & 8))
            return STATUS_NOT_SUPPORTED;
    }
    VbWrite(Controller, 0x30, 0);
    if (VbRead(Controller, 0x34) < VB_QUEUE_SIZE)
        return STATUS_NOT_SUPPORTED;
    High.QuadPart = MAXULONG;
    Controller->Arena = MmAllocateContiguousMemorySpecifyCache(VB_ARENA_SIZE, Low, High, Boundary, MmNonCached);
    if (!Controller->Arena)
        return STATUS_INSUFFICIENT_RESOURCES;
    RtlZeroMemory(Controller->Arena, VB_ARENA_SIZE);
    Controller->ArenaPa = MmGetPhysicalAddress(Controller->Arena);
    *(PUSHORT)(Controller->Arena + VB_QUEUE_SIZE * sizeof(VB_DESCRIPTOR)) = 1;
    VbWrite(Controller, 0x38, VB_QUEUE_SIZE);
    if (Controller->Version == 1)
    {
        VbWrite(Controller, 0x28, PAGE_SIZE);
        VbWrite(Controller, 0x3C, PAGE_SIZE);
        VbWrite(Controller, 0x40, (ULONG)(Controller->ArenaPa.QuadPart >> PAGE_SHIFT));
    }
    else
    {
        VbWrite(Controller, 0x80, Controller->ArenaPa.LowPart);
        VbWrite(Controller, 0x84, 0);
        VbWrite(Controller, 0x90, Controller->ArenaPa.LowPart + VB_QUEUE_SIZE * sizeof(VB_DESCRIPTOR));
        VbWrite(Controller, 0x94, 0);
        VbWrite(Controller, 0xA0, Controller->ArenaPa.LowPart + PAGE_SIZE);
        VbWrite(Controller, 0xA4, 0);
        VbWrite(Controller, 0x44, 1);
    }
    for (Attempts = 0; Attempts < 100; ++Attempts)
    {
        Generation = Controller->Version == 2 ? VbRead(Controller, 0xFC) : 0;
        Controller->Sectors = VbRead(Controller, 0x100);
        Controller->Sectors |= (ULONGLONG)VbRead(Controller, 0x104) << 32;
        Config = VbRead(Controller, 0x110);
        if (Controller->Version == 1 || Generation == VbRead(Controller, 0xFC))
            break;
    }
    if (Attempts == 100 || !Controller->Sectors || Controller->Sectors > MAXLONGLONG / 512)
        return STATUS_DEVICE_NOT_READY;
    Controller->Geometry.BytesPerSector = 512;
    Controller->Geometry.MediaType = FixedMedia;
    if ((Controller->Features & (1UL << 4)) && (Config & 0xFFFF) && (Config >> 24) && ((Config >> 16) & 255))
    {
        Controller->Geometry.Cylinders.QuadPart = Config & 0xFFFF;
        Controller->Geometry.TracksPerCylinder = (Config >> 16) & 255;
        Controller->Geometry.SectorsPerTrack = Config >> 24;
    }
    else
    {
        Controller->Geometry.Cylinders.QuadPart = Controller->Sectors;
        Controller->Geometry.TracksPerCylinder = 1;
        Controller->Geometry.SectorsPerTrack = 1;
    }
    KeMemoryBarrier();
    VbWrite(Controller, 0x70, State | 4);
    return STATUS_SUCCESS;
}

static NTSTATUS VbComplete(PIRP Irp, NTSTATUS Status, ULONG_PTR Information)
{
    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = Information;
    IoCompleteRequest(Irp, IO_DISK_INCREMENT);
    return Status;
}

static NTSTATUS NTAPI VbDispatch(PDEVICE_OBJECT Device, PIRP Irp)
{
    VB_EXTENSION *Extension = Device->DeviceExtension;
    VB_CONTROLLER *Controller = Extension->Controller;
    PIO_STACK_LOCATION Stack = IoGetCurrentIrpStackLocation(Irp);
    PVOID Output = Irp->AssociatedIrp.SystemBuffer;
    ULONG Length = Stack->Parameters.DeviceIoControl.OutputBufferLength;
    ULONGLONG Offset, Sector;
    ULONG Bytes, Remaining;
    PUCHAR Buffer;
    NTSTATUS Status;

    switch (Stack->MajorFunction)
    {
        case IRP_MJ_CREATE:
        case IRP_MJ_CLOSE:
        case IRP_MJ_CLEANUP:
            return VbComplete(Irp, STATUS_SUCCESS, 0);
        case IRP_MJ_FLUSH_BUFFERS:
            Status = VbRequest(Controller, 4, 0, NULL, 0);
            return VbComplete(Irp, Status, 0);
        case IRP_MJ_READ:
        case IRP_MJ_WRITE:
            if (KeGetCurrentIrql() > APC_LEVEL)
                return VbComplete(Irp, STATUS_INVALID_DEVICE_STATE, 0);
            if (Stack->MajorFunction == IRP_MJ_WRITE && (Controller->Features & (1UL << 5)))
                return VbComplete(Irp, STATUS_MEDIA_WRITE_PROTECTED, 0);
            Offset = Stack->Parameters.Read.ByteOffset.QuadPart;
            Remaining = Stack->Parameters.Read.Length;
            if ((Offset | Remaining) & 511 || Offset > Extension->Sectors * 512 || Remaining > Extension->Sectors * 512 - Offset)
                return VbComplete(Irp, STATUS_INVALID_PARAMETER, 0);
            if (!Remaining)
                return VbComplete(Irp, STATUS_SUCCESS, 0);
            Buffer = Irp->MdlAddress ? MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority) : Irp->AssociatedIrp.SystemBuffer;
            if (!Buffer)
                return VbComplete(Irp, STATUS_INSUFFICIENT_RESOURCES, 0);
            Sector = Extension->Start + Offset / 512;
            Length = 0;
            while (Remaining)
            {
                Bytes = min(Remaining, VB_DATA_SIZE);
                Status = VbRequest(Controller, Stack->MajorFunction == IRP_MJ_WRITE, Sector, Buffer, Bytes);
                if (!NT_SUCCESS(Status))
                    return VbComplete(Irp, Status, Length);
                Sector += Bytes / 512;
                Buffer += Bytes;
                Length += Bytes;
                Remaining -= Bytes;
            }
            return VbComplete(Irp, STATUS_SUCCESS, Length);
        case IRP_MJ_DEVICE_CONTROL:
            break;
        default:
            return VbComplete(Irp, STATUS_INVALID_DEVICE_REQUEST, 0);
    }
    switch (Stack->Parameters.DeviceIoControl.IoControlCode)
    {
        case IOCTL_MOUNTDEV_QUERY_DEVICE_NAME:
            if (Length < sizeof(MOUNTDEV_NAME)) break;
            RtlZeroMemory(Output, sizeof(MOUNTDEV_NAME));
            ((PMOUNTDEV_NAME)Output)->NameLength = Extension->NameLength;
            Bytes = FIELD_OFFSET(MOUNTDEV_NAME, Name) + Extension->NameLength;
            if (Length < Bytes)
                return VbComplete(Irp, STATUS_BUFFER_OVERFLOW, sizeof(MOUNTDEV_NAME));
            RtlCopyMemory(((PMOUNTDEV_NAME)Output)->Name, Extension->Name, Extension->NameLength);
            return VbComplete(Irp, STATUS_SUCCESS, Bytes);
        case IOCTL_MOUNTDEV_QUERY_UNIQUE_ID:
            if (Length < sizeof(MOUNTDEV_UNIQUE_ID)) break;
            RtlZeroMemory(Output, sizeof(MOUNTDEV_UNIQUE_ID));
            ((PMOUNTDEV_UNIQUE_ID)Output)->UniqueIdLength = sizeof(Controller->Signature) + sizeof(Offset);
            Bytes = FIELD_OFFSET(MOUNTDEV_UNIQUE_ID, UniqueId) + sizeof(Controller->Signature) + sizeof(Offset);
            if (Length < Bytes)
                return VbComplete(Irp, STATUS_BUFFER_OVERFLOW, sizeof(MOUNTDEV_UNIQUE_ID));
            Offset = Extension->Start * 512;
            Buffer = ((PMOUNTDEV_UNIQUE_ID)Output)->UniqueId;
            RtlCopyMemory(Buffer, &Controller->Signature, sizeof(Controller->Signature));
            RtlCopyMemory(Buffer + sizeof(Controller->Signature), &Offset, sizeof(Offset));
            return VbComplete(Irp, STATUS_SUCCESS, Bytes);
        case IOCTL_DISK_GET_DRIVE_GEOMETRY:
            if (Length < sizeof(DISK_GEOMETRY)) break;
            RtlCopyMemory(Output, &Controller->Geometry, sizeof(DISK_GEOMETRY));
            return VbComplete(Irp, STATUS_SUCCESS, sizeof(DISK_GEOMETRY));
        case IOCTL_DISK_GET_DRIVE_GEOMETRY_EX:
            if (Length < FIELD_OFFSET(DISK_GEOMETRY_EX, Data)) break;
            RtlZeroMemory(Output, FIELD_OFFSET(DISK_GEOMETRY_EX, Data));
            ((PDISK_GEOMETRY_EX)Output)->Geometry = Controller->Geometry;
            ((PDISK_GEOMETRY_EX)Output)->DiskSize.QuadPart = Controller->Sectors * 512;
            return VbComplete(Irp, STATUS_SUCCESS, FIELD_OFFSET(DISK_GEOMETRY_EX, Data));
        case IOCTL_DISK_GET_LENGTH_INFO:
            if (Length < sizeof(GET_LENGTH_INFORMATION)) break;
            ((PGET_LENGTH_INFORMATION)Output)->Length.QuadPart = Extension->Sectors * 512;
            return VbComplete(Irp, STATUS_SUCCESS, sizeof(GET_LENGTH_INFORMATION));
        case IOCTL_STORAGE_GET_DEVICE_NUMBER:
            if (Length < sizeof(STORAGE_DEVICE_NUMBER)) break;
            ((PSTORAGE_DEVICE_NUMBER)Output)->DeviceType = FILE_DEVICE_DISK;
            ((PSTORAGE_DEVICE_NUMBER)Output)->DeviceNumber = Controller->Number;
            ((PSTORAGE_DEVICE_NUMBER)Output)->PartitionNumber = Extension->Partition;
            return VbComplete(Irp, STATUS_SUCCESS, sizeof(STORAGE_DEVICE_NUMBER));
        case IOCTL_DISK_GET_PARTITION_INFO:
            if (Length < sizeof(PARTITION_INFORMATION)) break;
            RtlZeroMemory(Output, sizeof(PARTITION_INFORMATION));
            ((PPARTITION_INFORMATION)Output)->StartingOffset.QuadPart = Extension->Start * 512;
            ((PPARTITION_INFORMATION)Output)->PartitionLength.QuadPart = Extension->Sectors * 512;
            ((PPARTITION_INFORMATION)Output)->PartitionNumber = Extension->Partition;
            ((PPARTITION_INFORMATION)Output)->PartitionType = Extension->Type;
            ((PPARTITION_INFORMATION)Output)->BootIndicator = Extension->Boot;
            ((PPARTITION_INFORMATION)Output)->RecognizedPartition = IsRecognizedPartition(Extension->Type);
            ((PPARTITION_INFORMATION)Output)->HiddenSectors = (ULONG)Extension->Start;
            return VbComplete(Irp, STATUS_SUCCESS, sizeof(PARTITION_INFORMATION));
        case IOCTL_DISK_GET_PARTITION_INFO_EX:
            if (Length < sizeof(PARTITION_INFORMATION_EX)) break;
            RtlZeroMemory(Output, sizeof(PARTITION_INFORMATION_EX));
            ((PPARTITION_INFORMATION_EX)Output)->PartitionStyle = PARTITION_STYLE_MBR;
            ((PPARTITION_INFORMATION_EX)Output)->StartingOffset.QuadPart = Extension->Start * 512;
            ((PPARTITION_INFORMATION_EX)Output)->PartitionLength.QuadPart = Extension->Sectors * 512;
            ((PPARTITION_INFORMATION_EX)Output)->PartitionNumber = Extension->Partition;
            ((PPARTITION_INFORMATION_EX)Output)->Mbr.PartitionType = Extension->Type;
            ((PPARTITION_INFORMATION_EX)Output)->Mbr.BootIndicator = Extension->Boot;
            ((PPARTITION_INFORMATION_EX)Output)->Mbr.RecognizedPartition = IsRecognizedPartition(Extension->Type);
            ((PPARTITION_INFORMATION_EX)Output)->Mbr.HiddenSectors = (ULONG)Extension->Start;
            return VbComplete(Irp, STATUS_SUCCESS, sizeof(PARTITION_INFORMATION_EX));
        case IOCTL_DISK_IS_WRITABLE:
            return VbComplete(Irp, Controller->Features & (1UL << 5) ? STATUS_MEDIA_WRITE_PROTECTED : STATUS_SUCCESS, 0);
        case IOCTL_STORAGE_CHECK_VERIFY:
        case IOCTL_STORAGE_CHECK_VERIFY2:
        case IOCTL_DISK_CHECK_VERIFY:
            if (Length && Length < sizeof(ULONG)) break;
            if (Length) *(PULONG)Output = 0;
            return VbComplete(Irp, Controller->Failed ? STATUS_DEVICE_NOT_READY : STATUS_SUCCESS, Length ? sizeof(ULONG) : 0);
        default:
            return VbComplete(Irp, STATUS_INVALID_DEVICE_REQUEST, 0);
    }
    return VbComplete(Irp, STATUS_BUFFER_TOO_SMALL, 0);
}

static NTSTATUS VbCreateDisk(PDRIVER_OBJECT Driver, VB_CONTROLLER *Controller, ULONG Partition,
                             ULONGLONG Start, ULONGLONG Sectors, UCHAR Type, BOOLEAN Boot)
{
    WCHAR Buffer[96];
    UNICODE_STRING Name;
    PDEVICE_OBJECT Device;
    VB_EXTENSION *Extension;
    NTSTATUS Status;

    Status = RtlStringCchPrintfW(Buffer, RTL_NUMBER_OF(Buffer), L"\\Device\\Harddisk%lu\\Partition%lu", Controller->Number, Partition);
    if (!NT_SUCCESS(Status)) return Status;
    RtlInitUnicodeString(&Name, Buffer);
    Status = IoCreateDevice(Driver, sizeof(*Extension), &Name, FILE_DEVICE_DISK,
                            FILE_DEVICE_SECURE_OPEN, FALSE, &Device);
    if (!NT_SUCCESS(Status)) return Status;
    Extension = Device->DeviceExtension;
    Extension->Controller = Controller;
    Extension->Start = Start;
    Extension->Sectors = Sectors;
    Extension->Partition = Partition;
    Extension->Type = Type;
    Extension->Boot = Boot;
    Extension->NameLength = Name.Length;
    RtlCopyMemory(Extension->Name, Name.Buffer, Name.Length);
    Device->Flags |= DO_DIRECT_IO;
    Device->Flags &= ~DO_DEVICE_INITIALIZING;
    Device->AlignmentRequirement = FILE_WORD_ALIGNMENT;
    return STATUS_SUCCESS;
}

static VOID NTAPI VbAnnounceVolumes(PDRIVER_OBJECT Driver, PVOID Context, ULONG Count)
{
    UNICODE_STRING Name = RTL_CONSTANT_STRING(MOUNTMGR_DEVICE_NAME);
    PFILE_OBJECT File;
    PDEVICE_OBJECT MountManager, Device;
    VB_EXTENSION *Extension;
    WCHAR Buffer[1 + RTL_NUMBER_OF(Extension->Name)];
    PMOUNTMGR_TARGET_NAME Target = (PMOUNTMGR_TARGET_NAME)Buffer;
    IO_STATUS_BLOCK IoStatus;
    KEVENT Event;
    PIRP Irp;
    NTSTATUS Status;

    UNREFERENCED_PARAMETER(Context);
    UNREFERENCED_PARAMETER(Count);
    Status = IoGetDeviceObjectPointer(&Name, FILE_READ_ATTRIBUTES, &File, &MountManager);
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("VIRTBLK: mount manager unavailable: %08lx\n", Status);
        return;
    }
    for (Device = Driver->DeviceObject; Device; Device = Device->NextDevice)
    {
        Extension = Device->DeviceExtension;
        if (!Extension->Partition || !IsRecognizedPartition(Extension->Type)) continue;
        Target->DeviceNameLength = Extension->NameLength;
        RtlCopyMemory(Target->DeviceName, Extension->Name, Extension->NameLength);
        KeInitializeEvent(&Event, NotificationEvent, FALSE);
        Irp = IoBuildDeviceIoControlRequest(IOCTL_MOUNTMGR_VOLUME_ARRIVAL_NOTIFICATION,
                                            MountManager, Target,
                                            FIELD_OFFSET(MOUNTMGR_TARGET_NAME, DeviceName) + Extension->NameLength,
                                            NULL, 0, FALSE, &Event, &IoStatus);
        if (!Irp) { Status = STATUS_INSUFFICIENT_RESOURCES; break; }
        Status = IoCallDriver(MountManager, Irp);
        if (Status == STATUS_PENDING)
        {
            KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
            Status = IoStatus.Status;
        }
        if (!NT_SUCCESS(Status)) break;
    }
    ObDereferenceObject(File);
    if (!NT_SUCCESS(Status))
        DbgPrint("VIRTBLK: volume arrival failed: %08lx\n", Status);
}

NTSTATUS NTAPI DriverEntry(PDRIVER_OBJECT Driver, PUNICODE_STRING RegistryPath)
{
    VB_CONTROLLER *Controller;
    ULONG Index, Slot, Partition, Start, Sectors;
    UCHAR Mbr[512];
    WCHAR Buffer[64];
    UNICODE_STRING Name;
    OBJECT_ATTRIBUTES Attributes;
    NTSTATUS Status = STATUS_NO_SUCH_DEVICE;
    PDEVICE_OBJECT Device;
    BOOLEAN Reset;

    UNREFERENCED_PARAMETER(RegistryPath);
    Controller = ExAllocatePoolZero(NonPagedPool, sizeof(*Controller), VB_TAG);
    if (!Controller) return STATUS_INSUFFICIENT_RESOURCES;
    KeInitializeSpinLock(&Controller->Lock);
    for (Index = 0; Index < 32; ++Index)
    {
        Controller->Registers = VbPlatformMap(Index);
        if (!Controller->Registers) { Status = STATUS_INSUFFICIENT_RESOURCES; goto Failure; }
        if (VbRead(Controller, 0) == 0x74726976 && VbRead(Controller, 8) == 2)
            break;
        MmUnmapIoSpace(Controller->Registers, 0x200);
        Controller->Registers = NULL;
    }
    if (!Controller->Registers) goto Failure;
    Slot = Index;
    Status = VbInitialize(Controller);
    if (!NT_SUCCESS(Status)) goto Failure;
    Status = VbRequest(Controller, 0, 0, Mbr, sizeof(Mbr));
    if (!NT_SUCCESS(Status)) goto Failure;
    if (Mbr[510] != 0x55 || Mbr[511] != 0xAA)
    { Status = STATUS_UNRECOGNIZED_VOLUME; goto Failure; }
    RtlCopyMemory(&Controller->Signature, Mbr + 440, sizeof(Controller->Signature));
    Controller->Number = IoGetConfigurationInformation()->DiskCount;
    RtlStringCchPrintfW(Buffer, RTL_NUMBER_OF(Buffer), L"\\Device\\Harddisk%lu", Controller->Number);
    RtlInitUnicodeString(&Name, Buffer);
    InitializeObjectAttributes(&Attributes, &Name, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
    Status = ZwCreateDirectoryObject(&Controller->Directory, DIRECTORY_ALL_ACCESS, &Attributes);
    if (!NT_SUCCESS(Status)) goto Failure;
    for (Index = 0; Index <= IRP_MJ_MAXIMUM_FUNCTION; ++Index)
        Driver->MajorFunction[Index] = VbDispatch;
    Status = VbCreateDisk(Driver, Controller, 0, 0, Controller->Sectors, 0, FALSE);
    if (!NT_SUCCESS(Status)) goto Failure;
    for (Partition = 0; Partition < 4; ++Partition)
    {
        PUCHAR Entry = Mbr + 446 + Partition * 16;
        if (!Entry[4]) continue;
        if (Entry[4] == 0xEE || Entry[4] == 5 || Entry[4] == 0x0F || Entry[4] == 0x85)
        { Status = STATUS_NOT_SUPPORTED; goto Failure; }
        RtlCopyMemory(&Start, Entry + 8, sizeof(Start));
        RtlCopyMemory(&Sectors, Entry + 12, sizeof(Sectors));
        if (!Start || !Sectors || Start >= Controller->Sectors || Sectors > Controller->Sectors - Start)
        { Status = STATUS_DISK_CORRUPT_ERROR; goto Failure; }
        Status = VbCreateDisk(Driver, Controller, Partition + 1, Start, Sectors, Entry[4], Entry[0] == 0x80);
        if (!NT_SUCCESS(Status)) goto Failure;
    }
    IoGetConfigurationInformation()->DiskCount++;
    IoRegisterBootDriverReinitialization(Driver, VbAnnounceVolumes, NULL);
    DbgPrint("VIRTBLK: MMIO %lu, version %lu, %I64u sectors; boot disk online\n", Slot, Controller->Version, Controller->Sectors);
    return STATUS_SUCCESS;
Failure:
    while ((Device = Driver->DeviceObject) != NULL) IoDeleteDevice(Device);
    if (Controller->Directory) ZwClose(Controller->Directory);
    Reset = !Controller->Registers || VbReset(Controller);
    if (Controller->Arena && Reset) MmFreeContiguousMemorySpecifyCache(Controller->Arena, VB_ARENA_SIZE, MmNonCached);
    if (Controller->Registers) MmUnmapIoSpace(Controller->Registers, 0x200);
    if (Controller->Arena && !Reset)
        DbgPrint("VIRTBLK: device did not reset; DMA allocation retained\n");
    ExFreePoolWithTag(Controller, VB_TAG);
    return Status;
}
