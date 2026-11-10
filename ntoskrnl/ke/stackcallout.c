/*
 * PROJECT:     LiberNT Kernel
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Validated kernel stack callout dispatch
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */
#include <ntoskrnl/include/ntoskrnl.h>

NTSTATUS NTAPI KeExpandKernelStackAndCalloutEx(PEXPAND_STACK_CALLOUT Callout, PVOID Parameter,
                                               SIZE_T Size, BOOLEAN Wait, PVOID Context)
{
    UNREFERENCED_PARAMETER(Context);
    ASSERT(KeGetCurrentIrql() <= DISPATCH_LEVEL);
    if (Size > MAXIMUM_EXPANSION_SIZE) return STATUS_INVALID_PARAMETER_3;
    if (Wait && KeGetCurrentIrql() == DISPATCH_LEVEL) return STATUS_INVALID_PARAMETER_4;
    return KiExpandKernelStackCallout(Callout, Parameter, Size, Wait);
}

NTSTATUS NTAPI KeExpandKernelStackAndCallout(PEXPAND_STACK_CALLOUT Callout, PVOID Parameter, SIZE_T Size)
{
    return KeExpandKernelStackAndCalloutEx(Callout, Parameter, Size, TRUE, NULL);
}
