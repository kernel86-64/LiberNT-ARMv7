# Instalar LiberNT

> [English](../../INSTALL.md) | [中文](../zh-CN/INSTALL.md) | [Français](../fr-FR/INSTALL.md) | **Español** | [日本語](../ja-JP/INSTALL.md) | [العربية](../ar-MA/INSTALL.md) | [Українська](../uk-UA/INSTALL.md)

Este repositorio es un **fork de LiberNT para ARMv7-A (ARM de 32 bits)**, destinado a QEMU virt con Cortex-A15. Arranque: EDK II UEFI → FreeLoader → LiberNT.

Instala RosBE Clang/LLVM y ejecuta los siguientes comandos para configurar ARMv7 y construir la imagen completa. Las dependencias y la instalación de la herramienta están en el [README principal](../../README.md#building-libernt-yourself).

```sh
CCACHE_DISABLE=1 ./configure.sh --clang -a arm \
    -DSARCH=virt -DKD_DEBUGGER=NONE -DOPTIMIZE=0
cmake -S . -B output-Clang-arm-debug \
    '-DFREELDR_PREINSTALL_OPTIONS=/DEBUG /DEBUGPORT=COM1 /BAUDRATE=115200'
CCACHE_DISABLE=1 ninja -C output-Clang-arm-debug liberntimg -j8
```

Usa el [comando QEMU del README principal](../../README.md#step-2-boot-it), con firmware ARM32 y un archivo `QEMU_VARS.fd` con permiso de escritura.

Espera a que termine mini setup en el primer arranque. La cuenta inicial es **Administrator, sin contraseña**, con inicio automático. La plataforma validada es QEMU; el hardware real se abordará después.

[ARMv7: tests and platform details](../../media/doc/ARMV7_PORT.md)
