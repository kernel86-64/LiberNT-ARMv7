> [English](../../README.md) | [中文](../zh-CN/README.md) | [Français](../fr-FR/README.md) | [Español](../es-ES/README.md) | [日本語](../ja-JP/README.md) | [العربية](../ar-MA/README.md) | **Українська**

<p align="center">
  <img src="../../media/graphics/branding/libernt-banner.png" width="480" alt="LiberNT">
</p>

> [!IMPORTANT]
> Цей репозиторій — **форк LiberNT для ARMv7-A (32-бітний ARM)**. Поточна платформа — QEMU virt із Cortex-A15, ланцюжок завантаження: EDK II UEFI → FreeLoader → LiberNT.

<div align="center">

# [LiberNT-ARMv7](https://github.com/kernel86-64/LiberNT-ARMv7)
### Вільна операційна система NT

**ARMv7-A / ARM32 — QEMU virt, Cortex-A15.**

[![License: GPL 3.0](https://img.shields.io/badge/License-GPL_3.0-0161B6?style=for-the-badge&logo=gnu&logoColor=white)](#ліцензія)
[![Discord](https://img.shields.io/badge/Discord-Join-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/BpcKg7FZ2)
[![Matrix](https://img.shields.io/badge/Matrix-%23libernt-000000?style=for-the-badge&logo=matrix&logoColor=white)](https://matrix.to/#/#libernt:matrix.org)

<img src="https://img.shields.io/github/repo-size/kernel86-64/LiberNT-ARMv7?style=flat-square&color=blue" alt="Repo Size">
<img src="https://img.shields.io/github/stars/kernel86-64/LiberNT-ARMv7?style=flat-square&color=yellow" alt="Stars">
<img src="https://img.shields.io/github/forks/kernel86-64/LiberNT-ARMv7?style=flat-square&color=lightgrey" alt="Forks">
<img src="https://img.shields.io/github/last-commit/kernel86-64/LiberNT-ARMv7/main-nt10?style=flat-square&color=green" alt="Last Commit">
<img src="https://img.shields.io/github/issues/kernel86-64/LiberNT-ARMv7?style=flat-square&color=orange" alt="Issues">

</div>

## Спонсори

<p align="center">
  <a href="https://github.com/sponsors/ahmedarif193">
    <img src="https://img.shields.io/badge/Sponsor-LiberNT-ea4aaa?style=for-the-badge&logo=githubsponsors&logoColor=white" alt="Підтримати LiberNT">
  </a>
</p>

---

LiberNT — це вільна операційна система NT. Її назва говорить сама за себе: *liber* латиною означає **вільний**. Вона орієнтована на **сумісність із Windows® 11**, залишаючи простір для експериментальних та авторських можливостей.

Цей репозиторій — **форк LiberNT для ARMv7-A (32-бітний ARM)**. Поточна платформа — QEMU virt із Cortex-A15, ланцюжок завантаження: EDK II UEFI → FreeLoader → LiberNT.

Наша мета — система, яка залишається зручною для довготривалої підтримки: **стабільна, надійна та ефективна**, здатна працювати також на вбудованих пристроях і підтримувати всі можливості, які повинна мати сучасна операційна система.

> [!NOTE]
> Вперше чуєте про NT? **NT** — це архітектура, що лежить в основі кожної сучасної версії Windows: її ядро, модель драйверів та системні служби. LiberNT — це вільна операційна система, створена на основі цієї архітектури, а Windows 11 є еталоном, з яким вона співставляється.

---

## Зміст

- [Що входить до LiberNT](#що-входить-до-libernt)
- [Підтримувані платформи](#підтримувані-платформи)
- [Початок роботи](#початок-роботи)
  - [Крок 1: Завантажте збірку](#крок-1-завантажте-збірку)
  - [Крок 2: Завантажте систему](#крок-2-завантажте-систему)
  - [Крок 3: Встановіть LiberNT](#крок-3-встановіть-libernt)
- [Збирання LiberNT власноруч](#збирання-libernt-власноруч)
- [Хост-системи для збирання](#хост-системи-для-збирання)
- [Структура проєкту](#структура-проєкту)
- [Участь у розробці](#участь-у-розробці)
- [Використання ШІ](#використання-ші)
- [Ліцензія](#ліцензія)
- [Про проєкт](#про-проєкт)

---

## Що входить до LiberNT

Цей репозиторій — **форк LiberNT для ARMv7-A (32-бітний ARM)**. Поточна платформа — QEMU virt із Cortex-A15, ланцюжок завантаження: EDK II UEFI → FreeLoader → LiberNT.

[ARMv7: tested components and milestones](../../media/doc/ARMV7_PORT.md)

---

## Підтримувані платформи

**ARMv7-A / ARM32 — QEMU virt, Cortex-A15, 1 CPU, 512 MiB RAM.**

Перехресне збирання ARMv7 перевірено на Linux із RosBE Clang/LLVM. Цільова архітектура не залежить від архітектури хоста.

---

## Початок роботи

### Крок 1: Завантажте збірку

Установіть RosBE Clang/LLVM і виконайте наведені команди для налаштування ARMv7 та збирання повного образу. Залежності й установлення інструментарію описані в [основному README](../../README.md#building-libernt-yourself).

### Крок 2: Завантажте систему

Використовуйте [команду QEMU з основного README](../../README.md#step-2-boot-it), прошивку ARM32 і файл змінних `QEMU_VARS.fd`, доступний для запису.

### Крок 3: Встановіть LiberNT

Під час першого завантаження дочекайтеся завершення mini setup. Початковий обліковий запис — **Administrator, пароль порожній**, автоматичний вхід увімкнено. Перевірена платформа — QEMU; реальне обладнання буде наступним етапом.

---

## Збирання LiberNT власноруч

Установіть RosBE Clang/LLVM і виконайте наведені команди для налаштування ARMv7 та збирання повного образу. Залежності й установлення інструментарію описані в [основному README](../../README.md#building-libernt-yourself).

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

Використовуйте [команду QEMU з основного README](../../README.md#step-2-boot-it), прошивку ARM32 і файл змінних `QEMU_VARS.fd`, доступний для запису.

---

## Хост-системи для збирання

Перехресне збирання ARMv7 перевірено на Linux із RosBE Clang/LLVM. Цільова архітектура не залежить від архітектури хоста.

---

## Структура проєкту

```text
LiberNT/
├── base/            # Програми, служби, встановлення та оболонка
├── boot/            # FreeLoader, завантажувальні дані та образи
├── bootloader/      # Менеджер завантаження, завантажувач Windows, підтримка UEFI та Open Firmware
├── dll/             # DLL простору користувача, включно з ntdll та бібліотеками Win32
├── drivers/         # Драйвери режиму ядра: шини, накопичувачі, файлові системи, мережа, введення тощо
├── hal/             # Рівні абстракції від обладнання для x86, Arm, Arm64, PowerPC та RISC-V
├── media/           # Шрифти, теми, файли INF, звуки та брендування
├── modules/         # Додаткові програми, тести та шпалери
├── ntoskrnl/        # Ядро NT та виконавча підсистема
├── sdk/             # Заголовні файли, бібліотеки та інструменти збирання
├── submodules/      # Рецепти та патчі для зовнішніх компонентів
├── subsystems/      # CSRSS та підсистема Windows
├── win32ss/         # win32k, USER, GDI та драйвери дисплея
├── configure.sh     # Скрипт конфігурації для Linux, macOS та WSL2
└── configure.cmd    # Скрипт конфігурації для Windows
```

---

## Участь у розробці

Повідомляйте про помилки та пропонуйте нові можливості в [Issues](https://github.com/kernel86-64/LiberNT-ARMv7/issues); посібники та нотатки можна знайти у [Wiki](https://github.com/LiberNT/LiberNT/wiki).

Див. [CONTRIBUTING.md](CONTRIBUTING.md) та [PULL_REQUEST_MANAGEMENT.md](PULL_REQUEST_MANAGEMENT.md). Якщо ви плануєте значні зміни, спершу запитайте про це в [Issues](https://github.com/kernel86-64/LiberNT-ARMv7/issues) або в [Discord](https://discord.gg/BpcKg7FZ2).

> [!WARNING]
> **Юридичне застереження:** Якщо ви бачили пропрієтарний вихідний код Microsoft Windows (включно з, але не обмежуючись витоками вихідного коду Windows NT 3.5, NT 4, 2000 та Windows Research Kernel), ваш внесок не буде прийнято через потенційне порушення авторських прав.

---

## Використання ШІ

Ми хочемо пояснити прямо: ШІ є частиною того, як розробляється LiberNT. Він допомагає нам рецензувати та писати код, і все написане ним згодом перевіряється людиною. Перш ніж щось потрапить до гілки main, людина запускає та перевіряє наскрізні тести. Цей проєкт для нас надто важливий, щоб пропустити регресію.

Він також допомагає нам читати документацію, вилучати визначення та виправляти помилки. Для кожної помилки він порівнює написане в офіційній документації Microsoft Learn із тим, що насправді робить наша функція, щоб ми бачили, де саме різниться поведінка.

Як і будь-який інструмент, ШІ допомагає нам досягати поставлених цілей. Він дотримується тих самих правил, яких дотримуємося ми самі, описаних в [AGENTS.md](../../AGENTS.md):

- Ми розробляємо код на основі заголовків Windows SDK та WDK, публічних символів Windows 11, документації Microsoft та запусків за принципом «чорної скриньки» на Windows 11.
- Ми залишаємося в межах «чистої кімнати»: ми ніколи не копіюємо дизасембльований чи декомпільований код Windows і ніколи не копіюємо код під GPL з Linux.
- Виправлення вважається виправленням лише тоді, коли ми побачили його роботу під час реального завантаження або виконання тесту. Після успішного проходження тестів майбутній CI підтримуватиме їх проходження та зберігатиме наш прогрес.
- Ми порівнюємо результати наших тестів перевірка за перевіркою з тими самими тестами, запущеними на Windows 11.

> [!TIP]
> Якщо ви використовуєте ШІ у власних внесках, ми вам також раді. Просто вкажіть у пул-реквесті, з якими частинами він допоміг і як ви їх протестували. Ця чесність — усе, про що ми просимо.

---

## Ліцензія

LiberNT поширюється за ліцензією [GNU GPL 3.0](../../COPYING). Код, успадкований від інших проєктів, зберігає ліцензію, зазначену в заголовках його файлів — переважно GPL 2.0 або новішу. Тексти GPL 2.0, LGPL 2.1 та BSD, на які посилаються ці файли, наведено в [COPYING2](../../COPYING2), [COPYING.LIB](../../COPYING.LIB) та [COPYING.ARM](../../COPYING.ARM).

Зміни, внесені до LiberNT, захищені авторським правом © 2026 Автори LiberNT. Успадкований і сторонній код зберігає повідомлення про авторські права його авторів. Див. [CREDITS.md](../../CREDITS.md).

LiberNT — це незалежний проєкт. Він не пов'язаний із корпорацією Microsoft, не схвалений і не фінансується нею. Windows® є зареєстрованою торговою маркою корпорації Microsoft.

---

## Про проєкт

LiberNT створюється **авторами LiberNT**. Проєкт започаткував [Ahmed Arif](https://github.com/ahmedarif193), а підтримують його [Ahmed Arif](https://github.com/ahmedarif193), [Ostap Brehin](https://github.com/osbre) та [Vivyy](https://github.com/enginelesscc).

- [Організація LiberNT на GitHub](https://github.com/LiberNT)
- [Вебсайт](https://libernt.com)
- [Wiki](https://github.com/LiberNT/LiberNT/wiki)
- Спілкування в [Discord](https://discord.gg/BpcKg7FZ2) або [Matrix](https://matrix.to/#/#libernt:matrix.org)
