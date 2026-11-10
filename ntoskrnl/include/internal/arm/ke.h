#pragma once

#define KI_ARCH_THREAD_HANDOFF_USES_RUNNING 1
#define KI_USER_SLIST_POP_NO_ROLLBACK
#define KI_EMULATED_USER_MACHINE 0
#define KI_EMULATED_USER_SYSTEM_DLL NULL

#include "intrin_i.h"

#define KiServiceExit2 KiExceptionExit

#define SYNCH_LEVEL DISPATCH_LEVEL
#define PCR                     ((KPCR * const)KIP0PCRADDRESS)

//
//Lockdown TLB entries
//
#define PCR_ENTRY            0
#define PDR_ENTRY            2

//
// BKPT is 4 bytes long
//
#define KD_BREAKPOINT_TYPE        ULONG
#define KD_BREAKPOINT_SIZE        sizeof(ULONG)
#define KD_BREAKPOINT_VALUE       0xDEFE

//
// Maximum IRQs
//
#define MAXIMUM_VECTOR          16

//
// Macros for getting and setting special purpose registers in portable code
//
#define KeGetContextPc(Context) \
    ((Context)->Pc)

#define KeSetContextPc(Context, ProgramCounter) \
    ((Context)->Pc = (ProgramCounter))

#define KeGetTrapFramePc(TrapFrame) \
    ((TrapFrame)->Pc)

#define KeGetContextReturnRegister(Context) \
    ((Context)->R0)

#define KeSetContextReturnRegister(Context, ReturnValue) \
    ((Context)->R0 = (ReturnValue))

//
// Macro to get trap and exception frame from a thread stack
//
#define KeGetTrapFrame(Thread) \
    (PKTRAP_FRAME)((ULONG_PTR)((Thread)->InitialStack) - \
                   sizeof(KTRAP_FRAME))

#define KeGetExceptionFrame(Thread) \
    (PKEXCEPTION_FRAME)((ULONG_PTR)KeGetTrapFrame(Thread) - \
                        sizeof(KEXCEPTION_FRAME))

//
// Macro to get context switches from the PRCB
// All architectures but x86 have it in the PRCB's KeContextSwitches
//
#define KeGetContextSwitches(Prcb)  \
    (Prcb)->KeContextSwitches

//
// Macro to get the second level cache size field name which differs between
// CISC and RISC architectures, as the former has unified I/D cache
//
#define KiGetSecondLevelDCacheSize() ((PKIPCR)KeGetPcr())->SecondLevelDcacheSize

//
// Returns the Interrupt State from a Trap Frame.
// ON = TRUE, OFF = FALSE
//
#define KeGetTrapFrameInterruptState(TrapFrame) 0

FORCEINLINE
BOOLEAN
KeDisableInterrupts(VOID)
{
    ARM_STATUS_REGISTER Flags;

    //
    // Get current interrupt state and disable interrupts
    //
    Flags = KeArmStatusRegisterGet();
    _disable();

    //
    // Return previous interrupt state
    //
    return Flags.IrqDisable;
}

FORCEINLINE
VOID
KeRestoreInterrupts(BOOLEAN WereEnabled)
{
    if (WereEnabled) _enable();
}

//
// Invalidates the TLB entry for a specified address
//
FORCEINLINE
VOID
KeInvalidateTlbEntry(IN PVOID Address)
{
    /* Invalidate the TLB entry for this address */
    KeArmInvalidateTlbEntry(Address);
}

FORCEINLINE
VOID
KeFlushProcessTb(VOID)
{
    KeArmFlushTlb();
}

FORCEINLINE
VOID
KeSweepICache(IN PVOID BaseAddress,
              IN SIZE_T FlushSize)
{
    //
    // Always sweep the whole cache
    //
    UNREFERENCED_PARAMETER(BaseAddress);
    UNREFERENCED_PARAMETER(FlushSize);
    _MoveToCoprocessor(0, CP15_ICIALLU);
}

FORCEINLINE
VOID
KiRundownThread(IN PKTHREAD Thread)
{
    /* FIXME */
}

VOID
KiPassiveRelease(
    VOID
);

VOID
KiSystemService(IN PKTHREAD Thread,
                IN PKTRAP_FRAME TrapFrame,
                IN ULONG Instruction);

VOID
KiApcInterrupt(
    VOID
);

#include "mm.h"

VOID
KeFlushTb(
    VOID
);

//
// Cache clean and flush
//
VOID
HalSweepDcache(
    VOID
);

VOID
HalSweepIcache(
    VOID
);

#define Ki386PerfEnd()
#define KiEndInterrupt(x,y)

#define KiGetLinkedTrapFrame(x) \
    (PKTRAP_FRAME)((x)->TrapFrame)

typedef struct _KI_SERVICE_EXCEPTION_STATE
{
    UCHAR Reserved;
} KI_SERVICE_EXCEPTION_STATE, *PKI_SERVICE_EXCEPTION_STATE;

FORCEINLINE
PKEXCEPTION_FRAME
KiEnterServiceException(
    _In_ PKTHREAD Thread,
    _In_ PKTRAP_FRAME TrapFrame,
    _In_ PCONTEXT Context,
    _In_ KPROCESSOR_MODE PreviousMode,
    _In_opt_ PKEXCEPTION_FRAME ExceptionFrame,
    _Out_ PKI_SERVICE_EXCEPTION_STATE State)
{
    UNREFERENCED_PARAMETER(Thread);
    UNREFERENCED_PARAMETER(Context);
    UNREFERENCED_PARAMETER(PreviousMode);
    UNREFERENCED_PARAMETER(ExceptionFrame);
    UNREFERENCED_PARAMETER(State);
    return (PKEXCEPTION_FRAME)TrapFrame - 1;
}

FORCEINLINE
VOID
KiAbortServiceException(
    _In_ PKTRAP_FRAME TrapFrame,
    _In_opt_ PKEXCEPTION_FRAME ExceptionFrame,
    _In_ PKI_SERVICE_EXCEPTION_STATE State)
{
    UNREFERENCED_PARAMETER(TrapFrame);
    UNREFERENCED_PARAMETER(ExceptionFrame);
    UNREFERENCED_PARAMETER(State);
}

FORCEINLINE
VOID
KiLeaveServiceException(
    _In_ PKTHREAD Thread,
    _In_ PKTRAP_FRAME TrapFrame)
{
    Thread->PreviousMode = TrapFrame->PreviousMode;
    Thread->TrapFrame = KiGetLinkedTrapFrame(TrapFrame);
}

#define KiGetPreviousMode(tf) \
    ((((tf)->Cpsr & CPSRM_MASK) == CPSRM_USER) ? UserMode : KernelMode)

VOID KiArmDeliverUserApc(PKTRAP_FRAME TrapFrame, PKEXCEPTION_FRAME ExceptionFrame);

#define KI_DPC_NORMAL_PROCESSING_REQUESTED 0x00000002L
#define KI_DPC_NORMAL_DPC_PRESENT          0x00000010L
#define KI_DPC_THREAD_ACTIVE               0x00010000L
#define KI_DPC_THREAD_REQUESTED            0x00020000L

FORCEINLINE
BOOLEAN
KiIsDpcInterruptRequested(
    _In_ PKPRCB Prcb)
{
    return (Prcb->DpcRequestSummary & KI_DPC_NORMAL_PROCESSING_REQUESTED) != 0;
}

FORCEINLINE
VOID
KiSetDpcInterruptRequested(
    _Inout_ PKPRCB Prcb)
{
    InterlockedOr(&Prcb->DpcRequestSummary, KI_DPC_NORMAL_PROCESSING_REQUESTED);
}

FORCEINLINE
VOID
KiClearDpcInterruptRequested(
    _Inout_ PKPRCB Prcb)
{
    InterlockedAnd(&Prcb->DpcRequestSummary, ~KI_DPC_NORMAL_PROCESSING_REQUESTED);
}

FORCEINLINE
VOID
KiSetDpcPresent(
    _Inout_ PKPRCB Prcb)
{
    InterlockedOr(&Prcb->DpcRequestSummary, KI_DPC_NORMAL_DPC_PRESENT);
}

FORCEINLINE
VOID
KiClearDpcRequestState(
    _Inout_ PKPRCB Prcb)
{
    InterlockedAnd(&Prcb->DpcRequestSummary, ~(KI_DPC_NORMAL_PROCESSING_REQUESTED | KI_DPC_NORMAL_DPC_PRESENT));
}

FORCEINLINE
BOOLEAN
KiIsDpcThreadActive(
    _In_ PKPRCB Prcb)
{
    return (Prcb->DpcRequestSummary & KI_DPC_THREAD_ACTIVE) != 0;
}

FORCEINLINE
BOOLEAN
KiIsDpcThreadRequested(
    _In_ PKPRCB Prcb)
{
    return (Prcb->DpcRequestSummary & KI_DPC_THREAD_REQUESTED) != 0;
}

FORCEINLINE
VOID
KiSetDpcThreadActive(
    _Inout_ PKPRCB Prcb)
{
    InterlockedOr(&Prcb->DpcRequestSummary, KI_DPC_THREAD_ACTIVE);
}

FORCEINLINE
VOID
KiSetDpcThreadRequested(
    _Inout_ PKPRCB Prcb)
{
    InterlockedOr(&Prcb->DpcRequestSummary, KI_DPC_THREAD_REQUESTED);
}

FORCEINLINE
VOID
KiClearDpcThreadState(
    _Inout_ PKPRCB Prcb)
{
    InterlockedAnd(&Prcb->DpcRequestSummary, ~(KI_DPC_THREAD_ACTIVE | KI_DPC_THREAD_REQUESTED));
}

#define KiPrcbDpcTimeCount(Prcb) ((Prcb)->DpcTimeCount)
#define KiPrcbDpcTimeLimit(Prcb) ((Prcb)->DpcTimeLimit)
#define KiPrcbDpcWatchdogCount(Prcb) ((Prcb)->DpcWatchdogCount)
#define KiPrcbDpcWatchdogPeriod(Prcb) ((Prcb)->DpcWatchdogPeriod)
#define KiClockTickSuspended(Prcb) FALSE

#define KiIsUserModeTrap(TrapFrame) ((TrapFrame)->PreviousMode == UserMode)

#define KiPrcbAffinity(Prcb) ((KAFFINITY)(Prcb)->GroupSetMember)
#define KiPrcbCoreSet(Prcb) ((Prcb)->CoreProcessorSet ? (KAFFINITY)(Prcb)->CoreProcessorSet : KiPrcbAffinity(Prcb))
#define KiPrcbIsCorePrimary(Prcb) (KiPrcbCoreIndex(Prcb) == (Prcb)->Number)
#define KiPrcbIsSleeping(Prcb) ((Prcb)->IdleHalt)
#define KiInitializeDpcThreadWait(Prcb) KeInitializeGate(&(Prcb)->DpcGate)
#define KiSignalDpcThread(Prcb) KeSignalGateBoostPriority(&(Prcb)->DpcGate)
#define KiWaitForDpcThread(Prcb) KeWaitForGate(&(Prcb)->DpcGate, Executive, KernelMode)
#define KiTakeDpcThreadSignal(Prcb) (InterlockedAnd(&(Prcb)->DpcRequestSummary, ~4L) & 4)

FORCEINLINE ULONG
KiPrcbCoreIndex(PKPRCB Prcb)
{
    ULONG Index = Prcb->Number;
    _BitScanForward(&Index, KiPrcbCoreSet(Prcb));
    return Index;
}

#define KiPrcbTimerPending KiArmTimerPending
#define KiPrcbRequestTimer KiArmRequestTimer
#define KiPrcbTakeTimer KiArmTakeTimer

FORCEINLINE BOOLEAN
KiArmTimerPending(PKPRCB Prcb)
{
    return (InterlockedCompareExchange(&Prcb->DpcRequestSummary, 0, 0) & 8) != 0;
}

FORCEINLINE VOID
KiArmRequestTimer(PKPRCB Prcb, PKTRAP_FRAME TrapFrame, ULONG Hand)
{
    UNREFERENCED_PARAMETER(TrapFrame);
    Prcb->LastTimerHand = Hand;
    InterlockedOr(&Prcb->DpcRequestSummary, 8);
}

FORCEINLINE ULONG_PTR
KiArmTakeTimer(PKPRCB Prcb)
{
    ULONG Hand = Prcb->LastTimerHand;
    InterlockedAnd(&Prcb->DpcRequestSummary, ~8L);
    return Hand;
}

FORCEINLINE ULONG64
ReadTimeStampCounter(VOID)
{
    #if defined(__clang__) || defined(__GNUC__)
    ULONG Low, High;
    __asm__ __volatile__("isb\n\tmrrc p15, 1, %0, %1, c14" : "=r"(Low), "=r"(High) :: "memory");
    return ((ULONG64)High << 32) | Low;
    #else
    __isb(0xf);
    return _MoveFromCoprocessor64(15, 1, 14);
    #endif
}

DECLSPEC_NORETURN VOID
KiExceptionExit(PKTRAP_FRAME TrapFrame, PKEXCEPTION_FRAME ExceptionFrame);

KIRQL NTAPI HalpArmGetInterruptIrql(ULONG Source);
KIRQL KiArmGetInterruptIrql(ULONG Source);
VOID NTAPI KiRetireDpcListInDpcStack(PKPRCB Prcb, PVOID DpcStack);
BOOLEAN NTAPI HalpArmDispatchInterrupt(ULONG Source, PKTRAP_FRAME TrapFrame);

NTSTATUS KiExpandKernelStackCallout(PEXPAND_STACK_CALLOUT Callout, PVOID Parameter,
                                     SIZE_T Size, BOOLEAN Wait);
VOID KiArmCallOnKernelStack(PEXPAND_STACK_CALLOUT Callout, PVOID Parameter, PVOID StackBase, ULONG Flags);
