# ARMv7-A port

The current milestone is an ARM32 FreeLoader running under EDK II on QEMU
`virt`, with a Cortex-A15, one CPU and 512 MiB of RAM. This is an NT 10.0
port in progress, not a working ARM32 kernel or a ReactOS NT 5.2 build.

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

The test harness needs `qemu-system-arm`, `mkfs.vfat`, and mtools. Its default
firmware is `/usr/share/qemu/edk2-arm-code.fd`, with the corresponding
`edk2-arm-vars.fd` beside it. Override the code path with `--firmware`.
This must be ARM32 firmware; AArch64 AAVMF is a different execution environment.

The harness builds `arm-uefi-tests.img`, boots `EFI/BOOT/BOOTARM.EFI`, and reuses
`QEMU_VARS.fd`. These are disposable test media, not a full OS image. It
performs two independent boots:

- `abi`: a real EFI regression test of nonzero and zero frame-free `longjmp` returns,
  R4–R11, D8–D15 and FPSCR preservation, explicit establisher-frame capture,
  portable `floor`, `pow`, `log10` and trigonometric functions, and 32/64-bit
  interlocked-add results. It also checks the `setjmp.h` then `setjmpex.h`
  include order and frame capture through the extended macro.
  Build-time checks pin the ARM32 SDK unwind-table sizes, offsets and context
  flags. Structured unwinding itself remains unimplemented in the ARM runtime
  and is not covered by this test.
- `loader`: the actual FreeLoader initializes firmware services, mounts FAT32,
  reads `freeldr.ini`, and parses the selected entry. No kernel is supplied on
  the fixture. Passing this stage does not mean that kernel startup works.

Use `--stage abi` or `--stage loader` to select one boot. Each boot is headless,
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
- [QEMU virt documentation](https://www.qemu.org/docs/master/system/arm/virt.html)
  describes the virtual platform. Hardware discovery and the kernel HAL must
  ultimately consume its device tree rather than reuse OMAP assumptions.

The old ARM loader/NDK structures are existing implementation scaffolding.
They have not been established as NT 10 ARM32 layout parity by these tests.
No Windows binaries have been disassembled or decompiled for this work.

## Full-build status

The attempted `ninja ntoskrnl` build reaches system-DLL linking, which is a
prerequisite of the kernel's code-integrity catalog. ARM consumers of LLVM
libunwind fail to link because the ARM32 RTL and ntdll export list do not yet
provide `RtlUnwindEx`, `RtlVirtualUnwind`, `RtlRestoreContext` and
`RtlLookupFunctionEntry`. No dummy exports were added to mask this gap.

An isolated compilation of ARM `kiinit.c` also identifies NT 5-era consumers
of `KPRCB.DpcInterruptRequested` and `SetMember`, scalar assignment to the
modern `KTHREAD.Affinity`, and an incorrect pointer declaration for the vector
table. These must be ported to the existing NT 10 structures; restoring old
fields would not fix the underlying contract.

The shared headers and assembler-offset generator also compile for amd64 and
i386 with the generated SDK and those target defines. This is a compile check,
not a claim of runtime parity or a full build of those ports.

## Remaining kernel work

The configure script and LLVM-runtime build complete. The EFI ABI and loader
boots pass; `d3dcompiler_47` also links after the interlocked shims are supplied.
The latest build evidence is retained in `output-Clang-arm-debug/arm-build.log`.

A successful FreeLoader boot must be followed by all of these steps before
claiming an OS port:

1. Replace the old loader's fixed low-memory PDR allocation and ineffective
   paging hooks with mappings valid for QEMU RAM starting at 0x40000000.
   Reconcile PA/VA conversion and self-map consumers with NVS. Finalize
   `ExitBootServices`, stack ownership, cache/TLB maintenance and kernel entry.
2. Complete the ARM NVS architecture backend. `arch/arm/hooks.c` currently
   contains `STATUS_NOT_IMPLEMENTED` paths and no-op PTE/TLB operations;
   it cannot initialize the NT virtual memory subsystem.
3. Implement the `virt` HAL: device-tree discovery, GICv2, architectural timer,
   PL011, interrupt delivery and processor startup. `halarm` currently selects
   OMAP3 sources and is not a Cortex-A15 `virt` HAL.
4. Complete kernel entry, exception vectors, context switching and user-mode
   transitions. The retained ARM `KiSystemStartup` contains an unconditional
   loop and is not a usable entry point.
5. Complete ARM32 unwind/exception support, ntdll startup and a boot storage
   path that remains usable after firmware services have ended. UEFI's VirtIO
   Block I/O support does not provide an NT VirtIO driver.
6. Build the kernel, HAL, required drivers and native user space; boot them
   in QEMU and validate the first native process before moving on to SMP,
   graphics, broader NT 10 APIs or physical boards.

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
