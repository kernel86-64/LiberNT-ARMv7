/*
 * PROJECT:     LiberNT Kernel
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARMv7 TLB maintenance
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <nvs/include/mienv.h>

VOID
MiArchInvalidateTlbAll(MI_TLB_SCOPE Scope)
{
    ULONG Zero = 0;

    __asm__ __volatile__("dsb ishst" ::: "memory");
    if (Scope == MiTlbAllProcessors)
        __asm__ __volatile__("mcr p15, 0, %0, c8, c3, 0" :: "r" (Zero) : "memory");
    else
        __asm__ __volatile__("mcr p15, 0, %0, c8, c7, 0" :: "r" (Zero) : "memory");
    __asm__ __volatile__("dsb ish\n\tisb" ::: "memory");
}

VOID
MiArchInvalidateTlbSingle(PVOID VirtualAddress, MI_TLB_SCOPE Scope)
{
    ULONG Address = (ULONG_PTR)VirtualAddress & ~(PAGE_SIZE - 1);

    __asm__ __volatile__("dsb ishst" ::: "memory");
    if (Scope == MiTlbAllProcessors)
        __asm__ __volatile__("mcr p15, 0, %0, c8, c3, 3" :: "r" (Address) : "memory");
    else
        __asm__ __volatile__("mcr p15, 0, %0, c8, c7, 3" :: "r" (Address) : "memory");
    __asm__ __volatile__("dsb ish\n\tisb" ::: "memory");
}

VOID
MiArchInvalidateTlbRange(PVOID BaseAddress, SIZE_T Size, MI_TLB_SCOPE Scope)
{
    ULONG_PTR Address = (ULONG_PTR)BaseAddress & ~(ULONG_PTR)(PAGE_SIZE - 1);
    SIZE_T Pages;

    if (Size == 0)
        return;
    if (Size >= PAGE_SIZE * 64 || Size > ~(ULONG_PTR)BaseAddress)
    {
        MiArchInvalidateTlbAll(Scope);
        return;
    }
    Pages = (Size + ((ULONG_PTR)BaseAddress & (PAGE_SIZE - 1)) + PAGE_SIZE - 1) >> PAGE_SHIFT;
    while (Pages--)
    {
        MiArchInvalidateTlbSingle((PVOID)Address, Scope);
        Address += PAGE_SIZE;
    }
}

VOID
MiArchTlbInvalidate(ULONG64 VirtualAddress, ULONG64 PageCount, BOOLEAN AllProcessors)
{
    MI_TLB_SCOPE Scope = AllProcessors ? MiTlbAllProcessors : MiTlbLocal;

    if (PageCount > ((SIZE_T)-1 >> PAGE_SHIFT))
        MiArchInvalidateTlbAll(Scope);
    else
        MiArchInvalidateTlbRange((PVOID)(ULONG_PTR)VirtualAddress, (SIZE_T)(PageCount << PAGE_SHIFT), Scope);
}

VOID
MiArchTlbInvalidateProcessors(ULONG64 VirtualAddress, ULONG64 PageCount, ULONG64 Processors)
{
    if (Processors)
        MiArchTlbInvalidate(VirtualAddress, PageCount, TRUE);
}

VOID
MiArchTlbInvalidateAll(BOOLEAN AllProcessors)
{
    MiArchInvalidateTlbAll(AllProcessors ? MiTlbAllProcessors : MiTlbLocal);
}
