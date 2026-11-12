/*
 * PROJECT:     LiberNT Atmel mXT540E touchscreen
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     HID minidriver for the GT-N7000 mXT540E touch controller
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ntifs.h>
#include <hidport.h>

#define MXT_TAG '45TM'

#define MXT_GPIO1_BASE      0x11400000
#define MXT_GPIO2_BASE      0x11000000

#define MXT_I2C_ADDRESS     0x4C

#define MXT_GPA1_CON        0x20
#define MXT_GPA1_DAT        0x24
#define MXT_GPA1_PUD        0x28
#define MXT_GPL0_CON        0xC0
#define MXT_GPL0_DAT        0xC4
#define MXT_GPL0_PUD        0xC8
#define MXT_GPX0_CON        0xC00
#define MXT_GPX0_DAT        0xC04
#define MXT_GPX0_PUD        0xC08
#define MXT_EINT40_CON      0xE00
#define MXT_EINT40_MASK     0xF00
#define MXT_EINT40_PEND     0xF40

#define MXT_GPA1_SDA_SHIFT  8
#define MXT_GPA1_SCL_SHIFT  12
#define MXT_GPA1_SDA_BIT    2
#define MXT_GPA1_SCL_BIT    3
#define MXT_GPA1_SDA_PUD    4
#define MXT_GPA1_SCL_PUD    6
#define MXT_GPL0_LDO_SHIFT  12
#define MXT_GPL0_LDO_BIT    3
#define MXT_GPL0_LDO_PUD    6
#define MXT_GPX0_CHG_SHIFT  16
#define MXT_GPX0_CHG_BIT    4
#define MXT_GPX0_CHG_PUD    8
#define MXT_EINT_CHG_BIT    (1UL << 4)

#define MXT_GPIO_FUNC_OUT   1
#define MXT_GPIO_FUNC_INPUT 0

#define MXT_INFO_SIZE       7
#define MXT_OBJECT_TABLE    7
#define MXT_OBJECT_ENTRY    6
#define MXT_MAX_OBJECTS     64
#define MXT_MAX_MESSAGE     16
#define MXT_MAX_DRAIN       64

#define MXT_T5              5
#define MXT_T9              9
#define MXT_T9_DETECT       0x80
#define MXT_T9_RELEASE      0x20
#define MXT_REPORT_NONE     0xFF

#define MXT_MAX_CONTACTS    10
#define MXT_REPORT_TOUCH    1
#define MXT_REPORT_MAXCOUNT 2
#define MXT_SCREEN_X        800
#define MXT_SCREEN_Y        1280

#define MXT_POLL_MS         10
#define MXT_START_DELAY_US  130000

#pragma pack(push, 1)

typedef struct _MXT_CONTACT
{
    UCHAR Tip;
    UCHAR ContactId;
    USHORT X;
    USHORT Y;
} MXT_CONTACT, *PMXT_CONTACT;

typedef struct _MXT_TOUCH_REPORT
{
    UCHAR ReportId;
    MXT_CONTACT Contacts[MXT_MAX_CONTACTS];
    UCHAR ContactCount;
} MXT_TOUCH_REPORT, *PMXT_TOUCH_REPORT;

typedef struct _MXT_MAXCOUNT_REPORT
{
    UCHAR ReportId;
    UCHAR MaxCount;
} MXT_MAXCOUNT_REPORT, *PMXT_MAXCOUNT_REPORT;

#pragma pack(pop)

typedef struct _MXT_FINGER
{
    BOOLEAN Active;
    BOOLEAN Down;
    USHORT X;
    USHORT Y;
} MXT_FINGER, *PMXT_FINGER;

C_ASSERT(sizeof(MXT_CONTACT) == 6);
C_ASSERT(sizeof(MXT_TOUCH_REPORT) == 1 + MXT_MAX_CONTACTS * 6 + 1);
C_ASSERT(sizeof(MXT_MAXCOUNT_REPORT) == 2);

typedef struct _MXT_DEVICE
{
    PHID_DEVICE_EXTENSION Hid;
    PDEVICE_OBJECT Device;
    PVOID Gpio1;
    PVOID Gpio2;

    KSPIN_LOCK Lock;
    IO_CSQ Csq;
    LIST_ENTRY Reads;
    IO_REMOVE_LOCK RemoveLock;

    KEVENT StopEvent;
    KEVENT WakeEvent;
    PKTHREAD Thread;

    volatile LONG Started;

    USHORT MessageAddress;
    UCHAR MessageSize;
    UCHAR T9FirstReport;
    UCHAR T9Reports;
    USHORT XRange;
    USHORT YRange;
    UCHAR XDropBits;
    UCHAR YDropBits;

    MXT_FINGER Fingers[MXT_MAX_CONTACTS];
    MXT_TOUCH_REPORT Report;
    ULONG ReportLength;
    BOOLEAN ReportPending;

    HID_DEVICE_ATTRIBUTES Attributes;
} MXT_DEVICE, *PMXT_DEVICE;

#define MXT_FINGER_ITEM \
    0x05, 0x0D, 0x09, 0x22, 0xA1, 0x02, \
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x01, 0x09, 0x42, 0x81, 0x02, \
    0x95, 0x07, 0x81, 0x03, \
    0x75, 0x08, 0x95, 0x01, 0x25, 0x09, 0x09, 0x51, 0x81, 0x02, \
    0x05, 0x01, 0x75, 0x10, 0x55, 0x0E, 0x65, 0x11, 0x35, 0x00, \
    0x26, 0x1F, 0x03, 0x46, 0xA8, 0x02, 0x09, 0x30, 0x81, 0x02, \
    0x26, 0xFF, 0x04, 0x46, 0x42, 0x04, 0x09, 0x31, 0x81, 0x02, \
    0xC0

static UCHAR MxtReportDescriptor[] =
{
    0x05, 0x0D, 0x09, 0x04, 0xA1, 0x01,
    0x85, MXT_REPORT_TOUCH,
    MXT_FINGER_ITEM, MXT_FINGER_ITEM, MXT_FINGER_ITEM, MXT_FINGER_ITEM, MXT_FINGER_ITEM,
    MXT_FINGER_ITEM, MXT_FINGER_ITEM, MXT_FINGER_ITEM, MXT_FINGER_ITEM, MXT_FINGER_ITEM,
    0x05, 0x0D, 0x09, 0x54, 0x75, 0x08, 0x95, 0x01, 0x15, 0x00, 0x25, MXT_MAX_CONTACTS,
    0x81, 0x02,
    0x85, MXT_REPORT_MAXCOUNT, 0x09, 0x55, 0x25, MXT_MAX_CONTACTS, 0xB1, 0x02,
    0xC0
};

static HID_DESCRIPTOR MxtHidDescriptor =
{
    sizeof(HID_DESCRIPTOR),
    HID_HID_DESCRIPTOR_TYPE,
    0x0100,
    0,
    1,
    {{HID_REPORT_DESCRIPTOR_TYPE, sizeof(MxtReportDescriptor)}}
};

static ULONG MxtReadRegister(PVOID Base, ULONG Offset)
{
    return READ_REGISTER_ULONG((PULONG)((PUCHAR)Base + Offset));
}

static VOID MxtWriteRegister(PVOID Base, ULONG Offset, ULONG Value)
{
    WRITE_REGISTER_ULONG((PULONG)((PUCHAR)Base + Offset), Value);
}

static VOID MxtUpdateRegister(PVOID Base, ULONG Offset, ULONG Mask, ULONG Value)
{
    MxtWriteRegister(Base, Offset, (MxtReadRegister(Base, Offset) & ~Mask) | (Value & Mask));
}

static VOID MxtI2cDelay(VOID)
{
    KeStallExecutionProcessor(3);
}

static VOID MxtSda(PMXT_DEVICE Device, BOOLEAN High)
{
    if (High)
        MxtUpdateRegister(Device->Gpio1, MXT_GPA1_CON, 0x0FUL << MXT_GPA1_SDA_SHIFT, 0);
    else
    {
        MxtUpdateRegister(Device->Gpio1, MXT_GPA1_DAT, 1UL << MXT_GPA1_SDA_BIT, 0);
        MxtUpdateRegister(Device->Gpio1, MXT_GPA1_CON, 0x0FUL << MXT_GPA1_SDA_SHIFT,
                          MXT_GPIO_FUNC_OUT << MXT_GPA1_SDA_SHIFT);
    }
}

static VOID MxtScl(PMXT_DEVICE Device, BOOLEAN High)
{
    if (High)
        MxtUpdateRegister(Device->Gpio1, MXT_GPA1_CON, 0x0FUL << MXT_GPA1_SCL_SHIFT, 0);
    else
    {
        MxtUpdateRegister(Device->Gpio1, MXT_GPA1_DAT, 1UL << MXT_GPA1_SCL_BIT, 0);
        MxtUpdateRegister(Device->Gpio1, MXT_GPA1_CON, 0x0FUL << MXT_GPA1_SCL_SHIFT,
                          MXT_GPIO_FUNC_OUT << MXT_GPA1_SCL_SHIFT);
    }
}

static BOOLEAN MxtSdaRead(PMXT_DEVICE Device)
{
    return (MxtReadRegister(Device->Gpio1, MXT_GPA1_DAT) & (1UL << MXT_GPA1_SDA_BIT)) != 0;
}

static BOOLEAN MxtSclHigh(PMXT_DEVICE Device)
{
    ULONG Index;

    MxtScl(Device, TRUE);
    for (Index = 0; Index < 1000; ++Index)
    {
        if (MxtReadRegister(Device->Gpio1, MXT_GPA1_DAT) & (1UL << MXT_GPA1_SCL_BIT))
            return TRUE;
        KeStallExecutionProcessor(1);
    }
    return FALSE;
}

static BOOLEAN MxtI2cStart(PMXT_DEVICE Device)
{
    MxtSda(Device, TRUE);
    if (!MxtSclHigh(Device))
        return FALSE;
    MxtI2cDelay();
    MxtSda(Device, FALSE);
    MxtI2cDelay();
    MxtScl(Device, FALSE);
    MxtI2cDelay();
    return TRUE;
}

static VOID MxtI2cStop(PMXT_DEVICE Device)
{
    MxtSda(Device, FALSE);
    MxtScl(Device, FALSE);
    MxtI2cDelay();
    MxtSclHigh(Device);
    MxtI2cDelay();
    MxtSda(Device, TRUE);
    MxtI2cDelay();
}

static BOOLEAN MxtI2cWriteByte(PMXT_DEVICE Device, UCHAR Value)
{
    ULONG Bit;
    BOOLEAN Ack;

    for (Bit = 0x80; Bit; Bit >>= 1)
    {
        MxtSda(Device, (Value & Bit) != 0);
        MxtI2cDelay();
        if (!MxtSclHigh(Device))
            return FALSE;
        MxtI2cDelay();
        MxtScl(Device, FALSE);
        MxtI2cDelay();
    }
    MxtSda(Device, TRUE);
    MxtI2cDelay();
    if (!MxtSclHigh(Device))
        return FALSE;
    MxtI2cDelay();
    Ack = !MxtSdaRead(Device);
    MxtScl(Device, FALSE);
    MxtI2cDelay();
    return Ack;
}

static BOOLEAN MxtI2cReadByte(PMXT_DEVICE Device, BOOLEAN Ack, PUCHAR Value)
{
    UCHAR Data = 0;
    ULONG Bit;

    MxtSda(Device, TRUE);
    for (Bit = 0; Bit < 8; ++Bit)
    {
        MxtI2cDelay();
        if (!MxtSclHigh(Device))
            return FALSE;
        MxtI2cDelay();
        Data = (UCHAR)((Data << 1) | (MxtSdaRead(Device) ? 1 : 0));
        MxtScl(Device, FALSE);
        MxtI2cDelay();
    }
    MxtSda(Device, !Ack);
    MxtI2cDelay();
    if (!MxtSclHigh(Device))
        return FALSE;
    MxtI2cDelay();
    MxtScl(Device, FALSE);
    MxtI2cDelay();
    MxtSda(Device, TRUE);
    *Value = Data;
    return TRUE;
}

static NTSTATUS MxtRead(PMXT_DEVICE Device, USHORT Register, PVOID Buffer, ULONG Length)
{
    PUCHAR Data = Buffer;
    ULONG Index;
    BOOLEAN Ack;

    Ack = MxtI2cStart(Device);
    if (Ack)
        Ack = MxtI2cWriteByte(Device, (UCHAR)(MXT_I2C_ADDRESS << 1));
    if (Ack)
        Ack = MxtI2cWriteByte(Device, (UCHAR)Register);
    if (Ack)
        Ack = MxtI2cWriteByte(Device, (UCHAR)(Register >> 8));
    if (Ack)
        Ack = MxtI2cStart(Device);
    if (Ack)
        Ack = MxtI2cWriteByte(Device, (UCHAR)((MXT_I2C_ADDRESS << 1) | 1));
    if (!Ack)
    {
        MxtI2cStop(Device);
        return STATUS_IO_DEVICE_ERROR;
    }
    for (Index = 0; Index < Length; ++Index)
    {
        if (!MxtI2cReadByte(Device, Index != Length - 1, &Data[Index]))
        {
            MxtI2cStop(Device);
            return STATUS_IO_TIMEOUT;
        }
    }
    MxtI2cStop(Device);
    return STATUS_SUCCESS;
}

static NTSTATUS MxtHardwareMap(PMXT_DEVICE Device)
{
    PHYSICAL_ADDRESS Address;

    Address.QuadPart = MXT_GPIO1_BASE;
    Device->Gpio1 = MmMapIoSpace(Address, PAGE_SIZE, MmNonCached);
    Address.QuadPart = MXT_GPIO2_BASE;
    Device->Gpio2 = MmMapIoSpace(Address, PAGE_SIZE, MmNonCached);
    if (!Device->Gpio1 || !Device->Gpio2)
        return STATUS_INSUFFICIENT_RESOURCES;
    return STATUS_SUCCESS;
}

static VOID MxtHardwareUnmap(PMXT_DEVICE Device)
{
    if (Device->Gpio1)
        MmUnmapIoSpace(Device->Gpio1, PAGE_SIZE);
    if (Device->Gpio2)
        MmUnmapIoSpace(Device->Gpio2, PAGE_SIZE);
    Device->Gpio1 = NULL;
    Device->Gpio2 = NULL;
}

static VOID MxtPowerOn(PMXT_DEVICE Device)
{
    MxtUpdateRegister(Device->Gpio1, MXT_GPA1_CON,
                      0x0FUL << MXT_GPA1_SDA_SHIFT, MXT_GPIO_FUNC_INPUT << MXT_GPA1_SDA_SHIFT);
    MxtUpdateRegister(Device->Gpio1, MXT_GPA1_CON,
                      0x0FUL << MXT_GPA1_SCL_SHIFT, MXT_GPIO_FUNC_INPUT << MXT_GPA1_SCL_SHIFT);
    MxtUpdateRegister(Device->Gpio1, MXT_GPA1_PUD, 0x03UL << MXT_GPA1_SDA_PUD, 0x03UL << MXT_GPA1_SDA_PUD);
    MxtUpdateRegister(Device->Gpio1, MXT_GPA1_PUD, 0x03UL << MXT_GPA1_SCL_PUD, 0x03UL << MXT_GPA1_SCL_PUD);

    MxtUpdateRegister(Device->Gpio2, MXT_GPL0_CON,
                      0x0FUL << MXT_GPL0_LDO_SHIFT, MXT_GPIO_FUNC_OUT << MXT_GPL0_LDO_SHIFT);
    MxtUpdateRegister(Device->Gpio2, MXT_GPL0_PUD, 0x03UL << MXT_GPL0_LDO_PUD, 0);
    if (!(MxtReadRegister(Device->Gpio2, MXT_GPL0_DAT) & (1UL << MXT_GPL0_LDO_BIT)))
    {
        MxtUpdateRegister(Device->Gpio2, MXT_GPL0_DAT, 1UL << MXT_GPL0_LDO_BIT,
                          1UL << MXT_GPL0_LDO_BIT);
        KeStallExecutionProcessor(MXT_START_DELAY_US);
    }

    MxtUpdateRegister(Device->Gpio2, MXT_GPX0_CON,
                      0x0FUL << MXT_GPX0_CHG_SHIFT, MXT_GPIO_FUNC_INPUT << MXT_GPX0_CHG_SHIFT);
    MxtUpdateRegister(Device->Gpio2, MXT_GPX0_PUD, 0x03UL << MXT_GPX0_CHG_PUD, 0);
    MxtUpdateRegister(Device->Gpio2, MXT_EINT40_MASK, MXT_EINT_CHG_BIT, MXT_EINT_CHG_BIT);
    MxtWriteRegister(Device->Gpio2, MXT_EINT40_PEND, MXT_EINT_CHG_BIT);
}

static NTSTATUS MxtProbe(PMXT_DEVICE Device)
{
    UCHAR Info[MXT_INFO_SIZE];
    UCHAR Table[MXT_MAX_OBJECTS * MXT_OBJECT_ENTRY];
    UCHAR T9[22];
    PUCHAR Object;
    USHORT T9Address = 0;
    UCHAR ReportId = 1;
    ULONG Index;
    NTSTATUS Status;

    Status = MxtRead(Device, 0, Info, sizeof(Info));
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("mxt540e: info read failed 0x%08lx\n", Status);
        return Status;
    }
    DbgPrint("mxt540e: family 0x%02x variant 0x%02x version 0x%02x build 0x%02x objects %u\n",
             Info[0], Info[1], Info[2], Info[3], Info[6]);
    if (Info[6] > MXT_MAX_OBJECTS)
        return STATUS_BUFFER_OVERFLOW;

    Status = MxtRead(Device, MXT_OBJECT_TABLE, Table, Info[6] * MXT_OBJECT_ENTRY);
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("mxt540e: object table read failed 0x%08lx\n", Status);
        return Status;
    }

    for (Index = 0; Index < Info[6]; ++Index)
    {
        Object = Table + Index * MXT_OBJECT_ENTRY;
        if (Object[0] == MXT_T5)
        {
            Device->MessageAddress = Object[1] | (Object[2] << 8);
            Device->MessageSize = (UCHAR)min(Object[3] + 1, MXT_MAX_MESSAGE);
        }
        else if (Object[0] == MXT_T9)
        {
            T9Address = Object[1] | (Object[2] << 8);
            Device->T9FirstReport = ReportId;
            Device->T9Reports = Object[5];
        }
        ReportId = (UCHAR)(ReportId + Object[5] * (Object[4] + 1));
    }
    if (!Device->MessageAddress || !T9Address)
    {
        for (Index = 0; Index < Info[6]; ++Index)
        {
            Object = Table + Index * MXT_OBJECT_ENTRY;
            DbgPrint("mxt540e: obj %lu type=%u addr=%04x size=%u inst=%u reports=%u\n",
                     Index, Object[0], Object[1] | (Object[2] << 8),
                     Object[3], Object[4], Object[5]);
        }
        DbgPrint("mxt540e: no T5 or T9 object\n");
        return STATUS_NOT_FOUND;
    }

    Status = MxtRead(Device, T9Address, T9, sizeof(T9));
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("mxt540e: T9 read failed 0x%08lx\n", Status);
        return Status;
    }
    Device->XRange = T9[18] | (T9[19] << 8);
    Device->YRange = T9[20] | (T9[21] << 8);
    if (T9[9] & 1)
    {
        Device->XDropBits = ((T9[21] & 0xC) == 0) ? 2 : 0;
        Device->YDropBits = ((T9[19] & 0xC) == 0) ? 2 : 0;
    }
    else
    {
        Device->XDropBits = ((T9[19] & 0xC) == 0) ? 2 : 0;
        Device->YDropBits = ((T9[21] & 0xC) == 0) ? 2 : 0;
    }
    DbgPrint("mxt540e: T5 0x%x/%u T9 0x%x reports %u range %ux%u\n",
             Device->MessageAddress, Device->MessageSize, T9Address,
             Device->T9Reports, Device->XRange + 1, Device->YRange + 1);
    return STATUS_SUCCESS;
}

static VOID MxtHandleMessage(PMXT_DEVICE Device, PUCHAR Message)
{
    PMXT_FINGER Finger;
    ULONG Id;
    ULONG X, Y;

    if (Message[0] < Device->T9FirstReport ||
        Message[0] >= Device->T9FirstReport + Device->T9Reports)
        return;
    Id = Message[0] - Device->T9FirstReport;
    if (Id >= MXT_MAX_CONTACTS)
        return;

    Finger = &Device->Fingers[Id];
    if (Message[1] & MXT_T9_RELEASE)
    {
        Finger->Down = FALSE;
    }
    else if (Message[1] & MXT_T9_DETECT)
    {
        X = (((ULONG)Message[2] << 4) | (Message[4] >> 4)) >> Device->XDropBits;
        Y = (((ULONG)Message[3] << 4) | (Message[4] & 0xF)) >> Device->YDropBits;
        X = min(X, Device->XRange);
        Y = min(Y, Device->YRange);
        Finger->X = (USHORT)(X * MXT_SCREEN_X / (Device->XRange + 1));
        Finger->Y = (USHORT)(Y * MXT_SCREEN_Y / (Device->YRange + 1));
        Finger->Down = TRUE;
    }
}

static VOID MxtDeliverReport(PMXT_DEVICE Device)
{
    MXT_TOUCH_REPORT Report;
    PIRP Irp;
    KIRQL Irql;
    ULONG Length;

    if (Device->ReportPending)
        return;

    RtlZeroMemory(&Report, sizeof(Report));
    Report.ReportId = MXT_REPORT_TOUCH;
    for (Length = 0; Length < MXT_MAX_CONTACTS; ++Length)
    {
        PMXT_FINGER Finger = &Device->Fingers[Length];

        if (!Finger->Down && !Finger->Active)
            continue;
        Report.Contacts[Report.ContactCount].Tip = Finger->Down ? 1 : 0;
        Report.Contacts[Report.ContactCount].ContactId = (UCHAR)Length;
        Report.Contacts[Report.ContactCount].X = Finger->X;
        Report.Contacts[Report.ContactCount].Y = Finger->Y;
        Finger->Active = Finger->Down;
        Report.ContactCount++;
    }
    if (!Report.ContactCount)
        return;

    KeAcquireSpinLock(&Device->Lock, &Irql);
    Device->Report = Report;
    Device->ReportLength = sizeof(Report);
    Device->ReportPending = TRUE;
    KeReleaseSpinLock(&Device->Lock, Irql);
    KeSetEvent(&Device->WakeEvent, IO_NO_INCREMENT, FALSE);

    for (;;)
    {
        KeAcquireSpinLock(&Device->Lock, &Irql);
        if (!Device->ReportPending)
        {
            KeReleaseSpinLock(&Device->Lock, Irql);
            return;
        }
        KeReleaseSpinLock(&Device->Lock, Irql);

        Irp = IoCsqRemoveNextIrp(&Device->Csq, NULL);
        if (!Irp)
            return;

        KeAcquireSpinLock(&Device->Lock, &Irql);
        Length = Device->ReportLength;
        Report = Device->Report;
        Device->ReportPending = FALSE;
        KeReleaseSpinLock(&Device->Lock, Irql);

        if (!Irp->UserBuffer || IoGetCurrentIrpStackLocation(Irp)->Parameters.DeviceIoControl.OutputBufferLength < Length)
        {
            Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
            Irp->IoStatus.Information = 0;
        }
        else
        {
            RtlCopyMemory(Irp->UserBuffer, &Report, Length);
            Irp->IoStatus.Status = STATUS_SUCCESS;
            Irp->IoStatus.Information = Length;
        }
        IoReleaseRemoveLock(&Device->RemoveLock, Irp);
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    }
}

static BOOLEAN MxtChargeLow(PMXT_DEVICE Device)
{
    return (MxtReadRegister(Device->Gpio2, MXT_GPX0_DAT) & (1UL << MXT_GPX0_CHG_BIT)) == 0;
}

static VOID MxtDrain(PMXT_DEVICE Device)
{
    UCHAR Message[MXT_MAX_MESSAGE];
    ULONG Count;
    BOOLEAN Any = FALSE;

    for (Count = 0; Count < MXT_MAX_DRAIN; ++Count)
    {
        if (!MxtChargeLow(Device))
            break;
        if (!NT_SUCCESS(MxtRead(Device, Device->MessageAddress, Message, Device->MessageSize)) ||
            Message[0] == MXT_REPORT_NONE)
            break;
        MxtHandleMessage(Device, Message);
        Any = TRUE;
    }
    if (Any)
        MxtDeliverReport(Device);
}

static VOID NTAPI MxtWorker(PVOID Context)
{
    PMXT_DEVICE Device = Context;
    PVOID Objects[2] = {&Device->StopEvent, &Device->WakeEvent};
    LARGE_INTEGER Timeout;
    NTSTATUS Status;
    PIRP Irp;

    Timeout.QuadPart = -((LONGLONG)MXT_POLL_MS * 10000);
    for (;;)
    {
        Status = KeWaitForMultipleObjects(2, Objects, WaitAny, Executive, KernelMode,
                                          FALSE, &Timeout, NULL);
        if (Status == STATUS_WAIT_0)
            break;
        if (Status == STATUS_WAIT_1)
            KeResetEvent(&Device->WakeEvent);
        MxtDrain(Device);
        MxtDeliverReport(Device);
    }

    while ((Irp = IoCsqRemoveNextIrp(&Device->Csq, NULL)) != NULL)
    {
        Irp->IoStatus.Status = STATUS_DEVICE_NOT_READY;
        Irp->IoStatus.Information = 0;
        IoReleaseRemoveLock(&Device->RemoveLock, Irp);
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    }
    PsTerminateSystemThread(STATUS_SUCCESS);
}

static NTSTATUS MxtComplete(PMXT_DEVICE Device, PIRP Irp, NTSTATUS Status, ULONG Length)
{
    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = Length;
    IoReleaseRemoveLock(&Device->RemoveLock, Irp);
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

static NTSTATUS NTAPI MxtInsert(PIO_CSQ Csq, PIRP Irp, PVOID Context)
{
    PMXT_DEVICE Device = CONTAINING_RECORD(Csq, MXT_DEVICE, Csq);
    UNREFERENCED_PARAMETER(Context);
    if (!Device->Started)
        return STATUS_DEVICE_NOT_READY;
    InsertTailList(&Device->Reads, &Irp->Tail.Overlay.ListEntry);
    KeSetEvent(&Device->WakeEvent, IO_NO_INCREMENT, FALSE);
    return STATUS_SUCCESS;
}

static VOID NTAPI MxtRemove(PIO_CSQ Csq, PIRP Irp)
{
    UNREFERENCED_PARAMETER(Csq);
    RemoveEntryList(&Irp->Tail.Overlay.ListEntry);
}

static PIRP NTAPI MxtPeek(PIO_CSQ Csq, PIRP Irp, PVOID Context)
{
    PMXT_DEVICE Device = CONTAINING_RECORD(Csq, MXT_DEVICE, Csq);
    PLIST_ENTRY Entry = Irp ? Irp->Tail.Overlay.ListEntry.Flink : Device->Reads.Flink;
    UNREFERENCED_PARAMETER(Context);
    return (Entry == &Device->Reads) ? NULL : CONTAINING_RECORD(Entry, IRP, Tail.Overlay.ListEntry);
}

static VOID NTAPI MxtAcquire(PIO_CSQ Csq, PKIRQL Irql)
{
    KeAcquireSpinLock(&CONTAINING_RECORD(Csq, MXT_DEVICE, Csq)->Lock, Irql);
}

static VOID NTAPI MxtRelease(PIO_CSQ Csq, KIRQL Irql)
{
    KeReleaseSpinLock(&CONTAINING_RECORD(Csq, MXT_DEVICE, Csq)->Lock, Irql);
}

static VOID NTAPI MxtCancel(PIO_CSQ Csq, PIRP Irp)
{
    PMXT_DEVICE Device = CONTAINING_RECORD(Csq, MXT_DEVICE, Csq);
    Irp->IoStatus.Status = STATUS_CANCELLED;
    Irp->IoStatus.Information = 0;
    IoReleaseRemoveLock(&Device->RemoveLock, Irp);
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
}

static NTSTATUS MxtStart(PMXT_DEVICE Device)
{
    HANDLE ThreadHandle;
    NTSTATUS Status;

    if (Device->Started)
        return STATUS_SUCCESS;
    Status = MxtHardwareMap(Device);
    if (!NT_SUCCESS(Status))
    {
        MxtHardwareUnmap(Device);
        return Status;
    }

    MxtPowerOn(Device);
    Status = MxtProbe(Device);
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("mxt540e: probe failed 0x%08lx\n", Status);
        MxtHardwareUnmap(Device);
        return Status;
    }

    KeInitializeEvent(&Device->StopEvent, NotificationEvent, FALSE);
    KeInitializeEvent(&Device->WakeEvent, SynchronizationEvent, FALSE);
    RtlZeroMemory(Device->Fingers, sizeof(Device->Fingers));
    Device->ReportPending = FALSE;
    Device->ReportLength = 0;
    Status = PsCreateSystemThread(&ThreadHandle, THREAD_ALL_ACCESS, NULL, NULL, NULL,
                                  MxtWorker, Device);
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("mxt540e: worker creation failed 0x%08lx\n", Status);
        MxtHardwareUnmap(Device);
        return Status;
    }
    Status = ObReferenceObjectByHandle(ThreadHandle, SYNCHRONIZE, *PsThreadType,
                                       KernelMode, (PVOID *)&Device->Thread, NULL);
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("mxt540e: worker reference failed 0x%08lx\n", Status);
        KeSetEvent(&Device->StopEvent, IO_NO_INCREMENT, FALSE);
        ZwWaitForSingleObject(ThreadHandle, FALSE, NULL);
        ZwClose(ThreadHandle);
        MxtHardwareUnmap(Device);
        return Status;
    }
    ZwClose(ThreadHandle);

    Device->Started = TRUE;
    DbgPrint("mxt540e: started, %u contacts\n", MXT_MAX_CONTACTS);
    return STATUS_SUCCESS;
}

static VOID MxtStop(PMXT_DEVICE Device)
{
    PIRP Irp;

    Device->Started = FALSE;
    if (Device->Thread)
    {
        KeSetEvent(&Device->StopEvent, IO_NO_INCREMENT, FALSE);
        KeWaitForSingleObject(Device->Thread, Executive, KernelMode, FALSE, NULL);
        ObDereferenceObject(Device->Thread);
        Device->Thread = NULL;
    }

    while ((Irp = IoCsqRemoveNextIrp(&Device->Csq, NULL)) != NULL)
    {
        Irp->IoStatus.Status = STATUS_DEVICE_NOT_READY;
        Irp->IoStatus.Information = 0;
        IoReleaseRemoveLock(&Device->RemoveLock, Irp);
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    }
    MxtHardwareUnmap(Device);
}

static NTSTATUS NTAPI MxtLowerComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(Irp);
    KeSetEvent(Context, IO_NO_INCREMENT, FALSE);
    return STATUS_MORE_PROCESSING_REQUIRED;
}

static NTSTATUS MxtForwardSync(PMXT_DEVICE Device, PIRP Irp)
{
    KEVENT Event;
    NTSTATUS Status;

    KeInitializeEvent(&Event, NotificationEvent, FALSE);
    IoCopyCurrentIrpStackLocationToNext(Irp);
    IoSetCompletionRoutine(Irp, MxtLowerComplete, &Event, TRUE, TRUE, TRUE);
    Status = IoCallDriver(Device->Hid->NextDeviceObject, Irp);
    if (Status == STATUS_PENDING)
        KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
    return Irp->IoStatus.Status;
}

static NTSTATUS NTAPI MxtPnp(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PMXT_DEVICE Device = ((PHID_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->MiniDeviceExtension;
    PIO_STACK_LOCATION Stack = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS Status = IoAcquireRemoveLock(&Device->RemoveLock, Irp);

    if (!NT_SUCCESS(Status))
    {
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    switch (Stack->MinorFunction)
    {
        case IRP_MN_START_DEVICE:
            Status = MxtForwardSync(Device, Irp);
            if (NT_SUCCESS(Status))
                Status = MxtStart(Device);
            return MxtComplete(Device, Irp, Status, 0);

        case IRP_MN_STOP_DEVICE:
        case IRP_MN_SURPRISE_REMOVAL:
            MxtStop(Device);
            Irp->IoStatus.Status = STATUS_SUCCESS;
            break;

        case IRP_MN_REMOVE_DEVICE:
            MxtStop(Device);
            IoReleaseRemoveLockAndWait(&Device->RemoveLock, Irp);
            Irp->IoStatus.Status = STATUS_SUCCESS;
            IoSkipCurrentIrpStackLocation(Irp);
            return IoCallDriver(Device->Hid->NextDeviceObject, Irp);
    }

    IoReleaseRemoveLock(&Device->RemoveLock, Irp);
    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(Device->Hid->NextDeviceObject, Irp);
}

static NTSTATUS MxtCopy(PMXT_DEVICE Device, PIRP Irp, const VOID *Data, ULONG Length)
{
    ULONG OutputLength = IoGetCurrentIrpStackLocation(Irp)->Parameters.DeviceIoControl.OutputBufferLength;

    if (!Irp->UserBuffer || OutputLength < Length)
        return MxtComplete(Device, Irp, STATUS_BUFFER_TOO_SMALL, 0);
    RtlCopyMemory(Irp->UserBuffer, Data, Length);
    return MxtComplete(Device, Irp, STATUS_SUCCESS, Length);
}

static NTSTATUS NTAPI MxtControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PMXT_DEVICE Device = ((PHID_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->MiniDeviceExtension;
    PIO_STACK_LOCATION Stack = IoGetCurrentIrpStackLocation(Irp);
    PHID_XFER_PACKET Packet;
    MXT_MAXCOUNT_REPORT MaxCount;
    NTSTATUS Status = IoAcquireRemoveLock(&Device->RemoveLock, Irp);
    KIRQL Irql;
    ULONG Code;

    if (!NT_SUCCESS(Status))
    {
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    Code = Stack->Parameters.DeviceIoControl.IoControlCode;
    if (!Device->Started &&
        Code != IOCTL_HID_GET_DEVICE_DESCRIPTOR &&
        Code != IOCTL_HID_GET_REPORT_DESCRIPTOR &&
        Code != IOCTL_HID_GET_DEVICE_ATTRIBUTES)
        return MxtComplete(Device, Irp, STATUS_DEVICE_NOT_READY, 0);

    switch (Code)
    {
        case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
            return MxtCopy(Device, Irp, &MxtHidDescriptor, sizeof(MxtHidDescriptor));

        case IOCTL_HID_GET_REPORT_DESCRIPTOR:
            return MxtCopy(Device, Irp, MxtReportDescriptor, sizeof(MxtReportDescriptor));

        case IOCTL_HID_GET_DEVICE_ATTRIBUTES:
            return MxtCopy(Device, Irp, &Device->Attributes, sizeof(Device->Attributes));

        case IOCTL_HID_GET_FEATURE:
            Packet = Irp->UserBuffer;
            if (!Packet || !Packet->reportBuffer)
                return MxtComplete(Device, Irp, STATUS_INVALID_PARAMETER, 0);
            if (Packet->reportId != MXT_REPORT_MAXCOUNT && Packet->reportId != 0)
                return MxtComplete(Device, Irp, STATUS_INVALID_PARAMETER, 0);
            if (Packet->reportBufferLen < sizeof(MaxCount))
                return MxtComplete(Device, Irp, STATUS_BUFFER_TOO_SMALL, 0);
            MaxCount.ReportId = MXT_REPORT_MAXCOUNT;
            MaxCount.MaxCount = MXT_MAX_CONTACTS;
            RtlCopyMemory(Packet->reportBuffer, &MaxCount, sizeof(MaxCount));
            Packet->reportBufferLen = sizeof(MaxCount);
            return MxtComplete(Device, Irp, STATUS_SUCCESS, sizeof(MaxCount));

        case IOCTL_HID_READ_REPORT:
            if (!Irp->UserBuffer ||
                Stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(MXT_TOUCH_REPORT))
                return MxtComplete(Device, Irp, STATUS_BUFFER_TOO_SMALL, 0);
            KeAcquireSpinLock(&Device->Lock, &Irql);
            if (Device->ReportPending)
            {
                RtlCopyMemory(Irp->UserBuffer, &Device->Report, Device->ReportLength);
                Device->ReportPending = FALSE;
                KeReleaseSpinLock(&Device->Lock, Irql);
                return MxtComplete(Device, Irp, STATUS_SUCCESS, Device->ReportLength);
            }
            KeReleaseSpinLock(&Device->Lock, Irql);
            IoMarkIrpPending(Irp);
            Status = IoCsqInsertIrpEx(&Device->Csq, Irp, NULL, NULL);
            if (!NT_SUCCESS(Status))
                return MxtComplete(Device, Irp, Status, 0);
            return STATUS_PENDING;

        case IOCTL_HID_SET_OUTPUT_REPORT:
        case IOCTL_HID_WRITE_REPORT:
        case IOCTL_HID_SET_FEATURE:
        case IOCTL_HID_ACTIVATE_DEVICE:
        case IOCTL_HID_DEACTIVATE_DEVICE:
            return MxtComplete(Device, Irp, STATUS_SUCCESS, 0);

        default:
            return MxtComplete(Device, Irp, STATUS_NOT_SUPPORTED, 0);
    }
}

static NTSTATUS NTAPI MxtCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

static NTSTATUS NTAPI MxtPower(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PMXT_DEVICE Device = ((PHID_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->MiniDeviceExtension;
    PoStartNextPowerIrp(Irp);
    IoSkipCurrentIrpStackLocation(Irp);
    return PoCallDriver(Device->Hid->NextDeviceObject, Irp);
}

static NTSTATUS NTAPI MxtSystemControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PMXT_DEVICE Device = ((PHID_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->MiniDeviceExtension;
    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(Device->Hid->NextDeviceObject, Irp);
}

static NTSTATUS NTAPI MxtAddDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT DeviceObject)
{
    PHID_DEVICE_EXTENSION Hid = DeviceObject->DeviceExtension;
    PMXT_DEVICE Device = Hid->MiniDeviceExtension;
    UNREFERENCED_PARAMETER(DriverObject);

    RtlZeroMemory(Device, sizeof(*Device));
    Device->Hid = Hid;
    Device->Device = DeviceObject;
    Device->Attributes.Size = sizeof(Device->Attributes);
    Device->Attributes.VendorID = 0x03EB;
    Device->Attributes.ProductID = 0x0540;
    Device->Attributes.VersionNumber = 0x0100;
    KeInitializeSpinLock(&Device->Lock);
    InitializeListHead(&Device->Reads);
    IoInitializeRemoveLock(&Device->RemoveLock, MXT_TAG, 0, 0);
    return IoCsqInitializeEx(&Device->Csq, MxtInsert, MxtRemove, MxtPeek,
                             MxtAcquire, MxtRelease, MxtCancel);
}

static VOID NTAPI MxtUnload(PDRIVER_OBJECT DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);
}

NTSTATUS NTAPI DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    HID_MINIDRIVER_REGISTRATION Registration;

    DriverObject->MajorFunction[IRP_MJ_CREATE] = MxtCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = MxtCreateClose;
    DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = MxtControl;
    DriverObject->MajorFunction[IRP_MJ_PNP] = MxtPnp;
    DriverObject->MajorFunction[IRP_MJ_POWER] = MxtPower;
    DriverObject->MajorFunction[IRP_MJ_SYSTEM_CONTROL] = MxtSystemControl;
    DriverObject->DriverExtension->AddDevice = MxtAddDevice;
    DriverObject->DriverUnload = MxtUnload;

    RtlZeroMemory(&Registration, sizeof(Registration));
    Registration.Revision = HID_REVISION;
    Registration.DriverObject = DriverObject;
    Registration.RegistryPath = RegistryPath;
    Registration.DeviceExtensionSize = sizeof(MXT_DEVICE);
    Registration.DevicesArePolled = FALSE;
    return HidRegisterMinidriver(&Registration);
}
