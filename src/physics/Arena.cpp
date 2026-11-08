#include "orbduel/physics/Arena.hpp"

#include <algorithm>

namespace orbduel {

namespace {

Rect insetRect(const Rect& base, float inset01) {
    const float minSide = std::min(base.w, base.h);
    const float px = clampf(inset01, 0.0f, 0.9f) * 0.5f * minSide;
    Rect out;
    out.x = base.x + px;
    out.y = base.y + px;
    out.w = base.w - px * 2.0f;
    out.h = base.h - px * 2.0f;
    return out;
}

} // namespace

Arena::Arena() = default;

void Arena::configure(Rect baseBounds, float maxSpeed) {
    base_ = baseBounds;
    current_ = baseBounds;
    target_ = baseBounds;
    inset01_ = 0.0f;
    shrinkFrom_ = 0.0f;
    shrinkTo_ = 0.0f;
    shrinkTime_ = 0.0f;
    shrinkDuration_ = 0.0f;
    shrinking_ = false;
    maxSpeed_ = maxSpeed;
}

void Arena::addObstacle(const Obstacle& obstacle) {
    obstacles_.push_back(obstacle);
}

void Arena::clearObstacles() { obstacles_.clear(); }

Rect Arena::bounds() const { return current_; }
Rect Arena::baseBounds() const { return base_; }
Rect Arena::targetBounds() const { return target_; }
float Arena::maxSpeed() const { return maxSpeed_; }
bool Arena::isShrinking() const { return shrinking_; }
float Arena::inset01() const { return inset01_; }

void Arena::setShrinkTarget(float inset01, float duration) {
    const float clamped = clampf(inset01, 0.0f, 0.9f);
    shrinkFrom_ = inset01_;
    shrinkTo_ = clamped;
    shrinkTime_ = 0.0f;
    shrinkDuration_ = duration > 0.0f ? duration : 0.0f;
    shrinking_ = shrinkDuration_ > 0.0f;
    target_ = insetRect(base_, clamped);
    if (!shrinking_) {
        inset01_ = clamped;
        current_ = target_;
    }
}

std::vector<int> Arena::update(float dt) {
    std::vector<int> crushed;

    if (shrinking_) {
        shrinkTime_ += dt;
        float t = 1.0f;
        if (shrinkDuration_ > 0.0f) {
            t = clampf(shrinkTime_ / shrinkDuration_, 0.0f, 1.0f);
        }
        const float smooth = t * t * (3.0f - 2.0f * t);
        inset01_ = shrinkFrom_ + (shrinkTo_ - shrinkFrom_) * smooth;
        current_ = insetRect(base_, inset01_);
        if (t >= 1.0f) {
            shrinking_ = false;
            inset01_ = shrinkTo_;
            current_ = target_;
        }
    }

    // 缩圈压出边界的障碍直接碎掉
    for (std::size_t i = 0; i < obstacles_.size();) {
        const Obstacle& obstacle = obstacles_[i];
        bool inside = true;
        if (obstacle.shape == ObstacleShape::Rect) {
            const float halfW = obstacle.size.x * 0.5f;
            const float halfH = obstacle.size.y * 0.5f;
            inside = obstacle.pos.x - halfW >= current_.left() &&
                     obstacle.pos.x + halfW <= current_.right() &&
                     obstacle.pos.y - halfH >= current_.top() &&
                     obstacle.pos.y + halfH <= current_.bottom();
        } else {
            inside = obstacle.pos.x - obstacle.radius >= current_.left() &&
                     obstacle.pos.x + obstacle.radius <= current_.right() &&
                     obstacle.pos.y - obstacle.radius >= current_.top() &&
                     obstacle.pos.y + obstacle.radius <= current_.bottom();
        }

        if (inside) {
            ++i;
            continue;
        }

        crushed.push_back(obstacle.id);
        obstacles_.erase(obstacles_.begin() + static_cast<std::ptrdiff_t>(i));
    }

    return crushed;
}

bool Arena::contains(Vec2 point, float radius) const {
    return point.x - radius >= current_.left() &&
           point.x + radius <= current_.right() &&
           point.y - radius >= current_.top() &&
           point.y + radius <= current_.bottom();
}

bool Arena::resolveOverlaps(Body& body, float wallRestitution) {
    bool moved = false;
    const float r = body.radius;

    if (body.pos.x - r < current_.left()) {
        body.pos.x = current_.left() + r;
        if (body.vel.x < 0.0f) {
            body.vel.x = -body.vel.x * wallRestitution;
        }
        moved = true;
    }
    if (body.pos.x + r > current_.right()) {
        body.pos.x = current_.right() - r;
        if (body.vel.x > 0.0f) {
            body.vel.x = -body.vel.x * wallRestitution;
        }
        moved = true;
    }
    if (body.pos.y - r < current_.top()) {
        body.pos.y = current_.top() + r;
        if (body.vel.y < 0.0f) {
            body.vel.y = -body.vel.y * wallRestitution;
        }
        moved = true;
    }
    if (body.pos.y + r > current_.bottom()) {
        body.pos.y = current_.bottom() - r;
        if (body.vel.y > 0.0f) {
            body.vel.y = -body.vel.y * wallRestitution;
        }
        moved = true;
    }

    return moved;
}

std::vector<Obstacle>& Arena::obstacles() { return obstacles_; }
const std::vector<Obstacle>& Arena::obstacles() const { return obstacles_; }

} // namespace orbduel
