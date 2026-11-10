/*
 * PROJECT:         ReactOS Kernel
 * LICENSE:         BSD - See COPYING.ARM in the top level directory
 * FILE:            ntoskrnl/ke/arm/interrupt.c
 * PURPOSE:         Implements interrupt related routines for ARM machines
 * PROGRAMMERS:
 */

/* INCLUDES *******************************************************************/

#include <ntoskrnl.h>
#define NDEBUG
#include <debug.h>

/* FUNCTIONS ******************************************************************/

static PKINTERRUPT KiArmInterrupts[1024];

KIRQL KiArmGetInterruptIrql(ULONG Vector)
{
    PKINTERRUPT Interrupt = Vector < RTL_NUMBER_OF(KiArmInterrupts) ? KiArmInterrupts[Vector] : NULL;
    return Interrupt ? Interrupt->Irql : HalpArmGetInterruptIrql(Vector);
}

BOOLEAN NTAPI
KeDispatchSecondaryInterrupt(ULONG Vector, ULONG_PTR Flags, PVOID Reserved)
{
    PKINTERRUPT Head, Interrupt;
    PLIST_ENTRY Entry;
    KIRQL OldIrql;
    BOOLEAN Handled = FALSE, Serviced;

    UNREFERENCED_PARAMETER(Flags);
    UNREFERENCED_PARAMETER(Reserved);
    Head = Vector < RTL_NUMBER_OF(KiArmInterrupts) ? KiArmInterrupts[Vector] : NULL;
    if (!Head) return KiDispatchPassiveInterrupt(Vector, NULL);
    Entry = &Head->InterruptListEntry;
    do
    {
        Interrupt = CONTAINING_RECORD(Entry, KINTERRUPT, InterruptListEntry);
        OldIrql = KfRaiseIrql(Interrupt->SynchronizeIrql);
        KeAcquireSpinLockAtDpcLevel(Interrupt->ActualLock);
        ++Interrupt->DispatchCount;
        Serviced = Interrupt->ServiceRoutine(Interrupt, Interrupt->ServiceContext);
        if (Serviced) ++Interrupt->ServiceCount;
        Handled |= Serviced;
        KeReleaseSpinLockFromDpcLevel(Interrupt->ActualLock);
        KfLowerIrql(OldIrql);
        Entry = Entry->Flink;
    } while (Entry != &Head->InterruptListEntry);
    return Handled;
}

VOID
NTAPI
KeInitializeInterrupt(PKINTERRUPT Interrupt, PKSERVICE_ROUTINE ServiceRoutine,
                      PVOID ServiceContext, PKSPIN_LOCK SpinLock, ULONG Vector,
                      KIRQL Irql, KIRQL SynchronizeIrql, KINTERRUPT_MODE InterruptMode,
                      BOOLEAN ShareVector, CHAR ProcessorNumber, BOOLEAN FloatingSave)
{
    RtlZeroMemory(Interrupt, sizeof(*Interrupt));
    Interrupt->Type = InterruptObject;
    Interrupt->Size = sizeof(*Interrupt);
    InitializeListHead(&Interrupt->InterruptListEntry);
    KeInitializeSpinLock(&Interrupt->SpinLock);
    Interrupt->ActualLock = SpinLock ? SpinLock : &Interrupt->SpinLock;
    Interrupt->ServiceRoutine = ServiceRoutine;
    Interrupt->ServiceContext = ServiceContext;
    Interrupt->Vector = Vector;
    Interrupt->Irql = Irql;
    Interrupt->SynchronizeIrql = SynchronizeIrql;
    Interrupt->Mode = InterruptMode;
    Interrupt->ShareVector = ShareVector;
    Interrupt->Number = ProcessorNumber;
    Interrupt->FloatingSave = FloatingSave;
}

BOOLEAN
NTAPI
KeConnectInterrupt(PKINTERRUPT Interrupt)
{
    PKINTERRUPT Head;
    KIRQL OldIrql;
    BOOLEAN Result = FALSE;

    if (Interrupt->Number != 0 || KeGetCurrentProcessorNumber() != 0 ||
        Interrupt->Vector < 32 || Interrupt->Vector >= 1020 || !Interrupt->ServiceRoutine)
        return FALSE;
    if (Interrupt->Irql == PASSIVE_LEVEL) return KiConnectPassiveInterrupt(Interrupt);
    if (Interrupt->Irql <= DISPATCH_LEVEL || Interrupt->Irql >= HIGH_LEVEL ||
        Interrupt->SynchronizeIrql < Interrupt->Irql || Interrupt->SynchronizeIrql > HIGH_LEVEL)
        return FALSE;
    OldIrql = KfRaiseIrql(HIGH_LEVEL);
    if (Interrupt->Connected) goto Done;
    Head = KiArmInterrupts[Interrupt->Vector];
    if (Head)
    {
        if (!Head->ShareVector || !Interrupt->ShareVector ||
            Head->Mode != Interrupt->Mode || Head->Irql != Interrupt->Irql)
            goto Done;
        InsertTailList(&Head->InterruptListEntry, &Interrupt->InterruptListEntry);
    }
    else
    {
        KiArmInterrupts[Interrupt->Vector] = Interrupt;
        KeMemoryBarrier();
        if (!HalEnableSystemInterrupt(Interrupt->Vector, Interrupt->Irql, Interrupt->Mode))
        {
            KiArmInterrupts[Interrupt->Vector] = NULL;
            goto Done;
        }
    }
    Interrupt->Connected = TRUE;
    KeMemoryBarrier();
    Result = TRUE;
Done:
    KfLowerIrql(OldIrql);
    return Result;
}

BOOLEAN
NTAPI
KeDisconnectInterrupt(PKINTERRUPT Interrupt)
{
    PKINTERRUPT Head;
    KIRQL OldIrql;
    BOOLEAN Connected;

    if (Interrupt->Number != 0 || KeGetCurrentProcessorNumber() != 0 ||
        Interrupt->Vector >= RTL_NUMBER_OF(KiArmInterrupts))
        return FALSE;
    if (Interrupt->Irql == PASSIVE_LEVEL) return KiDisconnectPassiveInterrupt(Interrupt);
    OldIrql = KfRaiseIrql(HIGH_LEVEL);
    Connected = Interrupt->Connected;
    if (Connected)
    {
        Head = KiArmInterrupts[Interrupt->Vector];
        if (IsListEmpty(&Interrupt->InterruptListEntry))
        {
            HalDisableSystemInterrupt(Interrupt->Vector, Interrupt->Irql);
            KiArmInterrupts[Interrupt->Vector] = NULL;
        }
        else
        {
            if (Head == Interrupt)
                KiArmInterrupts[Interrupt->Vector] = CONTAINING_RECORD(
                    Interrupt->InterruptListEntry.Flink, KINTERRUPT, InterruptListEntry);
            RemoveEntryList(&Interrupt->InterruptListEntry);
        }
        InitializeListHead(&Interrupt->InterruptListEntry);
        Interrupt->Connected = FALSE;
        KeMemoryBarrier();
    }
    KfLowerIrql(OldIrql);
    return Connected;
}

VOID
KiUnexpectedInterrupt(VOID)
{
    /* Crash the machine */
    KeBugCheck(TRAP_CAUSE_UNKNOWN);
}

BOOLEAN
NTAPI
KeSynchronizeExecution(
    IN OUT PKINTERRUPT Interrupt,
    IN PKSYNCHRONIZE_ROUTINE SynchronizeRoutine,
    IN PVOID SynchronizeContext OPTIONAL)
{
    KIRQL OldIrql;
    BOOLEAN Result;

    if (Interrupt->Irql == PASSIVE_LEVEL)
        return KiSynchronizePassiveInterrupt(Interrupt, SynchronizeRoutine, SynchronizeContext);
    OldIrql = KfRaiseIrql(Interrupt->SynchronizeIrql);
    KeAcquireSpinLockAtDpcLevel(Interrupt->ActualLock);
    Result = SynchronizeRoutine(SynchronizeContext);
    KeReleaseSpinLockFromDpcLevel(Interrupt->ActualLock);
    KfLowerIrql(OldIrql);
    return Result;
}

BOOLEAN
NTAPI
KiSetClockDeadline(
    _In_ ULONGLONG InterruptTime)
{
    UNREFERENCED_PARAMETER(InterruptTime);
    return FALSE;
}
