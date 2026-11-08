/*
 * PROJECT:     LiberNT API tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARM32 user context sanitization and VFP conversion checks
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ntoskrnl/include/ntoskrnl.h>

static DECLSPEC_ALIGN(PAGE_SIZE) KIPCR ContextPcr;
static DECLSPEC_ALIGN(8) UCHAR ApcStack[8192];
PVOID KeUserApcDispatcher;
extern VOID ArmTestUserApcDispatcher(VOID);
extern VOID ArmTestApcThunk(VOID);
BOOLEAN ArmTestApcDispatch(PCONTEXT Context);

VOID NTAPI ProbeForWrite(PVOID Address, SIZE_T Length, ULONG Alignment)
{
    if ((ULONG_PTR)Address < (ULONG_PTR)ApcStack ||
        (ULONG_PTR)Address > (ULONG_PTR)(ApcStack + sizeof(ApcStack)) ||
        Length > (ULONG_PTR)(ApcStack + sizeof(ApcStack)) - (ULONG_PTR)Address ||
        !Alignment || ((ULONG_PTR)Address & (Alignment - 1)))
        KeBugCheck(INVALID_KERNEL_HANDLE);
}

static DECLSPEC_NORETURN VOID ArmContextUnexpectedIrql(VOID)
{
    KeBugCheck(IRQL_NOT_LESS_OR_EQUAL);
}

PVOID __imp_KfRaiseIrql = ArmContextUnexpectedIrql;
PVOID __imp_KfLowerIrql = ArmContextUnexpectedIrql;

BOOLEAN ArmTestContextFrames(VOID)
{
    CONTEXT Input, Output;
    KTRAP_FRAME Trap;
    KEXCEPTION_FRAME Exception;
    KARM_VFP_STATE Vfp;
    struct { KEXCEPTION_FRAME Exception; KTRAP_FRAME Trap; } Service;
    KEXCEPTION_FRAME Scratch;
    KI_SERVICE_EXCEPTION_STATE State;
    KTHREAD Thread;
    PKEXCEPTION_FRAME Frame;
    ULONG Pcr, Cpsr, Index;
    BOOLEAN Passed;

    RtlZeroMemory(&ContextPcr, sizeof(ContextPcr));
    ContextPcr.CurrentIrql = APC_LEVEL;
    __asm__ __volatile__("mrs %0, cpsr\n\tcpsid if" : "=r"(Cpsr) :: "memory");
    __asm__ __volatile__("mrc p15, 0, %0, c13, c0, 4" : "=r"(Pcr));
    __asm__ __volatile__("mcr p15, 0, %0, c13, c0, 4\n\tisb" :: "r"(&ContextPcr) : "memory");
    RtlZeroMemory(&Input, sizeof(Input));
    RtlZeroMemory(&Output, sizeof(Output));
    RtlZeroMemory(&Trap, sizeof(Trap));
    RtlZeroMemory(&Exception, sizeof(Exception));
    RtlZeroMemory(&Vfp, sizeof(Vfp));
    Trap.VfpState = &Vfp;
    RtlZeroMemory(&Service, sizeof(Service));
    RtlZeroMemory(&Scratch, sizeof(Scratch));
    RtlZeroMemory(&Thread, sizeof(Thread));
    Thread.TrapFrame = &Service.Trap;
    Thread.PreviousMode = KernelMode;
    Service.Trap.TrapFrame = (ULONG_PTR)&Trap;
    Service.Trap.PreviousMode = UserMode;
    Service.Exception.R4 = 0x12345678;
    Frame = KiEnterServiceException(&Thread, &Service.Trap, &Input, KernelMode, &Scratch, &State);
    Passed = Frame == &Service.Exception && Thread.TrapFrame == &Service.Trap &&
             Frame->R4 == 0x12345678 && Thread.PreviousMode == KernelMode;
    KiLeaveServiceException(&Thread, &Service.Trap);
    Passed = Passed && Thread.TrapFrame == &Trap && Thread.PreviousMode == UserMode;
    Input.ContextFlags = CONTEXT_FULL;
    Input.Cpsr = MAXULONG;
    Input.Pc = 0x10001;
    Input.Sp = 0x20000;
    Input.Lr = 0x30001;
    Input.Fpscr = 0x07C00000;
    for (Index = 0; Index < 13; ++Index) (&Input.R0)[Index] = 0x100 + Index;
    for (Index = 0; Index < 32; ++Index) Input.D[Index] = 0x1122334400000000ULL + Index;
    KeContextToTrapFrame(&Input, &Exception, &Trap, CONTEXT_FULL, UserMode);
    Output.ContextFlags = CONTEXT_FULL;
    KeTrapFrameToContext(&Trap, &Exception, &Output);
    Passed = Passed && Output.Cpsr == 0xFE0FFC30 && Output.Pc == Input.Pc &&
             Output.Sp == Input.Sp && Output.Lr == Input.Lr && Output.Fpscr == Input.Fpscr;
    for (Index = 0; Index < 13; ++Index)
        if ((&Output.R0)[Index] != (&Input.R0)[Index]) Passed = FALSE;
    for (Index = 0; Index < 32; ++Index)
        if (Output.D[Index] != Input.D[Index]) Passed = FALSE;
    Input.Cpsr = CPSRM_USER | CPSRC_THUMB | CPSR_IT_MASK;
    Input.Sp = (ULONG_PTR)(ApcStack + sizeof(ApcStack));
    KeContextToTrapFrame(&Input, &Exception, &Trap, CONTEXT_FULL, UserMode);
    KeUserApcDispatcher = ArmTestUserApcDispatcher;
    KiInitializeUserApc(&Exception, &Trap, (PKNORMAL_ROUTINE)ArmTestApcThunk,
                        NULL, (PVOID)0x10000000, NULL);
    if (Trap.Pc != ((ULONG_PTR)ArmTestUserApcDispatcher | 1) ||
        Trap.R3 != (ULONG_PTR)ArmTestApcThunk || Trap.Sp & 7 ||
        Trap.Lr != Input.Lr || Trap.Cpsr & CPSR_IT_MASK ||
        (Trap.Cpsr & (CPSRM_MASK | CPSRC_THUMB)) != (CPSRM_USER | CPSRC_THUMB))
        Passed = FALSE;
    if (!ArmTestApcDispatch((PCONTEXT)Trap.Sp)) Passed = FALSE;
    Input.Cpsr = CPSRM_SVC;
    Input.R0 = 0x77777777;
    KeContextToTrapFrame(&Input, &Exception, &Trap, CONTEXT_CONTROL, KernelMode);
    if (Trap.Cpsr != CPSRM_SVC || Trap.R0 != 0) Passed = FALSE;
    __asm__ __volatile__("mcr p15, 0, %0, c13, c0, 4\n\tisb" :: "r"(Pcr) : "memory");
    __asm__ __volatile__("msr cpsr_c, %0" :: "r"(Cpsr) : "memory");
    return Passed;
}
