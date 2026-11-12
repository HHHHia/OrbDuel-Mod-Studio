#pragma once

// 物理实体。故意不含 HP / 技能 / 贴图 —— 那些属于 match 与 render。
// 这个结构体要能被单独测试：两个 Body + 一个 Rect 就能跑完整个物理。

#include "orbduel/core/Types.hpp"

namespace orbduel {

struct Body {
    int id = -1;
    Vec2 pos{};
    Vec2 vel{};

    // 本步累积的外力（静默吸引等）。每步结束清零，不要跨步保留。
    Vec2 force{};

    float radius = 18.0f;
    float mass = 1.0f;

    // 1 / mass，由 World::addBody 计算；外部不要手改。
    float invMass = 1.0f;

    // 恢复系数 [0,1]。球球碰撞取两者较小值。
    float restitution = 0.95f;

    // > 0 时不做球-球分离：直接穿过对方，并沿碰撞法线把对方推开这么多速度（px/s）。
    // 谁生效由 match 每步决定（速度阈值是游戏规则，物理不认识技能）。
    float phasePush = 0.0f;

    // true 时对方可以直接穿进身体：不做分离、也不推人，但「刚接触」那一步照样发 Hit
    // （撞击伤害对双方都成立，否则黑洞不吃任何伤害）。谁生效同样由 match 决定。
    bool ghost = false;

    bool active = true;
};

} // namespace orbduel
