/*
 * PROJECT:     ReactOS vcruntime library
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Implementation of __longjmp_noframe for ARM
 * COPYRIGHT:   Copyright Timo Kreuzer <timo.kreuzer@reactos.org>
 */

#include <kxarm.h>

    TEXTAREA

    LEAF_ENTRY __longjmp_noframe
    mov r2, r0
    mov r0, r1
    cmp r0, #0
    it eq
    moveq r0, #1
    adds r2, #4
    ldmia r2!, {r4-r11}
    ldr r3, [r2], #4
    mov sp, r3
    ldr lr, [r2], #4
    ldr r3, [r2], #4
    vmsr fpscr, r3
    vldmia r2, {d8-d15}
    bx lr
    LEAF_END __longjmp_noframe

    END
