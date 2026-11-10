/*
 * PROJECT:     LiberNT VirtIO Input
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     VirtIO input event and HID report state
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#pragma once
#include <ntddk.h>

#define VI_REPORT_MAX 9

typedef struct _VI_EVENT
{
    USHORT Type;
    USHORT Code;
    LONG Value;
} VI_EVENT;
C_ASSERT(sizeof(VI_EVENT) == 8);

typedef struct _VI_INPUT_STATE
{
    BOOLEAN Keyboard;
    BOOLEAN Absolute;
    BOOLEAN Dirty;
    BOOLEAN Ready;
    UCHAR Modifiers;
    UCHAR Keys[16];
    UCHAR Buttons;
    LONG X, Y, Wheel;
    LONG Min[2], Max[2];
} VI_INPUT_STATE;

VOID ViDecodeEvent(VI_INPUT_STATE *State, const VI_EVENT *Event);
ULONG ViTakeReport(VI_INPUT_STATE *State, UCHAR Report[VI_REPORT_MAX]);
const UCHAR *ViReportDescriptor(BOOLEAN Keyboard, BOOLEAN Absolute, PULONG Length);
