/*
 * PROJECT:     LiberNT Run-Time Library
 * LICENSE:     LGPL-2.1-or-later (https://spdx.org/licenses/LGPL-2.1-or-later)
 * PURPOSE:     ARM32 protected language handler calls and consolidation frame
 * COPYRIGHT:   Copyright 2002 Marcus Meissner, SuSE Linux AG
 *              Copyright 2010-2013, 2015 André Hentschel
 */

/*
 * ARM signal handling routines
 *
 * Copyright 2002 Marcus Meissner, SuSE Linux AG
 * Copyright 2010-2013, 2015 André Hentschel
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <ksarm.h>

    TEXTAREA

    NESTED_ENTRY RtlpExecuteHandlerForException
    push {r3, lr}
    .seh_save_regs {r3, lr}
    PROLOG_END
    .seh_handler RtlpExceptionHandler, %except
    ldr r12, [sp, #8]
    blx r12
    .seh_startepilogue
    pop {r3, pc}
    .seh_save_regs {r3, lr}
    .seh_endepilogue
    NESTED_END

    NESTED_ENTRY RtlpExecuteHandlerForUnwind
    push {r3, lr}
    .seh_save_regs {r3, lr}
    PROLOG_END
    .seh_handler RtlpUnwindHandler, %except
    ldr r12, [sp, #8]
    blx r12
    .seh_startepilogue
    pop {r3, pc}
    .seh_save_regs {r3, lr}
    .seh_endepilogue
    NESTED_END

    NESTED_ENTRY RtlpArmConsolidateCallback
    push {r0-r2, lr}
    .seh_nop
    sub sp, sp, #CONTEXT_FRAME_LENGTH
    .seh_nop
    mov r1, r0
    .seh_nop
    mov r0, sp
    .seh_nop
    mov r2, #CONTEXT_FRAME_LENGTH
    .seh_nop_w
    bl memcpy
    .seh_custom 0xee, 0x02
    PROLOG_END
    ldrd r1, r2, [sp, #(CONTEXT_FRAME_LENGTH + 4)]
    mov r0, r2
    blx r1
    str r0, [sp, #CxPc]
    mov r0, sp
    movs r1, #0
    b ZwContinue
    NESTED_END

    END
