/*
 * PROJECT:         ReactOS HAL
 * LICENSE:         BSD - See COPYING.ARM in the top level directory
 * FILE:            hal/halarm/include/hal.h
 * PURPOSE:         Hardware Abstraction Layer Header
 * PROGRAMMERS:     ReactOS Portable Systems Group
 */

/* INCLUDES ******************************************************************/

/* C Headers */
#if !defined(_ARM_VIRT_)
#define DbgPrint DbgPrintEarly
#endif
#include <stdio.h>

/* WDK HAL Compilation hack */
#include <excpt.h>
#include <ntdef.h>
#undef NTSYSAPI
#define NTSYSAPI __declspec(dllimport)

/* IFS/DDK/NDK Headers */
#include <ntifs.h>
#include <ioaccess.h>
#include <bugcodes.h>
#include <ntdddisk.h>
#include <arc/arc.h>
#include <iotypes.h>
#include <kefuncs.h>
#include <intrin.h>
#include <halfuncs.h>
#include <inbvfuncs.h>
#include <iofuncs.h>
#include <ldrtypes.h>
#include <obfuncs.h>

/* Internal HAL Headers */
#include "halp.h"
#include <drivers/acpi/acpi.h>
#include <halacpi.h>

#define TAG_HAL ' laH'
#define HALP_ACPI_MAX_PHYSICAL_ADDRESS 0x7fffffffULL
extern PWCHAR HalHardwareIdString;
NTSTATUS NTAPI HaliInitPnpDriver(VOID);
NTSTATUS NTAPI HalpQueryAcpiResourceRequirements(PIO_RESOURCE_REQUIREMENTS_LIST *Requirements);
ULONG64 NTAPI HalpAllocPhysicalMemory(PLOADER_PARAMETER_BLOCK LoaderBlock, ULONG64 MaxAddress,
                                     PFN_NUMBER PageCount, BOOLEAN Aligned);
PVOID NTAPI HalpMapPhysicalMemory64(PHYSICAL_ADDRESS Address, PFN_COUNT PageCount);
VOID NTAPI HalpUnmapVirtualAddress(PVOID Address, PFN_COUNT PageCount);
NTSTATUS NTAPI HalpExynosInitializeCacheRanges(PLOADER_PARAMETER_BLOCK LoaderBlock);
NTSTATUS NTAPI HalpExynosMemoryCachingRequirements(PHYSICAL_ADDRESS Address, SIZE_T Length,
                                                  MEMORY_CACHING_TYPE *CacheType);

PDMA_ADAPTER NTAPI HalpArmGetDmaAdapter(PVOID Device, PDEVICE_DESCRIPTION Description, PULONG Registers);

/* EOF */
