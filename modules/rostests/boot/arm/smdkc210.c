/*
 * PROJECT:     LiberNT boot tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Exercise Cortex-A9 short-descriptor translation on SMDKC210
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include "shortdesc.h"
#include "processor.h"

static _Alignas(16384) uint32_t Root[ARM_SD_ROOT_ENTRIES];
static _Alignas(1024) uint32_t Tables[8 * ARM_SD_TABLE_ENTRIES];
static _Alignas(4096) uint32_t PageA[1024];
static _Alignas(4096) uint32_t PageB[1024];
static _Alignas(4096) uint32_t Code[1024];
static volatile uint32_t ExpectedKind, ExpectedStatus, ExpectedAddress, AbortCount;
static volatile uint32_t SgiCount, TimerCount;
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

static void MctWrite(uint32_t Offset, uint32_t Value, uint32_t Acknowledge)
{
    WriteRegister(0x10050000 + Offset, Value);
    while (!(ReadRegister(0x1005024c) & Acknowledge)) {}
    WriteRegister(0x1005024c, Acknowledge);
}

static uint64_t MctCounter(void)
{
    uint32_t High, Low;
    do
    {
        High = ReadRegister(0x10050104);
        Low = ReadRegister(0x10050100);
    } while (High != ReadRegister(0x10050104));
    return ((uint64_t)High << 32) | Low;
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
        WriteRegister(0x10050244, 1);
        TimerCount++;
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
    MctWrite(0x240, 0x100, 1u << 16);
    Before = MctCounter();
    Compare = Before + 240000;
    MctWrite(0x204, (uint32_t)(Compare >> 32), 2);
    MctWrite(0x200, (uint32_t)Compare, 1);
    MctWrite(0x208, 24000, 4);
    WriteRegister(0x10050244, 1);
    WriteRegister(0x10050248, 1);
    MctWrite(0x240, 0x103, 1u << 16);
    __asm__ volatile("cpsie i\n\tisb" ::: "memory");
    while (TimerCount < 5) __asm__ volatile("nop");
    __asm__ volatile("cpsid i" ::: "memory");
    WriteRegister(0x10050248, 0);
    MctWrite(0x240, 0x100, 1u << 16);
    Check(MctCounter() > Before && TimerCount >= 5, 25);
    Print("Exynos4210 MCT: counter and five comparator interrupts PASS\r\n");
}

static void Invalidate(uint32_t Address)
{
    __asm__ volatile("dsb ishst\n\tmcr p15, 0, %0, c8, c7, 1\n\tdsb ish\n\tisb"
                     :: "r"(Address) : "memory");
}

static void EnableMmu(void)
{
    uint32_t Sctlr, Zero = 0, Domain = 1, Base = (uint32_t)(uintptr_t)Root;

    __asm__ volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(Sctlr));
    Check(!(Sctlr & 5u), 1);
    Sctlr &= ~((1u << 28) | (1u << 20) | (1u << 19) | (1u << 13) | (1u << 12));
    Sctlr |= (1u << 29) | (1u << 23) | 1u;
    __asm__ volatile(
        "dsb sy\n\t"
        "mcr p15, 0, %0, c2, c0, 2\n\t"
        "mcr p15, 0, %0, c13, c0, 1\n\t"
        "mcr p15, 0, %1, c2, c0, 0\n\t"
        "mcr p15, 0, %2, c3, c0, 0\n\t"
        "mcr p15, 0, %0, c8, c7, 0\n\t"
        "mcr p15, 0, %0, c7, c5, 0\n\t"
        "dsb sy\n\tisb\n\t"
        "mcr p15, 0, %3, c1, c0, 0\n\tisb"
        :: "r"(Zero), "r"(Base), "r"(Domain), "r"(Sctlr) : "memory");
}

void SmdkMain(void)
{
    ARM_SD_CONTEXT Context;
    uint32_t Midr, Mmfr0, Pfr1, *Slot;

    Print("SMDKC210 Cortex-A9 tests: BEGIN\r\n");
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
    PageA[0] = 0x11223344;
    PageB[0] = 0x55667788;
    Code[0] = 0xe3a0002a;
    Code[1] = 0xe12fff1e;
    EnableMmu();
    Check(SmdkTryRead(0xc0000000) == PageA[0], 12);
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
    Check(AbortCount == 6, 23);
    Print("Short descriptors: break-before-make and TLB invalidation PASS\r\n");
    TestInterrupts();
    Print("SMDKC210 tests: PASS (processor/MMU only; no kernel startup)\r\n");
    {
        register uint32_t Operation __asm__("r0") = 0x18;
        register uint32_t Reason __asm__("r1") = 0x20026;
        __asm__ volatile("svc 0x123456" : "+r"(Operation), "+r"(Reason) :: "memory");
    }
}
