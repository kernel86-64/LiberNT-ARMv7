/*
 * PROJECT:     LiberNT Kernel
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARM32 processor names from documented MIDR implementer and part IDs
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#pragma once
#include <stdint.h>

static inline const char *KiArmProcessorName(uint32_t Midr)
{
    if ((Midr >> 24) != 0x41) return 0;
    switch ((Midr >> 4) & 0xfff)
    {
        case 0xc09: return "ARM Cortex-A9";
        case 0xc0f: return "ARM Cortex-A15";
        default: return 0;
    }
}
