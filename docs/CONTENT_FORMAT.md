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
  "id": "blaze",
  "name": { "en": "Blaze", "zh": "烈焰" },
  "skin": { "texture": "skin.png", "scale": 1.0, "tint": [255, 255, 255, 255] },
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
| `name.zh` | string | ✘ | 1–16 字符 | 缺省回退 `name.en` |
| `skin.texture` | string | ✔ | 相对本目录的文件名 | PNG，建议 256×256 |
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

---

## 2. skills/<id>.json

```json
{
  "format": 1,
  "id": "dash",
  "name": { "en": "Dash", "zh": "冲刺" },
  "trigger": { "kind": "cooldown", "cooldown": 6.0 },
  "modifiers": [
    { "op": "add_speed", "value": 220.0, "duration": 1.2, "curve": "ease_out" }
  ],
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
  "physics": { "wallRestitution": 0.98, "bounceJitterDeg": 1.5, "maxSpeed": 1800, "damageScale": 1.0, "refSpeed": 400 },
  "shrink": { "firstAt": 25.0, "interval": 12.0, "step": 0.08, "minInset": 0.40, "warnTime": 1.0, "animTime": 1.5, "damagePerSec": 4.0, "lullTrigger": 8.0 },
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
| `physics.damageScale` | float | ✘ | 0.1–10，默认 1.0 | 全局伤害倍率 |
| `shrink.firstAt` | float | ✘ | 5–300，默认 25 | 第一次缩圈的绝对时间 |
| `shrink.interval` | float | ✘ | 3–120，默认 12 | 之后每隔多久一档 |
| `shrink.step` | float | ✘ | 0.01–0.2，默认 0.08 | 每档内缩比例（相对原尺寸） |
| `shrink.minInset` | float | ✘ | 0–0.9，默认 0.40 | 缩到多少停（L4 地板另算） |
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
8. `shrink.minInset` 不能把场地缩到小于 `2.4 × (r1 + r2)`（加载时算，给出提示）。
9. 颜色必须是 4 个 0–255 的整数。

校验输出示例：

```
[error] characters/blaze: hitbox.radius=4 is below minimum 6
[warn ] skills/dash: unknown field "coolDown" (did you mean "cooldown"?)
[ok   ] 3 characters, 4 skills, 2 maps
```

---

## 5. 命名约定

- 文件名全小写，连字符分隔：`slow-field.json`，不要 `SlowField.json`。
- 技能 id 与文件名一致；角色 id 与文件夹名一致。
- 一个技能只做一件事；"冲刺 + 回血"拆成两个技能，组合靠角色携带。
