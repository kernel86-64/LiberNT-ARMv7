/*
 * PROJECT:     LiberNT Kernel
 * LICENSE:     BSD - See COPYING.ARM in the top level directory
 * PURPOSE:     ARM32 system service dispatch
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 * PROGRAMMERS: ReactOS Portable Systems Group
 */

#include <ntoskrnl/include/ntoskrnl.h>

#define BUILD_SYSCALLS \
SYSCALL(00, ()) \
SYSCALL(01, (_1)) \
SYSCALL(02, (_1, _2)) \
SYSCALL(03, (_1, _2, _3)) \
SYSCALL(04, (_1, _2, _3, _4 )) \
SYSCALL(05, (_1, _2, _3, _4, _5)) \
SYSCALL(06, (_1, _2, _3, _4, _5, _6)) \
SYSCALL(07, (_1, _2, _3, _4, _5, _6, _7)) \
SYSCALL(08, (_1, _2, _3, _4, _5, _6, _7, _8)) \
SYSCALL(09, (_1, _2, _3, _4, _5, _6, _7, _8, _9)) \
SYSCALL(0A, (_1, _2, _3, _4, _5, _6, _7, _8, _9, a)) \
SYSCALL(0B, (_1, _2, _3, _4, _5, _6, _7, _8, _9, a, b)) \
SYSCALL(0C, (_1, _2, _3, _4, _5, _6, _7, _8, _9, a, b, c)) \
SYSCALL(0D, (_1, _2, _3, _4, _5, _6, _7, _8, _9, a, b, c, d)) \
SYSCALL(0E, (_1, _2, _3, _4, _5, _6, _7, _8, _9, a, b, c, d, e)) \
SYSCALL(0F, (_1, _2, _3, _4, _5, _6, _7, _8, _9, a, b, c, d, e, f)) \
SYSCALL(10, (_1, _2, _3, _4, _5, _6, _7, _8, _9, a, b, c, d, e, f, _10)) \
SYSCALL(11, (_1, _2, _3, _4, _5, _6, _7, _8, _9, a, b, c, d, e, f, _10, _11))

#define PROTO
#include "ke_i.h"
BUILD_SYSCALLS

#define FUNC
#include "ke_i.h"
BUILD_SYSCALLS

typedef NTSTATUS (*PKI_SYSCALL_PARAM_HANDLER)(IN PVOID p, IN PVOID *g);
PKI_SYSCALL_PARAM_HANDLER KiSyscallHandlers[0x12] =
{
    KiSyscall00Param,
    KiSyscall01Param,
    KiSyscall02Param,
    KiSyscall03Param,
    KiSyscall04Param,
    KiSyscall05Param,
    KiSyscall06Param,
    KiSyscall07Param,
    KiSyscall08Param,
    KiSyscall09Param,
    KiSyscall0AParam,
    KiSyscall0BParam,
    KiSyscall0CParam,
    KiSyscall0DParam,
    KiSyscall0EParam,
    KiSyscall0FParam,
    KiSyscall10Param,
    KiSyscall11Param,
};


VOID
KiSystemService(PKTHREAD Thread, PKTRAP_FRAME TrapFrame, ULONG Instruction)
{
    PKSERVICE_TABLE_DESCRIPTOR Descriptor;
    ULONG Id = TrapFrame->R12;
    ULONG Offset = (Id >> SERVICE_TABLE_SHIFT) & SERVICE_TABLE_MASK;
    ULONG Number = Id & SERVICE_NUMBER_MASK;
    ULONG ArgumentCount, Index;
    PVOID Arguments[17];
    PVOID SystemCall;
    KPROCESSOR_MODE Mode = KiGetPreviousMode(TrapFrame);

    UNREFERENCED_PARAMETER(Instruction);
    KeGetCurrentPrcb()->KeSystemCalls++;
    if (Id & ~(SERVICE_NUMBER_MASK | (SERVICE_TABLE_MASK << SERVICE_TABLE_SHIFT)))
    {
        TrapFrame->R0 = STATUS_INVALID_SYSTEM_SERVICE;
        return;
    }
    /* TODO: convert first-time win32k callers and retry the shadow service table. */
    Descriptor = (PVOID)((ULONG_PTR)Thread->ServiceTable + Offset);
    if (!Thread->ServiceTable || Number >= Descriptor->Limit || !Descriptor->Base || !Descriptor->Number)
    {
        TrapFrame->R0 = STATUS_INVALID_SYSTEM_SERVICE;
        return;
    }
    ArgumentCount = Descriptor->Number[Number] / sizeof(ULONG);
    if (Descriptor->Number[Number] % sizeof(ULONG) || ArgumentCount >= RTL_NUMBER_OF(KiSyscallHandlers))
    {
        TrapFrame->R0 = STATUS_INVALID_SYSTEM_SERVICE;
        return;
    }
    SystemCall = (PVOID)Descriptor->Base[Number];
    if (!SystemCall)
    {
        TrapFrame->R0 = STATUS_INVALID_SYSTEM_SERVICE;
        return;
    }
    for (Index = 0; Index < min(ArgumentCount, 4); Index++)
        Arguments[Index] = (PVOID)(&TrapFrame->R0)[Index];
    if (ArgumentCount > 4)
    {
        SIZE_T Bytes = (ArgumentCount - 4) * sizeof(ULONG);
        _SEH2_TRY
        {
            if (Mode == UserMode)
                ProbeForRead((PVOID)TrapFrame->Sp, Bytes, sizeof(ULONG));
            RtlCopyMemory(Arguments + 4, (PVOID)TrapFrame->Sp, Bytes);
        }
        _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
        {
            TrapFrame->R0 = _SEH2_GetExceptionCode();
            _SEH2_YIELD(return);
        }
        _SEH2_END;
    }
    _enable();
    TrapFrame->R0 = KiSyscallHandlers[ArgumentCount](SystemCall, Arguments);
    _disable();
    if (Mode == UserMode && KeGetCurrentIrql() != PASSIVE_LEVEL)
        KeBugCheckEx(IRQL_GT_ZERO_AT_SYSTEM_SERVICE, (ULONG_PTR)SystemCall, KeGetCurrentIrql(), 0, 0);
    if (Mode == UserMode && (Thread->ApcStateIndex != OriginalApcEnvironment || Thread->CombinedApcDisable))
        KeBugCheckEx(APC_INDEX_MISMATCH, (ULONG_PTR)SystemCall, Thread->ApcStateIndex, Thread->CombinedApcDisable, 0);
}

VOID
KiSoftwareInterruptHandler(PKTRAP_FRAME TrapFrame)
{
    PKTHREAD Thread = KeGetCurrentThread();
    PKTRAP_FRAME OldTrap = Thread->TrapFrame;
    KPROCESSOR_MODE OldMode = Thread->PreviousMode;

    TrapFrame->TrapFrame = (ULONG_PTR)OldTrap;
    TrapFrame->PreviousMode = OldMode;
    Thread->TrapFrame = TrapFrame;
    Thread->PreviousMode = KiGetPreviousMode(TrapFrame);
    KiSystemService(Thread, TrapFrame, TrapFrame->R12);
    Thread->PreviousMode = OldMode;
    Thread->TrapFrame = OldTrap;
}
