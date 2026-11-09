/*
 * PROJECT:         ReactOS Kernel
 * LICENSE:         BSD - See COPYING.ARM in the top level directory
 * FILE:            ntoskrnl/ke/arm/thrdini.c
 * PURPOSE:         Implements thread context setup and startup for ARM machines
 * PROGRAMMERS:     ReactOS Portable Systems Group
 */

/* INCLUDES *******************************************************************/

#include <ntoskrnl.h>
#define NDEBUG
#include <debug.h>

/* GLOBALS ********************************************************************/

typedef struct _ARM_SWITCH_FRAME
{
    KSWITCH_FRAME Switch;
    KEXCEPTION_FRAME Exception;
    KARM_VFP_STATE Vfp;
} ARM_SWITCH_FRAME, *PARM_SWITCH_FRAME;

typedef struct _KUINIT_FRAME
{
    ARM_SWITCH_FRAME ContextSwitch;
    KARM_VFP_STATE Vfp;
    KEXCEPTION_FRAME Exception;
    KTRAP_FRAME Trap;
} KUINIT_FRAME, *PKUINIT_FRAME;

VOID NTAPI KiThreadStartup(VOID);
VOID KiArmSetThreadTeb(PKTHREAD Thread);

VOID
KiArmInitializeSwitchFrame(PVOID Frame, PKSYSTEM_ROUTINE SystemRoutine,
                           PKSTART_ROUTINE StartRoutine, PVOID StartContext,
                           PCONTEXT Context);

VOID NTAPI
KiInitializeContextThread(PKTHREAD Thread, PKSYSTEM_ROUTINE SystemRoutine,
                          PKSTART_ROUTINE StartRoutine, PVOID StartContext,
                          PCONTEXT Context)
{
    PARM_SWITCH_FRAME Switch;

    if (Context)
    {
        PKUINIT_FRAME Frame = (PVOID)ALIGN_DOWN_BY((ULONG_PTR)Thread->InitialStack - sizeof(KUINIT_FRAME), 8);
        RtlZeroMemory(Frame, sizeof(*Frame));
        Frame->Trap.VfpState = &Frame->Vfp;
        Frame->Vfp.Fpscr = Context->Fpscr;
        RtlCopyMemory(Frame->Vfp.VfpD, Context->D, sizeof(Frame->Vfp.VfpD));
        KeContextToTrapFrame(Context, &Frame->Exception, &Frame->Trap,
                             Context->ContextFlags | CONTEXT_CONTROL, UserMode);
        Thread->PreviousMode = UserMode;
        Thread->TrapFrame = &Frame->Trap;
        Switch = &Frame->ContextSwitch;
    }
    else
    {
        Switch = (PVOID)ALIGN_DOWN_BY((ULONG_PTR)Thread->InitialStack - sizeof(*Switch), 8);
        Thread->PreviousMode = KernelMode;
        Thread->TrapFrame = NULL;
    }
    KiArmInitializeSwitchFrame(Switch, SystemRoutine, StartRoutine, StartContext, Context);
    Thread->KernelStack = Switch;
}

DECLSPEC_NORETURN
VOID
KiIdleLoop(VOID)
{
    PKPRCB Prcb = KeGetCurrentPrcb();
    PKTHREAD OldThread, NewThread;

    /* Now loop forever */
    while (TRUE)
    {
        /* Start of the idle loop: disable interrupts */
        _enable();
        YieldProcessor();
        YieldProcessor();
        _disable();

        /* Check for pending timers, pending DPCs, or pending ready threads */
        if ((Prcb->DpcData[0].DpcQueueDepth) ||
            (KiPrcbTimerPending(Prcb)) ||
            (Prcb->DeferredReadyListHead.Next))
        {
            /* Quiesce the DPC software interrupt */
            HalClearSoftwareInterrupt(DISPATCH_LEVEL);

            /* Handle it */
            KiRetireDpcList(Prcb);
        }

        KiAcquirePrcbLock(Prcb);
        NewThread = Prcb->NextThread;
        if (NewThread != NULL)
        {
            Prcb->NextThread = NULL;
        }
#ifdef CONFIG_SMP
        else
        {
            NewThread = KiIdleSchedule(Prcb);
            if (NewThread == Prcb->IdleThread)
            {
                NewThread = Prcb->NextThread;
                if (NewThread != NULL)
                    Prcb->NextThread = NULL;
            }
        }
#endif

        if (NewThread != NULL)
        {
            OldThread = Prcb->CurrentThread;
#ifdef CONFIG_SMP
            KfRaiseIrql(SYNCH_LEVEL);
            InterlockedBitTestAndResetAffinity(&KiIdleSummary, Prcb->Number);
#endif
            Prcb->CurrentThread = NewThread;
            NewThread->State = Running;
            KiReleasePrcbLock(Prcb);

            _enable();
            KiSwapContext(APC_LEVEL, OldThread);

#ifdef CONFIG_SMP
            KeLowerIrql(DISPATCH_LEVEL);
#endif
        }
        else
        {
            KiReleasePrcbLock(Prcb);
            /* Continue staying idle. Note the HAL returns with interrupts on */
            Prcb->PowerState.IdleFunction(&Prcb->PowerState);
        }
    }
}

BOOLEAN
KiArmSwapContextResume(KIRQL WaitIrql, PKTHREAD OldThread, PKTHREAD NewThread)
{
    PKPRCB Prcb = KeGetCurrentPrcb();

    KiChargeThreadCycleTime(Prcb, OldThread);
    NewThread->ContextSwitches++;
    Prcb->KeContextSwitches++;
    if (Prcb->DpcRoutineActive)
        KeBugCheckEx(ATTEMPTED_SWITCH_FROM_DPC, (ULONG_PTR)OldThread,
                     (ULONG_PTR)NewThread, (ULONG_PTR)OldThread->InitialStack, 0);
    if (NewThread->ApcState.Process != OldThread->ApcState.Process)
        KiSwapProcess(NewThread->ApcState.Process, OldThread->ApcState.Process);
    KiArmSetThreadTeb(NewThread);
    _enable();
    KiCompleteThreadSwitch(OldThread, NewThread);
    _disable();
    if (NewThread->ApcState.KernelApcPending &&
        !NewThread->SpecialApcDisable && WaitIrql == PASSIVE_LEVEL)
        return TRUE;
    if (NewThread->ApcState.KernelApcPending)
        HalRequestSoftwareInterrupt(APC_LEVEL);
    return FALSE;
}

VOID
NTAPI
KiDispatchInterrupt(VOID)
{
    PKIPCR Pcr = (PKIPCR)KeGetPcr();
    PKPRCB Prcb = &Pcr->Prcb;
    PKTHREAD NewThread, OldThread;

    /* Disable interrupts */
    _disable();

    /* Check for pending timers, pending DPCs, or pending ready threads */
    if ((Prcb->DpcData[0].DpcQueueDepth) ||
        (KiPrcbTimerPending(Prcb)) ||
        (Prcb->DeferredReadyListHead.Next))
    {
        /* Retire DPCs while under the DPC stack */
        KiRetireDpcListInDpcStack(Prcb, Prcb->DpcStack);
    }

    /* Re-enable interrupts */
    _enable();

    /* Check for quantum end */
    if (Prcb->QuantumEnd)
    {
        /* Handle quantum end */
        Prcb->QuantumEnd = FALSE;
        KiQuantumEnd();
    }
    else if (Prcb->NextThread)
    {
        /* Acquire the PRCB lock */
        KiAcquirePrcbLock(Prcb);

        /* Capture current thread data */
        OldThread = Prcb->CurrentThread;
        NewThread = Prcb->NextThread;

        /* Set new thread data */
        Prcb->NextThread = NULL;
        Prcb->CurrentThread = NewThread;

        /* The thread is now running */
        NewThread->State = Running;
        OldThread->WaitReason = WrDispatchInt;

        /* Make the old thread ready */
        KxQueueReadyThread(OldThread, Prcb);

        /* Swap to the new thread */
        KiSwapContext(APC_LEVEL, OldThread);
    }
}

/* EOF */
