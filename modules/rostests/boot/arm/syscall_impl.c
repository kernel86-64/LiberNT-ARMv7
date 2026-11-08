/*
 * PROJECT:     LiberNT API tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Compile the production SVC dispatcher for the EFI test
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#define KiSoftwareInterruptHandler ArmTestRealSvcHandler
#include <ntoskrnl/ke/arm/syscall.c>
