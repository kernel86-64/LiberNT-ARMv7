<div dir="rtl" lang="ar">

# تثبيت LiberNT

> [English](../../INSTALL.md) | [中文](../zh-CN/INSTALL.md) | [Français](../fr-FR/INSTALL.md) | [Español](../es-ES/INSTALL.md) | [日本語](../ja-JP/INSTALL.md) | **العربية** | [Українська](../uk-UA/INSTALL.md)

هذا المستودع **تفريع من LiberNT لمعمارية ARMv7-A ‏(ARM ذات 32 بت)**. المنصة الحالية هي QEMU virt بمعالج Cortex-A15، ومسار الإقلاع هو EDK II UEFI → FreeLoader → LiberNT.

ثبّت RosBE Clang/LLVM، ثم نفّذ الأوامر التالية لتهيئة ARMv7 وبناء الصورة الكاملة. راجع [README الرئيسي](../../README.md#building-libernt-yourself) للاعتماديات وطريقة تثبيت الأدوات.

<div dir="ltr">

```sh
CCACHE_DISABLE=1 ./configure.sh --clang -a arm \
    -DSARCH=virt -DKD_DEBUGGER=NONE -DOPTIMIZE=0
cmake -S . -B output-Clang-arm-debug \
    '-DFREELDR_PREINSTALL_OPTIONS=/DEBUG /DEBUGPORT=COM1 /BAUDRATE=115200'
CCACHE_DISABLE=1 ninja -C output-Clang-arm-debug liberntimg -j8
```

</div>

استخدم [أمر QEMU في README الرئيسي](../../README.md#step-2-boot-it)، مع برنامج ثابت ARM32 وملف متغيرات `QEMU_VARS.fd` قابل للكتابة.

انتظر اكتمال mini setup عند الإقلاع الأول. الحساب الأولي هو **Administrator بكلمة مرور فارغة** مع تسجيل دخول تلقائي. المنصة المختبرة هي QEMU، ودعم العتاد الحقيقي مرحلة لاحقة.

[ARMv7: tests and platform details](../../media/doc/ARMV7_PORT.md)

</div>
