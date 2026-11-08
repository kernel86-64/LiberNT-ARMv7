/*
 * PROJECT:     LiberNT FreeLoader
 * LICENSE:     GPL-2.0-or-later (https://spdx.org/licenses/GPL-2.0-or-later)
 * PURPOSE:     ARMv7 UEFI entry with VFP enabled before C code
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

    .syntax unified
    .thumb
    .text

    .globl ArmEfiEntry
    .thumb_func
ArmEfiEntry:
    mrc p15, 0, r2, c1, c0, 2
    orr r2, r2, #0x00f00000
    mcr p15, 0, r2, c1, c0, 2
    isb
    mov r2, #0x40000000
    vmsr fpexc, r2
    b EfiEntry

