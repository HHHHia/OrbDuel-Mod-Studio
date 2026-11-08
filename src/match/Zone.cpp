#include "orbduel/match/Zone.hpp"

#include <algorithm>

namespace orbduel {

ZoneController::ZoneController(Arena& arena, const ShrinkDef& shrink)
    : arena_(arena), shrink_(shrink) {
    reset();
}

void ZoneController::reset() {
    elapsed_ = 0.0f;
    sincePrevTier_ = 0.0f;
    warnLeft_ = 0.0f;
    tier_ = 0;
    warning_ = false;
    atFloor_ = false;
}

void ZoneController::configure(const ShrinkDef& shrink) {
    shrink_ = shrink;
    reset();
}

void ZoneController::update(float dt, float lullSeconds) {
    elapsed_ += dt;
    sincePrevTier_ += dt;

    if (atFloor_) {
        return;
    }

    const bool timeTrigger = (tier_ == 0 && elapsed_ >= shrink_.firstAt) ||
                             (tier_ > 0 && sincePrevTier_ >= shrink_.interval);
    const bool lullTrigger = lullSeconds >= shrink_.lullTrigger && sincePrevTier_ > 2.0f;

    if (!warning_ && (timeTrigger || lullTrigger)) {
        warning_ = true;
        warnLeft_ = shrink_.warnTime;
    }

    if (warning_) {
        warnLeft_ -= dt;
        if (warnLeft_ <= 0.0f) {
            startTier(lullSeconds);
        }
    }

    (void)lullSeconds;
}

void ZoneController::startTier(float lullSeconds) {
    (void)lullSeconds;
    ++tier_;

    const float target = std::min(shrink_.minInset, static_cast<float>(tier_) * shrink_.step);
    arena_.setShrinkTarget(target, shrink_.animTime);

    warning_ = false;
    sincePrevTier_ = 0.0f;

    if (target >= shrink_.minInset - 1e-4f) {
        atFloor_ = true;
    }
}

int ZoneController::tier() const { return tier_; }
bool ZoneController::warningActive() const { return warning_; }
float ZoneController::timeToNextTier() const { return warnLeft_; }
float ZoneController::elapsed() const { return elapsed_; }
bool ZoneController::atFloor() const { return atFloor_; }

float ZoneController::floorInset(float radiusA, float radiusB) const {
    const Rect base = arena_.baseBounds();
    const float minSide = std::min(base.w, base.h);
    if (minSide <= 1.0f) {
        return 0.0f;
    }
    const float need = 2.4f * (radiusA + radiusB);
    const float inset = 1.0f - need / minSide;
    return clampf(inset, 0.0f, 0.9f);
}

} // namespace orbduel
