/*
 * PROJECT:     LiberNT API tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Exercise production ARM32 kernel service stubs
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <syscalls.inc>

    TEXTAREA
    STUB_K ArmTestZero, 0
    STUB_K ArmTestFour, 4
    STUB_K ArmTestSeven, 7
    STUB_K ArmTestSeventeen, 17
    STUB_K ArmTestInvalid, 0
