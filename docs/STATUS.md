# 实现状态 / Status

> **每次合并前更新这张表。** AI 开工第一件事是读这里，别猜哪些已经能跑。
> 最后更新：2026-11-13（黑洞定稿：只挨撞、疾锋 150 血）

## 总览

| 模块 | 接口 | 实现 | 说明 |
|---|---|---|---|
| `core/Types` | ✅ | ✅ | Vec2/Rgba/Rect 与内联数学 |
| `core/Clock` | ✅ | ✅ | 固定步长累加器（1/120，单帧最多 8 步） |
| `core/Rng` | ✅ | ✅ | xorshift64*，同 seed 可复现 |
| `core/Event` | ✅ | ✅ | 队列式事件总线，步末统一派发 |
| `core/Log` | ✅ | ✅ | 控制台 + 最近 16 行缓冲 |
| `physics/Body` | ✅ | ✅ | 结构体（含 `ghost` 穿透开关） |
| `physics/Arena` | ✅ | ✅ | 缩圈插值（内缩 0–1，可缩到 0）+ 越界推回 + 障碍压碎 |
| `physics/World` | ✅ | ✅ | 积分 / 球-球 / 障碍 / 墙反弹（带抖动） |
| `physics/Collision` | ✅ | ✅ | 圆-圆、圆-矩形、圆-静态圆（解析解） |
| `render/Renderer` | ✅ | ✅ | 窗口 / 画圆 / 线 / 矩形 / 精灵 / TTF 文本 / 截图 |
| `render/AssetCache` | ✅ | ✅ | PNG 懒加载 + 热重载接口 |
| `render/Fx` | ✅ | ✅ | 粒子 / 圆环 / 拖尾 / 震屏 / hit-stop（事件驱动） |
| `render/DebugDraw` | ✅ | ❌ | 头文件在，实现未写 |
| `content/DataModel` | ✅ | ✅ | 纯数据结构 |
| `content/ContentLoader` | ✅ | 🟡 | 解析 + 校验能跑；**热重载未接**（`poll()` 是空实现） |
| `match/Match` | ✅ | ✅ | HP、伤害、连击、死亡、结算、引力/万向天引/传送 |
| `match/MatchRule` | ✅ | ✅ | `IMatchRule` + `HpRule` |
| `match/StallGuard` | ✅ | ✅ | L2 静默吸引（L1 抖动在 physics） |
| `match/Zone` | ✅ | ✅ | 缩圈调度（时间触发，带预警）；缩到 0 时由 `Match` 结算 |
| `match/Skill` | ✅ | 🟡 | `charge` / `pierce` / `add_speed` / `gravity` / `omnipull` 生效；其余原语待接 |
| `game/Game` | ✅ | ✅ | 主循环、装配、HUD、结算、菜单↔对局切换、蓄力虚线、技能名飘字 |
| `game/Menu` | ✅ | ✅ | 主菜单：开始 / 退出（确认框）/ 左右各自选角，读 `content/characters/` 不写死 |
| 内容包 | — | ✅ | 3 角色 / 7 技能 / 1 地图 / 1 字体 |

图例：✅ 完成 / 🟡 部分 / ❌ 未开始

## 现在能玩什么

```powershell
.\build\bin\orbduel.exe                 # 开主菜单（点开始游戏 / 按 Enter 进对局）
.\build\bin\orbduel.exe --no-menu       # 跳过菜单直接开局
.\build\bin\orbduel.exe --seed 42       # 固定随机种子，可复现
.\build\bin\orbduel.exe --char lancer --enemy singularity --map duel-basic   # 参数只做菜单预选
.\build\bin\orbduel.exe --seconds 10    # 跑 10 秒自动退出
.\build\bin\orbduel.exe --screenshot out.png   # 第 2 秒截图后退出（在菜单里就截菜单）
.\build\bin\orbduel.exe --screenshot late.png --shot-at 12   # 第 12 秒截图（看得到大招爆炸）
.\build\bin\orbduel.exe --lang zh       # 角色/技能名显示中文（默认英文）
```

- 启动先进主菜单：左右两张卡各自用 `<` `>` 切换角色（三个角色可选，可以选同一个角色互打），`START GAME` 或 Enter 开局，`QUIT GAME` 弹确认框（点 No / 点框外 / ESC 取消）；
- **碰撞本身不掉血**（`duel-basic` 的 `damageScale = 0`），伤害只来自技能与缩圈；
- **Lancer（疾锋，150 血）**：橙色瘦长三角（`skin.shape: "triangle"`，**碰撞仍是半径 24 的圆**），尖角朝速度方向、转向限速 20 rad/s，蓄力时精确对齐虚线；
  Dash 冷却 6 s，原地蓄力 3 s 后沿一条会抖的虚线以 500 冲出去，撞到对手补一次额外伤害（≈ 0.12 × 相对速度）；
  被动「贯穿」：自身速度 ≥ 200 时撞到对手**不弹开**、扣 8 血、把对手沿法线推开 120，一次穿过只结算一次；
  Dash 越放越强（本局累计，重开一局重学）：索敌延迟 0.3 s → 0.05 s、蓄力时间 3 s → 0 s、第 15 次起打不断；
- **Singularity（黑洞，250 血）**：`hitbox.ghost = true` 让对手直接穿进它身体，`hitbox.contact = 0.6` 让它**只挨撞掉血、撞它的人不掉血**；
  质量 2.6 → 5.0（90 s 长满），吸力加速度乘「当前质量 / 2.6」；
- 黑洞被动「吸引」（`content/skills/gravity-well.json`）：范围 280 → 440、场内每秒掉 1（边缘）~8（中心）血；
  对手中心进到 0.72 × 30 = 21.6 再吃一次 6 → 24 的核心伤害，然后被**随机传送**到场地内站得住的位置（离黑洞 ≥ 240，1.5 s 保护期）；
- 黑洞大招「万向天引」（`content/skills/omnipull.json`）：冷却固定 8 s，吸 2 → 5 s、半径 220 → 420、爆炸 22 → 60（都按对局时间 90 s 长满）；
  吸的过程中场内每秒掉 1~3 血，自己无敌、被动失效、整对不吃撞击伤害；炸完才进冷却；
- **Wooden Dummy（木人桩）**：1000 血、没有任何技能，纯陪练；
- 任何技能施放时球上方弹出技能名，0.9 s 上浮淡出；大招吸力期间画一圈紫色吸力环；
- 25 秒后开始缩圈（每 10 s 一档，每档 10%，带 1 秒预警），场地一路缩到 0；装不下两个球的那一步立即结算，**HP 高者胜**，同 HP 平局；
- 5 秒没碰撞会触发「静默吸引」，球会缓慢靠近（肉眼几乎看不出）；
- HP 归零或场地归零 → 结算画面，显示 `RETURNING IN 5...`；期间按 `R` 重开同阵容，5 秒后自动回主菜单；
- 对局中 `ESC` 回主菜单（**ESC 不再退出进程**）；退出只能在菜单里点 `QUIT GAME` → `YES`。

![screenshot](screenshot.png)

## 当前构建验证（2026-11-13）

| 项 | 结果 | 备注 |
|---|---|---|
| 全量源码编译（21 个 `.cpp`） | ✅ | `-Wall -Wextra` 零警告 |
| `tools/validate_content.ps1` | ✅ | `3 characters, 7 skills, 1 maps` |
| `build-verify/gravity_probe.exe`（不在仓库里） | ✅ | 24 项全过：核心伤害 6 / 传送 ≥ 220 且落在场地内 / 1.5 s 保护期 / 穿透不弹开但 `Hit` 照发 / 只有黑洞挨撞掉血 / 无敌期间扣血无效 / 吸的时候被动失效 / 炸完冷却 8 s / 范围与伤害按时间长满 / 场内 DPS 半程 2、边缘 1 / 质量 2.6→3.8→5.0 |
| `--headless --seconds 130` | ✅ | 49 s 结算：Lancer 死亡，黑洞 5/250，`hits=17` |
| 主菜单三人可选 | ✅ | 截图：Lancer / Singularity / Wooden Dummy 三张卡都能切 |
| 万向天引吸力圈 + 爆炸 | ✅ | 截图：`T 10` 吸力圈半径 220；`T 12` 爆炸环 + 粒子 |

## 下一个该谁做什么

| 优先级 | 任务 | 负责人 | 依赖 |
|---|---|---|---|
| P0 | 热重载：`ContentLoader::poll()` + `AssetCache` 重载 + 屏幕报错 | C | 无 |
| P0 | 技能剩余原语：`lifesteal` / `shield` / `slow_field`（`content/skills/` 里三个占位包） | C | `Match` 上下文 |
| P1 | `--autotest <seconds>`（`Options::autotestSeconds` 还没有解析入口） | B | 无 |
| P1 | `DebugDraw`（F1 碰撞箱 / F2 场地 / F3 事件） | B | 无 |
| P1 | 缩圈压到球的持续伤害（`damagePerSec`，不计连击） | C | 无 |
| P2 | 地图选择界面 | B | 热重载 |
| P2 | 碰撞箱偏移 `hitbox.offset` 接进物理 | A | 无 |

## 已知缺口

- 技能只有「蓄力冲刺」「贯穿」「引力井」「万向天引」和 `add_speed` 真的生效，`lifesteal` / `shield` / `slow_field` 三个包是占位（会在日志里提示一次）。
- **木人桩没有任何伤害来源**（本来就是陪练）：没技能、不会主动打人，撞击伤害又被 `damageScale = 0` 关掉，实际为 0。
- 冲刺一次大约 60–100 伤害（0.12 × 相对速度），两下就能打死 150 血的对手——数值还没调。
- 数值只跑过固定 seed 的 headless（49 s、黑洞剩 5/250），**没手动试过手感**。
- 没有热重载；改 JSON 要重启。
- 缩圈边界压到球的持续伤害（`damagePerSec`）仍未接。
- 没有单元测试：物理回归靠 `--headless --seconds N` + 日志，技能机制靠 `build-verify/` 里的探针（该目录不进仓库）。
- `hitbox.offset` 还没进物理（现在只按半径算）。
- 没有音效。
- 中文名走**系统字体**（`C:/Windows/Fonts/msyh.ttc`，找不到再试 `msyhbd.ttc` / `simhei.ttf`）：默认仍显示英文，`--lang zh` 才切中文；一个字体都打不开就自动退回英文名，不会崩；换平台要在 `content/fonts/` 打包一个 CJK 字体。
- 菜单只能用鼠标操作（键盘只有 Enter 开局、ESC 关确认框），没有方向键导航。
- 菜单不做地图选择，地图仍由 `--map` 决定。
