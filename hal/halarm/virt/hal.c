/*
 * PROJECT:     LiberNT HAL
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     QEMU virt ARMv7 bootstrap platform services
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <hal.h>
#include <armvirt.h>
#include <reactos/arm/exynos4boot.h>
#include <reactos/arm/exynos4mct.h>
#include <reactos/arm/exynos4rtc.h>
#include <hal/acpi_pci.h>
#define HALP_ARM_HW_REDUCED_ACPI (1UL << 20)
#undef DbgPrint
#undef KeGetCurrentIrql
#undef READ_PORT_UCHAR
#undef WRITE_PORT_UCHAR
#undef READ_PORT_USHORT
#undef READ_PORT_ULONG
#undef WRITE_PORT_USHORT
#undef WRITE_PORT_ULONG

static ULONG HalpInterruptCount;
static ULONG HalpIncrement = 100000;
static ULONG HalpInterruptToken;
static ULONG HalpTimerFrequency;
static ULONG HalpTimerIncrement;
static ULONGLONG HalpTimerDeadline;
static BOOLEAN HalpExynos;

static ULONG HalpClockVector(VOID)
{
    return HalpExynos ? EXYNOS_MCT_VECTOR : ARM_VIRT_TIMER_VECTOR;
}

ULONG NTAPI HalpQueryReservedResources(PCM_PARTIAL_RESOURCE_DESCRIPTOR Descriptor)
{
    if (!HalpExynos) return 0;
    if (Descriptor)
    {
        RtlZeroMemory(Descriptor, sizeof(*Descriptor));
        Descriptor->Type = CmResourceTypeInterrupt;
        Descriptor->ShareDisposition = CmResourceShareDeviceExclusive;
        Descriptor->Flags = CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE;
        Descriptor->u.Interrupt.Level = HalpClockVector();
        Descriptor->u.Interrupt.Vector = HalpClockVector();
        Descriptor->u.Interrupt.Affinity = 1;
    }
    return 1;
}

VOID FASTCALL KeUpdateSystemTime(PKTRAP_FRAME TrapFrame, ULONG Increment, KIRQL Irql);

static VOID HalpSetTimerDeadline(ULONGLONG Deadline)
{
    if (HalpExynos)
    {
        ULONG Period = max((ULONGLONG)HalpTimerFrequency * HalpTimerIncrement / 10000000, 1);
        if (!ExynosMctComparator(EXYNOS_MCT_VIRTUAL, Deadline, Period))
            KeBugCheckEx(HAL_INITIALIZATION_FAILED, EXYNOS_MCT_PHYSICAL, 0x240, 0, 0);
        return;
    }
    __asm__ __volatile__("mcrr p15, 3, %0, %1, c14\n\tisb" ::
                         "r" ((ULONG)Deadline), "r" ((ULONG)(Deadline >> 32)) : "memory");
}

static ULONG HalpRead(ULONG Base, ULONG Offset)
{
    ULONG Value = *(volatile ULONG *)(Base + Offset);
    __asm__ __volatile__("dmb sy" ::: "memory");
    return Value;
}

static VOID HalpWrite(ULONG Base, ULONG Offset, ULONG Value)
{
    __asm__ __volatile__("dmb sy" ::: "memory");
    *(volatile ULONG *)(Base + Offset) = Value;
    __asm__ __volatile__("dsb sy" ::: "memory");
}

KIRQL NTAPI KeGetCurrentIrql(VOID)
{
    return KeGetPcr()->CurrentIrql;
}

static VOID HalpSetIrql(KIRQL Irql)
{
    ULONG Cpsr;

    __asm__ __volatile__("mrs %0, cpsr\n\tcpsid i" : "=r" (Cpsr) :: "memory");
    KeGetPcr()->CurrentIrql = Irql;
    HalpWrite(ARM_VIRT_GIC_CPU_VIRTUAL, 4, Irql == 0 ? 0xFF : ((15 - Irql) << 4));
    __asm__ __volatile__("isb" ::: "memory");
    __asm__ __volatile__("msr cpsr_c, %0" :: "r" (Cpsr) : "memory");
}

KIRQL FASTCALL KfRaiseIrql(KIRQL NewIrql)
{
    KIRQL OldIrql = KeGetCurrentIrql();
    if (NewIrql < OldIrql || NewIrql > HIGH_LEVEL)
        KeBugCheckEx(IRQL_NOT_GREATER_OR_EQUAL, OldIrql, NewIrql, 0, 0);
    HalpSetIrql(NewIrql);
    return OldIrql;
}

VOID FASTCALL KfLowerIrql(KIRQL NewIrql)
{
    KIRQL OldIrql = KeGetCurrentIrql();
    if (NewIrql > OldIrql)
        KeBugCheckEx(IRQL_NOT_LESS_OR_EQUAL, OldIrql, NewIrql, 0, 0);
    HalpSetIrql(NewIrql);
}

KIRQL NTAPI KeRaiseIrqlToDpcLevel(VOID) { return KfRaiseIrql(DISPATCH_LEVEL); }
KIRQL NTAPI KeRaiseIrqlToSynchLevel(VOID) { return KfRaiseIrql(DISPATCH_LEVEL); }

VOID NTAPI HalInitializeProcessor(ULONG ProcessorNumber, PLOADER_PARAMETER_BLOCK LoaderBlock)
{
    ULONG Index;
    HalpExynos = ExynosBootLogMapped();
    if (ProcessorNumber != 0)
        KeBugCheckEx(HAL_INITIALIZATION_FAILED, ProcessorNumber, 0, 0, 0);
    HalpWrite(ARM_VIRT_GIC_CPU_VIRTUAL, 0, 0);
    HalpWrite(ARM_VIRT_GIC_DIST_VIRTUAL, 0, 0);
    HalpInterruptCount = ((HalpRead(ARM_VIRT_GIC_DIST_VIRTUAL, 4) & 31) + 1) * 32;
    for (Index = 0; Index < HalpInterruptCount; Index += 32)
    {
        HalpWrite(ARM_VIRT_GIC_DIST_VIRTUAL, 0x180 + Index / 8, ~0UL);
        HalpWrite(ARM_VIRT_GIC_DIST_VIRTUAL, 0x280 + Index / 8, ~0UL);
    }
    for (Index = 0; Index < HalpInterruptCount; Index += 4)
        HalpWrite(ARM_VIRT_GIC_DIST_VIRTUAL, 0x400 + Index, 0xA0A0A0A0);
    HalpWrite(ARM_VIRT_GIC_DIST_VIRTUAL, 0x400, 0xA0A0E0D0);
    HalpWrite(ARM_VIRT_GIC_CPU_VIRTUAL, 8, 0);
    HalpSetIrql(KeGetCurrentIrql());
    HalpWrite(ARM_VIRT_GIC_DIST_VIRTUAL, 0, 1);
    HalpWrite(ARM_VIRT_GIC_CPU_VIRTUAL, 0, 1);
}

BOOLEAN NTAPI HalInitSystem(ULONG BootPhase, PLOADER_PARAMETER_BLOCK LoaderBlock)
{
    UNREFERENCED_PARAMETER(LoaderBlock);
    if (BootPhase > 1)
        return FALSE;
    if (BootPhase == 0)
    {
        HalGetDmaAdapter = HalpArmGetDmaAdapter;
        KeSetTimeIncrement(100000, 10000);
        if (HalpExynos)
        {
            PFADT Fadt;
            if (!NT_SUCCESS(HalpAcpiTableCacheInit(LoaderBlock))) return FALSE;
            Fadt = HalAcpiGetTable(LoaderBlock, FADT_SIGNATURE);
            if (!Fadt || Fadt->Header.Length < RTL_SIZEOF_THROUGH_FIELD(FADT, flags) ||
                !(Fadt->flags & HALP_ARM_HW_REDUCED_ACPI)) return FALSE;
            HalGetCachedAcpiTable = HaliGetCachedAcpiTable;
            HalInitPnpDriver = HaliInitPnpDriver;
        }
    }
    else
    {
        ULONG Control = 1;
        if (HalpExynos && !NT_SUCCESS(HalpExynosInitializeCacheRanges(LoaderBlock)))
            return FALSE;
        if (HalpExynos)
            HalpTimerFrequency = EXYNOS_MCT_RATE;
        else
            __asm__ __volatile__("mrc p15, 0, %0, c14, c0, 0" : "=r" (HalpTimerFrequency));
        if (!HalpTimerFrequency)
            return FALSE;
        HalpTimerIncrement = HalpIncrement;
        HalpTimerDeadline = KeQueryPerformanceCounter(NULL).QuadPart +
                           (ULONGLONG)HalpTimerFrequency * HalpTimerIncrement / 10000000;
        HalpSetTimerDeadline(HalpTimerDeadline);
        if (!HalEnableSystemInterrupt(HalpClockVector(), CLOCK_LEVEL, LevelSensitive))
            return FALSE;
        if (!HalpExynos)
            __asm__ __volatile__("mcr p15, 0, %0, c14, c3, 1\n\tisb" :: "r" (Control) : "memory");
    }
    return TRUE;
}

BOOLEAN NTAPI HalAllProcessorsStarted(VOID) { return TRUE; }
VOID NTAPI HalProcessorIdle(VOID) { __asm__ __volatile__("dsb sy\n\tcpsie i\n\twfi" ::: "memory"); }

VOID FASTCALL HalRequestSoftwareInterrupt(KIRQL Irql)
{
    if (Irql == DISPATCH_LEVEL || Irql == APC_LEVEL)
        HalpWrite(ARM_VIRT_GIC_DIST_VIRTUAL, 0xF00, (2UL << 24) | (Irql == APC_LEVEL));
}

VOID FASTCALL HalClearSoftwareInterrupt(KIRQL Irql)
{
    if (Irql == DISPATCH_LEVEL || Irql == APC_LEVEL)
        HalpWrite(ARM_VIRT_GIC_DIST_VIRTUAL, 0xF10, 0xFFUL << (Irql == APC_LEVEL ? 8 : 0));
}

ULONG FASTCALL HalGetInterruptSource(VOID)
{
    HalpInterruptToken = HalpRead(ARM_VIRT_GIC_CPU_VIRTUAL, 0xC);
    return HalpInterruptToken & 0x3FF;
}

KIRQL NTAPI HalpArmGetInterruptIrql(ULONG Source)
{
    if (Source == 0)
        return DISPATCH_LEVEL;
    if (Source == 1)
        return APC_LEVEL;
    return Source == HalpClockVector() ? CLOCK_LEVEL : 5;
}

BOOLEAN NTAPI HalpArmDispatchInterrupt(ULONG Source, PKTRAP_FRAME TrapFrame)
{
    ULONGLONG Now, Period, Elapsed, Remaining;
    ULONG Increment;
    BOOLEAN Rearm;

    if (Source != HalpClockVector())
        return FALSE;
    if (HalpExynos) ExynosMctAcknowledge(EXYNOS_MCT_VIRTUAL);
    Now = KeQueryPerformanceCounter(NULL).QuadPart;
    Period = max((ULONGLONG)HalpTimerFrequency * HalpTimerIncrement / 10000000, 1);
    Elapsed = (LONG64)(Now - HalpTimerDeadline) >= 0 ?
              (Now - HalpTimerDeadline) / Period + 1 : 1;
    Remaining = Elapsed * HalpTimerIncrement;
    HalpTimerDeadline += Elapsed * Period;
    Rearm = !HalpExynos || HalpTimerIncrement != HalpIncrement;
    if (HalpTimerIncrement != HalpIncrement)
    {
        HalpTimerIncrement = HalpIncrement;
        Period = max((ULONGLONG)HalpTimerFrequency * HalpTimerIncrement / 10000000, 1);
        HalpTimerDeadline = Now + Period;
    }
    while (Remaining)
    {
        Increment = (ULONG)min(Remaining, (ULONGLONG)MAXULONG);
        KeUpdateSystemTime(TrapFrame, Increment, TrapFrame->PreviousIrql);
        Remaining -= Increment;
    }
    if (Rearm)
    {
        Now = KeQueryPerformanceCounter(NULL).QuadPart;
        HalpSetTimerDeadline((LONG64)(Now - HalpTimerDeadline) >= 0 ? Now + Period : HalpTimerDeadline);
    }
    return TRUE;
}

VOID NTAPI HalEndSystemInterrupt(KIRQL OldIrql, PKTRAP_FRAME TrapFrame)
{
    UNREFERENCED_PARAMETER(TrapFrame);
    if ((HalpInterruptToken & 0x3FF) < 1020)
        HalpWrite(ARM_VIRT_GIC_CPU_VIRTUAL, 0x10, HalpInterruptToken);
    KfLowerIrql(OldIrql);
}

BOOLEAN NTAPI HalEnableSystemInterrupt(ULONG Vector, KIRQL Irql, KINTERRUPT_MODE Mode)
{
    ULONG Shift, Value;
    if (Vector >= HalpInterruptCount || Irql <= DISPATCH_LEVEL || Irql >= HIGH_LEVEL)
        return FALSE;
    Shift = (Vector % 4) * 8;
    Value = HalpRead(ARM_VIRT_GIC_DIST_VIRTUAL, 0x400 + (Vector & ~3));
    Value = (Value & ~(0xFFUL << Shift)) | (((15 - Irql) << 4) << Shift);
    HalpWrite(ARM_VIRT_GIC_DIST_VIRTUAL, 0x400 + (Vector & ~3), Value);
    if (Vector >= 32)
    {
        Value = HalpRead(ARM_VIRT_GIC_DIST_VIRTUAL, 0x800 + (Vector & ~3));
        Value = (Value & ~(0xFFUL << Shift)) | (1UL << Shift);
        HalpWrite(ARM_VIRT_GIC_DIST_VIRTUAL, 0x800 + (Vector & ~3), Value);
    }
    if (Vector >= 16)
    {
        Shift = (Vector % 16) * 2;
        Value = HalpRead(ARM_VIRT_GIC_DIST_VIRTUAL, 0xC00 + (Vector / 16) * 4);
        Value &= ~(2UL << Shift);
        if (Mode == Latched)
            Value |= 2UL << Shift;
        HalpWrite(ARM_VIRT_GIC_DIST_VIRTUAL, 0xC00 + (Vector / 16) * 4, Value);
    }
    HalpWrite(ARM_VIRT_GIC_DIST_VIRTUAL, 0x100 + (Vector / 32) * 4, 1UL << (Vector % 32));
    return TRUE;
}

VOID NTAPI HalDisableSystemInterrupt(ULONG Vector, KIRQL Irql)
{
    UNREFERENCED_PARAMETER(Irql);
    if (Vector < HalpInterruptCount)
        HalpWrite(ARM_VIRT_GIC_DIST_VIRTUAL, 0x180 + (Vector / 32) * 4, 1UL << (Vector % 32));
}

LARGE_INTEGER NTAPI KeQueryPerformanceCounter(PLARGE_INTEGER Frequency)
{
    LARGE_INTEGER Value;
    ULONG Low, High, Rate;
    if (HalpExynos)
    {
        Value.QuadPart = ExynosMctCounter(EXYNOS_MCT_VIRTUAL);
        if (Frequency) Frequency->QuadPart = EXYNOS_MCT_RATE;
        return Value;
    }
    __asm__ __volatile__("isb\n\tmrrc p15, 1, %0, %1, c14" : "=r" (Low), "=r" (High));
    Value.QuadPart = (ULONGLONG)High << 32 | Low;
    if (Frequency)
    {
        __asm__ __volatile__("mrc p15, 0, %0, c14, c0, 0" : "=r" (Rate));
        Frequency->QuadPart = Rate;
    }
    return Value;
}

VOID NTAPI KeStallExecutionProcessor(ULONG Microseconds)
{
    LARGE_INTEGER Frequency, Start = KeQueryPerformanceCounter(&Frequency);
    ULONGLONG Ticks = ((ULONGLONG)Microseconds * Frequency.QuadPart + 999999) / 1000000;
    while ((ULONGLONG)(KeQueryPerformanceCounter(NULL).QuadPart - Start.QuadPart) < Ticks)
        __asm__ __volatile__("yield");
}

ULONG NTAPI HalSetTimeIncrement(ULONG Increment)
{
    HalpIncrement = min(max(Increment, 10000), 100000);
    return HalpIncrement;
}

VOID NTAPI KeFlushWriteBuffer(VOID) { __asm__ __volatile__("dsb sy" ::: "memory"); }

VOID FASTCALL HalSweepIcache(VOID)
{
    ULONG Zero = 0;
    __asm__ __volatile__("dsb sy\n\tmcr p15, 0, %0, c7, c5, 0\n\tdsb sy\n\tisb" :: "r" (Zero) : "memory");
}

VOID FASTCALL HalSweepDcache(VOID)
{
    ULONG Clidr, Ccsidr, Level, Type, Set, Way, Sets, Ways, Line, WayShift, Operand;
    __asm__ __volatile__("mrc p15, 1, %0, c0, c0, 1" : "=r" (Clidr));
    __asm__ __volatile__("dsb sy" ::: "memory");
    for (Level = 0; Level < 7; Level++)
    {
        Type = (Clidr >> (Level * 3)) & 7;
        if (Type < 2 || Type > 4)
            continue;
        Operand = Level << 1;
        __asm__ __volatile__("mcr p15, 2, %0, c0, c0, 0\n\tisb" :: "r" (Operand) : "memory");
        __asm__ __volatile__("mrc p15, 1, %0, c0, c0, 0" : "=r" (Ccsidr));
        Sets = ((Ccsidr >> 13) & 0x7FFF) + 1;
        Ways = ((Ccsidr >> 3) & 0x3FF) + 1;
        Line = (Ccsidr & 7) + 4;
        WayShift = Ways == 1 ? 0 : __builtin_clz(Ways - 1);
        for (Set = 0; Set < Sets; Set++)
            for (Way = 0; Way < Ways; Way++)
            {
                Operand = (Level << 1) | (Set << Line) | (Way << WayShift);
                __asm__ __volatile__("mcr p15, 0, %0, c7, c14, 2" :: "r" (Operand) : "memory");
            }
    }
    Operand = 0;
    __asm__ __volatile__("mcr p15, 2, %0, c0, c0, 0\n\tdsb sy\n\tisb" :: "r" (Operand) : "memory");
}

static BOOLEAN HalpExynosRtcQuery(PTIME_FIELDS Time)
{
    ULONG Attempt, FirstSecond, LastSecond, RawMinute, RawHour;
    ULONG RawDate, RawMonth, RawYear;
    unsigned int RawSecond, Second, Minute, Hour, Date, Month, Century;
    TIME_FIELDS Fields;
    LARGE_INTEGER Value;

    for (Attempt = 0; Attempt < 4; Attempt++)
    {
        FirstSecond = ExynosRtcRead(ARM_VIRT_RTC_VIRTUAL, EXYNOS_RTC_BCDSEC);
        RawMinute = ExynosRtcRead(ARM_VIRT_RTC_VIRTUAL, EXYNOS_RTC_BCDMIN);
        RawHour = ExynosRtcRead(ARM_VIRT_RTC_VIRTUAL, EXYNOS_RTC_BCDHOUR);
        RawDate = ExynosRtcRead(ARM_VIRT_RTC_VIRTUAL, EXYNOS_RTC_BCDDATE);
        RawMonth = ExynosRtcRead(ARM_VIRT_RTC_VIRTUAL, EXYNOS_RTC_BCDMON);
        RawYear = ExynosRtcRead(ARM_VIRT_RTC_VIRTUAL, EXYNOS_RTC_BCDYEAR);
        LastSecond = ExynosRtcRead(ARM_VIRT_RTC_VIRTUAL, EXYNOS_RTC_BCDSEC);
        if (FirstSecond == LastSecond) break;
    }
    if (Attempt == 4) return FALSE;

    RawSecond = FirstSecond;
    if (!ExynosRtcBcdToBinary(RawSecond, &Second) ||
        !ExynosRtcBcdToBinary(RawMinute, &Minute) ||
        !ExynosRtcBcdToBinary(RawHour, &Hour) ||
        !ExynosRtcBcdToBinary(RawDate, &Date) ||
        !ExynosRtcBcdToBinary(RawMonth, &Month) ||
        !ExynosRtcBcdToBinary(RawYear & 0xff, &RawSecond) ||
        !ExynosRtcBcdToBinary((RawYear >> 8) & 0xf, &Century))
        return FALSE;

    RtlZeroMemory(&Fields, sizeof(Fields));
    Fields.Year = 1900 + Century * 100 + RawSecond;
    Fields.Month = (USHORT)Month;
    Fields.Day = (USHORT)Date;
    Fields.Hour = (USHORT)Hour;
    Fields.Minute = (USHORT)Minute;
    Fields.Second = (USHORT)Second;
    if (!RtlTimeFieldsToTime(&Fields, &Value)) return FALSE;
    RtlTimeToTimeFields(&Value, Time);
    return TRUE;
}

static BOOLEAN HalpExynosRtcSet(PTIME_FIELDS Time)
{
    TIME_FIELDS Fields = *Time;
    LARGE_INTEGER Value;
    ULONG Control, Year;

    if (!RtlTimeFieldsToTime(Time, &Value)) return FALSE;
    RtlTimeToTimeFields(&Value, &Fields);
    if (Fields.Year < 1900 || Fields.Year > 2899) return FALSE;
    Year = Fields.Year - 1900;

    Control = ExynosRtcRead(ARM_VIRT_RTC_VIRTUAL, EXYNOS_RTC_RTCCON);
    if (!(Control & EXYNOS_RTC_ENABLE))
        ExynosRtcWrite(ARM_VIRT_RTC_VIRTUAL, EXYNOS_RTC_RTCCON, Control | EXYNOS_RTC_ENABLE);

    ExynosRtcWrite(ARM_VIRT_RTC_VIRTUAL, EXYNOS_RTC_BCDYEAR,
                   ExynosRtcBinaryToBcd(Year % 100) |
                   (ExynosRtcBinaryToBcd(Year / 100) << 8));
    ExynosRtcWrite(ARM_VIRT_RTC_VIRTUAL, EXYNOS_RTC_BCDMON,
                   ExynosRtcBinaryToBcd(Fields.Month));
    ExynosRtcWrite(ARM_VIRT_RTC_VIRTUAL, EXYNOS_RTC_BCDDATE,
                   ExynosRtcBinaryToBcd(Fields.Day));
    ExynosRtcWrite(ARM_VIRT_RTC_VIRTUAL, EXYNOS_RTC_BCDDAY,
                   ExynosRtcBinaryToBcd(Fields.Weekday));
    ExynosRtcWrite(ARM_VIRT_RTC_VIRTUAL, EXYNOS_RTC_BCDHOUR,
                   ExynosRtcBinaryToBcd(Fields.Hour));
    ExynosRtcWrite(ARM_VIRT_RTC_VIRTUAL, EXYNOS_RTC_BCDMIN,
                   ExynosRtcBinaryToBcd(Fields.Minute));
    ExynosRtcWrite(ARM_VIRT_RTC_VIRTUAL, EXYNOS_RTC_BCDSEC,
                   ExynosRtcBinaryToBcd(Fields.Second));
    return TRUE;
}

BOOLEAN NTAPI HalQueryRealTimeClock(PTIME_FIELDS Time)
{
    LARGE_INTEGER Value;
    if (HalpExynos)
        return HalpExynosRtcQuery(Time);
    else
        RtlSecondsSince1970ToTime(HalpRead(ARM_VIRT_RTC_VIRTUAL, 0), &Value);
    RtlTimeToTimeFields(&Value, Time);
    return TRUE;
}

BOOLEAN NTAPI HalSetRealTimeClock(PTIME_FIELDS Time)
{
    LARGE_INTEGER Value;
    ULONG Seconds;
    if (HalpExynos) return HalpExynosRtcSet(Time);
    if (!RtlTimeFieldsToTime(Time, &Value) || !RtlTimeToSecondsSince1970(&Value, &Seconds))
        return FALSE;
    HalpWrite(ARM_VIRT_RTC_VIRTUAL, 8, Seconds);
    return TRUE;
}

VOID NTAPI HalReturnToFirmware(FIRMWARE_REENTRY Action)
{
    register ULONG Function __asm__("r0") = Action == HalRebootRoutine || Action == HalRestartRoutine ?
                                           0x84000009 : 0x84000008;
    __asm__ __volatile__("dsb sy\n\t.arch_extension virt\n\thvc #0" : "+r" (Function) :: "r1", "r2", "r3", "memory");
    for (;;)
        __asm__ __volatile__("cpsid if\n\twfi");
}

NTSTATUS NTAPI HalAdjustResourceList(PIO_RESOURCE_REQUIREMENTS_LIST *Resources)
{
    UNREFERENCED_PARAMETER(Resources);
    return STATUS_NOT_SUPPORTED;
}

NTSTATUS NTAPI HalAllocateAdapterChannel(PADAPTER_OBJECT Adapter, PWAIT_CONTEXT_BLOCK Context,
                                        ULONG Registers, PDRIVER_CONTROL Routine)
{
    UNREFERENCED_PARAMETER(Adapter);
    UNREFERENCED_PARAMETER(Context);
    UNREFERENCED_PARAMETER(Registers);
    UNREFERENCED_PARAMETER(Routine);
    /* TODO: provide DMA adapter allocation for virt devices. */
    return STATUS_NOT_SUPPORTED;
}

ULONG NTAPI HalGetBusDataByOffset(BUS_DATA_TYPE Type, ULONG Bus, ULONG Slot, PVOID Buffer, ULONG Offset, ULONG Length)
{
    UNREFERENCED_PARAMETER(Type); UNREFERENCED_PARAMETER(Bus); UNREFERENCED_PARAMETER(Slot);
    UNREFERENCED_PARAMETER(Buffer); UNREFERENCED_PARAMETER(Offset); UNREFERENCED_PARAMETER(Length);
    /* TODO: enumerate QEMU virt PCI configuration resources. */
    return 0;
}

ULONG NTAPI HalSetBusDataByOffset(BUS_DATA_TYPE Type, ULONG Bus, ULONG Slot, PVOID Buffer, ULONG Offset, ULONG Length)
{
    UNREFERENCED_PARAMETER(Type); UNREFERENCED_PARAMETER(Bus); UNREFERENCED_PARAMETER(Slot);
    UNREFERENCED_PARAMETER(Buffer); UNREFERENCED_PARAMETER(Offset); UNREFERENCED_PARAMETER(Length);
    return 0;
}

ULONG NTAPI HalGetInterruptVector(INTERFACE_TYPE Type, ULONG Bus, ULONG Level, ULONG Vector,
                                  PKIRQL Irql, PKAFFINITY Affinity)
{
    UNREFERENCED_PARAMETER(Bus); UNREFERENCED_PARAMETER(Level);
    if ((Type != Internal && Type != PNPBus && Type != ACPIBus) ||
        Vector < 32 || Vector >= HalpInterruptCount)
        return 0;
    *Irql = 5;
    *Affinity = 1;
    return Vector;
}

BOOLEAN NTAPI HalTranslateBusAddress(INTERFACE_TYPE Type, ULONG Bus, PHYSICAL_ADDRESS Address,
                                    PULONG AddressSpace, PPHYSICAL_ADDRESS Translated)
{
    UNREFERENCED_PARAMETER(Bus);
    if ((Type != Internal && Type != PNPBus && Type != ACPIBus) || *AddressSpace != 0)
        return FALSE;
    *Translated = Address;
    return TRUE;
}

ARC_STATUS NTAPI HalGetEnvironmentVariable(PCH Name, USHORT Length, PCH Buffer)
{
    UNREFERENCED_PARAMETER(Name); UNREFERENCED_PARAMETER(Length); UNREFERENCED_PARAMETER(Buffer);
    return ENOENT;
}

ARC_STATUS NTAPI HalSetEnvironmentVariable(PCH Name, PCH Value)
{
    UNREFERENCED_PARAMETER(Name); UNREFERENCED_PARAMETER(Value);
    return EACCES;
}

VOID NTAPI HalReportResourceUsage(VOID)
{
    /* TODO: publish QEMU virt platform resources in the hardware registry. */
}

UCHAR NTAPI READ_PORT_UCHAR(PUCHAR Port) { return READ_REGISTER_UCHAR(Port); }
VOID NTAPI WRITE_PORT_UCHAR(PUCHAR Port, UCHAR Value) { WRITE_REGISTER_UCHAR(Port, Value); }

USHORT NTAPI READ_PORT_USHORT(PUSHORT Port) { return READ_REGISTER_USHORT(Port); }
ULONG NTAPI READ_PORT_ULONG(PULONG Port) { return READ_REGISTER_ULONG(Port); }
VOID NTAPI WRITE_PORT_USHORT(PUSHORT Port, USHORT Value) { WRITE_REGISTER_USHORT(Port, Value); }
VOID NTAPI WRITE_PORT_ULONG(PULONG Port, ULONG Value) { WRITE_REGISTER_ULONG(Port, Value); }

BOOLEAN NTAPI HalMakeBeep(ULONG Frequency)
{
    UNREFERENCED_PARAMETER(Frequency);
    return FALSE;
}

PADAPTER_OBJECT NTAPI HalGetAdapter(PDEVICE_DESCRIPTION Description, PULONG NumberOfMapRegisters)
{
    UNREFERENCED_PARAMETER(Description);
    if (NumberOfMapRegisters) *NumberOfMapRegisters = 0;
    return NULL;
}

KIRQL NTAPI HalConvertDeviceIdtToIrql(ULONG Vector)
{
    return HalpArmGetInterruptIrql(Vector);
}

NTSTATUS NTAPI HalGetInterruptTargetInformation(PHAL_INTERRUPT_TARGET_INFORMATION Information)
{
    if (!Information || Information->Version != HAL_INTERRUPT_TARGET_INFORMATION_VERSION ||
        (Information->TargetProcessors & ~(KAFFINITY)1))
        return STATUS_INVALID_PARAMETER;
    Information->TargetProcessors = 1;
    Information->ProcessorNumber = 0;
    Information->DestinationId = 0;
    return STATUS_SUCCESS;
}

NTSTATUS NTAPI HalGetMessageRoutingInfo(PHAL_MESSAGE_ROUTING_INFO Information)
{
    if (!Information || Information->Version != HAL_MESSAGE_ROUTING_INFO_VERSION)
        return STATUS_INVALID_PARAMETER;
    return STATUS_NOT_SUPPORTED;
}

NTSTATUS NTAPI HalGetMemoryCachingRequirements(PHYSICAL_ADDRESS Address, SIZE_T Length,
                                               MEMORY_CACHING_TYPE *CacheType)
{
    if (!CacheType) return STATUS_INVALID_PARAMETER;
    if (HalpExynos) return HalpExynosMemoryCachingRequirements(Address, Length, CacheType);
    *CacheType = MmNonCached;
    return STATUS_NOT_SUPPORTED;
}

VOID NTAPI HalpConfigurePciRootBridge(const HAL_ACPI_PCI_ROOT_INFO *Information)
{
    UNREFERENCED_PARAMETER(Information);
}

VOID NTAPI HalpRegisterPciRouteQuery(PHAL_ACPI_PCI_ROUTE_QUERY Provider)
{
    UNREFERENCED_PARAMETER(Provider);
}

VOID NTAPI READ_PORT_BUFFER_UCHAR(PUCHAR Port, PUCHAR Buffer, ULONG Count)
{
    while (Count--) *Buffer++ = READ_PORT_UCHAR(Port);
}

VOID NTAPI WRITE_PORT_BUFFER_UCHAR(PUCHAR Port, PUCHAR Buffer, ULONG Count)
{
    while (Count--) WRITE_PORT_UCHAR(Port, *Buffer++);
}

VOID NTAPI READ_PORT_BUFFER_USHORT(PUSHORT Port, PUSHORT Buffer, ULONG Count)
{
    while (Count--) *Buffer++ = READ_PORT_USHORT(Port);
}

VOID NTAPI WRITE_PORT_BUFFER_USHORT(PUSHORT Port, PUSHORT Buffer, ULONG Count)
{
    while (Count--) WRITE_PORT_USHORT(Port, *Buffer++);
}

VOID NTAPI READ_PORT_BUFFER_ULONG(PULONG Port, PULONG Buffer, ULONG Count)
{
    while (Count--) *Buffer++ = READ_PORT_ULONG(Port);
}

VOID NTAPI WRITE_PORT_BUFFER_ULONG(PULONG Port, PULONG Buffer, ULONG Count)
{
    while (Count--) WRITE_PORT_ULONG(Port, *Buffer++);
}

ULONG NTAPI HalGetBusData(BUS_DATA_TYPE Type, ULONG Bus, ULONG Slot, PVOID Buffer, ULONG Length)
{
    return HalGetBusDataByOffset(Type, Bus, Slot, Buffer, 0, Length);
}

NTSTATUS NTAPI HalAssignSlotResources(PUNICODE_STRING RegistryPath, PUNICODE_STRING ClassName,
                                      PDRIVER_OBJECT Driver, PDEVICE_OBJECT Device,
                                      INTERFACE_TYPE Type, ULONG Bus, ULONG Slot,
                                      PCM_RESOURCE_LIST *Resources)
{
    UNREFERENCED_PARAMETER(RegistryPath);
    UNREFERENCED_PARAMETER(ClassName);
    UNREFERENCED_PARAMETER(Driver);
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(Type);
    UNREFERENCED_PARAMETER(Bus);
    UNREFERENCED_PARAMETER(Slot);
    UNREFERENCED_PARAMETER(Resources);
    return STATUS_NOT_SUPPORTED;
}

PVOID NTAPI HalAllocateCommonBuffer(PADAPTER_OBJECT Adapter, ULONG Length,
                                    PPHYSICAL_ADDRESS Address, BOOLEAN Cached)
{
    UNREFERENCED_PARAMETER(Adapter);
    UNREFERENCED_PARAMETER(Length);
    UNREFERENCED_PARAMETER(Cached);
    if (Address) Address->QuadPart = 0;
    return NULL;
}

VOID NTAPI HalFreeCommonBuffer(PADAPTER_OBJECT Adapter, ULONG Length,
                               PHYSICAL_ADDRESS Address, PVOID Buffer, BOOLEAN Cached)
{
    UNREFERENCED_PARAMETER(Adapter);
    UNREFERENCED_PARAMETER(Length);
    UNREFERENCED_PARAMETER(Address);
    UNREFERENCED_PARAMETER(Buffer);
    UNREFERENCED_PARAMETER(Cached);
}

VOID NTAPI IoFreeAdapterChannel(PADAPTER_OBJECT Adapter)
{
    UNREFERENCED_PARAMETER(Adapter);
}

VOID NTAPI IoFreeMapRegisters(PADAPTER_OBJECT Adapter, PVOID Registers, ULONG Count)
{
    UNREFERENCED_PARAMETER(Adapter);
    UNREFERENCED_PARAMETER(Registers);
    UNREFERENCED_PARAMETER(Count);
}

BOOLEAN NTAPI IoFlushAdapterBuffers(PADAPTER_OBJECT Adapter, PMDL Mdl, PVOID Registers,
                                    PVOID CurrentVa, ULONG Length, BOOLEAN WriteToDevice)
{
    UNREFERENCED_PARAMETER(Adapter);
    UNREFERENCED_PARAMETER(Mdl);
    UNREFERENCED_PARAMETER(Registers);
    UNREFERENCED_PARAMETER(CurrentVa);
    UNREFERENCED_PARAMETER(Length);
    UNREFERENCED_PARAMETER(WriteToDevice);
    return FALSE;
}

PHYSICAL_ADDRESS NTAPI IoMapTransfer(PADAPTER_OBJECT Adapter, PMDL Mdl, PVOID Registers,
                                     PVOID CurrentVa, PULONG Length, BOOLEAN WriteToDevice)
{
    PHYSICAL_ADDRESS Address;
    UNREFERENCED_PARAMETER(Adapter);
    UNREFERENCED_PARAMETER(Mdl);
    UNREFERENCED_PARAMETER(Registers);
    UNREFERENCED_PARAMETER(CurrentVa);
    UNREFERENCED_PARAMETER(WriteToDevice);
    if (Length) *Length = 0;
    Address.QuadPart = 0;
    return Address;
}
