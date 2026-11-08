/*
 * PROJECT:     LiberNT API tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARM32 SVC argument, mode and return regression tests
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#include <ntoskrnl/include/ntoskrnl.h>
#include <armvirt.h>
#undef DbgPrint

static DECLSPEC_ALIGN(PAGE_SIZE) KIPCR SvcPcr;
static KTHREAD SvcThread;
static KSERVICE_TABLE_DESCRIPTOR SvcTable[SSDT_MAX_ENTRIES];
static BOOLEAN SvcActive;
static ULONG SvcFailures;

VOID KiArmInstallVectors(VOID);
VOID ArmTestRealSvcHandler(PKTRAP_FRAME Trap);
NTSTATUS NTAPI ZwArmTestZero(VOID);
NTSTATUS NTAPI ZwArmTestFour(ULONG, ULONG, ULONG, ULONG);
NTSTATUS NTAPI ZwArmTestSeven(ULONG, ULONG, ULONG, ULONG, ULONG, ULONG, ULONG);
NTSTATUS NTAPI ZwArmTestSeventeen(ULONG, ULONG, ULONG, ULONG, ULONG, ULONG, ULONG, ULONG, ULONG,
                                 ULONG, ULONG, ULONG, ULONG, ULONG, ULONG, ULONG, ULONG);
NTSTATUS NTAPI ZwArmTestInvalid(VOID);

VOID NTAPI ProbeForRead(CONST VOID *Address, SIZE_T Length, ULONG Alignment)
{
    UNREFERENCED_PARAMETER(Address); UNREFERENCED_PARAMETER(Length); UNREFERENCED_PARAMETER(Alignment);
    KeBugCheck(KMODE_EXCEPTION_NOT_HANDLED);
}

struct _UNWIND_HISTORY_TABLE;

VOID NTAPI RtlUnwindEx(PVOID TargetFrame, PVOID TargetIp, PEXCEPTION_RECORD Record, PVOID Value,
                       PCONTEXT Context, struct _UNWIND_HISTORY_TABLE *History)
{
    UNREFERENCED_PARAMETER(TargetFrame); UNREFERENCED_PARAMETER(TargetIp); UNREFERENCED_PARAMETER(Record);
    UNREFERENCED_PARAMETER(Value); UNREFERENCED_PARAMETER(Context); UNREFERENCED_PARAMETER(History);
    KeBugCheck(KMODE_EXCEPTION_NOT_HANDLED);
}

ULONG __cdecl DbgPrint(PCSTR Format, ...)
{
    UNREFERENCED_PARAMETER(Format);
    KeBugCheck(KMODE_EXCEPTION_NOT_HANDLED);
}

DECLSPEC_NORETURN VOID NTAPI KeBugCheckEx(ULONG Code, ULONG_PTR A, ULONG_PTR B, ULONG_PTR C, ULONG_PTR D)
{
    UNREFERENCED_PARAMETER(A); UNREFERENCED_PARAMETER(B); UNREFERENCED_PARAMETER(C); UNREFERENCED_PARAMETER(D);
    KeBugCheck(Code);
}

BOOLEAN ArmTestDispatchSvc(PKTRAP_FRAME Trap)
{
    if (!SvcActive)
        return FALSE;
    ArmTestRealSvcHandler(Trap);
    return TRUE;
}

static NTSTATUS ServiceZero(VOID)
{
    if (SvcThread.PreviousMode != KernelMode || !SvcThread.TrapFrame)
        ++SvcFailures;
    return 42;
}

static NTSTATUS ServiceFour(ULONG A, ULONG B, ULONG C, ULONG D)
{
    if (A != 1 || B != 2 || C != 3 || D != 4)
        ++SvcFailures;
    return A + B + C + D;
}

static NTSTATUS ServiceSeven(ULONG A, ULONG B, ULONG C, ULONG D, ULONG E, ULONG F, ULONG G)
{
    if (E != 5 || F != 6 || G != 7)
        ++SvcFailures;
    return ServiceFour(A, B, C, D) + E + F + G;
}

static NTSTATUS ServiceSeventeen(ULONG A, ULONG B, ULONG C, ULONG D, ULONG E, ULONG F, ULONG G,
                                 ULONG H, ULONG I, ULONG J, ULONG K, ULONG L, ULONG M, ULONG N,
                                 ULONG O, ULONG P, ULONG Q)
{
    if (H != 8 || I != 9 || J != 10 || K != 11 || L != 12 || M != 13 || N != 14 || O != 15 || P != 16 || Q != 17)
        ++SvcFailures;
    return ServiceSeven(A, B, C, D, E, F, G) + H + I + J + K + L + M + N + O + P + Q;
}

BOOLEAN ArmTestSystemCalls(VOID)
{
    ULONG Pcr, Cpsr, Sctlr, Vbar, Mask;
    volatile ULONG *Pmr = (volatile ULONG *)(ARM_VIRT_GIC_CPU + 4);
    ULONG_PTR Services[] = {(ULONG_PTR)ServiceZero, (ULONG_PTR)ServiceFour,
                            (ULONG_PTR)ServiceSeven, (ULONG_PTR)ServiceSeventeen};
    UCHAR Sizes[] = {0, 16, 28, 68};
    BOOLEAN Passed = TRUE;

    RtlZeroMemory(&SvcPcr, sizeof(SvcPcr));
    RtlZeroMemory(&SvcThread, sizeof(SvcThread));
    RtlZeroMemory(SvcTable, sizeof(SvcTable));
    SvcTable[0].Base = Services;
    SvcTable[0].Limit = RTL_NUMBER_OF(Services);
    SvcTable[0].Number = Sizes;
    SvcThread.ServiceTable = SvcTable;
    SvcThread.PreviousMode = UserMode;
    SvcPcr.Prcb.CurrentThread = &SvcThread;
    SvcPcr.CurrentPrcb = &SvcPcr.Prcb;
    SvcFailures = 0;
    __asm__ __volatile__("mrs %0, cpsr\n\tcpsid if" : "=r"(Cpsr) :: "memory");
    __asm__ __volatile__("mrc p15, 0, %0, c1, c0, 0" : "=r"(Sctlr));
    __asm__ __volatile__("mrc p15, 0, %0, c12, c0, 0" : "=r"(Vbar));
    __asm__ __volatile__("mrc p15, 0, %0, c13, c0, 4" : "=r"(Pcr));
    Mask = *Pmr;
    *Pmr = 0;
    __asm__ __volatile__("dsb sy\n\tisb" ::: "memory");
    __asm__ __volatile__("mcr p15, 0, %0, c13, c0, 4\n\tisb" :: "r"(&SvcPcr) : "memory");
    KiArmInstallVectors();
    SvcActive = TRUE;
    Passed = ZwArmTestZero() == 42 && Passed;
    Passed = ZwArmTestFour(1, 2, 3, 4) == 10 && Passed;
    Passed = ZwArmTestSeven(1, 2, 3, 4, 5, 6, 7) == 28 && Passed;
    Passed = ZwArmTestSeventeen(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17) == 153 && Passed;
    Passed = ZwArmTestInvalid() == STATUS_INVALID_SYSTEM_SERVICE && Passed;
    SvcActive = FALSE;
    __asm__ __volatile__("cpsid if" ::: "memory");
    __asm__ __volatile__("mcr p15, 0, %0, c13, c0, 4\n\tisb" :: "r"(Pcr) : "memory");
    __asm__ __volatile__("dsb sy\n\tmcr p15, 0, %0, c12, c0, 0\n\tmcr p15, 0, %1, c1, c0, 0\n\tisb"
                         :: "r"(Vbar), "r"(Sctlr) : "memory");
    *Pmr = Mask;
    __asm__ __volatile__("dsb sy\n\tisb\n\tmsr cpsr_c, %0" :: "r"(Cpsr) : "memory");
    return Passed && SvcFailures == 0 && SvcPcr.Prcb.KeSystemCalls == 5 &&
           SvcThread.PreviousMode == UserMode && SvcThread.TrapFrame == NULL;
}
