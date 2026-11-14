# 我的第一个模组

需要 Windows x64、Visual Studio 2022 / Build Tools 的 C++ 桌面开发和 CMake 工具。

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
