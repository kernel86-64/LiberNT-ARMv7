/*
 * PROJECT:     LiberNT API tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARMv7 debugger processor control state
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ntoskrnl/include/ntoskrnl.h>

BOOLEAN ArmTestProcessorControl(VOID)
{
    KPROCESSOR_STATE Before, After;
    ULONG Original = _MoveFromCoprocessor(CP15_TPIDRURW);
    ULONG Marker = 0x12345678;
    BOOLEAN Passed;

    KiSaveProcessorControlState(&Before);
    _MoveToCoprocessor(Marker, CP15_TPIDRURW);
    KiSaveProcessorControlState(&After);
    Passed = After.SpecialRegisters.Cp15_Cr13_UsrRW == Marker &&
             Before.SpecialRegisters.Cp15_Cr13_UsrRW == Original &&
             Before.ArchState.Cp15_Cr0_CpuId == _MoveFromCoprocessor(CP15_MIDR) &&
             Before.ArchState.Cp15_Cr1_Control == _MoveFromCoprocessor(CP15_SCTLR) &&
             Before.ArchState.Cp15_Cr2_TtbControl == _MoveFromCoprocessor(CP15_TTBCR);
    KiRestoreProcessorControlState(&Before);
    KiSaveProcessorControlState(&After);
    return Passed && _MoveFromCoprocessor(CP15_TPIDRURW) == Original &&
           memcmp(&Before.SpecialRegisters, &After.SpecialRegisters,
                  sizeof(KSPECIAL_REGISTERS)) == 0;
}
