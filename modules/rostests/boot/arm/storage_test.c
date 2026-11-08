/*
 * PROJECT:     LiberNT API tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Verify native FAT file I/O through the ARM boot storage driver
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ntifs.h>

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
