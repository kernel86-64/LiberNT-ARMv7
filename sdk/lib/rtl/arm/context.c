/*
 * PROJECT:     LiberNT Run-Time Library
 * LICENSE:     LGPL-2.1-or-later (https://spdx.org/licenses/LGPL-2.1-or-later)
 * PURPOSE:     ARM32 context restoration and longjmp support
 * COPYRIGHT:   Adapted from Wine dlls/ntdll/signal_arm.c
 *              Copyright 2002 Marcus Meissner, SuSE Linux AG
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

#include <rtl.h>

typedef struct _ARM_JUMP_BUFFER
{
    ULONG Frame;
    ULONG Registers[8];
    ULONG Sp;
    ULONG Pc;
    ULONG Fpscr;
    ULONGLONG D[8];
} ARM_JUMP_BUFFER;

C_ASSERT(sizeof(ARM_JUMP_BUFFER) == 112);
C_ASSERT(FIELD_OFFSET(ARM_JUMP_BUFFER, D) == 48);

typedef PVOID (CALLBACK *PRTL_CONSOLIDATE_CALLBACK)(PEXCEPTION_RECORD);

DECLSPEC_NORETURN VOID NTAPI
RtlpArmConsolidateCallback(PCONTEXT Context, PRTL_CONSOLIDATE_CALLBACK Callback,
                           PEXCEPTION_RECORD Record);

VOID __cdecl
RtlRestoreContext(PCONTEXT Context, PEXCEPTION_RECORD Record)
{
    NTSTATUS Status;

    if (Record && Record->NumberParameters >= 1)
    {
        if (Record->ExceptionCode == STATUS_LONGJUMP)
        {
            ARM_JUMP_BUFFER *Buffer = (ARM_JUMP_BUFFER *)Record->ExceptionInformation[0];

            RtlCopyMemory(&Context->R4, Buffer->Registers, sizeof(Buffer->Registers));
            RtlCopyMemory(&Context->D[8], Buffer->D, sizeof(Buffer->D));
            Context->Sp = Buffer->Sp;
            Context->Pc = Buffer->Pc;
            Context->Fpscr = Buffer->Fpscr;
        }
        else if (Record->ExceptionCode == STATUS_UNWIND_CONSOLIDATE)
        {
            RtlpArmConsolidateCallback(Context,
                (PRTL_CONSOLIDATE_CALLBACK)Record->ExceptionInformation[0], Record);
        }
    }
    Status = ZwContinue(Context, FALSE);
    RtlRaiseStatus(Status);
}
