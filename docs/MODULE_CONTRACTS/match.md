# 契约：match

- Match::setup 将两个角色翻译为 Body/Fighter，SkillSystem::attach 接收 Match、World、Rng 上下文。
- `configure(timeLimit, shrink, stall)` 设置本局开关；`update` 推进计时，`finishStep` 在伤害派发后统一判胜负。
- 伤害统一经 `damage(source,target,amount,cause)` / applyDamage，状态改变经 heal、giveShield；护盾到期消失，记录实际伤害与吸收量。
- 普通碰撞按双方各自的攻击、入射速度和目标护甲独立计算，不保留同步全局连击。
- Simulation 拥有 SceneDocument 和 ContentPack 快照，持有插件实例、区域进入状态、延迟事件和对象增删队列；无 SDL 依赖。
- `Simulation::step` 固定步推进，`draw` 只调用模组绘制函数表；销毁所有实例后才能卸载插件管理器。
- 内置持续效果每步从基础半径/质量推导，结束时恢复；物理字段不存 HP 或技能名称。

- SkillState 增加 activeAge 和 casts；各效果独立到期。damageMultiplier() 查询当前碰撞攻击倍率，反伤只处理 contact 事件。
- Match::skillStates(index) 返回只读技能状态，用于界面显示冷却和效果；Fighter::healingDone 记录实际恢复量。
