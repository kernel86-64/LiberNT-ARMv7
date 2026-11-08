/*
 * PROJECT:     LiberNT Kernel
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARM32 executive SList entry points
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ntoskrnl.h>

PSLIST_ENTRY NTAPI RtlInterlockedPopEntrySList(PSLIST_HEADER Head);
PSLIST_ENTRY NTAPI RtlInterlockedPushEntrySList(PSLIST_HEADER Head, PSLIST_ENTRY Entry);

PSLIST_ENTRY NTAPI
ExpInterlockedPopEntrySList(PSLIST_HEADER Head)
{
    return RtlInterlockedPopEntrySList(Head);
}

PSLIST_ENTRY NTAPI
ExpInterlockedPushEntrySList(PSLIST_HEADER Head, PSLIST_ENTRY Entry)
{
    return RtlInterlockedPushEntrySList(Head, Entry);
}
