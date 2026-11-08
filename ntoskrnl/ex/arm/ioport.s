/*
* PROJECT:         ReactOS Kernel
* LICENSE:         GPL - See COPYING in the top level directory
* FILE:            ntoskrnl/ex/arm/ioport.s
* PURPOSE:         Low level port communication functions for ARM
* PROGRAMMERS:     Timo Kreuzer (timo.kreuzer@reactos.org)
*/

#include <ksarm.h>

    TEXTAREA

    LEAF_ENTRY READ_REGISTER_UCHAR
    dmb sy
    ldrb r0, [r0]
    dmb sy
    bx lr
    LEAF_END READ_REGISTER_UCHAR

    LEAF_ENTRY WRITE_REGISTER_UCHAR
    dmb sy
    strb r1, [r0]
    dmb sy
    bx lr
    LEAF_END WRITE_REGISTER_UCHAR

    LEAF_ENTRY READ_REGISTER_BUFFER_UCHAR
    dmb sy
    cmp r2, #0
    beq .LREAD_REGISTER_BUFFER_UCHAR_done
.LREAD_REGISTER_BUFFER_UCHAR_loop:
    ldrb r3, [r0], #1
    strb r3, [r1], #1
    subs r2, r2, #1
    bne .LREAD_REGISTER_BUFFER_UCHAR_loop
.LREAD_REGISTER_BUFFER_UCHAR_done:
    dmb sy
    bx lr
    LEAF_END READ_REGISTER_BUFFER_UCHAR

    LEAF_ENTRY WRITE_REGISTER_BUFFER_UCHAR
    dmb sy
    cmp r2, #0
    beq .LWRITE_REGISTER_BUFFER_UCHAR_done
.LWRITE_REGISTER_BUFFER_UCHAR_loop:
    ldrb r3, [r1], #1
    strb r3, [r0], #1
    subs r2, r2, #1
    bne .LWRITE_REGISTER_BUFFER_UCHAR_loop
.LWRITE_REGISTER_BUFFER_UCHAR_done:
    dmb sy
    bx lr
    LEAF_END WRITE_REGISTER_BUFFER_UCHAR

    LEAF_ENTRY READ_REGISTER_USHORT
    dmb sy
    ldrh r0, [r0]
    dmb sy
    bx lr
    LEAF_END READ_REGISTER_USHORT

    LEAF_ENTRY WRITE_REGISTER_USHORT
    dmb sy
    strh r1, [r0]
    dmb sy
    bx lr
    LEAF_END WRITE_REGISTER_USHORT

    LEAF_ENTRY READ_REGISTER_BUFFER_USHORT
    dmb sy
    cmp r2, #0
    beq .LREAD_REGISTER_BUFFER_USHORT_done
.LREAD_REGISTER_BUFFER_USHORT_loop:
    ldrh r3, [r0], #2
    strh r3, [r1], #2
    subs r2, r2, #1
    bne .LREAD_REGISTER_BUFFER_USHORT_loop
.LREAD_REGISTER_BUFFER_USHORT_done:
    dmb sy
    bx lr
    LEAF_END READ_REGISTER_BUFFER_USHORT

    LEAF_ENTRY WRITE_REGISTER_BUFFER_USHORT
    dmb sy
    cmp r2, #0
    beq .LWRITE_REGISTER_BUFFER_USHORT_done
.LWRITE_REGISTER_BUFFER_USHORT_loop:
    ldrh r3, [r1], #2
    strh r3, [r0], #2
    subs r2, r2, #1
    bne .LWRITE_REGISTER_BUFFER_USHORT_loop
.LWRITE_REGISTER_BUFFER_USHORT_done:
    dmb sy
    bx lr
    LEAF_END WRITE_REGISTER_BUFFER_USHORT

    LEAF_ENTRY READ_REGISTER_ULONG
    dmb sy
    ldr r0, [r0]
    dmb sy
    bx lr
    LEAF_END READ_REGISTER_ULONG

    LEAF_ENTRY WRITE_REGISTER_ULONG
    dmb sy
    str r1, [r0]
    dmb sy
    bx lr
    LEAF_END WRITE_REGISTER_ULONG

    LEAF_ENTRY READ_REGISTER_BUFFER_ULONG
    dmb sy
    cmp r2, #0
    beq .LREAD_REGISTER_BUFFER_ULONG_done
.LREAD_REGISTER_BUFFER_ULONG_loop:
    ldr r3, [r0], #4
    str r3, [r1], #4
    subs r2, r2, #1
    bne .LREAD_REGISTER_BUFFER_ULONG_loop
.LREAD_REGISTER_BUFFER_ULONG_done:
    dmb sy
    bx lr
    LEAF_END READ_REGISTER_BUFFER_ULONG

    LEAF_ENTRY WRITE_REGISTER_BUFFER_ULONG
    dmb sy
    cmp r2, #0
    beq .LWRITE_REGISTER_BUFFER_ULONG_done
.LWRITE_REGISTER_BUFFER_ULONG_loop:
    ldr r3, [r1], #4
    str r3, [r0], #4
    subs r2, r2, #1
    bne .LWRITE_REGISTER_BUFFER_ULONG_loop
.LWRITE_REGISTER_BUFFER_ULONG_done:
    dmb sy
    bx lr
    LEAF_END WRITE_REGISTER_BUFFER_ULONG

    END
