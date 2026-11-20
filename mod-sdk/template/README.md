# 我的第一个模组

需要 Windows x64、Visual Studio 2022 或 2026（或 Build Tools）的 C++ 桌面开发和 CMake 工具。VS 2026 需要 CMake 4.2 或更新。双击会自动寻找工具，并沿用已有工程的生成器。

1. 打开 `mod.json`，修改 `name`、`typeName` 和 `defaults`。旋风模板调整 `strength`（如 300 改为 -600，反向旋转）；分身模板调整 `interval`、`maxAlive`、`lifetime`、`hpScale`、`radiusScale`、`attackScale`、`speed`。
2. 双击 `build-install.cmd`。首次选择游戏的 `orbduel.exe`，成功后会记住位置。
3. 回到游戏的“模组工坊”，点击“重新加载模组和内容”。
4. 在地图编辑器选中你的类型，点击画布放置后测试。旋风可把半径调到 200；分身选择所属角色，并把地图碰撞伤害系数设为 1 来观察普通碰撞伤害。

重新修改后再双击即可；使用本轮新版游戏时不必退出游戏。旧对象保存了自己的参数，新默认值只影响新放置的对象；也可直接在右侧参数面板修改旧对象。

命令行也可以运行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build-install.ps1 -GameDirectory "..\OrbDuel"
```

分享 `build/package/<你的 id>/`，里面是 DLL、mod.json、SDK 许可证及可选 assets 素材。不要分享整个工程的 build 文件夹或 `.orbduel-user.json`：后者包含你本机的安装位置。

`my_mod.cpp` 是玩法逻辑。`mod_config.hpp.in` 由配置生成，改名时只改 `mod.json`，不要手工编辑生成的头文件。用生成器创建的工程包含 `include/orbduel_mod.hpp`，可整个移动到其他位置独立构建。

分身模板使用可选的 include/orbduel_entities.hpp，创建会移动、碰撞、受伤和死亡的额外实体；需要支持战斗实体扩展的新版游戏。旧宿主忽略扩展入口时示例不会召唤。完整教程位于游戏 docs/COMBAT_ENTITIES.md。

Mac 入口：修改 mod.json 后双击 Build-Install.command，或运行 bash build-install.sh；安装到用户 Application Support 的 OrbDuel/OrbDuel/mods，回游戏工坊重载。跨系统分享工程源码重新构建，Windows 用 DLL，Mac 用 dylib。Mac 平台适配本次未做编译和实机验证，完整说明在游戏 docs/MACOS.md。

## C++ 自建能力

生成器还支持 ability、rewind、guardian：分别为受伤保护作者起步模板、时间回溯、护卫召唤。修改 my_mod.cpp 的 step/event/draw 写自己的算法，修改 mod.json 的 defaults 调整已声明参数，构建安装后回工坊重载。在编辑器放置后选择附着的角色。

include/orbduel_abilities.hpp 提供冷却、范围/队伍查询、移动以及本能力创建资源的自动清理。护卫控制器是 runtime-only 类型，由召唤能力绑定，不在编辑器直接放置。完整教程见游戏 docs/CUSTOM_ABILITIES.md。

在 Windows 上，一键构建默认使用 Visual Studio 2022。若已安装其他受 CMake 支持的 Visual Studio 版本，可在当前 PowerShell 会话设置 `$env:CMAKE_GENERATOR` 后运行 `build-install.cmd`，例如 `$env:CMAKE_GENERATOR = 'Visual Studio 18 2026'`。已有构建目录继续使用原来的生成器；切换前先移走该目录。
