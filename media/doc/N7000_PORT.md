# Samsung Galaxy Note GT-N7000 bring-up

Target: Exynos4210, two Cortex-A9 cores, 1 GiB installed RAM, microSD boot
through the existing EDK II firmware. Kernel startup on this board has not
been demonstrated. The current artifact is an ARM32 EFI platform inventory
application, tested on QEMU virt with Cortex-A15 and on the GT-N7000 through
the supplied firmware and microSD Shell autostart.

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
GIC and MCT operation, not kernel startup. The existing NVS ARM backend and
FreeLoader kernel handoff still use LPAE.

Regression validation after adding this test: `uefildr`, `arm_uefi_test`,
`ntoskrnl` and `hal` build; QEMU virt `abi`, `loader` and `kernel` stages pass.
The latter reaches NVS initialization and the loaded kernel/HAL image list.
The interactive desktop image is not repacked. Simple probe/ABI/loader/kernel/
clock fixtures use disposable variable snapshots, allowing their runs without
overwriting an interactive VM's `QEMU_VARS.fd`.

### Remaining kernel work

1. Implement ARMv7 short-descriptor translation in FreeLoader and the NVS
   architecture backend. The existing ARM32 path uses LPAE, 64-bit PTE slots
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
