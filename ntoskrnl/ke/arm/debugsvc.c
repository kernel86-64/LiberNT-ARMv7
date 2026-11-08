/*
 * PROJECT:     LiberNT Kernel
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARM32 runtime debugger service argument dispatch
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ntoskrnl/include/ntoskrnl.h>

VOID KiArmDebugService(PKTRAP_FRAME Trap, ULONG Argument4)
{
    PKEXCEPTION_FRAME Exception = (PKEXCEPTION_FRAME)Trap - 1;
    KPROCESSOR_MODE Mode = KiGetPreviousMode(Trap);
    BOOLEAN Handled;

    switch (Trap->R0)
    {
        case BREAKPOINT_PRINT:
            Trap->R0 = KdpPrint(Trap->R3, Argument4, (PCHAR)Trap->R1, (USHORT)Trap->R2,
                                Mode, Trap, Exception, &Handled);
            break;
        case BREAKPOINT_PROMPT:
            Trap->R0 = KdpPrompt((PCHAR)Trap->R1, (USHORT)Trap->R2, (PCHAR)Trap->R3,
                                 (USHORT)Argument4, Mode, Trap, Exception);
            break;
        default:
            Trap->R0 = STATUS_NOT_IMPLEMENTED;
            break;
    }
    Trap->Pc += sizeof(USHORT);
}
