#include "orbduel/core/Rng.hpp"

namespace orbduel {

namespace {
// splitmix64：把种子打散，避免连续种子产生相似序列
std::uint64_t mixSeed(std::uint64_t seed) {
    std::uint64_t z = seed + 0x9E3779B97F4A7C15ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}
} // namespace

Rng::Rng(std::uint64_t seed) : state_(mixSeed(seed)) {
    if (state_ == 0) {
        state_ = 0x9E3779B97F4A7C15ULL;
    }
}

void Rng::reseed(std::uint64_t seed) {
    state_ = mixSeed(seed);
    if (state_ == 0) {
        state_ = 0x9E3779B97F4A7C15ULL;
    }
}

std::uint32_t Rng::nextU32() {
    // xorshift64*
    state_ ^= state_ >> 12;
    state_ ^= state_ << 25;
    state_ ^= state_ >> 27;
    return static_cast<std::uint32_t>((state_ * 2685821657736338717ULL) >> 32);
}

float Rng::nextFloat() {
    // 24 位精度足够，且不会返回 1.0
    const std::uint32_t bits = nextU32() >> 8;
    return static_cast<float>(bits) / 16777216.0f;
}

float Rng::range(float low, float high) {
    return low + (high - low) * nextFloat();
}

int Rng::rangeInt(int lowInclusive, int highExclusive) {
    if (highExclusive <= lowInclusive) {
        return lowInclusive;
    }
    const int span = highExclusive - lowInclusive;
    return lowInclusive + static_cast<int>(nextU32() % static_cast<std::uint32_t>(span));
}

Vec2 Rng::unitVector() {
    const float angle = nextFloat() * 2.0f * kPi;
    return Vec2{std::cos(angle), std::sin(angle)};
}

std::uint64_t Rng::state() const { return state_; }

} // namespace orbduel
