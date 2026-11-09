/*
 * PROJECT:     LiberNT Kernel
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARM32 first-chance user exception frame preparation
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ntoskrnl/include/ntoskrnl.h>

BOOLEAN
KiArmDispatchUserException(PKTRAP_FRAME Trap, PCONTEXT Context, PEXCEPTION_RECORD Record)
{
    struct
    {
        CONTEXT Context;
        EXCEPTION_RECORD Record;
    } *Frame;
    ULONG Cpsr;
    BOOLEAN Copied = FALSE;

    if (!KeUserExceptionDispatcher || Context->Sp < sizeof(*Frame)) return FALSE;
    Frame = (PVOID)ALIGN_DOWN_BY(Context->Sp - sizeof(*Frame), 8);
    __asm__ __volatile__("mrs %0, cpsr" : "=r"(Cpsr));
    _enable();
    _SEH2_TRY
    {
        ProbeForWrite(Frame, sizeof(*Frame), sizeof(ULONGLONG));
        Frame->Context = *Context;
        Frame->Record = *Record;
        Copied = TRUE;
    }
    _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
    {
        Copied = FALSE;
    }
    _SEH2_END;
    __asm__ __volatile__("msr cpsr_c, %0" :: "r"(Cpsr) : "memory");
    if (!Copied) return FALSE;
    Trap->Sp = (ULONG_PTR)Frame;
    Trap->Pc = (ULONG_PTR)KeUserExceptionDispatcher | 1;
    Trap->Cpsr = (Trap->Cpsr & ~(CPSRM_MASK | CPSR_IT_MASK)) | CPSRM_USER | CPSRC_THUMB;
    return TRUE;
}
