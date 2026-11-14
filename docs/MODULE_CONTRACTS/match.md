# 契约：match

- Match::setup 将两个角色翻译为 Body/Fighter，SkillSystem::attach 接收 Match、World、Rng 上下文。
- `configure(timeLimit, shrink, stall)` 设置本局开关；`update` 推进计时，`finishStep` 在伤害派发后统一判胜负。
- 伤害统一经 `damage(source,target,amount,cause)` / applyDamage，状态改变经 heal、giveShield；护盾到期消失，记录实际伤害与吸收量。
- 普通碰撞采用相对速度与挨撞者的连击倍率；系数为 max(地图 damageScale, 挨撞者 hitbox.contact)，再叠加攻击方攻击/技能倍率和目标护甲。双方数额先算好再结算。
- Simulation 拥有 SceneDocument 和 ContentPack 快照，持有插件实例、区域进入状态、延迟事件和对象增删队列；无 SDL 依赖。
- `Simulation::step` 固定步推进，`draw` 只调用模组绘制函数表；销毁所有实例后才能卸载插件管理器。
- 内置持续效果每步从基础半径/质量推导，结束时恢复；物理字段不存 HP 或技能名称。

- SkillState 增加 activeAge 和 casts；各效果独立到期。damageMultiplier() 查询当前碰撞攻击倍率，反伤只处理 contact 事件。
- Match::skillStates(index) 返回只读技能状态，用于界面显示冷却和效果；Fighter::healingDone 记录实际恢复量。
- SkillSystem::update 接收敌方 Body 和 seeded Rng，支持 ChargeDef 学习与中断；release 只计一次 casts，consumeRam 只消费一次窗口。
- Match::skills(index) 暴露只读技能查询。PullField、consumeBlast、invulnerable 与 suppressesPassive 保留引力大招规则；所有伤害转入 damage 以保留来源和统计。
- Fighter 保留 PierceMod、GravityMod 与核心伤害/传送间隔；质量每步从角色的 mass/massMax/massRamp 重算，再应用 resize。
- finishStep 在伤害派发后判胜负；完全缩圈比较原始 HP，时间上限比较 HP 比例。Simulation 在退化场地继续运行物理前调用 finishStep。

## 动态战斗实体

- fighterCount()/fighter(index) 枚举全部 Fighter；fighterById(bodyId) 按稳定编号查找，isEnemy(a,b) 根据队伍判断。同队物理接触不结算战斗命中；显式 damage 仍允许作者指定任意目标。
- addCombatant(id,def,pos,velocity,team,owner,lifetime) 在固定步边界复制角色定义、建立物理体及独立 SkillSystem；removeCombatant 只回收额外实体。Fighter 增加 team/owner/primary/lifetime/expires。最初两个槽始终是主角色，附属实体不参与胜负、计时与缩圈 HP 比较。
- 全体实体支持伤害、治疗、护盾、质量成长及技能。内置单目标技能选最近敌人，范围效果遍历敌人；保留贯穿、蓄力、引力成长、核心传送和万向天引状态机。
- Simulation::entityApi() 是静态可选函数表，调用传 Host::user。创建预留编号，创建/销毁/能力绑定在步首与步末提交；回调期间不改变枚举集合。绑定实例的 SceneObject.owner 为战斗实体 ID，每步跟随。
- 死亡先派发 death（快照仍可读），再派发 entity_removed，最后回收附属实体及绑定能力；寿命、显式清理不伪造 HP 死亡。所有者清理递归作用于后代，对局结束清理全部附属实体及角色绑定能力。主角色不可用扩展 destroy 移除。
- 实体 ID 与模组对象 ID 是独立空间。实体运行状态不写回草稿；create/bind 在预览和终局时返回 -1。无哈希、签名或版本匹配。细节见 COMBAT_ENTITIES。
