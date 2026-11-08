/*
 * PROJECT:     LiberNT Storage
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Initial QEMU virt VirtIO MMIO discovery
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include "virtblk.h"

PVOID VbPlatformMap(ULONG Index)
{
    PHYSICAL_ADDRESS Address;

    if (Index >= 32)
        return NULL;
    Address.QuadPart = 0x0A000000 + Index * 0x200;
    /* TODO: discover VirtIO MMIO resources from the firmware device tree. */
    return MmMapIoSpace(Address, 0x200, MmNonCached);
}
