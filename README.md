# OrbDuel / Mod Studio

C++17 + SDL2 自动物理对战与地图创作工具。通过独立 C++ DLL 新增能力，在地图编辑器中调参数，再通过离线分享码还原地图和规则。

OrbDuel is an auto-battler and arena workshop: native mods register new behaviors without recompiling the game, arenas are tuned in a live editor, and the finished layout is shared offline as text.

![地图编辑器](docs/previews/cosmos.png)

## 运行与构建

Windows x64；开发需要 Visual Studio 2022 / Build Tools 的“使用 C++ 的桌面开发”及 CMake。SDL2 和 Dear ImGui 已随仓库提供，不需要在线下载依赖。只支持 MSVC 2022 x64；`third_party/sdl2/mingw` 为历史遗留，不参与构建。

```powershell
.\build.ps1
.\build\bin\orbduel.exe
```

`build.ps1` 会自动寻找 VS 附带的 CMake，默认构建 Release 游戏与七个独立示例模组。手动构建：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Apple Silicon 的构建、启动和用户模组安装见 [macOS 指南](docs/MACOS.md)。当前提供 macOS 适配源码，编译和实机运行待验证。

## 创作流程

1. 打开“地图编辑器”，输入种子并生成场地。
2. 选择矩形、圆形障碍或模组机关，点击画布放置；拖动对象调整位置。
3. 选中传送门，在右侧选择关联出口、出口方向和速度倍率；其他机关也会自动显示自己的参数。
4. 调整角色、技能、时间上限、缩圈和风格，点击“测试地图”。
5. 返回编辑器继续修改；测试不会改写地图草稿。
6. 保存地图，或打开“分享码”复制完整配置。对方安装相同的模组文件夹后离线导入。

快捷键：`Ctrl+S` 保存、`Ctrl+Z/Y` 撤销/重做、`Delete` 删除选中机关或障碍；`Esc` 从对战返回编辑器或取消当前选择。

五套程序化风格：星空、地下城、霓虹都市、熔岩遗迹、冰晶圣殿；背景、边界和障碍配色随风格变化，装饰密度与动画可调，风格不改变物理规则。界面皮肤与地图风格分开设置，可在深夜与极光之间切换，选择保存在 `user/settings.json`；皮肤文件在 `content/ui/skins/`，复制一份改颜色、圆角、间距和字号，重新加载后出现在列表中，也可用 `--ui-skin aurora` 在启动时指定。无效配置会报错并保留当前皮肤，格式见[内容与工作台格式](docs/CONTENT_FORMAT.md)。

工作台使用随 `content/fonts/` 提供的 Noto Sans SC 显示中英文，无需系统安装微软雅黑；发布包会复制这份字体和 SIL OFL 许可证。字体缺失时仍可启动，但会提示错误，中文可能显示异常。

## 模组不只是调数值

游戏通过 `orbduel_register` 加载独立 DLL。传送门、引力场、弹射板的逻辑均在模组中，宿主通过统一接口读取类型与参数。模组可以查询实体、改变速度和位置、生成/移除机关、处理事件、自定义绘制；也可以附着在角色上，或创建独立战斗实体。

普通玩家只改界面参数；作者从 `mod-sdk/template` 的旋风模组开始，编写新的 C++ 行为：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\mod-sdk\new-mod.ps1 -Id my-vortex -Name "我的旋风"
```

打开生成的 `user/mod-projects/my-vortex`，修改 `mod.json`，双击 `build-install.cmd`；首次选择目标游戏，后续会记住位置。游戏运行时也能安装，回到模组工坊重新加载后，新放置的对象使用新默认参数。

[用 C++ 自建能力](docs/CUSTOM_ABILITIES.md) 介绍如何从作者模板写一个新行为，以及如何用回溯与护卫示例测试战斗实体。

## 文档

- [快速上手](docs/QUICK_START.md)（发布包内为“开始使用.md”）
- [第一个模组：从复制到安装](docs/FIRST_MOD.md)
- [模组与地图工作台](docs/MOD_STUDIO.md)
- [战斗实体与分身模组](docs/COMBAT_ENTITIES.md)
- [地图与分享格式](docs/CONTENT_FORMAT.md)
- [当前架构](docs/ARCHITECTURE.md)
- [平衡记录与复现方法](docs/BALANCE.md)
- [实际验证记录](docs/STATUS.md)
- [演示流程](docs/DEMO_SCRIPT.md)
- [公共 SDK](mod-sdk/include/orbduel_mod.hpp)

## 九个角色与十六个技能

八个竞技角色：疾锋（Lancer，蓄力与贯穿）、黑洞（Singularity，质量成长与核心传送）、烈焰、磐石、霜棱、雷脉、青芽、逐风；另有训练用木人桩，不参与竞技统计。

技能覆盖冲刺吸血、护盾反伤、反弹机动、领域追击、过载爆发与恢复续航。主页可直接选择角色与应用预设；对局显示技能冷却、生效状态和实际治疗量。

八个竞技角色已完成 28672 局独立种子验证，总体得分率 45.16%–54.43%，各地图 26.37%–64.73%，结果与复现方法见[平衡记录](docs/BALANCE.md)。

## 对战规则

两球自动移动。普通碰撞伤害 = 地图倍率 × 挨撞角色 `hitbox.contact`（缺省 1），再按攻击方碰撞前朝向对手的速度加成，叠加连击、攻击增益、护甲与护盾；蓄力撞击、贯穿、引力和模组伤害独立结算，ghost 本身不提供免伤。

工作台默认一分钟上限，超时比较剩余血量比例；缩圈和防僵局可开关，场地缩到无法容纳角色时按原始 HP 结算。技能与数值都能在编辑器中修改，自定义模组的平衡仍需试玩调整。完整字段与公式见[内容与工作台格式](docs/CONTENT_FORMAT.md)，设计取舍见[决策记录](docs/DECISIONS.md)。

## 命令行

```powershell
.\build\bin\orbduel.exe --selftest --window
.\build\bin\orbduel.exe --headless --seconds 60
.\build\bin\orbduel.exe --autotest 600
.\build\bin\orbduel.exe --scene user/arena.json --play
.\build\bin\orbduel.exe --no-menu --map duel-basic
.\build\bin\orbduel.exe --mods path/to/mods
.\build\bin\orbduel.exe --lang en
.\build\bin\orbduel.exe --ui-skin aurora
.\build\bin\orbduel.exe --sfx-test
```

`--headless` 不创建窗口，按固定步长推进；`--autotest` 连续跑多局，输出碰撞、机关触发和最长静默时间；`--hidden --view editor --theme cosmos --screenshot out.png` 生成截图；`--sfx-test` 按文件名顺序试听 `content/audio/orbduel` 的 WAV。`--lang en|zh` 切换工作台、角色、技能与说明文字，默认英文，内置模组参数随之切换，自制模组显示作者提供的文字。

发布包用 `tools/package.ps1` 生成，包含游戏、模组、资源、SDK 和教程；双击包里的游戏即可运行。

## 许可证

项目代码 MIT。SDL2 系列为 zlib，Dear ImGui 为 MIT，内置字体为 SIL OFL，许可证随仓库和发布包提供。程序化场景由本项目绘制；素材来源见 `content/ASSETS.md`。
