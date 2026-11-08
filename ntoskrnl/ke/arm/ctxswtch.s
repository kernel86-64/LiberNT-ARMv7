/*
 * PROJECT:         ReactOS Kernel
 * LICENSE:         BSD - See COPYING.ARM in the top level directory
 * FILE:            ntoskrnl/ke/arm/ctxswtch.s
 * PURPOSE:         Context Switch and Idle Thread on ARM
 * PROGRAMMERS:     Timo Kreuzer (timo.kreuzer@reactos.org)
 *                  ReactOS Portable Systems Group
 */

#include <ksarm.h>

    TEXTAREA

    NESTED_ENTRY KiSwapContext
    cpsid i
    .seh_nop
    vpush {d16-d31}
    .seh_save_fregs {d16-d31}
    vpush {d0-d15}
    .seh_save_fregs {d0-d15}
    sub sp, sp, #VsVfpD
    .seh_stackalloc VsVfpD
    vmrs r3, fpscr
    .seh_nop_w
    str r3, [sp, #VsFpscr]
    .seh_nop
    push.w {r4-r11, lr}
    .seh_save_regs_w {r4-r11, lr}
    sub sp, sp, #ExR4
    .seh_stackalloc ExR4
    sub sp, sp, #KSWITCH_FRAME_LENGTH
    .seh_stackalloc KSWITCH_FRAME_LENGTH
    strb r0, [sp, #SwApcBypass]
    .seh_nop_w
    str.w r11, [sp, #SwR11]
    .seh_nop_w
    adr.w r3, .Lswitch_restore
    .seh_nop_w
    orr r3, r3, #1
    .seh_nop_w
    str r3, [sp, #SwReturn]
    .seh_nop
    PROLOG_END
    mov r4, r1
    mov r3, sp
    str r3, [r1, #ThKernelStack]
    mrc p15, 0, r2, c13, c0, 4
    bic r2, r2, #0xff
    bic r2, r2, #0xf00
    ldr r2, [r2, #PcCurrentThread]
    ldr r3, [r2, #ThKernelStack]
    mov sp, r3
    ldrb r0, [sp, #SwApcBypass]
    mov r1, r4
    bl KiArmSwapContextResume
    ldr r3, [sp, #SwReturn]
    bx r3
.Lswitch_restore:
    .seh_startepilogue
    add sp, sp, #KSWITCH_FRAME_LENGTH
    .seh_stackalloc KSWITCH_FRAME_LENGTH
    add sp, sp, #ExR4
    .seh_stackalloc ExR4
    pop.w {r4-r11, lr}
    .seh_save_regs_w {r4-r11, lr}
    ldr r3, [sp, #VsFpscr]
    .seh_nop
    vmsr fpscr, r3
    .seh_nop_w
    add sp, sp, #VsVfpD
    .seh_stackalloc VsVfpD
    vpop {d0-d15}
    .seh_save_fregs {d0-d15}
    vpop {d16-d31}
    .seh_save_fregs {d16-d31}
    cpsie i
    .seh_nop
    bx lr
    .seh_nop
    .seh_endepilogue
    NESTED_END

    .global KiThreadStartup
    .thumb_func
KiThreadStartup:
    add sp, sp, #KSWITCH_FRAME_LENGTH
    add sp, sp, #ExR4
    pop.w {r4-r11, lr}
    ldr r3, [sp, #VsFpscr]
    vmsr fpscr, r3
    add sp, sp, #VsVfpD
    vpop {d0-d15}
    vpop {d16-d31}
    cpsie i
    .seh_proc KiThreadStartupContext
KiThreadStartupContext:
    push {r4, lr}
    .seh_save_regs {r4, lr}
    .seh_endprologue
    movs r0, #APC_LEVEL
    bl KfLowerIrql
    mov r0, r6
    mov r1, r5
    blx r7
    cbz r4, .Lsystem_thread_returned
    add sp, sp, #8
    add.w r0, sp, #(VFP_STATE_LENGTH + KEXCEPTION_FRAME_LENGTH)
    add.w r1, sp, #VFP_STATE_LENGTH
    b KiExceptionExit
.Lsystem_thread_returned:
    movs r0, #NO_USER_MODE_CONTEXT
    bl KeBugCheck
    NESTED_END

    END
