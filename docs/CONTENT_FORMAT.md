# 内容与工作台格式 / Content Format

角色和旧地图仍可从 `content/characters`、`content/skills`、`content/maps` 加载。工作台保存的地图是独立文档，可以包含这些内容的角色快照和原生模组对象。

## 工作台文档

```json
{
  "name": "My arena",
  "seed": 42,
  "size": [1280, 720],
  "style": {"preset":"cosmos", "decoration":0.7, "animate":true, "customColors":false,
            "background":[0.05,0.07,0.15], "accent":[0.63,0.66,1.0]},
  "rules": {"timeLimit":60, "shrink":false, "stall":true, "damageScale":1,
            "refSpeed":400, "maxSpeed":1800, "wallRestitution":0.98, "jitter":1.5},
  "fighters": [],
  "spawns": [{"pos":[192,360],"dir":[1,0]}, {"pos":[1088,360],"dir":[-1,0]}],
  "obstacles": [],
  "objects": [{"id":1,"type":"gravity.field","pos":[640,360],"size":[80,80],
               "radius":160,"angle":0,"owner":-1,"params":{"strength":180,"repel":false,"affect":0}}],
  "mods": ["gravity"]
}
```

这是字段结构示意；有效地图需要两个完整的 fighters。实际文件由编辑器生成，无需手写。示例地图在 `content/studio/`。

- fighters 保存角色的 ID、名字、贴图路径、半径、质量、生命、速度、恢复系数、攻击、护甲及技能 ID。
- objects 中 ID 在文档内唯一，type 是 DLL 注册的文本名称；params 由对应模组定义。
- owner 为 -1 时固定在地图，为 0/1 时跟随对应角色。
- 障碍有 shape、pos、size、radius、restitution；出生点有位置和方向。
- shrink 开启时可额外设置 firstAt、interval、step、minInset、warnTime、animTime、damagePerSec、lullTrigger。
- five presets: cosmos / dungeon / neon / lava / ice。

分享码将完整地图 JSON 编码为 Base64URL 并加 `OD1:` 前缀，不压缩、不加密、不添加哈希和签名。包含实际对象列表，因此手工位置不会丢失；DLL 和外部贴图不嵌入其中。

## 角色与内置技能

原角色 JSON 保持格式 1，在 stats 中新增可选 `attack`（默认 12）和 `armor`（默认 0）。技能字段仍为 trigger、modifiers；支持 cooldown、on_hit、on_hit_by、on_low_hp、on_wall_bounce，manual 仅保留数据定义，没有手动输入界面。

角色根级可选 `competitive`（默认 true）标记是否参与竞技平衡统计；木人桩设为 false，游戏与地图编辑器仍允许选择。该字段与角色快照一起保存。

普通碰撞使用 `地图 damageScale × 挨撞者 hitbox.contact`。contact 缺省为 1，0 为显式免疫普通碰撞；地图倍率为 0 则全局关闭普通碰撞。攻击加成使用攻击方碰撞前朝向对手的速度，倍率 `clamp(0.5+speed/refSpeed,0.5,2.5)`，再计连击、攻击增益、护甲和护盾。ghost 不代表免伤；蓄力重击、贯穿、引力和模组显式伤害独立结算。

旧地图若未保存 contact，将使用新的中性默认值 1；显式保存的 0 和其他自定义数值不会被修改。要采用当前默认角色数值，可在主页或属性面板点击“应用预设”。

角色和技能新增可选的中英 description；skin.pattern 可使用 flame、hex、chevron、crystal、spark、leaf 徽记，留空沿用贴图。这些字段也保存到地图与分享码中。

当前内置效果：add_speed（瞬时脉冲）、shield（带持续时间）、heal、lifesteal、slow_field、teleport、resize、bounce_boost、damage_boost（短时碰撞攻击倍率）、thorns（只响应碰撞来源的反伤）。各修饰器分别依自己的 duration 到期；加速不使用旧 curve 数值曲线。护盾刷新数值与时长，持续技能状态不做通用叠层解释。priority 决定同一角色的自动更新顺序。

VFX 描述仍可读取，但工作台当前不解释任意旧 VFX 配置；命中反馈、护盾显示和模组的 draw 接口承担实际表现。作者需要新机制时使用原生 SDK，不需要给旧 ModifierOp 枚举继续增加分支。

## 错误处理

内容加载使用候选快照；类型转换异常、未知效果和缺失引用进入 issues，失败不清空旧内容。地图导入失败保留旧草稿。模组按 ID 和 type 名查找，缺失时显示需要的名称。字段范围只处理运行必需的情况，不建立内容指纹或版本匹配系统。

反应技能在开局就绪，定时技能等待第一个冷却；低血量技能每局只触发一次。Thorns 反弹生命伤害与护盾吸收量之和的一部分，原因标记为 thorns，不再次触发反伤；吸血只按实际生命伤害回血。

## 主分支身体与特殊技能

`skin.shape` 支持 sprite/triangle；`hitbox.ghost` 决定是否允许穿入，contact 是挨撞倍率。`stats.massMax` 与 massRamp 控制质量随时间成长。地图快照使用 shape/ghost/contact/massMax/massRamp 同名字段，并保存 offset 和 anchor。缺少字段时采用模型默认值，其中 contact=1。

`dash` 保留原蓄力冲刺；瞬时加速使用 `burst-dash`。早期工作台自定义存档若要保留瞬时效果，需要将其技能引用从 dash 改为 burst-dash；内置示例已同步。

### charge（蓄力冲刺）

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

### passive（被动 / 贯穿）

`content/skills/pierce.json` 就是最小例子：

```json
"trigger": { "kind": "passive" },
"modifiers": [{ "op": "pierce", "value": 8.0, "minSpeed": 200.0, "push": 120.0 }]
```

开局读一次，之后每一步看速度：自身速度 ≥ `minSpeed` 就贯穿——撞上对手不分离、不换速度，
穿过时扣对手 `value` 血，并把对手沿法线推开 `push`。**一次穿过只扣一次血**（物理层按配对去重）。
速度不够时（例如原地蓄力，速度是 0）退回普通碰撞：该弹还弹、蓄力该打断还打断。

### gravity（被动 / 引力井）

`content/skills/gravity-well.json` 是唯一例子。有 `gravity` 块时 `modifiers` 可以为空。
它不靠撞击生效，而是**每步**给范围内的对手加一个向心加速度，并每秒扣一点血：

```json
"trigger": { "kind": "passive" },
"gravity": {
  "range": 280.0, "rangeMax": 440.0, "growthSeconds": 90.0,
  "accel": 460.0, "coreFactor": 0.72,
  "damage": 6.0, "damageMax": 24.0,
  "dpsEdge": 1.0, "dpsCenter": 8.0,
  "tickInterval": 0.5, "teleportGuard": 1.5, "teleportMinDist": 240.0
}
```

| 字段 | 类型 | 说明 |
|---|---|---|
| `range` | float 1–2000 | 起始吸力半径；出去就不拉，也不掉血 |
| `rangeMax` | float 1–2000 | 长满时的半径（不能小于 `range`） |
| `growthSeconds` | float (0,600] | 范围与核心伤害多少秒长满（按**对局时间**，不是放了几次） |
| `accel` | float 0–6000 | 向心加速度（px/s²）。保留对方切向速度 → 会绕着你转；实际值还会乘 `当前质量 / stats.mass` |
| `coreFactor` | float (0,1] | 核心半径 = 本值 × 自己 `hitbox.radius`；进核心才算「中心伤害」 |
| `damage` | float 0–500 | 起始中心伤害 |
| `damageMax` | float 0–500 | 中心伤害上限（按 `growthSeconds` 线性长到它） |
| `dpsEdge` | float 0–100 | 站在吸力场边缘每秒掉多少血 |
| `dpsCenter` | float 0–100 | 站在中心每秒掉多少血（按距离线性插值，0/0 = 场内不掉血） |
| `tickInterval` | float (0,10] | 核心内两次核心伤害的最小间隔 |
| `teleportGuard` | float 0–30 | 被传送走之后的保护期，期间不再吃中心伤害 |
| `teleportMinDist` | float 0–3000 | 落点离黑洞至少多远（防止刚传走就被吸回去） |

中心伤害之后对手会被随机丢到一个「站得住」的位置：在场地内、不压任何障碍、离黑洞 ≥ `teleportMinDist`。
采样 64 次都找不到就原地不动——宁可没传送，也不会把球塞进墙里。
配合 `hitbox.ghost` 才有「吞进去」的观感：不 ghost 时对手根本进不到核心圈。

### omnipull（大招 / 万向天引）

```json
"trigger": { "kind": "cooldown", "cooldown": 8.0 },
"omnipull": {
  "rangeFrom": 220.0, "rangeTo": 420.0, "accel": 1100.0,
  "durationFrom": 2.0, "durationTo": 5.0,
  "damageFrom": 22.0, "damageTo": 60.0, "growthSeconds": 90.0,
  "dpsEdge": 1.0, "dpsCenter": 3.0,
  "invincible": true, "suppressPassive": true
}
```

冷却到点后开始吸：范围内持续给对手加速度、每秒扣血 → 吸 `duration` 秒 → **原地爆炸**（半径 = 本次吸力半径，
命中范围内对手）→ 这时才进冷却。期间自己无敌、被动失效、整对不吃撞击伤害。
半径/时长/伤害按**对局时间**线性成长，`growthSeconds` 秒长满；**冷却固定 = `trigger.cooldown`**。

| 字段 | 说明 |
|---|---|
| `rangeFrom` → `rangeTo` | 吸力/爆炸半径 |
| `durationFrom` → `durationTo` | 吸多久才炸 |
| `damageFrom` → `damageTo` | 爆炸伤害 |
| `growthSeconds` | 多少秒长满（三组 `*From/*To` 共用） |
| `accel` | 吸力加速度，不成长（但会乘质量倍率） |
| `dpsEdge` / `dpsCenter` | 场内每秒掉血，算法同 §2.7 |
| `invincible` | 吸的过程中免伤害（默认 `true`） |
| `suppressPassive` | 吸的过程中自己的被动失效（默认 `true`） |

---
