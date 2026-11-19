# Apple Silicon：游戏与 C++ 模组

本次完成的是 macOS 适配源码、构建入口和新手流程。**按本次要求，没有进行 Mac 编译验证或 Mac 实机验证；不能把 Windows 测试通过当作 Mac 已可运行。** 分享包不包含编译好的 Mac 游戏。

游戏、物理、战斗实体、能力和示例模组均保持 C++17。Shell 和 CMake 只负责构建、安装与启动。

## 在你的 M 系列 Mac 上开始

1. 在终端进入克隆的 OrbDuel 源码目录。若使用团队生成的源码交接包，先解压并进入 `OrbDuel-source`，包内 `PROVENANCE.md` 记录其来源。
2. 安装 Apple 的 Command Line Tools。在终端运行 `xcode-select --install`，完成系统安装窗口。
3. 安装 [Homebrew](https://brew.sh)。这是本机准备工作，不需要购买开发者账号。
4. 在这个源码文件夹打开终端，运行：

```bash
bash tools/build-macos.sh
bash Play.command
```

构建脚本会在依赖缺少时通过 Homebrew 安装 CMake、pkgconf、SDL2 兼容库、SDL2_image 和 SDL2_ttf，然后用 Apple Clang 编译游戏和七个 C++ 示例模组。Homebrew 的 `sdl2-compat` 通过 SDL3 提供 SDL2 接口。

请使用 Apple Silicon 原生终端和 `/opt/homebrew` 的原生工具链。程序和原生模组必须使用相同 CPU 架构；Windows DLL 不可在 Mac 加载。脚本按终端的 `uname -m` 生成架构。

想双击启动时，可先执行一次：

```bash
chmod +x Build.command Play.command mod-sdk/New-Mod.command
```

然后双击 `Build.command` 构建，双击 `Play.command` 启动。首次解压的执行权限取决于解压工具；终端中的 `bash ...` 不要求脚本已有执行权限。macOS 的“隐私与安全性”若阻止来源未知的文件，请先确认来源和源码，再按系统的打开提示处理；脚本不全局关闭系统保护。

## 制作第一份模组

构建游戏后，在源码根目录运行：

```bash
bash mod-sdk/new-mod.sh --id my-vortex --kind vortex --name "我的旋风"
```

进入 `user/mod-projects/my-vortex`，打开 `mod.json`，把 `defaults.strength` 从 `300` 改成 `-600`，然后运行：

```bash
bash build-install.sh
```

也可以双击生成工程里的 `Build-Install.command`。成功后回游戏“模组工坊”点击“重新加载模组和内容”，再在编辑器放置“我的旋风”。负值反向旋转；旧对象保留自己的参数，新放置对象采用新默认值。

需要分身模板时：

```bash
bash mod-sdk/new-mod.sh --id my-summons --kind summon --name "我的分身"
```

修改其 `interval/maxAlive/lifetime/hpScale/radiusScale/attackScale/speed` 后，使用同一安装入口。示例玩法和生命周期见 [COMBAT_ENTITIES](COMBAT_ENTITIES.md)。生成工程携带 SDK 和许可证，可以整个移给队友；新增玩法时修改 `my_mod.cpp`，仍用 C++ 编译。

## 文件放在哪里

| 内容 | 位置 |
|---|---|
| 编译好的游戏 | `build-macos/bin/orbduel` |
| 内置示例模组 | `build-macos/bin/mods/` |
| 自制工程 | `user/mod-projects/<id>/` |
| 模组可分享成品 | 工程内 `build-macos/package/<id>/` |
| Mac 存档和设置 | `~/Library/Application Support/OrbDuel/OrbDuel/` |
| Mac 安装的自制模组 | 上述目录的 `mods/` |

用户模组与内置模组一起加载；相同模组 ID 时用户目录版本优先，其他内置模组保留。关闭某个 ID 会同时禁止它的内置版本和用户版本。重新加载前仍先销毁所有对局/预览实例。DLL/dylib 副本加载与独立素材路径保持原有设计。

**分享什么：** 给队友完整源码交接包，或只分享某个模组的 `package/<id>/`。Mac 成品是 `.dylib`，Windows 成品是 `.dll`；跨系统分享工程源码重新构建。不要发个人存档、`.orbduel-user.json`、整个 build 目录或原工作区的 `.git`。

Windows 的 `build.ps1`、`new-mod.ps1` 和 `build-install.cmd` 保持可用，详见 [FIRST_MOD](FIRST_MOD.md)。这次只运行 Windows 回归，结果和限制见 [VALIDATION](VALIDATION.md)。
