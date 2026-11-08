#pragma once

// 防僵局 L1 / L2（ARCHITECTURE §6）。
//
//   L1 墙反弹抖动：参数在 map.physics.bounceJitterDeg，由 physics 使用
//   L2 静默吸引：本类负责。无碰撞超过 lullStart 才开始施力，命中立刻清零。
//
// 不变量：吸引只能加在速度上，不改质量；距离太近时停止施力，避免「贴贴」。

#include "orbduel/core/Types.hpp"
#include "orbduel/physics/Body.hpp"

namespace orbduel {

struct StallParams {
    float lullStart = 5.0f;          // 多少秒没碰撞算「僵局」
    float rampTime = 1.5f;           // 加速度从 0 渐入到满的时间
    float maxAccel = 15.0f;          // px/s^2，必须很小，肉眼几乎看不出
    float stopDistanceFactor = 3.0f; // 距离 < factor*(r1+r2) 时不再施力
    float shrinkLull = 8.0f;         // 静默超过它，Zone 提前缩圈
};

class StallGuard {
public:
    explicit StallGuard(StallParams params = StallParams{});
    ~StallGuard() = default;

    void reset();

    // 每次命中调用。
    void onHit();

    // 每个固定步调用一次。
    void update(float dt);

    float lullSeconds() const;

    // 当前应施加在 self 上的加速度（已包含渐入与停止距离）。
    // 对局正常进行时返回零向量。
    Vec2 attraction(const Body& self, const Body& other) const;

    const StallParams& params() const;
    void setParams(const StallParams& params);

private:
    StallParams params_;
    float lull_ = 0.0f;
};

} // namespace orbduel
