> [English](../../README.md) | [中文](../zh-CN/README.md) | **Français** | [Español](../es-ES/README.md) | [日本語](../ja-JP/README.md) | [العربية](../ar-MA/README.md) | [Українська](../uk-UA/README.md)

<p align="center">
  <img src="../../media/graphics/branding/libernt-banner.png" width="480" alt="LiberNT">
</p>

> [!IMPORTANT]
> Ce dépôt est un **fork ARMv7-A (ARM 32 bits) de LiberNT**, destiné à QEMU virt avec un Cortex-A15. Démarrage : EDK II UEFI → FreeLoader → LiberNT.

<div align="center">

# [LiberNT-ARMv7](https://github.com/kernel86-64/LiberNT-ARMv7)
### Un système d'exploitation NT libre

**ARMv7-A / ARM32 — QEMU virt, Cortex-A15.**

[![License: GPL 3.0](https://img.shields.io/badge/License-GPL_3.0-0161B6?style=for-the-badge&logo=gnu&logoColor=white)](#licence)
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

LiberNT est un système d'exploitation NT libre. Le nom le dit : *liber* signifie **libre** en latin. Il vise la **compatibilité avec Windows® 11**, avec de la place pour des fonctions libres et expérimentales.

Ce dépôt est un **fork ARMv7-A (ARM 32 bits) de LiberNT**, destiné à QEMU virt avec un Cortex-A15. Démarrage : EDK II UEFI → FreeLoader → LiberNT.

Notre objectif est un système maintenable sur le long terme : **stable, fiable et efficace**, qui fonctionne aussi sur les appareils embarqués et qui puisse offrir toutes les fonctions qu'un système d'exploitation moderne doit avoir.

> [!NOTE]
> Vous découvrez NT ? **NT** est l'architecture qui se trouve derrière toutes les versions modernes de Windows : son noyau, son modèle de pilotes et ses services système. LiberNT est un système d'exploitation libre construit sur cette architecture, et Windows 11 est la référence à laquelle il se mesure.

---

## Table des matières

- [Ce que contient LiberNT](#ce-que-contient-libernt)
- [Plateformes prises en charge](#plateformes-prises-en-charge)
- [Prise en main](#prise-en-main)
  - [Étape 1 : télécharger une compilation](#étape-1-télécharger-une-compilation)
  - [Étape 2 : la démarrer](#étape-2-la-démarrer)
  - [Étape 3 : installer LiberNT](#étape-3-installer-libernt)
- [Compiler LiberNT vous-même](#compiler-libernt-vous-même)
- [Systèmes de compilation](#systèmes-de-compilation)
- [Structure du projet](#structure-du-projet)
- [Contribuer](#contribuer)
- [Utilisation de l'IA](#utilisation-de-lia)
- [Licence](#licence)
- [À propos](#à-propos)

---

## Ce que contient LiberNT

Ce dépôt est un **fork ARMv7-A (ARM 32 bits) de LiberNT**, destiné à QEMU virt avec un Cortex-A15. Démarrage : EDK II UEFI → FreeLoader → LiberNT.

[ARMv7: tested components and milestones](../../media/doc/ARMV7_PORT.md)

---

## Plateformes prises en charge

**ARMv7-A / ARM32 — QEMU virt, Cortex-A15, 1 CPU, 512 MiB RAM.**

La compilation croisée ARMv7 est validée sous Linux avec RosBE Clang/LLVM. L’architecture cible est indépendante de celle de l’hôte.

---

## Prise en main

### Étape 1 : télécharger une compilation

Installez RosBE Clang/LLVM, puis configurez ARMv7 et construisez l’image complète avec les commandes ci-dessous. Les dépendances et l’installation de la chaîne sont détaillées dans le [README principal](../../README.md#building-libernt-yourself).

### Étape 2 : la démarrer

Utilisez la [commande QEMU du README principal](../../README.md#step-2-boot-it), avec le micrologiciel ARM32 et un fichier `QEMU_VARS.fd` inscriptible.

### Étape 3 : installer LiberNT

Laissez mini setup se terminer au premier démarrage. Le compte initial est **Administrator, sans mot de passe**, avec connexion automatique. La plateforme validée est QEMU ; le matériel réel sera traité ensuite.

---

## Compiler LiberNT vous-même

Installez RosBE Clang/LLVM, puis configurez ARMv7 et construisez l’image complète avec les commandes ci-dessous. Les dépendances et l’installation de la chaîne sont détaillées dans le [README principal](../../README.md#building-libernt-yourself).

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

Utilisez la [commande QEMU du README principal](../../README.md#step-2-boot-it), avec le micrologiciel ARM32 et un fichier `QEMU_VARS.fd` inscriptible.

---

## Systèmes de compilation

La compilation croisée ARMv7 est validée sous Linux avec RosBE Clang/LLVM. L’architecture cible est indépendante de celle de l’hôte.

---

## Structure du projet

```text
LiberNT/
├── base/            # Applications, services, installation et shell
├── boot/            # FreeLoader, données et images de démarrage
├── bootloader/      # Gestionnaire de démarrage, chargeur Windows, prise en charge d'UEFI et d'Open Firmware
├── dll/             # DLL en mode utilisateur, dont ntdll et les bibliothèques Win32
├── drivers/         # Pilotes en mode noyau : bus, stockage, systèmes de fichiers, réseau, entrée, etc.
├── hal/             # Couches d'abstraction matérielle pour x86, Arm, Arm64, PowerPC et RISC-V
├── media/           # Polices, thèmes, fichiers INF, sons et identité visuelle
├── modules/         # Applications optionnelles, tests et fonds d'écran
├── ntoskrnl/        # Le noyau et l'exécutif NT
├── sdk/             # En-têtes, bibliothèques et outils de compilation
├── submodules/      # Recettes et correctifs des composants externes
├── subsystems/      # CSRSS et le sous-système Windows
├── win32ss/         # win32k, USER, GDI et pilotes d'affichage
├── configure.sh     # Prépare une compilation sous Linux, macOS et WSL2
└── configure.cmd    # Prépare une compilation sous Windows
```

---

## Contribuer

Signalez les bogues et proposez des fonctions dans les [Issues](https://github.com/kernel86-64/LiberNT-ARMv7/issues), et trouvez des guides et des notes dans le [Wiki](https://github.com/LiberNT/LiberNT/wiki).

Consultez [CONTRIBUTING.md](CONTRIBUTING.md) et [PULL_REQUEST_MANAGEMENT.md](PULL_REQUEST_MANAGEMENT.md). Si vous prévoyez une modification importante, posez d'abord la question dans les [Issues](https://github.com/kernel86-64/LiberNT-ARMv7/issues) ou sur [Discord](https://discord.gg/BpcKg7FZ2).

> [!WARNING]
> **Avis juridique :** si vous avez vu du code source propriétaire de Microsoft Windows (notamment, sans s'y limiter, le code source divulgué de Windows NT 3.5, NT 4 et 2000, et le Windows Research Kernel), votre contribution ne sera pas acceptée en raison d'une possible violation du droit d'auteur.

---

## Utilisation de l'IA

Soyons clairs : l'IA fait partie de la façon dont LiberNT est construit. Elle nous aide à relire et à écrire du code, et tout ce qu'elle écrit est ensuite relu par une personne. Avant que quoi que ce soit n'arrive dans la branche principale, une personne exécute et valide les tests de bout en bout. Ce projet nous tient trop à cœur pour laisser passer une régression.

Elle nous aide aussi à lire la documentation, à extraire des définitions et à corriger des bogues. Pour un bogue donné, elle compare ce que dit la documentation officielle de Microsoft Learn avec ce que fait réellement notre fonction, afin que nous voyions exactement où notre comportement diffère.

Comme tout autre outil, l'IA est là pour nous aider à atteindre nos objectifs. Elle suit les mêmes règles que celles que nous nous imposons, décrites dans [AGENTS.md](../../AGENTS.md) :

- Nous construisons à partir des en-têtes du Windows SDK et du WDK, des symboles publics de Windows 11, de la documentation Microsoft et d'exécutions en boîte noire sous Windows 11.
- Nous travaillons en salle blanche : nous ne retranscrivons jamais de code Windows désassemblé ou décompilé, et nous ne copions jamais de code GPL de Linux.
- Un correctif n'en est un qu'une fois que nous l'avons vu fonctionner lors d'un vrai démarrage ou d'une exécution de tests. Une fois les tests réussis, une future CI veillera à ce qu'ils le restent et préservera nos progrès.
- Nous comparons nos résultats de tests, vérification par vérification, avec les mêmes tests exécutés sous Windows 11.

> [!TIP]
> Si vous utilisez l'IA dans vos propres contributions, vous êtes aussi le bienvenu. Dites-nous simplement dans la pull request pour quelles parties elle vous a aidé et comment vous les avez testées. Cette honnêteté est tout ce que nous demandons.

---

## Licence

LiberNT est distribué sous la [GNU GPL 3.0](../../COPYING). Le code hérité d'autres projets conserve la licence indiquée dans l'en-tête de ses fichiers, le plus souvent la GPL 2.0 ou ultérieure. Les textes de la GPL 2.0, de la LGPL 2.1 et de la licence BSD auxquels ces fichiers font référence se trouvent dans [COPYING2](../../COPYING2), [COPYING.LIB](../../COPYING.LIB) et [COPYING.ARM](../../COPYING.ARM).

Les modifications apportées dans LiberNT sont sous Copyright © 2026 les contributeurs de LiberNT. Le code hérité et tiers conserve les mentions de copyright de ses auteurs. Consultez [CREDITS.md](../../CREDITS.md).

LiberNT est un projet indépendant. Il n'est ni affilié à Microsoft, ni approuvé, ni parrainé par Microsoft. Windows® est une marque déposée de Microsoft Corporation.

---

## À propos

LiberNT est développé par les **contributeurs de LiberNT** ; il a été lancé par [Ahmed Arif](https://github.com/ahmedarif193) et est maintenu par [Ahmed Arif](https://github.com/ahmedarif193), [Ostap Brehin](https://github.com/osbre) et [Vivyy](https://github.com/enginelesscc).

- [Visiter l'organisation LiberNT](https://github.com/LiberNT)
- [Site web](https://libernt.com)
- [Wiki](https://github.com/LiberNT/LiberNT/wiki)
- Discutez sur [Discord](https://discord.gg/BpcKg7FZ2) ou sur [Matrix](https://matrix.to/#/#libernt:matrix.org)
