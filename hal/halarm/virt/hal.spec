; PROJECT:     LiberNT HAL
; LICENSE:     GPL-3.0-or-later
; PURPOSE:     QEMU virt bootstrap HAL export surface
; COPYRIGHT:   Copyright 2026 LiberNT contributors

@ stdcall HalAdjustResourceList(ptr)
@ stdcall HalAllProcessorsStarted()
@ stdcall HalAllocateAdapterChannel(ptr ptr long ptr)
@ stdcall HalSetEnvironmentVariableEx(ptr ptr ptr long long)
@ fastcall HalClearSoftwareInterrupt(long)
@ stdcall HalDisableSystemInterrupt(long long)
@ stdcall HalDisplayString(str)
@ stdcall HalEnableSystemInterrupt(long long long)
@ stdcall HalGetBusDataByOffset(long long long ptr long long)
@ stdcall HalGetEnvironmentVariableEx(ptr ptr ptr ptr ptr)
@ stdcall HalGetEnvironmentVariable(str long str)
@ fastcall HalGetInterruptSource()
@ stdcall HalEndSystemInterrupt(long ptr)
@ stdcall HalGetInterruptVector(long long long long ptr ptr)
@ stdcall HalInitSystem(long ptr)
@ stdcall HalInitializeProcessor(long ptr)
@ stdcall HalProcessorIdle()
@ stdcall HalQueryRealTimeClock(ptr)
@ stdcall HalReportResourceUsage()
@ fastcall HalRequestSoftwareInterrupt(long)
@ stdcall HalReturnToFirmware(long)
@ stdcall HalSetEnvironmentVariable(str str)
@ stdcall HalSetProfileInterval(long)
@ stdcall HalSetRealTimeClock(ptr)
@ stdcall HalSetTimeIncrement(long)
@ stdcall HalStartProfileInterrupt(long)
@ stdcall HalStopProfileInterrupt(long)
@ fastcall HalSweepIcache()
@ fastcall HalSweepDcache()
@ stdcall HalTranslateBusAddress(long long long long ptr ptr)
@ fastcall KeAcquireInStackQueuedSpinLock(ptr ptr) HalpAcquireInStackQueuedSpinLock
@ fastcall KeAcquireInStackQueuedSpinLockRaiseToSynch(ptr ptr)
@ fastcall KeAcquireQueuedSpinLock(ptr) HalpAcquireQueuedSpinLock
@ fastcall KeAcquireSpinLockRaiseToSynch(ptr) HalpAcquireSpinLockRaiseToSynch
@ stdcall KeFlushWriteBuffer()
@ stdcall KeGetCurrentIrql()
@ stdcall KeQueryPerformanceCounter(ptr)
@ stdcall KeRaiseIrqlToDpcLevel()
@ stdcall KeRaiseIrqlToSynchLevel()
@ fastcall KeReleaseInStackQueuedSpinLock(ptr) HalpReleaseInStackQueuedSpinLock
@ fastcall KeReleaseQueuedSpinLock(ptr long) HalpReleaseQueuedSpinLock
@ stdcall KeStallExecutionProcessor(long)
@ fastcall KfAcquireSpinLock(ptr)
@ fastcall KfLowerIrql(long)
@ fastcall KfRaiseIrql(long)
@ fastcall KfReleaseSpinLock(ptr long)
@ stdcall READ_PORT_UCHAR(ptr)
@ stdcall WRITE_PORT_UCHAR(ptr long)

@ stdcall HalpArmGetInterruptIrql(long)
@ stdcall HalpArmDispatchInterrupt(long ptr)
