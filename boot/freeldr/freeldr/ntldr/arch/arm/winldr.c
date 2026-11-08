/*
 * PROJECT:         ReactOS Boot Loader
 * LICENSE:         BSD - See COPYING.ARM in the top level directory
 * FILE:            boot/freeldr/freeldr/arch/arm/winldr.c
 * PURPOSE:         ARM Kernel Loader
 * PROGRAMMERS:     ReactOS Portable Systems Group
 */

#include <uefildr.h>
#include <pstypes.h>
#include <debug.h>
#include <armvirt.h>
#include "../../winldr.h"

DBG_DEFAULT_CHANNEL(WINDOWS);

#define ARM_BOOT_TABLE_PAGES 96
#define ARM_LPAE_FRAME_MASK 0xFFFFFFF000ULL
#define ARM_LPAE_NORMAL ((1ULL << 55) | 0x703ULL)
#define ARM_LPAE_DEVICE ((1ULL << 55) | (1ULL << 54) | (1ULL << 53) | 0x707ULL)

static PULONGLONG ArmTables;
static ULONG ArmNextTable;
static PULONGLONG ArmRoot;

VOID ArmEnablePaging(ULONG Root);

static PULONGLONG
ArmAllocateTable(VOID)
{
    if (ArmNextTable == ARM_BOOT_TABLE_PAGES)
        return NULL;
    return ArmTables + (ArmNextTable++ * PAGE_SIZE / sizeof(ULONGLONG));
}

static PULONGLONG
ArmChildTable(PULONGLONG Parent, ULONG Index)
{
    PULONGLONG Child;

    if (Parent[Index] == 0)
    {
        Child = ArmAllocateTable();
        if (Child == NULL)
            return NULL;
        Parent[Index] = (ULONG_PTR)Child | 3ULL;
    }
    else if ((Parent[Index] & 3) != 3)
        return NULL;
    return (PULONGLONG)(ULONG_PTR)(Parent[Index] & ARM_LPAE_FRAME_MASK);
}

static BOOLEAN
ArmMapRange(ULONG Address, ULONG Physical, ULONG Pages, ULONGLONG Attributes)
{
    while (Pages)
    {
        PULONGLONG Middle = ArmChildTable(ArmRoot, Address >> 30);
        PULONGLONG Leaf;
        ULONG Index = (Address >> 21) & 511;

        if (Middle == NULL)
            return FALSE;
        if (((Address | Physical) & 0x1FFFFF) == 0 && Pages >= 512 &&
            (Middle[Index] == 0 || (Middle[Index] & 3) == 1))
        {
            Middle[Index] = Physical | (Attributes & ~2ULL);
            Address += 0x200000;
            Physical += 0x200000;
            Pages -= 512;
            continue;
        }
        if ((Middle[Index] & 3) == 1)
        {
            ULONG Offset;
            ULONGLONG Block = Middle[Index];

            if (((Block & ARM_LPAE_FRAME_MASK) + (Address & 0x1FFFFF)) == Physical &&
                (Block & ~ARM_LPAE_FRAME_MASK) == (Attributes & ~2ULL))
            {
                Address += PAGE_SIZE;
                Physical += PAGE_SIZE;
                Pages--;
                continue;
            }
            Leaf = ArmAllocateTable();
            if (Leaf == NULL)
                return FALSE;
            for (Offset = 0; Offset < 512; Offset++)
                Leaf[Offset] = (Block & ARM_LPAE_FRAME_MASK) + (Offset << PAGE_SHIFT) |
                               (Block & ~ARM_LPAE_FRAME_MASK) | 2;
            Middle[Index] = (ULONG_PTR)Leaf | 3ULL;
        }
        Leaf = ArmChildTable(Middle, Index);
        if (Leaf == NULL)
            return FALSE;
        Leaf[(Address >> PAGE_SHIFT) & 511] = Physical | Attributes;
        Address += PAGE_SIZE;
        Physical += PAGE_SIZE;
        Pages--;
    }
    return TRUE;
}

BOOLEAN
MempSetupPaging(PFN_NUMBER StartPage, PFN_NUMBER NumberOfPages, BOOLEAN KernelMapping)
{
    PFREELDR_MEMORY_DESCRIPTOR Memory;
    ULONG Count, Index, First, End, Limit;

    if (StartPage == 0 && NumberOfPages == 1)
        return TRUE;
    if (StartPage > 0xFFFFF || NumberOfPages > 0x100000 - StartPage)
        return FALSE;
    if (KernelMapping && (StartPage >= (KSEG0_BASE >> PAGE_SHIFT) ||
                          NumberOfPages > (KSEG0_BASE >> PAGE_SHIFT) - StartPage))
        return FALSE;
    Limit = StartPage + NumberOfPages;
    Count = MmGetBiosMemoryMap(&Memory);
    for (Index = 0; Index < Count; Index++)
    {
        ULONG Type = Memory[Index].MemoryType;

        if (Type == LoaderBad || Type == LoaderSpecialMemory || Type == LoaderBBTMemory || Type == LoaderReserve)
            continue;
        First = max(max(Memory[Index].BasePage, StartPage), 1);
        End = min(Memory[Index].BasePage + Memory[Index].PageCount, Limit);
        if (First < End)
        {
            ULONG Physical = First << PAGE_SHIFT;
            ULONG Address = KernelMapping ? (ULONG_PTR)PaToVa((PVOID)Physical) : Physical;

            if (!ArmMapRange(Address, Physical, End - First, ARM_LPAE_NORMAL))
                return FALSE;
        }
    }
    return TRUE;
}

VOID
MempUnmapPage(PFN_NUMBER Page)
{
    ULONG Address = (ULONG_PTR)PaToVa((PVOID)(Page << PAGE_SHIFT));
    PULONGLONG Middle = ArmChildTable(ArmRoot, Address >> 30);
    PULONGLONG Leaf;

    if (Middle == NULL || Middle[(Address >> 21) & 511] == 0)
        return;
    if (!ArmMapRange(Address, Page << PAGE_SHIFT, 1, ARM_LPAE_NORMAL))
        BugCheck("ARM page unmap failed");
    Leaf = ArmChildTable(Middle, (Address >> 21) & 511);
    Leaf[(Address >> PAGE_SHIFT) & 511] = 0;
}

VOID MempDump(VOID) { }

VOID
WinLdrSetupMachineDependent(PLOADER_PARAMETER_BLOCK LoaderBlock)
{
    PFREELDR_MEMORY_DESCRIPTOR Memory;
    ULONG Count, Index;
    PVOID Pcr, Process, Thread, Stack, Panic, Interrupt, Shared;
    ULONG Mmfr;

    __asm__ __volatile__("mrc p15, 0, %0, c0, c1, 4" : "=r" (Mmfr));
    if ((Mmfr & 15) < 5)
        BugCheck("ARM startup requires LPAE");
    ArmTables = MmAllocateMemoryWithType(ARM_BOOT_TABLE_PAGES * PAGE_SIZE, LoaderStartupPdrPage);
    Pcr = MmAllocateMemoryWithType(ROUND_UP(sizeof(KIPCR), PAGE_SIZE), LoaderStartupPcrPage);
    Process = MmAllocateMemoryWithType(sizeof(EPROCESS), LoaderMemoryData);
    Thread = MmAllocateMemoryWithType(sizeof(ETHREAD), LoaderMemoryData);
    Stack = MmAllocateMemoryWithType(KERNEL_STACK_SIZE, LoaderStartupKernelStack);
    Panic = MmAllocateMemoryWithType(3 * KERNEL_STACK_SIZE, LoaderStartupPanicStack);
    Interrupt = MmAllocateMemoryWithType(KERNEL_STACK_SIZE, LoaderStartupDpcStack);
    Shared = MmAllocateMemoryWithType(PAGE_SIZE, LoaderMemoryData);
    if (!ArmTables || !Pcr || !Process || !Thread || !Stack || !Panic || !Interrupt || !Shared)
        BugCheck("ARM startup allocation failed");
    RtlZeroMemory(ArmTables, ARM_BOOT_TABLE_PAGES * PAGE_SIZE);
    RtlZeroMemory(Pcr, ROUND_UP(sizeof(KIPCR), PAGE_SIZE));
    RtlZeroMemory(Process, sizeof(EPROCESS));
    RtlZeroMemory(Thread, sizeof(ETHREAD));
    RtlZeroMemory(Shared, PAGE_SIZE);
    ArmNextTable = 0;
    ArmRoot = ArmAllocateTable();
    Count = MmGetBiosMemoryMap(&Memory);
    for (Index = 0; Index < Count; Index++)
    {
        ULONG First = Memory[Index].BasePage, Pages = Memory[Index].PageCount;
        ULONG Type = Memory[Index].MemoryType;

        if (Type == LoaderBad || Type == LoaderSpecialMemory || Type == LoaderBBTMemory || Pages == 0)
            continue;
        if (First >= (KSEG0_BASE >> PAGE_SHIFT) || Pages > (KSEG0_BASE >> PAGE_SHIFT) - First)
            BugCheck("ARM startup RAM exceeds direct map");
        if (!MempSetupPaging(First, Pages, FALSE) || !MempSetupPaging(First, Pages, TRUE))
            BugCheck("ARM RAM mapping failed");
    }
    if (!ArmMapRange(KIP0PCRADDRESS, (ULONG_PTR)Pcr, BYTES_TO_PAGES(sizeof(KIPCR)), ARM_LPAE_NORMAL) ||
        !ArmMapRange(KI_USER_SHARED_DATA, (ULONG_PTR)Shared, 1, ARM_LPAE_NORMAL) ||
        !ArmMapRange(ARM_VIRT_UART_PHYSICAL, ARM_VIRT_UART_PHYSICAL, 1, ARM_LPAE_DEVICE) ||
        !ArmMapRange(ARM_VIRT_UART_VIRTUAL, ARM_VIRT_UART_PHYSICAL, 1, ARM_LPAE_DEVICE) ||
        !ArmMapRange(ARM_VIRT_GIC_DIST_VIRTUAL, ARM_VIRT_GIC_DIST, 1, ARM_LPAE_DEVICE) ||
        !ArmMapRange(ARM_VIRT_GIC_CPU_VIRTUAL, ARM_VIRT_GIC_CPU, 1, ARM_LPAE_DEVICE) ||
        !ArmMapRange(ARM_VIRT_RTC_VIRTUAL, ARM_VIRT_RTC_PHYSICAL, 1, ARM_LPAE_DEVICE))
        BugCheck("ARM special mapping failed");
    LoaderBlock->KernelStack = (ULONG_PTR)PaToVa((PUCHAR)Stack + KERNEL_STACK_SIZE);
    LoaderBlock->u.Arm.PanicStack = (ULONG_PTR)PaToVa((PUCHAR)Panic + 3 * KERNEL_STACK_SIZE);
    LoaderBlock->u.Arm.InterruptStack = (ULONG_PTR)PaToVa((PUCHAR)Interrupt + KERNEL_STACK_SIZE);
    LoaderBlock->u.Arm.PcrPage = (ULONG_PTR)Pcr >> PAGE_SHIFT;
    LoaderBlock->u.Arm.PdrPage = (ULONG_PTR)ArmRoot >> PAGE_SHIFT;
    LoaderBlock->Prcb = KIP0PCRADDRESS + FIELD_OFFSET(KIPCR, Prcb);
    LoaderBlock->Process = (ULONG_PTR)PaToVa(Process);
    LoaderBlock->Thread = (ULONG_PTR)PaToVa(Thread);
    LoaderBlock->u.Arm.FirstLevelDcacheSize = FirstLevelDcacheSize;
    LoaderBlock->u.Arm.FirstLevelDcacheFillSize = FirstLevelDcacheFillSize;
    LoaderBlock->u.Arm.FirstLevelIcacheSize = FirstLevelIcacheSize;
    LoaderBlock->u.Arm.FirstLevelIcacheFillSize = FirstLevelIcacheFillSize;
    LoaderBlock->u.Arm.SecondLevelDcacheSize = SecondLevelDcacheSize;
    LoaderBlock->u.Arm.SecondLevelDcacheFillSize = SecondLevelDcacheFillSize;
    LoaderBlock->u.Arm.SecondLevelIcacheSize = SecondLevelIcacheSize;
    LoaderBlock->u.Arm.SecondLevelIcacheFillSize = SecondLevelIcacheFillSize;
    TRACE("ARM LPAE root=%p, PCR bytes=%lx, kernel stack=%lx\n", ArmRoot, sizeof(KIPCR), LoaderBlock->KernelStack);
}

VOID
WinLdrSetProcessorContext(USHORT OperatingSystemVersion)
{
    UNREFERENCED_PARAMETER(OperatingSystemVersion);
    UefiSerialDisableFirmware();
    ArmEnablePaging((ULONG_PTR)ArmRoot);
}
