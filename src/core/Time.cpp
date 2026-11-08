#include "orbduel/core/Time.hpp"

namespace orbduel {

Clock::Clock(float fixedDt, int maxStepsPerFrame)
    : fixedDt_(fixedDt), maxSteps_(maxStepsPerFrame) {}

void Clock::tick(double frameSeconds) {
    if (frameSeconds <= 0.0) {
        return;
    }
    // 防止调试器断点后一次性补太多步
    if (frameSeconds > 0.25) {
        frameSeconds = 0.25;
    }
    accumulator_ += frameSeconds;
}

int Clock::consumeSteps() {
    int steps = 0;
    while (accumulator_ >= static_cast<double>(fixedDt_) && steps < maxSteps_) {
        accumulator_ -= static_cast<double>(fixedDt_);
        ++steps;
    }
    // 一帧最多补 maxSteps 步，剩下的时间直接丢，避免死亡螺旋
    const double cap = static_cast<double>(fixedDt_) * maxSteps_;
    if (accumulator_ > cap) {
        accumulator_ = cap;
    }
    elapsed_ += static_cast<double>(steps) * fixedDt_;
    return steps;
}

float Clock::alpha() const {
    if (fixedDt_ <= 0.0f) {
        return 0.0f;
    }
    return static_cast<float>(accumulator_ / fixedDt_);
}

float Clock::fixedDt() const { return fixedDt_; }
int Clock::maxStepsPerFrame() const { return maxSteps_; }
double Clock::elapsed() const { return elapsed_; }

void Clock::reset() {
    accumulator_ = 0.0;
    elapsed_ = 0.0;
    pendingSteps_ = 0;
}

} // namespace orbduel
