#pragma once

// 物理实体。故意不含 HP / 技能 / 贴图 —— 那些属于 match 与 render。
// 这个结构体要能被单独测试：两个 Body + 一个 Rect 就能跑完整个物理。

#include "orbduel/core/Types.hpp"

namespace orbduel {

struct Body {
    float wallBoost = 0.0f; // 单体附加的墙面恢复系数，不包含技能或 HP 状态。
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

    bool active = true;
};

} // namespace orbduel
