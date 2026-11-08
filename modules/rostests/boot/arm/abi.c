/*
 * PROJECT:     LiberNT boot tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARMv7 UEFI ABI regression tests
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <uefildr.h>
#include <setjmp.h>
#include <setjmpex.h>
#include <math.h>

static EFI_SYSTEM_TABLE *Firmware;
static jmp_buf Buffer;
static volatile long Counter;
static DECLSPEC_ALIGN(8) volatile long long Counter64;

extern int ArmTestSetjmpRegisters(void);

C_ASSERT(sizeof(void *) == 4);
C_ASSERT(sizeof(_JUMP_BUFFER) == 112);
C_ASSERT(FIELD_OFFSET(_JUMP_BUFFER, D) == 48);

VOID NTAPI
RtlUnwind(PVOID Frame, PVOID Target, PEXCEPTION_RECORD Record, PVOID Value)
{
    UNREFERENCED_PARAMETER(Frame);
    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(Record);
    UNREFERENCED_PARAMETER(Value);
    Firmware->ConOut->OutputString(Firmware->ConOut, L"ARMv7 ABI tests: unexpected unwind\r\n");
    Firmware->RuntimeServices->ResetSystem(EfiResetShutdown, EFI_ABORTED, 0, NULL);
    for (;;) __asm__ __volatile__("wfi");
}

EFI_STATUS
EfiEntry(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    int Value;
    volatile double Input = -1.25;
    BOOLEAN Passed = TRUE;

    UNREFERENCED_PARAMETER(ImageHandle);
    Firmware = SystemTable;
    Counter = 40;
    Counter64 = 0x100000000LL;
    Passed = Passed && (_InterlockedAdd(&Counter, 2) == 42) && (Counter == 42);
    Passed = Passed && (_InterlockedAdd64(&Counter64, 2) == 0x100000002LL) &&
             (Counter64 == 0x100000002LL);
    Value = _setjmp(Buffer, NULL);
    if (Value == 0)
        longjmp(Buffer, 42);
    Passed = Passed && (Value == 42);
    Value = _setjmp(Buffer, NULL);
    if (Value == 0)
        longjmp(Buffer, 0);
    Passed = Passed && (Value == 1);
    Passed = Passed && ArmTestSetjmpRegisters();
    Value = _setjmp(Buffer, (void *)0x1234);
    Passed = Passed && (Value == 0) && (((_JUMP_BUFFER *)Buffer)->Frame == 0x1234);
    Passed = Passed && (floor(Input) == -2.0);
    Value = setjmp(Buffer);
    Passed = Passed && (Value == 0) && (((_JUMP_BUFFER *)Buffer)->Frame != 0);
    Input = 2.0;
    Passed = Passed && (pow(Input, 10.0) == 1024.0);
    Input = 0.0;
    Passed = Passed && (sin(Input) == 0.0) && (cos(Input) == 1.0) && (tan(Input) == 0.0);
    Input = 1000.0;
    Input = log10(Input) - 3.0;
    Passed = Passed && (Input > -1e-12) && (Input < 1e-12);
    Firmware->ConOut->OutputString(Firmware->ConOut,
        Passed ? L"ARMv7 ABI tests: PASS\r\n" : L"ARMv7 ABI tests: FAIL\r\n");
    Firmware->RuntimeServices->ResetSystem(EfiResetShutdown,
        Passed ? EFI_SUCCESS : EFI_ABORTED, 0, NULL);
    return Passed ? EFI_SUCCESS : EFI_ABORTED;
}
