
#include <ksarm.h>

    TEXTAREA

    LEAF_ENTRY LdrInitializeThunk
    mov r0, r3
    b LdrpInit
    LEAF_END LdrInitializeThunk

    LEAF_ENTRY KiRaiseUserExceptionDispatcher
    __assertfail
    bx lr
    LEAF_END KiRaiseUserExceptionDispatcher

    NESTED_ENTRY KiUserApcDispatcher
    nop.w
    .seh_custom 0xee, 0x02
    PROLOG_END
    mov r12, r3
    mov r3, sp
    blx r12
    mov r0, sp
    movs r1, #1
    bl NtContinue
    bl RtlRaiseStatus
    NESTED_END KiUserApcDispatcher

    NESTED_ENTRY KiUserCallbackDispatcher
    nop.w
    .seh_custom 0xee, 0x01
    nop
    .seh_save_regs {lr}
    nop
    .seh_stackalloc 12
    PROLOG_END
    ldr r0, [sp, #CkApiNumber]
    ldr r1, [sp, #CkBuffer]
    ldr r2, [sp, #CkLength]
    bl KiArmUserCallbackDispatcher
    NESTED_END KiUserCallbackDispatcher

    NESTED_ENTRY KiUserExceptionDispatcher
    nop.w
    .seh_custom 0xee, 0x02
    PROLOG_END
    add.w r0, sp, #CONTEXT_FRAME_LENGTH
    mov r1, sp
    bl KiArmUserExceptionDispatcher
    NESTED_END KiUserExceptionDispatcher

    END
