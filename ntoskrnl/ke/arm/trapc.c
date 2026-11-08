/*
 * PROJECT:         ReactOS Kernel
 * LICENSE:         BSD - See COPYING.ARM in the top level directory
 * FILE:            ntoskrnl/ke/arm/trapc.c
 * PURPOSE:         Implements the various trap handlers for ARM exceptions
 * PROGRAMMERS:     ReactOS Portable Systems Group
 */

/* INCLUDES *******************************************************************/

#include <ntoskrnl.h>
#define NDEBUG
#include <debug.h>

/* FUNCTIONS ******************************************************************/

#if 0
DECLSPEC_NORETURN
VOID
KiIdleLoop(VOID)
{
    PKPCR Pcr = (PKPCR)KeGetPcr();
    PKPRCB Prcb = Pcr->Prcb;
    PKTHREAD OldThread, NewThread;

    //
    // Loop forever... that's why this is an idle loop
    //
    DPRINT1("[IDLE LOOP]\n");
    while (TRUE);

    while (TRUE)
    {
        //
        // Cycle interrupts
        //
        _disable();
        _enable();

        //
        // Check if there's DPC work to do
        //
        if ((Prcb->DpcData[0].DpcQueueDepth) ||
            (KiPrcbTimerPending(Prcb)) ||
            (Prcb->DeferredReadyListHead.Next))
        {
            //
            // Clear the pending interrupt
            //
            HalClearSoftwareInterrupt(DISPATCH_LEVEL);

            //
            // Retire DPCs
            //
            KiRetireDpcList(Prcb);
        }

        //
        // Check if there's a thread to schedule
        //
        if (Prcb->NextThread)
        {
            //
            // Out with the old, in with the new...
            //
            OldThread = Prcb->CurrentThread;
            NewThread = Prcb->NextThread;
            Prcb->CurrentThread = NewThread;
            Prcb->NextThread = NULL;

            //
            // Update thread state
            //
            NewThread->State = Running;

            //
            // Swap to the new thread
            // On ARM we call KiSwapContext instead of KiSwapContextInternal,
            // because we're calling this from C code and not assembly.
            // This is similar to how it gets called for unwaiting, on x86
            //
            KiSwapContext(OldThread, NewThread);
        }
        else
        {
            //
            // Go into WFI (sleep more)
            //
            KeArmWaitForInterrupt();
        }
    }
}
#endif


#if 0
BOOLEAN
KiSwapContextInternal(IN PKTHREAD OldThread,
                      IN PKTHREAD NewThread)
{
    PKIPCR Pcr = (PKIPCR)KeGetPcr();
    PKPRCB Prcb = Pcr->Prcb;
    PKPROCESS OldProcess, NewProcess;

    DPRINT1("SWAP\n");
    while (TRUE);

    //
    // Increase context switch count
    //
    Pcr->ContextSwitches++;

    //
    // Check if WMI tracing is enabled
    //
    if (Pcr->PerfGlobalGroupMask)
    {
        //
        // We don't support this yet on x86 either
        //
        DPRINT1("WMI Tracing not supported\n");
        ASSERT(FALSE);
    }

    //
    // Check if the processes are also different
    //
    OldProcess = OldThread->ApcState.Process;
    NewProcess = NewThread->ApcState.Process;
    if (OldProcess != NewProcess)
    {
        //
        // Check if address space switch is needed
        //
        if (OldProcess->DirectoryTableBase[0] !=
            NewProcess->DirectoryTableBase)
        {
            //
            // FIXME-USER: Support address space switch
            //
            DPRINT1("Address space switch not implemented\n");
            ASSERT(FALSE);
        }
    }

    //
    // Increase thread context switches
    //
    NewThread->ContextSwitches++;
#if 0 // I don't buy this
    //
    // Set us as the current thread
    // NOTE: On RISC Platforms, there is both a KPCR CurrentThread, and a
    // KPRCB CurrentThread.
    // The latter is set just like on x86-based builds, the former is only set
    // when actually doing the context switch (here).
    // Recall that the reason for the latter is due to the fact that the KPCR
    // is shared with user-mode (read-only), so that information is exposed
    // there as well.
    //
    Pcr->CurrentThread = NewThread;
#endif
    //
    // DPCs shouldn't be active
    //
    if (Prcb->DpcRoutineActive)
    {
        //
        // Crash the machine
        //
        KeBugCheckEx(ATTEMPTED_SWITCH_FROM_DPC,
                     (ULONG_PTR)OldThread,
                     (ULONG_PTR)NewThread,
                     (ULONG_PTR)OldThread->InitialStack,
                     0);
    }

    //
    // Kernel APCs may be pending
    //
    if (NewThread->ApcState.KernelApcPending)
    {
        //
        // Are APCs enabled?
        //
        if (NewThread->SpecialApcDisable == 0)
        {
            //
            // Request APC delivery
            //
            HalRequestSoftwareInterrupt(APC_LEVEL);
            return TRUE;
        }
    }

    //
    // Return
    //
    return FALSE;
}
#endif


#if 0
VOID
KiDispatchInterrupt(VOID)
{
    PKIPCR Pcr;
    PKPRCB Prcb;
    PKTHREAD NewThread, OldThread;

    DPRINT1("[DPC TRAP]\n");
    while (TRUE);

    //
    // Get the PCR and disable interrupts
    //
    Pcr = (PKIPCR)KeGetPcr();
    Prcb = Pcr->Prcb;
    _disable();

    //
    //Check if we have to deliver DPCs, timers, or deferred threads
    //
    if ((Prcb->DpcData[0].DpcQueueDepth) ||
        (KiPrcbTimerPending(Prcb)) ||
        (Prcb->DeferredReadyListHead.Next))
    {
        //
        // Retire DPCs
        //
        KiRetireDpcList(Prcb);
    }

    //
    // Re-enable interrupts
    //
    _enable();

    //
    // Check for quantum end
    //
    if (Prcb->QuantumEnd)
    {
        //
        // Handle quantum end
        //
        Prcb->QuantumEnd = FALSE;
        KiQuantumEnd();
        return;
    }

    //
    // Check if we have a thread to swap to
    //
    if (Prcb->NextThread)
    {
        //
        // Next is now current
        //
        OldThread = Prcb->CurrentThread;
        NewThread = Prcb->NextThread;
        Prcb->CurrentThread = NewThread;
        Prcb->NextThread = NULL;

        //
        // Update thread states
        //
        NewThread->State = Running;
        OldThread->WaitReason = WrDispatchInt;

        //
        // Make the old thread ready
        //
        KxQueueReadyThread(OldThread, Prcb);

        //
        // Swap to the new thread
        // On ARM we call KiSwapContext instead of KiSwapContextInternal,
        // because we're calling this from C code and not assembly.
        // This is similar to how it gets called for unwaiting, on x86
        //
        KiSwapContext(OldThread, NewThread);
    }
}
#endif


VOID KiArmDebugService(PKTRAP_FRAME Trap, ULONG Argument4);

NTSTATUS
KiUndefinedExceptionHandler(IN PKTRAP_FRAME TrapFrame)
{
    EXCEPTION_RECORD Record = {0};
    KPROCESSOR_MODE Mode = KiGetPreviousMode(TrapFrame);
    PKEXCEPTION_FRAME Exception = (PKEXCEPTION_FRAME)TrapFrame - 1;
    ULONG Argument4;

    if ((TrapFrame->Cpsr & CPSRC_THUMB) && *(PUSHORT)(TrapFrame->Pc & ~1UL) == 0xDEFD)
    {
        _SEH2_TRY
        {
            if (Mode == UserMode)
                ProbeForRead((PVOID)TrapFrame->Sp, sizeof(ULONG), sizeof(ULONG));
            Argument4 = *(PULONG)TrapFrame->Sp;
            KiArmDebugService(TrapFrame, Argument4);
        }
        _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
        {
            TrapFrame->R0 = _SEH2_GetExceptionCode();
            TrapFrame->Pc += sizeof(USHORT);
        }
        _SEH2_END;
        return STATUS_SUCCESS;
    }
    Record.ExceptionCode = STATUS_ILLEGAL_INSTRUCTION;
    if ((TrapFrame->Cpsr & CPSRC_THUMB) && *(PUSHORT)(TrapFrame->Pc & ~1UL) == 0xDEFE)
    {
        Record.ExceptionCode = STATUS_BREAKPOINT;
        Record.NumberParameters = 1;
        Record.ExceptionInformation[0] = BREAKPOINT_BREAK;
    }
    Record.ExceptionAddress = (PVOID)(TrapFrame->Pc & ~1UL);
    KiDispatchException(&Record, Exception, TrapFrame, Mode, TRUE);
    return STATUS_SUCCESS;
}
