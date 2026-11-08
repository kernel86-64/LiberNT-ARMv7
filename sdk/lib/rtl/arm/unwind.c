/*
 * PROJECT:     LiberNT Run-Time Library
 * LICENSE:     LGPL-2.1-or-later (https://spdx.org/licenses/LGPL-2.1-or-later)
 * PURPOSE:     ARM32 PE/COFF virtual unwinding, adapted from Wine dlls/ntdll/unwind.c
 * COPYRIGHT:   Copyright 1999, 2005, 2019, 2024 Alexandre Julliard
 *              Copyright 2021 Martin Storsjö
 */

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

#include <windef.h>
#include <winnt.h>

struct unwind_info
{
    DWORD function_length : 18;
    DWORD version : 2;
    DWORD x : 1;
    DWORD e : 1;
    DWORD f : 1;
    DWORD epilog : 5;
    DWORD codes : 4;
};

struct unwind_info_ext
{
    WORD epilog;
    BYTE codes;
    BYTE reserved;
};

struct unwind_info_epilog
{
    DWORD offset : 18;
    DWORD res : 2;
    DWORD cond : 4;
    DWORD index : 8;
};

static const BYTE unwind_code_len[256] =
{
 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
 1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,4,3,4,1,1,1,1,1
};

static const BYTE unwind_instr_len[256] =
{
 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,4,4,4,4,4,4,4,4,
 4,4,4,4,4,4,4,4,4,4,4,4,2,2,0,4,0,0,0,0,0,4,4,2,2,4,4,2,4,2,4,0
};

static unsigned int get_sequence_len( BYTE *ptr, BYTE *end, int include_end )
{
    unsigned int ret = 0;

    while (ptr < end)
    {
        if (*ptr >= 0xfd)
        {
            if (*ptr <= 0xfe && include_end)
                ret += unwind_instr_len[*ptr];
            break;
        }
        ret += unwind_instr_len[*ptr];
        ptr += unwind_code_len[*ptr];
    }
    return ret;
}

static void pop_regs_mask( int mask, CONTEXT *context, KNONVOLATILE_CONTEXT_POINTERS *ptrs )
{
    int i;
    for (i = 0; i <= 12; i++)
    {
        if (!(mask & (1 << i))) continue;
        if (ptrs && i >= 4 && i <= 11) (&ptrs->R4)[i - 4] = (DWORD *)context->Sp;
        if (i >= 4) (&context->R0)[i] = *(DWORD *)context->Sp;
        context->Sp += 4;
    }
}

static void pop_regs_range( int last, CONTEXT *context, KNONVOLATILE_CONTEXT_POINTERS *ptrs )
{
    int i;
    for (i = 4; i <= last; i++)
    {
        if (ptrs) (&ptrs->R4)[i - 4] = (DWORD *)context->Sp;
        (&context->R0)[i] = *(DWORD *)context->Sp;
        context->Sp += 4;
    }
}

static void pop_lr( int increment, CONTEXT *context, KNONVOLATILE_CONTEXT_POINTERS *ptrs )
{
    if (ptrs) ptrs->Lr = (DWORD *)context->Sp;
    context->Lr = *(DWORD *)context->Sp;
    context->Sp += increment;
}

static void pop_fpregs_range( int first, int last, CONTEXT *context, KNONVOLATILE_CONTEXT_POINTERS *ptrs )
{
    int i;
    for (i = first; i <= last; i++)
    {
        if (ptrs && i >= 8 && i <= 15) (&ptrs->D8)[i - 8] = (ULONGLONG *)context->Sp;
        context->D[i] = *(ULONGLONG *)context->Sp;
        context->Sp += 8;
    }
}

static void ms_opcode( BYTE opcode, CONTEXT *context, KNONVOLATILE_CONTEXT_POINTERS *ptrs )
{
    switch (opcode)
    {
    case 1:
        context->Pc = ((DWORD *)context->Sp)[1];
        context->Sp = ((DWORD *)context->Sp)[0];
        context->ContextFlags &= ~CONTEXT_UNWOUND_TO_CALL;
        break;
    case 2:
    {
        int i;
        CONTEXT *src = (CONTEXT *)context->Sp;

        *context = *src;
        if (!ptrs) break;
        for (i = 0; i < 8; i++) (&ptrs->R4)[i] = &src->R4 + i;
        ptrs->Lr = &src->Lr;
        for (i = 0; i < 8; i++) (&ptrs->D8)[i] = &src->D[i + 8];
        break;
    }
    default:
        return;
        break;
    }
}

static void process_unwind_codes( BYTE *ptr, BYTE *end, CONTEXT *context,
                                  KNONVOLATILE_CONTEXT_POINTERS *ptrs, int skip )
{
    unsigned int val, len;
    unsigned int i;


    while (ptr < end && skip)
    {
        if (*ptr >= 0xfd) break;
        skip -= unwind_instr_len[*ptr];
        ptr += unwind_code_len[*ptr];
    }

    while (ptr < end)
    {
        len = unwind_code_len[*ptr];
        if (ptr + len > end) break;
        val = 0;
        for (i = 0; i < len; i++)
            val = (val << 8) | ptr[i];

        if (*ptr <= 0x7f)
            context->Sp += 4 * (val & 0x7f);
        else if (*ptr <= 0xbf)
        {
            pop_regs_mask( val & 0x1fff, context, ptrs );
            if (val & 0x2000)
                pop_lr( 4, context, ptrs );
        }
        else if (*ptr <= 0xcf)
            context->Sp = (&context->R0)[val & 0x0f];
        else if (*ptr <= 0xd7)
        {
            pop_regs_range( (val & 0x03) + 4, context, ptrs );
            if (val & 0x04)
                pop_lr( 4, context, ptrs );
        }
        else if (*ptr <= 0xdf)
        {
            pop_regs_range( (val & 0x03) + 8, context, ptrs );
            if (val & 0x04)
                pop_lr( 4, context, ptrs );
        }
        else if (*ptr <= 0xe7)
            pop_fpregs_range( 8, (val & 0x07) + 8, context, ptrs );
        else if (*ptr <= 0xeb)
            context->Sp += 4 * (val & 0x3ff);
        else if (*ptr <= 0xed)
        {
            pop_regs_mask( val & 0xff, context, ptrs );
            if (val & 0x100)
                pop_lr( 4, context, ptrs );
        }
        else if (*ptr <= 0xee)
            ms_opcode( val & 0xff, context, ptrs );
        else if (*ptr <= 0xef && ((val & 0xff) <= 0x0f))
            pop_lr( 4 * (val & 0x0f), context, ptrs );
        else if (*ptr <= 0xf4)
            return;
        else if (*ptr <= 0xf5)
            pop_fpregs_range( (val & 0xf0) >> 4, (val & 0x0f), context, ptrs );
        else if (*ptr <= 0xf6)
            pop_fpregs_range( ((val & 0xf0) >> 4) + 16, (val & 0x0f) + 16, context, ptrs );
        else if (*ptr == 0xf7 || *ptr == 0xf9)
            context->Sp += 4 * (val & 0xffff);
        else if (*ptr == 0xf8 || *ptr == 0xfa)
            context->Sp += 4 * (val & 0xffffff);
        else if (*ptr <= 0xfc)
             ;
        else
            break;

        ptr += len;
    }
}

static void *unwind_packed_data( ULONG_PTR base, ULONG_PTR pc, RUNTIME_FUNCTION *func,
                                 CONTEXT *context, KNONVOLATILE_CONTEXT_POINTERS *ptrs )
{
    int i, pos = 0;
    int pf = 0, ef = 0, fpoffset = 0, stack = func->StackAdjust;
    int prologue_regmask = 0;
    int epilogue_regmask = 0;
    unsigned int offset, len;
    BYTE prologue[10], *prologue_end, epilogue[20], *epilogue_end;


    offset = (pc - base) - (func->BeginAddress & ~1u);
    if (func->StackAdjust >= 0x03f4)
    {
        pf = func->StackAdjust & 0x04;
        ef = func->StackAdjust & 0x08;
        stack = (func->StackAdjust & 3) + 1;
    }

    if (!func->R || pf)
    {
        int first = 4, last = func->Reg + 4;
        if (pf)
        {
            first = (~func->StackAdjust) & 3;
            if (func->R)
                last = 3;
        }
        for (i = first; i <= last; i++)
            prologue_regmask |= 1 << i;
        fpoffset = last + 1 - first;
    }

    if (!func->R || ef)
    {
        int first = 4, last = func->Reg + 4;
        if (ef)
        {
            first = (~func->StackAdjust) & 3;
            if (func->R)
                last = 3;
        }
        for (i = first; i <= last; i++)
            epilogue_regmask |= 1 << i;
    }

    if (func->C)
    {
        prologue_regmask |= 1 << 11;
        epilogue_regmask |= 1 << 11;
    }

    if (func->L)
    {
        prologue_regmask |= 1 << 14;
        if (func->Ret != 0)
            epilogue_regmask |= 1 << 14;
        else if (!func->H)
            epilogue_regmask |= 1 << 15;
    }


    if (stack && !pf)
    {
        if (stack <= 0x7f)
        {
            prologue[pos++] = stack;
        }
        else
        {
            prologue[pos++] = 0xe8 | (stack >> 8);
            prologue[pos++] = stack & 0xff;
        }
    }

    if (func->R && func->Reg != 7)
        prologue[pos++] = 0xe0 | func->Reg;

    if (func->C && fpoffset == 0)
        prologue[pos++] = 0xfb;
    else if (func->C)
        prologue[pos++] = 0xfc;

    if (prologue_regmask & 0xf00)
    {
        int bitmask = prologue_regmask & 0x1fff;
        if (prologue_regmask & (1 << 14))
            bitmask |= 0x2000;
        prologue[pos++] = 0x80 | (bitmask >> 8);
        prologue[pos++] = bitmask & 0xff;
    }
    else if (prologue_regmask)
    {
        int bitmask = prologue_regmask & 0xff;
        if (prologue_regmask & (1 << 14))
            bitmask |= 0x100;
        prologue[pos++] = 0xec | (bitmask >> 8);
        prologue[pos++] = bitmask & 0xff;
    }

    if (func->H)
        prologue[pos++] = 0x04;

    prologue[pos++] = 0xff;
    prologue_end = &prologue[pos];


    pos = 0;
    if (stack && !ef)
    {
        if (stack <= 0x7f)
        {
            epilogue[pos++] = stack;
        }
        else
        {
            epilogue[pos++] = 0xe8 | (stack >> 8);
            epilogue[pos++] = stack & 0xff;
        }
    }

    if (func->R && func->Reg != 7)
        epilogue[pos++] = 0xe0 | func->Reg;

    if (epilogue_regmask & 0x7f00)
    {
        int bitmask = epilogue_regmask & 0x1fff;
        if (epilogue_regmask & (3 << 14))
            bitmask |= 0x2000;
        epilogue[pos++] = 0x80 | (bitmask >> 8);
        epilogue[pos++] = bitmask & 0xff;
    }
    else if (epilogue_regmask)
    {
        int bitmask = epilogue_regmask & 0xff;
        if (epilogue_regmask & (1 << 15))
            bitmask |= 0x100;
        epilogue[pos++] = 0xec | (bitmask >> 8);
        epilogue[pos++] = bitmask & 0xff;
    }

    if (func->H && !(func->L && func->Ret == 0))
        epilogue[pos++] = 0x04;
    else if (func->H && (func->L && func->Ret == 0))
    {
        epilogue[pos++] = 0xef;
        epilogue[pos++] = 5;
    }

    if (func->Ret == 1)
        epilogue[pos++] = 0xfd;
    else if (func->Ret == 2)
        epilogue[pos++] = 0xfe;
    else
        epilogue[pos++] = 0xff;
    epilogue_end = &epilogue[pos];

    if (func->Flag == 1 && offset < 4 * (prologue_end - prologue)) {

        len = get_sequence_len( prologue, prologue_end, 0 );
        if (offset < len)
        {
            process_unwind_codes( prologue, prologue_end, context, ptrs, len - offset );
            return NULL;
        }
    }

    if (func->Ret != 3 && 2 * func->FunctionLength - offset <= 4 * (epilogue_end - epilogue)) {

        len = get_sequence_len( epilogue, epilogue_end, 1 );
        if (offset >= 2 * func->FunctionLength - len)
        {
            process_unwind_codes( epilogue, epilogue_end, context, ptrs, offset - (2 * func->FunctionLength - len) );
            return NULL;
        }
    }


    process_unwind_codes( prologue, prologue_end, context, ptrs, 0 );

    return NULL;
}

static BOOLEAN condition_holds(unsigned int condition, DWORD cpsr)
{
    BOOLEAN n = (cpsr >> 31) & 1;
    BOOLEAN z = (cpsr >> 30) & 1;
    BOOLEAN c = (cpsr >> 29) & 1;
    BOOLEAN v = (cpsr >> 28) & 1;
    BOOLEAN result;

    switch (condition >> 1)
    {
        case 0: result = z; break;
        case 1: result = c; break;
        case 2: result = n; break;
        case 3: result = v; break;
        case 4: result = c && !z; break;
        case 5: result = n == v; break;
        case 6: result = !z && (n == v); break;
        default: return condition == 14;
    }
    return result ^ (condition & 1);
}

static void *unwind_full_data( ULONG_PTR base, ULONG_PTR pc, RUNTIME_FUNCTION *func,
                               CONTEXT *context, PVOID *handler_data, KNONVOLATILE_CONTEXT_POINTERS *ptrs )
{
    struct unwind_info *info;
    struct unwind_info_epilog *info_epilog;
    unsigned int i, codes, epilogs, len, offset;
    void *data;
    BYTE *end;

    info = (struct unwind_info *)((char *)base + func->UnwindData);
    data = info + 1;
    epilogs = info->epilog;
    codes = info->codes;
    if (!codes && !epilogs)
    {
        struct unwind_info_ext *infoex = data;
        codes = infoex->codes;
        epilogs = infoex->epilog;
        data = infoex + 1;
    }
    info_epilog = data;
    if (!info->e) data = info_epilog + epilogs;

    offset = (pc - base) - (func->BeginAddress & ~1u);
    end = (BYTE *)data + codes * 4;



    if (offset < codes * 4 * 4 && !info->f)
    {
        len = get_sequence_len( data, end, 0 );
        if (offset < len)
        {
            process_unwind_codes( data, end, context, ptrs, len - offset );
            return NULL;
        }
    }


    if (!info->e)
    {
        for (i = 0; i < epilogs; i++)
        {
            if (offset < 2 * info_epilog[i].offset) break;
            if (!condition_holds(info_epilog[i].cond, context->Cpsr)) continue;
            if (offset - 2 * info_epilog[i].offset < (codes * 4 - info_epilog[i].index) * 4)
            {
                BYTE *ptr = (BYTE *)data + info_epilog[i].index;
                len = get_sequence_len( ptr, end, 1 );
                if (offset < 2 * info_epilog[i].offset + len)
                {
                    process_unwind_codes( ptr, end, context, ptrs, offset - 2 * info_epilog[i].offset );
                    return NULL;
                }
            }
        }
    }
    else if (2 * info->function_length - offset <= (codes * 4 - epilogs) * 4)
    {
        BYTE *ptr = (BYTE *)data + epilogs;
        len = get_sequence_len( ptr, end, 1 );
        if (offset >= 2 * info->function_length - len)
        {
            process_unwind_codes( ptr, end, context, ptrs, offset - (2 * info->function_length - len) );
            return NULL;
        }
    }

    process_unwind_codes( data, end, context, ptrs, 0 );


    if (info->x)
    {
        DWORD *handler_rva = (DWORD *)data + codes;
        *handler_data = handler_rva + 1;
        return (char *)base + *handler_rva;
    }
    return NULL;
}

PEXCEPTION_ROUTINE NTAPI
RtlVirtualUnwind(ULONG type, ULONG_PTR base, ULONG_PTR pc,
                 PRUNTIME_FUNCTION func, PCONTEXT context,
                 PVOID *handler_data, PULONG_PTR frame_ret,
                 PKNONVOLATILE_CONTEXT_POINTERS ptrs)
{
    PEXCEPTION_ROUTINE handler = NULL;
    PVOID data = NULL;

    context->Pc = 0;
    if (!func && pc == context->Lr)
        return NULL;

    if (func)
    {
        if (func->Flag == 1 || func->Flag == 2)
            handler = unwind_packed_data(base, pc & ~1u, func, context, ptrs);
        else if (!func->Flag)
            handler = unwind_full_data(base, pc & ~1u, func, context, &data, ptrs);
        else
            return NULL;
    }
    if (!context->Pc)
    {
        context->Pc = context->Lr;
        context->ContextFlags |= CONTEXT_UNWOUND_TO_CALL;
    }
    if (handler_data) *handler_data = data;
    *frame_ret = context->Sp;
    return (type & (UNW_FLAG_EHANDLER | UNW_FLAG_UHANDLER)) ? handler : NULL;
}

ULONG
RtlpGetFunctionStartAddress(PRUNTIME_FUNCTION FunctionEntry)
{
    return FunctionEntry->BeginAddress & ~1u;
}

ULONG_PTR
RtlpGetFunctionEndAddress(ULONG_PTR BaseAddress, PRUNTIME_FUNCTION FunctionEntry)
{
    ULONG Length;

    if (FunctionEntry->Flag)
        Length = FunctionEntry->FunctionLength;
    else
        Length = *(PULONG)(BaseAddress + FunctionEntry->UnwindData) & 0x3ffff;
    return RtlpGetFunctionStartAddress(FunctionEntry) + 2 * Length;
}

PRUNTIME_FUNCTION
RtlpArmFindFunctionEntry(ULONG_PTR ControlPc, ULONG_PTR ImageBase,
                         PRUNTIME_FUNCTION Table, ULONG Count)
{
    ULONG Low = 0, High = Count, Mid;
    ULONG Offset = (ControlPc & ~1u) - ImageBase;

    while (Low < High)
    {
        Mid = Low + (High - Low) / 2;
        if (Offset < RtlpGetFunctionStartAddress(&Table[Mid]))
            High = Mid;
        else
            Low = Mid + 1;
    }
    if (Low && Offset < RtlpGetFunctionEndAddress(ImageBase, &Table[Low - 1]))
        return &Table[Low - 1];
    return NULL;
}
