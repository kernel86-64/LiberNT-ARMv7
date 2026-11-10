/*
 * PROJECT:     LiberNT API tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     VirtIO event to HID report regression
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */
#include <ntddk.h>
#include "../../../../drivers/input/virtinput/reports.c"

static VOID InputEvent(VI_INPUT_STATE *State, USHORT Type, USHORT Code, LONG Value)
{
    VI_EVENT Event = {Type, Code, Value};
    ViDecodeEvent(State, &Event);
}
BOOLEAN ArmTestInput(VOID)
{
    VI_INPUT_STATE State;
    UCHAR Report[VI_REPORT_MAX];
    BOOLEAN Passed;
    ULONG Index, Length;
    RtlZeroMemory(&State, sizeof(State));
    State.Keyboard = TRUE;
    InputEvent(&State, 1, 29, 1);
    InputEvent(&State, 1, 30, 1);
    Passed = ViTakeReport(&State, Report) == 0;
    InputEvent(&State, 0, 0, 0);
    Passed = ViTakeReport(&State, Report) == 9 && Report[0] == 1 &&
             Report[1] == 1 && Report[2] == 0 && Report[3] == 4 && Passed;
    InputEvent(&State, 1, 30, 2);
    InputEvent(&State, 0, 0, 0);
    Passed = !ViTakeReport(&State, Report) && Passed;
    InputEvent(&State, 1, 29, 0);
    InputEvent(&State, 1, 30, 0);
    InputEvent(&State, 1, 111, 1);
    InputEvent(&State, 0, 0, 0);
    Passed = ViTakeReport(&State, Report) == 9 && Report[1] == 0 && Report[3] == 0x4c && Passed;
    for (Index = 2; Index <= 8; ++Index) InputEvent(&State, 1, (USHORT)Index, 1);
    InputEvent(&State, 0, 0, 0);
    Passed = ViTakeReport(&State, Report) == 9 && Passed;
    for (Index = 3; Index < 9; ++Index) Passed = Report[Index] == 1 && Passed;
    RtlZeroMemory(&State, sizeof(State));
    InputEvent(&State, 2, 0, 40000);
    InputEvent(&State, 2, 1, -12);
    InputEvent(&State, 2, 8, 1);
    InputEvent(&State, 1, 0x110, 1);
    InputEvent(&State, 0, 0, 0);
    Passed = ViTakeReport(&State, Report) == 7 && Report[1] == 1 &&
             Report[2] == 0xff && Report[3] == 0x7f && Report[4] == 0xf4 &&
             Report[5] == 0xff && Report[6] == 1 && Passed;
    Passed = ViTakeReport(&State, Report) == 7 && Report[2] == 0x41 &&
             Report[3] == 0x1c && !Report[4] && !Report[5] && !Report[6] && Passed;
    Passed = !ViTakeReport(&State, Report) && Passed;
    RtlZeroMemory(&State, sizeof(State));
    State.Absolute = TRUE;
    State.Min[0] = State.Min[1] = -100;
    State.Max[0] = State.Max[1] = 100;
    InputEvent(&State, 3, 0, 200);
    InputEvent(&State, 3, 1, -200);
    InputEvent(&State, 0, 0, 0);
    Passed = ViTakeReport(&State, Report) == 7 && Report[2] == 0xff &&
             Report[3] == 0x7f && !Report[4] && !Report[5] && Passed;
    Passed = ViReportDescriptor(TRUE, FALSE, &Length) != NULL && Length > 0 && Passed;
    return Passed;
}
