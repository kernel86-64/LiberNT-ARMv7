/*
 * PROJECT:     LiberNT Kernel
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARMv7 software interrupt delivery
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ntoskrnl.h>

VOID KiApcInterrupt(VOID)
{
    PKTRAP_FRAME Trap = KeGetCurrentThread()->TrapFrame;
    PKEXCEPTION_FRAME Exception = (PKEXCEPTION_FRAME)Trap - 1;

    HalClearSoftwareInterrupt(APC_LEVEL);
    KiDeliverApc(KiGetPreviousMode(Trap), Exception, Trap);
}

VOID KiInterruptHandler(PKTRAP_FRAME TrapFrame, ULONG Reserved)
{
    ULONG Source = HalGetInterruptSource();
    KIRQL OldIrql = KeGetCurrentIrql();
    KIRQL Level;
    PKTHREAD Thread;
    PKTRAP_FRAME OldTrap;
    BOOLEAN Handled;

    UNREFERENCED_PARAMETER(Reserved);

    if (Source >= 1020)
        return;
    Level = KiArmGetInterruptIrql(Source);
    if (Level < OldIrql)
    {
        if (Source <= 1)
        {
            HalEndSystemInterrupt(OldIrql, TrapFrame);
            return;
        }
    }
    KeGetCurrentPrcb()->InterruptCount++;
    TrapFrame->PreviousIrql = OldIrql;
    KfRaiseIrql(Level);
    Thread = KeGetCurrentThread();
    OldTrap = Thread->TrapFrame;
    Thread->TrapFrame = TrapFrame;

    if (Source <= 1)
    {
        HalClearSoftwareInterrupt(Level);
        HalEndSystemInterrupt(Level, TrapFrame);
        _enable();
        if (Source == 0)
        {
            KiClearDpcInterruptRequested(KeGetCurrentPrcb());
            KiDispatchInterrupt();
        }
        else
            KiApcInterrupt();
        _disable();
    }
    else if (Source == KI_ARM_SGI_IPI)
    {
        KiIpiServiceRoutine(TrapFrame, NULL);
        HalEndSystemInterrupt(Level, TrapFrame);
    }
    else if (Source == KI_ARM_SGI_CLOCK)
    {
        HalClearSoftwareInterrupt(Level);
        KeUpdateRunTime(TrapFrame, OldIrql);
        HalEndSystemInterrupt(Level, TrapFrame);
    }
    else
    {
        Handled = HalpArmDispatchInterrupt(Source, TrapFrame);
        if (!Handled)
            Handled = KeDispatchSecondaryInterrupt(Source, 0, NULL);
        HalEndSystemInterrupt(Level, TrapFrame);
    }
    Thread->TrapFrame = OldTrap;
    KfLowerIrql(OldIrql);
}
