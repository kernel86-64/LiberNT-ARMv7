/*
 * PROJECT:     LiberNT API tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Execute production ARM32 APC and loader entry assembly
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#define LdrInitializeThunk ArmTestApcThunk
#define KiUserApcDispatcher ArmTestUserApcDispatcher
#define LdrpInit ArmTestApcInit
#define NtContinue ArmTestApcContinue
#define RtlRaiseStatus ArmTestApcRaise
#define KiArmUserCallbackDispatcher ArmTestCallbackDispatcher
#define KiArmUserExceptionDispatcher ArmTestCallbackDispatcher
#include "../../../../dll/ntdll/dispatch/arm/stubs_asm.s"

    TEXTAREA
    LEAF_ENTRY ArmTestInvokeApc
    mov sp, r0
    mov r3, r1
    movs r0, #0
    movw r1, #0
    movt r1, #0x1000
    movs r2, #0
    b ArmTestUserApcDispatcher
    LEAF_END
    END
