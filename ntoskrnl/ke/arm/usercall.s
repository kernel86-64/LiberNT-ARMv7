/*
 * PROJECT:     LiberNT Kernel
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARM32 kernel callback continuation
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ksarm.h>

    TEXTAREA

    NESTED_ENTRY KiCallUserMode
    push.w {r11, lr}
    .seh_save_regs_w {r11, lr}
    mov r11, sp
    .seh_save_sp r11
    push.w {r4-r10}
    .seh_save_regs_w {r4-r10}
    sub sp, sp, #4
    .seh_stackalloc 4
    vpush {d8-d15}
    .seh_save_fregs {d8-d15}
    PROLOG_END
    vmrs r3, fpscr
    str r3, [sp, #64]
    mov r2, sp
    bl KiArmEnterCallback
    mrs r2, cpsr
    b .Lcallback_restore
    NESTED_END

    LEAF_ENTRY KiArmCallbackReturn
    mov sp, r0
    mov r0, r1
.Lcallback_restore:
    vpop {d8-d15}
    ldr r3, [sp]
    vmsr fpscr, r3
    add sp, sp, #4
    pop.w {r4-r10}
    pop.w {r11, lr}
    msr cpsr_c, r2
    bx lr
    LEAF_END

    END
