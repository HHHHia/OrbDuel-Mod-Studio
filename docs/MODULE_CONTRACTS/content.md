# 契约：content

**Owner**：C　**依赖**：`core` + nlohmann/json　**被依赖**：`game`、`match`

## 职责

- 定义数据模型（`CharacterDef` / `SkillDef` / `MapDef` 等）
- 扫描 `content/` 目录，解析 JSON，做结构与交叉校验
- 构建 `ContentPack`；提供 `LoadIssue` 错误列表
- 轮询文件修改时间，热重载变化的包

## 不负责

- 不创建 `Body`、不决定伤害（`match` 的活）
- 不碰 SDL2、不加载贴图（只给出路径）
- 不写文件

## 公开接口

| 文件 | 内容 |
|---|---|
| `content/DataModel.hpp` | `SkinDef` `HitboxDef` `StatsDef` `TriggerDef` `ModifierDef` `VfxDef` `SkillDef` `CharacterDef` `ObstacleDef` `SpawnDef` `ChargeDef` `GravityDef` `OmnipullDef` `ShrinkDef` `MapDef` `ContentPack` |
| `content/ContentLoader.hpp` | `loadAll(root)`、`poll()`、`pack()`、`issues()`、`hasFatal()` |

## 不变量

1. 加载失败**永远不抛异常给调用方**；失败信息进 `issues()`（`severity = error|warn`）。
2. 坏包不进入 `ContentPack`；同一个 id 的旧版本继续可用。
3. `poll()` 设计为每 0.5 s 最多执行一次、只在文件时间戳变化时重载——**目前是空实现（热重载未接）**。
4. 所有字符串 id 在加载时转小写并校验字符集。
5. 贴图路径统一转成相对 `content/` 的 POSIX 风格（`characters/lancer/skin.png`）。
6. `SkinDef.shape` 只存形状名（`sprite` 默认 / `triangle`），不认识几何细节；比例归 `game`。
7. `TriggerKind::Passive` 的技能不进冷却循环，由 `match` 在开局读一次；`ModifierOp::Pierce` 的 `minSpeed` / `push` 只对它有意义。
8. `gravity` / `omnipull` 是独立的参数块（不是 `modifiers` 里的 op）：`SkillDef.gravity/omnipull.enabled` 只有 JSON 里出现该块时才为真；
   `*From` → `*To` 的成长曲线归 `match`，`content` 只负责把数读对。有 `gravity` / `omnipull` 的技能允许 `modifiers` 为空。
9. `hitbox.ghost` 是纯载体属性（这具身体能不能被穿过去），不绑定任何技能。

## 禁止

- 在解析代码里写死任何具体角色/技能名
- 用 `assert` 处理内容错误（要能带错误信息继续跑）
- 直接 `LoadTexture`（渲染层的事）

## 验收

- `validate_content.ps1` 与运行期加载结果一致（同一份内容，同样的 error 数）
- 故意写坏一个 JSON：游戏不崩、屏幕出红字、旧角色仍可用
- 重命名一个角色目录：重启后列表自动更新
