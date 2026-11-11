/*
 * PROJECT:     LiberNT boot tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Cross-check NVS and FreeLoader ARM short descriptors
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <nvs/include/mienv.h>
#include <nvs/arch/arm/shortpte.h>
#include "shortdesc.h"
#include <assert.h>

static _Alignas(16384) uint32_t Root[ARM_SD_ROOT_ENTRIES];
static _Alignas(4096) uint32_t Table[ARM_SD_TABLE_WORDS];

int main(void)
{
    ARM_SD_CONTEXT Context;
    MI_PTE Value, Clean, Copy;
    ULONG Hardware, Other;
    static const ULONG Flags[] = {
        ARM_SD_EXECUTE | ARM_SD_WRITE_BACK | ARM_SD_NONSHAREABLE,
        ARM_SD_USER | ARM_SD_READ_ONLY | ARM_SD_NOT_GLOBAL | ARM_SD_EXECUTE,
        ARM_SD_DEVICE,
        ARM_SD_WRITE_BACK | ARM_SD_READ_ONLY
    };

    for (ULONG Index = 0; Index < sizeof(Flags) / sizeof(Flags[0]); ++Index)
    {
        assert(ArmSdInitialize(&Context, Root, Table, 0x41000000, 1) == ArmSdSuccess);
        assert(ArmSdMapSection(&Context, 0x80000000, 0xfff00000, Flags[Index]) == ArmSdSuccess);
        assert(MiArmShortDecodePte(Root[2048], TRUE, &Value));
        assert(MiArchPteIsBlock(Value, 1) && MiArchPteFrame(Value) == 0xfff00);
        assert(MiArmShortEncodePte(Value, TRUE, &Hardware) && Hardware == Root[2048]);
        assert(ArmSdMapPage(&Context, 0x90000000, 0xfffff000, Flags[Index]) == ArmSdSuccess);
        assert(MiArmShortDecodePte(Root[2304], TRUE, &Value));
        assert(MiArchPteFrame(Value) == 0x41000);
        assert(MiArmShortEncodePte(Value, TRUE, &Hardware) && Hardware == Root[2304]);
        assert(MiArmShortDecodePte(Table[0], FALSE, &Value));
        assert(MiArchPteFrame(Value) == 0xfffff);
        assert(MiArmShortEncodePte(Value, FALSE, &Hardware) && Hardware == Table[0]);
        assert(!Table[ARM_SD_TABLE_ENTRIES]);
    }
    Clean = MiArchPteMakeLeaf(0xfffff, MI_PROT_READWRITE, MI_LEAF_USER);
    Copy = MiArchPteMakeLeaf(0xfffff, MI_PROT_WRITECOPY, MI_LEAF_USER);
    assert(MiArchPteIsWritable(Clean) && !MiArchPteIsDirty(Clean));
    assert(MiArchPteIsCopyOnWrite(Copy) && Clean != Copy);
    assert(MiArmShortEncodePte(Clean, FALSE, &Hardware));
    assert(MiArmShortEncodePte(Copy, FALSE, &Other) && Other == Hardware);
    assert(MiArmShortEncodePte(MiArchPteSetDirty(Clean, TRUE), FALSE, &Other));
    assert((Hardware ^ Other) == (1u << 9));
    assert(MiArmShortEncodePte(MiArchPteSetAccessed(Clean, FALSE), FALSE, &Other));
    assert((Hardware ^ Other) == (1u << 4));
    assert(MiArmShortEncodePte(MiSoftMake(MiSoftTransition, MI_PROT_READWRITE, 0xfffff), FALSE, &Hardware));
    assert(!Hardware);
    assert(!MiArmShortEncodePte(MiArchPteMakeLeaf(0x100000, MI_PROT_READONLY, 0), FALSE, &Hardware));
    assert(!MiArmShortDecodePte(0x41000401, TRUE, &Value));
    assert(!MiArmShortDecodePte(0x40040002, TRUE, &Value));
    assert(!MiArmShortDecodePte(1, FALSE, &Value));
    puts("NVS short-descriptor conversion: PASS");
    return 0;
}
