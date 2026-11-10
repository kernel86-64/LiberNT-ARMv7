> [English](../../README.md) | [中文](../zh-CN/README.md) | [Français](../fr-FR/README.md) | [Español](../es-ES/README.md) | **日本語** | [العربية](../ar-MA/README.md) | [Українська](../uk-UA/README.md)

<p align="center">
  <img src="../../media/graphics/branding/libernt-banner.png" width="480" alt="LiberNT">
</p>

> [!IMPORTANT]
> このリポジトリは **ARMv7-A（32 ビット ARM）向け LiberNT フォーク**です。現在の対象は Cortex-A15 を使用する QEMU virt で、起動経路は EDK II UEFI → FreeLoader → LiberNT です。

<div align="center">

# [LiberNT-ARMv7](https://github.com/kernel86-64/LiberNT-ARMv7)
### 自由な NT オペレーティングシステム

**ARMv7-A / ARM32 — QEMU virt, Cortex-A15.**

[![License: GPL 3.0](https://img.shields.io/badge/License-GPL_3.0-0161B6?style=for-the-badge&logo=gnu&logoColor=white)](#ライセンス)
[![Discord](https://img.shields.io/badge/Discord-Join-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/BpcKg7FZ2)
[![Matrix](https://img.shields.io/badge/Matrix-%23libernt-000000?style=for-the-badge&logo=matrix&logoColor=white)](https://matrix.to/#/#libernt:matrix.org)

<img src="https://img.shields.io/github/repo-size/kernel86-64/LiberNT-ARMv7?style=flat-square&color=blue" alt="Repo Size">
<img src="https://img.shields.io/github/stars/kernel86-64/LiberNT-ARMv7?style=flat-square&color=yellow" alt="Stars">
<img src="https://img.shields.io/github/forks/kernel86-64/LiberNT-ARMv7?style=flat-square&color=lightgrey" alt="Forks">
<img src="https://img.shields.io/github/last-commit/kernel86-64/LiberNT-ARMv7/main-nt10?style=flat-square&color=green" alt="Last Commit">
<img src="https://img.shields.io/github/issues/kernel86-64/LiberNT-ARMv7?style=flat-square&color=orange" alt="Issues">

</div>

## スポンサー

<p align="center">
  <a href="https://github.com/sponsors/ahmedarif193">
    <img src="https://img.shields.io/badge/Sponsor-LiberNT-ea4aaa?style=for-the-badge&logo=githubsponsors&logoColor=white" alt="Sponsor LiberNT">
  </a>
</p>

---

LiberNT は自由な NT オペレーティングシステムです。名前がそれを表しています。*liber* はラテン語で**自由**を意味します。**Windows® 11 との互換性**を目標とし、自由で実験的な機能の余地も残しています。

このリポジトリは **ARMv7-A（32 ビット ARM）向け LiberNT フォーク**です。現在の対象は Cortex-A15 を使用する QEMU virt で、起動経路は EDK II UEFI → FreeLoader → LiberNT です。

私たちの目標は、長期にわたって保守できるシステムです。**安定していて、信頼でき、効率的**で、組み込み機器でも動作し、最新のオペレーティングシステムが備えるべきあらゆる機能をサポートできるシステムを目指しています。

> [!NOTE]
> NT は初めてですか？ **NT** は、最新のすべての Windows を支えるアーキテクチャです。カーネル、ドライバーモデル、システムサービスがこれにあたります。LiberNT はこのアーキテクチャの上に築かれた自由なオペレーティングシステムで、Windows 11 を比較の基準としています。

---

## 目次

- [LiberNT に含まれるもの](#libernt-に含まれるもの)
- [対応プラットフォーム](#対応プラットフォーム)
- [はじめに](#はじめに)
  - [ステップ 1: ビルドをダウンロードする](#ステップ-1-ビルドをダウンロードする)
  - [ステップ 2: 起動する](#ステップ-2-起動する)
  - [ステップ 3: LiberNT をインストールする](#ステップ-3-libernt-をインストールする)
- [LiberNT を自分でビルドする](#libernt-を自分でビルドする)
- [ビルドホスト](#ビルドホスト)
- [プロジェクトの構成](#プロジェクトの構成)
- [貢献する](#貢献する)
- [AI の利用](#ai-の利用)
- [ライセンス](#ライセンス)
- [このプロジェクトについて](#このプロジェクトについて)

---

## LiberNT に含まれるもの

このリポジトリは **ARMv7-A（32 ビット ARM）向け LiberNT フォーク**です。現在の対象は Cortex-A15 を使用する QEMU virt で、起動経路は EDK II UEFI → FreeLoader → LiberNT です。

[ARMv7: tested components and milestones](../../media/doc/ARMV7_PORT.md)

---

## 対応プラットフォーム

**ARMv7-A / ARM32 — QEMU virt, Cortex-A15, 1 CPU, 512 MiB RAM.**

Linux と RosBE Clang/LLVM による ARMv7 クロスビルドを検証しています。ターゲットのアーキテクチャはビルドホストとは独立しています。

---

## はじめに

### ステップ 1: ビルドをダウンロードする

RosBE Clang/LLVM を導入し、以下のコマンドで ARMv7 を構成して完全なイメージをビルドします。依存ツールと導入手順は[メイン README](../../README.md#building-libernt-yourself)を参照してください。

### ステップ 2: 起動する

[メイン README の QEMU コマンド](../../README.md#step-2-boot-it)を使用し、ARM32 ファームウェアと書き込み可能な `QEMU_VARS.fd` を用意してください。

### ステップ 3: LiberNT をインストールする

初回起動では mini setup の完了を待ちます。初期アカウントは **Administrator、パスワードなし**で、自動ログオンが有効です。現在の検証対象は QEMU で、実機対応は今後の作業です。

---

## LiberNT を自分でビルドする

RosBE Clang/LLVM を導入し、以下のコマンドで ARMv7 を構成して完全なイメージをビルドします。依存ツールと導入手順は[メイン README](../../README.md#building-libernt-yourself)を参照してください。

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

[メイン README の QEMU コマンド](../../README.md#step-2-boot-it)を使用し、ARM32 ファームウェアと書き込み可能な `QEMU_VARS.fd` を用意してください。

---

## ビルドホスト

Linux と RosBE Clang/LLVM による ARMv7 クロスビルドを検証しています。ターゲットのアーキテクチャはビルドホストとは独立しています。

---

## プロジェクトの構成

```text
LiberNT/
├── base/            # アプリケーション、サービス、セットアップ、シェル
├── boot/            # FreeLoader、ブートデータ、ブートイメージ
├── bootloader/      # ブートマネージャー、Windows ローダー、UEFI と Open Firmware のサポート
├── dll/             # ntdll と Win32 ライブラリを含むユーザーモード DLL
├── drivers/         # カーネルモードドライバー: バス、ストレージ、ファイルシステム、ネットワーク、入力など
├── hal/             # x86、Arm、Arm64、PowerPC、RISC-V 用のハードウェア抽象化レイヤー
├── media/           # フォント、テーマ、INF ファイル、サウンド、ブランド素材
├── modules/         # オプションのアプリケーション、テスト、壁紙
├── ntoskrnl/        # NT カーネルとエグゼクティブ
├── sdk/             # ヘッダー、ライブラリ、ビルドツール
├── submodules/      # 外部コンポーネントのレシピとパッチ
├── subsystems/      # CSRSS と Windows サブシステム
├── win32ss/         # win32k、USER、GDI、ディスプレイドライバー
├── configure.sh     # Linux、macOS、WSL2 でビルドを構成する
└── configure.cmd    # Windows でビルドを構成する
```

---

## 貢献する

バグの報告や機能の提案は [Issues](https://github.com/kernel86-64/LiberNT-ARMv7/issues) で、ガイドやメモは [Wiki](https://github.com/LiberNT/LiberNT/wiki) で見つけられます。

[CONTRIBUTING.md](CONTRIBUTING.md) と [PULL_REQUEST_MANAGEMENT.md](PULL_REQUEST_MANAGEMENT.md) を参照してください。大きな変更を計画している場合は、先に [Issues](https://github.com/kernel86-64/LiberNT-ARMv7/issues) または [Discord](https://discord.gg/BpcKg7FZ2) で相談してください。

> [!WARNING]
> **法的注意事項:** Microsoft Windows のプロプライエタリなソースコード（流出した Windows NT 3.5、NT 4、2000 のソースコードや Windows Research Kernel を含みますが、これらに限りません）を見たことがある場合、著作権侵害のおそれがあるため、あなたの貢献は受け付けられません。

---

## AI の利用

はっきりさせておきます。AI は LiberNT の開発方法の一部です。コードのレビューや作成を手伝い、AI が書いたものはすべて人間がレビューします。メインブランチに何かが入る前に、人間がエンドツーエンドのテストを実行して検証します。私たちはこのプロジェクトを大切に思っているので、リグレッションを見逃すことはしません。

AI は、ドキュメントを読む、定義を抽出する、バグを修正するといった作業も手伝います。個々のバグについて、Microsoft Learn の公式ドキュメントの記述と私たちの関数の実際の動作を比較し、動作がどこで食い違っているかを正確に把握できるようにします。

ほかのツールと同じように、AI は私たちが目標を達成するための助けです。AI も、私たちが自らに課している [AGENTS.md](../../AGENTS.md) のルールに従います。

- Windows SDK と WDK のヘッダー、Windows 11 の公開シンボル、Microsoft のドキュメント、Windows 11 上でのブラックボックス実行をもとに開発します。
- クリーンルームを守ります。逆アセンブルや逆コンパイルした Windows のコードを書き写すことは決してなく、Linux の GPL コードをコピーすることもありません。
- 修正は、実際の起動やテスト実行で動作を確認して初めて修正といえます。テストが通るようになったら、将来の CI がそれを維持し、全体の進捗を守ります。
- テスト結果は、Windows 11 で実行した同じテストと 1 項目ずつ比較します。

> [!TIP]
> 自分の貢献に AI を使っている方も歓迎します。どの部分で AI の助けを借り、どのようにテストしたかをプルリクエストで伝えてください。お願いしたいのは、その誠実さだけです。

---

## ライセンス

LiberNT は [GNU GPL 3.0](../../COPYING) でライセンスされています。ほかのプロジェクトから引き継いだコードは、ファイルのヘッダーに記載されたライセンスを維持しており、その多くは GPL 2.0 以降です。これらのファイルが参照する GPL 2.0、LGPL 2.1、BSD のライセンス文は [COPYING2](../../COPYING2)、[COPYING.LIB](../../COPYING.LIB)、[COPYING.ARM](../../COPYING.ARM) にあります。

LiberNT で加えられた変更の著作権は Copyright © 2026 LiberNT コントリビューターに帰属します。引き継いだコードやサードパーティのコードは、作者の著作権表示を維持します。[CREDITS.md](../../CREDITS.md) を参照してください。

LiberNT は独立したプロジェクトです。Microsoft との提携はなく、Microsoft による承認や後援も受けていません。Windows® は Microsoft Corporation の登録商標です。

---

## このプロジェクトについて

LiberNT は **LiberNT コントリビューター**によって開発されており、[Ahmed Arif](https://github.com/ahmedarif193) が立ち上げ、[Ahmed Arif](https://github.com/ahmedarif193)、[Ostap Brehin](https://github.com/osbre)、[Vivyy](https://github.com/enginelesscc) がメンテナンスしています。

- [LiberNT オーガニゼーションを見る](https://github.com/LiberNT)
- [ウェブサイト](https://libernt.com)
- [Wiki](https://github.com/LiberNT/LiberNT/wiki)
- [Discord](https://discord.gg/BpcKg7FZ2) または [Matrix](https://matrix.to/#/#libernt:matrix.org) でチャット
