> [English](../../README.md) | **中文** | [Français](../fr-FR/README.md) | [Español](../es-ES/README.md) | [日本語](../ja-JP/README.md) | [العربية](../ar-MA/README.md) | [Українська](../uk-UA/README.md)
>
> 翻译说明：本译文由 AI 初译并排版；术语、代码块、链接与锚点已与英文原版核对，中文术语已经人工核对。如有纰漏，请提交 issue 指出；表述以英文版为准。

<p align="center">
  <img src="../../media/graphics/branding/libernt-banner.png" width="480" alt="LiberNT">
</p>

> [!IMPORTANT]
> 此仓库是 LiberNT 的 **ARMv7-A（32 位 ARM）分支**，当前目标为 QEMU virt 和 Cortex-A15，启动链为 EDK II UEFI → FreeLoader → LiberNT。

<div align="center">

# [LiberNT-ARMv7](https://github.com/kernel86-64/LiberNT-ARMv7)
### 自由的 NT 操作系统

**ARMv7-A / ARM32 — QEMU virt, Cortex-A15.**

[![License: GPL 3.0](https://img.shields.io/badge/License-GPL_3.0-0161B6?style=for-the-badge&logo=gnu&logoColor=white)](#许可证)
[![Discord](https://img.shields.io/badge/Discord-Join-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/BpcKg7FZ2)
[![Matrix](https://img.shields.io/badge/Matrix-%23libernt-000000?style=for-the-badge&logo=matrix&logoColor=white)](https://matrix.to/#/#libernt:matrix.org)

<img src="https://img.shields.io/github/repo-size/kernel86-64/LiberNT-ARMv7?style=flat-square&color=blue" alt="Repo Size">
<img src="https://img.shields.io/github/stars/kernel86-64/LiberNT-ARMv7?style=flat-square&color=yellow" alt="Stars">
<img src="https://img.shields.io/github/forks/kernel86-64/LiberNT-ARMv7?style=flat-square&color=lightgrey" alt="Forks">
<img src="https://img.shields.io/github/last-commit/kernel86-64/LiberNT-ARMv7/main-nt10?style=flat-square&color=green" alt="Last Commit">
<img src="https://img.shields.io/github/issues/kernel86-64/LiberNT-ARMv7?style=flat-square&color=orange" alt="Issues">

</div>

## 赞助

<p align="center">
  <a href="https://github.com/sponsors/ahmedarif193">
    <img src="https://img.shields.io/badge/Sponsor-LiberNT-ea4aaa?style=for-the-badge&logo=githubsponsors&logoColor=white" alt="Sponsor LiberNT">
  </a>
</p>

---

LiberNT 是一个自由的 NT 操作系统。名字本身已经说明：*liber* 在拉丁语里意为**自由**。它以**兼容 Windows® 11** 为目标，也为自由发挥和实验性功能留出余地。

此仓库是 LiberNT 的 **ARMv7-A（32 位 ARM）分支**，当前目标为 QEMU virt 和 Cortex-A15，启动链为 EDK II UEFI → FreeLoader → LiberNT。

我们希望这套系统长期可维护：**稳定、可靠、高效**，既能跑在嵌入式设备上，也具备现代操作系统应有的完整能力。

> [!NOTE]
> 初次接触 NT？**NT** 是支撑所有现代 Windows 版本的架构，涵盖内核、驱动模型和系统服务。LiberNT 正是构建在这套架构之上，以 Windows 11 作为对标的参照。

---

## 目录

- [LiberNT 带来了什么](#libernt-带来了什么)
- [支持的平台](#支持的平台)
- [快速上手](#快速上手)
  - [第 1 步：下载构建](#第-1-步下载构建)
  - [第 2 步：启动](#第-2-步启动)
  - [第 3 步：安装 LiberNT](#第-3-步安装-libernt)
- [自行构建 LiberNT](#自行构建-libernt)
- [构建主机](#构建主机)
- [项目结构](#项目结构)
- [参与贡献](#参与贡献)
- [关于 AI 的使用](#关于-ai-的使用)
- [许可证](#许可证)
- [关于](#关于)

---

## LiberNT 带来了什么

此仓库是 LiberNT 的 **ARMv7-A（32 位 ARM）分支**，当前目标为 QEMU virt 和 Cortex-A15，启动链为 EDK II UEFI → FreeLoader → LiberNT。

[ARMv7: tested components and milestones](../../media/doc/ARMV7_PORT.md)

---

## 支持的平台

**ARMv7-A / ARM32 — QEMU virt, Cortex-A15, 1 CPU, 512 MiB RAM.**

已验证的构建主机为 Linux，使用 RosBE Clang/LLVM 交叉编译器。目标架构为 ARMv7，与主机架构无关。

---

## 快速上手

### 第 1 步：下载构建

安装 RosBE Clang/LLVM 工具链后，从此分支的源码目录执行以下 ARMv7 配置和完整镜像构建命令。完整依赖和安装步骤见[主 README](../../README.md#building-libernt-yourself)。

### 第 2 步：启动

按[主 README 的 QEMU 命令](../../README.md#step-2-boot-it)启动 ARM32 镜像，并先创建可写的 `QEMU_VARS.fd`。

### 第 3 步：安装 LiberNT

首次启动请等待 mini setup 完成。初始账户为 **Administrator，密码为空**，已启用自动登录。当前验证平台为 QEMU，实体硬件移植属于后续工作。

---

## 自行构建 LiberNT

安装 RosBE Clang/LLVM 工具链后，从此分支的源码目录执行以下 ARMv7 配置和完整镜像构建命令。完整依赖和安装步骤见[主 README](../../README.md#building-libernt-yourself)。

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

按[主 README 的 QEMU 命令](../../README.md#step-2-boot-it)启动 ARM32 镜像，并先创建可写的 `QEMU_VARS.fd`。

---

## 构建主机

已验证的构建主机为 Linux，使用 RosBE Clang/LLVM 交叉编译器。目标架构为 ARMv7，与主机架构无关。

---

## 项目结构

```text
LiberNT/
├── base/            # 应用、服务、安装程序与 shell
├── boot/            # FreeLoader、启动数据与启动镜像
├── bootloader/      # 启动管理器、Windows 加载器、UEFI 与 Open Firmware 支持
├── dll/             # 用户态 DLL,含 ntdll 与 Win32 库
├── drivers/         # 内核态驱动:总线、存储、文件系统、网络、输入等
├── hal/             # x86、Arm、Arm64、PowerPC 与 RISC-V 的硬件抽象层
├── media/           # 字体、主题、INF 文件、声音与品牌资源
├── modules/         # 可选应用、测试与壁纸
├── ntoskrnl/        # NT 内核与执行体
├── sdk/             # 头文件、库与构建工具
├── submodules/      # 外部组件的配方与补丁
├── subsystems/      # CSRSS 与 Windows 子系统
├── win32ss/         # win32k、USER、GDI 与显示驱动
├── configure.sh     # 在 Linux、macOS 与 WSL2 上配置构建
└── configure.cmd    # 在 Windows 上配置构建
```

---

## 参与贡献

问题反馈和功能建议请提交到 [Issues](https://github.com/kernel86-64/LiberNT-ARMv7/issues)，指南与笔记见 [Wiki](https://github.com/LiberNT/LiberNT/wiki)。

动手前请阅读 [CONTRIBUTING.md](../../CONTRIBUTING.md) 和 [PULL_REQUEST_MANAGEMENT.md](../../PULL_REQUEST_MANAGEMENT.md)。如果打算做较大的改动，建议先在 [Issues](https://github.com/kernel86-64/LiberNT-ARMv7/issues) 或 [Discord](https://discord.gg/BpcKg7FZ2) 上讨论。

> [!WARNING]
> **法律声明：** 如果你接触过微软 Windows 的专有源码(包括但不限于泄露的 Windows NT 3.5、NT 4、2000 源码以及 Windows Research Kernel)，由于存在版权风险，你的贡献将无法被接受。

---

## 关于 AI 的使用

先说清楚：AI 是 LiberNT 开发方式的一部分。它协助我们审查和编写代码，写出的内容随后由人工复核。任何改动进入主分支之前，都会有人运行并验证端到端测试。我们对这个项目足够上心，回归问题必须挡在合并之前。

AI 还帮我们读文档、提取定义、修复缺陷。遇到具体缺陷时，它会拿 Microsoft Learn 官方文档与函数的实际行为对照，把差异定位到具体位置。

和别的工具一样，AI 用来帮我们达成目标，并遵守 [AGENTS.md](../../AGENTS.md) 里写明的同一套规则：

- 我们基于 Windows SDK 和 WDK 头文件、Windows 11 公开符号、微软文档，以及在 Windows 11 上的黑盒运行来开发。
- 坚持净室方式：反汇编、反编译得到的 Windows 代码一律不照抄，Linux 的 GPL 代码也不复制。
- 修复要经过真实启动或测试运行的验证才算数。测试通过之后，后续 CI 会保证它们一直通过，守住整体进度。
- 测试结果与 Windows 11 上跑同一套测试的结果逐项比对。

> [!TIP]
> 你在自己的贡献里用了 AI，同样欢迎。只要在 PR 里说明 AI 帮了哪些部分、你又是怎么测试的。这份坦诚就足够了。

---

## 许可证

LiberNT 以 [GNU GPL 3.0](../../COPYING) 授权。沿用自其他项目的代码保留其文件头声明的许可证，大多为 GPL 2.0 或更高版本。这些文件引用的 GPL 2.0、LGPL 2.1 和 BSD 文本见 [COPYING2](../../COPYING2)、[COPYING.LIB](../../COPYING.LIB) 和 [COPYING.ARM](../../COPYING.ARM)。

LiberNT 中的改动版权归 LiberNT 贡献者所有，© 2026。沿用代码与第三方代码保留原作者的版权声明。详见 [CREDITS.md](../../CREDITS.md)。

LiberNT 是独立项目，与 Microsoft 没有隶属、认可或赞助关系。Windows® 是 Microsoft Corporation 的注册商标。

---

## 关于

LiberNT 由 **LiberNT 贡献者**共同构建，由 [Ahmed Arif](https://github.com/ahmedarif193) 发起，由 [Ahmed Arif](https://github.com/ahmedarif193)、[Ostap Brehin](https://github.com/osbre) 和 [Vivyy](https://github.com/enginelesscc) 维护。

- [访问 LiberNT 组织](https://github.com/LiberNT)
- [官网](https://libernt.com)
- [Wiki](https://github.com/LiberNT/LiberNT/wiki)
- 在 [Discord](https://discord.gg/BpcKg7FZ2) 或 [Matrix](https://matrix.to/#/#libernt:matrix.org) 上交流
