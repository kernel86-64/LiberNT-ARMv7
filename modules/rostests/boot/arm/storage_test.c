/*
 * PROJECT:     LiberNT API tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Verify native FAT file I/O through the ARM boot storage driver
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ntifs.h>
#include <mountdev.h>

static NTSTATUS ArmStorageControl(PDEVICE_OBJECT Device, ULONG Code, PVOID Buffer,
                                  ULONG Length, PIO_STATUS_BLOCK IoStatus)
{
    KEVENT Event;
    PIRP Irp;
    NTSTATUS Status;

    KeInitializeEvent(&Event, NotificationEvent, FALSE);
    Irp = IoBuildDeviceIoControlRequest(Code, Device, NULL, 0, Buffer, Length,
                                        FALSE, &Event, IoStatus);
    if (!Irp) return STATUS_INSUFFICIENT_RESOURCES;
    Status = IoCallDriver(Device, Irp);
    if (Status == STATUS_PENDING)
    {
        KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
        Status = IoStatus->Status;
    }
    return Status;
}

static NTSTATUS ArmStorageIdentity(VOID)
{
    UNICODE_STRING Name = RTL_CONSTANT_STRING(L"\\Device\\Harddisk0\\Partition1");
    PFILE_OBJECT File;
    PDEVICE_OBJECT Device;
    IO_STATUS_BLOCK IoStatus;
    WCHAR Buffer[96];
    PMOUNTDEV_NAME DeviceName = (PMOUNTDEV_NAME)Buffer;
    PMOUNTDEV_UNIQUE_ID Id = (PMOUNTDEV_UNIQUE_ID)Buffer;
    ULONG Code, Header, Index;
    ULONG Signature = 0x4C4E5441;
    ULONGLONG Offset = 2048 * 512;
    NTSTATUS Status;

    Status = IoGetDeviceObjectPointer(&Name, FILE_READ_ATTRIBUTES, &File, &Device);
    if (!NT_SUCCESS(Status)) return Status;
    for (Index = 0; Index < 2; ++Index)
    {
        Code = Index ? IOCTL_MOUNTDEV_QUERY_UNIQUE_ID : IOCTL_MOUNTDEV_QUERY_DEVICE_NAME;
        Header = Index ? sizeof(MOUNTDEV_UNIQUE_ID) : sizeof(MOUNTDEV_NAME);
        Status = ArmStorageControl(Device, Code, Buffer, 0, &IoStatus);
        if (Status != STATUS_BUFFER_TOO_SMALL) goto Failed;
        Status = ArmStorageControl(Device, Code, Buffer, Header, &IoStatus);
        if (Status != STATUS_BUFFER_OVERFLOW || IoStatus.Information != Header) goto Failed;
        if (Index ? Id->UniqueIdLength != sizeof(Signature) + sizeof(Offset) : DeviceName->NameLength != Name.Length)
            goto Failed;
        Status = ArmStorageControl(Device, Code, Buffer, sizeof(Buffer), &IoStatus);
        if (!NT_SUCCESS(Status)) goto Done;
        if (Index)
        {
            if (IoStatus.Information != FIELD_OFFSET(MOUNTDEV_UNIQUE_ID, UniqueId) + sizeof(Signature) + sizeof(Offset) ||
                RtlCompareMemory(Id->UniqueId, &Signature, sizeof(Signature)) != sizeof(Signature) ||
                RtlCompareMemory(Id->UniqueId + sizeof(Signature), &Offset, sizeof(Offset)) != sizeof(Offset))
                goto Failed;
        }
        else if (IoStatus.Information != FIELD_OFFSET(MOUNTDEV_NAME, Name) + Name.Length ||
                 RtlCompareMemory(DeviceName->Name, Name.Buffer, Name.Length) != Name.Length)
            goto Failed;
    }
    DbgPrint("ARM storage identity tests: PASS (name, persistent ID, short buffers)\n");
    Status = STATUS_SUCCESS;
    goto Done;
Failed:
    Status = STATUS_DATA_ERROR;
Done:
    ObDereferenceObject(File);
    return Status;
}

static NTSTATUS ArmProcessorIdentity(VOID)
{
    UNICODE_STRING Name = RTL_CONSTANT_STRING(L"\\Registry\\Machine\\Hardware\\Description\\System\\CentralProcessor\\0");
    UNICODE_STRING Value = RTL_CONSTANT_STRING(L"Identifier");
    OBJECT_ATTRIBUTES Attributes;
    HANDLE Key;
    ULONG Buffer[128], Length;
    PKEY_VALUE_PARTIAL_INFORMATION Information = (PKEY_VALUE_PARTIAL_INFORMATION)Buffer;
    NTSTATUS Status;

    InitializeObjectAttributes(&Attributes, &Name, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
    Status = ZwOpenKey(&Key, KEY_QUERY_VALUE, &Attributes);
    if (!NT_SUCCESS(Status)) return Status;
    Status = ZwQueryValueKey(Key, &Value, KeyValuePartialInformation, Buffer, sizeof(Buffer), &Length);
    ZwClose(Key);
    if (!NT_SUCCESS(Status)) return Status;
    if (Length < FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION, Data) || Length > sizeof(Buffer) ||
        Information->Type != REG_SZ || Information->DataLength <= sizeof(WCHAR) ||
        Information->DataLength % sizeof(WCHAR) ||
        Information->DataLength > Length - FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION, Data) ||
        ((PWCHAR)Information->Data)[Information->DataLength / sizeof(WCHAR) - 1] != UNICODE_NULL)
        return STATUS_DATA_ERROR;
    DbgPrint("ARM processor identity tests: PASS (HARDWARE registry identifier)\n");
    return STATUS_SUCCESS;
}

static NTSTATUS ArmFastMutexExports(VOID)
{
    FAST_MUTEX Mutex;
    KIRQL Before = KeGetCurrentIrql();
    BOOLEAN Passed;

    ExInitializeFastMutex(&Mutex);
    if (!ExTryToAcquireFastMutex(&Mutex)) return STATUS_UNSUCCESSFUL;
    Passed = KeGetCurrentIrql() == APC_LEVEL;
    ExReleaseFastMutex(&Mutex);
    Passed = Passed && KeGetCurrentIrql() == Before;
    ExAcquireFastMutex(&Mutex);
    Passed = Passed && KeGetCurrentIrql() == APC_LEVEL;
    ExReleaseFastMutex(&Mutex);
    if (!Passed || KeGetCurrentIrql() != Before) return STATUS_DATA_ERROR;
    DbgPrint("ARM fast mutex export tests: PASS (try/acquire/release, IRQL)\n");
    return STATUS_SUCCESS;
}

static VOID NTAPI ArmStorageReinitialize(PDRIVER_OBJECT Driver, PVOID Context, ULONG Count)
{
    UNICODE_STRING Name = RTL_CONSTANT_STRING(L"\\Device\\Harddisk0\\Partition1\\reactos\\storage-test.bin");
    OBJECT_ATTRIBUTES Attributes;
    IO_STATUS_BLOCK IoStatus;
    HANDLE File = NULL;
    PUCHAR Buffer;
    PHYSICAL_ADDRESS Highest;
    LARGE_INTEGER Offset;
    ULONG Index, Pass;
    NTSTATUS Status;

    UNREFERENCED_PARAMETER(Driver);
    UNREFERENCED_PARAMETER(Context);
    UNREFERENCED_PARAMETER(Count);
    Status = ArmStorageIdentity();
    if (NT_SUCCESS(Status)) Status = ArmProcessorIdentity();
    if (NT_SUCCESS(Status)) Status = ArmFastMutexExports();
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("ARM storage tests: FAIL identity status=%08lx\n", Status);
        return;
    }
    Highest.QuadPart = MAXULONG;
    Buffer = MmAllocateContiguousMemory(PAGE_SIZE, Highest);
    if (!Buffer)
    {
        DbgPrint("ARM storage tests: FAIL allocation\n");
        return;
    }
    InitializeObjectAttributes(&Attributes, &Name, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
    Status = ZwCreateFile(&File, FILE_READ_DATA | FILE_WRITE_DATA | SYNCHRONIZE,
                          &Attributes, &IoStatus, NULL, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                          FILE_OPEN, FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT |
                          FILE_NO_INTERMEDIATE_BUFFERING, NULL, 0);
    if (!NT_SUCCESS(Status)) goto Done;
    for (Pass = 0; Pass < 3; ++Pass)
    {
        Offset.QuadPart = 0;
        RtlZeroMemory(Buffer, PAGE_SIZE);
        Status = ZwReadFile(File, NULL, NULL, NULL, &IoStatus, Buffer, PAGE_SIZE, &Offset, NULL);
        if (!NT_SUCCESS(Status)) goto Done;
        if (IoStatus.Information != PAGE_SIZE) { Status = STATUS_DATA_ERROR; goto Done; }
        for (Index = 0; Index < PAGE_SIZE; ++Index)
        {
            UCHAR Expected = (UCHAR)(Index * 37 + 11);
            if (Pass == 1) Expected ^= 0xA5;
            if (Buffer[Index] != Expected) { Status = STATUS_DATA_ERROR; goto Done; }
            Buffer[Index] ^= 0xA5;
        }
        if (Pass == 2) break;
        Offset.QuadPart = 0;
        Status = ZwWriteFile(File, NULL, NULL, NULL, &IoStatus, Buffer, PAGE_SIZE, &Offset, NULL);
        if (!NT_SUCCESS(Status)) goto Done;
        if (IoStatus.Information != PAGE_SIZE) { Status = STATUS_DATA_ERROR; goto Done; }
        Status = ZwFlushBuffersFile(File, &IoStatus);
        if (!NT_SUCCESS(Status)) goto Done;
    }
Done:
    if (File) ZwClose(File);
    MmFreeContiguousMemory(Buffer);
    if (NT_SUCCESS(Status))
        DbgPrint("ARM storage tests: PASS (FAT read/write/flush/read, 4096 bytes)\n");
    else
        DbgPrint("ARM storage tests: FAIL status=%08lx\n", Status);
}

NTSTATUS NTAPI DriverEntry(PDRIVER_OBJECT Driver, PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);
    IoRegisterBootDriverReinitialization(Driver, ArmStorageReinitialize, NULL);
    return STATUS_SUCCESS;
}
