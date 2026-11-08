# 契约：physics

**Owner**：A　**依赖**：`core`　**被依赖**：`match`、`game`、`render`

## 职责

- 圆体运动积分（半隐式欧拉）
- 球-球、球-障碍、球-墙的检测与响应
- 可变场地 `Arena`（缩圈插值、把球推回、压碎障碍）
- 产生 `hit` / `wall_bounce` / `obstacle_crushed` 事件

## 不负责

- 不扣血、不算连击、不认识技能 → `match`
- 不加载贴图、不管皮肤半径 → `render` 用 `Body.radius` 画就行
- 不读 JSON

## 公开接口

| 文件 | 内容 |
|---|---|
| `physics/Body.hpp` | `Body{ id, pos, vel, radius, mass, invMass, restitution, speedMul }` |
| `physics/Collision.hpp` | `Contact`、`collideCircles`、`resolveCircleCircle`、`resolveCircleRect`、`resolveCircleCircleStatic` |
| `physics/Arena.hpp` | `Arena::bounds()`、`setShrinkTarget()`、`update()`、`resolveOverlaps()`、`obstacles()` |
| `physics/World.hpp` | `World(EventBus&, Arena&)`、`addBody`、`step(dt)`、`bodies()` |

## 不变量

1. `step()` 只做物理，**不**派发事件；事件入队，由外层 `dispatch()`。
2. 一次 `step(dt)` 内，每个 `Body` 的位置变化只由该步的速度与加速度决定（无隐式多步）。
3. 球速上限 `Arena::maxSpeed()`（默认 1800 px/s），超出即夹紧。
4. `resolveOverlaps()` 必须每步调用（缩圈期间 `bounds` 每步都变）。
5. 球被推出场地时，**只推回 + 反射，不加能量**。
6. `Body.mass > 0`；`invMass = 1/mass` 由 `addBody` 计算，外部不要手改。

## 禁止

- 在 `Body` 里加 `hp` / `skillId` / `texture` 字段（那是 `match` / `render` 的职责）
- 在物理里调用 `Renderer`
- 用 `float` 以外的类型做位置（保持一致，别混 `double`）

## 验收

- 两球对撞后总动量和能量在恢复系数 1.0 时守恒（误差 < 1e-3）
- `--autotest 600` 内没有球逃出 `Arena.bounds()`
- 缩圈到最小尺寸时仍无卡墙
