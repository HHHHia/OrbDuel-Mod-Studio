# 契约：game

**Owner**：B　**依赖**：全部　**被依赖**：无（可执行入口）

## 职责

- 解析命令行（`--seed` / `--char` / `--enemy` / `--map` / `--autotest` / `--selftest`）
- 装配：`ContentLoader` → `World` → `Match` → `Renderer` → `Fx`
- 主循环：帧计时 → `Clock` → 固定步推进 → 渲染
- 场景切换：标题 / 选角 / 对局 / 结算
- 读 `hit_stop` 状态决定本帧是否跳过物理推进

## 不负责

- 不实现游戏规则（全部委托 `match`）
- 不实现渲染细节（委托 `render`）
- 不解析内容（委托 `content`）

## 公开接口

| 文件 | 内容 |
|---|---|
| `game/Game.hpp` | `Game::run(const Options&)`、`Options`、`Game::requestScene(...)` |
| `game/Input.hpp` | `Input::sample()`；现阶段返回全零，留接口 |

## 不变量

1. 主循环里**物理步数只由 `Clock` 决定**，渲染帧率不影响结果。
2. 场景切换不重建 `ContentLoader`（热重载跨场景保持）。
3. `--autotest` 不创建窗口，输出机器可读的一行汇总（碰撞数、最长静默、总伤害、用时）。
4. `Options::seed == 0` 时用地图 `seed`；地图也没有才用时间戳，并打印实际 seed。
5. 退出码：`0` 正常，`2` 初始化失败，`3` 内容致命错误。

## 禁止

- 在 `Game` 里写伤害公式、碰撞公式
- 用 `std::this_thread::sleep` 控制帧率（用 `SDL_RENDERER_PRESENTVSYNC` 或 `SDL_GetTicks`）
- 把 `main()` 写成 300 行

## 验收

- 干净 clone + `build.ps1` + 双击 exe 能跑
- `--autotest 600` 在 10 秒内跑完（120 Hz × 600 s 的物理，允许加速）
- 关闭窗口不崩溃
