/*
 * PROJECT:     LiberNT VirtIO Input
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Convert evdev events to keyboard and pointer HID reports
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include "reports.h"

static const UCHAR ViKeyboardDescriptor[] = {
    0x05,0x01,0x09,0x06,0xa1,0x01,0x85,0x01,
    0x05,0x07,0x19,0xe0,0x29,0xe7,0x15,0x00,0x25,0x01,
    0x75,0x01,0x95,0x08,0x81,0x02,
    0x75,0x08,0x95,0x01,0x81,0x03,
    0x05,0x08,0x19,0x01,0x29,0x05,0x75,0x01,0x95,0x05,0x91,0x02,
    0x75,0x03,0x95,0x01,0x91,0x03,
    0x05,0x07,0x19,0x00,0x29,0x73,0x15,0x00,0x25,0x73,
    0x75,0x08,0x95,0x06,0x81,0x00,0xc0
};
#define VI_POINTER_DESCRIPTOR(minlo,minhi,flags) \
    0x05,0x01,0x09,0x02,0xa1,0x01,0x85,0x01,0x09,0x01,0xa1,0x00, \
    0x05,0x09,0x19,0x01,0x29,0x05,0x15,0x00,0x25,0x01, \
    0x75,0x01,0x95,0x05,0x81,0x02,0x75,0x03,0x95,0x01,0x81,0x03, \
    0x05,0x01,0x09,0x30,0x09,0x31,0x16,minlo,minhi,0x26,0xff,0x7f, \
    0x75,0x10,0x95,0x02,0x81,flags, \
    0x09,0x38,0x15,0x81,0x25,0x7f,0x75,0x08,0x95,0x01,0x81,0x06,0xc0,0xc0
static const UCHAR ViTabletDescriptor[] = {VI_POINTER_DESCRIPTOR(0x00,0x00,0x02)};
static const UCHAR ViMouseDescriptor[] = {VI_POINTER_DESCRIPTOR(0x01,0x80,0x06)};

static const UCHAR ViKeyUsages[128] = {
    [1]=0x29,[2]=0x1e,[3]=0x1f,[4]=0x20,[5]=0x21,[6]=0x22,[7]=0x23,[8]=0x24,[9]=0x25,[10]=0x26,[11]=0x27,
    [12]=0x2d,[13]=0x2e,[14]=0x2a,[15]=0x2b,[16]=0x14,[17]=0x1a,[18]=0x08,[19]=0x15,[20]=0x17,[21]=0x1c,
    [22]=0x18,[23]=0x0c,[24]=0x12,[25]=0x13,[26]=0x2f,[27]=0x30,[28]=0x28,[29]=0xe0,[30]=0x04,[31]=0x16,
    [32]=0x07,[33]=0x09,[34]=0x0a,[35]=0x0b,[36]=0x0d,[37]=0x0e,[38]=0x0f,[39]=0x33,[40]=0x34,[41]=0x35,
    [42]=0xe1,[43]=0x31,[44]=0x1d,[45]=0x1b,[46]=0x06,[47]=0x19,[48]=0x05,[49]=0x11,[50]=0x10,[51]=0x36,
    [52]=0x37,[53]=0x38,[54]=0xe5,[55]=0x55,[56]=0xe2,[57]=0x2c,[58]=0x39,
    [59]=0x3a,[60]=0x3b,[61]=0x3c,[62]=0x3d,[63]=0x3e,[64]=0x3f,[65]=0x40,[66]=0x41,[67]=0x42,[68]=0x43,
    [69]=0x53,[70]=0x47,[71]=0x5f,[72]=0x60,[73]=0x61,[74]=0x56,[75]=0x5c,[76]=0x5d,[77]=0x5e,[78]=0x57,
    [79]=0x59,[80]=0x5a,[81]=0x5b,[82]=0x62,[83]=0x63,[86]=0x64,[87]=0x44,[88]=0x45,
    [96]=0x58,[97]=0xe4,[98]=0x54,[99]=0x46,[100]=0xe6,[102]=0x4a,[103]=0x52,[104]=0x4b,[105]=0x50,
    [106]=0x4f,[107]=0x4d,[108]=0x51,[109]=0x4e,[110]=0x49,[111]=0x4c,[119]=0x48,[125]=0xe3,[126]=0xe7,[127]=0x65
};

static LONG ViAdd(LONG Left, LONG Right)
{
    LONGLONG Sum = (LONGLONG)Left + Right;
    return (LONG)max((LONGLONG)(LONG)MINLONG, min((LONGLONG)MAXLONG, Sum));
}

VOID ViDecodeEvent(VI_INPUT_STATE *State, const VI_EVENT *Event)
{
    UCHAR Usage, Mask;
    LONG *Coordinate, Value;
    ULONG Axis;

    if (Event->Type == 0 && Event->Code == 0)
    {
        State->Ready |= State->Dirty;
        State->Dirty = FALSE;
        return;
    }
    if (State->Keyboard)
    {
        if (Event->Type != 1 || Event->Code >= RTL_NUMBER_OF(ViKeyUsages) ||
            Event->Value < 0 || Event->Value > 1)
            return;
        Usage = ViKeyUsages[Event->Code];
        if (!Usage) return;
        if (Usage >= 0xe0)
        {
            Mask = 1u << (Usage - 0xe0);
            if (Event->Value) State->Modifiers |= Mask;
            else State->Modifiers &= ~Mask;
        }
        else
        {
            Mask = 1u << (Usage % 8);
            if (Event->Value) State->Keys[Usage / 8] |= Mask;
            else State->Keys[Usage / 8] &= ~Mask;
        }
        State->Dirty = TRUE;
        return;
    }
    if (Event->Type == 1 && Event->Code >= 0x110 && Event->Code <= 0x114 &&
        (Event->Value == 0 || Event->Value == 1))
    {
        Mask = 1u << (Event->Code - 0x110);
        if (Event->Value) State->Buttons |= Mask;
        else State->Buttons &= ~Mask;
        State->Dirty = TRUE;
    }
    else if (Event->Type == 2 && Event->Code == 8)
    {
        State->Wheel = ViAdd(State->Wheel, Event->Value);
        State->Dirty = TRUE;
    }
    else if (Event->Code <= 1 &&
             ((Event->Type == 2 && !State->Absolute) || (Event->Type == 3 && State->Absolute)))
    {
        Axis = Event->Code;
        Coordinate = Axis ? &State->Y : &State->X;
        if (State->Absolute)
        {
            if (State->Max[Axis] <= State->Min[Axis]) return;
            Value = max(State->Min[Axis], min(State->Max[Axis], Event->Value));
            *Coordinate = (LONG)(((LONGLONG)Value - State->Min[Axis]) * 32767 /
                                 ((LONGLONG)State->Max[Axis] - State->Min[Axis]));
        }
        else
            *Coordinate = ViAdd(*Coordinate, Event->Value);
        State->Dirty = TRUE;
    }
}

ULONG ViTakeReport(VI_INPUT_STATE *State, UCHAR Report[VI_REPORT_MAX])
{
    ULONG Usage, Count = 0;
    LONG X, Y, Wheel;

    if (!State->Ready) return 0;
    RtlZeroMemory(Report, VI_REPORT_MAX);
    Report[0] = 1;
    if (State->Keyboard)
    {
        Report[1] = State->Modifiers;
        for (Usage = 4; Usage <= 0x73; ++Usage)
        {
            if (!(State->Keys[Usage / 8] & (1u << (Usage % 8)))) continue;
            if (Count < 6) Report[3 + Count] = (UCHAR)Usage;
            ++Count;
        }
        if (Count > 6) RtlFillMemory(Report + 3, 6, 1);
        State->Ready = FALSE;
        return 9;
    }
    X = max(-32767, min(32767, State->X));
    Y = max(-32767, min(32767, State->Y));
    Wheel = max(-127, min(127, State->Wheel));
    Report[1] = State->Buttons;
    Report[2] = (UCHAR)X;
    Report[3] = (UCHAR)((USHORT)X >> 8);
    Report[4] = (UCHAR)Y;
    Report[5] = (UCHAR)((USHORT)Y >> 8);
    Report[6] = (UCHAR)Wheel;
    if (!State->Absolute)
    {
        State->X -= X;
        State->Y -= Y;
    }
    State->Wheel -= Wheel;
    State->Ready = State->Wheel || (!State->Absolute && (State->X || State->Y));
    return 7;
}

const UCHAR *ViReportDescriptor(BOOLEAN Keyboard, BOOLEAN Absolute, PULONG Length)
{
    if (Keyboard)
    {
        *Length = sizeof(ViKeyboardDescriptor);
        return ViKeyboardDescriptor;
    }
    *Length = sizeof(ViTabletDescriptor);
    return Absolute ? ViTabletDescriptor : ViMouseDescriptor;
}
