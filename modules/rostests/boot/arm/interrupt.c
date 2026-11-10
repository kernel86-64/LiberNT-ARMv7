/*
 * PROJECT:     LiberNT API tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARM32 interrupt connection, sharing and synchronization regression
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ntoskrnl/include/ntoskrnl.h>

static KIRQL TestIrql;
static ULONG Enables, Disables, Calls;
static BOOLEAN FailEnable;

static KIRQL IrqRaise(KIRQL Level) { KIRQL Old = TestIrql; TestIrql = Level; return Old; }
static VOID IrqLower(KIRQL Level) { TestIrql = Level; }
static BOOLEAN NTAPI IrqEnable(ULONG Vector, KIRQL Level, KINTERRUPT_MODE Mode)
{
    ++Enables;
    return !FailEnable && Vector >= 32 && Level == 5 && Mode == LevelSensitive;
}
static VOID NTAPI IrqDisable(ULONG Vector, KIRQL Level)
{
    if (Vector == 48 && Level == 5) ++Disables;
}
static BOOLEAN IrqPassiveConnect(PKINTERRUPT Interrupt) { UNREFERENCED_PARAMETER(Interrupt); return FALSE; }
static BOOLEAN IrqPassiveDispatch(ULONG Vector, PBOOLEAN Level)
{
    UNREFERENCED_PARAMETER(Vector); UNREFERENCED_PARAMETER(Level); return FALSE;
}
static BOOLEAN IrqPassiveSync(PKINTERRUPT Interrupt, PKSYNCHRONIZE_ROUTINE Routine, PVOID Context)
{
    UNREFERENCED_PARAMETER(Interrupt); return Routine(Context);
}
static KIRQL NTAPI IrqLevel(ULONG Vector) { UNREFERENCED_PARAMETER(Vector); return 4; }
static VOID IrqLock(PKSPIN_LOCK Lock) { ASSERT(!*Lock); *Lock = 1; }
static VOID IrqUnlock(PKSPIN_LOCK Lock) { ASSERT(*Lock == 1); *Lock = 0; }

#undef KeGetCurrentProcessorNumber
#define KeGetCurrentProcessorNumber() 0
#define KfRaiseIrql IrqRaise
#define KfLowerIrql IrqLower
#define HalEnableSystemInterrupt IrqEnable
#define HalDisableSystemInterrupt IrqDisable
#define HalpArmGetInterruptIrql IrqLevel
#define KiConnectPassiveInterrupt IrqPassiveConnect
#define KiDisconnectPassiveInterrupt IrqPassiveConnect
#define KiSynchronizePassiveInterrupt IrqPassiveSync
#define KiDispatchPassiveInterrupt IrqPassiveDispatch
#define KiUnexpectedInterrupt ArmTestUnusedInterrupt
#undef KeAcquireSpinLockAtDpcLevel
#undef KeReleaseSpinLockFromDpcLevel
#define KeAcquireSpinLockAtDpcLevel IrqLock
#define KeReleaseSpinLockFromDpcLevel IrqUnlock
#include "../../../../ntoskrnl/ke/arm/interrupt.c"

static BOOLEAN NTAPI IrqService(PKINTERRUPT Interrupt, PVOID Context)
{
    if (TestIrql != Interrupt->SynchronizeIrql) return FALSE;
    ++Calls;
    return Context != NULL;
}
static BOOLEAN NTAPI IrqSynchronized(PVOID Context)
{
    return Context == &Calls && TestIrql == 6;
}

BOOLEAN ArmTestInterrupts(VOID)
{
    KINTERRUPT First, Second, Failed;
    BOOLEAN Passed;

    Enables = Disables = Calls = 0;
    TestIrql = 0;
    FailEnable = FALSE;
    KeInitializeInterrupt(&First, IrqService, NULL, NULL, 48, 5, 6, LevelSensitive, TRUE, 0, FALSE);
    KeInitializeInterrupt(&Second, IrqService, &Calls, NULL, 48, 5, 6, LevelSensitive, FALSE, 0, FALSE);
    Passed = KeConnectInterrupt(&First) && First.Connected && Enables == 1;
    Passed = !KeConnectInterrupt(&Second) && !Second.Connected && Passed;
    Second.ShareVector = TRUE;
    Passed = KeConnectInterrupt(&Second) && Enables == 1 && Passed;
    Passed = KiArmGetInterruptIrql(48) == 5 && KiArmGetInterruptIrql(49) == 4 && Passed;
    TestIrql = 5;
    Passed = KeDispatchSecondaryInterrupt(48, 0, NULL) && Calls == 2 && TestIrql == 5 && Passed;
    TestIrql = 0;
    Passed = First.DispatchCount == 1 && !First.ServiceCount && Second.ServiceCount == 1 && Passed;
    Passed = KeSynchronizeExecution(&Second, IrqSynchronized, &Calls) && !TestIrql && Passed;
    Passed = KeDisconnectInterrupt(&First) && !Disables && !First.Connected && Passed;
    Passed = KeDispatchSecondaryInterrupt(48, 0, NULL) && Calls == 3 && Passed;
    Passed = KeDisconnectInterrupt(&Second) && Disables == 1 && !Second.Connected && Passed;
    Passed = !KeDispatchSecondaryInterrupt(48, 0, NULL) && Passed;
    KeInitializeInterrupt(&Failed, IrqService, NULL, NULL, 49, 5, 5, LevelSensitive, FALSE, 0, FALSE);
    FailEnable = TRUE;
    Passed = !KeConnectInterrupt(&Failed) && !Failed.Connected && KiArmGetInterruptIrql(49) == 4 && Passed;
    return Passed;
}
