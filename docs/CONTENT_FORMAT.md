# 内容格式 / Content Format

> `format: 1`。破坏性改动必须把 `format` 加一，并在本文件写迁移说明。
> 单位：长度 px，角度 deg，时间 s。坐标系：**左上角为原点，y 向下**。

---

## 0. 目录结构

```
content/
├── characters/<id>/
│   ├── character.json
│   └── skin.png
├── skills/<id>.json
├── maps/<id>/
│   ├── map.json
│   └── bg.png            (可选)
├── schema/               JSON Schema（编辑器提示用）
└── ASSETS.md             素材来源与许可证登记
```

`<id>` 规则：`[a-z0-9][a-z0-9-]{0,31}`，全局唯一（角色、技能、地图各自命名空间内唯一）。

---

## 1. character.json

```json
{
  "format": 1,
  "id": "lancer",
  "name": { "en": "Lancer", "zh": "疾锋" },
  "skin": { "texture": "skin.png", "shape": "sprite", "scale": 1.0, "tint": [255, 255, 255, 255] },
  "hitbox": { "shape": "circle", "radius": 18.0, "offset": [0, 0] },
  "stats": { "mass": 1.0, "restitution": 0.95, "maxHp": 100.0, "startSpeed": 420.0 },
  "skills": ["dash", "lifesteal"]
}
```

| 字段 | 类型 | 必填 | 约束 | 说明 |
|---|---|---|---|---|
| `format` | int | ✔ | 必须为 1 | 格式版本 |
| `id` | string | ✔ | 与目录名一致 | 唯一 id |
| `name.en` | string | ✔ | 1–32 字符 | 显示名 |
| `name.zh` | string | ✘ | 1–16 字符 | 缺省回退 `name.en`；需要系统有中文字体，找不到就显示英文 |
| `skin.texture` | string | ✔ | 相对本目录的文件名 | PNG，建议 256×256 |
| `skin.shape` | string | ✘ | `"sprite"`（默认）\| `"triangle"` | `sprite` 画贴图；`triangle` 不读贴图，直接画瘦长三角 |
| `skin.scale` | float | ✘ | 0.1–8 | 默认 1.0；**只影响绘制，不影响碰撞** |
| `skin.tint` | int[4] | ✘ | 0–255 | 乘算染色，默认白色 |
| `hitbox.shape` | string | ✔ | `"circle"` | v1 只支持圆 |
| `hitbox.radius` | float | ✔ | 6–120 | 运动碰撞半径 |
| `hitbox.offset` | float[2] | ✘ | ±40 | 相对贴图中心的偏移 |
| `stats.mass` | float | ✔ | 0.2–20 | 质量，影响碰撞后的速度分配 |
| `stats.restitution` | float | ✔ | 0–1 | 恢复系数（弹性） |
| `stats.maxHp` | float | ✔ | 10–10000 | 最大生命 |
| `stats.startSpeed` | float | ✔ | 0–1800 | 开局速度大小 |
| `skills` | string[] | ✔ | 0–2 项，必须是已存在的技能 id | 技能槽 |

**皮肤与碰撞箱的关系**（你最初问的那个点）：
贴图只是外观；`hitbox.radius` 是运动碰撞；以后加"像素伤害掩码"时，
`hitbox` 仍是运动圆，掩码只管"打没打中"，两者互不影响。

`skin.shape = "triangle"` 同理：**碰撞仍是那个圆**，三角形只决定画出来的轮廓。
尖角朝向 = 速度方向（蓄力时精确指向蓄力虚线，虚线从尖角出发）；
比例写死在 `src/game/BodyShape.hpp`（尖端 1.35×半径、尾部半宽 0.72×半径），
`skin.tint` 是填充色，描边自动亮一档。想加新形状就在这里加一个分支 + 一个 `shape` 值。

---

## 2. skills/<id>.json

```json
{
  "format": 1,
  "id": "dash",
  "name": { "en": "Dash", "zh": "冲刺" },
  "trigger": { "kind": "cooldown", "cooldown": 6.0 },
  "charge": {
    "duration": 3.0,
    "durationMin": 0.0,
    "durationLearningCasts": 10,
    "releaseSpeed": 500.0,
    "aimLag": 0.3,
    "aimJitterDeg": 6.0,
    "aimLagMin": 0.05,
    "aimJitterMinDeg": 0.5,
    "aimLearningCasts": 5,
    "interruptImmuneCasts": 15,
    "ramDamageScale": 0.12
  },
  "modifiers": [],
  "vfx": [
    { "on": "cast", "type": "trail", "color": [255, 180, 60, 255], "duration": 1.2 }
  ],
  "stack": "refresh",
  "priority": 10
}
```

### 2.1 trigger

| `kind` | 额外字段 | 何时触发 |
|---|---|---|
| `cooldown` | `cooldown` (0.5–60) | 冷却好就自动放（现阶段无输入的默认方式） |
| `on_hit` | `cooldown` 可选 | 自己命中对手后 |
| `on_hit_by` | `cooldown` 可选 | 自己被命中后 |
| `on_low_hp` | `hpBelow` (0–1) | HP 低于比例时 |
| `on_wall_bounce` | `cooldown` 可选 | 自己撞墙后 |

> 以后接玩家输入时，`trigger` 增加 `"kind": "manual"`，由 `Input` 决定何时放。

### 2.2 modifiers

| `op` | 参数 | 效果 | 备注 |
|---|---|---|---|
| `add_speed` | `value` px/s, `duration` | 沿当前速度方向加一个脉冲 | 最常见的"冲刺" |
| `shield` | `value` 吸收点数, `duration` | 吸收伤害，破盾即失效 | 护盾 |
| `lifesteal` | `value` 0–1, `duration` | 造成伤害的该比例回血 | 吸血 |
| `slow_field` | `radius` px, `factor` 0–1, `duration` | 半径内对手速度乘 `factor` | 减速力场 |
| `teleport` | `radius` px | 瞬移到场地内随机位置 | 逃脱 |
| `resize` | `value` 倍率 0.5–2.0, `duration` | 改半径与质量 | 体型变化 |
| `bounce_boost` | `value` 0–1, `duration` | 撞墙额外恢复系数 | 反弹强化 |
| `heal` | `value` HP | 直接回血 | 兜底原语 |

`curve`：`linear`（默认）/ `ease_out` / `ease_in_out`。只影响数值随时间的变化曲线。

### 2.3 vfx

| `type` | 参数 | 表现 |
|---|---|---|
| `trail` | `color`, `duration` | 拖尾 |
| `burst` | `color`, `count`, `value`(速度) | 粒子爆散 |
| `ring` | `color`, `value`(半径) | 扩散圆环 |
| `flash` | `color`, `duration` | 整体闪白/染色 |
| `screen_shake` | `value` 强度 px | 震屏 |
| `hit_stop` | `duration` | 短暂停顿（0.03–0.12 s） |

`on`：`cast` / `hit` / `expire`。

### 2.4 其他字段

| 字段 | 类型 | 说明 |
|---|---|---|
| `stack` | string | `refresh`（重置持续时间）/ `stack`（叠加数值） |
| `priority` | int | 同一帧多个技能触发时的顺序，小的先 |

### 2.5 charge（蓄力冲刺）

有 `charge` 块的技能不再「冷却好立刻放」，而是先原地蓄力再冲出去。
带 `charge` 的技能 `modifiers` 可以为空（位移与伤害由 `charge` 负责）。

| 字段 | 类型 | 说明 |
|---|---|---|
| `duration` | float 0.2–10 | 蓄力时长（第一次放的时候用）；这期间速度被清零，球定在原地 |
| `durationMin` | float 0–10 | 放熟之后的蓄力时长下限；`0` = 瞬发（不能大于 `duration`） |
| `durationLearningCasts` | int 1–50 | 放几次把蓄力时间压到 `durationMin` |
| `releaseSpeed` | float 50–2000 | 蓄满瞬间把速度设成这个值，方向 = 虚线方向 |
| `aimLag` | float 0–2 | 锁定点落后敌人多少秒（越大越打不准） |
| `aimJitterDeg` | float 0–45 | 锁定方向的随机抖动（度）；抖动是低频噪声，慢慢飘，不是每帧乱跳 |
| `aimLagMin` | float 0–2 | 「放熟」之后的延迟下限（不能大于 `aimLag`） |
| `aimJitterMinDeg` | float 0–45 | 「放熟」之后的抖动下限（不能大于 `aimJitterDeg`） |
| `aimLearningCasts` | int 1–50 | 放出几次后达到最准；本局累计，重开一局归零 |
| `interruptImmuneCasts` | int 0–100 | 本局放够几次之后蓄力不再被撞击打断 |
| `ramDamageScale` | float 0–5 | 冲出后第一次撞到对手的额外伤害 = 该值 × 接触相对速度 |

不变量：蓄力被任何撞击打断就作废并进 CD（放够 `interruptImmuneCasts` 次之后不再被打断）；一次冲刺只结算一次额外伤害。
三个「越放越强」的量都按本局释放次数线性插值，各自独立：蓄力时长（`durationLearningCasts`）、锁定延迟与抖动（`aimLearningCasts`）、打断免疫（`interruptImmuneCasts`）。

---

## 3. maps/<id>/map.json

```json
{
  "format": 1,
  "id": "duel-basic",
  "name": { "en": "Basic Duel", "zh": "基础场地" },
  "size": [1280, 720],
  "theme": { "bgColor": [16, 17, 24, 255], "wallColor": [90, 120, 255, 255], "wallThickness": 6 },
  "spawns": [
    { "pos": [220, 360], "dir": [1, 0] },
    { "pos": [1060, 360], "dir": [-1, 0] }
  ],
  "obstacles": [
    { "shape": "rect", "pos": [640, 140], "size": [260, 24], "restitution": 0.9 },
    { "shape": "circle", "pos": [640, 580], "radius": 40, "restitution": 1.0 }
  ],
  "physics": { "wallRestitution": 0.98, "bounceJitterDeg": 1.5, "maxSpeed": 1800, "damageScale": 0.0, "refSpeed": 400 },
  "shrink": { "firstAt": 25.0, "interval": 10.0, "step": 0.10, "minInset": 1.0, "warnTime": 1.0, "animTime": 1.5, "damagePerSec": 4.0, "lullTrigger": 60.0 },
  "seed": 20261124
}
```

| 字段 | 类型 | 必填 | 约束 | 说明 |
|---|---|---|---|---|
| `size` | int[2] | ✔ | 640–3840 × 480–2160 | 场地原始尺寸 = 窗口尺寸 |
| `spawns` | object[2] | ✔ | 恰好 2 个 | 出生点与朝向 |
| `spawns[].pos` | float[2] | ✔ | 在场地内 | 出生坐标 |
| `spawns[].dir` | float[2] | ✔ | 非零向量 | 初始方向（会归一化） |
| `obstacles[]` | object[] | ✘ | 0–12 个 | 静态障碍 |
| `obstacles[].shape` | string | ✔ | `rect` / `circle` | — |
| `physics.wallRestitution` | float | ✘ | 0–1，默认 0.98 | 墙的弹性 |
| `physics.bounceJitterDeg` | float | ✘ | 0–5，默认 1.5 | 防僵局 L1 |
| `physics.damageScale` | float | ✘ | 0–10，默认 1.0 | 碰撞伤害倍率；**0 = 关掉碰撞伤害**（只靠技能） |
| `shrink.firstAt` | float | ✘ | 5–300，默认 25 | 第一次缩圈的绝对时间 |
| `shrink.interval` | float | ✘ | 3–120，默认 12 | 之后每隔多久一档 |
| `shrink.step` | float | ✘ | 0.01–0.2，默认 0.08 | 每档内缩比例（相对原尺寸） |
| `shrink.minInset` | float | ✘ | 0–1，默认 0.40 | 缩到多少停；`1` = 缩到 0，场地消失即结算 |
| `shrink.damagePerSec` | float | ✘ | 0–50，默认 4 | 压到球的持续伤害 |
| `shrink.lullTrigger` | float | ✘ | 0–60，默认 8 | 静默超过它就提前缩 |
| `seed` | int | ✘ | 任意 | 不给则用时间戳（演示时建议给） |

---

## 4. 校验规则（`tools/validate_content.ps1` 会全部检查）

1. 所有 `format` 必须为 1；未知字段报 **warning**，缺必填字段报 **error**。
2. `id` 必须与目录/文件名一致，且符合 `[a-z0-9][a-z0-9-]*`。
3. 角色引用的每个技能 id 必须存在于 `content/skills/`。
4. `skin.texture` / `bgTexture` 指向的文件必须存在。
5. `skills` 长度 ≤ 2；`spawns` 长度 == 2。
6. `hitbox.radius` ≥ 6 且 < `min(size)/4`。
7. 障碍必须完全落在 `size` 内。
8. `minInset = 1` 允许把场地缩到 0；装不下两个球时 `match` 立即结算，HP 高者胜（不会出现球被挤在一条线上）。
9. 颜色必须是 4 个 0–255 的整数。

校验输出示例：

```
[error] characters/lancer: hitbox.radius=4 is below minimum 6
[warn ] skills/dash: unknown field "coolDown" (did you mean "cooldown"?)
[ok   ] 3 characters, 4 skills, 2 maps
```

---

## 5. 命名约定

- 文件名全小写，连字符分隔：`slow-field.json`，不要 `SlowField.json`。
- 技能 id 与文件名一致；角色 id 与文件夹名一致。
- 一个技能只做一件事；"冲刺 + 回血"拆成两个技能，组合靠角色携带。
