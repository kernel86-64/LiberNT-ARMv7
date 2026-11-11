/*
 * PROJECT:     LiberNT API tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARM32 nested kernel callback continuation regression
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ntoskrnl/include/ntoskrnl.h>

static DECLSPEC_ALIGN(8) UCHAR CallbackUserStack[8192];
static ULONG CallbackFailures, CallbackCalls;
PVOID ArmGrowStackPointer;
ULONG ArmGrowStackCalls;
NTSTATUS ArmGrowStackStatus;
PVOID KeUserCallbackDispatcher = (PVOID)1;
PVOID KeUserExceptionDispatcher = (PVOID)3;

static VOID NTAPI ArmCallbackProbe(PVOID Address, SIZE_T Length, ULONG Alignment)
{
    ULONG_PTR Low = (ULONG_PTR)CallbackUserStack;
    ULONG_PTR High = Low + sizeof(CallbackUserStack);

    if ((ULONG_PTR)Address < Low || (ULONG_PTR)Address > High ||
        Length > High - (ULONG_PTR)Address || ((ULONG_PTR)Address & (Alignment - 1)))
        KeBugCheck(INVALID_KERNEL_HANDLE);
}

NTSTATUS NTAPI MmGrowKernelStack(PVOID Stack)
{
    if (!ArmGrowStackPointer || Stack != ArmGrowStackPointer)
        KeBugCheck(KMODE_EXCEPTION_NOT_HANDLED);
    ++ArmGrowStackCalls;
    return ArmGrowStackStatus;
}

static DECLSPEC_NORETURN VOID ArmCallbackExit(PKTRAP_FRAME Trap, PKEXCEPTION_FRAME Exception);

#define KiExceptionExit ArmCallbackExit
#define ProbeForWrite ArmCallbackProbe
#include "../../../../ntoskrnl/ke/arm/usercall.c"
#include "../../../../ntoskrnl/ke/arm/userexcept.c"
#undef ProbeForWrite
#undef KiExceptionExit

DECLSPEC_NORETURN VOID ArmTestCallbackDispatcher(VOID)
{
    KeBugCheck(KMODE_EXCEPTION_NOT_HANDLED);
}

static DECLSPEC_NORETURN VOID ArmCallbackExit(PKTRAP_FRAME Trap, PKEXCEPTION_FRAME Exception)
{
    PKTHREAD Thread = KeGetCurrentThread();
    PUCALLOUT_FRAME Frame = (PVOID)Trap->Sp;
    PVOID Result;
    ULONG Length;
    ULONG Argument = 0x12345678;
    ULONG_PTR Initial = (ULONG_PTR)Thread->InitialStack;
    NTSTATUS Status;

    ++CallbackCalls;
    if (Thread->TrapFrame != Trap || !Thread->CallbackStack ||
        Exception->TrapFrame != (ULONG_PTR)Trap || Trap->Pc != 1 ||
        (Trap->Cpsr & CPSRM_MASK) != CPSRM_USER ||
        Initial != (ULONG_PTR)KeGetCurrentPrcb()->SpBase || (Initial & 7) ||
        Frame->Buffer != Frame + 1 || Frame->Length != sizeof(Argument) ||
        *(PULONG)Frame->Buffer != Argument || (Trap->Sp & 7))
        ++CallbackFailures;
    if (Frame->ApiNumber == 7)
    {
        Status = KeUserModeCallback(8, &Argument, sizeof(Argument), &Result, &Length);
        if (Status != STATUS_ACCESS_DENIED || Result != (PVOID)0x1234 || Length != 9 ||
            Thread->TrapFrame != Trap || Thread->InitialStack != (PVOID)Initial ||
            Thread->CallbackNestingLevel != 1)
            ++CallbackFailures;
    }
    else if (Frame->ApiNumber == 9)
    {
        Trap->Sp = (ULONG_PTR)(CallbackUserStack + sizeof(CallbackUserStack) - 128);
        Trap->R0 = 42;
        Trap->VfpState->VfpD[31] = 0xaabbccddeeff0011ULL;
        Exception->R4 = 0x12345678;
        NtCallbackReturn((PVOID)0x1234, 9, STATUS_CALLBACK_POP_STACK);
        KeBugCheck(KMODE_EXCEPTION_NOT_HANDLED);
    }
    else if (Frame->ApiNumber != 8 || Thread->CallbackNestingLevel != 2)
        ++CallbackFailures;
    __asm__ __volatile__("vmov.f64 d8, #16.0\n\tvmov.f64 d15, #16.0" ::: "d8", "d15");
    NtCallbackReturn((PVOID)0x1234, 9, STATUS_ACCESS_DENIED);
    KeBugCheck(KMODE_EXCEPTION_NOT_HANDLED);
}

BOOLEAN ArmTestCallbacks(PKTHREAD Thread)
{
    struct { KEXCEPTION_FRAME Exception; KTRAP_FRAME Trap; KARM_VFP_STATE Vfp; } Original = {0};
    KARM_VFP_STATE LinkedVfp = {0};
    PVOID Initial = Thread->InitialStack, PrcbBase = KeGetCurrentPrcb()->SpBase;
    PKTRAP_FRAME OldTrap = Thread->TrapFrame;
    ULONG_PTR Limit = Thread->StackLimit;
    PVOID Result;
    ULONG Length, Cpsr, After, Low, High;
    ULONG Argument = 0x12345678;
    NTSTATUS Status;
    BOOLEAN Passed;
    CONTEXT Context = {0};
    EXCEPTION_RECORD Record = {0};
    PCONTEXT UserContext;
    PEXCEPTION_RECORD UserRecord;

    Original.Trap.Sp = (ULONG_PTR)(CallbackUserStack + sizeof(CallbackUserStack));
    Original.Trap.Cpsr = CPSRM_USER | CPSRC_THUMB;
    Original.Exception.TrapFrame = (ULONG_PTR)&Original.Trap;
    Original.Trap.VfpState = &Original.Vfp;
    Original.Vfp.Link = &LinkedVfp;
    Thread->TrapFrame = &Original.Trap;
    Thread->StackLimit = 0;
    Thread->PreviousMode = UserMode;
    CallbackCalls = CallbackFailures = 0;
    Passed = NtCallbackReturn(NULL, 0, 0) == STATUS_NO_CALLBACK_ACTIVE;
    Thread->CallbackNestingLevel = KI_MAX_USER_CALLBACK_NESTING;
    Passed = KeUserModeCallback(7, &Argument, sizeof(Argument), &Result, &Length) ==
             STATUS_STACK_OVERFLOW && Passed;
    Thread->CallbackNestingLevel = 0;
    __asm__ __volatile__("mrs %0, cpsr" : "=r"(Cpsr));
    __asm__ __volatile__("vmov.f64 d8, #1.0\n\tvmov.f64 d15, #2.0" ::: "d8", "d15");
    Status = KeUserModeCallback(7, &Argument, sizeof(Argument), &Result, &Length);
    __asm__ __volatile__("mrs %0, cpsr" : "=r"(After));
    __asm__ __volatile__("vmov %0, %1, d8" : "=r"(Low), "=r"(High));
    Passed = Passed && Low == 0 && High == 0x3ff00000;
    __asm__ __volatile__("vmov %0, %1, d15" : "=r"(Low), "=r"(High));
    Passed = Passed && Low == 0 && High == 0x40000000;
    Passed = Passed && Status == STATUS_ACCESS_DENIED && Result == (PVOID)0x1234 &&
             Length == 9 && Thread->InitialStack == Initial && Thread->TrapFrame == &Original.Trap &&
             !Thread->CallbackStack && !Thread->CallbackNestingLevel &&
             !((Cpsr ^ After) & 0xc0) && CallbackCalls == 2 && !CallbackFailures &&
             Original.Trap.Sp == (ULONG_PTR)(CallbackUserStack + sizeof(CallbackUserStack));
    Status = KeUserModeCallback(9, &Argument, sizeof(Argument), &Result, &Length);
    Passed = Passed && Status == STATUS_CALLBACK_POP_STACK && CallbackCalls == 3 && !CallbackFailures &&
             Original.Trap.Sp == (ULONG_PTR)(CallbackUserStack + sizeof(CallbackUserStack) - 128) &&
             Original.Trap.R0 == 42 && Original.Exception.R4 == 0x12345678 &&
             Original.Trap.VfpState == &Original.Vfp && Original.Vfp.Link == &LinkedVfp &&
             Original.Vfp.VfpD[31] == 0xaabbccddeeff0011ULL;
    Context.ContextFlags = CONTEXT_FULL;
    Context.Sp = Original.Trap.Sp;
    Context.Pc = 0x12345;
    Context.R4 = 0x76543210;
    Context.D[31] = 0x1122334455667788ULL;
    Record.ExceptionCode = STATUS_ACCESS_VIOLATION;
    Record.NumberParameters = 2;
    Record.ExceptionInformation[1] = 0x1234;
    Passed = KiArmDispatchUserException(&Original.Trap, &Context, &Record) && Passed;
    UserContext = (PVOID)Original.Trap.Sp;
    UserRecord = (PVOID)(UserContext + 1);
    Passed = Passed && Original.Trap.Pc == 3 && !(Original.Trap.Sp & 7) &&
             UserContext->Sp == Context.Sp && UserContext->Pc == Context.Pc &&
             UserContext->R4 == Context.R4 && UserContext->D[31] == Context.D[31] &&
             UserRecord->ExceptionCode == Record.ExceptionCode &&
             UserRecord->NumberParameters == 2 && UserRecord->ExceptionInformation[1] == 0x1234;
    Thread->TrapFrame = OldTrap;
    Thread->StackLimit = Limit;
    KeGetCurrentPrcb()->SpBase = PrcbBase;
    return Passed;
}
