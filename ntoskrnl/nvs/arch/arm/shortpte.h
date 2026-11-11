/*
 * PROJECT:     LiberNT NT Virtual Memory Subsystem
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARMv7 short-descriptor conversion at the hardware boundary
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#pragma once

BOOLEAN MiArmShortEncodePte(MI_PTE Value, BOOLEAN Root, PULONG Hardware);
BOOLEAN MiArmShortDecodePte(ULONG Hardware, BOOLEAN Root, PMI_PTE Value);

extern BOOLEAN MiArmShortDescriptors;
extern ULONG MiArmShortBootRoot;
ULONG MiArmShortLock(VOID);
VOID MiArmShortUnlock(ULONG State);
VOID MiArmShortHardwareStore(PULONG Slot, ULONG Value, BOOLEAN Break);
ULONG64 MiArmShortPhysicalAddress(PVOID Address);
__attribute__((noreturn)) VOID MiArmShortFailure(ULONG Code, ULONG64 Address, MI_PTE Value);
BOOLEAN MiArmShortLocate(ULONG64 Physical, PULONG64 Hardware, PBOOLEAN Root);
BOOLEAN MiArmShortStore(PMI_PTE Slot, MI_PTE Expected, MI_PTE Value, BOOLEAN Compare);
BOOLEAN MiArmShortPrepareBoot(ULONG RootFrame, ULONG FirstFrame, ULONG LastFrame);
