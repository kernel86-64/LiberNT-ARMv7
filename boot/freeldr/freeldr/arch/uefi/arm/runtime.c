/*
 * PROJECT:     LiberNT FreeLoader
 * LICENSE:     GPL-2.0-or-later (https://spdx.org/licenses/GPL-2.0-or-later)
 * PURPOSE:     ARMv7 UEFI platform support
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <uefildr.h>
#if defined(_ARM_VIRT_)
#include <armvirt.h>
#endif

ULONG FirstLevelDcacheSize;
ULONG FirstLevelDcacheFillSize;
ULONG FirstLevelIcacheSize;
ULONG FirstLevelIcacheFillSize;
ULONG SecondLevelDcacheSize;
ULONG SecondLevelDcacheFillSize;
ULONG SecondLevelIcacheSize;
ULONG SecondLevelIcacheFillSize;

VOID
FrLdrCheckCpuCompatibility(VOID)
{
}

BOOLEAN
UefiArchSerialPutChar(_In_ UCHAR Character)
{
#if defined(_ARM_VIRT_)
    volatile ULONG *Uart = (volatile ULONG *)ARM_VIRT_UART_PHYSICAL;

    while (Uart[0x18 / sizeof(ULONG)] & 0x20)
        __asm__ __volatile__("yield");
    Uart[0] = Character;
    __asm__ __volatile__("dsb sy" ::: "memory");
    return TRUE;
#else
    UNREFERENCED_PARAMETER(Character);
    return FALSE;
#endif
}

#undef KeGetCurrentIrql

KIRQL
NTAPI
KeGetCurrentIrql(VOID)
{
    return PASSIVE_LEVEL;
}

VOID
NTAPI
RtlUnwind(
    _In_opt_ PVOID TargetFrame,
    _In_opt_ PVOID TargetIp,
    _In_opt_ PEXCEPTION_RECORD ExceptionRecord,
    _In_ PVOID ReturnValue)
{
    UNREFERENCED_PARAMETER(TargetFrame);
    UNREFERENCED_PARAMETER(TargetIp);
    UNREFERENCED_PARAMETER(ExceptionRecord);
    UNREFERENCED_PARAMETER(ReturnValue);
    BugCheck("ARM UEFI loader does not support structured unwinding");
}
