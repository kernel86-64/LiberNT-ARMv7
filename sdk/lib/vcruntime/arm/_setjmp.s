/*
 * PROJECT:     ReactOS vcruntime library
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Implementation of _setjmp for ARM
 * COPYRIGHT:   Copyright Timo Kreuzer <timo.kreuzer@reactos.org>
 */

#include <kxarm.h>

    TEXTAREA

    LEAF_ENTRY _setjmpex
    stmia r0!, {r1,r4-r11}
    mov r2, sp
    vmrs r3, fpscr
    stmia r0!, {r2,lr}
    str r3, [r0], #4
    vstmia r0, {d8-d15}
    movs r0, #0
    bx lr
    LEAF_END _setjmpex

    LEAF_ENTRY _setjmp
    b _setjmpex
    LEAF_END _setjmp

    LEAF_ENTRY __intrinsic_setjmp
    b _setjmpex
    LEAF_END __intrinsic_setjmp

    LEAF_ENTRY __intrinsic_setjmpex
    b _setjmpex
    LEAF_END __intrinsic_setjmpex

    LEAF_ENTRY setjmp
    b _setjmp
    LEAF_END setjmp

    END
