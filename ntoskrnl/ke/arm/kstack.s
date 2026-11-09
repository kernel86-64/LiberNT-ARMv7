/*
 * PROJECT:     LiberNT Kernel
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARM32 kernel stack pointer handoff
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ksarm.h>

    TEXTAREA

    NESTED_ENTRY KeSwitchKernelStack
    push.w {r11, lr}
    .seh_save_regs_w {r11, lr}
    mov r11, sp
    .seh_save_sp r11
    push.w {r4-r10}
    .seh_save_regs_w {r4-r10}
    sub sp, sp, #4
    .seh_stackalloc 4
    PROLOG_END
    mrs r2, cpsr
    cpsid if
    str r2, [sp]
    mov r4, r0
    mov r2, sp
    mov r3, r11
    bl KiArmSwitchKernelStack
    dsb sy
    sub r4, r4, r0
    add sp, sp, r4
    add r11, r11, r4
    ldr r2, [sp]
    .seh_startepilogue
    add sp, sp, #4
    .seh_stackalloc 4
    pop.w {r4-r10}
    .seh_save_regs_w {r4-r10}
    pop.w {r11, lr}
    .seh_save_regs_w {r11, lr}
    msr cpsr_c, r2
    .seh_nop_w
    bx lr
    .seh_nop
    .seh_endepilogue
    NESTED_END

    NESTED_ENTRY KiRetireDpcListInDpcStack
    push.w {r11, lr}
    .seh_save_regs_w {r11, lr}
    mov r11, sp
    .seh_save_sp r11
    push {r4, r5}
    .seh_save_regs {r4, r5}
    PROLOG_END
    mov r4, sp
    mov sp, r1
    bl KiRetireDpcList
    mov sp, r4
    .seh_startepilogue
    pop {r4, r5}
    .seh_save_regs {r4, r5}
    pop.w {r11, lr}
    .seh_save_regs_w {r11, lr}
    bx lr
    .seh_nop
    .seh_endepilogue
    NESTED_END

    END
