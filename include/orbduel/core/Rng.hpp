#pragma once

// 全项目唯一的随机源（ADR-007）。同一个 seed 必须给出同一个序列，
// 否则 --seed 复现演示、--autotest 回归都失效。

#include <cstdint>

#include "orbduel/core/Types.hpp"

namespace orbduel {

class Rng {
public:
    explicit Rng(std::uint64_t seed = 0x9E3779B97F4A7C15ULL);
    ~Rng() = default;

    void reseed(std::uint64_t seed);

    std::uint32_t nextU32();

    // [0, 1)
    float nextFloat();

    // [low, high)
    float range(float low, float high);

    // [lowInclusive, highExclusive)，high 必须 > low
    int rangeInt(int lowInclusive, int highExclusive);

    // 单位圆上均匀分布的方向向量
    Vec2 unitVector();

    std::uint64_t state() const;

private:
    std::uint64_t state_;
};

} // namespace orbduel
