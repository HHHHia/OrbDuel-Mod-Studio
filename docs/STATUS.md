# 实现状态 / Status

> **每次合并前更新这张表。** AI 开工第一件事是读这里，别猜哪些已经能跑。
> 最后更新：2026-11-09（MVP 可玩版：两球对撞 + HP + 缩圈 + 粒子）

## 总览

| 模块 | 接口 | 实现 | 说明 |
|---|---|---|---|
| `core/Types` | ✅ | ✅ | Vec2/Rgba/Rect 与内联数学 |
| `core/Clock` | ✅ | ✅ | 固定步长累加器（1/120，单帧最多 8 步） |
| `core/Rng` | ✅ | ✅ | xorshift64*，同 seed 可复现 |
| `core/Event` | ✅ | ✅ | 队列式事件总线，步末统一派发 |
| `core/Log` | ✅ | ✅ | 控制台 + 最近 16 行缓冲 |
| `physics/Body` | ✅ | ✅ | 结构体 |
| `physics/Arena` | ✅ | ✅ | 缩圈插值 + 越界推回 + 障碍压碎 |
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
| `match/Zone` | ✅ | ✅ | 缩圈调度（时间 + 静默触发，带预警） |
| `match/Skill` | ✅ | 🟡 | 冷却 + `add_speed` 生效；其余原语待接 |
| `game/Game` | ✅ | ✅ | 主循环、装配、HUD、结算、重开 |
| 内容包 | — | ✅ | 2 角色 / 4 技能 / 1 地图 / 1 字体 |

图例：✅ 完成 / 🟡 部分 / ❌ 未开始

## 现在能玩什么

```powershell
.\build\bin\orbduel.exe                 # 开一局（自动对战，无玩家输入）
.\build\bin\orbduel.exe --seed 42       # 固定随机种子，可复现
.\build\bin\orbduel.exe --char blaze --enemy bulwark --map duel-basic
.\build\bin\orbduel.exe --seconds 10    # 跑 10 秒自动退出
.\build\bin\orbduel.exe --screenshot out.png   # 第 2 秒截图后退出
```

- 两球自动对撞，撞击按相对速度扣血，2 秒内连击有倍率；
- 25 秒后开始缩圈（每 12 秒一档，带 1 秒预警），场地缩小、障碍被压碎；
- 5 秒没碰撞会触发「静默吸引」，球会缓慢靠近（肉眼几乎看不出）；
- HP 归零 → 结算画面，按 `R` 重开，`ESC` 退出。

![screenshot](screenshot.png)

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

- 技能只有 `add_speed` 真的生效，其余原语是空的（会在日志里提示一次）。
- 没有热重载；改 JSON 要重启。
- 缩圈只推回球，还没做「压到球扣血」。
- 没有单元测试；物理回归靠 `--headless --seconds N` + 日志。
- `hitbox.offset` 还没进物理（现在只按半径算）。
- 没有音效。
