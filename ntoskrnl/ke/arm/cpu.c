/*
 * PROJECT:         ReactOS Kernel
 * LICENSE:         BSD - See COPYING.ARM in the top level directory
 * FILE:            ntoskrnl/ke/arm/cpu.c
 * PURPOSE:         Implements routines for ARM CPU support
 * PROGRAMMERS:     ReactOS Portable Systems Group
 */

/* INCLUDES *******************************************************************/

#include <ntoskrnl.h>
#define NDEBUG
#include <debug.h>

/* GLOBALS ********************************************************************/

ULONG KeFixedTbEntries;
ULONG KiDmaIoCoherency;
ULONG KeIcacheFlushCount = 0;
ULONG KeDcacheFlushCount;
ULONG KeLargestCacheLine = 64; // FIXME: It depends

NTSTATUS NTAPI
KxSaveFloatingPointState(PKFLOATING_SAVE FloatingState)
{
    UNREFERENCED_PARAMETER(FloatingState);
    return STATUS_SUCCESS;
}

NTSTATUS NTAPI
KxRestoreFloatingPointState(PKFLOATING_SAVE FloatingState)
{
    UNREFERENCED_PARAMETER(FloatingState);
    return STATUS_SUCCESS;
}

VOID FASTCALL
KeInvalidateRangeAllCaches(PVOID BaseAddress, ULONG Length)
{
    ULONG Ctr, DLine, ILine;
    ULONG_PTR Address, Last;

    if (Length == 0)
        return;
    __asm__ __volatile__("mrc p15, 0, %0, c0, c0, 1" : "=r" (Ctr));
    DLine = 4UL << ((Ctr >> 16) & 15);
    ILine = 4UL << (Ctr & 15);
    Last = (ULONG_PTR)BaseAddress + Length - 1;
    __asm__ __volatile__("dsb ish" ::: "memory");
    for (Address = (ULONG_PTR)BaseAddress & ~(ULONG_PTR)(DLine - 1);; Address += DLine)
    {
        __asm__ __volatile__("mcr p15, 0, %0, c7, c14, 1" :: "r" (Address) : "memory");
        if (Last - Address < DLine)
            break;
    }
    __asm__ __volatile__("dsb ish" ::: "memory");
    for (Address = (ULONG_PTR)BaseAddress & ~(ULONG_PTR)(ILine - 1);; Address += ILine)
    {
        __asm__ __volatile__("mcr p15, 0, %0, c7, c5, 1" :: "r" (Address) : "memory");
        if (Last - Address < ILine)
            break;
    }
    __asm__ __volatile__("dsb ish\n\tisb" ::: "memory");
}

KCONTINUE_STATUS NTAPI
KxSwitchKdProcessor(ULONG ProcessorIndex)
{
    return ProcessorIndex == KeGetCurrentProcessorNumber() ? ContinueProcessorReselected : ContinueError;
}

/* FUNCTIONS ******************************************************************/

VOID
KiFlushSingleTb(IN BOOLEAN Invalid,
                IN PVOID Virtual)
{
    //
    // Just invalidate it
    //
    KeArmInvalidateTlbEntry(Virtual);
}

VOID
KeFlushTb(VOID)
{
    //
    // Flush the entire TLB
    //
    KeArmFlushTlb();
}

VOID
NTAPI
KeFlushCurrentTb(VOID)
{
    //
    // Rename?
    //
    KeFlushTb();
}

VOID
FASTCALL
KeZeroPages(IN PVOID Address,
            IN ULONG Size)
{
    /* Not using XMMI in this routine */
    RtlZeroMemory(Address, Size);
}

BOOLEAN
NTAPI
KeInvalidateAllCaches(VOID)
{
    //
    // Invalidate D cache and I cache
    //
    KeArmInvalidateAllCaches();
    return TRUE;
}


/* PUBLIC FUNCTIONS ***********************************************************/

/*
 * @implemented
 */
ULONG
NTAPI
KeGetRecommendedSharedDataAlignment(VOID)
{
    /* Return the global variable */
    return KeLargestCacheLine;
}

/*
 * @implemented
 */
VOID
NTAPI
KeFlushEntireTb(IN BOOLEAN Invalid,
                IN BOOLEAN AllProcessors)
{
    KIRQL OldIrql;

    //
    // Raise the IRQL for the TB Flush
    //
    OldIrql = KeRaiseIrqlToSynchLevel();

    //
    // Flush the TB for the Current CPU
    //
    KeFlushCurrentTb();

    //
    // Return to Original IRQL
    //
    KeLowerIrql(OldIrql);
}

/*
 * @implemented
 */
VOID
NTAPI
KeSetDmaIoCoherency(IN ULONG Coherency)
{
    //
    // Save the coherency globally
    //
    KiDmaIoCoherency = Coherency;
}

/*
 * @implemented
 */
VOID
__cdecl
KeSaveStateForHibernate(IN PKPROCESSOR_STATE State)
{
    //
    // Capture the context
    //
    RtlCaptureContext(&State->ContextFrame);

    //
    // Capture the control state
    //
    KiSaveProcessorControlState(State);
}

VOID
NTAPI
KeFlushIoBuffers(
    _In_ PMDL Mdl,
    _In_ BOOLEAN ReadOperation,
    _In_ BOOLEAN DmaOperation)
{
    DbgBreakPoint();
}

/* SYSTEM CALLS NOT VALID ON THIS CPU *****************************************/

/*
 * @implemented
 */
NTSTATUS
NTAPI
NtVdmControl(IN ULONG ControlCode,
             IN PVOID ControlData)
{
    //
    // Does not exist on ARM
    //
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
NTAPI
NtSetLdtEntries(IN ULONG Selector1,
                IN LDT_ENTRY LdtEntry1,
                IN ULONG Selector2,
                IN LDT_ENTRY LdtEntry2)
{
    //
    // Does not exist on ARM
    //
    return STATUS_NOT_IMPLEMENTED;
}

VOID
NTAPI
KiQueryProcessorTopology(
    _Out_writes_to_opt_(MaxRecords, *RecordCount) PKI_CACHE_RECORD Records,
    _In_ ULONG MaxRecords,
    _Out_ PULONG RecordCount,
    _Out_writes_to_opt_(MaxSets, *SetCount) PKAFFINITY Sets,
    _In_ ULONG MaxSets,
    _Out_ PULONG SetCount)
{
    UNREFERENCED_PARAMETER(Records);
    UNREFERENCED_PARAMETER(MaxRecords);
    UNREFERENCED_PARAMETER(Sets);
    UNREFERENCED_PARAMETER(MaxSets);

    *RecordCount = 0;
    *SetCount = 0;
}
