/*
 * PROJECT:     LiberNT Kernel
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARMv7 physical-mode trampoline for a parked application processor
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

    .thumb
    .syntax unified
    .text
    .align 12

    .global KiArmApBootStart
    .global KiArmApBootEnd
    .global KiArmApBootThumb

KiArmApBootStart:
    .word   0xE59FF000
    .word   KiArmApBootThumb + 1
    .word   KiArmApBootThumb + 1

    .thumb_func
KiArmApBootThumb:
    cpsid   aif
    movs    r1, #0xD3
    msr     cpsr_cxsf, r1
    ldr     r4, [r0, #0x10]
    movs    r0, #0x11
    str     r0, [r4, #0x24]

    ldr     r0, [r4, #0x00]
    mcr     p15, 0, r0, c2, c0, 0
    ldr     r0, [r4, #0x04]
    mcr     p15, 0, r0, c2, c0, 2
    ldr     r0, [r4, #0x08]
    mcr     p15, 0, r0, c3, c0, 0
    ldr     r0, [r4, #0x0C]
    mcr     p15, 0, r0, c10, c2, 0
    ldr     r0, [r4, #0x10]
    mcr     p15, 0, r0, c10, c2, 1
    ldr     r0, [r4, #0x20]
    mcr     p15, 0, r0, c12, c0, 0

    ldr     r0, [r4, #0x28]
    orr     r0, r0, #(1 << 6)
    mcr     p15, 0, r0, c1, c0, 1

    movs    r0, #0
    mcr     p15, 0, r0, c8, c7, 0
    mcr     p15, 0, r0, c7, c5, 0
    mcr     p15, 0, r0, c7, c5, 6
    mcr     p15, 2, r0, c0, c0, 0
    isb
    mrc     p15, 1, r0, c0, c0, 0
    ands    r1, r0, #7
    add     r1, r1, #4
    ubfx    r2, r0, #3, #10
    ubfx    r5, r0, #13, #15
    clz     r3, r2
1:
    mov     r0, r5
    lsl     r0, r0, r1
    mov     r6, r2
2:
    lsl     r7, r6, r3
    orr     r7, r7, r0
    mcr     p15, 0, r7, c7, c6, 2
    subs    r6, r6, #1
    bpl     2b
    subs    r5, r5, #1
    bpl     1b
    dsb     sy
    isb

    ldr     r10, [r4, #0x18]
    ldr     r11, [r4, #0x1C]
    ldr     r12, [r4, #0x14]
    mcr     p15, 0, r12, c1, c0, 0
    isb
    movs    r0, #0x22
    str     r0, [r4, #0x24]
    mov     r0, r11
    bx      r10

1:
    wfi
    b       1b

KiArmApBootEnd:
