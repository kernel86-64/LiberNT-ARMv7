/*
 * PROJECT:     LiberNT Kernel
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARMv7 inter-processor interrupt services
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ntoskrnl.h>

static struct
{
    PKIPI_BROADCAST_WORKER Function;
    ULONG_PTR Argument;
    volatile LONG TargetCount;
    volatile LONG Arrived;
    volatile LONG Release;
    volatile LONG Busy;
} KiArmIpiPacket;

static volatile LONG KiArmFreezeFlag;
static volatile LONG KiArmFreezeAck;

VOID
FASTCALL
KiIpiSend(
    IN KAFFINITY TargetProcessors,
    IN ULONG IpiRequest)
{
    KAFFINITY Bit;

    for (Bit = TargetProcessors; Bit; Bit &= Bit - 1)
    {
        PKPRCB Prcb;
        ULONG Cpu;

        if (!BitScanForwardAffinity(&Cpu, Bit) || Cpu >= (ULONG)KeNumberProcessors)
            break;
        Prcb = KiProcessorBlock[Cpu];
        if (Prcb)
            InterlockedBitTestAndSet((PLONG)&Prcb->IpiFrozen, IpiRequest);
    }
    HalRequestIpi(TargetProcessors);
}

BOOLEAN
NTAPI
KiIpiServiceRoutine(
    IN PKTRAP_FRAME TrapFrame,
    IN PKEXCEPTION_FRAME ExceptionFrame)
{
    PKPRCB Prcb = KeGetCurrentPrcb();

    UNREFERENCED_PARAMETER(ExceptionFrame);

    if (InterlockedBitTestAndReset((PLONG)&Prcb->IpiFrozen, IPI_APC))
        HalRequestSoftwareInterrupt(APC_LEVEL);
    if (InterlockedBitTestAndReset((PLONG)&Prcb->IpiFrozen, IPI_DPC))
    {
        KiSetDpcInterruptRequested(Prcb);
        HalRequestSoftwareInterrupt(DISPATCH_LEVEL);
    }
    if (InterlockedBitTestAndReset((PLONG)&Prcb->IpiFrozen, IPI_SYNCH_REQUEST))
    {
        PKIPI_BROADCAST_WORKER Function = KiArmIpiPacket.Function;
        ULONG_PTR Argument = KiArmIpiPacket.Argument;

        if (Function)
        {
            InterlockedIncrement(&KiArmIpiPacket.Arrived);
            __asm__ volatile ("dmb ish\n\tsev" ::: "memory");
            while (KiArmIpiPacket.Release == 0)
                YieldProcessor();
            if (TrapFrame)
                _enable();
            Function(Argument);
            if (TrapFrame)
                _disable();
            InterlockedDecrement(&KiArmIpiPacket.TargetCount);
            __asm__ volatile ("dmb ish\n\tsev" ::: "memory");
        }
    }
    if (InterlockedBitTestAndReset((PLONG)&Prcb->IpiFrozen, IPI_FREEZE))
    {
        InterlockedIncrement(&KiArmFreezeAck);
        while (KiArmFreezeFlag)
            YieldProcessor();
        InterlockedDecrement(&KiArmFreezeAck);
        __asm__ volatile ("dmb ish\n\tsev" ::: "memory");
    }
    return TRUE;
}

VOID
NTAPI
KiIpiSendPacket(
    IN KAFFINITY TargetProcessors,
    IN PKIPI_WORKER WorkerFunction,
    IN PKIPI_BROADCAST_WORKER BroadcastFunction,
    IN ULONG_PTR Context,
    IN PULONG Count)
{
    UNREFERENCED_PARAMETER(TargetProcessors);
    UNREFERENCED_PARAMETER(WorkerFunction);
    UNREFERENCED_PARAMETER(BroadcastFunction);
    UNREFERENCED_PARAMETER(Context);
    if (Count)
        *Count = 1;
}

VOID
FASTCALL
KiIpiSignalPacketDone(
    IN PKIPI_CONTEXT PacketContext)
{
    UNREFERENCED_PARAMETER(PacketContext);
}

VOID
FASTCALL
KiIpiSignalPacketDoneAndStall(
    IN PKIPI_CONTEXT PacketContext,
    IN volatile PULONG ReverseStall)
{
    UNREFERENCED_PARAMETER(PacketContext);
    if (ReverseStall)
        *ReverseStall = 0;
}

ULONG_PTR
NTAPI
KeIpiGenericCall(
    IN PKIPI_BROADCAST_WORKER Function,
    IN ULONG_PTR Argument)
{
    KIRQL OldIrql;
    ULONG_PTR Result;
    KAFFINITY Targets;
    LONG TargetCount;
    KAFFINITY Bit;

    if (!Function)
        return 0;

    OldIrql = KfRaiseIrql(IPI_LEVEL - 1);
    while (InterlockedCompareExchange(&KiArmIpiPacket.Busy, 1, 0) != 0)
        YieldProcessor();

    Targets = KeActiveProcessors & ~KiPrcbAffinity(KeGetCurrentPrcb());
    TargetCount = 0;
    for (Bit = Targets; Bit; Bit &= Bit - 1)
        TargetCount++;

    KiArmIpiPacket.Function = Function;
    KiArmIpiPacket.Argument = Argument;
    KiArmIpiPacket.Arrived = 0;
    KiArmIpiPacket.Release = 0;
    InterlockedExchange(&KiArmIpiPacket.TargetCount, TargetCount);
    KeMemoryBarrier();
    if (Targets)
        KiIpiSend(Targets, IPI_SYNCH_REQUEST);

    while (KiArmIpiPacket.Arrived != TargetCount)
        YieldProcessor();

    KfRaiseIrql(IPI_LEVEL);
    InterlockedExchange(&KiArmIpiPacket.Release, 1);
    __asm__ volatile ("dmb ish\n\tsev" ::: "memory");
    Result = Function(Argument);

    while (KiArmIpiPacket.TargetCount != 0)
        YieldProcessor();

    KiArmIpiPacket.Function = NULL;
    InterlockedExchange(&KiArmIpiPacket.Busy, 0);
    __asm__ volatile ("dmb ish\n\tsev" ::: "memory");
    KfLowerIrql(OldIrql);
    return Result;
}

VOID
NTAPI
KxFreezeExecution(
    VOID)
{
    KAFFINITY Targets;
    KAFFINITY Bit;
    ULONG TargetCount = 0;

    Targets = KeActiveProcessors & ~KiPrcbAffinity(KeGetCurrentPrcb());
    for (Bit = Targets; Bit; Bit &= Bit - 1)
        TargetCount++;
    InterlockedExchange(&KiArmFreezeAck, 0);
    InterlockedExchange(&KiArmFreezeFlag, 1);
    if (Targets)
        KiIpiSend(Targets, IPI_FREEZE);
    while (KiArmFreezeAck != (LONG)TargetCount)
        YieldProcessor();
}

VOID
NTAPI
KxThawExecution(
    VOID)
{
    InterlockedExchange(&KiArmFreezeFlag, 0);
    __asm__ volatile ("dmb ish\n\tsev" ::: "memory");
    while (KiArmFreezeAck != 0)
        YieldProcessor();
}
