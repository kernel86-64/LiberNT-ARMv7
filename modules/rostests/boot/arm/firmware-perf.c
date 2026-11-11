/*
 * PROJECT:     LiberNT boot tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Measure N7000 and QEMU virt firmware service costs
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <uefildr.h>
#include <stdarg.h>
#include <stdio.h>

static EFI_SYSTEM_TABLE *Firmware;
static EFI_SERIAL_IO_PROTOCOL *Serial;
static BOOLEAN GenericTimer;
static ULONG Frequency;

static UINT64 Counter(VOID)
{
    UINT64 Value;
    ULONG High, Low, Again;
    volatile ULONG *Mct = (PVOID)0x10050100;

    if (GenericTimer)
    {
        __asm__ __volatile__("isb\n\tmrrc p15, 1, %Q0, %R0, c14" : "=r"(Value) :: "memory");
        return Value;
    }
    do
    {
        High = Mct[1];
        Low = Mct[0];
        Again = Mct[1];
    } while (High != Again);
    return ((UINT64)High << 32) | Low;
}

static VOID Print(const CHAR *Format, ...)
{
    CHAR Buffer[256];
    CHAR16 Text[256];
    INT Length;
    UINTN Size, Index;
    va_list Args;

    va_start(Args, Format);
    Length = _vsnprintf(Buffer, sizeof(Buffer) - 1, Format, Args);
    va_end(Args);
    if (Length < 0 || Length >= sizeof(Buffer)) Length = sizeof(Buffer) - 1;
    Buffer[Length] = 0;
    if (Serial)
    {
        Size = Length;
        if (!EFI_ERROR(Serial->Write(Serial, &Size, Buffer)) && Size == (UINTN)Length) return;
    }
    for (Index = 0; Index < (UINTN)Length; ++Index) Text[Index] = (UCHAR)Buffer[Index];
    Text[Length] = 0;
    if (Firmware->ConOut) Firmware->ConOut->OutputString(Firmware->ConOut, Text);
}

static VOID Report(const CHAR *Name, UINT64 Ticks, EFI_STATUS Status)
{
    UINT64 Microseconds = Ticks * 1000000 / Frequency;
    Print("PERF %s us=%lu%09lu status=%08lx\r\n", Name,
          (ULONG)(Microseconds / 1000000000), (ULONG)(Microseconds % 1000000000), (ULONG)Status);
}

EFI_STATUS EfiEntry(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_GUID SerialGuid = EFI_SERIAL_IO_PROTOCOL_GUID;
    EFI_GUID GraphicsGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Graphics;
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Pixels;
    EFI_STATUS Status;
    UINT64 Start, Elapsed;
    ULONG Midr, Pfr1, Sctlr, Ttbcr, Ttbr, Actlr;
    ULONG Pmcr, Enables, Cycles, EndCycles;
    PVOID RamSource, RamTarget;
    ULONG Index, Descriptor = 0;
    UINTN Size, Width, Height;
    CHAR Sample[] = "Firmware serial performance sample 0123456789\r\n";

    Firmware = SystemTable;
    if (!Firmware || !Firmware->BootServices) return EFI_INVALID_PARAMETER;
    Firmware->BootServices->LocateProtocol(&SerialGuid, NULL, (PVOID *)&Serial);
    __asm__ __volatile__("mrc p15, 0, %0, c0, c0, 0" : "=r"(Midr));
    __asm__ __volatile__("mrc p15, 0, %0, c0, c1, 1" : "=r"(Pfr1));
    __asm__ __volatile__("mrc p15, 0, %0, c1, c0, 0" : "=r"(Sctlr));
    __asm__ __volatile__("mrc p15, 0, %0, c2, c0, 2" : "=r"(Ttbcr));
    __asm__ __volatile__("mrc p15, 0, %0, c1, c0, 1" : "=r"(Actlr));
    GenericTimer = ((Pfr1 >> 16) & 15) != 0;
    if (GenericTimer)
        __asm__ __volatile__("mrc p15, 0, %0, c14, c0, 0" : "=r"(Frequency));
    else
    {
        if ((Midr & 0xff0ffff0) != 0x410fc090) return EFI_UNSUPPORTED;
        Frequency = 24000000;
    }
    if (!Frequency) return EFI_UNSUPPORTED;
    Print("Firmware performance: BEGIN MIDR=%08lx SCTLR=%08lx TTBCR=%08lx counter_hz=%lu\r\n",
          Midr, Sctlr, Ttbcr, Frequency);
    Print("PERF ACTLR=%08lx\r\n", Actlr);
    __asm__ __volatile__("mrc p15, 0, %0, c9, c12, 0" : "=r"(Pmcr));
    __asm__ __volatile__("mrc p15, 0, %0, c9, c12, 1" : "=r"(Enables));
    __asm__ __volatile__("mrc p15, 0, %0, c9, c13, 0" : "=r"(Cycles));
    Start = Counter();
    Status = Firmware->BootServices->Stall(100000);
    Elapsed = Counter() - Start;
    __asm__ __volatile__("mrc p15, 0, %0, c9, c13, 0" : "=r"(EndCycles));
    Report("Stall100ms", Elapsed, Status);
    if ((Pmcr & 1) && (Enables & 0x80000000) && Elapsed)
        Print("PERF observed_CPU_MHz=%lu\r\n", (ULONG)((UINT64)(ULONG)(EndCycles - Cycles) *
              ((Pmcr & 8) ? 64 : 1) * Frequency / Elapsed / 1000000));
    Status = Firmware->BootServices->AllocatePool(EfiLoaderData, 2 * 1024 * 1024, &RamSource);
    if (!EFI_ERROR(Status))
    {
        if ((Sctlr & 1) && !Ttbcr)
        {
            __asm__ __volatile__("mrc p15, 0, %0, c2, c0, 0" : "=r"(Ttbr));
            Descriptor = ((volatile ULONG *)(ULONG_PTR)(Ttbr & 0xffffc000))[(ULONG_PTR)RamSource >> 20];
            Print("PERF RAM address=%08lx L1=%08lx\r\n", (ULONG)(ULONG_PTR)RamSource, Descriptor);
            if ((Descriptor & 3) == 1)
            {
                Descriptor = ((volatile ULONG *)(ULONG_PTR)(Descriptor & 0xfffffc00))[((ULONG_PTR)RamSource >> 12) & 255];
                Print("PERF RAM L2=%08lx\r\n", Descriptor);
            }
        }
        RamTarget = (PUCHAR)RamSource + 1024 * 1024;
        memset(RamSource, 0x5a, 1024 * 1024);
        Start = Counter();
        for (Index = 0; Index < 4; ++Index)
            Firmware->BootServices->CopyMem(RamTarget, RamSource, 1024 * 1024);
        Report("RAMcopy4MiB", Counter() - Start, EFI_SUCCESS);
        Firmware->BootServices->FreePool(RamSource);
    }
    if (Serial)
    {
        Start = Counter();
        Status = EFI_SUCCESS;
        for (Index = 0; Index < 16; ++Index)
        {
            Size = sizeof(Sample) - 1;
            Status = Serial->Write(Serial, &Size, Sample);
            if (EFI_ERROR(Status) || Size != sizeof(Sample) - 1) break;
        }
        Elapsed = Counter() - Start;
        Report("Serial16lines", Elapsed, Status);
    }
    Status = Firmware->BootServices->LocateProtocol(&GraphicsGuid, NULL, (PVOID *)&Graphics);
    if (!EFI_ERROR(Status) && Graphics && Graphics->Mode && Graphics->Mode->Info && Graphics->Blt)
    {
        Width = Graphics->Mode->Info->HorizontalResolution;
        Height = Graphics->Mode->Info->VerticalResolution;
        Print("PERF GOP width=%lu height=%lu framebuffer=%08lx\r\n",
              (ULONG)Width, (ULONG)Height, (ULONG)Graphics->Mode->FrameBufferBase);
        if ((Sctlr & 1) && !Ttbcr && Graphics->Mode->FrameBufferBase <= MAXULONG)
        {
            __asm__ __volatile__("mrc p15, 0, %0, c2, c0, 0" : "=r"(Ttbr));
            Descriptor = ((volatile ULONG *)(ULONG_PTR)(Ttbr & 0xffffc000))[(ULONG)Graphics->Mode->FrameBufferBase >> 20];
            Print("PERF framebuffer L1=%08lx\r\n", Descriptor);
            if ((Descriptor & 3) == 1)
            {
                Descriptor = ((volatile ULONG *)(ULONG_PTR)(Descriptor & 0xfffffc00))[((ULONG)Graphics->Mode->FrameBufferBase >> 12) & 255];
                Print("PERF framebuffer L2=%08lx\r\n", Descriptor);
            }
        }
        if (Width && Height && Width <= 4096 && Height <= 4096)
        {
            Size = Width * Height * sizeof(*Pixels);
            Status = Firmware->BootServices->AllocatePool(EfiLoaderData, Size, (PVOID *)&Pixels);
            if (!EFI_ERROR(Status))
            {
                Start = Counter();
                Status = Graphics->Blt(Graphics, Pixels, EfiBltVideoToBltBuffer, 0, 0, 0, 0, Width, Height, 0);
                Elapsed = Counter() - Start;
                if (!EFI_ERROR(Status))
                {
                    Start = Counter();
                    Status = Graphics->Blt(Graphics, Pixels, EfiBltBufferToVideo, 0, 0, 0, 0, Width, Height, 0);
                    Report("GOPrestore", Counter() - Start, Status);
                }
                Report("GOPread", Elapsed, Status);
                Firmware->BootServices->FreePool(Pixels);
            }
        }
    }
    Print("Firmware performance: COMPLETE (firmware only)\r\n");
    return EFI_SUCCESS;
}
