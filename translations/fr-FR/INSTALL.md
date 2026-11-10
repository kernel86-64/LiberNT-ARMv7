# Installer LiberNT

> [English](../../INSTALL.md) | [中文](../zh-CN/INSTALL.md) | **Français** | [Español](../es-ES/INSTALL.md) | [日本語](../ja-JP/INSTALL.md) | [العربية](../ar-MA/INSTALL.md) | [Українська](../uk-UA/INSTALL.md)

Ce dépôt est un **fork ARMv7-A (ARM 32 bits) de LiberNT**, destiné à QEMU virt avec un Cortex-A15. Démarrage : EDK II UEFI → FreeLoader → LiberNT.

Installez RosBE Clang/LLVM, puis configurez ARMv7 et construisez l’image complète avec les commandes ci-dessous. Les dépendances et l’installation de la chaîne sont détaillées dans le [README principal](../../README.md#building-libernt-yourself).

```sh
CCACHE_DISABLE=1 ./configure.sh --clang -a arm \
    -DSARCH=virt -DKD_DEBUGGER=NONE -DOPTIMIZE=0
cmake -S . -B output-Clang-arm-debug \
    '-DFREELDR_PREINSTALL_OPTIONS=/DEBUG /DEBUGPORT=COM1 /BAUDRATE=115200'
CCACHE_DISABLE=1 ninja -C output-Clang-arm-debug liberntimg -j8
```

Utilisez la [commande QEMU du README principal](../../README.md#step-2-boot-it), avec le micrologiciel ARM32 et un fichier `QEMU_VARS.fd` inscriptible.

Laissez mini setup se terminer au premier démarrage. Le compte initial est **Administrator, sans mot de passe**, avec connexion automatique. La plateforme validée est QEMU ; le matériel réel sera traité ensuite.

[ARMv7: tests and platform details](../../media/doc/ARMV7_PORT.md)
