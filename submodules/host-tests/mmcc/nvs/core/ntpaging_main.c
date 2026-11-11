/*
 * PROJECT:     LiberNT API tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Standalone host runner for synchronous NVS paging I/O
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include "mmharness.h"

int TestFailures;
int TestChecks;
_Thread_local KIRQL MiHostIrql;

KIRQL MiHostRaiseIrql(KIRQL NewIrql)
{
    KIRQL OldIrql = MiHostIrql;
    CHECK(NewIrql >= OldIrql);
    MiHostIrql = NewIrql;
    return OldIrql;
}

VOID MiHostLowerIrql(KIRQL NewIrql)
{
    CHECK(NewIrql <= MiHostIrql);
    MiHostIrql = NewIrql;
}

int main(void)
{
    TestNtPaging();
    printf("ntpaging: %d checks, %d failures\n", TestChecks, TestFailures);
    return TestFailures != 0;
}
