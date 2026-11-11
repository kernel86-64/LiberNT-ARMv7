/*
 * PROJECT:     LiberNT ARM platform support
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Exynos4210 real-time clock register access
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#pragma once

#define EXYNOS_RTC_PHYSICAL 0x10070000U
#define EXYNOS_RTC_RTCCON   0x0040U
#define EXYNOS_RTC_BCDSEC   0x0070U
#define EXYNOS_RTC_BCDMIN   0x0074U
#define EXYNOS_RTC_BCDHOUR  0x0078U
#define EXYNOS_RTC_BCDDATE  0x007cU
#define EXYNOS_RTC_BCDDAY   0x0080U
#define EXYNOS_RTC_BCDMON   0x0084U
#define EXYNOS_RTC_BCDYEAR  0x0088U
#define EXYNOS_RTC_ENABLE   0x0001U

static inline unsigned int
ExynosRtcRead(unsigned int Base, unsigned int Offset)
{
    unsigned int Value = *(volatile unsigned int *)(Base + Offset);
    __asm__ __volatile__("dmb sy" ::: "memory");
    return Value;
}

static inline void
ExynosRtcWrite(unsigned int Base, unsigned int Offset, unsigned int Value)
{
    __asm__ __volatile__("dmb sy" ::: "memory");
    *(volatile unsigned int *)(Base + Offset) = Value;
    __asm__ __volatile__("dsb sy" ::: "memory");
}

static inline int
ExynosRtcBcdToBinary(unsigned int Bcd, unsigned int *Value)
{
    unsigned int Low = Bcd & 15U;
    unsigned int High = (Bcd >> 4) & 15U;

    if (Low > 9U || High > 9U) return 0;
    *Value = High * 10U + Low;
    return 1;
}

static inline unsigned int
ExynosRtcBinaryToBcd(unsigned int Value)
{
    return ((Value / 10U) << 4) | (Value % 10U);
}
