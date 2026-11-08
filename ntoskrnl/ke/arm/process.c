/*
 * PROJECT:     LiberNT Kernel
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARMv7 LPAE process address-space switching
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ntoskrnl/include/ntoskrnl.h>

VOID NTAPI KiSwapProcess(PKPROCESS NewProcess, PKPROCESS OldProcess)
{
    ULONG Root, Zero = 0;
    KAFFINITY Member;

    if (NewProcess == OldProcess)
        return;
    Member = KiPrcbAffinity(KeGetCurrentPrcb());
    InterlockedOr((volatile LONG *)&NewProcess->ActiveProcessors, Member);
    Root = NewProcess->DirectoryTableBase;
    if (!Root || (Root & (PAGE_SIZE - 1)))
        KeBugCheck(INVALID_PROCESS_ATTACH_ATTEMPT);
    if (Root != OldProcess->DirectoryTableBase)
    {
        __asm__ __volatile__("dsb ish\n\tmcrr p15, 0, %0, %1, c2\n\t"
                             "mcr p15, 0, %1, c13, c0, 1\n\tisb\n\t"
                             "mcr p15, 0, %1, c8, c7, 0\n\tdsb ish\n\tisb"
                             :: "r" (Root), "r" (Zero) : "memory");
    }
    InterlockedAnd((volatile LONG *)&OldProcess->ActiveProcessors, ~Member);
}
