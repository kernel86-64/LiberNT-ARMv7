/*
 * PROJECT:         ReactOS Kernel
 * LICENSE:         BSD - See COPYING.ARM in the top level directory
 * FILE:            ntoskrnl/ke/arm/trap.s
 * PURPOSE:         Support for exceptions and interrupts on ARM machines
 * PROGRAMMERS:     ReactOS Portable Systems Group
 */

#include <ksarm.h>

#define ARM_TRAP_EX_OFFSET CONTEXT_FRAME_LENGTH
#define ARM_TRAP_TR_OFFSET (ARM_TRAP_EX_OFFSET + KEXCEPTION_FRAME_LENGTH)
#define ARM_TRAP_VFP_OFFSET (ARM_TRAP_TR_OFFSET + KTRAP_FRAME_LENGTH)
#define ARM_TRAP_STORAGE_LENGTH (ARM_TRAP_VFP_OFFSET + VFP_STATE_LENGTH)

    TEXTAREA
    .p2align 5
    .global KiArmVectorTable
KiArmVectorTable:
    ldr.w pc, [pc, #28]
    ldr.w pc, [pc, #28]
    ldr.w pc, [pc, #28]
    ldr.w pc, [pc, #28]
    ldr.w pc, [pc, #28]
    ldr.w pc, [pc, #28]
    ldr.w pc, [pc, #28]
    ldr.w pc, [pc, #28]
    .word KiArmResetException
    .word KiUndefinedInstructionException
    .word KiSoftwareInterruptException
    .word KiPrefetchAbortException
    .word KiDataAbortException
    .word KiArmReservedException
    .word KiInterruptException
    .word KiFastInterruptException

    LEAF_ENTRY KiArmInstallVectors
    dsb sy
    ldr r0, =KiArmVectorTable
    mcr p15, 0, r0, c12, c0, 0
    mrc p15, 0, r0, c1, c0, 0
    bic r0, r0, #0x2000
    orr r0, r0, #0x40000000
    mcr p15, 0, r0, c1, c0, 0
    isb
    bx lr
    LEAF_END

    .global KiUndefinedInstructionException
    .thumb_func
KiUndefinedInstructionException:
    srsdb sp!, #CPSRM_SVC
    cpsid if
    cps #CPSRM_SVC
    push.w {r0-r12, lr}
    mov r4, sp
    sub.w r5, sp, #ARM_TRAP_STORAGE_LENGTH
    bic r5, r5, #7
    mov sp, r5
    add.w r0, r5, #(ARM_TRAP_VFP_OFFSET + VsVfpD)
    vstmia r0!, {d0-d15}
    vstmia r0, {d16-d31}
    vmrs r0, fpscr
    str.w r0, [r5, #(ARM_TRAP_VFP_OFFSET + VsFpscr)]
    cps #CPSRM_SYS
    mov r7, sp
    mov r8, lr
    cps #CPSRM_SVC
    push.w {r8, r9}
    mov r0, r4
    mov r1, r5
    movs r2, #1
    mov r3, r7
    bl KiArmBuildExceptionFrames
    mov sp, r5

    .seh_proc KiUndefinedInstructionExceptionContext
KiUndefinedInstructionExceptionContext:
    nop.w
    .seh_custom 0xee, 0x02
    .seh_endprologue
    add.w r0, sp, #ARM_TRAP_TR_OFFSET
    movs r1, #0
    bl KiUndefinedExceptionHandler
    add.w r0, sp, #ARM_TRAP_TR_OFFSET
    add.w r1, sp, #ARM_TRAP_EX_OFFSET
    b KiExceptionExit
    .seh_endproc

    .global KiSoftwareInterruptException
    .thumb_func
KiSoftwareInterruptException:
    srsdb sp!, #CPSRM_SVC
    cpsid if
    cps #CPSRM_SVC
    push.w {r0-r12, lr}
    mov r4, sp
    sub.w r5, sp, #ARM_TRAP_STORAGE_LENGTH
    bic r5, r5, #7
    mov sp, r5
    add.w r0, r5, #(ARM_TRAP_VFP_OFFSET + VsVfpD)
    vstmia r0!, {d0-d15}
    vstmia r0, {d16-d31}
    vmrs r0, fpscr
    str.w r0, [r5, #(ARM_TRAP_VFP_OFFSET + VsFpscr)]
    cps #CPSRM_SYS
    mov r7, sp
    mov r8, lr
    cps #CPSRM_SVC
    push.w {r8, r9}
    mov r0, r4
    mov r1, r5
    movs r2, #2
    mov r3, r7
    bl KiArmBuildExceptionFrames
    mov sp, r5

    .seh_proc KiSoftwareInterruptExceptionContext
KiSoftwareInterruptExceptionContext:
    nop.w
    .seh_custom 0xee, 0x02
    .seh_endprologue
    add.w r0, sp, #ARM_TRAP_TR_OFFSET
    movs r1, #0
    bl KiSoftwareInterruptHandler
    add.w r0, sp, #ARM_TRAP_TR_OFFSET
    add.w r1, sp, #ARM_TRAP_EX_OFFSET
    b KiExceptionExit
    .seh_endproc

    .global KiPrefetchAbortException
    .thumb_func
KiPrefetchAbortException:
    srsdb sp!, #CPSRM_SVC
    cpsid if
    cps #CPSRM_SVC
    push.w {r0-r12, lr}
    mov r4, sp
    sub.w r5, sp, #ARM_TRAP_STORAGE_LENGTH
    bic r5, r5, #7
    mov sp, r5
    add.w r0, r5, #(ARM_TRAP_VFP_OFFSET + VsVfpD)
    vstmia r0!, {d0-d15}
    vstmia r0, {d16-d31}
    vmrs r0, fpscr
    str.w r0, [r5, #(ARM_TRAP_VFP_OFFSET + VsFpscr)]
    cps #CPSRM_SYS
    mov r7, sp
    mov r8, lr
    cps #CPSRM_SVC
    push.w {r8, r9}
    mov r0, r4
    mov r1, r5
    movs r2, #3
    mov r3, r7
    bl KiArmBuildExceptionFrames
    mov sp, r5

    .seh_proc KiPrefetchAbortExceptionContext
KiPrefetchAbortExceptionContext:
    nop.w
    .seh_custom 0xee, 0x02
    .seh_endprologue
    add.w r0, sp, #ARM_TRAP_TR_OFFSET
    movs r1, #0
    bl KiPrefetchAbortHandler
    add.w r0, sp, #ARM_TRAP_TR_OFFSET
    add.w r1, sp, #ARM_TRAP_EX_OFFSET
    b KiExceptionExit
    .seh_endproc

    .global KiDataAbortException
    .thumb_func
KiDataAbortException:
    srsdb sp!, #CPSRM_SVC
    cpsid if
    cps #CPSRM_SVC
    push.w {r0-r12, lr}
    mov r4, sp
    sub.w r5, sp, #ARM_TRAP_STORAGE_LENGTH
    bic r5, r5, #7
    mov sp, r5
    add.w r0, r5, #(ARM_TRAP_VFP_OFFSET + VsVfpD)
    vstmia r0!, {d0-d15}
    vstmia r0, {d16-d31}
    vmrs r0, fpscr
    str.w r0, [r5, #(ARM_TRAP_VFP_OFFSET + VsFpscr)]
    cps #CPSRM_SYS
    mov r7, sp
    mov r8, lr
    cps #CPSRM_SVC
    push.w {r8, r9}
    mov r0, r4
    mov r1, r5
    movs r2, #4
    mov r3, r7
    bl KiArmBuildExceptionFrames
    mov sp, r5

    .seh_proc KiDataAbortExceptionContext
KiDataAbortExceptionContext:
    nop.w
    .seh_custom 0xee, 0x02
    .seh_endprologue
    add.w r0, sp, #ARM_TRAP_TR_OFFSET
    movs r1, #0
    bl KiDataAbortHandler
    add.w r0, sp, #ARM_TRAP_TR_OFFSET
    add.w r1, sp, #ARM_TRAP_EX_OFFSET
    b KiExceptionExit
    .seh_endproc

    .global KiInterruptException
    .thumb_func
KiInterruptException:
    srsdb sp!, #CPSRM_SVC
    cpsid if
    cps #CPSRM_SVC
    push.w {r0-r12, lr}
    mov r4, sp
    sub.w r5, sp, #ARM_TRAP_STORAGE_LENGTH
    bic r5, r5, #7
    mov sp, r5
    add.w r0, r5, #(ARM_TRAP_VFP_OFFSET + VsVfpD)
    vstmia r0!, {d0-d15}
    vstmia r0, {d16-d31}
    vmrs r0, fpscr
    str.w r0, [r5, #(ARM_TRAP_VFP_OFFSET + VsFpscr)]
    cps #CPSRM_SYS
    mov r7, sp
    mov r8, lr
    cps #CPSRM_SVC
    push.w {r8, r9}
    mov r0, r4
    mov r1, r5
    movs r2, #6
    mov r3, r7
    bl KiArmBuildExceptionFrames
    mov sp, r5

    .seh_proc KiInterruptExceptionContext
KiInterruptExceptionContext:
    nop.w
    .seh_custom 0xee, 0x02
    .seh_endprologue
    add.w r0, sp, #ARM_TRAP_TR_OFFSET
    movs r1, #0
    bl KiInterruptHandler
    add.w r0, sp, #ARM_TRAP_TR_OFFSET
    add.w r1, sp, #ARM_TRAP_EX_OFFSET
    b KiExceptionExit
    .seh_endproc

    .global KiFastInterruptException
    .thumb_func
KiFastInterruptException:
    srsdb sp!, #CPSRM_SVC
    cpsid if
    cps #CPSRM_SVC
    push.w {r0-r12, lr}
    mov r4, sp
    sub.w r5, sp, #ARM_TRAP_STORAGE_LENGTH
    bic r5, r5, #7
    mov sp, r5
    add.w r0, r5, #(ARM_TRAP_VFP_OFFSET + VsVfpD)
    vstmia r0!, {d0-d15}
    vstmia r0, {d16-d31}
    vmrs r0, fpscr
    str.w r0, [r5, #(ARM_TRAP_VFP_OFFSET + VsFpscr)]
    cps #CPSRM_SYS
    mov r7, sp
    mov r8, lr
    cps #CPSRM_SVC
    push.w {r8, r9}
    mov r0, r4
    mov r1, r5
    movs r2, #7
    mov r3, r7
    bl KiArmBuildExceptionFrames
    mov sp, r5

    .seh_proc KiFastInterruptExceptionContext
KiFastInterruptExceptionContext:
    nop.w
    .seh_custom 0xee, 0x02
    .seh_endprologue
    add.w r0, sp, #ARM_TRAP_TR_OFFSET
    movs r1, #0
    bl KiInterruptHandler
    add.w r0, sp, #ARM_TRAP_TR_OFFSET
    add.w r1, sp, #ARM_TRAP_EX_OFFSET
    b KiExceptionExit
    .seh_endproc

    .global KiArmResetException
    .thumb_func
KiArmResetException:
    srsdb sp!, #CPSRM_SVC
    cpsid if
    cps #CPSRM_SVC
    push.w {r0-r12, lr}
    mov r4, sp
    sub.w r5, sp, #ARM_TRAP_STORAGE_LENGTH
    bic r5, r5, #7
    mov sp, r5
    add.w r0, r5, #(ARM_TRAP_VFP_OFFSET + VsVfpD)
    vstmia r0!, {d0-d15}
    vstmia r0, {d16-d31}
    vmrs r0, fpscr
    str.w r0, [r5, #(ARM_TRAP_VFP_OFFSET + VsFpscr)]
    cps #CPSRM_SYS
    mov r7, sp
    mov r8, lr
    cps #CPSRM_SVC
    push.w {r8, r9}
    mov r0, r4
    mov r1, r5
    movs r2, #0
    mov r3, r7
    bl KiArmBuildExceptionFrames
    mov sp, r5

    .seh_proc KiArmResetExceptionContext
KiArmResetExceptionContext:
    nop.w
    .seh_custom 0xee, 0x02
    .seh_endprologue
    add.w r0, sp, #ARM_TRAP_TR_OFFSET
    movs r1, #0
    bl KiUndefinedExceptionHandler
    add.w r0, sp, #ARM_TRAP_TR_OFFSET
    add.w r1, sp, #ARM_TRAP_EX_OFFSET
    b KiExceptionExit
    .seh_endproc

    .global KiArmReservedException
    .thumb_func
KiArmReservedException:
    srsdb sp!, #CPSRM_SVC
    cpsid if
    cps #CPSRM_SVC
    push.w {r0-r12, lr}
    mov r4, sp
    sub.w r5, sp, #ARM_TRAP_STORAGE_LENGTH
    bic r5, r5, #7
    mov sp, r5
    add.w r0, r5, #(ARM_TRAP_VFP_OFFSET + VsVfpD)
    vstmia r0!, {d0-d15}
    vstmia r0, {d16-d31}
    vmrs r0, fpscr
    str.w r0, [r5, #(ARM_TRAP_VFP_OFFSET + VsFpscr)]
    cps #CPSRM_SYS
    mov r7, sp
    mov r8, lr
    cps #CPSRM_SVC
    push.w {r8, r9}
    mov r0, r4
    mov r1, r5
    movs r2, #5
    mov r3, r7
    bl KiArmBuildExceptionFrames
    mov sp, r5

    .seh_proc KiArmReservedExceptionContext
KiArmReservedExceptionContext:
    nop.w
    .seh_custom 0xee, 0x02
    .seh_endprologue
    add.w r0, sp, #ARM_TRAP_TR_OFFSET
    movs r1, #0
    bl KiUndefinedExceptionHandler
    add.w r0, sp, #ARM_TRAP_TR_OFFSET
    add.w r1, sp, #ARM_TRAP_EX_OFFSET
    b KiExceptionExit
    .seh_endproc

    NESTED_ENTRY KiExceptionExit
    PROLOG_END
    ALTERNATE_ENTRY KiServiceExit
    cpsid if
    mov r4, r0
    mov r5, r1
    ldr r0, [r4, #TrCpsr]
    and r0, r0, #CPSRM_MASK
    cmp r0, #CPSRM_SVC
    beq .Lsvc_stack
    ldr r2, [r4, #TrSp]
    ldr r3, [r4, #TrLr]
    cps #CPSRM_SYS
    mov sp, r2
    mov lr, r3
    cps #CPSRM_SVC
    mrc p15, 0, r0, c13, c0, 4
    bic r0, r0, #0xff
    bic r0, r0, #0xf00
    ldr r0, [r0, #PcCurrentThread]
    ldr r0, [r0, #ThInitialStack]
    b .Lreturn_stack
.Lsvc_stack:
    ldr r0, [r4, #TrSp]
.Lreturn_stack:
    sub r6, r0, #64
    sub sp, sp, #16
    ldr r0, [r4, #TrR12]
    ldr r1, [r4, #TrLr]
    ldr r2, [r4, #TrPc]
    ldr r3, [r4, #TrCpsr]
    stmia sp, {r0-r3}
    add r0, r4, #TrR0
    mov r1, r6
    movs r2, #4
.Lcopy_volatile:
    ldr r3, [r0], #4
    str r3, [r1], #4
    subs r2, r2, #1
    bne .Lcopy_volatile
    add r0, r5, #ExR4
    movs r2, #8
.Lcopy_nonvolatile:
    ldr r3, [r0], #4
    str r3, [r1], #4
    subs r2, r2, #1
    bne .Lcopy_nonvolatile
    ldmia sp, {r0-r3}
    str r0, [r6, #48]
    str r1, [r6, #52]
    str r2, [r6, #56]
    str r3, [r6, #60]
    ldr r0, [r4, #TrVfpState]
    cbz r0, .Lrestore_integer
    ldr r1, [r0, #VsFpscr]
    vmsr fpscr, r1
    add r0, r0, #VsVfpD
    vldmia r0!, {d0-d15}
    vldmia r0, {d16-d31}
.Lrestore_integer:
    mov sp, r6
    pop.w {r0-r12, lr}
    rfeia sp!
    NESTED_END

    .p2align 2
    .global KiInterruptTemplate
KiInterruptTemplate:
    ldr.w pc, [pc, #4]
    nop.w
    .word KiUnexpectedInterrupt
    .word 0

    END
