# 契约：content

- `ContentLoader::loadAll()` 显式重新加载内置角色、技能和地图；先构建候选内容，失败保留上一份可用 pack，错误存入 issues。
- 删除未实现的 poll 接口和时间戳缓存。工作台通过重新加载按钮调用 loadAll。
- `StatsDef` 新增 attack、armor，旧角色不填时分别默认 12、0。
- `SceneDocument` 持有地图、两个角色快照、模组对象、规则与风格；SceneObject 的 params 是由模组解释的 JSON 对象。
- `sceneJson/readScene/openScene/saveScene/shareCode/importCode` 共用同一文档；读入失败不修改输出参数。
- `generateScene` 使用 core/Rng 生成镜像布局，GenerationType 由已注册的模组类型提供。
- content 不使用 SDL；SDK 参数声明和编辑器控件之间通过 PluginManager 转换。

- CharacterDef / SkillDef 增加可选 description，SkinDef 增加 pattern；地图快照完整保存，旧文档缺少时使用空值。
- ModifierOp 新增 DamageBoost 和 Thorns，字段与技能 schema、内容工具同步。
- 同时保留 skin.shape、hitbox.ghost/contact、stats.massMax/massRamp、Passive/Pierce、ChargeDef/GravityDef/OmnipullDef；加载器和 schema 解释全部字段。
- 地图快照完整保存程序化形状、身体属性、成长值、offset 与 anchor；旧快照缺少字段时使用模型默认值。
- dash 保持蓄力定义；原工作台瞬时技能更名为 burst-dash，所有内置工作台引用同步更新。
