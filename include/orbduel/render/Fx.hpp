#pragma once

// 表现层：粒子、圆环、拖尾、震屏、hit-stop。
// 只订阅事件，不读游戏状态，不改任何 Body（render 契约）。
//
// hit-stop 只是「标记」，由 game 读取后决定本帧是否跳过物理推进。

#include <vector>

#include "orbduel/core/Types.hpp"

namespace orbduel {

class EventBus;
class Renderer;

struct Particle {
    Vec2 pos{};
    Vec2 vel{};
    float life = 0.0f;
    float maxLife = 1.0f;
    float size = 2.0f;
    Rgba color{};
};

struct RingFx {
    Vec2 pos{};
    float elapsed = 0.0f;
    float duration = 0.35f;
    float maxRadius = 40.0f;
    Rgba color{};
};

struct TrailFx {
    Vec2 pos{};
    float life = 0.0f;
    float maxLife = 0.35f;
    float size = 6.0f;
    Rgba color{};
};

class Fx {
public:
    Fx();
    ~Fx() = default;

    void bind(EventBus& events);
    void update(float dt);
    void draw(Renderer& renderer);
    void clear();

    void spawnBurst(Vec2 pos, int count, float speed, Rgba color);
    void spawnRing(Vec2 pos, float maxRadius, Rgba color);
    void spawnTrail(Vec2 pos, float size, Rgba color);

    void addShake(float amount);
    void addHitStop(float seconds);

    bool hitStopActive() const;

    // 每帧读一次，读后清零。
    void consumeHitStop();

    float shakeOffsetX() const;
    float shakeOffsetY() const;

    static constexpr int kMaxParticles = 2048;
    static constexpr int kMaxRings = 64;
    static constexpr int kMaxTrails = 512;

private:
    std::vector<Particle> particles_;
    std::vector<RingFx> rings_;
    std::vector<TrailFx> trails_;

    float shake_ = 0.0f;
    float shakeTime_ = 0.0f;
    float hitStop_ = 0.0f;
};

} // namespace orbduel
