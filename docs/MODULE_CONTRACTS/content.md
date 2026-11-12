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
| `content/DataModel.hpp` | `SkinDef` `HitboxDef` `StatsDef` `TriggerDef` `ModifierDef` `VfxDef` `SkillDef` `CharacterDef` `ObstacleDef` `SpawnDef` `ChargeDef` `ShrinkDef` `MapDef` `ContentPack` |
| `content/ContentLoader.hpp` | `loadAll(root)`、`poll()`、`pack()`、`issues()`、`hasFatal()` |

## 不变量

1. 加载失败**永远不抛异常给调用方**；失败信息进 `issues()`（`severity = error|warn`）。
2. 坏包不进入 `ContentPack`；同一个 id 的旧版本继续可用。
3. `poll()` 每 0.5 s 最多执行一次；只在文件时间戳变化时重载。
4. 所有字符串 id 在加载时转小写并校验字符集。
5. 贴图路径统一转成相对 `content/` 的 POSIX 风格（`characters/blaze/skin.png`）。

## 禁止

- 在解析代码里写死任何具体角色/技能名
- 用 `assert` 处理内容错误（要能带错误信息继续跑）
- 直接 `LoadTexture`（渲染层的事）

## 验收

- `validate_content.ps1` 与运行期加载结果一致（同一份内容，同样的 error 数）
- 故意写坏一个 JSON：游戏不崩、屏幕出红字、旧角色仍可用
- 重命名一个角色目录：重启后列表自动更新
