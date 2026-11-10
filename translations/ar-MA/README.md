<div dir="rtl" lang="ar">

> [English](../../README.md) | [中文](../zh-CN/README.md) | [Français](../fr-FR/README.md) | [Español](../es-ES/README.md) | [日本語](../ja-JP/README.md) | **العربية** | [Українська](../uk-UA/README.md)

<p align="center">
  <img src="../../media/graphics/branding/libernt-banner.png" width="480" alt="LiberNT">
</p>

> [!IMPORTANT]
> هذا المستودع **تفريع من LiberNT لمعمارية ARMv7-A ‏(ARM ذات 32 بت)**. المنصة الحالية هي QEMU virt بمعالج Cortex-A15، ومسار الإقلاع هو EDK II UEFI → FreeLoader → LiberNT.

<div align="center">

# [LiberNT-ARMv7](https://github.com/kernel86-64/LiberNT-ARMv7)
### نظام تشغيل NT حر

**ARMv7-A / ARM32 — QEMU virt, Cortex-A15.**

[![License: GPL 3.0](https://img.shields.io/badge/License-GPL_3.0-0161B6?style=for-the-badge&logo=gnu&logoColor=white)](#الترخيص)
[![Discord](https://img.shields.io/badge/Discord-Join-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/BpcKg7FZ2)
[![Matrix](https://img.shields.io/badge/Matrix-%23libernt-000000?style=for-the-badge&logo=matrix&logoColor=white)](https://matrix.to/#/#libernt:matrix.org)

<img src="https://img.shields.io/github/repo-size/kernel86-64/LiberNT-ARMv7?style=flat-square&color=blue" alt="Repo Size">
<img src="https://img.shields.io/github/stars/kernel86-64/LiberNT-ARMv7?style=flat-square&color=yellow" alt="Stars">
<img src="https://img.shields.io/github/forks/kernel86-64/LiberNT-ARMv7?style=flat-square&color=lightgrey" alt="Forks">
<img src="https://img.shields.io/github/last-commit/kernel86-64/LiberNT-ARMv7/main-nt10?style=flat-square&color=green" alt="Last Commit">
<img src="https://img.shields.io/github/issues/kernel86-64/LiberNT-ARMv7?style=flat-square&color=orange" alt="Issues">

</div>

## الرعاة

<p align="center">
  <a href="https://github.com/sponsors/ahmedarif193">
    <img src="https://img.shields.io/badge/Sponsor-LiberNT-ea4aaa?style=for-the-badge&logo=githubsponsors&logoColor=white" alt="Sponsor LiberNT">
  </a>
</p>

---

LiberNT نظام تشغيل NT حر. والاسم يدل على ذلك: كلمة *liber* تعني **حر** باللاتينية. يهدف النظام إلى **التوافق مع Windows® 11**، مع ترك مساحة لميزات حرة وتجريبية.

هذا المستودع **تفريع من LiberNT لمعمارية ARMv7-A ‏(ARM ذات 32 بت)**. المنصة الحالية هي QEMU virt بمعالج Cortex-A15، ومسار الإقلاع هو EDK II UEFI → FreeLoader → LiberNT.

هدفنا نظام يبقى قابلًا للصيانة على المدى الطويل: **مستقر وموثوق وفعّال**، يعمل أيضًا على الأجهزة المدمجة، وقادر على دعم كل ميزة ينبغي أن يوفرها نظام تشغيل حديث.

> [!NOTE]
> جديد على NT؟ **NT** هي المعمارية التي تقوم عليها كل الإصدارات الحديثة من Windows: نواتها ونموذج برامج التشغيل وخدمات النظام. LiberNT نظام تشغيل حر مبني على هذه المعمارية، ويتخذ من Windows 11 مرجعًا يقيس نفسه به.

---

## جدول المحتويات

- [ما الذي يتضمنه LiberNT](#ما-الذي-يتضمنه-libernt)
- [المنصات المدعومة](#المنصات-المدعومة)
- [البدء](#البدء)
  - [الخطوة 1: تنزيل نسخة](#الخطوة-1-تنزيل-نسخة)
  - [الخطوة 2: الإقلاع](#الخطوة-2-الإقلاع)
  - [الخطوة 3: تثبيت LiberNT](#الخطوة-3-تثبيت-libernt)
- [بناء LiberNT بنفسك](#بناء-libernt-بنفسك)
- [أنظمة البناء](#أنظمة-البناء)
- [بنية المشروع](#بنية-المشروع)
- [المساهمة](#المساهمة)
- [استخدام الذكاء الاصطناعي](#استخدام-الذكاء-الاصطناعي)
- [الترخيص](#الترخيص)
- [حول المشروع](#حول-المشروع)

---

## ما الذي يتضمنه LiberNT

هذا المستودع **تفريع من LiberNT لمعمارية ARMv7-A ‏(ARM ذات 32 بت)**. المنصة الحالية هي QEMU virt بمعالج Cortex-A15، ومسار الإقلاع هو EDK II UEFI → FreeLoader → LiberNT.

[ARMv7: tested components and milestones](../../media/doc/ARMV7_PORT.md)

---

## المنصات المدعومة

**ARMv7-A / ARM32 — QEMU virt, Cortex-A15, 1 CPU, 512 MiB RAM.**

جرى اختبار البناء المتقاطع لـ ARMv7 على Linux باستخدام RosBE Clang/LLVM. معمارية الهدف مستقلة عن معمارية جهاز البناء.

---

## البدء

### الخطوة 1: تنزيل نسخة

ثبّت RosBE Clang/LLVM، ثم نفّذ الأوامر التالية لتهيئة ARMv7 وبناء الصورة الكاملة. راجع [README الرئيسي](../../README.md#building-libernt-yourself) للاعتماديات وطريقة تثبيت الأدوات.

### الخطوة 2: الإقلاع

استخدم [أمر QEMU في README الرئيسي](../../README.md#step-2-boot-it)، مع برنامج ثابت ARM32 وملف متغيرات `QEMU_VARS.fd` قابل للكتابة.

### الخطوة 3: تثبيت LiberNT

انتظر اكتمال mini setup عند الإقلاع الأول. الحساب الأولي هو **Administrator بكلمة مرور فارغة** مع تسجيل دخول تلقائي. المنصة المختبرة هي QEMU، ودعم العتاد الحقيقي مرحلة لاحقة.

---

## بناء LiberNT بنفسك

ثبّت RosBE Clang/LLVM، ثم نفّذ الأوامر التالية لتهيئة ARMv7 وبناء الصورة الكاملة. راجع [README الرئيسي](../../README.md#building-libernt-yourself) للاعتماديات وطريقة تثبيت الأدوات.

<div dir="ltr">

```sh
git clone --branch main-nt10 https://github.com/kernel86-64/LiberNT-ARMv7.git
cd LiberNT-ARMv7

CCACHE_DISABLE=1 ./configure.sh --clang -a arm \
    -DSARCH=virt -DKD_DEBUGGER=NONE -DOPTIMIZE=0
cmake -S . -B output-Clang-arm-debug \
    '-DFREELDR_PREINSTALL_OPTIONS=/DEBUG /DEBUGPORT=COM1 /BAUDRATE=115200'
CCACHE_DISABLE=1 ninja -C output-Clang-arm-debug liberntimg -j8
```

</div>

`output-Clang-arm-debug/LiberNT.img`

استخدم [أمر QEMU في README الرئيسي](../../README.md#step-2-boot-it)، مع برنامج ثابت ARM32 وملف متغيرات `QEMU_VARS.fd` قابل للكتابة.

---

## أنظمة البناء

جرى اختبار البناء المتقاطع لـ ARMv7 على Linux باستخدام RosBE Clang/LLVM. معمارية الهدف مستقلة عن معمارية جهاز البناء.

---

## بنية المشروع

<div dir="ltr">

```text
LiberNT/
├── base/            # التطبيقات والخدمات وبرنامج الإعداد والصدفة
├── boot/            # FreeLoader وبيانات الإقلاع وصور الإقلاع
├── bootloader/      # مدير الإقلاع ومحمّل Windows ودعم UEFI وOpen Firmware
├── dll/             # مكتبات DLL في وضع المستخدم، ومنها ntdll ومكتبات Win32
├── drivers/         # برامج التشغيل في وضع النواة: الناقلات والتخزين وأنظمة الملفات والشبكة والإدخال وغيرها
├── hal/             # طبقات تجريد العتاد لـ x86 وArm وArm64 وPowerPC وRISC-V
├── media/           # الخطوط والسمات وملفات INF والأصوات والهوية البصرية
├── modules/         # تطبيقات اختيارية واختبارات وخلفيات
├── ntoskrnl/        # نواة NT والطبقة التنفيذية
├── sdk/             # الترويسات والمكتبات وأدوات البناء
├── submodules/      # وصفات المكونات الخارجية ورقعها
├── subsystems/      # CSRSS ونظام Windows الفرعي
├── win32ss/         # win32k وUSER وGDI وبرامج تشغيل العرض
├── configure.sh     # يجهّز البناء على Linux وmacOS وWSL2
└── configure.cmd    # يجهّز البناء على Windows
```

</div>

---

## المساهمة

أبلغ عن الأخطاء واقترح الميزات في [Issues](https://github.com/kernel86-64/LiberNT-ARMv7/issues)، وستجد الأدلة والملاحظات في [Wiki](https://github.com/LiberNT/LiberNT/wiki).

راجع [CONTRIBUTING.md](CONTRIBUTING.md) و[PULL_REQUEST_MANAGEMENT.md](PULL_REQUEST_MANAGEMENT.md). إن كنت تخطط لتغيير كبير، فاسأل أولًا في [Issues](https://github.com/kernel86-64/LiberNT-ARMv7/issues) أو على [Discord](https://discord.gg/BpcKg7FZ2).

> [!WARNING]
> **تنبيه قانوني:** إذا كنت قد اطّلعت على الشيفرة المصدرية المملوكة لـ Microsoft Windows (بما في ذلك على سبيل المثال لا الحصر الشيفرة المسرّبة لـ Windows NT 3.5 وNT 4 و2000، وWindows Research Kernel)، فلن تُقبل مساهمتك بسبب احتمال انتهاك حقوق النشر.

---

## استخدام الذكاء الاصطناعي

نريد أن نكون واضحين: الذكاء الاصطناعي جزء من طريقة بناء LiberNT. فهو يساعدنا في مراجعة الشيفرة وكتابتها، وكل ما يكتبه يراجعه إنسان بعد ذلك. وقبل أن يصل أي شيء إلى الفرع الرئيسي، يشغّل شخص اختبارات شاملة من البداية إلى النهاية ويتحقق منها. نحن نهتم بهذا المشروع أكثر من أن نترك تراجعًا يمرّ دون أن نلاحظه.

ويساعدنا أيضًا في قراءة التوثيق واستخراج التعريفات وإصلاح الأخطاء. فلكل خطأ، يقارن ما يقوله التوثيق الرسمي في Microsoft Learn بما تفعله دالتنا فعلًا، لنرى بالضبط أين يختلف سلوكنا.

ومثل أي أداة أخرى، الذكاء الاصطناعي موجود لمساعدتنا على تحقيق أهدافنا. وهو يتبع القواعد نفسها التي نلزم بها أنفسنا، والمحددة في [AGENTS.md](../../AGENTS.md):

- نبني اعتمادًا على ترويسات Windows SDK وWDK، والرموز العامة لـ Windows 11، وتوثيق Microsoft، والتشغيل كصندوق أسود على Windows 11.
- نعمل بأسلوب الغرفة النظيفة: لا ننسخ أبدًا شيفرة Windows مفككة أو معاد تجميعها، ولا ننسخ أبدًا شيفرة GPL من Linux.
- لا يُعدّ الإصلاح إصلاحًا إلا بعد أن نراه يعمل في إقلاع حقيقي أو تشغيل للاختبارات. وبعد نجاح الاختبارات، سيحافظ نظام CI مستقبلي على نجاحها ويحفظ تقدّمنا العام.
- نقارن نتائج اختباراتنا، فحصًا بفحص، بنتائج الاختبارات نفسها عند تشغيلها على Windows 11.

> [!TIP]
> إذا كنت تستخدم الذكاء الاصطناعي في مساهماتك، فأنت مرحّب بك أيضًا. فقط أخبرنا في طلب السحب بالأجزاء التي ساعد فيها وكيف اختبرتها. هذه الصراحة هي كل ما نطلبه.

---

## الترخيص

يُرخَّص LiberNT بموجب [GNU GPL 3.0](../../COPYING). وتحتفظ الشيفرة الموروثة من مشاريع أخرى بالترخيص المذكور في ترويسات ملفاتها، ومعظمها GPL 2.0 أو أحدث. نصوص GPL 2.0 وLGPL 2.1 وBSD التي تشير إليها تلك الملفات موجودة في [COPYING2](../../COPYING2) و[COPYING.LIB](../../COPYING.LIB) و[COPYING.ARM](../../COPYING.ARM).

التغييرات التي أُجريت في LiberNT محمية بحقوق النشر © 2026 لمساهمي LiberNT. وتحتفظ الشيفرة الموروثة وشيفرة الأطراف الثالثة بإشعارات حقوق النشر الخاصة بمؤلفيها. راجع [CREDITS.md](../../CREDITS.md).

LiberNT مشروع مستقل. وهو غير تابع لشركة Microsoft، ولا تؤيده ولا ترعاه. Windows® علامة تجارية مسجلة لشركة Microsoft Corporation.

---

## حول المشروع

يبني LiberNT **مساهمو LiberNT**، وقد بدأه [Ahmed Arif](https://github.com/ahmedarif193) ويتولى صيانته [Ahmed Arif](https://github.com/ahmedarif193) و[Ostap Brehin](https://github.com/osbre) و[Vivyy](https://github.com/enginelesscc).

- [زُر منظمة LiberNT](https://github.com/LiberNT)
- [الموقع الإلكتروني](https://libernt.com)
- [Wiki](https://github.com/LiberNT/LiberNT/wiki)
- تحدّث معنا على [Discord](https://discord.gg/BpcKg7FZ2) أو [Matrix](https://matrix.to/#/#libernt:matrix.org)

</div>
