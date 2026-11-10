# Running LiberNT on ARMv7

> **English** | [中文](translations/zh-CN/INSTALL.md) | [Français](translations/fr-FR/INSTALL.md) | [Español](translations/es-ES/INSTALL.md) | [日本語](translations/ja-JP/INSTALL.md) | [العربية](translations/ar-MA/INSTALL.md) | [Українська](translations/uk-UA/INSTALL.md)

## Build the ARM32 image

Use the toolchain and source checkout instructions in [Building LiberNT Yourself](README.md#building-libernt-yourself). From the repository root:

```sh
CCACHE_DISABLE=1 ./configure.sh --clang -a arm \
    -DSARCH=virt -DKD_DEBUGGER=NONE -DOPTIMIZE=0
cmake -S . -B output-Clang-arm-debug \
    '-DFREELDR_PREINSTALL_OPTIONS=/DEBUG /DEBUGPORT=COM1 /BAUDRATE=115200'
CCACHE_DISABLE=1 ninja -C output-Clang-arm-debug liberntimg -j8
```

The output is `output-Clang-arm-debug/LiberNT.img`, containing the standard native ARM32 applications, services, fonts and themes. Reuse this build directory. Stop QEMU before rebuilding the image.

## Boot through UEFI in QEMU

Follow the complete [QEMU command in README](README.md#step-2-boot-it), including creation of `QEMU_VARS.fd`. Use ARM32 EDK II firmware and QEMU virt with one Cortex-A15 CPU and 512 MiB RAM.

The boot chain is **EDK II UEFI → FreeLoader → LiberNT**. The image has an EFI System Partition and an NTFS system partition containing `C:\Windows`.

## First boot and logon

Let mini setup finish. The initial account is **Administrator**, with an empty password and automatic logon enabled. The desktop and profile survive subsequent boots. Use the guest’s normal shutdown before closing QEMU.

## Testing and hardware scope

The current target is QEMU ARMv7. Real hardware bring-up is a later milestone. [ARMV7_PORT.md](media/doc/ARMV7_PORT.md) records the actual boot stages, application checks, headless test commands and platform limitations.

Report issues in [this fork’s issue tracker](https://github.com/kernel86-64/LiberNT-ARMv7/issues), including the commit, `ARCH=arm`, `SARCH=virt`, QEMU CPU and firmware, the build type, serial log and reproduction steps.
