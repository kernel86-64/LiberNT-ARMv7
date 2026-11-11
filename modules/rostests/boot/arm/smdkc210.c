/*
 * PROJECT:     LiberNT boot tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Exercise Cortex-A9 short-descriptor translation on SMDKC210
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include "shortdesc.h"
#include "processor.h"
#include "abort.h"
#include <reactos/arm/exynos4mct.h>
#include <reactos/arm/exynos4rtc.h>

static _Alignas(16384) uint32_t Root[ARM_SD_ROOT_ENTRIES];
static _Alignas(4096) uint32_t Tables[8 * ARM_SD_TABLE_WORDS];
static _Alignas(4096) uint32_t PageA[1024];
static _Alignas(4096) uint32_t PageB[1024];
static _Alignas(4096) uint32_t Code[1024];
static _Alignas(4096) uint32_t AlignmentPage[1024];
static volatile uint32_t ExpectedKind, ExpectedStatus, ExpectedAddress, AbortCount;
static volatile uint32_t SgiCount, TimerCount;
static volatile uint32_t PeriodicTimer;
static uint64_t TimerPrevious;
volatile uint32_t SmdkResume;

extern uint32_t SmdkTryRead(uint32_t Address);
extern void SmdkTryWrite(uint32_t Address, uint32_t Value);
extern uint32_t SmdkTryUnprivilegedRead(uint32_t Address);
extern void SmdkTryExecute(uint32_t Address);

static void Print(const char *Text)
{
    volatile uint32_t *Status = (void *)0x13800010;
    volatile uint32_t *Data = (void *)0x13800020;

    while (*Text)
    {
        while (!(*Status & 2u)) {}
        *Data = (uint8_t)*Text++;
    }
    __asm__ volatile("dsb sy" ::: "memory");
}

static void Hex(uint32_t Value)
{
    char Buffer[9];
    uint32_t Index;

    for (Index = 0; Index < 8; ++Index)
        Buffer[Index] = "0123456789abcdef"[(Value >> (28u - Index * 4u)) & 15u];
    Buffer[8] = 0;
    Print(Buffer);
}

void SmdkFailure(uint32_t Check)
{
    Print("SMDKC210 tests: FAIL check=");
    Hex(Check);
    Print("\r\n");
    for (;;) __asm__ volatile("wfi");
}

static void Check(int Condition, uint32_t Number)
{
    if (!Condition) SmdkFailure(Number);
}

void SmdkAbort(uint32_t Kind, uint32_t Status, uint32_t Address, uint32_t *Frame)
{
    uint32_t Fault = (Status & 15u) | ((Status >> 6) & 16u);

    Check(KiArmAbortStatus(Status) == Fault, 30);

    if (Kind != ExpectedKind || Address != ExpectedAddress || Fault != ExpectedStatus)
    {
        Print("Unexpected abort kind/status/address/pc=");
        Hex(Kind); Print("/"); Hex(Status); Print("/"); Hex(Address); Print("/"); Hex(Frame[13]);
        Print("\r\n");
        SmdkFailure(0xab0);
    }
    Frame[13] = Kind == 2 ? SmdkResume : Frame[13] + 4u;
    ExpectedKind = 0;
    AbortCount++;
}

static void Expect(uint32_t Kind, uint32_t Status, uint32_t Address)
{
    ExpectedKind = Kind;
    ExpectedStatus = Status;
    ExpectedAddress = Address;
}

static uint32_t ReadRegister(uint32_t Address)
{
    uint32_t Value = *(volatile uint32_t *)(uintptr_t)Address;
    __asm__ volatile("dmb sy" ::: "memory");
    return Value;
}

static void WriteRegister(uint32_t Address, uint32_t Value)
{
    __asm__ volatile("dmb sy" ::: "memory");
    *(volatile uint32_t *)(uintptr_t)Address = Value;
    __asm__ volatile("dsb sy" ::: "memory");
}

void SmdkInterrupt(void)
{
    uint32_t Acknowledge = ReadRegister(0x1048000c);
    uint32_t Interrupt = Acknowledge & 1023u;

    if (Interrupt == 1023) return;
    if (Interrupt == 1)
        SgiCount++;
    else if (Interrupt == 89)
    {
        ExynosMctAcknowledge(EXYNOS_MCT_PHYSICAL);
        TimerCount++;
        if (PeriodicTimer)
        {
            uint64_t Current = ExynosMctCounter(EXYNOS_MCT_PHYSICAL);
            Check(Current >= TimerPrevious, 33);
            TimerPrevious = Current;
        }
        else
            Check(ExynosMctDeadline(EXYNOS_MCT_PHYSICAL,
                                   ExynosMctCounter(EXYNOS_MCT_PHYSICAL) + 24000), 26);
    }
    else
        SmdkFailure(0x1000u + Interrupt);
    WriteRegister(0x10480010, Acknowledge);
}

static void TestInterrupts(void)
{
    uint64_t Before, Compare;
    uint32_t Interrupt = 89;

    WriteRegister(0x10490000, 1);
    WriteRegister(0x10480004, 0xff);
    WriteRegister(0x10480000, 1);
    WriteRegister(0x10490f00, (2u << 24) | 1u);
    __asm__ volatile("cpsie i\n\tisb" ::: "memory");
    while (!SgiCount) __asm__ volatile("nop");
    __asm__ volatile("cpsid i" ::: "memory");
    Check(SgiCount == 1, 24);
    Print("Exynos4210 GIC: self-directed SGI and IRQ return PASS\r\n");
    *(volatile uint8_t *)(uintptr_t)(0x10490800 + Interrupt) = 1;
    *(volatile uint8_t *)(uintptr_t)(0x10490400 + Interrupt) = 0x80;
    WriteRegister(0x10490100 + (Interrupt / 32) * 4, 1u << (Interrupt % 32));
    Check(ExynosMctLatch(EXYNOS_MCT_PHYSICAL, 0x240, 0x100, 1u << 16), 27);
    Before = ExynosMctCounter(EXYNOS_MCT_PHYSICAL);
    Compare = Before + 240000;
    Check(ExynosMctDeadline(EXYNOS_MCT_PHYSICAL, Compare), 28);
    __asm__ volatile("cpsie i\n\tisb" ::: "memory");
    while (TimerCount < 5) __asm__ volatile("nop");
    __asm__ volatile("cpsid i" ::: "memory");
    WriteRegister(0x10050248, 0);
    Check(ExynosMctLatch(EXYNOS_MCT_PHYSICAL, 0x240, 0x100, 1u << 16), 29);
    Check(ExynosMctCounter(EXYNOS_MCT_PHYSICAL) > Before && TimerCount >= 5, 25);
    Print("Exynos4210 MCT: counter and five comparator interrupts PASS\r\n");
    PeriodicTimer = 1;
    TimerCount = 0;
    Check(ExynosMctComparator(EXYNOS_MCT_PHYSICAL,
                              ExynosMctCounter(EXYNOS_MCT_PHYSICAL) + 240000, 24000), 34);
    TimerPrevious = ExynosMctCounter(EXYNOS_MCT_PHYSICAL);
    __asm__ volatile("cpsie i\n\tisb" ::: "memory");
    while (TimerCount < 32) __asm__ volatile("nop");
    __asm__ volatile("cpsid i" ::: "memory");
    ExynosMctWrite(EXYNOS_MCT_PHYSICAL, 0x248, 0);
    Check(ExynosMctLatch(EXYNOS_MCT_PHYSICAL, 0x240, 0x100, 1u << 16), 35);
    Print("Exynos4210 MCT: periodic comparator and monotonic counter PASS\r\n");
}

static uint32_t RtcBinary(uint32_t Value)
{
    uint32_t Result;
    Check(ExynosRtcBcdToBinary(Value, &Result), 50);
    return Result;
}

static void RtcReadStable(uint32_t *Second, uint32_t *Minute, uint32_t *Hour,
                          uint32_t *Date, uint32_t *Month, uint32_t *Year)
{
    uint32_t Attempt, First, Last;

    for (Attempt = 0; Attempt < 4; ++Attempt)
    {
        First = ExynosRtcRead(EXYNOS_RTC_PHYSICAL, EXYNOS_RTC_BCDSEC);
        *Minute = ExynosRtcRead(EXYNOS_RTC_PHYSICAL, EXYNOS_RTC_BCDMIN);
        *Hour = ExynosRtcRead(EXYNOS_RTC_PHYSICAL, EXYNOS_RTC_BCDHOUR);
        *Date = ExynosRtcRead(EXYNOS_RTC_PHYSICAL, EXYNOS_RTC_BCDDATE);
        *Month = ExynosRtcRead(EXYNOS_RTC_PHYSICAL, EXYNOS_RTC_BCDMON);
        *Year = ExynosRtcRead(EXYNOS_RTC_PHYSICAL, EXYNOS_RTC_BCDYEAR);
        Last = ExynosRtcRead(EXYNOS_RTC_PHYSICAL, EXYNOS_RTC_BCDSEC);
        if (First == Last) break;
    }
    Check(Attempt < 4, 51);
    *Second = First;
}

static void RtcWrite(const uint32_t *Values)
{
    ExynosRtcWrite(EXYNOS_RTC_PHYSICAL, EXYNOS_RTC_BCDYEAR, Values[5]);
    ExynosRtcWrite(EXYNOS_RTC_PHYSICAL, EXYNOS_RTC_BCDMON, Values[4]);
    ExynosRtcWrite(EXYNOS_RTC_PHYSICAL, EXYNOS_RTC_BCDDATE, Values[3]);
    ExynosRtcWrite(EXYNOS_RTC_PHYSICAL, EXYNOS_RTC_BCDHOUR, Values[2]);
    ExynosRtcWrite(EXYNOS_RTC_PHYSICAL, EXYNOS_RTC_BCDMIN, Values[1]);
    ExynosRtcWrite(EXYNOS_RTC_PHYSICAL, EXYNOS_RTC_BCDSEC, Values[0]);
}

static void TestRtc(void)
{
    uint32_t Saved[6], SavedDay, SavedControl, Second, Minute, Hour, Date, Month, Year;
    uint32_t Values[6];
    uint64_t Start;
    uint32_t Control;

    RtcReadStable(&Saved[0], &Saved[1], &Saved[2], &Saved[3], &Saved[4], &Saved[5]);
    SavedDay = ExynosRtcRead(EXYNOS_RTC_PHYSICAL, EXYNOS_RTC_BCDDAY);
    SavedControl = ExynosRtcRead(EXYNOS_RTC_PHYSICAL, EXYNOS_RTC_RTCCON);
    Control = SavedControl | EXYNOS_RTC_ENABLE;
    ExynosRtcWrite(EXYNOS_RTC_PHYSICAL, EXYNOS_RTC_RTCCON, Control);

    Values[0] = ExynosRtcBinaryToBcd(50);
    Values[1] = ExynosRtcBinaryToBcd(34);
    Values[2] = ExynosRtcBinaryToBcd(12);
    Values[3] = ExynosRtcBinaryToBcd(29);
    Values[4] = ExynosRtcBinaryToBcd(2);
    Values[5] = ExynosRtcBinaryToBcd(24) | (ExynosRtcBinaryToBcd(1) << 8);
    RtcWrite(Values);
    RtcReadStable(&Second, &Minute, &Hour, &Date, &Month, &Year);
    Check(RtcBinary(Year & 0xff) == 24 && RtcBinary((Year >> 8) & 0xf) == 1, 52);
    Check(RtcBinary(Month) == 2 && RtcBinary(Date) == 29, 53);
    Check(RtcBinary(Hour) == 12 && RtcBinary(Minute) == 34, 54);
    Check(RtcBinary(Second) >= 50 && RtcBinary(Second) <= 52, 55);

    Values[0] = ExynosRtcBinaryToBcd(58);
    Values[1] = ExynosRtcBinaryToBcd(59);
    Values[2] = ExynosRtcBinaryToBcd(23);
    Values[3] = ExynosRtcBinaryToBcd(29);
    Values[4] = ExynosRtcBinaryToBcd(2);
    RtcWrite(Values);
    Start = ExynosMctCounter(EXYNOS_MCT_PHYSICAL);
    while (ExynosMctCounter(EXYNOS_MCT_PHYSICAL) - Start < (uint64_t)EXYNOS_MCT_RATE * 3) {}
    RtcReadStable(&Second, &Minute, &Hour, &Date, &Month, &Year);
    Check(RtcBinary(Month) == 3 && RtcBinary(Date) == 1, 56);
    Check(RtcBinary(Hour) == 0 && RtcBinary(Minute) == 0 && RtcBinary(Second) <= 2, 57);

    ExynosRtcWrite(EXYNOS_RTC_PHYSICAL, EXYNOS_RTC_BCDYEAR, Saved[5]);
    ExynosRtcWrite(EXYNOS_RTC_PHYSICAL, EXYNOS_RTC_BCDMON, Saved[4]);
    ExynosRtcWrite(EXYNOS_RTC_PHYSICAL, EXYNOS_RTC_BCDDATE, Saved[3]);
    ExynosRtcWrite(EXYNOS_RTC_PHYSICAL, EXYNOS_RTC_BCDDAY, SavedDay);
    ExynosRtcWrite(EXYNOS_RTC_PHYSICAL, EXYNOS_RTC_BCDHOUR, Saved[2]);
    ExynosRtcWrite(EXYNOS_RTC_PHYSICAL, EXYNOS_RTC_BCDMIN, Saved[1]);
    ExynosRtcWrite(EXYNOS_RTC_PHYSICAL, EXYNOS_RTC_BCDSEC, Saved[0]);
    ExynosRtcWrite(EXYNOS_RTC_PHYSICAL, EXYNOS_RTC_RTCCON, SavedControl);
    Print("Exynos4210 RTC: BCD write, leap-day rollover and restore PASS\r\n");
}

static void Invalidate(uint32_t Address)
{
    __asm__ volatile("dsb ishst\n\tmcr p15, 0, %0, c8, c7, 1\n\tdsb ish\n\tisb"
                     :: "r"(Address) : "memory");
}

static _Alignas(4096) uint64_t ShortMetadata[512];
void MiArmWriteShortBootPtePhysical(uint32_t Hardware, uint32_t Entry, uint32_t Shadow, const uint64_t *Value);

void ArmEnablePaging(uint32_t RootPhysical);

static void EnableMmu(void)
{
    uint32_t Sctlr, Ttbcr, Ttbr;

    __asm__ volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(Sctlr));
    Check(!(Sctlr & 5u), 1);
    ArmEnablePaging((uint32_t)(uintptr_t)Root);
    __asm__ volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(Sctlr));
    __asm__ volatile("mrc p15, 0, %0, c2, c0, 2" : "=r"(Ttbcr));
    __asm__ volatile("mrc p15, 0, %0, c2, c0, 0" : "=r"(Ttbr));
    Check((Sctlr & 5u) == 5u && Ttbcr == 0, 22);
    Check((Ttbr & 0xffffc000u) == (uint32_t)(uintptr_t)Root, 23);
    Print("FreeLoader short-descriptor paging handoff: PASS\r\n");
}

void SmdkMain(void)
{
    ARM_SD_CONTEXT Context;
    uint32_t Midr, Mmfr0, Pfr1, *Slot, Sctlr, AlignmentAborts = 0;

    Print("SMDKC210 Cortex-A9 tests: BEGIN\r\n");
    Check(KiArmAbortStatus(0x406) == 0x16, 31);
    Check(KiArmAbortStatus(0x206) == 6, 32);
    Check(KiArmAbortCanPage(3) && KiArmAbortCanPage(6) &&
          KiArmAbortCanPage(7) && KiArmAbortCanPage(15), 33);
    Check(!KiArmAbortCanPage(8) && !KiArmAbortCanPage(9) &&
          !KiArmAbortCanPage(0x406), 34);
    Check(KiArmAbortCanPage(0x204) && KiArmAbortCanPage(0x20f) &&
          !KiArmAbortCanPage(0x210), 35);
    Check(KiArmAbortDebug(2) && KiArmAbortDebug(0x222) &&
          !KiArmAbortDebug(0x202) && !KiArmAbortDebug(0x21), 36);
    Check(KiArmAbortAlignment(1) && KiArmAbortAlignment(0x221) &&
          !KiArmAbortAlignment(0x201), 37);
    __asm__ volatile("mrc p15, 0, %0, c0, c0, 0" : "=r"(Midr));
    __asm__ volatile("mrc p15, 0, %0, c0, c1, 4" : "=r"(Mmfr0));
    __asm__ volatile("mrc p15, 0, %0, c0, c1, 1" : "=r"(Pfr1));
    Print("MIDR="); Hex(Midr); Print(" ID_MMFR0="); Hex(Mmfr0);
    Print(" ID_PFR1="); Hex(Pfr1); Print("\r\n");
    Check(((Midr >> 4) & 0xfffu) == 0xc09 && (Mmfr0 & 15u) == 3 && !(Pfr1 & 0xf0000), 2);
    Check(KiArmProcessorName(Midr) != 0, 31);
    Print("CPU name: "); Print(KiArmProcessorName(Midr)); Print("\r\n");
    Check(ArmSdInitialize(&Context, Root, Tables, (uint32_t)(uintptr_t)Tables, 8) == 0, 3);
    Check(ArmSdMapSection(&Context, 0x40000000, 0x40000000, ARM_SD_EXECUTE) == 0, 4);
    Check(ArmSdMapSection(&Context, 0x13800000, 0x13800000, ARM_SD_DEVICE) == 0, 5);
    Check(ArmSdMapSection(&Context, 0x10400000, 0x10400000, ARM_SD_DEVICE) == 0, 26);
    Check(ArmSdMapSection(&Context, 0x10000000, 0x10000000, ARM_SD_DEVICE) == 0, 27);
    Check(ArmSdMapPage(&Context, 0xc0000000, (uint32_t)(uintptr_t)PageA, 0) == 0, 6);
    Check(ArmSdMapPage(&Context, 0xc0001000, (uint32_t)(uintptr_t)PageB, 0) == 0, 7);
    Check(ArmSdMapPage(&Context, 0xc0002000, (uint32_t)(uintptr_t)PageA, ARM_SD_USER) == 0, 8);
    Check(ArmSdMapPage(&Context, 0xc0100000, (uint32_t)(uintptr_t)PageA, ARM_SD_READ_ONLY) == 0, 9);
    Check(ArmSdMapPage(&Context, 0xc0200000, (uint32_t)(uintptr_t)Code, 0) == 0, 10);
    Check(ArmSdMapPage(&Context, 0xc0300000, (uint32_t)(uintptr_t)Code, ARM_SD_EXECUTE) == 0, 11);
    Check(ArmSdMapSection(&Context, 0xe0000000, 0x40000000, ARM_SD_EXECUTE) == 0, 28);
    Check(ArmSdUnmapPage(&Context, 0xe000f000) == 0, 29);
    AlignmentPage[0] = 0x04030201;
    AlignmentPage[1] = 0x08070605;
    Check(ArmSdUnmapPage(&Context, (uint32_t)(uintptr_t)AlignmentPage) == 0, 40);
    Check(ArmSdMapPage(&Context, 0xc0400000, (uint32_t)(uintptr_t)AlignmentPage, ARM_SD_DEVICE) == 0, 41);
    PageA[0] = 0x11223344;
    PageB[0] = 0x55667788;
    Code[0] = 0xe3a0002a;
    Code[1] = 0xe12fff1e;
    {
        uint32_t MetaPage = (uint32_t)(uintptr_t)ShortMetadata & ~4095u;
        Check(ArmSdUnmapPage(&Context, MetaPage) == 0, 32);
        Check(ArmSdMapPage(&Context, MetaPage, MetaPage, ARM_SD_WRITE_BACK | ARM_SD_NONSHAREABLE) == 0, 33);
    }
    EnableMmu();
    {
        uint64_t Value = 0x01234567deadbeefULL;
        uint32_t Hardware = Root[0xc00];
        MiArmWriteShortBootPtePhysical((uint32_t)(uintptr_t)&Root[0xc00], Hardware,
                                      (uint32_t)(uintptr_t)ShortMetadata, &Value);
        Check(Root[0xc00] == Hardware && ShortMetadata[0] == Value, 34);
        Print("NVS short-descriptor physical writer: PASS\r\n");
    }
    Check(SmdkTryRead(0xc0000000) == PageA[0], 12);
    Expect(1, 1, 0xc0400001);
    SmdkTryRead(0xc0400001);
    if (ExpectedKind)
    {
        Print("QEMU note: Device-memory unaligned fault was not emulated\r\n");
        ExpectedKind = 0;
    }
    else
        AlignmentAborts++;
    Check(ArmSdUnmapPage(&Context, 0xc0400000) == 0, 43);
    Invalidate(0xc0400000);
    Check(ArmSdMapPage(&Context, 0xc0400000, (uint32_t)(uintptr_t)AlignmentPage,
                       ARM_SD_WRITE_BACK | ARM_SD_NONSHAREABLE) == 0, 44);
    Invalidate(0xc0400000);
    Check(SmdkTryRead(0xc0400001) == 0x05040302, 45);
    __asm__ volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(Sctlr));
    Midr = Sctlr | 2u;
    __asm__ volatile("mcr p15, 0, %0, c1, c0, 0\n\tisb" :: "r"(Midr) : "memory");
    Expect(1, 1, 0xc0400001);
    SmdkTryRead(0xc0400001);
    Check(!ExpectedKind, 46);
    __asm__ volatile("mcr p15, 0, %0, c1, c0, 0\n\tisb" :: "r"(Sctlr) : "memory");
    Print("Cortex-A9 Normal memory unaligned read and alignment trap PASS\r\n");
    Check(SmdkTryRead(0xc0001000) == PageB[0], 13);
    SmdkTryWrite(0xc0000000, 0x31415926);
    Check(PageA[0] == 0x31415926, 14);
    Print("Short descriptors: section identity and 4K high aliases PASS\r\n");
    Expect(1, 15, 0xc0100000);
    SmdkTryWrite(0xc0100000, 0xbad);
    Check(!ExpectedKind && PageA[0] == 0x31415926, 15);
    Expect(1, 15, 0xc0000000);
    SmdkTryUnprivilegedRead(0xc0000000);
    Check(!ExpectedKind, 16);
    Check(SmdkTryUnprivilegedRead(0xc0002000) == PageA[0], 17);
    Print("Short descriptors: read-only and user access permissions PASS\r\n");
    Expect(1, 5, 0xd0000000);
    SmdkTryRead(0xd0000000);
    Check(!ExpectedKind, 18);
    Expect(1, 7, 0xc000f000);
    SmdkTryRead(0xc000f000);
    Check(!ExpectedKind, 19);
    Expect(1, 7, 0xe000f000);
    SmdkTryRead(0xe000f000);
    Check(!ExpectedKind && SmdkTryRead(0xe0010000) == SmdkTryRead(0x40010000), 30);
    Expect(2, 15, 0xc0200000);
    SmdkTryExecute(0xc0200000);
    Check(!ExpectedKind, 20);
    Check(((uint32_t (*)(void))(uintptr_t)0xc0300000)() == 42, 21);
    Print("Short descriptors: translation faults and execute-never PASS\r\n");
    Slot = &Tables[0];
    *Slot = 0;
    Invalidate(0xc0000000);
    *Slot = Tables[1];
    Invalidate(0xc0000000);
    Check(SmdkTryRead(0xc0000000) == PageB[0], 22);
    Check(AbortCount == 7 + AlignmentAborts, 23);
    Print("Short descriptors: break-before-make and TLB invalidation PASS\r\n");
    TestInterrupts();
    TestRtc();
    Print("SMDKC210 tests: PASS (processor/MMU only; no kernel startup)\r\n");
    {
        register uint32_t Operation __asm__("r0") = 0x18;
        register uint32_t Reason __asm__("r1") = 0x20026;
        __asm__ volatile("svc 0x123456" : "+r"(Operation), "+r"(Reason) :: "memory");
    }
}
