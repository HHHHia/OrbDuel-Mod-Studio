# 架构 / Architecture

> 本文是工程的事实来源。改结构先改这里，再改代码。

## 0. 一句话

**引擎只认「实体 + 修饰器 + 事件」；角色、技能、地图是外部数据包；表现层只订阅事件。**

这条线同时解决三件事：
1. 你最初的问题「技能特效怎么覆盖到角色身上」→ 不覆盖，靠事件广播，特效层独立绘制；
2. 评分项 `innovative`（不易被现成包替代的可复用特性）→ 内容包 + 热重载就是那个特性；
3. 三人并行 → 模块边界即分工线，接口先冻结。

## 1. 硬约束

| 约束 | 来源 | 后果 |
|---|---|---|
| 截止 2026-11-24、3 个写码的人 | 作业截止日期 | 不做 ECS、不做插件 DLL、不做脚本引擎 |
| 队友环境未知 | 你上轮的判断 | 依赖全部离线预编译进仓库，一键脚本 |
| 公开仓库、要被助教看 | 评分项 published / documented | 素材必须 CC0，文档中英双语 |
| 1 分钟现场演示 | 评分项 functional | 必须一键跑起来；热重载是演示动作，**还没接**（见 `STATUS.md`） |

## 2. 分层

```
┌───────────────────────────────────────────────┐
│ game      装配、主循环、输入、场景切换          │  ← 唯一知道"这是一局游戏"的地方
├───────────────────────────────────────────────┤
│ match     HP、伤害、连击、技能冷却、缩圈、防僵局 │  ← 规则层，不画东西
├──────────────────────┬────────────────────────┤
│ physics               │ render                 │
│ 圆体、弹性碰撞、场地   │ SDL2 封装、精灵、粒子   │
├──────────────────────┴────────────────────────┤
│ content   JSON 模型、加载、校验、热重载          │
├───────────────────────────────────────────────┤
│ core      Vec2/Rect、固定步长时钟、RNG、事件总线 │
└───────────────────────────────────────────────┘
```

**依赖只能向下。** 反向依赖一律视为设计错误。

- `physics` 不认识 HP、技能、贴图。它只知道圆、速度、质量、恢复系数。
- `content` 不认识 SDL2，也不认识 `Body`；它产出纯数据。
- `match` 负责把 `CharacterDef` 「翻译」成 `Body` + `Fighter`。
- `render` 不推进物理、不改状态，只读。

## 3. 一帧的生命周期

```
frame(frameSeconds)
 ├─ Clock.tick(frameSeconds)            # 累加，算出这一帧要跑几个固定步
 ├─ 重复 N 次（N ≤ 8）:
 │   ├─ Input.sample()                  # 现阶段是空实现（自动对战）
 │   ├─ Match.update(dt)                # 技能冷却、连击计时、HP
 │   ├─ StallGuard.update(dt)           # 静默计时 → 可能施加微弱吸引
 │   ├─ World.step(dt)                  # 积分 → 碰撞检测 → 碰撞响应 → 产生事件
 │   ├─ Zone.update(dt)                 # 缩圈计时、Arena 插值、把球推回、障碍碎裂
 │   ├─ EventBus.dispatch()             # 统一派发本步产生的事件
 │   └─ Match.onEvent(e) / Fx.onEvent(e)# 结算伤害、播特效
 ├─ Renderer.beginFrame()
 ├─ 画背景 → 障碍 → 球 → 粒子 → HUD → 调试层
 └─ Renderer.endFrame()
```

要点：
- **事件在步末统一派发**，物理过程中不回调，避免重入（见 ADR-004）。
- 物理步长恒定 `1/120 s`；一帧最多补 8 步（`Clock` 的 `maxSteps`），超了直接丢时间，避免「死亡螺旋」。
- 渲染用 `Clock.alpha()` 做插值（可选，先不做也不影响正确性）。
- **主菜单是另一条帧路径**：在菜单里整段物理都不跑，只调 `Menu.update()` + `Menu.draw()`；`Clock` 不推进，菜单停留时间不算进对局（见 `MODULE_CONTRACTS/game.md` 不变量 6）。

## 4. 内容包

```
content/
  characters/<id>/character.json   + skin.png
  skills/<id>.json                  # 全局技能库，角色按 id 引用
  maps/<id>/map.json               + bg.png
```

加载流程（`ContentLoader::loadAll`）：

1. 扫描三个目录 → 读 JSON → 结构校验（字段、类型、范围）
2. 交叉校验（角色引用的技能 id 必须存在；贴图文件必须存在）
3. 构建 `ContentPack`（角色表、技能表、地图表）
4. **任何一条失败都不崩**：记 `LoadIssue{severity, packId, message}`，坏包不加载，旧包保留

热重载（`ContentLoader::poll`，设计为每 0.5 s 一轮，**目前 `poll()` 是空实现**）：

- 比对文件 `last_write_time`；有变化 → 只重载变化的包
- 贴图由 `AssetCache` 按路径重载（`UnloadTexture` + `LoadTexture`）
- 重载失败 → 屏幕上滚一行红字，游戏继续跑（这条是演示时的保险）

## 5. 物理模型

| 项 | 取值 | 说明 |
|---|---|---|
| 形状 | 圆 | 后续「像素伤害掩码」只影响伤害判定，不影响运动 |
| 步长 | 1/120 s | 见 ADR-003 |
| 重力 | 无 | 题目要求 |
| 积分 | 半隐式欧拉 | `v += a·dt; p += v·dt` |
| 球-球恢复系数 | `min(e1, e2)` | 各自 `stats.restitution` |
| 墙恢复系数 | `map.physics.wallRestitution` | 默认 0.98 |
| 墙反弹抖动 | `±bounceJitterDeg`（默认 1.5°） | 防周期轨道，见 §6 |
| 重叠修正 | 位置分离 80% + 速度沿法线交换 | 防止球黏在一起 |
| 最大速度 | 1800 px/s | 防止穿模与数值爆炸 |

碰撞检测顺序：球-球（1 对）→ 球-障碍 → 球-墙。全部解析解，无迭代求解器。

**确定性**：同一个 seed + 同一个二进制 + 同一台机器 → 同一局结果。
跨编译器不保证（浮点 `-ffast-math` 之类的差异），所以不要拿录像去逐帧对比。

## 6. 防僵局：四层

问题：两球同速同向、无重力、矩形场地时，可以停在同一条周期轨道上**永远不接触**。
单纯随机初速解决不了，因为对局中会重新进入近似周期轨道。

| 层 | 机制 | 参数 | 触发 | 可见性 |
|---|---|---|---|---|
| L1 | 撞墙反射角加噪 | `bounceJitterDeg = 1.5°` | 每次撞墙 | 看不见 |
| L2 | 静默吸引：向对方施加加速度 | 5 s 后渐入，1.5 s 到满，上限 15 px/s² | 无碰撞计时 | 几乎看不见 |
| L3 | 缩圈 | 每档内缩 8%，预警 1 s，动画 1.5 s | 对局 > 25 s 或静默 > 8 s | 看得见（故意的） |
| L4 | 场地地板 | 内切尺寸 ≥ 2.4 × (r₁ + r₂) | 缩到底 | — |

L2 的细节（这是最容易做难看的一层）：
- 只在 `lullSeconds > 5` 时生效，碰撞立刻清零 → 正常对打时完全不存在；
- 加速度从 0 线性爬升，不是阶跃，避免"球突然拐弯"；
- 两球距离 < `3 × (r₁+r₂)` 时停止施力，避免"吸过头"变成贴贴；
- 施力方向沿两球连线，且**只加在速度上、不改质量**。

L3 由 `Zone` 负责调度，L4 是它的停止条件。

> 判定「僵局」的阈值集中在 `match/StallGuard.hpp` 的 `StallParams`，调参只改这一处。

## 7. 可变场地与缩圈

`Arena` 不是常量矩形，而是三态：

```
base    原始矩形（来自 map.size 与上下留白）
current 当前矩形（球实际反弹的边界）
target  目标矩形（缩圈时被设置）
```

- `setShrinkTarget(inset01, duration)` 设置目标并开启插值；
- `update(dt)` 用 smoothstep 把 `current` 推向 `target`；
- 每步结束后调用 `resolveOverlaps()`：
  - 球在 `current` 外 → 沿最近边推回 + 反射速度（恢复系数 × 0.9，**不加能量**）；
  - 障碍不再完全包含于 `current` → 触发 `obstacle_crushed` 事件，障碍移除；
- 缩圈期间 `current` 每步都会变，所以**必须**每步都做一次 `resolveOverlaps`，不能只在缩圈结束时做。

缩圈参数在 `map.json` 里，每张图可以不一样（见 `CONTENT_FORMAT.md`）。

压到球时的伤害：`damagePerSec` 持续扣，默认 4 HP/s，且**不触发连击**（避免缩圈送分）。

## 8. 事件 → 表现层

事件是引擎与表现之间唯一的桥。目前定义：

| 事件 | 载荷 | 谁发 | 谁关心 |
|---|---|---|---|
| `hit` | 双方 id、接触点、相对速度 | physics | match（结算）、fx（火花/震屏）、hud |
| `wall_bounce` | 球 id、位置、速度 | physics | fx（拖尾/灰尘） |
| `obstacle_crushed` | 障碍 id、位置 | zone | fx（碎裂） |
| `skill_cast` | 施法者 id、技能 id | match | fx（光环/拖尾）、hud（冷却条） |
| `skill_expire` | 施法者 id、中心点、半径 | match（大招炸完） | fx（爆炸环/粒子/震屏） |
| `zone_shrink` | 新矩形、档位 | zone | fx（边界红光）、hud（警告） |
| `death` | 球 id | match | game（结算） |

**"技能特效怎么盖在角色身上"的答案**：

```
技能定义 ──(数据)──► Modifier 列表 ──► 改物理/状态
        └─(数据)──► vfx 绑定 ──► 订阅事件 ──► 粒子/拖尾/震屏
```

- 技能**只改状态、只发事件**，绝不直接画东西；
- 特效层根据「事件 + 角色自带的 vfx 覆盖」决定播什么；
- 所以换皮肤、换角色、换技能，渲染层一行不改。

绘制顺序（从下到上）：
`背景 → 场地边框 → 障碍 → 球体拖尾 → 球体贴图 → 粒子/光环 → 伤害飘字 → HUD → 调试层`

角色的 `skin.tint` 会给贴图染色（换色不换图）；`vfx.overrides` 可以按技能覆盖颜色与粒子类型。

## 9. 对局

- 单局：HP 归零结束（`HpRule`），平局仅在两球同一步死亡时成立。
- 伤害：`dmg = baseDamage × (relSpeed / refSpeed) × comboMult`，参考速度 400 px/s；
  `baseDamage` 的倍率按**挨撞的一方**取 `max(map.physics.damageScale, 该角色 hitbox.contact)`，
  `duel-basic` 把地图倍率设成 0，所以碰撞本身不掉血，伤害全来自技能。
- 连击：2 s 内再次命中，倍率 `1 + 0.15 × (combo-1)`，上限 2.5；3 s 无命中清零。
- `IMatchRule` 是接口，`HpRule` 是默认实现；以后要「限时得分」「三局两胜」加一个实现即可，不改 `Match`。

## 10. 失败与调试

- **内容错误**（JSON 写错、贴图缺失）：非致命，屏幕控制台红字 + `LoadIssue` 列表，游戏继续。
- **引擎错误**（窗口创建失败、SDL 初始化失败）：致命，`log::error` + 返回码 2。
- 调试键（`DebugDraw` **尚未实现**）：`F1` 碰撞箱与速度矢量，`F2` 场地边界与缩圈目标，`F3` 事件流，`F4` 帧时间/步数。
- `--autotest <seconds>`：**尚未实现**（`Options::autotestSeconds` 有字段、没解析入口）。改物理后现在用 `--headless --seconds 60` 看日志里的 `hits` / `hp0` / `hp1` 做回归。

## 11. 预留的扩展点

| 想做的事 | 在哪里加 | 要不要改接口 |
|---|---|---|
| 玩家操作 | `game/Input.hpp` 的实现类 | 否，接口已在 |
| 限时/多局模式 | `match/MatchRule.hpp` 新增实现 | 否 |
| 像素级伤害掩码 | `content/HitMask` + `match` 的伤害判定 | 是，`Body` 加 `HitMask*` |
| 新技能原语 | `match/Skill.cpp` 的 `op` 分派 + `CONTENT_FORMAT.md` | 是，但只有一处 |
| 换渲染后端 | `render/` 整个目录 | 是（有意为之，见 ADR-002） |
| 游戏内编辑器 | 新增 `editor/`，复用 `content` 模型 | 否 |

## 12. 目录对照

```
orbduel/
├── AGENTS.md                   AI/队友规则
├── README.md                   双语门面
├── CMakeLists.txt
├── build.ps1                   一键配置+构建+冒烟
├── include/orbduel/            公开接口（只有声明）
│   ├── core/       Types Time Rng Event Log
│   ├── physics/    Body Arena World Collision
│   ├── render/     Renderer Fx DebugDraw AssetCache
│   ├── content/    DataModel ContentLoader
│   ├── match/      Match MatchRule StallGuard Zone Skill
│   └── game/       Game Menu Input
├── src/                        实现（各模块 .cpp，共 21 个）
├── content/                    数据包 + schema
├── docs/                       文档
├── tools/                      校验脚本
└── third_party/sdl2/          离线预编译（mingw / msvc）
```
