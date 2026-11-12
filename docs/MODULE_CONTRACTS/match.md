# 契约：match

**Owner**：C　**依赖**：`core`、`physics`、`content`　**被依赖**：`game`、`render`

## 职责

- 把 `CharacterDef` 翻译成 `Body` + `Fighter`（HP、护盾、技能状态）
- 伤害结算、连击倍率、胜负判定
- 技能冷却与修饰器生效
- 缩圈调度（`Zone`）与防僵局（`StallGuard`）

## 不负责

- 不做碰撞检测（`physics` 发事件）
- 不画任何东西（只写状态，`render` 去读）
- 不解析 JSON（用 `content` 给的 `ContentPack`）

## 公开接口

| 文件 | 内容 |
|---|---|
| `match/Match.hpp` | `Match(spawn)`、`update(dt)`、`onEvent(e)`、`fighter(i)`、`skills(i)`、`phase()`、`reset()`、`Fighter{ ..., pierce, gravity, coreTick, coreGuard }`、`PierceMod{ damage, minSpeed, push }`、`GravityMod{ range, accel, coreFactor, ... }` |
| `match/MatchRule.hpp` | `IMatchRule`、`HpRule`、`MatchResult{ winner, reason }` |
| `match/StallGuard.hpp` | `StallParams`、`StallGuard::onHit/update/lullSeconds/attraction()` |
| `match/Zone.hpp` | `ZoneController(Arena&, ShrinkDef)`、`update(dt, lull)`、`tier()` |
| `match/Skill.hpp` | `SkillSystem::update(dt, hpRatio, body, enemy, rng, events)`、`interruptCharge()`、`consumeRam()`、`pullField(PullField&)`、`invulnerable()`、`suppressesPassive()`、`omnipullActive()`、`consumeBlast()`、`PullField{ radius, accel, dpsEdge, dpsCenter }`、`SkillState` |

## 不变量

1. `Fighter` 是 `match` 的私有概念；`physics::Body` 里**没有** HP。
2. 伤害公式唯一入口：`Match::applyDamage()`。任何绕开它的扣血都是 bug。
3. 连击窗口 2 s、衰减 3 s；**环境伤害（缩圈）不计连击**。
4. 技能修饰器只改 `Body` 的公开字段（速度、半径、质量）或 `Fighter` 的护盾值。
5. `Zone` 只能通过 `Arena::setShrinkTarget()` 改场地，不许直接写 `bounds`。
6. `StallGuard` 的吸引只能在 `lullSeconds > params.lullStart` 时施加，命中即清零。
7. 对局结束（`phase == Finished`）后 `update()` 不再结算伤害。
8. 蓄力（`SkillState.charging`）只在开始时清零一次速度；之后 `StallGuard` 的吸力仍能推动它。
9. 蓄力被任何撞击打断即作废并进 CD（不看这一下是否掉血）；冲刺重击一次只结算一次，且必须走 `applyDamage()`。
10. 被动贯穿（`pierce`）在 `setup()` 读一次，存进 `Fighter.pierce`；`update()` 每步按 `自身速度 ≥ minSpeed` 决定要不要给 `Body.phasePush` 赋值，`physics` 只消费这个开关。
11. 贯穿伤害同样只走 `applyDamage()`；一次穿过只结算一次（物理层保证该配对只发一次 `Hit`），与冲刺重击各算各的。
12. 场地缩到装不下两个球（`bounds` 的宽或高 `< r1 + r2`）时立即结算：`reason = "zone_zero"`，HP 高者胜，同 HP 平局（`winnerId = -1`）。
13. 引力井（`gravity`）在 `setup()` 读一次，存进 `Fighter.gravity`。`update()` 每步给范围内的对手 `body.force` 加一个**向心加速度**
    （`accel × 当前质量 / stats.mass`，方向 = 自己 − 对方），保留对方的切向速度，所以轨迹是螺旋而不是直线吸尘器；超出当前半径不加力。
14. 半径与核心伤害都按 `elapsed / growthSeconds` 线性成长（**对局时间，不是施放次数**），同一个 `growthSeconds` 共用。
15. 站在吸力场里（被动场或大招场）按秒掉血：`dps = dpsEdge + (dpsCenter − dpsEdge) × (1 − 距离/半径)`，每步 `applyDamage(dps × dt)`。
16. 对手中心进到 `coreFactor × 自己半径` 以内即判「中心伤害」：伤害 = `damage` 线性长到 `damageMax`，
    走 `applyDamage()`，然后**随机传送**对手（场地内、不压障碍、离自己 ≥ `teleportMinDist`；64 次采样都不行就原地不动）。
    同一个人两次中心伤害至少隔 `tickInterval`，并且传送后有 `teleportGuard` 保护期。
17. 万向天引进行中：被动失效（`suppressesPassive()`）、自己免伤害（`invulnerable()`，连缩圈伤害也不吃）、
    **整对不吃撞击伤害**（`omnipullActive()`）；吸力参数由 `pullField()` 提供，`Match` 负责施加。
    爆炸伤害与冷却都只在**吸完那一步**结算一次，冷却固定 = `trigger.cooldown`，从爆炸那一刻开始算。
18. 撞击伤害系数 = `max(地图 damageScale, 各角色 hitbox.contact)`；0 表示这张图撞人不掉血。
19. 质量成长：`stats.massMax > stats.mass` 时每步把 `body.mass` 从 `mass` 线性改到 `massMax`（`massRamp` 秒），并同步 `invMass`。
20. 传送、爆炸都必须走 `applyDamage()`；传送只改 `Body.pos`（并清 `force`），不改速度。

## 禁止

- 直接 `#include <SDL.h>`
- 在 `Match` 里做 IO / 读文件
- 把缩圈伤害塞进 `applyDamage` 的连击路径

## 验收

- 同一 seed 两局，HP 曲线一致
- 缩圈到地板尺寸前一定发生至少一次碰撞
- 技能冷却在暂停（`phase != Fighting`）时不走
