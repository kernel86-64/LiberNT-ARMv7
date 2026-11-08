# ARMv7-A port

The current milestone boots the real ARM32 kernel through EDK II and FreeLoader
on QEMU `virt`, with a Cortex-A15, one CPU and 512 MiB of RAM. The kernel reaches
`KiSystemStartup`, NVS initialization, native VirtIO/FAT disk access and the
first user-mode `smss.exe` entry after `ExitBootServices`. The `user` fixture
passes through ntdll initialization and `NtContinue` into `NtProcessStartup`.
This remains an NT 10.0 port in progress, not a complete operating-system boot.

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

The test harness needs `qemu-system-arm`, `mkfs.vfat`, and mtools.
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
  include order and frame capture through the extended macro.
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

Use `--stage abi`, `--stage loader`, `--stage kernel`, `--stage clock`,
`--stage storage` or `--stage user` to select one boot. Add `--virtio-modern`
to test VirtIO MMIO version 2; the default QEMU transport is version 1.
The default `both` runs the ABI and loader stages. Kernel, clock, storage and
user stages leave the successful guest stopped for inspection. The harness
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
service ID and rejects IDs at or above the service-table limit. Full win32k
service-thread conversion remains outside this bootstrap implementation. Runtime debug output uses the existing private ARM UDF
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
PnP storage stack. Full Session Manager setup and Win32 startup remain outside
the minimal image.

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
partitions, FDT discovery, asynchronous IRQ completion and full PnP/mount-manager
integration remain gaps. Unsupported dump/storage controls return failure.

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

Both legacy and modern MMIO boots passed the FAT regression. The user fixture
reaches the actual smss native entry; continuing the minimal image reaches
`SmpLoadDataFromRegistry` and currently terminates with `0xc000021a`, carrying
`STATUS_OBJECT_NAME_NOT_FOUND`, because the Session Manager configuration is
not populated. The implicit activation-context setup also reports a missing
path. This is not a working desktop or complete Session Manager startup.

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
