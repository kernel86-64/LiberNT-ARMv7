/*
 * PROJECT:     LiberNT Storage
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     VirtIO MMIO block transport declarations
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#pragma once
#include <ntddk.h>
#include <ntdddisk.h>
#include <ntddstor.h>
#include <ntstrsafe.h>

#define VB_TAG 'klbV'
#define VB_QUEUE_SIZE 8
#define VB_DATA_OFFSET (3 * PAGE_SIZE)
#define VB_DATA_SIZE (64 * 1024)
#define VB_ARENA_SIZE (VB_DATA_OFFSET + VB_DATA_SIZE)

typedef struct _VB_DESCRIPTOR
{
    ULONGLONG Address;
    ULONG Length;
    USHORT Flags;
    USHORT Next;
} VB_DESCRIPTOR;
C_ASSERT(sizeof(VB_DESCRIPTOR) == 16);

typedef struct _VB_CONTROLLER
{
    PUCHAR Registers;
    PUCHAR Arena;
    PHYSICAL_ADDRESS ArenaPa;
    ULONGLONG Sectors;
    DISK_GEOMETRY Geometry;
    KSPIN_LOCK Lock;
    HANDLE Directory;
    ULONG Number;
    ULONG Version;
    ULONG Features;
    USHORT Available;
    USHORT Used;
    BOOLEAN Failed;
} VB_CONTROLLER;

typedef struct _VB_EXTENSION
{
    VB_CONTROLLER *Controller;
    ULONGLONG Start;
    ULONGLONG Sectors;
    ULONG Partition;
    UCHAR Type;
    BOOLEAN Boot;
} VB_EXTENSION;

PVOID VbPlatformMap(ULONG Index);
