# 契约：match

- Match::setup 将两个角色翻译为 Body/Fighter，SkillSystem::attach 接收 Match、World、Rng 上下文。
- `configure(timeLimit, shrink, stall)` 设置本局开关；`update` 推进计时，`finishStep` 在伤害派发后统一判胜负。
- 伤害统一经 `damage(source,target,amount,cause)` / applyDamage，状态改变经 heal、giveShield；护盾到期消失，记录实际伤害与吸收量。
- 普通碰撞倍率为地图 damageScale × 挨撞者 hitbox.contact；任一为 0 都关闭该方普通碰撞伤害。角色缺省 contact=1。ghost 只控制碰撞响应，无敌由技能状态决定。
- 伤害为 max(0, 攻击 × 碰撞倍率 × clamp(0.5+攻击方碰撞前正向速度/refSpeed,0.5,2.5) × 连击倍率 × 技能倍率 - 目标护甲)，随后消耗护盾。连击保留原有三秒窗口及 2.5 上限。双方数额先算好再结算。
- 蓄力重击、贯穿、引力与模组显式 damage 入口不使用普通碰撞开关；万向天引期间仍阻止双方普通碰撞伤害，并保留自身技能无敌。
- Simulation 拥有 SceneDocument 和 ContentPack 快照，持有插件实例、区域进入状态、延迟事件和对象增删队列；无 SDL 依赖。
- `Simulation::step` 固定步推进，`draw` 只调用模组绘制函数表；销毁所有实例后才能卸载插件管理器。
- 内置持续效果每步从基础半径/质量推导，结束时恢复；物理字段不存 HP 或技能名称。

- SkillState 增加 activeAge 和 casts；各效果独立到期。damageMultiplier() 查询当前碰撞攻击倍率，反伤只处理 contact 事件。
- Match::skillStates(index) 返回只读技能状态，用于界面显示冷却和效果；Fighter::healingDone 记录实际恢复量。
- SkillSystem::update 接收敌方 Body 和 seeded Rng，支持 ChargeDef 学习与中断；release 只计一次 casts，consumeRam 只消费一次窗口。
- Match::skills(index) 暴露只读技能查询。PullField、consumeBlast、invulnerable 与 suppressesPassive 保留引力大招规则；所有伤害转入 damage 以保留来源和统计。
- Fighter 保留 PierceMod、GravityMod 与核心伤害/传送间隔；质量每步从角色的 mass/massMax/massRamp 重算，再应用 resize。
- finishStep 在伤害派发后判胜负；完全缩圈比较原始 HP，时间上限比较 HP 比例。Simulation 在退化场地继续运行物理前调用 finishStep。

- Simulation 解释地图 fields：加速度每个固定步应用，伤害和治疗以 0.25 秒为间隔结算，环境伤害独立于普通碰撞开关。
