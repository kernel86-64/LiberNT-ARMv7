/*
 * PROJECT:     LiberNT SD/SDIO/eMMC Bus Driver
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Exynos4210 HSMMC2 microSD controller support
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include "hardware.h"

typedef struct
{
    SDBUS_HARDWARE_EXTENSION Common;
    PUCHAR Gpio;
} SDBUS_EXYNOS_EXTENSION;

static VOID
SdBusExynosRelease(PFDO_EXTENSION FdoExtension)
{
    SDBUS_EXYNOS_EXTENSION *Extension = FdoExtension->HardwareExtension;
    MmUnmapIoSpace(Extension->Gpio, PAGE_SIZE);
    ExFreePoolWithTag(Extension, TAG_SDBUS);
    FdoExtension->HardwareExtension = NULL;
}

static VOID
SdBusExynosInitialize(PFDO_EXTENSION FdoExtension)
{
    SdBusWriteReg32(FdoExtension, 0x80,
                    (1UL << 31) | (1UL << 30) | (1UL << 14) | (1UL << 8) | (2UL << 4));
    SdBusWriteReg32(FdoExtension, 0x84, 0);
}

static VOID
SdBusExynosCapabilities(PFDO_EXTENSION FdoExtension, PULONG Capabilities, PULONG Capabilities2)
{
    UNREFERENCED_PARAMETER(FdoExtension);
    *Capabilities &= ~(SDHCI_CAP_ADMA2_SUPPORT | SDHCI_CAP_ADMA1_SUPPORT |
                       SDHCI_CAP_SDMA_SUPPORT | SDHCI_CAP_64BIT_SYSTEM_BUS |
                       SDHCI_CAP_HIGH_SPEED | SDHCI_CAP_VOLTAGE_180 | SDHCI_CAP_VOLTAGE_300);
    *Capabilities |= SDHCI_CAP_VOLTAGE_330;
    *Capabilities2 = 0;
}

static ULONG
SdBusExynosBaseClock(PFDO_EXTENSION FdoExtension)
{
    UNREFERENCED_PARAMETER(FdoExtension);
    return 50000;
}

static ULONG
SdBusExynosPresentState(PFDO_EXTENSION FdoExtension, ULONG State)
{
    SDBUS_EXYNOS_EXTENSION *Extension = FdoExtension->HardwareExtension;
    ULONG Data = READ_REGISTER_ULONG((PULONG)(Extension->Gpio + 0xc64));

    State &= ~(SDHCI_PS_CARD_INSERTED | SDHCI_PS_CARD_DETECT_PIN);
    State |= SDHCI_PS_CARD_STATE_STABLE | SDHCI_PS_WRITE_PROTECT;
    if (!(Data & (1UL << 4)))
        State |= SDHCI_PS_CARD_INSERTED | SDHCI_PS_CARD_DETECT_PIN;
    return State;
}

static const SDBUS_HARDWARE_OPS SdBusExynosOps =
{
    .Release = SdBusExynosRelease,
    .InitializeController = SdBusExynosInitialize,
    .GetSlotCapabilities = SdBusExynosCapabilities,
    .GetBaseClock = SdBusExynosBaseClock,
    .AdjustPresentState = SdBusExynosPresentState
};

NTSTATUS
SdBusExynosAttach(PFDO_EXTENSION FdoExtension)
{
    SDBUS_EXYNOS_EXTENSION *Extension;
    PHYSICAL_ADDRESS Address;

    Extension = ExAllocatePoolWithTag(NonPagedPool, sizeof(*Extension), TAG_SDBUS);
    if (!Extension) return STATUS_INSUFFICIENT_RESOURCES;
    RtlZeroMemory(Extension, sizeof(*Extension));
    Address.QuadPart = 0x11000000;
    Extension->Gpio = MmMapIoSpace(Address, PAGE_SIZE, MmNonCached);
    if (!Extension->Gpio)
    {
        ExFreePoolWithTag(Extension, TAG_SDBUS);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    Extension->Common.Ops = &SdBusExynosOps;
    FdoExtension->HardwareExtension = Extension;
    /* TODO: deliver card-detect notifications from the GPX3(4) interrupt. */
    return STATUS_SUCCESS;
}
