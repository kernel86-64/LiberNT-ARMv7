/*
 * PROJECT:     LiberNT Run-Time Library
 * LICENSE:     LGPL-2.1-or-later (https://spdx.org/licenses/LGPL-2.1-or-later)
 * PURPOSE:     ARM32 stack walking using PE unwind metadata
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <rtl.h>

ULONG NTAPI
RtlWalkFrameChain(PVOID *Callers, ULONG Count, ULONG Flags)
{
    CONTEXT Context;
    PRUNTIME_FUNCTION Function;
    ULONG_PTR Low, High, ImageBase, Frame, Pc, PreviousSp;
    PVOID Data;
    ULONG Index, Skip = Flags >> 8;
    volatile ULONG Captured = 0;

    if (!Callers || Count <= Skip)
        return 0;

    RtlCaptureContext(&Context);
    RtlpGetStackLimits(&Low, &High);
    _SEH2_TRY
    {
        for (Index = 0; Index < Count; ++Index)
        {
            if (Context.Sp < Low || Context.Sp >= High || (Context.Sp & 7))
                break;
            Pc = Context.Pc;
            if (Context.ContextFlags & CONTEXT_UNWOUND_TO_CALL) Pc -= 2;
            Function = RtlLookupFunctionEntry(Pc, &ImageBase, NULL);
            PreviousSp = Context.Sp;
            RtlVirtualUnwind(UNW_FLAG_NHANDLER, ImageBase, Pc, Function,
                            &Context, &Data, &Frame, NULL);
            if (!Context.Pc || Context.Sp < PreviousSp || Context.Sp > High ||
                (Context.Sp == PreviousSp && Context.Pc == Pc))
                break;
            if (Index >= Skip)
                Callers[Captured++] = (PVOID)Context.Pc;
        }
    }
    _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
    {
    }
    _SEH2_END;
    return Captured;
}
