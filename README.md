# OrbDuel / Mod Studio

C++17 + SDL2 自动物理对战与地图创作工具。通过独立 C++ DLL 新增能力，在地图编辑器中调参数，再通过离线分享码还原地图和规则。

A C++17 physics duel and offline arena workshop. Native C++ mods register new behaviors without recompiling the game. Edit their parameters, generate an arena from a seed, and share the complete edited layout offline.

![地图编辑器](docs/previews/cosmos.png)

## 运行与构建

Windows x64；开发需要 Visual Studio 2022 / Build Tools 的“使用 C++ 的桌面开发”及 CMake。SDL2 和 Dear ImGui 源码/运行库已随仓库提供，不需要在线下载依赖。

```powershell
.\build.ps1
.\build\bin\orbduel.exe
```

`build.ps1` 会自动寻找 VS 附带的 CMake。默认构建 Release 游戏与三个独立示例模组。

手动构建：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

本轮原生插件版本统一使用 MSVC x64，不再声称支持原来的 MinGW 构建。

## 创作流程

1. 打开“地图编辑器”，输入种子并生成场地。
2. 选择矩形、圆形障碍或模组机关，点击画布放置；拖动对象调整位置。
3. 选中传送门，在右侧选择关联出口、出口方向和速度倍率；其他机关也会自动显示自己的参数。
4. 调整角色、技能、时间上限、缩圈和风格，点击“测试地图”。
5. 返回编辑器继续修改；测试不会改写地图草稿。
6. 保存地图，或打开“分享码”复制完整配置。对方安装相同的模组文件夹后离线导入。

快捷键：`Ctrl+S` 保存、`Ctrl+Z/Y` 撤销/重做、`Delete` 删除选中机关或障碍；`Esc` 从对战返回编辑器或取消当前选择。

五种风格：星空、地下城、霓虹都市、熔岩遗迹、冰晶圣殿。背景、边界和障碍配色随风格变化；可以调整装饰密度、动画和自定义配色。风格不改变物理规则。

## 模组不只是调数值

游戏通过 `orbduel_register` 加载独立 DLL。传送门、引力场、弹射板的逻辑均在模组中，宿主通过统一接口读取类型与参数。模组可以查询实体、改变速度和位置、生成/移除机关、处理事件、自定义绘制；也可以附着在角色上。

普通玩家只改界面参数；作者从 `mod-sdk/template` 的旋风模组开始，编写新的 C++ 行为。

```powershell
cmake -S mod-sdk/template -B build-template -A x64
cmake --build build-template --config Release
```

把 `build-template/package/my-mod/` 整个文件夹复制到 `build/bin/mods/`，在“模组工坊”点击重新加载，即可在编辑器放置旋风。游戏不需要重新编译。

- [新手与作者教程](docs/MOD_STUDIO.md)
- [公共 SDK](mod-sdk/include/orbduel_mod.hpp)
- [地图与分享格式](docs/CONTENT_FORMAT.md)
- [当前架构](docs/ARCHITECTURE.md)
- [实际验证记录](docs/STATUS.md)

## 九角色与十六技能

新增逐风、霜棱、雷脉和青芽，搭配冲刺吸血、护盾反伤、反弹机动、领域追击、过载爆发和恢复续航。主页可直接选择角色与应用预设；对局显示技能冷却、生效状态和实际治疗量。

同时保留主分支的疾锋（Lancer）、黑洞（Singularity）和木人桩，包括蓄力学习、贯穿、可穿入身体、质量与引力成长、核心传送和万向天引。三者的角色文件、四个技能文件及基础地图保持主分支原值。原瞬时加速改名为 `burst-dash`，队友的蓄力技能继续使用 `dash`。

整合版完成 9216 局跨场地、交换位置回归；当前角色强弱尚未统一调整，旧六角色报告作为历史记录保留。结果和复现方法见 [平衡记录](docs/BALANCE.md)。旧地图保存的自定义数值不被覆盖，需要时点击“应用预设”。

## 当前对战规则

两球自动移动；碰撞伤害保留主分支的相对速度与连击规则，系数取地图倍率和挨撞角色 `hitbox.contact` 的较大值，再应用攻击、攻击增益、护甲和护盾。普通身体的六个工作台角色设置 contact=1；疾锋、黑洞和木人桩保留原值。蓄力撞击、贯穿和引力伤害独立结算；万向天引期间黑洞无敌，双方普通碰撞伤害关闭。

工作台默认一分钟上限，超时比较剩余血量比例；缩圈和防僵局可开关。场地缩到无法容纳角色时按原始 HP 结算。`--no-menu --map duel-basic` 使用基础地图原有布局、缩圈与无时限规则，不额外生成机关；`--lang en|zh` 选择角色与技能名称语言。

技能效果会影响胜负，但任意自定义技能或新模组的平衡仍需要试玩调整；编辑器中可以直接修改生命、攻击、护甲、速度、质量及技能。

## 命令行

```powershell
.\build\bin\orbduel.exe --selftest --window
.\build\bin\orbduel.exe --headless --seconds 60
.\build\bin\orbduel.exe --autotest 600
.\build\bin\orbduel.exe --scene user/arena.json --play
.\build\bin\orbduel.exe --mods path/to/mods
.\build\bin\orbduel.exe --sfx-test
```

`--headless` 真正跳过 SDL 初始化并快速推进逻辑。`--autotest` 连续运行多局，输出碰撞、机关触发和最长静默时间。截图可使用 `--hidden --view editor --theme cosmos --screenshot out.png`。`--sfx-test` 不开窗口，按文件名顺序把 `content/audio/orbduel` 的 WAV 各放一遍，用来确认声卡和素材。

发布包使用 `tools/package.ps1`，包含游戏、模组、资源、SDK 和教程；双击其中的游戏即可运行。

## 团队 / Team

CS2310 group08（CityU，2026 秋），6 人。

| 姓名 / Name | Student ID |
| ----------- | ---------- |
| LEE Pak Ho  | 60556000   |
| （待填）    | （待填）   |
| （待填）    | （待填）   |
| （待填）    | （待填）   |
| （待填）    | （待填）   |

## 可以接着做 / Next tasks

当前版本可构建、可运行。下面是尚未完成、适合分工认领的部分；做完一条在 commit 里说明即可。

**不用写代码（改内容或跑测试）**

| 任务         | 说明                                                                                      |
| ------------ | ----------------------------------------------------------------------------------------- |
| 加地图       | 编辑器里摆好后存进 `content/studio/`；仓库目前只有 `content/maps/duel-basic` 一张内置地图 |
| 加角色或技能 | 复制 `content/characters/blaze/` 改 JSON，再跑 `tools/validate_content.ps1`               |
| 手动验收     | 逐页检查概览、编辑器、模组工坊、对战、保存/读档、分享码导入，记下卡住的地方               |
| 补演示脚本   | [DEMO_SCRIPT.md](docs/DEMO_SCRIPT.md) 目前 5 步，可扩成能照着念的完整稿                   |
| 更新预览图   | 五套风格与蓄力、黑洞的截图是旧的，内容改动后需要重截                                      |

**要动代码**

| 任务            | 说明                                                                                 | 难度 |
| --------------- | ------------------------------------------------------------------------------------ | ---- |
| 全屏 / 窗口切换 | 窗口固定 1480×900，加 F11 或启动参数即可                                             | 低   |
| 界面中英文      | 界面文字目前写死在 `src/game/Studio.cpp`；`--lang en` 只换角色与技能名，界面仍是中文 | 中   |
| 界面美化        | 菜单配色、圆角、字号写在 `style()` 里，改为读取 `content/` 下的配置后可做皮肤        | 中   |
| 自带中文字体    | 目前依赖系统的 `msyh.ttc`，换机器可能缺字                                            | 低   |
| 音效            | 尚无声音；[DECISIONS.md](docs/DECISIONS.md) 的缩圈预警提到音效，未实现               | 高   |
| 平衡调参        | 九角色强弱差异大，现成回归工具与数据见 [BALANCE.md](docs/BALANCE.md)                 | 中   |
| 发布包自检      | 跑 `tools/package.ps1`，在仓库外双击运行，确认 DLL、字体、模组齐全                   | 低   |

建议的认领顺序：手动验收 → 全屏切换 → 界面中英文。

## 许可证

项目代码 MIT。SDL2 系列为 zlib，Dear ImGui 为 MIT，内置字体为 SIL OFL。对应许可证随仓库和发布包提供。程序化场景由本项目绘制；素材来源见 `content/ASSETS.md`。
