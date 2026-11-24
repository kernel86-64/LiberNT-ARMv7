# Samsung Galaxy Note GT-N7000 bring-up

Target: Exynos4210, two Cortex-A9 cores, 1 GiB installed RAM, microSD boot
through EDK II firmware. The real kernel, NVS, storage stack, SMSS, CSRSS,
LSASS and the BasicDisplay adapter have run on the GT-N7000. The current
graphics milestone is `DxgkAdapterStart` followed by
`desktop ready 800x1280 (scanout)`.

## Current state

On 2026-11-12 the GT-N7000 reached the Explorer desktop from a microSD
payload without flashing firmware. Winlogon loads the Administrator profile,
starts `userinit.exe`, and `explorer.exe` creates the shell and taskbar. The
graphics path initially executed stale ARM `MOVW/MOVT` immediates after
relocation. The loader now invalidates the instruction cache after a
relocated image is committed, including the system-image and user-mode
relocation paths.

Validated locally:

| Check | Result |
| --- | --- |
| N7000 storage, RTC and MCT tests | PASS |
| N7000 `DxgkAdapterStart` | reached |
| N7000 `DISPLAY_BRIDGE: ... desktop ready 800x1280 (scanout)` | reached |
| N7000 Explorer desktop and taskbar | reached |
| `SMDKC210 --stage login` | PASS (28.84 s), Explorer desktop |
| `UEFI --stage user` | PASS (8.31 s) |

The shell still reports non-fatal activation-context, theme, COM proxy and
drag-drop errors. The payload has no Aero visual style and no network
provider enumeration, so those paths are not hardware milestones yet.

## Hardware result

The 2026-11-10 hardware run reached `ARM platform probe: COMPLETE` and its
output was received over UsbLogDxe. This establishes firmware-stage EFI
execution and log reception, without ExitBootServices or kernel startup.

| Observation | Actual firmware output |
| --- | --- |
| CPU | Cortex-A9 r2p1, MIDR `0x412fc091`, MPIDR `0x80000900` |
| Translation | ID_MMFR0 `0x00100103`: short descriptors; TTBCR `0` |
| Generic timer | ID_PFR1 `0x00000011`: absent |
| Coprocessor access | CPACR `0x00f00000` |
| Display | 800 x 1280, pitch 800 pixels, GOP format 1 (BGR reserved 8-bit channels) |
| Framebuffer | `0x5ec00000`, size `0x003e8000`, reserved in the memory map |
| Storage | 15 Block I/O handles; loaded-image device matches removable partition handle (disk 14) |
| ACPI | ACPI 2 configuration-table pointer `0x7b72f010` |
| Memory map | 61 descriptors, stride 48, version 1; highest reported RAM endpoint `0x7c000000` |
| OS-log candidate | `0x5e900000` through `0x5e97ffff` is reserved; ring contents/protocol not yet checked |

Addresses of allocated EFI objects and handles above describe this run only.
They must not become fixed loader/kernel addresses. The received working log
is `output-Clang-arm-debug/arm-boot.log` and can be overwritten by later runs.

## FreeLoader hardware result and firmware performance

The 2026-11-10 UsbLogDxe capture contains `FatMount(0) success`, the SD volume
label `N7000_EFI`, and `LoadIdentifier=LiberNT ARMv7 - N7000 FreeLoader test`.
The loaded configuration uses `SystemPath=\Windows`; no Windows directory
was installed for this test. The maintainer also observed working touch input
in FreeLoader. These establish loader configuration/input operation on A9,
not kernel startup.

The running firmware identifies itself as built on October 30, 2026 at
18:42:47. Its log loads `Drivers/TouchDxe/TouchDxe`, then reports an mXT family
`0xa1`, variant `0x03`, version `0x13`, build `0xaa`, and range 800 x 1280.
That driver is absent from the inspected published source revision below.
A replacement firmware build must resolve that source mismatch to preserve
working touch input. Firmware changes belong in
[the maintainer's fork](https://github.com/kernel86-64/Exynos441xPkg).

The reported minute-long Shell/FreeLoader startup and sluggish UI remain
unresolved. Source audit found full-frame cache maintenance on serial
newlines, uncached/unbuffered framebuffer accesses, and a newline jump that
bypasses initialization of the saved interrupt state. Their performance
contribution has not yet been measured on hardware.

The diagnostic EFI application measures a 100 ms Boot Services stall, RAM
copies, 16 serial lines, full-frame GOP read/restore, the active framebuffer
translation descriptor, and the existing PMU cycle counter when enabled.
It writes no storage, changes no MMU or PMU configuration, and restores the
screen pixels it reads. The Cortex-A9 path is specifically for N7000 and
uses its 24 MHz MCT; the QEMU virt path uses the architectural timer.

```sh
CCACHE_DISABLE=1 ninja -C output-Clang-arm-debug arm_firmware_perf -j8
python3 -B scripts/test-arm-uefi.py --stage perf --no-build
```

For N7000, copy `arm_firmware_perf.efi` to
`EFI/TOOLS/libernt-perf.efi` and have `startup.nsh` call `libernt-perf.efi`.
Preserve the FreeLoader startup script as `startup-freeldr.nsh`.
Capture the hardware run with:

```sh
python3 -B scripts/n7000-usblog.py --until 'Firmware performance: COMPLETE'
```

The first hardware diagnostic run completed on 2026-11-10:

| Measurement | N7000 result |
| --- | --- |
| ACTLR | `0x00000000` |
| CPU frequency observed through the enabled PMU | 1200 MHz |
| Boot Services Stall(100000) | 102253 us |
| RAM copy, four 1 MiB operations | 54768 us |
| Serial I/O, sixteen 47-byte lines | 979073 us |
| GOP full-frame read, 800 x 1280 | 53298 us |
| GOP full-frame restore | 50759 us |
| Framebuffer level-1 section descriptor | `0x5ec11c02` |

All reported service status codes were success. The test returned to the
UEFI Shell. These numbers establish a high cost for serial output; they do
not yet isolate its cache-maintenance, glyph-painting and interrupt costs.
The second hardware run read the RAM section descriptor `0x7a511c0e`
for an allocation at `0x7a541010`: Shareable, TEX=1, C=1, B=1. ACTLR was
still zero. Thus the firmware requests cacheable shared RAM while disabling
the Cortex-A9 mode needed to cache it. The
[Arm Cortex-A9 TRM](https://documentation-service.arm.com/static/5f035b2ddbdee951c1cd7172)
states that Inner Cacheable Shareable memory is treated as non-cacheable
when ACTLR.SMP is clear. The second run again measured 1200 MHz, 102246 us
for the 100 ms stall, 57334 us for RAM copies, and 980332 us for 16 serial
lines. It returned to Shell. A firmware fix and its performance effect have
not yet been tested.

A rebuilt firmware with Write-Back non-shareable DRAM was flashed to KERNEL
with the maintainer's explicit authorization. It booted on N7000, ran the
same microSD diagnostic, and returned to Shell. Firmware payload SHA-256:
`a2741c8171b073c00131f5ab307fd736b2d0267690963d1b396f5d9e8ea72403`.
EDK II is `edk2-stable202508` (`d46aa46c8361194521391aa581593e556c707c6e`).

| Measurement | Rebuilt firmware |
| --- | --- |
| ACTLR | `0x00000000` |
| RAM section descriptor | `0x7a501c0e`: Write-Back, non-shareable |
| CPU frequency observed through the enabled PMU | 800 MHz |
| Boot Services Stall(100000) | 100004 us |
| RAM copy, four 1 MiB operations | 21699 us |
| Serial I/O, sixteen lines | 48601 us |
| GOP full-frame read | 19248 us |
| GOP full-frame restore | 30459 us |
| Framebuffer section descriptor | `0x5ec11c02`, unchanged attributes |

All service statuses were success. The maintainer independently reported a
much faster UI. Serial output took about 20 times less wall-clock time.
These runs used different CPU frequencies and firmware builds; they are not
a controlled same-frequency benchmark of the one-line fix. The rebuilt
firmware also omits the unavailable TouchDxe. Framebuffer mapping and the
framebuffer serial implementation were not changed. The following hardware
boot also reached FreeLoader configuration loading (`FatMount(0) success`
and the expected load identifier/options). SD autostart was restored to
FreeLoader afterward.

The diagnostic also runs on QEMU virt. QEMU timings do not establish N7000
performance.

## Firmware baseline

The supplied `/home/kernel64/Projects/N7000PXEDebug/bootpayload.bin` has SHA-256
`0f9b634e10d004fc9286c04fa10787a1f2867e1f899fe9e8978c2bf7338ce4d1`.
Its source commit is unknown. `file` recognizes its ARM zImage boot wrapper;
it is not a standalone EFI application.

The inspected [Exynos441xPkg source revision](https://github.com/J0SH1X/Exynos441xPkg/tree/e36caa59a68eb394610245cbb40f9c746bd872ee)
is checked out at `~/Projects/Exynos441xPkg`. It has not been built or changed.
Its configuration provides the following starting evidence; these values have
not been verified against the supplied firmware binary or a LiberNT hardware run.

| Resource | Inspected firmware configuration | Source at that revision |
| --- | --- | --- |
| System RAM | Base `0x40000000`, size `0x3c000000` (960 MiB), with further reserved ranges | `CommonDsc.dsc.inc`, `Library/Exynos441xPkgLib/Exynos441xPkgMem.c` |
| GIC | Distributor `0x10490000`, CPU interface `0x10480000` | `CommonDsc.dsc.inc`, `AcpiTables/Platform.h` |
| microSD | HSMMC2/SDHCI at `0x12530000`, 4-bit bus | `Drivers/SdCardDxe/SdCardDxe.h` |
| Internal eMMC | MSHCI at `0x12550000`, separate controller | `Drivers/SdCardDxe/SdCardDxe.h` |
| Firmware clock interrupt | MCT global comparator 0, interrupt ID 89 | `Drivers/MctTimerDxe/MctTimerDxe.c` |
| Firmware log | `0x40f00000`, 1 MiB, `LOGR` header, reset at each boot | `Include/ExynosLogRing.h` |
| USB device controller | DWC2 at `0x12480000` | `AcpiTables/Platform.h` |

Before implementing native drivers, check the controller programming details
against the Samsung/Arm hardware specifications. Obtain framebuffer geometry
and usable/reserved memory from the actual GOP and UEFI memory map.

## First hardware payload

Build and test from the LiberNT source directory:

```sh
CCACHE_DISABLE=1 ninja -C output-Clang-arm-debug arm_platform_probe -j8
python3 -B scripts/test-arm-uefi.py --stage probe --no-build
```

The binary is
`output-Clang-arm-debug/boot/freeldr/freeldr/arm_platform_probe.efi`.
The phone has no keyboard. Install these two ordinary files on its microSD's
existing FAT volume:

```text
/startup.nsh                    <- modules/rostests/boot/arm/n7000-startup.nsh
/EFI/TOOLS/libernt-platform.efi  <- arm_platform_probe.efi
```

Keep any existing `startup.nsh` before replacing it. The inspected platform
boot manager explicitly starts the built-in Shell ahead of removable-media
boot options. The Shell then searches for `startup.nsh` on its execution path.
Our script starts `libernt-platform.efi` through the standard `EFI/TOOLS` search
path, without assuming the card's `fsN:` alias. No keyboard input is needed.
See the [UEFI Shell specification](https://uefi.org/sites/default/files/resources/UEFI_Shell_Spec_2_0.pdf),
sections 3.3.1 and 3.6.1. This assumes the supplied firmware's Shell has the
standard startup-script behavior; its exact source revision is unknown.

The payload does not replace the microSD's fallback bootloader. No firmware
rebuild or internal-storage flashing is required. Remove/restore `startup.nsh`
after the inventory run to return to the previous startup behavior.

The unattended script path was also tested on QEMU ARMv7 using the published
EDK II UDK2018 ARM Shell application (no firmware build):

```sh
curl -L --fail https://raw.githubusercontent.com/tianocore/edk2/UDK2018/ShellBinPkg/UefiShell/Arm/Shell.efi \
    -o output-Clang-arm-debug/arm-shell.efi
python3 -B scripts/test-arm-uefi.py --stage probe --no-build \
    --shell output-Clang-arm-debug/arm-shell.efi
```

The test starts Shell as `BOOTARM.EFI`, installs the same startup script and
probe layout, supplies no keyboard input, and verifies the inventory and
return to the Shell prompt. It passed in 11.82 seconds. This tests Shell
autostart independently of the N7000 firmware's board initialization.

It prints MIDR, MPIDR, ID_MMFR0, ID_PFR1, SCTLR, TTBCR, CPSR and CPACR; GOP mode
and framebuffer; loaded-image device; Block I/O media; configuration-table
addresses and the complete UEFI memory map. It reads no board MMIO addresses,
does not change page tables or call ExitBootServices, and performs no disk
writes. It allocates and frees a boot-services memory-map buffer and returns
to firmware. Its compiler options prohibit implicit FP/SIMD operations, as
required by the [ARM UEFI binding](https://uefi.org/specs/UEFI/2.10/02_Overview.html#detailed-calling-convention-3).
The QEMU run completed with `CPACR=00000000` and `TTBCR=00000000`.

The QEMU `probe` fixture uses `arm-uefi-tests.img`, overwrites `arm-boot.log`
and writes its milestone to `arm-uefi-results.json`. Firmware variables use
a disposable QEMU snapshot of the standard template. It does not repack
`LiberNT.img` or touch `QEMU_VARS.fd` belonging to an interactive guest.

## USB logging and debugger versions

Start the receiver before booting the phone:

```sh
python3 -B scripts/n7000-usblog.py
```

This receives only interface 0, endpoint `0x81`, on USB device `1209:0001`.
It overwrites `output-Clang-arm-debug/arm-boot.log`, prints the same bytes,
and waits without a time limit until receiving the completion marker. Press
Ctrl+C to stop it manually. USB errors still terminate the receiver. The
current user needs permission to access that USB device. The receiver uses
the same log lock as the QEMU harness; run those tests separately.

If the receiver reports `LIBUSB_ERROR_ACCESS`, the device was found but the
current user cannot open it for USB transfers. `lsusb` can list a device with
read-only access. Run the receiver with USB device access, for example:

```sh
sudo python3 -B scripts/n7000-usblog.py
```

Host transport checks (fragmented completion marker, partial USB timeout,
disconnect and failed interface claim):

```sh
python3 -B modules/rostests/boot/arm/test-usblog.py
```

The two original tools in `~/Projects/N7000PXEDebug` serve different interfaces:

| Tool | Interface | Purpose |
| --- | --- | --- |
| `usblog.py` | 0, IN `0x81` | Firmware log stream; waits across reconnects and creates a log per boot |
| `usbdbg.py` | 1, OUT `0x02`, IN `0x82` | Custom UDBG commands: ping, memory access, file transfer and reset |

Neither protocol is a KD/GDB transport. In the inspected firmware,
`FrameBufferSerialPortLib` copies serial writes into the log ring, consumed
by `UsbLogDxe`. `UsbFnExitBootServices` disconnects the USB controller and
disables its interrupts. Consequently this transport does not provide live
kernel debugging after ExitBootServices in that revision.

The local `usblog.py` matches upstream. The local `usbdbg.py` additionally
implements `oslog` at `0x5e900000` with an `OSLG` header, but the inspected
firmware source does not define/reserve that ring. The hardware probe does
show a matching reserved range in the supplied firmware's memory map, so the
binary differs from that source revision. Reservation alone does not establish
the ring header, producer or post-ExitBootServices transport. Its
`flash` command programs the internal KERNEL partition and is unrelated to
loading this microSD EFI payload.

## Kernel port requirements

### SMDKC210 processor/platform tests

```sh
python3 -B scripts/test-arm-smdkc210.py
```

This builds the explicit `arm_smdkc210_test` target in the existing ARM build
directory and runs QEMU `smdkc210` with its Cortex-A9 cores. Only the primary
core executes the tests. The artifact is `arm-smdkc210.elf`, loaded directly
into emulated RAM; it does not boot the phone firmware, FreeLoader or the NT
kernel. There is no disk image attached. `LiberNT.img` and `QEMU_VARS.fd` are
not opened or repacked by this test.

The reusable boot-table constructor is
`boot/freeldr/freeldr/arch/arm/shortdesc.c`. Native host checks run with
AddressSanitizer and UndefinedBehaviorSanitizer before the emulated run.
Descriptor expectations use ARM DDI 0406C.d, figures B3-4/B3-5 and table B3-6:
16 KiB first-level table, 1 KiB coarse tables, 1 MiB sections, 4 KiB small
pages, domain 0 client access, SCTLR.AFE=1 and TEX remapping disabled.
The constructor builds inactive tables; it does not provide live kernel PTE
updates, cache cleaning or a complete NVS backend. Unmapping a single page
can split an inactive section into a coarse table while preserving its
neighboring translations and access/cache attributes. Allocation failure
leaves the section unchanged; host and emulated tests cover this behavior.

The real emulated Cortex-A9 validates:

- Identity section mapping and 4 KiB high-address aliases of RAM.
- Read-only protection and privileged versus unprivileged access.
- First-/second-level translation faults and execute-never versus executable pages.
- A guard hole made by splitting a section, retaining the neighboring mapping.
- Break-before-make replacement followed by invalidation of the affected TLB entry.
- External GIC self-directed SGI, IRQ entry/return and acknowledgement.
- Exynos MCT free-running counter and at least five comparator-0 interrupts.

The MMU tests run with data/instruction caches disabled; cached firmware
handoff and the second CPU remain separate milestones. UART/GIC/MCT MMIO
resources are confined to the board test. Register/interrupt evidence is the
inspected EDK II configuration, the Arm architecture manual,
[GIC architecture specification IHI 0048B.b](https://documentation-service.arm.com/static/5f8ff196f86e16515cdbf969), and
QEMU's [Exynos4210 model](https://github.com/qemu/qemu/blob/master/hw/arm/exynos4210.c)
and [MCT model](https://github.com/qemu/qemu/blob/master/hw/timer/exynos4210_mct.c).

The harness is headless, has no networking, uses the shared ARM test lock,
overwrites `arm-boot.log`/`arm-uefi-results.json`, and stops a failed/stalled
guest for GDB on localhost port 1234. Its limits are 30 seconds total and
6 seconds without log progress. The verified milestone is processor/MMU,
GIC and MCT operation, not kernel startup. The active NVS ARM backend still
requires LPAE; FreeLoader now also constructs short-descriptor tables.

Regression validation after adding this test: `uefildr`, `arm_uefi_test`,
`ntoskrnl` and `hal` build; QEMU virt `abi`, `loader` and `kernel` stages pass.
The latter reaches NVS initialization and the loaded kernel/HAL image list.
The interactive desktop image is not repacked. Simple probe/ABI/loader/kernel/
clock fixtures use disposable variable snapshots, allowing their runs without
overwriting an interactive VM's `QEMU_VARS.fd`.

### Remaining kernel work

1. Connect the NVS short-descriptor architecture backend and validate
   the complete FreeLoader-to-kernel handoff on A9. The existing ARM32 path uses LPAE, 64-bit PTE slots
   and three translation levels. Cortex-A9 implements short descriptors:
   ID_MMFR0.VMSA is 3, whereas long-descriptor support starts at 5. Reusing
   the LPAE entry points or truncating their PTEs cannot work.
2. Provide Exynos platform discovery/mappings, GIC and MCT clock handling
   through the architecture/HAL layer. The QEMU virtual generic timer and
   PL011 MMIO fallback cannot be used on this board. Initially start one CPU;
   bring up the second after uniprocessor boot works.
3. Provide native microSD controller/card support for the system volume.
   Firmware Block I/O is available only until ExitBootServices; the VirtIO
   boot-storage driver used on QEMU does not drive HSMMC2.
4. Establish a post-ExitBootServices log/debug transport and reserve its
   buffers through the actual loader memory map. The firmware USB service
   cannot simply be called after boot-services teardown.
5. Add board power control and then native input/display support. Retaining
   the GOP framebuffer can give early graphics; it does not supply touch
   or button drivers, clocks, panel recovery or suspend/resume.

CPU evidence is the [Cortex-A9 technical reference manual](https://documentation-service.arm.com/static/5f035b2ddbdee951c1cd7172)
and the [ARMv7-A/R architecture reference manual](https://documentation-service.arm.com/static/5f8daeb7f86e16515cdb8c4e)
(ID_MMFR0.VMSA and ID_PFR1.GenericTimer). Keep NVS; the retired ReactOS ARM3
memory manager is not a replacement backend.

QEMU offers [Exynos4210 `nuri` and `smdkc210` machines](https://www.qemu.org/docs/master/system/arm/exynos.html),
which can exercise Cortex-A9 processor/controller work. Neither emulates the
GT-N7000 board in full, and the phone firmware is not established to boot on
them. QEMU virt remains the regression target for the existing desktop port;
the platform-probe result alone does not establish any kernel hardware support.

## Windows RT driver source set

The supplied [exynos4_woa_drivers repository](https://github.com/J0SH1X/exynos4_woa_drivers/tree/9afc9a2939a6cecb90f0ec5d6a9700490aed678c)
is checked out at `~/Projects/exynos4_woa_drivers`. It contains source for
`exynosclk`, `exynosgpio`, `exynosi2c`, `mxt540e`, `exynosusb`, `exynosstor`
and `exynossd`. Individual files carry BSD-2-Clause-Patent or MS-PL notices;
check each file and its provenance before adapting it. No driver code or
binaries from that checkout have been imported into LiberNT.

`exynosstor` targets Storport on RT 8.1; `exynossd` targets Windows 10 Sdport.
I2C, GPIO and touch require their KMDF/SpbCx/GpioClx/HID contracts as well as
the native controllers. Their presence is not evidence that those binaries
already run on LiberNT.

`common/oslog.h` defines the ring at `0x5e900000`, size `0x80000`, magic
`OSLG`, matching the local debugger's definitions and the range reserved by
the actual firmware. The repository README still names `0x40e00000` and is
not the authority for that address. Live USB after ExitBootServices depends
on an OS-side driver such as `exynosusb`; the firmware's USB service alone
does not provide it.

## NVS short-descriptor preparation

NVS now allocates, aligns, adopts and frees a complete 16 KiB root span.
Slot reverse lookup handles entries beyond the first root page. These paths
are covered by native ASan/UBSan checks for 4-byte and 8-byte PTE words.

ARM software PTEs now reserve both low bits as zero, as required for a
hardware-invalid short descriptor. A five-bit tag combines the software
kind and the pagefile/cache selector; five protection bits and the payload
beginning at bit 12 remain separate. This retains all twenty PFN bits in a
32-bit word. The ARM LPAE build uses the same software encoding in its
64-bit PTE words; other 64-bit architecture defaults retain their encoding.

The native test reproduced a software PTE with a hardware-valid low-bit
pattern before the fix. Afterward all kind/protection combinations and
all eight file selectors for pagefile/resident entries pass, including
PFN `0xfffff`. QEMU virt/A15 reached the native user-process milestone
(`user: PASS`, 8.81 seconds). SMDKC210 processor/MMU tests also passed;
those tests do not boot FreeLoader or the kernel.

The short-descriptor NVS backend is not connected yet. Valid ARM short
PTEs have no spare software flag bits corresponding to the existing LPAE
write-intent/COW flags. Their metadata storage and the loader's coarse-table
allocation must be implemented together before enabling this backend.
Kernel startup on N7000 has not been established by these changes.


## FreeLoader short-descriptor handoff

FreeLoader selects the translation format from ID_MMFR0.VMSA. For short
translations it allocates an aligned 16 KiB root, constructs sections/coarse
page tables, and unmaps individual pages through section demotion. Normal
RAM uses non-shareable Write-Back attributes for the current single-core
boot path; device pages remain device mappings.

The production ArmEnablePaging assembly now has a short-descriptor branch
that programs TTBCR, TTBR0, TTBR1, DACR and SCTLR.AFE/XP/TRE appropriately.
SMDKC210 tests link that same assembly, omitting only its COFF unwind
annotations. They verify the selected TTBR and TTBCR, then exercise the
existing translation, permission, XN, TLB, GIC and MCT tests. The run reached
`FreeLoader short-descriptor paging handoff: PASS` and the platform test's
final PASS marker. This is an execution test of the loader's MMU switch on
emulated Cortex-A9, not a boot of FreeLoader or ntoskrnl on that machine.

The modified FreeLoader builds, and the existing QEMU virt/A15 LPAE path
again reaches `user: PASS` (8.31 seconds). No hardware kernel boot was
attempted: NVS still rejects short descriptors, and early kernel debug/HAL
still need Exynos platform support before this image can continue on N7000.


## First real kernel entry on N7000 (2026-11-11)

The experimental SD payload loads ntoskrnl, HAL, the screen-debug alias,
boot video libraries, NLS files and a minimal SYSTEM hive. A missing
`kdscreen.dll` alias and repeated page mappings within an existing 1 MiB
section initially prevented the handoff. The latter now accepts an
identical page translation without replacing or demoting the section;
the focused native regression failed before the fix and passed afterward.

Platform discovery recognizes the existing ACPI BIOS resource and its
RSDT/XSDT OEM identity (`SAMSNG`, `EXY4210 `). These definitions and the
GIC addresses come from the supplied Exynos441xPkg ACPI tables. The loader
maps the actual Exynos GIC interfaces at the HAL's bootstrap virtual
addresses. It reserves the existing driver OS-log ring at `0x5e900000`,
size `0x80000`, and maps it as device memory at `0xfff40000`. The `OSLG`
header and ring format follow the published exynos4_woa_drivers
`common/oslog.h` and the supplied usbdbg.py reader. No NT structure fields
were added or repurposed.

Early kernel serial output detects this dedicated short-table mapping
and writes to the ring instead of touching the QEMU PL011 registers.
This diagnostic does not provide live USB after ExitBootServices. After
a warm restart, the host recovered the ring through the firmware's USB
reader and paused SD autostart to preserve the result.

The recovered journal contains, in order:

```text
LiberNT FreeLoader: tables ready
LiberNT FreeLoader: ExitBootServices returned
LiberNT FreeLoader: MMU switched
LiberNT kernel: entry reached
ARM kernel: KiInitializeSystem, ... PCR=FFDFF000
*** STOP: 0x0000001A (0x4C504145,0x00000000,0x00000000,0x00000000)
```

The excerpt normalizes some damaged characters in the recovered text;
the raw bytes were captured in the single working `arm-boot.log`,
which subsequent tests may overwrite.
The stop parameters identify the existing MiInitializeKernelVaLayout
check in `ntoskrnl/nvs/arch/arm/cpu.c`: TTBCR.EAE is clear, and NVS still
requires LPAE. This establishes real kernel entry and an early bugcheck,
not a working NVS short-descriptor backend or a usable OS. A graphical
blue screen was not established.

New EFI tests exercise Exynos ACPI identification, loader virtual-pointer
conversion, persistent-ring initialization and wraparound. QEMU virt ABI
checks passed (6.31 s), followed by the normal user-process boot test
(8.31 s). The interactive virt image was not repacked or stopped. Changes
remain uncommitted pending an explicit LiberNT commit request.


## Short PTE hardware conversion and backing space

FreeLoader's short coarse tables now occupy distinct 4 KiB pages. The MMU
uses the first 256 32-bit entries; the remaining 3 KiB is zeroed and kept
available for NVS software data. This prevents metadata for one table from
sharing storage with another hardware table. The original count of available
coarse tables is retained. SMDKC210 runs the production MMU switch with this
layout, including the translation and protection checks.

The NVS ARM backend now has a converter between its existing 64-bit logical
PTE and 32-bit hardware entries. It accepts the loader's supported small
pages, 1 MiB sections and page-aligned coarse tables, and rejects unsupported
forms and physical-address truncation. The hardware word does not carry
COW or write intent; a host test verifies that different logical COW and
clean-writable entries can yield the same hardware word. Hardware-invalid
entries encode as zero. The host ASan/UBSan test cross-checks both directions
against the actual FreeLoader table builder.

The converter and spare backing space are preparation for connecting NVS
read/write and bootstrap adoption. The guard that stopped the real kernel
at `0x1A/0x4C504145` remains in place. This new layout has not been copied to
the phone; the last hardware milestone is still the confirmed kernel entry.


## Short-descriptor NVS connected on N7000

The short backend now adopts the loader's hardware descriptors into separate
64-bit software shadows. Coarse tables use a 4 KiB allocation (1 KiB hardware,
2 KiB shadow); roots use a 64 KiB aligned allocation (16 KiB hardware,
32 KiB shadow and padding). Table slot addressing and PFN backreferences account
for the shadow prefix. Physical bootstrap writes use the production assembly
covered by the SMDKC210 test. Other architectures retain zero prefixes.

The Exynos HAL uses the mapped loader log region as its early platform handoff.
It must not dereference the ACPI root's physical address at this stage: ACPI
reclaim memory is LoaderSpecialMemory and is absent from the initial mappings.
Exynos MCT provides the performance counter and global comparator clock IRQ 89;
Cortex-A9 has no architectural Generic Timer. Exynos RTC at `0x10070000` is
the persistent wall clock. FreeLoader maps it at the RTC bootstrap virtual
address; the HAL reads stable BCD fields and writes them through `RTCCON`.
SMDKC210 runs the same counter, acknowledge and RTC functions used by the HAL.
The kernel clock uses comparator 0 auto-increment mode; the processor test
checks both one-shot and periodic operation.

### Exynos RTC

The Exynos4210 RTC block at `0x10070000` supplies the persistent wall clock.
Register offsets and BCD field layout follow the published
[QEMU Exynos4210 RTC model](https://github.com/qemu/qemu/blob/master/hw/rtc/exynos4210_rtc.c).
FreeLoader maps the block at the existing RTC bootstrap virtual address. The
HAL reads the BCD fields with a stable-seconds retry, validates them through
`RtlTimeFieldsToTime`, and writes normalized fields after enabling `RTCCON`.
The year keeps the RTC's three BCD digits and maps them to 1900-2899.

The SMDKC210 processor test programs 2024-02-29 12:34:50, checks the readback,
then programs 2024-02-29 23:59:58 and verifies the 2024-03-01 00:00 rollover
before restoring the original registers. The raw test passed in 3.1 seconds.
The first physical N7000 run found `RTCCON=0` and malformed BCD fields, so the
kernel test now initializes an invalid clock instead of requiring prior
validity. On the GT-N7000 it reported `ARM N7000 RTC tests: PASS`; direct
debugger writes showed the registers accepting the values and the seconds
counter advancing at 1 Hz. QEMU virt kept the PL031 path and again passed ABI
(5.91 seconds), loader (12.22 seconds) and native user entry (8.31 seconds).
SMDKC210 repeated the updated kernel test in 10.47 seconds.

A hardware run reached:

```text
ARM short-descriptor NVS tables prepared: root=43f00
Memory manager: starting
Boot page tables adopted: root at 000043f00000, 19 tables, 958 regions, 923 MB free
Memory: 911 MB available, commit limit 915 MB
*** STOP: 0x0000001E (0xC000001D,0xC38CB08A,0x80007C18,0x00000000)
```

The faulting instruction at kernel RVA 0xf608a was `mrrc p15,1,r1,r0,c14`,
from the scheduler's inline ReadTimeStampCounter. This was a second Generic
Timer consumer outside the HAL. It now uses KeQueryPerformanceCounter, keeping
the scheduler on the HAL-selected time base. A subsequent hardware run passed this instruction and reached boot-driver
initialization, stopping at `0x7B INACCESSIBLE_BOOT_DEVICE` because the
minimal fixture has no storage drivers. Short-descriptor and long-descriptor abort formats,
including their different breakpoint and alignment codes, are now decoded
separately and checked by SMDKC210 tests.

Manual long-button resets damaged the RAM ring in the observed runs. A PMU
software reset preserved a controlled test record byte for byte. For kernel
tests, scripts/test-arm-n7000.py arms the existing Exynos watchdog near loader
handoff, writes the hold startup.nsh while FreeLoader is already running, waits for
the automatic reset and recovers
the OS ring into the one arm-boot.log. The watchdog's measured maximum period
was about 21.5 seconds; BootShim disables it on the next boot. The test does not
flash firmware or deploy binaries: copy the matching loader/kernel/HAL first.
The host USB debugger module and the prepared n7000-startup-original.nsh are
required inputs. A watchdog reset preserved the full kernel log above.

Host NVS lifecycle, short PTE conversion and shadow backing tests passed under
ASan/UBSan. SMDKC210 passed the production MMU switch, physical table writer,
abort decoding, GIC interrupts and MCT comparator checks. QEMU virt/A15 reached
native SMSS entry after the HAL and abort fixes (8.71 seconds). The running
interactive image and its QEMU process were left untouched. Changes remain
uncommitted.

### Full Cortex-A9 firmware and microSD kernel test

The Exynos441xPkg `--smdkc210` build profile boots its normal EDK II core,
ACPI tables, SDHCI PIO driver and FAT driver on QEMU SMDKC210. This profile
uses UART output, omits the phone's USB and keypad drivers, skips internal
eMMC discovery and the absent MAX8997 microSD power control. The default
build retains the N7000 configuration. `Artifacts/bootpayload.target`
identifies the selected profile; never flash a `smdkc210` payload on the phone.

Prepare the existing ARM user fixture, close its retained test VM using QMP,
then run:

```sh
python3 scripts/test-arm-smdkc210.py --firmware ../Exynos441xPkg/Artifacts/bootpayload.bin
```

The harness requires the `--smdkc210` firmware profile and reuses
`arm-uefi-tests.img`, `arm-boot.log`, the ARM test lock and debugger port.
It stops after 30 seconds or 6 seconds without output and retains the stopped
VM for GDB. It reads the kernel's physical OS-log ring through QMP while the
guest is running, without a warm reset or reliance on old page tables.

The complete path reached native SMSS entry in 13.29 seconds:

```text
ARM short-descriptor NVS tables prepared: root=43b00
ARM N7000 ACPI cache tests: PASS (0x00000000)
SdBusEnumerateCard: CID: MID=AA OID=5859 PNM=QEMU! PSN=DEADBEEF
SffdiskStartDevice: Card type 2, 262144 sectors, Standard, Writable
ARM N7000 clock tests: PASS (0x00000000)
ARM N7000 storage tests: PASS (0x00000000)
NtProcessStartup(00670000) called
```

The kernel enumerated the firmware's EXYN0006 controller through the ACPI HAL
root, used sdbus/sffdisk/partmgr to mount the microSD FAT partition, and
created, wrote, flushed and read a test file. The host separately verified
the persisted 256-byte pattern in the stopped VM's image. This milestone is
native Session Manager entry, not a desktop or a successful N7000 hardware run.
QEMU virt/A15 also passed its native SMSS and 4096-byte storage checks after
the shared loader changes (8.51 seconds).

Reserved EFI RAM descriptors now retain LoaderReserve in FreeLoader's map;
otherwise the newly reserved OS-log area appeared as an undescribed firmware
hole and failed the loader's overlap check. The minimal SYSTEM fixture also
needs Control\\DeviceClasses for ACPI and disk interface registration.

The hardware run preceding these fixes reached the ACPI interpreter and
stopped with STATUS_DATATYPE_MISALIGNMENT: ACPICA read an unaligned NameSeg
from an ACPI table mapped as Device memory. The Exynos HAL now reports normal
and firmware ACPI RAM as MmCached while preserving Device attributes for
controller MMIO and the OS log. ACPICA also reuses the synthesized RSDP
mapping on ARM32. The boot test checks the actual HAL policy and an unaligned
load from the mapped ACPI root. QEMU does not reproduce the Cortex-A9's
Device-memory unaligned fault; a separate processor test records that gap
and verifies Normal-memory access and the SCTLR.A alignment trap.

The periodic clock uses G_COMP0_ADD_INCR at MCT offset 0x208 and G_TCON bits
0/1/8, with their write-status acknowledgements. These definitions agree with
the published Exynos441xPkg MctTimerDxe definitions and the
[QEMU MCT model](https://github.com/qemu/qemu/blob/master/hw/timer/exynos4210_mct.c).
The model restarts its free-running ptimer when comparator registers change
without accumulating the current partial interval; reprogramming on every
kernel tick could therefore move QPC backwards. Auto-increment avoids those
per-tick writes and passed 32 comparator interrupts plus the kernel's clock
progress checks. Physical validation of these latest changes remains pending.

Samsung's warm-reset boot path overwrites parts of the low 16 MiB of RAM.
Do not follow a saved kernel page-table tree after reset: a surviving root
does not prove that its lower tables survived. Recover only the known OS-log
ring. An attempted read through such a stale table stopped the firmware USB
debugger; that state requires a physical reset before another hardware run.

### Firmware touch integration

The firmware fork now includes upstream `Drivers/TouchDxe` and
`Application/TouchTestApp` from commit
`af2d2c53aec9fd52529cf1405c0e1f8449ec63e9`. This provides the mXT540E
touchscreen and Wacom S Pen through EFI Absolute Pointer before
ExitBootServices. It does not provide kernel-mode touch input.

The local non-shareable write-back RAM setting is retained. The upstream
change to shared write-back memory and unrelated CpuFreqDxe were not imported.
The N7000 firmware build includes TouchDxe and exports `Artifacts/TouchTest.efi`;
`Artifacts/build-info.json` now records the selected target, DRAM attributes,
touch-driver inclusion and payload hash on each build.

`bash Tests/test-touch.sh` in Exynos441xPkg passed ASan/UBSan checks of the
production packet decoders and pointer-state code: first finger, coordinate
scaling, pressure, pen hover and priority, release and event consumption.
The SMDKC210 firmware profile excludes phone-only touch hardware and again
reached native SMSS with ACPI, clock and SD-file checks in 13.29 seconds.
A direct run of the N7000 profile on SMDKC210 stopped earlier, waiting for
the absent internal MSHCI eMMC, so it did not exercise touch initialization.
The N7000 firmware was flashed and booted with TouchDxe. It found the mXT540E
(family A1, variant 03, firmware 13/build AA, T9 800x1280) and Wacom digitizer
(12760x8360, pressure 255, firmware 0340). Physical finger/pen input validation
is still pending. The firmware performance application confirmed non-shareable
write-back RAM (`L1=7a501c0e`) at an observed 800 MHz: four 1 MiB copies
21640 us, sixteen serial lines 48621 us, GOP read 19211 us and restore 30451 us.
All service statuses were success; the framebuffer descriptor stayed
`5ec11c02`. The matching FreeLoader/kernel/HAL, ACPI and SD drivers, test
driver, SYSTEM hive and native runtime were written to microSD and verified
by SHA-256 readback.

### Physical RTC, storage and SMSS milestone

The 2026-11-11 GT-N7000 run passed the ACPI cache, native SD/FAT, MCT clock and
RTC tests, completed 17 create/write/flush/reopen/read cycles, created
`C:\pagefile.sys` (119 MiB initial, 1873 MiB maximum) and reported
`SMSS: Boot environment initialized`. The earlier paging-file failure in
IopWriteIoStatusBlock did not recur after NtCreatePagingFile switched from a
guarded mutex to a kernel mutex.

The minimal payload then stopped at `0xC000021A` because it has no `win32k.sys`;
SMSS reported `Load of WIN32K failed` with `0xC0000034`. This is the expected
end of the non-graphical fixture, not a storage or paging-file regression.
An interrupted storage run had left `\arm-n7000-test.bin` with a corrupt
cluster chain (`FatGrowEof: cluster chain size does not match file size`);
recreating the file through the UEFI USB debugger restored the test. This is
not an SD driver failure. The milestone remains SMSS, not GDI or a desktop.

### UEFI charging candidate

The firmware fork now contains a separately documented SMB136/SMB328 charger
and MAX17042 battery-gauge implementation with ADC7 thermal admission,
USB source/configuration checks, register readback, watchdog supervision and
ExitBootServices shutdown. See `../Exynos441xPkg/Docs/N7000Charging.md` for
pinned hardware sources, policy choices and unsupported board variants.
The nonshareable write-back RAM performance fix and touch driver are retained.

Host ASan/UBSan tests passed the production policy, per-operation I2C failures,
open-drain transport, ADC timeout, USB revocation, watchdog ownership and UEFI
shutdown. QEMU SMDKC210 reached paging-file creation and the SMSS boot environment
in 13.19 seconds; QEMU virt passed the equivalent stage in 9.53 seconds.
SMDKC210 excludes the unmodelled N7000 charger hardware.

The N7000 candidate was successfully flashed through Heimdall. ChargeTest.efi
and a startup script launching its 30-second monitor were written to microSD
and verified by SHA-256 readback. No kernel autostart is enabled by that script.
After the flash the user reported that the deeply discharged phone begins UEFI,
the screen whitens, and power cuts out before USB becomes available. No charger
registers, thermal values or charging-current indication were obtained.
Actual charging remains unverified; the battery needs a preliminary charge
outside this UEFI startup before further hardware tests. Do not mark the
candidate hardware-tested from a successful flash alone.

### Cortex-A9 win32k load: paging-I/O stack overflow

Continuing the SMDKC210 guest beyond the SMSS boot-environment checkpoint
reproduced a nested Data Abort during win32k dependency loading. The stopped
CPU had DFSR 0x807, DFAR 0x80120f90 and an SVC stack with limit 0x80121000,
base 0x80124000 and saved KernelStack 0x80121050. Exception entry could not
save its frame below the stack limit, so it repeatedly aborted without a BSOD.

The saved frame chain showed recursive MmLoadSystemImage calls, NtClose,
FAT cleanup, CcUnpinRepinnedBcb, cache writeback, MiSubmitPagingMdl and
IoSynchronousPageWrite. The filesystem/device path then exhausted the remaining
stack during a context switch. This was a stack-capacity failure, not evidence
of a missing SD interrupt or a faulty Cortex-A9 MMU translation.

MiSubmitPagingMdl now submits synchronous paging I/O through a stack-expansion
callout, requesting the architecture's normal kernel-stack size. The event,
IOSB and pending-I/O wait remain inside that callout. Expansion is nonwaiting:
allocation failure returns before dispatch and reports zero transferred bytes,
without waiting for memory while the filesystem/cache may hold resources.
This uses the documented
[KeExpandKernelStackAndCalloutEx contract](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-keexpandkernelstackandcalloutex),
not an increased ARM ABI stack constant or a special case for win32k.

The focused native target is built and run with:

```sh
CCACHE_DISABLE=1 cmake --build output-Clang-arm-debug/host-tests --target ntpaging_host -j8
output-Clang-arm-debug/host-tests/mmcc/nvs/core/ntpaging_host
```

It reuses the production paging-I/O implementation and existing NT paging tests.
The initial test run against the unprotected implementation failed 646 checks;
the final ASan/UBSan run passed 21337 checks. It covers synchronous/pending
read/write completions, completion errors, short transfers, IRQL restoration,
callout lifetime and expansion failure without dispatch. The separate full
host-suite build currently fails on unrelated lxcore host declarations; that
code was not changed to run this focused test.

The SMDKC210 harness now supports `--stage csr`. It additionally requires actual
`CSRSS: Server initialized`, rather than stopping at the earlier SMSS marker.
With the fix, EDK II -> FreeLoader -> LiberNT passed 17 FAT overwrite/flush/
reopen/read cycles, created the 16 MiB paging file, loaded win32k and reached
CSRSS server initialization on Cortex-A9 in 17.35 seconds. This is a QEMU
milestone; no phone CSRSS run or desktop/display milestone is claimed.
Physical validation still requires a charged N7000.
The QEMU virt/A15 paging-file and SMSS boot-environment regression also passed
in 8.82 seconds with the same new kernel. The firmware artifact was restored
to the N7000 profile after the SMDKC210 run; no phone flash was performed for
this kernel change.

### Cortex-A9 graphical startup on SMDKC210

The `startup` test fixture now supplies the WDDM/BasicDisplay drivers, fonts,
Winlogon/LSASS dependencies and complete initial SYSTEM, SOFTWARE, SAM,
SECURITY and DEFAULT hives. It prepares the same fixed test image from scratch
under the existing lock before every boot. Reusing a FAT image after stopping
a guest during startup left a SAM directory entry whose cluster chain was
incomplete; a later run failed with REGISTRY_FILE_FAILURE. The bounded tests
now start with clean filesystem/hive state instead of reusing that interrupted
writeback state. This does not test recovery from power loss.

The original generic-loader FIMD register presets were not retained: reads
showed zero configuration and SimpleFbDxe reported no bootloader framebuffer.
`arm_smdkc210_boot` now builds a small ARM ELF trampoline at 0x40008000. The
CPU programs FIMD window 0 after device reset, then branches to the unchanged
firmware payload at 0x40010000. It sets 800x1280, a 3200-byte stride and the
framebuffer range 0x5ea00000..0x5ede8000. The WINCON word-swap bit preserves
little-endian 32-bit pixel order; without it adjacent pixel pairs were reversed.
The register fields and scanout behavior were checked against the published
[QEMU Exynos4210 FIMD model](https://github.com/qemu/qemu/blob/master/hw/display/exynos4210_fimd.c).
No QEMU model code was copied into the trampoline. The tested emulator is
QEMU 11.1.3.

EDK II now discovers that buffer and publishes GOP. FreeLoader hands it to
the kernel, and WDDM acquires actual POST display ownership at 0x5ea00000.
The SMDKC210 firmware profile also uses zero Shell startup delay; the phone
profile retains its existing delay and drivers. The N7000 RAM cache-performance
fix is unchanged.

The completed run reached CSRSS, Winlogon and LSASS, followed by a visible
800x1280 LiberNT lock screen, in 27.07 seconds. The harness verifies both shared
clock counters advance during three seconds after Winlogon initialization,
checks GOP/scanout markers and saves `output-Clang-arm-debug/arm-screen.png`.
It remains headless with a 30-second limit and six-second stall bound. QEMU
virt's paging-file/SMSS regression passed in 8.31 seconds after the fixture
helper change; its working `LiberNT.img` and `QEMU_VARS.fd` were not modified.

To reproduce from the configured ARM workspace:

```sh
(cd ../Exynos441xPkg && PATH="$PWD/workspace/host-tools/usr/bin:$PATH" CCACHE_DISABLE=1 ./build.sh --smdkc210)
CCACHE_DISABLE=1 python3 -B scripts/test-arm-smdkc210.py --stage startup --firmware ../Exynos441xPkg/Artifacts/bootpayload.bin
```

The harness builds the trampoline and required kernel/session components,
prepares the fixed test image and pauses QEMU after verification. `--no-build`
reuses those binaries. The screenshot shows the actual FIMD display; it is not
an inferred graphical milestone from the CSRSS log alone.

The `--stage input` fixture now describes the emulated coherent EHCI controller
in ACPI and attaches a USB keyboard and tablet. Both enumerate and deliver input;
cursor movement and keyboard dismissal of the lock screen passed in 28.43 seconds.
The ARM HAL supplies a DMA adapter for coherent, identity-mapped 32-bit bus masters.
The IRQ-state helper now returns whether IRQs were enabled before masking them;
the EFI regression checks enabled, disabled and nested save/restore operations.

The `--stage login` fixture provisions `tester` with password `armv7` and enables
automatic logon by default. `--manual-login` exercises USB credential entry.
It includes the Aero theme, wallpaper, UI fonts and ARM Common Controls assemblies.
The ARM fault path grows a large kernel stack before page-fault resolution when
less than `KERNEL_STACK_SIZE` remains. This fixed the captured stack overflow in
scaled-wallpaper painting followed by a page fault and clock IRQ. EFI checks cover
the growth conditions and allocation-failure propagation.

The final SMDKC210 run passed boot in 27.73 seconds, automatic logon/profile/desktop
checks in a separate 26.42-second phase, and Start-menu/Explorer checks in 3.48
seconds. Each phase is bounded to 30 seconds and checks clock progress after six
seconds without log output. The observer verifies visible, responsive desktop,
taskbar, `StartMenu2` and `CabinetWClass` windows; screenshots also verify painting.
The final screenshot shows the Explorer window and taskbar; its file-list contents
are not part of the assertion.
Logs still report COM proxy and drag-drop registration errors; drag/drop was not
tested. The fixture remains separate from the full installed QEMU virt image.
QEMU virt was rerun after the latest IRQ/fault-stack changes: ABI passed in 5.91
seconds, loader in 12.22 seconds, SMSS with a 16 MiB paging file in 8.31 seconds,
and modern-VirtIO `csr --login` in 30.14 seconds with automatic logon, profile and
Explorer desktop checks. The SMDKC210 VM was stopped afterward. No phone GUI test
or flash was performed during this verification.

### Explorer desktop on N7000

The 2026-11-12 hardware run reached a visible Explorer desktop and taskbar on
the GT-N7000. Winlogon initialized the session, loaded the Administrator
profile, started `userinit.exe`, and `explorer.exe` created the shell. The
desktop milestone follows the earlier BasicDisplay scanout and does not use a
flashed firmware image.

Two user-mode failures had to be removed to reach that state. The card lacked
`ntmarta.dll`, so `advapi32!SetUserHiveSecurity` returned error 126 and
`userenv!CreateUserProfileW` could not create the profile. It also lacked
`userinit.exe` and `explorer.exe`; `explorer.exe` belongs in
`\Windows\explorer.exe`, not in `System32`. The payload now also carries
`lpk.dll`, `shsvcs.dll`, `rpcss.exe`, `actxprxy.dll`, `stobject.dll`,
`mmdevapi.dll`, and the UI fonts used by the shell. Remaining shell logs report
missing activation contexts for `Microsoft.Windows.Common-Controls (6.0.0.0)`,
the absent Aero style, COM proxy registration, and network provider
enumeration; none of these prevents the desktop from being displayed.

The hardware runs exposed a separate NVS defect: `MiCreatePebOrTeb` allocated
PEB and TEB pages from the randomized bottom-up region. When the random base
fell inside the mapped image, the following `RtlZeroMemory` wrote into a
read-only image page, and the nested fault became a `0x1E` bugcheck. PEB and
TEB allocations now pass `MI_MEM_TOP_DOWN`, matching the high user address
where Windows places these structures. The fix is in
`ntoskrnl/nvs/nt/ntprocess.c` and affects every process, not only ARM.

QEMU regressions after the change: `SMDKC210 --stage login` passed in 28.47
seconds with the Explorer desktop, and `UEFI --stage user` passed in 8.31
seconds. Both ran headless with the existing fixed images.

### mXT540E touchscreen

`drivers/hid/mxt540e` is an ARM32 HID minidriver for the GT-N7000 Atmel
mXT540E. It owns the controller directly: I2C3 at address `0x4c`, SDA/SCL on
GPA1[2:3], the LDO enable on GPL0[3], and the active-low CHG line on
GPX0[4]. The controller is powered and reset through the LDO, probed through
its info block and object table, and its T5 message queue is drained by a
10 ms polling thread. T9 multi-touch messages become a standard HID touch
report with up to ten contacts; the report descriptor exposes the 800x1280
panel range.

The current firmware DSDT exposes only `EXYN0001` and `EXYN0006`; it does not
describe the touchscreen's I2C, GPIO and interrupt resources. The driver INF
already matches `ACPI\EXYN0004` for the planned ACPI node, but the bring-up
path uses `ROOT\LIBERNTVIRTMXT540E` and `mxt540e_reg.inf`. That registry file
is opt-in through `-DLIBERNT_N7000_TOUCH=ON` or an equivalent N7000 image
install, so other ARM targets do not touch Exynos touch registers.

The SMDKC210 login fixture now includes the root device as a smoke test. On
that machine I2C3 has no mXT540E, so the driver logs
`mxt540e: info read failed 0xc0000185` (STATUS_IO_DEVICE_ERROR, NACK) and
fails the device without affecting the desktop. The login test passed in
28.44 seconds with that probe in the log.

For the phone test, mount the card and run `scripts/n7000-install-mxt540e.py`
with a `reged` binary from chntpw. It copies `mxt540e.sys`, `hidclass.sys`
and `hidparse.sys`, backs up the SYSTEM hive and its logs, and imports the
root device entries. Success is visible in the OS log as the `mxt540e:
family ...`, `mxt540e: T5 ...` and `mxt540e: started` lines. If the probe
returns `0xc0000185`, check the LDO, the I2C3 pin mux and the controller's
power state. The driver relies on the controller NVM configuration that Linux
and the firmware TouchDxe leave behind; it does not yet write a T9
configuration blob.

### SMBIOS processor identity

Exynos441xPkg publishes SMBIOS 3.8 Type 1 and Type 4 records. Type 1 reports
`Samsung Electronics` / `GT-N7000`; Type 4 reports `Samsung` /
`Samsung Exynos 4210` for the two Cortex-A9 cores. The ARM32 configuration
manager prefers Type 4's manufacturer/version over MIDR and keeps the MIDR
fallback for firmware without SMBIOS.

`SMDKC210 --stage session` passed in 12.2 seconds with
`ARM N7000 processor name tests: PASS`; the same run installed a 227-byte
SMBIOS 3.8 table. QEMU `virt` continues to pass its storage fixture with the
firmware-provided `virt-11.1` name.

### SMP parking and ARM32 processor startup

`Exynos441xPkg/Drivers/MpParkDxe` brings the second Cortex-A9 out of reset,
parks it through the ACPI parking protocol, and installs a runtime MADT with
the per-core GIC bases. The driver uses `PMU INFORM5` on Exynos4210 revision
1.1 and the SYSRAM boot vector on earlier revisions.

LiberNT ARM32 builds its kernel as SMP. `KeStartAllProcessors` learns the AP
mailbox from MADT, wakes it through an identity-mapped trampoline, and gives
the AP its own PCR, GIC CPU interface and kernel stacks. SGI 0, 1 and 2 carry
DPC, APC and IPI work. FreeLoader now maps the second Exynos GIC interface and
the SCU, and uses shareable normal-memory attributes for the SMP kernel.

The SMDKC210 harness has an explicit `--smp` mode. It starts CPU1 in a
stackless reset shim that waits for the firmware's Exynos4210 revision 1.1
PMU INFORM5 boot vector. MpParkDxe then parks it normally; the firmware
publishes two GICC entries and LiberNT starts the AP through its mailbox.
Without `--smp`, the original single-processor test remains available.

The 2026-11-13 two-processor session test reached `CPU1 online`, passed all
17 SD create/write/flush/reopen/read cycles, created the paging file and
reached `SMSS: Boot environment initialized` in 10.28 seconds. This is a
QEMU result. The same binaries have now been installed on the phone.
Hardware validation is described below; QEMU results do not establish
physical SD or display behavior.

After fixing the dispatch race and the unclaimed-interrupt bugcheck, the
two-processor `--stage login --smp` run passed in 24.78 seconds. It verified
the user's profile, Explorer desktop and taskbar, then checked that an
Explorer window remained responsive. The result file records `processors: 2`.

SMP bring-up exposed several independent defects: the HAL was compiled
without `CONFIG_SMP`, AP initialization repeated global HAL cache and timer
initialization, IPI mask iteration skipped every second destination, and
the dispatcher consumed `NextThread` without rechecking it after acquiring
the PRCB lock. The last race could publish a null current thread and recurse
through the data-abort handler. The ARM dispatcher now also consumes a stale
self-selection without switching stacks. An ISR declining an interrupt no
longer causes an unconditional kernel bugcheck.

Exynos4210 needs the `0x8000` processor stride for both the GIC CPU interface
and the private distributor registers. FreeLoader maps CPU1's distributor
window, and the HAL uses it for SGI initialization, generation and clearing.
The stride comes from Samsung's published Linux device-tree description and
the Linux GIC driver's non-banked register handling; no Linux code was copied.
QEMU's Exynos GIC aliases both distributor windows onto its normal banked
model, so a QEMU boot alone does not validate the physical window selection.
The binary regression checks record the actual HAL MMIO addresses instead.

Evidence for the processor mechanics and interrupt contracts:

- [Cortex-A9 TRM, ARM DDI 0388C, section 4.2.1](https://documentation-service.arm.com/static/5e8e2743fd977155116a5ea9): cache ways occupy bits 31:30. The AP trampoline must use `CLZ(ways - 1)` as the way shift, without subtracting it from 32.
- [GIC architecture specification, ARM IHI 0048B, section 4.3.16](https://documentation-service.arm.com/static/5f8ff21df86e16515cdbfafe): `CPENDSGIR` uses one byte per SGI and exists only in GICv2. GICv1 acknowledges SGIs through IAR and must not write these reserved locations.
- [Samsung Exynos4210 device tree](https://code.googlesource.com/linux/torvalds/linux/+/3c8f504b3a486e4e984ac8dc619eba3afa24cec4/arch/arm/boot/dts/exynos4210.dtsi) and [Linux GIC initialization](https://kernel.googlesource.com/pub/scm/linux/kernel/git/torvalds/linux/+/refs/heads/master/drivers/irqchip/irq-gic.c): the processor offset applies to both register windows.
- [Microsoft ISR contract](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/writing-an-isr): an ISR returns `FALSE` when the interrupt is not from its device.

`scripts/test-arm-ap-cache.py` executes the built AP invalidation loop for
five cache geometries and checks every set/way selector. `scripts/test-arm-gic.py`
executes the built HAL and kernel to check per-processor SGI addresses,
GICv1/v2 clearing, eight IPI destination masks, contended spinlocks, AP
initialization, an unclaimed IRQ and withdrawal/self-selection of NextThread
while acquiring the PRCB lock. These tests require Python Unicorn and
`llvm-nm`. PCR/PRCB/thread locations use the build's generated assembler
offsets; the emulator's current-IRQL byte follows the compiled ARM consumer
at PCR+0x1c. They test LiberNT processor mechanics, not Windows ABI parity.

The physical Cortex-A9 also requires ACTLR.FW (bit 0) alongside ACTLR.SMP
(bit 6) to broadcast cache and TLB maintenance. The Exynos HAL enables both,
and the AP trampoline inherits the BSP ACTLR rather than preserving the
firmware's per-core value. The source is the
[Arm Cortex-A9 maintenance-broadcasting specification](https://support.arm.com/documentation/ddi0407/h/Introduction/MPCore-considerations/Maintenance-operations-broadcasting).
The binary regressions check both the Exynos HAL setting and AP inheritance.

On the N7000, both processors reported ACTLR `0x41` and SCTLR `0x60c5187d`.
A boot-driver regression pinned its producer to CPU0 and consumer to CPU1,
verified 256 bidirectional shared-memory exchanges, and checked that
`KeIpiGenericCall` executed on both processors (mask `3`). These tests,
processor naming, ACPI caching, the clock and RTC passed on hardware.
Physical SD create/write testing subsequently passed, as recorded below.
Earlier timer-IRQ samples resolved to the SD PIO loop and
`FatExamineFatEntries`; those samples alone did not prove a deadlock. The diagnostic harness arms
its watchdog before FreeLoader; the boot test now refreshes that existing
watchdog for at most 20 seconds, only while its control and reload registers
match the harness configuration. It never enables a watchdog itself.

A diagnostic hardware run with a temporary high-priority CPU1 monitor passed
all 17 SD overwrite cycles at 5.71 seconds, initialized the SMSS boot
environment at 8.42 seconds and reported `WINLOGON: Session initialized`
at 21.08 seconds. This does not validate normal SMP scheduling or an
interactive desktop; the monitor was removed before the next hardware run.

The non-banked GIC exposes a migration race in `HalpSetIrql`: capturing the
processor number before disabling interrupts can select the old processor's
MMIO window if the thread migrates in between. The HAL now reads the
processor number after `CPSID I`. The binary regression injects migration
at that instruction and checks the PMR write in both directions; it fails
with the old ordering and passes with the corrected one.

The GIC acknowledgement token must survive nested IRQs. The HAL previously
kept one token per processor, so an inner acknowledge overwrote the outer
token and the outer EOI repeated the inner token. It now keeps a bounded
per-processor stack, preserves the complete IAR value including the SGI
source CPU, ignores spurious acknowledgements, and protects EOI stack
updates against IRQ entry. The source is
[Arm GIC architecture, IHI 0048B, sections 3.2 and 4.4.5](https://documentation-service.arm.com/static/5f8ff21df86e16515cdbfafe).
The binary test fails with the old ordering and checks nested tokens,
a spurious acknowledge between them, and preservation of the caller IRQ mask.

After a cold return to UEFI, the physical two-processor boot passed all
17 SD cycles at 6.12 seconds, initialized SMSS at 7.67 seconds and Winlogon
at 16.01 seconds. The maintainer saw the desktop on the phone. Both cores
continued taking interrupts until the diagnostic watchdog reset at about
42 seconds; RESET_STAT was `0x00100000`, confirming that scheduled reset.
That run still contained temporary tracing, which has since been removed.
Earlier intermittent stops are not yet established as resolved by the two
HAL corrections; ordinary boots without tracing require further observation.
A normal startup does not arm the diagnostic watchdog.

The maintainer then reproduced early boot-animation stalls in four ordinary
boots without tracing. These remain the active hardware defect. A later
RAM probe found both processors past exception return and back in idle,
with the last clock observations at interrupt time 5.72 seconds. Moving
IRQ unmasking after WFI did not fix the defect and was reverted. The current
investigation records the GIC running priority and the MCT comparator
state; the temporary probes are not a fix.

Subsequent watchdog runs still stopped during startup, with the last clock
observations between approximately 2.3 and 9.7 seconds. The physical MIDR
was `0x412fc091` (Cortex-A9 r2p1). Idle-call sequence counters distinguished
a new idle invocation from an earlier completed WFI: CPU0 entered the next
HAL idle call, then left its read probe at the instruction preceding a
GICC_CTLR load from `0xfff20000`. The final probe placed its markers and
load in one assembly block, without intervening stack accesses. The saved
page tables mapped that address to `0x10480000`, with descriptor
`0x10480417`. This identifies the last observed progress, not a confirmed
cause or correction of the hardware stall.

Some watchdog restarts also leave the microSD absent from UEFI: firmware
reports command CRC errors before publishing its BlockIo. A battery power
cycle restores enumeration. The hardware harness now recovers its RAM log
without requiring the microSD to reappear, using reads of at most 1024
bytes. A mocked recovery run verified log preservation with no SD volume.
A separate `SdRecoveryApp` in the companion Exynos441xPkg passed host tests
and a physical run: LDO17 changed from `E8` to `28`, and UEFI enumerated the
card after its warm restart. The firmware SD initialization now isolates
the pins and power-cycles LDO17 before identification. Its regression fails
on the old implementation and passes on the new one, including I2C errors.
With the maintainer's approval, the native N7000 image with SHA-256
`0097064ec0e114221103ecdbed704da1ca112fa411b514696732098f4781e5fe`
was flashed to KERNEL. Subsequent OS watchdog restarts retained the SD
volume without battery removal. An attempted internal-FAT upload stalled
the USB command channel; that separate failure remains unexplained.

The physical Diagnostic Control Register initially read `0x00010000`.
Per-processor HAL initialization now applies Cortex-A9 controls from
[Arm UAN 0007D](https://documentation-service.arm.com/static/608117c55e70d934bc69f136):
bits 4 and 11 for the affected revisions (742230 and 751472), and bit 22
for 845369. It preserves other bits and checks readback before enabling
the CPU interrupt interface. Binary regressions cover 13 MIDRs, existing
controls and denied writes. Both physical processors reported `00410810`.
These corrections do not establish that the original stall is fixed.

One traced run continued taking timer interrupts on both CPUs until its
scheduled reset at about 42 seconds, but LSASS encountered recursive
vectored-handler-lock errors and an access violation. NTDLL on the card
matched the local build by SHA-256. After removing IRQ/idle/exception-return
probes, early SD-test stalls recurred. A sparse watchdog-worker probe last
advanced at interrupt time 2.6 seconds. First-chance ARM user-exception
logging is temporary; the hardware startup defect remains active. The same
kernel reached an interactive desktop in QEMU SMP after the bounded initial
startup phase timed out and was inspected and continued; separate login,
Start-menu and Explorer checks passed.

The ARM C headers still defined `SYNCH_LEVEL` as 2 in an SMP build, while
the existing ARM assembler-generation contract in `ksx.template.h` defines
it as `IPI_LEVEL - 2` (12). The HAL raise-to-synch function and both
spinlock acquisition wrappers also used the dispatch-level implementation.
The ARM NDK definition now follows the existing SMP assembler contract,
the duplicate internal definition is removed, and all three HAL entry
points raise to that level. The binary regression fails on the former
HAL and checks the actual IRQL, GIC PMR and restoration after release on
both Exynos CPU windows and the shared virt window. QEMU SMP reached SMSS
in 14.24 seconds. The initial login phase exceeded its 30-second limit;
bounded follow-up runs reached the verified Explorer desktop and passed
Start-menu and Explorer responsiveness checks. The physical run with
kernel SHA-256 prefix `1b820575ba9ae24c` and HAL prefix `ef70f0825a2c7ad1`
still stopped in the first storage pass, after the 2.03-second clock and
RTC checks. The watchdog worker last advanced at about 2.61 seconds on
CPU0, and reset status confirmed the watchdog reset. Thus the SYNCH_LEVEL
correction does not fix this startup stall. This is consistency with
an existing ARM consumer, not evidence of Windows 11 ARM32 ABI parity.

A subsequent boot with storage-stage RAM markers stopped inside the first
`ZwCreateFile`, before any `ZwWriteFile`: stage 1, status `STATUS_PENDING`,
CPU1, IRQL 0, interrupt time about 2.03 seconds. Post-reset kernel static
data and the bootstrap root were readable, but the AP PCR mapping was not
recoverable from the saved table contents. That prevents a reliable stack
reconstruction from those contents alone; it is not proof of a live mapping
fault. Temporary IRQ snapshots capture exception registers and bounded
kernel-stack samples directly in the uncached diagnostic region.

IRQ snapshots showed thousands of vector-107 interrupts on CPU0, with both
CPUs completing their last C interrupt handlers near three seconds. This
does not establish that SD interrupt handling caused the stop. A subsequent
register probe read FPSID `41033092` on both CPUs (FPU revision 2).
Arm UAN 0007D, erratum 754320, identifies speculative VFP multiple loads
longer than eight 64-bit beats as a deadlock risk on these revisions; its
hardware ECO is identified by FPU revision 4. ARM exception return, context
switch and thread startup used 16-register loads. These restores are split
into eight-register loads, retaining the frame layout and register values.
The binary regression fails on the old load length and executes all three
restore paths to check all 32 D registers and source-pointer updates.
QEMU SMP reached SMSS in 10.38 seconds. The first physical run with kernel
SHA-256 prefix `3090b78c8517c163` passed all 17 storage cycles and reached
the SMSS boot environment at 8.01 seconds, but both CPUs stopped recording
interrupts around 8.47 seconds. This progress does not establish that the
startup hang is fixed. Temporary exception-exit markers distinguish the
C-handler return from completion of VFP restoration.

The next traced run still stopped in the first storage pass, near 5.52
seconds. Both CPUs reached the marker following the VFP loads; the last
CPU1 sample was in `READ_REGISTER_ULONG` called by the SDHCI PIO FIFO read
at `request.c:1574`. CPU0 repeatedly returned FALSE for vector 107. These
are last observations, not a stopped-CPU backtrace.

The SDHCI wait previously treated a latched buffer-ready interrupt as
permission to access the FIFO. A binary regression injects an old ISR
notification during command setup and catches a FIFO access with the
current buffer-enable bit clear. The wait now uses Present State bits 11
and 10 for PIO read/write readiness, while checking accumulated and live
error status. The register behavior comes from SD Association Part A2
v2.00, sections 1.7.1 and 2.2.9
([published specification](https://ercankoclar.com/wp-content/uploads/2017/11/Simplified_SD_Host_Controller_Spec.pdf)).
Read/write regressions cover stale and coalesced notifications, timeout
and errors. QEMU SMP reached SMSS in 10.58 seconds; physical validation of
the SDHCI change did not remove the SMP stall: the next run stopped near
2.46 seconds in the first storage pass.

A controlled `NUMPROC=1` run used the same kernel, HAL and SDHCI binaries
as that failed SMP run. CPU1 never came online; CPU0 continued taking clock
interrupts through 42.54 seconds, and the watchdog worker completed all
20 refreshes. All 17 storage cycles passed. Wininit nevertheless entered
recursive NTDLL exceptions around 11.21 seconds and terminated around
12.07 seconds. Thus the early total stall depends on running the second
core in these runs, while the user-mode startup defect also occurs with
only one active core. The normal two-core configuration was restored after
collecting the one-core log.

The next cache candidate targets short-descriptor publication under SMP.
HAL sets SCU Diagnostic Control bit 0 for Cortex-A9; the register is RAZ,
so this setting cannot be verified by readback. Normal and boot PTE
updates issue DSB/clean twice on Cortex-A9 to account for cache-line false
sharing, per UAN 0007D erratum 764369. Other CPUs retain one clean. The
binary regression catches the old missing pre-clean barrier and checks
both update paths on four MIDRs. This only addresses these PTE publication
paths; other MVA cache-maintenance paths still require review. QEMU SMP
reached SMSS in 10.58 seconds, but the physical run with kernel prefix
`af64195716c33795` and HAL prefix `024060da7c7b0ea3` again stopped in the
first storage pass after the 2.05-second checks. The candidate was reverted
because it did not correct the stall; full coverage of 764369 remains a
documented gap. NVS I/O mappings already request `MI_LEAF_DEVICE` for
`MiCacheNone`, so the source does not support assuming that SD MMIO is
mapped as ordinary noncached RAM. Low physical page tables can be overwritten
by firmware startup; their post-reset contents alone cannot prove live
mapping attributes.

A temporary watchdog worker now waits on QPC/MCT instead of a kernel
delay and samples the MCT comparator state. This changes the workload and
is not a correction. With kernel prefix `9bbd53f58846234c`, HAL prefix
`4c2f59ebedffef23` and test-driver prefix `1e26f6a593662e3d`, the SMP boot
passed storage and initialized CSRSS. It then failed creating wininit:
`MmCreateTeb` faulted in `memset` at VA `7ffc0000`, FSR `00000808`, and
`PspCreateThread` returned `c0000005`, leading to STOP `c000021a`.
The worker's last sample covered 11.00 seconds of MCT time and 10.01
seconds of kernel interrupt time; the comparator remained enabled with a
1-ms period and a future deadline. This run does not prove that a stopped
system clock caused earlier stalls. It provides a concrete abort to trace.
NTDLL on the phone still hashes to `f915b78e7822b76f2b545534b0ef90fed2b99c27d03f37ec816aaf740eb34280`;
local rebuilds have since changed the DLL and its symbols, so current
local symbols must not be used to assign source lines to that older DLL.

CP14 discovery on the phone returned DBGDIDR `35137021`, DBGDRAR
`1088F003`, DBGDSAR `00030003` and DBGDSCR `03000002`. An attempted CPU
memory read at the indicated debug address did not complete and stalled
the UEFI USB command channel. The firmware memory map covers this address
with its identity-mapped Device region from zero through `0x3fffffff`.
Availability of that external debug block
in the CPU physical address space, including its clocks and power, remains
`INFO_NEEDED`; do not repeat the access without that evidence.

A subsequent SMP run reported FSR `00000406`, whose short-descriptor status
is `0x16` (asynchronous external abort). Its saved PC and DFAR do not identify
the originating bus access. TTBR0 and the current process root both read
`47410000`; an ATS1CPR query of the saved DFAR returned PAR `0000000b`.

AP initialization also replaced the idle process's active-processor mask.
The binary regression starts with CPU0's bit set and catches the old result
`2`; an atomic OR preserves both bits and produces `3`. QEMU SMP reached
SMSS in 10.58 seconds. Physical kernel prefix `cfbcde51657a6ae1` passed all
17 storage cycles and initialized Winlogon with the busy watchdog worker,
then stopped around 22.61 seconds. Restoring the worker's one-second kernel
delay, with the same kernel and HAL, restored the early stall around 2.21
seconds in the first storage pass. The mask fix therefore does not resolve
the physical SMP hang; the busy worker only changes its manifestation.

The physical cross-core instruction test exposed a separate cache defect:
CPU0 alternates two assembled Thumb functions in executable pool, flushes
the range, and publishes it; CPU1 executes ISB before calling the function.
With local ICIALLU, round 2 returned the previous value `35` instead of
`6a`. KeInvalidateRangeAllCaches now issues ICIALLUIS; physical kernel
prefix `e19ce956774769cb` passed all 256 updates with test-driver prefix
`1b2be5d54fbd1af3`. This matches the API's
[all-processor cache contract](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-keinvalidaterangeallcaches)
and the Arm inner-shareable instruction-cache operation. The receiving
processor's ISB follows Arm's
[Barrier Litmus Tests and Cookbook, section 9.2.2](https://documentation-service.arm.com/static/5ece8d6a56a54774bd17a1f4).
The ordinary-delay watchdog run still stopped in the first storage pass;
this cache correction does not establish a fix for that hang.
QEMU SMP reached SMSS in 10.38 seconds with the correction; its cache model
does not replace the physical cross-core regression.

Independent MCT timestamps in the IRQ snapshots also stopped advancing near
the last interrupt-time samples. The last observed post-EOI state had GIC
running priority `ff`, token-stack depth zero, MCT control `103`, interrupt
enable `1`, and a future comparator deadline. These observations do not
identify the instruction at which either core eventually stopped. The
final exception-return PC/CPSR matched the entry values and had IRQ enabled.
Replacing WFI with NOP still stopped in the first storage pass (around
3.31 seconds); restoring WFI and unmasking asynchronous aborts in idle
also failed to reveal an earlier abort. Both experiments were reverted.
PL310 control read zero in UEFI, and SCU control read `1`.

A separate 742231 candidate enabled Diagnostic Control bit 12 on r2p0-r2p2.
The binary revision/readback regression failed before the change and passed
after it; both physical cores reported `00411810`. The first storage pass
still stopped. The candidate and its changed test expectations were reverted;
the next run confirmed the previous `00410810` controls again.

The board test temporarily redirects SD SPI 107 from CPU0 to CPU1 before
its storage test. With both cores active and the ordinary sleeping watchdog
worker, three successive runs passed all 17 storage cycles (5.85, 5.96 and
6.12 seconds). This is a diagnostic routing change, not a production fix.
Later failures varied: STOP `9` with a queued-lock address and IRQL `1`;
wininit failure during kernelbase locale initialization; and CSRSS prefetch
external abort `FSR=8`, IFAR `10030124`, at the first instruction of
RtlSetUnhandledExceptionFilter. Current local `.text` and `.symz` sections
match the phone's kernelbase and ntdll despite different full-image hashes.
The phone's locale.nls is byte-identical to `media/nls/locale.nls`
(794914 bytes, SHA-256
`868e5d3a3f115c2649706f566f9741aa3e2c6927c10b3f07d5e4c76b0e403115`).
The user abort trace now records raw hardware translation state to distinguish
a bad user pointer from a bus error or incorrect mapping. An ARM control-state
probe also preserves up to 8 KB of the current kernel stack in the reserved
log RAM before a crash, avoiding the overwritten page-table problem after reset.

One later routed-IRQ run initialized Winlogon and continued through 39 seconds,
then crashed returning to PC zero from a user callback. Its preserved stack
exposed a defect in the temporary exception-exit probe: calling
KiArmMarkExceptionExit after constructing the return frame lets a callee's
stack overlap the frame below the callback's current SP. The probe call and
helper were removed completely. `test-arm-exception-exit.py` executes the built
return path with the callback stack placement and ABI-permitted callee stack
use. Restoring the old call placement reproduces corrupted r0; the corrected
path preserves all integer registers and the PC/CPSR pair. This invalidates
attributing that callback crash to the operating-system path itself; it does
not establish the cause of the earlier pre-user-mode storage stall.

A 743622 candidate then enabled Diagnostic Control bit 6 on the affected r2
cores. The binary regression passed and both cores reported `00410850`, but
the first storage pass still stalled. The candidate and expectations were
reverted; the hardware returned to `00410810`.

With the exception-exit probe removed and SD SPI 107 routed normally to CPU0,
the early storage stall remained. Four isolated tests now run before the SD
driver starts: 1024 cross-core event exchanges, 1024 synthetic SPI 107/ISR/DPC
exchanges, the same IRQ exchanges with an MDL alias mapped and unmapped every
round, and the same exchanges with concurrent reads of SD controller registers.
The MMIO test also writes zero to the W1C interrupt-status register; it issues
no card commands and does not access the FIFO. All four passed physically;
the MMIO test completed at 2.08 seconds, while the subsequent first file cycle
stalled with the last IRQ samples near 2.84 seconds. These bounded tests do
not establish that every scheduler or mapping path is correct.

Serializing each PIO FIFO block with the existing interrupt spin lock did not
fix the stall: the first file cycle remained incomplete, and the final IRQ
samples were near 5.74 seconds. The candidate was removed. The binary PIO
regression now exercises both polled and interrupt-connected execution for
read/write, stale/coalesced readiness, timeout and controller-error cases.
QEMU SMP reached SMSS with the candidate in 11.79 seconds; this does not
establish physical correctness.

An idle-path diagnostic replaced WFI with continued polling and recorded
per-core idle-call counts, MCT time, GIC PMR/RPR/HPPIR and comparator state.
It still stalled in the first file cycle: both idle records stopped near the
final IRQ time of 4.17 seconds. CPU0 had more than 170000 idle calls and CPU1
more than 220000; both last records had RPR `ff`, MCT control `103`, interrupt
enable `1`, status `0` and a future deadline. No core continued updating its
idle heartbeat after interrupt time stopped. This does not locate a stopped
instruction or prove a hardware deadlock. The polling/idle diagnostic was
removed before the next SD interrupt-signal experiment.

Disabling only SDHCI interrupt signaling (register 0x38) for the file test,
with both processors and the clock active, passed all 17 cycles at 6.17
seconds. Restoring the previous `07ff00fb` signal mask was followed by a stall
near 6.88 seconds during subsequent boot I/O. Keeping SD signaling disabled
for the rest of the next boot passed storage at 6.11 seconds, initialized
CSRSS and Winlogon, and continued shell activity through 41.22 seconds.
Both CPUs still recorded interrupts at 42.14 seconds. RESET_STAT was
`00100000`, the diagnostic watchdog reset, rather than a recorded bugcheck.
The maintainer observed startup followed by that reset. This experiment
does not establish correct interrupt-driven SD operation.

A separate command-reset race was reproduced in the built SD driver: an ISR
between software-status reset and hardware-status clear can leave the old
command's completion in CommandInterruptStatus. The model catches a response
read before the new command completes. Clearing hardware and accumulated
status together under the interrupt spin lock, after clearing CommandEvent,
passes that interleaving. The same race was then reproduced for both enum.c
command helpers; all three now use the synchronized reset helper. The
regression covers 20 binary execution cases. The first physical test with
only the main request path corrected still stalled near 2.47 seconds, so
that race correction alone does not explain the SMP boot stall.

With the synchronized reset applied to all three command paths, a physical
boot with the normal SD signal mask still stalled in the first file cycle.
The reset-race regression was strengthened to delay the ISR's accumulation
until the worker tries to acquire the ISR lock, or until the new command is
issued if no synchronization occurs. Temporarily restoring the original
reset sequences again fails with premature response access; restoring the
synchronized helper passes all 20 cases. This is a separately reproduced
driver race, not a demonstrated correction of the physical system stall.

With only FIFO-ready signaling disabled (`07ff00cb`), storage passed at 6.10
seconds, but creating the first user thread failed with a synchronous external
abort: VA `7ffc0000`, FSR `808`, TTBR `47500000`, PAR `e00d9090`. The physical
address encoded by PAR is outside the board RAM. A later run with the same
signal mask stalled near 19.35 seconds without an abort. Disabling only these
signals therefore changes the failure; it does not establish a fix.

A deterministic pre-storage page-walk visibility test now prepares a scratch
root with the current TTBR attributes. Its child table contains a valid poison
mapping cleaned to RAM, followed by zeros written through the cached alias.
It queries ATS1CPR without accessing the translated address, then restores the
original root. Physical CPU0 read cached PTE `00000000`, yet PAR was `e00000d4`
with TTBR `44460000`: the hardware walker saw the previous RAM contents.
QEMU returned a translation fault (`PAR=f`) for the same test, so its cache
model does not reproduce this defect.

Short-descriptor table walks now use Inner Shareable
Normal Write-Back Write-Allocate attributes in both boot-root installation and
process switches. AP startup inherits the boot TTBR. The field encodings come
from [Arm's TTBR register description](https://arm-software.github.io/CMSIS_6/latest/Core_A/group__CMSIS__TTBR.html),
and [Cortex-A9 TRM, section 6.3](https://documentation-service.arm.com/static/5f0370fccafe527e86f5bfb2)
explains that noncacheable walks access external memory, while write-back
walks look up the L1 data cache. A native ASan/UBSan model of local and remote
cached table writes fails with the previous root-only value and passes with
the matching attributes. Both physical cores passed with TTBR `4446004a`
and PAR `f`; Cortex-A9 reads TTBR.NOS as zero. With normal SD signaling the
same boot still stalled in its first storage pass near 2.64 seconds. The
page-walk correction fixes a separately reproduced cache-visibility defect,
but does not resolve the original SD IRQ stall.

The polled milestone filters every `SDHCI_INT_SIGNAL_ENABLE` write through
the hardware backend. Exynos returns zero, while other backends preserve the
requested value. Interrupt-status reporting remains enabled for polling.
The binary SD regression checks this policy for Exynos, a generic backend
and an absent hardware extension, including writes to status enable.
Temporary IRQ, exception-return, user-fault and watchdog stack snapshots
have been removed from the kernel. The permanent page-walk, cross-core
instruction/data, clock, RTC and storage regressions remain in the board
test driver. When the host explicitly arms the diagnostic watchdog, its
worker checks both processors with `KeIpiGenericCall` after 120 seconds,
then allows the watchdog to reset the phone for log recovery. Normal boot
does not arm this watchdog.

The cleaned polled build passed the physical test on 2026-11-15: kernel
SHA-256 prefix `8b9ec306b1a34c65`, HAL `554a1d3d2dc37844`, SD bus
`0256b9ae47868d82`, board test `a6ce21e8e8d10985`. Both page-walk tests and
all 256 cross-core instruction updates passed. All 17 file cycles passed at
5.22 seconds, CSRSS initialized at 9.51 seconds and Winlogon at 14.43 seconds.
The sustained check passed with CPU mask `3` at 121.75 seconds. No STOP or
recorded kernel abort occurred; RESET_STAT `00100000` confirms the scheduled
diagnostic watchdog reset. QEMU SMDKC210 SMP passed the matching session
fixture in 10.18 seconds. The disabled Exynos signal policy is an explicitly
requested interim baseline; interrupt-driven operation remains unfinished.

This is a two-processor boot milestone, not a complete SMP API claim. The
legacy KPRCB packet IPI helpers remain unimplemented on ARM32; no ARM32
consumer calls them. The architecture implementation uses its own generic
IPI rendezvous for `KeIpiGenericCall`, software interrupts and freezing.

Pre-commit cppcheck found numeric arguments passed to the type-based
`ALIGN_UP` macro in AP allocation. These are now `ALIGN_UP_BY` calls, so the
PCR is aligned to a page and its thread to 16 bytes. The binary regression
supplies four deliberately unaligned pool addresses: the former build
fails with PCR `02200004`, and the corrected build passes all four cases.

The subsequent physical run of kernel `0f6ec89bf99c4b87` passed storage and
the 121.70-second SMP check, but failed to reach Winlogon. `services.exe`
failed loading `msvcrt.dll` with `c0000005`. The saved kernel stack locates
the original abort at DCCIMVAC in `KeInvalidateRangeAllCaches`, called by
`NtFlushInstructionCache`: FSR `807`, VA `011d0fe0`. Thus two-minute timer/IPI
progress alone does not establish a successful user-session boot.

A cache-line/table-lock experiment passed a modeled withdrawal race, but the
physical boot with kernel `77690480219350c8` still failed: Winlogon initialized
at 15.15 seconds, then `DbgkCreateThread` asserted that its saved thread was
not `PsGetCurrentThread` at 36.53 seconds. The cache-line locking candidate
was removed before the next attempt.


SD interrupt signaling remains disabled in this baseline. The successful
121.75-second run is recorded above, but subsequent runs demonstrate that
other SMP failures remain. A PCR/current-thread migration correction is
being developed separately and is not included in this milestone.

With all SD interrupts enabled, reading back the SDHCI status after
acknowledging it in the ISR and synchronized polling paths did not prevent
the stall. On the GT-N7000, SD bus build `64b050f0596f5f9d` enumerated the
card and reached the first file cycle at 1.96 seconds before stopping.
The register readback experiment was reverted.

Enabling only `SDHCI_INT_CMD_COMPLETE` left PIO readiness, transfer completion,
errors, and card detection on the existing fallback paths. On the GT-N7000,
the diagnostic SD bus build `bb877aceeb73375b` entered the ISR with status
and signal mask both equal to `00000001`. It passed all 17 file cycles
at 5.18 seconds, initialized Winlogon at 14.32 seconds, and passed the SMP
check with CPU mask `3` at 121.85 seconds. The maintainer observed the desktop.
The watchdog reset the phone for log recovery. The Exynos signal policy
allows the command completion bit and preserves existing masks on other
hardware backends.

The clean driver `e400a1b803331b99` passed all 17 file cycles at 5.21 seconds,
initialized Winlogon at 14.42 seconds, and passed the two-core IPI check at
125.49 seconds. The QEMU SMDKC210 test reached SMSS and passed the storage
test after continuing from a pre-kernel firmware read stall.

The ARM HAL regression also reproduced a migration between reading TPIDRPRW
and loading the old IRQL. The former CPU had reached IRQL 5, so the old HAL
returned 5 for a raise from 0 to 2 and skipped the GIC priority update.
Both raise and lower now mask local IRQs across the read and update and
restore the incoming mask. With command completion enabled, HAL
`73319da7fcf53454` reached SMSS in QEMU SMDKC210 SMP in 10.18 seconds.
On the GT-N7000 it passed all 17 file cycles, initialized Winlogon at 14.38
seconds and passed the two-core IPI check at 121.73 seconds.

Adding `SDHCI_INT_XFER_COMPLETE` to the command signal mask passed the
SMDKC210 storage and SMSS checks after resuming the firmware stage. On the
GT-N7000, SD bus build `c6a36d05fb3a2c4b` passed all 17 file cycles at
5.17 seconds and initialized Winlogon at 14.28 seconds. The log stopped
at 15.99 seconds. The watchdog reset the phone before the sustained SMP
check, with no recorded bugcheck. The transfer-completion signal was
removed. The preceding run used the same kernel and HAL and passed with
the command-completion signal alone.

With only `SDHCI_INT_XFER_COMPLETE` enabled, diagnostic build
`12bed2a8e581fd0f` entered the ISR with status and signals both `00000002`.
It passed all 17 file cycles at 5.11 seconds and initialized Winlogon
at 14.24 seconds. At 34.48 seconds, `DbgkCreateThread` asserted
`Thread == PsGetCurrentThread()` and the kernel stopped with `0x1e`.
That assertion also occurred with all SD signals disabled. The isolated
run confirms delivery of the transfer interrupt and exposes a separate
SMP thread-identification failure during startup. The phone's stable
signal policy continues to allow only command completion.

The secondary CPU used IRQL 5 for its runtime clock SGI. A GIC priority
mask at a peripheral's IRQL could block those ticks even though it would
admit the hardware clock. The HAL now assigns the runtime SGI the same
IRQL and GIC priority as the hardware clock and clears the matching SGI
on GICv2. HAL build `ccaf640e441f8b3a` passed SMDKC210 storage and SMSS
with both CPUs after resuming the firmware stage. On the GT-N7000, with
only command-completion signaling, all 17 file cycles passed at 5.26
seconds, Winlogon initialized at 14.51 seconds, and both CPUs answered the
IPI at 121.72 seconds.

An ARM binary test migrates the executing thread immediately after the
PCR read and runs a different thread on the old CPU. The previous
current-thread getter returns the other thread. The getter now masks
local IRQs across the PCR and CurrentThread reads, restores the prior
CPSR control bits, and supplies PreviousMode from that same thread.
The eight migration cases pass. Kernel `eaa363de3c40e6db` reached SMSS
in QEMU SMDKC210 SMP in 10.69 seconds. With command-completion IRQs on
the GT-N7000, all 17 file cycles passed at 5.24 seconds, Winlogon started
at 14.32 seconds, and both processors responded at 121.86 seconds.

Retesting both completion signals after the clock and current-thread
changes passed the SMDKC210 SMP session test in 10.38 seconds. On the
GT-N7000, SD bus build `47c68d8d5bda2b6b` passed 17 file cycles at
5.12 seconds and reached SMSS. Its last entry was `NtProcessStartup`
at 7.90 seconds. The watchdog reset the phone before Winlogon and the
sustained SMP check, with no recorded bugcheck. The command-only
policy is kept for normal boot.
