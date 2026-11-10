> **English** | [中文](translations/zh-CN/README.md) | [Français](translations/fr-FR/README.md) | [Español](translations/es-ES/README.md) | [日本語](translations/ja-JP/README.md) | [العربية](translations/ar-MA/README.md) | [Українська](translations/uk-UA/README.md)

<p align="center">
  <img src="media/graphics/branding/libernt-banner.png" width="480" alt="LiberNT">
</p>

> [!IMPORTANT]
> This fork targets ARMv7-A (32-bit ARM) on QEMU virt with a Cortex-A15. Build the ARM32 image from this repository using the instructions below. Real hardware bring-up is a later milestone.

<div align="center">

# [LiberNT-ARMv7](https://github.com/kernel86-64/LiberNT-ARMv7)
### An ARMv7-A fork of LiberNT

**32-bit ARM, QEMU virt, Cortex-A15. EDK II UEFI → FreeLoader → LiberNT.**

[![License: GPL 3.0](https://img.shields.io/badge/License-GPL_3.0-0161B6?style=for-the-badge&logo=gnu&logoColor=white)](#license)
[![Discord](https://img.shields.io/badge/Discord-Join-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/BpcKg7FZ2)
[![Matrix](https://img.shields.io/badge/Matrix-%23libernt-000000?style=for-the-badge&logo=matrix&logoColor=white)](https://matrix.to/#/#libernt:matrix.org)

<img src="https://img.shields.io/github/repo-size/kernel86-64/LiberNT-ARMv7?style=flat-square&color=blue" alt="Repo Size">
<img src="https://img.shields.io/github/stars/kernel86-64/LiberNT-ARMv7?style=flat-square&color=yellow" alt="Stars">
<img src="https://img.shields.io/github/forks/kernel86-64/LiberNT-ARMv7?style=flat-square&color=lightgrey" alt="Forks">
<img src="https://img.shields.io/github/last-commit/kernel86-64/LiberNT-ARMv7/main-nt10?style=flat-square&color=green" alt="Last Commit">
<img src="https://img.shields.io/github/issues/kernel86-64/LiberNT-ARMv7?style=flat-square&color=orange" alt="Issues">

</div>

## Sponsors

<p align="center">
  <a href="https://github.com/sponsors/ahmedarif193">
    <img src="https://img.shields.io/badge/Sponsor-LiberNT-ea4aaa?style=for-the-badge&logo=githubsponsors&logoColor=white" alt="Sponsor LiberNT">
  </a>
</p>

---

This repository is the **ARMv7-A (32-bit ARM) fork of [LiberNT](https://github.com/LiberNT/LiberNT)**, which is derived from ReactOS. The initial platform is **QEMU virt with a Cortex-A15**, booting through **EDK II UEFI → FreeLoader → LiberNT**.

The fork retains LiberNT’s NVS memory manager and NT10 compatibility goals. The full native ARM32 image reaches the desktop with working VirtIO keyboard and pointer input. Windows 11 remains the reference for shared API behavior; ARM32 processor and firmware behavior follows ARM32 documentation.

Our goal is a system that stays maintainable in the long run: **stable, reliable and efficient**, running on embedded devices too, and able to support every feature a modern operating system should have.

> [!NOTE]
> New to NT? **NT** is the architecture behind every modern version of Windows: its kernel, driver model and system services. LiberNT is a free operating system built on that architecture, and Windows 11 is the reference it measures itself against.

---

## Table of Contents

- [What LiberNT Includes](#what-libernt-includes)
- [Supported Platforms](#supported-platforms)
- [Getting Started](#getting-started)
  - [Step 1: Build the ARMv7 Image](#step-1-build-the-armv7-image)
  - [Step 2: Boot It](#step-2-boot-it)
  - [Step 3: First Boot](#step-3-first-boot)
- [Building LiberNT Yourself](#building-libernt-yourself)
- [Build Hosts](#build-hosts)
- [Project Structure](#project-structure)
- [Contributing](#contributing)
- [Use of AI](#use-of-ai)
- [License](#license)
- [About](#about)

---

## What LiberNT Includes

| Component | ARMv7 status |
| --- | --- |
| **ARM32 kernel and HAL** | ARMv7-A on QEMU virt, one Cortex-A15 CPU, with NVS. |
| **UEFI boot** | ARM32 EDK II loads FreeLoader from the EFI System Partition. |
| **Storage** | VirtIO block storage and an NTFS system partition containing `C:\Windows`. |
| **Input** | VirtIO keyboard and absolute pointer through HID. |
| **Desktop** | Explorer, the Aero theme, fonts and the standard native application set. |
| **Verified applications** | winver, system properties, Notepad, Calculator, Registry Editor and Task Manager. |
| **Boot tests** | EFI regressions, native ARM page-table checks and a bounded headless QEMU harness. |

See [ARMV7_PORT.md](media/doc/ARMV7_PORT.md) for the tested milestones, reference sources and remaining platform gaps.

---

## Supported Platforms

| Target | Build configuration | Tested platform |
| --- | --- | --- |
| **ARMv7-A, 32-bit ARM** | RosBE Clang, `ARCH=arm`, `SARCH=virt`, Debug | QEMU virt, Cortex-A15, one CPU, 512 MiB RAM |

This fork’s build and boot instructions use ARMv7. Real hardware support is planned after the QEMU port.

---

## Getting Started

### Step 1: Build the ARMv7 Image

Follow [Building LiberNT Yourself](#building-libernt-yourself). The full image is `output-Clang-arm-debug/LiberNT.img` (1.5 GiB), with a 64 MiB EFI System Partition and an NTFS system partition.

### Step 2: Boot It

Install `qemu-system-arm` and the ARM32 EDK II firmware. The paths below match the development host; adjust them if your distribution installs firmware elsewhere.

Run these commands **from the repository root**, after the build has finished. Create a writable variables file once, preserving it on subsequent boots:

```sh
test -f output-Clang-arm-debug/QEMU_VARS.fd || \
    cp /usr/share/qemu/edk2-arm-vars.fd output-Clang-arm-debug/QEMU_VARS.fd

qemu-system-arm -machine virt -cpu cortex-a15 -m 512 -smp 1 \
    -drive if=pflash,format=raw,readonly=on,file=/usr/share/qemu/edk2-arm-code.fd \
    -drive if=pflash,format=raw,file=output-Clang-arm-debug/QEMU_VARS.fd \
    -drive if=none,format=raw,id=os,file=output-Clang-arm-debug/LiberNT.img \
    -device virtio-blk-device,drive=os -device ramfb \
    -global virtio-mmio.force-legacy=false \
    -device virtio-keyboard-device -device virtio-tablet-device \
    -nic none -serial stdio -monitor none
```

Stop QEMU before rebuilding or replacing the image or firmware variables. Keep `-smp 1`; ARM32 SMP is not implemented in this port.

### Step 3: First Boot

Allow the preinstalled image to finish mini setup. The initial session uses **Administrator with an empty password**, with automatic logon enabled. Subsequent boots retain the profile.

See [INSTALL.md](INSTALL.md) for the ARMv7 image workflow and [ARMV7_PORT.md](media/doc/ARMV7_PORT.md) for headless tests and debugging.

---

## Building LiberNT Yourself

The validated configuration is a Linux host with the **RosBE Clang/LLVM** cross toolchain, targeting **ARMv7-A**.

### 1. Install the toolchain

Install Git, CMake, Ninja, Flex, Bison, Python 3 and a host C/C++ compiler. For QEMU tests, also install `qemu-system-arm`, ARM32 EDK II firmware, mtools, dosfstools and GDB with ARM support. Ensure the LLVM command-line tools, including `llvm-readobj`, are available on `PATH`.

Install RosBE using the upstream toolchain bootstrap:

```sh
curl -fsSL https://raw.githubusercontent.com/LiberNT/libernt-build-toolchain/main/rosbe-unix-bootstrap.sh | sh
```

### 2. Get this fork

```sh
git clone --branch main-nt10 https://github.com/kernel86-64/LiberNT-ARMv7.git
cd LiberNT-ARMv7
```

### 3. Configure and build ARMv7

Run from the repository root:

```sh
CCACHE_DISABLE=1 ./configure.sh --clang -a arm \
    -DSARCH=virt -DKD_DEBUGGER=NONE -DOPTIMIZE=0
cmake -S . -B output-Clang-arm-debug \
    '-DFREELDR_PREINSTALL_OPTIONS=/DEBUG /DEBUGPORT=COM1 /BAUDRATE=115200'
CCACHE_DISABLE=1 ninja -C output-Clang-arm-debug liberntimg -j8
```

The configure step fetches the external sources selected by the build. Use `--no-feeds-update` on later configure runs when those sources are already present and you need an offline build.

`-a arm` selects 32-bit ARM, `-DSARCH=virt` selects QEMU virt, and the output directory is `output-Clang-arm-debug`. Keep `-DOPTIMIZE=0` for the current Clang exception-handling limitation described in [ARMV7_PORT.md](media/doc/ARMV7_PORT.md). Compiler caching is disabled for these builds.

`liberntimg` builds and packages the full native installation into `output-Clang-arm-debug/LiberNT.img`. Reuse this build directory and image for subsequent builds, with QEMU stopped while the image is rebuilt.

For optional EFI regression tests, follow [Build and boot tests](media/doc/ARMV7_PORT.md#build-and-boot-tests).

---

## Build Hosts

The ARMv7 port has been built and tested on **Linux with RosBE Clang/LLVM**. The host architecture and the target architecture are separate: even on an x86_64 build host, the commands above produce ARM32 binaries.

The toolchain is maintained upstream in [LiberNT/libernt-build-toolchain](https://github.com/LiberNT/libernt-build-toolchain).

---

## Project Structure

```text
LiberNT/
├── base/            # Applications, services, setup and the shell
├── boot/            # FreeLoader, boot data and boot images
├── bootloader/      # Boot manager, Windows loader, UEFI and Open Firmware support
├── dll/             # User-mode DLLs, including ntdll and the Win32 libraries
├── drivers/         # Kernel-mode drivers: buses, storage, file systems, network, input and more
├── hal/             # Hardware abstraction layers for x86, Arm, Arm64, PowerPC and RISC-V
├── media/           # Fonts, themes, INF files, sounds and branding
├── modules/         # Optional applications, tests and wallpapers
├── ntoskrnl/        # The NT kernel and executive
├── sdk/             # Headers, libraries and build tools
├── submodules/      # Recipes and patches for external components
├── subsystems/      # CSRSS and the Windows subsystem
├── win32ss/         # win32k, USER, GDI and display drivers
├── configure.sh     # Sets up a build on Linux, macOS and WSL2
└── configure.cmd    # Sets up a build on Windows
```

---

## Contributing

Report bugs and suggest features in [Issues](https://github.com/kernel86-64/LiberNT-ARMv7/issues), and find guides and notes in the [Wiki](https://github.com/LiberNT/LiberNT/wiki).

See [CONTRIBUTING.md](CONTRIBUTING.md) and [PULL_REQUEST_MANAGEMENT.md](PULL_REQUEST_MANAGEMENT.md). If you're planning a larger change, ask in [Issues](https://github.com/kernel86-64/LiberNT-ARMv7/issues) or on [Discord](https://discord.gg/BpcKg7FZ2) first.

> [!WARNING]
> **Legal notice:** If you have seen proprietary Microsoft Windows source code (including but not limited to the leaked Windows NT 3.5, NT 4, 2000 source code and the Windows Research Kernel), your contribution won't be accepted because of potential copyright violation.

---

## Use of AI

We want to make it clear: AI is part of how LiberNT is built. It helps us review and write code, and everything it writes is then reviewed by a human. Before anything reaches the main branch, a person runs and validates the end-to-end tests. We care about this project too much to let a regression slip through.

It also helps us read documentation, extract definitions and fix bugs. For any given bug, it compares what the official Microsoft Learn documentation says with what our function actually does, so we can see exactly where our behavior differs.

Like any other tool, AI is there to help us achieve our goals. It follows the same rules we hold ourselves to, set out in [AGENTS.md](AGENTS.md):

- We build from the Windows SDK and WDK headers, Windows 11 public symbols, Microsoft documentation and black-box runs on Windows 11.
- We stay clean-room: we never transcribe disassembled or decompiled Windows code, and we never copy GPL code from Linux.
- A fix is only a fix once we have seen it work in a real boot or test run. Once tests pass, a future CI will keep them passing and preserve our overall progress.
- We compare our test results check by check with the same tests run on Windows 11.

> [!TIP]
> If you use AI in your own contributions, you are welcome here too. Just tell us in the pull request which parts it helped with and how you tested them. That honesty is all we ask.

---

## License

LiberNT is licensed under the [GNU GPL 3.0](COPYING). Code inherited from other projects keeps the licence stated in its file headers, most of it GPL 2.0 or later. The GPL 2.0, LGPL 2.1 and BSD texts those files refer to are in [COPYING2](COPYING2), [COPYING.LIB](COPYING.LIB) and [COPYING.ARM](COPYING.ARM).

The changes made in LiberNT are Copyright © 2026 the LiberNT contributors. Inherited and third-party code keeps the copyright notices of its authors. See [CREDITS.md](CREDITS.md).

LiberNT is an independent project. It is not affiliated with, endorsed by or sponsored by Microsoft. Windows® is a registered trademark of Microsoft Corporation.

---

## About

This ARMv7 fork is maintained at [kernel86-64/LiberNT-ARMv7](https://github.com/kernel86-64/LiberNT-ARMv7).

Upstream LiberNT is built by the **LiberNT contributors**. It was started by [Ahmed Arif](https://github.com/ahmedarif193) and is maintained by [Ahmed Arif](https://github.com/ahmedarif193), [Ostap Brehin](https://github.com/osbre) and [Vivyy](https://github.com/enginelesscc).

- [Visit the LiberNT organization](https://github.com/LiberNT)
- [Website](https://libernt.com)
- [Build the ARMv7 image](#building-libernt-yourself)
- [Wiki](https://github.com/LiberNT/LiberNT/wiki)
- Chat on [Discord](https://discord.gg/BpcKg7FZ2) or [Matrix](https://matrix.to/#/#libernt:matrix.org)
