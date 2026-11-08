#pragma once

// 场地：可缩放的矩形边界 + 静态障碍。
//
// 三态矩形：
//   base    原始边界（由 map.size 决定）
//   current 球真正反弹的边界（缩圈时被插值）
//   target  缩圈目标
//
// 不变量：只能用 setShrinkTarget() 改尺寸，不要直接写 bounds（见 AGENTS 常见坑）。

#include <vector>

#include "orbduel/core/Types.hpp"
#include "orbduel/physics/Body.hpp"

namespace orbduel {

enum class ObstacleShape {
    Rect = 0,
    Circle
};

struct Obstacle {
    int id = -1;
    ObstacleShape shape = ObstacleShape::Rect;
    Vec2 pos{};            // Rect 用中心；Circle 用圆心
    Vec2 size{};           // 仅 Rect：宽高
    float radius = 0.0f;   // 仅 Circle
    float restitution = 1.0f;
};

class Arena {
public:
    Arena();
    ~Arena() = default;

    void configure(Rect baseBounds, float maxSpeed);

    void addObstacle(const Obstacle& obstacle);
    void clearObstacles();

    Rect bounds() const;
    Rect baseBounds() const;
    Rect targetBounds() const;
    float maxSpeed() const;

    // inset01：相对原始尺寸的内缩比例，[0, 0.9]；duration 秒内平滑过渡。
    void setShrinkTarget(float inset01, float duration);

    bool isShrinking() const;
    float inset01() const;

    // 每步调用，推进缩圈插值；返回本步被压出边界的障碍 id。
    // 调用方（Zone / World）负责把这些 id 转成 ObstacleCrushed 事件。
    std::vector<int> update(float dt);

    // 球是否完整落在当前边界内。
    bool contains(Vec2 point, float radius) const;

    // 把越界的球推回并反射（恢复系数 ×0.9，不加能量）。返回是否触发。
    bool resolveOverlaps(Body& body, float wallRestitution);

    std::vector<Obstacle>& obstacles();
    const std::vector<Obstacle>& obstacles() const;

private:
    Rect base_{};
    Rect current_{};
    Rect target_{};

    float inset01_ = 0.0f;
    float shrinkFrom_ = 0.0f;
    float shrinkTo_ = 0.0f;
    float shrinkTime_ = 0.0f;
    float shrinkDuration_ = 0.0f;
    bool shrinking_ = false;

    float maxSpeed_ = 1800.0f;
    std::vector<Obstacle> obstacles_;
};

} // namespace orbduel
