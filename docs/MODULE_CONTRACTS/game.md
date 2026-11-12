# 契约：game

**Owner**：B　**依赖**：全部　**被依赖**：无（可执行入口）

## 职责

- 解析命令行（`--seed` / `--char` / `--enemy` / `--map` / `--no-menu` / `--headless` / `--seconds` / `--screenshot` / `--lang` / `--selftest`）
- 装配：`ContentLoader` → `World` → `Match` → `Renderer` → `Fx`
- 主循环：帧计时 → `Clock` → 固定步推进 → 渲染
- 场景切换：主菜单（`Menu`）/ 对局 / 结算；结算 5 秒后自动回主菜单
- 读 `hit_stop` 状态决定本帧是否跳过物理推进

## 不负责

- 不实现游戏规则（全部委托 `match`）
- 不实现渲染细节（委托 `render`）
- 不解析内容（委托 `content`）

## 公开接口

| 文件 | 内容 |
|---|---|
| `game/Game.hpp` | `Game::run(const Options&)`、`Options`（含 `startInMenu` / `headless` / `language`） |
| `game/Menu.hpp` | `Menu::open/update/draw/...`；`draw(..., bool chineseLabels)` 决定名字用中文还是英文 |
| `game/Input.hpp` | `Input::sample()`；现阶段返回全零，留接口 |

## 不变量

1. 主循环里**物理步数只由 `Clock` 决定**，渲染帧率不影响结果。
2. 场景切换不重建 `ContentLoader`（热重载跨场景保持）。
3. `--autotest` 不创建窗口，输出机器可读的一行汇总（碰撞数、最长静默、总伤害、用时）——**未实现**，现在用 `--headless --seconds N` + 日志回归。
4. `Options::seed == 0` 时用地图 `seed`；地图也没有才用时间戳，并打印实际 seed。
5. 退出码：`0` 正常，`2` 初始化失败，`3` 内容致命错误。
6. 菜单帧**不推进** `Clock` 与物理：`Menu::update/draw` 只在主菜单状态下调用，菜单停留时间不计入对局。
7. `ESC` 不退出进程：对局内回主菜单，菜单内只关闭「退出确认框」。退出只有两条路——菜单里 `QUIT GAME` → `YES`，或关闭窗口。
8. `--headless` 一律不进菜单（没有输入来源，停在菜单等于空转）；`--no-menu` 直接开局；`--char/--enemy` 只作为菜单的预选项。
9. 左右两侧各自选角，允许同一个角色互打；同角色时右侧整体偏蓝以便区分。
10. `Options::language` 默认 `"en"`：角色名/技能名画英文；`"zh"`（`--lang zh`）且系统有中文字体时才画中文。字体始终加载，缺字体只降级文字，不报错。

## 禁止

- 在 `Game` 里写伤害公式、碰撞公式
- 用 `std::this_thread::sleep` 控制帧率（用 `SDL_RENDERER_PRESENTVSYNC` 或 `SDL_GetTicks`）
- 把 `main()` 写成 300 行

## 验收

- 干净 clone + `build.ps1` + 双击 exe 能跑
- 主菜单可点：开始游戏 / 退出（确认框）/ 左右各切角色 / 结算 5 秒自动回菜单
- `--autotest 600` 在 10 秒内跑完（120 Hz × 600 s 的物理，允许加速）——**未实现**：`Options::autotestSeconds` 还没有解析入口
- 关闭窗口不崩溃
