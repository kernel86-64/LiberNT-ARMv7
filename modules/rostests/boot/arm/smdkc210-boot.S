/*
 * PROJECT:     LiberNT boot tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Establish the SMDKC210 display before entering the UEFI payload
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

.syntax unified
.arm
.section .text, "ax", %progbits
.global SmdkBoot
.type SmdkBoot, %function
SmdkBoot:
    cpsid if
    mrc p15, 0, r0, c0, c0, 5
    ands r0, r0, #0xff
    bne SmdkSecondaryBoot
    ldr r0, =0x11c00000
    mov r1, #0
    str r1, [r0, #0x00]
    str r1, [r0, #0x20]
    str r1, [r0, #0x40]
    ldr r1, =((1280 - 1) << 11) | (800 - 1)
    str r1, [r0, #0x18]
    ldr r1, =((800 - 1) << 11) | (1280 - 1)
    str r1, [r0, #0x44]
    ldr r1, =(800 * 1280)
    str r1, [r0, #0x48]
    ldr r1, =0x5ea00000
    str r1, [r0, #0xa0]
    ldr r1, =(0x5ea00000 + 800 * 1280 * 4)
    str r1, [r0, #0xd0]
    ldr r1, =(800 * 4)
    str r1, [r0, #0x100]
    ldr r1, =0x8035
    str r1, [r0, #0x20]
    mov r1, #3
    str r1, [r0, #0x00]
    dsb sy
    mov r0, #0
    mov r1, #0
    mov r2, #0
    ldr pc, =0x40010000
.size SmdkBoot, . - SmdkBoot

.type SmdkSecondaryBoot, %function
SmdkSecondaryBoot:
    ldr r0, =0x10020814
1:
    ldr r1, [r0]
    cmp r1, #0
    beq 1b
    dsb sy
    bx r1
.size SmdkSecondaryBoot, . - SmdkSecondaryBoot
