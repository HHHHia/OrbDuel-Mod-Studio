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
| `match/Match.hpp` | `Match(spawn)`、`update(dt)`、`onEvent(e)`、`fighters()`、`phase()`、`reset()` |
| `match/MatchRule.hpp` | `IMatchRule`、`HpRule`、`MatchResult{ winner, reason }` |
| `match/StallGuard.hpp` | `StallParams`、`StallGuard::onHit/update/lullSeconds/attraction()` |
| `match/Zone.hpp` | `ZoneController(Arena&, ShrinkDef)`、`update(dt, lull)`、`tier()` |
| `match/Skill.hpp` | `SkillSystem`、`SkillState`、`Modifier`、`TriggerKind`、`ModifierOp` |

## 不变量

1. `Fighter` 是 `match` 的私有概念；`physics::Body` 里**没有** HP。
2. 伤害公式唯一入口：`Match::applyDamage()`。任何绕开它的扣血都是 bug。
3. 连击窗口 2 s、衰减 3 s；**环境伤害（缩圈）不计连击**。
4. 技能修饰器只改 `Body` 的公开字段（速度、半径、质量）或 `Fighter` 的护盾值。
5. `Zone` 只能通过 `Arena::setShrinkTarget()` 改场地，不许直接写 `bounds`。
6. `StallGuard` 的吸引只能在 `lullSeconds > params.lullStart` 时施加，命中即清零。
7. 对局结束（`phase == Finished`）后 `update()` 不再结算伤害。

## 禁止

- 直接 `#include <SDL.h>`
- 在 `Match` 里做 IO / 读文件
- 把缩圈伤害塞进 `applyDamage` 的连击路径

## 验收

- 同一 seed 两局，HP 曲线一致
- 缩圈到地板尺寸前一定发生至少一次碰撞
- 技能冷却在暂停（`phase != Fighting`）时不走
