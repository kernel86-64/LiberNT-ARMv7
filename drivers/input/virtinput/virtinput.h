/*
 * PROJECT:     LiberNT VirtIO Input
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     VirtIO input transport state
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */
#pragma once
#include "reports.h"
#include <hidport.h>

#define VI_EVENTS 64
#define VI_STATUS 8
#define VI_TAG 'nIiV'

typedef struct _VI_DESC
{
    ULONGLONG Address;
    ULONG Length;
    USHORT Flags, Next;
} VI_DESC;
typedef struct _VI_USED { ULONG Id, Length; } VI_USED;
C_ASSERT(sizeof(VI_DESC) == 16);
C_ASSERT(sizeof(VI_USED) == 8);

typedef struct _VI_QUEUE
{
    VI_DESC *Descriptors;
    volatile USHORT *Available;
    volatile USHORT *Used;
    VI_EVENT *Events;
    USHORT Size, LastUsed, NextAvailable;
} VI_QUEUE;

typedef struct _VI_DEVICE
{
    PHID_DEVICE_EXTENSION Hid;
    PDEVICE_OBJECT Device;
    PUCHAR Registers;
    ULONG Slot, Vector;
    PKINTERRUPT Interrupt;
    PVOID Dma;
    PHYSICAL_ADDRESS DmaAddress;
    VI_QUEUE EventQueue, StatusQueue;
    KDPC Dpc;
    KSPIN_LOCK Lock;
    IO_CSQ Csq;
    LIST_ENTRY Reads;
    IO_REMOVE_LOCK RemoveLock;
    volatile LONG Started;
    VI_INPUT_STATE State;
    HID_DEVICE_ATTRIBUTES Attributes;
    UCHAR Report[VI_REPORT_MAX];
    ULONG ReportLength;
    UCHAR LedMask, DesiredLeds, SentLeds, StatusBusy;
} VI_DEVICE;

PVOID ViPlatformMap(ULONG Slot, PULONG Vector);
static __inline ULONG ViRead(VI_DEVICE *Device, ULONG Offset)
{
    return READ_REGISTER_ULONG((PULONG)(Device->Registers + Offset));
}
static __inline VOID ViWrite(VI_DEVICE *Device, ULONG Offset, ULONG Value)
{
    WRITE_REGISTER_ULONG((PULONG)(Device->Registers + Offset), Value);
}
