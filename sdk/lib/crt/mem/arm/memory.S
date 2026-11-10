/*
 * PROJECT:     LiberNT runtime library
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARMv7 aligned scalar memory copying and filling
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */
#include <kxarm.h>

    .syntax unified
    .thumb
    .text
    .p2align 2

    LEAF_ENTRY memcpy
    mov r12, r0
    cmp r2, #0
    beq .Lcopy_done
    cmp r0, r1
    bls .Lcopy_forward
    sub r3, r0, r1
    cmp r3, r2
    bhs .Lcopy_forward
    add r12, r12, r2
    add r1, r1, r2
    eor r3, r12, r1
    tst r3, #3
    bne .Lcopy_back_bytes
.Lcopy_back_align:
    tst r12, #3
    beq .Lcopy_back_words
    ldrb r3, [r1, #-1]!
    strb r3, [r12, #-1]!
    subs r2, r2, #1
    beq .Lcopy_done
    b .Lcopy_back_align
.Lcopy_back_words:
    cmp r2, #4
    blo .Lcopy_back_bytes
    ldr r3, [r1, #-4]!
    str r3, [r12, #-4]!
    subs r2, r2, #4
    b .Lcopy_back_words
.Lcopy_back_bytes:
    cmp r2, #0
    beq .Lcopy_done
.Lcopy_back_byte:
    ldrb r3, [r1, #-1]!
    strb r3, [r12, #-1]!
    subs r2, r2, #1
    bne .Lcopy_back_byte
    b .Lcopy_done
.Lcopy_forward:
    eor r3, r12, r1
    tst r3, #3
    bne .Lcopy_bytes
.Lcopy_align:
    tst r12, #3
    beq .Lcopy_blocks
    ldrb r3, [r1], #1
    strb r3, [r12], #1
    subs r2, r2, #1
    beq .Lcopy_done
    b .Lcopy_align
.Lcopy_blocks:
    cmp r2, #16
    blo .Lcopy_words
    ldr r3, [r1], #4
    str r3, [r12], #4
    ldr r3, [r1], #4
    str r3, [r12], #4
    ldr r3, [r1], #4
    str r3, [r12], #4
    ldr r3, [r1], #4
    str r3, [r12], #4
    subs r2, r2, #16
    b .Lcopy_blocks
.Lcopy_words:
    cmp r2, #4
    blo .Lcopy_bytes
    ldr r3, [r1], #4
    str r3, [r12], #4
    subs r2, r2, #4
    b .Lcopy_words
.Lcopy_bytes:
    cmp r2, #0
    beq .Lcopy_done
.Lcopy_byte:
    ldrb r3, [r1], #1
    strb r3, [r12], #1
    subs r2, r2, #1
    bne .Lcopy_byte
.Lcopy_done:
    bx lr
    LEAF_END memcpy

    LEAF_ENTRY memmove
    b memcpy
    LEAF_END memmove

    LEAF_ENTRY memset
    mov r12, r0
    cmp r2, #0
    beq .Lfill_done
    uxtb r1, r1
    orr r1, r1, r1, lsl #8
    orr r1, r1, r1, lsl #16
.Lfill_align:
    tst r12, #3
    beq .Lfill_blocks
    strb r1, [r12], #1
    subs r2, r2, #1
    beq .Lfill_done
    b .Lfill_align
.Lfill_blocks:
    cmp r2, #16
    blo .Lfill_words
    str r1, [r12], #4
    str r1, [r12], #4
    str r1, [r12], #4
    str r1, [r12], #4
    subs r2, r2, #16
    b .Lfill_blocks
.Lfill_words:
    cmp r2, #4
    blo .Lfill_bytes
    str r1, [r12], #4
    subs r2, r2, #4
    b .Lfill_words
.Lfill_bytes:
    cmp r2, #0
    beq .Lfill_done
.Lfill_byte:
    strb r1, [r12], #1
    subs r2, r2, #1
    bne .Lfill_byte
.Lfill_done:
    bx lr
    LEAF_END memset
