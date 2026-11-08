#pragma once

// 固定步长时钟：物理永远按 fixedDt 推进，和渲染帧率解耦（ADR-003）。
//
// 用法：
//   clock.tick(frameSeconds);
//   int steps = clock.consumeSteps();   // 这一帧该跑几个物理步
//   for (int i = 0; i < steps; ++i) world.step(clock.fixedDt());

namespace orbduel {

class Clock {
public:
    // fixedDt 默认 1/120 秒；maxStepsPerFrame 防止卡顿后一次性补太多步（死亡螺旋）。
    explicit Clock(float fixedDt = 1.0f / 120.0f, int maxStepsPerFrame = 8);
    ~Clock() = default;

    // 累加一帧的真实耗时（秒）。超出上限的时间会被丢弃。
    void tick(double frameSeconds);

    // 取出这一帧要执行的固定步数，并把账本减掉对应时间。
    int consumeSteps();

    // 渲染插值系数 [0,1)：上一物理步与下一步之间的比例。
    float alpha() const;

    float fixedDt() const;
    int maxStepsPerFrame() const;

    // 自 reset() 以来推进过的物理时间（秒）。
    double elapsed() const;

    void reset();

private:
    float fixedDt_;
    int maxSteps_;
    double accumulator_ = 0.0;
    double elapsed_ = 0.0;
    int pendingSteps_ = 0;
};

} // namespace orbduel
