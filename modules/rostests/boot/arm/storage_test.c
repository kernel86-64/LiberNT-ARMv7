/*
 * PROJECT:     LiberNT API tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Verify native FAT file I/O through the ARM boot storage driver
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ntifs.h>
#include <mountdev.h>
#include <ntosp.h>
#include <ntstrsafe.h>

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
    ULONG Buffer[128], Length, Midr;
    WCHAR Expected[64];
    UNICODE_STRING ActualString, ExpectedString;
    PKEY_VALUE_PARTIAL_INFORMATION Information = (PKEY_VALUE_PARTIAL_INFORMATION)Buffer;
    NTSTATUS Status;

    if (KeGetCurrentProcessorIndex() != KeGetCurrentProcessorNumberEx(NULL) ||
        KeGetCurrentProcessorIndex() >= KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS))
        return STATUS_DATA_ERROR;
    InitializeObjectAttributes(&Attributes, &Name, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
    Status = ZwOpenKey(&Key, KEY_QUERY_VALUE, &Attributes);
    if (!NT_SUCCESS(Status)) return Status;
    Status = ZwQueryValueKey(Key, &Value, KeyValuePartialInformation, Buffer, sizeof(Buffer), &Length);
    if (!NT_SUCCESS(Status))
    {
        ZwClose(Key);
        return Status;
    }
    if (Length < FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION, Data) || Length > sizeof(Buffer) ||
        Information->Type != REG_SZ || Information->DataLength <= sizeof(WCHAR) ||
        Information->DataLength % sizeof(WCHAR) ||
        Information->DataLength > Length - FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION, Data) ||
        ((PWCHAR)Information->Data)[Information->DataLength / sizeof(WCHAR) - 1] != UNICODE_NULL)
    {
        ZwClose(Key);
        return STATUS_DATA_ERROR;
    }
    __asm__ __volatile__("mrc p15, 0, %0, c0, c0, 0" : "=r"(Midr));
    Status = RtlStringCchPrintfW(Expected, RTL_NUMBER_OF(Expected), L"ARM MIDR %08lx", Midr);
    if (!NT_SUCCESS(Status))
    {
        ZwClose(Key);
        return Status;
    }
    RtlInitUnicodeString(&ExpectedString, Expected);
    RtlInitUnicodeString(&ActualString, (PWCHAR)Information->Data);
    if (!Midr || !RtlEqualUnicodeString(&ExpectedString, &ActualString, FALSE))
    {
        DbgPrint("ARM processor identity mismatch: hardware MIDR=%08lx registry=%wZ\n", Midr, &ActualString);
        ZwClose(Key);
        return STATUS_DATA_ERROR;
    }
    RtlInitUnicodeString(&Value, L"ProcessorNameString");
    Status = ZwQueryValueKey(Key, &Value, KeyValuePartialInformation, Buffer, sizeof(Buffer), &Length);
    ZwClose(Key);
    if (!NT_SUCCESS(Status)) return Status;
    if (Length < FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION, Data) || Length > sizeof(Buffer) ||
        Information->Type != REG_SZ || Information->DataLength <= sizeof(WCHAR) ||
        Information->DataLength % sizeof(WCHAR) ||
        Information->DataLength > Length - FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION, Data) ||
        ((PWCHAR)Information->Data)[Information->DataLength / sizeof(WCHAR) - 1] != UNICODE_NULL)
        return STATUS_DATA_ERROR;
    RtlInitUnicodeString(&ActualString, (PWCHAR)Information->Data);
    if (Midr >> 24 == 0x41 && ((Midr >> 4) & 0xfff) == 0xc09)
    {
        RtlInitUnicodeString(&ExpectedString, L"Samsung Exynos 4210");
        if (!RtlEqualUnicodeString(&ExpectedString, &ActualString, FALSE))
        {
            DbgPrint("ARM processor name mismatch: MIDR=%08lx got=%wZ\n",
                     Midr, &ActualString);
            return STATUS_DATA_ERROR;
        }
    }
    DbgPrint("ARM processor identity tests: PASS (MIDR=%08lx, name=%wZ)\n", Midr, &ActualString);
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

static NTSTATUS ArmPlatformExports(VOID)
{
    UCHAR Buffer[130];
    ULONG Index, Port = 0, Values[3] = {0x11, 0x22, 0x33}, Output[3] = {0};
    HAL_INTERRUPT_TARGET_INFORMATION Target = {0};
    HAL_MESSAGE_ROUTING_INFO Routing = {0};
    MEMORY_CACHING_TYPE Cache = MmCached;
    PHYSICAL_ADDRESS Address = {{0}};

    WRITE_PORT_ULONG(&Port, 0x12345678);
    if (READ_PORT_ULONG(&Port) != 0x12345678) return STATUS_DATA_ERROR;
    WRITE_PORT_USHORT((PUSHORT)&Port, 0xabcd);
    if (READ_PORT_USHORT((PUSHORT)&Port) != 0xabcd || Port != 0x1234abcd)
        return STATUS_DATA_ERROR;
    READ_PORT_BUFFER_ULONG(&Port, Output, ARRAYSIZE(Output));
    for (Index = 0; Index < ARRAYSIZE(Output); ++Index)
        if (Output[Index] != Port) return STATUS_DATA_ERROR;
    WRITE_PORT_BUFFER_ULONG(&Port, Values, ARRAYSIZE(Values));
    if (Port != Values[ARRAYSIZE(Values) - 1]) return STATUS_DATA_ERROR;
    WRITE_PORT_BUFFER_ULONG(&Port, NULL, 0);
    if (Port != Values[ARRAYSIZE(Values) - 1]) return STATUS_DATA_ERROR;
    if (HalGetInterruptTargetInformation(&Target) != STATUS_INVALID_PARAMETER)
        return STATUS_DATA_ERROR;
    Target.Version = HAL_INTERRUPT_TARGET_INFORMATION_VERSION;
    if (HalGetInterruptTargetInformation(&Target) != STATUS_SUCCESS ||
        Target.TargetProcessors != 1 || Target.ProcessorNumber || Target.DestinationId)
        return STATUS_DATA_ERROR;
    Target.TargetProcessors = 2;
    if (HalGetInterruptTargetInformation(&Target) != STATUS_INVALID_PARAMETER)
        return STATUS_DATA_ERROR;
    Routing.Version = HAL_MESSAGE_ROUTING_INFO_VERSION;
    if (HalGetMessageRoutingInfo(&Routing) != STATUS_NOT_SUPPORTED) return STATUS_DATA_ERROR;
    if (HalGetMemoryCachingRequirements(Address, PAGE_SIZE, &Cache) != STATUS_NOT_SUPPORTED ||
        Cache != MmNonCached) return STATUS_DATA_ERROR;
    if (HalGetMemoryCachingRequirements(Address, PAGE_SIZE, NULL) != STATUS_INVALID_PARAMETER)
        return STATUS_DATA_ERROR;
    if (HalMakeBeep(0) || HalMakeBeep(440)) return STATUS_DATA_ERROR;
    for (Index = 0; Index < sizeof(Buffer); ++Index) Buffer[Index] = (UCHAR)Index;
    if (HalSetBusDataByOffset(MaximumBusDataType, 0, 0, Buffer, 1, sizeof(Buffer)) != 0)
        return STATUS_DATA_ERROR;
    KeInvalidateRangeAllCaches(NULL, 0);
    KeInvalidateRangeAllCaches(Buffer + 1, sizeof(Buffer) - 2);
    for (Index = 0; Index < sizeof(Buffer); ++Index)
        if (Buffer[Index] != (UCHAR)Index) return STATUS_DATA_ERROR;
    DbgPrint("ARM platform export tests: PASS (unsupported bus, unaligned cache range)\n");
    return STATUS_SUCCESS;
}

static NTSTATUS ArmBitmapExports(VOID)
{
    static const ULONG Positions[] = {0, 31, 32, 63, 64, 127, 128, 255};
    RTL_BITMAP_EX Bitmap;
    ULONG64 Storage[4];
    ULONG Index;

    RtlInitializeBitMapEx(&Bitmap, Storage, 256);
    if (Bitmap.SizeOfBitMap != 256 || Bitmap.Buffer != Storage) return STATUS_DATA_ERROR;
    RtlClearAllBitsEx(&Bitmap);
    for (Index = 0; Index < ARRAYSIZE(Storage); ++Index)
        if (Storage[Index]) return STATUS_DATA_ERROR;
    for (Index = 0; Index < ARRAYSIZE(Positions); ++Index)
        RtlSetBitEx(&Bitmap, Positions[Index]);
    for (Index = 0; Index < ARRAYSIZE(Positions); ++Index)
        if (!RtlTestBitEx(&Bitmap, Positions[Index])) return STATUS_DATA_ERROR;
    if (RtlTestBitEx(&Bitmap, 1) || RtlTestBitEx(&Bitmap, 254)) return STATUS_DATA_ERROR;
    for (Index = 0; Index < ARRAYSIZE(Positions); ++Index)
        RtlClearBitEx(&Bitmap, Positions[Index]);
    for (Index = 0; Index < ARRAYSIZE(Storage); ++Index)
        if (Storage[Index]) return STATUS_DATA_ERROR;
    DbgPrint("ARM extended bitmap exports: PASS (32/64-bit boundaries, final bit, clear)\n");
    return STATUS_SUCCESS;
}

static NTSTATUS ArmIoCacheExports(VOID)
{
    PUCHAR Buffer;
    PMDL Mdl;
    ULONG Index, Pass;
    NTSTATUS Status = STATUS_SUCCESS;

    Buffer = ExAllocatePoolWithTag(NonPagedPool, 2 * PAGE_SIZE, 'tcIA');
    if (!Buffer) return STATUS_INSUFFICIENT_RESOURCES;
    Mdl = IoAllocateMdl(Buffer + 1, PAGE_SIZE + 9, FALSE, FALSE, NULL);
    if (!Mdl)
    {
        ExFreePoolWithTag(Buffer, 'tcIA');
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    MmBuildMdlForNonPagedPool(Mdl);
    for (Pass = 0; Pass < 2; ++Pass)
    {
        for (Index = 0; Index < 2 * PAGE_SIZE; ++Index)
            Buffer[Index] = (UCHAR)(Index * 37 + Pass);
        KeFlushIoBuffers(Mdl, FALSE, (BOOLEAN)Pass);
        KeFlushIoBuffers(Mdl, TRUE, (BOOLEAN)Pass);
        for (Index = 0; Index < 2 * PAGE_SIZE; ++Index)
            if (Buffer[Index] != (UCHAR)(Index * 37 + Pass)) Status = STATUS_DATA_ERROR;
    }
    IoFreeMdl(Mdl);
    ExFreePoolWithTag(Buffer, 'tcIA');
    if (NT_SUCCESS(Status))
        DbgPrint("ARM I/O cache tests: PASS (unaligned MDL, DMA and PIO directions)\n");
    return Status;
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
    if (NT_SUCCESS(Status)) Status = ArmPlatformExports();
    if (NT_SUCCESS(Status)) Status = ArmIoCacheExports();
    if (NT_SUCCESS(Status)) Status = ArmBitmapExports();
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
