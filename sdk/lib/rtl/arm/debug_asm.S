/*
 * PROJECT:         ReactOS Kernel
 * LICENSE:         BSD - See COPYING.ARM in the top level directory
 * FILE:            lib/rtl/arm/debug_asm.S
 * PURPOSE:         Cross-privilege Debugging and Exception Support for ARM
 * PROGRAMMERS:     ReactOS Portable Systems Group
 */

#include <ksarm.h>

    TEXTAREA

    LEAF_ENTRY DbgBreakPoint
    __debugbreak
    bx lr
    LEAF_END DbgBreakPoint

    LEAF_ENTRY DbgUserBreakPoint
    __debugbreak
    bx lr
    LEAF_END DbgUserBreakPoint

    LEAF_ENTRY DbgBreakPointWithStatus
    ALTERNATE_ENTRY RtlpBreakWithStatusInstruction
    __debugbreak
    bx lr
    LEAF_END

    LEAF_ENTRY DebugService
    __debugservice
    //bkpt BREAKPOINT_PRINT // Could be prompt too, we check this later
    bx lr
    LEAF_END DebugService

    LEAF_ENTRY DebugService2
    //bkpt BREAKPOINT_LOAD_SYMBOLS // Could be unload too, we check this later
    bx lr
    LEAF_END DebugService2

    LEAF_ENTRY RtlCaptureContext
    str r1, [r0, #CxR1]
    add r1, r0, #CxR2
    stmia r1, {r2-r12}
    str sp, [r0, #CxSp]
    str lr, [r0, #CxPc]
    mrs r1, cpsr
    bfi r1, lr, #5, #1
    str r1, [r0, #CxCpsr]
    vmrs r1, fpscr
    str r1, [r0, #CxFpscr]
    add r1, r0, #CxD
    vstmia r1!, {d0-d15}
    vstmia r1, {d16-d31}
    movs r1, #0
    str r1, [r0, #CxR0]
    str r1, [r0, #CxLr]
    mov32 r1, #CONTEXT_FULL
    str r1, [r0, #CxContextFlags]
    bx lr
    LEAF_END RtlCaptureContext

    END
/* EOF */
