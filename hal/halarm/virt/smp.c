/*
 * PROJECT:     LiberNT HAL
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARM32 ACPI parking-protocol processor startup
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <hal.h>
#include <armvirt.h>
#include <reactos/arm/exynos4boot.h>

#define HALP_ARM_MAX_CPUS          2
#define HALP_ARM_GICD_SGIR         0xF00
#define HALP_ARM_SGI_IPI           2
#define HALP_ARM_SGI_CLOCK         3
#define HALP_ARM_SCU_CONTROL       0x0000
#define HALP_ARM_SCU_ENABLE        0x0001
#define HALP_ARM_MB_JUMP_ADDRESS   0x008
#define HALP_ARM_MB_BOOT_DATA      0x010
#define HALP_ARM_MADT_HEADER_SIZE  (sizeof(DESCRIPTION_HEADER) + 2 * sizeof(ULONG))

typedef struct _HALP_ARM_GICC
{
    ULONG Mpidr;
    ULONG GicId;
    ULONG Uid;
    ULONG Flags;
    ULONG ParkingVersion;
    ULONG ParkedAddress;
} HALP_ARM_GICC, *PHALP_ARM_GICC;

#pragma pack(push, 1)
typedef struct _HALP_ARM_MADT_GICC
{
    UCHAR Type;
    UCHAR Length;
    USHORT Reserved;
    ULONG CpuInterfaceNumber;
    ULONG Uid;
    ULONG Flags;
    ULONG ParkingVersion;
    ULONG PerformanceInterrupt;
    ULONGLONG ParkedAddress;
    ULONGLONG BaseAddress;
    ULONGLONG GicvBaseAddress;
    ULONGLONG GichBaseAddress;
    ULONG VgicMaintenanceInterrupt;
    ULONGLONG GicrBaseAddress;
    ULONGLONG Mpidr;
} HALP_ARM_MADT_GICC, *PHALP_ARM_MADT_GICC;
#pragma pack(pop)

C_ASSERT(FIELD_OFFSET(HALP_ARM_MADT_GICC, Mpidr) == 68);
C_ASSERT(sizeof(HALP_ARM_MADT_GICC) == 76);

static HALP_ARM_GICC HalpArmGicc[HALP_ARM_MAX_CPUS];
static ULONG HalpArmCpuCount;
static ULONG HalpArmNextProcessor = 1;

static ULONG
HalpArmRead(
    ULONG Base,
    ULONG Offset)
{
    ULONG Value = *(volatile ULONG *)(Base + Offset);
    __asm__ __volatile__("dmb sy" ::: "memory");
    return Value;
}

static VOID
HalpArmWrite(
    ULONG Base,
    ULONG Offset,
    ULONG Value)
{
    __asm__ __volatile__("dmb sy" ::: "memory");
    *(volatile ULONG *)(Base + Offset) = Value;
    __asm__ __volatile__("dsb sy" ::: "memory");
}

static VOID
HalpArmApplyErrata(
    VOID)
{
    ULONG Midr, Variant, Revision, Mask = 1UL << 22, Control;

    __asm__ __volatile__("mrc p15, 0, %0, c0, c0, 0" : "=r"(Midr));
    if ((Midr & 0xff0ffff0UL) != 0x410fc090UL)
        return;
    Variant = (Midr >> 20) & 15;
    Revision = Midr & 15;
    if (Variant <= 2)
        Mask |= 1UL << 11;
    if ((Variant == 1 && Revision <= 3) || (Variant == 2 && Revision <= 2))
        Mask |= 1UL << 4;
    __asm__ __volatile__("mrc p15, 0, %0, c15, c0, 1" : "=r"(Control));
    if ((Control & Mask) != Mask)
    {
        Control |= Mask;
        __asm__ __volatile__("mcr p15, 0, %0, c15, c0, 1\n\tisb" :: "r"(Control) : "memory");
    }
    __asm__ __volatile__("mrc p15, 0, %0, c15, c0, 1" : "=r"(Control));
    if ((Control & Mask) != Mask)
        KeBugCheckEx(HAL_INITIALIZATION_FAILED, Midr, Mask, Control, 0);
}

static VOID
HalpArmEnableSmp(
    VOID)
{
    ULONG Control;

    __asm__ __volatile__("mrc p15, 0, %0, c1, c0, 1" : "=r" (Control));
    Control |= 1UL << 6;
    if (HalpExynos)
        Control |= 1UL;
    __asm__ __volatile__("mcr p15, 0, %0, c1, c0, 1\n\tdsb sy\n\tisb" :: "r" (Control) : "memory");
}

static VOID
HalpArmEnableScu(
    VOID)
{
    ULONG Control = HalpArmRead(ARM_VIRT_SCU_VIRTUAL, HALP_ARM_SCU_CONTROL);

    if (!(Control & HALP_ARM_SCU_ENABLE))
        HalpArmWrite(ARM_VIRT_SCU_VIRTUAL, HALP_ARM_SCU_CONTROL, Control | HALP_ARM_SCU_ENABLE);
}

static VOID
HalpArmParseMadt(
    PLOADER_PARAMETER_BLOCK LoaderBlock)
{
    PDESCRIPTION_HEADER Header;
    PUCHAR Entry;
    ULONG Offset;

    HalpArmCpuCount = 0;
    Header = HalAcpiGetTable(LoaderBlock, APIC_SIGNATURE);
    if (!Header || Header->Length < sizeof(*Header))
        return;

    for (Offset = HALP_ARM_MADT_HEADER_SIZE; Offset + 2 <= Header->Length; Offset += Entry[1])
    {
        PHALP_ARM_MADT_GICC Gicc;

        Entry = (PUCHAR)Header + Offset;
        if (Entry[1] < 2 || Offset + Entry[1] > Header->Length)
            break;
        if (Entry[0] != 0x0B ||
            Entry[1] < FIELD_OFFSET(HALP_ARM_MADT_GICC, Mpidr) + sizeof(ULONGLONG))
            continue;
        Gicc = (PHALP_ARM_MADT_GICC)Entry;
        if (!(Gicc->Flags & 1) || HalpArmCpuCount == HALP_ARM_MAX_CPUS)
            continue;
        HalpArmGicc[HalpArmCpuCount].Mpidr = (ULONG)Gicc->Mpidr;
        HalpArmGicc[HalpArmCpuCount].GicId = Gicc->CpuInterfaceNumber;
        HalpArmGicc[HalpArmCpuCount].Uid = Gicc->Uid;
        HalpArmGicc[HalpArmCpuCount].Flags = Gicc->Flags;
        HalpArmGicc[HalpArmCpuCount].ParkingVersion = Gicc->ParkingVersion;
        HalpArmGicc[HalpArmCpuCount].ParkedAddress =
            Gicc->ParkedAddress <= MAXULONG ? (ULONG)Gicc->ParkedAddress : 0;
        HalpArmCpuCount++;
    }

    if (!HalpArmCpuCount)
    {
        HalpArmGicc[0].GicId = 0;
        HalpArmGicc[0].Uid = 0;
        HalpArmCpuCount = 1;
    }
}

ULONG
HalpArmProcessorCount(
    VOID)
{
    return HalpArmCpuCount ? HalpArmCpuCount : 1;
}

ULONG
HalpArmProcessorGicId(
    ULONG ProcessorNumber)
{
    if (ProcessorNumber >= HalpArmCpuCount)
        return ProcessorNumber;
    return HalpArmGicc[ProcessorNumber].GicId;
}

VOID
HalpArmSendSgi(
    ULONG TargetList,
    ULONG Sgi)
{
    if (TargetList && Sgi < 16)
        HalpArmWrite(HalpArmGicDistBase(KeGetCurrentProcessorNumber()), HALP_ARM_GICD_SGIR,
                     Sgi | (TargetList << 16));
}

VOID
HalpArmSendClockIpi(
    VOID)
{
    static BOOLEAN Logged;
    ULONG Index;
    ULONG Self = KeGetCurrentProcessorNumber();
    ULONG TargetList = 0;

    for (Index = 0; Index < HalpArmCpuCount && Index < 32; Index++)
    {
        if (Index != Self)
            TargetList |= 1UL << (HalpArmGicc[Index].GicId & 0x1F);
    }
    if (!Logged)
    {
        Logged = TRUE;
        DbgPrint("ARM SMP: clock IPI cpu=%lu targets=%lx\n", Self, TargetList);
    }
    HalpArmSendSgi(TargetList, HALP_ARM_SGI_CLOCK);
}

VOID
HalpArmInitializeProcessor(
    ULONG ProcessorNumber)
{
    ULONG Base = HalpArmGicCpuBase(ProcessorNumber);
    ULONG Distributor = HalpArmGicDistBase(ProcessorNumber);

    HalpArmApplyErrata();
    HalpArmWrite(Base, 0x000, 0);
    HalpArmWrite(Base, 0x008, 0);
    HalpArmWrite(Distributor, 0x400, ((15UL - CLOCK_LEVEL) << 28) | 0x0010E0D0);
    HalpArmWrite(Distributor, 0x100, 0xF);
    HalpArmWrite(Base, 0x004, 0xFF);
    HalpArmWrite(Base, 0x000, 1);
}

VOID
HalpArmInitializeSmp(
    PLOADER_PARAMETER_BLOCK LoaderBlock)
{
    ULONG Index;

    if (HalpExynos)
    {
        HalpArmEnableScu();
        HalpArmParseMadt(LoaderBlock);
    }
    else
    {
        HalpArmCpuCount = 1;
        HalpArmGicc[0].GicId = 0;
        HalpArmGicc[0].Uid = 0;
    }

    for (Index = 0; Index < HalpArmCpuCount; Index++)
    {
        DbgPrint("ARM SMP: CPU%lu MPIDR=%08lx GIC=%lu UID=%lu park=%lx/%lu\n",
                Index,
                HalpArmGicc[Index].Mpidr,
                HalpArmGicc[Index].GicId,
                HalpArmGicc[Index].Uid,
                HalpArmGicc[Index].ParkedAddress,
                HalpArmGicc[Index].ParkingVersion);
    }

    HalpArmEnableSmp();
}

VOID
NTAPI
HalRequestIpi(
    KAFFINITY TargetProcessors)
{
    ULONG Index;
    ULONG TargetList = 0;

    for (Index = 0; Index < HalpArmCpuCount && Index < 32; Index++)
    {
        if (TargetProcessors & ((KAFFINITY)1 << Index))
            TargetList |= 1UL << (HalpArmGicc[Index].GicId & 0x1F);
    }
    HalpArmSendSgi(TargetList, HALP_ARM_SGI_IPI);
}

BOOLEAN
NTAPI
HalStartNextProcessor(
    PLOADER_PARAMETER_BLOCK LoaderBlock,
    PKPROCESSOR_STATE ProcessorState)
{
    PHYSICAL_ADDRESS Physical;
    volatile ULONG *Mailbox;
    ULONG Index;

    UNREFERENCED_PARAMETER(LoaderBlock);

    if (!ProcessorState || !ProcessorState->ContextFrame.Pc ||
        !ProcessorState->ContextFrame.R0)
        return FALSE;

    for (Index = HalpArmNextProcessor; Index < HalpArmCpuCount; Index++)
    {
        if (HalpArmGicc[Index].ParkingVersion != 0 &&
            HalpArmGicc[Index].ParkedAddress != 0)
            break;
    }
    if (Index == HalpArmCpuCount)
        return FALSE;

    Physical.QuadPart = HalpArmGicc[Index].ParkedAddress;
    Mailbox = MmMapIoSpace(Physical, PAGE_SIZE, MmNonCached);
    if (!Mailbox)
        return FALSE;

    Mailbox[HALP_ARM_MB_BOOT_DATA / sizeof(ULONG)] = (ULONG)ProcessorState->ContextFrame.R0;
    Mailbox[HALP_ARM_MB_JUMP_ADDRESS / sizeof(ULONG) + 1] = 0;
    Mailbox[HALP_ARM_MB_JUMP_ADDRESS / sizeof(ULONG)] = (ULONG)ProcessorState->ContextFrame.Pc;
    Mailbox[0] = HalpArmGicc[Index].GicId;
    KeMemoryBarrier();

    DbgPrint("ARM SMP: releasing CPU%lu through mailbox %lx\n",
            Index, HalpArmGicc[Index].ParkedAddress);
    HalpArmSendSgi(1UL << (HalpArmGicc[Index].GicId & 0x1F), 1);
    __asm__ volatile ("sev" ::: "memory");
    MmUnmapIoSpace((PVOID)Mailbox, PAGE_SIZE);

    HalpArmNextProcessor = Index + 1;
    return TRUE;
}
