#pragma once

// 缩圈调度（防僵局 L3，ARCHITECTURE §7）。
//
// 触发条件：对局时间到 firstAt，或静默超过 shrink.lullTrigger，取先。
// 每档内缩 step，预警 warnTime 秒后开始 animTime 秒的插值，缩到 minInset 为止。
//
// 不变量：只能通过 Arena::setShrinkTarget() 改场地。

#include "orbduel/content/DataModel.hpp"
#include "orbduel/physics/Arena.hpp"

namespace orbduel {

class ZoneController {
public:
    ZoneController(Arena& arena, const ShrinkDef& shrink);
    ~ZoneController() = default;

    void reset();

    // 换地图时重新设置缩圈参数（不影响 Arena 当前尺寸）。
    void configure(const ShrinkDef& shrink);

    // 每步调用；lullSeconds 来自 StallGuard。
    void update(float dt, float lullSeconds);

    int tier() const;
    bool warningActive() const;
    float timeToNextTier() const;
    float elapsed() const;

    // 缩到地板了吗（再缩球就没地方了）。
    bool atFloor() const;

    // 把两个球的半径传进来算地板；返回允许的最小 inset。
    float floorInset(float radiusA, float radiusB) const;

private:
    void startTier(float lullSeconds);

    Arena& arena_;
    ShrinkDef shrink_;

    float elapsed_ = 0.0f;
    float sincePrevTier_ = 0.0f;
    float warnLeft_ = 0.0f;
    int tier_ = 0;
    bool warning_ = false;
    bool atFloor_ = false;
};

} // namespace orbduel
