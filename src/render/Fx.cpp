#include "orbduel/render/Fx.hpp"

#include <cmath>

#include "orbduel/core/Event.hpp"
#include "orbduel/render/Renderer.hpp"

namespace orbduel {

namespace {
std::uint8_t fade(std::uint8_t base, float ratio) {
    const float value = static_cast<float>(base) * ratio;
    if (value <= 0.0f) {
        return 0;
    }
    if (value >= 255.0f) {
        return 255;
    }
    return static_cast<std::uint8_t>(value);
}
} // namespace

Fx::Fx() = default;

void Fx::bind(EventBus& events) {
    events.subscribe(EventType::Hit, [this](const Event& event) {
        const float speed = event.value;
        spawnBurst(event.pos, 16, 90.0f + speed * 0.35f, Rgba{255, 190, 110, 255});
        spawnRing(event.pos, 22.0f + speed * 0.06f, Rgba{255, 240, 200, 200});
        addShake(1.5f + speed * 0.008f);
    });

    events.subscribe(EventType::WallBounce, [this](const Event& event) {
        if (event.value > 260.0f) {
            spawnBurst(event.pos, 5, 40.0f, Rgba{140, 170, 255, 160});
        }
    });

    events.subscribe(EventType::ObstacleCrushed, [this](const Event& event) {
        spawnBurst(event.pos, 24, 130.0f, Rgba{200, 200, 210, 255});
        spawnRing(event.pos, 40.0f, Rgba{220, 220, 230, 200});
        addShake(3.0f);
    });

    events.subscribe(EventType::SkillCast, [this](const Event& event) {
        spawnRing(event.pos, 34.0f, Rgba{120, 220, 255, 220});
    });

    events.subscribe(EventType::SkillExpire, [this](const Event& event) {
        spawnBurst(event.pos, 28, 240.0f, Rgba{190, 140, 255, 255});
        spawnRing(event.pos, event.value > 8.0f ? event.value : 40.0f, Rgba{200, 160, 255, 220});
        addShake(4.5f);
    });

    events.subscribe(EventType::Death, [this](const Event&) {
        addShake(6.0f);
        addHitStop(0.08f);
    });
}

void Fx::update(float dt) {
    for (Particle& p : particles_) {
        p.life -= dt;
        p.pos += p.vel * dt;
        p.vel *= 0.985f;
    }

    particles_.erase(
        std::remove_if(particles_.begin(), particles_.end(),
                       [](const Particle& p) { return p.life <= 0.0f; }),
        particles_.end());

    for (RingFx& ring : rings_) {
        ring.elapsed += dt;
    }
    rings_.erase(
        std::remove_if(rings_.begin(), rings_.end(),
                       [](const RingFx& r) { return r.elapsed >= r.duration; }),
        rings_.end());

    for (TrailFx& trail : trails_) {
        trail.life -= dt;
    }
    trails_.erase(
        std::remove_if(trails_.begin(), trails_.end(),
                       [](const TrailFx& t) { return t.life <= 0.0f; }),
        trails_.end());

    if (shake_ > 0.0f) {
        shakeTime_ += dt;
        shake_ = std::max(0.0f, shake_ - dt * 18.0f);
    } else {
        shakeTime_ = 0.0f;
    }

    if (hitStop_ > 0.0f) {
        hitStop_ = std::max(0.0f, hitStop_ - dt);
    }
}

void Fx::draw(Renderer& renderer) {
    for (const TrailFx& trail : trails_) {
        Rgba color = trail.color;
        color.a = fade(color.a, trail.life / trail.maxLife);
        renderer.drawCircle(trail.pos, trail.size * 0.5f, color);
    }

    for (const Particle& p : particles_) {
        Rgba color = p.color;
        color.a = fade(color.a, p.life / p.maxLife);
        renderer.drawCircle(p.pos, p.size, color);
    }

    for (const RingFx& ring : rings_) {
        const float t = ring.duration > 0.0f ? (ring.elapsed / ring.duration) : 1.0f;
        const float radius = ring.maxRadius * t;
        Rgba color = ring.color;
        color.a = fade(color.a, 1.0f - t);
        renderer.drawCircleLines(ring.pos, radius, color);
    }
}

void Fx::clear() {
    particles_.clear();
    rings_.clear();
    trails_.clear();
    shake_ = 0.0f;
    hitStop_ = 0.0f;
}

void Fx::spawnBurst(Vec2 pos, int count, float speed, Rgba color) {
    if (count <= 0) {
        return;
    }
    for (int i = 0; i < count; ++i) {
        if (particles_.size() >= static_cast<std::size_t>(kMaxParticles)) {
            break;
        }
        // 用索引做确定性的角度与速度扰动，不额外消耗全局 RNG
        const float angle = (2.0f * kPi * static_cast<float>(i)) / static_cast<float>(count) +
                            static_cast<float>((i * 37) % 11) * 0.07f;
        const float speedScale = 0.55f + 0.45f * static_cast<float>((i * 53) % 100) / 100.0f;

        Particle p;
        p.pos = pos;
        p.vel = Vec2{std::cos(angle), std::sin(angle)} * (speed * speedScale);
        p.maxLife = 0.35f + 0.05f * static_cast<float>(i % 4);
        p.life = p.maxLife;
        p.size = 2.0f + static_cast<float>(i % 3);
        p.color = color;
        particles_.push_back(p);
    }
}

void Fx::spawnRing(Vec2 pos, float maxRadius, Rgba color) {
    if (rings_.size() >= static_cast<std::size_t>(kMaxRings)) {
        rings_.erase(rings_.begin());
    }
    RingFx ring;
    ring.pos = pos;
    ring.maxRadius = maxRadius;
    ring.duration = 0.32f;
    ring.color = color;
    rings_.push_back(ring);
}

void Fx::spawnTrail(Vec2 pos, float size, Rgba color) {
    if (trails_.size() >= static_cast<std::size_t>(kMaxTrails)) {
        trails_.erase(trails_.begin());
    }
    TrailFx trail;
    trail.pos = pos;
    trail.size = size;
    trail.maxLife = 0.3f;
    trail.life = trail.maxLife;
    trail.color = color;
    trails_.push_back(trail);
}

void Fx::addShake(float amount) {
    shake_ = std::min(14.0f, shake_ + amount);
}

void Fx::addHitStop(float seconds) {
    hitStop_ = std::max(hitStop_, seconds);
}

bool Fx::hitStopActive() const { return hitStop_ > 0.0f; }
void Fx::consumeHitStop() { hitStop_ = 0.0f; }

float Fx::shakeOffsetX() const {
    if (shake_ <= 0.0f) {
        return 0.0f;
    }
    return std::sin(shakeTime_ * 71.0f) * shake_;
}

float Fx::shakeOffsetY() const {
    if (shake_ <= 0.0f) {
        return 0.0f;
    }
    return std::cos(shakeTime_ * 57.0f) * shake_;
}

} // namespace orbduel
