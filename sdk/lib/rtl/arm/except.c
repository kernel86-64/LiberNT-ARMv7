/*
 * PROJECT:     LiberNT Run-Time Library
 * LICENSE:     LGPL-2.1-or-later (https://spdx.org/licenses/LGPL-2.1-or-later)
 * PURPOSE:     ARM32 exception dispatch using PE unwind metadata
 * COPYRIGHT:   Adapted from the LiberNT ARM64 exception dispatcher
 */

#include <rtl.h>
#include <intrin.h>

#define NDEBUG
#include <debug.h>

typedef struct _ARM_NONVOLATILE_REGISTERS
{
    ULONG GpNvRegs[8];
    ULONGLONG FpNvRegs[8];
} ARM_NONVOLATILE_REGISTERS;

VOID NTAPI
RtlpArmRestoreCollidedFrame(PDISPATCHER_CONTEXT DispatcherContext,
                            PCONTEXT UnwindContext, PVOID NonVolatileRegisters,
                            ULONG NonVolatileRegistersLength, PULONG EstablisherFrame);

PRTLP_DISPATCH_EXCEPTION_HOOK RtlpDispatchExceptionHook;

static VOID RtlpArmCaptureNonVolatileRegisters(_Out_ ARM_NONVOLATILE_REGISTERS *NonVolatileRegisters, _In_ PCONTEXT Context)
{
    RtlCopyMemory(NonVolatileRegisters->GpNvRegs, &Context->R4, sizeof(NonVolatileRegisters->GpNvRegs));
    RtlCopyMemory(NonVolatileRegisters->FpNvRegs, &Context->D[8], sizeof(NonVolatileRegisters->FpNvRegs));
}

#define ARM_HANDLER_FRAME_DISPATCHER_OFFSET 8

EXCEPTION_DISPOSITION
NTAPI
RtlpExceptionHandler(_In_ PEXCEPTION_RECORD ExceptionRecord, _In_ PVOID EstablisherFrame, _Inout_ PCONTEXT ContextRecord, _Inout_ PVOID DispatcherContext)
{
    PDISPATCHER_CONTEXT NestedDispatcherContext = (PDISPATCHER_CONTEXT)DispatcherContext;
    PDISPATCHER_CONTEXT ActiveDispatcherContext;

    UNREFERENCED_PARAMETER(ContextRecord);

    if (ExceptionRecord->ExceptionFlags & EXCEPTION_UNWIND)
    {
        return ExceptionContinueSearch;
    }

    ActiveDispatcherContext = *(PDISPATCHER_CONTEXT *)((PUCHAR)EstablisherFrame - ARM_HANDLER_FRAME_DISPATCHER_OFFSET);
    NestedDispatcherContext->EstablisherFrame = ActiveDispatcherContext->EstablisherFrame;
    return ExceptionNestedException;
}

EXCEPTION_DISPOSITION
NTAPI
RtlpUnwindHandler(_In_ PEXCEPTION_RECORD ExceptionRecord, _In_ PVOID EstablisherFrame, _Inout_ PCONTEXT ContextRecord, _Inout_ PVOID DispatcherContext)
{
    PDISPATCHER_CONTEXT CollidedDispatcherContext = (PDISPATCHER_CONTEXT)DispatcherContext;
    PDISPATCHER_CONTEXT ActiveDispatcherContext;
    ULONG TargetPc;

    UNREFERENCED_PARAMETER(ExceptionRecord);
    UNREFERENCED_PARAMETER(ContextRecord);

    ActiveDispatcherContext = *(PDISPATCHER_CONTEXT *)((PUCHAR)EstablisherFrame - ARM_HANDLER_FRAME_DISPATCHER_OFFSET);
    TargetPc = CollidedDispatcherContext->TargetPc;
    *CollidedDispatcherContext = *ActiveDispatcherContext;
    CollidedDispatcherContext->TargetPc = TargetPc;
    return ExceptionCollidedUnwind;
}

VOID
NTAPI
RtlpStepContextToCaller(_Inout_ PCONTEXT Context)
{
    PRUNTIME_FUNCTION FunctionEntry;
    ULONG_PTR ImageBase;
    ULONG EstablisherFrame;
    PVOID HandlerData;

    FunctionEntry = RtlLookupFunctionEntry(Context->Pc, &ImageBase, NULL);
    if (FunctionEntry == NULL)
    {
        return;
    }

    RtlVirtualUnwind(UNW_FLAG_NHANDLER, ImageBase, Context->Pc, FunctionEntry, Context, &HandlerData, &EstablisherFrame, NULL);
}

VOID
NTAPI
RtlpArmRestoreCollidedFrame(_Inout_ PDISPATCHER_CONTEXT DispatcherContext, _Out_ PCONTEXT UnwindContext, _Out_writes_bytes_(NonVolatileRegistersLength) PVOID NonVolatileRegisters, _In_ ULONG NonVolatileRegistersLength, _Out_ PULONG EstablisherFrame)
{
    ULONG_PTR LookupPc;
    ULONG UnwoundFrame;
    PVOID HandlerData;

    *UnwindContext = *DispatcherContext->ContextRecord;
    RtlCopyMemory(NonVolatileRegisters, DispatcherContext->NonVolatileRegisters, NonVolatileRegistersLength);

    LookupPc = (ULONG_PTR)DispatcherContext->ControlPc;
    if (DispatcherContext->ControlPcIsUnwound)
    {
        LookupPc -= 2;
    }

    RtlVirtualUnwind(UNW_FLAG_NHANDLER, DispatcherContext->ImageBase, LookupPc, DispatcherContext->FunctionEntry, UnwindContext, &HandlerData, &UnwoundFrame, NULL);

    DispatcherContext->NonVolatileRegisters = (PBYTE)NonVolatileRegisters;
    *EstablisherFrame = DispatcherContext->EstablisherFrame;
}

VOID
NTAPI
RtlGetCallersAddress(
    _Out_ PVOID *CallersAddress,
    _Out_ PVOID *CallersCaller)
{
    PVOID Callers[4];
    ULONG Number;

    Number = RtlWalkFrameChain(Callers, 4, 0);
    *CallersAddress = (Number >= 3) ? Callers[2] : NULL;
    *CallersCaller = (Number == 4) ? Callers[3] : NULL;
}

BOOLEAN
NTAPI
RtlDispatchException(
    _In_ PEXCEPTION_RECORD ExceptionRecord,
    _In_ PCONTEXT ContextRecord)
{
    CONTEXT UnwindContext;
    ULONG_PTR ImageBase;
    PRUNTIME_FUNCTION FunctionEntry;
    PEXCEPTION_ROUTINE ExceptionRoutine;
    DISPATCHER_CONTEXT DispatcherContext;
    ARM_NONVOLATILE_REGISTERS NonVolatileRegisters;
    PVOID HandlerData;
    EXCEPTION_DISPOSITION Disposition;
    ULONG PreviousSp;
    ULONG EstablisherFrame;
    ULONG NestedFrame;
    ULONG_PTR StackLow;
    ULONG_PTR StackHigh;
    ULONG_PTR LookupPc;
    ULONG ControlPc;
    BOOLEAN ControlPcIsUnwound;
    BOOLEAN Handled;

    if (RtlpDispatchExceptionHook != NULL &&
        RtlpDispatchExceptionHook(ExceptionRecord, ContextRecord, &Handled))
    {
        return Handled;
    }

    if (RtlCallVectoredExceptionHandlers(ExceptionRecord, ContextRecord))
    {
        RtlCallVectoredContinueHandlers(ExceptionRecord, ContextRecord);
        return TRUE;
    }

    UnwindContext = *ContextRecord;
    if (ExceptionRecord->ExceptionFlags & EXCEPTION_SOFTWARE_ORIGINATE)
        UnwindContext.ContextFlags |= CONTEXT_UNWOUND_TO_CALL;
    NestedFrame = 0;
    RtlpGetStackLimits(&StackLow, &StackHigh);

    for (;;)
    {
        ImageBase = 0;
        ControlPc = UnwindContext.Pc;
        ControlPcIsUnwound = !!(UnwindContext.ContextFlags & CONTEXT_UNWOUND_TO_CALL);
        LookupPc = ControlPcIsUnwound ? (ControlPc - 2) : ControlPc;
        FunctionEntry = RtlLookupFunctionEntry(LookupPc,
                                               (PULONG_PTR)&ImageBase,
                                               NULL);
        if (FunctionEntry == NULL)
        {
            if ((UnwindContext.Lr == 0) ||
                (UnwindContext.Lr == UnwindContext.Pc))
            {
                break;
            }

            UnwindContext.Pc = UnwindContext.Lr;
            UnwindContext.ContextFlags |= CONTEXT_UNWOUND_TO_CALL;
            continue;
        }

        EstablisherFrame = 0;
        PreviousSp = UnwindContext.Sp;
        RtlpArmCaptureNonVolatileRegisters(&NonVolatileRegisters, &UnwindContext);
        ExceptionRoutine = RtlVirtualUnwind(UNW_FLAG_EHANDLER,
                                            (ULONG)ImageBase,
                                            LookupPc,
                                            FunctionEntry,
                                            &UnwindContext,
                                            &HandlerData,
                                            &EstablisherFrame,
                                            NULL);

        if ((EstablisherFrame < StackLow) ||
            (EstablisherFrame > StackHigh) ||
            (EstablisherFrame & (8 - 1)) ||
            ((UnwindContext.Sp == PreviousSp) && (UnwindContext.Pc == ControlPc)))
        {
            ExceptionRecord->ExceptionFlags |= EXCEPTION_STACK_INVALID;
            break;
        }

        if (ExceptionRoutine != NULL)
        {
            RtlZeroMemory(&DispatcherContext, sizeof(DispatcherContext));
            DispatcherContext.ControlPc = ControlPc;
            DispatcherContext.ImageBase = ImageBase;
            DispatcherContext.FunctionEntry = FunctionEntry;
            DispatcherContext.EstablisherFrame = EstablisherFrame;
            DispatcherContext.ContextRecord = &UnwindContext;
            DispatcherContext.LanguageHandler = ExceptionRoutine;
            DispatcherContext.HandlerData = HandlerData;
            DispatcherContext.ScopeIndex = 0;
            DispatcherContext.ControlPcIsUnwound = ControlPcIsUnwound;
            DispatcherContext.NonVolatileRegisters = (PBYTE)&NonVolatileRegisters;

            do
            {
                Disposition = RtlpExecuteHandlerForException(ExceptionRecord, (PVOID)(ULONG_PTR)EstablisherFrame, ContextRecord, &DispatcherContext, DispatcherContext.LanguageHandler);

                if (EstablisherFrame == NestedFrame)
                {
                    ExceptionRecord->ExceptionFlags &= ~EXCEPTION_NESTED_CALL;
                    NestedFrame = 0;
                }

                if (Disposition == ExceptionContinueExecution)
                {
                    if (ExceptionRecord->ExceptionFlags & EXCEPTION_NONCONTINUABLE)
                    {
                        RtlRaiseStatus(STATUS_NONCONTINUABLE_EXCEPTION);
                    }

                    RtlCallVectoredContinueHandlers(ExceptionRecord, ContextRecord);
                    return TRUE;
                }

                if (Disposition == ExceptionNestedException)
                {

                    ExceptionRecord->ExceptionFlags |= EXCEPTION_NESTED_CALL;
                    if (DispatcherContext.EstablisherFrame > NestedFrame)
                    {
                        NestedFrame = DispatcherContext.EstablisherFrame;
                    }
                }
                else if (Disposition == ExceptionCollidedUnwind)
                {
                    RtlpArmRestoreCollidedFrame(&DispatcherContext, &UnwindContext, (PBYTE)&NonVolatileRegisters, sizeof(NonVolatileRegisters), &EstablisherFrame);
                    DispatcherContext.ContextRecord = &UnwindContext;
                }
            }
            while (Disposition == ExceptionCollidedUnwind);

            if ((Disposition != ExceptionContinueSearch) &&
                (Disposition != ExceptionNestedException))
            {
                break;
            }
        }

        if (UnwindContext.Pc == 0)
        {
            break;
        }
    }

    RtlCallVectoredContinueHandlers(ExceptionRecord, ContextRecord);
    return FALSE;
}

VOID
NTAPI
RtlInitializeContext(
    _In_ HANDLE ProcessHandle,
    _Out_ PCONTEXT ThreadContext,
    _In_opt_ PVOID ThreadStartParam,
    _In_ PTHREAD_START_ROUTINE ThreadStartAddress,
    _In_ PINITIAL_TEB InitialStack)
{
    UNREFERENCED_PARAMETER(ProcessHandle);

    RtlZeroMemory(ThreadContext, sizeof(*ThreadContext));

    ThreadContext->ContextFlags = CONTEXT_FULL;
    ThreadContext->Pc = (ULONG_PTR)ThreadStartAddress | 1;
    ThreadContext->Sp = ALIGN_DOWN_BY((ULONG_PTR)InitialStack->StackBase, 8);
    ThreadContext->Cpsr = 0x30;
    ThreadContext->R0 = (ULONG)ThreadStartParam;
    ThreadContext->Lr = (ULONG)RtlExitUserThread;
}

VOID
NTAPI
RtlUnwindEx(
    _In_opt_ PVOID TargetFrame,
    _In_opt_ PVOID TargetIp,
    _In_opt_ PEXCEPTION_RECORD ExceptionRecord,
    _In_ PVOID ReturnValue,
    _In_ PCONTEXT ContextRecord,
    _In_opt_ PUNWIND_HISTORY_TABLE HistoryTable)
{
    EXCEPTION_RECORD LocalExceptionRecord;
    DISPATCHER_CONTEXT DispatcherContext;
    ARM_NONVOLATILE_REGISTERS NonVolatileRegisters;
    PEXCEPTION_ROUTINE ExceptionRoutine;
    EXCEPTION_DISPOSITION Disposition;
    PRUNTIME_FUNCTION FunctionEntry;
    ULONG ImageBase, EstablisherFrame;
    CONTEXT UnwindContext;
    ULONG_PTR StackLow, StackHigh;
    ULONG_PTR LookupPc;
    ULONG ControlPc;
    BOOLEAN ControlPcIsUnwound;
    ULONG PreviousSp;
    BOOLEAN HaveTarget;

    if (ContextRecord == NULL)
        return;

    RtlCaptureContext(ContextRecord);

    if (ExceptionRecord == NULL)
    {
        RtlZeroMemory(&LocalExceptionRecord, sizeof(LocalExceptionRecord));
        LocalExceptionRecord.ExceptionCode = STATUS_UNWIND;
        LocalExceptionRecord.ExceptionAddress = (PVOID)(ULONG_PTR)ContextRecord->Pc;
        ExceptionRecord = &LocalExceptionRecord;
    }

    ExceptionRecord->ExceptionFlags |= EXCEPTION_UNWINDING;
    if (TargetFrame == NULL)
        ExceptionRecord->ExceptionFlags |= EXCEPTION_EXIT_UNWIND;

    HaveTarget = (TargetFrame != NULL) || (TargetIp != NULL);
    UnwindContext = *ContextRecord;
    RtlZeroMemory(&DispatcherContext, sizeof(DispatcherContext));
    DispatcherContext.ContextRecord = ContextRecord;
    DispatcherContext.HistoryTable = HistoryTable;
    DispatcherContext.TargetPc = (ULONG)(ULONG_PTR)TargetIp;
    DispatcherContext.NonVolatileRegisters = (PBYTE)&NonVolatileRegisters;

    RtlpGetStackLimits(&StackLow, &StackHigh);

    for (;;)
    {
        if ((UnwindContext.Sp < StackLow) ||
            (UnwindContext.Sp > StackHigh) ||
            (UnwindContext.Sp & (8 - 1)))
        {
            ExceptionRecord->ExceptionFlags |= EXCEPTION_STACK_INVALID;
            return;
        }

        ControlPc = UnwindContext.Pc;
        ControlPcIsUnwound = !!(UnwindContext.ContextFlags & CONTEXT_UNWOUND_TO_CALL);
        LookupPc = ControlPcIsUnwound ? (ControlPc - 2) : ControlPc;
        FunctionEntry = RtlLookupFunctionEntry(LookupPc, &ImageBase, NULL);
        if (FunctionEntry == NULL)
        {
            if ((UnwindContext.Lr == 0) ||
                (UnwindContext.Lr == UnwindContext.Pc))
            {
                break;
            }

            UnwindContext.Pc = UnwindContext.Lr;
            UnwindContext.ContextFlags |= CONTEXT_UNWOUND_TO_CALL;
            continue;
        }

        DispatcherContext.ControlPc = ControlPc;
        DispatcherContext.ControlPcIsUnwound = ControlPcIsUnwound;
        PreviousSp = UnwindContext.Sp;
        RtlpArmCaptureNonVolatileRegisters(&NonVolatileRegisters, &UnwindContext);
        ExceptionRoutine = RtlVirtualUnwind(UNW_FLAG_UHANDLER,
                                            ImageBase,
                                            LookupPc,
                                            FunctionEntry,
                                            &UnwindContext,
                                            &DispatcherContext.HandlerData,
                                            &EstablisherFrame,
                                            NULL);

        if ((EstablisherFrame < StackLow) ||
            (EstablisherFrame >= StackHigh) ||
            (EstablisherFrame & (8 - 1)) ||
            ((UnwindContext.Sp == PreviousSp) && (UnwindContext.Pc == ControlPc)))
        {
            ExceptionRecord->ExceptionFlags |= EXCEPTION_STACK_INVALID;
            return;
        }

        if (TargetFrame && EstablisherFrame > (ULONG_PTR)TargetFrame)
            RtlRaiseStatus(STATUS_INVALID_UNWIND_TARGET);

        if (ExceptionRoutine != NULL)
        {
            DispatcherContext.ImageBase = ImageBase;
            DispatcherContext.FunctionEntry = FunctionEntry;
            DispatcherContext.LanguageHandler = ExceptionRoutine;
            DispatcherContext.EstablisherFrame = EstablisherFrame;
            DispatcherContext.ScopeIndex = 0;

            do
            {
                if (HaveTarget && (EstablisherFrame == (ULONG)(ULONG_PTR)TargetFrame))
                    ExceptionRecord->ExceptionFlags |= EXCEPTION_TARGET_UNWIND;

                Disposition = RtlpExecuteHandlerForUnwind(ExceptionRecord, (PVOID)EstablisherFrame, ContextRecord, &DispatcherContext, DispatcherContext.LanguageHandler);

                ExceptionRecord->ExceptionFlags &= ~(EXCEPTION_TARGET_UNWIND |
                                                     EXCEPTION_COLLIDED_UNWIND);

                if (Disposition == ExceptionContinueExecution)
                {
                    if (ExceptionRecord->ExceptionFlags & EXCEPTION_NONCONTINUABLE)
                        RtlRaiseStatus(STATUS_NONCONTINUABLE_EXCEPTION);
                    return;
                }

                if (Disposition == ExceptionCollidedUnwind)
                {

                    *ContextRecord = *DispatcherContext.ContextRecord;
                    RtlpArmRestoreCollidedFrame(&DispatcherContext, &UnwindContext, (PBYTE)&NonVolatileRegisters, sizeof(NonVolatileRegisters), &EstablisherFrame);
                    DispatcherContext.ContextRecord = ContextRecord;
                    ExceptionRecord->ExceptionFlags |= EXCEPTION_COLLIDED_UNWIND;
                }
                else if (Disposition != ExceptionContinueSearch)
                {
                    RtlRaiseStatus(STATUS_INVALID_DISPOSITION);
                }
            }
            while (Disposition == ExceptionCollidedUnwind);
        }

        if (HaveTarget && (EstablisherFrame == (ULONG)(ULONG_PTR)TargetFrame))
        {
            if ((TargetIp != NULL) &&
                (ExceptionRecord->ExceptionCode != STATUS_UNWIND_CONSOLIDATE))
            {
                ContextRecord->Pc = (ULONG)(ULONG_PTR)TargetIp;
            }
            else if ((ExceptionRecord->ExceptionCode == STATUS_UNWIND_CONSOLIDATE) &&
                     (ExceptionRecord->ExceptionInformation[10] == (ULONG_PTR)-1))
            {
                ExceptionRecord->ExceptionInformation[10] = (ULONG_PTR)&NonVolatileRegisters;
            }

            ContextRecord->R0 = (ULONG)(ULONG_PTR)ReturnValue;
            RtlRestoreContext(ContextRecord, ExceptionRecord);
        }

        *ContextRecord = UnwindContext;
    }
    RtlRaiseStatus(STATUS_INVALID_UNWIND_TARGET);
}

VOID
NTAPI
RtlUnwind(
    _In_opt_ PVOID TargetFrame,
    _In_opt_ PVOID TargetIp,
    _In_opt_ PEXCEPTION_RECORD ExceptionRecord,
    _In_ PVOID ReturnValue)
{
    CONTEXT ContextRecord;

    RtlCaptureContext(&ContextRecord);
    RtlUnwindEx(TargetFrame,
                TargetIp,
                ExceptionRecord,
                ReturnValue,
                &ContextRecord,
                NULL);
}
