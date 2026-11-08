/*
 * PROJECT:     LiberNT boot tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARM32 SDK unwind layout checks
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <windows.h>

C_ASSERT(sizeof(UNWIND_HISTORY_TABLE) == 112);
C_ASSERT(sizeof(KNONVOLATILE_CONTEXT_POINTERS) == 68);
C_ASSERT(FIELD_OFFSET(UNWIND_HISTORY_TABLE, Entry) == 16);
C_ASSERT(FIELD_OFFSET(KNONVOLATILE_CONTEXT_POINTERS, D8) == 36);

C_ASSERT(CONTEXT_ALL == 0x0020000F);
C_ASSERT(CONTEXT_FULL == 0x00200007);
