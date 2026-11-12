# 实现状态 / Status

> **每次合并前更新这张表。** AI 开工第一件事是读这里，别猜哪些已经能跑。
> 最后更新：2026-11-13（主菜单：开始 / 退出 / 左右各自选角）

## 总览

| 模块 | 接口 | 实现 | 说明 |
|---|---|---|---|
| `core/Types` | ✅ | ✅ | Vec2/Rgba/Rect 与内联数学 |
| `core/Clock` | ✅ | ✅ | 固定步长累加器（1/120，单帧最多 8 步） |
| `core/Rng` | ✅ | ✅ | xorshift64*，同 seed 可复现 |
| `core/Event` | ✅ | ✅ | 队列式事件总线，步末统一派发 |
| `core/Log` | ✅ | ✅ | 控制台 + 最近 16 行缓冲 |
| `physics/Body` | ✅ | ✅ | 结构体 |
| `physics/Arena` | ✅ | ✅ | 缩圈插值（内缩 0–1，可缩到 0）+ 越界推回 + 障碍压碎 |
| `physics/World` | ✅ | ✅ | 积分 / 球-球 / 障碍 / 墙反弹（带抖动） |
| `physics/Collision` | ✅ | ✅ | 圆-圆、圆-矩形、圆-静态圆（解析解） |
| `render/Renderer` | ✅ | ✅ | 窗口 / 画圆 / 线 / 矩形 / 精灵 / TTF 文本 / 截图 |
| `render/AssetCache` | ✅ | ✅ | PNG 懒加载 + 热重载接口 |
| `render/Fx` | ✅ | ✅ | 粒子 / 圆环 / 拖尾 / 震屏 / hit-stop（事件驱动） |
| `render/DebugDraw` | ✅ | ❌ | 头文件在，实现未写 |
| `content/DataModel` | ✅ | ✅ | 纯数据结构 |
| `content/ContentLoader` | ✅ | 🟡 | 解析 + 校验能跑；**热重载未接**（`poll()` 是空实现） |
| `match/Match` | ✅ | ✅ | HP、伤害、连击、死亡、结算 |
| `match/MatchRule` | ✅ | ✅ | `IMatchRule` + `HpRule` |
| `match/StallGuard` | ✅ | ✅ | L2 静默吸引（L1 抖动在 physics） |
| `match/Zone` | ✅ | ✅ | 缩圈调度（时间触发，带预警）；缩到 0 时由 `Match` 结算 |
| `match/Skill` | ✅ | 🟡 | 蓄力冲刺（`charge`）全通；`add_speed` 仍生效；其余原语待接 |
| `game/Game` | ✅ | ✅ | 主循环、装配、HUD、结算、菜单↔对局切换、蓄力虚线、技能名飘字 |
| `game/Menu` | ✅ | ✅ | 主菜单：开始 / 退出（确认框）/ 左右各自选角，读 `content/characters/` 不写死 |
| 内容包 | — | ✅ | 2 角色 / 4 技能 / 1 地图 / 1 字体 |

图例：✅ 完成 / 🟡 部分 / ❌ 未开始

## 现在能玩什么

```powershell
.\build\bin\orbduel.exe                 # 开主菜单（点开始游戏 / 按 Enter 进对局）
.\build\bin\orbduel.exe --no-menu       # 跳过菜单直接开局
.\build\bin\orbduel.exe --seed 42       # 固定随机种子，可复现
.\build\bin\orbduel.exe --char lancer --enemy bulwark --map duel-basic   # 参数只做菜单预选
.\build\bin\orbduel.exe --seconds 10    # 跑 10 秒自动退出
.\build\bin\orbduel.exe --screenshot out.png   # 第 2 秒截图后退出（在菜单里就截菜单）
```

- 启动先进主菜单：左右两张卡各自用 `<` `>` 切换角色（可以选同一个角色互打），`START GAME` 或 Enter 开局，`QUIT GAME` 弹确认框（点 No / 点框外 / ESC 取消）；
- **碰撞已经不掉血**（`damageScale = 0`），伤害只来自技能与缩圈；
- 默认角色 Lancer（疾锋）的 Dash：冷却好（6 s）自动原地蓄力 3 s，地上画一条虚线指向「0.3 s 前的你」，蓄满后沿虚线以 500 冲出去，撞到对手造成一次额外伤害（≈ 0.12 × 相对速度）；
- Lancer 是橙色瘦长三角（`skin.shape: "triangle"`，**碰撞仍是半径 24 的圆**）：尖角朝速度方向、转向限速 20 rad/s，蓄力时精确对齐虚线，虚线从尖角出发；主菜单头像用的是同一套画法；
- Dash 会**越放越强**（本局累计，重开一局重学）：虚线是「慢慢飘」的（每步约 0.08°），索敌第 5 次最准（延迟 0.3 s → 0.05 s、抖动 6° → 0.5°），蓄力时间第 10 次归零（3 s → 0 s，瞬发），第 15 次起**打不断**；
- 任何技能施放时球上方弹出技能英文名，0.9 s 上浮淡出；
- 25 秒后开始缩圈（每 10 s 一档，每档 10%，带 1 秒预警），场地一路缩到 0；装不下两个球的那一步立即结算，**HP 高者胜**，同 HP 平局；
- 5 秒没碰撞会触发「静默吸引」，球会缓慢靠近（肉眼几乎看不出）；
- HP 归零或场地归零 → 结算画面，显示 `RETURNING IN 5...`；期间按 `R` 重开同阵容，5 秒后自动回主菜单；
- 对局中 `ESC` 回主菜单（**ESC 不再退出进程**）；退出只能在菜单里点 `QUIT GAME` → `YES`。

![screenshot](screenshot.png)

## 验证记录（2026-11-13，蓄力冲刺 + 缩圈到 0）

| 项 | 结果 | 备注 |
|---|---|---|
| 全量源码编译（21 个 `.cpp`） | ✅ | `-Wall -Wextra` 零警告 |
| 蓄力自检 `build-verify/skill_probe.exe` | ✅ | 17 项全过：蓄力/打断/冲速/一次重击/窗口过期/虚线每步 0.082°/误差 4.98°→1.58°/蓄力时长 0.517 s→0.008 s/放熟后打不断 |
| `tools/validate_content.ps1` | ✅ | `2 characters, 4 skills, 1 maps` |
| `--headless --seconds 12` | ✅ | 无崩溃；`hits=2`，伤害只来自冲刺（碰撞已关） |
| 场地缩到 0 的结算 | ✅ | 临时把 `firstAt/interval` 改成 1/1 跑 25 s：20 s 时 `phase=2`，HP 高者胜，之后已还原 |
| 虚线 + 技能名飘字 | ✅ | 截图验收：蓄力时虚线从球指向「延迟锁定点」并逐帧抖动；施放后英文名贴在球上方上浮淡出 |
| 真实节奏长跑 60 s | ✅ | `charge start` / `charge release` 成对出现（每 6 s 一轮，基本都能放出来），无崩溃 |

## 验证记录（2026-11-13，主菜单）

| 项 | 结果 | 备注 |
|---|---|---|
| 全量源码编译（21 个 `.cpp`） | ✅ | `-Wall -Wextra` 零警告，14.3 s |
| 主菜单渲染 | ✅ | 1280×720：两张卡 + 头像 + 数值 + 技能名 + 按钮 |
| 菜单点击自检（`SDL_PushEvent` 模拟鼠标） | ✅ | 12 项全过：开始 / 左右切换 / 循环回绕 / 确认框点框外取消 / YES 退出 / ESC 取消 / Enter 开局 / 点空白无反应 |
| `--headless` 默认跳过菜单 | ✅ | 与改动前一致：`hits=1 hp0=78 hp1=118` |
| `--no-menu --headless --seconds 10` | ✅ | 同上，回归无变化 |
| 长跑 120 s | ✅ | 55 s 结算 → 5 s 倒计时 → 回菜单，无崩溃 |

## 验证记录（2026-11-09）

| 项 | 结果 | 备注 |
|---|---|---|
| 全量源码编译 | ✅ | `-Wall -Wextra` 零警告 |
| CMake + Ninja 构建 | ✅ | 23 个构建目标，`build/bin/orbduel.exe` |
| `--selftest` | ✅ | 退出码 0 |
| `--selftest --window` | ✅ | `video=windows, renderer=direct3d` |
| 无窗口跑完整一局 | ✅ | 55 s，7 次命中，正常结算 |
| 截图渲染 | ✅ | 1280×720，HUD/精灵/粒子/障碍都在 |
| 内容校验脚本 | ✅ | `2 characters, 4 skills, 1 maps` |

## 下一个该谁做什么

| 优先级 | 任务 | 负责人 | 依赖 |
|---|---|---|---|
| P0 | 热重载：`ContentLoader::poll()` + `AssetCache` 重载 + 屏幕报错 | C | 无 |
| P0 | 技能剩余原语：shield / lifesteal / slow_field / teleport / resize / bounce_boost | C | `Match` 上下文 |
| P1 | `DebugDraw`（F1 碰撞箱 / F2 场地 / F3 事件） | B | 无 |
| P1 | 缩圈压到球的持续伤害（`damagePerSec`，不计连击） | C | 无 |
| P2 | 角色/地图选择界面 | B | 热重载 |
| P2 | 碰撞箱偏移 `hitbox.offset` 接进物理 | A | 无 |

## 已知缺口

- 技能里只有「蓄力冲刺」和 `add_speed` 真的生效，其余原语是空的（会在日志里提示一次）。
- **Bulwark 目前没有任何伤害来源**：碰撞伤害已关，它的护盾 / 减速都还没实现，只能等缩圈把对手耗死。
- 冲刺一次大约 60–100 伤害（0.12 × 相对速度），两下就能打死 140 HP 的对手——数值还没调。
- 没有热重载；改 JSON 要重启。
- 缩圈边界压到球的持续伤害（`damagePerSec`）仍未接。
- 没有单元测试；物理回归靠 `--headless --seconds N` + 日志，技能机制靠 `build-verify/skill_probe.exe`（不在仓库里）。
- `hitbox.offset` 还没进物理（现在只按半径算）。
- 没有音效。
- 主菜单不显示中文名：当前字体 PressStart2P 没有 CJK 字形，`name.zh` 会渲染成方框；要显示得换一个带中文的 TTF。
- 菜单只能用鼠标操作（键盘只有 Enter 开局、ESC 关确认框），没有方向键导航。
- 菜单不做地图选择，地图仍由 `--map` 决定。
