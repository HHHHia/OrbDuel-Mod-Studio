#include "orbduel/match/StallGuard.hpp"

#include <algorithm>

namespace orbduel {

StallGuard::StallGuard(StallParams params) : params_(params) {}

void StallGuard::reset() { lull_ = 0.0f; }

void StallGuard::onHit() { lull_ = 0.0f; }

void StallGuard::update(float dt) { lull_ += dt; }

float StallGuard::lullSeconds() const { return lull_; }

Vec2 StallGuard::attraction(const Body& self, const Body& other) const {
    if (lull_ <= params_.lullStart) {
        return Vec2{0.0f, 0.0f};
    }

    const Vec2 delta = other.pos - self.pos;
    const float distance = lengthOf(delta);
    if (distance <= 1e-4f) {
        return Vec2{0.0f, 0.0f};
    }

    // 已经很近了就停止施力，避免「吸过头」
    const float stopDistance = params_.stopDistanceFactor * (self.radius + other.radius);
    if (distance <= stopDistance) {
        return Vec2{0.0f, 0.0f};
    }

    const float rampTime = params_.rampTime > 0.01f ? params_.rampTime : 0.01f;
    const float ramp = clampf((lull_ - params_.lullStart) / rampTime, 0.0f, 1.0f);

    return normalized(delta) * (params_.maxAccel * ramp);
}

const StallParams& StallGuard::params() const { return params_; }

void StallGuard::setParams(const StallParams& params) { params_ = params; }

} // namespace orbduel
