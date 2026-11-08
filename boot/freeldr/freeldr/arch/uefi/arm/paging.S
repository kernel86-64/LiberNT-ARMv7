/*
 * PROJECT:     LiberNT FreeLoader
 * LICENSE:     GPL-2.0-or-later (https://spdx.org/licenses/GPL-2.0-or-later)
 * PURPOSE:     ARMv7 LPAE handoff from UEFI
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

    .syntax unified
    .thumb
    .text
    .globl ArmEnablePaging
    .thumb_func
    .seh_proc ArmEnablePaging
ArmEnablePaging:
    push.w {r4-r10, lr}
    .seh_save_regs_w {r4-r10, lr}
    .seh_endprologue
    cpsid if
    mov r4, r0
    mrc p15, 1, r5, c0, c0, 1
    movs r6, #0
.Lcache_level:
    add r1, r6, r6, lsl #1
    lsr r0, r5, r1
    and r0, r0, #7
    cmp r0, #2
    blo .Lcache_next
    cmp r0, #4
    bhi .Lcache_next
    lsl r0, r6, #1
    mcr p15, 2, r0, c0, c0, 0
    isb
    mrc p15, 1, r0, c0, c0, 0
    ubfx r7, r0, #3, #10
    ubfx r8, r0, #13, #15
    and r9, r0, #7
    add r9, r9, #4
    clz r10, r7
    mov r2, r8
.Lcache_set:
    mov r3, r7
.Lcache_way:
    lsl r0, r6, #1
    lsl r1, r3, r10
    orr r0, r0, r1
    lsl r1, r2, r9
    orr r0, r0, r1
    mcr p15, 0, r0, c7, c14, 2
    subs r3, r3, #1
    bpl .Lcache_way
    subs r2, r2, #1
    bpl .Lcache_set
.Lcache_next:
    adds r6, r6, #1
    cmp r6, #7
    blo .Lcache_level
    dsb sy
    movs r0, #0
    mcr p15, 2, r0, c0, c0, 0
    isb
    mrc p15, 0, r5, c1, c0, 0
    bic r5, r5, #1
    bic r5, r5, #4
    bic r5, r5, #0x1800
    bic r5, r5, #0x80000
    mcr p15, 0, r5, c1, c0, 0
    isb
    ldr r0, =0x80803500
    mcr p15, 0, r0, c2, c0, 2
    ldr r0, =0x444400ff
    mcr p15, 0, r0, c10, c2, 0
    movs r0, #0
    mcr p15, 0, r0, c10, c2, 1
    mcrr p15, 0, r4, r0, c2
    isb
    mcr p15, 0, r0, c8, c7, 0
    mcr p15, 0, r0, c7, c5, 0
    mcr p15, 0, r0, c7, c5, 6
    dsb sy
    isb
    orr r5, r5, #1
    orr r5, r5, #4
    orr r5, r5, #0x1800
    mcr p15, 0, r5, c1, c0, 0
    isb
    .seh_startepilogue
    pop.w {r4-r10, pc}
    .seh_save_regs_w {r4-r10, lr}
    .seh_endepilogue
    .seh_endproc
