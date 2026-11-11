/*
 * PROJECT:     LiberNT ARM platform support
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Exynos4210 global MCT counter and comparator access
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#pragma once

#define EXYNOS_MCT_PHYSICAL 0x10050000U
#define EXYNOS_MCT_VIRTUAL  0xfffc0000U
#define EXYNOS_MCT_VECTOR  89U
#define EXYNOS_MCT_RATE    24000000U

static inline unsigned int
ExynosMctRead(unsigned int Base, unsigned int Offset)
{
    unsigned int Value = *(volatile unsigned int *)(Base + Offset);
    __asm__ __volatile__("dmb sy" ::: "memory");
    return Value;
}

static inline void
ExynosMctWrite(unsigned int Base, unsigned int Offset, unsigned int Value)
{
    __asm__ __volatile__("dmb sy" ::: "memory");
    *(volatile unsigned int *)(Base + Offset) = Value;
    __asm__ __volatile__("dsb sy" ::: "memory");
}

static inline int
ExynosMctLatch(unsigned int Base, unsigned int Offset, unsigned int Value, unsigned int Ack)
{
    unsigned int Limit = 1000000;
    ExynosMctWrite(Base, Offset, Value);
    while (!(ExynosMctRead(Base, 0x24c) & Ack))
        if (!--Limit) return 0;
    ExynosMctWrite(Base, 0x24c, Ack);
    return 1;
}

static inline unsigned long long
ExynosMctCounter(unsigned int Base)
{
    unsigned int High, Low;
    do
    {
        High = ExynosMctRead(Base, 0x104);
        Low = ExynosMctRead(Base, 0x100);
    } while (High != ExynosMctRead(Base, 0x104));
    return ((unsigned long long)High << 32) | Low;
}

static inline void
ExynosMctAcknowledge(unsigned int Base)
{
    ExynosMctWrite(Base, 0x244, 1);
    ExynosMctWrite(Base, 0x244, 1);
}

static inline int
ExynosMctComparator(unsigned int Base, unsigned long long Deadline, unsigned int Period)
{
    unsigned int Control = ExynosMctRead(Base, 0x240) & ~3U;
    if (!ExynosMctLatch(Base, 0x240, Control | 0x100, 1U << 16)) return 0;
    ExynosMctAcknowledge(Base);
    if (!ExynosMctLatch(Base, 0x200, (unsigned int)Deadline, 1) ||
        !ExynosMctLatch(Base, 0x204, (unsigned int)(Deadline >> 32), 2)) return 0;
    if (Period && !ExynosMctLatch(Base, 0x208, Period, 4)) return 0;
    ExynosMctWrite(Base, 0x248, 1);
    return ExynosMctLatch(Base, 0x240, Control | 0x101 | (Period ? 2 : 0), 1U << 16);
}

static inline int
ExynosMctDeadline(unsigned int Base, unsigned long long Deadline)
{
    return ExynosMctComparator(Base, Deadline, 0);
}
