/*
 * PROJECT:     LiberNT runtime library
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Default handling of processor specific image relocations
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ntdef.h>

BOOLEAN RtlpProcessMachineRelocation(ULONG Type, PUSHORT Address, LONGLONG Delta)
{
    UNREFERENCED_PARAMETER(Type);
    UNREFERENCED_PARAMETER(Address);
    UNREFERENCED_PARAMETER(Delta);
    return FALSE;
}
