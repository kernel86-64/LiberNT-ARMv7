/*
* PROJECT:         ReactOS Kernel
* LICENSE:         GPL - See COPYING in the top level directory
* FILE:            lib/sdk/crt/except/arm/ehandler.c
* PURPOSE:         Low level exception handler functions
* PROGRAMMERS:     Timo Kreuzer (timo.kreuzer@reactos.org)
*/

/* INCLUDES *****************************************************************/

#include <precomp.h>

/*
 * Exception unwinding
 *
 * Copyright 1999, 2005, 2019, 2024 Alexandre Julliard
 * Copyright 2021 Martin Storsjö
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

LONG __cdecl
__C_ExecuteExceptionFilter(PEXCEPTION_POINTERS Pointers, PVOID Frame,
                           PVOID Handler, PBYTE NonVolatileRegisters);

EXCEPTION_DISPOSITION __cdecl __C_specific_handler( EXCEPTION_RECORD *rec, void *frame,
                                                   CONTEXT *context, DISPATCHER_CONTEXT *dispatch )
{
    const SCOPE_TABLE *table = dispatch->HandlerData;
    ULONG_PTR base = dispatch->ImageBase;
    ULONG_PTR pc = dispatch->ControlPc;
    unsigned int i;
    void *handler;

    if (dispatch->ControlPcIsUnwound) pc -= 2;

    if (rec->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND))
    {
        for (i = dispatch->ScopeIndex; i < table->Count; i++)
        {
            if (pc < base + table->ScopeRecord[i].BeginAddress) continue;
            if (pc >= base + table->ScopeRecord[i].EndAddress) continue;
            if (table->ScopeRecord[i].JumpTarget) continue;

            if (rec->ExceptionFlags & EXCEPTION_TARGET_UNWIND &&
                dispatch->TargetPc >= base + table->ScopeRecord[i].BeginAddress &&
                dispatch->TargetPc < base + table->ScopeRecord[i].EndAddress)
            {
                break;
            }
            handler = (void *)(base + table->ScopeRecord[i].HandlerAddress);
            dispatch->ScopeIndex = i + 1;
            __C_ExecuteExceptionFilter( (PEXCEPTION_POINTERS)(ULONG_PTR)TRUE, frame, handler, dispatch->NonVolatileRegisters );
        }
    }
    else
    {
        for (i = dispatch->ScopeIndex; i < table->Count; i++)
        {
            if (pc < base + table->ScopeRecord[i].BeginAddress) continue;
            if (pc >= base + table->ScopeRecord[i].EndAddress) continue;
            if (!table->ScopeRecord[i].JumpTarget) continue;

            if (table->ScopeRecord[i].HandlerAddress != EXCEPTION_EXECUTE_HANDLER)
            {
                EXCEPTION_POINTERS ptrs = { rec, context };

                handler = (void *)(base + table->ScopeRecord[i].HandlerAddress);
                switch (__C_ExecuteExceptionFilter( &ptrs, frame, handler, dispatch->NonVolatileRegisters ))
                {
                case EXCEPTION_EXECUTE_HANDLER:
                    break;
                case EXCEPTION_CONTINUE_SEARCH:
                    continue;
                case EXCEPTION_CONTINUE_EXECUTION:
                    return ExceptionContinueExecution;
                }
            }
            RtlUnwindEx( frame, (char *)base + table->ScopeRecord[i].JumpTarget,
                         rec, ULongToPtr(rec->ExceptionCode), dispatch->ContextRecord,
                         dispatch->HistoryTable );
        }
    }
    return ExceptionContinueSearch;
}

DWORD
__CxxFrameHandler3(
    PEXCEPTION_RECORD rec,
    EXCEPTION_REGISTRATION_RECORD* ExceptionRegistrationFrame,
    PCONTEXT context,
    EXCEPTION_REGISTRATION_RECORD** _ExceptionRecord)
{
    UNIMPLEMENTED;
    __debugbreak();
    return 0;
}

