/*
 * PROJECT:     LiberNT VirtIO Input
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     QEMU virt VirtIO input platform discovery
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */
#include "virtinput.h"

PVOID ViPlatformMap(ULONG Slot, PULONG Vector)
{
    PHYSICAL_ADDRESS Address;
    if (Slot >= 32) return NULL;
    Address.QuadPart = 0x0a000000 + Slot * 0x200;
    *Vector = 48 + Slot;
    /* TODO: enumerate VirtIO MMIO resources through the firmware device tree. */
    return MmMapIoSpace(Address, 0x200, MmNonCached);
}
