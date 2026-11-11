/*
 * PROJECT:     LiberNT Kernel
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Decode ARMv7 short and long descriptor abort status formats
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#pragma once
#include <stdint.h>

static inline uint32_t KiArmAbortStatus(uint32_t Fault)
{
    return (Fault & (1U << 9)) ? Fault & 0x3f : (Fault & 15) | ((Fault >> 6) & 16);
}

static inline int KiArmAbortCanPage(uint32_t Fault)
{
    uint32_t Status = KiArmAbortStatus(Fault);
    if (Fault & (1U << 9)) return Status >= 4 && Status <= 15;
    return Status == 3 || Status == 5 || Status == 6 || Status == 7 ||
           Status == 13 || Status == 15;
}

static inline int KiArmAbortAlignment(uint32_t Fault)
{
    return KiArmAbortStatus(Fault) == ((Fault & (1U << 9)) ? 0x21U : 1U);
}

static inline int KiArmAbortDebug(uint32_t Fault)
{
    return KiArmAbortStatus(Fault) == ((Fault & (1U << 9)) ? 0x22U : 2U);
}
