# 契约：physics

- Body 保存位置、速度、半径、质量与恢复系数，不存 HP 或技能 ID；新增 wallBoost 为单体附加墙面恢复系数。
- World 的 Hit 事件保存双方碰撞前正向速度；分离中的接触不重复产生 Hit。
- 恢复系数为 min(1, wallRestitution + wallBoost)，最大速度在积分前限制。
- Arena 更新边界后由 Simulation 调用 resolveOverlaps，再结算开启缩圈时的挤压伤害。
- Body::ghost 不分离、不推开；phasePush 允许穿过并推开对手。两类重叠配对每次接触只发布一次 Hit，分开后重置。
- Arena 允许 inset=1；Match 在边界无法容纳角色时终局，Simulation 停止后续物理。
- physics 不依赖内容、模组或 SDL；输入为固定 1/120 秒步长。
