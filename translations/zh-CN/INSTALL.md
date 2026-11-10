# 安装 LiberNT

> [English](../../INSTALL.md) | **中文** | [Français](../fr-FR/INSTALL.md) | [Español](../es-ES/INSTALL.md) | [日本語](../ja-JP/INSTALL.md) | [العربية](../ar-MA/INSTALL.md) | [Українська](../uk-UA/INSTALL.md)

此仓库是 LiberNT 的 **ARMv7-A（32 位 ARM）分支**，当前目标为 QEMU virt 和 Cortex-A15，启动链为 EDK II UEFI → FreeLoader → LiberNT。

安装 RosBE Clang/LLVM 工具链后，从此分支的源码目录执行以下 ARMv7 配置和完整镜像构建命令。完整依赖和安装步骤见[主 README](../../README.md#building-libernt-yourself)。

```sh
CCACHE_DISABLE=1 ./configure.sh --clang -a arm \
    -DSARCH=virt -DKD_DEBUGGER=NONE -DOPTIMIZE=0
cmake -S . -B output-Clang-arm-debug \
    '-DFREELDR_PREINSTALL_OPTIONS=/DEBUG /DEBUGPORT=COM1 /BAUDRATE=115200'
CCACHE_DISABLE=1 ninja -C output-Clang-arm-debug liberntimg -j8
```

按[主 README 的 QEMU 命令](../../README.md#step-2-boot-it)启动 ARM32 镜像，并先创建可写的 `QEMU_VARS.fd`。

首次启动请等待 mini setup 完成。初始账户为 **Administrator，密码为空**，已启用自动登录。当前验证平台为 QEMU，实体硬件移植属于后续工作。

[ARMv7: tests and platform details](../../media/doc/ARMV7_PORT.md)
