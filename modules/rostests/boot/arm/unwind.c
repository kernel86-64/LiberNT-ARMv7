/*
 * PROJECT:     LiberNT boot tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Microsoft ARM32 PE unwind format regression tests
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <windows.h>

static ULONG Failures;
ULONG ArmTestFailureLine;

static void CheckAt(BOOLEAN Value, ULONG Line)
{
    if (!Value)
    {
        ++Failures;
        if (!ArmTestFailureLine) ArmTestFailureLine = Line;
    }
}

#define Check(Value) CheckAt(!!(Value), __LINE__)

static void TestPacked(void)
{
    RUNTIME_FUNCTION Function = {0};
    CONTEXT Context;
    KNONVOLATILE_CONTEXT_POINTERS Pointers;
    ULONG Stack[8];
    ULONG_PTR Frame;
    PVOID Data;
    ULONG Offset;
    PEXCEPTION_ROUTINE Handler;
    static const ULONG Offsets[] = {0, 2, 4, 8, 28, 30};

    Function.BeginAddress = 0x101;
    Function.Flag = 1;
    Function.FunctionLength = 16;
    Function.L = 1;
    Function.StackAdjust = 2;
    Stack[2] = 0x44444444;
    Stack[3] = 0x12345679;
    for (Offset = 0; Offset < ARRAYSIZE(Offsets); ++Offset)
    {
        ZeroMemory(&Context, sizeof(Context));
        ZeroMemory(&Pointers, sizeof(Pointers));
        Context.Lr = 0x12345679;
        Context.R4 = 0x44444444;
        Context.Sp = (ULONG_PTR)&Stack[(Offsets[Offset] == 0) ? 4 :
                         (Offsets[Offset] == 2 || Offsets[Offset] == 30) ? 2 : 0];
        Data = (PVOID)1;
        Handler = RtlVirtualUnwind(UNW_FLAG_NHANDLER, 0x1000,
                    0x1101 + Offsets[Offset], &Function, &Context, &Data, &Frame, &Pointers);
        Check(!Handler && !Data);
        Check(Context.Sp == (ULONG_PTR)&Stack[4] && Frame == Context.Sp);
        Check(Context.Pc == 0x12345679 && Context.R4 == 0x44444444);
        Check(!!(Context.ContextFlags & CONTEXT_UNWOUND_TO_CALL));
        Check(Offsets[Offset] == 0 ? !Pointers.R4 : Pointers.R4 == &Stack[2]);
        Check(Offsets[Offset] == 0 ? !Pointers.Lr : Pointers.Lr == &Stack[3]);
    }
    Function.Flag = 2;
    ZeroMemory(&Context, sizeof(Context));
    Context.Sp = (ULONG_PTR)Stack;
    RtlVirtualUnwind(0, 0x1000, 0x1101, &Function, &Context, &Data, &Frame, NULL);
    Check(Context.Sp == (ULONG_PTR)&Stack[4] && Context.Pc == Stack[3]);
}

static void TestFull(void)
{
    ULONG Image[64] = {0};
    ULONG Stack[8] = {0};
    RUNTIME_FUNCTION Function = {0};
    CONTEXT Context;
    ULONG_PTR Base = (ULONG_PTR)Image, Frame;
    PVOID Data;
    PEXCEPTION_ROUTINE Handler;

    Function.BeginAddress = 0x101;
    Function.UnwindData = 0x40;
    Image[16] = 16 | (1u << 20) | (1u << 21) | (1u << 28);
    Image[17] = 0x00ffd402;
    Image[18] = 0x80;
    Image[19] = 0xcafebabe;
    Stack[2] = 0x44444444;
    Stack[3] = 0x12345679;
    ZeroMemory(&Context, sizeof(Context));
    Context.Sp = (ULONG_PTR)Stack;
    Handler = RtlVirtualUnwind(UNW_FLAG_EHANDLER, Base, Base + 0x109,
                               &Function, &Context, &Data, &Frame, NULL);
    Check((ULONG_PTR)Handler == Base + 0x80 && Data == &Image[19]);
    Check(Context.Sp == (ULONG_PTR)&Stack[4] && Context.Pc == Stack[3]);
    Check(Context.R4 == Stack[2] && Frame == Context.Sp);
    ZeroMemory(&Context, sizeof(Context));
    Context.Sp = (ULONG_PTR)Stack;
    Handler = RtlVirtualUnwind(UNW_FLAG_EHANDLER, Base, Base + 0x11d,
                               &Function, &Context, &Data, &Frame, NULL);
    Check(!Handler && !Data && Context.Pc == Stack[3]);

    Image[16] = 16 | (1u << 23) | (1u << 28);
    Image[17] = 14;
    Image[18] = 0x00ffd402;
    ZeroMemory(&Context, sizeof(Context));
    Context.Sp = (ULONG_PTR)Stack;
    RtlVirtualUnwind(0, Base, Base + 0x11f, &Function, &Context, &Data, &Frame, NULL);
    Check(Context.Sp == (ULONG_PTR)&Stack[4] && Context.R4 == Stack[2]);
    ZeroMemory(&Context, sizeof(Context));
    Context.Cpsr = 1u << 30;
    Context.Sp = (ULONG_PTR)&Stack[2];
    RtlVirtualUnwind(0, Base, Base + 0x11f, &Function, &Context, &Data, &Frame, NULL);
    Check(Context.Sp == (ULONG_PTR)&Stack[4] && Context.R4 == Stack[2]);
}

static void TestFloatAndFrames(void)
{
    RUNTIME_FUNCTION Function = {0};
    CONTEXT Context, Saved;
    KNONVOLATILE_CONTEXT_POINTERS Pointers;
    ULONGLONG Stack[4] = {0x1234567812345678ULL, 0x8765432187654321ULL};
    ULONG Image[32] = {0};
    ULONG_PTR Base = (ULONG_PTR)Image, Frame;
    PVOID Data;

    Function.BeginAddress = 0x101;
    Function.Flag = 1;
    Function.FunctionLength = 16;
    Function.R = 1;
    Function.Reg = 1;
    Function.Ret = 1;
    ZeroMemory(&Context, sizeof(Context));
    ZeroMemory(&Pointers, sizeof(Pointers));
    Context.Sp = (ULONG_PTR)Stack;
    Context.Lr = 0x12345679;
    RtlVirtualUnwind(0, Base, Base + 0x109, &Function, &Context, &Data, &Frame, &Pointers);
    Check(Context.D[8] == Stack[0] && Context.D[9] == Stack[1]);
    Check(Pointers.D8 == &Stack[0] && Pointers.D9 == &Stack[1]);
    Check(Context.Sp == (ULONG_PTR)&Stack[2] && Context.Pc == Context.Lr);

    Function.UnwindData = 0x40;
    Image[16] = 16 | (1u << 22) | (1u << 28);
    Image[17] = 0x00ff01ee;
    ((ULONG *)Stack)[0] = 0x80001000;
    ((ULONG *)Stack)[1] = 0x80002001;
    Context.Sp = (ULONG_PTR)Stack;
    Context.ContextFlags = CONTEXT_UNWOUND_TO_CALL;
    RtlVirtualUnwind(0, Base, Base + 0x109, &Function, &Context, &Data, &Frame, NULL);
    Check(Context.Sp == 0x80001000 && Context.Pc == 0x80002001);
    Check(!(Context.ContextFlags & CONTEXT_UNWOUND_TO_CALL));

    Image[17] = 0x00ff02ee;
    ZeroMemory(&Saved, sizeof(Saved));
    Saved.Sp = 0x80003000;
    Saved.Pc = 0x80004001;
    Saved.R4 = 0x4455;
    Saved.D[8] = Stack[1];
    Context.Sp = (ULONG_PTR)&Saved;
    RtlVirtualUnwind(0, Base, Base + 0x109, &Function, &Context, &Data, &Frame, &Pointers);
    Check(Context.Sp == Saved.Sp && Context.Pc == Saved.Pc && Context.R4 == Saved.R4);
    Check(Pointers.R4 == &Saved.R4 && Pointers.D8 == &Saved.D[8]);

    ZeroMemory(&Context, sizeof(Context));
    Context.Sp = 0x80001000;
    Context.Lr = 0x80002001;
    RtlVirtualUnwind(0, 0, 0x80003001, NULL, &Context, &Data, &Frame, NULL);
    Check(Context.Pc == Context.Lr && Context.Sp == 0x80001000 && !Data);
}

ULONG ArmTestVirtualUnwind(void)
{
    Failures = 0;
    {
        CONTEXT Captured;
        ZeroMemory(&Captured, sizeof(Captured));
        RtlCaptureContext(&Captured);
        Check(Captured.ContextFlags == CONTEXT_FULL);
        Check(Captured.R0 == 0 && Captured.Lr == 0);
        Check((Captured.Pc & 1) && (Captured.Cpsr & 0x20));
        Check(Captured.Sp != 0 && !(Captured.Sp & 7));
    }
    TestPacked();
    TestFull();
    TestFloatAndFrames();
    return Failures;
}

PRUNTIME_FUNCTION
RtlpArmFindFunctionEntry(ULONG_PTR ControlPc, ULONG_PTR ImageBase,
                         PRUNTIME_FUNCTION Table, ULONG Count);

DECLSPEC_NOINLINE int
ArmTestPeUnwindFrame(PVOID Image, ULONG_PTR CallerSp, ULONG_PTR CallerPc)
{
    PIMAGE_DOS_HEADER Dos = Image;
    PIMAGE_NT_HEADERS Headers = (PIMAGE_NT_HEADERS)((PUCHAR)Image + Dos->e_lfanew);
    PIMAGE_DATA_DIRECTORY Directory = &Headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    PRUNTIME_FUNCTION Table = (PRUNTIME_FUNCTION)((PUCHAR)Image + Directory->VirtualAddress);
    PRUNTIME_FUNCTION Function;
    CONTEXT Context;
    KNONVOLATILE_CONTEXT_POINTERS Pointers;
    ULONG_PTR Frame;
    PVOID Data;
    ULONG Index;
    ULONG Before = Failures;
    static const ULONGLONG Fp[] = {
        0x3ff0000000000000ULL, 0x4000000000000000ULL, 0x4008000000000000ULL,
        0x4010000000000000ULL, 0x4014000000000000ULL, 0x4018000000000000ULL,
        0x401c000000000000ULL, 0x4020000000000000ULL};

    ZeroMemory(&Pointers, sizeof(Pointers));
    RtlCaptureContext(&Context);
    Function = RtlpArmFindFunctionEntry(Context.Pc, (ULONG_PTR)Image, Table,
                                       Directory->Size / sizeof(*Table));
    Check(Function != NULL);
    if (!Function) return 0;
    Check(RtlpArmFindFunctionEntry((ULONG_PTR)Image, (ULONG_PTR)Image, Table,
                                   Directory->Size / sizeof(*Table)) == NULL);
    RtlVirtualUnwind(0, (ULONG_PTR)Image, Context.Pc, Function,
                    &Context, &Data, &Frame, &Pointers);
    Check(Context.Sp == CallerSp && Context.Pc == CallerPc && Frame == CallerSp);
    for (Index = 0; Index < 8; ++Index)
    {
        Check((&Context.R4)[Index] == 68 + Index);
        Check(Context.D[8 + Index] == Fp[Index]);
    }
    return Before == Failures;
}
