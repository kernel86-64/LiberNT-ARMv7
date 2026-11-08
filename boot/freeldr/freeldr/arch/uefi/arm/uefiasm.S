/*
 * PROJECT:     LiberNT FreeLoader
 * LICENSE:     GPL-2.0-or-later (https://spdx.org/licenses/GPL-2.0-or-later)
 * PURPOSE:     ARMv7 UEFI stack transitions
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

    .syntax unified
    .thumb
    .text

    .globl _changestack
    .thumb_func
_changestack:
    mov r0, sp
    ldr r1, =BasicStack
    ldr r1, [r1]
    bic r1, r1, #7
    mov sp, r1
    b ExecuteLoaderCleanly

    .globl _exituefi
    .thumb_func
_exituefi:
    push {r4, lr}
    mov r4, sp
    ldr r0, =UefiServiceStack
    ldr r0, [r0]
    cbz r0, 1f
    bic r0, r0, #7
    mov sp, r0
1:
    bl UefiExitBootServices
    dsb sy
    isb
    mov sp, r4
    pop {r4, pc}
