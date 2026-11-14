# 契约：core

- Types 是纯数学类型；Rng 是有种子的随机源，不消费系统随机设备。
- EventBus 为步末队列，回调中新发事件留给下一次派发。
- Event 新增 Damage 类型，以及 attackSpeedA、attackSpeedB（碰撞前正向速度）、tag（技能名或伤害原因）。
- Clock 只处理固定步长，不用反向 tick 实现 hit-stop。工作台暂停时不推进比赛时钟。

- log::setConsole(bool) 为批量模拟关闭控制台输出，仍保留最近日志缓冲。

- EntitySpawned / EntityRemoved：a 为实体 ID，b 为 owner；移除事件 tag 说明原因。Death 仍仅代表 HP 死亡。
