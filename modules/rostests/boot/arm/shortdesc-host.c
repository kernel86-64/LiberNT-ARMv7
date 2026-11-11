/*
 * PROJECT:     LiberNT boot tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Host checks for ARMv7 short-descriptor boot tables
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include "shortdesc.h"
#include <assert.h>
#include <stdio.h>

static _Alignas(16384) uint32_t Root[ARM_SD_ROOT_ENTRIES];
static _Alignas(4096) uint32_t Tables[2 * ARM_SD_TABLE_WORDS];

int main(void)
{
    ARM_SD_CONTEXT Context;
    uint32_t Physical;

    Root[0] = 0x12345678;
    assert(ArmSdInitialize(&Context, Root + 1, Tables, 0x41000000, 2) == ArmSdInvalid);
    assert(Root[0] == 0x12345678);
    assert(ArmSdInitialize(&Context, Root, Tables, 0xfffff000, 2) == ArmSdInvalid);
    assert(ArmSdInitialize(&Context, Root, Tables, 0xfffff000, 1) == ArmSdSuccess);
    assert(ArmSdMapPage(&Context, 0xfffff000, 0xfffff000, 0) == ArmSdSuccess);
    assert(Root[4095] == 0xfffff001);
    assert(Tables[255] == 0xfffff453);
    assert(ArmSdQuery(&Context, 0xffffffff, &Physical) == ArmSdSuccess);
    assert(Physical == 0xffffffff);

    assert(ArmSdInitialize(&Context, Root, Tables, 0x41000000, 2) == ArmSdSuccess);
    assert(ArmSdMapSection(&Context, 0x80000000, 0x40000000, ARM_SD_EXECUTE) == ArmSdSuccess);
    assert(Root[2048] == 0x40011402);
    assert(ArmSdMapPage(&Context, 0x80004000, 0x40004000, ARM_SD_EXECUTE) == ArmSdSuccess);
    assert(ArmSdQuery(&Context, 0x800f1234, &Physical) == ArmSdSuccess);
    assert(Physical == 0x400f1234);
    assert(ArmSdMapSection(&Context, 0x80000000, 0x40000000, ARM_SD_EXECUTE) == ArmSdSuccess);
    assert(ArmSdMapPage(&Context, 0x80000000, 0x40000000, 0) == ArmSdConflict);
    assert(ArmSdMapSection(&Context, 1, 0, 0) == ArmSdInvalid);

    assert(ArmSdMapPage(&Context, 0x90001000, 0x42000000,
                        ARM_SD_READ_ONLY | ARM_SD_USER | ARM_SD_NOT_GLOBAL | ARM_SD_WRITE_BACK) == ArmSdSuccess);
    assert(Root[2304] == 0x41000001);
    assert(Tables[1] == 0x42000e7f);
    assert(ArmSdMapPage(&Context, 0x90002000, 0x42001000, ARM_SD_DEVICE) == ArmSdSuccess);
    assert(Tables[2] == 0x42001417);
    assert(ArmSdQuery(&Context, 0x90001fff, &Physical) == ArmSdSuccess);
    assert(Physical == 0x42000fff);
    assert(ArmSdMapPage(&Context, 0x90001000, 0x43000000, 0) == ArmSdConflict);
    assert(Tables[1] == 0x42000e7f);
    assert(ArmSdMapSection(&Context, 0x90000000, 0x42000000, 0) == ArmSdConflict);
    Tables[ARM_SD_TABLE_ENTRIES] = 0x12345678;
    assert(ArmSdMapPage(&Context, 0xa0000000, 0x43000000, 0) == ArmSdSuccess);
    assert(Root[2560] == 0x41001001);
    assert(Tables[ARM_SD_TABLE_ENTRIES] == 0x12345678);
    assert(ArmSdMapPage(&Context, 0xb0000000, 0x44000000, 0) == ArmSdNoMemory);
    assert(Root[2816] == 0 && Context.Used == 2);
    assert(ArmSdMapPage(&Context, 0, 0, ARM_SD_DEVICE | ARM_SD_EXECUTE) == ArmSdInvalid);
    assert(ArmSdMapPage(&Context, 0, 0, ARM_SD_DEVICE | ARM_SD_WRITE_BACK) == ArmSdInvalid);
    assert(ArmSdMapPage(&Context, 0, 0, 128) == ArmSdInvalid);
    assert(ArmSdQuery(&Context, 0xb0000000, &Physical) == ArmSdInvalid);
    Root[2816] = 0x41000801;
    assert(ArmSdQuery(&Context, 0xb0000000, &Physical) == ArmSdInvalid);

    assert(ArmSdInitialize(&Context, Root, Tables, 0x41000000, 1) == ArmSdSuccess);
    assert(ArmSdMapSection(&Context, 0x80000000, 0x40000000,
                           ARM_SD_EXECUTE | ARM_SD_READ_ONLY | ARM_SD_USER |
                           ARM_SD_NOT_GLOBAL | ARM_SD_WRITE_BACK) == ArmSdSuccess);
    assert(Root[2048] == 0x40039c0e);
    assert(ArmSdUnmapPage(&Context, 0x80001000) == ArmSdSuccess);
    assert(Root[2048] == 0x41000001 && Tables[1] == 0);
    assert(Tables[0] == 0x40000e7e && Tables[255] == 0x400ffe7e);
    assert(ArmSdQuery(&Context, 0x80001000, &Physical) == ArmSdInvalid);
    assert(ArmSdQuery(&Context, 0x800fffff, &Physical) == ArmSdSuccess);
    assert(Physical == 0x400fffff);
    assert(ArmSdMapSection(&Context, 0x90000000, 0x50000000, 0) == ArmSdSuccess);
    assert(ArmSdUnmapPage(&Context, 0x90001000) == ArmSdNoMemory);
    assert(Root[2304] == 0x50011412);
    assert(ArmSdUnmapPage(&Context, 0x80000000) == ArmSdSuccess);
    assert(ArmSdUnmapPage(&Context, 0x80000000) == ArmSdSuccess);
    assert(ArmSdUnmapPage(&Context, 0x80000001) == ArmSdInvalid);
    assert(ArmSdUnmapPage(&Context, 0) == ArmSdSuccess);
    assert(ArmSdInitialize(&Context, Root, Tables, 0x41000000, 2) == ArmSdSuccess);
    assert(ArmSdMapSection(&Context, 0x01000000, 0x01000000,
                           ARM_SD_EXECUTE | ARM_SD_WRITE_BACK | ARM_SD_NONSHAREABLE) == ArmSdSuccess);
    assert(!(Context.Root[16] & 0x10000u));
    assert(ArmSdUnmapPage(&Context, 0x01001000) == ArmSdSuccess);
    assert(!(Context.Tables[((Context.Root[16] & 0xfffffc00u) - Context.TablesPhysical) / 4] & 0x400u));
    puts("ARM short-descriptor host tests: PASS");
    return 0;
}
