# ARMv7-A port

For the Samsung Galaxy Note GT-N7000 firmware baseline, platform probe and
remaining Cortex-A9 requirements, see [N7000 bring-up](N7000_PORT.md).

CPU identification captures MIDR during each processor's PCR initialization.
The HARDWARE registry retains the raw MIDR in `Identifier` and supplies
`ProcessorNameString` and `VendorIdentifier`, which the existing system
properties page reads. Arm implementer `0x41`, part `0xc0f` is displayed as
`ARM Cortex-A15`; part `0xc09` as `ARM Cortex-A9`. Other implementers/parts
retain an explicit MIDR description instead of being assigned an Arm core's
name. This identifies the processor core, not the containing SoC.

Part/implementer evidence is the Arm
[Cortex-A9 TRM, Main ID Register](https://documentation-service.arm.com/static/5f0377b2cafe527e86f5c247)
and [Cortex-A15 TRM, c0 registers](https://support.arm.com/documentation/ddi0438/d/System-Control/Register-summary/c0-registers?lang=en).
The `storage` kernel regression checks the raw identifier against CP15 and
requires the Cortex-A15 display name on QEMU virt. Native name-decoder checks
cover A9/A15 revisions, zero/unknown MIDRs and a different implementer with
the same part bits; the SMDKC210 test uses its real emulated Cortex-A9 MIDR.
The former kernel produced `ARM MIDR 00000000` despite CP15/GDB reporting
`0x414fc0f0`; the fixed kernel reports `ARM Cortex-A15`.

MIDR bit slices are not Win32 processor-level/revision values. Their old
unsubstantiated assignments were removed when initializing MIDR; the previous
zero-initialized API values are retained. Microsoft documents ARM
`SYSTEM_INFO.wProcessorRevision` as reserved; its ARM processor-level mapping
remains `INFO_NEEDED`, rather than guessing from MIDR.
[SYSTEM_INFO documentation](https://learn.microsoft.com/windows/win32/api/sysinfoapi/ns-sysinfoapi-system_info).

The updated existing full image was booted to the desktop and its system
properties page displayed `ARM Limited` / `ARM Cortex-A15`, preserving the
installation and profile. A later cold boot stopped after boot-driver loading
in a repeating Data Abort: DFSR `0xa07`, DFAR `0x801f8fe8`. Its saved frame chain
shows a clock interrupt inside image paging I/O, with the clock handler using
the interrupted thread's SVC stack near its lower boundary. Stack exhaustion
and the unused dedicated ISR stack are under investigation; no fix for that
separate intermittent startup failure is included here.

The current milestone boots the real ARM32 kernel through EDK II and FreeLoader
on QEMU `virt`, with a Cortex-A15, one CPU and 512 MiB of RAM. NVS, VirtIO/FAT/NTFS,
SMSS, CSRSS, Winlogon and LSASS initialize. The `csr --input` fixture reaches
the lock screen, moves the cursor, dismisses the lock screen with a key, and
uses a mouse click and keyboard input to enter a masked password character.
The `csr --login` fixture additionally provisions the ordinary local account
`tester` (password `armv7`), verifies wrong-password rejection, logs in through
Winlogon, and checks the user identity, profile registry write access, Explorer
desktop and taskbar. A second boot of the same image passes without provisioning
again. These fixtures remain minimal test installations.

The standard `liberntimg` target also builds the full native ARM32 installation,
using the ordinary preinstall manifest, services, applications, fonts and theme
resources. Its 1.5 GiB `LiberNT.img` has a 64 MiB EFI System Partition and an
NTFS system partition containing `C:\Windows`. It completes `setup -mini`,
automatically logs on as Administrator with a blank password, and reaches the
LiberNT desktop. The application smoke test passes for winver, system
properties, Notepad, Calculator, Registry Editor and Task Manager, together
with the active Aero theme, native winprint environment, user APC wait, NLS
conversion and relocated kernel32 KnownDLL. A cold boot retains the profile.
The prepared image is powered off through `shutdown -s -t 0` after validation.

Build and observe the full image with no QEMU process holding it open:

```sh
CCACHE_DISABLE=1 ninja -C output-Clang-arm-debug liberntimg -j8
python3 -B scripts/test-arm-uefi.py --stage csr \
    --installed-image output-Clang-arm-debug/LiberNT.img
```

To open the prepared image interactively after stopping the test guest:

```sh
qemu-system-arm -machine virt -cpu cortex-a15 -m 512 -smp 1 \
    -drive if=pflash,format=raw,readonly=on,file=/usr/share/qemu/edk2-arm-code.fd \
    -drive if=pflash,format=raw,file=output-Clang-arm-debug/QEMU_VARS.fd \
    -drive if=none,format=raw,id=os,file=output-Clang-arm-debug/LiberNT.img \
    -device virtio-blk-device,drive=os -device ramfb \
    -global virtio-mmio.force-legacy=false \
    -device virtio-keyboard-device -device virtio-tablet-device \
    -nic none -serial stdio -monitor none
```

The installed-image mode boots the image in place without repacking it and
pauses after 30 seconds. Its `csr: PASS` reports CSRSS, LSASS and Winlogon
initialization, not desktop readiness. Initial setup needs additional bounded
execution intervals. `arm_account_setup.exe --desktop-smoke` checks the theme,
native print processor, user APC delivery and mapping, and visible windows for
winver, system properties, Notepad, Calculator, Registry Editor and Task Manager.

NTFS create processing now uses a temporary expanded kernel stack through
`KeExpandKernelStackAndCalloutEx`. The normal ARM32 stack remains 12 KiB; the
callout uses the existing large-stack allocation and restores the thread and
processor stack metadata before releasing it. The full NTFS boot previously
overflowed the stack while recursively loading drivers. EFI checks exercise
callout bounds, allocation failure, validation and metadata restoration.
The contract comes from Microsoft's
[KeExpandKernelStackAndCalloutEx documentation](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-keexpandkernelstackandcalloutex).

The loader permits relocatable KnownDLL images to move when their preferred
bases collide. It retains the PE relocation checks. The removed special case
rejected kernel32 and user32 outright; ARM requires ASLR support under the
documented [/DYNAMICBASE contract](https://learn.microsoft.com/en-us/cpp/build/reference/dynamicbase).
No Windows 11 runtime baseline is available for this shared loader change.

ARM exception return delivers a pending user APC before restoring user state.
Without this, setup's service notification left `UserApcPending` set and a later
RPC pipe wait returned `WAIT_IO_COMPLETION` without completed input. EFI checks
cover user/kernel return selection and APC-level delivery; normal setup now
passes that RPC failure. Microsoft's
[QueueUserAPC contract](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-queueuserapc)
defines callback execution before the alertable wait returns.

The ARM architecture descriptor reserves the first 64 KiB of user addresses,
consistent with the existing NT minimum-address consumers and other ports.
Previously a zero-address NLS mapping was mistaken for an absent table by the
loader and corrupted imported DLL names. The native ARM descriptor regression
failed before the fix and passes 50,325 checks after it.

The virt HAL rearms the timer after accounting for elapsed time. If interrupt
processing has already passed the next comparison, the hardware comparison is
placed one requested period ahead of the current counter; the accounting
deadline is retained so the next interrupt includes every elapsed period.
Programming an already overdue comparison left the clock and the dispatch SGI
pending together and starved ready threads at a 1 ms resolution. Disabling only
the clock interrupt in the live debugger immediately admitted the pending
dispatch interrupt. The updated image boots past that state; the desktop smoke
test requests 1 ms resolution and verifies that two CPU-bound threads both run
while the caller sleeps and wakes. Timer comparison behavior is specified in
chapter B8 of the [Armv7-A/R Architecture Reference Manual](https://documentation-service.arm.com/static/5f8daeb7f86e16515cdb8c4e).

The native print environment is `Windows ARM`, with `ARM` as its spool directory,
as specified by [MS-RPRN](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-rprn/e81cbc09-ab05-4a32-ae4a-8ec57b436c43).
Its hive entries and winprint packaging now agree. Legacy HAL DMA adapters and
MSI remain unsupported on this platform; the VirtIO drivers use their existing
DMA and interrupt paths.

`GetFinalPathNameByHandleW` derives the volume device prefix from the file path
suffix when it matches the object name. Taking the first component after
`\Device\` truncated VirtIO's nested `Harddisk0\Partition2` device name and
made the mount-manager query fail with `STATUS_OBJECT_TYPE_MISMATCH`.
The `arm_account_setup.exe --verify-path` regression checks the documented
[DOS path contract](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfinalpathnamebyhandlew)
against the executable's module path; it failed before the change and passes
after it. Both kernel32 and kernel32_vista are rebuilt. Cross-target
syntax checks pass for amd64, i386, ARM64 and RISC-V64. No Windows 11 runtime
baseline was available. Cppcheck with the ARM compile database stops parsing
the existing `crtdefs.h` `errno_t` declaration, so it provides no clean analysis
result for these ARM C translation units.

## Build and boot tests

Use the existing RosBE LLVM installation and one reusable build directory:

```sh
CCACHE_DISABLE=1 ./configure.sh --clang -a arm --no-feeds-update \
    -DSARCH=virt -DKD_DEBUGGER=NONE -DOPTIMIZE=0
CCACHE_DISABLE=1 ninja -C output-Clang-arm-debug uefildr arm_uefi_test -j8
python3 -B sdk/tools/tests/test_arm_uefi.py
python3 -B scripts/test-arm-uefi.py --no-build
```

ARM32 selects the FreeLoader UEFI implementation because the separate
`bootloader/` replacement has no ARM32 architecture backend. There is no
ARM32 BIOS target in this configuration. The historical `Windows2003` entry
type uses the compiled `_WIN32_WINNT` value, which is 0x0a00 in this build.

The test harness needs `qemu-system-arm`, `mkfs.vfat`, and mtools. The `user`
and `session` fixtures also use `llvm-readobj` to collect server DLL import
dependencies from the existing Ninja build outputs.
The `clock` stage also uses a GDB build with ARM support. Its default
firmware is `/usr/share/qemu/edk2-arm-code.fd`, with the corresponding
`edk2-arm-vars.fd` beside it. Override the code path with `--firmware`.
This must be ARM32 firmware; AArch64 AAVMF is a different execution environment.

The harness builds `arm-uefi-tests.img`, boots `EFI/BOOT/BOOTARM.EFI`, and reuses
`QEMU_VARS.fd`. These are disposable test media, not a full OS image. It
supports these boot stages:

- `abi`: a real EFI regression test of nonzero and zero frame-free `longjmp` returns,
  R4–R11, D8–D15 and FPSCR preservation, explicit establisher-frame capture,
  portable `floor`, `pow`, `log10` and trigonometric functions, and 32/64-bit
  interlocked-add results. It also checks the `setjmp.h` then `setjmpex.h`
  include order and frame capture through the extended macro. ARM scalar
  `memcpy`, `memmove` and `memset` tests cover lengths 0–128, all byte alignments,
  overlapping moves, return pointers, fill truncation and unchanged boundary bytes.
  Build-time checks pin the ARM32 SDK unwind-table sizes, offsets and context
  flags. Virtual unwind tests cover packed and full metadata, partial prologues
  and epilogues, conditional epilogues, VFP saves, nonvolatile context pointers,
  machine/context frames and leaf functions. A second test unwinds real Clang
  PE metadata and checks the recovered caller PC, SP, R4–R11 and D8–D15.
  Context capture checks include context flags, Thumb state and stack alignment.
  Kernel architecture helpers are linked directly into the EFI fixture: DPC
  request-summary operations preserve unrelated bits; MMIO routines exercise
  byte/halfword/word accesses, consecutive buffers, boundaries and zero counts.
  These memory-backed MMIO tests do not prove real device behavior or SMP
  ordering. Portable `_logb`/`_logbf` are checked on normal, subnormal, infinite
  and NaN inputs. OS exception dispatch, collided unwinds and `NtContinue`
  restoration have not yet run under LiberNT. Additional tests execute real
  SVC, undefined-instruction and data-abort entry/return, switch between two
  kernel stacks, preserve all VFP registers across switching, check same-root
  process affinity updates, and exercise ARM/Thumb MOV32 relocation fixtures
  and runtime debug-service argument dispatch. Real SVC calls through the
  production kernel stubs and dispatcher cover 0, 4, 7 and 17 arguments,
  caller LR preservation, restoration of `PreviousMode`, return values and
  an out-of-range service ID. The fixture rejects unexpected user probing
  or exception unwinding; it does not validate user-stack faults. A direct
  user-mode dispatch frame additionally verifies the original APC environment
  and restoration of the caller mode. Processor
  control-state tests read the real CPU ID and MMU control registers, modify
  TPIDRURW, restore it, and compare the supported debugger register snapshot.
  Thread-switch checks now verify SDK `NtCurrentTeb` across both stacks.
  Production context conversions preserve all 32 VFP registers, apply the
  supplied context-field mask and sanitize user CPSR privilege/state bits.
  The actual ntdll APC dispatcher and loader thunk run with checked argument
  bindings and a continuation fixture; the real user boot separately exercises
  ntdll initialization and `NtContinue`. Service exception hooks retain the
  saved nonvolatile frame and restore the previous mode and trap link.
  PL011 receive tests compile the production debugger transport against register
  fixtures and check empty FIFO, received bytes and receive errors. The kernel
  stack test moves a live C frame chain to another stack and back, checking
  locals, trap/exception/VFP links, user SP and interrupt masks. SVC tests cover
  GUI conversion success/failure, allocation failure, repeated shadow-table
  calls and the process mitigation that forbids Win32 services.
- `loader`: the actual FreeLoader initializes firmware services, mounts FAT32,
  reads `freeldr.ini`, and parses the selected entry. No kernel is supplied on
  the fixture. Passing this stage does not mean that kernel startup works.

- `kernel`: the actual FreeLoader loads `ntoskrnl.exe`, the QEMU virt HAL,
  debugger transport and framebuffer boot video driver with a small SYSTEM
  hive and NLS payload. Success requires NVS phase-0 memory and boot-image
  diagnostics, not just reaching a firmware or kernel entry marker. The
  fixture has no boot storage drivers and is not a complete installation.

- `clock`: uses the kernel fixture, checks continued growth of the shared
  interrupt-time and tick counters through GDB, and reaches phase 1's boot
  partition lookup. Its expected result is `INACCESSIBLE_BOOT_DEVICE`
  (`0x7b`, `STATUS_OBJECT_NAME_NOT_FOUND`) because the fixture has no kernel
  storage drivers. This is a clock/startup regression, not a successful OS boot.

- `storage`: builds an MBR/FAT32 fixture with the native `virtblk` and FastFAT
  boot drivers. A test driver opens a 4096-byte file through the real FAT
  stack, checks its contents, writes a changed pattern, flushes, rereads and
  restores it. It does not use firmware block I/O after kernel entry.
- `user`: includes ntdll and smss, requires the storage regression to pass,
  and stops after the native `NtProcessStartup` entry. This proves first user
  entry, not complete Session Manager initialization.
- `session`: requires the storage, volume-identity, CPU registry and fast-mutex
  export regressions, then stops after SMSS initializes KnownDlls and its dynamic
  environment. Success does not claim completed CSRSS or Win32 initialization.
- `csr`: continues through the first Win32 system calls and requires the native
  system-session CSRSS server to finish initialization and the display bridge
  to publish an 800x600 scanout. Wininit and its import dependencies are
  included. Add `--observe` to require LSASS and Winlogon initialization and
  continue for 30 seconds instead of stopping at CSRSS. Quiet intervals check
  the shared clock. This does not claim an interactive desktop session.

Use `--stage abi`, `--stage loader`, `--stage kernel`, `--stage clock`,
`--stage storage`, `--stage user`, `--stage session` or `--stage csr` to select one boot. Add `--virtio-modern`
to test VirtIO MMIO version 2; the default QEMU transport is version 1.
The default `both` runs the ABI and loader stages. Kernel, clock, storage and
user/session/csr stages leave the successful guest stopped for inspection. The harness
resets the same firmware variables file before each run so a stale BootOrder
cannot target the previous superfloppy layout after switching to MBR. Use GDB's `monitor quit` before
reusing the image. Each boot is headless,
network-isolated, limited to 30 seconds and a six-second serial-log stall.
Results are written to `arm-uefi-results.json`; `arm-boot.log` holds the latest
boot. Exceptions stop the guest through QMP and leave it available to GDB on
`127.0.0.1:1234`. Inspect it and shut it down before reusing the test media.

## Toolchain findings

RosBE Clang 24 (`720de96891932b393be49926a56b1e532772733b`) crashes in
`MachineBlockPlacement::buildCFGChains` while compiling
`LdrpRunShimEngineInitRoutine` with native asynchronous SEH at `-O1`.
Clang 23.1.4 compiles that file but emits an out-of-range Thumb `cbz` for
`EnumProcessModules` in `psapi.c`. Both files compile with RosBE Clang 24 at
`-O0`, which is the initial ARM debug default. Optimized ARM builds remain
unverified. Exception handling has not been disabled to work around either
compiler defect.

Host tools use the selected NT version as well; they must not silently build
against NT 5.2 while generating an NT 10.0 image.

## Evidence

- [Microsoft ARM32 ABI conventions](https://learn.microsoft.com/en-us/cpp/build/overview-of-arm-abi-conventions)
  specifies Thumb-2, VFP/NEON, eight-byte stack alignment and the nonvolatile
  integer/floating-point registers.
- Microsoft's published SDK headers
  [winnt.h](https://github.com/microsoft/win32metadata/blob/main/generation/WinSDK/RecompiledIdlHeaders/um/winnt.h)
  and [rtlsupportapi.h](https://github.com/microsoft/win32metadata/blob/main/generation/WinSDK/RecompiledIdlHeaders/um/rtlsupportapi.h)
  supply the ARM32 unwind declarations and structures. Sizes in the compile
  tests follow those fields with four-byte pointers, not ARM64 offsets.
- Microsoft's [InterlockedAdd contract](https://learn.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-interlockedadd)
  specifies the updated return value and a full barrier. The missing ARM
  compiler shims use sequentially consistent compiler atomics; the EFI test
  checks their result and stored value on the target CPU.
- EDK II's published ARM
  [PE loader](https://github.com/tianocore/edk2/blob/edk2-stable202408/MdePkg/Library/BasePeCoffLib/Arm/PeCoffLoaderEx.c)
  accepts `IMAGE_FILE_MACHINE_ARMTHUMB_MIXED` (0x1c2). LLVM emits Windows
  `ARMNT` (0x1c4). `sdk/tools/arm_uefi.py` changes only the machine field of
  linked ARM32 EFI applications; native NT images retain `ARMNT`.
- EDK II's [ARM library](https://github.com/tianocore/edk2/blob/edk2-stable202408/ArmPkg/Library/ArmLib/Arm/ArmV7Support.S)
  and its ARM processor binding document CPACR, FPEXC and stack setup.
  QEMU's first FreeLoader exception independently showed CPACR=0 and FPEXC=0
  on entry. The architecture entry enables VFP before compiler-generated C
  code can execute a NEON instruction.
- [Armv7-A/R Architecture Reference Manual](https://documentation-service.arm.com/static/5f8daeb7f86e16515cdb8c4e),
  sections B3.6 and A8, defines LPAE geometry and descriptor attributes,
  software-use bits, XN/PXN, barriers, exception return and MOVW/MOVT encodings.
  [Microsoft PE/COFF](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#base-relocation-types)
  defines ARM MOV32 (5) and Thumb MOV32 (7) base relocations.
- [GIC architecture specification](https://documentation-service.arm.com/static/5f8ff196f86e16515cdbf969)
  and [PL031 technical reference](https://documentation-service.arm.com/static/5fa13c81b1a7c5445f29021e)
  supply the register interfaces used by the bootstrap BSP. The Armv7 manual's
  generic timer chapter defines CNTFRQ, CNTVCT, CNTV_CVAL and CNTV_CTL. QEMU's
  [virt platform definitions](https://github.com/qemu/qemu/blob/master/include/hw/arm/virt.h)
  assign virtual timer PPI 27.
- Microsoft's [ARM undefined instruction conventions](https://devblogs.microsoft.com/oldnewthing/20210618-00/?p=105324)
  identify Thumb `0xdefe` as `__debugbreak`. The kernel distinguishes it from
  the existing private `0xdefd` runtime debug service.
- [VirtIO 1.2](https://docs.oasis-open.org/virtio/virtio/v1.2/virtio-v1.2.html),
  sections 2.7, 3.1, 4.2 and 5.2, defines the split queues, MMIO transport,
  feature/status negotiation and block requests used by `virtblk`.
  [QEMU's memory map](https://github.com/qemu/qemu/blob/master/hw/arm/virt.c)
  and [platform declarations](https://github.com/qemu/qemu/blob/master/include/hw/arm/virt.h)
  specify the initial 32 slots at 0x0a000000 with a 0x200 stride.
- Microsoft's published SDK `winnt.h` defines ARM `NtCurrentTeb` through
  `CP15_TPIDRURW`, and its PE specification defines ARMNT (0x1c4) as Thumb-2.
  The DDK image header now supplies that canonical constant alongside its
  existing ARMV7 alias; the native-image selector uses ARMNT rather than ARM.
- [QEMU virt documentation](https://www.qemu.org/docs/master/system/arm/virt.html)
  describes the virtual platform. Hardware discovery and the kernel HAL must
  ultimately consume its device tree rather than reuse OMAP assumptions.

The old ARM loader/NDK structures are existing implementation scaffolding.
They have not been established as NT 10 ARM32 layout parity by these tests.
No Windows binaries have been disassembled or decompiled for this work.

## Full-build status

The ARM32 RTL now supplies `RtlLookupFunctionEntry`, `RtlVirtualUnwind`,
`RtlUnwindEx` and `RtlRestoreContext`; ntdll exports use the ARM32 argument
widths. The decoder follows Microsoft's ARM PE format and is adapted from
published [Wine unwind code](https://github.com/wine-mirror/wine/blob/master/dlls/ntdll/unwind.c).
The dispatch walk uses the current LiberNT ARM64 implementation with ARM32
register sets and call-site adjustment. Context restoration uses `ZwContinue`;
its kernel trap/continuation path still needs implementation and testing.
Do not treat DLL linkage as validation of the complete exception runtime.

Dynamic table address arithmetic uses pointer-sized values. Architecture hooks
supply function bounds instead of assuming an x64 `EndAddress` field. These
shared changes pass syntax compilation for amd64, ARM64 and RISC-V64 using
those compiler targets; these are not full builds or runtime parity tests.

The build now links ntdll, framedyn, mmdevapi, msvcrt, msvcr80 and msvcr90 and
produces the system-image integrity catalog. ARM framedyn exports retain their
published names and map to the compiler's actual implementation symbols.
The portable audio backend is included on ARM, and portable binary-exponent
math replaces the remaining `_logb` breakpoint stub.

`kiinit.c` compiles after converting the old affinity/processor-set consumers
to existing NT10 fields and correcting the vector-table declaration. DPC helpers
use interlocked operations on the existing ARM `DpcRequestSummary` bitfields;
the EFI test exercises these production helpers with the kernel headers.
These fields remain existing ARM scaffolding, not a claim of NT10 layout parity.

The kernel, HAL, FreeLoader, debugger transport and framebuffer boot video
modules link. `scripts/test-arm-uefi.py --stage kernel` reaches NVS phase 0:

```text
ARM kernel: KiInitializeSystem, LPB=C3389000 PCR=FFDFF000
Boot page tables adopted: root at 000043acc000, 13 tables, 1 regions, 483 MB free
Kernel address space: system mappings at 80000000, nonpaged pool at A0000000 (64 MB), paged pool at A4000000 (64 MB)
Memory: 478 MB available, commit limit 482 MB
Boot images: ntoskrnl.exe at C33B2000-C399BFFF, hal.dll at C399C000-C39A5FFF, 4 loaded
```

Addresses depend on the firmware allocations. The loader allocates the PCR
from `sizeof(KIPCR)` and derives the PRCB pointer from its actual C field.
ARM entry offsets use the beginning of the current LPB, not the legacy
`LoadOrderListHead`-relative offsets. Space for the idle thread's initial
switch frame is reserved above the active bootstrap stack.

The NVS ARM backend uses three-level LPAE tables: 32-bit virtual addresses,
40-bit descriptor output addresses, 64-bit entries and 2 MiB blocks. Cortex-A15
supports LPAE; the loader rejects CPUs without it. The initial direct mapping
requires RAM below 2 GiB. It maps the firmware's real memory ranges rather
than the lookup table's synthetic reserved holes. NVS retains its own memory
manager and pools; the old empty ARM hooks have been removed.

Live boot blocks are split through an identity-mapped assembly routine. This
keeps code accessible across the required break-before-make interval. The
normal PTE path uses atomic 64-bit operations and explicit barriers. Software
write/copy state uses the Arm-defined software descriptor bits; ARMv8 DBM and
UXN behavior are not assumed on ARMv7.

Native `mm_arm_pte` and ASan/UBSan checks cover 50,324 descriptor and geometry
assertions. The amd64/i386 PTE and ARM64 TLB native regressions pass as well.
Shared C changes pass syntax compilation for amd64, ARM64 and RISC-V64; these
are not full builds or boot tests of those ports. The broader native core
harness currently fails to build because its Linux-subsystem shim lacks the
new DRM declarations and implementations; it was not counted as passing.

## Remaining kernel work

The `virt` HAL currently exports the bootstrap services exercised by the
kernel. It uses the board's fixed PL011, GICv2 and PL031 locations; device-tree
discovery, device interrupt binding, DMA,
complete HAL exports and secondary processor startup remain to be implemented.
The generic counter backs performance queries and stalls. Phase 1 arms the
virtual generic timer on PPI 27 at CLOCK_LEVEL. Absolute deadlines account for
missed periods, and the next period applies a requested interval change.
Clock IRQs feed the shared kernel time, timer DPCs and runtime accounting.
The `clock` regression observed over 1,100 ticks and the System thread waking
from its boot-volume wait before the expected `0x7b` stop. The original stop
is now reported without recursive control-state breakpoints or an IRQL drop
while looking up unwind metadata. Device IRQ handling and ARM32 user transitions have
not been validated by the component EFI tests.

Kernel DPC/APC software interrupts and same-root process switching now have
implementations. Kernel service stubs preserve LR around same-mode SVC and
copy stack arguments below that saved frame. The dispatcher uses R12 for the
service ID and rejects IDs at or above the service-table limit. First Win32
user calls prepare a large kernel stack at the assembly SVC boundary, then
invoke the existing GUI process/thread callouts and retry the shadow table.
The stack handoff runs after the allocator returns and before the C service
dispatcher starts, so no active dispatcher C frame retains an old-stack address.
Runtime debug output uses the existing private ARM UDF
service ABI with the fifth AAPCS argument taken from the interrupted stack.
LPAE translation/access/permission aborts now enter NVS with the decoded
read/write/execute access. QEMU verified a demand-zero fault being resolved
and execution retrying into later PnP initialization. Full user-fault delivery
and exception unwinding remain unvalidated. Debugger snapshots preserve the
supported debug registers, thread-pointer registers and FPEXC; banked exception
registers, full PMU capture and complete processor-freeze restoration remain gaps.
All SEH paths, full bugcheck processor-state capture, user-mode exception
restoration and the Win32 callback path remain unfinished. The native VirtIO boot path now validates firmware-independent disk access
and first user entry; it is a bootstrap legacy disk driver rather than a full
PnP storage stack. Interactive-session startup and display-device publication
remain unfinished in the minimal image.

Cppcheck was run with the ARM build's compile database on 54 changed C/C++
files present in that database. It reports a parser error in the existing
`crtdefs.h` `errno_t` declaration; this is not a clean static-analysis result.
The shared `RtlPcToFileHeader` crash-path change additionally passes syntax
compilation for amd64, ARM64 and RISC-V64.

## Native storage and user entry

`virtblk` supports MMIO versions 1 and 2 with a single serialized split queue,
noncached contiguous DMA buffers, bounded completion polling, explicit memory
barriers and FLUSH requests. The QEMU platform scan is isolated in its own
file. Initial disk publication supports primary MBR partitions; GPT, extended
partitions, FDT discovery, asynchronous IRQ completion and full PnP integration
remain gaps. Unsupported dump/storage controls return failure.

A boot filesystem that already has Start=0 is now reused and marked critical
rather than appended a second time. This fixes the observed FastFAT driver-name
collision. The storage fixture deliberately keeps Start=0 to retain the
regression trigger.

The ARM native runtime no longer contains the old unconditional startup spin.
The loader APC enters its dispatcher through PC, supplies the saved context to
`LdrpInit`, and restores user execution through `NtContinue`. ARM's C SList pop
uses its own SEH retry and has no assembler rollback exports; ARM therefore
uses the existing no-rollback architecture policy. Syscall exit checks the
stored original APC environment, not the contextual enum used when creating
an APC.

Both legacy and modern MMIO boots passed the FAT regression. Mount Manager
assigns the boot volume's drive letter through the documented legacy-volume
arrival notification. Virtblk returns its counted device name and an opaque
persistent MBR identity made from the disk signature and partition byte offset.
Kernel tests cover short output buffers, the device name, identity, the CPU
registry identifier and the ARM fast-mutex exports' IRQL transitions.

The `session` fixture adds Session Manager registry configuration, the server
DLL import closure and required NLS files. SMSS opens KnownDlls, runs Autochk
on `C:`, and creates its dynamic environment. ARM's machine-dependent registry
initialization publishes each processor's saved MIDR through the existing
configuration-manager node builder. This identifier is descriptive LiberNT
scaffolding, not a claim about an undocumented Windows ARM32 string format.

Win32k and its font/HID dependencies load. The `csr` fixture also loads
Dxgmms2, DxgKrnl, BasicDisplay and Framebuf, using the existing WDDM and
software-adapter registry definitions. It supplies the base `DeviceClasses`
key used by device-interface registration. BasicDisplay takes ownership of
the UEFI framebuffer and publishes the display bridge with an 800x600 scanout.
CSRSS loads Basesrv and Winsrv and finishes system-session initialization.
A live process-list inspection found SMSS, two CSRSS processes and Wininit.
Paging-file creation remains unsuccessful in this minimal image. This
milestone proves adapter initialization, not a working desktop.

Continuing beyond the original `csr` stop point reproduced `0xC000021A`
with `STATUS_OBJECT_NAME_NOT_FOUND`: SMSS could not find Winlogon. The startup
fixture now supplies Winlogon, Services, LSASS and their static dependencies,
plus the dynamically loaded authentication package, keyboard layout and CDD.
It includes NPFS and the `PIPE` DOS-device mapping for local RPC, KsecDD for
the existing process CSPRNG, a computer name, Winlogon configuration and GUI
fonts in a minimal SOFTWARE hive. No critical-process termination is bypassed.

`--stage csr --observe` continues for a bounded 30-second run, requires LSASS
and Winlogon initialization, rejects critical-process exception exits and
checks the shared clock every six seconds without log output. The ARM guest
reaches the rendered 800x600 lock screen. Live inspection found both CSRSS
processes, Wininit, Winlogon, Services and LSASS. This is a lock-screen startup
milestone; interactive logon and an Explorer desktop remain untested.

The ARM callback path now preserves kernel nonvolatile registers, VFP state,
IRQ/FIQ masks, callback nesting and existing stack metadata across user-mode
execution and `NtCallbackReturn`. Continuation state is an internal stack
local, not a claim about a Windows KCALLOUT_FRAME layout. The EFI test exercises
nested callbacks, return status and result lengths, no-active-callback and
nesting-limit errors. First-chance user exceptions now receive CONTEXT and
EXCEPTION_RECORD on an aligned user stack and pass through ntdll SEH before
second-chance termination. Frame-copy tests preserve integer and VFP values.
Both VirtIO MMIO versions pass the 30-second observation with these startup
markers. Shared Kernel32 context and GDI text changes also pass syntax
compilation for amd64, ARM64 and RISC-V64; no additional architecture build
cache was created.

The user dispatcher shapes follow the published
[Wine ARM implementation](https://github.com/wine-mirror/wine/blob/master/dlls/ntdll/signal_arm.c)
and the Windows ARM ABI; no Windows binaries were inspected.

Win32 thread/process creation uses the existing ntdll `RtlUserThreadStart`
gateway with an eight-byte-aligned stack and User/Thumb CPSR (`0x30`, Armv7
CPSR mode/T bits). The old context embedded a parent Kernel32 thunk address,
which is unsafe when the child maps that DLL elsewhere. The gateway resolves
the initialized child's existing Kernel32 thread-start callback.

The linked Win32u ARM stubs previously started at zero, selecting the NT
service table. Their IDs now start at the Win32k table base. A PE regression
checks 834 exported stubs against the compiled ARM32 kernel service order;
it failed on `NtGdiAbortDoc` before the fix and passes afterward. These IDs
are LiberNT internal integration, not evidence for Windows syscall numbering.

A live GDI failure exposed two independent batching bugs. Odd UTF-16 string
lengths left the next command unaligned; variable text records now include
padding to their C structure alignment. ARM also omitted the registered GDI
batch-flush callback before Win32k services. Without it, `NtGdiFlush` left the
TEB buffer full and later commands overwrote `Win32ClientInfo`; the stopped
guest had offset `0x53c` beyond the 1240-byte buffer. The ARM dispatcher now
flushes before executing the service. EFI tests check ordering and that NT
services do not trigger this GUI flush. An ExtTextOut API regression queues
consecutive odd-length commands on a compatible bitmap. It passes ARM syntax
compilation; its Windows 11 baseline has not been run. QEMU startup verifies
the real font and GUI path.

ARM exports `HalSetBusDataByOffset` with the documented zero-byte result for
unsupported buses; it does not claim PCI configuration support. The existing
ARM cache-range implementation is now exported to graphics drivers.
`KeFlushIoBuffers` maintains the first MDL's mapped range: writes clean data
to the point of coherency; DMA reads invalidate complete cache lines and
clean/invalidate partial lines; PIO reads clean/invalidate the range. Reads
also invalidate instruction-cache lines. Cache geometry comes from CTR and
completion uses DSB/ISB. Kernel tests exercise unaligned MDLs in both directions
for DMA and PIO and check buffer contents. QEMU does not establish coherency
on real hardware. Mapping exhaustion remains an explicit gap: the routine
logs and returns when a system mapping cannot be allocated; Windows behavior
for this case is `INFO_NEEDED`.

The next graphics boot exposed a separate stack overflow: a timer interrupt
ran a graphics DPC on the interrupted worker's nearly full 12 KiB stack.
Live R11 frames led through `KiRetireDpcList` to DxgKrnl's retirement callback.
The ARM dispatcher now retires DPCs on the resident `LoaderStartupDpcStack`
already supplied by FreeLoader. The stack-switch fixture checks the actual SP,
alignment, PRCB argument and return to the original stack. Its EFI run passes;
the separate kernel boot then reaches display-bridge and CSRSS initialization.
The updated `csr` checks pass with both VirtIO MMIO v1 and v2.

The I/O-cache contract comes from Microsoft's
[KeFlushIoBuffers documentation](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-keflushiobuffers),
with processor operations from the
[Armv7-A/R manual](https://documentation-service.arm.com/static/5f8daeb7f86e16515cdb8c4e).
The bus-write result follows
[HalSetBusDataByOffset](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-halsetbusdatabyoffset).
Existing ARM PRCB fields and loader allocations are explicit LiberNT
scaffolding; no Windows layout is inferred from the successful boot.

The ARM stack handoff copies live stack contents with IRQ/FIQ masked, repairs
the frame chain and trap/exception/VFP links, updates existing thread stack
metadata, and frees the old small stack only after SP moves. User SP and user
register values stay intact. Stack metadata is published before the handoff
with an explicit barrier. GUI conversion runs on the new stack through the
existing process/thread callbacks; failed conversion preserves its error.
An existing unparenthesized `KiGetPreviousMode` macro prevented the preparation
path from recognizing user mode in comparisons. The new allocator-selection
regression failed before the macro fix and passed in a separate run afterward.
The production stack switch and dispatcher also pass the EFI ABI fixture.

The R11 frame-chain and eight-byte stack-alignment requirements come from
Microsoft's [ARM ABI conventions](https://learn.microsoft.com/en-us/cpp/build/overview-of-arm-abi-conventions).
The existing ARM32 thread fields remain LiberNT consumer scaffolding; this
change adds no fields or claims of Windows ARM64 layout equivalence.

LLD sets the Thumb bit on ARM function forwarder RVAs, corrupting forwarder
strings at even addresses. Spec2def marks these export-directory entries as
data while preserving function import-library declarations. The linked-PE
regression reproduces the corrupted strings before the fix and checks complete
forwarder names and Thumb code addresses afterward:

```sh
python3 modules/rostests/tests/spec2def/test-arm-forwarders.py \
    output-Clang-arm-debug/host-tools/bin/spec2def \
    ~/.local/opt/rosbe/llvm/bin/clang ~/.local/opt/rosbe/llvm/bin/llvm-readobj
python3 scripts/test-arm-uefi.py --stage csr --observe
python3 scripts/test-arm-uefi.py --stage csr --observe --virtio-modern
```

Contracts come from Microsoft's [Mount Manager requests](https://learn.microsoft.com/en-us/windows-hardware/drivers/storage/supporting-mount-manager-requests-in-a-storage-class-driver),
[volume-arrival notification](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/mountmgr/ni-mountmgr-ioctl_mountmgr_volume_arrival_notification)
and [PE export format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format).
PL011 register behavior follows the [Arm PL011 reference manual](https://documentation-service.arm.com/static/5e8e3655fd977155116a9042).
The firmware/platform tests have no Windows 11 ARM32 baseline.
ARM C static analysis still stops on the existing `crtdefs.h` `errno_t` parser
error. Host Spec2def analysis completes without findings. The older Spec2def
snapshot suite also reports differences with the unmodified host generator;
it is not counted as passing.

The changed common `cmboot.c` and native entry code additionally pass syntax
compilation for amd64, ARM64 and RISC-V64. No Windows binaries were inspected
for this milestone; headers, published specifications and existing LiberNT
consumers supply the ABI scaffolding.

The latest compiler evidence is in `output-Clang-arm-debug/arm-build.log`,
and the reusable guest log is `output-Clang-arm-debug/arm-boot.log`.

Do not enable WoW64/FEX for a 32-bit host. ARM64EC/CHPE and ARM64 structure
layouts are not a substitute for ARM32 ABI evidence. Audit pointer truncation,
page-table geometry and explicit memory ordering in shared code as each
subsystem becomes executable.

## VirtIO keyboard and pointer

Build and run the input integration test with:

```sh
python3 -B scripts/test-arm-uefi.py --stage csr --input
```

`--input` selects modern VirtIO MMIO, adds `virtio-keyboard-device` and
`virtio-tablet-device`, and includes the HID/class drivers and their existing
keyboard/mouse INF configurations. The headless QMP test runs for 30 seconds;
it checks cursor movement outside the clock region, lock-screen dismissal by
Space, and a click followed by typing in the password field. Its final capture
is `output-Clang-arm-debug/arm-screen.ppm`. Like the other observation stages,
it leaves the guest stopped for inspection; release that guest before starting
another VM or rebuilding the image.

For an interactive QEMU run using the prepared image, include these options
alongside the existing firmware, disk and `ramfb` options:

```sh
-global virtio-mmio.force-legacy=false \
-device virtio-keyboard-device -device virtio-tablet-device
```

The dedicated `virtinput` HID minidriver translates VirtIO evdev events to HID
reports; existing `kbdhid`, `mouhid`, `kbdclass` and `mouclass` deliver input to
Win32k. Event and LED status queues use contiguous noncached DMA memory, split
rings, explicit publication barriers and GIC interrupts. Platform discovery
currently scans the QEMU virt MMIO slots in the driver's platform file; firmware
resource enumeration and real boards remain outside this milestone. IRQ routing
is restricted to CPU 0, matching the current HAL; SMP input routing, suspend and
hot removal have not been validated.

The ARM interrupt connection implementation now installs, shares, synchronizes
and removes device ISRs. The EFI fixture covers those operations and failure
rollback, along with evdev-to-HID report boundaries, modifiers, key rollover,
relative motion splitting and absolute coordinate scaling. The QEMU input run
separately exercises the real GIC, ISR and DPC path.

Two existing obstacles also blocked this path. HID collection enumeration
previously required optional hardware IDs from the parent root PDO, preventing
HID keyboard/mouse IDs from being generated. It now derives the collection IDs
when the parent returns `STATUS_NOT_SUPPORTED`, while preserving other errors.
The ARM Clang keyboard-layout link previously merged code into data; LLD then
set Thumb bits on data addresses. Keeping ARM code and data sections separate
restores aligned layout pointers and working scan-code translation. The QMP
keyboard check catches the resulting dropped-key regression.

Protocol definitions come from the
[VirtIO 1.2 input specification](https://docs.oasis-open.org/virtio/virtio/v1.2/virtio-v1.2.html),
[USB HID specification and usage tables](https://www.usb.org/hid),
[Microsoft HID minidriver registration](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/hidport/nf-hidport-hidregisterminidriver),
[read-report contract](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/hidport/ni-hidport-ioctl_hid_read_report),
[HID collection hardware IDs](https://learn.microsoft.com/en-us/windows-hardware/drivers/hid/hidclass-hardware-ids-for-top-level-collections)
and the [published keyboard-layout samples](https://github.com/microsoft/Windows-driver-samples/tree/main/input/layout).
QEMU's published `hw/arm/virt.c` and `hw/input/virtio-input*.c` describe the
platform slots and advertised event capabilities. Existing ARM32 `KINTERRUPT`
fields are LiberNT scaffolding; no Windows ARM64 structure offsets are used.
The common HID change passes amd64, ARM64 and RISC-V64 syntax compilation.
Windows 11 runtime comparison and real hardware validation were not performed.
C static analysis remains blocked by the existing `crtdefs.h` `errno_t` parser
error, so this milestone does not claim a clean cppcheck result.

## Local account and desktop fixture

```sh
python3 -B scripts/test-arm-uefi.py --stage csr --login
# Quit the stopped QEMU guest before the next command.
python3 -B scripts/test-arm-uefi.py --stage csr --login --reuse-image
```

The second command preserves the image and firmware variables. Both runs use
modern VirtIO input, the same disk and bounded headless QEMU execution. After
shutting down the test guest, boot `arm-uefi-tests.img` normally, dismiss the
lock screen, select `tester`, and enter `armv7`. The harness leaves its guest
paused on the GDB port; shut it down before manually opening the image.

The fixture supplies SAM/SECURITY/DEFAULT hives, a default profile, US locale,
RpcSs, shell COM registrations from the existing setup generator, dynamically
loaded shell DLLs and neutral `.mun` resources. It uses the normal setup,
authentication, profile and Userinit paths. The session verifier runs with the
user token alongside Userinit and requires both `Progman` and `Shell_TrayWnd`.
The test password is deliberately public and belongs only to this fixture.

Profile provisioning now resets inherited permissions throughout the newly
created user hive, retaining explicit ACEs. Updating only the root with
`RegSetKeySecurity` left existing protected template keys inaccessible to the
new user. The operation uses Microsoft's documented
[TreeResetNamedSecurityInfoW](https://learn.microsoft.com/en-us/windows/win32/api/aclapi/nf-aclapi-treeresetnamedsecurityinfow)
contract, with `KeepExplicit=TRUE`; the ordinary user's registry write check
failed before the fix and passes after it. No Windows 11 runtime baseline was
available for this shared profile change.

Account creation and logon use the documented
[NetUserAdd](https://learn.microsoft.com/en-us/windows/win32/api/lmaccess/nf-lmaccess-netuseradd),
[LsaAddAccountRights](https://learn.microsoft.com/en-us/windows/win32/api/ntsecapi/nf-ntsecapi-lsaaddaccountrights)
and [LogonUserW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-logonuserw)
interfaces. The account receives interactive logon permission. Everyone receives
the standard traverse privilege, consistent with the existing `defltwk.inf` and
Microsoft's [traverse privilege documentation](https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/checking-for-traverse-privilege-on-irp-mj-create).
The account remains a normal user.

The CRT `_onexit` export is a Thumb function. `spec2def -import-data` keeps the
import library pointer-only while emitting a function export in the provider,
so the linker preserves its Thumb bit. Exporting it as DLL data had entered
ARM state when Explorer called it. The linked-PE regression checks the actual
ARM export bit, IAT import and coexistence with the consumer's local wrapper;
it also checks import/export definitions for i386, amd64 and ARM64.

Scalar ARM memory primitives leave VFP state untouched and use aligned word
accesses. They replace the unoptimized byte-at-a-time C routines in the ARM
runtime. With the old routines, a 1 ms clock period starved startup while
constructing interrupt frames; the new routines reach account setup in about
four guest seconds without changing the timer period. EFI memory and exception
regressions pass with the production routines linked in.

## Windows RT reference

Windows RT 8.1 may provide an additional black-box ARM32 NT 6.3 baseline;
it is not the NT 10 API reference. Microsoft's
[Surface recovery portal](https://support.microsoft.com/en-us/surface-recovery-image)
provides device recovery downloads. A Surface RT recovery archive is also
listed at `https://archive.org/download/surface-rt-8.1/SurfaceRT_BMR_20.8.17.0.zip`.
Its archive metadata reports 3,208,320,104 bytes and SHA-1
`a7abacd2af81b447c42cb44c04ae9a616d4f91e2`.

The attempted mirror download was too slow and remains incomplete in the
build directory's `reference/` directory. No RT reference run has been made.
A Surface-specific recovery image must not be assumed to boot on QEMU `virt`.
Keep a complete reference immutable and use throwaway overlays for tests.
