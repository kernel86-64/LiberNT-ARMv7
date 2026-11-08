/*
 * PROJECT:     LiberNT API tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARM PE relocation regression tests
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ntdef.h>
#include <string.h>

BOOLEAN RtlpProcessMachineRelocation(ULONG Type, PUSHORT Address, LONGLONG Delta);

BOOLEAN ArmTestRelocations(VOID)
{
    USHORT Thumb[] = {0xF245, 0x6378, 0xF2C1, 0x2334};
    const USHORT ThumbAdjusted[] = {0xF246, 0x6378, 0xF2C1, 0x2334};
    const USHORT ThumbOriginal[] = {0xF245, 0x6378, 0xF2C1, 0x2334};
    USHORT Wrapped[] = {0xF64F, 0x73FF, 0xF6CF, 0x73FF};
    const USHORT Zero[] = {0xF240, 0x0300, 0xF2C0, 0x0300};
    USHORT Arm[] = {0x3678, 0xE305, 0x3234, 0xE341};
    const USHORT ArmAdjusted[] = {0x3678, 0xE306, 0x3234, 0xE341};
    USHORT Malformed[] = {0, 0, 0, 0};

    if (!RtlpProcessMachineRelocation(7, Thumb, 0x1000) || memcmp(Thumb, ThumbAdjusted, sizeof(Thumb)))
        return FALSE;
    if (!RtlpProcessMachineRelocation(7, Thumb, -0x1000) || memcmp(Thumb, ThumbOriginal, sizeof(Thumb)))
        return FALSE;
    if (!RtlpProcessMachineRelocation(7, Wrapped, 1) || memcmp(Wrapped, Zero, sizeof(Wrapped)))
        return FALSE;
    if (!RtlpProcessMachineRelocation(5, Arm, 0x1000) || memcmp(Arm, ArmAdjusted, sizeof(Arm)))
        return FALSE;
    if (RtlpProcessMachineRelocation(7, Malformed, 0x1000) ||
        RtlpProcessMachineRelocation(5, Malformed, 0x1000) ||
        RtlpProcessMachineRelocation(6, Thumb, 0x1000))
        return FALSE;
    return TRUE;
}
