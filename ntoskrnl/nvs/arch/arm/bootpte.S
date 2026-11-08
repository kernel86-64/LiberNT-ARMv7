/*
 * PROJECT:     LiberNT Kernel
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Replace a live boot block through its identity alias
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

    .syntax unified
    .thumb
    .text
    .global MiArmWriteBootPtePhysical
    .thumb_func
    .seh_proc MiArmWriteBootPtePhysical
MiArmWriteBootPtePhysical:
    push.w {r4-r8, lr}
    .seh_save_regs_w {r4-r8, lr}
    .seh_endprologue
    mov r4, r0
    mrs r5, cpsr
    cpsid if
1:
    ldrexd r6, r7, [r4]
    movs r6, #0
    movs r7, #0
    strexd r8, r6, r7, [r4]
    cmp r8, #0
    bne 1b
    dsb ishst
    movs r1, #0
    mcr p15, 0, r1, c8, c3, 0
    dsb ish
    isb
2:
    ldrexd r6, r7, [r4]
    strexd r8, r2, r3, [r4]
    cmp r8, #0
    bne 2b
    dsb ishst
    isb
    msr cpsr_c, r5
    .seh_startepilogue
    pop.w {r4-r8, pc}
    .seh_save_regs_w {r4-r8, lr}
    .seh_endepilogue
    .seh_endproc
