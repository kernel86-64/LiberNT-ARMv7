/*
 * PROJECT:     LiberNT HAL
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARMv7 bootstrap spinlock services
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <hal.h>
#include <internal/spinlock.h>

static VOID HalpAcquire(PKSPIN_LOCK Lock)
{
    KxAcquireSpinLock(Lock);
    __asm__ __volatile__("dmb ish" ::: "memory");
}

static VOID HalpRelease(PKSPIN_LOCK Lock)
{
    __asm__ __volatile__("dmb ish" ::: "memory");
    KxReleaseSpinLock(Lock);
}

KIRQL FASTCALL KfAcquireSpinLock(PKSPIN_LOCK Lock)
{
    KIRQL Irql = KfRaiseIrql(DISPATCH_LEVEL);
    HalpAcquire(Lock);
    return Irql;
}

VOID FASTCALL KfReleaseSpinLock(PKSPIN_LOCK Lock, KIRQL Irql)
{
    HalpRelease(Lock);
    KfLowerIrql(Irql);
}

KIRQL FASTCALL HalpAcquireSpinLockRaiseToSynch(PKSPIN_LOCK Lock) { return KfAcquireSpinLock(Lock); }

VOID FASTCALL HalpAcquireInStackQueuedSpinLock(PKSPIN_LOCK Lock, PKLOCK_QUEUE_HANDLE Handle)
{
    Handle->OldIrql = KfRaiseIrql(DISPATCH_LEVEL);
    Handle->LockQueue.Next = NULL;
    Handle->LockQueue.Lock = Lock;
    HalpAcquire(Lock);
}

VOID FASTCALL KeAcquireInStackQueuedSpinLockRaiseToSynch(PKSPIN_LOCK Lock, PKLOCK_QUEUE_HANDLE Handle)
{
    HalpAcquireInStackQueuedSpinLock(Lock, Handle);
}

VOID FASTCALL HalpReleaseInStackQueuedSpinLock(PKLOCK_QUEUE_HANDLE Handle)
{
    HalpRelease(Handle->LockQueue.Lock);
    KfLowerIrql(Handle->OldIrql);
}

KIRQL FASTCALL HalpAcquireQueuedSpinLock(KSPIN_LOCK_QUEUE_NUMBER Number)
{
    KIRQL Irql = KfRaiseIrql(DISPATCH_LEVEL);
    HalpAcquire(KeGetCurrentPrcb()->LockQueue[Number].Lock);
    return Irql;
}

VOID FASTCALL HalpReleaseQueuedSpinLock(KSPIN_LOCK_QUEUE_NUMBER Number, KIRQL Irql)
{
    HalpRelease(KeGetCurrentPrcb()->LockQueue[Number].Lock);
    KfLowerIrql(Irql);
}
