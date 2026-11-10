> [English](../../README.md) | [中文](../zh-CN/README.md) | [Français](../fr-FR/README.md) | **Español** | [日本語](../ja-JP/README.md) | [العربية](../ar-MA/README.md) | [Українська](../uk-UA/README.md)

<p align="center">
  <img src="../../media/graphics/branding/libernt-banner.png" width="480" alt="LiberNT">
</p>

> [!IMPORTANT]
> Este repositorio es un **fork de LiberNT para ARMv7-A (ARM de 32 bits)**, destinado a QEMU virt con Cortex-A15. Arranque: EDK II UEFI → FreeLoader → LiberNT.

<div align="center">

# [LiberNT-ARMv7](https://github.com/kernel86-64/LiberNT-ARMv7)
### Un sistema operativo NT libre

**ARMv7-A / ARM32 — QEMU virt, Cortex-A15.**

[![License: GPL 3.0](https://img.shields.io/badge/License-GPL_3.0-0161B6?style=for-the-badge&logo=gnu&logoColor=white)](#licencia)
[![Discord](https://img.shields.io/badge/Discord-Join-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/BpcKg7FZ2)
[![Matrix](https://img.shields.io/badge/Matrix-%23libernt-000000?style=for-the-badge&logo=matrix&logoColor=white)](https://matrix.to/#/#libernt:matrix.org)

<img src="https://img.shields.io/github/repo-size/kernel86-64/LiberNT-ARMv7?style=flat-square&color=blue" alt="Repo Size">
<img src="https://img.shields.io/github/stars/kernel86-64/LiberNT-ARMv7?style=flat-square&color=yellow" alt="Stars">
<img src="https://img.shields.io/github/forks/kernel86-64/LiberNT-ARMv7?style=flat-square&color=lightgrey" alt="Forks">
<img src="https://img.shields.io/github/last-commit/kernel86-64/LiberNT-ARMv7/main-nt10?style=flat-square&color=green" alt="Last Commit">
<img src="https://img.shields.io/github/issues/kernel86-64/LiberNT-ARMv7?style=flat-square&color=orange" alt="Issues">

</div>

## Patrocinadores

<p align="center">
  <a href="https://github.com/sponsors/ahmedarif193">
    <img src="https://img.shields.io/badge/Sponsor-LiberNT-ea4aaa?style=for-the-badge&logo=githubsponsors&logoColor=white" alt="Sponsor LiberNT">
  </a>
</p>

---

LiberNT es un sistema operativo NT libre. El nombre lo dice: *liber* significa **libre** en latín. Su objetivo es la **compatibilidad con Windows® 11**, con espacio para funciones libres y experimentales.

Este repositorio es un **fork de LiberNT para ARMv7-A (ARM de 32 bits)**, destinado a QEMU virt con Cortex-A15. Arranque: EDK II UEFI → FreeLoader → LiberNT.

Nuestro objetivo es un sistema que se pueda mantener a largo plazo: **estable, fiable y eficiente**, que funcione también en dispositivos embebidos y que pueda ofrecer todas las funciones que debe tener un sistema operativo moderno.

> [!NOTE]
> ¿Nuevo en NT? **NT** es la arquitectura que hay detrás de todas las versiones modernas de Windows: su kernel, su modelo de controladores y sus servicios del sistema. LiberNT es un sistema operativo libre construido sobre esa arquitectura, y Windows 11 es la referencia con la que se compara.

---

## Índice

- [Qué incluye LiberNT](#qué-incluye-libernt)
- [Plataformas compatibles](#plataformas-compatibles)
- [Primeros pasos](#primeros-pasos)
  - [Paso 1: descargar una compilación](#paso-1-descargar-una-compilación)
  - [Paso 2: arrancarla](#paso-2-arrancarla)
  - [Paso 3: instalar LiberNT](#paso-3-instalar-libernt)
- [Compilar LiberNT por tu cuenta](#compilar-libernt-por-tu-cuenta)
- [Sistemas de compilación](#sistemas-de-compilación)
- [Estructura del proyecto](#estructura-del-proyecto)
- [Contribuir](#contribuir)
- [Uso de la IA](#uso-de-la-ia)
- [Licencia](#licencia)
- [Acerca de](#acerca-de)

---

## Qué incluye LiberNT

Este repositorio es un **fork de LiberNT para ARMv7-A (ARM de 32 bits)**, destinado a QEMU virt con Cortex-A15. Arranque: EDK II UEFI → FreeLoader → LiberNT.

[ARMv7: tested components and milestones](../../media/doc/ARMV7_PORT.md)

---

## Plataformas compatibles

**ARMv7-A / ARM32 — QEMU virt, Cortex-A15, 1 CPU, 512 MiB RAM.**

La compilación cruzada ARMv7 está validada en Linux con RosBE Clang/LLVM. La arquitectura de destino es independiente de la del equipo de compilación.

---

## Primeros pasos

### Paso 1: descargar una compilación

Instala RosBE Clang/LLVM y ejecuta los siguientes comandos para configurar ARMv7 y construir la imagen completa. Las dependencias y la instalación de la herramienta están en el [README principal](../../README.md#building-libernt-yourself).

### Paso 2: arrancarla

Usa el [comando QEMU del README principal](../../README.md#step-2-boot-it), con firmware ARM32 y un archivo `QEMU_VARS.fd` con permiso de escritura.

### Paso 3: instalar LiberNT

Espera a que termine mini setup en el primer arranque. La cuenta inicial es **Administrator, sin contraseña**, con inicio automático. La plataforma validada es QEMU; el hardware real se abordará después.

---

## Compilar LiberNT por tu cuenta

Instala RosBE Clang/LLVM y ejecuta los siguientes comandos para configurar ARMv7 y construir la imagen completa. Las dependencias y la instalación de la herramienta están en el [README principal](../../README.md#building-libernt-yourself).

```sh
git clone --branch main-nt10 https://github.com/kernel86-64/LiberNT-ARMv7.git
cd LiberNT-ARMv7

CCACHE_DISABLE=1 ./configure.sh --clang -a arm \
    -DSARCH=virt -DKD_DEBUGGER=NONE -DOPTIMIZE=0
cmake -S . -B output-Clang-arm-debug \
    '-DFREELDR_PREINSTALL_OPTIONS=/DEBUG /DEBUGPORT=COM1 /BAUDRATE=115200'
CCACHE_DISABLE=1 ninja -C output-Clang-arm-debug liberntimg -j8
```

`output-Clang-arm-debug/LiberNT.img`

Usa el [comando QEMU del README principal](../../README.md#step-2-boot-it), con firmware ARM32 y un archivo `QEMU_VARS.fd` con permiso de escritura.

---

## Sistemas de compilación

La compilación cruzada ARMv7 está validada en Linux con RosBE Clang/LLVM. La arquitectura de destino es independiente de la del equipo de compilación.

---

## Estructura del proyecto

```text
LiberNT/
├── base/            # Aplicaciones, servicios, instalación y el shell
├── boot/            # FreeLoader, datos de arranque e imágenes de arranque
├── bootloader/      # Gestor de arranque, cargador de Windows, soporte de UEFI y Open Firmware
├── dll/             # DLL de modo usuario, incluidas ntdll y las bibliotecas Win32
├── drivers/         # Controladores de modo kernel: buses, almacenamiento, sistemas de archivos, red, entrada y más
├── hal/             # Capas de abstracción de hardware para x86, Arm, Arm64, PowerPC y RISC-V
├── media/           # Fuentes, temas, archivos INF, sonidos e identidad visual
├── modules/         # Aplicaciones opcionales, pruebas y fondos de pantalla
├── ntoskrnl/        # El kernel y el ejecutivo de NT
├── sdk/             # Cabeceras, bibliotecas y herramientas de compilación
├── submodules/      # Recetas y parches de componentes externos
├── subsystems/      # CSRSS y el subsistema de Windows
├── win32ss/         # win32k, USER, GDI y controladores de pantalla
├── configure.sh     # Prepara una compilación en Linux, macOS y WSL2
└── configure.cmd    # Prepara una compilación en Windows
```

---

## Contribuir

Informa de errores y propón funciones en [Issues](https://github.com/kernel86-64/LiberNT-ARMv7/issues), y encuentra guías y notas en la [Wiki](https://github.com/LiberNT/LiberNT/wiki).

Consulta [CONTRIBUTING.md](CONTRIBUTING.md) y [PULL_REQUEST_MANAGEMENT.md](PULL_REQUEST_MANAGEMENT.md). Si planeas un cambio grande, pregunta antes en [Issues](https://github.com/kernel86-64/LiberNT-ARMv7/issues) o en [Discord](https://discord.gg/BpcKg7FZ2).

> [!WARNING]
> **Aviso legal:** si has visto código fuente propietario de Microsoft Windows (incluido, entre otros, el código filtrado de Windows NT 3.5, NT 4 y 2000, y el Windows Research Kernel), tu contribución no se aceptará por una posible infracción de derechos de autor.

---

## Uso de la IA

Queremos dejarlo claro: la IA forma parte de cómo se construye LiberNT. Nos ayuda a revisar y escribir código, y todo lo que escribe lo revisa después una persona. Antes de que algo llegue a la rama principal, una persona ejecuta y valida las pruebas de extremo a extremo. Nos importa demasiado este proyecto como para dejar pasar una regresión.

También nos ayuda a leer documentación, extraer definiciones y corregir errores. Para cada error, compara lo que dice la documentación oficial de Microsoft Learn con lo que hace realmente nuestra función, para que veamos exactamente dónde difiere nuestro comportamiento.

Como cualquier otra herramienta, la IA está ahí para ayudarnos a lograr nuestros objetivos. Sigue las mismas reglas que nos imponemos a nosotros mismos, recogidas en [AGENTS.md](../../AGENTS.md):

- Construimos a partir de las cabeceras del Windows SDK y del WDK, los símbolos públicos de Windows 11, la documentación de Microsoft y ejecuciones de caja negra en Windows 11.
- Trabajamos en sala limpia: nunca transcribimos código de Windows desensamblado o descompilado, y nunca copiamos código GPL de Linux.
- Una corrección solo es una corrección cuando la hemos visto funcionar en un arranque real o en una ejecución de pruebas. Cuando las pruebas pasan, una futura CI se encargará de que sigan pasando y de preservar nuestro progreso.
- Comparamos nuestros resultados de pruebas, comprobación por comprobación, con las mismas pruebas ejecutadas en Windows 11.

> [!TIP]
> Si usas IA en tus propias contribuciones, también eres bienvenido. Solo dinos en la pull request en qué partes te ha ayudado y cómo las has probado. Esa honestidad es todo lo que pedimos.

---

## Licencia

LiberNT se distribuye bajo la [GNU GPL 3.0](../../COPYING). El código heredado de otros proyectos conserva la licencia indicada en la cabecera de sus archivos, en su mayoría GPL 2.0 o posterior. Los textos de la GPL 2.0, la LGPL 2.1 y la licencia BSD a los que hacen referencia esos archivos están en [COPYING2](../../COPYING2), [COPYING.LIB](../../COPYING.LIB) y [COPYING.ARM](../../COPYING.ARM).

Los cambios realizados en LiberNT son Copyright © 2026 de los colaboradores de LiberNT. El código heredado y de terceros conserva los avisos de copyright de sus autores. Consulta [CREDITS.md](../../CREDITS.md).

LiberNT es un proyecto independiente. No está afiliado a Microsoft, ni respaldado ni patrocinado por ella. Windows® es una marca registrada de Microsoft Corporation.

---

## Acerca de

LiberNT lo construyen los **colaboradores de LiberNT**; lo inició [Ahmed Arif](https://github.com/ahmedarif193) y lo mantienen [Ahmed Arif](https://github.com/ahmedarif193), [Ostap Brehin](https://github.com/osbre) y [Vivyy](https://github.com/enginelesscc).

- [Visita la organización LiberNT](https://github.com/LiberNT)
- [Sitio web](https://libernt.com)
- [Wiki](https://github.com/LiberNT/LiberNT/wiki)
- Chatea en [Discord](https://discord.gg/BpcKg7FZ2) o en [Matrix](https://matrix.to/#/#libernt:matrix.org)
