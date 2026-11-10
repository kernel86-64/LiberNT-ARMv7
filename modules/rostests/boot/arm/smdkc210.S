/*
 * PROJECT:     LiberNT boot tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Cortex-A9 bare-metal exception entry for SMDKC210 tests
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

    .syntax unified
    .arm
    .section .vectors, "ax"
    .balign 32
    .global SmdkStart
SmdkStart:
    b SmdkReset
    b SmdkUnexpected
    b SmdkUnexpected
    b SmdkPrefetch
    b SmdkDataAbort
    b SmdkUnexpected
    b SmdkIrq
    b SmdkUnexpected

    .text
SmdkReset:
    cpsid if
    mrc p15, 0, r0, c0, c0, 5
    ands r0, r0, #0xff
    bne SmdkPark
    cps #0x17
    ldr sp, =SmdkAbortStack
    cps #0x12
    ldr sp, =SmdkIrqStack
    cps #0x13
    ldr sp, =SmdkStack
    ldr r0, =SmdkBssStart
    ldr r1, =SmdkBssEnd
    mov r2, #0
1:
    cmp r0, r1
    strlo r2, [r0], #4
    blo 1b
    ldr r0, =SmdkStart
    mcr p15, 0, r0, c12, c0, 0
    isb
    bl SmdkMain
SmdkPark:
    wfi
    b SmdkPark

SmdkUnexpected:
    mov r0, #0
    b SmdkFailure

SmdkDataAbort:
    sub lr, lr, #8
    push {r0-r12, lr}
    mov r0, #1
    mrc p15, 0, r1, c5, c0, 0
    mrc p15, 0, r2, c6, c0, 0
    mov r3, sp
    bl SmdkAbort
    pop {r0-r12, lr}
    subs pc, lr, #0

SmdkPrefetch:
    sub lr, lr, #4
    push {r0-r12, lr}
    mov r0, #2
    mrc p15, 0, r1, c5, c0, 1
    mrc p15, 0, r2, c6, c0, 2
    mov r3, sp
    bl SmdkAbort
    pop {r0-r12, lr}
    subs pc, lr, #0

SmdkIrq:
    sub lr, lr, #4
    push {r0-r12, lr}
    bl SmdkInterrupt
    pop {r0-r12, lr}
    subs pc, lr, #0

    .global SmdkTryRead
SmdkTryRead:
    ldr r0, [r0]
    bx lr

    .global SmdkTryWrite
SmdkTryWrite:
    str r1, [r0]
    bx lr

    .global SmdkTryUnprivilegedRead
SmdkTryUnprivilegedRead:
    ldrt r0, [r0]
    bx lr

    .global SmdkTryExecute
SmdkTryExecute:
    push {r4, lr}
    adr r1, SmdkExecuteReturn
    ldr r2, =SmdkResume
    str r1, [r2]
    blx r0
SmdkExecuteReturn:
    pop {r4, pc}
