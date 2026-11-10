# Встановлення LiberNT

> [English](../../INSTALL.md) | [中文](../zh-CN/INSTALL.md) | [Français](../fr-FR/INSTALL.md) | [Español](../es-ES/INSTALL.md) | [日本語](../ja-JP/INSTALL.md) | [العربية](../ar-MA/INSTALL.md) | **Українська**

Цей репозиторій — **форк LiberNT для ARMv7-A (32-бітний ARM)**. Поточна платформа — QEMU virt із Cortex-A15, ланцюжок завантаження: EDK II UEFI → FreeLoader → LiberNT.

Установіть RosBE Clang/LLVM і виконайте наведені команди для налаштування ARMv7 та збирання повного образу. Залежності й установлення інструментарію описані в [основному README](../../README.md#building-libernt-yourself).

```sh
CCACHE_DISABLE=1 ./configure.sh --clang -a arm \
    -DSARCH=virt -DKD_DEBUGGER=NONE -DOPTIMIZE=0
cmake -S . -B output-Clang-arm-debug \
    '-DFREELDR_PREINSTALL_OPTIONS=/DEBUG /DEBUGPORT=COM1 /BAUDRATE=115200'
CCACHE_DISABLE=1 ninja -C output-Clang-arm-debug liberntimg -j8
```

Використовуйте [команду QEMU з основного README](../../README.md#step-2-boot-it), прошивку ARM32 і файл змінних `QEMU_VARS.fd`, доступний для запису.

Під час першого завантаження дочекайтеся завершення mini setup. Початковий обліковий запис — **Administrator, пароль порожній**, автоматичний вхід увімкнено. Перевірена платформа — QEMU; реальне обладнання буде наступним етапом.

[ARMv7: tests and platform details](../../media/doc/ARMV7_PORT.md)
