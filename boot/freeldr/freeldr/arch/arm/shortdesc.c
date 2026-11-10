/*
 * PROJECT:     LiberNT FreeLoader
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Construct ARM DDI 0406 short-descriptor boot mappings
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include "shortdesc.h"

static int ArmSdFlagsValid(uint32_t Flags)
{
    return !(Flags & ~63u) &&
           !((Flags & ARM_SD_DEVICE) && (Flags & (ARM_SD_EXECUTE | ARM_SD_WRITE_BACK)));
}

static uint32_t ArmSdAttributes(uint32_t Flags, int Section)
{
    uint32_t Entry = 2u | (Section ? 0x10400u : 0x410u);

    if (!(Flags & ARM_SD_EXECUTE)) Entry |= Section ? 0x10u : 1u;
    if (Flags & ARM_SD_READ_ONLY) Entry |= Section ? 0x8000u : 0x200u;
    if (Flags & ARM_SD_USER) Entry |= Section ? 0x800u : 0x20u;
    if (Flags & ARM_SD_NOT_GLOBAL) Entry |= Section ? 0x20000u : 0x800u;
    if (Flags & ARM_SD_DEVICE)
        Entry |= 4u;
    else
    {
        Entry |= Section ? 0x1000u : 0x40u;
        if (Flags & ARM_SD_WRITE_BACK) Entry |= 12u;
    }
    return Entry;
}

int ArmSdInitialize(ARM_SD_CONTEXT *Context, uint32_t *Root,
                    uint32_t *Tables, uint32_t TablesPhysical, uint32_t Count)
{
    uint32_t Index;

    if (!Context || !Root || ((uintptr_t)Root & 16383u) || Count > ARM_SD_ROOT_ENTRIES ||
        (Count && (!Tables || ((uintptr_t)Tables & 1023u) || (TablesPhysical & 1023u))) ||
        (uint64_t)TablesPhysical + (uint64_t)Count * 1024u > 0x100000000ull)
        return ArmSdInvalid;
    Context->Root = Root;
    Context->Tables = Tables;
    Context->TablesPhysical = TablesPhysical;
    Context->Capacity = Count;
    Context->Used = 0;
    for (Index = 0; Index < ARM_SD_ROOT_ENTRIES; ++Index) Root[Index] = 0;
    for (Index = 0; Index < Count * ARM_SD_TABLE_ENTRIES; ++Index) Tables[Index] = 0;
    return ArmSdSuccess;
}

int ArmSdMapSection(ARM_SD_CONTEXT *Context, uint32_t Virtual, uint32_t Physical, uint32_t Flags)
{
    uint32_t *Slot, Entry;

    if (!Context || !Context->Root || ((Virtual | Physical) & 0xfffffu) || !ArmSdFlagsValid(Flags))
        return ArmSdInvalid;
    Slot = &Context->Root[Virtual >> 20];
    Entry = Physical | ArmSdAttributes(Flags, 1);
    if (*Slot && *Slot != Entry) return ArmSdConflict;
    *Slot = Entry;
    return ArmSdSuccess;
}

static uint32_t *ArmSdTable(const ARM_SD_CONTEXT *Context, uint32_t Entry)
{
    uint32_t Physical = Entry & 0xfffffc00u;
    uint32_t Offset = Physical - Context->TablesPhysical;

    if ((Entry & 3u) != 1u || Physical < Context->TablesPhysical ||
        Offset / 1024u >= Context->Used)
        return 0;
    return Context->Tables + Offset / sizeof(uint32_t);
}

int ArmSdMapPage(ARM_SD_CONTEXT *Context, uint32_t Virtual, uint32_t Physical, uint32_t Flags)
{
    uint32_t *Root, *Table, *Slot, Entry;

    if (!Context || !Context->Root || ((Virtual | Physical) & 4095u) || !ArmSdFlagsValid(Flags))
        return ArmSdInvalid;
    Root = &Context->Root[Virtual >> 20];
    if (!*Root)
    {
        if (Context->Used == Context->Capacity) return ArmSdNoMemory;
        *Root = (Context->TablesPhysical + Context->Used++ * 1024u) | 1u;
    }
    Table = ArmSdTable(Context, *Root);
    if (!Table) return ArmSdConflict;
    Slot = &Table[(Virtual >> 12) & 255u];
    Entry = Physical | ArmSdAttributes(Flags, 0);
    if (*Slot && *Slot != Entry) return ArmSdConflict;
    *Slot = Entry;
    return ArmSdSuccess;
}

int ArmSdQuery(const ARM_SD_CONTEXT *Context, uint32_t Virtual, uint32_t *Physical)
{
    uint32_t Entry, *Table;

    if (!Context || !Context->Root || !Physical) return ArmSdInvalid;
    Entry = Context->Root[Virtual >> 20];
    if ((Entry & 3u) == 2u && !(Entry & 0x40000u))
    {
        *Physical = (Entry & 0xfff00000u) | (Virtual & 0xfffffu);
        return ArmSdSuccess;
    }
    Table = ArmSdTable(Context, Entry);
    if (!Table) return ArmSdInvalid;
    Entry = Table[(Virtual >> 12) & 255u];
    if (!(Entry & 2u)) return ArmSdInvalid;
    *Physical = (Entry & 0xfffff000u) | (Virtual & 4095u);
    return ArmSdSuccess;
}

int ArmSdUnmapPage(ARM_SD_CONTEXT *Context, uint32_t Virtual)
{
    uint32_t *Root, *Table, Entry, Index, Flags;

    if (!Context || !Context->Root || (Virtual & 4095u)) return ArmSdInvalid;
    Root = &Context->Root[Virtual >> 20];
    Entry = *Root;
    if (!Entry) return ArmSdSuccess;
    if ((Entry & 3u) == 2u && !(Entry & 0x40000u))
    {
        if (Context->Used == Context->Capacity) return ArmSdNoMemory;
        Table = Context->Tables + Context->Used * ARM_SD_TABLE_ENTRIES;
        Flags = 2u | (Entry & 12u) | ((Entry >> 4) & 1u) | ((Entry >> 6) & 0xff0u);
        for (Index = 0; Index < ARM_SD_TABLE_ENTRIES; ++Index)
            Table[Index] = (Entry & 0xfff00000u) | (Index << 12) | Flags;
        *Root = (Context->TablesPhysical + Context->Used++ * 1024u) | 1u;
    }
    Table = ArmSdTable(Context, *Root);
    if (!Table) return ArmSdInvalid;
    Table[(Virtual >> 12) & 255u] = 0;
    return ArmSdSuccess;
}
