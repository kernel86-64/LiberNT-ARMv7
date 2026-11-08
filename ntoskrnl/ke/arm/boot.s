/*
 * PROJECT:         ReactOS Kernel
 * LICENSE:         BSD - See COPYING.ARM in the top level directory
 * FILE:            ntoskrnl/ke/arm/boot.s
 * PURPOSE:         Implements the kernel entry point for ARM machines
 * PROGRAMMERS:     ReactOS Portable Systems Group
 */

#include <ksarm.h>

    TEXTAREA
    IMPORT KiInitializeSystem
    IMPORT KiArmInstallVectors

    NESTED_ENTRY KiSystemStartup
    PROLOG_END KiSystemStartup
    cpsid if
    mov r4, r0
    ldr r1, [r4, #ArmLpbPrcb]
    sub r1, r1, #PcPrcb
    mcr p15, 0, r1, c13, c0, 4
    isb
    ldr r2, [r4, #LpbArmPanicStack]
    cps #CPSRM_FIQ
    mov sp, r2
    sub r2, r2, #KERNEL_STACK_SIZE
    cps #CPSRM_ABT
    mov sp, r2
    sub r2, r2, #KERNEL_STACK_SIZE
    cps #CPSRM_UDF
    mov sp, r2
    cps #CPSRM_INT
    ldr sp, [r4, #LpbArmInterruptStack]
    cps #CPSRM_SVC
    ldr sp, [r4, #ArmLpbKernelStack]
    sub sp, sp, #(KSWITCH_FRAME_LENGTH + KEXCEPTION_FRAME_LENGTH + VFP_STATE_LENGTH)
    mrc p15, 0, r1, c1, c0, 2
    orr r1, r1, #0x00F00000
    mcr p15, 0, r1, c1, c0, 2
    isb
    mov r1, #0x40000000
    vmsr fpexc, r1
    bl KiArmInstallVectors
    mov r0, r4
    b KiInitializeSystem
    NESTED_END KiSystemStartup
    END
