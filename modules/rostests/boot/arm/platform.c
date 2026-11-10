/*
 * PROJECT:     LiberNT boot tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Inspect ARM32 firmware services before hardware bring-up
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <uefildr.h>
#include <stdarg.h>
#include <stdio.h>

static EFI_SYSTEM_TABLE *Firmware;
static EFI_SERIAL_IO_PROTOCOL *Serial;

static VOID ProbePrint(const CHAR *Format, ...)
{
    CHAR Buffer[256];
    CHAR16 Text[256];
    va_list Arguments;
    INT Length;
    UINTN Size, Index;
    EFI_STATUS Status;

    va_start(Arguments, Format);
    Length = _vsnprintf(Buffer, sizeof(Buffer) - 1, Format, Arguments);
    va_end(Arguments);
    if (Length < 0 || (UINTN)Length >= sizeof(Buffer)) Length = sizeof(Buffer) - 1;
    Buffer[Length] = 0;
    if (Serial)
    {
        Size = Length;
        Status = Serial->Write(Serial, &Size, Buffer);
        if (!EFI_ERROR(Status) && Size == (UINTN)Length) return;
    }
    if (!Firmware->ConOut || !Firmware->ConOut->OutputString) return;
    for (Index = 0; Index < (UINTN)Length; ++Index) Text[Index] = (UCHAR)Buffer[Index];
    Text[Length] = 0;
    Firmware->ConOut->OutputString(Firmware->ConOut, Text);
}

static const CHAR *ProbeTranslation(ULONG Mmfr0)
{
    switch (Mmfr0 & 15)
    {
        case 3:
        case 4: return "short-descriptor";
        case 5: return "long-descriptor-capable";
        default: return "unknown";
    }
}

static BOOLEAN ProbeCpu(VOID)
{
    ULONG Midr, Mpidr, Mmfr0, Pfr1, Sctlr, Ttbcr, Cpsr, Cpacr;

    if (strcmp(ProbeTranslation(0x00100103), "short-descriptor") ||
        strcmp(ProbeTranslation(5), "long-descriptor-capable") ||
        strcmp(ProbeTranslation(0), "unknown"))
        return FALSE;
    __asm__ __volatile__("mrc p15, 0, %0, c0, c0, 0" : "=r"(Midr));
    __asm__ __volatile__("mrc p15, 0, %0, c0, c0, 5" : "=r"(Mpidr));
    __asm__ __volatile__("mrc p15, 0, %0, c0, c1, 4" : "=r"(Mmfr0));
    __asm__ __volatile__("mrc p15, 0, %0, c0, c1, 1" : "=r"(Pfr1));
    __asm__ __volatile__("mrc p15, 0, %0, c1, c0, 0" : "=r"(Sctlr));
    __asm__ __volatile__("mrc p15, 0, %0, c2, c0, 2" : "=r"(Ttbcr));
    __asm__ __volatile__("mrs %0, cpsr" : "=r"(Cpsr));
    __asm__ __volatile__("mrc p15, 0, %0, c1, c0, 2" : "=r"(Cpacr));
    ProbePrint("CPU MIDR=%08lx MPIDR=%08lx part=%03lx\r\n", Midr, Mpidr, (Midr >> 4) & 0xfff);
    ProbePrint("CPU ID_MMFR0=%08lx translation=%s\r\n", Mmfr0, ProbeTranslation(Mmfr0));
    ProbePrint("CPU ID_PFR1=%08lx generic_timer=%lu\r\n", Pfr1, (Pfr1 >> 16) & 15);
    ProbePrint("CPU SCTLR=%08lx TTBCR=%08lx CPSR=%08lx\r\n", Sctlr, Ttbcr, Cpsr);
    ProbePrint("CPU CPACR=%08lx\r\n", Cpacr);
    ProbePrint("LiberNT LPAE kernel prerequisite: %s\r\n",
               (Mmfr0 & 15) == 5 ? "present" : "not established");
    return TRUE;
}

static VOID ProbeGraphics(VOID)
{
    EFI_GUID Guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Graphics;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    EFI_STATUS Status;

    Status = Firmware->BootServices->LocateProtocol(&Guid, NULL, (PVOID *)&Graphics);
    if (EFI_ERROR(Status) || !Graphics || !Graphics->Mode || !Graphics->Mode->Info)
    {
        ProbePrint("GOP unavailable: status=%08lx\r\n", (ULONG)Status);
        return;
    }
    Info = Graphics->Mode->Info;
    ProbePrint("GOP %lux%lu pitch=%lu format=%u\r\n", Info->HorizontalResolution,
               Info->VerticalResolution, Info->PixelsPerScanLine, Info->PixelFormat);
    ProbePrint("GOP framebuffer=%08lx%08lx size=%08lx\r\n",
               (ULONG)(Graphics->Mode->FrameBufferBase >> 32),
               (ULONG)Graphics->Mode->FrameBufferBase, (ULONG)Graphics->Mode->FrameBufferSize);
    ProbePrint("GOP masks R=%08lx G=%08lx B=%08lx X=%08lx\r\n",
               Info->PixelInformation.RedMask, Info->PixelInformation.GreenMask,
               Info->PixelInformation.BlueMask, Info->PixelInformation.ReservedMask);
}

static VOID ProbeStorage(EFI_HANDLE ImageHandle)
{
    EFI_GUID Guid = EFI_BLOCK_IO_PROTOCOL_GUID;
    EFI_GUID ImageGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_BLOCK_IO_PROTOCOL *Block;
    EFI_LOADED_IMAGE *Image;
    EFI_HANDLE *Handles = NULL;
    UINTN Count = 0, Index;
    EFI_STATUS Status;

    Status = Firmware->BootServices->HandleProtocol(ImageHandle, &ImageGuid, (PVOID *)&Image);
    if (!EFI_ERROR(Status) && Image)
        ProbePrint("Application base=%p size=%08lx%08lx device=%p\r\n", Image->ImageBase,
                   (ULONG)(Image->ImageSize >> 32), (ULONG)Image->ImageSize, Image->DeviceHandle);
    Status = Firmware->BootServices->LocateHandleBuffer(ByProtocol, &Guid, NULL, &Count, &Handles);
    ProbePrint("Block I/O handles=%lu status=%08lx\r\n", (ULONG)Count, (ULONG)Status);
    if (EFI_ERROR(Status)) return;
    for (Index = 0; Index < Count; ++Index)
    {
        Status = Firmware->BootServices->HandleProtocol(Handles[Index], &Guid, (PVOID *)&Block);
        if (EFI_ERROR(Status) || !Block || !Block->Media) continue;
        ProbePrint("Disk %lu handle=%p block=%lu last=%08lx%08lx present=%u removable=%u partition=%u readonly=%u\r\n",
                   (ULONG)Index, Handles[Index], Block->Media->BlockSize,
                   (ULONG)(Block->Media->LastBlock >> 32), (ULONG)Block->Media->LastBlock,
                   Block->Media->MediaPresent, Block->Media->RemovableMedia,
                   Block->Media->LogicalPartition, Block->Media->ReadOnly);
    }
    Firmware->BootServices->FreePool(Handles);
}

static EFI_STATUS ProbeMemory(VOID)
{
    EFI_MEMORY_DESCRIPTOR *Map = NULL, *Descriptor;
    UINTN Size = 0, Capacity, Key, Stride, Offset;
    UINT32 Version;
    EFI_STATUS Status;

    Status = Firmware->BootServices->GetMemoryMap(&Size, NULL, &Key, &Stride, &Version);
    if (Status != EFI_BUFFER_TOO_SMALL || Stride < sizeof(*Map) || Stride > MAXULONG / 8 ||
        Size > MAXULONG - 8 * Stride)
        return EFI_BAD_BUFFER_SIZE;
    for (;;)
    {
        Capacity = Size + 8 * Stride;
        Status = Firmware->BootServices->AllocatePool(EfiLoaderData, Capacity, (PVOID *)&Map);
        if (EFI_ERROR(Status)) return Status;
        Size = Capacity;
        Status = Firmware->BootServices->GetMemoryMap(&Size, Map, &Key, &Stride, &Version);
        if (Status != EFI_BUFFER_TOO_SMALL) break;
        Firmware->BootServices->FreePool(Map);
        Map = NULL;
        if (Stride < sizeof(*Map) || Stride > MAXULONG / 8 ||
            Size > MAXULONG - 8 * Stride) return EFI_BAD_BUFFER_SIZE;
    }
    if (!EFI_ERROR(Status) && Stride >= sizeof(*Map) && Size <= Capacity && !(Size % Stride))
    {
        ProbePrint("UEFI memory map entries=%lu stride=%lu version=%lu\r\n",
                   (ULONG)(Size / Stride), (ULONG)Stride, (ULONG)Version);
        for (Offset = 0; Offset < Size; Offset += Stride)
        {
            Descriptor = (PVOID)((PUCHAR)Map + Offset);
            ProbePrint("Memory type=%lu base=%08lx%08lx pages=%08lx%08lx attr=%08lx%08lx\r\n",
                       Descriptor->Type, (ULONG)(Descriptor->PhysicalStart >> 32),
                       (ULONG)Descriptor->PhysicalStart, (ULONG)(Descriptor->NumberOfPages >> 32),
                       (ULONG)Descriptor->NumberOfPages, (ULONG)(Descriptor->Attribute >> 32),
                       (ULONG)Descriptor->Attribute);
        }
    }
    else if (!EFI_ERROR(Status))
        Status = EFI_BAD_BUFFER_SIZE;
    Firmware->BootServices->FreePool(Map);
    return Status;
}

EFI_STATUS EfiEntry(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_GUID SerialGuid = EFI_SERIAL_IO_PROTOCOL_GUID;
    EFI_GUID AcpiGuid = EFI_ACPI_20_TABLE_GUID;
    EFI_STATUS Status;
    UINTN Index;
    EFI_CONFIGURATION_TABLE *Table;

    Firmware = SystemTable;
    if (!Firmware || !Firmware->BootServices) return EFI_INVALID_PARAMETER;
    Status = Firmware->BootServices->LocateProtocol(&SerialGuid, NULL, (PVOID *)&Serial);
    if (EFI_ERROR(Status) || (Serial && !Serial->Write)) Serial = NULL;
    ProbePrint("ARM platform probe: BEGIN (boot services only)\r\n");
    ProbePrint("UEFI revision=%08lx firmware_revision=%08lx serial=%u\r\n",
               Firmware->Hdr.Revision, Firmware->FirmwareRevision, Serial != NULL);
    if (!ProbeCpu()) return EFI_ABORTED;
    ProbeGraphics();
    ProbeStorage(ImageHandle);
    for (Index = 0; Index < Firmware->NumberOfTableEntries; ++Index)
    {
        Table = &Firmware->ConfigurationTable[Index];
        ProbePrint("Configuration table %08lx-%04x-%04x address=%p%s\r\n",
                   Table->VendorGuid.Data1, Table->VendorGuid.Data2, Table->VendorGuid.Data3,
                   Table->VendorTable, memcmp(&Table->VendorGuid, &AcpiGuid, sizeof(AcpiGuid)) ? "" : " ACPI2");
    }
    Status = ProbeMemory();
    if (EFI_ERROR(Status))
    {
        ProbePrint("ARM platform probe: FAIL memory map status=%08lx\r\n", (ULONG)Status);
        return Status;
    }
    ProbePrint("ARM platform probe: COMPLETE (firmware only; returning to UEFI)\r\n");
    return EFI_SUCCESS;
}
