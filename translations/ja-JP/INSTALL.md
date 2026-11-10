# LiberNT のインストール

> [English](../../INSTALL.md) | [中文](../zh-CN/INSTALL.md) | [Français](../fr-FR/INSTALL.md) | [Español](../es-ES/INSTALL.md) | **日本語** | [العربية](../ar-MA/INSTALL.md) | [Українська](../uk-UA/INSTALL.md)

このリポジトリは **ARMv7-A（32 ビット ARM）向け LiberNT フォーク**です。現在の対象は Cortex-A15 を使用する QEMU virt で、起動経路は EDK II UEFI → FreeLoader → LiberNT です。

RosBE Clang/LLVM を導入し、以下のコマンドで ARMv7 を構成して完全なイメージをビルドします。依存ツールと導入手順は[メイン README](../../README.md#building-libernt-yourself)を参照してください。

```sh
CCACHE_DISABLE=1 ./configure.sh --clang -a arm \
    -DSARCH=virt -DKD_DEBUGGER=NONE -DOPTIMIZE=0
cmake -S . -B output-Clang-arm-debug \
    '-DFREELDR_PREINSTALL_OPTIONS=/DEBUG /DEBUGPORT=COM1 /BAUDRATE=115200'
CCACHE_DISABLE=1 ninja -C output-Clang-arm-debug liberntimg -j8
```

[メイン README の QEMU コマンド](../../README.md#step-2-boot-it)を使用し、ARM32 ファームウェアと書き込み可能な `QEMU_VARS.fd` を用意してください。

初回起動では mini setup の完了を待ちます。初期アカウントは **Administrator、パスワードなし**で、自動ログオンが有効です。現在の検証対象は QEMU で、実機対応は今後の作業です。

[ARMv7: tests and platform details](../../media/doc/ARMV7_PORT.md)
