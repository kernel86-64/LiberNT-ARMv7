/*
 * PROJECT:     LiberNT Run-Time Library
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARM32 PE and dynamic function table lookup
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <rtl.h>

PRUNTIME_FUNCTION NTAPI
RtlpLookupDynamicFunctionEntry(ULONG_PTR ControlPc, PULONG_PTR ImageBase,
                                PUNWIND_HISTORY_TABLE HistoryTable);

PRUNTIME_FUNCTION
RtlpArmFindFunctionEntry(ULONG_PTR ControlPc, ULONG_PTR ImageBase,
                         PRUNTIME_FUNCTION Table, ULONG Count);

PRUNTIME_FUNCTION NTAPI
RtlLookupFunctionEntry(ULONG_PTR ControlPc, PULONG ImageBase,
                       PUNWIND_HISTORY_TABLE HistoryTable)
{
    PVOID Base;
    PIMAGE_NT_HEADERS Headers;
    PRUNTIME_FUNCTION Table;
    ULONG Size;
    PRUNTIME_FUNCTION Entry;

    ControlPc &= ~1u;
    *ImageBase = 0;
    if (RtlPcToFileHeader((PVOID)ControlPc, &Base))
    {
        Headers = RtlImageNtHeader(Base);
        if (Headers)
        {
            *ImageBase = (ULONG_PTR)Base;
            Size = Headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION].Size;
            Table = (PRUNTIME_FUNCTION)((PUCHAR)Base +
                    Headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION].VirtualAddress);
            Entry = RtlpArmFindFunctionEntry(ControlPc, *ImageBase, Table, Size / sizeof(*Table));
            if (Entry) return Entry;
        }
    }
    return RtlpLookupDynamicFunctionEntry(ControlPc, ImageBase, HistoryTable);
}
