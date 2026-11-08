
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

    LEAF_ENTRY KiUserCallbackDispatcher
    __assertfail
    bx lr
    LEAF_END KiUserCallbackDispatcher

    LEAF_ENTRY KiUserExceptionDispatcher
    __assertfail
    bx lr
    LEAF_END KiUserExceptionDispatcher

    END
