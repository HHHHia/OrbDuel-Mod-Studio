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

角色和技能新增可选的中英 description；skin.pattern 可使用 flame、hex、chevron、crystal、spark、leaf 徽记，留空沿用贴图。这些字段也保存到地图与分享码中。

当前内置效果：add_speed（瞬时脉冲）、shield（带持续时间）、heal、lifesteal、slow_field、teleport、resize、bounce_boost、damage_boost（短时碰撞攻击倍率）、thorns（只响应碰撞来源的反伤）。各修饰器分别依自己的 duration 到期；加速不使用旧 curve 数值曲线。护盾刷新数值与时长，持续技能状态不做通用叠层解释。priority 决定同一角色的自动更新顺序。

VFX 描述仍可读取，但工作台当前不解释任意旧 VFX 配置；命中反馈、护盾显示和模组的 draw 接口承担实际表现。作者需要新机制时使用原生 SDK，不需要给旧 ModifierOp 枚举继续增加分支。

## 错误处理

内容加载使用候选快照；类型转换异常、未知效果和缺失引用进入 issues，失败不清空旧内容。地图导入失败保留旧草稿。模组按 ID 和 type 名查找，缺失时显示需要的名称。字段范围只处理运行必需的情况，不建立内容指纹或版本匹配系统。

反应技能在开局就绪，定时技能等待第一个冷却；低血量技能每局只触发一次。Thorns 反弹生命伤害与护盾吸收量之和的一部分，原因标记为 thorns，不再次触发反伤；吸血只按实际生命伤害回血。
