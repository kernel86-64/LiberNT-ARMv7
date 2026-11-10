/*
 * PROJECT:     LiberNT FreeLoader
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARMv7 boot-time short-descriptor table construction
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#pragma once
#include <stdint.h>

#define ARM_SD_ROOT_ENTRIES 4096u
#define ARM_SD_TABLE_ENTRIES 256u
#define ARM_SD_READ_ONLY 1u
#define ARM_SD_USER 2u
#define ARM_SD_EXECUTE 4u
#define ARM_SD_DEVICE 8u
#define ARM_SD_WRITE_BACK 16u
#define ARM_SD_NOT_GLOBAL 32u

typedef struct
{
    uint32_t *Root;
    uint32_t *Tables;
    uint32_t TablesPhysical;
    uint32_t Capacity;
    uint32_t Used;
} ARM_SD_CONTEXT;

enum
{
    ArmSdSuccess = 0,
    ArmSdInvalid = -1,
    ArmSdNoMemory = -2,
    ArmSdConflict = -3
};

int ArmSdInitialize(ARM_SD_CONTEXT *Context, uint32_t *Root,
                    uint32_t *Tables, uint32_t TablesPhysical, uint32_t Count);
int ArmSdMapSection(ARM_SD_CONTEXT *Context, uint32_t Virtual, uint32_t Physical, uint32_t Flags);
int ArmSdMapPage(ARM_SD_CONTEXT *Context, uint32_t Virtual, uint32_t Physical, uint32_t Flags);
int ArmSdUnmapPage(ARM_SD_CONTEXT *Context, uint32_t Virtual);
int ArmSdQuery(const ARM_SD_CONTEXT *Context, uint32_t Virtual, uint32_t *Physical);
