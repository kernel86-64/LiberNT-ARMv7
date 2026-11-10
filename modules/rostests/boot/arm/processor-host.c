/*
 * PROJECT:     LiberNT boot tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Validate ARM MIDR processor-name decoding
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include "processor.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    assert(!strcmp(KiArmProcessorName(0x412fc091), "ARM Cortex-A9"));
    assert(!strcmp(KiArmProcessorName(0x410fc090), "ARM Cortex-A9"));
    assert(!strcmp(KiArmProcessorName(0x414fc0f0), "ARM Cortex-A15"));
    assert(!strcmp(KiArmProcessorName(0x412fc0f1), "ARM Cortex-A15"));
    assert(!KiArmProcessorName(0));
    assert(!KiArmProcessorName(0x514fc0f0));
    assert(!KiArmProcessorName(0x414ffff0));
    puts("ARM processor-name host tests: PASS");
    return 0;
}
