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
#ifndef ARM_PAGING_BARE_TEST
    .seh_proc MiArmWriteBootPtePhysical
#endif
MiArmWriteBootPtePhysical:
    push.w {r4-r8, lr}
#ifndef ARM_PAGING_BARE_TEST
    .seh_save_regs_w {r4-r8, lr}
#endif
#ifndef ARM_PAGING_BARE_TEST
    .seh_endprologue
#endif
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
#ifndef ARM_PAGING_BARE_TEST
    .seh_startepilogue
#endif
    pop.w {r4-r8, pc}
#ifndef ARM_PAGING_BARE_TEST
    .seh_save_regs_w {r4-r8, lr}
#endif
#ifndef ARM_PAGING_BARE_TEST
    .seh_endepilogue
#endif
#ifndef ARM_PAGING_BARE_TEST
    .seh_endproc
#endif


    .global MiArmWriteShortBootPtePhysical
    .thumb_func
#ifndef ARM_PAGING_BARE_TEST
    .seh_proc MiArmWriteShortBootPtePhysical
#endif
MiArmWriteShortBootPtePhysical:
    push.w {r4-r10, lr}
#ifndef ARM_PAGING_BARE_TEST
    .seh_save_regs_w {r4-r10, lr}
#endif
#ifndef ARM_PAGING_BARE_TEST
    .seh_endprologue
#endif
    mov r7, r0
    mov r8, r1
    mov r6, r2
    ldrd r4, r5, [r3]
    mrs r9, cpsr
    cpsid if
    movs r0, #0
    str r0, [r7]
    mcr p15, 0, r7, c7, c10, 1
    dsb sy
    mcr p15, 0, r0, c8, c3, 0
    dsb sy
    isb
3:
    ldrexd r0, r1, [r6]
    strexd r2, r4, r5, [r6]
    cmp r2, #0
    bne 3b
    mcr p15, 0, r6, c7, c10, 1
    dsb sy
    str r8, [r7]
    mcr p15, 0, r7, c7, c10, 1
    dsb sy
    isb
    msr cpsr_c, r9
#ifndef ARM_PAGING_BARE_TEST
    .seh_startepilogue
#endif
    pop.w {r4-r10, pc}
#ifndef ARM_PAGING_BARE_TEST
    .seh_save_regs_w {r4-r10, lr}
#endif
#ifndef ARM_PAGING_BARE_TEST
    .seh_endepilogue
#endif
#ifndef ARM_PAGING_BARE_TEST
    .seh_endproc
#endif
