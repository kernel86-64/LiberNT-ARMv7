/*
 * PROJECT:     LiberNT API tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Check N7000 ACPI caching, clock progress and boot-volume I/O
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ntifs.h>
#include <arc/arc.h>
#include <drivers/acpi/acpi.h>
#include <halfuncs.h>
#include <arm/exynos4mct.h>
#include <arm/exynos4boot.h>
#include <ndk/ketypes.h>

static VOID NTAPI
ArmN7000Watch(PVOID Context)
{
    PKTHREAD Thread = Context;
    LARGE_INTEGER Delay;
    ULONG Index;
    Delay.QuadPart = -10000000;
    for (Index = 0; Index < 8; ++Index)
    {
        KIRQL OldIrql;
        PKWAIT_BLOCK Block;
        ULONG Frame, Depth;
        KeDelayExecutionThread(KernelMode, FALSE, &Delay);
        KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);
        if (Thread->State != Waiting)
        {
            DbgPrint("ARM N7000 watch state=%u\n", Thread->State);
            KeLowerIrql(OldIrql);
            continue;
        }
        Block = Thread->WaitBlockList;
        DbgPrint("ARM N7000 watch state=%u reason=%u stack=%p apc=%d/%d object=%p type=%u\n",
                 Thread->State, Thread->WaitReason, Thread->KernelStack,
                 Thread->KernelApcDisable, Thread->SpecialApcDisable,
                 Block ? Block->Object : NULL, Block ? ((PDISPATCHER_HEADER)Block->Object)->Type : 255);
        Frame = ((ULONG_PTR)Thread->KernelStack >= Thread->StackLimit &&
                 (ULONG_PTR)Thread->KernelStack <= (ULONG_PTR)Thread->StackBase - sizeof(KSWITCH_FRAME)) ?
                ((PKSWITCH_FRAME)Thread->KernelStack)->R11 : 0;
        for (Depth = 0; Depth < 12 && !(Frame & 3) && Frame >= Thread->StackLimit &&
             Frame <= (ULONG_PTR)Thread->StackBase - 8; ++Depth)
        {
            PULONG Link = (PULONG)(ULONG_PTR)Frame;
            DbgPrint("ARM N7000 wait frame %lu return=%08lx\n", Depth, Link[1]);
            if (Link[0] <= Frame) break;
            Frame = Link[0];
        }
        KeLowerIrql(OldIrql);
    }
    ObDereferenceObject(Thread);
    PsTerminateSystemThread(STATUS_SUCCESS);
}

extern NTSYSAPI PLOADER_PARAMETER_BLOCK KeLoaderBlock;
extern NTSYSAPI PCONFIGURATION_COMPONENT_DATA NTAPI
KeFindConfigurationNextEntry(PCONFIGURATION_COMPONENT_DATA Child, CONFIGURATION_CLASS Class,
                             CONFIGURATION_TYPE Type, PULONG ComponentKey,
                             PCONFIGURATION_COMPONENT_DATA *NextLink);

static NTSTATUS
ArmN7000CacheTest(VOID)
{
    PCONFIGURATION_COMPONENT_DATA Node, Next = NULL;
    PCM_PARTIAL_RESOURCE_LIST List;
    PACPI_BIOS_MULTI_NODE Bios;
    PHYSICAL_ADDRESS Address;
    MEMORY_CACHING_TYPE Cache;
    NTSTATUS Status;
    PUCHAR Mapping;
    ULONG Value, Expected;

    if (!KeLoaderBlock) return STATUS_NOT_FOUND;
    Node = KeFindConfigurationNextEntry(KeLoaderBlock->ConfigurationRoot,
                                        AdapterClass, MultiFunctionAdapter, NULL, &Next);
    while (Node && (!Node->ComponentEntry.Identifier ||
                   _stricmp(Node->ComponentEntry.Identifier, "ACPI BIOS")))
    {
        Next = Node;
        Node = KeFindConfigurationNextEntry(KeLoaderBlock->ConfigurationRoot,
                                            AdapterClass, MultiFunctionAdapter, NULL, &Next);
    }
    if (!Node || !Node->ConfigurationData || Node->ComponentEntry.ConfigurationDataLength <
        FIELD_OFFSET(CM_PARTIAL_RESOURCE_LIST, PartialDescriptors[1]) +
        FIELD_OFFSET(ACPI_BIOS_MULTI_NODE, E820Entry)) return STATUS_NOT_FOUND;
    List = Node->ConfigurationData;
    if (List->Count != 1 || List->PartialDescriptors[0].Type != CmResourceTypeDeviceSpecific)
        return STATUS_DATA_ERROR;
    Bios = (PVOID)&List->PartialDescriptors[1];
    Address = Bios->RsdtAddress;
    Status = HalGetMemoryCachingRequirements(Address, sizeof(DESCRIPTION_HEADER), &Cache);
    if (!NT_SUCCESS(Status)) return Status;
    if (Cache != MmCached) return STATUS_DATA_ERROR;
    Mapping = MmMapIoSpace(Address, sizeof(DESCRIPTION_HEADER), Cache);
    if (!Mapping) return STATUS_INSUFFICIENT_RESOURCES;
    Expected = Mapping[1] | ((ULONG)Mapping[2] << 8) | ((ULONG)Mapping[3] << 16) | ((ULONG)Mapping[4] << 24);
    __asm__ __volatile__("ldr %0, [%1]" : "=r"(Value) : "r"(Mapping + 1) : "memory");
    MmUnmapIoSpace(Mapping, sizeof(DESCRIPTION_HEADER));
    if (Value != Expected) return STATUS_DATA_ERROR;
    Address.QuadPart = EXYNOS_MCT_PHYSICAL;
    Status = HalGetMemoryCachingRequirements(Address, sizeof(ULONG), &Cache);
    if (!NT_SUCCESS(Status)) return Status;
    if (Cache != MmNonCached) return STATUS_DATA_ERROR;
    Address.QuadPart = EXYNOS_BOOT_LOG_PHYSICAL;
    Status = HalGetMemoryCachingRequirements(Address, sizeof(ULONG), &Cache);
    if (!NT_SUCCESS(Status)) return Status;
    return Cache == MmNonCached ? STATUS_SUCCESS : STATUS_DATA_ERROR;
}

static NTSTATUS
ArmN7000ClockTest(VOID)
{
    LARGE_INTEGER Previous, Current, Frequency, Delay;
    ULONGLONG FirstTime, PreviousTime, CurrentTime;
    ULONG Index;

    Previous = KeQueryPerformanceCounter(&Frequency);
    if (Frequency.QuadPart != EXYNOS_MCT_RATE) return STATUS_DATA_ERROR;
    FirstTime = PreviousTime = KeQueryInterruptTime();
    Delay.QuadPart = -10000;
    for (Index = 0; Index < 32; ++Index)
    {
        KeDelayExecutionThread(KernelMode, FALSE, &Delay);
        Current = KeQueryPerformanceCounter(NULL);
        CurrentTime = KeQueryInterruptTime();
        if (Current.QuadPart <= Previous.QuadPart || CurrentTime < PreviousTime ||
            CurrentTime - PreviousTime > 100000000) return STATUS_DATA_ERROR;
        Previous = Current;
        PreviousTime = CurrentTime;
    }
    return PreviousTime > FirstTime ? STATUS_SUCCESS : STATUS_DATA_ERROR;
}

static NTSTATUS
ArmN7000RtcTest(VOID)
{
    TIME_FIELDS Original, Set, Read;
    BOOLEAN OriginalValid;
    NTSTATUS Status;

    OriginalValid = HalQueryRealTimeClock(&Original) &&
                    Original.Year >= 1900 && Original.Year <= 2899 &&
                    Original.Month >= 1 && Original.Month <= 12 &&
                    Original.Day >= 1 && Original.Day <= 31;

    RtlZeroMemory(&Set, sizeof(Set));
    Set.Year = 2024;
    Set.Month = 2;
    Set.Day = 29;
    Set.Hour = 12;
    Set.Minute = 34;
    Set.Second = 50;
    Status = HalSetRealTimeClock(&Set) ? STATUS_SUCCESS : STATUS_DEVICE_NOT_READY;
    if (NT_SUCCESS(Status) && (!HalQueryRealTimeClock(&Read) ||
        Read.Year != 2024 || Read.Month != 2 || Read.Day != 29 ||
        Read.Hour != 12 || Read.Minute != 34 || Read.Second < 50 || Read.Second > 52))
        Status = STATUS_DATA_ERROR;
    if (OriginalValid && !HalSetRealTimeClock(&Original)) Status = STATUS_DEVICE_NOT_READY;
    return Status;
}

static NTSTATUS
ArmN7000StoragePass(PUCHAR Written, PUCHAR Read, ULONG Length, UCHAR Seed)
{
    UNICODE_STRING Name = RTL_CONSTANT_STRING(L"\\Device\\Harddisk0\\Partition1\\arm-n7000-test.bin");
    OBJECT_ATTRIBUTES Attributes;
    IO_STATUS_BLOCK IoStatus;
    HANDLE File;
    LARGE_INTEGER Offset;
    FILE_STANDARD_INFORMATION Information;
    ULONG Index;
    NTSTATUS Status;

    for (Index = 0; Index < Length; ++Index) Written[Index] = (UCHAR)(Index ^ Seed);
    InitializeObjectAttributes(&Attributes, &Name, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
    Status = ZwCreateFile(&File, FILE_READ_DATA | FILE_WRITE_DATA | SYNCHRONIZE,
                          &Attributes, &IoStatus, NULL, FILE_ATTRIBUTE_NORMAL, 0,
                          FILE_OVERWRITE_IF, FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE,
                          NULL, 0);
    if (!NT_SUCCESS(Status)) return Status;
    Offset.QuadPart = 0;
    Status = ZwWriteFile(File, NULL, NULL, NULL, &IoStatus, Written, Length, &Offset, NULL);
    if (NT_SUCCESS(Status) && IoStatus.Information != Length) Status = STATUS_DATA_ERROR;
    if (NT_SUCCESS(Status)) Status = ZwFlushBuffersFile(File, &IoStatus);
    ZwClose(File);
    if (!NT_SUCCESS(Status)) return Status;

    Status = ZwCreateFile(&File, FILE_READ_DATA | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
                          &Attributes, &IoStatus, NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ,
                          FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE,
                          NULL, 0);
    if (!NT_SUCCESS(Status)) return Status;
    Status = ZwQueryInformationFile(File, &IoStatus, &Information, sizeof(Information),
                                    FileStandardInformation);
    if (NT_SUCCESS(Status) && Information.EndOfFile.QuadPart != Length) Status = STATUS_DATA_ERROR;
    if (NT_SUCCESS(Status))
    {
        RtlZeroMemory(Read, Length);
        Status = ZwReadFile(File, NULL, NULL, NULL, &IoStatus, Read, Length, &Offset, NULL);
        if (NT_SUCCESS(Status) && (IoStatus.Information != Length ||
            RtlCompareMemory(Written, Read, Length) != Length)) Status = STATUS_DATA_ERROR;
    }
    ZwClose(File);
    return Status;
}

static NTSTATUS
ArmN7000StorageTest(VOID)
{
    PUCHAR Buffers;
    ULONG Round, Length;
    NTSTATUS Status = STATUS_SUCCESS;

    Buffers = ExAllocatePoolWithTag(NonPagedPool, 16384, 't7NA');
    if (!Buffers) return STATUS_INSUFFICIENT_RESOURCES;
    for (Round = 0; Round < 16; ++Round)
    {
        Length = Round % 3 == 0 ? 256 : Round % 3 == 1 ? 4096 : 8192;
        DbgPrint("ARM N7000 storage: overwrite round=%lu bytes=%lu irql=%u apcs=%u\n",
                 Round, Length, KeGetCurrentIrql(), KeAreAllApcsDisabled());
        Status = ArmN7000StoragePass(Buffers, Buffers + 8192, Length, (UCHAR)(Round ^ 0xa9));
        if (!NT_SUCCESS(Status)) break;
    }
    if (NT_SUCCESS(Status)) Status = ArmN7000StoragePass(Buffers, Buffers + 8192, 256, 0xa9);
    DbgPrint("ARM N7000 overwrite tests: %s (17 create/write/flush/reopen/read cycles)\n",
             NT_SUCCESS(Status) ? "PASS" : "FAIL");
    ExFreePoolWithTag(Buffers, 't7NA');
    return Status;
}

static VOID NTAPI
ArmN7000Reinitialize(PDRIVER_OBJECT DriverObject, PVOID Context, ULONG Count)
{
    NTSTATUS Status;
    UNREFERENCED_PARAMETER(DriverObject);
    UNREFERENCED_PARAMETER(Context);
    UNREFERENCED_PARAMETER(Count);

    {
        HANDLE Watch;
        PKTHREAD Thread = KeGetCurrentThread();
        ObReferenceObject(Thread);
        Status = PsCreateSystemThread(&Watch, THREAD_ALL_ACCESS, NULL, NULL, NULL, ArmN7000Watch, Thread);
        if (NT_SUCCESS(Status)) ZwClose(Watch);
        else ObDereferenceObject(Thread);
    }

    Status = ArmN7000ClockTest();
    DbgPrint("ARM N7000 clock tests: %s (0x%08lx)\n", NT_SUCCESS(Status) ? "PASS" : "FAIL", Status);
    if (NT_SUCCESS(Status)) Status = ArmN7000RtcTest();
    DbgPrint("ARM N7000 RTC tests: %s (0x%08lx)\n", NT_SUCCESS(Status) ? "PASS" : "FAIL", Status);
    if (NT_SUCCESS(Status)) Status = ArmN7000StorageTest();
    DbgPrint("ARM N7000 storage tests: %s (0x%08lx)\n", NT_SUCCESS(Status) ? "PASS" : "FAIL", Status);
}

NTSTATUS NTAPI
DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    NTSTATUS Status;
    UNREFERENCED_PARAMETER(RegistryPath);
    Status = ArmN7000CacheTest();
    DbgPrint("ARM N7000 ACPI cache tests: %s (0x%08lx)\n",
              NT_SUCCESS(Status) ? "PASS" : "FAIL", Status);
    if (NT_SUCCESS(Status)) IoRegisterBootDriverReinitialization(DriverObject, ArmN7000Reinitialize, NULL);
    return Status;
}
