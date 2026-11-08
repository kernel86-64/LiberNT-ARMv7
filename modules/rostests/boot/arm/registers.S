/*
 * PROJECT:     LiberNT boot tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARMv7 UEFI ABI regression tests
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <kxarm.h>

    TEXTAREA

    NESTED_ENTRY ArmTestSetjmpRegisters
    push {r3-r11, lr}
    .seh_save_regs_w {r3-r11, lr}
    vpush {d8-d15}
    .seh_save_fregs {d8-d15}
    sub sp, sp, #120
    .seh_stackalloc 120
    PROLOG_END
    vmrs r3, fpscr
    str r3, [sp]
    mov r3, #0x01000000
    vmsr fpscr, r3
    mov r4, #68
    mov r5, #69
    mov r6, #70
    mov r7, #71
    mov r8, #72
    mov r9, #73
    mov r10, #74
    mov r11, #75
    vmov.f64 d8, #1.0
    vmov.f64 d9, #2.0
    vmov.f64 d10, #3.0
    vmov.f64 d11, #4.0
    vmov.f64 d12, #5.0
    vmov.f64 d13, #6.0
    vmov.f64 d14, #7.0
    vmov.f64 d15, #8.0
    add r0, sp, #8
    movs r1, #0
    bl _setjmp
    cmp r0, #0
    bne .Lreturned
    mov r4, #0
    mov r5, #0
    mov r6, #0
    mov r7, #0
    mov r8, #0
    mov r9, #0
    mov r10, #0
    mov r11, #0
    vmov.f64 d8, #16.0
    vmov.f64 d9, #16.0
    vmov.f64 d10, #16.0
    vmov.f64 d11, #16.0
    vmov.f64 d12, #16.0
    vmov.f64 d13, #16.0
    vmov.f64 d14, #16.0
    vmov.f64 d15, #16.0
    movs r3, #0
    vmsr fpscr, r3
    add r0, sp, #8
    movs r1, #42
    bl longjmp
.Lreturned:
    vmrs r2, fpscr
    cmp r2, #0x01000000
    bne .Lfail
    cmp r0, #42
    bne .Lfail
    cmp r4, #68
    bne .Lfail
    cmp r5, #69
    bne .Lfail
    cmp r6, #70
    bne .Lfail
    cmp r7, #71
    bne .Lfail
    cmp r8, #72
    bne .Lfail
    cmp r9, #73
    bne .Lfail
    cmp r10, #74
    bne .Lfail
    cmp r11, #75
    bne .Lfail
    vmov r2, r3, d8
    cmp r2, #0
    bne .Lfail
    movw r1, #0
    movt r1, #16368
    cmp r3, r1
    bne .Lfail
    vmov r2, r3, d9
    cmp r2, #0
    bne .Lfail
    movw r1, #0
    movt r1, #16384
    cmp r3, r1
    bne .Lfail
    vmov r2, r3, d10
    cmp r2, #0
    bne .Lfail
    movw r1, #0
    movt r1, #16392
    cmp r3, r1
    bne .Lfail
    vmov r2, r3, d11
    cmp r2, #0
    bne .Lfail
    movw r1, #0
    movt r1, #16400
    cmp r3, r1
    bne .Lfail
    vmov r2, r3, d12
    cmp r2, #0
    bne .Lfail
    movw r1, #0
    movt r1, #16404
    cmp r3, r1
    bne .Lfail
    vmov r2, r3, d13
    cmp r2, #0
    bne .Lfail
    movw r1, #0
    movt r1, #16408
    cmp r3, r1
    bne .Lfail
    vmov r2, r3, d14
    cmp r2, #0
    bne .Lfail
    movw r1, #0
    movt r1, #16412
    cmp r3, r1
    bne .Lfail
    vmov r2, r3, d15
    cmp r2, #0
    bne .Lfail
    movw r1, #0
    movt r1, #16416
    cmp r3, r1
    bne .Lfail
    movs r0, #1
    b .Ldone
.Lfail:
    movs r0, #0
.Ldone:
    ldr r3, [sp]
    vmsr fpscr, r3
    add sp, sp, #120
    vpop {d8-d15}
    pop {r3-r11, pc}
    NESTED_END ArmTestSetjmpRegisters

    NESTED_ENTRY ArmTestPeUnwindRegisters
    push.w {r3-r11, lr}
    .seh_save_regs_w {r3-r11, lr}
    vpush {d8-d15}
    .seh_save_fregs {d8-d15}
    PROLOG_END
    mov r4, #68
    mov r5, #69
    mov r6, #70
    mov r7, #71
    mov r8, #72
    mov r9, #73
    mov r10, #74
    mov r11, #75
    vmov.f64 d8, #1.0
    vmov.f64 d9, #2.0
    vmov.f64 d10, #3.0
    vmov.f64 d11, #4.0
    vmov.f64 d12, #5.0
    vmov.f64 d13, #6.0
    vmov.f64 d14, #7.0
    vmov.f64 d15, #8.0
    mov r1, sp
    adr r2, .Lpe_unwound
    orr r2, r2, #1
    bl ArmTestPeUnwindFrame
.Lpe_unwound:
    .seh_startepilogue
    vpop {d8-d15}
    .seh_save_fregs {d8-d15}
    pop.w {r3-r11, pc}
    .seh_save_regs_w {r3-r11, lr}
    .seh_endepilogue
    NESTED_END
